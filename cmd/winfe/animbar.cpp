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
#include "animbar.h"

#define ANIMATION_WIDTH     20
#define ANIMATION_HEIGHT    20
#undef ANIMATION_PERIOD
#define ANIMATION_PERIOD (4000)

CAnimation::CAnimation(CWnd *pParent ): CWnd()
{
	m_uAnimationClock = 0;
	m_bInited = FALSE;
	m_bCaptured = FALSE;
	m_bDepressed = FALSE;
	m_hAnimBitmap = NULL;
	m_iAnimationCount = 0;

	m_iconSize.cx = ANIMATION_WIDTH;
	m_iconSize.cy = ANIMATION_HEIGHT;

	CreateEx(0, NULL, _T("NSAnimation"), 
			 WS_CHILD|WS_VISIBLE,
			 0, 0, 
			 m_iconSize.cx + 4,
			 m_iconSize.cy + 4,
			 pParent->GetSafeHwnd(), (HMENU) 101);
             
//Changed AfxGetInstanceHandle to AfxGetResourceHandle to insure loading bitmaps
//from the LANG DLL and not netscape.exe benito
	if (!m_bInited) {
        CDC *pDC = GetDC();
        
		m_hAnimBitmap = wfe_LoadBitmap(AfxGetResourceHandle(), pDC->m_hDC, MAKEINTRESOURCE(IDB_ANIMSMALL_0));
		BITMAP bm;
		GetObject( m_hAnimBitmap, sizeof(BITMAP), &bm );
		m_iFrameCount = bm.bmWidth / m_iconSize.cx;

        ReleaseDC( pDC );
        
		m_bInited = TRUE;
	}
}

BEGIN_MESSAGE_MAP(CAnimation, CWnd)
    ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CAnimation::OnPaint()
{
    CPaintDC dc(this);
	RECT rect;
	GetClientRect(&rect);
	WFE_Draw3DButtonRect( dc.m_hDC, &rect, m_bDepressed );
	AnimateIcon();
}


void CAnimation::OnTimer(UINT nID)
{
	CWnd::OnTimer(nID);
	if (nID == WIN_ANIMATE_ICON_TIMER) {
		if(m_bInited)
		{
			m_iAnimationCount = ( m_iAnimationCount % m_iFrameCount ) + 1;
			AnimateIcon();
		}
	}
}

void CAnimation::OnLButtonDown( UINT nFlags, CPoint point )
{
	RECT rect;
	GetClientRect(&rect);
	m_bDepressed = ::PtInRect(&rect, point);
	if (m_bDepressed) {
		SetCapture();
		m_bCaptured = TRUE;
		Invalidate();
		UpdateWindow();
	}
}

void CAnimation::OnMouseMove( UINT nFlags, CPoint point )
{
	if (m_bCaptured) {
		RECT rect;
		GetClientRect(&rect);
		BOOL bDepressed = ::PtInRect(&rect, point);
		if (bDepressed != m_bDepressed) {
			m_bDepressed = bDepressed;
			Invalidate();
			UpdateWindow();
		}
	}
}

void CAnimation::OnLButtonUp( UINT nFlags, CPoint point )
{

	if (m_bCaptured) {
		ReleaseCapture();
		m_bCaptured = FALSE;

		RECT rect;
		GetClientRect(&rect);
		BOOL bDepressed = ::PtInRect(&rect, point);
		GetParentFrame()->SendMessage( WM_COMMAND, (WPARAM) ID_ANIMATION_BONK, (LPARAM) 0 );
	}

	m_bDepressed = FALSE;
	Invalidate();
	UpdateWindow();
}

void CAnimation::OnDestroy()
{
	if (m_hAnimBitmap)
		VERIFY(DeleteObject( m_hAnimBitmap ));
	// Right now, the context has already been deleted, since
	// it's not a true COM object.
//	m_pIMWContext->Release();
}

void CAnimation::StopAnimation()
{
    m_iAnimationCount = 0;
	if (m_uAnimationClock)
		KillTimer(m_uAnimationClock);
	m_uAnimationClock = 0;
    AnimateIcon();
}

void CAnimation::StartAnimation()
{
    m_iAnimationCount = 1;
	m_uAnimationClock = SetTimer(WIN_ANIMATE_ICON_TIMER, ANIMATION_PERIOD/m_iFrameCount, NULL);
}

void CAnimation::AnimateIcon()
{
    CClientDC dc(this);
    CRect rect;
    GetClientRect(&rect);
	
	// check if our application level CDC exists
	if(!theApp.pIconDC) {
		theApp.pIconDC = new CDC;
		theApp.pIconDC->CreateCompatibleDC(&dc);
	}

    HBITMAP hOldBitmap = (HBITMAP) theApp.pIconDC->SelectObject( m_hAnimBitmap );
	
    dc.BitBlt(  2, 2,
                m_iconSize.cx, m_iconSize.cy, 
                theApp.pIconDC, 
                m_iconSize.cx * m_iAnimationCount,      // X offset into the strip
                0, 
                SRCCOPY);

    //  Reselect the old object
    theApp.pIconDC->SelectObject( hOldBitmap );
}

/////////////////////////////////////////////////////////////////////////////
// CAnimationBar toolbar derived class

CAnimationBar::CAnimationBar( ): CAnimationBarParent ()
{
	m_iconPt.x = 0;
	m_iconPt.y = 0;

	m_wndAnimation = NULL;
}

CAnimationBar::~CAnimationBar()
{
	if ( m_wndAnimation ) {
		m_wndAnimation->DestroyWindow();
	}
}

void CAnimationBar::StartAnimation()
{
	if (m_wndAnimation)
		m_wndAnimation->StartAnimation();
}

void CAnimationBar::StopAnimation()
{
	if (m_wndAnimation)
		m_wndAnimation->StopAnimation();
}

#ifdef XP_WIN32
CSize CAnimationBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	CSize size = CAnimationBarParent::CalcFixedLayout(bStretch, bHorz);
	if (m_wndAnimation) {
		CRect rectAnimation;
		m_wndAnimation->GetWindowRect(&rectAnimation);

		int cy = rectAnimation.Height() + m_cyTopBorder + m_cyBottomBorder + 4;
		if (size.cy < cy )
			size.cy = cy;
	}
	return size;
}

CSize CAnimationBar::CalcDynamicLayout(int nLength, DWORD dwMode )
{
	int nToolbarHeight = 0;
	int nAnimationHeight = 0;

	CRect animationRect;

	if(m_wndAnimation)
	{
		m_wndAnimation->GetWindowRect(animationRect);
		nAnimationHeight = animationRect.Height() + 4;
	}

	if(m_pToolbar)
	{
		nToolbarHeight = m_pToolbar->GetHeight() + 4;
	}

	return CSize(32767, 
		(nToolbarHeight > nAnimationHeight) ? nToolbarHeight : nAnimationHeight);

}
#endif

void CAnimationBar::PlaceToolbar(void)
{
	CRect rectAnimation;

	if ( m_wndAnimation ) {
		m_wndAnimation->GetWindowRect(&rectAnimation);
		CRect rectBar;
		GetClientRect(&rectBar);

		m_iconPt.x = rectBar.Width() - rectAnimation.Width() - GetSystemMetrics(SM_CXBORDER) - 1;
		m_iconPt.y = rectBar.Height() / 2 - rectAnimation.Height() / 2 + 2;

		if(m_pToolbar)
		{
			int nHeight = m_pToolbar->GetHeight();
		//	m_pToolbar->MoveWindow(0, (rectBar.Height() - nHeight) / 2 + 1, cx - rectAnimation.Width(), nHeight, FALSE);
			m_pToolbar->SetWindowPos(NULL, 0, (rectBar.Height() - nHeight) / 2 + 1, m_iconPt.x, nHeight,
				/*SWP_NOMOVE|*/ SWP_NOREDRAW);
		}

	}

}

BEGIN_MESSAGE_MAP(CAnimationBar, CAnimationBarParent)
	ON_WM_CREATE()
    ON_WM_SIZE()
END_MESSAGE_MAP()

int CAnimationBar::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	int res = CAnimationBarParent::OnCreate( lpCreateStruct );

	if ( res != -1 ) {
		m_wndAnimation = new CAnimation( this );
		res = m_wndAnimation ? 0 : -1;
	}
	return res;
}

void CAnimationBar::OnSize( UINT nType, int cx, int cy )
{
	CRect rectAnimation;

	if ( nType != SIZE_MINIMIZED && m_wndAnimation ) {
		m_wndAnimation->GetWindowRect(&rectAnimation);
		CRect rectBar;
		GetClientRect(&rectBar);
		CClientDC dc( this );

		m_iconPt.x = rectBar.Width() - rectAnimation.Width() - GetSystemMetrics(SM_CXBORDER) - 1;
		m_iconPt.y = rectBar.Height() / 2 - rectAnimation.Height() / 2 + 2;
		m_wndAnimation->SetWindowPos(NULL, m_iconPt.x, m_iconPt.y, 0, 0, 
									 SWP_NOZORDER|SWP_NOSIZE);

		if(m_pToolbar)
		{
			int nHeight = m_pToolbar->GetHeight();
		//	m_pToolbar->MoveWindow(0, (rectBar.Height() - nHeight) / 2 + 1, cx - rectAnimation.Width(), nHeight, FALSE);
			m_pToolbar->SetWindowPos(NULL, 0, (rectBar.Height() - nHeight) / 2 + 1, m_iconPt.x, nHeight,
				/*SWP_NOMOVE|*/ SWP_NOREDRAW);
		}

	}


}
