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
#include "slavewnd.h"

//  Global must be present first, as other globals may want
//      to register early on.
#pragma warning(disable:4074)
#pragma init_seg(compiler)
CSlaveWindow slavewnd;
#pragma warning(default:4074)

struct SlaveStruct  {
    UINT m_msg;
    SlaveWindowCallback m_swc;
};

const char *pSlaveWindowClass = "NetscapeSlaveClass";
const char *pSlaveWindowTitle = "NetscapeSlaveWindow";

CSlaveWindow::CSlaveWindow()
{
    m_hWnd = NULL;
    m_hInstance = NULL;

    m_pHandlers = XP_ListNew();
}

CSlaveWindow::~CSlaveWindow()
{
    if(m_hWnd)  {
        VERIFY(::DestroyWindow(m_hWnd));
        m_hWnd = NULL;
    }

    if(m_hInstance) {
        VERIFY(::UnregisterClass(pSlaveWindowClass, m_hInstance));
        m_hInstance = NULL;
    }

    if(m_pHandlers) {
        XP_List *pTraverse = m_pHandlers;
        SlaveStruct *pSlave = NULL;
        while(pSlave = (SlaveStruct *)XP_ListNextObject(pTraverse))    {
            delete pSlave;
        }

        XP_ListDestroy(m_pHandlers);
        m_pHandlers = NULL;
    }
}

LRESULT
CALLBACK
#ifndef _WIN32
_export
#endif
SlaveWindowProc(HWND hWnd,
                UINT uMsg,
                WPARAM wParam,
                LPARAM lParam)
{
    LRESULT lRetval = NULL;

    slavewnd.WindowProc(uMsg, wParam, lParam);

    lRetval = DefWindowProc(hWnd, uMsg, wParam, lParam);

    return(lRetval);
}

void CSlaveWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(m_pHandlers) {
        //  Go through list of handlers, see if anyone wanted this
        //      message.
        //  Keep a seperate list, so callbacks can add and remove
        //      themselves via the registration and not cause a
        //      crash.
        //  This also has the side effect of calling the callbacks
        //      in the order of registration, instead of in the
        //      reverse order.
        XP_List *pTraverse = m_pHandlers;
        XP_List *pTemp = NULL;
        SlaveStruct *pSlave = NULL;
        while(pSlave = (SlaveStruct *)XP_ListNextObject(pTraverse)) {
            if(pSlave->m_msg == uMsg)   {
                if(NULL == pTemp) {
                    pTemp = XP_ListNew();
                }
                if(pTemp) {
                    XP_ListAddObject(pTemp, (void *)pSlave);
                }
            }
        }
        if(pTemp) {
            pTraverse = pTemp;
            while(pSlave = (SlaveStruct *)XP_ListNextObject(pTraverse)) {
                //  Fire.
                pSlave->m_swc(uMsg, wParam, lParam);
            }
            XP_ListDestroy(pTemp);
            pTemp = NULL;
        }
    }
}

void CSlaveWindow::InitInstance(HINSTANCE hInst)
{
    //  Only do once.
    if(hInst != NULL && m_hInstance == NULL) {
        m_hInstance = hInst;

        //  Register the window class.
        WNDCLASS wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = SlaveWindowProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = pSlaveWindowClass;
        ATOM aWndClass = ::RegisterClass(&wc);
        if(aWndClass != NULL)   {
            //  Instantiate the window.
            m_hWnd = ::CreateWindow(
                pSlaveWindowClass,
                pSlaveWindowTitle,
                WS_DISABLED,
                10,
                10,
                10,
                10,
                NULL,
                NULL,
                m_hInstance,
                NULL);
        }
        ASSERT(m_hWnd);
    }
}

HWND CSlaveWindow::GetWindow()
{
    //  Null until initted.
    return(m_hWnd);
}

void *CSlaveWindow::Register(UINT msg, SlaveWindowCallback func)
{
    void *pRetval = NULL;

    //  Can't call into nothing.
    //  Must have list.
    if(func && m_pHandlers)    {
        SlaveStruct *pNew = new SlaveStruct;
        if(pNew)    {
            pNew->m_msg = msg;
            pNew->m_swc = func;

            XP_ListAddObject(m_pHandlers, (void *)pNew);

            pRetval = (void *)pNew;
        }
    }

    return(pRetval);
}

BOOL CSlaveWindow::UnRegister(void *pCookie)
{
    BOOL bRetval = FALSE;

    //  Must have cookie, must have list.
    if(pCookie && m_pHandlers) {
        bRetval = XP_ListRemoveObject(m_pHandlers, pCookie);
        if(bRetval) {
            SlaveStruct *pDel = (SlaveStruct *)pCookie;
            delete pCookie;
        }
    }

    return(bRetval);
}
