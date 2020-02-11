/*
 * My_Sweet_Home_V5.c
 *
 * Created: 7/16/2017 4:10:08 PM
 *  Author: NAVID_Home
 */ 


#define F_CPU 11059200UL


#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#define RXD						PIND0
#define TXD						PIND1
#define INT0_Pir_enter_exit		PIND2				//door of room
#define INT1_pir_motion			PIND3				//center of room
#define Auto_Enable_Button1		PIND4				//internally pulled up
#define Auto_Enable_Button2		PIND5				//internally pulled up
#define First_key				PIND6				//1->lamp should be off	internally pulled up
#define Second_Key				PIND7				//1->lamp should be off	internally pulled up
#define Auto_LED1				PINB0
#define Auto_LED2				PINB1
#define Relay1					PINB2
#define Relay2					PINB3
#define GPIO1					PINB4				//Reserved
#define GPIO2					PINB5				//Reserved
#define Reserved				PINC0
#define Day_Night				PINC1
#define Inside_Port				PORTD
#define Outside_Port			PORTB
#define Day_Night_Or_PIR_Port	PORTC
#define Inside_Pin				PIND
#define Outside_Pin				PINB
#define Day_Night_Or_PIR_Pin	PINC


// timer0 : Button Debouncing
// timer1 : Lamp On/Off Timer
// timer2 : PIR Sensor Check <- gablan  vali  alan -> zaman baraye tashkhis nabudan kasi dar otag

struct ROOM 
{
	char *name;				 						//room name
	char day_night_adc_reference_value;				//reference value for detect day or night
	char day_night_adc_reference_tolerance;	
	char lamp1_in_auto_mode;						//0:manual mode								1:auto mode
	char lamp2_in_auto_mode;						//0:manual mode								1:auto mode
	char key1_is_disconnect;						//0:key1 is connect(lamp should be on)		1:key1 is disconnect(lamp should be off)
	char key2_is_disconnect;						//0:key2 is connect(lamp should be on)		1:key2 is disconnect(lamp should be off)
	char have_wifi_module;							//0:don't have wifi module					1:have wifi module
	char wifi_initializing_state;					//0:start initialize	1:cipap		2:cipsta	3:cipmux	4:cipserver
	char wifi_initializing_scape;
	uint16_t lamp_on_off_timer_min;
	char timer_finish_lamp_on_or_off;				//0:off										1:on
	char timer_finish_lamp_1_or_2;					//0:2										1:1
	char Is_Dark;									//0:no										1:yes
	char tolerance_time_for_no_person_in_room;		//second
	char top_lamp_on_with_mobile_command;			//0:no										1:yes
	char side_lamp_on_with_mobile_command;			//0:no										1:yes
	char someone_in_room;							//0:no										1:yes
	char need_for_rx_operation;						//0:no										1:yes
	char was_time2_enabled;							//0:no										1:yes
	char INT0_Pir_enter_exit_last_situation;		//baraye jelo giri az amal kardane pulse ke az taghire jarayane barg tolid mishavad
};


//char exit_ch_counter;							//shomarandeye zaman baraye halati ke sensore vurud va khuruj, 0 mishavad va sensor harekat 1 ast ke bayad barrasi shavad ke asar harekate khuruj ast ke 1 mande ya kasi dakhele otag ast
char auto_button_can_action;						//0:button can't action						1:button can action			for button debounce
char timer0_count;
char timer1_count;
char timer2_count;
char timer2_second_counter;
char buffer[30];									//rx_operation_command_buffer
char buffer2[30];									//RX Receive Buffer
uint8_t sp;											//Buffer Stack Pointer
uint8_t sp2;										//Buffer Stack Pointer
uint8_t day_night_adc_reference_value_eeprom;
uint8_t day_night_adc_reference_tolerance_eeprom;
									
struct ROOM room;


void esp_initialize(char i)		//0:usart=ready		1:usart=OK				2:usart=ERROR
{
	if (i==0)
	{
		_delay_ms(100);
		room.wifi_initializing_state=0;
		room.wifi_initializing_scape=0;
	}
	if (i==1)
	{
		room.wifi_initializing_state++;
		room.wifi_initializing_scape=0;
	}		
	if (i==2)
	{
		room.wifi_initializing_scape++;
	}
	if (room.wifi_initializing_scape<=4)
	{
		switch(room.wifi_initializing_state)
		{
			case 0:
				esp_command("ATE1");
			break;
			case 1:
				esp_command("AT+CWMODE=3");			
			break;
			case 2:
				esp_command("AT+CIPAP=#192.168.1.250#,#192.168.1.250#,#255.255.255.0#");
			break;
			case 3:
				esp_command("AT+CIPSTA=#192.168.1.250#,#192.168.1.1#,#255.255.255.0#");
			break;
			case 4:
				esp_command("AT+CIPMUX=1");
			break;
			case 5:
				esp_command("AT+CIPSERVER=1,5010");
			break;
		}
	}
	
 	
}
void initialize()				//mand
{
	DDRD &= ~(1<<RXD | 1<<Auto_Enable_Button1 | 1<<Auto_Enable_Button2 | 1<<First_key | 1<<Second_Key | 1<<INT0_Pir_enter_exit | 1<<INT1_pir_motion);			
	DDRD |= (1<<TXD);
	Inside_Port |= 1<<Auto_Enable_Button1 | 1<<Auto_Enable_Button2 | 1<<First_key | 1<<Second_Key;										//buttons internally pulled up	
																	
	DDRB |= 1<<Auto_LED1 | 1<<Auto_LED2 | 1<<Relay1 | 1<<Relay2;
	Outside_Port |= 1<<Auto_LED1 | 1<<Auto_LED2;
	
	DDRC &= ~(1<<Day_Night);
	Day_Night_Or_PIR_Port &= ~(1<<Day_Night);						//Both are tristate	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// Variable Initialize
	sp = 0;
	sp2=0;
	timer0_count = 0;
	timer1_count = 0;
	//timer2_count = 0;
	auto_button_can_action = 1;
	room.name = "navid";// TODO
	room.lamp1_in_auto_mode = 1;
	room.lamp2_in_auto_mode = 1;
	//room.day_night_adc_reference_value = 128; // TODO
	//room.day_night_adc_reference_tolerance = 10; // TODO
	room.have_wifi_module = 1;
	room.wifi_initializing_scape = 0;
	
	room.key2_is_disconnect = 1;
	room.key1_is_disconnect = 1;
	
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// Interrupts enable
	sei();
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// Uart
	UCSRB |= 1<<RXCIE | 1<<RXEN | 1<<TXEN;
	UCSRC |= 1<<URSEL | 1<<UCSZ0 | 1<<UCSZ1;
	UBRRH = 0x00;
	UBRRL = 5;    //////////////115,200
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// Timers Interrupts Enable
	TIMSK |= (1<<TOIE2 | 1<<TOIE1| 1<<TOIE0);
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// External Interrupts Enable
	MCUCR &= ~(1<<ISC01);
	MCUCR |= (1<<ISC00);
	GICR |= (1<<INT0);
	MCUCR |= (1<<ISC10 | 1<<ISC11);
	GICR |= (1<<INT1);
}
void lamp_on(char n )
{
	if(n==1)	Outside_Port |= 1<<Relay1;
	if(n==2)	Outside_Port |= 1<<Relay2;
}
void lamp_off(char n)
{
	if(n==1)	Outside_Port &= ~(1<<Relay1);
	if(n==2)	Outside_Port &= ~(1<<Relay2);
}
void lamp_auto_on()
{
	if(room.Is_Dark)			//if night
	{
		if(room.lamp1_in_auto_mode)		Outside_Port |= 1<<Relay1;
	    if(room.lamp2_in_auto_mode)		Outside_Port |= 1<<Relay2;
	}	
	///////// !!!user check must be done
}
void auto_toggle(char n)
{
	if(n==1)
	{
		if (room.lamp1_in_auto_mode)
		{
			room.lamp1_in_auto_mode = 0;
			Outside_Port &= ~(1<<Auto_LED1);
		}
		else
		{
			room.lamp1_in_auto_mode = 1;
			Outside_Port |= 1<<Auto_LED1;
		}
		if (room.someone_in_room)
		{
			//There are somebody in the room
			if (room.Is_Dark)
			{
				lamp_auto_on();
			}
		}
	}
	if(n==2)
	{
		if (room.lamp2_in_auto_mode)
		{
			room.lamp2_in_auto_mode = 0;
			Outside_Port &= ~(1<<Auto_LED2);
		}
		else
		{
			room.lamp2_in_auto_mode = 1;
			Outside_Port |= 1<<Auto_LED2;
		}
		if (room.someone_in_room)
		{
			//There are somebody in the room
			if (room.Is_Dark)
			{
				lamp_auto_on();
			}
		}
	}
	/////////////////////////////////////
	if(!(room.lamp1_in_auto_mode))	
	{
		if (room.key1_is_disconnect)
		{
			lamp_off(1);
		}
		else
		{
			lamp_on(1);
		}
	}	
	if(!(room.lamp2_in_auto_mode))	
	{
		if (room.key2_is_disconnect)
		{
			lamp_off(2);
		}
		else
		{
			lamp_on(2);
		}
	}	
}
void timer0_start()									//////////// for auto button debounce
{
	TCNT0 = 0;
	TCCR0 |= 1<<CS00 | 1<<CS02;
	auto_button_can_action=0;
}
void timer0_stop()
{
	TCCR0 &= ~(1<<CS00 | 1<<CS02);
	auto_button_can_action=1;
}
void timer1_on()									//////////// for lamp on off timer
{
	TCCR1B |= 1<<CS10 | 1<<CS12;  // clkI/O/1024 (From prescaler)
}
void timer1_off()
{
	TCCR1B &= ~(1<<CS10 | 1<<CS11 | 1<<CS12);
	room.lamp_on_off_timer_min=0;
	timer1_count=0;
		
}	
void time_end()
{
	if(room.timer_finish_lamp_on_or_off)	
	{
		if (room.timer_finish_lamp_1_or_2)
		{
			lamp_on(1);
			room.top_lamp_on_with_mobile_command=1;
		}
		else
		{
			lamp_on(2);
			room.side_lamp_on_with_mobile_command=1;
		}
		
	}
	else
	{
		if (room.timer_finish_lamp_1_or_2)
		{
			lamp_off(1);
			room.top_lamp_on_with_mobile_command=0;
		}
		else
		{
			lamp_off(2);
			room.side_lamp_on_with_mobile_command=0;
		}
	}
}
void timer_enable()							//////////// i:shomareye avvalin makane ragham
{
	int i=0;
	timer1_count = 0;
	room.lamp_on_off_timer_min=0;
	while(!((buffer[i] <= 57) && (buffer[i] >= 48)))
	{
		i++;
	}
	while((buffer[i] <= 57) && (buffer[i] >= 48))
	{
		room.lamp_on_off_timer_min = 10*room.lamp_on_off_timer_min;
		room.lamp_on_off_timer_min += buffer[i]-48;
		i++;
	}
	//////////////////////////////
	TCNT1H = 0X1C;
	TCNT1L = 0XC0;
	buffer_wash();
	timer1_on();
}
void rx_operation()
{
	if (room.have_wifi_module)
	{
		if (/*(string_compare(buffer,"ready")>-1) |*/ (string_compare(buffer,"CHECK")>-1))
		{
			esp_initialize(0);
			buffer_wash();
		}
		if ((string_compare(buffer,"OK")>-1) | (string_compare(buffer,"Ok")>-1) | (string_compare(buffer,"ok")>-1))
		{
			esp_initialize(1);
			buffer_wash();
		}
		if (string_compare(buffer,"ERROR")>-1)
		{
			esp_initialize(2);
			buffer_wash();
		}
	}
	if (string_compare(buffer , room.name)>-1)
	{
		
		if (string_compare(buffer,"turn on top") >-1)		
		{
			lamp_on(1);
			room.top_lamp_on_with_mobile_command=1;
		}		
		if (string_compare(buffer,"turn on side") >-1)		
		{
			lamp_on(2);
			room.side_lamp_on_with_mobile_command=1;
		}		
		if (string_compare(buffer,"turn off top") >-1)		
		{
			lamp_off(1);
			room.top_lamp_on_with_mobile_command=0;
		}
		if (string_compare(buffer,"turn off side") >-1)		
		{
			lamp_off(2);
			room.side_lamp_on_with_mobile_command=0;
		}
		if (string_compare(buffer,"auto enable top") >-1)
		{
			if (!(room.lamp1_in_auto_mode))	auto_toggle(1);
		}
		if (string_compare(buffer,"auto enable side") >-1)
		{
			if (!(room.lamp2_in_auto_mode))	auto_toggle(2);
		}
		if (string_compare(buffer,"auto disable top") >-1)
		{
			if (room.lamp1_in_auto_mode)	auto_toggle(1);
		}
		if (string_compare(buffer,"auto disable side") >-1)
		{
			if (room.lamp2_in_auto_mode)	auto_toggle(2);
		}
		if (string_compare(buffer,"timer on top") >-1)
		{
			timer_enable();
			room.timer_finish_lamp_on_or_off = 1;
			room.timer_finish_lamp_1_or_2 = 1;
		}
		if (string_compare(buffer,"timer on side") >-1)
		{
			timer_enable();
			room.timer_finish_lamp_on_or_off = 1;
			room.timer_finish_lamp_1_or_2 = 0;
		}
		if (string_compare(buffer,"timer off top") >-1)
		{
			timer_enable();
			room.timer_finish_lamp_on_or_off = 0;
			room.timer_finish_lamp_1_or_2 = 1;
		}
		if (string_compare(buffer,"timer off side") >-1)
		{
			timer_enable();
			room.timer_finish_lamp_on_or_off = 0;
			room.timer_finish_lamp_1_or_2 = 0;
		}
		if (string_compare(buffer,"timer disable") >-1)		
		{
			timer1_off();	
		}
		buffer_wash();
	}
}
int string_compare(char *buff , char *string )
{
	char i=0, j=0, k=0;
	
	check_again:
	while((*(buff+i) != *(string)) && (i<30))
	{
		i++;
	}
	k=i+1;
	while ((i<30) && (*(string+j) != '\0'))
	{
		if(*(buff+i) != *(string+j))
		{
			i=k;
			j=0;
			goto check_again;
		}
		i++;
		j++;
	}
	if(*(string+j) == '\0')		return k-1;
	else						return -1;
}
void buffer_wash()
{
	for (int j=0;j<30 ; j++)
		{
			buffer[j]=0;
		}
}
void send(char *p , char Enter_Selection)			//////////// 0-> char	1->No Enter	2->with Enter
{
	int i=0;
	
	do{
		if(*(p+i) == '#')	*(p+i)='"';
		while (!(UCSRA & (1<<UDRE)));
		UDR=*(p+i);
		i++;
	}while ((*(p+i) != '\0')&&(Enter_Selection!=0));
	if (Enter_Selection==2)
	{
		while (!(UCSRA & (1<<UDRE)));
 		UDR=0x0D;
 		while (!(UCSRA & (1<<UDRE)));
 		UDR=0x0A;
	}
}	
void esp_command(char *word)
{
	send(word,2);
}
void keys_check()									//////////// dar timer over flow seda zade shavad kheyli behtar mishavad
{
	if (room.key1_is_disconnect)
	{
		if (!(Inside_Pin & (1<<First_key)))
		{
			room.key1_is_disconnect=0;
			lamp_on(1);
			_delay_ms(10);
		}
	} 
	else
	{
		if (Inside_Pin & (1<<First_key))
		{
			room.key1_is_disconnect=1;
			lamp_off(1);
			room.top_lamp_on_with_mobile_command=0;
			_delay_ms(10);
		}
	}
	if (room.key2_is_disconnect)
	{
		if (!(Inside_Pin & (1<<Second_Key)))
		{
			room.key2_is_disconnect=0;
			lamp_on(2);
			_delay_ms(10);
		}
	} 
	else
	{
		if (Inside_Pin & (1<<Second_Key))
		{
			room.key2_is_disconnect=1;
			lamp_off(2);
			room.side_lamp_on_with_mobile_command=0;
			_delay_ms(10);
		}
	}
}
void auto_buttons_check()
{
	if (!(Inside_Pin & (1<<Auto_Enable_Button1)))
	{
		if (auto_button_can_action)
		{
			auto_toggle(1);
			timer0_start();
		}
	}
	if (!(Inside_Pin & (1<<Auto_Enable_Button2)))
	{
		if (auto_button_can_action)
		{
			auto_toggle(2);
			timer0_start();
		}
	}
}
void Light_Check()				//0:Light Check							1:PIR Check
{
	ADMUX |= ((1<<MUX0) | (1<<ADLAR));												// Voltage Reference : AREF , Internally Vref turned off    and     data in ADCH
	ADMUX &= ~((1<<REFS0) | (1<<REFS1) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1));		// Signal Reference : ADC1(PINC1) , ADLAR = 1	
	ADCSRA |= ((1<<ADEN) | (1<<ADIE) | (1<<ADPS0));										// ADC enable and ADC interrupt enable
	ADCSRA &= ~((1<<ADFR) | (1<<ADPS2) | (1<<ADPS1));									// ADC free running mode = off and  division factor = 2
	ADCSRA |= 1<<ADSC;	
}
void Day_Night_ADC_Values_Read_From_EEPROM()
{
	// TODO
	//day_night_adc_reference_value_eeprom = 10;
	//day_night_adc_reference_tolerance_eeprom = 11;
	//room.day_night_adc_reference_value = eeprom_read_byte(day_night_adc_reference_value_eeprom);
	//room.day_night_adc_reference_tolerance = eeprom_read_byte(day_night_adc_reference_tolerance_eeprom);
	room.day_night_adc_reference_value = 100;
	room.day_night_adc_reference_tolerance = 10;
}
void Timer2_Start()                                  ////////////// baraye tashkhis dadane nabudane kasi dar otag
{
	TCNT2 = 0;
	TCCR2 |= (1<<CS20 | 1<<CS21 | 1<<CS22);	// clkI/O/1024 (From prescaler)
}
void Timer2_Stop()
{
	TCNT2 = 0;
	TCCR2 &= ~(1<<CS20 | 1<<CS21 | 1<<CS22);	// clkI/O/1024 (From prescaler)
	timer2_count=0;
	timer2_second_counter=0;
}
void Timer2_time_end()
{
	TCNT2 = 0;
	TCCR2 &= ~(1<<CS20 | 1<<CS21 | 1<<CS22);	// clkI/O/1024 (From prescaler)
	timer2_count=0;
	timer2_second_counter=0;
	if (!(Inside_Pin & (1<<INT1_pir_motion)))
	{
		// khuruj anjam shode
		room.someone_in_room=0;
		if (room.lamp1_in_auto_mode)
		{
			if (!(room.top_lamp_on_with_mobile_command))
			{
				lamp_off(1);
			}
		}
		if (room.lamp2_in_auto_mode)
		{
			if (!(room.side_lamp_on_with_mobile_command))
			{
				lamp_off(2);
			}
		}	
	}
}
int main(void)
{
	room.need_for_rx_operation=0;
	room.INT0_Pir_enter_exit_last_situation=1;					// dar startup system, har do module dar sathe mantegiye 1 mibashand
	initialize();
	sei();
	Day_Night_ADC_Values_Read_From_EEPROM();
	// TODO read tolerance_time_for_no_person_in_room from eeprom
	room.top_lamp_on_with_mobile_command=0;
	room.side_lamp_on_with_mobile_command=0;
	room.tolerance_time_for_no_person_in_room=30;
	room.someone_in_room=0;
	room.Is_Dark=1;
	char counter=0;
    while(1)
    {
		keys_check();
		auto_buttons_check();
		_delay_ms(10);
		counter++;
		if (counter==100)
		{
			counter=0;
			Light_Check();
		}
		if (room.need_for_rx_operation)
		{
			room.need_for_rx_operation=0;
			rx_operation();
			sp=0;
		}
    }
}

ISR(TIMER0_OVF_vect)
{
	timer0_count++;
	if(timer0_count == 25)	// gablan 85 bud
	{
		timer0_count=0;
		timer0_stop();
	}
}
ISR(TIMER1_OVF_vect)
{
	timer1_count++;
	if(timer1_count==10)
	{
		room.lamp_on_off_timer_min--;
 		TCNT1H = 0x1C;
 		TCNT1L = 0xC0;
		timer1_count=0;	
	}
	if(room.lamp_on_off_timer_min==0)		
	{
		timer1_off();
		time_end();
	}	
}
ISR(TIMER2_OVF_vect)
{
	timer2_count++;
	if (timer2_count == 42)
	{
		timer2_count=0;
		timer2_second_counter++;
		if (timer2_second_counter > 5)
		{
			if (Inside_Pin & (1<<INT1_pir_motion))
			{
				Timer2_Stop();
			}
		}
		
		if (timer2_second_counter==room.tolerance_time_for_no_person_in_room)
		{
			timer2_second_counter=0;
			Timer2_time_end();
		}
	}
}
ISR (USART_RXC_vect)
{
	if (sp2 == 30)
	{
		for (int j = 0 ; j < 29 ; j++ )
		{
			buffer2[j] = buffer2[j+1];
		}
		sp2--;
	}
	buffer2[sp]=UDR;
	sp2++;
	if(buffer2[sp2-2]==0X0D)
	{
		if(buffer2[sp2-1]==0X0A)
		{
			//if ((buffer[sp-3]=='!') | (buffer[sp-5]=='$') | (buffer[sp-4]=='$') | (buffer[sp-6]=='$'))
			//{
				//sp=0;
			//}
			//else
			//{
				//rx_operation();
				for(char i=0;i<sp2;i++)
				{
					buffer[i]=buffer2[i];
				}
				sp=sp2;
				room.need_for_rx_operation=1;
				sp2=0;		
			//}

		}
	}
}
ISR(INT0_vect)
{
	if ((!(Inside_Pin & (1<<INT0_Pir_enter_exit))) != (!(room.INT0_Pir_enter_exit_last_situation)))  //if((Inside_Pin & (1<<INT0_Pir_enter_exit)) logical xor (room.INT0_Pir_enter_exit_last_situation))	//baraye jelo giri az amal kardane pulse ke az taghire jarayane barg tolid mishavad
	{
		if(Inside_Pin & (1<<INT0_Pir_enter_exit))
		{
			room.INT0_Pir_enter_exit_last_situation=1;
			// ehtemalan vurud ya khuruji surat gereft
			if (!(Inside_Pin & (1<<INT1_pir_motion)))
			{
				//vurud anjam shode
				Timer2_Stop();
				room.someone_in_room=1;
				if (room.Is_Dark)
				{
					lamp_auto_on();
				}
			}
			else
			{
				// ehtemalan khuruj anjam shode va bayad check shavad
				Timer2_Start();
			}
		}
		else
		{
			room.INT0_Pir_enter_exit_last_situation=0;
			if (Inside_Pin & (1<<INT1_pir_motion))
			{
				
			}
			else
			{
				if (TCCR2 & (1<<CS20 | 1<<CS21 | 1<<CS22))
				{
					// ehtemalan khuruj anjam shode va dar hale check shodan ast
				}
				else
				{
					// ehtemalan khuruj anjam shode va bayad check shavad
					Timer2_Start();
				}
			}
		}
	}
}
ISR(INT1_vect)
{
	//There are somebody in the room
	if (Inside_Pin & (1<<INT1_pir_motion))									//baraye jelo giri az amal kardane pulse ke az taghire jarayane barg tolid mishavad
	{
		room.someone_in_room=1;
		Timer2_Stop();
		if (room.Is_Dark)
		{
			lamp_auto_on();
		}
	}	
}
ISR(ADC_vect)
{									
	//char v = ADCH;
	//if ((v > (room.day_night_adc_reference_value + room.day_night_adc_reference_tolerance)) && (room.Is_Dark))
	//{
		////Ruz shod
		//if (!(Outside_Port & (1<<Relay1 | 1<<Relay2)))
		//{
			//room.Is_Dark = 0;
		//}
		////send("ruz shod",2);
		////room.Is_Dark = 0;
		////if ((room.key1_is_disconnect) && (room.lamp1_in_auto_mode))
		////{
			////lamp_off(1);
		////}
		////if ((room.key2_is_disconnect) && (room.lamp2_in_auto_mode))
		////{
			////lamp_off(2);
		////}
	//}
	//if ((v < (room.day_night_adc_reference_value - room.day_night_adc_reference_tolerance)) && (!(room.Is_Dark)))
	//{
		////shab shod
		//room.Is_Dark = 1;
		//if (room.someone_in_room)
		//{
			////There are somebody in the room
			//if (room.Is_Dark)
			//{
				//lamp_auto_on();
			//}
		//}
	//}	
}