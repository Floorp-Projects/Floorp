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
#ifndef __SMEM_HPP__
#define __SMEM_HPP__

//
// Need this for Win16 since it is an undocumented message
//
#ifndef WIN32

#define WM_COPYDATA                     0x004A

/*
 * lParam of WM_COPYDATA message points to...
 */
typedef struct tagCOPYDATASTRUCT {
    DWORD         dwData;
    DWORD         cbData;
    LPVOID        lpData;
} COPYDATASTRUCT, *PCOPYDATASTRUCT;

# ifndef LPCTSTR
#   define LPCTSTR LPCSTR
# endif


#endif // ifndef WIN32

// The following structure will be stored in the shared memory 
// and will be used to pass data back and forth 

#pragma pack(4) 

typedef struct  
{ 
  DWORD m_dwSize;       // size of the shared memory block 
  BYTE  m_buf[1];       // this is the buffer of memory to be used
} CSharedMem; 

#pragma pack(4) 

// ******************************************************
// Public routines...
// ******************************************************
// 
// 
// *create new* shared memory chunk 
// once this is created, use the pointer
// to the segment to to store data 
// e.g.: 
//     lpString = "string for communicator"; 
//     lstrcpy((LPSTR)pData->m_buf[0], lpString); 
//     pData->m_dwBytesUsed = lstrlen(lpString) + 1; // count '\0' 
//  
CSharedMem *
NSCreateSharedMemory(DWORD memSize, LPCTSTR memName, HANDLE *hSharedMemory);

// 
// *open existing* shared memory chunk 
// once you have the pointer to the new segment 
// use this pointer to access data, e.g.: 
//
// This will return the pointer to the memory chunk as well as
// fill out the hSharedMemory argument that is needed for subsequent
// operations.
// 
//  if(pData->m_dwBytesUsed > 0) 
//  { 
//    // use pData->m_buf here 
//  } 
//
CSharedMem *
NSOpenExistingSharedMemory(LPCTSTR memName, HANDLE *hSharedMemory);

//
// You must pass in the pointer to the memory chunk as well as
// the hSharedMemory HANDLE to close shared memory segment
// 
void
NSCloseSharedMemory(CSharedMem *pData, HANDLE hSharedMemory);

#endif  // __SMEM_HPP__
