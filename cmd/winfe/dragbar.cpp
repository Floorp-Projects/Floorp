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
#include "dragbar.h"
#include "navfram.h"
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif
#define CX_BORDER 2
#define CY_BORDER 2


CDragBar::CDragBar(CRect& crRect, CNSNavFrame* pDockFrame)
{
	//	Set our members.
	m_bDragging = FALSE;
	m_crTracker = crRect;
	m_dockedFrame = pDockFrame;
	m_bInDeskCoord = FALSE;
}

CDragBar::~CDragBar()
{
	//	If we were tracking, then let our clipped region go, etc.
	if(m_bDragging == TRUE)	{
		//	Release the bounds established on the mouse.
		::ClipCursor(NULL);
	}
}

void CDragBar::PostNcDestroy( )
{
	delete this;
}


void CDragBar::OnDestroy()
{
	CWnd::OnDestroy();
}

BEGIN_MESSAGE_MAP(CDragBar, CWnd)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_SETCURSOR()
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_DESTROY()
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

void CDragBar::SetRect(CRect& rect, BOOL inDeskCoord) 
{  
	m_crTracker = rect;
	m_bInDeskCoord = FALSE;

	MoveWindow(rect);
}

void CDragBar::GetRect(CRect& rect)
{
	rect = m_crTracker;
	if (m_bInDeskCoord) {
		GetDesktopWindow()->MapWindowPoints( m_parentView, &rect);
	}
}

void CDragBar::InitLoop()
{
	// handle pending WM_PAINT messages
	MSG msg;
	while (::PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_NOREMOVE))
	{
		if (!GetMessage(&msg, NULL, WM_PAINT, WM_PAINT))
			return;
		DispatchMessage(&msg);
	}
	m_bDitherLast = FALSE;
	m_rectLast.SetRectEmpty();

	CWnd* pWnd = CWnd::GetDesktopWindow();
	if (pWnd->LockWindowUpdate())
		m_pDC = pWnd->GetDCEx(NULL, DCX_WINDOW|DCX_CACHE|DCX_LOCKWINDOWUPDATE);
	else
		m_pDC = pWnd->GetDCEx(NULL, DCX_WINDOW|DCX_CACHE);
	ASSERT(m_pDC != NULL);
}

void CDragBar::CancelLoop()
{
	DrawFocusRect(TRUE);    // gets rid of focus rect
	ReleaseCapture();

	CWnd* pWnd = CWnd::GetDesktopWindow();
#ifdef XP_WIN32
	pWnd->UnlockWindowUpdate();
#else
	ASSERT(::IsWindow(m_hWnd));
	::LockWindowUpdate(NULL); 
#endif
	if (m_pDC != NULL)
	{
		pWnd->ReleaseDC(m_pDC);
		m_pDC = NULL;
	}
}

void CDragBar::DrawFocusRect(BOOL bRemoveRect)
{
	ASSERT(m_pDC != NULL);

	// default to thin frame
	CSize size(CX_BORDER, CY_BORDER);

	// determine new rect and size
	CRect rect;
	CBrush* pWhiteBrush = CBrush::FromHandle((HBRUSH)::GetStockObject(WHITE_BRUSH));
	if (bRemoveRect)
		size.cx = size.cy = 0;

#ifdef XP_WIN32
	CBrush *pBrush = CDC::GetHalftoneBrush();
	HBRUSH hOldBrush = NULL;
	if(pBrush != NULL)	{
		hOldBrush = (HBRUSH)SelectObject(m_pDC->m_hDC, pBrush->m_hObject);
	}
	m_pDC->PatBlt(m_crTracker.left, m_crTracker.top,
		m_crTracker.Width(), m_crTracker.Height(), PATINVERT);

	if(hOldBrush != NULL)	{
		SelectObject(m_pDC->m_hDC, hOldBrush);
	}
#else
	m_pDC->PatBlt(m_crTracker.left, m_crTracker.top, m_crTracker.Width(),
			m_crTracker.Height(), DSTINVERT);
#endif
	m_rectLast = rect;
	m_sizeLast = size;
}

void CDragBar::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if (!m_dockedFrame->IsTreeVisible())
		return;

	//	Capture mouse events.
	InitLoop();

	//	Mark the point at which the mouse went down.
	//	This can be used to calculate the relative distance of the movement of the tracker.
	m_cpLBDown = point;

	//	Set tracking state.
	m_bDragging = TRUE;


	CPoint cursorPt(point);
	MapWindowPoints( m_parentView, &cursorPt, 1 );
	m_parentView->MapWindowPoints( GetDesktopWindow(), &cursorPt, 1 );
	m_ptLast = cursorPt;

	CRect parentRect;
	CalcClientArea(&parentRect);
	::ClientToScreen(m_parentView->GetSafeHwnd(), &parentRect.TopLeft());
    ::ClientToScreen(m_parentView->GetSafeHwnd(), &parentRect.BottomRight());
	::ClipCursor(parentRect);

	if (!m_bInDeskCoord) {
		m_parentView->MapWindowPoints( GetDesktopWindow(), &m_crTracker);
		m_bInDeskCoord = TRUE;
	}
	//	Invert the tracker.
	Move(cursorPt);   // call it here to handle special keys
	Track();
}

void CDragBar::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_dockedFrame->CollapseWindow();
}

void CDragBar::EndDrag()
{
	CancelLoop();
	::ClipCursor(NULL);

	//	Set that we are no longer tracking.
	m_bDragging = FALSE;
	CRect tempRect(m_crTracker);
	GetDesktopWindow()->MapWindowPoints( m_parentView, &tempRect);
	MoveWindow(tempRect);
	Invalidate();
	m_dockedFrame->m_pSelector->Invalidate();
	m_parentView->RecalcLayout();
}

void CDragBar::Move( CPoint point )
{
	CPoint ptOffset = point - m_ptLast;
	//	If we're currently tracking, simply update the coords.
	if(m_bDragging == TRUE)	{
		//	Turn off the tracker.
		DrawFocusRect();

		if (m_orientation == DOCKSTYLE_VERTLEFT ||
			(m_orientation == DOCKSTYLE_VERTRIGHT)) {
			ptOffset.y = 0;
		}
		else {
			ptOffset.x = 0;
		}
		m_crTracker.OffsetRect(ptOffset);
		//	Turn on the tracker.
		DrawFocusRect();
		m_ptLast = point;
		return;
	}
}
void CDragBar::OnMouseMove( UINT nFlags, CPoint point )
{
	if(m_bDragging == TRUE)	return;
	if (!m_dockedFrame->IsTreeVisible())  // Cannot drag when tree is closed.
		return;

	static HCURSOR hcurLast = NULL;
	static HCURSOR hcurDestroy = NULL;
	static UINT uLastCursorID = 0;
	UINT uCursorID;

	//	Decide which cursor we want to load.
	if (m_orientation == DOCKSTYLE_HORZTOP || 
		m_orientation == DOCKSTYLE_HORZBOTTOM) {
		//	Horizontal.
		uCursorID = AFX_IDC_VSPLITBAR;
	}
	else	{
		//	Vertical.
		uCursorID = AFX_IDC_HSPLITBAR;
	}

	//	Only if not already loaded.
	HCURSOR hcurToDestroy = NULL;
	if(uCursorID != uLastCursorID)	{
		HINSTANCE hInst =
#ifdef _AFXDLL
		AfxFindResourceHandle(MAKEINTRESOURCE(uCursorID), RT_GROUP_CURSOR);
#else
		AfxGetResourceHandle();
#endif

		//	Load in another cursor.
		hcurToDestroy = hcurDestroy;

		hcurDestroy = hcurLast = ::LoadCursor(hInst, MAKEINTRESOURCE(uCursorID));
		uLastCursorID = uCursorID;
	}

	//	Set the cursor.
	::SetCursor(hcurLast);

	//	Destroy the previous cursor if need be.
	if(hcurToDestroy != NULL)	{
		::DestroyCursor(hcurToDestroy);
	}
}

BOOL CDragBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if (nHitTest == HTCLIENT && pWnd == this && !m_bDragging)	{
		return TRUE;    // we will handle it in the mouse move
	}
	
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CDragBar::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	//	The area we're drawing.
	CRect crRect;
	GetClientRect(crRect);
	//	Draw it as the button face if no other color available.
	WFE_FillSolidRect((CDC *)&dc, crRect, sysInfo.m_clrBtnFace);
#ifndef XP_WIN32
	short dockStyle = m_dockedFrame->GetDockStyle();

	WFE_FillSolidRect((CDC *)&dc, crRect, RGB(255, 255, 255));
	HDC hdc = dc.GetSafeHdc();
	::SelectObject(hdc, ::GetStockObject(BLACK_PEN));
	if ((dockStyle == DOCKSTYLE_VERTLEFT) || (dockStyle == DOCKSTYLE_VERTRIGHT)) {
		::MoveTo(hdc,crRect.left, crRect.top);
		::LineTo(hdc,crRect.left, crRect.bottom);
		::MoveTo(hdc,crRect.right-1, crRect.top);
		::LineTo(hdc,crRect.right-1, crRect.bottom);
	}
	else {
		::MoveTo(hdc,crRect.left, crRect.bottom);
		::LineTo(hdc,crRect.right, crRect.bottom);
		::MoveTo(hdc,crRect.left, crRect.top);
		::LineTo(hdc,crRect.right, crRect.top);
	}
#endif
}

void CDragBar::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	//	Treat same as button down.
	OnLButtonDown(nFlags, point);
}

void CDragBar::CalcClientArea(RECT* lpRectClient)
{

	AFX_SIZEPARENTPARAMS layout;
#ifdef XP_WIN32
	layout.bStretch = FALSE;
	layout.sizeTotal.cx = layout.sizeTotal.cy = 0;
#endif
	m_parentView->GetClientRect(&layout.rect);    // starting rect comes from client rect
	CWnd *pView = m_parentView->GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
	HWND hWnd = pView->GetSafeHwnd();

	layout.hDWP = NULL; // not actually doing layout

	for (HWND hWndChild = ::GetTopWindow(m_parentView->GetSafeHwnd()); hWndChild != NULL;
		hWndChild = ::GetNextWindow(hWndChild, GW_HWNDNEXT))
	{
		if (hWndChild != hWnd && hWndChild != GetSafeHwnd() && hWndChild != m_dockedFrame->GetSafeHwnd())
			::SendMessage(hWndChild, WM_SIZEPARENT, 0, (LPARAM)&layout);
	}
	short dockStyle = m_dockedFrame->GetDockStyle();

	lpRectClient->left = layout.rect.left;
	lpRectClient->right = layout.rect.right;
	lpRectClient->top = layout.rect.top;
	lpRectClient->bottom = layout.rect.bottom;
	if (dockStyle == DOCKSTYLE_VERTLEFT) 
	   lpRectClient->left += MIN_CATEGORY_WIDTH;
	else if (dockStyle == DOCKSTYLE_VERTRIGHT) 
	   lpRectClient->right -= MIN_CATEGORY_WIDTH;
	else if (dockStyle == DOCKSTYLE_HORZTOP) 
	   lpRectClient->top += MIN_CATEGORY_HEIGHT;
	else if (dockStyle == DOCKSTYLE_HORZBOTTOM) 
	   lpRectClient->bottom -= MIN_CATEGORY_HEIGHT;

}

BOOL CDragBar::Track()
{
	// don't handle if capture already set
	if (::GetCapture() != NULL)
		return FALSE;

	// set capture to the window which received this message
	SetCapture();

	// get messages until capture lost or cancelled/accepted
	while (CWnd::GetCapture() == this) {
		MSG msg;
		if (!::GetMessage(&msg, NULL, 0, 0))
		{
#ifdef XP_WIN32
			AfxPostQuitMessage(msg.wParam);
#endif
			break;
		}

		switch (msg.message)
		{
		case WM_LBUTTONUP:
			if (m_bDragging)
				m_bDragging = FALSE;
			EndDrag();
			return TRUE;
		case WM_MOUSEMOVE:
			if (m_bDragging)
				Move(msg.pt);
			break;
		case WM_RBUTTONDOWN:
			CancelLoop();
			return FALSE;

		// just dispatch rest of the messages
		default:
			DispatchMessage(&msg);
			break;
		}
	}
	CancelLoop();

	return FALSE;
}
