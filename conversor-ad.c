#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "ssd1306.h"

#define GREEN_LED  11
#define BLUE_LED   12
#define RED_LED    13
#define BUTTON_A   5
#define VRX        26
#define VRY        27
#define BUTTON_JOYSTICK 22

#define I2C_SDA 14
#define I2C_SCL 15
#define I2C_ID           i2c1
#define SSD1306_ADDR     0x3C
#define I2C_FREQ         100000

const uint16_t PERIOD = 4095;
const float DIVIDER_PWM = 255.0;
bool pwm_enabled = true;

static volatile uint32_t last_time = 0;
ssd1306_t ssd;

#define DEAD_ZONE 100

void setup();
void gpio_irq_handler(uint gpio, uint32_t events);
void update_leds(int x, int y);
void update_display(int x, int y, int last_x_pos, int last_y_pos);
void handle_BUTTON_A(int vrx_value, int vry_value);
void handle_BUTTON_JOYSTICK();

void setup() {
    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);
    gpio_put(GREEN_LED, 0);

    gpio_set_function(BLUE_LED, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BLUE_LED);
    pwm_set_clkdiv(slice, DIVIDER_PWM);
    pwm_set_wrap(slice, PERIOD);
    pwm_set_gpio_level(BLUE_LED, 0);
    pwm_set_enabled(slice, true);

    gpio_set_function(RED_LED, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(RED_LED);
    pwm_set_clkdiv(slice, DIVIDER_PWM);
    pwm_set_wrap(slice, PERIOD);
    pwm_set_gpio_level(RED_LED, 0);
    pwm_set_enabled(slice, true);

    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_JOYSTICK);
    gpio_set_dir(BUTTON_JOYSTICK, GPIO_IN);
    gpio_pull_up(BUTTON_JOYSTICK);

    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);

    i2c_init(I2C_ID, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, SSD1306_ADDR, I2C_ID);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

void update_leds(int x, int y) {
    if (pwm_enabled) {
        uint16_t red_intensity = (abs(x - 2048) > DEAD_ZONE) ? abs(x - 2048) : 0;
        uint16_t blue_intensity = (abs(y - 2048) > DEAD_ZONE) ? abs(y - 2048) : 0;
        if (abs(x - 2048) <= DEAD_ZONE) red_intensity = 0;

        pwm_set_gpio_level(RED_LED, red_intensity);
        pwm_set_gpio_level(BLUE_LED, blue_intensity);
    }
}

void update_display(int x, int y, int last_x_pos, int last_y_pos) {
    int norm_x = x * 120 / 4096 + 4;
    int norm_y = (4096 - y) * 56 / 4096 + 4;

    int norm_last_x = last_x_pos * 120 / 4096 + 4;
    int norm_last_y = (4096 - last_y_pos) * 56 / 4096 + 4;

    ssd1306_rect(&ssd, norm_last_y, norm_last_x, 8, 8, 0, 1);
    ssd1306_rect(&ssd, norm_y, norm_x, 8, 8, 1, 1);
    ssd1306_send_data(&ssd);
}

void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (current_time - last_time > 300000) {
        if (gpio == BUTTON_A) {
            int vrx_value = adc_read();
            int vry_value = adc_read();
            handle_BUTTON_A(vrx_value, vry_value);
        } else if (gpio == BUTTON_JOYSTICK) {
            handle_BUTTON_JOYSTICK();
        }
        last_time = current_time;
    }
}

void handle_BUTTON_A(int vrx_value, int vry_value) {
    pwm_enabled = !pwm_enabled;
    uint slice = pwm_gpio_to_slice_num(BLUE_LED);
    pwm_set_enabled(slice, pwm_enabled);

    slice = pwm_gpio_to_slice_num(RED_LED);
    pwm_set_enabled(slice, pwm_enabled);

    if (!pwm_enabled) {
        pwm_set_gpio_level(RED_LED, 0);
        pwm_set_gpio_level(BLUE_LED, 0);
    } else {
        if (abs(vrx_value - 2048) <= DEAD_ZONE && abs(vry_value - 2048) <= DEAD_ZONE) {
            pwm_set_gpio_level(RED_LED, 0);
            pwm_set_gpio_level(BLUE_LED, 0);
        }
    }
}

void handle_BUTTON_JOYSTICK() {
    gpio_put(GREEN_LED, !gpio_get(GREEN_LED));
    ssd1306_rect(&ssd, 3, 3, 122, 60, gpio_get(GREEN_LED), 0);
    ssd1306_send_data(&ssd);
}

int main() {
    stdio_init_all();
    setup();

    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);
    ssd1306_send_data(&ssd);

    uint16_t vrx_value, vry_value;
    uint16_t last_x_pos = 2048, last_y_pos = 2048;

    while (true) {
        adc_select_input(1);
        vrx_value = adc_read();

        adc_select_input(0);
        vry_value = adc_read();

        update_leds(vrx_value, vry_value);
        update_display(vrx_value, vry_value, last_x_pos, last_y_pos);
        last_x_pos = vrx_value;
        last_y_pos = vry_value;

        sleep_ms(10);
    }
}
