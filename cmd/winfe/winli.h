/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifdef MOZ_LOC_INDEP

#include "widgetry.h"
#include "pw_public.h"


extern Bool winli_QueryNetworkProfile(BOOL bTempProfile);
extern Bool FEU_StartGetCriticalFiles(const char * szProfileName, const char * szProfileDir);
extern Bool winli_Exit(void);
extern void winli_RegisterCallbacks();


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
	void CRemoteProfileLoginDlg::GetPreferenceString(const CString name, CString & buffer, int nID); 
    void CRemoteProfileLoginDlg::SetCheckItemLock(CString csPrefName, int  nID) ;

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
	BOOL	m_bHistory;
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
	void CAdvRemoteProfileLoginDlg::GetPreferenceString(const CString name, CString & buffer, int nID); 
	void CAdvRemoteProfileLoginDlg::GetPreferenceString(const CString name, CString& buffer) ;
	void CAdvRemoteProfileLoginDlg::GetPreferenceBool(const CString name, BOOL * bPref, int nID);
    void CAdvRemoteProfileLoginDlg::SetCheckItemLock(CString csPrefName, int  nID) ;

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

#endif /* MOZ_LOC_INDEP */
