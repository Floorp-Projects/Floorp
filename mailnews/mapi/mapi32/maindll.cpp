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
//  DLLMain to get a handle on an hInstance 
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
#include <windows.h>

//  
// global variables
//
HINSTANCE hInstance;

//
// DLL entry
//  
#ifdef WIN32
/****************************************************************************
   FUNCTION: DllMain(HANDLE, DWORD, LPVOID)

   PURPOSE:  DllMain is called by Windows when
             the DLL is initialized, Thread Attached, and other times.
             Refer to SDK documentation, as to the different ways this
             may be called.
             
             The DllMain function should perform additional initialization
             tasks required by the DLL.  In this example, no initialization
             tasks are required.  DllMain should return a value of 1 if
             the initialization is successful.
           
*******************************************************************************/
BOOL APIENTRY DllMain(HANDLE hInstLocal, DWORD ul_reason_being_called, LPVOID lpReserved) 
{
  hInstance = (HINSTANCE)hInstLocal;

  if (hInstance != NULL) 
	  return 1;
  else  
	  return 0;
}

#else  // WIN16


//--------------------------------------------------------------------
// LibMain( hInstance, wDataSegment, wHeapSize, lpszCmdLine ) : WORD
//
//    hInstance      library instance handle
//    wDataSegment   library data segment
//    wHeapSize      default heap size
//    lpszCmdLine    command line arguments
//
//--------------------------------------------------------------------

int CALLBACK LibMain(HINSTANCE hInstLocal, WORD wDataSegment, WORD wHeapSize, LPSTR lpszCmdLine) 
{
  hInstance = hInstLocal;
  /* return result 1 = success; 0 = fail */
  if (hInstance != NULL) 
	  return 1;
  else
	  return 0;
}

#endif // WIN16

