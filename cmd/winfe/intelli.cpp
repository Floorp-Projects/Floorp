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

#include "stdafx.h"
#include "intelli.h"
#include "slavewnd.h"

//  Be sure to handle WM_MOUSEWHEEL and this registered message
//      also.
UINT msg_MouseWheel =
#if _MSC_VER >= 1100
    RegisterWindowMessage(MSH_MOUSEWHEEL);
#else
    (UINT)-1;
#endif

//  Internal to this file.
UINT msg_ScrollLines =
#if _MSC_VER >= 1100
    RegisterWindowMessage(MSH_SCROLL_LINES);
#else
    (UINT)-1;
#endif

CMouseWheel intelli;

void UpdateMouseSettings(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //  Make sure we want to take a look at this.
    if(uMsg == WM_SETTINGCHANGE)    {
        //  What happened?
#if _MSC_VER >= 1100
        if(wParam == SPI_SETWHEELSCROLLLINES)   {
            //  Number of scroll lines changed.
            intelli.Update();
        }
#endif
    }
}

CMouseWheel::CMouseWheel()
{
    m_uScrollLines = 0;

    //  Initialize mouse settings.
    Update();

    //  Register to watch mouse settings.
    m_pWatcher = slavewnd.Register(WM_SETTINGCHANGE, UpdateMouseSettings);
}

CMouseWheel::~CMouseWheel()
{
    if(m_pWatcher)  {
        slavewnd.UnRegister(m_pWatcher);
        m_pWatcher = NULL;
    }
}

void CMouseWheel::Update()
{
    HWND hWheel = GetWheelWnd();

    //  Determine number of lines to scroll.
    m_uScrollLines = 3;

    if(sysInfo.m_bWinNT && sysInfo.m_dwMajor >= 4)  {
#if _MSC_VER >= 1100
        ::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &m_uScrollLines, 0);
#endif
    }
    else if(hWheel)    {
        m_uScrollLines = (UINT)::SendMessage(hWheel, msg_ScrollLines, 0, 0);
    }
}

HWND CMouseWheel::GetWheelWnd()
{
    HWND hRetval = NULL;
#if _MSC_VER >= 1100
    hRetval = ::FindWindow(MSH_WHEELMODULE_CLASS, MSH_WHEELMODULE_TITLE);
#endif
    return(hRetval);
}

UINT CMouseWheel::ScrollLines()
{
    return(m_uScrollLines);
}
