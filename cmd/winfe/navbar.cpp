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

BEGIN_MESSAGE_MAP(CNavMenuButton, CLinkToolbarButton)
	ON_MESSAGE(NSDRAGMENUOPEN, OnDragMenuOpen) 
END_MESSAGE_MAP()

CNavMenuButton::CNavMenuButton()
:m_HTView(NULL)
{
	m_bShouldShowRMMenu = FALSE;
}

extern void DrawUpButton(HDC dc, CRect& rect);

LRESULT CNavMenuButton::OnDragMenuOpen(WPARAM wParam, LPARAM lParam)
{
// Set the focus to the tree view.
	if (!m_HTView)
		return 1;

	CSelectorButton* pSelectorButton = (CSelectorButton*)HT_GetViewFEData(m_HTView);
	if (!pSelectorButton)
		return 1;

	// Get the tree view and set focus to it.
	CRDFContentView* pView = pSelectorButton->GetTreeView();
	if (pView)
		pView->SetFocus();
	
// Set our drop menu's user data.
	m_pCachedDropMenu = (CDropMenu *)lParam;  // Set our drop menu
	
	m_MenuCommandMap.Clear();
	HT_View theView = m_HTView;
	HT_Resource node = NULL;
	node = HT_GetNextSelection(theView, node);
	BOOL bg = (node == NULL);
	
	HT_Cursor theCursor = HT_NewContextualMenuCursor(theView, PR_FALSE, (PRBool)bg);
	if (theCursor != NULL)
	{
		// We have a cursor. Attempt to iterate
		HT_MenuCmd theCommand; 
		while (HT_NextContextMenuItem(theCursor, &theCommand))
		{
			char* menuName = HT_GetMenuCmdName(theCommand);
			if (theCommand == HT_CMD_SEPARATOR)
				m_pCachedDropMenu->AppendMenu(MF_SEPARATOR, 0, TRUE, NULL, NULL);
			else
			{
				// Add the command to our command map
				CRDFMenuCommand* rdfCommand = new CRDFMenuCommand(menuName, theCommand);
				int index = m_MenuCommandMap.AddCommand(rdfCommand);
				m_pCachedDropMenu->AppendMenu(MF_STRING, index+FIRST_HT_MENU_ID, menuName, TRUE, NULL, NULL, NULL);
			}
		}
		HT_DeleteCursor(theCursor);
	}
	return 1;
}

BOOL CNavMenuButton::OnCommand(UINT wParam, LONG lParam)
{
	BOOL bRtn = TRUE;
	
	if (wParam >= FIRST_HT_MENU_ID && wParam <= LAST_HT_MENU_ID)
	{
		// A selection was made from the context menu.
		// Use the menu map to get the HT command value
		CRDFMenuCommand* theCommand = (CRDFMenuCommand*)(m_MenuCommandMap.GetCommand((int)wParam-FIRST_HT_MENU_ID));
		if (theCommand)
		{
			HT_MenuCmd htCommand = theCommand->GetHTCommand();
			HT_DoMenuCmd(HT_GetPane(m_HTView), htCommand);
		}
		
	}

	return bRtn;
}

void CNavMenuButton::UpdateView(HT_View v)
{
	m_HTView = v;
	if (v == NULL)
		SetText("No View Selected.");
	else SetText(HT_GetNodeName(HT_TopNode(m_HTView)));
}

void CNavMenuButton::UpdateButtonText(CRect rect)
{
	CString originalText = "No View Selected.";
	if (m_HTView != NULL)
		originalText = HT_GetNodeName(HT_TopNode(m_HTView));    
	
	int currCount = originalText.GetLength();
        
    // Start off at the maximal string
    CString strTxt = originalText;

	CSize theSize = GetButtonSizeFromChars(originalText, currCount);
	int horExtent = theSize.cx;
	int width = horExtent;
	int cutoff = rect.right - NAVBAR_CLOSEBOX - 9;
	if (width > cutoff && cutoff > 0)
			width = cutoff;
		
	while (horExtent > width)
	{
		strTxt = originalText.Left(currCount-3) + "...";
		theSize = GetButtonSizeFromChars(strTxt, currCount);
		horExtent = theSize.cx;
		currCount--;
	}

	SetTextWithoutResize(strTxt);

	int height = GetRequiredButtonSize().cy;
	MoveWindow(2, (rect.Height()-height)/2, width, height);
}

BEGIN_MESSAGE_MAP(CNavMenuBar, CWnd)
	//{{AFX_MSG_MAP(CNavMenuBar)
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

CNavMenuBar::CNavMenuBar()
:m_pSelectorButton(NULL), m_pMenuButton(NULL), m_bHasFocus(FALSE)
{
}

CNavMenuBar::~CNavMenuBar()
{
	delete m_pMenuButton;
}

void CNavMenuBar::OnPaint( )
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);
	
	if (m_bHasFocus)
		m_pMenuButton->SetCustomColors(::GetSysColor(COLOR_CAPTIONTEXT), ::GetSysColor(COLOR_ACTIVECAPTION));
	else m_pMenuButton->SetCustomColors(::GetSysColor(COLOR_INACTIVECAPTIONTEXT), ::GetSysColor(COLOR_INACTIVECAPTION));

	CBrush faceBrush(::GetSysColor(m_bHasFocus? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION));

	dc.FillRect(&rect, &faceBrush);
	
	// Draw the close box at the edge of the bar.
	
	int left = rect.right - NAVBAR_CLOSEBOX - 5;
	int right = left + NAVBAR_CLOSEBOX;
	int top = rect.top + (rect.Height() - NAVBAR_CLOSEBOX)/2;
	int bottom = top + NAVBAR_CLOSEBOX;

	CRect edgeRect(left, top, right, bottom);
	CBrush* closeBoxBrush = CBrush::FromHandle(sysInfo.m_hbrBtnFace);
	dc.FillRect(&edgeRect, closeBoxBrush);

	DrawUpButton(dc.m_hDC, edgeRect);

	HPEN hPen = (HPEN)::CreatePen(PS_SOLID, 1, RGB(0,0,0));
	HPEN hOldPen = (HPEN)(dc.SelectObject(hPen));

	dc.MoveTo(edgeRect.left+4, edgeRect.top+4);
	dc.LineTo(edgeRect.right-5, edgeRect.bottom-4);
	dc.MoveTo(edgeRect.right-6, edgeRect.top+4);
	dc.LineTo(edgeRect.left+3, edgeRect.bottom-4);
	
	HPEN hShadowPen = ::CreatePen(PS_SOLID, 1, ::GetSysColor(COLOR_3DSHADOW));

	dc.SelectObject(hShadowPen);
	dc.MoveTo(rect.left, rect.bottom-1);
	dc.LineTo(rect.right, rect.bottom-1);
	dc.SelectObject(hOldPen);

	VERIFY(::DeleteObject(hPen));
	VERIFY(::DeleteObject(hShadowPen));
}

int CNavMenuBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	m_pMenuButton = new CNavMenuButton();
	BOOKMARKITEM dummy;

	m_pMenuButton->Create(this, FALSE, CSize(32, 21), CSize(32, 21), "No view selected.", 
						  "Click here to view the options menu.",
						  "Click on the button to view the options menu.", CSize(0,0), 100, 15, dummy,
						  NULL, TB_HAS_IMMEDIATE_MENU | TB_HAS_DRAGABLE_MENU);
	return 0;
}

void CNavMenuBar::OnLButtonDown (UINT nFlags, CPoint point )
{
	// Called when the user clicks on the menu bar.  Start a drag or collapse the view.
	CRect rect;
	GetClientRect(&rect);
	
	int left = rect.right - NAVBAR_CLOSEBOX - 5;
	int right = left + NAVBAR_CLOSEBOX;
	int top = rect.top + (rect.Height() - NAVBAR_CLOSEBOX)/2;
	int bottom = top + NAVBAR_CLOSEBOX;

	CRect edgeRect(left, top, right, bottom);
	if (edgeRect.PtInRect(point))
	{
		// Collapse the view
		if (m_pSelectorButton)
			m_pSelectorButton->OnAction();
	}
	else
	{
		// Set the focus.
		if (m_pSelectorButton)
		{
			CRDFContentView* pView = m_pSelectorButton->GetTreeView();
			if (pView)
				pView->SetFocus();
		}

		m_PointHit = point;
		SetCapture();
	}
}


void CNavMenuBar::OnMouseMove(UINT nFlags, CPoint point)
{
	if (GetCapture() == this)
	{
		CNSNavFrame* navFrameParent = (CNSNavFrame*)GetParentFrame();
		
		if (abs(point.x - m_PointHit.x) > 3 ||
			abs(point.y - m_PointHit.y) > 3)
		{
			ReleaseCapture();

			// Start a drag
			MapWindowPoints(navFrameParent, &point, 1); 
			navFrameParent->StartDrag(point);
		}
	}
}

void CNavMenuBar::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (GetCapture() == this) 
	{
		ReleaseCapture();
	}
}

void CNavMenuBar::OnSize( UINT nType, int cx, int cy )
{	
	if (m_pMenuButton)
	{
		CRect rect;
		GetClientRect(&rect);
		m_pMenuButton->UpdateButtonText(rect);
	}
}

void CNavMenuBar::UpdateView(CSelectorButton* pButton, HT_View view)
{ 
	m_pSelectorButton = pButton; 

	if (m_pMenuButton)
	{
		CRect rect;
		GetClientRect(&rect);
		m_pMenuButton->UpdateView(view);
		m_pMenuButton->UpdateButtonText(rect);
	}

	if (pButton && pButton->GetTreeView())
	{
		((CRDFOutliner*)(pButton->GetTreeView()->GetOutlinerParent()->GetOutliner()))->SetDockedMenuBar(this);
	}
}
