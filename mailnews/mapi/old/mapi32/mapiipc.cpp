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
//  MAPI IPC Routines
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include <windows.h>
#include <windowsx.h>
#include <nscpmapi.h>   // Should live in Communicator

#include "resource.h"
#include "mapiipc.h"
#include "mapismem.h"
#include "trace.h"

#ifndef WIN32
#include <string.h>
#endif 

//
// Necessary variables...
//
static LONG       instanceCount = 0;
HWND				      hWndMAPI = NULL;
char				      szClassName[] = "NetscapeMAPIClient";
char              szWindowName[] = "NetscapeMAPI";

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
      hWndMAPI = hWndMain;
    }
    break;
    
  case WM_CLOSE:
//    DestroyWindow(hWndMain);
    break;
    
  case WM_DESTROY:
    hWndMain = NULL;
    break;

  case WM_COMMAND:
	  HANDLE_WM_COMMAND(hWndMAPI, wParam, lParam, ProcessCommand);
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
  hWndMAPI = CreateDialog((HINSTANCE) hInstance, 
        MAKEINTRESOURCE(ID_DIALOG_QAHOOK),
        (HWND) NULL, (DLGPROC) MyDlgProc);
   ******/
  hWndMAPI = CreateWindow(
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
  if (!hWndMAPI)
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
  if (hWndMAPI != NULL)
    return TRUE;

  if (!InitApp()) 
  { 
    return FALSE;
  }
  
  if (!InitInstance(hInstance)) 
  {    
    return FALSE;
  }
  
//  ShowWindow(hWndMAPI, SW_SHOW);    Just for jollies
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
DWORD nsMAPI_OpenAPI(void)
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
void nsMAPI_CloseAPI(void)
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
SendMAPIRequest(HWND hWnd, 
                DWORD mapiRequestID, 
                MAPIIPCType *ipcInfo)
{
LRESULT         returnVal = 0;
COPYDATASTRUCT  cds;

  if (!InitDLL())
  {
    return 0;
  }

  cds.dwData = mapiRequestID;
  cds.cbData = sizeof(MAPIIPCType);
  cds.lpData = ipcInfo;

  // Make the call into Communicator
  returnVal = SendMessage(hWnd, WM_COPYDATA, (WPARAM) hWndMAPI, (LPARAM) &cds);

  // Now kill the window...
  DestroyWindow(hWndMAPI);
  hWndMAPI = NULL;
  UnregisterClass(szClassName, hInstance);
  
  return returnVal;
}
