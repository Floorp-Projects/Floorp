/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
//
//  Address Book API
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  March 1998
//
#include <windows.h>  
#include <string.h> 
#include <time.h>

#include <mapismem.h>
#include <nsstrseq.h>

#include "xpapi.h"
#include "trace.h"
#include "nabipc.h"
#include "nabutl.h"
#include "nsstrseq.h"
#include "nabmem.h"

#include "abapi.h"
#include "nabapi.h"

#define       MAX_MEMARRAY_ENTRIES      2000

//
// Necessary tracking variables...
//
nabMemTrackerType    *memArray = NULL;

//
// For remembering memory...how ironic.
//
void
SetPointerArray(LPVOID ptr, BYTE type)
{
int i;

  if (!memArray)  
    return;

  for (i=0; i<MAX_MEMARRAY_ENTRIES; i++)
  {
    if (memArray[i].lpMem == NULL)
    {
      memArray[i].lpMem = ptr;
      memArray[i].memType = type;
      break;
    }
  }
}

/*
 * NAB_Open() will start the connection to the API. It will initialize any necessary 
 * internal variables and return the major and minor version as defined here. This
 * will allow client applications to verify the version they are running against.  
 */
NABError  
NAB_Open(NABConnectionID *id, UINT_16 *majorVerNumber, UINT_16 *minorVerNumber)
{
  NABError      result;  
  NABIPCType    ipcInfo;

  TRACE("NAB_Open()\n");

  // First allocate a memory tracker array...
  if (memArray != NULL)
  {
    DWORD size = MAX_MEMARRAY_ENTRIES * sizeof(nabMemTrackerType);
    memArray = (nabMemTrackerType *)malloc(size);
    if (!memArray)
      return(NAB_MEMORY_FAILURE);

    memset(memArray, 0, size);
  }

  // Init before we start...
  TRACE("NAB_Open: SET RETURN SESSION TO NULL\n"); 
  *id = 0;
  *majorVerNumber = 0;
  *minorVerNumber = 0;

  //
  // Init the stuff we will need for WM_COPYDATA calls and anything else
  //
  if (!nsNAB_OpenAPI())
  {
    TRACE("NAB_Open: Can't open API\n"); 
    return(NAB_FAILURE);
  }

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(FALSE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_Open: Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem    *sMem;
  DWORD         memSize;
  HANDLE        hSharedMemory;
  NAB_OpenType  *openPtr;

  memSize = sizeof(NAB_OpenType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_Open: SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  openPtr = (NAB_OpenType *)sMem->m_buf;

  // memcpy(sMem->m_buf, ptr to my args, size of my args);
  // or openPtr->(type specific thing) = whatever

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_Open, &ipcInfo);

  // Check for IPC completion
  if (!openPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  // 
  // If successfull, then we need to get the information
  if (result == NAB_SUCCESS)
  {
    *id = openPtr->abConnection;
    *majorVerNumber = openPtr->majorVerNumber;
    *minorVerNumber = openPtr->minorVerNumber;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  TRACE("NAB_Open: Returning connection ID=%d\n", *id);
  TRACE("NAB_Open: API Version %d.%d\n", *majorVerNumber, *minorVerNumber);
  
	return(result);
}

/*
 * This closes the connection to the API. Any subsequent call to the NABAPI will fail with 
 * an NAB_NOT_OPEN return code.   
 */
NABError 
NAB_Close(NABConnectionID id)
{
  NABIPCType    ipcInfo;

  if (id == 0)
    return NAB_INVALID_CONNID;

  TRACE("NAB_Close()\n");
  if (!memArray)
  {
    free(memArray);
    memArray = NULL;
  }

  //
  // Close IPC stuff...
  //
  nsNAB_CloseAPI();
 
  //
  // This call locates a Communicator Window, if one is not running,
  // it bails out now.
  //
  if (Is_16_OR_32_BIT_CommunitorRunning() == 0)
  {
    TRACE("NAB_Close: Communicator not Running\n"); 
    return NAB_FAILURE;
  }

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_Close: Can't get Window\n"); 
    return NAB_FAILURE;
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for close call.
  //
  CSharedMem    *sMem;
  DWORD         memSize;
  HANDLE        hSharedMemory;
  NAB_CloseType *closePtr;

  memSize = sizeof(NAB_CloseType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_Close: SMEM Failure\n"); 
      return NAB_FAILURE;
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  closePtr = (NAB_CloseType *)sMem->m_buf;
  closePtr->abConnection = id;

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  NABError result = SendNABRequest(hWndIPC, NSCP_NAB_Close, &ipcInfo);

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  TRACE("NAB_Close: Done\n");
  return result;
}

/********************************************************
 * Address Book Information Calls
 ********************************************************/

/*
 * This call will return a list of pointers to the list of available address 
 * books in NABAddrBookDescType format. The memory for this array will be allocated 
 * by the NABAPI and the caller is responsible for freeing the memory with a call 
 * to the NAB_FreeMemory() function call. NOTE: The call to NAB_FreeMemory() must be
 * made for each entry in the returned array and not just a pointer to the array itself. 
 * The number of address books found will also be returned in the abookCount
 * argument to the call. If the returned list is NULL or the abookCount value is 
 * less than or equal to ZERO, no address books were found.  
 */
NABError
NAB_GetAddressBookList(NABConnectionID id, UINT_32 *abookCount, NABAddrBookDescType **aBooks[])
{
  NABError      result;  
  NABIPCType    ipcInfo;

  TRACE("NAB_GetAddressBookList()\n");
  // Some sanity checking...
  if (id == 0)
    return( NAB_INVALID_CONNID );

  if (!abookCount)
    return( NAB_MEMORY_FAILURE );

  *abookCount = 0;      // Set the address book count to zero

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_GetAddressBookList: Can't get Window\n"); 
    return(NULL);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                  *sMem;
  DWORD                       memSize;
  HANDLE                      hSharedMemory;
  NAB_GetAddressBookListType  *getPtr;

  memSize = sizeof(NAB_GetAddressBookListType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_GetAddressBookList: SMEM Failure\n"); 
      return(NULL);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  getPtr = (NAB_GetAddressBookListType *)sMem->m_buf;
  getPtr->abConnection = id;

  // memcpy(sMem->m_buf, ptr to my args, size of my args);
  // or openPtr->(type specific thing) = whatever

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_GetAddressBookList, &ipcInfo);

  // Check for IPC completion
  if (!getPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  // 
  // If successfull, then we need to get the information
  if (result == NAB_SUCCESS)
  {
    *abookCount = getPtr->abookCount;

    if (getPtr->abookCount > 0)
    {
      *aBooks = (NABAddrBookDescType **)malloc(getPtr->abookCount * sizeof(NABAddrBookDescType *) );
      if (!*aBooks)
      {
        result = NAB_MEMORY_FAILURE;
        goto EARLYOUT;
      }

      for (DWORD i=0; i < getPtr->abookCount; i++)
      {
        (*aBooks)[i] = (NABAddrBookDescType *)malloc(sizeof(NABAddrBookDescType) );
        if (!(*aBooks)[i])          
        {
          result = NAB_MEMORY_FAILURE;
          goto EARLYOUT;
        }
      }

      NSstringSeq seq = (NSstringSeq) LoadBlobToMemory((LPSTR) getPtr->blobFileName);
      if (seq)
      {
        DWORD loc = 0;
        for (DWORD i=0; i<getPtr->abookCount; i++)
        {
          // Get the combinations of:
          //     char *description; // Description of address book - MUST BE UNIQUE
          //     char *fileName;    // The file name of the address book
          LPSTR desc = NSStrSeqGet(seq, loc++);          
          (*aBooks)[i]->description = strdup(CheckNullString(desc));
          TRACE("Description %d: %s\n", i, desc);

          LPSTR fName = NSStrSeqGet(seq, loc++);
          (*aBooks)[i]->fileName = strdup(CheckNullString(fName));
          TRACE("Description %d: %s\n", i, fName);

          SetPointerArray(aBooks[i], NAB_ADDRBOOK_STRUCT);
        }
      }
    }
  }

  //
  // Now time to do some cleanup...
  //
EARLYOUT:
  NSCloseSharedMemory(sMem, hSharedMemory);
  TRACE("NAB_GetAddressBookList: Total entries found =%d\n", *abookCount);
  return(result);
}

/*
 * This call will return the default address book for the client. If the 
 * returned value is NULL, no default address book was found. The memory 
 * for this return value will be allocated by the ABAPI and the caller is 
 * responsible for freeing the memory with a call to the NAB_FreeMemory() 
 * function call.  
 */
NABError
NAB_GetDefaultAddressBook(NABConnectionID id, NABAddrBookDescType **aBook)
{
  TRACE("NAB_GetDefaultAddressBook()\n");
  NABError      result;  
  NABIPCType    ipcInfo;

  // Some sanity checking...
  if (id == 0)
    return( NAB_INVALID_CONNID );

  *aBook = NULL;

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_GetDefaultAddressBook: Can't get Window\n"); 
    return(NULL);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                     *sMem;
  DWORD                          memSize;
  HANDLE                         hSharedMemory;
  NAB_GetDefaultAddressBookType  *getPtr;

  memSize = sizeof(NAB_GetDefaultAddressBookType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_GetDefaultAddressBook: SMEM Failure\n"); 
      return(NULL);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  getPtr = (NAB_GetDefaultAddressBookType *)sMem->m_buf;
  getPtr->abConnection = id;
  getPtr->defaultFound = FALSE;

  // memcpy(sMem->m_buf, ptr to my args, size of my args);
  // or openPtr->(type specific thing) = whatever

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_GetDefaultAddressBook, &ipcInfo);

  // Check for IPC completion
  if (!getPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  // 
  // If successfull, then we need to get the information
  if (result == NAB_SUCCESS)
  {
    if (getPtr->defaultFound)
    {
      *aBook = (NABAddrBookDescType *)malloc( sizeof(NABAddrBookDescType) );
      if (!*aBook)
      {
        result = NAB_MEMORY_FAILURE;
        goto EARLYOUT;
      }

      // Get the combinations of:
      //     char *description; // Description of address book - MUST BE UNIQUE
      //     char *fileName;    // The file name of the address book
      (*aBook)->description = strdup(CheckNullString(getPtr->abookDesc));
      (*aBook)->fileName = strdup(CheckNullString(getPtr->abookFileName));
      SetPointerArray(*aBook, NAB_ADDRBOOK_STRUCT);
    }
  }

  //
  // Now time to do some cleanup...
  //
EARLYOUT:
  NSCloseSharedMemory(sMem, hSharedMemory);
  TRACE("NAB_GetDefaultAddressBook: Returning\n");
  return(result);
}

/* 
 * This call will create a local/personal address book with the name given.   
 */
NABError 
NAB_CreatePersonalAddressBook(NABConnectionID id, char *addrBookName, NABAddrBookDescType **addrBook)
{
  NABError      result;  
  NABIPCType    ipcInfo;

  TRACE("NAB_CreatePersonalAddressBook()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);
  
  if (!ValidNonNullString(addrBookName))
    return(NAB_INVALID_NAME);

  if (!addrBook)
    return(NAB_MEMORY_FAILURE);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_CreatePersonalAddressBook: Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_CreatePersonalAddressBookType  *createPtr;

  memSize = sizeof(NAB_CreatePersonalAddressBookType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_CreatePersonalAddressBook: SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  createPtr = (NAB_CreatePersonalAddressBookType *)sMem->m_buf;
  createPtr->abConnection = id;


  // Copy the name for the new address book...
  lstrcpy(createPtr->abookName, addrBookName);
  createPtr->abookFileName[0] = '\0';

  // memcpy(sMem->m_buf, ptr to my args, size of my args);
  // or openPtr->(type specific thing) = whatever

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_CreatePersonalAddressBook, &ipcInfo);

  // Check for IPC completion
  if (!createPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  // 
  // If successfull, then we need to get the information
  if (result == NAB_SUCCESS)
  {
    (*addrBook) = (NABAddrBookDescType * )malloc(sizeof(NABAddrBookDescType));
    if (!(*addrBook))
    {
      NSCloseSharedMemory(sMem, hSharedMemory);
      return(NAB_MEMORY_FAILURE);
    }
    (*addrBook)->description = strdup(CheckNullString((LPSTR) createPtr->abookName));
    (*addrBook)->fileName = strdup(CheckNullString((LPSTR) createPtr->abookFileName));
    SetPointerArray(*addrBook, NAB_ADDRBOOK_STRUCT);
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  TRACE("NAB_CreatePersonalAddressBook: Returning ABook Name  = %s\n", (*addrBook)->description);
  TRACE("NAB_CreatePersonalAddressBook: Returning ABook Fname = %s\n", (*addrBook)->fileName);  
	return(result);
}

/********************************************************
 * Memory Handling Calls
 ********************************************************/

/*
 * This call frees memory allocated by the NABAPI.   
 */
NABError 
NAB_FreeMemory(void *ptr)
{
  int   i;
  BOOL  found = FALSE;

  TRACE("NAB_FreeMemory()\n");
  if (!ptr)
  	return(NAB_SUCCESS);

  if (!memArray)
    return(NAB_SUCCESS);

  for (i=0; i<MAX_POINTERS; i++)
  {
    if (ptr == memArray[i].lpMem)
    {
      if (memArray[i].memType == NAB_ADDRBOOK_STRUCT)
      {
        found = TRUE;
        FreeAddrStruct((NABAddrBookDescType *)ptr);
        memArray[i].lpMem = NULL;
        memArray[i].memType = 0;
      }
    }
  }

  if (!found)
    free(ptr);

  ptr = NULL;
	return(NAB_SUCCESS);
}

/********************************************************
 * Utility Calls
 ********************************************************/
/*
 * This will save the address book specified by the addrBook parameter into a 
 * table formatted, HTML file. The fields to be saved should be passed into 
 * the call via the ldifFields parameter.  
 */
NABError 
NAB_SaveAddressBookToHTML(NABConnectionID id, NABAddrBookDescType *addrBook, 
                          char *fileName)
{
  NABError      result;  
  NABIPCType    ipcInfo;
  TRACE("NAB_SaveAddressBookToHTML()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  if (!addrBook)
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(addrBook->description))
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(fileName))
    return(NAB_FILE_ERROR);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_SaveAddressBookToHTML(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_SaveAddressBookToHTMLType      *savePtr;

  memSize = sizeof(NAB_SaveAddressBookToHTMLType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_SaveAddressBookToHTML(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  savePtr = (NAB_SaveAddressBookToHTMLType *)sMem->m_buf;
  savePtr->abConnection = id;

  // Copy the input parameters for this call...
  lstrcpy(savePtr->abookName, CheckNullString(addrBook->description));
  lstrcpy(savePtr->abookFileName, CheckNullString(addrBook->fileName));
  lstrcpy(savePtr->HTMLFileName, CheckNullString(fileName));

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_SaveAddressBookToHTML, &ipcInfo);

  // Check for IPC completion
  if (!savePtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  TRACE("NAB_SaveAddressBookToHTML(): Returning %d\n", result);
	return(result);
}

/*
 * This call will import an LDIF formatted file from a disk file to the address 
 * book specified by the addrBook parameter.  
 */
NABError 
NAB_ImportLDIFFile(NABConnectionID id, NABAddrBookDescType *addrBook, char *fileName, NABBool deleteFileWhenFinished)
{
  NABError      result;  
  NABIPCType    ipcInfo;

  TRACE("NAB_ImportLDIFFile()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  if (!addrBook)
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(addrBook->description))
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(fileName))
    return(NAB_FILE_NOT_FOUND);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_ImportLDIFFile(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_ImportLDIFFileType            *importPtr;

  memSize = sizeof(NAB_ImportLDIFFileType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_ImportLDIFFile(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  importPtr = (NAB_ImportLDIFFileType *)sMem->m_buf;
  importPtr->abConnection = id;

  // Copy the input parameters for this call...
  lstrcpy(importPtr->abookName, CheckNullString(addrBook->description));
  lstrcpy(importPtr->abookFileName, CheckNullString(addrBook->fileName));
  lstrcpy(importPtr->LDIFFileName, CheckNullString(fileName));
  importPtr->deleteImportFile = deleteFileWhenFinished;

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_ImportLDIFFile, &ipcInfo);

  // Check for IPC completion
  if (!importPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  TRACE("NAB_ImportLDIFFile(): Returning %d\n", result);
	return(result);
}

/*
 * This call will export the contents of an address book specified by the 
 * addrBook parameter to an LDIF formatted disk file.  
 */
NABError 
NAB_ExportLDIFFile(NABConnectionID id, NABAddrBookDescType *addrBook, char *fileName)
{
  NABError      result;  
  NABIPCType    ipcInfo;
  TRACE("NAB_ExportLDIFFile()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  if (!addrBook)
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(addrBook->description))
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(fileName))
    return(NAB_FILE_ERROR);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_ExportLDIFFile(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_ExportLDIFFileType            *exportPtr;

  memSize = sizeof(NAB_ExportLDIFFileType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_ExportLDIFFile(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  exportPtr = (NAB_ExportLDIFFileType *)sMem->m_buf;
  exportPtr->abConnection = id;

  // Copy the input parameters for this call...
  lstrcpy(exportPtr->abookName, CheckNullString(addrBook->description));
  lstrcpy(exportPtr->abookFileName, CheckNullString(addrBook->fileName));
  lstrcpy(exportPtr->LDIFFileName, CheckNullString(fileName));

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_ExportLDIFFile, &ipcInfo);

  // Check for IPC completion
  if (!exportPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  TRACE("NAB_ExportLDIFFile(): Returning %d\n", result);
	return(result);
}

/********************************************************
 * Individual Entry Calls
 ********************************************************/
                                              
/*
 * This call will get the User ID for the first person in the specified address 
 * book and it will also return the attributes for the user in the ldifEntry field. The memory for
 * this field must be freed by a call to NAB_FreeMemory(). The addrBook argument is 
 * passed back via the calls that return NABAddrBookDescType structures.
 * The call will also return the time this entry was last updated via the NABUpdateTime 
 * paramenter. The memory for this structure should be allocated by the
 * calling application. It will return NAB_SUCCESS if an entry was found or NAB_NOT_FOUND 
 * if no entries are found. If a user is found, ID for the user returned is
 * stored in the userID argument.  
 */
NABError 
NAB_GetFirstAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, 
                             NABUserID *userID, char **ldifEntry, 
                             NABUpdateTime *updTime)
{
  NABError      result;  
  NABIPCType    ipcInfo;
  TRACE("NAB_GetFirstAddressBookEntry()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  if (!addrBook)
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(addrBook->description))
    return(NAB_INVALID_ABOOK);

  // Reset time
  *updTime = 0;
  *userID = 0;

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_GetFirstAddressBookEntry(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_GetFirstAddressBookEntryType   *getPtr;

  memSize = sizeof(NAB_GetFirstAddressBookEntryType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_GetFirstAddressBookEntry(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  getPtr = (NAB_GetFirstAddressBookEntryType *)sMem->m_buf;
  getPtr->abConnection = id;

  // Copy the input parameters for this call...
  lstrcpy(getPtr->abookName, CheckNullString(addrBook->description));
  lstrcpy(getPtr->abookFileName, CheckNullString(addrBook->fileName));
  
  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_GetFirstAddressBookEntry, &ipcInfo);

  // Check for IPC completion
  if (!getPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  if (result == NAB_SUCCESS)
  {
    *userID = getPtr->userID;
    *updTime = getPtr->updateTime;
    *ldifEntry = strdup(CheckNullString(getPtr->userInfo));
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
  TRACE("NAB_GetFirstAddressBookEntry(): Returning %d\n", result);
	return(result);
}

/* 
 * This call will get the next address book entry from the specified address book and it will also 
 * return the attributes for the user in the ldifEntry field. The memory for
 * this field must be freed by a call to NAB_FreeMemory(). The addrBook argument is passed back 
 * via the calls that return NABAddrBookDescType structures. The call will also return the time this 
 * entry was last updated via the NABUpdateTime paramenter. The memory for this structure should be allocated by the
 * calling application. It will return NAB_SUCCESS if an entry was found or NAB_NOT_FOUND if the 
 * final entry has already been returned. If a user is found, ID for the user returned is stored 
 * in the userID argument.  
 */ 
NABError 
NAB_GetNextAddressBookEntry(NABConnectionID id, NABUserID *userID, 
                            char **ldifEntry, NABUpdateTime *updTime)
{
  NABError      result;  
  NABIPCType    ipcInfo;
  TRACE("NAB_GetNextAddressBookEntry()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  // Reset time
  *updTime = 0;
  *userID = 0;

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_GetNextAddressBookEntry(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_GetNextAddressBookEntryType   *getPtr;

  memSize = sizeof(NAB_GetNextAddressBookEntryType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_GetNextAddressBookEntry(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  getPtr = (NAB_GetNextAddressBookEntryType *)sMem->m_buf;
  getPtr->abConnection = id;

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_GetNextAddressBookEntry, &ipcInfo);

  // Check for IPC completion
  if (!getPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  if (result == NAB_SUCCESS)
  {
    *userID = getPtr->userID;
    *updTime = getPtr->updateTime;
    *ldifEntry = strdup(CheckNullString(getPtr->userInfo));
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
  TRACE("NAB_GetNextAddressBookEntry(): Returning %d\n", result);
	return(result);
}

/* 
 * This call will perform a query on the specified ldif attribute pair. The memory for the ldifEntry 
 * will be allocated by the API and must be freed by a call to NAB_FreeMemory(). The addrBook argument 
 * is passed back via the calls that return NABAddrBookDescType structures. The call will also return the time
 * this entry was last updated via the NABUpdateTime paramenter. The memory for this structure should be 
 * allocated by the calling application. It will return NAB_SUCCESS if an entry was found or NAB_NOT_FOUND 
 * if the search failed.  
 */
NABError 
NAB_FindAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, 
                         NABUserID *userID, char *ldifSearchAttribute, 
                         char **ldifEntry, NABUpdateTime *updTime)
{
  NABError      result;  
  NABIPCType    ipcInfo;
  TRACE("NAB_FindAddressBookEntry()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  /**
  if (!addrBook)
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(addrBook->description))
    return(NAB_INVALID_ABOOK);
  ***/

  if (ValidNonNullString(ldifSearchAttribute))
    *userID = 0;

  // Reset time
  *updTime = 0;

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_FindAddressBookEntry(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_FindAddressBookEntryType       *findPtr;

  memSize = sizeof(NAB_FindAddressBookEntryType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_FindAddressBookEntry(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  findPtr = (NAB_FindAddressBookEntryType *)sMem->m_buf;
  findPtr->abConnection = id;

  // Copy the input parameters for this call...
  findPtr->abookName[0] = '\0';
  findPtr->abookFileName[0] = '\0';

  if (!ValidNonNullString(ldifSearchAttribute))
  {
    findPtr->userID = *userID;
  }

  if (addrBook)
  {
    if (ValidNonNullString(addrBook->description))
    {
      lstrcpy(findPtr->abookName, CheckNullString(addrBook->description));
      lstrcpy(findPtr->abookFileName, CheckNullString(addrBook->fileName));
    }
  }

  if (ValidNonNullString(ldifSearchAttribute))
    lstrcpy(findPtr->ldifSearchAttribute, CheckNullString(ldifSearchAttribute));
  else
    findPtr->ldifSearchAttribute[0] = '\0';  

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_FindAddressBookEntry, &ipcInfo);

  // Check for IPC completion
  if (!findPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  if (result == NAB_SUCCESS)
  {
    *userID = findPtr->userID;
    *updTime = findPtr->updateTime;
    *ldifEntry = strdup(CheckNullString(findPtr->userInfo));
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
  TRACE("NAB_FindAddressBookEntry(): Returning %d\n", result);
	return(result);
}
    
/* 
 * This call will insert an individual address book entry (in LDIF format) into the (addrBookID) address 
 * book. The addrBook argument is passed back via the calls that return NABAddrBookDescType structures. 
 * If this user already exists in the specified address book, the operation will fail.  
 */
NABError 
NAB_InsertAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, 
                           char *ldifEntry, NABUserID *userID)
{
  NABError      result;  
  NABIPCType    ipcInfo;
  TRACE("NAB_InsertAddressBookEntry()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  if (!addrBook)
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(addrBook->description))
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(ldifEntry))
    return(NAB_INVALID_ENTRY);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_InsertAddressBookEntry(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_InsertAddressBookEntryType     *insertPtr;

  memSize = sizeof(NAB_InsertAddressBookEntryType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_InsertAddressBookEntry(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  insertPtr = (NAB_InsertAddressBookEntryType *)sMem->m_buf;
  insertPtr->abConnection = id;
  insertPtr->userID = 0;

  // Copy the input parameters for this call...
  lstrcpy(insertPtr->abookName, CheckNullString(addrBook->description));
  lstrcpy(insertPtr->abookFileName, CheckNullString(addrBook->fileName));
  lstrcpy(insertPtr->userInfo, CheckNullString(ldifEntry));
  
  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_InsertAddressBookEntry, &ipcInfo);

  // Check for IPC completion
  if (!insertPtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }
  else
  {
    *userID = insertPtr->userID;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  TRACE("NAB_InsertAddressBookEntry(): Returning %d\n", result);
	return(result);
}

/* This call will update the specified address book entry in the specified address book with the 
 * information passed in via the LDIF formatted entry. Attributes that exists will be replaced by the 
 * new entry and new attributes will be added to the user record. The addrBook argument is passed back 
 * via the calls that return NABAddrBookDescType structures.  If the entry is not found, the call will fail.  
 */
NABError 
NAB_UpdateAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, 
                           NABUserID userID, char *ldifEntry)
{
  NABError      result;  
  NABIPCType    ipcInfo;
  TRACE("NAB_UpdateAddressBookEntry()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  if (!addrBook)
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(addrBook->description))
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(ldifEntry))
    return(NAB_INVALID_ENTRY);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_UpdateAddressBookEntry(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_UpdateAddressBookEntryType     *updatePtr;

  memSize = sizeof(NAB_UpdateAddressBookEntryType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_UpdateAddressBookEntry(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  updatePtr = (NAB_UpdateAddressBookEntryType *)sMem->m_buf;
  updatePtr->abConnection = id;

  // Copy the input parameters for this call...
  lstrcpy(updatePtr->abookName, CheckNullString(addrBook->description));
  lstrcpy(updatePtr->abookFileName, CheckNullString(addrBook->fileName));
  lstrcpy(updatePtr->userInfo, CheckNullString(ldifEntry));
  updatePtr->userID = userID;
  
  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_UpdateAddressBookEntry, &ipcInfo);

  // Check for IPC completion
  if (!updatePtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
  TRACE("NAB_UpdateAddressBookEntry(): Returning %d\n", result);
	return(result);
}

/*
 * This call will delete the specified address book entry in the specified address book. The addrBook 
 * argument is passed back via the calls that return NABAddrBookDescType structures.  If the entry is 
 * not found, the call will fail.   
 */
NABError 
NAB_DeleteAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, 
                           NABUserID userID)
{
  NABError      result;  
  NABIPCType    ipcInfo;
  TRACE("NAB_DeleteAddressBookEntry()\n");

  if (id == 0)
    return(NAB_INVALID_CONNID);

  if (!addrBook)
    return(NAB_INVALID_ABOOK);

  if (!ValidNonNullString(addrBook->description))
    return(NAB_INVALID_ABOOK);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  HWND hWndIPC = GetCommunicatorIPCWindow(TRUE);
  if (NULL == hWndIPC)
  {
    TRACE("NAB_DeleteAddressBookEntry(): Can't get Window\n"); 
    return(NAB_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  srand( (unsigned)time( NULL ) );
  BuildMemName((LPSTR)ipcInfo.smemName, rand());

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem                         *sMem;
  DWORD                              memSize;
  HANDLE                             hSharedMemory;
  NAB_DeleteAddressBookEntryType     *deletePtr;

  memSize = sizeof(NAB_DeleteAddressBookEntryType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("NAB_DeleteAddressBookEntry(): SMEM Failure\n"); 
      return(NAB_MEMORY_FAILURE);
  }

  //
  // Here is where we would set any values in the share memory segment (sMem)
  // and copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  deletePtr = (NAB_DeleteAddressBookEntryType *)sMem->m_buf;
  deletePtr->abConnection = id;

  // Copy the input parameters for this call...
  lstrcpy(deletePtr->abookName, CheckNullString(addrBook->description));
  lstrcpy(deletePtr->abookFileName, CheckNullString(addrBook->fileName));
  deletePtr->userID = userID;
  
  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendNABRequest(hWndIPC, NSCP_NAB_DeleteAddressBookEntry, &ipcInfo);

  // Check for IPC completion
  if (!deletePtr->ipcWorked)
  {
    result = NAB_FAILURE;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
  TRACE("NAB_DeleteAddressBookEntry(): Returning %d\n", result);
	return(result);
}

BOOL
TackOnNextAttribute(LPSTR retLine, LPSTR attribValue, LPSTR attribName)
{
  if ( (!attribValue) || (!(*attribValue)) )
    return 0;

  lstrcat(retLine, attribName);
  lstrcat(retLine, attribValue);
  lstrcat(retLine, NAB_CRLF);
  return TRUE;
}

DWORD
StringOK(LPSTR attribValue, LPSTR attribName)
{
  if ( (!attribValue) || (!(*attribValue)) )
    return 0;
  else
    return ( (lstrlen(attribValue) + 1) + (lstrlen(attribName) + 2 /* 2 for CRLF */) );
}

char FAR *
NAB_FormatLDIFLine(char *firstName, 
                   char *lastName, 
                   char *generalNotes, 
                   char *city, 
                   char *state, 
                   char *email, 
                   char *title, 
                   char *addrLine1, 
                   char *addrLine2, 
                   char *zipCode, 
                   char *country, 
                   char *businessPhone, 
                   char *faxPhone, 
                   char *homePhone, 
                   char *organization, 
                   char *nickname, 
                   char *useHTML)
{
#define   TOTAL_ATTRIBS   17

  LPSTR   retLine  = NULL;
  DWORD   totalSize = 0;

  totalSize += StringOK(firstName,       "sn: ");                 
  totalSize += StringOK(lastName,        "givenname: ");          
  totalSize += StringOK(generalNotes,    "description: ");        
  totalSize += StringOK(city,            "locality: ");           
  totalSize += StringOK(state,           "st: ");                 
  totalSize += StringOK(email,           "mail: ");               
  totalSize += StringOK(title,           "title: ");              
  totalSize += StringOK(addrLine1,       "postOfficeBox: ");      
  totalSize += StringOK(addrLine2,       "streetaddress: ");      
  totalSize += StringOK(zipCode,         "postalcode: ");         
  totalSize += StringOK(country,         "countryname: ");        
  totalSize += StringOK(businessPhone,   "telephonenumber: ");    
  totalSize += StringOK(faxPhone,        "facsimiletelephonenumber :");
  totalSize += StringOK(homePhone,       "homephone: ");          
  totalSize += StringOK(organization,    "o: ");                  
  totalSize += StringOK(nickname,        "xmozillanickname: ");   
  totalSize += StringOK(useHTML,         "xmozillausehtmlmail: ");

  if (totalSize == 0)
    return NULL;

  totalSize++;  // Add one for NULL

  retLine = (char *)malloc(totalSize);
  if (!retLine)
    return NULL;
  memset(retLine, 0, totalSize);

  TackOnNextAttribute(retLine, firstName,       "sn: ");
  TackOnNextAttribute(retLine, lastName,        "givenname: ");
  TackOnNextAttribute(retLine, generalNotes,    "description: ");
  TackOnNextAttribute(retLine, city,            "locality: ");
  TackOnNextAttribute(retLine, state,           "st: ");
  TackOnNextAttribute(retLine, email,           "mail: ");
  TackOnNextAttribute(retLine, title,           "title: ");
  TackOnNextAttribute(retLine, addrLine1,       "postOfficeBox: ");
  TackOnNextAttribute(retLine, addrLine2,       "streetaddress: ");
  TackOnNextAttribute(retLine, zipCode,         "postalcode: ");
  TackOnNextAttribute(retLine, country,         "countryname: ");
  TackOnNextAttribute(retLine, businessPhone,   "telephonenumber: ");
  TackOnNextAttribute(retLine, faxPhone,        "facsimiletelephonenumber: ");
  TackOnNextAttribute(retLine, homePhone,       "homephone: ");
  TackOnNextAttribute(retLine, organization,    "o: ");
  TackOnNextAttribute(retLine, nickname,        "xmozillanickname: ");
  TackOnNextAttribute(retLine, useHTML,         "xmozillausehtmlmail: ");

  return retLine;
}
