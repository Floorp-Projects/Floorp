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
//  NAB IPC Routines
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include <windows.h>
#include <windowsx.h>

#include "resource.h"

#include <mapismem.h>
#include "nabipc.h"
#include "trace.h"

#ifndef WIN32
#include <string.h>
#endif 

//
// Necessary variables...
//
static LONG       instanceCount = 0;
HWND				      hWndNAB = NULL;
char				      szClassName[] = "NetscapeNABClient";
char              szWindowName[] = "NetscapeNAB";

//
// External declares...
//
extern HINSTANCE  hInstance;

void 
ProcessCommand(HWND hWnd, int id, HWND hCtl, UINT codeNotify) 
{ 
	switch (id) 
	{
	case IDOK:   
	case IDCANCEL:
		{
			ShowWindow(hWnd, SW_HIDE);
		}
		
	default:
		;
	}
}

BOOL CALLBACK LOADDS 
MyDlgProc(HWND hWndMain, UINT wMsg, WPARAM wParam, LPARAM lParam) 
{
  switch (wMsg) 
  {
  case WM_INITDIALOG: 
    {
      hWndNAB = hWndMain;
    }
    break;
    
  case WM_CLOSE:
//    DestroyWindow(hWndMain);
    break;
    
  case WM_DESTROY:
    hWndMain = NULL;
    break;

  case WM_COMMAND:
	  HANDLE_WM_COMMAND(hWndNAB, wParam, lParam, ProcessCommand);
    break;

  default:
    return FALSE;
  }

  return TRUE;
}

BOOL 
InitInstance(HINSTANCE hInstance)
{
  //
  // Create a main window for this application instance.  
  //
  /* RICHIE - TRY SOME CHANGES!!!
  hWndNABI = CreateDialog((HINSTANCE) hInstance, 
        MAKEINTRESOURCE(ID_DIALOG_QAHOOK),
        (HWND) NULL, (DLGPROC) MyDlgProc);
   ******/
  hWndNAB = CreateWindow(
              szClassName,	// pointer to registered class name
              szWindowName,	// pointer to window name
              WS_CHILD,	    // window style
              -10,          	// horizontal position of window
              -10,	          // vertical position of window
              1,	          // window width
              1,	          // window height
              GetDesktopWindow(),	// handle to parent or owner window
              NULL,	        // handle to menu or child-window identifier
              hInstance,	  // handle to application instance
              NULL          // pointer to window-creation data
              );
  if (!hWndNAB)
    return FALSE;
  else
    return TRUE;
}

BOOL 
InitApp(void)
{
#ifdef WIN32
  WNDCLASS wc;
  wc.style         = 0; 
  wc.lpfnWndProc   = DefDlgProc; 
  wc.cbClsExtra    = 0; 
  wc.cbWndExtra    = DLGWINDOWEXTRA; 
  wc.hInstance     = hInstance; 
  wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(ID_ICON_APP)); 
  wc.hCursor       = LoadCursor(0, IDC_ARROW); 
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszMenuName  = NULL; 
  wc.lpszClassName = szClassName;
  
  if(!RegisterClass(&wc))
    return FALSE;
#endif
  
  return TRUE;
} // end InitApp

BOOL
InitDLL(void)
{
  if (hWndNAB != NULL)
    return TRUE;

  if (!InitApp()) 
  { 
    return FALSE;
  }
  
  if (!InitInstance(hInstance)) 
  {    
    return FALSE;
  }
  
//  ShowWindow(hWndNAB, SW_SHOW);    Just for jollies
  return(TRUE);
}

//*************************************************************
//* Calls exposed for rest of DLL...
//*************************************************************
//
// Purpose: Open the API
// Return: 1 on success
//         0 on failure
//
DWORD nsNAB_OpenAPI(void)
{
  if (instanceCount > 0)
  {
    return(1);
  }

  ++instanceCount;
  return(1);
}

//
// Purpose: Close the API
//
void nsNAB_CloseAPI(void)
{
  --instanceCount;
  if (instanceCount <= 0)
  {
    instanceCount = 0;
  }

  return;
}

//
// Send the actual request to Communicator
//
LRESULT
SendNABRequest(HWND hWnd, 
                DWORD nabRequestID, 
                NABIPCType *ipcInfo)
{
LRESULT         returnVal = 0;
COPYDATASTRUCT  cds;

  if (!InitDLL())
  {
    return 0;
  }

  cds.dwData = nabRequestID;
  cds.cbData = sizeof(NABIPCType);
  cds.lpData = ipcInfo;

  // Make the call into Communicator
  returnVal = SendMessage(hWnd, WM_COPYDATA, (WPARAM) hWndNAB, (LPARAM) &cds);

  // Now kill the window...
  DestroyWindow(hWndNAB);
  hWndNAB = NULL;
  UnregisterClass(szClassName, hInstance);
  
  return returnVal;
}
