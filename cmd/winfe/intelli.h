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

#ifndef __IntellipointMouse_H
#define __IntellipointMouse_H

#ifndef _WIN32
#error intelli.h is Win32 only
#endif

#define MOUSEWHEEL_KEYS(wparam, lparam) ((WORD)LOWORD(wparam))
#define MOUSEWHEEL_DELTA(wparam, lparam) ((short)HIWORD(wparam))
#define MOUSEWHEEL_X(wparam, lparam) ((short)LOWORD(lparam))
#define MOUSEWHEEL_Y(wparam, lparam) ((short)HIWORD(lparam))

extern UINT msg_MouseWheel;

class CMouseWheel   {
public:
    //  Determine number of lines to scroll once
    //      WHEEL_DELTA is accumulated.
    //  Be sure to check if == WHEEL_PAGESCROLL
    UINT ScrollLines();

//  Stuff you don't care about.
public:
    CMouseWheel();
    ~CMouseWheel();
private:
    void *m_pWatcher;
    UINT m_uScrollLines;
    void Update();
    HWND GetWheelWnd();
    friend void UpdateMouseSettings(UINT, WPARAM, LPARAM);
};

extern CMouseWheel intelli;

#endif // __IntellipointMouse_H
