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

#ifndef _SPLASH_H_
#define _SPLASH_H_
// splash.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CBigIcon window

class CBigIcon : public CButton
{
// Construction
public:
	CBigIcon();
	~CBigIcon();
// Attributes
public:
	HBITMAP m_hBitmap;
	CSize m_sizeBitmap;

// Operations
public:
	void 	SizeToContent(CSize& SizeBitmap);
	void	DisplayStatus(LPCSTR lpszStatus);
	void	CleanupResources(void);

// Implementation
protected:
    CFont   m_font;
	CFont	m_copyrightFont;

	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	void	DisplayCopyright(void);
	void CenterText(CClientDC &dc, LPCSTR lpszStatus, int top);

	//{{AFX_MSG(CBigIcon)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSplash dialog

class CSplashWnd : public CDialog
{
private:
    BOOL    m_bNavBusyIniting;
    int     m_timerID;
	CSize   m_sizeBitmap;

// Construction
public:
    CSplashWnd();
	BOOL Create(CWnd* pParent);

	void	DisplayStatus(LPCSTR lpszStatus);

// Dialog Data
	//{{AFX_DATA(CSplashWnd)
	enum { IDD = IDD_PLUGIN_SPLASH };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

// Implementation
    void NavDoneIniting();
	void SafeHide();

protected:
	CBigIcon m_icon; // self-draw button
    void OnLogoClicked();

	// Generated message map functions
	//{{AFX_MSG(CSplashWnd)
	virtual void OnTimer(UINT);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // _SPLASH_H_
