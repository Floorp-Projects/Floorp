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

#include "mnwizard.h"
#include "mnprefs.h"
#include "prefapi.h"
#include "msgcom.h"
#include "wfemsg.h"

#define BUFSZ 1000

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" BOOL IsNumeric(char* pStr);

extern "C" BOOL EmailValid(char* pEmail)
{
	if (strlen(pEmail))
	{
		char* pAddress = pEmail;
		while (*pAddress != '\0')
		{
			if (*pAddress == '@' && *(++pAddress) != '\0')
				return TRUE;
			pAddress++;
		}
		return FALSE;
	}
	else
		return FALSE;
}

void GetImapServerName(char* pServerName)
{
	char serverStr[512];
	int nLen = 511;
	if (PREF_NOERROR == PREF_GetCharPref("network.hosts.imap_servers", serverStr, &nLen))
	{
		nLen = strlen(serverStr);
		int j = 0;
		for (int i = 0; i <= nLen; i++)
		{
			if (serverStr[i] != ',' && serverStr[i] != '\0')
				pServerName[j++] = serverStr[i];
			else
			{	
				pServerName[j] = '\0';
				break;
			}
		}
	}
}

void GetImapUserName(char* pServerName, char* pUserName)
{
	if (strlen(pServerName) > 0)
	{
		char* pName = NULL;
		IMAP_GetCharPref(pServerName, CHAR_USERNAME, &pName);
		if (pName)
		{
			strcat(pUserName, pName);
			XP_FREE(pName);
		}
	}
}

void SetImapServerName(const char* pServerName)
{
	char *pServerList = NULL;
	PREF_CopyCharPref("network.hosts.imap_servers", &pServerList);

	if (pServerList)
	{
		int nListLen = XP_STRLEN(pServerList) + strlen(pServerName) + 1;
		char *pNewList = nListLen ? (char *) XP_ALLOC(nListLen) : 0;
		if (pNewList)
		{
			XP_STRCAT(pNewList, ",");
			XP_STRCAT(pNewList, pServerName);
			PREF_SetCharPref("network.hosts.imap_servers", pNewList);
			XP_FREE(pNewList);
		}
		XP_FREE(pServerList);
	}
	else
		PREF_SetCharPref("network.hosts.imap_servers", pServerName);

}

#ifdef _WIN32
/////////////////////////////////////////////////////////////////////////////
// CCoverPage

CCoverPage::CCoverPage(CWnd *pParent)
	: CNetscapePropertyPage(IDD)
{
    //{{AFX_DATA_INIT(CCoverPage)
    //}}AFX_DATA_INIT
	m_pParent = (CMailNewsWizard*)pParent;
}

BOOL CCoverPage::OnInitDialog()
{   
	CString text;
	text.LoadString(IDS_WIZARD_COVER);
	SetDlgItemText(IDC_STATIC_TITLE, LPCTSTR(text));
	return CNetscapePropertyPage::OnInitDialog();
}

BOOL CCoverPage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_NEXT);
    return CNetscapePropertyPage::OnSetActive();
}

void CCoverPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CCoverPage, CNetscapePropertyPage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSendMailPage

CSendMailPage::CSendMailPage(CWnd *pParent, BOOL bVerify)
	: CNetscapePropertyPage(IDD)
{
    //{{AFX_DATA_INIT(CSendMailPage)
    //}}AFX_DATA_INIT
	m_pParent = (CMailNewsWizard*)pParent;
	m_bVerify = bVerify;   //No verification on profile wizard
}

BOOL CSendMailPage::OnInitDialog()
{
	BOOL ret;
	ret = CNetscapePropertyPage::OnInitDialog();

	CString text;
	text.LoadString(IDS_WIZARD_SENDMAIL);
	SetDlgItemText(IDC_STATIC_TITLE, LPCTSTR(text));

	char buffer[256];
	int nLen = 255;

	if (PREF_NOERROR == PREF_GetCharPref("mail.identity.username", buffer, &nLen))
        SetDlgItemText(IDC_EDIT_NAME, buffer);
	if (PREF_NOERROR == PREF_GetCharPref("mail.identity.useremail", buffer, &nLen))
        SetDlgItemText(IDC_EDIT_ADDRESS, buffer);
	if (PREF_NOERROR == PREF_GetCharPref("network.hosts.smtp_server", buffer, &nLen))
		SetDlgItemText(IDC_EDIT_SMTP_SERVER, buffer);
	return ret;
}

BOOL CSendMailPage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_NEXT | PSWIZB_BACK);
    return CNetscapePropertyPage::OnSetActive();
}

LRESULT CSendMailPage::OnWizardNext()
{
	if (!m_bVerify)
		return 0;

    char text[BUFSZ];
	text[0] = '\0';
    int nLen = GetDlgItemText(IDC_EDIT_ADDRESS, text, BUFSZ); 
	if (nLen)
	{
		text[nLen] = '\0';
		if (EmailValid(text))
			return 0;
		else
		{
			AfxMessageBox(IDS_INVALID_EMAIL);
			GetDlgItem(IDC_EDIT_ADDRESS)->SetFocus();
			return -1;
		}
	}
	return 0;
}

void CSendMailPage::DoFinish()
{
    char text[BUFSZ];
	text[0] = '\0';
    GetDlgItemText(IDC_EDIT_NAME, text, BUFSZ);
	PREF_SetCharPref("mail.identity.username", text);
	
	text[0] = '\0';
    GetDlgItemText(IDC_EDIT_ADDRESS, text, BUFSZ);  
	PREF_SetCharPref("mail.identity.useremail", text);

	text[0] = '\0';
    GetDlgItemText(IDC_EDIT_SMTP_SERVER, text, BUFSZ); 
	PREF_SetCharPref("network.hosts.smtp_server", text);
}

void CSendMailPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSendMailPage, CNetscapePropertyPage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CReceiveMailPage

CReceiveMailPage::CReceiveMailPage(CWnd *pParent, BOOL bVerify)
	: CNetscapePropertyPage(IDD)
{
    //{{AFX_DATA_INIT(CReceiveMailPage)
    //}}AFX_DATA_INIT
	m_pParent = (CMailNewsWizard*)pParent;
	m_bVerify = bVerify;   //No verification on profile wizard
}

BOOL CReceiveMailPage::OnInitDialog()
{
	BOOL ret;
	ret = CNetscapePropertyPage::OnInitDialog();

	CString text;
	text.LoadString(IDS_WIZARD_READMAIL);
	SetDlgItemText(IDC_STATIC_TITLE, LPCTSTR(text));

	char buffer[256];
	int nLen = 255;

	long prefLong = MSG_Pop3;
	PREF_GetIntPref("mail.server_type",&prefLong);
	if (prefLong == MSG_Imap4)
	{	// imap server
		CheckDlgButton(IDC_RADIO_IMAP, TRUE);

		char serverName[128];
		GetImapServerName(serverName);

		if (strlen(serverName) > 0)
		{
			char userName[128];
			SetDlgItemText(IDC_EDIT_MAIL_SERVER, serverName);
			GetImapUserName(serverName, userName);
			if (strlen(userName) > 0)
				SetDlgItemText(IDC_EDIT_USER_NAME, userName);
		}
	}
	else
	{	// pop server
		CheckDlgButton(IDC_RADIO_POP, TRUE);
		if (PREF_NOERROR == PREF_GetCharPref("mail.pop_name", buffer, &nLen))
			SetDlgItemText(IDC_EDIT_USER_NAME, buffer);
		if (PREF_NOERROR == PREF_GetCharPref("network.hosts.pop_server", buffer, &nLen))
		{
			int nameLen = strlen(buffer);
			if (nameLen)
				SetDlgItemText(IDC_EDIT_MAIL_SERVER, buffer);
		}
	}
	if (!GetDlgItemText(IDC_EDIT_MAIL_SERVER, buffer, 255))
	{
		if (PREF_NOERROR == PREF_GetCharPref("network.hosts.smtp_server", buffer, &nLen))
			SetDlgItemText(IDC_EDIT_MAIL_SERVER, buffer);
	}
	return ret;
}

BOOL CReceiveMailPage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_NEXT | PSWIZB_BACK);
    return CNetscapePropertyPage::OnSetActive();
}

LRESULT CReceiveMailPage::OnWizardNext()
{
	if (!m_bVerify)
		return 0;

    char text[BUFSZ];
	text[0] = '\0';
    int nLen = GetDlgItemText(IDC_EDIT_USER_NAME, text, BUFSZ); 
	
	if (nLen)
	{
		text[0] = '\0';
		nLen = GetDlgItemText(IDC_EDIT_MAIL_SERVER, text, BUFSZ); 
		if (nLen)
			return 0;
		else
		{
			AfxMessageBox(IDS_EMPTY_STRING);
			GetDlgItem(IDC_EDIT_MAIL_SERVER)->SetFocus();
			return -1;
		}
	}
	else
	{
		AfxMessageBox(IDS_EMPTY_STRING);
		GetDlgItem(IDC_EDIT_USER_NAME)->SetFocus();
		return -1;
	}
}

void CReceiveMailPage::DoFinish()
{
    char text[BUFSZ];
	text[0] = '\0';
    GetDlgItemText(IDC_EDIT_USER_NAME, text, BUFSZ);
	PREF_SetCharPref("mail.pop_name", text);
	
	text[0] = '\0';
    GetDlgItemText(IDC_EDIT_MAIL_SERVER, text, BUFSZ);  
	PREF_SetCharPref("network.hosts.pop_server", text);

	if (IsDlgButtonChecked(IDC_RADIO_IMAP)) 
	{	// IMAP server
		PREF_SetIntPref("mail.server_type", (int32)MSG_Imap4);

		char server[BUFSZ];
		server[0] = '\0';
		GetDlgItemText(IDC_EDIT_MAIL_SERVER, server, BUFSZ);  
		SetImapServerName(server);

		text[0] = '\0';
		GetDlgItemText(IDC_EDIT_USER_NAME, text, BUFSZ);
		IMAP_SetCharPref(server, CHAR_USERNAME, text);
		
	}
	else
	{	// pop server
		PREF_SetIntPref("mail.server_type", (int32)MSG_Pop3);

		GetDlgItemText(IDC_EDIT_USER_NAME, text, BUFSZ);
		PREF_SetCharPref("mail.pop_name", text);
		
		text[0] = '\0';
		GetDlgItemText(IDC_EDIT_MAIL_SERVER, text, BUFSZ);  
		PREF_SetCharPref("network.hosts.pop_server", text);
	}
}

void CReceiveMailPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CReceiveMailPage, CNetscapePropertyPage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CReadNewsPage

CReadNewsPage::CReadNewsPage(CWnd *pParent)
	: CNetscapePropertyPage(IDD)
{
    //{{AFX_DATA_INIT(CReadNewsPage)
    //}}AFX_DATA_INIT
	m_pParent = (CMailNewsWizard*)pParent;
	m_bPEFinish = FALSE;   
}

CReadNewsPage::~CReadNewsPage()
{
}

BOOL CReadNewsPage::OnInitDialog()
{
	BOOL ret;
	ret = CNetscapePropertyPage::OnInitDialog();

	CString text;
	text.LoadString(IDS_WIZARD_READNEWS);
	SetDlgItemText(IDC_STATIC_TITLE, LPCTSTR(text));

	char buffer[256];
	int nLen = 255;

	if (PREF_NOERROR == PREF_GetCharPref("network.hosts.nntp_server", buffer, &nLen))
        SetDlgItemText(IDC_EDIT_NEWS_SERVER, buffer);
	((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetLimitText(5);
	if (strlen(buffer) > 0)
	{
		XP_Bool bSecure;
		int32	lPort;
		char	szPort[16];

		PREF_GetBoolPref("news.server_is_secure", &bSecure);
		CheckDlgButton(IDC_SECURE, bSecure);
		PREF_GetIntPref("news.server_port", &lPort);
		sprintf(szPort, "%ld", lPort);
		SetDlgItemText(IDC_EDIT_PORT, szPort);
	}
	else
	{
		SetDlgItemInt(IDC_EDIT_PORT, NEWS_PORT);
	}
	if(theApp.m_bPEEnabled && !m_bPEFinish)
	{
		text.LoadString(IDS_PEMUC_READNEWS_FINISH);
		GetDlgItem(IDC_MNWIZ_READNEWS_FINISH)->SetWindowText(text);
	}

	return ret;
}

BOOL CReadNewsPage::OnSetActive()
{
	if(theApp.m_bPEEnabled && (!m_bPEFinish))
		m_pParent->SetWizardButtons(PSWIZB_NEXT | PSWIZB_BACK);
	else
		m_pParent->SetWizardButtons(PSWIZB_FINISH | PSWIZB_BACK);
    return CNetscapePropertyPage::OnSetActive();
}

BOOL CReadNewsPage::DoFinish()
{
	char	szHostName[MSG_MAXGROUPNAMELENGTH];
	XP_Bool	bIsSecure = FALSE;
	int32	lnPort;
	char	port[16];

	szHostName[0] = '\0';
    int nLen = GetDlgItemText(IDC_EDIT_NEWS_SERVER, szHostName, 
		                       MSG_MAXGROUPNAMELENGTH);
	PREF_SetCharPref("network.hosts.nntp_server", szHostName);

	if (nLen == 0)
	{
		PREF_SetBoolPref("news.server_is_secure", FALSE);
		PREF_SetIntPref("news.server_port", NEWS_PORT);
		return TRUE;
	}

	if (IsDlgButtonChecked(IDC_SECURE))	
		bIsSecure = TRUE;
	if (GetDlgItemText(IDC_EDIT_PORT, port, 16))
	{
		int32 lPort = atol(port);
		if (lPort < 0 || lPort> MAX_PORT_NUMBER)
		{
			AfxMessageBox(IDS_PORT_RANGE);
			((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;	    
		}
		if (!::IsNumeric(port))
		{
			AfxMessageBox(IDS_NUMBERS_ONLY);
			((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;
		}
		lnPort = lPort;
	}
	else
	{
		if (bIsSecure)
			lnPort = SECURE_NEWS_PORT;
		else
			lnPort = NEWS_PORT;
	}

	PREF_SetBoolPref("news.server_is_secure", bIsSecure);
	PREF_SetIntPref("news.server_port", lnPort);
	return TRUE;
}

void CReadNewsPage::SetFinish(BOOL nFinish)
{
	m_bPEFinish = nFinish;
	if(theApp.m_bPEEnabled 	&& ::IsWindow(this->GetSafeHwnd()))
	{
		CString	text;
		if(!m_bPEFinish )
			text.LoadString(IDS_PEMUC_READNEWS_FINISH);
		else 
			text.LoadString(IDS_CLICK_FINISH);

		GetDlgItem(IDC_MNWIZ_READNEWS_FINISH)->SetWindowText(text);
	}
}

void CReadNewsPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CReadNewsPage::OnCheckSecure() 
{
	char port[16];
	BOOL bIsSecure;

	if (IsDlgButtonChecked(IDC_SECURE))
		bIsSecure = TRUE;
	else
		bIsSecure = FALSE;
	if (GetDlgItemText(IDC_EDIT_PORT, port, 16) == 0)
	{
		if (bIsSecure)
			SetDlgItemInt(IDC_EDIT_PORT, SECURE_NEWS_PORT);
		else
			SetDlgItemInt(IDC_EDIT_PORT, NEWS_PORT);
	}
	else
	{
		int32 lPort = atol(port);
		if (bIsSecure && lPort == NEWS_PORT)
			SetDlgItemInt(IDC_EDIT_PORT, SECURE_NEWS_PORT);
		else  if (!bIsSecure && lPort == SECURE_NEWS_PORT)
			SetDlgItemInt(IDC_EDIT_PORT, NEWS_PORT);
	}
}

BEGIN_MESSAGE_MAP(CReadNewsPage, CNetscapePropertyPage)
	ON_BN_CLICKED(IDC_SECURE, OnCheckSecure)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMailNewsWizard		   
CMailNewsWizard::CMailNewsWizard(CWnd *pParent)
	: CNetscapePropertySheet("", pParent)
{
	m_pCoverPage = new CCoverPage(this);
	m_pSendMailPage = new CSendMailPage(this, TRUE); 
	m_pReceiveMailPage = new CReceiveMailPage(this);
	m_pReadNewsPage = new CReadNewsPage(this);

	AddPage(m_pCoverPage);	  
	AddPage(m_pSendMailPage);
	AddPage(m_pReceiveMailPage);
	AddPage(m_pReadNewsPage);
	SetWizardMode();
}

CMailNewsWizard::~CMailNewsWizard()
{
	if (m_pCoverPage)
		delete m_pCoverPage;
	if (m_pSendMailPage)
		delete m_pSendMailPage;
	if (m_pReceiveMailPage)
		delete m_pReceiveMailPage;   
	if (m_pReadNewsPage)
		delete m_pReadNewsPage;   
}

BOOL CMailNewsWizard::OnInitDialog()
{
	BOOL ret = CNetscapePropertySheet::OnInitDialog();

	GetDlgItem(IDHELP)->ShowWindow(SW_HIDE);

	return ret;
}

void CMailNewsWizard::DoFinish()
{
	BOOL bFinished = TRUE;
	if (m_pSendMailPage && ::IsWindow(m_pSendMailPage->GetSafeHwnd()))
		m_pSendMailPage->DoFinish();
	if (m_pReceiveMailPage && ::IsWindow(m_pReceiveMailPage->GetSafeHwnd()))
		m_pReceiveMailPage->DoFinish();   
	if (m_pReadNewsPage && ::IsWindow(m_pReadNewsPage->GetSafeHwnd()))
		bFinished = m_pReadNewsPage->DoFinish(); 
	if (bFinished)
	{
		PressButton(PSBTN_FINISH);
		PREF_SavePrefFile();
	}
}

BEGIN_MESSAGE_MAP(CMailNewsWizard, CNetscapePropertySheet)
    ON_BN_CLICKED(ID_WIZFINISH, DoFinish)
END_MESSAGE_MAP()

#else  //Win16 Code 

/////////////////////////////////////////////////////////////////////////////
// CMailNewsWizard		   
CMailNewsWizard::CMailNewsWizard(CWnd *pParent)
	: CDialog(IDD, pParent)
{
	m_nCurrentPage = ID_PAGE_COVER;

	char* pPrefStr;

	pPrefStr = NULL;
	PREF_CopyCharPref("mail.identity.username", &pPrefStr);
	m_szFullName = pPrefStr;
	if (pPrefStr) XP_FREE(pPrefStr); 
	
	pPrefStr = NULL;
	PREF_CopyCharPref("mail.identity.useremail", &pPrefStr);
	m_szEmail = pPrefStr;
	if (pPrefStr) XP_FREE(pPrefStr); 

	pPrefStr = NULL;
	PREF_CopyCharPref("network.hosts.smtp_server", &pPrefStr);
	m_szMailServer = pPrefStr;
	if (pPrefStr) XP_FREE(pPrefStr); 

	pPrefStr = NULL;
	PREF_CopyCharPref("mail.pop_name", &pPrefStr);
	m_szPopName = pPrefStr;
	if (pPrefStr) XP_FREE(pPrefStr); 

	pPrefStr = NULL;
	PREF_CopyCharPref("network.hosts.pop_server", &pPrefStr);
	m_szInMailServer = pPrefStr;
	if (pPrefStr) XP_FREE(pPrefStr); 

	long prefLong = MSG_Pop3;
	PREF_GetIntPref("mail.server_type",&prefLong);
	m_bUseIMAP = prefLong == MSG_Imap4;

	if (m_bUseIMAP)
	{
		char serverName[128];
		GetImapServerName(serverName);
		m_szPopName = serverName;
		if (strlen(serverName) > 0)
		{
			char userName[128];
			GetImapUserName(serverName, userName);
			if (strlen(userName) > 0)
				m_szInMailServer = userName;
		}
	}
	else
	{   //POP server
		pPrefStr = NULL;
		PREF_CopyCharPref("mail.pop_name", &pPrefStr);
		m_szPopName = pPrefStr;
		if (pPrefStr) XP_FREE(pPrefStr); 

		pPrefStr = NULL;
		PREF_CopyCharPref("network.hosts.pop_server", &pPrefStr);
		m_szInMailServer = pPrefStr;
		if (pPrefStr) XP_FREE(pPrefStr); 
	}

	pPrefStr = NULL;
	PREF_CopyCharPref("network.hosts.nntp_server", &pPrefStr);
	m_szNewsServer = pPrefStr;
	if (pPrefStr) XP_FREE(pPrefStr); 

	if (m_szNewsServer.IsEmpty())
	{
		m_bIsSecure = FALSE;
		m_lPort = NEWS_PORT;
	}
	else
	{
		PREF_GetBoolPref("news.server_is_secure", &m_bIsSecure);
		PREF_GetIntPref("news.server_port", &m_lPort);
	}
}

void CMailNewsWizard::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CMailNewsWizard::OnInitDialog()
{
	GetDlgItem(IDC_EDIT1)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_EG1)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_EG2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC3)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT3)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_READMAIL_POP)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_READMAIL_IMAP)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC5)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_SECURE)->ShowWindow(SW_HIDE);

	GetDlgItem(IDC_BUTTON_BACK)->EnableWindow(FALSE);

	SetControlText(IDC_STATIC_TITLE, IDS_WIZARD_COVER);
	SetControlText(IDC_STATIC1, IDS_COVER_STATIC1);
	return CDialog::OnInitDialog();
}

void CMailNewsWizard::DoBack()
{
	m_nCurrentPage -= 1;
	switch 	(m_nCurrentPage)
	{
	case ID_PAGE_COVER:		// 1st page
		ShowHideSendMailControls(SW_HIDE); 
		ShowCoverPage();
		GetDlgItem(IDC_BUTTON_BACK)->EnableWindow(FALSE);
		break;
	case ID_PAGE_SENDMAIL:  // 2nd page
		ShowHideReadMailControls(SW_HIDE);
		ShowSendMailPage();
		break;
	case ID_PAGE_READMAIL:	// 3rd page
		SetControlText(IDOK, IDS_NEXT);
		ShowHideReadNewsControls(SW_HIDE);
		ShowReadMailPage();
		break;
	default:
		break;
	}
}

void CMailNewsWizard::DoNext()
{
    char text[BUFSZ];
	int nLen = 0;

	m_nCurrentPage += 1;
	switch 	(m_nCurrentPage)
	{
	case ID_PAGE_SENDMAIL:  // 2nd page
		ShowSendMailPage();
		GetDlgItem(IDC_BUTTON_BACK)->EnableWindow(TRUE);
		break;
	case ID_PAGE_READMAIL:	// 3rd page
		nLen = GetDlgItemText(IDC_EDIT2, text, BUFSZ); 
		text[nLen] = '\0';
		if (nLen == 0 || (nLen && EmailValid(text)))
		{
			ShowHideSendMailControls(SW_HIDE); 
			if (m_szInMailServer.IsEmpty())
				m_szInMailServer = m_szMailServer;
			ShowReadMailPage();
		}
		else
		{
			AfxMessageBox(IDS_INVALID_EMAIL);
			GetDlgItem(IDC_EDIT2)->SetFocus();
			m_nCurrentPage -= 1;
		}
		break;
	case ID_PAGE_READNEWS:	// 4th page
		SetControlText(IDOK, IDS_FINISH);
		ShowHideReadMailControls(SW_HIDE); 
		ShowReadNewsPage();
		break;
	case ID_PAGE_FINISH:	// done, save value
		if (DoFinish())
			EndDialog(IDOK);
		break;
	default:
		break;
	}
}

BOOL CMailNewsWizard::CheckValidText() 
{
    char text[BUFSZ];
	text[0] = '\0';
    int nLen = GetDlgItemText(IDC_EDIT1, text, BUFSZ); 
	
	if (nLen)
	{
		text[0] = '\0';
		nLen = GetDlgItemText(IDC_EDIT2, text, BUFSZ); 
		if (nLen)
			return TRUE;
		else
		{
			AfxMessageBox(IDS_EMPTY_STRING);
			GetDlgItem(IDC_EDIT2)->SetFocus();
			return FALSE;
		}
	}
	else
	{
		AfxMessageBox(IDS_EMPTY_STRING);
		GetDlgItem(IDC_EDIT1)->SetFocus();
		return FALSE;
	}
}

void CMailNewsWizard::SetControlText(int nID, int nStringID) 
{
	CString text;
	text.LoadString(nStringID);
	SetDlgItemText(nID, LPCTSTR(text));
}

void CMailNewsWizard::ShowCoverPage() 
{
	GetDlgItem(IDC_STATIC5)->ShowWindow(SW_HIDE);
	SetControlText(IDC_STATIC_TITLE, IDS_WIZARD_COVER);
	SetControlText(IDC_STATIC1, IDS_COVER_STATIC1);
}

void CMailNewsWizard::ShowSendMailPage()
{
	ShowHideSendMailControls(SW_SHOW);
	SetControlText(IDC_STATIC_TITLE, IDS_WIZARD_SENDMAIL);
	SetControlText(IDC_STATIC1, IDS_SENDMAIL_STATIC1);
	SetControlText(IDC_STATIC_EG1, IDS_SENDMAIL_EG1);
	SetControlText(IDC_STATIC2, IDS_SENDMAIL_STATIC2);
	SetControlText(IDC_STATIC_EG2, IDS_SENDMAIL_EG2);
	SetControlText(IDC_STATIC3, IDS_SENDMAIL_STATIC3);
	SetControlText(IDC_STATIC5, IDS_SENDREADMAIL_STATIC5);
}

void CMailNewsWizard::ShowHideSendMailControls(int nShowCmd)
{
	GetDlgItem(IDC_EDIT1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_EG1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EDIT2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_EG2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC3)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EDIT3)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC5)->ShowWindow(nShowCmd);
	if (nShowCmd == SW_SHOW)
	{	// init value
		SetDlgItemText(IDC_EDIT1, m_szFullName);
		SetDlgItemText(IDC_EDIT2, m_szEmail);
		SetDlgItemText(IDC_EDIT3, m_szMailServer);
	}
	else
	{	// save value
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

void CMailNewsWizard::ShowReadMailPage()
{
	ShowHideReadMailControls(SW_SHOW);
	SetControlText(IDC_STATIC_TITLE, IDS_WIZARD_READMAIL);
	SetControlText(IDC_STATIC1, IDS_READMAIL_STATIC1);
	SetControlText(IDC_STATIC_EG1, IDS_READMAIL_EG1);
	SetControlText(IDC_STATIC2, IDS_READMAIL_STATIC2);
	SetControlText(IDC_STATIC3, IDS_READMAIL_STATIC3);
	SetControlText(IDC_STATIC5, IDS_SENDREADMAIL_STATIC5);
}

void CMailNewsWizard::ShowHideReadMailControls(int nShowCmd)
{
	GetDlgItem(IDC_EDIT1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC_EG1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EDIT2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC3)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_READMAIL_POP)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_READMAIL_IMAP)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC5)->ShowWindow(nShowCmd);
	if (nShowCmd == SW_SHOW)
	{	// init value
		SetDlgItemText(IDC_EDIT1, m_szPopName);
		SetDlgItemText(IDC_EDIT2, m_szInMailServer);
		if (m_bUseIMAP)
			((CButton*)GetDlgItem(IDC_READMAIL_IMAP))->SetCheck(TRUE);
		else
			((CButton*)GetDlgItem(IDC_READMAIL_POP))->SetCheck(TRUE);
	}
	else
	{	// save value
		char text[BUFSZ];

		m_bUseIMAP = FALSE;
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
	}
}

void CMailNewsWizard::ShowReadNewsPage()
{
	ShowHideReadNewsControls(SW_SHOW);
	SetControlText(IDC_STATIC_TITLE, IDS_WIZARD_READNEWS);
	SetControlText(IDC_STATIC1, IDS_READNEWS_STATIC1);
	SetControlText(IDC_STATIC2, IDS_READNEWS_STATIC2);
	SetControlText(IDC_STATIC5, IDS_READNEWS_STATIC5);
}

void CMailNewsWizard::ShowHideReadNewsControls(int nShowCmd)
{
	GetDlgItem(IDC_EDIT1)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_EDIT2)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_STATIC5)->ShowWindow(nShowCmd);
	GetDlgItem(IDC_SECURE)->ShowWindow(nShowCmd);
	if (nShowCmd == SW_SHOW)
	{	// init value
		char szPort[16];
		SetDlgItemText(IDC_EDIT1, m_szNewsServer); 
		sprintf(szPort, "%ld", m_lPort);
		SetDlgItemText(IDC_EDIT2, szPort);
		if (m_bIsSecure)
			((CButton*)GetDlgItem(IDC_SECURE))->SetCheck(TRUE);
		else
			((CButton*)GetDlgItem(IDC_SECURE))->SetCheck(FALSE);
	}
	else
	{	// save value
		char text[BUFSZ];
		if(m_nNewsServerLen = GetDlgItemText(IDC_EDIT1, text, BUFSZ)) 
			m_szNewsServer = text;
		else 
			m_szNewsServer = "";
		if (IsDlgButtonChecked(IDC_SECURE)) 
			m_bIsSecure = TRUE;
		else
			m_bIsSecure = FALSE;
		if (GetDlgItemText(IDC_EDIT2, text, BUFSZ))
			m_lPort = atol(text);
		else
		{
			if (m_bIsSecure)
				m_lPort = SECURE_NEWS_PORT;
			else
				m_lPort = NEWS_PORT;
		}
	}
}

BOOL CMailNewsWizard::DoFinish()
{
	char text[BUFSZ], szPort[10];
	int  nPortLen = 0;

	if (m_nNewsServerLen = GetDlgItemText(IDC_EDIT1, text, BUFSZ)) 
		m_szNewsServer = text;
	else 
		m_szNewsServer = "";

	if (IsDlgButtonChecked(IDC_SECURE)) 
		m_bIsSecure = TRUE;
	else
		m_bIsSecure = FALSE;
	if (nPortLen = GetDlgItemText(IDC_EDIT2, szPort, 10) > 0)
		m_lPort = atol(szPort);
	else
	{
		if (m_bIsSecure)
			m_lPort = SECURE_NEWS_PORT;
		else
			m_lPort = NEWS_PORT;
	}

	if (m_nNewsServerLen && nPortLen)
	{
		if (m_lPort < 0 || m_lPort> MAX_PORT_NUMBER)
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

	if (m_nNewsServerLen)
	{
		PREF_SetBoolPref("news.server_is_secure", m_bIsSecure);
		PREF_SetIntPref("news.server_port", m_lPort);
	}
	else
	{
		PREF_SetBoolPref("news.server_is_secure", FALSE);
		PREF_SetIntPref("news.server_port", NEWS_PORT);
	}

	PREF_SetCharPref("mail.identity.username", LPCTSTR(m_szFullName));
	PREF_SetCharPref("mail.identity.useremail", LPCTSTR(m_szEmail));
	PREF_SetCharPref("network.hosts.smtp_server", LPCTSTR(m_szMailServer));

	long imapPref = m_bUseIMAP ? MSG_Imap4 : MSG_Pop3;
	PREF_SetIntPref("mail.server_type", imapPref);
	if (m_bUseIMAP)
	{
		SetImapServerName(LPCTSTR(m_szInMailServer));
		IMAP_SetCharPref(LPCTSTR(m_szInMailServer), CHAR_USERNAME, LPCTSTR(m_szPopName));
	}
	else
	{
		PREF_SetCharPref("mail.pop_name", LPCTSTR(m_szPopName));
		PREF_SetCharPref("network.hosts.pop_server", LPCTSTR(m_szInMailServer));
	}

	PREF_SetCharPref("network.hosts.nntp_server", LPCTSTR(m_szNewsServer));

	PREF_SavePrefFile();

	return TRUE;
}

void CMailNewsWizard::OnCheckSecure()
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

BEGIN_MESSAGE_MAP(CMailNewsWizard, CDialog)
    ON_BN_CLICKED(IDC_BUTTON_BACK, DoBack)
    ON_BN_CLICKED(IDC_SECURE, OnCheckSecure)
    ON_BN_CLICKED(IDOK, DoNext)
END_MESSAGE_MAP()

#endif

