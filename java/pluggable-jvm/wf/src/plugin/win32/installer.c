/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: installer.c,v 1.1 2001/07/12 20:26:30 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include <windows.h>
#include <stdio.h>
//#include "dialogs.h"
//#include "ifuncns.h"

#define CLASS_NAME "WFInstall"

/* global variables */
HINSTANCE       hInst;

HANDLE          hAccelTable;

HWND            hDlgUninstall;
HWND            hDlgMessage;
HWND            hWndMain;

LPSTR           szEGlobalAlloc;
LPSTR           szEStringLoad;
LPSTR           szEDllLoad;
LPSTR           szEStringNull;
LPSTR           szTempSetupPath;

LPSTR           szClassName;
LPSTR           szUninstallDir;
LPSTR           szTempDir;
LPSTR           szOSTempDir;
LPSTR           szFileIniUninstall;
LPSTR           gszSharedFilename;

ULONG           ulOSType;
DWORD           dwScreenX;
DWORD           dwScreenY;

void Log(char* str)
{
  MessageBox(hWndMain, str, NULL, MB_ICONEXCLAMATION);
}


BOOL InitApplication(HINSTANCE hInstance)
{
  WNDCLASS wc;

  wc.style         = CS_DBLCLKS | CS_SAVEBITS | CS_BYTEALIGNWINDOW;
  wc.lpfnWndProc   = DefDlgProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = DLGWINDOWEXTRA;
  wc.hInstance     = hInstance;
  //wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_UNINSTALL));
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = szClassName;

  return(RegisterClass(&wc));
}

HRESULT Initialize(HINSTANCE hInstance)
{
  szClassName = (LPSTR)GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, 50);
  lstrcpy(szClassName, CLASS_NAME);
  Log(szClassName);
  return(0);
}

int APIENTRY WinMain(HINSTANCE hInstance,
		     HINSTANCE hPrevInstance, 
		     LPSTR lpszCmdLine, 
		     int nCmdShow)
{
  /***********************************************************************/
  /* HANDLE hInstance;       handle for this instance                    */
  /* HANDLE hPrevInstance;   handle for possible previous instances      */
  /* LPSTR  lpszCmdLine;     long pointer to exec command line           */
  /* int    nCmdShow;        Show code for main window display           */
  /***********************************************************************/

  MSG   msg;

  if(!hPrevInstance)
  {
    hInst = GetModuleHandle(NULL);
    Log("1");
    if(Initialize(hInst))
    {
      Log("2");     
      PostQuitMessage(1);
    }   
    else if(!InitApplication(hInst))
    {
      Log("3");
      PostQuitMessage(1);
    }
   }
  Log("4");
  while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
  return(msg.wParam);
} /*  End of WinMain */
