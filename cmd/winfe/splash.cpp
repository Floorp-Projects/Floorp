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

// splash.cpp : implementation file
//

#include "stdafx.h"
#include "splash.h"


#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#ifdef EDITOR
#endif

#define STATUS_TEXT_HEIGHT -14
#define COPYRIGHT_TEXT_HEIGHT 10

BEGIN_MESSAGE_MAP(CSplashWnd, CDialog)
    //{{AFX_MSG_MAP(CSplashWnd)
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_BIGSPLASH, OnLogoClicked)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()
/////////////////////////////////////////////////////////////////////////////
// CSplashWnd dialog

CSplashWnd::CSplashWnd()
{
    m_bNavBusyIniting = TRUE;
}

BOOL CSplashWnd::Create(CWnd* pParent)
{
    //{{AFX_DATA_INIT(CSplashWnd)
        // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT

    if (!CDialog::Create(CSplashWnd::IDD, pParent))
    {
        TRACE0("Warning: creation of CSplashWnd dialog failed\n");
        return FALSE;
    }

    int frameCX = ::GetSystemMetrics(SM_CXDLGFRAME);
    int frameCY = ::GetSystemMetrics(SM_CYDLGFRAME);

#ifdef WIN32
    // figure out whether to change for Win 95
	if (sysInfo.m_bWin4) {
		frameCX = ::GetSystemMetrics(SM_CXFIXEDFRAME);
		frameCY = ::GetSystemMetrics(SM_CYFIXEDFRAME);
	}
#endif

    SetWindowPos(&wndTopMost,
                 0, 0,
                 m_sizeBitmap.cx + (frameCX * 2),
                 m_sizeBitmap.cy + (frameCY * 2),
                 SWP_NOMOVE);

	//Put this after we size the dialog with the bitmap.
    CenterWindow();

	
    return TRUE;
}

BOOL CSplashWnd::OnInitDialog()
{
    const WORD  splashTimeout = 2000;

    CDialog::OnInitDialog();

    // initialize the big icon control
    m_icon.SubclassDlgItem(IDC_BIGSPLASH, this);
    m_icon.SizeToContent(m_sizeBitmap);

    m_timerID = 55; 
    m_timerID = SetTimer(m_timerID, splashTimeout, NULL);   // N seconds

    return TRUE;  // return TRUE  unless you set the focus to a control
}

/////////////////////////////////////////////////////////////////////////////
// CSplashWnd message handlers

void CSplashWnd::OnLogoClicked()
{
    KillTimer(m_timerID);
    EndDialog(NULL);
}
     
void CSplashWnd::OnTimer(UINT uTimerID)
{
    if(m_bNavBusyIniting)
        return; // wait for next timer interval

    OnLogoClicked();
}

void CSplashWnd::NavDoneIniting()
{
    m_bNavBusyIniting = FALSE;
	DestroyWindow();
	m_icon.CleanupResources();
}

void CSplashWnd::SafeHide()
{
	if (GetSafeHwnd()) {
		ShowWindow(SW_HIDE);
	}
}
void CSplashWnd::DisplayStatus(LPCSTR lpszStatus)
{
	ASSERT(lpszStatus);
	m_icon.DisplayStatus(lpszStatus);
	SetWindowText(lpszStatus);
}

/////////////////////////////////////////////////////////////////////////////
// CBigIcon

BEGIN_MESSAGE_MAP(CBigIcon, CButton)
    //{{AFX_MSG_MAP(CBigIcon)
    ON_WM_DRAWITEM()
    ON_WM_ERASEBKGND()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBigIcon message handlers

CBigIcon::CBigIcon()
{
    HFONT hFont;
	LOGFONT lf1, lf2;
	int Adjustment = 0;

#if _WIN32 // Try to fix bug #37584 (unicode support)
    
    if( sysInfo.m_bWin4 )
    {
        hFont = (HFONT)GetStockObject( DEFAULT_GUI_FONT );
		Adjustment = 2;
    }
    else
    {
        hFont = (HFONT)GetStockObject( OEM_FIXED_FONT );
    }

#else
    hFont = (HFONT)GetStockObject( OEM_FIXED_FONT );
#endif    

    

	VERIFY(::GetObject(hFont, sizeof(LOGFONT), &lf1));
	lf1.lfHeight = STATUS_TEXT_HEIGHT + Adjustment;
	lf1.lfWidth = 2 + Adjustment;
	VERIFY(m_font.CreateFontIndirect(&lf1));
	VERIFY(::GetObject(hFont, sizeof(LOGFONT), &lf2));
	lf2.lfHeight = COPYRIGHT_TEXT_HEIGHT + Adjustment;
	lf2.lfWidth = 2 + Adjustment;
	VERIFY(m_copyrightFont.CreateFontIndirect(&lf2));
	m_hBitmap = NULL;
}

CBigIcon::~CBigIcon()
{
}

void CBigIcon::CleanupResources(void)
{
	if(m_hBitmap)
		DeleteObject(m_hBitmap);

	m_copyrightFont.DeleteObject();
	m_font.DeleteObject();

}

#ifdef XP_WIN16
#define ADMNLIBNAME "adm1640.dll"
#elif defined XP_WIN32
#define ADMNLIBNAME "adm3240.dll"
#endif

void CBigIcon::SizeToContent(CSize& SizeBitmap)
{
	// if admin kit library is here, load splash screen bitmap from it
	HINSTANCE h = 0;
	BOOL bFreeLibrary = FALSE;

#ifdef MOZ_ADMIN_LIB
	h = LoadLibrary(ADMNLIBNAME);
#endif   
	UINT nBitmapID;
	if (h < (HINSTANCE)HINSTANCE_ERROR) {
		nBitmapID = IDB_PREVIEWSPLASH;
		h = AfxGetResourceHandle();
	} else {
		nBitmapID = IDB_PREVIEWSPLASH;
		bFreeLibrary = TRUE;
	}

	HDC hDC = ::GetDC(m_hWnd);
	WFE_InitializeUIPalette(hDC);
	HPALETTE hPalette = WFE_GetUIPalette(NULL);
	HPALETTE hOldPalette = ::SelectPalette(hDC, hPalette, FALSE);
	::RealizePalette(hDC);
	m_hBitmap = wfe_LoadBitmap(h, hDC, MAKEINTRESOURCE(nBitmapID)); 

	if (!m_hBitmap && bFreeLibrary) {
		m_hBitmap = wfe_LoadBitmap(AfxGetResourceHandle(), hDC, MAKEINTRESOURCE(nBitmapID)); 		
	}

	::SelectPalette(hDC, hOldPalette, TRUE);
	::ReleaseDC(m_hWnd, hDC);

	if (bFreeLibrary) 
		FreeLibrary(h);

    BITMAP bm;
    ::GetObject(m_hBitmap, sizeof(bm), &bm);
    m_sizeBitmap = CSize(bm.bmWidth, bm.bmHeight);
    SizeBitmap = m_sizeBitmap;

    SetWindowPos(NULL, 0, 0, bm.bmWidth, bm.bmHeight,
        SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);
}

void CBigIcon::DisplayCopyright(void)
{
	CClientDC	dc(this);
	CFont*		pOldFont = dc.SelectObject(&m_copyrightFont);
	CRect		rect;
	LPCSTR lpszCopyright = szLoadString(IDS_COPYRIGHT);
	// The splash screen background is black, use light grey text
    dc.SetBkColor(RGB(0,0,0));
    dc.SetTextColor(RGB(192, 192, 192));

	CenterText(dc, lpszCopyright, 237);

	// Clean up
	dc.SetBkColor(RGB(255,255,255));
    dc.SetTextColor(RGB(0,0,0));
	dc.SelectObject(pOldFont);
}

void CBigIcon::DisplayStatus(LPCSTR lpszStatus)
{
	CClientDC	dc(this);
	CFont*		pOldFont = dc.SelectObject(&m_font);

	// The splash screen background is black, so use white text
    dc.SetBkColor(RGB(0,0,0));
    dc.SetTextColor(RGB(255,255,255));

	CenterText(dc, lpszStatus, 219);

	// Clean up
	dc.SetBkColor(RGB(255,255,255));
    dc.SetTextColor(RGB(0,0,0));
	dc.SelectObject(pOldFont);
}

void CBigIcon::CenterText(CClientDC &dc, LPCSTR lpszStatus, int top)
{
	int			nMargin;
	CRect		rect;
	TEXTMETRIC	tm;

	// Leave a horizontal margin equal to the widest character
	VERIFY(dc.GetTextMetrics(&tm));
	nMargin = tm.tmMaxCharWidth;

	// Compute the opaque rect
	rect.left = nMargin;
	rect.right = m_sizeBitmap.cx - nMargin;
	rect.top = top;
	rect.bottom = rect.top + tm.tmHeight;

	// We need to compute where to draw the text so it is centered
	// horizontally
	int		x = rect.left;
    CSize   extent = CIntlWin::GetTextExtent(0, dc.m_hDC, lpszStatus, XP_STRLEN(lpszStatus));

	if (extent.cx < rect.Width())
		x += (rect.Width() - extent.cx) / 2;

	// Draw opaquely so we can avoid erasing the old text
	dc.ExtTextOut(x, rect.top, ETO_OPAQUE, &rect, lpszStatus, strlen(lpszStatus), NULL);


}
void CBigIcon::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
    ASSERT(pDC != NULL);

	HPALETTE hPalette = WFE_GetUIPalette(NULL);
	HPALETTE hOldPalette = ::SelectPalette(lpDrawItemStruct->hDC, hPalette, FALSE);
	::RealizePalette(lpDrawItemStruct->hDC);

    CRect rect;
    GetClientRect(rect);
    int cxClient = rect.Width();
    int cyClient = rect.Height();

    // draw the bitmap contents
    CDC dcMem;
    if (!dcMem.CreateCompatibleDC(pDC))
        return;
    HBITMAP hOldBitmap = (HBITMAP)::SelectObject(dcMem.m_hDC, m_hBitmap);
    if (hOldBitmap == NULL)
        return;

    pDC->BitBlt(0, 0, m_sizeBitmap.cx, m_sizeBitmap.cy, &dcMem, 0, 0, SRCCOPY);

    ::SelectObject(dcMem.m_hDC, hOldBitmap);
	DisplayCopyright();
	::SelectPalette(lpDrawItemStruct->hDC, hOldPalette, TRUE);

    ReleaseDC(pDC);
}

BOOL CBigIcon::OnEraseBkgnd(CDC*)
{
    return TRUE;    // we don't do any erasing...
}

/////////////////////////////////////////////////////////////////////////////
