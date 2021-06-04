//
#define X_MIN 0
#define X_MAX 1023
#define Y_MIN 0
#define Y_MAX 1023

char buttonState[16];

//
uint8_t pr, pg, pb;//pending colors
ISR(TIMER1_COMPA_vect)
{
    TIMSK1 &= ~(1 << OCIE1A); //disable interrupt
    setColor(pr, pg, pb); //apply color
}

//TODO: mask and shift both bits at once
void setPos(unsigned int x, unsigned int y)
{
    //flip x
    x = 1023 - x;
    
    PORTA = (y & 0xFF);
    PORTC = ((y & (1 << 8)) >> 1) + ((y & (1 << 9)) >> 3);
    
    PORTL = (x & 0xFF);
    PORTG = ((x & (1 << 8)) >> 8) + ((x & (1 << 9)) >> 8);
}

//
void line(long x1, long y1, long x2, long y2, long step)
{
    //TODO: optimize, cache this since line lens will not change?
    long steps = sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) / step;
    for (unsigned int s = 0; s < steps; s++)
    {
        //TODO: optimize to eliminate divide, use fixed point instead?
        long x = x1 + (x2 - x1) * s / steps;
        //unsigned int x = x1 + s;
        long y = y1 + (y2 - y1) * s / steps;

        setPos(x,y);        

        //delay(1);
        //delayMicroseconds(10);
    }
}

//
void box(int x, int y, int width, int height, int step)
{
    int x2 = x + width;
    int y2 = y + height;
    
    line(x, y, x2, y, step);
    line(x2, y, x2, y2, step);
    line(x2, y2, x, y2, step);
    line(x, y2, x, y, step);
}

//
void setColorDelayed(uint8_t r, uint8_t g, uint8_t b, uint16_t _delay)
{
    pr = r;
    pg = g;
    pb = b;

  OCR1A = _delay; //15624 = 1 second
  TCNT1  = 0; //initialize counter value to 0
  TIFR1 |= ICF1; //clear any interrupt flag
  TIMSK1 |= (1 << OCIE1A); //enable interrupt
}

//
void setColor(uint8_t r, uint8_t g, uint8_t b)
{
    analogWrite(5, 255 - r);
    analogWrite(6, 255 - g);
    if (b == 0)
        digitalWrite(7, HIGH);
    else
        digitalWrite(7, LOW);
//    analogWrite(7, 255 - b);
}

#define CLOCK_PIN 12
#define LATCH_PIN 11
#define DATA_PIN 10

#define CONTROLLER_UP 3
#define CONTROLLER_DOWN 4
#define CONTROLLER_LEFT 5
#define CONTROLLER_RIGHT 6
#define CONTROLLER_A 7

//
void refreshButtonState()
{
    digitalWrite(LATCH_PIN, HIGH);
    delayMicroseconds(12);
    digitalWrite(LATCH_PIN, LOW);
    delayMicroseconds(6);
    
    for (int i = 0; i < 16; i++)
    {
        digitalWrite(CLOCK_PIN, HIGH);
        delayMicroseconds(3);
        buttonState[i] = !digitalRead(DATA_PIN);
        delayMicroseconds(3);
        
        digitalWrite(CLOCK_PIN, LOW);
        delayMicroseconds(6);
    }
}

//
void logButtonState()
{
    for (int i = 0; i < 16; i++)
    {
        Serial.print(i);
        Serial.print(":");
        Serial.print((int)buttonState[i]);
        Serial.print("  ");
    }
    
    Serial.println();
}

#define PADDLE_WIDTH 300
#define PADDLE_HEIGHT 80

#define BALL_STATE_DOCKED 0
#define BALL_STATE_LAUNCHED 1

int ballX, ballY;
int ballVelX, ballVelY;
int paddleX = 512, paddleY = 100;
char ballState = BALL_STATE_DOCKED;

//
#define CONTROL_SPEED 12
#define PADDLE_MIN_X 0
#define PADDLE_MAX_X (1023 - PADDLE_WIDTH)
#define PADDLE_MIN_Y 0
#define PADDLE_MAX_Y (1023 - PADDLE_HEIGHT)
void updatePaddle()
{
    if (buttonState[CONTROLLER_UP])
        paddleY += CONTROL_SPEED;
    if (buttonState[CONTROLLER_DOWN])
        paddleY -= CONTROL_SPEED;
    if (buttonState[CONTROLLER_LEFT])
        paddleX -= CONTROL_SPEED;
    if (buttonState[CONTROLLER_RIGHT])
        paddleX += CONTROL_SPEED;
    
    paddleX = constrain(paddleX, PADDLE_MIN_X, PADDLE_MAX_X);
    paddleY = constrain(paddleY, PADDLE_MIN_Y, PADDLE_MAX_Y);
}

//
#define BALL_WIDTH 30
#define BALL_HEIGHT 30
#define BALL_MIN_X 0
#define BALL_MAX_X (1023 - BALL_WIDTH)
#define BALL_MIN_Y 0
#define BALL_MAX_Y (1023 - BALL_HEIGHT)
void updateBall()
{
    //sync ball pos to paddle pos
    if (ballState == BALL_STATE_DOCKED)
    {
        ballX = paddleX + PADDLE_WIDTH / 2;
        ballY = paddleY + PADDLE_HEIGHT;
        
        if (buttonState[CONTROLLER_A])
        {
            ballState = BALL_STATE_LAUNCHED;
            ballVelY = 20;
            ballVelX = 20;
        }
        
        return;
    }
    
    //ball 'physics'
    
    ballX += ballVelX;
    ballY += ballVelY;
    
    //lost ball
    if (ballY < 0)
    {
        ballState = BALL_STATE_DOCKED;
        return;
    }
    
    //bounce off ceiling
    if (ballY > BALL_MAX_Y)
    {
        ballVelY = -ballVelY;
        ballY = BALL_MAX_Y;
    }
    
    //bounce off left wall
    if (ballX < BALL_MIN_X)
    {
        ballX = BALL_MIN_X;
        ballVelX = -ballVelX;
    }

    //bounce off right wall
    if (ballX > BALL_MAX_X)
    {
        ballX = BALL_MAX_X;
        ballVelX = -ballVelX;
    }
    
    //bounce off paddle if moving down
    if (ballVelY < 0 &&
        ballY < paddleY + PADDLE_HEIGHT &&
        ballY + BALL_HEIGHT > paddleY &&
        ballX < paddleX + PADDLE_WIDTH &&
        ballX + BALL_WIDTH > paddleX)
    {
        ballVelY = -ballVelY;
    }
}

//
void render()
{
    //walls
    setColorDelayed(0, 255, 0, 10);
    line(0, 0, 0, 1023, 20);
    line(0, 1023, 1023, 1023, 60);
    line(1023, 1023, 1023, 0, 20);
    setColor(0,0,0);

    //paddle
    setColorDelayed(255, 0, 0, 30);
    box(paddleX, paddleY, PADDLE_WIDTH, PADDLE_HEIGHT, 10);
    setColor(0,0,0);
    
    //ball
    if (ballState == BALL_STATE_LAUNCHED)
        setColorDelayed(0, 0, 255, 30);
    else
        setColorDelayed(0, 0, 255, 30);
    box(ballX, ballY, BALL_WIDTH, BALL_HEIGHT, 2);
    setColor(0,0,0);
    setColorDelayed(0, 0, 0, 1);
}

//
void setup()
{
  TIMSK1 &= ~(1 << OCIE1A); //disable interrupt

  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  
    
    Serial.begin(115200);
    
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(LATCH_PIN, OUTPUT);
    pinMode(DATA_PIN, INPUT);
            
    DDRA = 0xFF;
    DDRC = 0xFF;
    
    DDRL = 0xFF;
    DDRG = 0xFF;
    
    while (1)
    {
        refreshButtonState();
        logButtonState();
        updatePaddle();
        updateBall();
        render();
    }
}

//
void loop()
{
}

