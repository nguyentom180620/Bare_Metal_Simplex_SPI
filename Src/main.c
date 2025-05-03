/*
 * Name: Bare_Metal_Simplex_SPI
 * Purpose: To implement an SPI Driver Bare Metal
 * Author: Tom Nguyen
 * Date: 5/2/2025
 */

#include <stdint.h>

#define RCC 			0x40023800
#define RCC_CR			(RCC + 0x00)
#define RCC_CFGR		(RCC + 0x08)
#define RCC_AHB1ENR		(RCC + 0x30)
#define RCC_APB2ENR		(RCC + 0x44)
#define FLASH			0x40023C00
#define FLASH_ACR		(FLASH + 0x00)
#define SPI1			0x40013000
#define SPI1_CR1		(SPI1 + 0x00)
#define SPI1_CR2		(SPI1 + 0x04)
#define SPI1_SR			(SPI1 + 0x08)
#define SPI1_DR			(SPI1 + 0x0C)
#define GPIOA			0x40020000
#define GPIOA_MODER		(GPIOA + 0x00)
#define GPIOA_AFRL		(GPIOA + 0x20)
#define CS_Port			GPIOA
#define CS_Pin			4
#define CS_BSRR			(CS_Port + 0x18)

static void SetSystemClockto16MHz(void);
static void SPI1ClockEnable(void);
static void GPIOAClockEnable(void);
static void SPI1Init(void);
static void SPI1WriteToDR(uint16_t data);
static void WaitForTransmissionEnd(void);
static void EnableSlave(void);
static void DisableSlave(void);
static void SPI1_Transmit(uint16_t data);
static void SPI1PinsInit(void);
//static void CS_PinInit(void);

int main(void)
{
	SetSystemClockto16MHz();
	SPI1ClockEnable();
	GPIOAClockEnable();

	SPI1PinsInit();
	SPI1Init();

	// Write Data out
	uint16_t myData = 0x3701;	// We want to see 0b0011 0111 0000 0001 for MSB first
	uint16_t myData2 = 0x1234;

	while(1)
	{
		SPI1_Transmit(myData);
		SPI1_Transmit(myData2);
	}
}

void SetSystemClockto16MHz(void)
{
	// Initialize System Clock
	uint32_t *RCC_CR_Ptr = (uint32_t*)RCC_CR;
	// Turn on HSI
	*RCC_CR_Ptr |= (uint32_t)0x1;
	// Wait for HSI Clock to be ready
	while ((*RCC_CR_Ptr & 0x2) == 0);

	// Configure Prescalers
	uint32_t *RCC_CFGR_Ptr = (uint32_t*)RCC_CFGR;
	// HPRE
	*RCC_CFGR_Ptr &= ~(uint32_t)(0b1111 << 4);
	// PPRE1
	*RCC_CFGR_Ptr &= ~(uint32_t)(0b111 << 10);
	// PPRE2
	*RCC_CFGR_Ptr &= ~(uint32_t)(0b111 << 13);

	// Set HSI as Clock Source
	*RCC_CFGR_Ptr &= ~(uint32_t)(0b11);

	// Configure Flash
	uint32_t *FLASH_ACR_Ptr = (uint32_t*)FLASH_ACR;
	// Latency
	*FLASH_ACR_Ptr |= (uint32_t)(0b0000 << 0);
	// ICEN
	*FLASH_ACR_Ptr |= (uint32_t)(0b1 << 9);
	// DCEN
	*FLASH_ACR_Ptr |= (uint32_t)(0b1 << 10);

	// Turn off HSE
	*RCC_CR_Ptr &= ~((uint32_t)0x1 << 16);
}

void SPI1ClockEnable(void)
{
	// First, SPI clock through APB2 Bus
	uint32_t *RCC_APB2ENR_Ptr = (uint32_t*)RCC_APB2ENR;
	*RCC_APB2ENR_Ptr |= (uint32_t)(0x1 << 12);
}

void GPIOAClockEnable(void)
{
	// Now, Enable GPIOA Clock through AHB1 Bus
	uint32_t *RCC_AHB1ENR_Ptr = (uint32_t*)RCC_AHB1ENR;
	*RCC_AHB1ENR_Ptr |= (uint32_t)0x1;
}

void SPI1Init(void)
{
	// Set Up SPI Init
	uint32_t *SPI1_CR1_Ptr = (uint32_t*)SPI1_CR1;

	// NOTE: Simplex is basically just full duplex but we don't use MISO

	// BIDIMODE off
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x1 << 15);
	// CRC Calculations off
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x1 << 13);
	// DFF to 16 bits
	*SPI1_CR1_Ptr |= (uint32_t)(0x1 << 11);
	// RXOnly off since we are transferring from master to slave
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x1 << 10);
	// SSM Disabled
	// MSB Selected
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x1 << 7);
	// Baud Rate of 2 MBits/S
	*SPI1_CR1_Ptr &= ~(uint32_t)(0b111 << 3);
	*SPI1_CR1_Ptr |= (uint32_t)(0b010 << 3);
	// Put into Master Mode
	*SPI1_CR1_Ptr |= (uint32_t)(0x1 << 2);
	// Set CPOL and CPHA
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x3);

	// SSOE enabled
	uint32_t *SPI1_CR2_Ptr = (uint32_t*)SPI1_CR2;
	*SPI1_CR2_Ptr |= 0x4;

	// Finally, enable SPI
	*SPI1_CR1_Ptr |= (uint32_t)(0x1 << 6);
}

void SPI1WriteToDR(uint16_t data)
{
	// Load data into SPI1 data register
	uint32_t *SPI1_DR_Ptr = (uint32_t*)SPI1_DR;
	*SPI1_DR_Ptr = (uint32_t)data;
}

void WaitForTransmissionEnd(void)
{
	// Wait for transmission to end by checking BSY and TXE
	uint32_t *SPI1_SR_Ptr = (uint32_t*)SPI1_SR;
	while ((*SPI1_SR_Ptr & (0b1 << 7)) != 0);
	while ((*SPI1_SR_Ptr & (0b1 << 1)) == 0);
}

void EnableSlave(void)
{
	// Enable Slave
	uint32_t *CS_BSRR_Ptr = (uint32_t*)CS_BSRR;
	*CS_BSRR_Ptr |= (uint32_t)(0b1 << (CS_Pin + 16));
}

void DisableSlave(void)
{
	// Disable Slave
	uint32_t *CS_BSRR_Ptr = (uint32_t*)CS_BSRR;
	*CS_BSRR_Ptr |= (uint32_t)(0b1 << CS_Pin);
}

void SPI1_Transmit(uint16_t data)
{
	// Enable Slave
	EnableSlave();
	SPI1WriteToDR(data);
	WaitForTransmissionEnd();
	DisableSlave();
}

void SPI1PinsInit(void)
{
	// Initialize SPI GPIO Pins
	// First, PinA5 for SCLK
	uint32_t *GPIOA_MODER_Ptr = (uint32_t*)GPIOA_MODER;
	// Set to Alternate Function
	*GPIOA_MODER_Ptr &= ~(uint32_t)(0b11 << 10);
	*GPIOA_MODER_Ptr |= (uint32_t)(0b10 << 10);
	// Next, PinA7 for MOSI
	*GPIOA_MODER_Ptr &= ~(uint32_t)(0b11 << 14);
	*GPIOA_MODER_Ptr |= (uint32_t)(0b10 << 14);

	// Set a GPIO Pin for CS Pin
	uint32_t *CS_Port_Ptr = (uint32_t*)CS_Port;
	// Set Pin 4 to output
	*CS_Port_Ptr &= ~(uint32_t)(0b11 << 2 * CS_Pin);
	*CS_Port_Ptr |= (uint32_t)(0b01 << 2 * CS_Pin);

	// Set up alternate function by selecting AF5 (According to datasheet)
	uint32_t *GPIOA_AFRL_Ptr = (uint32_t*)GPIOA_AFRL;
	*GPIOA_AFRL_Ptr |= (uint32_t)(0b0101 << 16);
	*GPIOA_AFRL_Ptr |= (uint32_t)(0b0101 << 20);
	*GPIOA_AFRL_Ptr |= (uint32_t)(0b0101 << 28);
	// Initialize to High
	DisableSlave();
}

//void CS_PinInit(void)
//{
//	// Set a GPIO Pin for CS Pin
//	uint32_t *CS_Port_Ptr = (uint32_t*)CS_Port;
//	// Set Pin 4 to output
//	*CS_Port_Ptr &= ~(uint32_t)(0b11 << 2 * CS_Pin);
//	*CS_Port_Ptr |= (uint32_t)(0b01 << 2 * CS_Pin);
//	// Initialize to High
//	DisableSlave();
//}
