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

    #include "stdafx.h"
    #include "prefapi.h"
    #include "logindg.h"
    #include "winli.h"
    #include "li_public.h"
    #include "net.h"
    #include "cast.h"
    
    
	/*   This function will "spin" until it retreives the necessary files (LI & MC).  It will then
     *   continue with the normal initialization
     */
    Bool FEU_GetCriticalFiles(const char * szProfileName, const char * szProfileDir)
    {
    	int iNumFiles = 0;
	 	theApp.m_bInGetCriticalFiles = TRUE;
    
    	LI_StartGettingCriticalFiles(&iNumFiles);
	 	// REMIND:  Add Mission Control .JSC file retreival here
    
    	while (iNumFiles > 0) {
	        NET_ProcessNet(NULL, NET_EVERYTIME_TYPE);
    		FEU_StayingAlive();
    	}
	 	theApp.m_bInGetCriticalFiles = FALSE;
    
    	return login_ProfileSelectedCompleteTheLogin(szProfileName,szProfileDir);	
    }
	    

    Bool winli_QueryNetworkProfile(void) {
	 #ifdef _DEBUG
	 	// REMIND  need to get rid of this.
    	PREF_SetCharPref("li.protocol","ldap");
	 	PREF_SetCharPref("li.server.ldap.url","ldap://nearpoint.mcom.com/nsLIProfileName=$USERID,ou=LIProfiles,o=Airius.com");
	    PREF_SetCharPref("li.server.ldap.userbase","uid=$USERID,ou=People,o=Airius.com");
//	 	PREF_SetCharPref("li.server.ldap.searchBase","uid=$USERID, ou=LIProfiles, o=airius.com");
//	 	PREF_SetCharPref("li.login.name","");
	 #endif

    	CRemoteProfileLoginDlg dlg;
    	if (dlg.DoModal() == IDOK) {
    	    int ret;
    		XP_StatStruct statinfo; 
    
    		PREF_SetBoolPref("li.enabled",TRUE);
    
    		CString pTempDir;
    
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
    
    		return FEU_GetCriticalFiles(dlg.m_csLoginName,pTempDir);
    	}
    	return FALSE;
    }
    

	void verifyLoginCallbackFunction2(void * closure, LIStatus result) {
		int * pi = (int *)closure;

		if(result == LINoError) {
			*pi = 0;
			return ;
		}

		*pi = -result;

		LI_Shutdown();
		//REMIND deal with error here.
		AfxMessageBox("You cannot be logged in to the li server -- fix this error.");
	}
    
	/*   This function will "spin" until it gets the verify login return  It will then
     *   continue with FEU_GetCriticalFiles to fetch the startup files.
     */
    Bool FEU_StartGetCriticalFiles(const char * szProfileName, const char * szProfileDir)
    {
    	int iResult = 1;
	 	theApp.m_bInGetCriticalFiles = TRUE;
    
		LI_Init();

		LIMediator::verifyLogin(verifyLoginCallbackFunction2,(void*)&iResult);
    
    	while (iResult > 0) {
	        NET_ProcessNet(NULL, NET_EVERYTIME_TYPE);
    		FEU_StayingAlive();
    	}
		// the login failed if iResult is < 0 
		// REMIND we need to find a way to get back to the profile dialog.
		if (iResult < 0)
			return FALSE;

	 	theApp.m_bInGetCriticalFiles = FALSE;
    
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
    
    
    void CRemoteProfileLoginDlg::DoDataExchange(CDataExchange* pDX) 
    {
    	CDialog::DoDataExchange(pDX);
    	//{{AFX_DATA_MAP(CLoginDlg)
    	DDX_Text(pDX, IDC_LOGINNAME, m_csLoginName);
    	DDX_Text(pDX, IDC_PASSWORD, m_csPasswordName);
    	//}}AFX_DATA_MAP
    }
    
    BOOL CRemoteProfileLoginDlg::OnInitDialog() 
    {
    	BOOL bRet;
    	bRet = CDialog::OnInitDialog();
    
    	CWnd * widget;
    	
    	widget = GetDlgItem(IDC_LOGINNAME);
    	widget->SetFocus();
    
    	return bRet;
    }
    
void verifyLoginCallbackFunction(void * closure, 
    									LIStatus result) {
	if(result == LINoError) {
		((CRemoteProfileLoginDlg*)closure)->Done();
		return;
	}

	LI_Shutdown();
	//REMIND deal with error here.
	AfxMessageBox("Error");
}

void CRemoteProfileLoginDlg::Done()
{
	CDialog::OnOK();
}

void CRemoteProfileLoginDlg::OnOK() 
{

    UpdateData();

    PREF_SetCharPref("li.login.name",m_csLoginName);
    PREF_SetCharPref("li.login.password",m_csPasswordName);	

	LI_Init();

	LIMediator::verifyLogin(verifyLoginCallbackFunction,(void*)this);
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
	void GetPreferenceString(const CString name, CString& buffer) 
	{
		char *pstr;

		PREF_CopyCharPref(name, &pstr);
		buffer = pstr;
		if (pstr) 
			XP_FREE(pstr);
	}

	CAdvRemoteProfileLoginDlg::CAdvRemoteProfileLoginDlg(CWnd* pParent /*=NULL*/)
		: CDialog(CAdvRemoteProfileLoginDlg::IDD, pParent)
	{
		CString csProtoStr;
		//{{AFX_DATA_INIT(CLoginDlg)
		GetPreferenceString("li.protocol", csProtoStr);
		m_iLDAP = (csProtoStr == "ldap");
		m_iHTTP = (csProtoStr == "http");
		GetPreferenceString("li.server.ldap.url", m_csLdapAddress);
		GetPreferenceString("li.server.ldap.userbase", m_csLdapBase);
		GetPreferenceString("li.server.http.baseURL", m_csHttpBase);
		PREF_GetBoolPref("li.client.bookmarks", &m_bBookmarks);
		PREF_GetBoolPref("li.client.cookies",  &m_bCookies);
		PREF_GetBoolPref("li.client.filters",  &m_bFilters);
		PREF_GetBoolPref("li.client.addressbook",  &m_bAddressBook);
		PREF_GetBoolPref("li.client.navcntr", &m_bNavcntr);
		PREF_GetBoolPref("li.client.liprefs",  &m_bPrefs);
		PREF_GetBoolPref("li.client.security",  &m_bSecurity);
		PREF_GetBoolPref("li.client.javasecurity",  &m_bJavaSec);
    	//}}AFX_DATA_INIT
    }
    
    BOOL CAdvRemoteProfileLoginDlg::OnInitDialog() 
    {
    	BOOL bRet;
    	bRet = CDialog::OnInitDialog();
    	
    	CButton * widget;
    	
    	widget = (CButton *) GetDlgItem(IDC_LDAP_SERVER);
    	widget->SetCheck(1);
    
    	widget = (CButton *) GetDlgItem(IDC_HTTP_SERVER);
    	widget->SetCheck(0);
    
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
		DDX_Check(pDX, IDC_SELECTEDBOOKMARKS, m_bNavcntr);
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
		PREF_SetBoolPref("li.client.navcntr", IsDlgButtonChecked(IDC_SELECTEDBOOKMARKS) );
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
    
    BOOL CXPProgressDialog::OnInitDialog( )
    {
    	m_ProgressMeter.SubclassDlgItem( IDC_PROGRESSMETER, this );
    	return CDialog::OnInitDialog();
    }
    
    BEGIN_MESSAGE_MAP(CXPProgressDialog, CDialog)
    	ON_WM_CREATE()
    END_MESSAGE_MAP()
    
    CXPProgressDialog::CXPProgressDialog(CWnd* pParent /*=NULL*/)
    	: CDialog(IDD_XPPROGRESS, pParent)
    {
    	m_Min =0;
    	m_Max =100;
    	m_Range = 100;
	 	m_cancelCallback = NULL;
		m_cancelClosure = NULL;
    }
    
    BOOL CXPProgressDialog::PreTranslateMessage( MSG* pMsg )
    {
    	return CDialog::PreTranslateMessage(pMsg);
    }
    
    int CXPProgressDialog::OnCreate(LPCREATESTRUCT lpCreateStruct) 
    {
    	int res = CDialog::OnCreate(lpCreateStruct);
    	return res;
    }
    
    void CXPProgressDialog::OnCancel()
    {
		if (m_cancelCallback)
			m_cancelCallback(m_cancelClosure);
    	DestroyWindow();
    }
    
    void CXPProgressDialog::DoDataExchange(CDataExchange *pDX)
    {
    	CDialog::DoDataExchange(pDX);
    	DDX_Control(pDX, IDC_PERCENTCOMPLETE, m_PercentComplete);
    }
