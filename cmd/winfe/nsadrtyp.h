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

// NSAdrTyp.h : header file
//

#ifndef __NSADRTYP_H__
#define __NSADRTYP_H__


/////////////////////////////////////////////////////////////////////////////
// CAddressTypeComboBox window

class CNSAddressTypeControl : public CListBox //CComboBox
{
protected:
    CPen pen3dLight;
    CPen pen3dShadow;
    CPen pen3dHilight;
    CPen pen3dDkShadow;
    CPen penFace;
    CBrush brushFace;
    HFONT m_cfTextFont;
    int m_iTypeBitmapWidth;
// Construction
public:
	CNSAddressTypeControl();
	BOOL Create( CWnd *pParent );
    void DrawItemSoItLooksLikeAButton(CDC * pDC, CRect & rect, CString & cs);

protected:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNSAddressTypeControl)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CNSAddressTypeControl();

	// Generated message map functions
protected:
    void DrawHighlight(HDC hDC, CRect &rect, HPEN hpenTopLeft, HPEN hpenBottomRight);
    void DrawRaisedRect(HDC hDC, CRect &rect);

	//{{AFX_MSG(CNSAddressTypeControl)
	afx_msg void OnKillFocus(CWnd *);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSelchange();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

void DrawTransparentBitmap(
    HDC hdc, HBITMAP hBitmap, short xStart, short yStart, COLORREF cTransparentColor 
    );
void DrawTypeBitmap(CRect &rect, CDC * pDC, BOOL bPressed, BOOL bHighlight = FALSE);

#define TEXT_PAD    8

#endif // __NSADRTYP_H__  include class CNSAddressTypeControl
