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

#ifndef NAVMENUBAR_H
#define NAVMENUBAR_H

#include "htrdf.h"
#include "usertlbr.h"

#define NAVBAR_HEIGHT 23
#define NAVBAR_CLOSEBOX	15

class CSelectorButton;

/*  This class may yet be of use.  For now comment it out.
class CNavMenuButton : public CRDFToolbarButton
{
protected:
	HT_View m_HTView; // Pointer to HT_View if one exists.
	
public:
	CNavMenuButton();

	void UpdateView(HT_View v);
	void UpdateButtonText(CRect target);

	afx_msg LRESULT OnDragMenuOpen(WPARAM wParam, LPARAM lParam);
	afx_msg BOOL OnCommand(UINT wParam, LONG lParam);

	DECLARE_MESSAGE_MAP()
};
*/

class CNavMenuBar : public CWnd, public CCustomImageObject
{
	//CNavMenuButton* m_pMenuButton;	// Pointer to the button
	CSelectorButton* m_pSelectorButton; // Selector button
	BOOL m_bHasFocus;	// Determines what colors to use for the caption
	CPoint m_PointHit;	// MouseDown tracking
	CString titleText;	// Name of the current workspace

	COLORREF m_ForegroundColor;
	COLORREF m_BackgroundColor;
	CString m_BackgroundImageURL;
	CRDFImage* m_pBackgroundImage;
	HT_View m_View; // The current HT_View.

public:
	CNavMenuBar();
	~CNavMenuBar();

	void UpdateView(CSelectorButton* pButton, HT_View view); 
	void NotifyFocus(BOOL hasFocus) { m_bHasFocus = hasFocus; Invalidate(); }

	void LoadComplete(HT_Resource r) { Invalidate(); }

	//{{AFX_MSG(CNavMenuBar)
	
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDown (UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp (UINT nFlags, CPoint point );
	afx_msg void OnMouseMove (UINT nFlags, CPoint point );
    afx_msg void OnSize( UINT nType, int cx, int cy );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // NAVBAR_H

