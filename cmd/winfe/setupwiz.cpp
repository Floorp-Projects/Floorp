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

#include "property.h"
#include "styles.h"

#include "helper.h"
#include "display.h"
#include "dialog.h"

#include "secnav.h"
#include "custom.h"
#include "cxabstra.h"
#include "setupwiz.h"
#include "logindg.h"
#include "prefapi.h"
#include "mnwizard.h"
#include "msgcom.h"
#include "mailmisc.h"
#include "mucproc.h"
#include "profile.h"
#include "mnprefs.h"

#define BUFSZ MAX_PATH+1

extern "C" BOOL IsNumeric(char* pStr);

#ifdef XP_WIN32

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CConfirmPage

CConfirmPage::CConfirmPage(CWnd *pParent)
	: CNetscapePropertyPage(IDD)
{
    //{{AFX_DATA_INIT(CIntroPage)
    //}}AFX_DATA_INIT
	m_pParent = (CNewProfileWizard*)pParent;
}

CConfirmPage::~CConfirmPage()
{
}

BOOL CConfirmPage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_FINISH | PSWIZB_BACK);
    return CNetscapePropertyPage::OnSetActive();
}

void CConfirmPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CConfirmPage::OnInitDialog()
{
	BOOL ret;
	
	ret = CNetscapePropertyPage::OnInitDialog();

	CButton * pMove = (CButton *) GetDlgItem(IDC_MOVEFILES);

	if (!m_pParent->m_bUpgrade) {
		GetDlgItem(IDC_MOVEFILES)->EnableWindow(FALSE);
		GetDlgItem(IDC_COPYFILES)->EnableWindow(FALSE);
		((CButton *)GetDlgItem(IDC_IGNOREFILES))->SetCheck(TRUE);
	}
	else if (pMove)
		pMove->SetCheck(TRUE);

	return ret;
}

int CConfirmPage::DoFinish()
{
	CButton * pMove = (CButton *) GetDlgItem(IDC_MOVEFILES);
	CButton * pCopy = (CButton *) GetDlgItem(IDC_COPYFILES);
	CButton * pIgnore = (CButton *) GetDlgItem(IDC_IGNOREFILES);
	int iMove = TRUE;
	int iCopy = FALSE;
	int iIgnore = FALSE;

	if (pMove)
		iMove = pMove->GetCheck();
	if (pCopy)
		iCopy = pCopy->GetCheck();
	if (pIgnore)
		iIgnore = pIgnore->GetCheck();

	if (iMove) {
		login_UpdateFilesToNewLocation(m_pParent->m_pProfilePath,m_pParent,FALSE);  // move files
		login_UpdatePreferencesToJavaScript(m_pParent->m_pProfilePath); // upgrade prefs
	} else if (iCopy) {
		login_UpdateFilesToNewLocation(m_pParent->m_pProfilePath,m_pParent,TRUE);  // Copy files
		login_UpdatePreferencesToJavaScript(m_pParent->m_pProfilePath); // upgrade prefs
	} else {
		// just create the directories --
		login_CreateEmptyProfileDir(m_pParent->m_pProfilePath, m_pParent,m_pParent->m_bExistingDir); 
	}
	return TRUE;
}

BEGIN_MESSAGE_MAP(CConfirmPage, CNetscapePropertyPage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CIntroPage

CIntroPage::CIntroPage(CWnd *pParent)
	: CNetscapePropertyPage(IDD)
{
    //{{AFX_DATA_INIT(CIntroPage)
    //}}AFX_DATA_INIT
	m_pParent = (CNewProfileWizard*)pParent;
}

BOOL CIntroPage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_NEXT);
    return CNetscapePropertyPage::OnSetActive();
}

BOOL CIntroPage::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();

	if(theApp.m_bPEEnabled)
	{
		CString m_str;
		m_str.LoadString(IDS_PE_INTROPAGE_TEXT);
		GetDlgItem(IDC_INTRO_TEXT)->SetWindowText(m_str);
	}
	return ret;
}

void CIntroPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CIntroPage, CNetscapePropertyPage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNamePage

CNamePage::CNamePage(CWnd *pParent)
	: CNetscapePropertyPage(IDD)
{
    //{{AFX_DATA_INIT(CProfileNamePage)
    //}}AFX_DATA_INIT
	m_pParent = (CNewProfileWizard*)pParent;
}

CNamePage::~CNamePage()
{
}

BOOL CNamePage::OnInitDialog()
{
	BOOL ret;
	char * pString = NULL;
	
	ret = CNetscapePropertyPage::OnInitDialog();

	if (m_pParent->m_bUpgrade) {
		// if we aren't creating a new profile, we are updating an existing one
		CString csUserAddr = theApp.GetProfileString("User","User_Addr","DefaultUser");
		CString csFullName = theApp.GetProfileString("User","User_Name","");

		m_pParent->m_pUserAddr = csUserAddr;
		m_pParent->m_pFullName = csFullName;
	} else {
		char buffer[256];
		int nLen = 255;

		if (PREF_NOERROR == PREF_GetCharPref("mail.identity.username", buffer, &nLen))
			m_pParent->m_pFullName = buffer;
		if (PREF_NOERROR == PREF_GetCharPref("mail.identity.useremail", buffer, &nLen))
			m_pParent->m_pUserAddr = buffer;
	}

	SetDlgItemText(IDC_USER_NAME, m_pParent->m_pFullName);
	SetDlgItemText(IDC_EMAIL_ADDR, m_pParent->m_pUserAddr);

	if(theApp.m_bPEEnabled)
		ShowHideEmailName();

	return ret;
}
void CNamePage::ShowHideEmailName()
{
	// PE: disable email entry
	int nShowCmd;
	CString text;

	if(theApp.m_bPEEnabled && m_pParent->m_bASWEnabled)
	{
		text.LoadString(IDS_PEMUC_NAMEPAGE_TEXT);
		SetDlgItemText(IDC_EMAIL_TEXT1,(LPCTSTR)text);
		nShowCmd = SW_HIDE;
	}
	else
	{
		text.LoadString(IDS_MUP_NAMEPAGE_TEXT);
		SetDlgItemText(IDC_EMAIL_TEXT1,(LPCTSTR)text);
		nShowCmd = SW_SHOW;
	}

	GetDlgItem(IDC_EMAIL_ADDRTEXT)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EMAIL_ADDR)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EMAIL_ADDREG)->ShowWindow(nShowCmd);

}
BOOL CNamePage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_NEXT | PSWIZB_BACK);
    return CNetscapePropertyPage::OnSetActive();
}

void CNamePage::DoFinish()
{
	if (!m_pParent->m_pFullName.IsEmpty())
		PREF_SetCharPref("mail.identity.username",m_pParent->m_pFullName);
	if (!m_pParent->m_pUserAddr.IsEmpty())
		PREF_SetCharPref("mail.identity.useremail",m_pParent->m_pUserAddr);
}

void CNamePage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	if (pDX->m_bSaveAndValidate) {
		char name[BUFSZ];
	    if (GetDlgItemText(IDC_EMAIL_ADDR, name, BUFSZ)) 
			m_pParent->m_pUserAddr = name;
		if (GetDlgItemText(IDC_USER_NAME, name, BUFSZ))
			m_pParent->m_pFullName = name;

		// Set the preference now so that future pages (like the mail/news pages) will be able
		// to access it.
		if (!m_pParent->m_pFullName.IsEmpty())
			PREF_SetCharPref("mail.identity.username",m_pParent->m_pFullName);
		if (!m_pParent->m_pUserAddr.IsEmpty())
			PREF_SetCharPref("mail.identity.useremail",m_pParent->m_pUserAddr);

	}
}

BEGIN_MESSAGE_MAP(CNamePage, CNetscapePropertyPage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProfileNamePage

CProfileNamePage::CProfileNamePage(CWnd *pParent)
	: CNetscapePropertyPage(IDD)
{
    //{{AFX_DATA_INIT(CProfileNamePage)
    //}}AFX_DATA_INIT
	m_pParent = (CNewProfileWizard*)pParent;
	m_pParent->m_pProfileName = "";
	m_pParent->m_pProfilePath = "";
}

CProfileNamePage::~CProfileNamePage()
{
}

BOOL CProfileNamePage::OnInitDialog()
{
	BOOL ret;
	char * pString = NULL;
	
	ret = CNetscapePropertyPage::OnInitDialog();

	CString csUserAddrShort;
	CString csUserDirectory;
	
	csUserDirectory.Empty();

	int iAtSign = m_pParent->m_pUserAddr.Find('@');
	
	if (iAtSign != -1) 
		csUserAddrShort = m_pParent->m_pUserAddr.Left(iAtSign);
	else
		csUserAddrShort = m_pParent->m_pUserAddr;

	if (csUserAddrShort.IsEmpty()) 
		csUserAddrShort = "default";

	CUserProfileDB::AssignProfileDirectoryName(csUserAddrShort,csUserDirectory);

    SetDlgItemText(IDC_PROFILE_DIR, csUserDirectory);
	SetDlgItemText(IDC_PROFILE_NAME, csUserAddrShort);
	PREF_SetCharPref("mail.pop_name",csUserAddrShort);
	return ret;
}

BOOL CProfileNamePage::OnSetActive()
{
#ifndef MOZ_MAIL_NEWS // Is this the correct ifdef?
	if(!theApp.m_bPEEnabled ||( theApp.m_bPEEnabled && (!m_pParent->m_bMucEnabled) && (!m_pParent->m_bASWEnabled)))
	{
		if(!m_pParent->m_bUpgrade)
		{
			m_pParent->SetWizardButtons(PSWIZB_FINISH | PSWIZB_BACK);

			CString	text;
			text.LoadString(IDS_CLICK_FINISH);
			GetDlgItem(IDC_PROFILENAME_TEXT)->SetWindowText(text);
		}

	}
	else
			m_pParent->SetWizardButtons(PSWIZB_NEXT | PSWIZB_BACK);
#else
	m_pParent->SetWizardButtons(PSWIZB_NEXT | PSWIZB_BACK);
#endif /* MOZ_MAIL_NEWS */
    return CNetscapePropertyPage::OnSetActive();
}


int CProfileNamePage::DoFinish()
{
    char path[BUFSZ],name[BUFSZ];
	int ret;
	XP_StatStruct statinfo; 

    if (!GetDlgItemText(IDC_PROFILE_NAME, name, BUFSZ)) {
		AfxMessageBox(szLoadString(IDS_INVALID_PROFILE_NAME),MB_OK);
		return FALSE;
	}

    if (GetDlgItemText(IDC_PROFILE_DIR, path, BUFSZ)) { 
		if (path[strlen(path)-1] == '\\') path[strlen(path)-1] = NULL;  // remove last slash...

		ret = _stat((char *) path, &statinfo);
		if (!ret) {
			// Directory already exists!
			if (AfxMessageBox(szLoadString(IDS_PROFDIR_EXISTS),MB_OKCANCEL) == IDCANCEL)
				return FALSE;
			else 
				m_pParent->m_bExistingDir = TRUE;
		}
		if(ret == -1) {
			// see if we can just create it
			char * slash = strchr(path,'\\');
			while (slash) {
				slash[0] = NULL;
				ret = CreateDirectory(path,NULL);
				slash[0] = '\\';
				if (slash+1) slash = strchr(slash+1,'\\');
			}
			ret = CreateDirectory(path,NULL);
			if (!ret) {
				AfxMessageBox(szLoadString(IDS_UNABLE_CREATE_DIR),MB_OK);
				return FALSE;
			}
		}
	} else {
		AfxMessageBox(szLoadString(IDS_INVALID_PROFILE_NAME),MB_OK);
		return FALSE;
	}

    if (GetDlgItemText(IDC_PROFILE_NAME, name, BUFSZ)) 
		login_CreateNewUserKey(name,path);
	m_pParent->m_pProfileName = name;
	m_pParent->m_pProfilePath = path;
	return TRUE;
}

void CProfileNamePage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CProfileNamePage::UpdateData(BOOL bValidate) 
{
    char path[BUFSZ];

	if (bValidate) {
	    if (GetDlgItemText(IDC_PROFILE_DIR, path, BUFSZ)) {
			if (!path || !path[0]) {
				AfxMessageBox(szLoadString(IDS_PROFILE_EMPTY));
				return FALSE;
			}
		}
		else return FALSE;
	}
	return TRUE;
}

void CProfileNamePage::GetProfilePath(char *path)
{

	if (GetDlgItemText(IDC_PROFILE_DIR, path, BUFSZ)) 
		if (path[strlen(path)-1] == '\\') 
			path[strlen(path)-1] = NULL;  // remove last slash...
}

BEGIN_MESSAGE_MAP(CProfileNamePage, CNetscapePropertyPage)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CNewProfileWizard               
CNewProfileWizard::CNewProfileWizard(CWnd *pParent, BOOL bUpgrade)
	: CNetscapePropertySheet("", pParent)
{
	m_pIntroPage = new CIntroPage(this);
	m_pNamePage = new CNamePage(this);
	m_pProfileNamePage = new CProfileNamePage(this);
	m_pConfirmPage = new CConfirmPage(this);

	m_bUpgrade = bUpgrade;
	m_bExistingDir = FALSE;
	m_pUserAddr = "defaultuser@domain.com";

#ifdef MOZ_MAIL_NEWS
	m_pSendMailPage = new CSendMailPage(this);
	m_pReceiveMailPage = new CReceiveMailPage(this);
	m_pReadNewsPage = new CReadNewsPage(this);
#endif /* MOZ_MAIL_NEWS */

	// PE: pe multiple user configuration
	if(theApp.m_bPEEnabled){
		m_pMucIntroPage = new CMucIntroPage(this);
		m_pMucEditPage = new CMucEditPage(this,TRUE);
		m_pASWReadyPage = new CASWReadyPage(this);
		m_pMucReadyPage = new CMucReadyPage(this);
	}

	// PE: replace intro page with pe intro page
	if(theApp.m_bPEEnabled)
	{
		AddPage(m_pIntroPage);
		AddPage(m_pMucIntroPage);
		AddPage(m_pNamePage);     
		AddPage(m_pProfileNamePage);
		AddPage(m_pASWReadyPage);
#ifdef MOZ_MAIL_NEWS      
		AddPage(m_pSendMailPage);
		AddPage(m_pReceiveMailPage);
		AddPage(m_pReadNewsPage);
#endif // MOZ_MAIL_NEWS      
		AddPage(m_pMucReadyPage);
		AddPage(m_pMucEditPage);
		AddPage(m_pConfirmPage);
	}
	else
	{
		AddPage(m_pIntroPage);
		AddPage(m_pNamePage);     
		AddPage(m_pProfileNamePage);
		if (bUpgrade) 
			AddPage(m_pConfirmPage);
		else 
		{
#ifdef MOZ_MAIL_NEWS      
			AddPage(m_pSendMailPage);
			AddPage(m_pReceiveMailPage);
			AddPage(m_pReadNewsPage);
#endif /* MOZ_MAIL_NEWS */
		}
	}
	SetWizardMode();
}

BOOL CNewProfileWizard::OnInitDialog()
{
	BOOL ret = CNetscapePropertySheet::OnInitDialog();
	GetWindowText(m_title);
	GetDlgItem(IDHELP)->ShowWindow(SW_HIDE);

	return ret;
}

CNewProfileWizard::~CNewProfileWizard()
{
	if (m_pIntroPage)
		delete m_pIntroPage;
	if (m_pNamePage)
		delete m_pNamePage;
	if (m_pProfileNamePage)
		delete m_pProfileNamePage;
	if (m_pConfirmPage)
		delete m_pConfirmPage;
#ifdef MOZ_MAIL_NEWS      
	if (m_pSendMailPage)
		delete m_pSendMailPage;
	if (m_pReceiveMailPage)
		delete m_pReceiveMailPage;   
	if (m_pReadNewsPage)
		delete m_pReadNewsPage;   
#endif // MOZ_MAIL_NEWS
	// PE: MUC
	if (theApp.m_bPEEnabled && m_pMucIntroPage)
		delete m_pMucIntroPage;
	if (theApp.m_bPEEnabled && m_pMucEditPage)
		delete m_pMucEditPage;
	if (theApp.m_bPEEnabled && m_pMucReadyPage)
		delete m_pMucReadyPage;
	if (theApp.m_bPEEnabled && m_pASWReadyPage)
		delete m_pASWReadyPage;
}

// flag is to enable finish button push
void CNewProfileWizard::DoFinish()
{
	if (m_pNamePage && ::IsWindow(m_pNamePage->GetSafeHwnd()))
		m_pNamePage->DoFinish();

	if (m_pProfileNamePage && ::IsWindow(m_pProfileNamePage->GetSafeHwnd())) {
		if (m_pProfileNamePage->DoFinish())     {
			if (m_bUpgrade) {
				if (m_pConfirmPage && ::IsWindow(m_pConfirmPage->GetSafeHwnd()))
					m_pConfirmPage->DoFinish();
			} else {
				login_CreateEmptyProfileDir(m_pProfilePath, this, m_bExistingDir); 

#ifdef MOZ_MAIL_NEWS
				if (m_pSendMailPage && ::IsWindow(m_pSendMailPage->GetSafeHwnd()))
					m_pSendMailPage->DoFinish();
				if (m_pReceiveMailPage && ::IsWindow(m_pReceiveMailPage->GetSafeHwnd()))
					m_pReceiveMailPage->DoFinish();   
				if (m_pReadNewsPage && ::IsWindow(m_pReadNewsPage->GetSafeHwnd()))
					m_pReadNewsPage->DoFinish(); 
#endif // MOZ_MAIL_NEWS               
			}

			if (theApp.m_bPEEnabled)
			{
				// PE: upgrad case
				if(m_bMucEnabled && m_bUpgrade && m_pMucEditPage 
					&& ::IsWindow(m_pMucEditPage->GetSafeHwnd()))
						m_pMucEditPage->DoFinish();

				// PE: dialer thread: 
				if(m_bMucEnabled && m_pMucEditPage 
					&& ::IsWindow(m_pMucEditPage->GetSafeHwnd()))
						m_pMucEditPage->DoFinish();

				// PE: account setup thread
				else if (m_bASWEnabled && m_pASWReadyPage 
					&& ::IsWindow(m_pASWReadyPage->GetSafeHwnd()))
						m_pASWReadyPage->DoFinish();

				// PE: network thread
				else 
				{
					CMucProc        m_mucProc;
					m_mucProc.SetDialOnDemand("",FALSE);
				}
			}

			PressButton(PSBTN_FINISH);
		}
	}
}

void CNewProfileWizard::DoNext()
{
	PressButton(PSBTN_NEXT);

	if(theApp.m_bPEEnabled)
	{
		CPropertyPage* curPage = GetActivePage();

		if(curPage == m_pMucIntroPage && m_bUpgrade)    // skip muc intro page
				SetActivePage(m_pNamePage);
		
		if(curPage == m_pNamePage && ::IsWindow(m_pNamePage->GetSafeHwnd()))
				m_pNamePage->ShowHideEmailName();
		
		if(curPage == m_pASWReadyPage)
		{
			if(!m_bASWEnabled)
			{
				if (m_bUpgrade) 
					SetActivePage(m_pMucEditPage);
				else
#ifdef MOZ_MAIL_NEWS               
					SetActivePage(m_pSendMailPage);
#else
					SetActivePage(m_pMucReadyPage);
#endif /* MOZ_MAIL_NEWS */
			}
		}
		
#ifdef MOZ_MAIL_NEWS      
		if(curPage == m_pReceiveMailPage)
			m_pReadNewsPage->SetFinish(!m_bMucEnabled);
#endif /* MOZ_MAIL_NEWS */
		
		if(curPage == m_pMucReadyPage || curPage == m_pMucEditPage)
			SetTitle(m_title);
	}
}

void CNewProfileWizard::DoBack()
{
	PressButton(PSBTN_BACK);

	if(theApp.m_bPEEnabled)
	{
		CPropertyPage* curPage = GetActivePage();
		
		if(curPage == m_pASWReadyPage)  
			SetActivePage(m_pProfileNamePage);
		
		if(curPage == m_pMucReadyPage && m_bUpgrade)
			SetActivePage(m_pProfileNamePage);

		if(curPage == m_pMucIntroPage && m_bUpgrade)
			SetActivePage(m_pIntroPage);
	}
}

void CNewProfileWizard::GetProfilePath(char *str)
{
	if (m_pProfileNamePage && ::IsWindow(m_pProfileNamePage->GetSafeHwnd())) 
		m_pProfileNamePage->GetProfilePath(str);
}

BEGIN_MESSAGE_MAP(CNewProfileWizard, CNetscapePropertySheet)
	ON_BN_CLICKED(ID_WIZFINISH,DoFinish)
	ON_BN_CLICKED(ID_WIZNEXT,DoNext)
	ON_BN_CLICKED(ID_WIZBACK,DoBack)
END_MESSAGE_MAP()

#else
/////////////////////////////////////////////////////////////////////////////
// CNewProfileWizard               
CNewProfileWizard::CNewProfileWizard(CWnd *pParent, BOOL bUpgrade)
	: CDialog(IDD, pParent)
{
	m_nCurrentPage = ID_PAGE_INTRO;
	m_bUpgrade = bUpgrade;

	// PE: pe multiple user configuration
	if(theApp.m_bPEEnabled)
	{
		m_pMucIntroPage = new CMucIntroPage(this);
		m_pMucEditPage = new CMucEditPage(this,TRUE);
		m_pASWReadyPage = new CASWReadyPage(this);
		m_pMucReadyPage = new CMucReadyPage(this);  
	}
}

CNewProfileWizard::~CNewProfileWizard()
{
}

void CNewProfileWizard::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CNewProfileWizard::InitPrefStrings()
{
	CString csUserAddr = "";
	CString csFullName = "";

	if (m_bUpgrade) {
		csUserAddr = theApp.GetProfileString("User","User_Addr","");
		csFullName = theApp.GetProfileString("User","User_Name","");   
	}

	m_pUserAddr = csUserAddr;
	m_pFullName = csFullName;
	m_pProfileName = "";
	m_pProfilePath = "";
	m_bExistingDir = FALSE;

#ifdef MOZ_MAIL_NEWS
	PREF_GetBoolPref("mail.leave_on_server", &m_bLeftOnServer);

	m_szFullName = g_MsgPrefs.m_csUsersFullName;
	m_szEmail = g_MsgPrefs.m_csUsersEmailAddr;
#endif /* MOZ_MAIL_NEWS */

	char *prefStr = NULL;

	PREF_CopyCharPref("network.hosts.smtp_server", &prefStr);
	if (prefStr)
		m_szMailServer = prefStr;
	XP_FREEIF(prefStr);

	prefStr = NULL;
	PREF_CopyCharPref("mail.pop_name", &prefStr);
	if (prefStr)
		m_szPopName = prefStr;
	XP_FREEIF(prefStr);

	prefStr = NULL;
	PREF_CopyCharPref("network.hosts.pop_server", &prefStr);
	if (prefStr)
		m_szInMailServer = prefStr;
	XP_FREEIF(prefStr);

	if (m_szInMailServer.IsEmpty())
		m_szInMailServer = m_szMailServer;

	int32 prefInt = 0;
	PREF_GetIntPref("mail.server_type", &prefInt);
	m_bUseIMAP = prefInt == MSG_Imap4;

	prefStr = NULL;
	PREF_CopyCharPref("network.hosts.nntp_server", &prefStr);
	if (prefStr)
		m_szNewsServer = prefStr;
	XP_FREEIF(prefStr);

	m_bIsSecure = FALSE;
	m_nPort = NEWS_PORT;
}

BOOL CNewProfileWizard::OnInitDialog()
{
	GetDlgItem(IDC_EDIT1)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_EG1)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_EG2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC3)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT3)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_READMAIL_POP1)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_READMAIL_POP2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_READMAIL_IMAP)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC5)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_SECURE)->ShowWindow(SW_HIDE);

	GetDlgItem(IDC_STATIC_TITLE)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC1)->ShowWindow(SW_HIDE);

	ShowHideNamePage(SW_HIDE); 
	ShowHideProfilePage(SW_HIDE);
	ShowHideConfirmPage(SW_HIDE);
	ShowHideSendPage(SW_HIDE);
	ShowHideReceivePage(SW_HIDE);
	ShowHideNewsPage(SW_HIDE);

	InitPrefStrings();

	SetDlgItemText(IDC_USER_NAME, m_pFullName);
	SetDlgItemText(IDC_EMAIL_ADDR, m_pUserAddr);
	GetDlgItem(IDC_BUTTON_BACK)->EnableWindow(FALSE);  

	if(theApp.m_bPEEnabled)
	{
		m_pMucIntroPage->Create(IDD_MUCWIZARD_INTRO, this);   
		m_pMucEditPage->Create(IDD_MUCWIZARD_EDIT, this);
		m_pASWReadyPage->Create(IDD_MUCWIZARD_ASWREADY, this);
		m_pMucReadyPage->Create(IDD_MUCWIZARD_MUCREADY, this);   
		
		ShowHidePEMucIntroPage(SW_HIDE);
		ShowHidePEMucEditPage(SW_HIDE);
		ShowHidePEMucASWReadyPage(SW_HIDE);
		ShowHidePEMucReadyPage(SW_HIDE);
	}

	return CDialog::OnInitDialog();
}

void CNewProfileWizard::DoBack()
{
	// PE:
	if(theApp.m_bPEEnabled)
	{
		if(m_nCurrentPage == ID_PEMUC_INTRO) 
			m_nCurrentPage = ID_PAGE_INTRO;
		else if(m_nCurrentPage == ID_PAGE_NAME && !m_bUpgrade) 
			m_nCurrentPage = ID_PEMUC_INTRO;
		else if(m_nCurrentPage == ID_PAGE_NAME && m_bUpgrade) 
			m_nCurrentPage = ID_PAGE_INTRO;
		else if(m_nCurrentPage == ID_PEMUC_ASWREADY) 
			m_nCurrentPage = ID_PAGE_PROFILE;  
#ifdef MOZ_MAIL_NEWS
		else if(m_nCurrentPage == ID_PEMUC_MUCREADY && !m_bUpgrade) 
			m_nCurrentPage = ID_PAGE_READNEWS;     
#else
		else if(m_nCurrentPage == ID_PEMUC_MUCREADY && !m_bUpgrade) 
			m_nCurrentPage = ID_PAGE_PROFILE;     
#endif /* MOZ_MAIL_NEWS */
		else if(m_nCurrentPage == ID_PEMUC_MUCREADY && m_bUpgrade) 
			m_nCurrentPage = ID_PAGE_PROFILE;
		else if(m_nCurrentPage == ID_PEMUC_MUCEDIT) 
			m_nCurrentPage = ID_PEMUC_MUCREADY;   
		else if (m_nCurrentPage == ID_PAGE_SENDMAIL) 
			m_nCurrentPage = ID_PAGE_PROFILE;
		else if (m_nCurrentPage == ID_PAGE_FINISH && m_bUpgrade) 
			m_nCurrentPage = ID_PAGE_CONFIRM;
		else if (m_nCurrentPage == ID_PAGE_CONFIRM) 
			m_nCurrentPage = ID_PEMUC_MUCEDIT;
		else if (m_nCurrentPage == ID_PAGE_FINISH && m_bASWEnabled) 
			m_nCurrentPage = ID_PEMUC_ASWREADY;
		else if (m_nCurrentPage == ID_PAGE_FINISH && m_bMucEnabled) 
			m_nCurrentPage = ID_PEMUC_MUCEDIT;  
#ifndef MOZ_MAIL_NEWS
		else if (m_nCurrentPage == ID_PAGE_FINISH && (!m_bMucEnabled) && (!m_bASWEnabled)) 
			m_nCurrentPage = ID_PAGE_PROFILE;   
#else 
		else if (m_nCurrentPage == ID_PAGE_FINISH && (!m_bMucEnabled) && (!m_bASWEnabled)) 
			m_nCurrentPage = ID_PAGE_READNEWS;   
#endif // MOZ_MAIL_NEWS
		else
			m_nCurrentPage -= 1; 
			
	} //pe
	else
	{   
#ifdef MOZ_MAIL_NEWS
		if (m_nCurrentPage == ID_PAGE_SENDMAIL) 
			m_nCurrentPage = ID_PAGE_PROFILE;
		else     
			m_nCurrentPage -= 1;    
#else           
		if(m_bUpgrade && m_nCurrentPage == ID_PAGE_FINISH) 
			m_nCurrentPage = ID_PAGE_CONFIRM;
		else if(m_bUpgrade && m_nCurrentPage == ID_PAGE_CONFIRM) 
			m_nCurrentPage = ID_PAGE_PROFILE; 
		else m_nCurrentPage -= 1;  
#endif /* MOZ_MAIL_NEWS */

	} 
	
	
	switch  (m_nCurrentPage)
	{
	case ID_PAGE_INTRO:             // 1st page
		if(theApp.m_bPEEnabled)
			ShowHidePEMucIntroPage(SW_HIDE);
		ShowHideNamePage(SW_HIDE); 
		ShowHideIntroPage(SW_SHOW);
		GetDlgItem(IDC_BUTTON_BACK)->EnableWindow(FALSE);
		break;
	case ID_PAGE_NAME:  // 2nd page
#ifndef MOZ_MAIL_NEWS // Is this the correct ifdef?
		SetControlText(IDOK, IDS_NEXT);
#endif // MOZ_MAIL_NEWS
		ShowHideProfilePage(SW_HIDE);
		ShowHideNamePage(SW_SHOW);
		break;
	case ID_PEMUC_INTRO:  // PE: 2nd page
		ShowHideNamePage(SW_HIDE);
		ShowHidePEMucIntroPage(SW_SHOW);
		break;
	case ID_PAGE_PROFILE:   // 3rd page
#ifndef MOZ_MAIL_NEWS // Is this the correct ifdef?
		if((!theApp.m_bPEEnabled) && (!m_bUpgrade))
			SetControlText(IDOK, IDS_FINISH);      
		else if (theApp.m_bPEEnabled && (m_bASWEnabled || m_bMucEnabled || m_bUpgrade))
			SetControlText(IDOK, IDS_NEXT);
#else
		SetControlText(IDOK, IDS_NEXT);
#endif // MOZ_MAIL_NEWS
		ShowHideConfirmPage(SW_HIDE);
		ShowHideSendPage(SW_HIDE);
		if(theApp.m_bPEEnabled) 
		{
			ShowHidePEMucASWReadyPage(SW_HIDE);
			ShowHidePEMucReadyPage(SW_HIDE);     //upgrade
		}
		ShowHideProfilePage(SW_SHOW);
		break;
#ifdef MOZ_MAIL_NEWS
	case ID_PAGE_SENDMAIL:  // 4th page
		ShowHideReceivePage(SW_HIDE);
		ShowHideSendPage(SW_SHOW);
		break;
	case ID_PAGE_RECEIVEMAIL:       // 5th page
		SetControlText(IDOK, IDS_NEXT);
		ShowHideNewsPage(SW_HIDE);
		ShowHideReceivePage(SW_SHOW);
		break;
	case ID_PAGE_READNEWS:  // PE: 6th page 
		if(theApp.m_bPEEnabled && m_bMucEnabled)
			SetControlText(IDOK, IDS_NEXT);
		else
			SetControlText(IDOK, IDS_FINISH);
		ShowHidePEMucReadyPage(SW_HIDE);
		ShowHideNewsPage(SW_SHOW);
		break;
#endif /* MOZ_MAIL_NEWS */
	case ID_PEMUC_MUCREADY: // PE: 7th page
		SetControlText(IDOK, IDS_NEXT);
		ShowHidePEMucReadyPage(SW_SHOW);
		ShowHidePEMucEditPage(SW_HIDE);
		break;
	case ID_PEMUC_MUCEDIT: // PE: 8th page 
		if(m_bUpgrade)
		{
			SetControlText(IDOK, IDS_NEXT);
			ShowHideConfirmPage(SW_HIDE);
			ShowHidePEMucEditPage(SW_SHOW); 
		}
		break;
	default:
		break;
	}
}

void CNewProfileWizard::DoNext()
{
	CString text;
 
	// PE:
	if(theApp.m_bPEEnabled) 
	{
		if (m_nCurrentPage == ID_PAGE_INTRO && !m_bUpgrade)
			m_nCurrentPage = ID_PEMUC_INTRO;
		else if (m_nCurrentPage == ID_PAGE_INTRO && m_bUpgrade)
			m_nCurrentPage = ID_PAGE_NAME;
		else if (m_nCurrentPage == ID_PEMUC_INTRO)
			m_nCurrentPage = ID_PAGE_NAME;
		else if (m_nCurrentPage == ID_PAGE_PROFILE && m_bASWEnabled && !m_bUpgrade)
			m_nCurrentPage = ID_PEMUC_ASWREADY;   
#ifdef MOZ_MAIL_NEWS 
		else if (m_nCurrentPage == ID_PAGE_PROFILE && (!m_bASWEnabled) && !m_bUpgrade)
			m_nCurrentPage = ID_PAGE_SENDMAIL;
#else  
		else if (m_nCurrentPage == ID_PAGE_PROFILE && (m_bMucEnabled))
			m_nCurrentPage = ID_PEMUC_MUCREADY; 
		else if (m_nCurrentPage == ID_PAGE_PROFILE && (!m_bMucEnabled))
			m_nCurrentPage = ID_PAGE_FINISH; 
#endif /* MOZ_MAIL_NEWS */
		else if (m_nCurrentPage == ID_PAGE_PROFILE && (!m_bASWEnabled) && m_bUpgrade)
			m_nCurrentPage = ID_PEMUC_MUCREADY;
		else if (m_nCurrentPage == ID_PEMUC_ASWREADY)
			m_nCurrentPage = ID_PAGE_FINISH;
		else if (m_nCurrentPage == ID_PEMUC_MUCEDIT && m_bUpgrade)
			m_nCurrentPage = ID_PAGE_CONFIRM;
		else if (m_nCurrentPage == ID_PAGE_CONFIRM)
			m_nCurrentPage = ID_PAGE_FINISH;
		else if (m_nCurrentPage == ID_PEMUC_MUCEDIT&& !m_bUpgrade)
			m_nCurrentPage = ID_PAGE_FINISH;
		else if (m_nCurrentPage == ID_PEMUC_MUCREADY)
			m_nCurrentPage = ID_PEMUC_MUCEDIT;
#ifdef MOZ_MAIL_NEWS
		else if (m_nCurrentPage == ID_PAGE_READNEWS && m_bMucEnabled)
			m_nCurrentPage = ID_PEMUC_MUCREADY;
		else if (m_nCurrentPage == ID_PAGE_READNEWS && (!m_bMucEnabled))
			m_nCurrentPage = ID_PAGE_FINISH;
#endif /* MOZ_MAIL_NEWS */
		else if (m_nCurrentPage == ID_PAGE_FINISH)
			m_nCurrentPage = ID_PAGE_FINISH;
		else
			m_nCurrentPage += 1;
	}
	else
	{
		if (m_nCurrentPage == ID_PAGE_PROFILE) {
			// special magic on this page
			if (m_bUpgrade) m_nCurrentPage = ID_PAGE_CONFIRM;
#ifndef MOZ_MAIL_NEWS 
			else m_nCurrentPage = ID_PAGE_FINISH; 
#else         
			else m_nCurrentPage = ID_PAGE_SENDMAIL; 
#endif /* MOZ_MAIL_NEWS */
		} 
		else if (m_nCurrentPage == ID_PAGE_CONFIRM) 
			// confirm page next is finish
			m_nCurrentPage = ID_PAGE_FINISH;
		else if (m_nCurrentPage == ID_PAGE_FINISH)
			m_nCurrentPage = ID_PAGE_FINISH;
		else
			m_nCurrentPage += 1;
	}

	switch  (m_nCurrentPage)
	{
	case ID_PAGE_NAME:  // 2nd page
		if(theApp.m_bPEEnabled)
			ShowHidePEMucIntroPage(SW_HIDE);
		ShowHideIntroPage(SW_HIDE);
		ShowHideNamePage(SW_SHOW);
		GetDlgItem(IDC_BUTTON_BACK)->EnableWindow(TRUE);
		break;
	case ID_PEMUC_INTRO:  // PE: 2nd page
		ShowHideIntroPage(SW_HIDE);
		ShowHidePEMucIntroPage(SW_SHOW);
		GetDlgItem(IDC_BUTTON_BACK)->EnableWindow(TRUE);
		break;
	case ID_PAGE_PROFILE:   // 3rd page
		ShowHideNamePage(SW_HIDE); 
		ShowHideProfilePage(SW_SHOW);    
#ifndef MOZ_MAIL_NEWS // Is this the correct ifdef??
		if(!m_bUpgrade)
			if(!((theApp.m_bPEEnabled && m_bASWEnabled) || (theApp.m_bPEEnabled && m_bMucEnabled))) 
			{
				SetControlText(IDOK, IDS_FINISH);
				SetControlText(IDC_NAME3, IDS_CLICK_FINISH);
			}				
#endif /* MOZ_MAIL_NEWS */  
		break;
	case ID_PAGE_CONFIRM:   // 4th page
		SetControlText(IDOK, IDS_FINISH);
		ShowHideProfilePage(SW_HIDE); 
		ShowHidePEMucEditPage(SW_HIDE);
		ShowHideConfirmPage(SW_SHOW);
		break;
	case ID_PEMUC_ASWREADY: // PE: 4th page
		SetControlText(IDOK, IDS_FINISH);
		ShowHideProfilePage(SW_HIDE); 
		ShowHidePEMucASWReadyPage(SW_SHOW);
		break;
#ifdef MOZ_MAIL_NEWS
	case ID_PAGE_SENDMAIL:  // other 4th page
		ShowHideProfilePage(SW_HIDE); 
		ShowHideSendPage(SW_SHOW);
		break;
	case ID_PAGE_RECEIVEMAIL:       // 5th page
		ShowHideSendPage(SW_HIDE); 
		ShowHideReceivePage(SW_SHOW);
		break;
	case ID_PAGE_READNEWS:  // 6th page    
		if(!(theApp.m_bPEEnabled && m_bMucEnabled))
			SetControlText(IDOK, IDS_FINISH);
		ShowHideReceivePage(SW_HIDE); 
		ShowHideNewsPage(SW_SHOW);
		break;
#endif /* MOZ_MAIL_NEWS */
	case ID_PEMUC_MUCREADY: // PE: 7th page
		ShowHideNewsPage(SW_HIDE); 
		ShowHidePEMucReadyPage(SW_SHOW);
		break;
	case ID_PEMUC_MUCEDIT:  // PE: 8th page    
		if(!m_bUpgrade)
			SetControlText(IDOK, IDS_FINISH); 
		else
		    SetControlText(IDOK, IDS_NEXT);
		ShowHidePEMucReadyPage(SW_HIDE); 
		ShowHidePEMucEditPage(SW_SHOW);
		break;
	case ID_PAGE_FINISH:    // done, save value
		if (DoFinish())
			EndDialog(IDOK);
		else
			DoBack();
		break;
	default:
		break;
	}
}

void CNewProfileWizard::SetControlText(int nID, int nStringID) 
{
	CString text;
	text.LoadString(nStringID);
	SetDlgItemText(nID, LPCTSTR(text));
}

void CNewProfileWizard::ShowHideIntroPage(int nShowCmd) 
{
	GetDlgItem(IDC_INTRO1)->ShowWindow(nShowCmd);
	if(theApp.m_bPEEnabled)
	{
		CString m_str;
		m_str.LoadString(IDS_PE_INTROPAGE_TEXT);
		SetDlgItemText(IDC_INTRO2, (LPCSTR)m_str);
	}       
	GetDlgItem(IDC_INTRO2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_INTRO3)->ShowWindow(nShowCmd);
}

void CNewProfileWizard::ShowHideNamePage(int nShowCmd)
{
    char name[BUFSZ];

	GetDlgItem(IDC_PROFILE1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_PROFILE2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_PROFILE3)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_PROFILE4)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_USER_NAME)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EMAIL_ADDR)->ShowWindow(nShowCmd);
	if (nShowCmd == SW_HIDE) {
		if (GetDlgItemText(IDC_EMAIL_ADDR, name, BUFSZ)) 
			m_pUserAddr = name;
		if (GetDlgItemText(IDC_USER_NAME, name, BUFSZ))
			m_pFullName = name;
		// setup mail/news wizard parts
		m_szFullName = m_pFullName;
		m_szEmail = m_pUserAddr;
	}

	if (nShowCmd == SW_SHOW) {
		GetDlgItem(IDC_USER_NAME)->SetFocus();
	}
	
	//PE: disable email addr.
	if(theApp.m_bPEEnabled & nShowCmd==SW_SHOW)
	{
		int nCmd;
		CString text;
		if(m_bASWEnabled)
		{
			text.LoadString(IDS_PEMUC_NAMEPAGE_TEXT);
			nCmd = SW_HIDE;
		}
		else
		{
			text.LoadString(IDS_MUP_NAMEPAGE_TEXT);
			nCmd = SW_SHOW;
		} 
		SetDlgItemText(IDC_PROFILE1,(LPCTSTR)text);
		GetDlgItem(IDC_PROFILE4)->ShowWindow(nCmd);
		GetDlgItem(IDC_EMAIL_ADDR)->ShowWindow(nCmd);
	}
}

void CNewProfileWizard::ShowHideProfilePage(int nShowCmd)
{
	GetDlgItem(IDC_NAME1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_NAME2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_NAME3)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_NAME4)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_PROFILE_NAME)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_PROFILE_DIR)->ShowWindow(nShowCmd);

	if (nShowCmd == SW_SHOW) {
		CString csUserDirectory;
		CString csUserAddrShort;
		char buf[4];

		int iAtSign = m_pUserAddr.Find('@');
		
		if (iAtSign != -1) 
			csUserAddrShort = m_pUserAddr.Left(iAtSign);
		else
			csUserAddrShort = m_pUserAddr;

		if (csUserAddrShort.IsEmpty()) 
			csUserAddrShort = "default";

		CUserProfileDB::AssignProfileDirectoryName(csUserAddrShort,csUserDirectory);

		PREF_SetCharPref("mail.pop_name",csUserAddrShort);

		if (!GetDlgItemText(IDC_PROFILE_DIR,buf,4))
			SetDlgItemText(IDC_PROFILE_DIR, csUserDirectory);
		if (!GetDlgItemText(IDC_PROFILE_NAME,buf,4))
			SetDlgItemText(IDC_PROFILE_NAME, csUserAddrShort);
		GetDlgItem(IDC_PROFILE_NAME)->SetFocus();
	}
}

void CNewProfileWizard::ShowHideConfirmPage(int nShowCmd)
{
	GetDlgItem(IDC_CONFIRM1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_MOVEFILES)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_COPYFILES)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_IGNOREFILES)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_CONFIRM2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_CONFIRM3)->ShowWindow(nShowCmd);

	if (!m_bUpgrade) {
		GetDlgItem(IDC_MOVEFILES)->EnableWindow(FALSE);
		GetDlgItem(IDC_COPYFILES)->EnableWindow(FALSE);
		((CButton *)GetDlgItem(IDC_IGNOREFILES))->SetCheck(TRUE);
	}
	else ((CButton *)GetDlgItem(IDC_MOVEFILES))->SetCheck(TRUE);
}

void CNewProfileWizard::ShowHideSendPage(int nShowCmd)
{
	if (nShowCmd == SW_SHOW) {
		SetControlText(IDC_STATIC_TITLE, IDS_WIZARD_SENDMAIL);
		SetControlText(IDC_STATIC1, IDS_SENDMAIL_STATIC1);
		SetControlText(IDC_STATIC_EG1, IDS_SENDMAIL_EG1);
		SetControlText(IDC_STATIC2, IDS_SENDMAIL_STATIC2);
		SetControlText(IDC_STATIC_EG2, IDS_SENDMAIL_EG2);
		SetControlText(IDC_STATIC3, IDS_SENDMAIL_STATIC3);
		SetControlText(IDC_STATIC5, IDS_SENDREADMAIL_STATIC5);
	}
	GetDlgItem(IDC_EDIT1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_EG1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EDIT2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_EG2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC3)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EDIT3)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC5)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_TITLE)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC1)->ShowWindow(nShowCmd);
	if (nShowCmd == SW_SHOW)
	{       // init value
		SetDlgItemText(IDC_EDIT1, m_szFullName);
		SetDlgItemText(IDC_EDIT2, m_szEmail);   
		SetDlgItemText(IDC_EDIT3, m_szMailServer);
		GetDlgItem(IDC_EDIT1)->SetFocus();
	}
	else
	{       // save value
		char text[BUFSZ];
		if (GetDlgItemText(IDC_EDIT1, text, BUFSZ)) 
			m_szFullName = text;
		else 
			m_szFullName = "";
		if (GetDlgItemText(IDC_EDIT2, text, BUFSZ))  
			m_szEmail = text;
		else
			m_szEmail = "";
		if (GetDlgItemText(IDC_EDIT3, text, BUFSZ))  
			m_szMailServer = text;
		else 
			m_szMailServer = "";
	}
}

void CNewProfileWizard::ShowHideReceivePage(int nShowCmd)
{
	if (nShowCmd == SW_SHOW) {
		SetControlText(IDC_STATIC_TITLE, IDS_WIZARD_READMAIL);
		SetControlText(IDC_STATIC1, IDS_READMAIL_STATIC1);
		SetControlText(IDC_STATIC_EG1, IDS_READMAIL_EG1);
		SetControlText(IDC_STATIC2, IDS_READMAIL_STATIC2);
		SetControlText(IDC_STATIC3, IDS_READMAIL_STATIC3);
		SetControlText(IDC_STATIC5, IDS_SENDREADMAIL_STATIC5);
	}
	GetDlgItem(IDC_EDIT1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_EG1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EDIT2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC3)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_READMAIL_POP1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_READMAIL_POP2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_READMAIL_IMAP)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC5)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_TITLE)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC1)->ShowWindow(nShowCmd);
	if (nShowCmd == SW_SHOW)
	{       // init value
		SetDlgItemText(IDC_EDIT1, m_szPopName);
		SetDlgItemText(IDC_EDIT2, m_szInMailServer);
		if (m_bUseIMAP)
			((CButton*)GetDlgItem(IDC_READMAIL_IMAP))->SetCheck(TRUE);
		else if (m_bLeftOnServer)
			((CButton*)GetDlgItem(IDC_READMAIL_POP2))->SetCheck(TRUE);
		else
			((CButton*)GetDlgItem(IDC_READMAIL_POP1))->SetCheck(TRUE);
		GetDlgItem(IDC_EDIT1)->SetFocus();
	}
	else
	{       // save value
		char text[BUFSZ];

		m_bUseIMAP = m_bLeftOnServer = FALSE;
		if (GetDlgItemText(IDC_EDIT1, text, BUFSZ))  
			m_szPopName = text;
		else 
			m_szPopName = "";
		if (GetDlgItemText(IDC_EDIT2, text, BUFSZ)) 
			m_szInMailServer = text;
		else
			m_szInMailServer = "";
		if (IsDlgButtonChecked(IDC_READMAIL_IMAP)) 
			m_bUseIMAP = TRUE;
		else if (IsDlgButtonChecked(IDC_READMAIL_POP2))  
			m_bLeftOnServer = TRUE;
	}
}

void CNewProfileWizard::ShowHideNewsPage(int nShowCmd)
{
	if (nShowCmd == SW_SHOW) {
		SetControlText(IDC_STATIC_TITLE, IDS_WIZARD_READNEWS);
		SetControlText(IDC_STATIC1, IDS_READNEWS_STATIC1);
		SetControlText(IDC_STATIC2, IDS_READNEWS_STATIC2);
		if(theApp.m_bPEEnabled && m_bMucEnabled)
			SetControlText(IDC_STATIC5, IDS_PEMUC_NEWSPAGE);
		else
			SetControlText(IDC_STATIC5, IDS_READNEWS_STATIC5);
	}
	GetDlgItem(IDC_EDIT1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC5)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_TITLE)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EDIT2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_SECURE)->ShowWindow(nShowCmd);
	if (nShowCmd == SW_SHOW)
	{       // init value
		char szPort[16];
		SetDlgItemText(IDC_EDIT1, m_szNewsServer); 
		sprintf(szPort, "%ld", m_nPort);
		SetDlgItemText(IDC_EDIT2, szPort);
		if (m_bIsSecure)
			((CButton*)GetDlgItem(IDC_SECURE))->SetCheck(TRUE);
		else
			((CButton*)GetDlgItem(IDC_SECURE))->SetCheck(FALSE);
		GetDlgItem(IDC_EDIT1)->SetFocus();
	}
	else
	{       // save value
		char text[BUFSZ];
		if(GetDlgItemText(IDC_EDIT1, text, BUFSZ)) 
			m_szNewsServer = text;
		else 
			m_szNewsServer = "";

		if (IsDlgButtonChecked(IDC_SECURE)) 
			m_bIsSecure = TRUE;
		else
			m_bIsSecure = FALSE;
		if (GetDlgItemText(IDC_EDIT2, text, BUFSZ))
			m_nPort = atol(text);
		else
		{
			if (m_bIsSecure)
				m_nPort = SECURE_NEWS_PORT;
			else
				m_nPort = NEWS_PORT;
		}
	}
}

void CNewProfileWizard::ShowHidePEMucIntroPage(int nShowCmd)
{
	if(theApp.m_bPEEnabled && m_pMucIntroPage 
			&& ::IsWindow(m_pMucIntroPage->GetSafeHwnd()))
		m_pMucIntroPage->ShowWindow(nShowCmd);    
	if(nShowCmd == SW_SHOW)
		SetFocus();
}

void CNewProfileWizard::ShowHidePEMucASWReadyPage(int nShowCmd)
{
	if(theApp.m_bPEEnabled && m_pASWReadyPage 
			&& ::IsWindow(m_pASWReadyPage->GetSafeHwnd()))
		m_pASWReadyPage->ShowWindow(nShowCmd);
	if(nShowCmd == SW_SHOW)
		SetFocus();
}

void CNewProfileWizard::ShowHidePEMucReadyPage(int nShowCmd)
{
	if(theApp.m_bPEEnabled && m_pMucReadyPage 
			&& ::IsWindow(m_pMucReadyPage->GetSafeHwnd()))
		m_pMucReadyPage->ShowWindow(nShowCmd);
	if(nShowCmd == SW_SHOW)
		SetFocus();
}

void CNewProfileWizard::ShowHidePEMucEditPage(int nShowCmd)
{ 
	CString m_str;

	if(theApp.m_bPEEnabled && m_pMucEditPage 
			&& ::IsWindow(m_pMucEditPage->GetSafeHwnd()))
		m_pMucEditPage->ShowWindow(nShowCmd);
	if(nShowCmd == SW_SHOW)
	{
		if(m_bUpgrade)
			m_str.LoadString(IDS_PEMUCEDIT_UPGRADE);
		else
			m_str.LoadString(IDS_PEMUCEDIT_NORMAL);
		(m_pMucEditPage->GetDlgItem(IDC_EDIT_TEXT))->SetWindowText(m_str);
		SetFocus();
	}
}

void CNewProfileWizard::OnMove(int x, int y)
{
	CDialog::OnMove(x, y);

	if(theApp.m_bPEEnabled && m_pMucIntroPage && m_pASWReadyPage 
							&& m_pMucReadyPage && m_pMucEditPage)
	{   
		int     nShowCmd;
		
		if (m_nCurrentPage == ID_PEMUC_INTRO)   nShowCmd = SW_SHOW;
		else    nShowCmd = SW_HIDE;
		m_pMucIntroPage->SetMove(x,y, nShowCmd);  
		
		if (m_nCurrentPage == ID_PEMUC_ASWREADY)        nShowCmd = SW_SHOW;
		else    nShowCmd = SW_HIDE;
		m_pASWReadyPage->SetMove(x,y,nShowCmd);
			
		if (m_nCurrentPage == ID_PEMUC_MUCREADY)        nShowCmd = SW_SHOW;
		else    nShowCmd = SW_HIDE;
		m_pMucReadyPage->SetMove(x,y,nShowCmd);
		
		if (m_nCurrentPage == ID_PEMUC_MUCEDIT) nShowCmd = SW_SHOW;
		else    nShowCmd = SW_HIDE;
		m_pMucEditPage->SetMove(x,y,nShowCmd);  
	} 
}

// this internal function does not attempt to insure that every aspect of the
// path is valid.  If they enter an invalid character for example, it will show
// up when we attempt to create the directory.  This simply insures that there
// are no parts of the path greater than 8 characters...Since mkdir simply truncates
// this, it works but we are then screwed because we have the wrong pointer...
BOOL checkValidWin16Path(char *path) 
{
	BOOL bRet = TRUE;
	char * slash1 = path;  // start of path
	char * slash2 = strchr(path,'\\');
	
	if (!slash2) bRet = FALSE;  // need at least 1 slash!

	while (slash2) {
		// the difference between any 2 slashes can't be greater than 8
		if ((slash2 - slash1) > 9)
			bRet = FALSE;
		if (slash2+1) {
			slash1 = slash2;
			slash2 = strchr(slash2+1,'\\');
		} else slash2 = NULL;
	}
	// what is on the end can't be too large either
	if (strlen(slash1) > 9)  // 8 + slash
		bRet = FALSE;
	return bRet;    
}

BOOL CNewProfileWizard::DoFinish()
{
	if (!m_pFullName.IsEmpty())
		PREF_SetCharPref("mail.identity.username",m_pFullName);
	if (!m_pUserAddr.IsEmpty())
		PREF_SetCharPref("mail.identity.useremail",m_pUserAddr);

    char path[BUFSZ],name[BUFSZ];
	int ret;
	XP_StatStruct statinfo; 

    if (!GetDlgItemText(IDC_PROFILE_NAME, name, BUFSZ)) {
		AfxMessageBox(szLoadString(IDS_INVALID_PROFILE_NAME),MB_OK);
		return FALSE;
	}

    if (GetDlgItemText(IDC_PROFILE_DIR, path, BUFSZ)) { 
		if (path[strlen(path)-1] == '\\') path[strlen(path)-1] = NULL;  // remove last slash...
		
		if (!checkValidWin16Path(path)) {
			AfxMessageBox(szLoadString(IDS_INVALID_WIN16_DIR)); 
			return FALSE;
		}
		
		ret = _stat((char *) path, &statinfo);
		if (!ret) {
			// Directory already exists!
			if (AfxMessageBox(szLoadString(IDS_PROFDIR_EXISTS),MB_OKCANCEL) == IDCANCEL)
				return FALSE;
			else 
				m_bExistingDir = TRUE;
		}
		if(ret == -1) {
			char * slash = strchr(path,'\\');
			while (slash) {
				slash[0] = NULL;
				ret = _mkdir(path);
				slash[0] = '\\';
				if (slash+1) slash = strchr(slash+1,'\\');
			}
			ret = _mkdir(path);
			if (ret == -1) {
				AfxMessageBox(szLoadString(IDS_UNABLE_CREATE_DIR),MB_OK);
				return FALSE;
			}
		}
		// else "Warn already exists"
	} else {
		AfxMessageBox(szLoadString(IDS_INVALID_PROFILE_NAME),MB_OK);
		return FALSE;
	}

	
    if (GetDlgItemText(IDC_PROFILE_NAME, name, BUFSZ)) 
		login_CreateNewUserKey(name,path);
	
	m_pProfileName = name;
	m_pProfilePath = path;

	// PE finish - 
	if (theApp.m_bPEEnabled)
	{
		// asw thread
		if(m_bASWEnabled && m_pASWReadyPage && ::IsWindow(m_pASWReadyPage->GetSafeHwnd()))
			m_pASWReadyPage->DoFinish();

		// dialer cfg thread
		else if (m_bMucEnabled && m_pMucEditPage && ::IsWindow(m_pMucEditPage->GetSafeHwnd()))
					m_pMucEditPage->DoFinish();

		// network thread   
		else
		{
			CMucProc        m_mucProc;
			m_mucProc.SetDialOnDemand("",FALSE);
		}
	}

	CButton * pMove = (CButton *) GetDlgItem(IDC_MOVEFILES);
	CButton * pCopy = (CButton *) GetDlgItem(IDC_COPYFILES);
	CButton * pIgnore = (CButton *) GetDlgItem(IDC_IGNOREFILES);
	int iMove = TRUE;
	int iCopy = FALSE;
	int iIgnore = FALSE;

	if (m_bUpgrade) {
		if (pMove)
			iMove = pMove->GetCheck();
		if (pCopy)
			iCopy = pCopy->GetCheck();
		if (pIgnore)
			iIgnore = pIgnore->GetCheck();
	} else {
		iIgnore = TRUE;
		iMove = FALSE;
		iCopy = FALSE;
	}

	if (iMove) {
		login_UpdateFilesToNewLocation(m_pProfilePath,this,FALSE);  // move files
		login_UpdatePreferencesToJavaScript(m_pProfilePath); // upgrade prefs
	} else if (iCopy) {
		login_UpdateFilesToNewLocation(m_pProfilePath,this,TRUE);  // Copy files
		login_UpdatePreferencesToJavaScript(m_pProfilePath); // upgrade prefs
	} else {
		// just create the directories --
		login_CreateEmptyProfileDir(m_pProfilePath, this, m_bExistingDir); 
	}

	if (!m_bUpgrade) {
#ifdef MOZ_MAIL_NEWS
		PREF_SetCharPref("mail.identity.username", m_szFullName);
		PREF_SetCharPref("mail.identity.useremail", m_szEmail);
		PREF_SetCharPref("network.hosts.smtp_server", m_szMailServer);

		PREF_SetCharPref("network.hosts.pop_server", m_szInMailServer);

		PREF_SetCharPref("mail.pop_name", m_szPopName);

		if (m_bUseIMAP) 
		{
			PREF_SetBoolPref("mail.leave_on_server", m_bUseIMAP);
		}
		else
		{
			PREF_SetBoolPref("mail.leave_on_server", m_bLeftOnServer);
		}
		long imapPref = m_bUseIMAP ? MSG_Imap4 : MSG_Pop3;
		PREF_SetIntPref("mail.server_type", imapPref);

		char szPort[10];
		int  nPortLen = 0;
		int	 nNewsServerLen = 0;

		if (nNewsServerLen = GetDlgItemText(IDC_EDIT1, name, BUFSZ)) 
			m_szNewsServer = name;
		else 
			m_szNewsServer = "";

		if (IsDlgButtonChecked(IDC_SECURE)) 
			m_bIsSecure = TRUE;
		else
			m_bIsSecure = FALSE;
		if (nPortLen = GetDlgItemText(IDC_EDIT2, szPort, 10) > 0)
			m_nPort = atoi(szPort);
		else
		{
			if (m_bIsSecure)
				m_nPort = SECURE_NEWS_PORT;
			else
				m_nPort = NEWS_PORT;
		}

		if (nNewsServerLen && nPortLen)
		{
			if (m_nPort < 0 || m_nPort> MAX_PORT_NUMBER)
			{
				AfxMessageBox(IDS_PORT_RANGE);
				((CEdit*)GetDlgItem(IDC_EDIT2))->SetFocus();
				((CEdit*)GetDlgItem(IDC_EDIT2))->SetSel((DWORD)MAKELONG(0, -1));
				return FALSE;	    
			}
			if (!::IsNumeric(szPort))
			{
				AfxMessageBox(IDS_NUMBERS_ONLY);
				((CEdit*)GetDlgItem(IDC_EDIT2))->SetFocus();
				((CEdit*)GetDlgItem(IDC_EDIT2))->SetSel((DWORD)MAKELONG(0, -1));
				return FALSE;
			}
		}

		PREF_SetCharPref("network.hosts.nntp_server", LPCTSTR(m_szNewsServer));

		if (nNewsServerLen)
		{
			PREF_SetBoolPref("news.server_is_secure", m_bIsSecure);
			PREF_SetIntPref("news.server_port", (int32)m_nPort);
		}
		else
		{
			PREF_SetBoolPref("news.server_is_secure", FALSE);
			PREF_SetIntPref("news.server_port", (int32)NEWS_PORT);
		}
#endif // MOZ_MAIL_NEWS

		PREF_SavePrefFile();
	}
	return TRUE;
}

void CNewProfileWizard::OnCheckSecure()
{
	char port[16];
	BOOL bIsSecure;

	if (IsDlgButtonChecked(IDC_SECURE))
		bIsSecure = TRUE;
	else
		bIsSecure = FALSE;
	if (GetDlgItemText(IDC_EDIT2, port, 16) == 0)
	{
		if (bIsSecure)
			SetDlgItemInt(IDC_EDIT2, SECURE_NEWS_PORT);
		else
			SetDlgItemInt(IDC_EDIT2, NEWS_PORT);
	}
	else
	{
		int32 lPort = atol(port);

		if (bIsSecure && lPort == NEWS_PORT)
			SetDlgItemInt(IDC_EDIT2, SECURE_NEWS_PORT);
		else  if (!bIsSecure && lPort == SECURE_NEWS_PORT)
			SetDlgItemInt(IDC_EDIT2, NEWS_PORT);
	}
}

BEGIN_MESSAGE_MAP(CNewProfileWizard, CDialog)
    ON_BN_CLICKED(IDC_BUTTON_BACK, DoBack)
    ON_BN_CLICKED(IDOK, DoNext)
    ON_BN_CLICKED(IDC_SECURE, OnCheckSecure)
	ON_WM_MOVE()
END_MESSAGE_MAP()

#endif XPWIN_32
