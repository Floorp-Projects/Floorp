/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "prtypes.h"
#include "nsVoidArray.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include <stdio.h>
#include <stdlib.h>
#include "resources.h"

#include <windows.h>

static char* class1Name = "TimerTest";

static HANDLE gInstance, gPrevInstance;
static nsVoidArray *gTimeouts = NULL;

static void CreateRepeat(PRUint32 aDelay);

void
MyCallback (nsITimer *aTimer, void *aClosure)
{
    printf("Timer executed with delay %d\n", (int)aClosure);
    
    if (gTimeouts->RemoveElement(aTimer) == PR_TRUE) {
      NS_RELEASE(aTimer);
    }
}

void
MyRepeatCallback (nsITimer *aTimer, void *aClosure)
{
    printf("Timer executed with delay %d\n", (int)aClosure);
    
    if (gTimeouts->RemoveElement(aTimer) == PR_TRUE) {
      NS_RELEASE(aTimer);
    }

    CreateRepeat((PRUint32)aClosure);
}

static void
CreateOneShot(PRUint32 aDelay)
{
    nsITimer *timer;

    NS_NewTimer(&timer);
    timer->Init(MyCallback, (void *)aDelay, aDelay);
    
    gTimeouts->AppendElement(timer);
}

static void
CreateRepeat(PRUint32 aDelay)
{
    nsITimer *timer;

    NS_NewTimer(&timer);
    timer->Init(MyRepeatCallback, (void *)aDelay, aDelay);
    
    gTimeouts->AppendElement(timer);
}

static void
CancelAll()
{
    int i, count = gTimeouts->Count();

    for (i=0; i < count; i++) {
        nsITimer *timer = (nsITimer *)gTimeouts->ElementAt(i);
	
	if (timer != NULL) {
	    timer->Cancel();
	    NS_RELEASE(timer);
	}
    }

    gTimeouts->Clear();
}

long PASCAL
WndProc(HWND hWnd, UINT msg, WPARAM param, LPARAM lparam)
{
  HMENU hMenu;

  switch (msg) {
    case WM_COMMAND:
      hMenu = GetMenu(hWnd);

      switch (LOWORD(param)) {
        case TIMER_EXIT:
	  ::DestroyWindow(hWnd);
	  exit(0);

        case TIMER_1SECOND:
	  CreateOneShot(1000);
	  break;
        case TIMER_5SECOND:
	  CreateOneShot(5000);
	  break;
        case TIMER_10SECOND:
	  CreateOneShot(10000);
	  break;

        case TIMER_1REPEAT:
	  CreateRepeat(1000);
	  break;
        case TIMER_5REPEAT:
	  CreateRepeat(5000);
	  break;
        case TIMER_10REPEAT:
	  CreateRepeat(10000);
	  break;

        case TIMER_CANCEL:
	  CancelAll();
	  break;

        default:
	  break;
    }

    default:
      break;
  }

  return DefWindowProc(hWnd, msg, param, lparam);
}

static HWND CreateTopLevel(const char* clazz, const char* title,
                                  int aWidth, int aHeight)
{
  // Create a simple top level window
  HWND window = ::CreateWindowEx(WS_EX_CLIENTEDGE,
                                 clazz, title,
                                 WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 aWidth, aHeight,
                                 HWND_DESKTOP,
                                 NULL,
                                 gInstance,
                                 NULL);

  ::ShowWindow(window, SW_SHOW);
  ::UpdateWindow(window);
  return window;
}

int PASCAL
WinMain(HANDLE instance, HANDLE prevInstance, LPSTR cmdParam, int nCmdShow)
{
  gInstance = instance;

  if (!prevInstance) {
    WNDCLASS wndClass;
    wndClass.style = 0;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = gInstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH) GetStockObject(LTGRAY_BRUSH);
    wndClass.lpszMenuName = class1Name;
    wndClass.lpszClassName = class1Name;
    RegisterClass(&wndClass);
  }

  // Create our first top level window
  HWND window = CreateTopLevel(class1Name, "Raptor HTML Viewer", 620, 400);

  gTimeouts = new nsVoidArray();

  // Process messages
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }
  return msg.wParam;
}

void main(int argc, char **argv)
{
  WinMain(GetModuleHandle(NULL), NULL, 0, SW_SHOW);
}
