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

#ifndef __GridEdges_H
#define __GridEdges_H
// gridedge.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGridEdge window

class CGridEdge : public CWnd
{
// Construction
public:
	CGridEdge(LO_EdgeStruct *pEdge, CWinCX *pOwnerCX);

// Our context and grid edge.
private:
	CWinCX *m_pCX;
	LO_EdgeStruct *m_pEdge;

//	Whether or not we are currently tracking.
private:
	BOOL m_bTracking;
	CRect m_crTracker;
	CRect m_crOriginalTracker;
	CPoint m_cpLBDown;
        BOOL m_bParentClipChildren;
	void InvertTracker();

// Operations
public:
	void UpdateEdge(LO_EdgeStruct *pEdge);
	BOOL CanResize() const	{
		BOOL bRetval = FALSE;
		if(m_pEdge != NULL && m_pEdge->movable == TRUE)	{
			bRetval = TRUE;
		}
		return(bRetval);
	}

// Implementation
public:
	virtual ~CGridEdge();

	// Generated message map functions
protected:
	//{{AFX_MSG(CGridEdge)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
#endif // __GridEdges_H
