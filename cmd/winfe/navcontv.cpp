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
#include "navcontv.h"
#include "htrdf.h"
#include "rdfliner.h"
#include "pain.h"
#include "navcntr.h"
//-------------------------------------------------------------------------------------
// Start Nav center Content View.
//-------------------------------------------------------------------------------------


//IMPLEMENT_DYNAMIC(CContentView, CScrollView)
IMPLEMENT_DYNAMIC(CContentView, CView)
BEGIN_MESSAGE_MAP(CContentView, CView)
	//{{AFX_MSG_MAP(CMainFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(WM_NAVCENTER_QUERYPOSITION, OnNavCenterQueryPosition)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
CContentView::CContentView()
{
    m_pChildSizeInfo = NULL;
}

CContentView::~CContentView()
{
    ASSERT(m_pChildSizeInfo == NULL);
}

LRESULT CContentView::OnNavCenterQueryPosition(WPARAM wParam, LPARAM lParam)
{
    NAVCENTPOS *pPos = (NAVCENTPOS *)lParam;
    
    //  We like being in the middle.
    pPos->m_iYDisposition = 0;
    
    //  We like being this many units in size.
    pPos->m_iYVector = 100;
    
    //  Handled.
    return(NULL);
}

// MHW need this to fool MFC.  We have the CView without a CDocument object.
int CContentView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	return 0;
}

void CContentView::OnMouseMove( UINT nFlags, CPoint point )
{
}

void CContentView::OnDraw(CDC* pDC)  // overridden to draw this view
{
}

BOOL CALLBACK QuerySizeCallback(HWND hwnd, LPARAM lParam)  {
    CContentView *pThis = (CContentView *)lParam;
    
    //  Must be direct child and visible.
    if(GetParent(hwnd) == pThis->GetSafeHwnd() && ::IsWindowVisible(hwnd)) {
        NAVCENTPOS ncp;
        memset(&ncp, 0, sizeof(ncp));
        
        LRESULT lHandled = SendMessage(hwnd, WM_NAVCENTER_QUERYPOSITION, 0, (LPARAM)&ncp);
        ASSERT(lHandled == NULL);  // You must handle this message.
        ncp.m_hChild = hwnd;
        
        pThis->AddChildSizeInfo(&ncp);
    }
    
    return(TRUE);
}

void CContentView::AddChildSizeInfo(NAVCENTPOS *pPreference)
{
    if(m_pChildSizeInfo == NULL) {
        m_pChildSizeInfo = XP_ListNew();
    }
    
    NAVCENTPOS *pNew = (NAVCENTPOS *)XP_ALLOC(sizeof(NAVCENTPOS));
    memcpy(pNew, pPreference, sizeof(NAVCENTPOS));
    
    XP_ListAddObject(m_pChildSizeInfo, pNew);
}

void CContentView::OnActivateView( BOOL bActivate, CView* pActivateView, CView* pDeactiveView )
{
	CView *pView = (CView*)GetDescendantWindow( NC_IDW_OUTLINER );
	// The default implementation is to give the focus to RDF tree view( NC_IDW_OUTLINER).
	// If we can not find a RDF tree view, we will call the base class to set focus properly.
	if (pView)
		pView->SetFocus();
	else
		CView::OnActivateView(bActivate, pActivateView,pDeactiveView); 
}


void CContentView::OnSize( UINT nType, int cx, int cy )
{
	CView::OnSize(nType, cx, cy);
	
	//  Calculate the information.
	CalcChildSizes();
}

void CContentView::CalcChildSizes()
{
    //  Ask each descendant window what size it would like to be.
    ::EnumChildWindows(GetSafeHwnd(), QuerySizeCallback, (LPARAM)this);

    CRect crClient;
    GetClientRect(crClient);
    int cx = crClient.Width();
    int cy = crClient.Height();

    //  Total the sizes of all the child window vectors.
    //  This will let us scale by percentage.
    XP_List *pTraverse = m_pChildSizeInfo;
    NAVCENTPOS *pPos = NULL;
    int iTotalHeight = 0;
    while(pPos = (NAVCENTPOS *)XP_ListNextObject(pTraverse)) {
        iTotalHeight += pPos->m_iYVector;
    }
    
    //  Find the child with the lowest Y disposition.
    int iCurPos;
    int iNextY = 0;
    while(XP_ListCount(m_pChildSizeInfo)) {
        pTraverse = m_pChildSizeInfo;
        iCurPos = INT_MAX;
        //  Find topmost position.
        while(pPos = (NAVCENTPOS *)XP_ListNextObject(pTraverse)) {
            if(pPos->m_iYDisposition < iCurPos) {
                iCurPos = pPos->m_iYDisposition;
            }
        }
        pTraverse = m_pChildSizeInfo;
        while(pPos = (NAVCENTPOS *)XP_ListNextObject(pTraverse)) {
            if(pPos->m_iYDisposition == iCurPos) {
                //  Resize it.
                float fPercent = (float)(pPos->m_iYVector) / (float)iTotalHeight;
                int iNewHeight = (int)(fPercent * (float)cy);
                
                //  Handle rounding errors.
                if(iNextY + iNewHeight > cy) {
                    iNewHeight = cy - iNextY;
                }
                
				RECT rect;
				::GetClientRect(pPos->m_hChild, &rect);
				::SetWindowPos(pPos->m_hChild, HWND_BOTTOM, 0, iNextY, cx, iNewHeight, SWP_NOZORDER);
                
				iNextY += iNewHeight;
                
                //  Remove this one from the list.
                XP_ListRemoveObject(m_pChildSizeInfo, pPos);
                XP_FREE(pPos);
                break;
            }
        }
    }
    
    if(m_pChildSizeInfo) {
        XP_ListDestroy(m_pChildSizeInfo);
        m_pChildSizeInfo = NULL;
    }
}

void CContentView::OnSetFocus ( CWnd * pOldWnd )
{
	CView *pView = (CView*)GetDescendantWindow( NC_IDW_OUTLINER );
	if (pView)
		pView->SetFocus();
}

//////////////////////////////////////////////////////////////////////////////
