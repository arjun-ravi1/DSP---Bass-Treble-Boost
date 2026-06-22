#include "stdio.h"
#include "usbstk5515.h"
#include "aic3204.h"
#include "PLL.h"
#include "stereo.h"
#include "usbstk5515_led.h"
#include "pushbuttons.h"

#define mod(x,m) (((x < 0) ? ((x % m) + m) : x) % m)
#define FILTER_LEN 17
#define DELAY_LINE_LEN 64

/*
 FIR filter coefficients Q1.15
 LOW PASS:
 652, 658, 664, 671, 675, 678, 681, 682, 682, 682, 682, 678, 675, 671, 664, 658, 652
 
 HIGH PASS:
 -1130, -1436, -1740, -2013, -2257, -2458, -2608, -2698, 30036, -2698, -2608, -2458, -2257, -2013, -1740, -1436, -1130
 */

Int16 x_left;
Int16 x_right;
Int16 y_left;
Int16 y_right;
Int16 x, x_delayed, y_lp, y_hp;
Int16 delay_line[DELAY_LINE_LEN] = {0};

int b_lp[FILTER_LEN] = {
    652, 658, 664, 671, 675, 678, 681, 682, 682, 682, 682, 678, 675, 671, 664, 658, 652
};

int b_hp[FILTER_LEN] = {
    -1130, -1436, -1740, -2013, -2257, -2458, -2608, -2698, 30036, -2698, -2608, -2458, -2257, -2013, -1740, -1436, -1130
};

long acc_lp = 0, acc_hp = 0;
int delay_line_index = 0, j;
char treble_enabled = 0, bass_enabled = 0;
unsigned int switches = 0, switches_1 = 0;

#define SAMPLES_PER_SECOND 48000
#define GAIN_IN_dB 0 // ADC gain in dB

void main( void ) {
USBSTK5515_init( );
pll_frequency_setup(120);
aic3204_hardware_init();
aic3204_init();
SAR_init();
USBSTK5515_ULED_init();
USBSTK5515_LED_off(0);
printf( "<-> ECET 339 - Lab 10<->\n" );

set_sampling_frequency_and_gain(SAMPLES_PER_SECOND, GAIN_IN_dB);

for ( ; ; ) {
    aic3204_codec_read(&x_left, &x_right);
    x = stereo_to_mono(x_left, x_right);

    delay_line[delay_line_index] = x; // push x[n]

    // LOW-PASS FILTER
    acc_lp = 0;
    for(j=0; j<FILTER_LEN; j++) {
        x_delayed = delay_line[ mod(delay_line_index - j, DELAY_LINE_LEN) ];
        acc_lp += (long)b_lp[j] * x_delayed >> 15;
    }
    y_lp = (int)(acc_lp * 4);

    // HIGH-PASS FILTER
    acc_hp = 0;
    for(j=0; j<FILTER_LEN; j++) {
        x_delayed = delay_line[ mod(delay_line_index - j, DELAY_LINE_LEN) ];
        acc_hp += (long)b_hp[j] * x_delayed >> 15;
    }
    y_hp = (int)(acc_hp * 4);

    y_left = 0;
    if(bass_enabled)
        y_left += y_lp;
    if(treble_enabled)
        y_left += y_hp;

    y_right = x_right;

    delay_line_index++;
    if(delay_line_index >= DELAY_LINE_LEN) delay_line_index = 0;

    aic3204_codec_write(y_left, y_right);

    switches = pushbuttons_read_raw();

    if( switches == 1 && switches_1 == 0 ) { // SW1
        bass_enabled ^= 1;
        USBSTK5515_ULED_toggle(1);
    }
    if( switches == 2 && switches_1 == 0 ) { // SW2
        treble_enabled ^= 1;
        USBSTK5515_ULED_toggle(2);
    }
    if( switches == 3 )  // BOTH pressed
        break;

    switches_1 = switches;
}

aic3204_disable();
printf( "\n***Program has Terminated***\n" );
SW_BREAKPOINT;
}
