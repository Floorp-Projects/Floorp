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

// AskMeDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAskMeDlg dialog

class CAskMeDlg : public CDialog
{
// Construction
public:
	CAskMeDlg(BOOL bDefault = FALSE, int nOnOffLine = 0, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAskMeDlg)
	enum { IDD = IDD_ASKME_DIALOG };
	int		m_nStartupSelection;
	BOOL	m_bAskMeDefault;
	//}}AFX_DATA

	LOGFONT m_LogFont;
	HFONT m_hFont;
public:
	BOOL GetDefaultMode() const {return m_bAskMeDefault;};
	BOOL GetStartupSelection() const {return m_nStartupSelection;};
	void SetDefaultMode(BOOL bDefaultMode) {m_bAskMeDefault = bDefaultMode;};
	void SetStartupSelection(int nStartupSelection) {m_nStartupSelection = nStartupSelection;};
    void EnableDisableItem(BOOL bState, UINT nIDC);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAskMeDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAskMeDlg)
	virtual void OnOK();
	afx_msg void OnCheckAskMeDefault();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
