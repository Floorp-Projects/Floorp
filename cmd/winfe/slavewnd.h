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

#ifndef __SlaveWindow_H
#define __SlaveWindow_H

//  Purpose is to create a hidden window.
//  External code can register to be called when the
//      window receives messages of a particular type.

typedef void (*SlaveWindowCallback)(UINT uMessage, WPARAM wParam, LPARAM lParam);

class CSlaveWindow  {
public:
    //  How to get the HWND, in case you need to target for
    //      events.
    HWND GetWindow();

    //  How to [un]register to handle an event.
    void *Register(UINT uEvent, SlaveWindowCallback swcCallback);
    BOOL UnRegister(void *pCookie);

    //  Needs to be called once the hInstance of the applciation
    //      is known.
    void InitInstance(HINSTANCE hApp);

//  Details you do not care about.
private:
    HWND m_hWnd;
    XP_List *m_pHandlers;
    HINSTANCE m_hInstance;
public:
    CSlaveWindow();
    ~CSlaveWindow();
    void WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

extern CSlaveWindow slavewnd;

//  Special messages we like to send to our slave window.
//  Must not go over 0x7FFF, WM_USER is 0x0400.
#define SLAVE_BASE (WM_USER + 0x7000)
#define SLAVE_EXITINSTANCE (SLAVE_BASE + 0x0000) //  theApp.ExitInstance entered.
#define SLAVE_INITINSTANCE (SLAVE_BASE + 0x0001) //  theApp.InitInstance entered.

#endif // __SlaveWindow_H
