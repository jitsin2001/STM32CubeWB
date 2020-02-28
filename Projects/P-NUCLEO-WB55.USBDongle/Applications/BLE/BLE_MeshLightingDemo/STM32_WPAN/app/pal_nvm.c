/**
  ******************************************************************************
  * @file    pal_nvm.c
  * @author  BLE Mesh Team
  * @brief   Flash management for the Controller
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "types.h"
#include "pal_nvm.h"
#include "pal_if.h"  
#include <string.h>

#include "ble.h"
#include "shci.h"
#include "mesh_cfg_usr.h"

#include "hw_flash.h"

/* Private define ------------------------------------------------------------*/
#define FLASH_SECTOR_SIZE 0x1000
#define MAX_NVM_PENDING_WRITE_REQS         1

/* Private variables ---------------------------------------------------------*/
typedef struct
{
    MOBLEUINT16 offset;
    MOBLEUINT16 size;
    void const *buff;
} BNRGM_NVM_WRITE;

typedef struct
{
    MOBLEUINT8 no_of_pages_to_be_erased;
    MOBLEUINT8 no_of_write_reqs;
    MOBLEBOOL backup_req;
    BNRGM_NVM_WRITE write_req[MAX_NVM_PENDING_WRITE_REQS];
} BNRGM_NVM_REQS;

BNRGM_NVM_REQS BnrgmNvmReqs = {0};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Gets the page of a given address
  * @param  Addr: Address of the FLASH Memory
  * @retval The page of a given address
  */
static MOBLEUINT32 GetPage(MOBLEUINT32 Addr)
{
  MOBLEUINT32 page = 0;

  if (Addr < (FLASH_BASE + FLASH_BANK_SIZE))
  {
    /* Bank 1 */
    page = (Addr - FLASH_BASE) / FLASH_PAGE_SIZE;
  }
  else
  {
    /* Bank 2 */
    page = (Addr - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
  }

  return page;
}

#if 0
/**
* @brief  PalNvmErase
* @param  None
* @retval Result
*/
MOBLE_RESULT PalNvmErase(MOBLEUINT32 address,
                         MOBLEUINT32 offset)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t pageError = 0;

//  printf("PalNvmErase >>>\r\n");
  
  FLASH_EraseInitTypeDef erase;
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Page = GetPage(address + offset); /* 126 or 127 */;
  erase.NbPages = FLASH_SECTOR_SIZE >> 12;
  
  while( LL_HSEM_1StepLock( HSEM, CFG_HW_FLASH_SEMID ) );
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_OPTVERR);
  
  SHCI_C2_FLASH_EraseActivity( ERASE_ACTIVITY_ON);

  while(LL_FLASH_IsActiveFlag_OperationSuspended());
  status = HAL_FLASHEx_Erase(&erase, &pageError);
  while(LL_FLASH_IsActiveFlag_OperationSuspended());
  
  SHCI_C2_FLASH_EraseActivity( ERASE_ACTIVITY_OFF);

  HAL_FLASH_Lock();
  LL_HSEM_ReleaseLock( HSEM, CFG_HW_FLASH_SEMID, 0 );
  
//  printf("PalNvmErase <<<\r\n");
  
  return status == HAL_OK ? MOBLE_RESULT_SUCCESS : MOBLE_RESULT_FAIL;
}
#endif

/**
* @brief  returns NVM write protect status
* @param  None
* @retval TRUE if flash is write protected
*/
MOBLEBOOL MoblePalNvmIsWriteProtected(void)
{
    /* All flash is writable */
    return MOBLE_FALSE;
}

/**
* @brief  Read NVM
* @param  address: read start address of nvm
* @param  offset: offset read start address of nvm
* @param  buf: copy of read content
* @param  size: size of memory to be read
* @param  backup: If read from backup memory
* @retval Result of read operation
*/
MOBLE_RESULT MoblePalNvmRead(MOBLEUINT32 address,
                             MOBLEUINT32 offset, 
                             void *buf, 
                             MOBLEUINT32 size, 
                             MOBLEBOOL backup)
{
  MOBLE_RESULT result = MOBLE_RESULT_SUCCESS;
  
//  printf("MoblePalNvmRead >>>\r\n");  

  if (offset > NVM_SIZE)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else if (size == 0)
  {
    result = MOBLE_RESULT_FALSE;
  }
  else if (offset + size > NVM_SIZE)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else
  {
    memcpy(buf, (void *)(address + offset), size);
  }
  
//  printf("MoblePalNvmRead <<<\r\n");  
  return result;
}

/**
* @brief  Compare with NVM
* @param  offset: start address of nvm to compare
* @param  offset: offset start address of nvm
* @param  buf: copy of content
* @param  size: size of memory to be compared
* @param  comparison: outcome of comparison
* @retval Result
*/
MOBLE_RESULT MoblePalNvmCompare(MOBLEUINT32 address,
                                MOBLEUINT32 offset, 
                                void const *buf, 
                                MOBLEUINT32 size, 
                                MOBLE_NVM_COMPARE* comparison)
{
  MOBLE_RESULT result = MOBLE_RESULT_SUCCESS;
  
#ifdef ENABLE_SAVE_MODEL_STATE_NVM
//  printf("MoblePalNvmCompare >>>\r\n");
  
  if ((comparison == NULL) || (buf == NULL))
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else if (offset > NVM_SIZE)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else if (size == 0)
  {
    result = MOBLE_RESULT_FALSE;
  }
  else if (offset + size > NVM_SIZE)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else if (offset & 3)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else if (size & 3)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else
  {
    *comparison = MOBLE_NVM_COMPARE_EQUAL;

    uint8_t* src = (uint8_t*)buf;
    uint8_t* dst = (uint8_t*)(address + offset);
    
    for (MOBLEUINT32 i=0; i<size; ++i)
    {
      if ((src[i] != dst[i]) && (*comparison == MOBLE_NVM_COMPARE_EQUAL))
      {
        *comparison = MOBLE_NVM_COMPARE_NOT_EQUAL;
      }
        if ((src[i] & dst[i]) != dst[i])
        {
          *comparison = MOBLE_NVM_COMPARE_NOT_EQUAL_ERASE;
          break;
        }
    }
  }
  
//  printf("MoblePalNvmCompare <<<\r\n");
#else
  *comparison = MOBLE_NVM_COMPARE_EQUAL;
#endif  
  return result;
}

/**
* @brief  Erase NVM
* @param  None
* @retval Result
*/
MOBLE_RESULT MoblePalNvmErase(MOBLEUINT32 address,
                              MOBLEUINT32 offset)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t pageError = 0;

//  printf("MoblePalNvmErase >>>\r\n");
  
  FLASH_EraseInitTypeDef erase;
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Page = GetPage(address + offset); /* 126 or 127 */;
  erase.NbPages = FLASH_SECTOR_SIZE >> 12;
  
  while( LL_HSEM_1StepLock( HSEM, CFG_HW_FLASH_SEMID ) );
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_OPTVERR);
  
  SHCI_C2_FLASH_EraseActivity( ERASE_ACTIVITY_ON);

  while(LL_FLASH_IsActiveFlag_OperationSuspended());
  status = HAL_FLASHEx_Erase(&erase, &pageError);
  while(LL_FLASH_IsActiveFlag_OperationSuspended());
  
  SHCI_C2_FLASH_EraseActivity( ERASE_ACTIVITY_OFF);
  
  HAL_FLASH_Lock();
  LL_HSEM_ReleaseLock( HSEM, CFG_HW_FLASH_SEMID, 0 );
  
//  printf("MoblePalNvmErase <<<\r\n");
  
  return status == HAL_OK ? MOBLE_RESULT_SUCCESS : MOBLE_RESULT_FAIL;
}

/**
* @brief  Write to NVM
* @param  offset: wrt start address of nvm
* @param  offset: offset wrt start address of nvm
* @param  buf: copy of write content
* @param  size: size of memory to be written
* @retval Result
*/
MOBLE_RESULT MoblePalNvmWrite(MOBLEUINT32 address,
                              MOBLEUINT32 offset, 
                              void const *buf, 
                              MOBLEUINT32 size)
{
  MOBLE_RESULT result = MOBLE_RESULT_SUCCESS;
  
#ifdef ENABLE_SAVE_MODEL_STATE_NVM
  
  //  printf("MoblePalNvmWrite >>>\r\n");
  
  if (offset > NVM_SIZE)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else if (size == 0)
  {
    result = MOBLE_RESULT_FALSE;
  }
  else if (offset + size > NVM_SIZE)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else if (offset & 3)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else if (size & 3)
  {
    result = MOBLE_RESULT_INVALIDARG;
  }
  else
  {
    size >>= 3;
    
    uint64_t* src = (uint64_t*)buf;
    
    while( LL_HSEM_1StepLock( HSEM, CFG_HW_FLASH_SEMID ) );
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_OPTVERR | FLASH_FLAG_PGSERR | FLASH_FLAG_SIZERR | FLASH_FLAG_PGAERR);

    for (size_t i = 0; i < size; i++)
    {
      do
      {
        while(LL_FLASH_IsActiveFlag_OperationSuspended());
//#ifndef ENABLE_PROVISIONER_FEATURE
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + offset + (i <<3), src[i]);
//#else
//        HW_FLASH_Write(address + offset + (i <<3), src[i]);
//#endif
        while(LL_FLASH_IsActiveFlag_OperationSuspended());
      } while(*((uint64_t*)(address + offset + (i <<3))) != src[i]);
    }

    HAL_FLASH_Lock();
    LL_HSEM_ReleaseLock( HSEM, CFG_HW_FLASH_SEMID, 0 );
  }
//  printf("MoblePalNvmWrite <<<\r\n");
#endif
  
  return result;
}

#if 0
/**
* @brief  Backup process
* @param  None
* @retval Result
*/
static MOBLE_RESULT BnrgmPalNvmBackupProcess(void)
{
  MOBLEUINT32 buff[4*N_BYTES_WORD];
  static MOBLEUINT8 backup_pages_to_be_erased = 0;    
  MOBLE_RESULT result = MOBLE_RESULT_SUCCESS;
    
  if (backup_pages_to_be_erased == 0)
  {
    backup_pages_to_be_erased = 1;
  }
      
  if(backup_pages_to_be_erased != 0)
  {
#if 0
    BluenrgMesh_StopAdvScan();
    ATOMIC_SECTION_BEGIN();
    if(BluenrgMesh_IsFlashReadyToErase())
    {
      FLASH_ErasePage((uint16_t)((BNRGM_NVM_BACKUP_BASE - RESET_MANAGER_FLASH_BASE_ADDRESS) / PAGE_SIZE +
                       BNRGM_NVM_BACKUP_SIZE/PAGE_SIZE - backup_pages_to_be_erased));
         
      if (FLASH_GetFlagStatus(Flash_CMDERR) == SET)
      {
        result = MOBLE_RESULT_FAIL;
      }
      else
      {
        backup_pages_to_be_erased--;
      }
    }
    else
    {
      /* do nothing */
    }
    ATOMIC_SECTION_END();
#else
    result = PalNvmErase(NVM_BASE, FLASH_SECTOR_SIZE);
    if(result == MOBLE_RESULT_SUCCESS)
      backup_pages_to_be_erased = 0;
#endif
  }
    
  if (result == MOBLE_RESULT_SUCCESS && backup_pages_to_be_erased == 0)
  {
#if 0
    BluenrgMesh_StopAdvScan();
    ATOMIC_SECTION_BEGIN();
    if(BluenrgMesh_IsFlashReadyToErase())
    {
      for (size_t i = 0; i < BNRGM_NVM_BACKUP_SIZE && FLASH_GetFlagStatus(Flash_CMDERR) == RESET; )
      {
        memcpy((MOBLEUINT8*)buff, (void *)(BNRGM_NVM_BASE + i), 4*N_BYTES_WORD);
        FLASH_ProgramWordBurst(BNRGM_NVM_BACKUP_BASE + i, (uint32_t*)buff);
        i += 4*N_BYTES_WORD;
      }
          
      if (FLASH_GetFlagStatus(Flash_CMDERR) == SET)
      {
        result = MOBLE_RESULT_FAIL;
      }
      else
      {
        BnrgmNvmReqs.backup_req = MOBLE_FALSE;
      }
    }
    else
    {
      /* do nothing */
    }
    ATOMIC_SECTION_END();
#else
#endif
  }
 return result;
}
#endif

/**
* @brief  NVM process
* @param  None
* @retval Result
*/
MOBLE_RESULT BnrgmPalNvmProcess(void)
{
    /* do nothing */
    return MOBLE_RESULT_SUCCESS;
}

/******************* (C) COPYRIGHT 2019 STMicroelectronics *****END OF FILE****/
