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
// AdvOpDlg.h : header file
//
#ifndef _ADVOPDLG__H_
#define _ADVOPDLG__H_

#include <afxwin.h>

#define WM_ADVANCED_OPTIONS_DONE (WM_USER + 1555)
/////////////////////////////////////////////////////////////////////////////
// CAdvSearchOptionsDlg dialog

class CAdvSearchOptionsDlg : public CDialog
{
// Construction
public:
	CAdvSearchOptionsDlg(CWnd* pParent = NULL);   // standard constructor

	void PostNcDestroy();
// Dialog Data
	//{{AFX_DATA(CAdvSearchOptionsDlg)
	enum { IDD = IDD_ADVANCED_SEARCH };
	BOOL	m_bIncludeSubfolders;
	int		m_iSearchArea;
	//}}AFX_DATA
	BOOL m_bChanges;
	CWnd *m_pParent;
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAdvSearchOptionsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAdvSearchOptionsDlg)
	afx_msg void OnOK();
	afx_msg BOOL OnInitDialog();
	afx_msg void OnHelp();
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif

