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

#include "pch.h"
#include <assert.h>
#include "dllcom.h"
#include "pages.h"
#include "resource.h"
#include "xp_core.h"
#include "xp_help.h"
#include "net.h"
#include "prefapi.h"
#include "brprefid.h"
#ifdef MOZ_SMARTUPDATE
#include "VerReg.h"
#endif /* MOZ_SMARTUPDATE */
#ifdef _WIN32
#include <shlobj.h>
#endif

//Bug# 61956 disable mailnews prefs lock check for B4
//also in mnpref\src\pages.h, winfe\mnprefs.cpp
//enable for B6
#define MNPREF_PrefIsLocked(pPrefName) PREF_PrefIsLocked(pPrefName)

/////////////////////////////////////////////////////////////////////////////
// Helper functions

static int NEAR
PREF_GetUnsignedPref(LPCSTR lpszPref, unsigned *puVal)
{
	int32	n;
	int		nResult;

	nResult = PREF_GetIntPref(lpszPref, &n);
	*puVal = (unsigned)n;
	return nResult;
}

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPrefs implementation

CAdvancedPrefs::CAdvancedPrefs()
	: CBrowserPropertyPage(IDD_ADVANCED, HELP_PREFS_ADVANCED)
{
}

BOOL
CAdvancedPrefs::InitDialog()
{
	// Check for locked preferences
	CheckIfLockedPref("general.always_load_images", IDC_CHECK1);
	CheckIfLockedPref("security.enable_java", IDC_CHECK2);
	CheckIfLockedPref("javascript.enabled", IDC_CHECK3);
	CheckIfLockedPref("browser.enable_style_sheets", IDC_CHECK4);
	CheckIfLockedPref("security.email_as_ftp_password", IDC_CHECK6);

	if (PREF_PrefIsLocked("network.cookie.cookieBehavior")) {
		// Disable all the radio buttons in the group
		DisableRadioButtonGroup(IDC_RADIO1);
	}

	CheckIfLockedPref("network.cookie.warnAboutCookies", IDC_CHECK7);

	return CBrowserPropertyPage::InitDialog();
}

// Initialize member data using XP preferences
STDMETHODIMP
CAdvancedPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		PREF_GetBoolPref("general.always_load_images", &m_bAutoLoadImages);
		PREF_GetBoolPref("security.enable_java", &m_bEnableJava);
		PREF_GetBoolPref("javascript.enabled", &m_bEnableJavaScript);
		PREF_GetBoolPref("browser.enable_style_sheets", &m_bEnableStyleSheets);
		PREF_GetBoolPref("security.email_as_ftp_password", &m_bSendEmailAddressForFTPPassword);

		int32	n;

		PREF_GetIntPref("network.cookie.cookieBehavior", &n);
		m_nCookieAcceptance = (int)n;

		PREF_GetBoolPref("network.cookie.warnAboutCookies", &m_bWarnAboutCookies);
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CAdvancedPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	CheckBoxTransfer(IDC_CHECK1, m_bAutoLoadImages, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK2, m_bEnableJava, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK3, m_bEnableJavaScript, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK4, m_bEnableStyleSheets, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK6, m_bSendEmailAddressForFTPPassword, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO1, m_nCookieAcceptance, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK7, m_bWarnAboutCookies, bSaveAndValidate);
	return TRUE;
}

// Apply changes using XP preferences
BOOL
CAdvancedPrefs::ApplyChanges()
{
	PREF_SetBoolPref("general.always_load_images", m_bAutoLoadImages);
	PREF_SetBoolPref("security.enable_java", m_bEnableJava);
	PREF_SetBoolPref("javascript.enabled", m_bEnableJavaScript);
	PREF_SetBoolPref("browser.enable_style_sheets", m_bEnableStyleSheets);
	PREF_SetBoolPref("security.email_as_ftp_password", m_bSendEmailAddressForFTPPassword);
	PREF_SetIntPref("network.cookie.cookieBehavior", (int32)m_nCookieAcceptance);
	PREF_SetBoolPref("network.cookie.warnAboutCookies", m_bWarnAboutCookies);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CCachePrefs implementation

CCachePrefs::CCachePrefs()
#ifdef _WIN32
	: CBrowserPropertyPage(IDD_CACHE, HELP_PREFS_ADVANCED_CACHE)
#else
	: CBrowserPropertyPage(IDD_CACHE16, HELP_PREFS_ADVANCED_CACHE)
#endif
{
}

// Initialize member data using XP preferences
STDMETHODIMP
CCachePrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		int32	n;
		
		PREF_GetIntPref("browser.cache.disk_cache_size", &n);
		m_uDiskCacheSize = (UINT)n;

		PREF_GetIntPref("browser.cache.memory_cache_size", &n);
		m_uMemoryCacheSize = (UINT)n;

		PREF_GetStringPref("browser.cache.directory", m_strDiskCacheDir);
		
		PREF_GetIntPref("browser.cache.check_doc_frequency", &n);
		m_nCheckDocFrequency = (int)n;
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CCachePrefs::DoTransfer(BOOL bSaveAndValidate)
{
	EditFieldTransfer(IDC_EDIT1, m_uDiskCacheSize, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT2, m_uMemoryCacheSize, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT3, m_strDiskCacheDir, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO1, m_nCheckDocFrequency, bSaveAndValidate);
	return TRUE;
}

// Apply changes using XP preferences
BOOL
CCachePrefs::ApplyChanges()
{
	PREF_SetIntPref("browser.cache.disk_cache_size", (int32)m_uDiskCacheSize);
	PREF_SetIntPref("browser.cache.memory_cache_size", (int32)m_uMemoryCacheSize);
	PREF_SetCharPref("browser.cache.directory", (LPCSTR)m_strDiskCacheDir);
	PREF_SetIntPref("browser.cache.check_doc_frequency", (int32)m_nCheckDocFrequency);
	return TRUE;
}

BOOL
CCachePrefs::InitDialog()
{
	// Limit the length of the cache size fields
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT1), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT2), 5);

	// Check for locked preferences
	CheckIfLockedPref("browser.cache.disk_cache_size", IDC_EDIT1);
	CheckIfLockedPref("browser.cache.memory_cache_size", IDC_EDIT2);
	if (CheckIfLockedPref("browser.cache.directory", IDC_EDIT3)) {
		// Disable Choose Folder button
		EnableDlgItem(IDC_BUTTON3, FALSE);
	}

	if (PREF_PrefIsLocked("browser.cache.check_doc_frequency")) {
		// Disable all the radio buttons in the group
		DisableRadioButtonGroup(IDC_RADIO1);
	}

	return CBrowserPropertyPage::InitDialog();
}

#ifdef _WIN32
BOOL
CCachePrefs::BrowseForCacheFolder(LPSTR lpszPath)
{
	char			szDisplayName[MAX_PATH];
	BROWSEINFO		bi;
	LPITEMIDLIST	lpidl;
	CString			strTitle;

	strTitle.LoadString(m_hInstance, IDS_CHOOSE_CACHE);

	// Initialize the browse info structure
	memset(&bi, 0, sizeof(bi));
	bi.hwndOwner = GetParent(m_hwndDlg);
	bi.pszDisplayName = szDisplayName;
	bi.lpszTitle = (LPCSTR)strTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS;

	// Display the dialog box to select a shell folder
	lpidl = SHBrowseForFolder(&bi);
	if (lpidl) {
		LPMALLOC	pMalloc;

		// Get the file system path
		SHGetPathFromIDList(lpidl, lpszPath);

		// Free the identifier list
		SHGetMalloc(&pMalloc);
		assert(pMalloc->DidAlloc(lpidl));
		pMalloc->Free(lpidl);
		pMalloc->Release();
		return TRUE;
	}

	return FALSE;
}
#endif

BOOL
CCachePrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_BUTTON1 && notifyCode == BN_CLICKED) {
		CString	strText, strCaption;
		int32	nSize;

		strText.LoadString(m_hInstance, IDS_CONTINUE_CLEAR_CACHE);
		strCaption.LoadString(m_hInstance, IDS_DISK_CACHE);

		if (MessageBox(GetParent(m_hwndDlg), (LPCSTR)strText, (LPCSTR)strCaption, MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
			PREF_GetIntPref("browser.cache.disk_cache_size", &nSize);
			PREF_SetIntPref("browser.cache.disk_cache_size", 0);
			PREF_SetIntPref("browser.cache.disk_cache_size", nSize);
		}
		return TRUE;

	} else if (id == IDC_BUTTON2 && notifyCode == BN_CLICKED) {
		CString	strText, strCaption;
		int32	nSize;

		strText.LoadString(m_hInstance, IDS_CONTINUE_CLEAR_MEM_CACHE);
		strCaption.LoadString(m_hInstance, IDS_MEMORY_CACHE);

		if (MessageBox(GetParent(m_hwndDlg), (LPCSTR)strText, (LPCSTR)strCaption, MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
			PREF_GetIntPref("browser.cache.memory_cache_size", &nSize);
			PREF_SetIntPref("browser.cache.memory_cache_size", 0);
			PREF_SetIntPref("browser.cache.memory_cache_size", nSize);
		}
		return TRUE;

#ifdef _WIN32
	} else if (id == IDC_BUTTON3 && notifyCode == BN_CLICKED) {
		char	szPath[_MAX_PATH];

		if (BrowseForCacheFolder(szPath))
			Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT3), szPath);
#endif
	}

	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CViewProxiesDialog implementation

class CViewProxiesDialog : public CDialog {
	public:
		CViewProxiesDialog();

	protected:
		BOOL	InitDialog();
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		
		// Event processing
		void	OnOK();

	private:
		CString		m_strHTTPServer;
		unsigned	m_nHTTPPort;
		CString		m_strSSLServer;
		unsigned	m_nSSLPort;
		CString		m_strFTPServer;
		unsigned	m_nFTPPort;
		CString		m_strSocksServer;
		unsigned	m_nSocksPort;
		CString		m_strGopherServer;
		unsigned	m_nGopherPort;
		CString		m_strWAISServer;
		unsigned	m_nWAISPort;
		CString		m_strNoProxiesOn;
};

CViewProxiesDialog::CViewProxiesDialog()
	: CDialog(CComDll::m_hInstance, IDD_VIEW_PROXIES)
{
	// Set member data using XP preferences
	PREF_GetStringPref("network.proxy.http", m_strHTTPServer);
	PREF_GetUnsignedPref("network.proxy.http_port", &m_nHTTPPort);

	PREF_GetStringPref("network.proxy.ssl", m_strSSLServer);
	PREF_GetUnsignedPref("network.proxy.ssl_port", &m_nSSLPort);
	
	PREF_GetStringPref("network.proxy.ftp", m_strFTPServer);
	PREF_GetUnsignedPref("network.proxy.ftp_port", &m_nFTPPort);
	
	PREF_GetStringPref("network.hosts.socks_server", m_strSocksServer);
	PREF_GetUnsignedPref("network.hosts.socks_serverport", &m_nSocksPort);
	
	PREF_GetStringPref("network.proxy.gopher", m_strGopherServer);
	PREF_GetUnsignedPref("network.proxy.gopher_port", &m_nGopherPort);
	
	PREF_GetStringPref("network.proxy.wais", m_strWAISServer);
	PREF_GetUnsignedPref("network.proxy.wais_port", &m_nWAISPort);
	
	PREF_GetStringPref("network.proxy.no_proxies_on", m_strNoProxiesOn);
}

BOOL
CViewProxiesDialog::InitDialog()
{
	// Limit the length of the port numbers
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT2), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT4), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT6), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT8), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT10), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT12), 5);
	
	//  Disable any locked prefs.
	if(PREF_PrefIsLocked("network.proxy.http")) {
		EnableDlgItem(IDC_EDIT1, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.http_port")) {
		EnableDlgItem(IDC_EDIT2, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.ssl")) {
		EnableDlgItem(IDC_EDIT3, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.ssl_port")) {
		EnableDlgItem(IDC_EDIT4, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.ftp")) {
		EnableDlgItem(IDC_EDIT5, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.ftp_port")) {
		EnableDlgItem(IDC_EDIT6, FALSE);
	}
	if(PREF_PrefIsLocked("network.hosts.socks_server")) {
		EnableDlgItem(IDC_EDIT7, FALSE);
	}
	if(PREF_PrefIsLocked("network.hosts.socks_serverport")) {
		EnableDlgItem(IDC_EDIT8, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.gopher")) {
		EnableDlgItem(IDC_EDIT9, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.gopher_port")) {
		EnableDlgItem(IDC_EDIT10, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.wais")) {
		EnableDlgItem(IDC_EDIT11, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.wais_port")) {
		EnableDlgItem(IDC_EDIT12, FALSE);
	}
	if(PREF_PrefIsLocked("network.proxy.no_proxies_on")) {
		EnableDlgItem(IDC_EDIT13, FALSE);
	}

	return CDialog::InitDialog();
}

BOOL
CViewProxiesDialog::DoTransfer(BOOL bSaveAndValidate)
{
	EditFieldTransfer(IDC_EDIT1, m_strHTTPServer, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT2, m_nHTTPPort, bSaveAndValidate);

	EditFieldTransfer(IDC_EDIT3, m_strSSLServer, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT4, m_nSSLPort, bSaveAndValidate);

	EditFieldTransfer(IDC_EDIT5, m_strFTPServer, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT6, m_nFTPPort, bSaveAndValidate);
	
	EditFieldTransfer(IDC_EDIT7, m_strSocksServer, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT8, m_nSocksPort, bSaveAndValidate);
	
	EditFieldTransfer(IDC_EDIT9, m_strGopherServer, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT10, m_nGopherPort, bSaveAndValidate);
	
	EditFieldTransfer(IDC_EDIT11, m_strWAISServer, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT12, m_nWAISPort, bSaveAndValidate);
	
	EditFieldTransfer(IDC_EDIT13, m_strNoProxiesOn, bSaveAndValidate);
	return TRUE;
}

void
CViewProxiesDialog::OnOK()
{
	// Transfer data from the dialog and then destroy the dialog window
	CDialog::OnOK();

	// Apply changes using XP preferences
	PREF_SetCharPref("network.proxy.http", m_strHTTPServer);
	PREF_SetIntPref("network.proxy.http_port", (int32)m_nHTTPPort);

	PREF_SetCharPref("network.proxy.ssl", m_strSSLServer);
	PREF_SetIntPref("network.proxy.ssl_port", (int32)m_nSSLPort);
	
	PREF_SetCharPref("network.proxy.ftp", m_strFTPServer);
	PREF_SetIntPref("network.proxy.ftp_port", (int32)m_nFTPPort);
	
	PREF_SetCharPref("network.hosts.socks_server", m_strSocksServer);
	PREF_SetIntPref("network.hosts.socks_serverport", (int32)m_nSocksPort);
	
	PREF_SetCharPref("network.proxy.gopher", m_strGopherServer);
	PREF_SetIntPref("network.proxy.gopher_port", (int32)m_nGopherPort);
	
	PREF_SetCharPref("network.proxy.wais", m_strWAISServer);
	PREF_SetIntPref("network.proxy.wais_port", (int32)m_nWAISPort);
	
	PREF_SetCharPref("network.proxy.no_proxies_on", m_strNoProxiesOn);
}

/////////////////////////////////////////////////////////////////////////////
// CProxiesPrefs implementation

CProxiesPrefs::CProxiesPrefs()
	: CBrowserPropertyPage(IDD_PROXIES, HELP_PREFS_ADVANCED_PROXIES)
{
}

BOOL
CProxiesPrefs::InitDialog()
{
	// Check for locked preferences
	if (PREF_PrefIsLocked("network.proxy.type")) {
		// Disable all the radio buttons in the group
		DisableRadioButtonGroup(IDC_RADIO1);

		// Disable the View button to manually configure proxies
		EnableDlgItem(IDC_BUTTON1, FALSE);

		// Disable edit field for the auto proxy config URL
		EnableDlgItem(IDC_EDIT1, FALSE);
	}

	return CBrowserPropertyPage::InitDialog();
}

// Initialize member data using XP preferences
STDMETHODIMP
CProxiesPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		int32	n;

		PREF_GetIntPref("network.proxy.type", &n);

		// Map from netlib values
		if (n == PROXY_STYLE_MANUAL)
			m_nProxyType = 1;
		else if (n == PROXY_STYLE_AUTOMATIC)
			m_nProxyType = 2;
		else
			m_nProxyType = 0;

		PREF_GetStringPref("network.proxy.autoconfig_url", m_strAutoConfigURL);
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CProxiesPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	RadioButtonTransfer(IDC_RADIO1, m_nProxyType, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT1, m_strAutoConfigURL, bSaveAndValidate);

	if (!bSaveAndValidate)
		EnableControls();

	return TRUE;
}

// Apply changes using XP preferences
BOOL
CProxiesPrefs::ApplyChanges()
{
	int32	n;

	// Convert to netlib values
	if (m_nProxyType == 0)
		n = PROXY_STYLE_NONE;
	else if (m_nProxyType == 1)
		n = PROXY_STYLE_MANUAL;
	else {
		assert(m_nProxyType == 2);
		n = PROXY_STYLE_AUTOMATIC;
	}
	PREF_SetIntPref("network.proxy.type", n);

	PREF_SetCharPref("network.proxy.autoconfig_url", (LPCSTR)m_strAutoConfigURL);
	return TRUE;
}

void
CProxiesPrefs::EnableControls()
{
	int	nProxyType;

	// Enable the buttons and edit field as appropriate
	RadioButtonTransfer(IDC_RADIO1, nProxyType, TRUE);

	EnableDlgItem(IDC_BUTTON1, nProxyType == 1);
	EnableDlgItem(IDC_EDIT1, nProxyType == 2);
	EnableDlgItem(IDC_BUTTON2, nProxyType == 2);
}

BOOL
CProxiesPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_BUTTON1 && notifyCode == BN_CLICKED) {
		CViewProxiesDialog	dlg;

		// Let the user manually configure the proxies
		dlg.DoModal(GetParent(m_hwndDlg));
		return TRUE;

	} else if (id == IDC_BUTTON2 && notifyCode == BN_CLICKED) {
		CString	strAutoConfigURL;

		// Reload the auto-configuration proxy
		PREF_GetStringPref("network.proxy.autoconfig_url", strAutoConfigURL);
		PREF_SetCharPref("network.proxy.autoconfig_url", "");
		PREF_SetCharPref("network.proxy.autoconfig_url", strAutoConfigURL);

	} else if (id == IDC_RADIO1 || id == IDC_RADIO2 || id == IDC_RADIO3) {
		if (notifyCode == BN_CLICKED)
			EnableControls();
	}

	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CDiskSpacePrefs implementation

BOOL IsNumeric(char* pStr)
{
    
	for (char* p = pStr; p && *p; p++)
	{
        if (0 == isdigit(*p))
            return FALSE; 
	}
	return TRUE;
}

void ValidateNumeric
(HWND hParent, HINSTANCE hInst, int nCaptionID, int nID, int& data)
{
	HWND	hwndCtl = GetDlgItem(hParent, nID);
	char	szBuf[32];
	DWORD	dwStart, dwEnd;

	assert(hwndCtl);
	Edit_GetText(hwndCtl, szBuf, sizeof(szBuf));
	SendMessage(hwndCtl, EM_GETSEL, (WPARAM)(LPDWORD)&dwStart, 
			    (LPARAM)(LPDWORD)&dwEnd);
	if (!IsNumeric(szBuf))
	{
		CString	strText, strCaption;

		strText.LoadString(hInst, IDS_NUMERIC_ONLY);
		strCaption.LoadString(hInst, nCaptionID);

		MessageBeep(MB_ICONHAND);
		MessageBox(GetParent(hParent), (LPCSTR)strText, (LPCSTR)strCaption, 
			       MB_OK | MB_ICONSTOP);

		wsprintf(szBuf, "%i", data);
		Edit_SetText(hwndCtl, szBuf);
		Edit_SetSel(hwndCtl, dwStart - 1, dwEnd - 1);
	}
	else
	{
		data = atoi(szBuf);
	}
}

CDiskSpacePrefs::CDiskSpacePrefs()
	: CBrowserPropertyPage(IDD_DISK_SPACE, HELP_PREFS_ADVANCED_DISK_SPACE)
{
}

// Initialize member data using XP preferences
STDMETHODIMP
CDiskSpacePrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		int32	n;

		PREF_GetBoolPref("mail.limit_message_size", &m_bLimitSize);
		PREF_GetIntPref("mail.max_size", &n);
		m_nLimitSize = (int)n;

		PREF_GetBoolPref("mail.prompt_purge_threshhold", &m_bPromptPurge);
		PREF_GetIntPref("mail.purge_threshhold", &n);
		m_nPurgeSize = (int)n;

		PREF_GetIntPref("news.keep.method", &n);

		// ui radio buttons do not correspond to the preference
		if (n == 0)
			m_nKeepMethod = 1;
		else if (n == 1)
			m_nKeepMethod = 0;
		else
			m_nKeepMethod = n;

		PREF_GetIntPref("news.keep.days", &n);
		m_nKeepDays = (int)n;
		PREF_GetIntPref("news.keep.count", &n);
		m_nKeepCounts = (int)n;

		PREF_GetBoolPref("news.keep.only_unread", &m_bKeepUnread);
		PREF_GetBoolPref("news.remove_bodies.by_age", &m_bRemoveBody);
		PREF_GetIntPref("news.remove_bodies.days", &n);
		m_nRemoveDays = (int)n;
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL 
CDiskSpacePrefs::InitDialog()
{
	// Limit the length of the string the user can type
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_MSG_SIZE), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_COMPACT_SIZE), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_MSG_DAYS), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_MSG_COUNT), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_MSG_DAYS), 5);

	// Check for locked preferences
	if (MNPREF_PrefIsLocked("mail.limit_message_size"))
		EnableDlgItem(IDC_CHECK_MSG_SIZE, FALSE);
	if (MNPREF_PrefIsLocked("mail.max_size"))
		EnableDlgItem(IDC_EDIT_MSG_SIZE, FALSE);
	if (MNPREF_PrefIsLocked("mail.prompt_purge_threshhold"))
		EnableDlgItem(IDC_CHECK_COMPACT_SIZE, FALSE);
	if (MNPREF_PrefIsLocked("mail.purge_threshhold"))
		EnableDlgItem(IDC_EDIT_COMPACT_SIZE, FALSE);

	if (MNPREF_PrefIsLocked("news.keep.method")) {
		// Disable all the radio buttons in the group
		DisableRadioButtonGroup(IDC_KEEP_MSG_DAYS);
	}

	if (MNPREF_PrefIsLocked("news.keep.days"))
		EnableDlgItem(IDC_EDIT_MSG_DAYS, FALSE);
	if (MNPREF_PrefIsLocked("news.keep.count"))
		EnableDlgItem(IDC_EDIT_MSG_COUNT, FALSE);
	if (MNPREF_PrefIsLocked("news.keep.only_unread"))
		EnableDlgItem(IDC_KEEP_UNREAD, FALSE);
	
	if (MNPREF_PrefIsLocked("news.remove_bodies.by_age"))
		EnableDlgItem(IDC_CHECK_MSG_DAYS, FALSE);
	if (MNPREF_PrefIsLocked("news.remove_bodies.days"))
		EnableDlgItem(IDC_EDIT_MSG_DAYS, FALSE);

	return CBrowserPropertyPage::InitDialog();
}

BOOL
CDiskSpacePrefs::DoTransfer(BOOL bSaveAndValidate)
{
	CheckBoxTransfer(IDC_CHECK_MSG_SIZE, m_bLimitSize, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_MSG_SIZE, m_nLimitSize, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_COMPACT_SIZE, m_bPromptPurge, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_COMPACT_SIZE, m_nPurgeSize, bSaveAndValidate);
	RadioButtonTransfer(IDC_KEEP_MSG_DAYS, m_nKeepMethod, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_MSG_DAYS, m_nKeepDays, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_MSG_COUNT, m_nKeepCounts, bSaveAndValidate);
	CheckBoxTransfer(IDC_KEEP_UNREAD, m_bKeepUnread, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_MSG_DAYS, m_bRemoveBody, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_MSG_DAYS, m_nRemoveDays, bSaveAndValidate);

	return TRUE;
}

// Apply changes using XP preferences
BOOL
CDiskSpacePrefs::ApplyChanges()
{
	PREF_SetBoolPref("mail.limit_message_size", m_bLimitSize);
	PREF_SetIntPref("mail.max_size", (int32)m_nLimitSize);

	PREF_SetBoolPref("mail.prompt_purge_threshhold", m_bPromptPurge);
	PREF_SetIntPref("mail.purge_threshhold", (int32)m_nPurgeSize);

	// ui radio buttons do not correspond to the preference
	int32	realKeepMethod;
	if (m_nKeepMethod == 0)
		realKeepMethod = 1;
	else if (m_nKeepMethod == 1)
		realKeepMethod = 0;
	else
		realKeepMethod = m_nKeepMethod;

	PREF_SetIntPref("news.keep.method", (int32)realKeepMethod);
	PREF_SetIntPref("news.keep.days", (int32)m_nKeepDays);
	PREF_SetIntPref("news.keep.count", (int32)m_nKeepCounts);

	PREF_SetBoolPref("news.keep.only_unread", m_bKeepUnread);
	PREF_SetBoolPref("news.remove_bodies.by_age", m_bRemoveBody);
	PREF_SetIntPref("news.remove_bodies.days", (int32)m_nRemoveDays);

	return TRUE;
}

BOOL
CDiskSpacePrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	switch (id)
	{
		case IDC_EDIT_MSG_SIZE:
			if (notifyCode == EN_UPDATE)
				ValidateNumeric(m_hwndDlg, m_hInstance, IDS_DISK_SPACE,
				                id, m_nLimitSize);
			return TRUE;

		case IDC_EDIT_COMPACT_SIZE:
			if (notifyCode == EN_UPDATE)
				ValidateNumeric(m_hwndDlg, m_hInstance, IDS_DISK_SPACE, 
				                id, m_nPurgeSize);
			return TRUE;

		case IDC_EDIT_MSG_DAYS:
			if (notifyCode == EN_UPDATE)
				ValidateNumeric(m_hwndDlg, m_hInstance, IDS_DISK_SPACE, 
				                id, m_nKeepDays);
			return TRUE;

		case IDC_EDIT_MSG_COUNT:
			if (notifyCode == EN_UPDATE)
				ValidateNumeric(m_hwndDlg, m_hInstance, IDS_DISK_SPACE, 
				                id, m_nKeepCounts);
			return TRUE;
		case IDC_EDIT_MSG_DAYS2:
			if (notifyCode == EN_UPDATE)
				ValidateNumeric(m_hwndDlg, m_hInstance, IDC_EDIT_MSG_DAYS, 
				                id, m_nRemoveDays);
			return TRUE;
	}
	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

#ifdef MOZ_SMARTUPDATE

/////////////////////////////////////////////////////////////////////////////
// CSmartUpdatePrefs implementation

CSmartUpdatePrefs::CSmartUpdatePrefs()
	: CBrowserPropertyPage(IDD_SMARTUPDATE, HELP_PREFS_ADVANCED_SMARTUPDATE)
{
}


// Initialize member data using XP preferences
STDMETHODIMP
CSmartUpdatePrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
        PREF_GetBoolPref("autoupdate.enabled", &m_bEnableAutoInstall);
        PREF_GetBoolPref("autoupdate.confirm_install", &m_bEnableConfirmInstall);
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CSmartUpdatePrefs::DoTransfer(BOOL bSaveAndValidate)
{
    CheckBoxTransfer(IDC_CHECK1, m_bEnableAutoInstall, bSaveAndValidate);
    CheckBoxTransfer(IDC_CHECK2, m_bEnableConfirmInstall, bSaveAndValidate);
  	return TRUE;
}

// Apply changes using XP preferences
BOOL
CSmartUpdatePrefs::ApplyChanges()
{
    PREF_SetBoolPref("autoupdate.enabled", m_bEnableAutoInstall);
    PREF_SetBoolPref("autoupdate.confirm_install", m_bEnableConfirmInstall);
	return TRUE;
}

BOOL
CSmartUpdatePrefs::InitDialog()
{
	// Check for locked preferences
    CheckIfLockedPref("autoupdate.enabled", IDC_CHECK1);
    CheckIfLockedPref("autoupdate.confirm_install", IDC_CHECK2);

    HWND    hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
	int		nIndex;

    
	// Fill the list box with the list of helper applications
    assert(m_pObject);
	if (m_pObject) {
        LPADVANCEDPREFS lpAdvancedPrefs;
        if (SUCCEEDED(m_pObject->QueryInterface(IID_IAdvancedPrefs, (void **)&lpAdvancedPrefs))) {

		    LPSMARTUPDATEPREFS lpSmartUpdatePrefs;

		    // Request the ISmartUpdatePrefs interface from our data object
		    if (SUCCEEDED(lpAdvancedPrefs->QueryInterface(IID_ISmartUpdatePrefs, (void **)&lpSmartUpdatePrefs))) {
			    
                LONG err;
                void* context = NULL;
                LPPACKAGEINFO packageInfo = new PACKAGEINFO;
                LPPACKAGEINFO pInfo;
                *(packageInfo->userPackageName) = '\0';
                *(packageInfo->regPackageName) = '\0';

                if (packageInfo != NULL) {
                    err = lpSmartUpdatePrefs->EnumUninstall(&context, packageInfo->userPackageName, sizeof(packageInfo->userPackageName),
                                              packageInfo->regPackageName, sizeof(packageInfo->regPackageName));
                    while (err == REGERR_OK) {
                        if (*(packageInfo->regPackageName) != '\0' &&
                            strcmp(packageInfo->regPackageName, UNINSTALL_NAV_STR) !=0 ) 
                        {
                            pInfo = new PACKAGEINFO;
                            if (*(packageInfo->userPackageName) != '\0') {
                                lstrcpy(pInfo->userPackageName, packageInfo->userPackageName);
                            } else {
                                lstrcpy(pInfo->userPackageName, packageInfo->regPackageName);
                            }
                            lstrcpy(pInfo->regPackageName, packageInfo->regPackageName);
                            nIndex = ListBox_AddString(hList, pInfo->userPackageName);
                            int lvalue;
				            lvalue = ListBox_SetItemData(hList, nIndex, pInfo);
                            if (lvalue == LB_ERR) {
                                delete pInfo;
                            }
                        }
                        *(packageInfo->regPackageName) = '\0';
                        *(packageInfo->userPackageName) = '\0';
                        err = lpSmartUpdatePrefs->EnumUninstall(&context, packageInfo->userPackageName, sizeof(packageInfo->userPackageName),
                                                      packageInfo->regPackageName, sizeof(packageInfo->regPackageName));
                    }
                }
                lpSmartUpdatePrefs->Release();
            }  
        }
	}
    // Select the first item in the list
	ListBox_SetCurSel(hList, 0);
    return CBrowserPropertyPage::InitDialog();
}

BOOL
CSmartUpdatePrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_BUTTON1 && notifyCode == BN_CLICKED) {
       	assert(m_pObject);
		if (m_pObject) {
            LPADVANCEDPREFS lpAdvancedPrefs;

            if (SUCCEEDED(m_pObject->QueryInterface(IID_IAdvancedPrefs, (void **)&lpAdvancedPrefs))) {

			    LPSMARTUPDATEPREFS lpSmartUpdatePrefs;

			    if (SUCCEEDED(lpAdvancedPrefs->QueryInterface(IID_ISmartUpdatePrefs, (void **)&lpSmartUpdatePrefs))) {
				    
				    lpSmartUpdatePrefs->RegPack();
				    lpSmartUpdatePrefs->Release();
			    }
            }
		}

		return TRUE;

	} else if (id == IDC_BUTTON2 && notifyCode == BN_CLICKED) {
        LPPACKAGEINFO packageInfo = NULL;
	    HWND			 hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
	    int				 nIndex = 0;
        int              count = 0;
        int              err;

        char	        szCaption[256];
        char	        szMessage[256];
        LPSTR           lpszText;
                		
        // Get the currently selected item
	    nIndex = ListBox_GetCurSel(hList);
	 
        if (nIndex != LB_ERR) {
            // Get the packageInfo data structure
	        packageInfo = (LPPACKAGEINFO)ListBox_GetItemData(hList, nIndex);
	                   
            if (packageInfo != NULL) {
                if (*(packageInfo->regPackageName) != '\0') {

                    // Load the caption for the message box and message string
                    LoadString(m_hInstance, IDS_CONTINUE_UNINSTALL, szMessage, sizeof(szMessage));
		            LoadString(m_hInstance, IDS_UNINSTALL, szCaption, sizeof(szCaption));

                    // Format the text
				    lpszText = (LPSTR)CoTaskMemAlloc(lstrlen(szMessage) + sizeof(packageInfo->userPackageName));
				    wsprintf(lpszText, szMessage, (LPCSTR)(packageInfo->userPackageName));

		            if (MessageBox(GetParent(m_hwndDlg), (LPCSTR)lpszText, (LPCSTR)szCaption,
                        MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        CoTaskMemFree(lpszText);
                        assert(m_pObject);
		                if (m_pObject) {
                            LPADVANCEDPREFS lpAdvancedPrefs;

                            if (SUCCEEDED(m_pObject->QueryInterface(IID_IAdvancedPrefs, (void **)&lpAdvancedPrefs))) {

			                    LPSMARTUPDATEPREFS lpSmartUpdatePrefs;

			                    if (SUCCEEDED(lpAdvancedPrefs->QueryInterface(IID_ISmartUpdatePrefs, (void **)&lpSmartUpdatePrefs))) {
				                    
				                    err = lpSmartUpdatePrefs->Uninstall(packageInfo->regPackageName);
                                    if (err == REGERR_OK) {
                                        count = ListBox_GetCount(hList);
                                        ListBox_DeleteString(hList, nIndex);
                                        delete packageInfo;
                                        if ((count == LB_ERR) || (nIndex == 0))
                                            ListBox_SetCurSel(hList, 0);
                                        else if (nIndex < count-1)
	                                        ListBox_SetCurSel(hList, nIndex);
                                        else
                                            ListBox_SetCurSel(hList, nIndex-1);
                                    } else {
                                        LoadString(m_hInstance, IDS_UNINSTALL, szCaption, sizeof(szCaption));
                                        LoadString(m_hInstance, IDS_ERROR_UNINSTALL, szMessage, sizeof(szMessage));
                                        MessageBox(GetParent(m_hwndDlg), (LPCSTR)szMessage, (LPCSTR)szCaption, MB_OK );
                                    }
				                    lpSmartUpdatePrefs->Release();
			                    }
                            }
		                }
                    }
                }
            }
        }
   		return TRUE;
	}

	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

#endif /* MOZ_SMARTUPDATE */=======
