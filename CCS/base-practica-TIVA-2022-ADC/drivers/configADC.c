// En este archivo se configura el ADC al completo:
// 1.- Trigger
// 2.- Inicializaci�n de la secuencia
// 3.- Recibir de la cola
// 4.- Interrupci�n del ADC

#include<stdint.h>
#include<stdbool.h>

#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_adc.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/adc.h"
#include "driverlib/timer.h"
#include "FreeRTOS.h"
#include "configADC.h"
#include "task.h"
#include "queue.h"

#include "ColaEventos.h"

// Al ser una cola static, la cola solo se puede emplear en este fichero
static QueueHandle_t cola_adc;
static QueueHandle_t cola_adc2;

// DISPARO ADC0, Provoca el disparo de una conversion (hemos configurado el ADC con "disparo software" (Processor trigger)
void configADC0_DisparaADC(void)
{
	ADCProcessorTrigger(ADC0_BASE,0);
}

void configADC1_ConfigTimer(uint8_t control, double val)
{

    //DESACTIVAR, bot�n de cambio de frecuencia no pulsado
    if (control == 0)
    {
        //Desactivo la Secuencia
        ADCSequenceDisable(ADC1_BASE, 0);

        //Desactiva el disparo ADC del temporizador
        TimerControlTrigger(TIMER2_BASE, TIMER_A, false);

        //DesHabilito Timer
        TimerDisable(TIMER2_BASE, TIMER_A);

        // Deshabilitar interrupcion ADC1
        ADCIntDisable(ADC1_BASE, 0);

        // Deshabilitar NVIC
        IntDisable(INT_ADC1SS0);
    }

    //ACTIVAR, bot�n de cambio de frecuencia pulsado
    else if (control == 1)
    {
        ADCSequenceEnable(ADC1_BASE, 0);
        TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);

        //Cargar el valor que contar� el temporizador
        TimerLoadSet(TIMER2_BASE, TIMER_A, (SysCtlClockGet()/val));
        TimerEnable(TIMER2_BASE, TIMER_A);

        // Habilitar interrupcion ADC1
        ADCIntClear(ADC1_BASE, 0);
        ADCIntEnable(ADC1_BASE, 0);

        // Habilitar interrupcion por parte del NVIC
        IntPrioritySet(INT_ADC1SS0, configMAX_SYSCALL_INTERRUPT_PRIORITY);
        IntEnable(INT_ADC1SS0);
    }

    //CAMBIAR, bot�n de cambio de frecuencia ya pulsado y s�lo cambia frecuencia
    else if (control == 2)
    {
        //DesHabilito Timer
        TimerDisable(TIMER2_BASE, TIMER_A);

        //Cargar el valor que contar� el temporizador
        TimerLoadSet(TIMER2_BASE, TIMER_A, (SysCtlClockGet()/val));
        TimerEnable(TIMER2_BASE, TIMER_A);

        // Reset ADC1
        ADCIntClear(ADC1_BASE, 0);
    }
}

void configADC0_IniciaADC0(void)
{
    // ADC0 empleado para polling, para los 6 canales
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

    // Especificamos que el periferico siga encendido en modo de bajo consumo
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_ADC0);

    //HABILITAMOS EL GPIOE, GPIOB y GPIOD
    // Para configurar el multiplexor de se�ales
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOD);

    // Enable pin PE3 for ADC AIN0|AIN1|AIN2|AIN3
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0);
    // Enable pin PD3 for ADC AIN4
    GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_3);
    // Enable pin PB5 for ADC AIN11, este ser� al que conectemos el sensor de intensidad de ruido
    GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_5);

    // CONFIGURAR ADC0, Vamos a configurar el secuenciador 0 porque admite 8 pasos, nosotros tenemos 6
    // Secuenciador 0 -> admite 8, Secuenciador 1/2 -> admite 4 y Secuenciador 3 -> admite 1
    ADCSequenceDisable(ADC0_BASE, 0);

    // CONFIGURAR ADC0, Configuramos la velocidad de conversion al maximo (1MS/s)
    // 1us por cada muestra
    ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_RATE_FULL, 1);

    //Disparo software (processor trigger)
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 1, ADC_CTL_CH1);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 2, ADC_CTL_CH2);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 3, ADC_CTL_CH3);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 4, ADC_CTL_CH4);
    //La ultima muestra provoca la interrupcion
    ADCSequenceStepConfigure(ADC0_BASE, 0, 5, ADC_CTL_CH11 | ADC_CTL_IE | ADC_CTL_END);
    //ACTIVO LA SECUENCIA
    ADCSequenceEnable(ADC0_BASE,0);

    //Habilita las interrupciones, ADC0
    ADCIntClear(ADC0_BASE, 0);
    ADCIntEnable(ADC0_BASE, 0);
    IntPrioritySet(INT_ADC0SS0, configMAX_SYSCALL_INTERRUPT_PRIORITY);
    IntEnable(INT_ADC0SS0);

    //Creamos una cola de mensajes para la comunicacion entre la ISR y la tara que llame a configADC_LeeADC(...)
    // Se ha modificado las estructuras donde se encuentra definida "MuestraADC"
    cola_adc = xQueueCreate(12, sizeof(MuestrasADC));
    if (cola_adc == NULL)
    {
        while(1);
    }
}

void configADC1_IniciaADC1(void)
{
    //Habilitar Temporizador 2
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
    // Permitimos el funcionamiento del Temporizador 2 en modo de bajo consumo
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_TIMER2);
    //Configurar temporizador como peri�dico
    TimerConfigure(TIMER2_BASE, TIMER_CFG_PERIODIC);

    //Habilitar disparador de ADC por temporizador
    TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);

    // Habilitar ADC1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
    // Permitimos el funcionamiento del ADC1 en modo de bajo consumo
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_ADC1);

    //Habilitar pin de entrada anal�gica
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_5);
    GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_5);

    // Configurar secuencia de muestreo
    ADCSequenceDisable(ADC1_BASE, 0);
    ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH11 | ADC_CTL_IE | ADC_CTL_END);

    cola_adc2 = xQueueCreate(16, sizeof(MuestrasADC));
    if (cola_adc2 == NULL)
    {
        while(1);
    }
}

void configADC0_LeeADC0(MuestrasADC *datos)
{
	xQueueReceive(cola_adc, datos, portMAX_DELAY);
}

void configADC1_LeeADC1(MuestrasADC *datos)
{
    xQueueReceive(cola_adc2, datos, portMAX_DELAY);
}

void configADC0_ISR(void)
{
	portBASE_TYPE higherPriorityTaskWoken = pdFALSE;

	MuestrasLeida leidas;
	MuestrasADC finales;
	ADCIntClear(ADC0_BASE, 0);//LIMPIAMOS EL FLAG DE INTERRUPCIONES

	// Hay que tener cuidado con el tipo empleado para coger los datos guardados
	ADCSequenceDataGet(ADC0_BASE, 0, (uint32_t *)&leidas);//COGEMOS LOS DATOS GUARDADOS

	//Pasamos de 32 bits a 16 (el conversor es de 12 bits, as� que s�lo son significativos los bits del 0 al 11)
	// valores de entre 0 hasta 4096 ()
    uint8_t i;
    for(i = 0;i < 6;i++)
    {
        finales.chan[i] = leidas.chan[i];
    }

    xEventGroupSetBitsFromISR(FlagsEventos, ADC0_COLA, &higherPriorityTaskWoken);

	//Guardamos en la cola
	xQueueSendFromISR(cola_adc, &finales, &higherPriorityTaskWoken);
	portEND_SWITCHING_ISR(higherPriorityTaskWoken);
}

void configADC1_ISR(void)
{
    static uint8_t contador = 0;

    portBASE_TYPE higherPriorityTaskWoken = pdFALSE;

    MuestrasLeida leidas;
    MuestrasADC finales;
    ADCIntClear(ADC1_BASE, 0);//LIMPIAMOS EL FLAG DE INTERRUPCIONES

    // tomar las 8 secuencias primero tutoria static

    if(contador < 8)
    {
        contador++;
    }
    else
    {
        contador = 0;

        // Hay que tener cuidado con el tipo empleado para coger los datos guardados
        ADCSequenceDataGet(ADC1_BASE, 0, (uint32_t *)&leidas);//COGEMOS LOS DATOS GUARDADOS

        // Pasamos de 32 bits a 16 (el conversor es de 12 bits, as� que s�lo son significativos los bits del 0 al 11)
        // valores de entre 0 hasta 4096 ()
        uint8_t i;
        for(i = 0;i < 8;i++)
        {
            finales.chan[i] = leidas.chan[i];
        }

        xEventGroupSetBitsFromISR(FlagsEventos, ADC1_COLA, &higherPriorityTaskWoken);

        //Guardamos en la cola
        xQueueSendFromISR(cola_adc2, &finales, &higherPriorityTaskWoken);
    }

    portEND_SWITCHING_ISR(higherPriorityTaskWoken);
}
