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

/////////////////////////////////////////////////////////////////////////////
// CTip window

#define NSTTS_LEFT			0x0000
#define NSTTS_SELECTED		0x0001
#define NSTTS_ALWAYSSHOW	0x0002
#define NSTTS_RIGHT			0x0004 

class CTip : public CWnd
{
// Construction
public:
	CTip();
	virtual ~CTip();

// Attributes
protected:
	DWORD m_dwStyle;
	int m_iCSID;
	HFONT m_hFont;
	CString csTitle;
	int m_cxChar;

public:

// Operations
public:

	void Create();
	void SetCSID( int csid ) { m_iCSID = csid; }
	void Show( HWND owner, int, int, int, int, LPCSTR, DWORD = 0, HFONT = NULL);
	void Hide();

	// Generated message map functions
protected:
	afx_msg void OnPaint();
	afx_msg UINT OnNcHitTest(CPoint point);
	DECLARE_MESSAGE_MAP()
};
