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
#include "cxabstra.h"
#include "navbar.h"
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
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CNavTitleBar::CNavTitleBar()
:m_bHasFocus(FALSE), m_bDrawCloseFrame(FALSE), m_bDrawModeFrame(FALSE), m_bDrawAddFrame(FALSE)
{
	m_pBackgroundImage = NULL;
	m_View = NULL;
	m_hFocusTimer = 0;
}

CNavTitleBar::~CNavTitleBar()
{
}

void CNavTitleBar::OnPaint( )
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);

	// Read in all the properties
	if (!m_View) return;
	
	HT_Resource topNode = HT_TopNode(m_View);
	void* data;
	PRBool foundData = FALSE;
	
	m_ForegroundColor = RGB(0,0,0);
	m_BackgroundColor = RGB(192,192,192);
	m_BackgroundImageURL = "";

	m_ControlStripForegroundColor = RGB(255,255,255);
	m_ControlStripBackgroundColor = RGB(64,64,64);
	m_ControlStripBackgroundImageURL = "";

	// Control strip colors
	// Foreground color
	HT_GetTemplateData(topNode, gNavCenter->controlStripFGColor, HT_COLUMN_STRING, &data);
	if (data)
		WFE_ParseColor((char*)data, &m_ControlStripForegroundColor);
	
	// background color
	HT_GetTemplateData(topNode, gNavCenter->controlStripBGColor, HT_COLUMN_STRING, &data);
	if (data)
		WFE_ParseColor((char*)data, &m_ControlStripBackgroundColor);
	
	// Background image URL
	HT_GetTemplateData(topNode, gNavCenter->controlStripBGURL, HT_COLUMN_STRING, &data);
	if (data)
		m_ControlStripBackgroundImageURL = (char*)data;
	m_pControlStripBackgroundImage = NULL; // Clear out the BG image.

	// Main Title strip colors
	// Foreground color
	HT_GetTemplateData(topNode, gNavCenter->titleBarFGColor, HT_COLUMN_STRING, &data);
	if (data)
		WFE_ParseColor((char*)data, &m_ForegroundColor);
	
	// background color
	HT_GetTemplateData(topNode, gNavCenter->titleBarBGColor, HT_COLUMN_STRING, &data);
	if (data)
		WFE_ParseColor((char*)data, &m_BackgroundColor);
	
	// Background image URL
	HT_GetTemplateData(topNode, gNavCenter->titleBarBGURL, HT_COLUMN_STRING, &data);
	if (data)
		m_BackgroundImageURL = (char*)data;
	m_pBackgroundImage = NULL; // Clear out the BG image.
	
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
		ResolveToPaletteColor(m_ControlStripBackgroundColor, hPalette);
		ResolveToPaletteColor(m_ControlStripForegroundColor, hPalette);
	}

	CRect controlStripRect(rect);
	CRect titleBarRect(rect);

	int navBarTotalHeight = 0;
	int navBarControlStripHeight = 0;
	int navBarTitleBarHeight = 0;

	HT_GetTemplateData(topNode, gNavCenter->showTitleBar, HT_COLUMN_STRING, &data);
	if (data)
	{
		CString answer((char*)data);
		if (answer.GetLength() > 0 && (answer.GetAt(0) == 'Y' || answer.GetAt(0) == 'y'))
		{
			navBarTotalHeight += NAVBAR_TITLEBAR_HEIGHT;
			navBarTitleBarHeight += NAVBAR_TITLEBAR_HEIGHT;
		}
	}

	// Whether or not to show the control strip.
	HT_GetTemplateData(topNode, gNavCenter->showControlStrip, HT_COLUMN_STRING, &data);
	if (data)
	{
		CString answer((char*)data);
		if (answer.GetLength() > 0 && (answer.GetAt(0) == 'Y' || answer.GetAt(0) == 'y'))
		{
			navBarTotalHeight += NAVBAR_CONTROLSTRIP_HEIGHT;
			navBarControlStripHeight = NAVBAR_CONTROLSTRIP_HEIGHT;
		}
	}

	controlStripRect.top = navBarTitleBarHeight;
	controlStripRect.bottom = navBarTotalHeight;
	titleBarRect.top = 0;
	titleBarRect.bottom = controlStripRect.top;

	CBrush faceBrush(m_BackgroundColor); 
	if (m_BackgroundImageURL != "")
	{
		// There's a background that needs to be drawn.
		m_pBackgroundImage = LookupImage(m_BackgroundImageURL, NULL);
	}

	if (m_pBackgroundImage && m_pBackgroundImage->FrameSuccessfullyLoaded())
	{
		// Draw the strip of the background image that should be placed
		// underneath this line.
		PaintBackground(dc.m_hDC, titleBarRect, m_pBackgroundImage);
	}
	else
	{
		dc.FillRect(&titleBarRect, &faceBrush);
	}

	CBrush controlStripFaceBrush(m_ControlStripBackgroundColor); 
	if (m_ControlStripBackgroundImageURL != "")
	{
		// There's a background that needs to be drawn.
		m_pControlStripBackgroundImage = LookupImage(m_ControlStripBackgroundImageURL, NULL);
	}

	if (m_pControlStripBackgroundImage && m_pControlStripBackgroundImage->FrameSuccessfullyLoaded())
	{
		// Draw the strip of the background image that should be placed
		// underneath this line.
		PaintBackground(dc.m_hDC, controlStripRect, m_pControlStripBackgroundImage);
	}
	else
	{
		dc.FillRect(&controlStripRect, &controlStripFaceBrush);
	}

	if (navBarTitleBarHeight > 0)
	{
		// Draw the title strip text.
		CString titleText(HT_GetNodeName(HT_TopNode(m_View)));

		CFont arialFont;
		LOGFONT lf;
		XP_MEMSET(&lf,0,sizeof(LOGFONT));
		lf.lfHeight = 120;
		lf.lfWeight = 700;
		strcpy(lf.lfFaceName, "Arial");
		arialFont.CreatePointFontIndirect(&lf, &dc);
		HFONT font = (HFONT)arialFont.GetSafeHandle();

		HFONT hOldFont = (HFONT)::SelectObject(dc.m_hDC, font);
		CRect sizeRect(titleBarRect);
		int height = CIntlWin::DrawText(CS_UTF8, dc.m_hDC, titleText.GetBuffer(0), titleText.GetLength(), &sizeRect, DT_CALCRECT | DT_WORDBREAK);
		
		if (sizeRect.Width() > rect.Width() - 9)
		{
			// Don't write into the close box area!
			sizeRect.right = sizeRect.left + (rect.Width() - 9);
		}
		sizeRect.left += 4;	// indent slightly horizontally
		sizeRect.right += 4;

		// Center the text vertically.
		sizeRect.top = (titleBarRect.Height() - height) / 2;
		sizeRect.bottom = sizeRect.top + height;

		// Draw the text
		int nOldBkMode = dc.SetBkMode(TRANSPARENT);

		UINT nFormat = DT_SINGLELINE | DT_VCENTER | DT_EXTERNALLEADING;
		COLORREF oldColor;

		oldColor = dc.SetTextColor(m_ForegroundColor);
		CIntlWin::DrawText(CS_UTF8, dc.m_hDC, titleText.GetBuffer(0), -1, &sizeRect, nFormat);

		dc.SetTextColor(oldColor);
		dc.SetBkMode(nOldBkMode);
		::SelectObject(dc.m_hDC, hOldFont);	
	}

	if (navBarControlStripHeight > 0)
	{
		// Draw the control strip text.
		CFont smallArialFont;
		LOGFONT lf2;
		XP_MEMSET(&lf2,0,sizeof(LOGFONT));
		lf2.lfHeight = 90;
		lf2.lfWeight = 400;
		strcpy(lf2.lfFaceName, "Arial");
		smallArialFont.CreatePointFontIndirect(&lf2, &dc);
		HFONT smallFont = (HFONT)smallArialFont.GetSafeHandle();
		HFONT hOldFont = (HFONT)::SelectObject(dc.m_hDC, smallFont);

		// The ADD button
		CString addText("Add Page"); // Will use an extensible HT mechanism eventually.  Hardcode for now.
		//HT_GetTemplateData(topNode, gNavCenter->controlStripModeText, HT_COLUMN_STRING, &data);
		//if (data)
		//	modeText = (char*)data;
		
		CRect addRect(controlStripRect);
		int smallHeight = CIntlWin::DrawText(CS_UTF8, dc.m_hDC, addText.GetBuffer(0), addText.GetLength(), &addRect, DT_CALCRECT | DT_WORDBREAK);
		
		if (addRect.Width() > rect.Width() - 9)
		{
			// Don't write into the close box area!
			addRect.right = addRect.left + (rect.Width() - 9);
		}
		addRect.left += 4;	// indent slightly horizontally
		addRect.right += 4;

		// Center the text vertically.
		addRect.top = controlStripRect.top + (controlStripRect.Height() - smallHeight) / 2;
		addRect.bottom = addRect.top + smallHeight;

		// Cache the rect
		cachedAddRect.top = controlStripRect.top;
		cachedAddRect.left = 0;
		cachedAddRect.bottom = controlStripRect.bottom;
		cachedAddRect.right = addRect.right + 3;

		// The MANAGE button
		CString modeText("Edit Bookmarks"); // Will use an extensible HT mechanism eventually.  Hardcode for now.
		//HT_GetTemplateData(topNode, gNavCenter->controlStripModeText, HT_COLUMN_STRING, &data);
		//if (data)
		//	modeText = (char*)data;

		CRect modeRect(controlStripRect);
		smallHeight = CIntlWin::DrawText(CS_UTF8,dc.m_hDC, modeText.GetBuffer(0), modeText.GetLength(), &modeRect, DT_CALCRECT | DT_WORDBREAK);
		
		if (modeRect.Width() > rect.Width() - 9)
		{
			// Don't write into the close box area!
			modeRect.right = modeRect.left + (rect.Width() - 9);
		}
		modeRect.left += cachedAddRect.right + 10;	// account for add rect and indent slightly horizontally
		modeRect.right += cachedAddRect.right + 10;

		// Center the text vertically.
		modeRect.top = addRect.top;
		modeRect.bottom = addRect.bottom;
		modeRect.right += 4;

		// Cache the rect
		cachedModeRect.top = controlStripRect.top;
		cachedModeRect.left = cachedAddRect.right + 6;
		cachedModeRect.bottom = controlStripRect.bottom;
		cachedModeRect.right = cachedModeRect.left + modeRect.Width() + 3;

		CString closeText("Close");  
		HT_GetTemplateData(topNode, gNavCenter->controlStripCloseText, HT_COLUMN_STRING, &data);
		if (data)
		  closeText = (char*)data;

		CRect closeRect(controlStripRect);
		CIntlWin::DrawText(CS_UTF8, dc.m_hDC, closeText.GetBuffer(0), closeText.GetLength(), &closeRect, DT_CALCRECT | DT_WORDBREAK);
		
		int closeWidth = closeRect.Width();

		closeRect.right = rect.right - 4;
		closeRect.left = closeRect.right - closeWidth;

		// Center the text vertically.
		closeRect.top = modeRect.top;
		closeRect.bottom = modeRect.bottom;

		CRect arrowRect;
		arrowRect.top = controlStripRect.top;
		arrowRect.left = closeRect.left - 12;
		arrowRect.right = arrowRect.left + 12;
		arrowRect.bottom = controlStripRect.bottom;
		
		// Cache the rect
		cachedCloseRect.top = controlStripRect.top;
		cachedCloseRect.left = arrowRect.left;
		cachedCloseRect.bottom = controlStripRect.bottom;
		cachedCloseRect.right = closeRect.right + 3;

		// Now compute the close box rect.
		HT_Pane pane = HT_GetPane(HT_GetView(topNode));
		if (HT_GetWindowType(pane) == HT_STANDALONE_WINDOW ||
			HT_GetWindowType(pane) == HT_EMBEDDED_WINDOW)
		{
			// Don't allow the close box to exist.
			cachedCloseRect.SetRect(0,0,0,0);
			closeRect.SetRect(0,0,0,0);
			arrowRect.SetRect(0,0,0,0);
		}

		if (HT_GetWindowType(pane) != HT_STANDALONE_WINDOW &&
			HT_GetWindowType(pane) != HT_EMBEDDED_WINDOW)
			DrawArrow(dc.m_hDC, m_ControlStripForegroundColor, LEFT_ARROW, arrowRect, TRUE);

		// Draw the text
		int nOldBkMode = dc.SetBkMode(TRANSPARENT);
		int oldColor = dc.SetTextColor(m_ControlStripForegroundColor);
		UINT nFormat = DT_SINGLELINE | DT_VCENTER | DT_EXTERNALLEADING;

		if (HT_GetWindowType(pane) != HT_STANDALONE_WINDOW &&
			HT_GetWindowType(pane) != HT_EMBEDDED_WINDOW)
			CIntlWin::DrawText(CS_UTF8, dc.m_hDC, closeText.GetBuffer(0), -1, &closeRect, nFormat);
		CIntlWin::DrawText(CS_UTF8, dc.m_hDC, modeText.GetBuffer(0), -1, &modeRect, nFormat);
		CIntlWin::DrawText(CS_UTF8, dc.m_hDC, addText.GetBuffer(0), -1, &addRect, nFormat);

		// See if we're supposed to draw a framing rect.
		
		CBrush controlBrush(m_ControlStripForegroundColor);
		if (m_bDrawCloseFrame)
		{
			dc.FrameRect(cachedCloseRect, &controlBrush);
		}
		if (m_bDrawModeFrame)
		{
			dc.FrameRect(cachedModeRect, &controlBrush);
		}
		if (m_bDrawAddFrame)
		{
			dc.FrameRect(cachedAddRect, &controlBrush);
		}

		dc.SetTextColor(oldColor);
		dc.SetBkMode(nOldBkMode);
		::SelectObject(dc.m_hDC, hOldFont);	
	}
}

int CNavTitleBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	return 0;
}

void CNavTitleBar::OnLButtonDown (UINT nFlags, CPoint point )
{
	// Called when the user clicks on us.
	m_PointHit = point;
	CFrameWnd* pFrameWnd = GetParentFrame();
	if (pFrameWnd->IsKindOf(RUNTIME_CLASS(CNSNavFrame)))
		SetCapture();
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
	else
	{
		BOOL oldCloseFrame = m_bDrawCloseFrame;
		BOOL oldModeFrame = m_bDrawModeFrame;
		BOOL oldAddFrame = m_bDrawAddFrame;

		m_bDrawCloseFrame = FALSE;
		m_bDrawModeFrame = FALSE;
		m_bDrawAddFrame = FALSE;

		if (cachedCloseRect.PtInRect(point))
		{
			m_bDrawCloseFrame = TRUE;
			m_hFocusTimer = SetTimer(IDT_STRIPFOCUS, STRIPFOCUS_DELAY_MS, NULL);
		}
		else if (cachedModeRect.PtInRect(point))
		{
			m_bDrawModeFrame = TRUE;
			m_hFocusTimer = SetTimer(IDT_STRIPFOCUS, STRIPFOCUS_DELAY_MS, NULL);
		}
		else if (cachedAddRect.PtInRect(point))
		{
			m_bDrawAddFrame = TRUE;
			m_hFocusTimer = SetTimer(IDT_STRIPFOCUS, STRIPFOCUS_DELAY_MS, NULL);
		}
		
		if (oldCloseFrame != m_bDrawCloseFrame)
			InvalidateRect(cachedCloseRect);

		if (oldModeFrame != m_bDrawModeFrame)
			InvalidateRect(cachedModeRect);

		if (oldAddFrame != m_bDrawAddFrame)
			InvalidateRect(cachedAddRect);
	}
}

void CNavTitleBar::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (GetCapture() == this) 
	{
		ReleaseCapture();
	}
	
	if (cachedCloseRect.PtInRect(point))
	{
		// Destroy the window.
		CFrameWnd* pFrameWnd = GetParentFrame();
		if (pFrameWnd->IsKindOf(RUNTIME_CLASS(CNSNavFrame)))
			((CNSNavFrame*)pFrameWnd)->DeleteNavCenter();
	}
	else if (cachedModeRect.PtInRect(point))
	{
		//CRDFOutliner* pOutliner = (CRDFOutliner*)HT_GetViewFEData(m_View);
		//HT_ToggleTreeMode(m_View);

		CFrameWnd* pFrameWnd = GetParentFrame();
		if (pFrameWnd->IsKindOf(RUNTIME_CLASS(CNSNavFrame)))
		{
			CNSNavFrame* pNavFrame = (CNSNavFrame*)pFrameWnd;
			
			CRDFOutliner* pOutliner = (CRDFOutliner*)
					(pNavFrame->GetContentView()->GetOutlinerParent()->GetOutliner());
			pOutliner->SetIsPopup(FALSE);
			pNavFrame->UnhookFromButton();
			pNavFrame->ForceFloat(TRUE);
		}
	}
	else if (cachedAddRect.PtInRect(point))
	{
		CGenericFrame* pFrame = (CGenericFrame*)FEU_GetLastActiveFrame(MWContextBrowser);
		if (pFrame)
		{
			CAbstractCX* pAbstract = pFrame->GetMainContext();
			MWContext* mwContext = pAbstract->GetContext();
			History_entry *pHistEnt = SHIST_GetCurrent( &(mwContext->hist) );
			if (pHistEnt)
			{
				HT_AddToContainer( HT_TopNode(m_View), pHistEnt->address, pHistEnt->title );
			}
		}
	}
}

void CNavTitleBar::OnSize( UINT nType, int cx, int cy )
{	
}


void CNavTitleBar::SetHTView(HT_View view)
{
	m_View = view;
	Invalidate();
}

void CNavTitleBar::OnTimer(UINT nIDEvent)
{
	if (nIDEvent == IDT_STRIPFOCUS)
	{
		POINT point;

		KillTimer(IDT_STRIPFOCUS);
		m_hFocusTimer = 0;
		
		GetCursorPos(&point);

		CRect rcClient;
		GetWindowRect(&rcClient);

		if (!rcClient.PtInRect(point))
		{
			m_bDrawCloseFrame = FALSE;
			m_bDrawModeFrame = FALSE;
			m_bDrawAddFrame = FALSE;

			Invalidate();
			UpdateWindow();
		}
		else
			m_hFocusTimer = SetTimer(IDT_STRIPFOCUS, STRIPFOCUS_DELAY_MS, NULL);
	}

	CWnd::OnTimer(nIDEvent);
}

int CNavTitleBar::GetHeightBasedOnProperties()
{
	if (!m_View)
		return 0;

	HT_Resource topNode = HT_TopNode(m_View);
	void* data;
	PRBool foundData = FALSE;
	
	// Whether or not to show the title bar.
	int height = 0;
	HT_GetTemplateData(topNode, gNavCenter->showTitleBar, HT_COLUMN_STRING, &data);
	if (data)
	{
		CString answer((char*)data);
		if (answer.GetLength() > 0 && (answer.GetAt(0) == 'Y' || answer.GetAt(0) == 'y'))
			height += NAVBAR_TITLEBAR_HEIGHT;
	}

	// Whether or not to show the control strip.
	HT_GetTemplateData(topNode, gNavCenter->showControlStrip, HT_COLUMN_STRING, &data);
	if (data)
	{
		CString answer((char*)data);
		if (answer.GetLength() > 0 && (answer.GetAt(0) == 'Y' || answer.GetAt(0) == 'y'))
			height += NAVBAR_CONTROLSTRIP_HEIGHT;
	}

	return height;
}
