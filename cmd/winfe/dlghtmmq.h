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

// dlghtmmq.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CHtmlMailQuestionDlg dialog
#ifndef __dlghtmmq__
#define __dlghtmmq__

class CHtmlMailQuestionDlg : public CDialog
{
// Construction
public:
	CHtmlMailQuestionDlg(MSG_Pane* pComposePane,CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CHtmlMailQuestionDlg)
	enum { IDD = IDD_HTML_MAIL_QUESTION };
	int		m_nRadioValue;
	//}}AFX_DATA
	MSG_Pane* m_pComposePane;
	MSG_HTMLComposeAction m_HTMLComposeAction;


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHtmlMailQuestionDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CHtmlMailQuestionDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnBtnRecipients();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


int CreateAskHTMLDialog (MSG_Pane* composepane, void* closure);

#endif

