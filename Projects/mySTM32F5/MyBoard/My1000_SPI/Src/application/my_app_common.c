/*! ----------------------------------------------------------------------------
 *  @file    my_app_common.c
 *  @brief   DecaWave application level common instance functions
 *
 * @author Kai
 */
#include "compiler.h"
#include "port.h"
#include "deca_device_api.h"
#include "deca_spi.h"


#include "my_ranging.h"

extern const uint16 rfDelays[2];
extern const tx_struct txSpectrumConfig[8];
extern uint8 result_read_flag;

static status_info_t set_info;
static device_info DeviceList[MAX_DEV_NUM];
static uint8 SEQ_blink=0;
static uint8 SEQ_range=0;
static uint8 blink_msg[BLINK_LEN];
static uint8 msg_recieve[RANGE_FRAME_LEN_BYTES];
static uint8 msg_send[RANGE_FRAME_LEN_BYTES];
static	uint8 rangeDis[20];
static uint64 t64;



void tag_loop(void);
void anchor_loop(void);

void DeviceListInit(void);
int add_Device(uint8 *shortADD, uint8 *fullADD);

//////////////////////////prepare and send msg////////////////////////
void prepare_msg_header(uint8 SEQ);
void prepare_blink_msg(void);
void prepare_range_init_msg(event_data_t newevent);
int prepare_poll_msg(void );
int prepare_range_msg(void );
void prepare_final_msg(int Dev_idx);
void prepare_p_ack_msg(void);

void send_blink(void);
void sendRangeInit(event_data_t newevent);
void sendPollAck(uint32 Delay);
void sendPoll(void);
void sendRange(void);
void sendFinal(int Dev_idx);
////////////////////////check after recieve///////////////////////////
int checkRecievedPoll(event_data_t event,uint8 *plan_time);
void save_poll_timestamp_AnndSendPollACK(event_data_t event,uint8* plantime);
int decode_range_AndSolveDis(event_data_t event);

void	save_PollAck_Timestamp(void);


void process_event(event_data_t event);
void put_event(event_data_t event);	
void check_event(void);
void clearevents(void);
void checkDeviceStatus(void);


void computeDis(int Dev_idx);
void correctTimestamp(uint8* timestamp);


void instance_txcallback(const dwt_cb_data_t *txd)
{


	if(set_info.mode==ANCHOR){
		switch(set_info.ranging_step)
		{
			case STEP_POLL_ACK_PULLED:
					set_info.ranging_step=STEP_POLL_ACK_SENT;
					dwt_readtxtimestamp(DeviceList[set_info.last_send_tag_idx].timePollAckSentBytes);
					DeviceList[set_info.last_send_tag_idx].timePollAckSent=	(uint64) DeviceList[set_info.last_send_tag_idx].timePollAckSentBytes[0]
																															+((uint64) DeviceList[set_info.last_send_tag_idx].timePollAckSentBytes[1]<<8)
																															+((uint64) DeviceList[set_info.last_send_tag_idx].timePollAckSentBytes[2]<<16)
																															+((uint64) DeviceList[set_info.last_send_tag_idx].timePollAckSentBytes[3]<<24)
																															+((uint64) DeviceList[set_info.last_send_tag_idx].timePollAckSentBytes[4]<<32);
					writetoLCD( 1, 1, "N"); //send some data
			break;
			case STEP_FINAL_PULLED:
					set_info.ranging_step=STEP_IDLE;
					writetoLCD( 1, 1, "J"); //send some data
			break;
			case STEP_RANGE_INIT_SENDING:
					set_info.ranging_step=STEP_IDLE;
					writetoLCD( 1, 1, "G"); //send some data
			break;
		}
		instancerxon(0,0);
		
	}else{
		uint8 txTimeStamp[5] = {0, 0, 0, 0, 0};

		switch(set_info.ranging_step)
		{
			case STEP_BLINK_SENDING:
				writetoLCD( 1, 1, "A"); //send some data
				set_info.ranging_step=STEP_IDLE;
			break;
			case STEP_POLL_SENDING:
				writetoLCD( 1, 1, "H"); //send some data
				set_info.ranging_step=STEP_POLL_SENT;
			  
				dwt_readtxtimestamp(&txTimeStamp[0]) ;
				memcpy(set_info.timePollSentBytes,txTimeStamp,5);
				set_info.timePollSent=   (uint64)txTimeStamp[0] 
															+ ((uint64)txTimeStamp[1] << 8) 
															+ ((uint64)txTimeStamp[2] << 16) 
															+ ((uint64)txTimeStamp[3] << 24)
															+ ((uint64)txTimeStamp[4] << 32);
			break;
			case STEP_RANGE_PULLED:
				writetoLCD( 1, 1, "M"); //send some data

//			dwt_readtxtimestamp(&txTimeStamp[0]) ;
//			sprintf((char*)rangeDis,"%2x",txTimeStamp[4]);
//			sprintf((char*)rangeDis+2,"%2x",txTimeStamp[3]);
//			sprintf((char*)rangeDis+4,"%2x",txTimeStamp[2]);
//			sprintf((char*)rangeDis+6,"%2x",txTimeStamp[1]);
//			sprintf((char*)rangeDis+8,"%2x",txTimeStamp[0]);
//			writetoLCD( 10, 1,rangeDis );
			
				set_info.ranging_step=STEP_RANGE_SENT;
			break;
		}
		instancerxon(0,0);
		
		
	}
	

}

void instance_rxtimeoutcallback(const dwt_cb_data_t *rxd)
{
	writetoLCD( 1, 1, "D");
	instancerxon(0,0);
}

void instance_rxerrorcallback(const dwt_cb_data_t *rxd)
{
	writetoLCD( 1, 1, "C");
	instancerxon(0,0);

}

void instance_rxgoodcallback(const dwt_cb_data_t *rxd)
{
   
	if(set_info.mode==ANCHOR){

		switch(rxd->fctrl[0])
		{
			
			//if get a blink msg
			case FC_1_BLINK:
				
				dwt_readrxdata((uint8 *)&blink_msg[0], rxd->datalength, 0);  // Read Data Frame

#if USING_64BIT_ADDR==0			
				uint8 shortADD_recieve[2];
				memcpy(shortADD_recieve,blink_msg+10,2);
				uint8 fullADD_recieve[8];
				memcpy(fullADD_recieve,blink_msg+2,8);
				if(add_Device(shortADD_recieve, fullADD_recieve)==1){
					event_data_t dw_event;
					dw_event.databuf[0]=blink_msg[1];
					dw_event.type=SendRangeInit;
					put_event(dw_event);
				}
				writetoLCD( 1, 1, "R"); //send some data
#endif
//			writetoLCD( rxd->datalength, 1,blink_msg ); //send some data

			break;
			
			case FC_1:
				
				
#if USING_64BIT_ADDR==0
			dwt_readrxdata(msg_recieve, rxd->datalength, 0);  // Read Data Frame
				if(msg_recieve[5+2*ADD_LEN]==POLL){
					 writetoLCD( 1, 1, "I"); //send some data
					
					event_data_t dw_event;
					memcpy(&dw_event.databuf[0],msg_recieve, rxd->datalength);  // Read Data Frame					
					dw_event.dataLength=rxd->datalength;
					dw_event.type=SendPollAck;
					put_event(dw_event);
					
//					  uint8 plan_time[2];
//						if(checkRecievedPoll(rxd->datalength,plan_time)==1){
//							 save_poll_timestamp_AnndSendPollACK(plan_time);
//						}
				}else if(msg_recieve[5+2*ADD_LEN]==RANGE){
				  writetoLCD( 1, 1, "P"); //send some data
					event_data_t dw_event;
					memcpy(&dw_event.databuf[0],msg_recieve, rxd->datalength);  // Read Data Frame					
					dw_event.dataLength=rxd->datalength;
					dw_event.type=SendFinal;
					put_event(dw_event);
					


				}
#endif			
			break;
		}
		set_info.recieve_flag++;
		instancerxon( 0, 0);
	}else{
		switch(rxd->fctrl[0])
		{
			case FC_1:
				dwt_readrxdata((uint8 *)&msg_recieve[0], rxd->datalength, 0);  // Read Data Frame
#if USING_64BIT_ADDR==0			
			   //if get a range init msg
				if(msg_recieve[5+2*ADD_LEN]==RANGING_INIT){

					uint8 shortADD_recieve[2];
					memcpy(shortADD_recieve,msg_recieve+10,2);
					uint8 fullADD_recieve[8];
					memcpy(fullADD_recieve,msg_recieve+12,8);
					if(add_Device(shortADD_recieve, fullADD_recieve)==1){
						writetoLCD( 1, 1,"B" ); //send some data
					}
					
					instancerxon( 0, 0);

				}else if(msg_recieve[9]==POLL_ACK){
					
					save_PollAck_Timestamp();
					set_info.poll_replied_num++;
					writetoLCD( 1, 1,"E" );
					instancerxon( 0, 0);
					
				}else if(msg_recieve[9]==FINAL){
					writetoLCD( 1, 1,"K" );
					instancerxon( 0, 0);
					
					float a;
					memcpy(&a,msg_recieve+12,4);
					sprintf((char*)rangeDis,"%-10.3f",a);
					writetoLCD( 10, 1,rangeDis );
				}
				
#endif
			break;
		}
	}
	
			


}

#define TXCFG_ADDRESS  (0x10) // OTP address at which the TX power calibration value is stored
							  // The TX power configuration read from OTP (6 channels consecutively with PRF16 then 64, e.g. Ch 1 PRF 16 is index 0 and PRF 64 index 1)
#define ANTDLY_ADDRESS (0x1C) // OTP address at which the antenna delay calibration value is stored
extern const uint8 chan_idx[];

// -------------------------------------------------------------------------------------------------------------------
//
// function to allow application configuration be passed into instance and affect underlying device opetation
//
void instance_config(InitConfig_t *config)
{

    uint32 power = 0;

    set_info.txAntennaDelay = 0;
    set_info.rxAntennaDelay = 0;

    set_info.configData.chan = config->channelNumber ;
    set_info.configData.rxCode =  config->preambleCode ;
    set_info.configData.txCode = config->preambleCode ;
    set_info.configData.prf = config->pulseRepFreq ;
    set_info.configData.dataRate = config->dataRate ;
    set_info.configData.txPreambLength = config->preambleLen ;
    set_info.configData.rxPAC = config->pacSize ;
    set_info.configData.nsSFD = config->nsSFD ;
    set_info.configData.phrMode = DWT_PHRMODE_STD ;
    set_info.configData.sfdTO = config->sfdTO;

    //configure the channel parameters
    dwt_configure(&set_info.configData) ;

    //NOTE: For EVK1000 the OTP stores calibrated antenna and TX power values for configuration modes 3 and 5,

    //check if to use the antenna delay calibration values as read from the OTP
    if(dwt_otprevision() <= 1) //in revision 0, 1 of EVB1000/EVK1000
    {
    	
    	uint32 otpPower[12];

    	//MUST change the SPI to < 3MHz as the dwt_otpread will change to XTAL clock
    	port_set_dw1000_slowrate(); //reduce SPI to < 3MHz
#if USE_OTP_ANTD==1
			uint32 antennaDelay;
    	dwt_otpread(ANTDLY_ADDRESS, &antennaDelay, 1);

    	set_info.txAntennaDelay = ((antennaDelay >> (16*(config->pulseRepFreq - DWT_PRF_16M))) & 0xFFFF) >> 1;
#else
			set_info.txAntennaDelay = INPUT_ANT_DELAY;
#endif
    	set_info.rxAntennaDelay = set_info.txAntennaDelay ;

    	//read any data from the OTP for the TX power
    	dwt_otpread(TXCFG_ADDRESS, otpPower, 12);

    	port_set_dw1000_fastrate(); //increase SPI to max

        power = otpPower[(config->pulseRepFreq - DWT_PRF_16M) + (chan_idx[set_info.configData.chan] * 2)];
    }

    // if nothing was actually programmed then set a reasonable value anyway
    if(set_info.txAntennaDelay == 0)//otherwise a default values should be used
    {
    	set_info.rxAntennaDelay = set_info.txAntennaDelay = rfDelays[config->pulseRepFreq - DWT_PRF_16M];
    }

    // -------------------------------------------------------------------------------------------------------------------
    // set the antenna delay, we assume that the RX is the same as TX.
    dwt_setrxantennadelay(set_info.txAntennaDelay);
    dwt_settxantennadelay(set_info.txAntennaDelay);

    if((power == 0x0) || (power == 0xFFFFFFFF)) //if there are no calibrated values... need to use defaults
    {
        power = txSpectrumConfig[config->channelNumber].txPwr[config->pulseRepFreq- DWT_PRF_16M];
    }

    set_info.configTX.power = power;
    set_info.configTX.PGdly = txSpectrumConfig[config->channelNumber].PGdelay ;

    //configure the tx spectrum parameters (power and PG delay)
    dwt_configuretxrf(&set_info.configTX);

    set_info.antennaDelayChanged = 0;

    if(config->preambleLen == DWT_PLEN_64) //if preamble length is 64
	{
    	port_set_dw1000_slowrate(); //reduce SPI to < 3MHz

		dwt_loadopsettabfromotp(0);

		port_set_dw1000_slowrate(); //increase SPI to max
    }



// my added initial parameters		
		set_info.Time_next_blink=portGetTickCnt();
		set_info.Time_next_Poll=portGetTickCnt();
		set_info.Time_next_Dev_Check=portGetTickCnt();
	  set_info.Time_next_Ranging=portGetTickCnt()+100000;
		set_info.AppState=TA_INIT;
		
		set_info.delayedReplyTime=DEFAULT_REPLY_DELAY_TIME;
		set_info.active_device_num=0;
		set_info.ranging_step=STEP_IDLE;
		set_info.recieve_flag=0;
}

// -------------------------------------------------------------------------------------------------------------------
// function to initialise instance structures
//
// Returns 0 on success and -1 on error
int instance_init(void)
{

    int result;
    //uint16 temp = 0;

    set_info.mode =  ANCHOR;                                // assume listener,

    // Reset the IC (might be needed if not getting here from POWER ON)
    dwt_softreset();

	//we can enable any configuration loding from OTP/ROM on initialisation
    result = dwt_initialise(DWT_LOADUCODE) ;

    //this is platform dependent - only program if DW EVK/EVB
//    dwt_setleds(3) ; //configure the GPIOs which control the leds on EVBs

    if (DWT_SUCCESS != result)
    {
        return (-1) ;   // device initialise has failed
    }

    //enable TX, RX states on GPIOs 6 and 5
//    dwt_setlnapamode(1,1); //Ӳ������û�ӣ�

 //   instanceclearcounts() ;

    set_info.sleepingEabled = 1;



    clearevents();

    dwt_geteui(set_info.eui64);

    set_info.clockOffset = 0;
    return 0 ;
}
// -------------------------------------------------------------------------------------------------------------------
// function to set the tag sleep time (in ms)
//
void instancesettagsleepdelay(int sleepdelay, int blinksleepdelay) //sleep in ms
{
//    set_info.tagSleepTime_ms = sleepdelay ;
    set_info.tagBlinkSleepTime_ms = blinksleepdelay ;
}

// -------------------------------------------------------------------------------------------------------------------
// Functions
// -------------------------------------------------------------------------------------------------------------------
/* @fn 	  instance_get_local_structure_ptr
 * @brief function to return the pointer to local instance data structure
 * */
status_info_t* instance_get_local_structure_ptr(void)
{
	return &set_info;
}
//////////////////////////////////////////////////
///////////////////Main Loop /////////////////////
//////////////////////////////////////////////////

void tag_loop(void){
	if(set_info.Time_next_Poll<portGetTickCnt()){
		checkDeviceStatus();
		if(set_info.active_device_num>0){

			sendPoll();
			uint32 current=portGetTickCnt();
			set_info.Time_next_Ranging=current+2*set_info.active_device_num*set_info.delayedReplyTime/1000;
			set_info.poll_replied_num=0;
			if(set_info.Time_next_blink-current<20){
			  set_info.Time_next_blink=current+20;
			}
		}
		set_info.Time_next_Poll=portGetTickCnt()+POLL_SLEEP_DELAY;
		
	}
//	if(set_info.Time_next_Ranging<portGetTickCnt() && set_info.active_device_num>0){
	if((set_info.poll_replied_num>0 &&(set_info.poll_replied_num==set_info.active_device_num |set_info.Time_next_Ranging>portGetTickCnt()) )){
		sendRange();
	  set_info.poll_replied_num=0;
		
		uint32 current=portGetTickCnt();
//		set_info.Time_next_blink=current+100;
		set_info.Time_next_Ranging=portGetTickCnt()+100000;
	}
	if(set_info.Time_next_blink<portGetTickCnt()){
		
		set_info.Time_next_blink=portGetTickCnt()+set_info.tagBlinkSleepTime_ms;
		if(result_read_flag==0){
			result_read_flag=1;
			sendresult();
		}else{
			send_blink();
		}
		
	}
	
	
}

void anchor_loop(void){
	
	
	
	if(set_info.Time_next_Dev_Check<portGetTickCnt()){
		checkDeviceStatus();
		
		if(set_info.recieve_flag ==0){
			instancerxon(0,0);
			set_info.recieve_flag=0;
		}
		
		set_info.Time_next_Dev_Check=portGetTickCnt()+CHECK_DEV_INTERVAL;
	}
	check_event();
	
	
}



void send_blink(){
				dwt_forcetrxoff();
				prepare_blink_msg();
	        /* Write frame data to DW1000 and prepare transmission. See NOTE 4 below.*/
        dwt_writetxdata(BLINK_LEN, (uint8 *) blink_msg, 0); /* Zero offset in TX buffer. */
        dwt_writetxfctrl(BLINK_LEN, 0, 0); /* Zero offset in TX buffer, no ranging. */
				set_info.ranging_step=STEP_BLINK_SENDING;
        /* Start transmission. */
        dwt_starttx(DWT_START_TX_IMMEDIATE );
}
void prepare_blink_msg(){
	blink_msg[0]=FC_1_BLINK;
	blink_msg[1]=SEQ_blink;
	SEQ_blink++;
	memcpy(&blink_msg[2],set_info.eui64,8);
	memcpy(&blink_msg[10],set_info.ShortAdd,2);
}

void sendRangeInit(event_data_t newevent){
				dwt_forcetrxoff();
				prepare_range_init_msg(newevent);
	        /* Write frame data to DW1000 and prepare transmission. See NOTE 4 below.*/
        dwt_writetxdata(RNG_INIT_FRAME_LEN_BYTES, (uint8 *) msg_send, 0); /* Zero offset in TX buffer. */
        dwt_writetxfctrl(RNG_INIT_FRAME_LEN_BYTES, 0, 0); /* Zero offset in TX buffer, no ranging. */
			  set_info.ranging_step=STEP_RANGE_INIT_SENDING;
        /* Start transmission. */
        dwt_starttx(DWT_START_TX_IMMEDIATE );    //recieve always on
}
void prepare_range_init_msg(event_data_t newevent){
	prepare_msg_header(newevent.databuf[0]);
	msg_send[5+2*ADD_LEN]=RANGING_INIT;
#if USING_64BIT_ADDR==0
	memcpy(&msg_send[10],set_info.ShortAdd,2);
	memcpy(&msg_send[12],set_info.eui64,8);
#endif
}

void sendPoll(void){
	if(set_info.active_device_num>0){
		dwt_forcetrxoff();
	
		int msg_len=prepare_poll_msg();
		
		dwt_writetxdata( msg_len+2, (uint8 *) msg_send, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(msg_len+2, 0, 0); /* Zero offset in TX buffer, no ranging. */
        /* Start transmission. */
		set_info.ranging_step=STEP_POLL_SENDING;
    dwt_starttx(DWT_START_TX_IMMEDIATE );    //recieve always on
	}
}
void sendPollAck(uint32 Delay){
	dwt_forcetrxoff();
	
	prepare_p_ack_msg();
	dwt_writetxdata( RESP_FRAME_LEN_BYTES, (uint8 *) msg_send, 0); /* Zero offset in TX buffer. */
  dwt_writetxfctrl(RESP_FRAME_LEN_BYTES, 0, 0); /* Zero offset in TX buffer, no ranging. */
	dwt_setdelayedtrxtime(Delay);
	set_info.ranging_step=STEP_POLL_ACK_PULLED;
	dwt_starttx(DWT_START_TX_DELAYED );
}
void sendRange(void){
	dwt_forcetrxoff();
	int range_len =prepare_range_msg();
	
	uint64 RangeDelay;
  uint32 timeDelayToSet;
	uint8 SystimeBytes[5];
	dwt_readsystime(SystimeBytes);
	RangeDelay= (uint64)SystimeBytes[0]
							+((uint64)SystimeBytes[1]<<8)
							+((uint64)SystimeBytes[2]<<16)
							+((uint64)SystimeBytes[3]<<24)
							+((uint64)SystimeBytes[4]<<32);
	RangeDelay=RangeDelay+  RANGE_SEND_DELAY;
	timeDelayToSet=(RangeDelay>>8) &0xFFFFFFFE;
	RangeDelay=RangeDelay & 0xFFFFFFFFFFFFFE00;
	RangeDelay=RangeDelay+set_info.txAntennaDelay;
	set_info.timeRangeSentBytes[0]=RangeDelay &0xFF;
	set_info.timeRangeSentBytes[1]= (RangeDelay>>8) &0xFF;
	set_info.timeRangeSentBytes[2]= (RangeDelay>>16) &0xFF;
	set_info.timeRangeSentBytes[3]= (RangeDelay>>24) &0xFF;
	set_info.timeRangeSentBytes[4]= (RangeDelay>>32) &0xFF;
	
	
	memcpy(&msg_send[6+2*ADD_LEN+5],set_info.timeRangeSentBytes,5);

	dwt_writetxdata( range_len+2, (uint8 *) msg_send, 0); /* Zero offset in TX buffer. */
  dwt_writetxfctrl(range_len+2, 0, 0); /* Zero offset in TX buffer, no ranging. */
	dwt_setdelayedtrxtime(timeDelayToSet);
//	dwt_writetodevice(0x0A,0x00,5,set_info.timeRangeSentBytes);
	set_info.ranging_step=STEP_RANGE_PULLED;
	if(dwt_starttx(DWT_START_TX_DELAYED)==DWT_SUCCESS){

	}
	
}
void sendFinal(int Dev_idx){
	dwt_forcetrxoff();
	prepare_final_msg(Dev_idx);
	
	dwt_writetxdata( FINAL_FRAME_LEN_BYTES, (uint8 *) msg_send, 0); /* Zero offset in TX buffer. */
  dwt_writetxfctrl(FINAL_FRAME_LEN_BYTES, 0, 0); /* Zero offset in TX buffer, no ranging. */
	DeviceList[Dev_idx].timeFinalSent=DeviceList[Dev_idx].timeRangeReceived+DeviceList[Dev_idx].dertatime;
	DeviceList[Dev_idx].timeDelayToSet=(DeviceList[Dev_idx].timeFinalSent>>8)& 0xFFFFFFFF;
	dwt_setdelayedtrxtime(DeviceList[Dev_idx].timeDelayToSet);
	set_info.ranging_step=STEP_FINAL_PULLED;
	dwt_starttx(DWT_START_TX_DELAYED );
	
}
void prepare_final_msg(int Dev_idx){
	prepare_msg_header(SEQ_range);
	msg_send[5+2*ADD_LEN]=FINAL;
#if USING_64BIT_ADDR==0
	memcpy(&msg_send[10],DeviceList[Dev_idx]._shortAddress,2);
	memcpy(&msg_send[12],&(DeviceList[Dev_idx]._range),4);
	
#endif
}

int checkRecievedPoll(event_data_t event,uint8 *plan_time){
#if USING_64BIT_ADDR==0
	if(event.databuf[5]==BROADCAST_1 && event.databuf[6]==BROADCAST_2 ){
			int loadcounter=0;
			while(event.dataLength-FRAME_HEADER_LEN-loadcounter*(ADD_LEN+2)>0){
				if(memcmp(&event.databuf[FRAME_HEADER_LEN+loadcounter*(ADD_LEN+2)],set_info.ShortAdd,2)==0){
					memcpy(plan_time,&event.databuf[FRAME_HEADER_LEN+loadcounter*(ADD_LEN+2)+ADD_LEN],2);
					return 1;
				}
				loadcounter++;
			}
			
	}
#endif
return 0;
}
int checkRecievedRange(event_data_t event,int Dev_idx){
	#if USING_64BIT_ADDR==0
	if(event.databuf[5]==BROADCAST_1 && event.databuf[6]==BROADCAST_2 ){
			int loadcounter=0;
			while(event.dataLength-FRAME_HEADER_LEN-10-loadcounter*(ADD_LEN+5)>0){
				if(memcmp(&event.databuf[FRAME_HEADER_LEN+10+loadcounter*(ADD_LEN+5)],set_info.ShortAdd,2)==0){
					memcpy(&DeviceList[Dev_idx].timePollSentBytes[0],&event.databuf[FRAME_HEADER_LEN],5);
					memcpy(&DeviceList[Dev_idx].timeRangeSentBytes[0],&event.databuf[FRAME_HEADER_LEN+5],5);
					memcpy(&DeviceList[Dev_idx].timePollAckReceivedBytes[0],&event.databuf[FRAME_HEADER_LEN+10+loadcounter*(ADD_LEN+5)+2],5);
					
					DeviceList[Dev_idx].timePollSent= (uint64)DeviceList[Dev_idx].timePollSentBytes[0]
																					+((uint64)DeviceList[Dev_idx].timePollSentBytes[1]<<8)
																					+((uint64)DeviceList[Dev_idx].timePollSentBytes[2]<<16)
															  					+((uint64)DeviceList[Dev_idx].timePollSentBytes[3]<<24)
																					+((uint64)DeviceList[Dev_idx].timePollSentBytes[4]<<32);
					
					DeviceList[Dev_idx].timeRangeSent=(uint64)DeviceList[Dev_idx].timeRangeSentBytes[0]
																					+((uint64)DeviceList[Dev_idx].timeRangeSentBytes[1]<<8)
																					+((uint64)DeviceList[Dev_idx].timeRangeSentBytes[2]<<16)
															  					+((uint64)DeviceList[Dev_idx].timeRangeSentBytes[3]<<24)
																					+((uint64)DeviceList[Dev_idx].timeRangeSentBytes[4]<<32);
					
					DeviceList[Dev_idx].timePollAckReceived=(uint64)DeviceList[Dev_idx].timePollAckReceivedBytes[0]
																								+((uint64)DeviceList[Dev_idx].timePollAckReceivedBytes[1]<<8)
																								+((uint64)DeviceList[Dev_idx].timePollAckReceivedBytes[2]<<16)
																								+((uint64)DeviceList[Dev_idx].timePollAckReceivedBytes[3]<<24)
																								+((uint64)DeviceList[Dev_idx].timePollAckReceivedBytes[4]<<32);

					
					
					return 1; //Anchor self in Tag's list
				}
				loadcounter++;
			}
			
	}
#endif
return 0;
}

int prepare_poll_msg(void ){
	prepare_msg_header(SEQ_range);
	msg_send[5+2*ADD_LEN]=POLL;
	uint8 loadcounter=0;
	uint16 DelayTime;
	uint8 DelayTimeU8[2];
	for(uint8 i=0;i<MAX_DEV_NUM;i++){
		if(DeviceList[i]._isactivate==1){
#if USING_64BIT_ADDR==0
			memcpy(&msg_send[6+2*ADD_LEN+loadcounter*(ADD_LEN+2)],DeviceList[i]._shortAddress,ADD_LEN);
#endif
			DelayTime=set_info.delayedReplyTime*(2*loadcounter+1);
			DelayTimeU8[0]=DelayTime &0xFF;
			DelayTimeU8[1]=(DelayTime>>8) &0xFF;
			memcpy(&msg_send[6+2*ADD_LEN+ADD_LEN+loadcounter*(ADD_LEN+2)],DelayTimeU8,2);
			loadcounter++;
		}
	}
	return 6+2*ADD_LEN+loadcounter*(ADD_LEN+2);
	
}
void prepare_p_ack_msg(void){
	prepare_msg_header(SEQ_range);
	msg_send[5+2*ADD_LEN]=POLL_ACK;
	
}

int prepare_range_msg(void){
	prepare_msg_header(SEQ_range);
	uint8 loadcounter=0;
	msg_send[5+2*ADD_LEN]=RANGE;
	memcpy(&msg_send[6+2*ADD_LEN],set_info.timePollSentBytes,5);
//	memcpy(&msg_send[6+2*ADD_LEN+5],set_info.timeRangeSentBytes,5);    //should set after 
	for(uint8 i=0;i<MAX_DEV_NUM;i++){
		if(DeviceList[i]._isactivate==1){
#if USING_64BIT_ADDR==0
			memcpy(&msg_send[6+2*ADD_LEN+10+loadcounter*(ADD_LEN+5)],DeviceList[i]._shortAddress,ADD_LEN);
#endif
			memcpy(&msg_send[6+2*ADD_LEN+10+loadcounter*(ADD_LEN+5)+ADD_LEN],DeviceList[i].timePollAckReceivedBytes,5);
			loadcounter++;
		}
	}
	return 6+2*ADD_LEN+10+loadcounter*(ADD_LEN+5);
}

void prepare_msg_header(uint8 SEQ){
	msg_send[0]=FC_1;
	msg_send[1]=FC_2;
	msg_send[2]=SEQ_range;
	msg_send[3]=PAN_ID_1;
	msg_send[4]=PAN_ID_2;
#if USING_64BIT_ADDR==0
	msg_send[5]=BROADCAST_1;
	msg_send[6]=BROADCAST_2;
	memcpy(&msg_send[7],set_info.ShortAdd,2);
	
	
#endif
}


////////////////////////////////////////////////////
////////////  Handle Device List ///////////////////
////////////////////////////////////////////////////


void DeviceListInit(void){
	for(int i=0;i<MAX_DEV_NUM;i++){
		DeviceList[i]._isactivate=0;
	}
	set_info.active_device_num=0;
}

void removeDeviceByShortADD(uint8 *shortAdd){
	for(int i=0;i<MAX_DEV_NUM;i++){
		if(DeviceList[i]._isactivate==1 && *shortAdd==DeviceList[i]._shortAddress[0] && *(shortAdd+1)==DeviceList[i]._shortAddress[1]){
				DeviceList[i]._isactivate=0;
			  set_info.active_device_num--;
		}
	}
}

void checkDeviceStatus(void){
	uint32 current= (uint32)portGetTickCnt();
	for(int i=0;i<MAX_DEV_NUM;i++){
		if(DeviceList[i]._isactivate==1 && current-DeviceList[i].lastActionTime>DEVICE_TIMEOUT_PERIOD){
				DeviceList[i]._isactivate=0;
			  set_info.active_device_num--;
		}
	}
}
int add_Device(uint8 *shortADD, uint8 *fullADD){
	uint32 current= (uint32)portGetTickCnt();
	for(int i=0;i<MAX_DEV_NUM;i++){
#if USING_64BIT_ADDR==0
		if(DeviceList[i]._isactivate==1 && DeviceList[i]._shortAddress[0]==*(shortADD) &&DeviceList[i]._shortAddress[1]==*(shortADD+1) ){
//			DeviceList[i].lastActionTime=current;
			return 0;
		}
#endif
	}
	if(set_info.active_device_num==MAX_DEV_NUM){
		return 0;
	}
	
	for(int i=0;i<MAX_DEV_NUM;i++){
		if(DeviceList[i]._isactivate==0){
			memcpy(DeviceList[i]._shortAddress, shortADD,2);
			memcpy(DeviceList[i]._ownAddress, fullADD,2);
			DeviceList[i]._isactivate=1;
			DeviceList[i].lastActionTime=current;
			set_info.active_device_num++;
			return 1;
		}
	}
	return 0;
}

void save_poll_timestamp_AnndSendPollACK(event_data_t event, uint8* plantime){
	for(int i=0;i<MAX_DEV_NUM;i++){
#if USING_64BIT_ADDR==0
		if(DeviceList[i]._isactivate==1 && DeviceList[i]._shortAddress[0]==event.databuf[7] && DeviceList[i]._shortAddress[1]==event.databuf[8] ){
			DeviceList[i].lastActionTime=portGetTickCnt();
//			dwt_readrxtimestamp(&(DeviceList[i].timePollReceivedBytes[0]));
		  correctTimestamp(&(DeviceList[i].timePollReceivedBytes[0]));
			DeviceList[i].timePollReceived=			(uint64)DeviceList[i].timePollReceivedBytes[0]
																				+((uint64)DeviceList[i].timePollReceivedBytes[1]<<8)
																				+((uint64)DeviceList[i].timePollReceivedBytes[2]<<16)
																				+((uint64)DeviceList[i].timePollReceivedBytes[3]<<24)
																				+((uint64)DeviceList[i].timePollReceivedBytes[4]<<32);
			DeviceList[i].dertatime=(uint64) plantime[0]+((uint64) plantime[0]<<8);
			DeviceList[i].dertatime=DeviceList[i].dertatime*TIME_RES_INV;
			DeviceList[i].timePollAckSent=DeviceList[i].timePollReceived+DeviceList[i].dertatime;
			DeviceList[i].timeDelayToSet=(DeviceList[i].timePollAckSent>>8)& 0xFFFFFFFF;

			set_info.last_send_tag_idx=i;
			sendPollAck(DeviceList[i].timeDelayToSet);
			return;
		}
#endif
	}
}

int decode_range_AndSolveDis(event_data_t event){
		for(int i=0;i<MAX_DEV_NUM;i++){
#if USING_64BIT_ADDR==0
			if(DeviceList[i]._isactivate==1 && DeviceList[i]._shortAddress[0]==event.databuf[7] && DeviceList[i]._shortAddress[1]==event.databuf[8] )
			{
				
//				dwt_readrxtimestamp(&(DeviceList[i].timeRangeReceivedBytes[0]));
				correctTimestamp(&(DeviceList[i].timePollReceivedBytes[0]));
				DeviceList[i].timeRangeReceived= (uint64)DeviceList[i].timeRangeReceivedBytes[0]
																			+((uint64) DeviceList[i].timeRangeReceivedBytes[1]<<8)
																			+((uint64) DeviceList[i].timeRangeReceivedBytes[2]<<16)
																			+((uint64) DeviceList[i].timeRangeReceivedBytes[3]<<24)
																			+((uint64) DeviceList[i].timeRangeReceivedBytes[4]<<32);
				if (checkRecievedRange(event,i)==1){
					DeviceList[i].lastActionTime=portGetTickCnt();
					computeDis(i);
					set_info.last_send_tag_idx=i;
					return 1;
				}
			}
#endif
	}
		return 0;
}
////////////////////////////////////////////////////
////////////  Handle TimeStamp /////////////////////////
////////////////////////////////////////////////////
void	save_PollAck_Timestamp(void)
{
	for(int i=0;i<MAX_DEV_NUM;i++){
		if(DeviceList[i]._isactivate==1 && DeviceList[i]._shortAddress[0]==msg_recieve[7] && DeviceList[i]._shortAddress[1]==msg_recieve[8] ){
			DeviceList[i].lastActionTime=portGetTickCnt();
//			dwt_readrxtimestamp(&(DeviceList[i].timePollAckReceivedBytes[0]));
			correctTimestamp(&(DeviceList[i].timePollAckReceivedBytes[0]));
		}
	}
}


////////////////////////////////////////////////////
////////////  Handle Event /////////////////////////
////////////////////////////////////////////////////
void put_event(event_data_t newevent)
{

	//copy event
	set_info.dwevent[set_info.dweventIdxIn] = newevent;
  
	set_info.dwevent[set_info.dweventIdxIn].isactivate=1;
	set_info.dweventIdxIn++;

	if(MAX_EVENT_NUMBER == set_info.dweventIdxIn)
		set_info.dweventIdxIn = 0;
}

void check_event(){
  set_info.dweventIdxOut=set_info.dweventIdxIn;
	for(int i=0;i<MAX_EVENT_NUMBER;i++){
		if(set_info.dwevent[set_info.dweventIdxOut].isactivate==1){
			process_event(set_info.dwevent[set_info.dweventIdxOut]);
			set_info.dwevent[set_info.dweventIdxOut].isactivate=0;
		}
		set_info.dweventIdxOut++;
		if(MAX_EVENT_NUMBER == set_info.dweventIdxOut)
		set_info.dweventIdxOut = 0;
	}
}


void process_event(event_data_t event){
	if(event.type==SendRangeInit){
		sendRangeInit(event);
	}else if(event.type==SendFinal){
		if(decode_range_AndSolveDis(event)==1){      //return 1 only when Anchor itself in code and Tag in Anchor's list
			sendFinal(set_info.last_send_tag_idx);
		}
	}else if(event.type==SendPollAck){
		uint8 plan_time[2];
		if(checkRecievedPoll(event,plan_time)==1){      // Anchor self in tag's code
				save_poll_timestamp_AnndSendPollACK(event, plan_time);   //send only when Tag in Anchor's list
		}
	}


	
}
void clearevents(){
	for(int i=0;i<MAX_EVENT_NUMBER;i++){
		set_info.dwevent[i].isactivate=0;
	}
	set_info.dweventIdxIn=0;
	set_info.dweventIdxOut=0;
}

///////////////////////////////////////////////////////////////////
//////////////////////Compute Dis and Loc//////////////////////////
///////////////////////////////////////////////////////////////////

void computeDis(int Dev_idx){
	
	DeviceList[Dev_idx].round1=(int64_t)DeviceList[Dev_idx].timePollAckReceived-(int64_t)DeviceList[Dev_idx].timePollSent;
	if(DeviceList[Dev_idx].round1<0){
		DeviceList[Dev_idx].round1=DeviceList[Dev_idx].round1+TIME_OVERFLOW;

	}
	DeviceList[Dev_idx].round2=(int64_t)DeviceList[Dev_idx].timeRangeReceived-(int64_t)DeviceList[Dev_idx].timePollAckSent;
	if(DeviceList[Dev_idx].round2<0){
		DeviceList[Dev_idx].round2=DeviceList[Dev_idx].round2+TIME_OVERFLOW;

	}
	DeviceList[Dev_idx].reply1=(int64_t)DeviceList[Dev_idx].timePollAckSent-(int64_t)DeviceList[Dev_idx].timePollReceived;
	if(DeviceList[Dev_idx].reply1<0){
		DeviceList[Dev_idx].reply1=DeviceList[Dev_idx].reply1+TIME_OVERFLOW;

	}
	DeviceList[Dev_idx].reply2=(int64_t)DeviceList[Dev_idx].timeRangeSent-(int64_t)DeviceList[Dev_idx].timePollAckReceived;
	if(DeviceList[Dev_idx].reply2<0){
		DeviceList[Dev_idx].reply2=DeviceList[Dev_idx].reply2+TIME_OVERFLOW;

	}
	DeviceList[Dev_idx].timeT=(float)(DeviceList[Dev_idx].round1*DeviceList[Dev_idx].round2-DeviceList[Dev_idx].reply1*DeviceList[Dev_idx].reply2)/(float)(DeviceList[Dev_idx].round1+DeviceList[Dev_idx].round2+DeviceList[Dev_idx].reply1+DeviceList[Dev_idx].reply2);
	DeviceList[Dev_idx]._range= DISTANCE_OF_RADIO*DeviceList[Dev_idx].timeT;


	
//	sprintf((char*)rangeDis,"%-6.3f",DeviceList[Dev_idx].timeT);
//  if(memcmp(rangeDis,"0.000",4)==0){
//		writetoLCD(1,1,"x");
//	}
//  writetoLCD( 6, 1,rangeDis );
	
}


// TODO check function, different type violations between byte and int
void correctTimestamp(uint8* timestamp) {
#if CORRECT_TIME_BY_RePWR==1
	uint8 t[5];
	dwt_readrxtimestamp(&t[0]);
  t64= (uint64) t[0]
	            +((uint64)t[1]<<8)
							+((uint64)t[2]<<16)
							+((uint64)t[3]<<24)
							+((uint64)t[4]<<32);
	
	
	// base line dBm, which is -61, 2 dBm steps, total 18 data points (down to -95 dBm)
	float rxPowerBase     = -(getReceivePower()+61.0f)*0.5f;
	int16_t   rxPowerBaseLow  = (int16_t)rxPowerBase; // TODO check type
	int16_t   rxPowerBaseHigh = rxPowerBaseLow+1; // TODO check type
	if(rxPowerBaseLow < 0) {
		rxPowerBaseLow  = 0;
		rxPowerBaseHigh = 0;
	} else if(rxPowerBaseHigh > 17) {
		rxPowerBaseLow  = 17;
		rxPowerBaseHigh = 17;
	}
	// select range low/high values from corresponding table
	int16_t rangeBiasHigh;
	int16_t rangeBiasLow;
	if(set_info.configData.chan== 4 || set_info.configData.chan == 7) {
		// 900 MHz receiver bandwidth
		if(set_info.configData.prf == DWT_PRF_16M) {
			rangeBiasHigh = (rxPowerBaseHigh < BIAS_900_16_ZERO ? -BIAS_900_16[rxPowerBaseHigh] : BIAS_900_16[rxPowerBaseHigh]);
			rangeBiasHigh <<= 1;
			rangeBiasLow  = (rxPowerBaseLow < BIAS_900_16_ZERO ? -BIAS_900_16[rxPowerBaseLow] : BIAS_900_16[rxPowerBaseLow]);
			rangeBiasLow <<= 1;
		} else if(set_info.configData.prf == DWT_PRF_64M) {
			rangeBiasHigh = (rxPowerBaseHigh < BIAS_900_64_ZERO ? -BIAS_900_64[rxPowerBaseHigh] : BIAS_900_64[rxPowerBaseHigh]);
			rangeBiasHigh <<= 1;
			rangeBiasLow  = (rxPowerBaseLow < BIAS_900_64_ZERO ? -BIAS_900_64[rxPowerBaseLow] : BIAS_900_64[rxPowerBaseLow]);
			rangeBiasLow <<= 1;
		} else {
			// TODO proper error handling
			return;
		}
	} else {
		// 500 MHz receiver bandwidth
		if(set_info.configData.prf == DWT_PRF_16M) {
			rangeBiasHigh = (rxPowerBaseHigh < BIAS_500_16_ZERO ? -BIAS_500_16[rxPowerBaseHigh] : BIAS_500_16[rxPowerBaseHigh]);
			rangeBiasLow  = (rxPowerBaseLow < BIAS_500_16_ZERO ? -BIAS_500_16[rxPowerBaseLow] : BIAS_500_16[rxPowerBaseLow]);
		} else if(set_info.configData.prf == DWT_PRF_64M) {
			rangeBiasHigh = (rxPowerBaseHigh < BIAS_500_64_ZERO ? -BIAS_500_64[rxPowerBaseHigh] : BIAS_500_64[rxPowerBaseHigh]);
			rangeBiasLow  = (rxPowerBaseLow < BIAS_500_64_ZERO ? -BIAS_500_64[rxPowerBaseLow] : BIAS_500_64[rxPowerBaseLow]);
		} else {
			// TODO proper error handling
			return;
		}
	}
	// linear interpolation of bias values
	float      rangeBias = rangeBiasLow+(rxPowerBase-rxPowerBaseLow)*(rangeBiasHigh-rangeBiasLow);
	// range bias [mm] to timestamp modification value conversion
	int64_t adjustTime=(int64_t)(rangeBias*DISTANCE_OF_RADIO_INV*0.001f);
	t64=t64+adjustTime;
	
	t[0]= t64       & 0xFF;
	t[1]= (t64>>8)  & 0xFF;
	t[2]= (t64>>16) & 0xFF;
	t[3]= (t64>>24) & 0xFF;
	t[4]= (t64>>32) & 0xFF;
	memcpy(timestamp,&t[0],5);

	return ;
#else
	dwt_readrxtimestamp(timestamp);
	return;
#endif
}
