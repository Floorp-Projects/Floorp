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
#include "tip.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#define COL_LEFT_MARGIN	((m_cxChar+1)/2)

/////////////////////////////////////////////////////////////////////////////
// CTip

CTip::CTip()
{
	m_hFont = NULL;
	m_dwStyle = 0;
	m_iCSID = CS_LATIN1;
}

CTip::~CTip()
{
	DestroyWindow();
}


BEGIN_MESSAGE_MAP(CTip, CWnd)
	ON_WM_PAINT()
	ON_WM_NCHITTEST()
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTip message handlers

void CTip::OnPaint() 
{
	CRect rect;
	HFONT OldFont = NULL;
	CPaintDC dc(this); // device context for painting
	
	if (m_hFont) {
		OldFont = (HFONT) dc.SelectObject(m_hFont);
	}	
	GetClientRect(&rect);

	COLORREF crInfoBack, crInfoFore;

#ifdef _WIN32
	if ( sysInfo.m_bWin4 ) {
		crInfoFore = GetSysColor( COLOR_INFOTEXT );
		crInfoBack = GetSysColor( COLOR_INFOBK );
	}
	else
#endif
	{
		crInfoBack = RGB(255,255,225);
		crInfoFore = RGB(  0,  0,  0);
	}

	// paint background and text
	CBrush brush;
	brush.CreateSolidBrush(crInfoBack);
	dc.FillRect(&rect, &brush);
	dc.SetBkMode(TRANSPARENT);

	CSize TextSize = CIntlWin::GetTextExtent(0, dc.m_hDC, csTitle, csTitle.GetLength());
	dc.SetTextColor(crInfoFore);
	rect.left += COL_LEFT_MARGIN;
	CIntlWin::DrawText(m_iCSID, dc.m_hDC, (LPSTR) (LPCSTR) csTitle, -1, &rect, DT_LEFT|DT_NOPREFIX);
	
	if (OldFont) {
		dc.SelectObject(OldFont);
	}
}

void CTip::Create()
{
	DWORD dwStyleEx = 0;
	DWORD dwStyle = WS_POPUP|WS_BORDER;
	LPCTSTR lpszClass = AfxRegisterWndClass(CS_SAVEBITS);

#ifdef _WIN32
	if ( sysInfo.m_bWin4 ) {
		dwStyleEx |= WS_EX_TOOLWINDOW;
	} 
#endif
	if (!CreateEx( dwStyleEx, lpszClass, _T(""), dwStyle,
				   0, 0, 0, 0, NULL, NULL)) {
		TRACE0("COULDN'T CREATE TIP WINDOW\n");
	}
}

void CTip::Show( HWND owner, int x, int y, int cx, int cy, 
				 LPCSTR text, DWORD dwStyle, HFONT font )
{
	m_dwStyle = dwStyle;
	m_hFont = font;

	RECT rect;
	CClientDC dc(this);
	HFONT OldFont = NULL;
	
	if (m_hFont) {
		OldFont = (HFONT) dc.SelectObject(m_hFont);
	}	
    TEXTMETRIC tm;
    dc.GetTextMetrics ( &tm );
    m_cxChar = tm.tmAveCharWidth;

	::GetWindowRect(owner, &rect);
	x += rect.left;
	y += rect.top;
	cy += 1;

	csTitle = text;

	// calc window size
	SIZE TextSize = CIntlWin::GetTextExtent(m_iCSID, dc.m_hDC, csTitle, csTitle.GetLength());
	TextSize.cx += COL_LEFT_MARGIN * 2 + 1;

	if ( m_dwStyle & NSTTS_RIGHT ) {
		x = x + cx - TextSize.cx;
	} else {
		x--;
	}

	// Bump onto screen, if necessary.
	int xScreenRight = ::GetSystemMetrics(SM_CXSCREEN);
	int yScreenBottom = ::GetSystemMetrics(SM_CYSCREEN);
	if (x + TextSize.cx > xScreenRight)
		x -= x + TextSize.cx - xScreenRight;
	if (y + cy > yScreenBottom)
		y -= y + cy - yScreenBottom;

	SetWindowText(csTitle);
	if ( csTitle.GetLength() && ( (TextSize.cx > cx) || (m_dwStyle & NSTTS_ALWAYSSHOW)) ) {
		SetWindowPos(&wndTopMost, x, y, TextSize.cx, cy, SWP_SHOWWINDOW|SWP_NOACTIVATE);
		UpdateWindow();
	}
	if (OldFont) {
		dc.SelectObject(OldFont);
	}
}

void CTip::Hide()
{
	ShowWindow( SW_HIDE );
}

UINT CTip::OnNcHitTest(CPoint point)
{
	return (UINT) HTTRANSPARENT;
}
