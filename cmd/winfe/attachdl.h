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

// attachdl.h : header file
//

#include "msgctype.h"
#include "mmnmsg.h"

/////////////////////////////////////////////////////////////////////////////
// CAttachDlg dialog

class CAttachDlg : public CDialog
{
// Construction
public:
	CAttachDlg( MSG_Pane * pPane = NULL,
	    char * pUrl = NULL, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAttachDlg)
	enum { IDD = IDD_MULTIATTACH };
	CButton	m_DeleteButton;
	CButton	m_AsIsButton;
	CButton	m_ConvertButton;
	CListBox	m_AttachmentList;
	int		m_bAsIs;
	//}}AFX_DATA

    char * m_pUrl;
    MSG_Pane * m_pPane;
    MSG_Pane * GetMsgPane() { return m_pPane; }


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAttachDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

    void CleanupAttachList(void);

	// Generated message map functions
	//{{AFX_MSG(CAttachDlg)
	afx_msg void OnAttachfile();
	afx_msg void OnAttachurl();
	afx_msg void OnConvert();
	afx_msg void OnDelete();
	afx_msg void OnAsis();
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeAttachments();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
/////////////////////////////////////////////////////////////////////////////
// CLocationDlg dialog

class CLocationDlg : public CDialog
{
// Construction
public:
	CLocationDlg(char * pUrl = NULL, CWnd* pParent = NULL);   // standard constructor

    char * m_pUrl;
// Dialog Data
	//{{AFX_DATA(CLocationDlg)
	enum { IDD = IDD_ATTACHLOCATION };
	CEdit	m_LocationBox;
	CString	m_Location;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLocationDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLocationDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
