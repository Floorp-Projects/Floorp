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

#include "netsprnt.h"
#include "netsvw.h"
#include "cxprint.h"

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CNetscapePreviewView,CPreviewView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CNetscapePreviewView, CPreviewView)
	//{{AFX_MSG_MAP(CPreviewView)
	ON_COMMAND(AFX_ID_PREVIEW_CLOSE, OnPreviewClose)
	ON_COMMAND(AFX_ID_PREVIEW_PRINT, OnPreviewPrint)
	ON_UPDATE_COMMAND_UI(AFX_ID_PREVIEW_NUMPAGE, OnUpdatePreviewNumpage)
	ON_UPDATE_COMMAND_UI(AFX_ID_PREVIEW_NEXT, OnUpdatePreviewNext)
	ON_UPDATE_COMMAND_UI(AFX_ID_PREVIEW_PRINT, OnUpdatePreviewPrint)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNetscapePreviewView construction/destruction

CNetscapePreviewView::CNetscapePreviewView()
{
    // TODO: add construction code here
}

void CNetscapePreviewView::OnPreviewClose()
{
	m_pToolBar->DestroyWindow();
	m_pToolBar = NULL;

	m_pPreviewInfo->m_nCurPage = m_nCurrentPage;
	((CNetscapeView *)m_pOrigView)->OnEndPrintPreview(m_pPreviewDC, m_pPreviewInfo,
									CPoint(0, 0), this);
}

void CNetscapePreviewView::OnPreviewPrint()
{
	CNetscapeView* pOrigView = (CNetscapeView *)m_pOrigView;
	OnPreviewClose();               // force close of Preview

    //  Route through view.
	if(pOrigView && pOrigView->GetFrame() && pOrigView->GetFrame()->GetActiveWinContext() && pOrigView->GetFrame()->GetActiveWinContext()->GetPane())    {
        pOrigView->GetFrame()->GetActiveWinContext()->GetView()->OnFilePrint();
	}
}

void CNetscapePreviewView::OnUpdatePreviewPrint(CCmdUI* pCmdUI)
{
    //  Route through view.
	CNetscapeView* pOrigView = (CNetscapeView *)m_pOrigView;
	if(pOrigView && pOrigView->GetFrame() && pOrigView->GetFrame()->GetActiveWinContext() && pOrigView->GetFrame()->GetActiveWinContext()->GetPane())    {
        pOrigView->GetFrame()->GetActiveWinContext()->GetView()->OnUpdateFilePrint(pCmdUI);
	}
}

void CNetscapePreviewView::OnUpdatePreviewNext(CCmdUI* pCmdUI)
{
    //  Call the base.
    CPreviewView::OnUpdateNextPage(pCmdUI);

    //  Only enable if we are not on the last page.
	CNetscapeView* pOrigView = (CNetscapeView *)m_pOrigView;
    if(pOrigView && pOrigView->m_pPreviewContext)    {
        pCmdUI->Enable(pOrigView->m_pPreviewContext->LastPagePrinted() != pOrigView->m_pPreviewContext->PageCount() ? TRUE : FALSE);
    }
}

void CNetscapePreviewView::OnUpdatePreviewNumpage(CCmdUI* pCmdUI)
{
    //  Call the base.
    CPreviewView::OnUpdateNumPageChange(pCmdUI);

    //  Only enable if there is more than 1 page and we aren't already showing two pages.
    if(m_nZoomState == ZOOM_OUT && m_nPages == 1) {
	    CNetscapeView* pOrigView = (CNetscapeView *)m_pOrigView;
        if(pOrigView && pOrigView->m_pPreviewContext)    {
            pCmdUI->Enable(pOrigView->m_pPreviewContext->PageCount() > 1 ? TRUE : FALSE);
        }
    }
}
