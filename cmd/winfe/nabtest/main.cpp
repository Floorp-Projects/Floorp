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
#include <windows.h>
#include <windowsx.h>
#include <string.h>
#include <stdlib.h>

#include "port.h"
#include "resource.h"
#include "nabapi.h"

#ifndef WM_PAINTICON
#define WM_PAINTICON              0x26
#endif // WM_PAINTICON

/* 
 * Forward Declarations...
 */
BOOL      InitInstance(HINSTANCE hInstance, int nCmdShow);
BOOL CALLBACK LOADDS MyDlgProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
extern void ProcessCommand(HWND hWnd, int id, HWND hCtl, UINT codeNotify);
BOOL      OpenNAB(void);
void      CloseNAB(void);

/*  
 * Global variables
 */
HINSTANCE   hInst;
HWND        hWnd;    

#ifdef WIN16
HICON	hIconApp;
#endif

char NEAR szAppName[] = "Netscape AB Test";
char NEAR szShortAppName[] = "ABTEST";
char szClassName[] = "Netscape_ABTest_Class_Name";

void
AppCleanup(void)
{
  extern void    DoNAB_Close(HWND hWnd);
  static BOOL isDone = FALSE;

  if (isDone)
    return;

  extern NABConnectionID nabConn;  

  if (nabConn != 0)
  {
    DoNAB_Close(hWnd);
  }

  CloseNAB();
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

  if (!OpenNAB()) 
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
