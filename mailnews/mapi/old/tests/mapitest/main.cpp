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
#include <windows.h>
#include <windowsx.h>
#include <string.h>
#include <stdlib.h>
#ifndef MAPI_OLE	// Because MSFT doesn't do this for us :-(
#include <mapi.h>
#endif

#include "port.h"
#include "resource.h"

#ifndef WM_PAINTICON
#define WM_PAINTICON              0x26
#endif // WM_PAINTICON

/* 
 * Forward Declarations...
 */
BOOL      InitInstance(HINSTANCE hInstance, int nCmdShow);
BOOL CALLBACK LOADDS MyDlgProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
extern void ProcessCommand(HWND hWnd, int id, HWND hCtl, UINT codeNotify);
BOOL      OpenMAPI(void);
void      CloseMAPI(void);

/*  
 * Global variables
 */
HINSTANCE   hInst;
HWND      hWnd;    

#ifdef WIN16
HICON	hIconApp;
#endif

char NEAR szAppName[] = "Netscape QA Helper";
char NEAR szShortAppName[] = "QAHelper";
char szClassName[] = "Netscape_QAHelper_Class_Name";

void
AppCleanup(void)
{
  extern void    DoMAPILogoff(HWND hWnd);
  static BOOL isDone = FALSE;

  if (isDone)
    return;

  extern LHANDLE    mapiSession;

  if (mapiSession != 0)
  {
    DoMAPILogoff(hWnd);
  }

  CloseMAPI();
  isDone = TRUE;
}

BOOL 
InitInstance(HINSTANCE hInstance, int nCmdShow)
{
  /* Create a main window for this application instance.  */
  hWnd = CreateDialog((HINSTANCE) hInstance, 
            MAKEINTRESOURCE(ID_DIALOG),
            (HWND) NULL, (DLGPROC) MyDlgProc);
  
  if (!hWnd)
    return FALSE;
  else
    return TRUE;
}

BOOL InitApp(void)
{
#ifndef WIN16
  WNDCLASS wc;
  wc.style         = 0; 
  wc.lpfnWndProc   = DefDlgProc; 
  wc.cbClsExtra    = 0; 
  wc.cbWndExtra    = DLGWINDOWEXTRA; 
  wc.hInstance     = hInst; 
  wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_APP)); 
  wc.hCursor       = LoadCursor(0, IDC_ARROW); 
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszMenuName  = NULL; 
  wc.lpszClassName = szClassName;
  
  if(!RegisterClass(&wc))
    return FALSE;

#else                
  hIconApp = LoadIcon(hInst, MAKEINTRESOURCE(ID_ICON_APP));
#endif
  
  return TRUE;
} // end InitApp

// Win Main 
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  MSG     msg;
  
  hInst = hInstance;
  
  if (!InitApp()) 
  { 
    return FALSE;
  }
  
  if (!InitInstance(hInstance, nCmdShow)) 
  {    
    return FALSE;
  }

  if (!OpenMAPI()) 
  {    
    return FALSE;
  }

  ShowWindow(hWnd, SW_SHOW);
  
  // Start the application
  while ((GetMessage(&msg, (HWND) NULL, (UINT) NULL, (UINT) NULL)))
  {
    if(IsDialogMessage(hWnd, &msg))
      continue;
    
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return(msg.wParam);
}

BOOL CALLBACK LOADDS 
MyDlgProc(HWND hWndMain, UINT wMsg, WPARAM wParam, LPARAM lParam) 
{
  switch (wMsg) 
  {
  case WM_INITDIALOG: 
    {
      hWnd = hWndMain;
	    SetDlgItemText(hWnd, ID_EDIT_ROW, "0");
    }
    break;
    
  case WM_CLOSE:
    {
      DestroyWindow(hWnd);
      break;
    }

  case WM_DESTROY:
    {
      AppCleanup();

#ifndef WIN16
      UnregisterClass(szClassName, hInst);
#else              
      // Destroy the 16 bit windows icon
      if(hIconApp != 0)
        DestroyIcon(hIconApp);
#endif
      
      PostQuitMessage(0);
      break;
    }
  case WM_COMMAND:
    HANDLE_WM_COMMAND(hWnd, wParam, lParam, ProcessCommand);
    break;
    
  case WM_QUERYDRAGICON:     
#ifdef WIN16
    return (BOOL)hIconApp;
#endif
    
  case WM_PAINTICON:
#ifdef WIN16    
    SetClassWord(hWnd, GCW_HICON, 0);
    
    // fall trough
  case WM_PAINT: 
    {
      if(!IsIconic(hWnd))
        return FALSE;
      PAINTSTRUCT ps;
      HDC hDC = BeginPaint(hWnd, &ps);
      SetMapMode(hDC, MM_TEXT);
      DrawIcon(hDC, 2, 2, hIconApp);
      EndPaint(hWnd, &ps);
      break;
    } 
#endif //WIN16
    break;    // RICHIE - if this is not here NT 3.51 Pukes!!!!  

  default:
    return FALSE;
  }
  
  //~~av  return (DefWindowProc(hWnd, wMsg, wParam, lParam));
  return TRUE;
}
