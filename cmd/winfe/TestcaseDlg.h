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
// TabStopDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CWhiteBoxDlg dialog

#include "tclist.h"
#include "TestCaseManager.h"

class CWhiteBoxDlg : public CDialog
{
// Construction
public:
	CWhiteBoxDlg(CWnd* pParent = NULL);	// standard constructor
	~CWhiteBoxDlg();
// Dialog Data
	//{{AFX_DATA(CWhiteBoxDlg)
	enum { IDD = IDD_QA_DIALOG };
	CTCList	m_list;
	CButton	m_delete;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWhiteBoxDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
	CTestCaseManager m_tcManager;
	void ListTestCases();
protected:
	// Generated message map functions
	//{{AFX_MSG(CWhiteBoxDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnAdd();
	afx_msg void OnDelete();
	afx_msg void OnRun();
	afx_msg void OnSelChangeList();
	afx_msg void OnSave();
	afx_msg void OnImport();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
