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

#include "widgetry.h"
#include "pw_public.h"


extern Bool winli_QueryNetworkProfile(void);
extern Bool FEU_StartGetCriticalFiles(const char * szProfileName, const char * szProfileDir);

class CXPProgressDialog: public CDialog
{
public:
	CXPProgressDialog(CWnd* pParent =NULL);

	virtual  BOOL OnInitDialog( );
	BOOL PreTranslateMessage( MSG* pMsg );
    CProgressMeter m_ProgressMeter;
	CStatic	m_PercentComplete;
	int32 m_Min;
	int32 m_Max;
	int32 m_Range;
	PW_CancelCallback m_cancelCallback;
	void * m_cancelClosure;

protected:
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange*);
	afx_msg int OnCreate( LPCREATESTRUCT );

	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CLoginDlg dialog

class CRemoteProfileLoginDlg : public CDialog
{
// Construction
public:
	CRemoteProfileLoginDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CLoginDlg)
	enum { IDD = IDD_NETPROFILE_LOGIN };
	CString	m_csLoginName;
	CString m_csPasswordName;
	//}}AFX_DATA

	void Done();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLoginDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLoginDlg)
	virtual void OnOK();
	void OnAdvanced();
	BOOL OnInitDialog();

	//virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
};

class CAdvRemoteProfileLoginDlg : public CDialog
{
// Construction
public:
	CAdvRemoteProfileLoginDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CLoginDlg)
	enum { IDD = IDD_NETPROFILE_ADVANCED };
	CString	m_csLdapAddress;
	CString m_csLdapBase;
	CString m_csHttpBase;
	BOOL	m_bBookmarks;
	BOOL	m_bCookies;
	BOOL	m_bFilters;
	BOOL	m_bAddressBook;
	BOOL	m_bSecurity;
	BOOL	m_bJavaSec;
	BOOL	m_bNavcntr;
	BOOL	m_bPrefs;
	int		m_iLDAP;
	int		m_iHTTP;

	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLoginDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLoginDlg)
	virtual void OnOK();
	void OnLDAP();
	void OnHTTP();
	BOOL OnInitDialog();
	//virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
};
