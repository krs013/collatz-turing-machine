#include <msp430.h> 

#define GREEN           0xAA
#define RED             0x55
#define LED0            0x03
#define LED1            0x0C
#define LED2            0x30
#define LED3            0xC0

#define TAPE_WORD(X)    (tape[(X) >> 4])
#define TAPE_BIT(X)     (1 << (X & 0xF))
#define READ_TAPE(X)    (TAPE_WORD(X) >> ((X) & 0xF) & 1)

unsigned char leds[3] = {0};
unsigned tape[16] = {0};
unsigned char left = 127, right = 128, head = 128;


int get_bit(unsigned index) {
    if (left < right)
        return left < index && index < right ? (int) READ_TAPE(index) : -1;
    else
        return index < left || right < index ? (int) READ_TAPE(index) : -1;
}


void set_led(unsigned led, int color) {
    unsigned bank;
    unsigned char mask;

    if (led >= 12)
        return;

    bank = led >> 2;
    led &= 0x3;
    mask = LED0 << led << led;
    leds[bank] &= ~mask;

    if (color > 0)
        leds[bank] |= mask & GREEN;
    else if (color == 0)
        leds[bank] |= mask & RED;
}


void draw_leds() {

    // Clear buffer
    leds[0] = leds[1] = leds[2] = 0;

    // If tape is empty, just return
    if (left + 1 == right)
        return;

    set_led(0, get_bit(head-2));
    set_led(2, get_bit(head-1));
    set_led(3, get_bit(head-1));
    set_led(5, get_bit(head));
    set_led(6, get_bit(head));
    set_led(8, get_bit(head+1));
    set_led(9, get_bit(head+1));
    set_led(11, get_bit(head+2));
}


void update_head_leds() {
    set_led(5, get_bit(head));
    set_led(6, get_bit(head));
}


int write(int symbol) {
    // -1 to erase, 0 for 0, +1 for 1
    if (symbol < 0) {
        // Only should be erasing at ends
        if (head + 1 == right)
            --right;
        else if (head - 1 == left)
            ++left;
        else
            return -1;
        return 0;
    } else if (symbol > 0) {
        tape[((unsigned) head) >> 4] |= 1 << (head & 0x0F);
    } else {
        tape[((unsigned) head) >> 4] &= ~(1 << (head & 0x0F));
    }

    // Can't add more than 255 bits
    if (right == left)
        return -1;

    // Extend tape boundaries if necessary
    if (head == left)
        --left;
    else if (head == right)
        ++right;

    return 0;
}


int push_right(int symbol) {
    head = right;
    return write(symbol);
}


int push_left(int symbol) {
    head = left;
    return write(symbol);
}


int load_number(long input) {
    do {
        if (push_left(input & 0x1) < 0)
            return -1;
    } while (input >>= 1);
    return 0;
}


void shift_left(int newled) {
    leds[0] >>= 1;
    if (leds[1] & 0x01) leds[0] |= 0x80;
    leds[1] >>= 1;
    if (leds[2] & 0x01) leds[1] |= 0x80;
    leds[2] >>= 1;

    leds[0] >>= 1;
    if (leds[1] & 0x01) leds[0] |= 0x80;
    leds[1] >>= 1;
    if (leds[2] & 0x01) leds[1] |= 0x80;
    leds[2] >>= 1;

    if (newled > 0) leds[2] |= LED3 & GREEN;
    else if (newled == 0) leds[2] |= LED3 & RED;

    return;
}


void shift_right(int newled) {
    leds[2] <<= 1;
    if (leds[1] & 0x80) leds[2] |= 0x01;
    leds[1] <<= 1;
    if (leds[0] & 0x80) leds[1] |= 0x01;
    leds[0] <<= 1;

    leds[2] <<= 1;
    if (leds[1] & 0x80) leds[2] |= 0x01;
    leds[1] <<= 1;
    if (leds[0] & 0x80) leds[1] |= 0x01;
    leds[0] <<= 1;

    if (newled > 0) leds[0] |= LED0 & GREEN;
    else if (newled == 0) leds[0] |= LED0 & RED;

    return;
}


int advance() {
    static enum led_state_t {READWRITE, READWRITE1, READWRITE2, LMOVE, LMOVE1, LMOVE2, RMOVE, RMOVE1, RMOVE2, IDLE} state = READWRITE;
    static enum turing_state_t {MOVE, CHECK1, CHECK2, MUL0, MUL1, MUL2, HALT, ERROR} turing = MOVE;

    while (1) {
        switch (state) {
        int symbol;
        case READWRITE:
            state = READWRITE1;
            if (turing != MOVE)
                break;
        case READWRITE1:
            state = READWRITE2;
            if (turing != MOVE)
                break;
        case READWRITE2:
            symbol = get_bit(head);
            switch(turing) {
            case MOVE:
                if (symbol < 0)
                    turing = CHECK1, state = LMOVE;
                else
                    state = RMOVE;
                break;
            case CHECK1:
                if (symbol == 0)
                    turing = write(-1) < 0 ? ERROR : turing, state = LMOVE;
                else if (symbol > 0)
                    turing = CHECK2, state = LMOVE;
                else
                    turing=ERROR, state=READWRITE;
                break;
            case CHECK2:
                if (symbol < 0)
                    turing=HALT, state=RMOVE;
                else
                    turing=MUL1, state=RMOVE;
                break;
            case MUL0:
                if (symbol == 0)
                    state=LMOVE;
                else if (symbol > 0)
                    turing=MUL1, state=LMOVE;
                else
                    turing=MOVE, state=RMOVE;
                break;
            case MUL1:
                if (symbol == 0)
                    turing = write(1) < 0 ? ERROR : MUL0, state=LMOVE;
                else if (symbol > 0)
                    turing = write(0) < 0 ? ERROR : MUL2, state=LMOVE;
                else
                    turing = write(1) < 0 ? ERROR : MUL0, state=LMOVE;
                break;
            case MUL2:
                if (symbol == 0)
                    turing=MUL1, state=LMOVE;
                else if (symbol > 0)
                    turing=MUL2, state=LMOVE;
                else
                    turing = write(0) < 0 ? ERROR : MUL1, state=LMOVE;
                break;
            case HALT:
                leds[0] = GREEN;
                leds[2] = GREEN;
                state = IDLE;
                break;
            case ERROR:
            default:
                leds[0] = RED;
                leds[2] = RED;
                state = IDLE;
                break;
            }
            update_head_leds();
            if (turing==MOVE)
                continue;
            else
                break;
        case LMOVE:
            shift_right(get_bit(head - 2));
            --head;
            ++state;
            break;
        case LMOVE1:
            shift_right(-1);
            ++state;
            break;
        case LMOVE2:
            shift_right(get_bit(head - 2));
            state = READWRITE;
            break;
        case RMOVE:
            shift_left(get_bit(head + 2));
            ++head;
            ++state;
            break;
        case RMOVE1:
            shift_left(-1);
            ++state;
            break;
        case RMOVE2:
            shift_left(get_bit(head + 2));
            state = READWRITE;
            break;
        case IDLE:
        default:
            break;
        };
        break;
    }
    return state == IDLE ? (turing == ERROR ? -1 : 1) : 0;
}


int main(void) {
    int i;
    unsigned n = 0;

    WDTCTL = WDT_MDLY_32;               // 32 ms WDT interrupt
    SFRIE1 |= WDTIE;

    // Sample a random 16-bit number from the diff in VLO vs DCO clocks
    TA0CTL = TASSEL_3 | MC_2;           // continuous mode, VLOCLK
    TA0CCTL0 = CM_3 | CCIS_2 | SCS | CAP;
    __enable_interrupt();
    for (i=16; i>0; i--) {
        LPM0;
        TA0CCTL0 ^= CCIS0;
        n <<= 1;
        n |= TA0R & 1;
    }

    TA0CTL = TACLR;                     // reset TA0
    TA0CCR0 = 256;                      // period
    TA0CCTL0 = CCIE;                    // enable CCR0 EQU0 interrupt
    TA0CTL = TASSEL_2 | MC_1;           // up mode, SMCLK

    P1SEL0 = 0;
    P1SEL1 = 0;
    P2SEL0 = 0;
    P2SEL1 = 0;

    P1DIR = 0x00;
    P2DIR = 0xFF;

    P1OUT = 0x00;
    P2OUT = 0x07;

    PM5CTL0 &= ~LOCKLPM5;

    // Sequence starts here
    load_number((long) n);

    draw_leds();

    while (!advance()) {
        __enable_interrupt();
        LPM1;
        // Would be better to sync to scan than WDT, or spin lock until blank
        __disable_interrupt();
    }

    __enable_interrupt();
    i = 80;
    while (--i)
        LPM0;

    // Shutdown
    __disable_interrupt();

    WDTCTL = WDTPW | WDTHOLD;

    TA0CCTL0 = 0;
    TA0CTL = 0;

    P1OUT = 0x00;
    P2OUT = 0x00;

    P1DIR = 0xFF;
    P2DIR = 0xFF;

    // Disable VCORE, so LPM4 becomes LPM4.5
    PMMCTL0_H = PMMPW_H;
    PMMCTL0_L |= PMMREGOFF;
    PMMCTL0_L &= ~SVSHE;
    PMMCTL0_H = 0;

    LPM4;

    // Wakeup shouldn't be possible except by BOR, but just in case, reset
    WDTCTL = 0;
}


#define WDT_SLOW 3
#pragma vector = WDT_VECTOR
__interrupt void WDT_ISR() {
    static unsigned slow = WDT_SLOW;
    if (--slow == 0)
        slow = WDT_SLOW;
    else
        return;

    LPM1_EXIT;
    return;
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void TIMER0_CCR0_ISR() {
    static unsigned bank = 0, led = 1;
    P2DIR = 0xF8;
    if (--led == 0) {
        led = 4;
        bank = bank > 0 ? bank - 1 : 2;
        P2OUT = 0x07 ^ (1 << bank);
        P1OUT = leds[bank];
        P1DIR = 0xC0;
    } else {
        P1DIR >>= 2;
    }
    P2DIR = 0xFF;
    return;
}
