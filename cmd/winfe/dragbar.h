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

#ifndef __DragEdges_H
#define __DragEdges_H

#define DOCKSTYLE_VERTLEFT		1
#define DOCKSTYLE_VERTRIGHT		2
#define DOCKSTYLE_HORZTOP		3
#define DOCKSTYLE_HORZBOTTOM	4

class CNSNavFrame;
class CDragBar : public CWnd
{
// Construction
public:
	CDragBar(CRect& crRect, CNSNavFrame* pDockFrame);

//	Wether or not we are currently tracking.
private:
	BOOL m_bDragging;
	CRect m_crTracker;
	CRect m_rectLast;
	CPoint m_cpLBDown;
	CPoint m_ptLast;
	BOOL m_bDitherLast;
	BOOL m_bInDeskCoord;
	CSize m_sizeLast;
	CDC* m_pDC;
    BOOL m_bParentClipChildren;
	void InvertTracker();
	CFrameWnd* m_parentView;
	CNSNavFrame* m_dockedFrame;
	int m_orientation;

// Operations
public:
	void SetRect(CRect& rect, BOOL inDeskCoord = FALSE); 
	void GetRect(CRect& rect); 
	void SetParentFrame(CFrameWnd* pFrame) { m_parentView = pFrame;}
	void SetOrientation(int orientation) {m_orientation = orientation;}
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGridEdge)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CDragBar();
	virtual void PostNcDestroy( );
	// Generated message map functions
protected:
	void InitLoop();
	void CancelLoop();
	BOOL Track();

	void CalcClientArea(RECT* lpRectClient);
	void DrawFocusRect(BOOL bRemoveRect = FALSE);
	void EndDrag();
	void Move( CPoint point );
	
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnDestroy();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
#endif // __DragEdges_H
