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

// gridedge.cpp : implementation file
//

#include "stdafx.h"
#include "gridedge.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGridEdge

// Returns the rectangle, in pixels, of where to position the edge,
// The coordinates are relative to the owning context's view
static void
GetGridEdgeRect(LO_EdgeStruct* pEdge, CWinCX* pOwnerCX, CRect &rect)
{
	int32 lAlign = pEdge->x + pEdge->x_offset;
	int32 lLeft = pOwnerCX->Twips2PixX(lAlign);

	lAlign = pEdge->y + pEdge->y_offset;
	int32 lTop = pOwnerCX->Twips2PixY(lAlign);

	lAlign = pEdge->x + pEdge->x_offset + pEdge->width;
	int32 lRight = pOwnerCX->Twips2PixX(lAlign);

	lAlign = pEdge->y + pEdge->y_offset + pEdge->height;
	int32 lBottom = pOwnerCX->Twips2PixY(lAlign);
	
	rect.left = CASTINT(lLeft);
	rect.top = CASTINT(lTop);
	rect.right = CASTINT(lRight);
	rect.bottom = CASTINT(lBottom);
}

CGridEdge::CGridEdge(LO_EdgeStruct *pEdge, CWinCX *pOwnerCX)
{
	//	Set our members.
	m_pEdge = pEdge;
	m_pCX = pOwnerCX;

	//	Not currently tracking.
	m_bTracking = FALSE;

    m_bParentClipChildren = FALSE;
    
	//	We need to create a window for ourselves.
	CRect crRect;

	GetGridEdgeRect(m_pEdge, m_pCX, crRect);

    DWORD dwEdgeStyle = WS_CHILD | WS_CLIPSIBLINGS;
    if(pEdge->visible)  {
        //  Make it visible.
        dwEdgeStyle |= WS_VISIBLE;
    }

	CreateEx(0, NULL, "GridEdge", dwEdgeStyle, 
			 crRect.left, crRect.top, 
			 crRect.right - crRect.left,
			 crRect.bottom - crRect.top, 
			 m_pCX->GetPane(), (HMENU) 0, NULL);
}

CGridEdge::~CGridEdge()
{
	//	If we were tracking, then let our clipped region go, etc.
	if(m_bTracking == TRUE)	{
		//	Release capture.
		ReleaseCapture();

		//	Release the bounds established on the mouse.
		::ClipCursor(NULL);

		//	Invert the tracker (return screen to normal state).
		InvertTracker();
	}

	//	Destroy our window.
	DestroyWindow();

	//	Clear our members.
	m_pEdge = NULL;
	m_pCX = NULL;
}


BEGIN_MESSAGE_MAP(CGridEdge, CWnd)
	//{{AFX_MSG_MAP(CGridEdge)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_PAINT()
	ON_WM_LBUTTONDBLCLK()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CGridEdge message handlers

void CGridEdge::UpdateEdge(LO_EdgeStruct *pEdge)	{
	//	Update what our edge is.
	m_pEdge = pEdge;

    //  See if our visibility is a concern.
    if(m_pEdge->visible && IsWindowVisible() == FALSE)  {
        ShowWindow(SW_SHOW);
    }
    else if(!m_pEdge->visible && IsWindowVisible()) {
        ShowWindow(SW_HIDE);
    }

	// Position the window properly
	CRect crRect;

	GetGridEdgeRect(m_pEdge, m_pCX, crRect);
	MoveWindow(crRect);
}

void CGridEdge::InvertTracker()	{
	// Get our frame context's DC (need to draw over children). The tracker
	// coordinates are already in the frame window's coordinate system
	if(m_pCX->GetFrame()->GetFrameWnd() != NULL)	{
		CDC *pDC = m_pCX->GetFrame()->GetFrameWnd()->GetDC();

		//	Invert a brush pattern.
#ifdef XP_WIN16
		pDC->PatBlt(m_crTracker.left, m_crTracker.top, m_crTracker.Width(),
			m_crTracker.Height(), DSTINVERT);
#else
		CBrush *pBrush = CDC::GetHalftoneBrush();
		HBRUSH hOldBrush = NULL;
		if(pBrush != NULL)	{
			hOldBrush = (HBRUSH)SelectObject(pDC->m_hDC, pBrush->m_hObject);
		}

		pDC->PatBlt(m_crTracker.left, m_crTracker.top,
			m_crTracker.Width(), m_crTracker.Height(), PATINVERT);

		if(hOldBrush != NULL)	{
			SelectObject(pDC->m_hDC, hOldBrush);
		}
#endif

		m_pCX->GetFrame()->GetFrameWnd()->ReleaseDC(pDC);
	}
}

void CGridEdge::OnLButtonDown(UINT nFlags, CPoint point) 
{
	//	Only do something if we can resize.
	if(CanResize() == FALSE)	{
		return;
	}

	//	Capture mouse events.
	SetCapture();

	// Bound the mouse to a rectangle that is acceptable for use to perform a resize.
	// Layout knows how big this should be, and it's stored in left_top_bound and
	// right_bottom_bound (how these are interpreted depends on whether it's a vertical
	// or horizontal edge)
	CRect crRect;

	// Start with the grid edge rect, and then use the bounds that layout has
	GetGridEdgeRect(m_pEdge, m_pCX, crRect);

	if(m_pEdge->is_vertical) {
		crRect.left = CASTINT(m_pCX->Twips2PixX(m_pEdge->left_top_bound));
		crRect.right = CASTINT(m_pCX->Twips2PixX(m_pEdge->right_bottom_bound));

	} else {
		crRect.top = CASTINT(m_pCX->Twips2PixY(m_pEdge->left_top_bound));
		crRect.bottom = CASTINT(m_pCX->Twips2PixY(m_pEdge->right_bottom_bound));
	}

    ::ClientToScreen(m_pCX->GetPane(), &crRect.TopLeft());
    ::ClientToScreen(m_pCX->GetPane(), &crRect.BottomRight());

	TRACE("Clipping cursor to (%d,%d)(%d,%d)\n", crRect.left, crRect.top,
		crRect.right, crRect.bottom);
	::ClipCursor(crRect);

	//	Mark the point at which the mouse went down.
	//	This can be used to calculate the relative distance of the movement of the tracker.
	m_cpLBDown = point;

	//	Set tracking state.
	m_bTracking = TRUE;

	// Update our tracker with our current coordinates. Tracker coordinates must be in the
	// frame window's coordinate system
	GetClientRect(m_crTracker);
	MapWindowPoints(m_pCX->GetFrame()->GetFrameWnd(), m_crTracker);

	// Save the current tracker bounds for later
	m_crOriginalTracker = m_crTracker;

    // Turn off clipping of children on the parent window before we 
    // start dragging. This is so we can draw the tracker directly on
    // the parent window (and probably over its child windows).
    CWnd *pWnd = m_pCX->GetFrame()->GetFrameWnd();
	if(pWnd)	{
        DWORD dwStyle = ::GetWindowLong(pWnd->m_hWnd, GWL_STYLE);
        m_bParentClipChildren = ((dwStyle & WS_CLIPCHILDREN) != 0);
        ::SetWindowLong(pWnd->m_hWnd, GWL_STYLE, (dwStyle & ~WS_CLIPCHILDREN));
    }

	//	Invert the tracker.
	InvertTracker();
}

void CGridEdge::OnLButtonUp(UINT nFlags, CPoint point) 
{
	//	Only do something if we can resize.
	if(CanResize() == FALSE)	{
		return;
	}

	//	Release capture.
	ReleaseCapture();

	//	Release the bounds established on the mouse.
	::ClipCursor(NULL);

	//	Invert the tracker (return screen to normal state).
	InvertTracker();

    // Turn clipping children back on if our parent had it.
    CWnd *pWnd = m_pCX->GetFrame()->GetFrameWnd();
	if(pWnd && m_bParentClipChildren)	{
        DWORD dwStyle = ::GetWindowLong(pWnd->m_hWnd, GWL_STYLE);
        ::SetWindowLong(pWnd->m_hWnd, GWL_STYLE, (dwStyle|WS_CLIPCHILDREN));
    }

	//	Set that we are no longer tracking.
	m_bTracking = FALSE;

	// Convert tracker from the frame window's coordinate system to the
	// owning context view's coordinate system
	CPoint cpPoint;
	m_pCX->GetViewOffsetInFrame(cpPoint);
	m_crTracker.left -= cpPoint.x;
	m_crTracker.top -= cpPoint.y;

	//	Report to layout the new position of the tracker.
	//	Make sure we didn't pass any limits.
	if(m_pEdge->is_vertical)	{
		//	Check the left argument.
		if(m_crTracker.left > m_pEdge->right_bottom_bound)	{
			m_crTracker.left = CASTINT(m_pEdge->right_bottom_bound);
		}
		if(m_crTracker.left < m_pEdge->left_top_bound)	{
			m_crTracker.left = CASTINT(m_pEdge->left_top_bound);
		}
	}
	else	{
		//	Check the top argument.
		if(m_crTracker.top > m_pEdge->right_bottom_bound)	{
			m_crTracker.top = CASTINT(m_pEdge->right_bottom_bound);
		}
		if(m_crTracker.top < m_pEdge->left_top_bound)	{
			m_crTracker.top = CASTINT(m_pEdge->left_top_bound);
		}
	}

	//	Restore our tracker to its original value, in case layout does nothing.
	CRect crTemp = m_crTracker;
	m_crTracker = m_crOriginalTracker;

	LO_MoveGridEdge(m_pCX->GetContext(), m_pEdge, crTemp.left, crTemp.top);
}

void CGridEdge::OnMouseMove(UINT nFlags, CPoint point) 
{
	//	Only do this if we can resize.
	if(CanResize() == FALSE)	{
		return;
	}

	//	If we're currently tracking, simply update the coords.
	if(m_bTracking == TRUE)	{
		//	Turn off the tracker.
		InvertTracker();

		//	Figure out which direction we're allowing this to be updated.
		if(m_pEdge->is_vertical)	{
			//	Can go left to right.
			m_crTracker.left = CASTINT(m_crOriginalTracker.left + m_pCX->Pix2TwipsX(point.x - m_cpLBDown.x));
			m_crTracker.right = m_crTracker.left + m_crOriginalTracker.Width();
		}
		else	{
			//	Can go top to bottom.
			m_crTracker.top = CASTINT(m_crOriginalTracker.top + m_pCX->Pix2TwipsY(point.y - m_cpLBDown.y));
			m_crTracker.bottom = m_crTracker.top + m_crOriginalTracker.Height();
		}

		//	Turn on the tracker.
		InvertTracker();
		return;
	}

	static HCURSOR hcurLast = NULL;
	static HCURSOR hcurDestroy = NULL;
	static UINT uLastCursorID = 0;
	UINT uCursorID;

	//	Decide which cursor we want to load.
	if(m_pEdge->is_vertical == FALSE)	{
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

BOOL CGridEdge::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if (nHitTest == HTCLIENT && pWnd == this && !m_bTracking && CanResize())	{
		return TRUE;    // we will handle it in the mouse move
	}
	
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CGridEdge::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	//	The area we're drawing.
	CRect crRect;
	GetClientRect(crRect);

	//	Draw it as the button face if no other color available.
    if(m_pEdge && m_pEdge->bg_color)    {
        WFE_FillSolidRect((CDC *)&dc, crRect,
            RGB(m_pEdge->bg_color->red, m_pEdge->bg_color->green, m_pEdge->bg_color->blue));
    }
    else    {
	    WFE_FillSolidRect((CDC *)&dc, crRect, sysInfo.m_clrBtnFace);
    }

	// Draw the border edges if appropriate. We can't really do much unless the edge
	// is at least 3 pixels wide/high
	if (m_pEdge->visible) {
		if (m_pEdge->is_vertical) {
			if (m_pEdge->width >= 3) {
				int	nLeft = m_pEdge->width >= 6 ? crRect.left + 1 : crRect.left;

				dc.PatBlt(nLeft, crRect.top, 1, crRect.Height(), WHITENESS);
				HBRUSH	hOldBrush = (HBRUSH)::SelectObject(dc.m_hDC, ::GetStockObject(GRAY_BRUSH));
				dc.PatBlt(crRect.right - 2, crRect.top, 1, crRect.Height(), PATCOPY);
				::SelectObject(dc.m_hDC, hOldBrush);
				dc.PatBlt(crRect.right - 1, crRect.top, 1, crRect.Height(), BLACKNESS);
			}
	
		} else {
			if (m_pEdge->height >= 3) {
				int	nTop = m_pEdge->height >= 6 ? crRect.top + 1 : crRect.top;

				dc.PatBlt(crRect.left, nTop, crRect.Width(), 1, WHITENESS);
				HBRUSH	hOldBrush = (HBRUSH)::SelectObject(dc.m_hDC, ::GetStockObject(GRAY_BRUSH));
				dc.PatBlt(crRect.left, crRect.bottom - 2, crRect.Width(), 1, PATCOPY);
				::SelectObject(dc.m_hDC, hOldBrush);
				dc.PatBlt(crRect.left, crRect.bottom - 1, crRect.Width(), 1, BLACKNESS);
			}
		}
	}
}

void CGridEdge::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	//	Treat same as button down.
	OnLButtonDown(nFlags, point);
}
