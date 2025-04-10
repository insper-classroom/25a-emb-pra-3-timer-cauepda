/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include <stdio.h>
 #include <string.h>
 #include "pico/stdlib.h"
 #include "hardware/gpio.h"
 #include "hardware/rtc.h"
 #include "pico/time.h"

// Definição dos pinos
#define TRIG_PIN 15
#define ECHO_PIN 14

volatile bool timer_fired = false;
volatile bool action_completed = false;

volatile absolute_time_t t_descida;
volatile absolute_time_t t_subida;


int64_t alarm_callback(alarm_id_t id, void *user_data) {
    timer_fired = true;
    return 0;
}

void echo_callback(uint gpio, uint32_t events) {
    if (gpio == ECHO_PIN && events == 0x8) { // rise edge
        t_subida = get_absolute_time();
    }

    if (gpio == ECHO_PIN && events == 0x4) { // fall edge
        t_descida = get_absolute_time();
        timer_fired = false;
        action_completed = true;
    }
}

void print_datetime(void) {
    datetime_t now;
    rtc_get_datetime(&now);
    printf("%02d:%02d:%02d - ", now.hour, now.min, now.sec);
}

int main() {
    stdio_init_all();
    sleep_ms(2000);

    rtc_init();
    datetime_t t = {
        .year  = 2025,
        .month = 3,
        .day   = 19,
        .dotw  = 3,
        .hour  = 1,
        .min   = 0,
        .sec   = 0
    };
    rtc_set_datetime(&t);
    
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);
    
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, echo_callback);

    bool reading_active = false;
    char command[20];
    int cmd_index = 0;
    alarm_id_t alarm;
    memset(command, 0, sizeof(command));

    printf("Digite 'start' para iniciar a leitura e 'stop' para parar:\n");

    while (true) {
        int ch = getchar_timeout_us(0);
        if (ch != PICO_ERROR_TIMEOUT) {
            if (ch == '\n' || ch == '\r') {
                if (cmd_index > 0) {
                    command[cmd_index] = '\0';
                    if (strcmp(command, "start") == 0) {
                        reading_active = true;
                        printf("Leitura iniciada!\n");
                    } else if (strcmp(command, "stop") == 0) {
                        reading_active = false;
                        printf("Leitura parada!\n");
                    } else {
                        printf("Comando desconhecido. Use 'start' ou 'stop'.\n");
                    }
                    cmd_index = 0;
                    memset(command, 0, sizeof(command));
                }
            } else {
                if (cmd_index < (int)(sizeof(command) - 1)) {
                    command[cmd_index++] = (char) ch;
                }
            }
        }
        
        if (reading_active) {
            action_completed = false;
            timer_fired = false;

            gpio_put(TRIG_PIN, 1);
            sleep_us(10);
            gpio_put(TRIG_PIN, 0);
            alarm = add_alarm_in_ms(500, alarm_callback, NULL, false);

            absolute_time_t measure_start = get_absolute_time();
            while (!action_completed && !timer_fired) {
                sleep_ms(1);
                if (absolute_time_diff_us(measure_start, get_absolute_time()) > 1000000)
                    break;
            }
            cancel_alarm(alarm);
            
            print_datetime();
            if (action_completed) {
                
                int64_t pulse_duration = absolute_time_diff_us(t_subida, t_descida);
                float distance = (pulse_duration * 0.0343f) / 2.0f;
                printf("%.2f cm\n", distance);
            } else if (timer_fired) {
                printf("Falha\n");
            } else {
                printf("Leitura não concluída\n");
            }
            sleep_ms(1000);
        }
    
    sleep_ms(10);
    }
    
    return 0;
}