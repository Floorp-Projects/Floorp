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

#ifndef NAVCONTV_H
#define NAVCONTV_H
#include "htrdf.h"
#define WM_NAVCENTER_BASE 999
#define WM_NAVCENTER_QUERYPOSITION (WM_USER + WM_NAVCENTER_BASE + 1) /* return zero if you process */
struct NAVCENTPOS   {
    int m_iYDisposition;    /* INT_MIN is top, 0 middle, INT_MAX bottom
                             * You must provide a unique value, so do some grepping
                             */
    int m_iYVector;         /* This number is how large the child window would
                             *  like to be in the Y direction.
                             * It is relative to all other child windows.
                             * You must provide this value, or the window will be
                             *  disproportionate, do some grepping to figure the
                             *  scale it should be on.
                             */
    HWND m_hChild;
};

class CContentView : public CView
{
	DECLARE_DYNAMIC(CContentView)
	CContentView();
	virtual ~CContentView();
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
private:
    XP_List *m_pChildSizeInfo;
public:
    void CalcChildSizes();
    void AddChildSizeInfo(NAVCENTPOS *pPreference);

protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg void CContentView::OnActivateView( BOOL bActivate, CView* pActivateView, CView* pDeactiveView );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnSetFocus(CWnd* pWnd);
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg LRESULT OnNavCenterQueryPosition(WPARAM wParam, LPARAM lParam);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
#endif
