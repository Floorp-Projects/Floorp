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
	ON_WM_PAINT()
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
	m_pSelector = 0;
	m_nsContent= 0;
	m_bDragging = FALSE;
	m_DragWnd = 0;
    m_dockingState = unknown;
}

CNSNavFrame::~CNSNavFrame()
{
    if (m_dwOverDockStyle != DOCKSTYLE_FLOATING)
    {
        PREF_SetIntPref(gPrefDockPercentage, m_DockSize);
        PREF_SetIntPref(gPrefDockOrientation, m_dwOverDockStyle);
    }
    else
    {
        PREF_SetRectPref(gPrefFloatRect, (int16)m_rectFloat.left, (int16)m_rectFloat.top, (int16)m_rectFloat.right, (int16)m_rectFloat.bottom);
    }

	delete m_pNavMenu;
}

void CNSNavFrame::UpdateTitleBar(HT_View pView)
{
	CString title("Navigation Center [");
	title += HT_GetNodeName(HT_TopNode(pView));
	title += "]";

	SetWindowText(title);
}

void CNSNavFrame::OnClose( )
{
	CFrameWnd::OnClose();
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
// Read in our float rect pref.
	int16 left,right,top, bottom;
	PREF_GetRectPref(gPrefFloatRect,&left, &top, &right, &bottom);

// Create the window there.
	m_rectFloat.SetRect(left, top, right, bottom);
// Don't reset this if being undocked.
    if ( m_dockingState != beingUndocked )
    {
	    m_rectDrag = m_rectFloat;
    }

	CString title = "Navigation Center";
	Create( NULL, title, WS_OVERLAPPEDWINDOW, m_rectFloat, NULL);

// Hide initially.
	ShowWindow(SW_HIDE);
	
// Determine the pixels we should consume if docked
	int32 width;
	PREF_GetIntPref(gPrefDockPercentage, &width);
    if ((int)width > 400) width = 400;
	m_DockSize = (int)width;
	
// Read in our dockstyle
	PREF_GetIntPref(gPrefDockOrientation, &m_dwOverDockStyle);

// Find out if a view should be displayed
	
	if (pParentFrame != NULL)
	{	
		// create a docked window
		DockFrame(pParentFrame, m_dwOverDockStyle);
        m_dockingState = docked;
	}
	else 
	{
		// Create a floating window
		m_dwOverDockStyle = DOCKSTYLE_FLOATING;

        // Position to where user requested, if being undocked.
        if ( m_dockingState == beingUndocked )
        {
            MoveWindow( m_rectDrag );
        }
        m_dockingState = undocked;
	}

// Put the selector buttons into the pane.
	m_pSelector->PopulatePane();

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

	DestroyWindow();
}

BOOL CNSNavFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	m_pSelector = new CSelector();
	m_nsContent = new CContentView();
	m_pNavMenu = new CNavMenuBar();

	CRect  rect1;
	rect1.left = rect1.top = 0;
	rect1.right =  MIN_CATEGORY_WIDTH;
	rect1.bottom = 1;
	m_pSelector->Create( NULL, "", WS_CHILD | WS_VISIBLE, rect1, 
		this, NC_IDW_SELECTOR, pContext );

	rect1.right = rect1.bottom = 1;
	m_pNavMenu->Create(NULL, "", WS_CHILD | WS_VISIBLE, rect1, this, NC_IDW_NAVMENU, pContext);

	m_nsContent->Create( NULL, "", WS_CHILD | WS_VISIBLE, rect1, 
		this, NC_IDW_MISCVIEW, pContext );

	return TRUE;
}

void CNSNavFrame::OnSize( UINT nType, int cx, int cy )
{
	if(GetHTPane() && !XP_IsNavCenterDocked(GetHTPane())) 
	{
		// Make sure our float rect always matches our size when floating.
		GetWindowRect(&m_rectFloat);
	}
	else
	{
		// Recompute our docked percentage if the tree is visible
		CRect rect;
		GetClientRect(&rect);
		if (IsTreeVisible())
		{
			CalcClientArea(&m_parentRect);
			if (m_dwOverDockStyle == DOCKSTYLE_VERTLEFT || m_dwOverDockStyle == DOCKSTYLE_VERTRIGHT)
				m_DockWidth = m_DockSize = rect.Width();
			else m_DockHeight = m_DockSize = rect.Height();
		}
	}

	int top = 0;
	if (m_dwOverDockStyle == DOCKSTYLE_FLOATING || m_dwOverDockStyle == DOCKSTYLE_VERTLEFT ||
		m_dwOverDockStyle == DOCKSTYLE_VERTRIGHT) 
	{
		CRect tempRect;
		// We want to handle the redraw ourself to reduce the flickering.  
		m_pSelector->GetClientRect(&tempRect);
		m_pSelector->MapWindowPoints( this, &tempRect);
		m_pSelector->SetWindowPos( NULL, 0, top, MIN_CATEGORY_WIDTH, cy - STARTY - top, SWP_NOREPOSITION  | SWP_NOREDRAW);
		if ((tempRect.Height()) < cy - STARTY) 
		{
			int orgHeight = tempRect.Height();
			tempRect.top = tempRect.bottom;
			tempRect.bottom =  tempRect.top + (cy - orgHeight);
			MapWindowPoints(m_pSelector, &tempRect);
			m_pSelector->InvalidateRect( &tempRect, TRUE);
		}

		m_pNavMenu->ShowWindow(SW_SHOW);
		m_pNavMenu->SetWindowPos(NULL, MIN_CATEGORY_WIDTH+2, top, cx - MIN_CATEGORY_WIDTH, NAVBAR_HEIGHT, SWP_NOREPOSITION);
		m_nsContent->SetWindowPos( NULL, MIN_CATEGORY_WIDTH+2, top+NAVBAR_HEIGHT, cx - MIN_CATEGORY_WIDTH - 2, cy - STARTY - top - NAVBAR_HEIGHT, 0);
	}
	else 
	{  //dock horiz.
		CRect tempRect;
		m_pSelector->GetClientRect(&tempRect);
		m_pSelector->MapWindowPoints( this, &tempRect);
		m_pSelector->SetWindowPos( NULL, STARTX, top, cx - STARTX, MIN_CATEGORY_HEIGHT, SWP_NOREPOSITION  | SWP_NOREDRAW);
		if ((tempRect.Width()) < cx - STARTX) {
			int orgWidth = tempRect.Width();
			tempRect.right = tempRect.left;
			tempRect.right =  tempRect.left + (cx - orgWidth);
			MapWindowPoints(m_pSelector, &tempRect);
			m_pSelector->InvalidateRect( &tempRect, FALSE);
		}
		m_nsContent->SetWindowPos( NULL, STARTX, MIN_CATEGORY_HEIGHT + 2 ,
						cx - STARTX, cy - MIN_CATEGORY_HEIGHT, SWP_NOREPOSITION | SWP_NOREDRAW);
	}
	ShowWindow(SW_SHOW);
	if (m_DragWnd)
		m_DragWnd->ShowWindow(SW_SHOW);
}

BOOL CNSNavFrame::PreCreateWindow(CREATESTRUCT & cs)
{

	cs.lpszClass = AfxRegisterWndClass( CS_DBLCLKS,
					theApp.LoadStandardCursor(IDC_ARROW), 
					(HBRUSH)(COLOR_BTNFACE + 1));
    return (CFrameWnd::PreCreateWindow(cs));
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
	
	if (XP_IsNavCenterDocked(GetHTPane())) 
	{
		ForceFloat(FALSE);		
	}
	else ShowWindow(SW_HIDE);

	m_bDragging = TRUE;
	InitLoop();
	
	m_rectDrag = m_rectFloat;
  
	CNSGenFrame* pFrame = NULL;
    CFrameGlue *pGlue = CFrameGlue::GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERANDEDITOR);
    CFrameWnd *pBaseWnd = pGlue ? pGlue->GetFrameWnd() : NULL;
    if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) 
    {
       pFrame = (CNSGenFrame *)pBaseWnd;
       CalcClientArea(&m_parentRect, pFrame);
       pFrame->MapWindowPoints( NULL, &m_parentRect);
    }

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
		
	if (dwOverDockStyle == DOCKSTYLE_VERTLEFT ||
		dwOverDockStyle == DOCKSTYLE_VERTRIGHT)
	{
		int height = m_dockingDragRect.Height();
		m_dockingDragRect.right = m_dockingDragRect.left + m_DockWidth;
		int yOffset = m_nYOffset;
		if (yOffset > height)
			yOffset = height;
		m_dockingDragRect.top = pt.y - yOffset;
		m_dockingDragRect.bottom = m_dockingDragRect.top + height;
	}
	else if (dwOverDockStyle == DOCKSTYLE_HORZTOP ||
			 dwOverDockStyle == DOCKSTYLE_HORZBOTTOM) 
	{
		m_dockingDragRect.top = pt.y - (int)m_DockHeight;
		m_dockingDragRect.bottom = m_dockingDragRect.top + m_DockHeight;
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
	DWORD dwNewStyle;
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
	CFrameWnd *pLayout = GetParentFrame();
	
	nsModifyStyle( GetSafeHwnd(), GWL_STYLE, WS_CHILD, WS_OVERLAPPEDWINDOW);
		
	if (show)
	{	
		MoveWindow( m_rectDrag);
	}
	else ShowWindow(SW_HIDE);

	SetParent(NULL);
	m_DragWnd->DestroyWindow();
	m_DragWnd = NULL;

	m_dwOverDockStyle = DOCKSTYLE_FLOATING;
	
	if (show)
		ShowWindow(SW_SHOW);
	else ShowWindow(SW_HIDE);

    //  Tell XP NavCenter layer that we are no longer docked.
    XP_UndockNavCenter(GetHTPane());
    
    // Tell ParentFrame that we are not docked anymore.
    pLayout->RecalcLayout();

	// Select a new view if we were formerly collapsed.
	if (m_pSelector->GetCurrentButton() == NULL)
		m_pSelector->SelectNthView(0);

	if (show)
	{
		m_nsContent->ShowWindow(SW_SHOW);
		m_nsContent->CalcChildSizes();
	}
}

void CNSNavFrame::ComputeDockingSizes()
{
    if (IsTreeVisible())
	{
		m_DockWidth = m_DockSize + 1;  // Not sure what the error is here yet.
		m_DockHeight = m_DockSize + 1;
	}
	else
	{

		m_DockWidth = MIN_CATEGORY_WIDTH + DRAGWIDTH;
		m_DockHeight = MIN_CATEGORY_HEIGHT + DRAGWIDTH;
	}
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
			{ // if the parent frame has a docked frame, tell this frame to float.
				pFrame->DeleteNavCenter();
			}
			nsModifyStyle( GetSafeHwnd(), GWL_STYLE, WS_OVERLAPPEDWINDOW , WS_CHILD);
			SetParent(pParent);

			//  Tell XP layer that we are now docked.
			if(GetTopLevelFrame()) 
			{
				CFrameGlue *pGlue = CFrameGlue::GetFrameGlue(GetTopLevelFrame());
				if(pGlue && pGlue->GetMainContext()) {
					XP_DockNavCenter(GetHTPane(), pGlue->GetMainContext()->GetContext());
			}
		}
		
		CRect rect = m_dockingDragRect;

		// Hide the window before we move it.
		m_dwOverDockStyle = dockStyle;
		if (m_dwOverDockStyle == DOCKSTYLE_VERTLEFT || m_dwOverDockStyle == DOCKSTYLE_VERTRIGHT) 
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

		// Now move the window to the correct location.
		ShowWindow(SW_HIDE);
		SetWindowPos( &wndBottom, rect.left, rect.top, rect.right - rect.left, 
								rect.bottom - rect.top,0 );
		
		// Figure out the correct location to display the resize bar.
		CRect dragBarRect(rect);
		if (m_dwOverDockStyle == DOCKSTYLE_VERTLEFT) {
			dragBarRect.left = rect.right;
			dragBarRect.right = dragBarRect.left+ DRAGWIDTH;
		}
		else if (m_dwOverDockStyle == DOCKSTYLE_VERTRIGHT) {
			dragBarRect.right = rect.left;
			dragBarRect.left = dragBarRect.right - DRAGWIDTH;
		}
		else if (m_dwOverDockStyle == DOCKSTYLE_HORZTOP) {
			dragBarRect.top = rect.bottom;
			dragBarRect.bottom = dragBarRect.top + DRAGWIDTH;
		}
		else if (m_dwOverDockStyle == DOCKSTYLE_HORZBOTTOM) {
			dragBarRect.bottom = rect.top;
			dragBarRect.top = dragBarRect.bottom - DRAGWIDTH;
		}
		if (!m_DragWnd) {
			m_DragWnd = new CDragBar(dragBarRect, this);
#ifdef XP_WIN32
			m_DragWnd->CreateEx(0, NULL, "GridEdge", WS_CHILD | WS_VISIBLE, 
					 dragBarRect.left, dragBarRect.top, 
					 dragBarRect.right - dragBarRect.left,
					 dragBarRect.bottom - dragBarRect.top, 
					 GetParentFrame()->GetSafeHwnd(), (HMENU)NC_IDW_DRAGEDGE, NULL);
#else
			m_DragWnd->Create(0, "GridEdge", WS_CHILD | WS_VISIBLE, 
					 dragBarRect, 
					 GetParentFrame(), NC_IDW_DRAGEDGE);
#endif
		}
		// If we are not yet docked
		m_DragWnd->SetOrientation(m_dwOverDockStyle);
		m_DragWnd->SetParentFrame(GetParentFrame());
		m_DragWnd->SetRect(dragBarRect); 
		m_DragWnd->SetParent(GetParentFrame());

		m_DragWnd->ShowWindow(SW_SHOW);
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

	if (dwOverDockStyle == DOCKSTYLE_VERTLEFT || dwOverDockStyle == DOCKSTYLE_VERTRIGHT ||
		dwOverDockStyle == DOCKSTYLE_HORZTOP || dwOverDockStyle == DOCKSTYLE_HORZBOTTOM) 
	{
        CNSGenFrame* pFrame = NULL;
        CFrameGlue *pGlue = CFrameGlue::GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERANDEDITOR);
        CFrameWnd *pBaseWnd = pGlue->GetFrameWnd();
        if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) 
		{
            pFrame = (CNSGenFrame *)pBaseWnd;
        }

        // see if we're being re-docked.
        if ( m_dockingState == undocked )
        {
            // Indicate we need to be reborn docked.
            m_dockingState = beingDocked;

            // Destroy the old windows.
            DeleteNavCenter();
        } else {
            DockFrame(pFrame, dwOverDockStyle);
    		m_nsContent->CalcChildSizes();
    
        
            ShowWindow(SW_SHOW);
        }
	}
	else 
	{
		// Float this frame.
		MoveWindow( m_rectDrag);

        // See if we're being undocked.
        if ( m_dockingState == docked ) {
            // Indicate we need to be reborn undocked.
            m_dockingState = beingUndocked;

            // Destroy the old windows.
            DeleteNavCenter();
        } else {
            ShowWindow(SW_SHOW);
        }
	}
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
	if (dwOverDockStyle == DOCKSTYLE_HORZTOP || dwOverDockStyle == DOCKSTYLE_HORZBOTTOM)	{
		rect = m_dockingDragRect;
	}
	else if (dwOverDockStyle == DOCKSTYLE_VERTLEFT || dwOverDockStyle == DOCKSTYLE_VERTRIGHT) {
		rect = m_dockingDragRect;
	}
	else {
		rect = m_rectDrag;
	}
	if (bRemoveRect)
		size.cx = size.cy = 0;

#ifdef XP_WIN32
	CBrush* pDitherBrush = CDC::GetHalftoneBrush();
#else
	CBrush* pDitherBrush =  CBrush::FromHandle((HBRUSH)::GetStockObject(GRAY_BRUSH));
#endif

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
    CFrameWnd *pBaseWnd = pGlue ? pGlue->GetFrameWnd() : NULL;
    if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) {
        pFrame = (CNSGenFrame *)pBaseWnd;
    }

	if (pFrame && mapDesktop) {

		CWnd *pView = pFrame->GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
		if (XP_IsNavCenterDocked(GetHTPane()))	{
			MapWindowPoints( pView, &pt, 1 );
			pView->MapWindowPoints( GetDesktopWindow(), &pt, 1 );
		}
		else
			MapWindowPoints( GetDesktopWindow(), &pt, 1 );
	}

	// If the top most activated frame is not a Navigator window, do not dock.
	if (pFrame && pFrame->AllowDocking()) { 
		if (m_parentRect.PtInRect(pt)) {
			if ((pt.x < (m_parentRect.left +m_DockWidth)) &&
				pt.x > m_parentRect.left)
				dockStyle = DOCKSTYLE_VERTLEFT;
		//	else if	(( pt.x > (m_parentRect.right - m_DockWidth)) &&
		//		pt.x < m_parentRect.right)
		//		dockStyle = DOCKSTYLE_VERTRIGHT;
		//	else if ((pt.y < (m_parentRect.top +m_DockHeight)) &&
		//		 pt.y > m_parentRect.top)
		//	   dockStyle = DOCKSTYLE_HORZTOP;
		//	else if	(( pt.y > (m_parentRect.bottom - m_DockHeight)) &&
		//		 pt.y < m_parentRect.bottom)
		//	   dockStyle = DOCKSTYLE_HORZBOTTOM;
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

	CRect oldRect;
	GetWindowRect(&oldRect);
	CRect resizeRect(lpLayout->rect);
	CRect oldDragRect;
	m_DragWnd->GetRect(oldDragRect);

	CRect dragRect(resizeRect);

	if (m_dwOverDockStyle == DOCKSTYLE_VERTLEFT) {
		resizeRect.right = lpLayout->rect.left + oldRect.Width();
		if (oldDragRect.left != resizeRect.right) // we move the drag bar.  Need to resize here.
			resizeRect.right += oldDragRect.left - resizeRect.right;
		dragRect.left = resizeRect.right;
		dragRect.right = dragRect.left +  + DRAGWIDTH;
		lpLayout->rect.left = resizeRect.right + DRAGWIDTH;
		m_DragWnd->SetRect(dragRect);
		m_DockWidth = resizeRect.Width();
	}
	else if (m_dwOverDockStyle == DOCKSTYLE_VERTRIGHT) {
		resizeRect.left = lpLayout->rect.right - oldRect.Width();
		if (oldDragRect.right != resizeRect.left) // we moved the drag bar.  Need to resize here.
			resizeRect.left -= resizeRect.left - oldDragRect.right;
		dragRect.right = resizeRect.left;
		dragRect.left = dragRect.right - DRAGWIDTH;
		lpLayout->rect.right = resizeRect.left - DRAGWIDTH;
		m_DragWnd->SetRect(dragRect);
		m_DockWidth = resizeRect.Width();
	}
	else if (m_dwOverDockStyle == DOCKSTYLE_HORZTOP) {
		resizeRect.bottom = lpLayout->rect.top + oldRect.Height();
		if (oldDragRect.top != resizeRect.bottom) // we moved the drag bar.  Need to resize here.
			resizeRect.bottom += oldDragRect.top - resizeRect.bottom;
		dragRect.top = resizeRect.bottom;
		dragRect.bottom = dragRect.top + DRAGWIDTH;
		lpLayout->rect.top = resizeRect.bottom + DRAGWIDTH;
		m_DragWnd->SetRect(dragRect);
		m_DockHeight = resizeRect.Height();
	}
	else if (m_dwOverDockStyle == DOCKSTYLE_HORZBOTTOM) {
		resizeRect.top = lpLayout->rect.bottom - oldRect.Height();
		if (oldDragRect.bottom != resizeRect.top) // we moved the drag bar.  Need to resize here.
			resizeRect.top -= resizeRect.top - oldDragRect.bottom;
		dragRect.bottom = resizeRect.top;
		dragRect.top = dragRect.bottom - DRAGWIDTH;
		lpLayout->rect.bottom = resizeRect.top - DRAGWIDTH;
		m_DragWnd->SetRect(dragRect);
		m_DockHeight = resizeRect.Height();
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


//------------------------------------------------------------------------------
// void CNSNavFrame::OnPaint( )
//
//	This function is used to paint the background on CNSNavFrame
//------------------------------------------------------------------------------

void CNSNavFrame::OnPaint( )
{
	CRect rect;
	GetClientRect(&rect);
	CDC* pDC = GetDC();
	HDC hdc = pDC->GetSafeHdc();
	CRect rect1;
	CRgn pRgn;
	HBRUSH hBrush;

	HBRUSH hBr = ::CreateSolidBrush((COLORREF)GetSysColor(COLOR_3DSHADOW));
	HPEN hPen = ::CreatePen(PS_SOLID, 1,(COLORREF)GetSysColor(COLOR_3DLIGHT)); 

	HPEN hOldPen;
	hOldPen = (HPEN)::SelectObject(hdc, hPen);
	if (m_dwOverDockStyle == DOCKSTYLE_FLOATING || 
		m_dwOverDockStyle == DOCKSTYLE_VERTLEFT || 
		m_dwOverDockStyle == DOCKSTYLE_VERTRIGHT) {

		// paint the div line between selector pane and content pane.
		rect1 = rect;
		rect1.left = rect.left + MIN_CATEGORY_WIDTH;
		rect1.right = rect1.left + 2;
		::FillRect(hdc, &rect1, hBr);
		::SelectObject(pDC->GetSafeHdc(), (HPEN)::GetStockObject(WHITE_PEN));
		::MoveToEx(pDC->GetSafeHdc(), rect1.left+1, rect1.top, NULL);
		::LineTo(pDC->GetSafeHdc(), rect1.left+1, rect1.bottom);
	}
	else {

		// paint the div line between selector pane and content pane.
		rect1 = rect;
		rect1.top = rect.top + MIN_CATEGORY_HEIGHT;
		rect1.bottom = rect1.top + 2;
		::FillRect(hdc, &rect1, hBr);
		::SelectObject(pDC->GetSafeHdc(), (HPEN)::GetStockObject(WHITE_PEN));
		::MoveToEx(pDC->GetSafeHdc(), rect1.left, rect1.top+1, NULL);
		::LineTo(pDC->GetSafeHdc(), rect1.right, rect1.top+1);
	}
	::SelectObject(hdc, hOldPen);
	::DeleteObject(hPen);
	::DeleteObject(hBr);
	::DeleteObject(hBrush);
	ReleaseDC(pDC);
	CFrameWnd::OnPaint();
}

void CNSNavFrame::CollapseWindow()
{
	if (m_dwOverDockStyle == DOCKSTYLE_FLOATING) {
		// Handle floating window differently.  Floating window does not have
		// m_DragWnd and m_pParent.
	}
	else 
	{	
		// Move m_DragWnd when we want to collapse docking window.
		CRect tempRect;
		GetWindowRect(&tempRect);;
		CRect dragBox;
		m_DragWnd->GetRect(dragBox);
	
		if (m_dwOverDockStyle == DOCKSTYLE_HORZTOP)
		{
			dragBox.top -= (tempRect.Height() - MIN_CATEGORY_HEIGHT - DRAGWIDTH);
			dragBox.bottom = dragBox.top + DRAGWIDTH;
		}
		else if (m_dwOverDockStyle == DOCKSTYLE_HORZBOTTOM)
		{
			dragBox.top += (tempRect.Height() - MIN_CATEGORY_HEIGHT - DRAGWIDTH);
			dragBox.bottom = dragBox.top + DRAGWIDTH;
		}
		else if (m_dwOverDockStyle == DOCKSTYLE_VERTLEFT)
		{
			dragBox.left -= (tempRect.Width() - MIN_CATEGORY_WIDTH - DRAGWIDTH);
			dragBox.right = dragBox.left + DRAGWIDTH;
		}
		else 
		{
			// docked to the right
			dragBox.left += (tempRect.Width() - MIN_CATEGORY_WIDTH - DRAGWIDTH);
			dragBox.right += dragBox.left + DRAGWIDTH;
		}
			
		m_pSelector->UnSelectAll();

		if (GetParentFrame()) 
		{
			m_DragWnd->SetRect(dragBox);
			m_DragWnd->MoveWindow(dragBox);
			GetParentFrame()->RecalcLayout();
		}
	}
}


void CNSNavFrame::ExpandWindow()
{
	if (m_dwOverDockStyle == DOCKSTYLE_FLOATING) {
		// Handle floating window differently.  Floating window does not have
		// m_DragWnd and  m_pParent.
	}
	else {	// Move m_DragWnd when we want to expand docking window.
	
		ComputeDockingSizes();

		CRect tempRect;
		GetClientRect(&tempRect);;
		CRect dragBox;
		m_DragWnd->GetRect(dragBox);
		int step = 0;
		if (m_dwOverDockStyle == DOCKSTYLE_HORZTOP || m_dwOverDockStyle == DOCKSTYLE_HORZBOTTOM)
			step = m_DockHeight - MIN_CATEGORY_HEIGHT;
		else
			step = m_DockWidth - MIN_CATEGORY_WIDTH;

		if (m_dwOverDockStyle == DOCKSTYLE_HORZTOP) 
		{
			dragBox.top += step;
			dragBox.bottom = dragBox.top + DRAGWIDTH;
		}
		else if (m_dwOverDockStyle == DOCKSTYLE_HORZBOTTOM) {
			dragBox.bottom -=  step;
			dragBox.top = dragBox.bottom - DRAGWIDTH;
		}
		else  if (m_dwOverDockStyle == DOCKSTYLE_VERTLEFT) {
			dragBox.left += step;
			dragBox.right = dragBox.left + DRAGWIDTH;
		}
		else  if (m_dwOverDockStyle == DOCKSTYLE_VERTRIGHT) {
			dragBox.right -= step;
			dragBox.left = dragBox.right - DRAGWIDTH;
		}

		if (GetParentFrame()) {
			m_DragWnd->ShowWindow(SW_HIDE);
			m_DragWnd->SetRect(dragBox);
			m_DragWnd->MoveWindow(dragBox);
			GetParentFrame()->RecalcLayout();
		}
	}
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
    HT_Pane htRetval = NULL;
    if(m_pSelector) {
        htRetval = m_pSelector->GetHTPane();
    }
    return(htRetval);
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

void CNSNavFrame::PostNcDestroy() 
{
    // Check to see if we need to recreate the NavCenter windows.        
    switch ( m_dockingState ) {
        case beingDocked:
            // Re-create within docked-to frame.
            {
                CFrameGlue *pGlue = CFrameGlue::GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERANDEDITOR);
                CFrameWnd *pBaseWnd = pGlue ? pGlue->GetFrameWnd() : NULL;
                if(pBaseWnd && pBaseWnd->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) 
                {
                    CreateNewNavCenter( (CNSGenFrame*)pBaseWnd );
                }
                else
                {
                    CreateNewNavCenter(NULL);
                }
            }
            break;

        case beingUndocked:
            // Re-create with no parent.
            CreateNewNavCenter(NULL);
            break;

        case unknown:
        case docked:
        case undocked:
        default:
            // Pass on to base class (which will "delete this").
            CFrameWnd::PostNcDestroy();
            break;
    }
}
