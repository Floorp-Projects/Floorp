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

