/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
//
//  smem.cpp - This deals with all shared memory functions needed for 
//  the MAPI component of Communicator
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include <windows.h>
#include <windowsx.h>

#include "mapismem.h"

#ifndef ZeroMemory
#include <memory.h>
#define ZeroMemory(PTR, SIZE) memset(PTR, 0, SIZE)
#endif // ZeroMemory

// 
// *create new* shared memory chunk 
// once this is created, use the pointer
// to the segment to to store data 
// e.g.: 
//     lpString = "string for communicator"; 
//     lstrcpy((LPSTR)pData->m_buf[0], lpString); 
//  
CSharedMem *
NSCreateSharedMemory(DWORD memSize, LPCTSTR memName, HANDLE *hSharedMemory)
{
#ifdef WIN32

  BOOL bExistedBefore;
  CSharedMem *pData;

  LPCTSTR szObjectName = memName; 
  DWORD dwSize = sizeof(CSharedMem) + memSize; 
  *hSharedMemory = CreateFileMapping( 
    (HANDLE)0xFFFFFFFF,0,PAGE_READWRITE,0,dwSize,szObjectName); 
  if(*hSharedMemory == 0) 
  { 
    return NULL; 
  } 
  bExistedBefore = (GetLastError() == ERROR_ALREADY_EXISTS); 
  if(bExistedBefore) 
  { 
  	return NULL;
  } 

  pData = (CSharedMem *)MapViewOfFile( 
            *hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, 0); 
  if(pData == NULL) 
  { 
    return NULL; 
  } 

  ZeroMemory(pData, dwSize); 
  pData->m_dwSize = memSize; 

  return pData;
#else
  CSharedMem 	*sMemChunk = NULL;
  DWORD dwSize = memSize = (sizeof(CSharedMem) + memSize); 

  if (sMemChunk != NULL)
	  return(sMemChunk);

  sMemChunk = (CSharedMem *) GlobalAllocPtr(GMEM_MOVEABLE, dwSize);
  ZeroMemory(sMemChunk, (size_t) dwSize);
  sMemChunk->m_dwSize = dwSize;   // Missing in Communicator code!
  return(sMemChunk);

#endif // WIN32
}

// 
// *open existing* shared memory chunk 
// once you have the pointer to the new segment 
// use this pointer to access data, e.g.: 
// 
CSharedMem *
NSOpenExistingSharedMemory(LPCTSTR memName, HANDLE *hSharedMemory)
{
#ifdef WIN32
  CSharedMem *pData;
  DWORD dwSize;

  LPCTSTR szObjectName = memName; 
  *hSharedMemory = OpenFileMapping( 
    PAGE_READWRITE,FALSE,szObjectName); 
  if(*hSharedMemory == 0) 
  { 
    return NULL; 
  } 

  pData = (CSharedMem *)MapViewOfFile( 
              *hSharedMemory,FILE_MAP_ALL_ACCESS,0,0,0); 
  if(pData == NULL) 
  { 
    return NULL; 
  } 
  
  dwSize = pData->m_dwSize; 
  return pData;
#else

  return(NULL);   // In Win16, this is really meaningless...

#endif
}  

// 
// to close shared memory segment
// 
void
NSCloseSharedMemory(CSharedMem *pData, HANDLE hSharedMemory)
{
#ifdef WIN32
  if(pData != 0) 
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

	if (pData != NULL)
	{
		GlobalFreePtr(pData);
		pData = NULL;
	}

#endif // WIN32
}
