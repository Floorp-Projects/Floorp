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
// Necessary hooks for QA partner to access custom objects within
// Communicator
//
#include <windows.h>
#include <windowsx.h>
#include "qahook.h"

static HANDLE		hSharedMemory = NULL;
static CSharedData	*pData = NULL;

// 
// *open existing* shared memory chunk 
// once you have the pointer to the new segment 
// use this pointer to access data, e.g.: 
// 
//  if(pData->m_dwBytesUsed > 0) 
//  { 
//    // use pData->m_buf here 
//  } 
//
CSharedData *
OpenExistingSharedMemory(void)
{
#ifdef WIN32

  DWORD dwSize;
  LPCTSTR szObjectName = SHARED_MEMORY_CHUNK_NAME; 

  if (pData != NULL)
	  return pData;

  hSharedMemory = OpenFileMapping( 
    FILE_MAP_WRITE /*PAGE_READWRITE*/,FALSE,szObjectName); 
  if(hSharedMemory == 0) 
  { 
    return NULL; 
  } 

  pData = (CSharedData *)MapViewOfFile( 
    hSharedMemory,FILE_MAP_ALL_ACCESS,0,0,0); 
  if(pData == NULL) 
  { 
    return NULL; 
  } 
  
  dwSize = pData->m_dwSize; 
  return pData;
#else
  return NULL;
#endif  // WIN32
}  

// 
// to close shared memory segment
// 
void
CloseSharedMemory(void)
{
#ifdef WIN32
  if (pData != 0) 
  { 
    UnmapViewOfFile(pData); 
    pData = 0; 
  } 

  if(hSharedMemory != 0) 
  { 
    CloseHandle(hSharedMemory); 
    hSharedMemory = 0; 
  } 
#else
  return;
#endif	// WIN32
}
