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
#include "dialog.h"
#include "msgcom.h"
#include "nethelp.h"
#include "xp_help.h"
#include "mailpriv.h"
#include "mnprefs.h"
#ifdef XP_WIN16
#include "mninterf.h"
#else
#include "winprefs/mninterf.h"
#endif
#include "subnews.h"
#include "wfemsg.h"

#include "ldap.h"

extern "C" {
#include "xpgetstr.h"
#ifdef MOZ_MAIL_NEWS
extern int MK_MSG_LOCAL_MAIL;
#endif
};

//also in mnpref\src\pages.h, brpref\src\advpages.cpp
#define MNPREF_PrefIsLocked(pPrefName) PREF_PrefIsLocked(pPrefName)

static void ShowMapiError(UINT nId, LPCSTR pParam, CWnd* pWnd)
{
	char buffer[1024];
	CString msg;
	msg.LoadString(nId);
	if (pParam)
		sprintf(buffer, msg, pParam);
	else
		strcpy(buffer, msg);
	if (pWnd)
	{
		pWnd->MessageBox(buffer, NULL, MB_OK | MB_ICONSTOP);
	}
	else
	{
		AfxMessageBox(buffer);
	}
}

static void ShowError(UINT nId, CWnd* pWnd)
{
	ShowMapiError(nId, NULL, pWnd);
}


#ifdef _WIN32

static const char szMapiDll[] = "Mapi32.dll";

static BOOL IsOurMapiInstalled(void)
{
	// see if our MAPI support is already installed
	BOOL ourDLLInstalled = FALSE;
	HINSTANCE hMAPIInst = LoadLibrary(szMapiDll);
	if (hMAPIInst)
	{
		FARPROC funcPtr = GetProcAddress(hMAPIInst, "MAPIGetNetscapeVersion");
		if (funcPtr)
		{
			ourDLLInstalled = TRUE;
		}
		FreeLibrary(hMAPIInst);
	}
	return(ourDLLInstalled);
}

static BOOL UpdateMapiDll(BOOL prefUseMAPI, CWnd* pWnd)
{
	// get the two directories to copy between
	const char szMapiBackupDll[] = "Mapi32bak.dll";
	const char sznscpMapiDll[] = "nsmapi32.dll";

	CString nsMapiFile;
	CString systemPath;
	CString msMapiFile;
	CString msMapiBackupFile;
	CFileStatus status;
	GetSystemDirectory(systemPath.GetBuffer(_MAX_PATH), _MAX_PATH);
	systemPath.ReleaseBuffer();
	if (systemPath.Right(1) != "\\")
	{
		systemPath += "\\";
	}
	GetModuleFileName(theApp.m_hInstance, nsMapiFile.GetBuffer(_MAX_PATH), _MAX_PATH);
	nsMapiFile.ReleaseBuffer();
	// strip off the netscape.exe from the end
	int pos = nsMapiFile.ReverseFind('\\');
	if (pos != -1)
	{
		nsMapiFile = nsMapiFile.Left(pos);
	}
	if (nsMapiFile.Right(1) != "\\")
	{
		nsMapiFile += "\\";
	}
	nsMapiFile += sznscpMapiDll;

	msMapiFile = systemPath + szMapiDll;
	msMapiBackupFile = systemPath + szMapiBackupDll;

	BOOL ourDLLInstalled = IsOurMapiInstalled();

	if (prefUseMAPI)
	{
		// Before the Send option will show up in other applications, you must have
		// [Mail]
		// MAPIX=1
		// MAPI=1
		WriteProfileString("Mail", "MAPIX", "1");
		WriteProfileString("Mail", "MAPI", "1");
		if (ourDLLInstalled)
		{
			// if the nsmapi32.dll that we have is a newer date than the installed one
			// copy it over anyway
			CFileStatus installedFile, ourFile;
			if (CFile::GetStatus(msMapiFile, installedFile))
			{
				if (CFile::GetStatus(nsMapiFile, ourFile))
				{
					// if ours is newer, install it anyway
					if (ourFile.m_mtime > installedFile.m_mtime)
					{
						// don't need to do a backup because our file is already installed
						if (!CopyFile(nsMapiFile, msMapiFile, FALSE))
						{
							// can't install ours, tell the user
							ShowMapiError(IDS_NSMAPI32_COPY_FAILED, msMapiFile, pWnd);
							return(FALSE);
						}
					}
				}
			}
		}
		else
		{
			// check for the existance of nsmapi32.dll
			if (!CFile::GetStatus(nsMapiFile, status))
			{
				// nsmapi32.dll doesn't exist, tell the user
				ShowMapiError(IDS_NO_NSMAPI32, nsMapiFile, pWnd);
				return(FALSE);
			}
			// check for the existance of mapi32.dll and only make the backup if it exists
			if (CFile::GetStatus(msMapiFile, status))
			{
				if (!CopyFile(msMapiFile, msMapiBackupFile, FALSE))
				{
					// can't make backup copy, refuse to install
					ShowMapiError(IDS_MAPI_BACKUP_FAILED, msMapiBackupFile, pWnd);
					return(FALSE);
				}
			}
			if (!CopyFile(nsMapiFile, msMapiFile, FALSE))
			{
				// can't install ours, tell the user
				ShowMapiError(IDS_NSMAPI32_COPY_FAILED, msMapiFile, pWnd);
				return(FALSE);
			}
		}
	}
	else	// don't use our MAPI
	{
		if (ourDLLInstalled)
		{
			// restore the backup DLL
			if (!CFile::GetStatus(msMapiBackupFile, status))
			{
				// old mapi file doesn't exist, tell the user
				ShowMapiError(IDS_MAPI_BACKUP_MISSING, msMapiBackupFile, pWnd);
				return(FALSE);
			}
			if (!CopyFile(msMapiBackupFile, msMapiFile, FALSE))
			{
				// copy failed, tell the user
				ShowMapiError(IDS_NSMAPI32_COPY_FAILED2, msMapiFile, pWnd);
				return(FALSE);
			}
			// remove the backup file
			remove(msMapiBackupFile);
		}
	}
	return(TRUE);
}

static BOOL UpdateMapiDll(void)
{
	XP_Bool prefUseMAPI = FALSE;
 	PREF_GetBoolPref("mail.use_mapi_server", &prefUseMAPI);
	return(UpdateMapiDll(prefUseMAPI, NULL));
}
#endif

extern "C" BOOL IsNumeric(char* pStr)
{
    
	for (char* p = pStr; p && *p; p++)
	{
        if (0 == isdigit(*p))
            return FALSE; 
	}
	return TRUE;
}

#ifdef MOZ_MAIL_NEWS

extern "C" MSG_Host *DoAddNewsServer(CWnd* pParent, int nFromWhere)
{								
	CNewsServerDialog addServerDialog(pParent, NULL, nFromWhere);
	if (IDOK == addServerDialog.DoModal())
	{
		char* pName = addServerDialog.GetNewsHostName();
		XP_Bool bSecure = addServerDialog.GetSecure();
		XP_Bool bAuthentication = addServerDialog.GetAuthentication();
		int32 nPort = addServerDialog.GetNewsHostPort();

		MSG_NewsHost *pNewHost = MSG_CreateNewsHost(WFE_MSGGetMaster(), pName, 
													bSecure, nPort);
					 
		if (pNewHost)
		{
			// tell the back-end to ask for name/pw without getting challenged
			MSG_SetNewsHostPushAuth (pNewHost, bAuthentication);

			return MSG_GetMSGHostFromNewsHost(pNewHost);
		}
		else
			return NULL;
	}
	else
		return NULL;
}

extern "C" MSG_Host *DoAddIMAPServer(CWnd* pParent, char* pServerName, BOOL bImap)
{
	CString title;
	title.LoadString(IDS_ADD_MAIL_SERVER);
    CMailServerPropertySheet addMailServer(pParent, title, pServerName, TYPE_IMAP, FALSE);
    if (IDOK == addMailServer.DoModal())
	{
		return NULL;
	}
	else
		return NULL;
}

void UnixToDosString(char* pStr)
{
	if (!pStr || !strlen(pStr))
		return;
    for (char * p = pStr; p && *p; p++)
	{
        if(*p == '/')
            *p = '\\';
        else if (*p == '|')
            *p = ':'; 
	}
}

void DosToUnixString(char* pStr)
{
	if (!pStr || !strlen(pStr))
		return;
    for (char * p = pStr; p && *p; p++)
	{
        if(*p == '\\')
            *p = '/';
        else if(*p == ':')
            *p = '|'; 
	}
}

//Make sure caller function XP_FREE the return string
extern "C" char* ConvertPathToURL(char* lpPath)
{
	static const char *formatString = "mailbox:/%s";

	if (!lpPath || !strlen(lpPath))
		return NULL;

	DosToUnixString(lpPath);
	char *urlString = (char*)XP_ALLOC(XP_STRLEN(formatString) + XP_STRLEN(lpPath));
	if (urlString)
		sprintf(urlString, formatString, lpPath);
	UnixToDosString(lpPath);
	return urlString;
}

MSG_FolderInfo * GetFolderInfoFromPref(char* lpPref)
{ 
	MSG_FolderInfo *pFolderInfo = NULL;

	int nLen = strlen(lpPref);
	if (nLen)
	{
		MSG_Master *pMaster	= WFE_MSGGetMaster();
		int nUrlType = NET_URL_Type(lpPref);
		if (0 == nUrlType || MAILBOX_TYPE_URL == nUrlType)
		{
			if (0 == nUrlType)
			{
				char* pURL = NULL;
				pURL = ConvertPathToURL(lpPref);
				if (pURL)
				{
					pFolderInfo = MSG_GetFolderInfoFromURL(pMaster, pURL);
					XP_FREE(pURL);
				}
			}
			else
				pFolderInfo = MSG_GetFolderInfoFromURL(pMaster, lpPref);
		}
		else if (IMAP_TYPE_URL == nUrlType)
			pFolderInfo = MSG_GetFolderInfoFromURL(pMaster, lpPref);
	}
	return pFolderInfo;
}

void SetFolderComboCurSel(HWND hControl, char* lpName, int nDefaultID)
{ 
	int nFolderIndex = 0;
	int nLen = strlen(lpName);

	if (nLen)
	{
		MSG_FolderInfo *pFolder = GetFolderInfoFromPref(lpName);

		int nCount = (int)SendMessage(hControl, CB_GETCOUNT, 0, 0);
		for (int i = 0; i < nCount; i++ ) 
		{
			MSG_FolderInfo *pFolderInfo = (MSG_FolderInfo*)SendMessage(hControl, 
														   CB_GETITEMDATA, i, 0);
			if (pFolderInfo == pFolder)
			{
				nFolderIndex = i;
				break;
			}
		}
	}
	SendMessage(hControl, CB_SETCURSEL, nFolderIndex, 0);
}

BOOL SetServerComboCurSel(HWND hControl, char* lpName, int nDefaultID)
{ 
	MSG_Master *pMaster = WFE_MSGGetMaster();
	MSG_FolderInfo *pFolder = NULL;
	int nLen = strlen(lpName);
	int nUrlType = NET_URL_Type(lpName);
	BOOL bLocalMail = FALSE;
	BOOL bUseDefaultName = FALSE;
	char* pLocal = NULL;
	char* pDefaultName = XP_GetString(nDefaultID);
	MSG_FolderLine folderLine;

	if (nLen)
	{
		pFolder = GetFolderInfoFromPref(lpName);
		MSG_GetFolderLineById (pMaster, pFolder, &folderLine);
		if (!XP_FILENAMECMP(pDefaultName, folderLine.name))
		{
			bUseDefaultName = TRUE;
		}
		if (0 == nUrlType || MAILBOX_TYPE_URL == nUrlType)
			bLocalMail = TRUE;
	}
	else
	{
		bLocalMail = TRUE;
		bUseDefaultName = TRUE;
	}

	int nFolderIndex = 0;

	if (bLocalMail)
		pLocal = XP_GetString(MK_MSG_LOCAL_MAIL);
	MSG_FolderInfo*  pHostFolderInfo = GetHostFolderInfo(pFolder);
	int nCount = (int)SendMessage(hControl, CB_GETCOUNT, 0, 0);
	for (int i = 0; i < nCount; i++ ) 
	{
		MSG_FolderInfo *pFolderInfo = (MSG_FolderInfo*)SendMessage(hControl, 
													   CB_GETITEMDATA, i, 0);
		if (bLocalMail)
		{
			MSG_GetFolderLineById (pMaster, pFolderInfo, &folderLine);
			if (!XP_FILENAMECMP(pLocal, folderLine.name))
			{
				nFolderIndex = i;
				break;
			}
			nFolderIndex = (int)SendMessage(hControl, CB_FINDSTRING, (WPARAM)-1,
											(LPARAM)(LPCSTR)pLocal);
		}
		else
		{
			if (pHostFolderInfo == pFolderInfo)
			{
				nFolderIndex = i;
				break;
			}
		}
	}
	SendMessage(hControl, CB_SETCURSEL, nFolderIndex, 0);

	if (bUseDefaultName)
		return TRUE;
	else
		return FALSE;
}

extern "C" void GetFolderServerNames
(char* lpName, int nDefaultID, CString& folder, CString& server)
{ 
	MSG_FolderInfo *pFolderInfo = NULL;

	int nLen = strlen(lpName);
	if (nLen)
	{
		pFolderInfo = GetFolderInfoFromPref(lpName);
	}
	else
	{
		char* pDefault = XP_GetString(nDefaultID);

		char defaultName[256];
		int nLen = 255;

		if (PREF_NOERROR == PREF_GetCharPref("mail.directory", defaultName, &nLen))
		{
			defaultName[nLen] = '\0';
			XP_STRCAT(defaultName, "\\");
			XP_STRCAT(defaultName, pDefault);
		}
		pFolderInfo = GetFolderInfoFromPref(defaultName);
	}

	if (pFolderInfo)
	{
		MSG_Master* pMaster = WFE_MSGGetMaster();
		MSG_FolderLine folderLine;
		if (MSG_GetFolderLineById(pMaster, pFolderInfo, &folderLine)) 
			folder = folderLine.name;

		MSG_FolderInfo*  pHostFolderInfo = GetHostFolderInfo(pFolderInfo);
		if (pHostFolderInfo)
		{
			if (MSG_GetFolderLineById(pMaster, pHostFolderInfo, &folderLine)) 
				server = folderLine.name;
		}
		else
			server = XP_GetString(MK_MSG_LOCAL_MAIL);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CChooseFolderDialog	
//	   
CChooseFolderDialog::CChooseFolderDialog(CWnd *pParent, char* pFolderPath, int nTypeID)
	: CDialog(IDD, pParent)
{
	m_pFolderPath =  pFolderPath;
	m_nDefaultID = nTypeID;
}

BOOL CChooseFolderDialog::OnInitDialog()
{
	BOOL ret = CDialog::OnInitDialog();

	CString formatString, defaultTitle;
	formatString.LoadString(IDS_SPECIAL_FOLDER);
	defaultTitle.Format(LPCTSTR(formatString), XP_GetString(m_nDefaultID));
	SetDlgItemText(IDC_RADIO_SENT, LPCTSTR(defaultTitle));

	if ( ret ) {
		// Subclass Server combo
		m_ServerCombo.SubclassDlgItem( IDC_COMBO_SERVERS, this );
		m_ServerCombo.NoPrettyName();
		m_ServerCombo.PopulateMailServer( WFE_MSGGetMaster() );
		if (SetServerComboCurSel(m_ServerCombo.GetSafeHwnd(), 
			m_pFolderPath, m_nDefaultID))
			CheckDlgButton(IDC_RADIO_SENT, TRUE);
		else
			CheckDlgButton(IDC_RADIO_OTHER, TRUE);

		// Subclass folder combo
		m_FolderCombo.SubclassDlgItem( IDC_COMBO_FOLDERS, this );
		m_FolderCombo.PopulateMail( WFE_MSGGetMaster() );
		SetFolderComboCurSel(m_FolderCombo.GetSafeHwnd(), 
			m_pFolderPath, m_nDefaultID);
	}
	
	return ret;
}

void CChooseFolderDialog::OnOK() 
{
	MSG_FolderInfo *pFolder = NULL;
	MSG_FolderLine folderLine;
	MSG_Master* pMaster = WFE_MSGGetMaster();

	if (IsDlgButtonChecked(IDC_RADIO_SENT))
	{
		pFolder = (MSG_FolderInfo*)m_ServerCombo.GetItemData(m_ServerCombo.GetCurSel());
		m_szFolder = XP_GetString(m_nDefaultID);
		if (MSG_GetFolderLineById(pMaster, pFolder, &folderLine)) 
			m_szServer = folderLine.name;
		URL_Struct *url = MSG_ConstructUrlForFolder(NULL, pFolder);
		m_szPrefUrl = url->address;
	}
	else if (IsDlgButtonChecked(IDC_RADIO_OTHER))
	{
		pFolder = (MSG_FolderInfo*)m_FolderCombo.GetItemData(m_FolderCombo.GetCurSel());

		if (MSG_GetFolderLineById(pMaster, pFolder, &folderLine)) 
		{
			m_szFolder = folderLine.name;
			MSG_FolderInfo*  pHostFolderInfo = GetHostFolderInfo(pFolder);
			if (pHostFolderInfo)
			{
				if (MSG_GetFolderLineById(pMaster, pHostFolderInfo, &folderLine)) 
					m_szServer = folderLine.name;
			}
			else
				m_szServer = XP_GetString(MK_MSG_LOCAL_MAIL);

			URL_Struct *url = MSG_ConstructUrlForFolder(NULL, pFolder);
			m_szPrefUrl = url->address;
		}
	}

	CDialog::OnOK();
}

void CChooseFolderDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CChooseFolderDialog::OnNewFolder()
{   
    CPrefNewFolderDialog newFolderDlg( this, NULL);
	if (IDOK == newFolderDlg.DoModal())
	{
		MSG_FolderInfo *pNewFolder = newFolderDlg.GetNewFolder();
		m_FolderCombo.PopulateMail(WFE_MSGGetMaster());

		for (int i = m_FolderCombo.GetCount(); i >= 0 ; i--)
		{
			MSG_FolderInfo *pFolderInfo = (MSG_FolderInfo*)m_FolderCombo.GetItemData(i);
			if (pNewFolder == pFolderInfo)
			{
				m_FolderCombo.SetCurSel(i);
				break;
			}
		}
	}
}                                     

BEGIN_MESSAGE_MAP(CChooseFolderDialog, CDialog)
    ON_BN_CLICKED(IDC_NEW_FOLDER, OnNewFolder)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewsServerDialog	
//	   
CNewsServerDialog::CNewsServerDialog(CWnd *pParent, const char* pName, int nFromWhere, MSG_NewsHost *pHost)
	: CDialog(CNewsServerDialog::IDD, pParent)
{
	m_hostName[0] = '\0';
	if (pName && strlen(pName))
		XP_STRCAT(m_hostName, pName);
	m_bIsSecure = FALSE;
	m_bAuthentication = FALSE;
	m_nFromWhere = nFromWhere;
	m_lPort = NEWS_PORT;
	m_pEditHost = pHost;
    if (pHost)
	{
		 m_bIsSecure = MSG_IsNewsHostSecure(pHost);
		 m_lPort = MSG_GetNewsHostPort(pHost);
		 m_bAuthentication = MSG_GetNewsHostPushAuth(pHost);
	}
}

BOOL CNewsServerDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	CString title;
	char port[16];

	sprintf(port, "%ld", m_lPort);
	SetDlgItemText(IDC_EDIT_PORT, port);
	if (m_pEditHost)
	{
		title.LoadString(IDS_NEWS_SERVER_PROPERTY);
		SetWindowText(LPCTSTR(title));
		SetDlgItemText(IDC_STATIC_HOST, MSG_GetNewsHostName(m_pEditHost));
		GetDlgItem(IDC_EDIT_HOST)->ShowWindow(SW_HIDE);

		if (m_bAuthentication)	// Authentication
			CheckDlgButton(IDC_USE_NAME, TRUE);
		else
			CheckDlgButton(IDC_USE_NAME, FALSE);
		((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetFocus();
	}
	else
	{
		title.LoadString(IDS_ADD_NEWS_SERVER);
		SetWindowText(LPCTSTR(title));
		GetDlgItem(IDC_STATIC_HOST)->ShowWindow(SW_HIDE);
	#ifdef _WIN32
		((CEdit*)GetDlgItem(IDC_EDIT_HOST))->SetLimitText(MAX_HOSTNAME_LEN - 1);
	#else
		((CEdit*)GetDlgItem(IDC_EDIT_HOST))->LimitText(MAX_HOSTNAME_LEN - 1);
	#endif
		((CEdit*)GetDlgItem(IDC_EDIT_HOST))->SetFocus();
	}
	return 0;
}

void CNewsServerDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CNewsServerDialog::OnOK() 
{
	char port[16];
	BOOL bServerExist = FALSE;

	if (!m_pEditHost && 0 == GetDlgItemText(IDC_EDIT_HOST, m_hostName, MAX_HOSTNAME_LEN))
	{
		AfxMessageBox(IDS_EMPTY_STRING);
		((CEdit*)GetDlgItem(IDC_EDIT_HOST))->SetFocus();
		return;
	}
	if (!m_pEditHost && NewsHostExists())
	{
		AfxMessageBox(IDS_SERVER_EXISTS);
		((CEdit*)GetDlgItem(IDC_EDIT_HOST))->SetFocus();
		((CEdit*)GetDlgItem(IDC_EDIT_HOST))->SetSel((DWORD)MAKELONG(0, -1));
		return;
	}  
	if (GetDlgItemText(IDC_EDIT_PORT, port, 16))
	{
		int32 lPort = GetPortNumber();
		if (lPort < 0 || lPort> MAX_PORT_NUMBER)
		{
			AfxMessageBox(IDS_PORT_RANGE);
			((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetSel((DWORD)MAKELONG(0, -1));
			return;	    
		}
		if (!::IsNumeric(port))
		{
			AfxMessageBox(IDS_NUMBERS_ONLY);
			((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_PORT))->SetSel((DWORD)MAKELONG(0, -1));
			return;
		}
		m_lPort = lPort;
	}
	else
	{
		if (m_bIsSecure)
			m_lPort = SECURE_NEWS_PORT;
		else
			m_lPort = NEWS_PORT;
	}
	if (IsDlgButtonChecked(IDC_USE_NAME))	// Authentication
		m_bAuthentication = TRUE;
	else
		m_bAuthentication = FALSE;
	CDialog::OnOK();
}

BOOL CNewsServerDialog::NewsHostExists()
{	
	if (m_nFromWhere == FROM_SUBSCRIBEUI)
	{
		CSubscribePropertySheet* pSheet;
		CSubscribePropertyPage* pPage;

		pSheet = (CSubscribePropertySheet*)GetParent();
		pPage = (CSubscribePropertyPage*)pSheet->GetCurrentPage();
		CComboBox* pServerCombo = pPage->GetServerCombo();
		int nTotal = pServerCombo->GetCount();
		for (int i = 0; i < nTotal; i++)
		{
			MSG_Host *pNewsHost = (MSG_Host *)pServerCombo->GetItemDataPtr(i);
			if (IsSameServer(pNewsHost))
				return TRUE;
		}
		return FALSE;
	}
	else 
	{
 		MSG_Host** hNewsHost = NULL;
		BOOL bSameHost = FALSE;
		int32 nTotal = 0;

		MSG_Master* pMaster = WFE_MSGGetMaster();
		if (m_nFromWhere == FROM_FOLDERPANE)
			nTotal =  MSG_GetSubscribingHosts(pMaster, NULL, 0);
		else
			nTotal =  MSG_GetNewsHosts(pMaster, NULL, 0);
		if (nTotal)
		{
			hNewsHost = new MSG_Host* [nTotal];
			ASSERT(hNewsHost != NULL);
			if (m_nFromWhere == FROM_FOLDERPANE)
				nTotal =  MSG_GetSubscribingHosts(pMaster, hNewsHost, nTotal);
			else
				nTotal =  MSG_GetNewsHosts(pMaster, (MSG_NewsHost**)hNewsHost, nTotal);
			for (int i = 0; i < nTotal; i++)
			{
				if (bSameHost = IsSameServer(hNewsHost[i]))
					break;
			}
			if (hNewsHost)
				delete [] hNewsHost;
		}
		return bSameHost;
	}	
}

BOOL CNewsServerDialog::IsSameServer(MSG_Host *pHost)
{	
	XP_Bool bIsSecure = IsDlgButtonChecked(IDC_SECURE);
	int32 lEditPort = GetPortNumber();
	const char* pHostName = MSG_GetNewsHostName((MSG_NewsHost*)pHost);
	if (0 == lstrcmp(m_hostName, pHostName))
	{
		XP_Bool bSecure = MSG_IsNewsHostSecure((MSG_NewsHost*)pHost);
		int32 lPort = MSG_GetNewsHostPort((MSG_NewsHost*)pHost);
		if (bIsSecure == bSecure && lEditPort == lPort)
			return TRUE;
	}  
	return FALSE;
}

int32 CNewsServerDialog::GetPortNumber()
{
	char szPort[16];

	if (GetDlgItemText(IDC_EDIT_PORT, szPort, 15) > 0)
		return atol(szPort);
	else
		return -1;
}

void CNewsServerDialog::OnCheckSecure() 
{
	char port[16];
	if (IsDlgButtonChecked(IDC_SECURE))
		m_bIsSecure = TRUE;
	else
		m_bIsSecure = FALSE;
	if (GetDlgItemText(IDC_EDIT_PORT, port, 16) == 0)
	{
		if (m_bIsSecure)
			SetDlgItemInt(IDC_EDIT_PORT, SECURE_NEWS_PORT);
		else
			SetDlgItemInt(IDC_EDIT_PORT, NEWS_PORT);
	}
	else
	{
		int32 lPort = GetPortNumber();
		if (m_bIsSecure && lPort == NEWS_PORT)
			SetDlgItemInt(IDC_EDIT_PORT, SECURE_NEWS_PORT);
		else  if (!m_bIsSecure && lPort == SECURE_NEWS_PORT)
			SetDlgItemInt(IDC_EDIT_PORT, NEWS_PORT);
	}
}

void CNewsServerDialog::OnHelp()
{
	NetHelp(HELP_ADD_SERVER);
}

BEGIN_MESSAGE_MAP(CNewsServerDialog, CDialog)
	ON_BN_CLICKED(IDOK, OnOK)
	ON_BN_CLICKED(IDC_SECURE, OnCheckSecure)
	ON_BN_CLICKED(ID_HELP, OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// IMAP pref stuff

typedef enum
{
  CHAR_USERNAME,
  BOOL_REMEMBER_PASSWORD,
  BOOL_CHECK_NEW_MAIL,
  INT_CHECK_TIME,
  BOOL_OFFLINE_DOWNLOAD,
  BOOL_MOVE_TO_TRASH,
  BOOL_IS_SECURE,
  CHAR_PERAONAL_DIR,
  CHAR_PUBLIC_DIR,
  CHAR_OTHER_USER_DIR,
  BOOL_OVERRIDE_NAMESPACES

}IMAP_PREF;


static const char *kPrefTemplate = "mail.imap.server.%s.%s";

// Make sure XP_FREE() from the caller function;
char* IMAP_GetPrefString(const char *pHostName, int nID)
{
	int		prefSize = XP_STRLEN(pHostName) + 60;
	char	*pPrefName = (char *) XP_ALLOC(prefSize);

	if (!pPrefName)
		return pPrefName;

	switch (nID)
	{
	case CHAR_USERNAME:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "userName");
		break;

	case BOOL_REMEMBER_PASSWORD:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "remember_password");
		break;

	case BOOL_CHECK_NEW_MAIL:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "check_new_mail");
		break;

	case INT_CHECK_TIME:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "check_time");
		break;

	case BOOL_OFFLINE_DOWNLOAD:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "offline_download");
		break;

	case BOOL_MOVE_TO_TRASH:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "delete_is_move_to_trash");
		break;

	case BOOL_IS_SECURE:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "isSecure");
		break;

	case CHAR_PERAONAL_DIR:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "namespace.personal");
		break;

	case CHAR_PUBLIC_DIR:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "namespace.public");
		break;
	
	case CHAR_OTHER_USER_DIR:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "namespace.other_users");
		break;

	case BOOL_OVERRIDE_NAMESPACES:
		PR_snprintf(pPrefName, prefSize, kPrefTemplate, pHostName, "override_namespaces");
		break;

	default:
		ASSERT(0);
		break;
	}

	return pPrefName;
}

BOOL IMAP_PrefIsLocked(const char *pHostName, int nID)
{
	BOOL	bResult = FALSE;
	char*	pPrefName = NULL;

	pPrefName = IMAP_GetPrefString(pHostName, nID);
	if (pPrefName)
	{
		bResult = PREF_PrefIsLocked(pPrefName);
		XP_FREE(pPrefName);
	}
	return bResult;
}

void IMAP_SetCharPref(const char *pHostName, int nID, const char* pValue)
{
	char*	pPrefName = NULL;

	pPrefName = IMAP_GetPrefString(pHostName, nID);
	if (pPrefName)
	{
		PREF_SetCharPref(pPrefName, pValue);
		XP_FREE(pPrefName);
	}
}

void IMAP_SetIntPref(const char *pHostName, int nID, int32 lValue)
{
	char*	pPrefName = NULL;

	pPrefName = IMAP_GetPrefString(pHostName, nID);
	if (pPrefName)
	{
		PREF_SetIntPref(pPrefName, lValue);
		XP_FREE(pPrefName);
	}
}

void IMAP_SetBoolPref(const char *pHostName, int nID, XP_Bool bValue) 
{
	char*	pPrefName = NULL;

	pPrefName = IMAP_GetPrefString(pHostName, nID);
	if (pPrefName)
	{
		PREF_SetBoolPref(pPrefName, bValue);
		XP_FREE(pPrefName);
	}
}

void IMAP_GetCharPref(const char *pHostName, int nID, char **hBuffer) 
{
	char*	pPrefName = NULL;

	pPrefName = IMAP_GetPrefString(pHostName, nID);
	if (pPrefName)
	{
		PREF_CopyCharPref(pPrefName, hBuffer);
		XP_FREE(pPrefName);
	}
}

void IMAP_GetIntPref(const char *pHostName, int nID, int32 *pInt) 	
{
	char*	pPrefName = NULL;

	pPrefName = IMAP_GetPrefString(pHostName, nID);
	if (pPrefName)
	{
		PREF_GetIntPref(pPrefName, pInt);
		XP_FREE(pPrefName);
	}
}

void IMAP_GetBoolPref(const char *pHostName, int nID, XP_Bool *pBool) 	
{
	char*	pPrefName = NULL;

	pPrefName = IMAP_GetPrefString(pHostName, nID);
	if (pPrefName)
	{
		PREF_GetBoolPref(pPrefName, pBool);
		XP_FREE(pPrefName);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMailServerPropertySheet		
//   
CMailServerPropertySheet::CMailServerPropertySheet
(CWnd *pParent, const char* pTitle, const char* pName, int nType, BOOL bEdit, BOOL bBothType)
	: CPropertySheet(pTitle, pParent)
{
	m_hostName[0] = '\0';
	if (pName && strlen(pName))
		XP_STRCAT(m_hostName, pName);
	if (nType == TYPE_IMAP)
		m_bPop = FALSE;
	else
		m_bPop = TRUE;
	m_bEdit = bEdit;
	m_bBothType = bBothType;

	m_pGeneralPage = NULL;
	m_pPopPage = NULL;
	m_pIMAPPage = NULL;
	m_pAdvancedPage = NULL;

	if (m_bPop)
	{
		m_pGeneralPage = new CGeneralServerPage(this, pName);
		m_pPopPage = new CPopServerPage(this);
		AddPage(m_pGeneralPage);
		AddPage(m_pPopPage);
	}
	else
	{
		m_pGeneralPage = new CGeneralServerPage(this, pName);
		m_pIMAPPage = new CIMAPServerPage(this, pName);
		m_pAdvancedPage = new CIMAPAdvancedPage(this, pName);
		AddPage(m_pGeneralPage);
		AddPage(m_pIMAPPage);
		AddPage(m_pAdvancedPage);
	}
}

CMailServerPropertySheet::~CMailServerPropertySheet()
{
	if (m_pGeneralPage)
		delete m_pGeneralPage;
	if (m_pPopPage)
		delete m_pPopPage;
	if (m_pIMAPPage)
		delete m_pIMAPPage;   
	if (m_pAdvancedPage)
		delete m_pAdvancedPage;   
}

void CMailServerPropertySheet::SetMailHostName(char* pName)
{
	if (pName && strlen(pName))
		XP_STRCAT(m_hostName, pName);
}

XP_Bool CMailServerPropertySheet::GetIMAPUseSSL()
{
	if (m_pIMAPPage && IsWindow(m_pIMAPPage->GetSafeHwnd()))
		return m_pIMAPPage->GetUseSSL();
	else
		return FALSE;
}

void CMailServerPropertySheet::GetIMAPPersonalDir(char* pDir, int nLen)
{
	if (m_pAdvancedPage && IsWindow(m_pAdvancedPage->GetSafeHwnd()))
	{
		m_pAdvancedPage->GetPersonalDir(pDir, nLen);
	}
	else
		strcpy(pDir, "");
}

void CMailServerPropertySheet::GetIMAPPublicDir(char* pDir, int nLen)
{
	if (m_pAdvancedPage && IsWindow(m_pAdvancedPage->GetSafeHwnd()))
	{
		m_pAdvancedPage->GetPublicDir(pDir, nLen);
	}
	else
		strcpy(pDir, "");
}

void CMailServerPropertySheet::GetIMAPOthersDir(char* pDir, int nLen)
{
	if (m_pAdvancedPage && IsWindow(m_pAdvancedPage->GetSafeHwnd()))
	{
		m_pAdvancedPage->GetOthersDir(pDir, nLen);
	}
	else
		strcpy(pDir, "");
}

XP_Bool CMailServerPropertySheet::GetIMAPOverrideNameSpaces()
{
	if (m_pAdvancedPage && IsWindow(m_pAdvancedPage->GetSafeHwnd()))
		return m_pAdvancedPage->GetOverrideNameSpaces();
	else
		return FALSE;
}

void CMailServerPropertySheet::ShowHidePages(int nShowType) 
{
	if (nShowType == TYPE_POP)
	{
		m_bPop = TRUE;
		m_pPopPage = new CPopServerPage(this);
		AddPage(m_pPopPage);
		if (m_pIMAPPage)
		{
			RemovePage(m_pIMAPPage);
			m_pIMAPPage = NULL;
		}
		if (m_pAdvancedPage)
		{
			RemovePage(m_pAdvancedPage);
			m_pAdvancedPage = NULL;
		}
	}
	else
	{
		m_bPop = FALSE;
		m_pIMAPPage = new CIMAPServerPage(this, m_hostName);
		m_pAdvancedPage = new CIMAPAdvancedPage(this, m_hostName);
		AddPage(m_pIMAPPage);
		AddPage(m_pAdvancedPage);
		if (m_pPopPage)
		{
			RemovePage(m_pPopPage);
			m_pPopPage = NULL;
		}
	}
}

#ifdef _WIN32
 
BOOL CMailServerPropertySheet::OnInitDialog()
{
	BOOL ret = CPropertySheet::OnInitDialog();

	return ret;
}

#else

int CMailServerPropertySheet::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int ret = CPropertySheet::OnCreate(lpCreateStruct);
	
	return ret;
}

#endif

BOOL CMailServerPropertySheet::IsPageValid(CPropertyPage* pPage)
{
	if (pPage && IsWindow(pPage->GetSafeHwnd()))
		return TRUE;
	else
		return FALSE;
}

void CMailServerPropertySheet::OnOK()
{
	ASSERT_VALID(this);

    if (GetActivePage()->OnKillActive())
	{
		if (m_bPop)
		{
			if (IsPageValid(m_pGeneralPage))
			{
				if (!m_pGeneralPage->ProcessOK())
				{	
					SetActivePage(m_pGeneralPage);
					return;
				}
			}
			if (IsPageValid(m_pPopPage))
			{
				if (!m_pPopPage->ProcessOK())
				{	
					SetActivePage(m_pPopPage);
					return;
				}
			}
		}
		else
		{
			if (IsPageValid(m_pGeneralPage))
			{
				if (!m_pGeneralPage->ProcessOK())
				{	
					SetActivePage(m_pGeneralPage);
					return;
				}
			}
			if (IsPageValid(m_pIMAPPage))
			{
				if (!m_pIMAPPage->ProcessOK())
				{	
					SetActivePage(m_pIMAPPage);
					return;
				}
			}
			if (IsPageValid(m_pAdvancedPage))
			{
				if (!m_pAdvancedPage->ProcessOK())
				{	
					SetActivePage(m_pAdvancedPage);
					return;
				}
			}
		}
		EndDialog(IDOK);
	}

}

void CMailServerPropertySheet::OnHelp()
{
}

BEGIN_MESSAGE_MAP(CMailServerPropertySheet, CPropertySheet)
#ifndef _WIN32
    ON_WM_CREATE()
#endif
	ON_BN_CLICKED(IDOK, OnOK)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGeneralServerPage
//
CGeneralServerPage::CGeneralServerPage(CWnd *pParent, const char* pName)
	: CPropertyPage(IDD)
{
    m_pParent = (CMailServerPropertySheet*)pParent;
	m_szServerName = pName;
}

BOOL CGeneralServerPage::OnInitDialog()
{
	BOOL ret = CPropertyPage::OnInitDialog();

#ifdef _WIN32
	((CEdit*)GetDlgItem(IDC_EDIT_CHECK_MAIL))->SetLimitText(5);
#else
	((CEdit*)GetDlgItem(IDC_EDIT_CHECK_MAIL))->LimitText(5);
#endif

	if (!m_szServerName.IsEmpty())
	{
		SetDlgItemText(IDC_STATIC_MAIL_SERVER, szLoadString(IDS_MAIL_SERVER));
		SetDlgItemText(IDC_STATIC_SERVER, LPCTSTR(m_szServerName));
		((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->ShowWindow(SW_HIDE);
	}
	else
	{
		GetDlgItem(IDC_STATIC_SERVER)->ShowWindow(SW_HIDE);
	#ifdef _WIN32
		((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->SetLimitText(MAX_HOSTNAME_LEN - 1);
	#else
		((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->LimitText(MAX_HOSTNAME_LEN - 1);
	#endif
	}

	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO_TYPE);
	if (m_pParent->AllowBothTypes())
	{
		int nImap = pCombo->AddString(szLoadString(IDS_SERVER_IMAP_STATIC));
		pCombo->SetItemData(nImap, TYPE_IMAP);
		int nPop = pCombo->AddString(szLoadString(IDS_SERVER_POP3_STATIC));
		pCombo->SetItemData(nPop, TYPE_POP);
		if (m_pParent->IsPopServer())
			pCombo->SetCurSel(nPop);
		else
			pCombo->SetCurSel(nImap);
	}
    else 
	{
		int nIndex;
		if (m_pParent->IsPopServer())
		{
			nIndex = pCombo->AddString(szLoadString(IDS_SERVER_POP3_STATIC));
			pCombo->SetItemData(nIndex, TYPE_POP);
		}
		else
		{
			nIndex = pCombo->AddString(szLoadString(IDS_SERVER_IMAP_STATIC));
			pCombo->SetItemData(nIndex, TYPE_IMAP);
		}
		pCombo->SetCurSel(nIndex);
	}
	char* pUserName = NULL;
	int32 lCheckTime = 0;
	char buffer[32];
	XP_Bool bRememberPassword = FALSE;
	XP_Bool bCheckMail = FALSE;

	if (m_pParent->IsPopServer())
	{
		PREF_CopyCharPref("mail.pop_name", &pUserName);
		PREF_GetBoolPref("mail.remember_password",&bRememberPassword);
		PREF_GetBoolPref("mail.check_new_mail", &bCheckMail);
		PREF_GetIntPref("mail.check_time", &lCheckTime);
	}
	else
	{
		if (m_pParent->EditServer())
		{
			IMAP_GetCharPref(LPCTSTR(m_szServerName), CHAR_USERNAME, &pUserName);
			IMAP_GetBoolPref(LPCTSTR(m_szServerName), BOOL_CHECK_NEW_MAIL, &bCheckMail);
			IMAP_GetIntPref(LPCTSTR(m_szServerName), INT_CHECK_TIME, &lCheckTime);
			IMAP_GetBoolPref(LPCTSTR(m_szServerName), BOOL_REMEMBER_PASSWORD, &bRememberPassword);
		}
	}
	if (pUserName)
	{
		SetDlgItemText(IDC_EDIT_USERNAME, pUserName);
		XP_FREE(pUserName);
	}
	CheckDlgButton(IDC_CHECK_PASSWORD, bRememberPassword);
	CheckDlgButton(IDC_CHECK_MAIL, bCheckMail);

    if (lCheckTime < 0) 
	{
        SetDlgItemText(IDC_EDIT_CHECK_MAIL, "");
    } 
	else 
	{
        sprintf(buffer, "%ld", lCheckTime);
        SetDlgItemText(IDC_EDIT_CHECK_MAIL, buffer);
    }

	if (m_pParent->IsPopServer())
	{
		if (PREF_PrefIsLocked("mail.pop_name")) 
			GetDlgItem(IDC_EDIT_USERNAME)->EnableWindow(FALSE);
		if (PREF_PrefIsLocked("mail.check_new_mail")) 
			GetDlgItem(IDC_CHECK_MAIL)->EnableWindow(FALSE);
		if (PREF_PrefIsLocked("mail.check_time")) 
			GetDlgItem(IDC_EDIT_CHECK_MAIL)->EnableWindow(FALSE);
		if (PREF_PrefIsLocked("mail.remember_password")) 
			GetDlgItem(IDC_CHECK_PASSWORD)->EnableWindow(FALSE);
	}
	else
	{
		if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), CHAR_USERNAME)) 
			GetDlgItem(IDC_EDIT_USERNAME)->EnableWindow(FALSE);
		if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), BOOL_CHECK_NEW_MAIL) || 
			IMAP_PrefIsLocked(LPCTSTR(m_szServerName), INT_CHECK_TIME)) 
		{
			GetDlgItem(IDC_CHECK_MAIL)->EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT_CHECK_MAIL)->EnableWindow(FALSE);
		}
		if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), BOOL_REMEMBER_PASSWORD)) 
			GetDlgItem(IDC_CHECK_PASSWORD)->EnableWindow(FALSE);
	}
	return ret;
}

void CGeneralServerPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CGeneralServerPage::ProcessOK()
{
    char name[MAX_HOSTNAME_LEN];
    char userName[256];
    char text[MAX_DESCRIPTION_LEN];

	if (m_szServerName.IsEmpty() && 
		0 == GetDlgItemText(IDC_EDIT_SERVER, name, MAX_HOSTNAME_LEN))
	{
		AfxMessageBox(IDS_EMPTY_STRING);
		((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->SetFocus();
		return FALSE;
	}

	if (0 == GetDlgItemText(IDC_EDIT_USERNAME, userName, 255))
	{
		AfxMessageBox(IDS_EMPTY_STRING);
		((CEdit*)GetDlgItem(IDC_EDIT_USERNAME))->SetFocus();
		return FALSE;
	}

	if (IsDlgButtonChecked(IDC_CHECK_MAIL) &&  
	    GetDlgItemText(IDC_EDIT_CHECK_MAIL, text, MAX_DESCRIPTION_LEN))
	{
		if (!::IsNumeric(text))
		{
			ShowError(IDS_NUMBERS_ONLY, this);
			((CEdit*)GetDlgItem(IDC_EDIT_CHECK_MAIL))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_CHECK_MAIL))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;
		}
	}
	if (m_szServerName.IsEmpty())
		m_pParent->SetMailHostName(name);
	else
		strncpy(name, LPCTSTR(m_szServerName), m_szServerName.GetLength());

	int nCheckTime	= 0;
	XP_Bool bRememberPassword = IsDlgButtonChecked(IDC_CHECK_PASSWORD);
	XP_Bool bCheckMail = IsDlgButtonChecked(IDC_CHECK_MAIL);
	if (GetDlgItemText(IDC_EDIT_CHECK_MAIL, text, MAX_DESCRIPTION_LEN)) 
		nCheckTime = atoi(text);

	if (m_pParent->IsPopServer())
	{
		PREF_SetIntPref("mail.server_type", (int32)TYPE_POP);
		PREF_SetCharPref("network.hosts.pop_server", LPCTSTR(m_szServerName));
		PREF_SetCharPref("mail.pop_name", userName);
		PREF_SetBoolPref("mail.remember_password",bRememberPassword);
		PREF_SetBoolPref("mail.check_new_mail", bCheckMail);
		PREF_SetIntPref("mail.check_time", (int32)nCheckTime);
	}
	else
	{
		if (!m_pParent->EditServer())
		{
			XP_Bool bIsSecure = m_pParent->GetIMAPUseSSL();
			XP_Bool bOverrideNamespaces = m_pParent->GetIMAPOverrideNameSpaces();
			char personalDir[256];
			char publicDir[256];
			char othersDir[256];
			m_pParent->GetIMAPPersonalDir(personalDir, 255);
			m_pParent->GetIMAPPublicDir(publicDir, 255);
			m_pParent->GetIMAPOthersDir(othersDir, 255);

			MSG_IMAPHost* pHost = MSG_CreateIMAPHost(WFE_MSGGetMaster(),
											name,
											bIsSecure, 
											userName,
											bCheckMail,
											nCheckTime,
											bRememberPassword,
											FALSE,
											bOverrideNamespaces, 
											personalDir,
											publicDir, 
											othersDir 
											);
		}

		PREF_SetIntPref("mail.server_type", (int32)TYPE_IMAP);
		IMAP_SetCharPref(LPCTSTR(m_szServerName), CHAR_USERNAME, userName);
		IMAP_SetBoolPref(LPCTSTR(m_szServerName), BOOL_CHECK_NEW_MAIL, bCheckMail);
		IMAP_SetIntPref(LPCTSTR(m_szServerName), INT_CHECK_TIME, (int32)nCheckTime);
		IMAP_SetBoolPref(LPCTSTR(m_szServerName), BOOL_REMEMBER_PASSWORD, bRememberPassword);
	}
	return TRUE;
}

void CGeneralServerPage::OnChangeServerType()
{
	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO_TYPE);
	int nType = (int)pCombo->GetItemData(pCombo->GetCurSel());
	if ((m_pParent->IsPopServer() && nType == TYPE_POP) ||
		(!m_pParent->IsPopServer() && nType == TYPE_IMAP))
		return;
	m_pParent->ShowHidePages(nType);
}

BEGIN_MESSAGE_MAP(CGeneralServerPage, CPropertyPage)
	ON_CBN_SELCHANGE(IDC_COMBO_TYPE, OnChangeServerType)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPopServerPage
//
CPopServerPage::CPopServerPage(CWnd *pParent)
	: CPropertyPage(IDD)
{
}

BOOL CPopServerPage::OnInitDialog()
{
	BOOL ret = CPropertyPage::OnInitDialog();

	XP_Bool prefBool = FALSE;
	PREF_GetBoolPref("mail.leave_on_server",&prefBool);
	if (prefBool)
		CheckDlgButton(IDC_CHECK_POP_REMOTE, TRUE);
	else 
		CheckDlgButton(IDC_CHECK_POP_REMOTE, FALSE);

	if (PREF_PrefIsLocked("mail.leave_on_server")) 
		GetDlgItem(IDC_CHECK_POP_REMOTE)->EnableWindow(FALSE);
 
	return ret;
}

void CPopServerPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CPopServerPage::ProcessOK()
{
	if (IsDlgButtonChecked(IDC_CHECK_POP_REMOTE))
		PREF_SetBoolPref("mail.leave_on_server", TRUE);
	else
		PREF_SetBoolPref("mail.leave_on_server", FALSE);
	return TRUE;
}

BEGIN_MESSAGE_MAP(CPopServerPage, CPropertyPage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CIMAPServerPage
//
CIMAPServerPage::CIMAPServerPage(CWnd *pParent, const char* pName)
	: CPropertyPage(IDD)
{
    m_pParent = (CMailServerPropertySheet*)pParent;
	m_szServerName = pName;
}

BOOL CIMAPServerPage::OnInitDialog()
{
	BOOL ret = CPropertyPage::OnInitDialog();

	XP_Bool bOfflineDownload = FALSE;
	XP_Bool bMoveToTrash = FALSE;
	XP_Bool bUseSSL = FALSE;

	IMAP_GetBoolPref(LPCTSTR(m_szServerName), BOOL_OFFLINE_DOWNLOAD, &bOfflineDownload);
	IMAP_GetBoolPref(LPCTSTR(m_szServerName), BOOL_MOVE_TO_TRASH, &bMoveToTrash);
	IMAP_GetBoolPref(LPCTSTR(m_szServerName), BOOL_IS_SECURE, &bUseSSL);
	CheckDlgButton(IDC_CHECK_IMAP_LOCAL, bOfflineDownload);
	CheckDlgButton(IDC_CHECK_IMAP_TRASH, bMoveToTrash);
	CheckDlgButton(IDC_CHECK_IMAP_SSL, bUseSSL);

	if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), BOOL_OFFLINE_DOWNLOAD)) 
		GetDlgItem(IDC_CHECK_IMAP_LOCAL)->EnableWindow(FALSE);
	if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), BOOL_MOVE_TO_TRASH)) 
		GetDlgItem(IDC_CHECK_IMAP_TRASH)->EnableWindow(FALSE);
	if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), BOOL_IS_SECURE)) 
		GetDlgItem(IDC_CHECK_IMAP_SSL)->EnableWindow(FALSE);

	return ret;
}

XP_Bool CIMAPServerPage::GetUseSSL()
{
	return (IsDlgButtonChecked(IDC_CHECK_IMAP_SSL) == 0 ? FALSE : TRUE);
}

void CIMAPServerPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CIMAPServerPage::ProcessOK()
{
	XP_Bool bOfflineDownload = IsDlgButtonChecked(IDC_CHECK_IMAP_LOCAL);
	XP_Bool bMoveToTrash = IsDlgButtonChecked(IDC_CHECK_IMAP_TRASH);
	XP_Bool bUseSSL = IsDlgButtonChecked(IDC_CHECK_IMAP_SSL);

	IMAP_SetBoolPref(LPCTSTR(m_szServerName), BOOL_OFFLINE_DOWNLOAD, bOfflineDownload);
	IMAP_SetBoolPref(LPCTSTR(m_szServerName), BOOL_MOVE_TO_TRASH, bMoveToTrash);
	IMAP_SetBoolPref(LPCTSTR(m_szServerName), BOOL_IS_SECURE, bUseSSL);
	return TRUE;
}

BEGIN_MESSAGE_MAP(CIMAPServerPage, CPropertyPage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CIMAPAdvancedPage
//
CIMAPAdvancedPage::CIMAPAdvancedPage(CWnd *pParent, const char* pName)
	: CPropertyPage(IDD)
{
    m_pParent = (CMailServerPropertySheet*)pParent;
	m_szServerName = pName;
}

BOOL CIMAPAdvancedPage::OnInitDialog()
{
	BOOL ret = CPropertyPage::OnInitDialog();

	XP_Bool bOverrideNamespaces = TRUE;

	char* pPersonalDir = NULL;
	char* pPublicDir = NULL;
	char* pOtherUserDir = NULL;
	IMAP_GetCharPref(LPCTSTR(m_szServerName), CHAR_PERAONAL_DIR, &pPersonalDir);
	if (pPersonalDir)
	{
		SetDlgItemText(IDC_EDIT_IMAP_DIR, pPersonalDir);
		XP_FREE(pPersonalDir);
	}
	IMAP_GetCharPref(LPCTSTR(m_szServerName), CHAR_PUBLIC_DIR, &pPublicDir);
	if (pPublicDir)
	{
		SetDlgItemText(IDC_EDIT_PUBLIC_DIR, pPublicDir);
		XP_FREE(pPublicDir);
	}
	IMAP_GetCharPref(LPCTSTR(m_szServerName), CHAR_OTHER_USER_DIR, &pOtherUserDir);
	if (pOtherUserDir)
	{
		SetDlgItemText(IDC_EDIT_OTHERS_DIR, pOtherUserDir);
		XP_FREE(pOtherUserDir);
	}
	IMAP_GetBoolPref(LPCTSTR(m_szServerName), BOOL_OVERRIDE_NAMESPACES, &bOverrideNamespaces);
	CheckDlgButton(IDC_CHECK_OVERRIDE, bOverrideNamespaces);

	if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), CHAR_PERAONAL_DIR)) 
		GetDlgItem(IDC_EDIT_IMAP_DIR)->EnableWindow(FALSE);
	if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), CHAR_PUBLIC_DIR)) 
		GetDlgItem(IDC_EDIT_PUBLIC_DIR)->EnableWindow(FALSE);
	if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), CHAR_OTHER_USER_DIR)) 
		GetDlgItem(IDC_EDIT_OTHERS_DIR)->EnableWindow(FALSE);
	if (IMAP_PrefIsLocked(LPCTSTR(m_szServerName), BOOL_OVERRIDE_NAMESPACES)) 
		GetDlgItem(IDC_CHECK_OVERRIDE)->EnableWindow(FALSE);

	return ret;
}

void CIMAPAdvancedPage::GetPersonalDir(char* pDir, int nLen)
{
	if (0 == GetDlgItemText(IDC_EDIT_IMAP_DIR, pDir, nLen))
		;
}

void CIMAPAdvancedPage::GetPublicDir(char* pDir, int nLen)
{
	if (0 == GetDlgItemText(IDC_EDIT_PUBLIC_DIR, pDir, nLen))
		;
}

void CIMAPAdvancedPage::GetOthersDir(char* pDir, int nLen)
{
	if (0 == GetDlgItemText(IDC_EDIT_OTHERS_DIR, pDir, nLen))
		;
}

XP_Bool CIMAPAdvancedPage::GetOverrideNameSpaces()
{
	return (IsDlgButtonChecked(IDC_CHECK_OVERRIDE) == 0 ? FALSE : TRUE);
}

void CIMAPAdvancedPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CIMAPAdvancedPage::ProcessOK()
{
    char directory[256];
	if (0 == GetDlgItemText(IDC_EDIT_IMAP_DIR, directory, 255))
		IMAP_SetCharPref(LPCTSTR(m_szServerName), CHAR_PERAONAL_DIR, "");
	else
		IMAP_SetCharPref(LPCTSTR(m_szServerName), CHAR_PERAONAL_DIR, directory);
	if (0 == GetDlgItemText(IDC_EDIT_PUBLIC_DIR, directory, 255))
		IMAP_SetCharPref(LPCTSTR(m_szServerName), CHAR_PUBLIC_DIR, "");
	else
		IMAP_SetCharPref(LPCTSTR(m_szServerName), CHAR_PUBLIC_DIR, directory);
	if (0 == GetDlgItemText(IDC_EDIT_OTHERS_DIR, directory, 255))
		IMAP_SetCharPref(LPCTSTR(m_szServerName), CHAR_OTHER_USER_DIR, "");
	else
		IMAP_SetCharPref(LPCTSTR(m_szServerName), CHAR_OTHER_USER_DIR, directory);

	XP_Bool bOverrideNamespaces = IsDlgButtonChecked(IDC_CHECK_OVERRIDE);
	IMAP_SetBoolPref(LPCTSTR(m_szServerName), BOOL_OVERRIDE_NAMESPACES, bOverrideNamespaces);
	return TRUE;
}

BEGIN_MESSAGE_MAP(CIMAPAdvancedPage, CPropertyPage)
END_MESSAGE_MAP()

#endif /* MOZ_MAIL_NEWS */
