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
#include "netsvw.h"
#include "xp_ncent.h"
#include "pain.h"
#include "navcntr.h"

CPaneCX *wfe_CreateNavCenterHTMLPain(HT_View htView, HWND hParent)
{
    CPaneCX *pRetval = NULL;

    CCreateContext cccGrid;
    cccGrid.m_pNewViewClass = RUNTIME_CLASS(CNetscapeView);
    cccGrid.m_pCurrentDoc = new CGenericDoc();
    if(cccGrid.m_pCurrentDoc) {
        CNetscapeView *pNewView = new CNetscapeView();
#ifdef MOZ_NGLAYOUT
        pNewView->NoWebWidgetHack();
#endif
        if(pNewView) {
            CRect crClient(0, 0, 50, 50);
            BOOL bCreated = pNewView->Create(NULL,
                                             "NS NavCenter HTML Pane",
                                             WS_VISIBLE,
                                             crClient,
                                             CWnd::FromHandle(hParent),
                                             NC_IDW_HTMLPANE,
                                             &cccGrid);
            if(bCreated) {
                CWinCX *pWinCX = new CWinCX((CGenericDoc *)cccGrid.m_pCurrentDoc, NULL, pNewView, MWContextPane, Pane);
                if(pWinCX) {
                    pWinCX->Initialize(pWinCX->CDCCX::IsOwnDC(), crClient);
                    XP_RegisterViewHTMLPane(htView, pWinCX->GetContext());
                    pRetval = VOID2CX(pWinCX, CPaneCX);
                }
            }
            if(NULL == pRetval) {
                delete pNewView;
                pNewView = NULL;
            }
        }
        if(NULL == pRetval) {
            delete cccGrid.m_pCurrentDoc;
            cccGrid.m_pCurrentDoc = NULL;
        }
    }
    return(pRetval);
}

