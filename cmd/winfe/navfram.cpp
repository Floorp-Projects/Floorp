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

// navfram.cpp : implementation of the CNSNavFrame class
//

#include "stdafx.h"
#include "rdfliner.h"
#include "navfram.h"
#include "feimage.h"
#include "cxicon.h"
#include "prefapi.h"
#include "xp_ncent.h"
#include "navbar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CNSNavFrame

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CNSNavFrame, CFrameWnd)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CNSNavFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CNSNavFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_NCLBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_MESSAGE(WM_SIZEPARENT, OnSizeParent)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
#define MFS_MOVEFRAME       0x00000800L // no sizing, just moving
#define MFS_SYNCACTIVE      0x00000100L // synchronize activation w/ parent
#define MFS_4THICKFRAME     0x00000200L // thick frame all around (no tiles)
#define MFS_THICKFRAME      0x00000400L // use instead of WS_THICKFRAME
#define MFS_MOVEFRAME       0x00000800L // no sizing, just moving
#define MFS_BLOCKSYSMENU    0x00001000L // block hit testing on system menu

/////////////////////////////////////////////////////////////////////////////
// CNSNavFrame construction/destruction

CNSNavFrame::CNSNavFrame()
{
	m_nsContent= NULL;
	m_bDragging = FALSE;
	m_DragWnd = NULL;
	m_Node = NULL;
	m_pButton = NULL;
}

CNSNavFrame::~CNSNavFrame()
{
	if (m_dwOverDockStyle == DOCKSTYLE_DOCKEDLEFT)
	{
		PREF_SetIntPref(gPrefDockPercentage, m_DockSize);
	}
	else if (m_dwOverDockStyle == DOCKSTYLE_FLOATING)
	{
		PREF_SetRectPref(gPrefFloatRect, (int16)m_rectFloat.left, (int16)m_rectFloat.top, (int16)m_rectFloat.right, (int16)m_rectFloat.bottom);
	}
}

void CNSNavFrame::OnClose()
{
	if (m_dwOverDockStyle == DOCKSTYLE_FLOATING)
	{
		GetWindowRect(&m_rectFloat);
	}

	CFrameWnd::OnClose();
}

void CNSNavFrame::UpdateTitleBar(HT_View pView)
{
	CString title("Navigation Center [");
	title += HT_GetNodeName(HT_TopNode(pView));
	title += "]";

	SetWindowText(title);
}

int CNSNavFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
#ifdef XP_WIN32
	SetClassLong(GetSafeHwnd(), GCL_HICON, 	(long)LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(2)));
#else
	SetClassWord(GetSafeHwnd(), GCW_HICON, 	(WORD)LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(2)));
#endif
	LONG style =  ::GetWindowLong(GetSafeHwnd(), GWL_STYLE);
	::SetWindowLong(GetSafeHwnd(), GWL_STYLE,  style | WS_CLIPCHILDREN);
#ifdef XP_WIN32
	::SetClassLong(GetSafeHwnd(), GCL_HBRBACKGROUND, (COLOR_BTNFACE + 1));
#else
	::SetClassWord(GetSafeHwnd(), GCW_HBRBACKGROUND, (COLOR_BTNFACE + 1));
#endif
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	return 0;
}

//------------------------------------------------------------------------
// void CNSNavFrame::CreateNewNavCenter() 
// Create a New NavCenter and Show it.
//------------------------------------------------------------------------
void CNSNavFrame::CreateNewNavCenter(CNSGenFrame* pParentFrame, BOOL useViewType, HT_ViewType viewType) 
{
// OBSOLETE: WILL GO AWAY SOON

// Read in our float rect pref.
	int16 left,right,top, bottom;
	PREF_GetRectPref(gPrefFloatRect,&left, &top, &right, &bottom);

// Create the window there.
	m_rectFloat.SetRect(left, top, right, bottom);
	m_rectDrag = m_rectFloat;

	CString title = "Navigation Center";
	Create( NULL, title, WS_OVERLAPPEDWINDOW, m_rectFloat, NULL);

// Hide initially.
	ShowWindow(SW_HIDE);
	
// Determine the pixels we should consume if docked
	int32 width;
	PREF_GetIntPref(gPrefDockPercentage, &width);
    m_DockSize = (int)width;
	
// Read in our dockstyle
	m_dwOverDockStyle = DOCKSTYLE_DOCKEDLEFT;

// Find out if a view should be displayed
	
	if (pParentFrame != NULL)
	{	
		// create a docked window
		DockFrame(pParentFrame, m_dwOverDockStyle);
	}
	else 
	{
		// Create a floating window
		m_dwOverDockStyle = DOCKSTYLE_FLOATING;
	}

// Show the window.
	ShowWindow(SW_SHOW);

	theApp.m_pRDFCX->TrackRDFWindow(this);

	if (useViewType)
		HT_SetSelectedView(GetHTPane(), HT_GetViewType(GetHTPane(), viewType));
}

void CNSNavFrame::DeleteNavCenter()
{
	CFrameWnd *pLayout = GetParentFrame();
	
	ShowWindow(SW_HIDE);
	SetParent(NULL);
	
    // Tell ParentFrame that we are not docked anymore.
    if (pLayout)
		pLayout->RecalcLayout();

	if (m_DragWnd)
		m_DragWnd->DestroyWindow();

	PostMessage(WM_CLOSE);

	UnhookFromButton();
}

void CNSNavFrame::UnhookFromButton()
{
	if (m_pButton)
	{
		m_pButton->SetDepressed(FALSE);
		m_pButton->Invalidate();
		CRDFToolbarHolder* pHolder = (CRDFToolbarHolder*)(m_pButton->GetParent()->GetParent()->GetParent());
		if (pHolder->GetCurrentButton(HT_POPUP_WINDOW) == m_pButton)
			pHolder->SetCurrentButton(NULL, HT_POPUP_WINDOW);
		else if (pHolder->GetCurrentButton(HT_DOCKED_WINDOW) == m_pButton)
			pHolder->SetCurrentButton(NULL, HT_DOCKED_WINDOW);

		m_pButton = NULL;
	}
}

void CNSNavFrame::MovePopupToDocked()
{
	if (m_pButton)
	{
		CRDFToolbarHolder* pHolder = (CRDFToolbarHolder*)(m_pButton->GetParent()->GetParent()->GetParent());
		if (pHolder->GetCurrentButton(HT_POPUP_WINDOW) == m_pButton)
		{
			pHolder->SetCurrentButton(NULL, HT_POPUP_WINDOW);
			pHolder->SetCurrentButton(m_pButton, HT_DOCKED_WINDOW);
		}
	}
}

BOOL CNSNavFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	m_nsContent = CRDFContentView::DisplayRDFTreeFromResource(this, 0, 0, 100, 100, m_Node, pContext); 
	CRDFOutliner* pOutliner = (CRDFOutliner*)(m_nsContent->GetOutlinerParent()->GetOutliner());
	SetHTNode(HT_TopNode(pOutliner->GetHTView()));

	// Read in our float rect pref.
	int16 left,right,top, bottom;
	PREF_GetRectPref(gPrefFloatRect,&left, &top, &right, &bottom);

	// Create the window there.
	m_rectFloat.SetRect(left, top, right, bottom);
	m_rectDrag = m_rectFloat;

	// Determine the pixels we should consume if docked
	int32 width;
	PREF_GetIntPref(gPrefDockPercentage, &width);
    m_DockSize = (int)width;
	
	return TRUE;
}

void CNSNavFrame::OnSize( UINT nType, int cx, int cy )
{
	if(m_dwOverDockStyle == DOCKSTYLE_FLOATING) 
	{
		// Make sure our float rect always matches our size when floating.
		GetWindowRect(&m_rectFloat);
		PREF_SetRectPref(gPrefFloatRect, (int16)m_rectFloat.left, (int16)m_rectFloat.top, (int16)m_rectFloat.right, (int16)m_rectFloat.bottom);
	}
	else if (m_dwOverDockStyle == DOCKSTYLE_DOCKEDLEFT)
	{
		// Recompute our docked percentage if the tree is visible
		CRect rect;
		GetClientRect(&rect);
		CalcClientArea(&m_parentRect);
		m_DockWidth = m_DockSize = rect.Width();
		PREF_SetIntPref(gPrefDockPercentage, m_DockSize);
	}

	int top = 0;
	CRect tempRect;
	m_nsContent->SetWindowPos(NULL, 0, 0, cx, cy, 0);
}

BOOL CNSNavFrame::PreCreateWindow(CREATESTRUCT & cs)
{

	cs.lpszClass = AfxRegisterWndClass( CS_DBLCLKS,
					theApp.LoadStandardCursor(IDC_ARROW), 
					(HBRUSH)(COLOR_BTNFACE + 1));
    return (CFrameWnd::PreCreateWindow(cs));
}

CNSNavFrame* CNSNavFrame::CreateFramedRDFViewFromResource(CWnd* pParent, int xPos, int yPos,
											 int width, int height, HT_Resource node)
{
	CNSNavFrame* pNavFrame = new CNSNavFrame();
	pNavFrame->SetHTNode(node);
	pNavFrame->Create(NULL, HT_GetNodeName(node), WS_POPUP, 
					  CRect(xPos, yPos, width, height),
					  pParent);
	pNavFrame->ShowWindow(SW_HIDE);

	HT_View view = HT_GetSelectedView(pNavFrame->GetHTPane());
	
	CRDFOutliner* pOutliner = (CRDFOutliner*)(pNavFrame->GetContentView()->GetOutlinerParent()->GetOutliner());
	
	// Display in the appropriate spot depending on our tree state (docked, standalone, or popup)
	int treeState = HT_GetTreeStateForButton(node);
	XP_RegisterNavCenter(pNavFrame->GetHTPane(), NULL);

	if (treeState == HT_POPUP_WINDOW)
	{
		HT_SetWindowType(pNavFrame->GetHTPane(), HT_POPUP_WINDOW);

		// Actually appear at the specified position and set the popup flag to be true.
		pNavFrame->SetDockStyle(DOCKSTYLE_POPUP);
	
		pNavFrame->SetWindowPos(&wndTopMost, xPos, yPos, width, height, 0);
		pNavFrame->ShowWindow(SW_SHOW);
		pOutliner->SetIsPopup(TRUE);
		
	}
	else if (treeState == HT_DOCKED_WINDOW)
	{
		HT_SetWindowType(pNavFrame->GetHTPane(), HT_DOCKED_WINDOW);

		// We're supposed to come up docked to the window.  Call DockFrame after setting
		// the correct dock style
		pNavFrame->SetDockStyle(DOCKSTYLE_DOCKEDLEFT);
		
		// Use the toolbar button to get to the top-level frame.
		CRDFToolbarButton* pButton = (CRDFToolbarButton*)HT_GetNodeFEData(node);
		CFrameWnd* pBaseWnd = pButton->GetTopLevelFrame();
		if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) 
		{
			CNSGenFrame* pFrame = (CNSGenFrame *)pBaseWnd;
			pNavFrame->DockFrame(pFrame, DOCKSTYLE_DOCKEDLEFT);
		}
		else pNavFrame->DeleteNavCenter(); // Somehow couldn't find the window. Should never happen.
	}
	/*else if (treeState == "Standalone" || treeState == "standalone")
	{
		pNavFrame->SetDockStyle(DOCKSTYLE_FLOATING);
		pNavFrame->MoveWindow(pNavFrame->GetFloatRect());
		pNavFrame->ForceFloat(TRUE);
	}*/

	pOutliner->SetFocus();
	
	pNavFrame->ShowWindow(SW_SHOW);

	return pNavFrame;
}

//------------------------------------------------------------------------------
// void CNSNavFrame::StartDrag(CPoint pt)
//	
//------------------------------------------------------------------------------

void CNSNavFrame::StartDrag(CPoint pt, BOOL mapDesktop)
{
	GetWindowRect(&m_dockingDragRect);
	CPoint cursorPt(pt);

	if (mapDesktop)
		MapWindowPoints(NULL, &cursorPt, 1);	// Convert the point to screen coords.
	
	CRDFOutliner* pOutliner = (CRDFOutliner*)(GetContentView()->GetOutlinerParent()->GetOutliner());
	
	if (XP_IsNavCenterDocked(GetHTPane()) || pOutliner->IsPopup()) 
	{
		pOutliner->SetIsPopup(FALSE);
		ForceFloat(FALSE);		
	}
	else ShowWindow(SW_HIDE);

	m_bDragging = TRUE;
	InitLoop();
	
	m_rectDrag = m_rectFloat;
  
	CNSGenFrame* pFrame = NULL;
    CFrameGlue *pGlue = CFrameGlue::GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERANDEDITOR);
    CFrameWnd *pBaseWnd = pGlue->GetFrameWnd();
    if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) 
	{
       pFrame = (CNSGenFrame *)pBaseWnd;
    }
    CalcClientArea(&m_parentRect, pFrame);
	pFrame->MapWindowPoints( NULL, &m_parentRect);

	// Cache the offset that we wish to preserve.
	m_nXOffset = cursorPt.x - m_dockingDragRect.left;
	m_nYOffset = cursorPt.y - m_dockingDragRect.top;

	// Compute the pixel sizes of vertically and horizontally docked windows
	ComputeDockingSizes();

	// initialize tracking state and enter tracking loop
	Move(cursorPt);   // call it here to handle special keys
	Track();
}

//------------------------------------------------------------------------------
//  void CNSNavFrame::Move(CPoint pt)       
//     called when we get WM_MOUSEMOVE message.
//------------------------------------------------------------------------------

void CNSNavFrame::Move(CPoint pt)       // called when mouse has moved
{
	short dwOverDockStyle = CanDock(pt);
	m_dockingDragRect =  m_parentRect;
		
	if (dwOverDockStyle == DOCKSTYLE_DOCKEDLEFT)
	{
		int height = m_dockingDragRect.Height();
		m_dockingDragRect.right = m_dockingDragRect.left + m_DockWidth;
		int yOffset = m_nYOffset;
		if (yOffset > height)
			yOffset = height;
		m_dockingDragRect.top = pt.y - yOffset;
		m_dockingDragRect.bottom = m_dockingDragRect.top + height;
	}
	else 
	{
		int height = m_rectDrag.Height();
		int width = m_rectDrag.Width();
		int xOffset = m_nXOffset;
		int yOffset = m_nYOffset;
		if (xOffset > width)
			xOffset = width;
		if (yOffset > height)
			yOffset = height;

		m_rectDrag.left = pt.x - m_nXOffset;
		m_rectDrag.top = pt.y - m_nYOffset;
		m_rectDrag.bottom = m_rectDrag.top + height;
		m_rectDrag.right = m_rectDrag.left + width;
	}
	
	DrawFocusRect(dwOverDockStyle);
}

//------------------------------------------------------------------------------
//	static BOOL nsModifyStyle(HWND hWnd, int nStyleOffset,
//		DWORD dwRemove, DWORD dwAdd, UINT nFlags)
//------------------------------------------------------------------------------

BOOL nsModifyStyle(HWND hWnd, int nStyleOffset, DWORD dwRemove, DWORD dwAdd, UINT nFlags)
{
	ASSERT(hWnd != NULL);
	DWORD dwStyle = ::GetWindowLong(hWnd, nStyleOffset);
	DWORD dwNewStyle = dwStyle;
	if (dwRemove != NULL)
		dwNewStyle = dwStyle & ~dwRemove;
	if (dwAdd != NULL)
		dwNewStyle = dwNewStyle | dwAdd;
	if (dwStyle == dwNewStyle)
		return FALSE;

	::SetWindowLong(hWnd, nStyleOffset, dwNewStyle);
	if (nFlags != 0)
	{
		::SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
			SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | nFlags);
	}
	return TRUE;
}


//------------------------------------------------------------------------------
//	void CNSNavFrame::CalcClientArea(RECT* lpRectClient)
//  
//	The function will return the the client area of CMainFrame minus any of the spaces
//  occupied by the tool bar.
//------------------------------------------------------------------------------

void CNSNavFrame::CalcClientArea(RECT* lpRectClient, CNSGenFrame * pParentFrame)
{

	AFX_SIZEPARENTPARAMS layout;
	HWND hWndLeftOver = NULL;
#ifdef XP_WIN32
	layout.bStretch = FALSE;
	layout.sizeTotal.cx = layout.sizeTotal.cy = 0;
#endif
	CNSGenFrame* pFrame = NULL;
	if (pParentFrame == NULL) {
        CFrameGlue *pGlue = CFrameGlue::GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERANDEDITOR);
        CFrameWnd *pBaseWnd = pGlue->GetFrameWnd();
        if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) {
            pFrame = (CNSGenFrame *)pBaseWnd;
        }
	}
	else {
		pFrame = pParentFrame;
	}
	CNSNavFrame* pDockedFrame = pFrame->GetDockedNavCenter();
	pFrame->GetClientRect(&layout.rect);    // starting rect comes from client rect
	CWnd *pView = pFrame->GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
	HWND hWnd = pView->GetSafeHwnd();
	layout.hDWP = NULL; // not actually doing layout

	for (HWND hWndChild = ::GetTopWindow(pFrame->GetSafeHwnd()); hWndChild != NULL;
		hWndChild = ::GetNextWindow(hWndChild, GW_HWNDNEXT))
	{
		if (hWndChild != hWnd && hWndChild != GetSafeHwnd() && hWndChild != pDockedFrame->GetSafeHwnd())
			::SendMessage(hWndChild, WM_SIZEPARENT, 0, (LPARAM)&layout);
	}
	lpRectClient->left = layout.rect.left;
	lpRectClient->right = layout.rect.right;
	lpRectClient->top = layout.rect.top;
	lpRectClient->bottom = layout.rect.bottom;
}


//------------------------------------------------------------------------------
// void CNSNavFrame::ForceFloat()
// Force this frame to be a floated frame.
//------------------------------------------------------------------------------
void CNSNavFrame::ForceFloat(BOOL show)
{
	// Find out what our parent frame is before we go futzing around with styles.
	CFrameWnd *pLayout = GetParentFrame();
	
	// Notify HT of our new state. Reset to the popup state.
	HT_SetTreeStateForButton(HT_TopNode(HT_GetSelectedView(GetHTPane())), HT_POPUP_WINDOW);
	HT_SetWindowType(GetHTPane(), HT_STANDALONE_WINDOW);

	nsModifyStyle( GetSafeHwnd(), GWL_STYLE, WS_POPUP, 0 );
	nsModifyStyle( GetSafeHwnd(), GWL_STYLE, WS_CHILD, WS_OVERLAPPEDWINDOW | WS_CAPTION);
	nsModifyStyle( GetSafeHwnd(), GWL_EXSTYLE, 0, WS_EX_CLIENTEDGE);
	
	if (show)
		SetWindowPos(&wndNoTopMost, m_rectDrag.left, m_rectDrag.top, m_rectDrag.Width(),
					m_rectDrag.Height(), SWP_FRAMECHANGED);
	else SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_FRAMECHANGED
						| SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE | SWP_HIDEWINDOW);

	SetParent(NULL);

	if (m_DragWnd)
	{
		m_DragWnd->DestroyWindow();
		m_DragWnd = NULL;
	}

	m_dwOverDockStyle = DOCKSTYLE_FLOATING;
	
	if (show)
		ShowWindow(SW_SHOW);
	else ShowWindow(SW_HIDE);

	//  Tell XP NavCenter layer that we are no longer docked.
    XP_UndockNavCenter(GetHTPane());

	// Tell ParentFrame that we are not docked anymore.
	if (pLayout)
		pLayout->RecalcLayout();

}

void CNSNavFrame::ComputeDockingSizes()
{
    m_DockWidth = m_DockSize + 1;  // Not sure what the error is here yet.
	m_DockHeight = m_DockSize + 1;
}


void CNSNavFrame::DockFrame(CNSGenFrame* pParent, short dockStyle)
{
		CWnd *pView = pParent->GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
		CalcClientArea(&m_parentRect, pParent);
		ComputeDockingSizes();

		if (GetHTPane() == NULL || XP_IsNavCenterDocked(GetHTPane()) == FALSE) 
		{ 
			CNSNavFrame* pFrame = pParent->GetDockedNavCenter();
			if (pFrame)	
			{ // if the parent frame has a docked frame, then destroy it.
				pFrame->DeleteNavCenter();
				pFrame->UnhookFromButton();
			}
			nsModifyStyle( GetSafeHwnd(), GWL_STYLE, WS_OVERLAPPEDWINDOW, WS_CHILD);
			nsModifyStyle( GetSafeHwnd(), GWL_EXSTYLE, WS_EX_CLIENTEDGE, 0);
			nsModifyStyle( GetSafeHwnd(), GWL_STYLE, WS_POPUP, 0);

			SetParent(pParent);

			//  Tell XP layer that we are now docked.
			if(GetTopLevelFrame()) 
			{
				CFrameGlue *pGlue = CFrameGlue::GetFrameGlue(GetTopLevelFrame());
				if(pGlue && pGlue->GetMainContext()) {
					XP_DockNavCenter(GetHTPane(), pGlue->GetMainContext()->GetContext());
			}
		}
		
		// Notify HT of our new state.
		HT_SetTreeStateForButton(HT_TopNode(HT_GetSelectedView(GetHTPane())), HT_DOCKED_WINDOW);
		HT_SetWindowType(GetHTPane(), HT_DOCKED_WINDOW);
		
		// If are the popup button, then do the adjustment.
		MovePopupToDocked();

		CRect rect = m_dockingDragRect;

		// Hide the window before we move it.
		m_dwOverDockStyle = dockStyle;
		if (m_dwOverDockStyle == DOCKSTYLE_DOCKEDLEFT) 
		{
			GetDesktopWindow()->MapWindowPoints( pView, &rect ); 
			
			// snap to right, when the drag rect is closer to the right edge.
			if (rect.right > ((m_parentRect.right - m_parentRect.left) / 2)) 
			{
				rect.right = m_parentRect.right;
				rect.left = rect.right - m_DockWidth;
			}
			else { // otherwise snap to the left.
				rect.left = m_parentRect.left;
				rect.right = rect.left + m_DockWidth;
			}
			rect.top = m_parentRect.top;
			rect.bottom = m_parentRect.bottom;
		}
		else 
		{
			GetDesktopWindow()->MapWindowPoints( pView, &rect ); 
			// snap to bottom, when the drag rect is closer to the bottom edge.
			if (rect.bottom > ((m_parentRect.bottom - m_parentRect.top) / 2)) 
			{
				rect.bottom = m_parentRect.bottom;
				rect.top = rect.bottom - m_DockHeight;
			}
			else { // otherwise snap to the left.
				rect.top = m_parentRect.top;
				rect.bottom = rect.top + m_DockHeight;
			}
			rect.left = m_parentRect.left;
			rect.right = m_parentRect.right;
		}

		// Figure out the correct location to display the resize bar.
		CRect dragBarRect(rect);
		if (m_dwOverDockStyle == DOCKSTYLE_DOCKEDLEFT) 
		{
			dragBarRect.left = rect.right;
			dragBarRect.right = dragBarRect.left+ DRAGWIDTH;
		}
		
		if (!m_DragWnd) 
		{
			m_DragWnd = new CDragBar(dragBarRect, this);
			m_DragWnd->CreateEx(0, NULL, "GridEdge", WS_CHILD | WS_VISIBLE, 
					 dragBarRect.left, dragBarRect.top, 
					 dragBarRect.right - dragBarRect.left,
					 dragBarRect.bottom - dragBarRect.top, 
					 GetParentFrame()->GetSafeHwnd(), (HMENU)NC_IDW_DRAGEDGE, NULL);
		}
		// If we are not yet docked
		m_DragWnd->SetOrientation(m_dwOverDockStyle);
		m_DragWnd->SetParentFrame(GetParentFrame());
		m_DragWnd->SetRect(dragBarRect); 
		m_DragWnd->SetParent(GetParentFrame());

		// Now move the window to the correct location.
		ShowWindow(SW_HIDE);
		SetWindowPos( &wndBottom, rect.left, rect.top, rect.right - rect.left, 
								rect.bottom - rect.top,0 );
		
		ShowWindow(SW_SHOW);
    }
    
    //  Have our frame parent recalc.
    GetParentFrame()->RecalcLayout();
}
//------------------------------------------------------------------------------
//	void CNSNavFrame::EndDrag(CPoint pt)             // drop
//  
//------------------------------------------------------------------------------
void CNSNavFrame::EndDrag(CPoint pt)             // drop
{
	short dwOverDockStyle = CanDock(pt);
	CancelLoop(dwOverDockStyle);
	CRect rect;
	CRect parentRect;

	if (dwOverDockStyle == DOCKSTYLE_DOCKEDLEFT)
	{
        CNSGenFrame* pFrame = NULL;
        CFrameGlue *pGlue = CFrameGlue::GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERANDEDITOR);
        CFrameWnd *pBaseWnd = pGlue->GetFrameWnd();
        if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) 
		{
            pFrame = (CNSGenFrame *)pBaseWnd;
        }
        DockFrame(pFrame, dwOverDockStyle);
	//	m_nsContent->CalcChildSizes();
	}
	else 
	{
		// Float this frame.
		MoveWindow( m_rectDrag);
		
		// If it had a button associated with it, now it doesn't.
		UnhookFromButton();
	}

	ShowWindow(SW_SHOW);
}


void CNSNavFrame::InitLoop()
{
	// handle pending WM_PAINT messages
	MSG msg;
	while (::PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_NOREMOVE))
	{
		if (!GetMessage(&msg, NULL, WM_PAINT, WM_PAINT))
			return;
		DispatchMessage(&msg);
	}


	// initialize state
	m_rectLast.SetRectEmpty();
	m_sizeLast.cx = m_sizeLast.cy = 0;
	m_bDitherLast = FALSE;
	CWnd* pWnd = CWnd::GetDesktopWindow();
	if (pWnd->LockWindowUpdate())
		m_pDC = pWnd->GetDCEx(NULL, DCX_WINDOW|DCX_CACHE|DCX_LOCKWINDOWUPDATE);
	else
		m_pDC = pWnd->GetDCEx(NULL, DCX_WINDOW|DCX_CACHE);
	ASSERT(m_pDC != NULL);
}


void CNSNavFrame::CancelLoop(short dwDockStyle)
{
	DrawFocusRect(dwDockStyle, TRUE);    // gets rid of focus rect
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


//---------------------------------------------------------------------------------
//  void CNSNavFrame::DrawFocusRect(BOOL bRemoveRect)
//
//  Draw ghost selector rect.
//---------------------------------------------------------------------------------
void CNSNavFrame::DrawFocusRect(short dwOverDockStyle, BOOL bRemoveRect)
{
	ASSERT(m_pDC != NULL);

	// default to thin frame
	CSize size(CX_BORDER, CY_BORDER);

	// determine new rect and size
	CRect rect;
	CBrush* pWhiteBrush = CBrush::FromHandle((HBRUSH)::GetStockObject(WHITE_BRUSH));
	CBrush* pBrush = pWhiteBrush;
	if (dwOverDockStyle == DOCKSTYLE_DOCKEDLEFT)
	{
		rect = m_dockingDragRect;
	}
	else 
	{
		rect = m_rectDrag;
	}
	if (bRemoveRect)
		size.cx = size.cy = 0;

	CBrush* pDitherBrush = CDC::GetHalftoneBrush();

	// draw it and remember last size
	WFE_DrawDragRect(m_pDC, &rect, size, &m_rectLast, m_sizeLast,
		pBrush, m_bDitherLast ? pDitherBrush : pWhiteBrush);
	m_bDitherLast = (pBrush == pDitherBrush);
	m_rectLast = rect;
	m_sizeLast = size;
}


//---------------------------------------------------------------------------------
//  short CNSNavFrame::CanDock(CPoint pt)
//  Check whether we can dock to this frame.  If we can, return the dock orientation.
//---------------------------------------------------------------------------------
short CNSNavFrame::CanDock(CPoint pt, BOOL mapDesktop)
{
	short dockStyle = DOCKSTYLE_FLOATING;

    CNSGenFrame* pFrame = NULL;
    CFrameGlue *pGlue = CFrameGlue::GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERANDEDITOR);
    CFrameWnd *pBaseWnd = pGlue->GetFrameWnd();
    if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) {
        pFrame = (CNSGenFrame *)pBaseWnd;
    }

	if (mapDesktop) {

		CWnd *pView = pFrame->GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
		if (XP_IsNavCenterDocked(GetHTPane()))	{
			MapWindowPoints( pView, &pt, 1 );
			pView->MapWindowPoints( GetDesktopWindow(), &pt, 1 );
		}
		else
			MapWindowPoints( GetDesktopWindow(), &pt, 1 );
	}

	// If the top most activated frame is not a Navigator window, do not dock.
	if (pFrame->AllowDocking()) { 
		if (m_parentRect.PtInRect(pt)) {
			if ((pt.x < (m_parentRect.left +m_DockWidth)) &&
				pt.x > m_parentRect.left)
				dockStyle = DOCKSTYLE_DOCKEDLEFT;
		}
	}
	else {
		dockStyle = DOCKSTYLE_FLOATING;
	}
	
	return dockStyle;
}

//------------------------------------------------------------------------------
// Track mouse movement after the LMouseDown.
//------------------------------------------------------------------------------

BOOL CNSNavFrame::Track()
{
	// don't handle if capture already set
	if (::GetCapture() != NULL)
		return FALSE;

	// set capture to the window which received this message
	SetCapture();

	// get messages until capture is lost or cancelled/accepted
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
				EndDrag(msg.pt);
			return TRUE;
		case WM_MOUSEMOVE:
			if (m_bDragging)
				Move(msg.pt);
			break;
		// just dispatch rest of the messages
		default:
			DispatchMessage(&msg);
			break;
		}
	}

	return FALSE;
}


//------------------------------------------------------------------------------
// LRESULT CNSNavFrame::OnSizeParent(WPARAM, LPARAM lParam)
//
// This function will be called when the parent window calls recalcLayout().
// It will subtract this window's client rect from lpLayout.  Parent uses this 
// function to layout other window in the left over lpLayout.
//
//------------------------------------------------------------------------------

LRESULT CNSNavFrame::OnSizeParent(WPARAM, LPARAM lParam)
{
	AFX_SIZEPARENTPARAMS* lpLayout = (AFX_SIZEPARENTPARAMS*)lParam;

	// dragRect is in the parent window's coordinates(which is the main frame).
	// the top and bottom coordinates of the dragRect should already be included
	// in the toolbar.

	if (!m_DragWnd) return 0;

	CRect oldRect;
	GetWindowRect(&oldRect);
	CRect resizeRect(lpLayout->rect);
	CRect oldDragRect;
	m_DragWnd->GetRect(oldDragRect);

	CRect dragRect(resizeRect);
	
	if (m_dwOverDockStyle == DOCKSTYLE_DOCKEDLEFT) 
	{
		resizeRect.right = lpLayout->rect.left + oldRect.Width();
		if (oldDragRect.left != resizeRect.right) // we move the drag bar.  Need to resize here.
			resizeRect.right += oldDragRect.left - resizeRect.right;

		CWnd* pParent = GetParent();
		CRect parentRect;
		pParent->GetClientRect(&parentRect);

		if (parentRect.Width() < resizeRect.Width() + DRAGWIDTH)
		{
			resizeRect.right = resizeRect.left + parentRect.Width() - DRAGWIDTH;
		}
		
		dragRect.left = resizeRect.right;
		dragRect.right = dragRect.left + DRAGWIDTH;

		lpLayout->rect.left = resizeRect.right + DRAGWIDTH;
		
		m_DragWnd->SetRect(dragRect);
		m_DockWidth = resizeRect.Width();
	}
	
	if (lpLayout->hDWP != NULL) {
		// reposition our frame window.
		SetWindowPos( &wndBottom, resizeRect.left, resizeRect.top, resizeRect.right - resizeRect.left, 
								resizeRect.bottom - resizeRect.top, SWP_SHOWWINDOW );
		m_dockingDragRect = resizeRect;

		m_DragWnd->SetWindowPos( &wndBottom, dragRect.left, dragRect.top, dragRect.Width(), 
									 dragRect.Height(), SWP_SHOWWINDOW );

	}

	return 0;
}

static void FillSolidRect(HDC hdc, RECT* rect, COLORREF clr)
{
	::SetBkColor(hdc, clr);
	::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, rect, NULL, 0, NULL);
}




/////////////////////////////////////////////////////////////////////////////
// CNSNavFrame diagnostics

#ifdef _DEBUG
void CNSNavFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CNSNavFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

HT_Pane CNSNavFrame::GetHTPane()
{
    if(m_Node)
		return HT_GetPane(HT_GetView(m_Node));
    return NULL;
}

void CNSNavFrame::OnNcLButtonDown( UINT nHitTest, CPoint point )
{
	if (nHitTest == HTCAPTION)
	{
		m_ptLast = point;
		SetCapture();
	}

	CWnd::OnNcLButtonDown(nHitTest, point);
}

void CNSNavFrame::OnLButtonUp( UINT nHitTest, CPoint point )
{
	if (GetCapture() == this) 
	{
		ReleaseCapture();
	}

	CWnd::OnNcLButtonUp(nHitTest, point);
}

void CNSNavFrame::OnMouseMove( UINT nHitTest, CPoint point )
{
	if (GetCapture() == this)
	{
		MapWindowPoints(NULL, &point, 1); 
		if (abs(point.x - m_ptLast.x) > 3 ||
			abs(point.y - m_ptLast.y) > 3)
		{
			ReleaseCapture();
			StartDrag(point, FALSE);
		}
	}

	CWnd::OnNcMouseMove(nHitTest, point);
}

