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
#include "xp_ncent.h"

void laxpg_frameactive(CFrameGlue *pFrame)
{
    MWContext *pLastActive = NULL;

    if(pFrame) {
        if(pFrame->GetActiveContext() &&
           pFrame->GetActiveContext()->GetContext() &&
           FALSE == pFrame->GetActiveContext()->IsDestroyed()) {
            pLastActive = pFrame->GetActiveContext()->GetContext();
        }
        else if(pFrame->GetMainContext() &&
                pFrame->GetMainContext()->GetContext() &&
                FALSE == pFrame->GetMainContext()->IsDestroyed()) {
            pLastActive = pFrame->GetMainContext()->GetContext();
        }
    }

    if(pLastActive) {
        XP_SetLastActiveContext(pLastActive);
    }
}

void laxpg_contextactive(CFrameGlue *pFrame, CAbstractCX *pCX)
{
    MWContext *pLastActive = NULL;
    
    if(pFrame && pCX) {
        CFrameWnd *pFWnd = pFrame->GetFrameWnd();
        HWND hFWnd = pFWnd ? pFWnd->m_hWnd : NULL;
        if(hFWnd) {
            //  Must be active, or we do not want to reflect into
            //      xp active layer.
            HWND hFocus = ::GetFocus();
            if(hFocus && (hFocus == hFWnd || ::IsChild(hFWnd, hFocus))) {
                if(pCX->GetContext() && FALSE == pCX->IsDestroyed()) {
                    pLastActive = pCX->GetContext();
                }
            }
        }
    }
    
    if(pLastActive) {
        XP_SetLastActiveContext(pLastActive);
    }
}

void laxpg_init(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //  Register with FrameGlue to be notified when
    //      frame level activation changes.
    CFrameGlue::AddActiveNotifyCB(laxpg_frameactive);
    
    //  Register with GenView to be notified when
    //      view level activation changes.
    CFrameGlue::AddActiveContextCB(laxpg_contextactive);
}

void laxpg_exit(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CFrameGlue::RemoveActiveNotifyCB(laxpg_frameactive);
    CFrameGlue::RemoveActiveContextCB(laxpg_contextactive);
}

class CLastActiveXPGlue {
private:
    void *m_pInitCookie;
    void *m_pExitCookie;
public:
    CLastActiveXPGlue();
    ~CLastActiveXPGlue();
} laxpg;

CLastActiveXPGlue::CLastActiveXPGlue()
{
    m_pInitCookie = slavewnd.Register(SLAVE_INITINSTANCE, laxpg_init);
    m_pExitCookie = slavewnd.Register(SLAVE_EXITINSTANCE, laxpg_exit);
}

CLastActiveXPGlue::~CLastActiveXPGlue()
{
    if(m_pInitCookie) {
        slavewnd.UnRegister(m_pInitCookie);
        m_pInitCookie = NULL;
    }
    if(m_pExitCookie) {
        slavewnd.UnRegister(m_pExitCookie);
        m_pExitCookie = NULL;
    }
}

