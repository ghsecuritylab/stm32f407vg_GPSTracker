/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "lwip/api.h"
#include "task.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

osThreadId_t defaultTaskHandle;
osThreadId_t networkTaskHandle;
osThreadId_t tempTaskHandle;
/* USER CODE BEGIN PV */

/* Generic */
signed char* overflowTaskName;

/* Network */
extern struct netif gnetif;
IpAddr serverIp = { 192, 168, 1, 9 };
ip_addr_t serverAddr;
u16_t serverPort = 3000;
char jsonString[50];
char requestString[150];
osSemaphoreId_t networkSemaphore;
err_t serverConnStatus = -1;

/* Temperature */
uint8_t data_write[3];
uint8_t data_read[2];
double avgTemp = -1; // average temp computed from our temp buffer
double baselineTemp = -1; // last sent temp. We use it to only send temp when the difference is big enough
double tempBuffer[TEMP_BUFFER_SIZE]; // temperature value buffer
osMessageQueueId_t tempQueue;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
void StartDefaultTask(void *argument);
void StartNetworkTask(void *argument);
void StartTempTask(void *argument);

/* USER CODE BEGIN PFP */

void createHttpTempRequest(char* outputString, IpAddr ip, u16_t port, double temp);
void parseDouble(char* output, double value, uint8_t precision);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

	// Convert our ip data structure into a format that lwip uses
	IP_ADDR4(&serverAddr, serverIp.addr0, serverIp.addr1, serverIp.addr2, serverIp.addr3);

	tempQueue = osMessageQueueNew(1, sizeof(double), NULL);
	networkSemaphore = osSemaphoreNew(1, 0, NULL);

  /* USER CODE END 1 */
  

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 512
  };
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* definition and creation of networkTask */
  const osThreadAttr_t networkTask_attributes = {
    .name = "networkTask",
    .priority = (osPriority_t) osPriorityLow,
    .stack_size = 1024
  };
  networkTaskHandle = osThreadNew(StartNetworkTask, NULL, &networkTask_attributes);

  /* definition and creation of tempTask */
  const osThreadAttr_t tempTask_attributes = {
    .name = "tempTask",
    .priority = (osPriority_t) osPriorityLow,
    .stack_size = 256
  };
  tempTaskHandle = osThreadNew(StartTempTask, NULL, &tempTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

  /*const osThreadAttr_t accTask_attributes = {
      .name = "accTask",
      .priority = (osPriority_t) osPriorityNormal,
      .stack_size = 128
    };
  accTaskHandle = osThreadNew(StartAccTask, NULL, &accTask_attributes);*/

  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();
  
  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pins : PD12 PD13 PD14 PD15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
/*void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	accDataRdyFlag = 1;
	HAL_GPIO_TogglePin(GPIOD, LED_ORANGE);
	//HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}*/

void parseDouble(char* output, double value, uint8_t precision) {
	int integerPart = (int)value;
	float floatingPointPart = (value - integerPart) * pow(10, precision);
	sprintf(output, "%d.%d", (int)integerPart, (int)floatingPointPart);
}

void createHttpTempRequest(char* outputString, IpAddr ip, u16_t port, double temp) {
	char ipString[15];
	sprintf(ipString, "%d.%d.%d.%d", ip.addr0, ip.addr1, ip.addr2, ip.addr3);

	char jsonFormat[] = "{\"temp\":%s}";
	char tempString[10];
	parseDouble(tempString, temp, 4);
	sprintf(jsonString, jsonFormat, tempString);

	char requestFormat[] = "POST /temp HTTP/1.0\r\n\
Host: %s:%d\r\n\
Content-Type: application/json\r\n\
Content-Length: %d\r\n\
\r\n\
%s\r\n\
\r\n";

	sprintf(outputString, requestFormat, ipString, port, strlen(jsonString), jsonString);
}

void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName) {
	overflowTaskName = pcTaskName;
	HAL_GPIO_WritePin(GPIOD, LED_RED, 1);
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used 
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN 5 */

  /* Infinite loop */
  for(;;)
  {
	  /* Async init code */


	  /* LED indicators */

	  // Check if DHCP got an addr from router
	  if (gnetif.ip_addr.addr != 0) {
		  // Signal network task that the network is ready for sending data
		  osSemaphoreRelease(networkSemaphore);
		  HAL_GPIO_WritePin(GPIOD, LED_GREEN, 1);
	  }

	  osDelay(50);
  }
  /* USER CODE END 5 */ 
}

/* USER CODE BEGIN Header_StartNetworkTask */
/**
* @brief Function implementing the networkTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartNetworkTask */
void StartNetworkTask(void *argument)
{
  /* USER CODE BEGIN StartNetworkTask */

	// Wait for network connection to be established
	osSemaphoreAcquire(networkSemaphore, portMAX_DELAY);

  /* Infinite loop */
  for(;;)
  {
	  // Wait for the new avg temp message
	  osMessageQueueGet(tempQueue, &avgTemp, NULL, portMAX_DELAY);

	  // Establish connection to server
	  struct netconn* conn = netconn_new(NETCONN_TCP);
	  serverConnStatus = netconn_connect(conn, &serverAddr, serverPort);
	  if (serverConnStatus == ERR_OK) {
		  // Send json temp value to server
		  createHttpTempRequest(requestString, serverIp, serverPort, avgTemp);
		  netconn_write(conn, requestString, strlen(requestString), NETCONN_NOFLAG);

		  // Indicate that data was sent
		  HAL_GPIO_TogglePin(GPIOD, LED_BLUE);
	  }
	  else {
		  HAL_GPIO_WritePin(GPIOD, LED_BLUE, 0);
	  }

	  netconn_delete(conn);

	  osDelay(100);
  }
  /* USER CODE END StartNetworkTask */
}

/* USER CODE BEGIN Header_StartTempTask */
/**
* @brief Function implementing the tempTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTempTask */
void StartTempTask(void *argument)
{
  /* USER CODE BEGIN StartTempTask */

	data_write[0] = MCP9808_REG_CONF;
	data_write[1] = 0x00;  // config msb
	data_write[2] = 0x00;  // config lsb
	HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)MCP9808_ADDR, (uint8_t*)data_write, 3, 10);
	osDelay(20);

	int buffer_index = 0;

  /* Infinite loop */
  for(;;)
  {
	  // Read temperature register
	  data_write[0] = MCP9808_REG_TEMP;
	  HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)MCP9808_ADDR, (uint8_t*)data_write, 1, 10); // no stop
	  HAL_I2C_Master_Receive(&hi2c1, (uint16_t)MCP9808_ADDR, (uint8_t*)data_read, 2, 10);

	  data_read[0] = data_read[0] & 0x1F;  // clear flag bits

	  /* Calculate temp with 0.25 precision */
	  double temp = -1;
	  if((data_read[0] & 0x10) == 0x10) {
		  // Negative temp
		  data_read[0] = data_read[0] & 0x0F;
		  temp = 256 - (data_read[0] << 4) + (data_read[1] >> 4);
	  } else {
		  // Positive temp
		  temp = (data_read[0] << 4) + (data_read[1] >> 4);
	  }

	  // fractional part (0.25°C precision)
	  if (data_read[1] & 0x08) {
		  if(data_read[1] & 0x04) {
			  temp += 0.75;
		  } else {
			  temp += 0.5;
		  }
	  } else {
		  if(data_read[1] & 0x04) {
			  temp += 0.25;
		  }
	  }

	  tempBuffer[buffer_index++] = temp;

	  if (buffer_index == TEMP_BUFFER_SIZE) {
		  // Calculate avg. temp
		  avgTemp = 0;
		  for (int i = 0; i < TEMP_BUFFER_SIZE; i++) {
			  avgTemp += tempBuffer[i];
		  }
		  avgTemp = avgTemp/TEMP_BUFFER_SIZE;

		  // Send avg. temp to network task via message queue
		  if (avgTemp > (baselineTemp + TEMP_DIFF) || avgTemp < (baselineTemp - TEMP_DIFF)) {
			  osMessageQueuePut(tempQueue, &avgTemp, 0, 0);
			  baselineTemp = avgTemp; // Set baseline temp to new baseline
		  }
		  else {
			  // Reset the server send led status because the task is blocked while waiting for new message
			  // This is redundant piece of code
			  HAL_GPIO_WritePin(GPIOD, LED_BLUE, 0);
		  }

		  buffer_index = 0;
	  }

	  //HAL_GPIO_TogglePin(GPIOD, LED_ORANGE);

	  osDelay(50);
  }
  /* USER CODE END StartTempTask */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
