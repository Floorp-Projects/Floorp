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
#include "bitmpbtn.h"
#include "resource.h"
#include "xp_core.h"
#include "xp_help.h"
#include "prefapi.h"
#include "brprefid.h"
#include "../../../defaults.h"
#ifndef _WIN32
#include <toolhelp.h>
#endif

#ifndef _WIN32
#define OFN_NONETWORKBUTTON          0x00020000
#endif

#define IDS_ACCEPTLANG_START IDS_ACCEPTLANG_AF
#define IDS_ACCEPTLANG_END   IDS_ACCEPTLANG_ESMX	// Change me if you add new string

// How to handle an external application
#define HANDLE_VIA_NETSCAPE  50
#define HANDLE_UNKNOWN      100
#define HANDLE_EXTERNAL     200
#define HANDLE_SAVE		    300
#define HANDLE_VIA_PLUGIN   400
#define HANDLE_MOREINFO     500
#define HANDLE_SHELLEXECUTE 600
#define HANDLE_BY_OLE		700

/////////////////////////////////////////////////////////////////////////////
// Helper routines

// Retrieve the file type class for a given extension
static BOOL
GetFileClass(LPCSTR lpszExtension, LPSTR lpszFileClass, LONG cbData)
{
    char    szKey[_MAX_EXT + 1];  // space for '.'
	LONG	lResult;

	// Look up the file association key which maps a file extension
	// to a file class
    wsprintf(szKey, ".%s", lpszExtension);
    lResult = RegQueryValue(HKEY_CLASSES_ROOT, szKey, lpszFileClass, &cbData);
	
#ifdef _WIN32
	assert(lResult != ERROR_MORE_DATA);
#endif
	return lResult == ERROR_SUCCESS;
}

// Get the description for a file type class
static BOOL
GetFileTypeDescription(LPCSTR lpszFileClass, CString &strDescription)
{
	LONG    cbData;
	LONG    lResult;
	 
#ifdef _WIN32
	// See how much space we need
	cbData = 0;
	lResult = RegQueryValue(HKEY_CLASSES_ROOT, lpszFileClass, NULL, &cbData);
	if (lResult == ERROR_SUCCESS && cbData > 1) {
		LPSTR   lpszDescription = strDescription.BufferSetLength((int)cbData);

		if (!lpszDescription)
			return FALSE;

		// Get the string
		lResult = RegQueryValue(HKEY_CLASSES_ROOT, lpszFileClass, lpszDescription, &cbData);
		return lResult == ERROR_SUCCESS;
	}
#else
	// Win 3.1 RegQueryValue() doesn't support asking for the size of the data
	char	szDescription[128];

	// Get the string
	cbData = sizeof(szDescription);
	lResult = RegQueryValue(HKEY_CLASSES_ROOT, lpszFileClass, szDescription, &cbData);
	if (lResult == ERROR_SUCCESS && cbData > 1) {
		strDescription = szDescription;
		return TRUE;
	}
#endif

    return FALSE;
}

// Set the description for a file type class
static void
SetFileTypeDescription(LPCSTR lpszFileClass, LPCSTR lpszDescription)
{
	HKEY	hKey;
	LONG	lResult;

	lResult = RegOpenKey(HKEY_CLASSES_ROOT, lpszFileClass, &hKey);
	if (lResult == ERROR_SUCCESS) {
		RegSetValue(hKey, NULL, REG_SZ, lpszDescription, lstrlen(lpszDescription));
		RegCloseKey(hKey);
	}
}

// Retrieves the HICON that's associated with the given file class
static HICON
GetDocumentIcon(HINSTANCE hInstance, LPCSTR lpszFileClass)
{
	char    szBuf[_MAX_PATH];
	LONG	lResult;
	HKEY	hKey;
#ifdef _WIN32
	DWORD	cbData;
	DWORD	dwType;
#else
	LONG	cbData;
#endif

	// Open the DefaultIcon key
#ifdef _WIN32
	wsprintf(szBuf, "%s\\DefaultIcon", lpszFileClass);
	lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, szBuf, 0, KEY_QUERY_VALUE, &hKey);
#else
	wsprintf(szBuf, "%s", lpszFileClass);
	lResult = RegOpenKey(HKEY_CLASSES_ROOT, szBuf, &hKey);
#endif
	if (lResult != ERROR_SUCCESS)
		return NULL;

	// Get the default value of the key
	cbData = sizeof(szBuf);
#ifdef _WIN32
	lResult = RegQueryValueEx(hKey, NULL, NULL, &dwType, (LPBYTE)szBuf, &cbData);
#else
	lResult = RegQueryValue(hKey, "DefaultIcon", (LPSTR)szBuf, &cbData);
#endif
	RegCloseKey(hKey);

	if (lResult == ERROR_SUCCESS) {
		// The syntax is a filename and an icon number separated by a comma
		LPCSTR	lpszFilename = szBuf;
		LPSTR	lpszComma;
		int		nIconIndex = 0;

		lpszComma = strchr(szBuf, ',');
		if (lpszComma) {
			*lpszComma = '\0';  // null terminate the filename

			// Get the icon index
			nIconIndex = atoi(lpszComma + 1);
		}

		// Get the icon
		return ExtractIcon(hInstance, lpszFilename, nIconIndex);
	}

	return NULL;
}

static HICON
GetAppIcon(HINSTANCE hInstance, LPCSTR lpszAppPath)
{
#ifdef _WIN32
	SHFILEINFO	sfi;
	LPSTR		lpszFilePart;
	char		szBuf[_MAX_PATH];
	
	// Get a full pathname
	SearchPath(NULL, lpszAppPath, ".exe", sizeof(szBuf), szBuf, &lpszFilePart);
	SHGetFileInfo(szBuf, 0, &sfi, sizeof(sfi), SHGFI_ICON);
	return sfi.hIcon;
#else
	return ExtractIcon(hInstance, lpszAppPath, 0);
#endif
}

// Sets the shell open command string value for the given file class
static void
SetShellOpenCommand(LPCSTR lpszFileClass, LPCSTR lpszCmdString)
{
	char	szKey[_MAX_PATH];
#ifdef _WIN32
	DWORD	dwDisposition;
#endif
	HKEY	hKey;
	LONG	lResult;

	// Build the subkey string
	wsprintf(szKey, "%s\\shell\\open\\command", lpszFileClass);

	// Update the shell\open\command for the file type class
#ifdef _WIN32
	lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT, szKey, 0, NULL, 0, KEY_WRITE,
		NULL, &hKey, &dwDisposition);

	if (lResult == ERROR_SUCCESS) {
		RegSetValueEx(hKey, NULL, 0, REG_SZ, (CONST BYTE *)lpszCmdString,
			lstrlen(lpszCmdString) + 1);
		RegCloseKey(hKey);
	}
#else
	lResult = RegCreateKey(HKEY_CLASSES_ROOT, szKey, &hKey);

	if (lResult == ERROR_SUCCESS) {
		RegSetValue(hKey, NULL, REG_SZ, lpszCmdString, lstrlen(lpszCmdString));
		RegCloseKey(hKey);
	}
#endif
}

// Returns the shell open command string value (path and filename for the application
// plus any command line options) associated with the given file class
static BOOL
GetShellOpenCommand(LPCSTR lpszFileClass, LPSTR lpszCmdString, DWORD cbCmdString)
{
	char    szBuf[_MAX_PATH];
	LONG	lResult;
	HKEY	hKey;
#ifdef _WIN32
	DWORD	dwType;
#else
	LONG	cbData;
#endif

	*lpszCmdString = '\0';
	
	// Get the Open command string value for the file class
#ifdef _WIN32
	wsprintf(szBuf, "%s\\shell\\open\\command", lpszFileClass);
	lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, szBuf, 0, KEY_QUERY_VALUE, &hKey);
#else
	wsprintf(szBuf, "%s\\shell\\open", lpszFileClass);
	lResult = RegOpenKey(HKEY_CLASSES_ROOT, szBuf, &hKey);
#endif
	if (lResult != ERROR_SUCCESS)
		return FALSE;

	// Get the value
#ifdef _WIN32
	lResult = RegQueryValueEx(hKey, NULL, NULL, &dwType, (LPBYTE)lpszCmdString, &cbCmdString);
#else
	cbData = (LONG)cbCmdString;
	lResult = RegQueryValue(hKey, "command", (LPSTR)lpszCmdString, &cbData);
#endif
	RegCloseKey(hKey);

	return lResult == ERROR_SUCCESS;
}

// lpszPath must be _MAX_PATH characters in size. Quotes the filename if
// it's a long filename with a space (Win32 only)
static BOOL
BrowseForProgram(HINSTANCE hInstance, HWND hwndOwner, LPSTR lpszPath)
{
    OPENFILENAME	ofn;
    CString			strFilters;

    // Load the description and file patterns for the filters
    strFilters.LoadString(hInstance, IDS_FILTER_PROGRAM);
    
    // Replace the '\n' with '\0' before calling the common dialogs
    for (LPSTR lpsz = (LPSTR)(LPCSTR)strFilters; *lpsz;) {
        if (*lpsz == '\n')
            *lpsz++ = '\0';
        else
            lpsz += IsDBCSLeadByte(*lpsz) ? 2 : 1;
    }

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFilter = strFilters;
    ofn.lpstrFile = lpszPath;
    *lpszPath = '\0';
    ofn.nMaxFile = _MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NONETWORKBUTTON;

    // Bring up the common dialog
    if (GetOpenFileName(&ofn)) {
#ifdef _WIN32
		// Don't quote an already quoted string. Note: this doesn't seem to be
		// an issue, because GetOpenFileName() strips leading quotes and trims
		// left and right spaces
		if (*lpszPath && *lpszPath != '"') {
			// See if the path contains a space
			if (strchr(lpszPath, ' ')) {
				int	nLen = lstrlen(lpszPath);

				assert(nLen > 0);
				// Make sure we can do this without overwriting the buffer. Note: the
				// 3 refers to the opening/closing quotes and the terminating '\0'
				if (nLen + 3 <= _MAX_PATH) {
					// Return a quoted string
					memmove(lpszPath + 1, lpszPath, nLen);
					lpszPath[0] = '"';
					lpszPath[nLen + 1] = '"';
					lpszPath[nLen + 2] = '\0';
				}
			}
		}
#endif
        return TRUE;
	}

    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CBrowserPrefs implementation

CBrowserPrefs::CBrowserPrefs()
	: CBrowserPropertyPage(IDD_BROWSER, HELP_PREFS_BROWSER)
{
}

// Initialize member data using XP preferences
STDMETHODIMP
CBrowserPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		int32	n;

		PREF_GetIntPref("browser.startup.page", &n);
		m_nStartsWith = (int)n;

		PREF_GetStringPref("browser.startup.homepage", m_strHomePageURL);
		
		PREF_GetIntPref("browser.link_expiration", &n);
		m_nExpireAfter = (int)n;

		// See if there's a current page URL available
		assert(m_pObject);
		if (m_pObject) {
			LPBROWSERPREFS lpBrowserPrefs;

			// Request the IBrowserPrefs interface from our data object
			if (SUCCEEDED(m_pObject->QueryInterface(IID_IBrowserPrefs, (void **)&lpBrowserPrefs))) {
				LPOLESTR	lpoleCurrentPage;

				lpBrowserPrefs->GetCurrentPage(&lpoleCurrentPage);
				if (lpoleCurrentPage) {
					m_strCurrentPage = lpoleCurrentPage;
					CoTaskMemFree(lpoleCurrentPage);
				}
				
				lpBrowserPrefs->Release();
			}
		}
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CBrowserPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	RadioButtonTransfer(IDC_RADIO1, m_nStartsWith, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT1, m_strHomePageURL, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT2, m_nExpireAfter, bSaveAndValidate);
	return TRUE;
}

// Apply changes using XP preferences
BOOL
CBrowserPrefs::ApplyChanges()
{
	PREF_SetIntPref("browser.startup.page", (int32)m_nStartsWith);
	PREF_SetCharPref("browser.startup.homepage", (LPCSTR)m_strHomePageURL);
	PREF_SetIntPref("browser.link_expiration", (int32)m_nExpireAfter);
	return TRUE;
}

BOOL
CBrowserPrefs::InitDialog()
{
	// Only enable the Use Current Page button if we were given a URL
	EnableDlgItem(IDC_BUTTON1, !m_strCurrentPage.IsEmpty());

	// Check for locked preferences
	if (PREF_PrefIsLocked("browser.startup.page")) {
		// Disable all the radio buttons in the group
		DisableRadioButtonGroup(IDC_RADIO1);
	}
	
	if (CheckIfLockedPref("browser.startup.homepage", IDC_EDIT1)) {
		// Disable Use Current Page and Browser buttons
		EnableDlgItem(IDC_BUTTON1, FALSE);
		EnableDlgItem(IDC_BUTTON2, FALSE);
	}

	CheckIfLockedPref("browser.link_expiration", IDC_EDIT2);

	return CBrowserPropertyPage::InitDialog();
}

BOOL
CBrowserPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_BUTTON1 && notifyCode == BN_CLICKED) {
		assert(!m_strCurrentPage.IsEmpty());
		Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT1), (LPCSTR)m_strCurrentPage);
		return TRUE;

	} else if (id == IDC_BUTTON2 && notifyCode == BN_CLICKED) {
		OPENFILENAME	ofn;
		CString			strFilters;
		CString			strTitle;
		char			szBuf[_MAX_PATH];

		// Load the description and file patterns for the filters
#ifdef _WIN32
		strFilters.LoadString(m_hInstance, IDS_FILTER_HTM32);
#else
		strFilters.LoadString(m_hInstance, IDS_FILTER_HTM16);
#endif
		
		// Replace the '\n' with '\0' before calling the common dialogs
		for (LPSTR lpsz = (LPSTR)(LPCSTR)strFilters; *lpsz;) {
			if (*lpsz == '\n')
				*lpsz++ = '\0';
			else
				lpsz += IsDBCSLeadByte(*lpsz) ? 2 : 1;
		}

		// Get the title for the dialog
		strTitle.LoadString(m_hInstance, IDS_BROWSE_HOMEPAGE);

		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = m_hwndDlg;
		ofn.lpstrFilter = strFilters;
		ofn.lpstrFile = szBuf;
		szBuf[0] = '\0';
		ofn.nMaxFile = sizeof(szBuf);
		ofn.lpstrTitle = strTitle;
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NONETWORKBUTTON;

		// Bring up the common dialog
		if (GetOpenFileName(&ofn)) {
			Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT1), szBuf);
		}

		return TRUE;

	} else if (id == IDC_BUTTON3 && notifyCode == BN_CLICKED) {
		CString	strText, strCaption;
		int32	nSize;

		strText.LoadString(m_hInstance, IDS_CONTINUE_CLEAR_HISTORY);
		strCaption.LoadString(m_hInstance, IDS_HISTORY);

		if (MessageBox(GetParent(m_hwndDlg), (LPCSTR)strText, (LPCSTR)strCaption, MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
			PREF_GetIntPref("browser.link_expiration", &nSize);
			PREF_SetIntPref("browser.link_expiration", 0);
			PREF_SetIntPref("browser.link_expiration", nSize);
		}
		return TRUE;
	}

	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CAddLanguagesDialog implementation

class CAddLanguagesDialog : public CDialog {
	public:
		CAddLanguagesDialog();

		// Returns the language that was selected
		LPCSTR	GetLanguage();

	protected:
		BOOL	InitDialog();
		BOOL	DoTransfer(BOOL bSaveAndValidate);

		// Event processing
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:
		CString	m_strLanguage;
		CString	m_strOther;
};

CAddLanguagesDialog::CAddLanguagesDialog()
	: CDialog(CComDll::m_hInstance, IDD_ADD_LANGUAGES)
{
}

BOOL
CAddLanguagesDialog::InitDialog()
{
	HWND	hList;
	char	szBuf[256];

	// Fill the list box
	hList = GetDlgItem(m_hwndDlg, IDC_LIST1);

	// NOTE: All the strings to be added to the list box must be numbered
	// consecutively
	for (int i = IDS_ACCEPTLANG_START; i < IDS_ACCEPTLANG_END; i++) {
		int	nLen = LoadString(m_hInstance, i, szBuf, sizeof(szBuf));
		
		assert(nLen > 0);
		ListBox_AddString(hList, szBuf);
	}

	// Limit the length of the string the user can type
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT1), 64);

	return CDialog::InitDialog();
}

LPCSTR
CAddLanguagesDialog::GetLanguage()
{
	if (!m_strOther.IsEmpty())
		return (LPCSTR)m_strOther;

	return m_strLanguage.IsEmpty() ? NULL : (LPCSTR)m_strLanguage;
}

BOOL
CAddLanguagesDialog::DoTransfer(BOOL bSaveAndValidate)
{
	if (bSaveAndValidate)
		ListBoxTransfer(IDC_LIST1, m_strLanguage, bSaveAndValidate);
	else
		ListBox_SetCurSel(GetDlgItem(m_hwndDlg, IDC_LIST1), 0);
	EditFieldTransfer(IDC_EDIT1, m_strOther, bSaveAndValidate);
	return TRUE;
}

BOOL
CAddLanguagesDialog::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_LIST1 && notifyCode == LBN_DBLCLK) {
		FORWARD_WM_COMMAND(m_hwndDlg, IDOK, GetDlgItem(m_hwndDlg, IDOK), BN_CLICKED, SendMessage);
		return TRUE;

	} else if (id == IDC_EDIT1 && notifyCode == EN_CHANGE) {
		// Disable the list box if the user has typed something into
		// the edit field
		EnableDlgItem(IDC_LIST1, Edit_GetTextLength(hwndCtl) == 0);
	}

	return CDialog::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CLanguagesPrefs implementation

CLanguagesPrefs::CLanguagesPrefs()
	: CBrowserPropertyPage(IDD_LANGUAGES, HELP_PREFS_BROWSER_LANGUAGES)
{
}

// Initialize member data using XP preferences
STDMETHODIMP
CLanguagesPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		PREF_GetStringPref("intl.accept_languages", m_strAcceptLangs);
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

// XXX - move into cdialog.h
static void
ListBox_GetString(HWND hwndList, int nIndex, CString &str)
{
	assert(nIndex >= 0);
	ListBox_GetText(hwndList, nIndex,
		str.BufferSetLength(ListBox_GetTextLen(hwndList, nIndex)));
}

BOOL
CLanguagesPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	if (bSaveAndValidate) {
		HWND	hwndList = GetDlgItem(m_hwndDlg, IDC_LIST1);
		int		nListCount = ListBox_GetCount(hwndList);
		CString	strText;
		
		// Build up the list of accept languages
		m_strAcceptLangs.Empty();

		if (nListCount != LB_ERR) {
			for (int i = 0; i < nListCount; i++) {
				ListBox_GetString(hwndList, i, strText);

				if (i == 0)
					m_strAcceptLangs = strText;
				else
					m_strAcceptLangs += ',' + strText;
			}
		}
		
	}

	if (!bSaveAndValidate)
		CheckButtons();

	return TRUE;
}

// Apply changes using XP preferences
BOOL
CLanguagesPrefs::ApplyChanges()
{
	PREF_SetCharPref("intl.accept_languages", (LPCSTR)m_strAcceptLangs);
	return TRUE;
}

BOOL
CLanguagesPrefs::InitDialog()
{
	// Load the bitmaps
#ifdef _WIN32
	m_hUpBitmap = LoadImage(m_hInstance, MAKEINTRESOURCE(IDB_UPARROW), IMAGE_BITMAP,
		0, 0, LR_LOADTRANSPARENT | LR_LOADMAP3DCOLORS);
	m_hDownBitmap = LoadImage(m_hInstance, MAKEINTRESOURCE(IDB_DOWNARROW), IMAGE_BITMAP,
		0, 0, LR_LOADTRANSPARENT | LR_LOADMAP3DCOLORS);
#else
	m_hUpBitmap = LoadTransparentBitmap(m_hInstance, IDB_UPARROW);
	m_hDownBitmap = LoadTransparentBitmap(m_hInstance, IDB_DOWNARROW);
#endif

	// Create the memory DC we'll need
	m_hMemDC = CreateCompatibleDC(NULL);

	// Size the buttons to fit the bitmaps
	SizeToFitBitmapButton(GetDlgItem(m_hwndDlg, IDC_BUTTON3), m_hUpBitmap);
	SizeToFitBitmapButton(GetDlgItem(m_hwndDlg, IDC_BUTTON4), m_hDownBitmap);

	// We don't receive the WM_MEASUREITEM message, because it's sent before
	// the WM_INITDIALOG, so set the height of the list box items now
	SetLBoxItemHeight();

	// Fill the list box with the list of accept languages
	FillListBox();

	EnableDlgItem(IDC_LIST1, !PREF_PrefIsLocked("intl.accept_languages"));
	EnableDlgItem(IDC_BUTTON1, !PREF_PrefIsLocked("intl.accept_languages"));
	EnableDlgItem(IDC_BUTTON2, !PREF_PrefIsLocked("intl.accept_languages"));
	EnableDlgItem(IDC_BUTTON3, !PREF_PrefIsLocked("intl.accept_languages"));
	EnableDlgItem(IDC_BUTTON4, !PREF_PrefIsLocked("intl.accept_languages"));


	return CBrowserPropertyPage::InitDialog();
}

void
CLanguagesPrefs::FillListBox()
{
	if (!m_strAcceptLangs.IsEmpty()) {
		HWND	hwndList = GetDlgItem(m_hwndDlg, IDC_LIST1);
		int		nIndex;
		CString	strTmp(m_strAcceptLangs);

		while ((nIndex = strTmp.Find(',')) != -1) {
			ListBox_AddString(hwndList, (LPCSTR)strTmp.Left(nIndex));
			strTmp = strTmp.Mid(nIndex + 1);
		}

		ListBox_AddString(hwndList, (LPCSTR)strTmp);  // add last string
	}
}

BOOL
CLanguagesPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	switch (id) {
		case IDC_BUTTON1:
			if (notifyCode == BN_CLICKED) {
				CAddLanguagesDialog	dlg;
		
				// Display the "Add Languages" dialog box
				if (dlg.DoModal(GetParent(m_hwndDlg)) == IDOK) {
					HWND 	hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
					int		nIndex;
		
					// See if the string is already in the list. If it is then just
					// select the existing list item
					nIndex = ListBox_FindStringExact(hList, 0, dlg.GetLanguage());

					if (nIndex != LB_ERR)
						ListBox_SetCurSel(hList, nIndex);

					else {
						nIndex = ListBox_AddString(hList, dlg.GetLanguage());
						ListBox_SetCurSel(hList, nIndex);
						CheckButtons();
					}
				}
			}
			return TRUE;

		case IDC_BUTTON2:
			if (notifyCode == BN_CLICKED) {
				HWND 	hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
				int		nCurSel = ListBox_GetCurSel(hList);

				// Delete the selected list box item
				assert(nCurSel != LB_ERR);
				if (nCurSel != LB_ERR) {
					ListBox_DeleteString(hList, nCurSel);
					if (ListBox_GetCount(hList) > 0)
						ListBox_SetCurSel(hList, nCurSel > 0 ? nCurSel - 1 : 0);
					CheckButtons();
				}
			}
			return TRUE;

		case IDC_BUTTON3:
		case IDC_BUTTON4:
			if (notifyCode == BN_CLICKED || notifyCode == BN_DOUBLECLICKED) {
				HWND 	hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
				int		nCurSel = ListBox_GetCurSel(hList);

				// Move the selected list box item up or down
				assert(nCurSel != LB_ERR);
				if (nCurSel != LB_ERR) {
					CString	strItem;
					int		nLen = ListBox_GetTextLen(hList, nCurSel);
					int		nIndex;

					ListBox_GetText(hList, nCurSel, strItem.BufferSetLength(nLen));
					ListBox_DeleteString(hList, nCurSel);
					nIndex = ListBox_InsertString(hList, id == IDC_BUTTON3 ? nCurSel - 1 :
						nCurSel + 1, (LPCSTR)strItem);
					ListBox_SetCurSel(hList, nIndex);
					CheckButtons();
				}

			}
			return TRUE;

		case IDC_LIST1:
			if (notifyCode == LBN_SELCHANGE)
				CheckButtons();
			return TRUE;
	}

	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

void
CLanguagesPrefs::SetLBoxItemHeight()
{
	TEXTMETRIC	tm;
	HWND		hwndCtl = GetDlgItem(m_hwndDlg, IDC_LIST1);
	HFONT		hFont = GetWindowFont(hwndCtl);
	HDC			hDC = GetDC(hwndCtl);
	HFONT		hOldFont = NULL;

	if (hFont)
		hOldFont = SelectFont(hDC, hFont);

	GetTextMetrics(hDC, &tm);

	// Cleanup and release the HDC
	if (hOldFont)
		SelectObject(hDC, hOldFont);
	ReleaseDC(hwndCtl, hDC);

	// Compute the tab stops. The second tab stop is left aligned
	// with the "Language" column heading
	HWND	hStatic = GetDlgItem(m_hwndDlg, IDC_LANGUAGE_HEADING);
	RECT	r;

	m_nTabStops[0] = 2 * tm.tmAveCharWidth;
	assert(hStatic);
	GetWindowRect(hStatic, &r);
	MapWindowPoints(NULL, hwndCtl, (LPPOINT)&r, 2);
	m_nTabStops[1] = r.left;

	// Set the height of a single item
	ListBox_SetItemHeight(hwndCtl, 0, tm.tmHeight);
}

void
CLanguagesPrefs::DrawLBoxItem(LPDRAWITEMSTRUCT lpdis)
{
	char	szBuf[256];
	int		nLen;
    int     nColorIndex;
	HWND	hwndFocus = GetFocus();  // don't trust ODS_FOCUS

	switch (lpdis->itemAction) {
		case ODA_DRAWENTIRE:
		case ODA_SELECT:
		case ODA_FOCUS:
			nColorIndex = COLOR_WINDOWTEXT;
			if ((lpdis->itemState & ODS_SELECTED) && (hwndFocus == lpdis->hwndItem))
				nColorIndex = COLOR_HIGHLIGHTTEXT;
			SetTextColor(lpdis->hDC, GetSysColor(nColorIndex));

			// Behave like a 32-bit list view and use a different color to indicate
			// selection when we don't have the focus
			nColorIndex = COLOR_WINDOW;
			if (lpdis->itemState & ODS_SELECTED)
				nColorIndex = (hwndFocus == lpdis->hwndItem ? COLOR_HIGHLIGHT : COLOR_BTNFACE);
			SetBkColor(lpdis->hDC, GetSysColor(nColorIndex));
	
			assert(lpdis->itemID != (UINT)-1);

			// Draw the order number. Paint the list item rect at the same time
			wsprintf(szBuf, "%i", lpdis->itemID + 1);
			ExtTextOut(lpdis->hDC, lpdis->rcItem.left + m_nTabStops[0], lpdis->rcItem.top,
				ETO_OPAQUE, &lpdis->rcItem, szBuf, lstrlen(szBuf), NULL);
	
			// Draw the language. Because this is an owner-draw list box, we need to
			// ask the list box for the string
			nLen = ListBox_GetTextLen(lpdis->hwndItem, lpdis->itemID);
	
			if (nLen < sizeof(szBuf)) {
				ListBox_GetText(lpdis->hwndItem, lpdis->itemID, szBuf);
			
				ExtTextOut(lpdis->hDC, lpdis->rcItem.left + m_nTabStops[1], lpdis->rcItem.top,
				0, &lpdis->rcItem, szBuf, nLen, NULL);
			}
			
			// Draw the focus rect if necessary
			if (lpdis->itemState & ODS_FOCUS)
				::DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
			break;
	}
}

LRESULT
CLanguagesPrefs::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPDRAWITEMSTRUCT	lpdis;

	switch (uMsg) {
		case WM_DESTROY:
			DeleteBitmap(m_hUpBitmap);
			DeleteBitmap(m_hDownBitmap);
			DeleteDC(m_hMemDC);
			break;

		case WM_DRAWITEM:
			lpdis = (LPDRAWITEMSTRUCT)lParam;

			if (lpdis->CtlID == IDC_LIST1)
				DrawLBoxItem(lpdis);
			else
				DrawBitmapButton(lpdis, lpdis->CtlID == IDC_BUTTON3 ? m_hUpBitmap :
					m_hDownBitmap, m_hMemDC);
			return TRUE;
	}

	return CBrowserPropertyPage::WindowProc(uMsg, wParam, lParam);
}

void
CLanguagesPrefs::CheckButtons()
{
	HWND	hwndList = GetDlgItem(m_hwndDlg, IDC_LIST1);
	int		nListCount, nCurSel;
	HWND	hwndUpBtn, hwndDownBtn, hwndFocus;

	assert(hwndList);
	nListCount = ListBox_GetCount(hwndList);
	nCurSel = ListBox_GetCurSel(hwndList);

	// Enable the Delete button if there's something selected in
	// the list box
	EnableDlgItem(IDC_BUTTON2, nCurSel != LB_ERR);

	// Enable the Up/Down buttons
	hwndUpBtn = GetDlgItem(m_hwndDlg, IDC_BUTTON3);
	hwndDownBtn = GetDlgItem(m_hwndDlg, IDC_BUTTON4);
	hwndFocus = GetFocus();
	
	if (nListCount <= 1 || nCurSel == LB_ERR) {
		EnableWindow(hwndUpBtn, FALSE);
		EnableWindow(hwndDownBtn, FALSE);
	
		// Make sure there's still a focus window. If one of the buttons had
		// focus, and we disabled that button then no one has the focus
		if (hwndFocus == hwndUpBtn || hwndFocus == hwndDownBtn)
			SetFocus(hwndList);

	} else {
		EnableWindow(hwndUpBtn, nCurSel > 0);
		EnableWindow(hwndDownBtn, nCurSel != LB_ERR && nCurSel < (nListCount - 1));
		
		// Make sure there's still a focus window. If one of the buttons had
		// focus, and we disabled that button then no one has the focus
		if (hwndFocus == hwndUpBtn && !IsWindowEnabled(hwndUpBtn))
			SetFocus(hwndDownBtn);

		if (hwndFocus == hwndDownBtn && !IsWindowEnabled(hwndDownBtn))
			SetFocus(hwndUpBtn);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CEditTypeDialog implementation

class CEditTypeDialog : public CDialog {
	public:
		CEditTypeDialog();

	protected:
		BOOL	InitDialog();
		BOOL	DoTransfer(BOOL bSaveAndValidate);

        // Event processing
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);
	
    public:
		HICON		m_hDocIcon;
		CString		m_strDescription;
		CString		m_strExtension;
		CString		m_strMimeType;
		int			m_nHowToHandle;
		BOOL		m_bConfirmBeforeOpening;
		CString		m_strOpenCmd;
		BOOL		m_bURLProtocol;
#ifdef MOZ_MAIL_NEWS
		int			m_UseAsDefaultMIMEType;
		BOOL		m_LockDefaultMIMEType;
#endif /* MOZ_MAIL_NEWS */

	private:
		void	CheckControls();
};

CEditTypeDialog::CEditTypeDialog()
	: CDialog(CComDll::m_hInstance, IDD_EDIT_TYPE)
{
	m_bConfirmBeforeOpening = TRUE;
	m_bURLProtocol = FALSE;
#ifdef MOZ_MAIL_NEWS
	m_UseAsDefaultMIMEType = 0;		// initialized later from the caller, which knows about extension lists
	m_LockDefaultMIMEType = FALSE;	// initialized later from the caller, which knows about extension lists
#endif /* MOZ_MAIL_NEWS */
}

BOOL
CEditTypeDialog::InitDialog()
{
	// Set the description, document icon, and list of extensions
	Static_SetText(GetDlgItem(m_hwndDlg, IDC_DESCRIPTION), m_strDescription);
	Static_SetText(GetDlgItem(m_hwndDlg, IDC_EXTENSION), m_strExtension);
	if (m_hDocIcon)
		Static_SetIcon(GetDlgItem(m_hwndDlg, IDC_ICON1), m_hDocIcon);

	// Only enable the option to have Navigator handle it if it's one of
	// the types we know how to view
    if (strcmp(m_strMimeType, TEXT_HTML) == 0 || 
        strcmp(m_strMimeType, TEXT_PLAIN) == 0 ||
        strcmp(m_strMimeType, IMAGE_GIF) == 0 || 
        strcmp(m_strMimeType, IMAGE_JPG) == 0 || 
        strcmp(m_strMimeType, IMAGE_XBM) == 0) {

		EnableDlgItem(IDC_RADIO1, TRUE);
	}

	// See if it's a URL protocol
	if (m_bURLProtocol) {
		EnableDlgItem(IDC_EDIT1, FALSE);
		EnableDlgItem(IDC_RADIO2, FALSE);
		EnableDlgItem(IDC_CHECK1, FALSE);
#ifdef MOZ_MAIL_NEWS
		EnableDlgItem(IDC_USEASDEFAULT, FALSE);
#endif /* MOZ_MAIL_NEWS */
	}

#ifdef MOZ_MAIL_NEWS
	// Check for locked preferences
	if (m_LockDefaultMIMEType) {
		// Disable the "Set this type as the default" pref
		EnableDlgItem(IDC_USEASDEFAULT, FALSE);
	}
#endif /* MOZ_MAIL_NEWS */

	return CDialog::InitDialog();
}

void
CEditTypeDialog::CheckControls()
{
	int	nHowToHandle;

	// Only enable the check box if we're launching a helper app, i.e. not if
	// Netscape is handling it internally or it's set as save to disk
	RadioButtonTransfer(IDC_RADIO1, nHowToHandle, TRUE);
	EnableDlgItem(IDC_CHECK1, nHowToHandle == 2);
}

BOOL
CEditTypeDialog::DoTransfer(BOOL bSaveAndValidate)
{
	int		nRadio;

	if (!bSaveAndValidate) {
		// Map from the values the browser uses
		switch (m_nHowToHandle) {
			case HANDLE_VIA_NETSCAPE:
				nRadio = 0;
				break;

			case HANDLE_SAVE:
				nRadio = 1;
				break;

			case HANDLE_EXTERNAL:
			case HANDLE_SHELLEXECUTE:
			case HANDLE_BY_OLE:
				nRadio = 2;
				break;
		}
	}

	RadioButtonTransfer(IDC_RADIO1, nRadio, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT1, m_strMimeType, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT2, m_strOpenCmd, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK1, m_bConfirmBeforeOpening, bSaveAndValidate);
#ifdef MOZ_MAIL_NEWS
	CheckBoxTransfer(IDC_USEASDEFAULT, m_UseAsDefaultMIMEType, bSaveAndValidate);
#endif /* MOZ_MAIL_NEWS */

	if (bSaveAndValidate) {
		// Map it back to the values the browser uses
		switch (nRadio) {
			case 0:
				m_nHowToHandle = HANDLE_VIA_NETSCAPE;
				break;

			case 1:
				m_nHowToHandle = HANDLE_SAVE;
				break;

			case 2:
				if (m_nHowToHandle != HANDLE_EXTERNAL)
					m_nHowToHandle = HANDLE_SHELLEXECUTE;
				break;

			default:
				assert(FALSE);
				break;
		}

	} else {
		// Check whether the check box to prompt before opening a downloaded
		// file should be enabled
		CheckControls();
	}

	return TRUE;
}

BOOL
CEditTypeDialog::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_BUTTON1 && notifyCode == BN_CLICKED) {
        char    szPath[_MAX_PATH];

        if (BrowseForProgram(m_hInstance, m_hwndDlg, szPath))
            Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT2), szPath);
		return TRUE;

	} else if ((id == IDC_RADIO1 || id == IDC_RADIO2 || id == IDC_RADIO3) && (notifyCode == BN_CLICKED)) {
		// Check whether the check box to prompt before opening a downloaded
		// file should be enabled
		CheckControls();
	}
#ifdef MOZ_MAIL_NEWS
	else if (id == IDC_USEASDEFAULT && notifyCode == BN_CLICKED)
	{
		// handle tri-state silliness
		if (m_UseAsDefaultMIMEType == 0 || m_UseAsDefaultMIMEType == 2)	// off, or partial
		{
			m_UseAsDefaultMIMEType = 1;	// set it on for all of them
		}
		else
		{
			m_UseAsDefaultMIMEType = 0;	// it was on;  set it to off.
		}
		CheckBoxTransfer(IDC_USEASDEFAULT, m_UseAsDefaultMIMEType, FALSE);
		return TRUE;
	}
#endif /* MOZ_MAIL_NEWS */

	return CDialog::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CNewTypeDialog implementation

class CNewTypeDialog : public CDialog {
	public:
		CNewTypeDialog();

	protected:
		BOOL	DoTransfer(BOOL bSaveAndValidate);
        
        // Event processing
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	public:
		CString		m_strDescription;
		CString		m_strExtension;
		CString		m_strMimeType;
		CString		m_strOpenCmd;
#ifdef MOZ_MAIL_NEWS
		int			m_UseAsDefaultMIMEType;
#endif /* MOZ_MAIL_NEWS */

	private:
		void	DisplayMessageBox(UINT nID);
};

CNewTypeDialog::CNewTypeDialog()
	: CDialog(CComDll::m_hInstance, IDD_NEW_TYPE)
{
#ifdef MOZ_MAIL_NEWS
	m_UseAsDefaultMIMEType = 0;
#endif /* MOZ_MAIL_NEWS */
}

void
CNewTypeDialog::DisplayMessageBox(UINT nID)
{
	char	szCaption[256];
	char	szText[256];

	// Load the caption for the message box
	LoadString(m_hInstance, IDS_FILE_TYPES, szCaption, sizeof(szCaption));

	// Load the text to display
	LoadString(m_hInstance, nID, szText, sizeof(szText));

	// Display the message box
	MessageBox(m_hwndDlg, szText, szCaption, MB_OK | MB_ICONSTOP);
}

#ifdef _WIN32
static BOOL
HasMimeType(LPCSTR lpszExtension)
{
    char    szKey[_MAX_EXT + 1];  // space for '.'
	HKEY	hKey;
	LONG	lResult;

    // Build the file association key. It must begin with a '.'
    wsprintf(szKey, ".%s", lpszExtension);
	
    // See if the extension has a mime type (Content Type).
    lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, szKey, 0, KEY_QUERY_VALUE, &hKey);
	if (lResult == ERROR_SUCCESS) {
        DWORD	dwType;
        DWORD	cbData;
        
		// Get the size of the data for the Content Type value
        cbData = 0;
        lResult = RegQueryValueEx(hKey, "Content Type", NULL, &dwType, NULL, &cbData);
		RegCloseKey(hKey);
		return lResult == ERROR_SUCCESS && cbData > 1;
	}
	
	return FALSE;
}
#endif

BOOL
CNewTypeDialog::DoTransfer(BOOL bSaveAndValidate)
{
	EditFieldTransfer(IDC_EDIT1, m_strDescription, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT2, m_strExtension, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT3, m_strMimeType, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT4, m_strOpenCmd, bSaveAndValidate);
#ifdef MOZ_MAIL_NEWS
	CheckBoxTransfer(IDC_USEASDEFAULT, m_UseAsDefaultMIMEType, bSaveAndValidate);
#endif /* MOZ_MAIL_NEWS */

    // Validate the data
	if (bSaveAndValidate) {
		char	szFileClass[80];
		
		// See if the extension begins with a leading '.' If it does strip
		// it off
		while (m_strExtension[0] == '.') {
			m_strExtension = m_strExtension.Mid(1);

			// Update what's displayed so the user sees we changed the extension
			Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT2), (LPCSTR)m_strExtension);
		}

		if (m_strExtension.IsEmpty()) {
			DisplayMessageBox(IDS_NO_EXTENSION);
			SetFocus(GetDlgItem(m_hwndDlg, IDC_EDIT2));
			return FALSE;
		}

		if (m_strMimeType.IsEmpty()) {
			DisplayMessageBox(IDS_NO_MIME_TYPE);
			SetFocus(GetDlgItem(m_hwndDlg, IDC_EDIT3));
			return FALSE;
		}
		
		if (m_strOpenCmd.IsEmpty()) {
			DisplayMessageBox(IDS_NO_OPEN_CMD);
			SetFocus(GetDlgItem(m_hwndDlg, IDC_EDIT4));
			return FALSE;
		}

#ifdef _WIN32
        // See if there's a file type class already associated with this extension.
		// Note that we're checking for a file class, and not just whether the extension
		// exists
        if (GetFileClass((LPCSTR)m_strExtension, szFileClass, sizeof(szFileClass))) {
            CString strDescription;
            char	szCaption[256];
            char	szMessage[256];
            LPSTR   lpszText;
        
			// We allow a file extension to have more than one MIME type, so see whether this
			// extension already has a MIME type. If it does then the user is adding an
			// additional MIME type and that's okay
			//
			// If there is no MIME type then we pref they edit the existing type, and specify
			// specify the MIME type rather than creating a brand new type. The only reason for
			// this is that we want to avoid having two entries in the list, one with no MIME
			// type
			//
			// Note: on Win16 the mapping from MIME type to suffix isn't stored in the registry so
			// we don't bother with this. We could scan through the list of NET_cdataStruct
			// structures, but that wouldn't necessarily tell us whether there's a MIME type
			// specified in the Netscape specific suffix information
			//
			// Some day when we switch to using XP prefs for the Netscape specific information
			// this will be easy to do for Win16 as well
			if (!HasMimeType((LPCSTR)m_strExtension)) {
				// Get the description of the existig file type
				GetFileTypeDescription(szFileClass, strDescription);
	
				// Load the caption for the message box and message string
				LoadString(m_hInstance, IDS_FILE_TYPES, szCaption, sizeof(szCaption));
				LoadString(m_hInstance, IDS_EXT_IN_USE, szMessage, sizeof(szMessage));
			
				// Format the text
				lpszText = (LPSTR)CoTaskMemAlloc(lstrlen(szMessage) + m_strExtension.GetLength() + strDescription.GetLength());
				wsprintf(lpszText, szMessage, (LPCSTR)m_strExtension, (LPCSTR)strDescription);
			
				// Display the message box
				MessageBox(m_hwndDlg, lpszText, szCaption, MB_OK | MB_ICONSTOP);
				CoTaskMemFree(lpszText);
				SetFocus(GetDlgItem(m_hwndDlg, IDC_EDIT2));
				return FALSE;
			}
        }
#endif
	}

	return TRUE;
}

BOOL
CNewTypeDialog::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_BUTTON1 && notifyCode == BN_CLICKED) {
        char    szPath[_MAX_PATH];

        if (BrowseForProgram(m_hInstance, m_hwndDlg, szPath))
            Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT4), szPath);
		return TRUE;

	} else if (id == IDC_EDIT2 && notifyCode == EN_KILLFOCUS) {
		CString	strExtension;
		char	szFileClass[80];

		// The edit control for specifying the extension has lost focus.
		// See if there's an existing file type class for the extension, and
		// pre-fill the open command edit control
		EditFieldTransfer(IDC_EDIT2, strExtension, TRUE);

		// See if the extension begins with a leading '.' If it does strip
		// it off
		while (strExtension[0] == '.') {
			strExtension = strExtension.Mid(1);

			// Update what's displayed so the user sees we changed the extension
			Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT2), (LPCSTR)strExtension);
		}

		// Look up the file type class for the extension
        if (GetFileClass((LPCSTR)strExtension, szFileClass, sizeof(szFileClass))) {
			char	szOpenCmd[_MAX_PATH + 32];
	
			// Get the shell open command
			if (GetShellOpenCommand(szFileClass, szOpenCmd, sizeof(szOpenCmd))) {
				// Pre-fill the open command edit control
				Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT4), szOpenCmd);
			}

			// If the user didn't specify a description then pre-fill that, too
			if (Edit_GetTextLength(GetDlgItem(m_hwndDlg, IDC_EDIT1)) == 0) {
				CString	strDescription;

				if (GetFileTypeDescription(szFileClass, strDescription)) {
					// Pre-fill the description edit control
					Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT1), (LPCSTR)strDescription);
				}
			}
		}

		return TRUE;
	}

	return CDialog::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CApplicationsPrefs implementation

CApplicationsPrefs::CApplicationsPrefs()
	: CBrowserPropertyPage(IDD_APPLICATIONS, HELP_PREFS_BROWSER_APPLICATIONS)
{
	m_lpBrowserPrefs = NULL;
}


// Override SetObjects() member function to acquire/release the IBrowserPrefs
// interface pointer
STDMETHODIMP
CApplicationsPrefs::SetObjects(ULONG cObjects, LPUNKNOWN FAR* ppunk)
{
	HRESULT	hres = CBrowserPropertyPage::SetObjects(cObjects, ppunk);

	if (SUCCEEDED(hres)) {
		if (cObjects == 0) {
			assert(m_lpBrowserPrefs);
			if (m_lpBrowserPrefs) {
				// Release the interface pointer
				m_lpBrowserPrefs->Release();
				m_lpBrowserPrefs = NULL;
			}

		} else {
			assert(!m_lpBrowserPrefs);
			if (!m_lpBrowserPrefs) {
				m_pObject->QueryInterface(IID_IBrowserPrefs, (void **)&m_lpBrowserPrefs);
				assert(m_lpBrowserPrefs);
			}
		}
	}

	return hres;
}

BOOL
CApplicationsPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	return TRUE;
}

// Apply changes using XP preferences
BOOL
CApplicationsPrefs::ApplyChanges()
{
	return TRUE;
}

BOOL
CApplicationsPrefs::InitDialog()
{
	HWND    hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
	int		nIndex;

	// We don't receive the WM_MEASUREITEM message, because it's sent before
	// the WM_INITDIALOG, so set the height of the list box items now
	SetLBoxItemHeight();
	
	// Fill the list box with the list of helper applications
	assert(m_lpBrowserPrefs);
	if (m_lpBrowserPrefs) {
		LPENUMHELPERS	pEnumHelpers;

		if (SUCCEEDED(m_lpBrowserPrefs->EnumHelpers(&pEnumHelpers))) {
			NET_cdataStruct	*cdata;
		
			while (pEnumHelpers->Next(&cdata) == NOERROR) {
				assert(cdata->ci.desc);
				nIndex = ListBox_AddString(hList, cdata->ci.desc);
				ListBox_SetItemData(hList, nIndex, cdata);
			}
		
			pEnumHelpers->Release();
		}
	}

	// Add telnet and tn3270 items. Store the file class in the type field
	memset(&m_telnet, 0, sizeof(m_telnet));
	m_telnet.ci.type = "telnet";
	if (!GetFileTypeDescription("telnet", m_strTelnet))
		m_strTelnet.LoadString(m_hInstance, IDS_TELNET);
	m_telnet.ci.desc = (LPSTR)(LPCSTR)m_strTelnet;
	nIndex = ListBox_AddString(hList, m_telnet.ci.desc);
	ListBox_SetItemData(hList, nIndex, &m_telnet);

	memset(&m_tn3270, 0, sizeof(m_tn3270));
	m_tn3270.ci.type = "tn3270";
	if (!GetFileTypeDescription("tn3270", m_strTN3270))
		m_strTN3270.LoadString(m_hInstance, IDS_TN3270);
	m_tn3270.ci.desc = (LPSTR)(LPCSTR)m_strTN3270;
	nIndex = ListBox_AddString(hList, m_tn3270.ci.desc);
	ListBox_SetItemData(hList, nIndex, &m_tn3270);

	// Select the first item in the list and update the file details
	ListBox_SetCurSel(hList, 0);
	DisplayFileDetails();
	
	return CBrowserPropertyPage::InitDialog();
}

static void
GetAppPath(LPCSTR lpszOpenCmd, LPSTR lpszAppPath)
{
    LPSTR   lpszEnd;
    
    // The command string value specifies the path and filename of the application
    // and includes command-line options. Do what the shell does which is assume
    // that the path and filename ends at the first space. Long filenames that
    // include spaces will be in quotes
    if (*lpszOpenCmd == '"') {
        // Skip the beginning quote
        lstrcpy(lpszAppPath, lpszOpenCmd + 1);

        // Look for the terminating quote
        lpszEnd = strchr(lpszAppPath, '"');

        if (lpszEnd)
            *lpszEnd = '\0';  // null terminate at the closing quote

    } else {
        lstrcpy(lpszAppPath, lpszOpenCmd);

        // Look for the first space
        lpszEnd = strchr(lpszAppPath, ' ');
        
        if (lpszEnd)
            *lpszEnd = '\0';  // null terminate at the space
    }

    // Ignore a filename that is "%1". This happens for things like MS-DOS
    // Batch Files which just expand into the name of the file and a command
    // line option
    if (strcmp(lpszAppPath, "%1") == 0)
        lpszAppPath[0] = '\0';
}

// Build the file extension list
static void
BuildExtensionList(NET_cdataStruct *pcdata, CString &strExts)
{
	for (int i = 0; i < pcdata->num_exts; i++) {
		if (i > 0)
			strExts += ' ';

		strExts += pcdata->exts[i];
	}
	
	strExts.MakeUpper();
}

#ifdef MOZ_MAIL_NEWS
// Scans through prefs for the extensions listed in cdata, and sees whether or
// not this is the default outgoing MIME type for the given list.  If all in the
// list say it is the default, returns 1.  If none do, returns 0.  If some do and
// some don't, returns 2.
static int
GetDefaultMIMEStateFromExtensionList(NET_cdataStruct *pcdata)
{
	BOOL someDefaultsFound = FALSE, someNonDefaultsFound = FALSE;
	int i = 0;

	while (i < pcdata->num_exts)
	{
		BOOL defaultFound = FALSE;
		CString pref("mime.table.extension.");
		pref += CString(pcdata->exts[i]);
		pref += CString(".outgoing_default_type");
		CString defaultMimeTypeFromPrefs;
		PREF_GetStringPref(pref, defaultMimeTypeFromPrefs);
		defaultFound = (defaultMimeTypeFromPrefs.CompareNoCase(pcdata->ci.type) == 0);

		someDefaultsFound = someDefaultsFound || defaultFound;
		someNonDefaultsFound = someNonDefaultsFound || !defaultFound;
		i++;
	}

	if (someDefaultsFound && someNonDefaultsFound)
		return 2;
	else if (someDefaultsFound && !someNonDefaultsFound)
		return 1;
	else // none in the list, or none are the default
		return 0;
}

static void
SetDefaultMIMEStateFromExtensionList(NET_cdataStruct *pcdata, int newState)
{
	int i = 0;

	if (newState != 0 && newState != 1)
		return;

	while (i < pcdata->num_exts)
	{
		CString pref("mime.table.extension.");
		pref += CString(pcdata->exts[i]);
		pref += CString(".outgoing_default_type");
		CString defaultMimeTypeFromPrefs;
		if (newState == 1)
			PREF_SetCharPref(pref, pcdata->ci.type);
		else
			PREF_ClearUserPref(pref);
		i++;
	}
}

// Scans through prefs for the extensions listed in cdata, and sees whether or
// not those preferences are locked down.  If any of them are, returns TRUE, otherwise FALSE.
static BOOL
GetDefaultMIMEStateLockedFromExtensionList(NET_cdataStruct *pcdata)
{
	BOOL locked = FALSE;
	int i = 0;

	while (i < pcdata->num_exts && !locked)
	{
		CString pref("mime.table.extension.");
		pref += CString(pcdata->exts[i]);
		pref += (".outgoing_default_type");
		locked = PREF_PrefIsLocked(pref);
		i++;
	}

	return locked;
}
#endif /* MOZ_MAIL_NEWS */

static HICON
GetNavigatorAppIcon(HINSTANCE hInstance)
{
	char	szFilename[_MAX_PATH];

#ifdef _WIN32
	SHFILEINFO	sfi;

	GetModuleFileName(NULL, szFilename, sizeof(szFilename));
	SHGetFileInfo(szFilename, 0, &sfi, sizeof(sfi), SHGFI_ICON);
	return sfi.hIcon;
#else
	TASKENTRY	te;

	te.dwSize = sizeof(te);
	if (TaskFindHandle(&te, GetCurrentTask())) {
		GetModuleFileName(te.hModule, szFilename, sizeof(szFilename));
		return ExtractIcon(hInstance, szFilename, 0);
	}
#endif

	return NULL;
}
						
// Given the Open command returns the base filename without any extension and
// the associated application icon
static void
GetAppNameAndIcon(HINSTANCE hInstance, LPCSTR lpszOpenCmd, CString &strFilename, HICON &hIcon)
{
	char    szAppPath[_MAX_PATH];
	char	szFile[_MAX_FNAME];

	// Get the app path and filename
	GetAppPath(lpszOpenCmd, szAppPath);

	// Get the base filename without extension
	_splitpath(szAppPath, NULL, NULL, szFile, NULL);
	strFilename = szFile;
	strFilename.MakeUpper();

	// Get the icon associated with the application
#ifdef _WIN32
	SHFILEINFO	sfi;
	LPSTR		lpszFilePart;
	char		szBuf[MAX_PATH];

	// Get a full pathname
	SearchPath(NULL, szAppPath, ".exe", sizeof(szBuf), szBuf, &lpszFilePart);
	SHGetFileInfo(szBuf, 0, &sfi, sizeof(sfi), SHGFI_ICON);
	hIcon = sfi.hIcon;
#else
	hIcon = ExtractIcon(hInstance, szAppPath, 0);
#endif
}

void
CApplicationsPrefs::DisplayFileDetails()
{
	HICON			 hDocIcon = NULL, hAppIcon = NULL;
#ifndef _WIN32
	HICON			 hOldDocIcon = NULL, hOldAppIcon = NULL;
#endif
	NET_cdataStruct	*cdata;
	HWND			 hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
	HELPERINFO		 info;
	int				 nIndex;
	CString			 strHandledBy, strExts;
	LPCSTR			 lpszMimeType = "";
	BOOL			 bIsPlugin = FALSE;
//Begin CRN_MIME
	BOOL			 bIsLocked = FALSE; 
	XP_Bool			 bAddEnabled = TRUE;
	XP_Bool			 bEditEnabled = TRUE;
	XP_Bool			 bRemoveEnabled = TRUE;

	PREF_GetBoolPref("mime.table.allow_add", &bAddEnabled);
	PREF_GetBoolPref("mime.table.allow_edit", &bEditEnabled);
	PREF_GetBoolPref("mime.table.allow_remove", &bRemoveEnabled);
//End CRN_MIME

	// Get the currently selected item
	nIndex = ListBox_GetCurSel(hList);
	assert(nIndex != LB_ERR);

	// Get the netlib data structure
	cdata = (NET_cdataStruct *)ListBox_GetItemData(hList, nIndex);
	assert(cdata);

	// If there's no front-end data structure then it's one of the special
	// items (telnet or tn3270)
	if (cdata->ci.fe_data) {
		// Get the MIME type
		if (strncmp(cdata->ci.type, SZ_WINASSOC, lstrlen(SZ_WINASSOC)) != 0)
			lpszMimeType = cdata->ci.type;
		
		// Get some information about the helper app
		assert(m_lpBrowserPrefs);
		if (m_lpBrowserPrefs && m_lpBrowserPrefs->GetHelperInfo(cdata, &info) == ERROR_SUCCESS) {
			switch (info.nHowToHandle) {
				case HANDLE_VIA_NETSCAPE:
					strHandledBy.LoadString(m_hInstance, IDS_INTERNAL);
	
					// Get the application icon
					hAppIcon = GetNavigatorAppIcon(m_hInstance);
					break;
	
				case HANDLE_UNKNOWN:
				case HANDLE_MOREINFO:
					assert(FALSE);
					break;
	
				case HANDLE_EXTERNAL:
				case HANDLE_SHELLEXECUTE:
				case HANDLE_BY_OLE:
					if (*info.szOpenCmd != '\0') {
						// Get the name to display in the handled by field (no extension),
						// and the application icon
						GetAppNameAndIcon(m_hInstance, info.szOpenCmd, strHandledBy, hAppIcon);
					}
					break;
	
				case HANDLE_SAVE:
					strHandledBy.LoadString(m_hInstance, IDS_SAVE_TO_DISK);
					break;
	
				case HANDLE_VIA_PLUGIN:
					strHandledBy.LoadString(m_hInstance, IDS_PLUGIN);
					bIsPlugin = TRUE;
					break;
			}

			bIsLocked = info.bIsLocked; //CRN_MIME
		}

	} else {
		char	szOpenCmd[_MAX_PATH + 32];

		// It's either telnet or tn3270
		assert(cdata->ci.type);
		hDocIcon = GetDocumentIcon(m_hInstance, cdata->ci.type);
		
		if (GetShellOpenCommand(cdata->ci.type, szOpenCmd, sizeof(szOpenCmd))) {
			// Get the name to display in the handled by field (no extension),
			// and the application icon
			GetAppNameAndIcon(m_hInstance, szOpenCmd, strHandledBy, hAppIcon);
		}
	}
	
	// Build the file extension list
	BuildExtensionList(cdata, strExts);

	// Get the document icon
	if (cdata->num_exts > 0) {
#ifdef _WIN32
        char		szBuf[_MAX_PATH];
		SHFILEINFO	sfi;

		wsprintf(szBuf, "file.%s", cdata->exts[0]);
		SHGetFileInfo(szBuf, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES);
		hDocIcon = sfi.hIcon;
#else
		char	szFileClass[80];
		
		if (GetFileClass(cdata->exts[0], szFileClass, sizeof(szFileClass)))
			hDocIcon = GetDocumentIcon(m_hInstance, szFileClass);

		if (!hDocIcon)
			hDocIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_DOCUMENT));
#endif
	}

	// Update the file type details
	Static_SetText(GetDlgItem(m_hwndDlg, IDC_MIME_TYPE), lpszMimeType);
	Static_SetText(GetDlgItem(m_hwndDlg, IDC_EXTENSION), (LPCSTR)strExts);
	Static_SetText(GetDlgItem(m_hwndDlg, IDC_HANDLED_BY), (LPCSTR)strHandledBy);
#ifndef _WIN32
	Static_GetIcon(GetDlgItem(m_hwndDlg, IDC_ICON1), hOldDocIcon);
	Static_GetIcon(GetDlgItem(m_hwndDlg, IDC_ICON2), hOldAppIcon);
#endif
	Static_SetIcon(GetDlgItem(m_hwndDlg, IDC_ICON1), hDocIcon);
	Static_SetIcon(GetDlgItem(m_hwndDlg, IDC_ICON2), hAppIcon);
#ifndef _WIN32
	// Minimize system resource usage by destroying the previously displayed icons
	if (hOldDocIcon && (hOldDocIcon != hDocIcon))
		DestroyIcon(hOldDocIcon);
	if (hOldAppIcon && (hOldAppIcon != hAppIcon))
		DestroyIcon(hOldAppIcon);
#endif

	EnableDlgItem(IDC_BUTTON1, bAddEnabled); //This is the "New Type.." button.	CRN_MIME 

	// Disable the Edit and Remove buttons if the item is handled by a plug-in,
	// because changing or removing the item won't actually do anything
	EnableDlgItem(IDC_BUTTON2, !bIsPlugin && !bIsLocked && bEditEnabled); //CRN_MIME Added "&& !bIsLocked && bEditEnabled
	EnableDlgItem(IDC_BUTTON3, !bIsPlugin && !bIsLocked && bRemoveEnabled); //CRN_MIME Added "&& !bIsLocked && bRemoveEnabled
}

void
CApplicationsPrefs::OnEditItem()
{
	NET_cdataStruct	*cdata;
	CEditTypeDialog	 dlg;
	HWND			 hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
	HELPERINFO		 info;
	int				 nIndex;

	// Get the currently selected item
	nIndex = ListBox_GetCurSel(hList);
	assert(nIndex != LB_ERR);

	// Get the netlib data structure
	cdata = (NET_cdataStruct *)ListBox_GetItemData(hList, nIndex);
	assert(cdata);

	// Get some information about the helper app
	if (!cdata->ci.fe_data) {
		char	szOpenCmd[_MAX_PATH + 32];

		// This is telnet or tn3270
		dlg.m_bURLProtocol = TRUE;
		dlg.m_nHowToHandle = HANDLE_SHELLEXECUTE;
		if (GetShellOpenCommand(cdata->ci.type, szOpenCmd, sizeof(szOpenCmd)))
			dlg.m_strOpenCmd = szOpenCmd;

	} else {
		assert(m_lpBrowserPrefs);
		if (m_lpBrowserPrefs && m_lpBrowserPrefs->GetHelperInfo(cdata, &info) == ERROR_SUCCESS) {
			dlg.m_nHowToHandle = info.nHowToHandle;
			dlg.m_strOpenCmd = info.szOpenCmd;
			dlg.m_bConfirmBeforeOpening = info.bAskBeforeOpening;
		}
		
		dlg.m_strMimeType = strncmp(cdata->ci.type, SZ_WINASSOC, lstrlen(SZ_WINASSOC)) == 0 ?
			"" : cdata->ci.type;
	}
	
	dlg.m_hDocIcon = Static_GetIcon(GetDlgItem(m_hwndDlg, IDC_ICON1), hOldDocIcon);
	dlg.m_strDescription = cdata->ci.desc;
	
	// Build the file extension list
	BuildExtensionList(cdata, dlg.m_strExtension);
#ifdef MOZ_MAIL_NEWS
	int originalUseAsDefaultMIMEType = GetDefaultMIMEStateFromExtensionList(cdata);
	dlg.m_UseAsDefaultMIMEType = originalUseAsDefaultMIMEType;
	dlg.m_LockDefaultMIMEType = GetDefaultMIMEStateLockedFromExtensionList(cdata);
#endif /* MOZ_MAIL_NEWS */

	// Display the dialog
	if (dlg.DoModal(GetParent(m_hwndDlg)) == IDOK) {

#ifdef MOZ_MAIL_NEWS
		if (originalUseAsDefaultMIMEType != dlg.m_UseAsDefaultMIMEType)
		{
			SetDefaultMIMEStateFromExtensionList(cdata, dlg.m_UseAsDefaultMIMEType);
		}
#endif /* MOZ_MAIL_NEWS */

		if (!cdata->ci.fe_data) {
			CString	strDescription;

			// This is telnet or tn3270
			assert(cdata->ci.type);
			SetShellOpenCommand(cdata->ci.type, (LPCSTR)dlg.m_strOpenCmd);

			// Make sure there's a description for the file type class
            if (!GetFileTypeDescription(cdata->ci.type, strDescription))
				SetFileTypeDescription(cdata->ci.type, cdata->ci.desc);
			
			// Update the file details
			DisplayFileDetails();

		} else {
			// Call browser to change the type
			assert(m_lpBrowserPrefs);
			if (m_lpBrowserPrefs) {
				HELPERINFO	info;

				info.nHowToHandle = dlg.m_nHowToHandle;
				lstrcpy(info.szOpenCmd, (LPCSTR)dlg.m_strOpenCmd);
				info.bAskBeforeOpening = dlg.m_bConfirmBeforeOpening;
				info.lpszMimeType = (LPCSTR)dlg.m_strMimeType;

				if (m_lpBrowserPrefs->SetHelperInfo(cdata, &info) == NOERROR)
					DisplayFileDetails();
			}
		}
	}
}

void
CApplicationsPrefs::OnNewItem()
{
	CNewTypeDialog	dlg;

	if (dlg.DoModal(GetParent(m_hwndDlg)) == IDOK) {
		// If the user didn't supply a description then create one
		// based on the file extension
		if (dlg.m_strDescription.IsEmpty()) {
			char	szFile[256];

			dlg.m_strDescription = dlg.m_strExtension;
			dlg.m_strDescription.MakeUpper();
			dlg.m_strDescription += ' ';
			LoadString(m_hInstance, IDS_FILE, szFile, sizeof(szFile));
			dlg.m_strDescription += szFile;
		}

		// Call browser to create the new type
		assert(m_lpBrowserPrefs);
		if (m_lpBrowserPrefs) {
			NET_cdataStruct	*cdata;
			int				 nIndex;

			if (m_lpBrowserPrefs->NewFileType((LPCSTR)dlg.m_strDescription,
											  (LPCSTR)dlg.m_strExtension,
											  (LPCSTR)dlg.m_strMimeType,
											  (LPCSTR)dlg.m_strOpenCmd,
											  &cdata) == NOERROR) {

				HWND	hList = GetDlgItem(m_hwndDlg, IDC_LIST1);

				// Add a new list box item
				assert(cdata && cdata->ci.desc);
				nIndex = ListBox_AddString(hList, cdata->ci.desc);
				ListBox_SetItemData(hList, nIndex, cdata);

#ifdef MOZ_MAIL_NEWS 
				if (dlg.m_UseAsDefaultMIMEType)
				{
					CString pref("mime.table.extension.");
					pref += CString(dlg.m_strExtension);
					pref += CString(".outgoing_default_type");
					if (!PREF_PrefIsLocked(pref))
						PREF_SetCharPref(pref, dlg.m_strMimeType);
				}
#endif /* MOZ_MAIL_NEWS */

				// Select the item and update the file details
				ListBox_SetCurSel(hList, nIndex);
				DisplayFileDetails();
			}
		}
	}
}

void
CApplicationsPrefs::OnRemoveItem()
{
	NET_cdataStruct	*cdata;
	HWND			 hList = GetDlgItem(m_hwndDlg, IDC_LIST1);
	int				 nCurSel;
	char			 szTitle[256];
	char			 szCaption[256];

	// Get the currently selected item
	nCurSel = ListBox_GetCurSel(hList);
	assert(nCurSel != LB_ERR);

	// Get the netlib data structure
	cdata = (NET_cdataStruct *)ListBox_GetItemData(hList, nCurSel);
	assert(cdata);

	// Ask the user to confirm
	LoadString(m_hInstance, IDS_CONFIRM_REMOVE, szTitle, sizeof(szTitle));
	LoadString(m_hInstance, IDS_FILE_TYPES, szCaption, sizeof(szCaption));
	if (MessageBox(GetParent(m_hwndDlg), szTitle, szCaption, MB_YESNO | MB_ICONQUESTION) == IDYES) {
		// Call browser to remove the item
		assert(m_lpBrowserPrefs);
		if (m_lpBrowserPrefs) {
			if (m_lpBrowserPrefs->RemoveFileType(cdata) == NOERROR) {
				// Remove this item from the list
				ListBox_DeleteString(hList, nCurSel);
				if (ListBox_GetCount(hList) > 0)
					ListBox_SetCurSel(hList, nCurSel > 0 ? nCurSel - 1 : 0);
				DisplayFileDetails();
			}
		}
	}
}

BOOL
CApplicationsPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_LIST1 && notifyCode == LBN_SELCHANGE) {
		DisplayFileDetails();
		return TRUE;

	} else if (id == IDC_BUTTON1 && notifyCode == BN_CLICKED) {
		OnNewItem();
		return TRUE;
	
	} else if (id == IDC_BUTTON2 && notifyCode == BN_CLICKED) {
		OnEditItem();
		return TRUE;
	
	} else if (id == IDC_BUTTON3 && notifyCode == BN_CLICKED) {
		OnRemoveItem();
		return TRUE;
	}

	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

void
CApplicationsPrefs::SetLBoxItemHeight()
{
	TEXTMETRIC	tm;
	HWND		hwndCtl = GetDlgItem(m_hwndDlg, IDC_LIST1);
	HFONT		hFont = GetWindowFont(hwndCtl);
	HDC			hDC = GetDC(hwndCtl);
	HFONT		hOldFont = NULL;

	if (hFont)
		hOldFont = SelectFont(hDC, hFont);

	GetTextMetrics(hDC, &tm);

	// Cleanup and release the HDC
	if (hOldFont)
		SelectObject(hDC, hOldFont);
	ReleaseDC(hwndCtl, hDC);

	// Set the height of a single item
	ListBox_SetItemHeight(hwndCtl, 0, tm.tmHeight + 3);
}

#define LEFT_PADDING	2
#define TOP_PADDING		1
#define RIGHT_PADDING	5

void
CApplicationsPrefs::DrawLBoxItem(LPDRAWITEMSTRUCT lpdis)
{
	NET_cdataStruct *cdata;
	int				 nColorIndex;
	HWND			 hwndFocus = GetFocus();  // don't trust ODS_FOCUS
	int				 nStrLen;
	SIZE			 size;

	switch (lpdis->itemAction) {
		case ODA_DRAWENTIRE:
		case ODA_SELECT:
		case ODA_FOCUS:
			nColorIndex = COLOR_WINDOWTEXT;
			if ((lpdis->itemState & ODS_SELECTED) && (hwndFocus == lpdis->hwndItem))
				nColorIndex = COLOR_HIGHLIGHTTEXT;
			SetTextColor(lpdis->hDC, GetSysColor(nColorIndex));

			// Behave like a 32-bit list view and use a different color to indicate
			// selection when we don't have the focus
			nColorIndex = COLOR_WINDOW;
			if (lpdis->itemState & ODS_SELECTED)
				nColorIndex = (hwndFocus == lpdis->hwndItem ? COLOR_HIGHLIGHT : COLOR_BTNFACE);
			SetBkColor(lpdis->hDC, GetSysColor(nColorIndex));
	
			assert(lpdis->itemID != (UINT)-1);
			cdata = (NET_cdataStruct *)lpdis->itemData;

			// Display the description
			nStrLen = lstrlen(cdata->ci.desc);
#ifdef _WIN32
			GetTextExtentPoint32(lpdis->hDC, cdata->ci.desc, nStrLen, &size);
#else
			GetTextExtentPoint(lpdis->hDC, cdata->ci.desc, nStrLen, &size);
#endif
			lpdis->rcItem.right = lpdis->rcItem.left + size.cx + LEFT_PADDING + RIGHT_PADDING;

			ExtTextOut(lpdis->hDC, lpdis->rcItem.left + LEFT_PADDING, lpdis->rcItem.top + TOP_PADDING,
				ETO_OPAQUE, &lpdis->rcItem, cdata->ci.desc, nStrLen, NULL);
			
			// Draw the focus rect if necessary
			if (lpdis->itemState & ODS_FOCUS)
				::DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
			break;
	}
}

LRESULT
CApplicationsPrefs::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPDRAWITEMSTRUCT	lpdis;
	LPCOMPAREITEMSTRUCT	lpcis;

	switch (uMsg) {
		case WM_DRAWITEM:
			lpdis = (LPDRAWITEMSTRUCT)lParam;

			if (lpdis->CtlID == IDC_LIST1)
				DrawLBoxItem(lpdis);
			return TRUE;

		case WM_COMPAREITEM:
			lpcis = (LPCOMPAREITEMSTRUCT)lParam;
			if (lpcis->CtlID == IDC_LIST1) {
				return lstrcmp(((NET_cdataStruct *)lpcis->itemData1)->ci.desc,
						       ((NET_cdataStruct *)lpcis->itemData2)->ci.desc);
			}
			return TRUE;
	}

	return CBrowserPropertyPage::WindowProc(uMsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////
// CSmartBrowsingPrefs implementation

CSmartBrowsingPrefs::CSmartBrowsingPrefs()
	: CBrowserPropertyPage(IDD_SMARTBROWSING, HELP_PREFS_BROWSER)
{
}

// Initialize member data using XP preferences
STDMETHODIMP
CSmartBrowsingPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		PREF_GetStringPref("browser.related.disabledForDomains", m_strExcludedDomains);
		PREF_GetBoolPref("browser.goBrowsing.enabled", &m_bEnableKeywords);
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CSmartBrowsingPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	CheckBoxTransfer(IDC_CHECK2, m_bEnableKeywords, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT1, m_strExcludedDomains, bSaveAndValidate);
	return TRUE;
}

// Apply changes using XP preferences
BOOL
CSmartBrowsingPrefs::ApplyChanges()
{
	PREF_SetBoolPref("browser.goBrowsing.enabled", m_bEnableKeywords);
	PREF_SetCharPref("browser.related.disabledForDomains", (LPCSTR)m_strExcludedDomains);
	return TRUE;
}

BOOL
CSmartBrowsingPrefs::InitDialog()
{
	CheckIfLockedPref("browser.related.disabledForDomains", IDC_EDIT1);
	CheckIfLockedPref("browser.goBrowsing.enabled", IDC_CHECK2);

	return CBrowserPropertyPage::InitDialog();
}
