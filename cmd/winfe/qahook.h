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
// This is a header file for the QA hooks that are getting added
// to Communicator. 
//
// Written by: Rich Pizzarro (rhp)
//
#ifndef _QAHOOK
#define _QAHOOK

//
// Need this for Win16 since it is an undocumented message
//
#ifndef WIN32

#define WM_COPYDATA                     0x004A
//#define WM_COPYDATA                     0x7666

/*
 * lParam of WM_COPYDATA message points to...
 */
typedef struct tagCOPYDATASTRUCT {
    DWORD 	dwData;
    DWORD 	cbData;
    LPVOID 	lpData;
} COPYDATASTRUCT, *PCOPYDATASTRUCT;

#endif // ifndef WIN32

// A chunk of shared memory will be identified by name: 

#define SHARED_MEMORY_CHUNK_NAME "NSCP_QA_SMEM" 
#define SHARED_MEMORY_CHUNK_SIZE (16 * 1024) 

// The following structure will be stored in the shared memory 
// and will be used to pass data back and forth 

#pragma pack(4) 

typedef struct  
{ 
  DWORD m_dwSize;               // size of the shared memory block 
  DWORD m_dwBytesUsed;          // size of data in m_Data 
  BYTE  m_buf[1]; 
} CSharedData; 

#pragma pack() 

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
CSharedData     *OpenExistingSharedMemory(void);

// 
// to close shared memory segment
// 
void            CloseSharedMemory();

//
// Defines needed for requests being made with the WM_COPYDATA call...
//
typedef enum {
	QA_OLGETCOUNT = 0,
	QA_OLGETVISIBLECOUNT,
	QA_OLGETTEXT,
	QA_OLSETFOCUSLINE,
	QA_OLGETFOCUSLINE,
	QA_OLGETCOLCOUNT,
	QA_OLGETNUMCHILDREN,
	QA_OLSCROLLINTOVIEW,
  QA_OLGETISCOLLAPSED
} QA_REQUEST_TYPE;

#endif	// _QAHOOK
