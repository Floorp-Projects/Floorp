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

// edhdrdlg.h : header file
//

#ifndef __EDHDRDLG_H
#define __EDHDRDLG_H

#include "msg_filt.h"
//#include "apiimg.h"
#ifndef _WIN32
#include "ctl3d.h"
#endif
#include "mnrccln.h"



#define WM_EDIT_CUSTOM_DONE (WM_USER + 1556)


class CEditHeadersDlg : public CDialog
{
// Construction
public:
	CEditHeadersDlg(CString strHeader, CWnd* pParent = NULL );   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditHeadersDlg)
	enum { IDD = IDD_EDIT_HEADER };
	CString	m_strHeader;
	//}}AFX_DATA

	CMailNewsResourceSwitcher m_MNResourceSwitcher;

public:
	CWnd *m_pParent;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditHeadersDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	void GetHeader(CString &strHeader);

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CEditHeadersDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEditHeader();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CCustomHeadersDlg dialog

class CCustomHeadersDlg : public CDialog
{
// Construction
public:
	CCustomHeadersDlg(CWnd* pParent = NULL);   // standard constructor
	void PostNcDestroy();

// Dialog Data
	//{{AFX_DATA(CCustomHeadersDlg)
	enum { IDD = IDD_HEADER_LIST };
	CListBox	m_lbHeaderList;
	CString	m_strHeader;
	//}}AFX_DATA

public:
	CWnd *m_pParent;
	int m_nReturnCode; //tells us if the user pressed OK to close the dialog

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCustomHeadersDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CCustomHeadersDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnAddHeader();
	afx_msg void OnChangeEditHeaders();
	afx_msg void OnSetfocusHeaderList();
	afx_msg void OnRemoveHeader();
	afx_msg void OnEditHeader();
	virtual void OnOK();
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
#endif

