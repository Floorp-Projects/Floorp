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

// Embedded menu and close box for Aurora (Created by Dave Hyatt)

#include "stdafx.h"
#include "navbar.h"
#include "navcntr.h"
#include "navfram.h"
#include "usertlbr.h"
#include "dropmenu.h"
#include "rdfliner.h"
#include "htrdf.h"
#include "xp_ncent.h"

// The Nav Center vocab element
extern "C" RDF_NCVocab gNavCenter;

extern void DrawUpButton(HDC dc, CRect& rect);

BEGIN_MESSAGE_MAP(CNavTitleBar, CWnd)
	//{{AFX_MSG_MAP(CNavTitleBar)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP ( )
	ON_WM_MOUSEMOVE()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CNavTitleBar::CNavTitleBar()
:m_bHasFocus(FALSE) 
{
	m_pBackgroundImage = NULL;
	m_View = NULL;
}

CNavTitleBar::~CNavTitleBar()
{
//	delete m_pMenuButton;
}

void CNavTitleBar::OnPaint( )
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);
	
//	if (m_bHasFocus)
//		m_pMenuButton->SetCustomColors(::GetSysColor(COLOR_CAPTIONTEXT), ::GetSysColor(COLOR_ACTIVECAPTION));
//	else m_pMenuButton->SetCustomColors(::GetSysColor(COLOR_INACTIVECAPTIONTEXT), ::GetSysColor(COLOR_INACTIVECAPTION));

	// Read in all the properties
	if (!m_View) return;
	
	HT_Resource topNode = HT_TopNode(m_View);
	void* data;
	PRBool foundData = FALSE;
	
	CRDFOutliner* pOutliner = (CRDFOutliner*)HT_GetViewFEData(m_View);
	if (pOutliner->InNavigationMode())
	{
		m_ForegroundColor = pOutliner->GetForegroundColor();
		m_BackgroundColor = pOutliner->GetBackgroundColor();
		m_BackgroundImageURL = pOutliner->GetBackgroundImageURL();
	}
	else
	{
		m_ForegroundColor = RGB(255,255,255);
		m_BackgroundColor = RGB(64,64,64);
		m_BackgroundImageURL = "";
	}

	// Foreground color
	HT_GetNodeData(topNode, gNavCenter->titleBarFGColor, HT_COLUMN_STRING, &data);
	if (data)
		WFE_ParseColor((char*)data, &m_ForegroundColor);
	
	// background color
	HT_GetNodeData(topNode, gNavCenter->titleBarBGColor, HT_COLUMN_STRING, &data);
	if (data)
		WFE_ParseColor((char*)data, &m_BackgroundColor);
	
	// Background image URL
	HT_GetNodeData(topNode, gNavCenter->titleBarBGURL, HT_COLUMN_STRING, &data);
	if (data)
		m_BackgroundImageURL = (char*)data;
	m_pBackgroundImage = NULL; // Clear out the BG image.
	
	CBrush faceBrush(m_BackgroundColor); // (::GetSysColor(m_bHasFocus? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION));

	if (m_BackgroundImageURL != "")
	{
		// There's a background that needs to be drawn.
		m_pBackgroundImage = LookupImage(m_BackgroundImageURL, NULL);
	}

	if (m_pBackgroundImage && m_pBackgroundImage->FrameSuccessfullyLoaded())
	{
		// Draw the strip of the background image that should be placed
		// underneath this line.
		PaintBackground(dc.m_hDC, rect, m_pBackgroundImage);
	}
	else
	{
		dc.FillRect(&rect, &faceBrush);
	}

	HPALETTE pOldPalette = NULL;
	if (sysInfo.m_iBitsPerPixel < 16 && (::GetDeviceCaps(dc.m_hDC, RASTERCAPS) & RC_PALETTE))
	{
		// Use the palette, since we have less than 16 bits per pixel and are
		// using a palette-based device.
		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
		::SelectPalette(dc.m_hDC, hPalette, FALSE);	

		// Find the nearest match in our palette for our colors.
		ResolveToPaletteColor(m_BackgroundColor, hPalette);
		ResolveToPaletteColor(m_ForegroundColor, hPalette);
	}

	// Draw the text.
	//HFONT font = WFE_GetUIFont(dc.m_hDC);
	CFont arialFont;
	LOGFONT lf;
	XP_MEMSET(&lf,0,sizeof(LOGFONT));
	lf.lfHeight = 120;
	lf.lfWeight = 700;
	strcpy(lf.lfFaceName, "Arial");
	arialFont.CreatePointFontIndirect(&lf, &dc);
	HFONT font = (HFONT)arialFont.GetSafeHandle();

	HFONT hOldFont = (HFONT)::SelectObject(dc.m_hDC, font);
	CRect sizeRect(0,0,10000,0);
	int height = ::DrawText(dc.m_hDC, titleText, titleText.GetLength(), &sizeRect, DT_CALCRECT | DT_WORDBREAK);
	
	if (sizeRect.Width() > rect.Width() - NAVBAR_CLOSEBOX - 9)
	{
		// Don't write into the close box area!
		sizeRect.right = sizeRect.left + (rect.Width() - NAVBAR_CLOSEBOX - 9);
	}
	sizeRect.left += 4;	// indent slightly horizontally
	sizeRect.right += 4;

	// Center the text vertically.
	sizeRect.top = (rect.Height() - height) / 2;
	sizeRect.bottom = sizeRect.top + height;

	// Draw the text
	int nOldBkMode = dc.SetBkMode(TRANSPARENT);

	UINT nFormat = DT_SINGLELINE | DT_VCENTER | DT_EXTERNALLEADING;
	COLORREF oldColor;

	oldColor = dc.SetTextColor(m_ForegroundColor);
	dc.DrawText((LPCSTR)titleText, -1, &sizeRect, nFormat);

	dc.SetTextColor(oldColor);
	dc.SetBkMode(nOldBkMode);
	
	int top = rect.top + (rect.Height() - NAVBAR_CLOSEBOX)/2;
	int left = rect.right - (3*(NAVBAR_CLOSEBOX+1)) - 4;
	
	HDC hDC = dc.m_hDC;
	CRDFImage* pImage = LookupImage("http://rdf.netscape.com/rdf/closebox.gif", NULL);
	DrawRDFImage(pImage, left, top, 16, 16, hDC, m_BackgroundColor);

	left += NAVBAR_CLOSEBOX+1;
	pImage = LookupImage("http://rdf.netscape.com/rdf/closebox.gif", NULL);
	DrawRDFImage(pImage, left, top, 16, 16, hDC, m_BackgroundColor);

	left += NAVBAR_CLOSEBOX+1;
	pImage = LookupImage("http://rdf.netscape.com/rdf/closebox.gif", NULL);
	DrawRDFImage(pImage, left, top, 16, 16, hDC, m_BackgroundColor);
}

int CNavTitleBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	return 0;
}

void CNavTitleBar::OnLButtonDown (UINT nFlags, CPoint point )
{
	// Called when the user clicks on the menu bar.  Start a drag or collapse the view.
	CRect rect;
	GetClientRect(&rect);
	
	int left = rect.right - NAVBAR_CLOSEBOX - 5;
	int right = left + NAVBAR_CLOSEBOX;
	int top = rect.top + (rect.Height() - NAVBAR_CLOSEBOX)/2;
	int bottom = top + NAVBAR_CLOSEBOX;

	CRect closeBoxRect(left, top, right, bottom);
	left -= NAVBAR_CLOSEBOX;
	right -= NAVBAR_CLOSEBOX;
	CRect modeBoxRect(left, top, right, bottom);
	left -= NAVBAR_CLOSEBOX;
	right -= NAVBAR_CLOSEBOX;
	CRect sortBoxRect(left, top, right, bottom);

	if (closeBoxRect.PtInRect(point))
	{
		// Destroy the window.
		CFrameWnd* pFrameWnd = GetParentFrame();
		if (pFrameWnd->IsKindOf(RUNTIME_CLASS(CNSNavFrame)))
			((CNSNavFrame*)pFrameWnd)->DeleteNavCenter();
	}
	else if (modeBoxRect.PtInRect(point))
	{
		CRDFOutliner* pOutliner = (CRDFOutliner*)HT_GetViewFEData(m_View);
		pOutliner->SetNavigationMode(!pOutliner->InNavigationMode());
	}
	else if (sortBoxRect.PtInRect(point))
	{
		
	}
	else
	{
		m_PointHit = point;
		CFrameWnd* pFrameWnd = GetParentFrame();
		if (pFrameWnd->IsKindOf(RUNTIME_CLASS(CNSNavFrame)))
			SetCapture();
	}
}


void CNavTitleBar::OnMouseMove(UINT nFlags, CPoint point)
{
	if (GetCapture() == this)
	{
		CNSNavFrame* navFrameParent = (CNSNavFrame*)GetParentFrame();
		
		if (abs(point.x - m_PointHit.x) > 3 ||
			abs(point.y - m_PointHit.y) > 3)
		{
			ReleaseCapture();

			// Start a drag
			CRDFOutliner* pOutliner = (CRDFOutliner*)HT_GetViewFEData(m_View);
			MapWindowPoints(navFrameParent, &point, 1); 
			navFrameParent->StartDrag(point);
		}
	}
}

void CNavTitleBar::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (GetCapture() == this) 
	{
		ReleaseCapture();
	}
}

void CNavTitleBar::OnSize( UINT nType, int cx, int cy )
{	
}


void CNavTitleBar::SetHTView(HT_View view)
{
	titleText = "";
	m_View = view;
	if (view)
	{
		HT_Resource r = HT_TopNode(view);
		if (r)
		{
			titleText = HT_GetNodeName(r);
		}
		Invalidate();
	}
}
