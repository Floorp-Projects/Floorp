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
#include "tooltip.h"
#include "animbar2.h"
#include "statbar.h"
#include "prefapi.h"
#include "custom.h"
#include "sysinfo.h"

#define ANIMATION_WIDTH     16
#define ANIMATION_HEIGHT    16
#define ANIMATION_WIDTH_L   32
#define ANIMATION_HEIGHT_L  32
#undef ANIMATION_PERIOD
#define ANIMATION_PERIOD (8000)

#define IDT_BUTTONFOCUS 16411
#define BUTTONFOCUS_DELAY_MS 10

#define BUTTON_WIDTH        2

// We cache the animation bitmaps for speed
HBITMAP CAnimation2::m_hSmall    = NULL;
HBITMAP CAnimation2::m_hBig      = NULL;
ULONG   CAnimation2::m_uRefCount = 0L;

void DrawUpButton(HDC hDC, CRect & rect)
{
    // Highlight (the white color)
    HBRUSH br = ::CreateSolidBrush(sysInfo.m_clrBtnHilite);
    CRect rc(rect.left, rect.top, rect.right-1, rect.top+1);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.left, rect.top, rect.left+1, rect.bottom-1);
	::FillRect(hDC, rc, br);

#ifdef _WIN32
	::DeleteObject(br);
	
    // Light shadow (the light gray color)
	br = ::CreateSolidBrush(::GetSysColor(COLOR_3DLIGHT));
#endif // _WIN32

	rc.SetRect(rect.left+1, rect.top+1, rect.right - 2 , rect.top+2);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.left+1, rect.top+1, rect.left+2, rect.bottom - 2);
	::FillRect(hDC, rc, br);
    ::DeleteObject(br);

    // Shadow (the dark grey color)
    br = ::CreateSolidBrush(sysInfo.m_clrBtnShadow);
	rc.SetRect(rect.left+1, rect.bottom - 2, rect.right-1, rect.bottom-1);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.right - 2, rect.top+1, rect.right-1, rect.bottom-1);
	::FillRect(hDC, rc, br);

#ifdef _WIN32
	::DeleteObject(br);

	// Dark Shadow (the black color)
	br = ::CreateSolidBrush(::GetSysColor(COLOR_3DDKSHADOW));
#endif // _WIN32

	rc.SetRect(rect.left, rect.bottom - 1, rect.right, rect.bottom);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.right - 1, rect.top, rect.right, rect.bottom);
	::FillRect(hDC, rc, br);
	::DeleteObject(br);
} 

void DrawDownButton(HDC hDC, CRect & rect)
{
    // Highlight (the white color)
#ifdef XP_WIN16
	HBRUSH br = ::CreateSolidBrush(sysInfo.m_clrBtnShadow);
#else // _WIN32
    HBRUSH br = ::CreateSolidBrush(::GetSysColor(COLOR_3DDKSHADOW));
#endif
    
	CRect rc(rect.left, rect.top, rect.right-1, rect.top+1);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.left, rect.top, rect.left+1, rect.bottom-1);
	::FillRect(hDC, rc, br);

#ifdef _WIN32
	::DeleteObject(br);
	
    // Light shadow (the light gray color)
	br = ::CreateSolidBrush(sysInfo.m_clrBtnShadow);
#endif

	rc.SetRect(rect.left+1, rect.top+1, rect.right - 2 , rect.top+2);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.left+1, rect.top+1, rect.left+2, rect.bottom - 2);
	::FillRect(hDC, rc, br);
    ::DeleteObject(br);

    // Shadow (the dark grey color)
#ifdef XP_WIN16
	br = ::CreateSolidBrush(sysInfo.m_clrBtnHilite);
#else
    br = ::CreateSolidBrush(::GetSysColor(COLOR_3DLIGHT));
#endif // _WIN16

	rc.SetRect(rect.left+1, rect.bottom - 2, rect.right-1, rect.bottom-1);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.right - 2, rect.top+1, rect.right-1, rect.bottom-1);
	::FillRect(hDC, rc, br);

#ifdef _WIN32
	::DeleteObject(br);

	// Dark Shadow (the black color)
	br = ::CreateSolidBrush(sysInfo.m_clrBtnHilite);
#endif // _WIN32

	rc.SetRect(rect.left, rect.bottom - 1, rect.right, rect.bottom);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.right - 1, rect.top, rect.right, rect.bottom);
	::FillRect(hDC, rc, br);
	::DeleteObject(br);
	
}

CAnimation2::CAnimation2( CWnd *pParent, LPUNKNOWN pUnk ): CWnd()
{
	m_uAnimationClock = 0;
	m_bInited = FALSE;
	m_bCaptured = FALSE;
	m_bDepressed = FALSE;
	m_hAnimBitmap = NULL;
	m_iAnimationCount = 0;
        
	m_iconSize.cx = 0;
	m_iconSize.cy = 0;

	CreateEx(0, NULL, _T("NSAnimation"), 
			 WS_CHILD|WS_VISIBLE,
			 0, 0, 0, 0,
			 pParent->GetSafeHwnd(), (HMENU) 101);
    
    if( !m_bInited ) 
    {
        SetBitmap();
        
		m_bInited = TRUE;
	}

#ifdef WIN32
	m_ToolTip.Create(this, TTS_ALWAYSTIP);
#else
	m_ToolTip.Create(this);
#endif

    char *pURL;
    PREF_CopyConfigString("toolbar.logo.url",&pURL);

    if( !CUST_IsCustomAnimation( &m_iFrameCount ) )
    {
        m_ToolTip.AddTool(this, szLoadString(IDS_NETSCAPE_TIP));
        m_ToolTip.Activate(TRUE);
	    m_ToolTip.SetDelayTime(250);
    }
        
    m_hFocusTimer = 0;
    m_bHaveFocus = FALSE;
    
	m_pIMWContext = NULL;
	if (pUnk)
		pUnk->QueryInterface( IID_IMWContext, (LPVOID *) &m_pIMWContext );
        
    CAnimation2::m_uRefCount++;
}

CAnimation2::~CAnimation2()
{
    CAnimation2::m_uRefCount--;
    ASSERT( CAnimation2::m_uRefCount >= 0 );
    
    if( !CAnimation2::m_uRefCount )
    {
        if( CAnimation2::m_hSmall )
        {
            DeleteObject( CAnimation2::m_hSmall );
            CAnimation2::m_hSmall = NULL;
        }
        
        if( CAnimation2::m_hBig )
        {
            DeleteObject( CAnimation2::m_hBig );
            CAnimation2::m_hBig = NULL;
        }
    }
}

BEGIN_MESSAGE_MAP(CAnimation2, CWnd)
	ON_WM_ERASEBKGND()
    ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_DESTROY()
    ON_WM_SETCURSOR()
END_MESSAGE_MAP()

BOOL CAnimation2::OnEraseBkgnd( CDC* pDC )
{
	return TRUE;
}

void CAnimation2::OnPaint()
{
    // Do not remove next useless line, somehow needed or
    //  painting will hang.
    CPaintDC dc(this);
    
    AnimateIcon();
}


void CAnimation2::OnTimer(UINT nID)
{
	CWnd::OnTimer(nID);
	if (nID == WIN_ANIMATE_ICON_TIMER) {

		if(m_bInited)
		{
		    m_iAnimationCount = ( m_iAnimationCount % m_iFrameCount ) + 1;
            
			AnimateIcon();
		}
	}
    else if(nID == IDT_BUTTONFOCUS)
	{
		RemoveButtonFocus();
	}
}

BOOL CAnimation2::OnSetCursor( CWnd *, UINT, UINT )
{
    SetCursor( theApp.LoadCursor( IDC_SELECTANCHOR ) );
    
    return TRUE;
}

void CAnimation2::OnLButtonDown( UINT nFlags, CPoint point )
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
    
    MSG msg = *(GetCurrentMessage());
	m_ToolTip.RelayEvent(&msg);
}

void CAnimation2::OnMouseMove( UINT nFlags, CPoint point )
{
    RECT rect;
    GetClientRect(&rect);

    if (m_bCaptured) 
    {
		    
		    BOOL bDepressed = ::PtInRect(&rect, point);
		    if (bDepressed != m_bDepressed)
            {
			    m_bDepressed = bDepressed;
                Invalidate();
    	        UpdateWindow();
            }
    }

    if (!m_bHaveFocus)
	{
		if(GetParentFrame() == GetActiveWindow() || GetParentFrame() == AfxGetMainWnd())
		{
			m_bHaveFocus = TRUE;
			if( !CUST_IsCustomAnimation( &m_iFrameCount ) && m_hFocusTimer == 0)
			{
				m_hFocusTimer = SetTimer(IDT_BUTTONFOCUS, BUTTONFOCUS_DELAY_MS, NULL);

					WFE_GetOwnerFrame(this)->SendMessage( WM_SETMESSAGESTRING,
														   (WPARAM)0, (LPARAM)szLoadString(IDS_NETSCAPE_TIP));

            }

            m_ToolTip.Activate(TRUE);
        }
    }

    if(GetParentFrame() == GetActiveWindow() || GetParentFrame() == AfxGetMainWnd())
	{
		MSG msg = *(GetCurrentMessage());
		m_ToolTip.RelayEvent(&msg);
	}
}

void CAnimation2::RemoveButtonFocus()
{
    POINT point;

	KillTimer(IDT_BUTTONFOCUS);
	m_hFocusTimer = 0;
	GetCursorPos(&point);

	CRect rcClient;
	GetWindowRect(&rcClient);

	if (!rcClient.PtInRect(point))
	{
		m_bHaveFocus = FALSE;

		if ( WFE_GetOwnerFrame(this) != NULL) {
			WFE_GetOwnerFrame(this)->SendMessage( WM_SETMESSAGESTRING,
										   (WPARAM) 0, (LPARAM) "" );
		}
        m_bDepressed = FALSE;
        Invalidate();
        UpdateWindow();
    }
    else
		m_hFocusTimer = SetTimer(IDT_BUTTONFOCUS, BUTTONFOCUS_DELAY_MS, NULL);
}

void CAnimation2::OnLButtonUp( UINT nFlags, CPoint point )
{

	if (m_bCaptured) {
		ReleaseCapture();
		m_bCaptured = FALSE;

		RECT rect;
		GetClientRect(&rect);
		BOOL bDepressed = ::PtInRect(&rect, point);
		if ( bDepressed) {
			CAbstractCX * pCX = FEU_GetLastActiveFrameContext();
			ASSERT(pCX != NULL);
			if (pCX != NULL)
			{
				char *pURL;
				PREF_CopyConfigString("toolbar.logo.url",&pURL);
				if (pURL && *pURL) {
					pCX->NormalGetUrl(pURL);
					XP_FREE(pURL);
				}

			}
		}
	}

	m_bDepressed = FALSE;
	Invalidate();
	UpdateWindow();
}

void CAnimation2::OnDestroy()
{
}

void CAnimation2::StopAnimation()
{
    m_iAnimationCount = 0;
	if (m_uAnimationClock)
		KillTimer(m_uAnimationClock);
	m_uAnimationClock = 0;
    AnimateIcon();
}

void CAnimation2::StartAnimation()
{
    m_iAnimationCount = 1;
	m_uAnimationClock = SetTimer(WIN_ANIMATE_ICON_TIMER, ANIMATION_PERIOD/m_iFrameCount, NULL);
}

void CAnimation2::AnimateIcon()
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

    if (m_bDepressed)
        DrawDownButton(dc.m_hDC, rect);
    else DrawUpButton(dc.m_hDC, rect);

    //  Reselect the old object
    theApp.pIconDC->SelectObject( hOldBitmap );

}

void CAnimation2::Initialize( LPUNKNOWN pUnk )
{
	ASSERT(pUnk);
	if (m_pIMWContext) {
		m_pIMWContext->Release();
		m_pIMWContext = NULL;
	}
	if (pUnk) {
		pUnk->QueryInterface( IID_IMWContext, (LPVOID *) &m_pIMWContext );
	}
}

void CAnimation2::SetBitmap( BOOL bSmall /*=TRUE*/ )
{
    CDC *pDC = GetDC();

    if( !CAnimation2::m_hSmall )
    {
		CAnimation2::m_hSmall = wfe_LoadBitmap( AfxGetResourceHandle(), pDC->m_hDC, MAKEINTRESOURCE(IDB_ANIMSMALL_0) );
        ASSERT( CAnimation2::m_hSmall );
        
		CAnimation2::m_hBig = wfe_LoadBitmap( AfxGetResourceHandle(), pDC->m_hDC, MAKEINTRESOURCE(IDB_ANIM_0) );        
        ASSERT( CAnimation2::m_hBig );
    }

    if( CUST_IsCustomAnimation( &m_iFrameCount ) )
    {
        ASSERT( m_iFrameCount > 0 );
        
     	m_hAnimBitmap = bSmall ? CAnimation2::m_hSmall : CAnimation2::m_hBig;

     	BITMAP bm;
       	GetObject( m_hAnimBitmap, sizeof(BITMAP), &bm );

     	m_iconSize.cx = bm.bmWidth / m_iFrameCount;
     	m_iconSize.cy = bm.bmHeight;
    }
    else
    {
        if( bSmall )
        {
        	m_iconSize.cx = ANIMATION_WIDTH;
        	m_iconSize.cy = ANIMATION_HEIGHT;
         	m_hAnimBitmap = CAnimation2::m_hSmall;
        }
        else
        {
        	m_iconSize.cx = ANIMATION_WIDTH_L;
        	m_iconSize.cy = ANIMATION_HEIGHT_L;
         	m_hAnimBitmap = CAnimation2::m_hBig;
        }
        
     	BITMAP bm;
       	GetObject( m_hAnimBitmap, sizeof(BITMAP), &bm );
     	m_iFrameCount = bm.bmWidth / m_iconSize.cx;
    }

    ReleaseDC( pDC );
    
    SetWindowPos( NULL, 0, 0, m_iconSize.cx+4, m_iconSize.cy+4, SWP_NOMOVE | SWP_NOZORDER );
    GetParent()->SetWindowPos( NULL, 0, 0, m_iconSize.cx+4, m_iconSize.cy+4, SWP_NOMOVE | SWP_NOZORDER );
}

HBITMAP CAnimation2::GetBitmap( BOOL bSmall /*=TRUE*/ )
{
    return bSmall ? CAnimation2::m_hSmall : CAnimation2::m_hBig;
}

void CAnimation2::GetSize( CSize &size )
{
    size = m_iconSize;
}

/////////////////////////////////////////////////////////////////////////////
// CAnimationBar2 toolbar derived class

CAnimationBar2::CAnimationBar2( LPUNKNOWN pUnk ): CAnimationBar2Parent ()
{
	m_iconPt.x = 0;
	m_iconPt.y = 0;

	m_wndAnimation = NULL;
	m_pUnk = pUnk;
}

CAnimationBar2::~CAnimationBar2()
{
	if ( m_wndAnimation ) {
		m_wndAnimation->DestroyWindow();
		delete m_wndAnimation;
	}
}

void CAnimationBar2::StartAnimation()
{
	if( !m_wndAnimation )
    {
        return;
    }
    
    m_wndAnimation->StartAnimation();
}

void CAnimationBar2::StopAnimation()
{
	if( !m_wndAnimation )
    {
        return;
    }
    
    m_wndAnimation->StopAnimation();
}

void CAnimationBar2::Initialize( LPUNKNOWN pUnk )
{
	m_pUnk = pUnk;
	if (m_wndAnimation) 
		m_wndAnimation->Initialize( pUnk );
}

BEGIN_MESSAGE_MAP(CAnimationBar2, CAnimationBar2Parent)
	ON_WM_CREATE()
    ON_WM_SIZE()
END_MESSAGE_MAP()

int CAnimationBar2::OnCreate( LPCREATESTRUCT lpCreateStruct )
{

	int res = CAnimationBar2Parent::OnCreate( lpCreateStruct );

	if ( res != -1 ) {
		m_wndAnimation = new CAnimation2( this, m_pUnk );
		res = m_wndAnimation ? 0 : -1;
	}
	return res;
}

void CAnimationBar2::OnSize( UINT nType, int cx, int cy )
{
}

BOOL CAnimationBar2::OnEraseBkgnd( CDC* pDC )
{
	return TRUE;
}
  
void CAnimationBar2::GetSize( CSize &size, BOOL bSmall /*=TRUE*/ )
{
    if( !m_wndAnimation )
    {
        return;
    }
 
    int iFrameCount = 0;
    
    if( CUST_IsCustomAnimation( &iFrameCount ) )
    {
        ASSERT( iFrameCount > 0 );
        
     	BITMAP bm;
       	GetObject( m_wndAnimation->GetBitmap( bSmall ), sizeof(BITMAP), &bm );

     	size.cx = bm.bmWidth / iFrameCount;
     	size.cy = bm.bmHeight;
    }
    else
    {
       	size.cx = (bSmall ? ANIMATION_WIDTH  : ANIMATION_WIDTH_L);
       	size.cy = (bSmall ? ANIMATION_HEIGHT : ANIMATION_HEIGHT_L);
    }

    size.cx += 4;
    size.cy += 4;

}
