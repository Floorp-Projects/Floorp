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
// CViewerSecurity dialog

class CViewerSecurity : public CDialog
{
// Construction
public:
	CViewerSecurity(CWnd* pParent = NULL);   // standard constructor

    //  Whether or not the cancel button was hit.
    BOOL m_bCanceled;

// Dialog Data
	//{{AFX_DATA(CViewerSecurity)
	enum { IDD = IDD_VIEWER_SECURITY };
	CString	m_csMessage;
	BOOL	m_bAskNoMore;
	CString	m_csDontAskText;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CViewerSecurity)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CViewerSecurity)
	virtual void OnCancel();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CLaunchHelper dialog

#define HELPER_OPEN_IT		0
#define HELPER_SAVE_TO_DISK	1

class CLaunchHelper : public CDialog
{
// Construction
public:
	CLaunchHelper(LPCSTR lpszFilename, LPCSTR lpszHelperApp, BOOL canDoOLE, CWnd* pParent = NULL);

	// Overridables (special message map entries)
	virtual BOOL OnInitDialog();

// Dialog Data
	//{{AFX_DATA(CLaunchHelper)
	enum { IDD = IDD_LAUNCH_HELPER };
	CString m_doc;
	CString	m_strMessage;
	BOOL    m_canDoOLE;
	BOOL	m_bAlwaysAsk;
	BOOL	m_bHandleByOLE;
	int		m_nAction;  // one of HELPER_OPEN_IT or HELPER_SAVE_TO_DISK
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLaunchHelper)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
    afx_msg void OnOpenit();
	afx_msg void OnSaveit();

	void EnableOLECheck(BOOL enable);
	// Generated message map functions
	//{{AFX_MSG(CLaunchHelper)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

