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
#include "stdafx.h"
#include "prefapi.h"
#include "logindg.h"
#include "winli.h"
#include "li_public.h"
#include "net.h"
#include "cast.h"  
#include "secnav.h"
#include "LIConflictDlg.h"

static int LIActive = 0;

void FE_makeConflictDialog(LIConflictDialogCallback f, void* closure, const char* title, const char* message,
						   const char* leftButton, const char* rightButton)
{
	// Create a new one, set bSuicide to TRUE so it deletes itself on exit.
	new CLIConflictDlg(f,closure,title,message,leftButton,rightButton,PR_TRUE);
}


void winli_RegisterCallbacks() 
{
	LI_RegisterCallbacks();
}

BOOL winli_StartAndVerify()
{
	int iResults;
	theApp.m_bInGetCriticalFiles = TRUE;

	LI_StartAndVerify(&iResults);

    while (iResults > 0) {
	    NET_ProcessNet((NETSOCK)-1, NET_EVERYTIME_TYPE);
    	FEU_StayingAlive();
    }

	theApp.m_bInGetCriticalFiles = FALSE;

	if (iResults == 0) 
		return TRUE;

	return FALSE;
}

/*   This function will "spin" until it retreives the necessary files (LI & MC).  It will then
 *   continue with the normal initialization
 */
Bool FEU_GetCriticalFiles(const char * szProfileName, const char * szProfileDir)
{
    int iResults = 0;
	theApp.m_bInGetCriticalFiles = TRUE;

	// if we got here, li is active
	LIActive = 1;

    LI_GetFiles(&iResults);

    while (iResults > 0) {
	    NET_ProcessNet((NETSOCK)-1, NET_EVERYTIME_TYPE);
    	FEU_StayingAlive();
    }

	theApp.m_bInGetCriticalFiles = FALSE;

//    return login_ProfileSelectedCompleteTheLogin(szProfileName,szProfileDir);	
    return TRUE;
}
	

Bool winli_QueryNetworkProfile(BOOL bTempProfile) {
    CRemoteProfileLoginDlg dlg;

    if (dlg.DoModal() == IDOK) {
   		CString pTempDir;
		if (bTempProfile) {
    		int ret;
    		XP_StatStruct statinfo; 

			if(getenv("TEMP"))
			  pTempDir = getenv("TEMP");  // environmental variable
			if (pTempDir.IsEmpty() && getenv("TMP"))
			  pTempDir = getenv("TMP");  // How about this environmental variable?
    		if (pTempDir.IsEmpty()) {
    			  // Last resort: put it in the windows directory
    		  char path[_MAX_PATH];
    		  GetWindowsDirectory(path, sizeof(path));
    		  pTempDir = path;
    		}

    		pTempDir += "\\nstmpusr";		
    		ret = _stat((char *)(const char *)pTempDir, &statinfo);
    		if (ret == 0) {
    			XP_RemoveDirectoryRecursive(pTempDir, xpURL);
    		}
    		mkdir(pTempDir);

    		login_CreateEmptyProfileDir(pTempDir,NULL,TRUE);

    		theApp.m_UserDirectory = pTempDir;
    		if (!pTempDir.IsEmpty())
    			PREF_SetDefaultCharPref( "profile.directory", pTempDir );
		}
    	return FEU_GetCriticalFiles(dlg.m_csLoginName, theApp.m_UserDirectory);
    }
    return FALSE;
}


/*   This function will "spin" until it gets the verify login return  It will then
 *   continue with FEU_GetCriticalFiles to fetch the startup files.
 */
Bool FEU_StartGetCriticalFiles(const char * szProfileName, const char * szProfileDir)
{
	// the login failed if the return is false 
	if (! winli_StartAndVerify())
		return FALSE;

    return FEU_GetCriticalFiles(szProfileName, szProfileDir);	
}

// Remote Profile Login Dialog code
CRemoteProfileLoginDlg::CRemoteProfileLoginDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CRemoteProfileLoginDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CLoginDlg)
    m_csLoginName = _T("");
    m_csPasswordName = _T("");
    //}}AFX_DATA_INIT
}


//  returns true when done
Bool winli_Exit() 
{
	static BOOL beenHere = FALSE;
	static int iResult;

	if (LIActive > 0) {  // REMIND, is this the right test to do?  should we check the pref instead?
		if (!beenHere) {
			beenHere = TRUE;

			theApp.m_bInGetCriticalFiles = TRUE;
		

			LI_Exit(&iResult);
		}

		NET_ProcessNet((NETSOCK)-1, NET_EVERYTIME_TYPE);
   		FEU_StayingAlive();
		if (iResult > 0) 
			return FALSE;

		LIActive = 0;

		/* ok to close browser */
		theApp.m_bInGetCriticalFiles = FALSE;

	}
	LI_UnregisterCallbacks();
	return TRUE;
}


void CRemoteProfileLoginDlg::DoDataExchange(CDataExchange* pDX) 
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CLoginDlg)
    DDX_Text(pDX, IDC_LOGINNAME, m_csLoginName);
    DDX_Text(pDX, IDC_PASSWORD, m_csPasswordName);
    //}}AFX_DATA_MAP
}

void CRemoteProfileLoginDlg::SetCheckItemLock(CString csPrefName, int  nID) 
{
	if (PREF_PrefIsLocked(csPrefName)) 
		GetDlgItem(nID)->EnableWindow(FALSE);
}

 /* fill the 'global' buffer that is passed in with the preference */
void CRemoteProfileLoginDlg::GetPreferenceString(const CString name, CString& buffer, int nID) 
{
	char *pstr;

	PREF_CopyCharPref(name, &pstr);
	SetCheckItemLock(name, nID);
	buffer = pstr;
	if (pstr) 
		XP_FREE(pstr);
}


BOOL CRemoteProfileLoginDlg::OnInitDialog() 
{
    BOOL bRet;

	GetPreferenceString("li.login.name", m_csLoginName, IDC_LOGINNAME);

    bRet = CDialog::OnInitDialog();

    CWnd * widget;
    
    widget = GetDlgItem(IDC_LOGINNAME);
    widget->SetFocus();

    return bRet;
}
    
void CRemoteProfileLoginDlg::OnOK() 
{
    UpdateData();

    PREF_SetCharPref("li.login.name",m_csLoginName);

	// always save the password, we will get rid of it in startandverify if 
	// the user doesn't want it saved.
	LI_SavePassword(m_csPasswordName);

	PREF_SetBoolPref("li.enabled",TRUE);

	if (winli_StartAndVerify())
		CDialog::OnOK();
	else  // we failed
		PREF_SetBoolPref("li.enabled",FALSE);

}


    void CAdvRemoteProfileLoginDlg::SetCheckItemLock(CString csPrefName, int  nID) 
	{
		if (PREF_PrefIsLocked(csPrefName)) 
			GetDlgItem(nID)->EnableWindow(FALSE);
	}

    void CRemoteProfileLoginDlg::OnAdvanced() 
    {
    	CAdvRemoteProfileLoginDlg dlg;
    	dlg.DoModal();
    }
    
    BEGIN_MESSAGE_MAP(CRemoteProfileLoginDlg, CDialog)
    	//{{AFX_MSG_MAP(CLoginDlg)
    	ON_BN_CLICKED(IDC_ADVANCED, OnAdvanced)
    	//}}AFX_MSG_MAP
    END_MESSAGE_MAP()
    
	 /* fill the 'global' buffer that is passed in with the preference */
	void CAdvRemoteProfileLoginDlg::GetPreferenceString(const CString name, CString& buffer, int nID) 
	{
		char *pstr;

		PREF_CopyCharPref(name, &pstr);
		SetCheckItemLock(name, nID);
		buffer = pstr;
		if (pstr) 
			XP_FREE(pstr);
	}

	void CAdvRemoteProfileLoginDlg::GetPreferenceString(const CString name, CString& buffer) 
	{
		char *pstr;

		PREF_CopyCharPref(name, &pstr);
		buffer = pstr;
		if (pstr) 
			XP_FREE(pstr);
	}

	void CAdvRemoteProfileLoginDlg::GetPreferenceBool(const CString name, BOOL* bPref, int nID) 
	{
		PREF_GetBoolPref(name, bPref);
		SetCheckItemLock(name, nID);
	}

	CAdvRemoteProfileLoginDlg::CAdvRemoteProfileLoginDlg(CWnd* pParent /*=NULL*/)
		: CDialog(CAdvRemoteProfileLoginDlg::IDD, pParent)
	{
    }
    
    BOOL CAdvRemoteProfileLoginDlg::OnInitDialog() 
    {
    	BOOL bRet;
    	bRet = CDialog::OnInitDialog();
    	
		CString csProtoStr;
		//{{AFX_DATA_INIT(CLoginDlg)
		GetPreferenceString("li.protocol", csProtoStr);

		m_iLDAP = (csProtoStr == "ldap");
		m_iHTTP = (csProtoStr == "http");
		GetPreferenceString("li.server.ldap.url", m_csLdapAddress, IDC_LDAP_ADDRESS);
		GetPreferenceString("li.server.ldap.userbase", m_csLdapBase, IDC_LDAP_SEARCHBASE);
		GetPreferenceString("li.server.http.baseURL", m_csHttpBase, IDC_HTTP_ADDRESS);
		GetPreferenceBool("li.client.bookmarks", &m_bBookmarks, IDC_BOOKMARKS);
		GetPreferenceBool("li.client.cookies",  &m_bCookies, IDC_COOKIES);
		GetPreferenceBool("li.client.filters",  &m_bFilters, IDC_FILTERS);
		GetPreferenceBool("li.client.addressbook",  &m_bAddressBook, IDC_ADDBOOK);
		GetPreferenceBool("li.client.globalhistory", &m_bHistory, IDD_HISTORY);
		GetPreferenceBool("li.client.liprefs",  &m_bPrefs, IDC_SUGGESTIONS);
		GetPreferenceBool("li.client.security",  &m_bSecurity, IDC_SECURITY_TYPE);
		GetPreferenceBool("li.client.javasecurity",  &m_bJavaSec, IDC_JAVA);
    	//}}AFX_DATA_INIT

		CButton * widget;
    	
    	widget = (CButton *) GetDlgItem(IDC_LDAP_SERVER);
    	widget->SetCheck(1);
    
    	widget = (CButton *) GetDlgItem(IDC_HTTP_SERVER);
    	widget->SetCheck(0);
    
		SetCheckItemLock("li.protocol", IDC_LDAP_SERVER); 
		SetCheckItemLock("li.protocol", IDC_HTTP_SERVER); 

		UpdateData(FALSE);
    	return bRet;
    }
    
    void CAdvRemoteProfileLoginDlg::DoDataExchange(CDataExchange* pDX) 
    {
    	CDialog::DoDataExchange(pDX);
    	//{{AFX_DATA_MAP(CLoginDlg)
    	DDX_Text(pDX, IDC_LDAP_ADDRESS, m_csLdapAddress);
    	DDX_Text(pDX, IDC_LDAP_SEARCHBASE, m_csLdapBase);
    	DDX_Text(pDX, IDC_HTTP_ADDRESS, m_csHttpBase);
    	DDX_Check(pDX, IDC_BOOKMARKS, m_bBookmarks);
    	DDX_Check(pDX, IDC_COOKIES, m_bCookies);
    	DDX_Check(pDX, IDC_FILTERS, m_bFilters);
    	DDX_Check(pDX, IDC_ADDBOOK, m_bAddressBook);
		DDX_Check(pDX, IDD_HISTORY, m_bHistory);
    	DDX_Check(pDX, IDC_SUGGESTIONS, m_bPrefs);
    	DDX_Check(pDX, IDC_JAVA, m_bJavaSec);
    	DDX_Check(pDX, IDC_SECURITY_TYPE, m_bSecurity);
		DDX_Check(pDX, IDC_LDAP_SERVER, (BOOL)m_iLDAP);
	 	DDX_Check(pDX, IDC_HTTP_SERVER, (BOOL)m_iHTTP);
    	//}}AFX_DATA_MAP
    }
    
    void CAdvRemoteProfileLoginDlg::OnOK() 
    {
    	CDialog::OnOK();
    	CButton * widget;
    	
    	widget = (CButton *) GetDlgItem(IDC_LDAP_SERVER);
    	m_iLDAP = widget->GetCheck();
    
    	widget = (CButton *) GetDlgItem(IDC_HTTP_SERVER);
    	m_iHTTP = widget->GetCheck();
    
    	if (m_iLDAP == 1) {
    		PREF_SetCharPref("li.protocol","ldap");
  			PREF_SetCharPref("li.server.ldap.url",m_csLdapAddress);
    		PREF_SetCharPref("li.server.ldap.userbase",m_csLdapBase);
    	}
    	if (m_iHTTP == 1) {
    		PREF_SetCharPref("li.protocol","http");
    		PREF_SetCharPref("li.server.http.baseURL",m_csHttpBase);
    	}
    	PREF_SetBoolPref("li.client.bookmarks", IsDlgButtonChecked(IDC_BOOKMARKS) );
    	PREF_SetBoolPref("li.client.cookies", IsDlgButtonChecked(IDC_COOKIES) );
    	PREF_SetBoolPref("li.client.filters", IsDlgButtonChecked(IDC_FILTERS) );
    	PREF_SetBoolPref("li.client.addressbook", IsDlgButtonChecked(IDC_ADDBOOK) );
		PREF_SetBoolPref("li.client.globalhistory", IsDlgButtonChecked(IDD_HISTORY) );
    	PREF_SetBoolPref("li.client.liprefs", IsDlgButtonChecked(IDC_SUGGESTIONS) );
    	PREF_SetBoolPref("li.client.security", IsDlgButtonChecked(IDC_SECURITY_TYPE) );
    	PREF_SetBoolPref("li.client.javasecurity", IsDlgButtonChecked(IDC_JAVA) );
    
    }
    
    void CAdvRemoteProfileLoginDlg::OnLDAP() 
    {
    	CWnd * widget;
    	
    	widget = GetDlgItem(IDC_HTTP_ADDRESS);
    	if (widget) widget->EnableWindow(FALSE);
    
    	widget = GetDlgItem(IDC_LDAP_ADDRESS);
    	if (widget) widget->EnableWindow(TRUE);
    
    	widget = GetDlgItem(IDC_LDAP_SEARCHBASE);
    	if (widget) widget->EnableWindow(TRUE);
    
    }
    
    void CAdvRemoteProfileLoginDlg::OnHTTP() 
    {
    	CWnd * widget;
    	
    	widget = GetDlgItem(IDC_HTTP_ADDRESS);
    	if (widget) widget->EnableWindow(TRUE);
    
    	widget = GetDlgItem(IDC_LDAP_ADDRESS);
    	if (widget) widget->EnableWindow(FALSE);
    
    	widget = GetDlgItem(IDC_LDAP_SEARCHBASE);
    	if (widget) widget->EnableWindow(FALSE);
    }
    
    BEGIN_MESSAGE_MAP(CAdvRemoteProfileLoginDlg, CDialog)
    	//{{AFX_MSG_MAP(CLoginDlg)
    	ON_BN_CLICKED( IDC_LDAP_SERVER, OnLDAP)
    	ON_BN_CLICKED( IDC_HTTP_SERVER, OnHTTP)
    	//}}AFX_MSG_MAP
    END_MESSAGE_MAP()
    
#endif //MOZ_LOC_INDEP
