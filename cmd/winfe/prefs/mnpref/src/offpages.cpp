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
#include "colorbtn.h"
#include "bitmpbtn.h"
#include "resource.h"
#include "structs.h"
#include "xp_core.h"
#include "xp_help.h"
#include "prefapi.h"
#include "mninterf.h"
#include "mnprefid.h"

extern void ValidateNumeric
(HWND hParent, HINSTANCE hInst, int nCaptionID, int nID, int& data);

/////////////////////////////////////////////////////////////////////////////
// COfflinePrefs implementation

COfflinePrefs::COfflinePrefs()
	: CMailNewsPropertyPage(IDD_OFFLINE, HELP_PREFS_OFFLINE)
{
	m_nModeRadioChoice = 0;	//  0 = IDC_PREVIOUS_STATE
							//  1 = IDC_ASK_ME
	m_nOnlineRadioChoice = 0; // 0 = IDC_ONLINE_RADIO1 Ask me	  
							  // 1 = IDC_ONLINE_RADIO2 Automatically send
							  // 2 = IDC_ONLINE_RADIO3 Do not send
	m_bPromptSynchronize = TRUE;
}

BOOL COfflinePrefs::InitDialog()
{
	if (MNPREF_PrefIsLocked("offline.startup_state"))
	{
		EnableDlgItem(IDC_PREVIOUS_STATE, FALSE);
		EnableDlgItem(IDC_ASK_ME, FALSE);
	}
	
	if(MNPREF_PrefIsLocked("offline.send.unsent_messages"))
	{
		EnableDlgItem(IDC_ONLINE_RADIO1, FALSE);
		EnableDlgItem(IDC_ONLINE_RADIO2, FALSE);
		EnableDlgItem(IDC_ONLINE_RADIO3, FALSE);
	}

	if(MNPREF_PrefIsLocked("offline.prompt_synch_on_exit"))
	{
		EnableDlgItem(IDC_PROMPT_SYNCHRONIZE, FALSE);
	}

	return CMailNewsPropertyPage::InitDialog();
}

// Initialize member data using XP preferences
STDMETHODIMP COfflinePrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		int32 n;

		PREF_GetIntPref("offline.startup_state", &n);
		m_nModeRadioChoice = (int)n;

		PREF_GetIntPref("offline.send.unsent_messages", &n);
		m_nOnlineRadioChoice = (int)n;

		XP_Bool prompt;
		PREF_GetBoolPref("offline.prompt_synch_on_exit", &prompt);
		m_bPromptSynchronize = prompt;
	}

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL COfflinePrefs::DoTransfer(BOOL bSaveAndValidate)
{
	RadioButtonTransfer(IDC_PREVIOUS_STATE, m_nModeRadioChoice, bSaveAndValidate);
	RadioButtonTransfer(IDC_ONLINE_RADIO1, m_nOnlineRadioChoice, bSaveAndValidate);
	CheckBoxTransfer(IDC_PROMPT_SYNCHRONIZE, m_bPromptSynchronize, bSaveAndValidate);

	return TRUE;
}

BOOL COfflinePrefs::ApplyChanges()
{
	PREF_SetIntPref("offline.startup_state", (int32)m_nModeRadioChoice);
	PREF_SetIntPref("offline.send.unsent_messages", (int32)m_nOnlineRadioChoice);
	PREF_SetBoolPref("offline.prompt_synch_on_exit", m_bPromptSynchronize);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// COfflineNewsPrefs implementation

COfflineNewsPrefs::COfflineNewsPrefs()
	: CMailNewsPropertyPage(IDD_OFFLINE_NEWS, HELP_PREFS_OFFLINE_GROUPS)
{
	m_nDownloadRadioChoice = 0;
}

BOOL COfflineNewsPrefs::InitDialog()
{
	char	szBuf[256];

	HWND hwndCombo = GetDlgItem(m_hwndDlg, IDC_COMBO_FROM);
	for (int i = IDS_YESTERDAY; i <= IDS_1_YEAR; i++) 
	{
		int	nLen = LoadString(m_hInstance, i, szBuf, sizeof(szBuf));
		
		assert(nLen > 0);
		ComboBox_AddString(hwndCombo, szBuf);
	}
	EnableCheckByDate(m_bByDate);

	// Limit the length of the string the user can type
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_DAYS), 5);

	CheckIfLockedPref("offline.news.download.unread_only", IDC_UNREAD_ONLY);
	if (CheckIfLockedPref("offline.news.download.by_date", IDC_DOWNLOAD_BY_DATE)
		|| MNPREF_PrefIsLocked("offline.news.download.use_days"))
	{
		EnableDlgItem(IDC_RADIO_FROM, FALSE);
		EnableDlgItem(IDC_RADIO_SINCE, FALSE);
		EnableDlgItem(IDC_COMBO_FROM, FALSE);
		EnableDlgItem(IDC_EDIT_DAYS, FALSE);
	}
	CheckIfLockedPref("offline.news.download.days", IDC_COMBO_FROM);
	CheckIfLockedPref("offline.news.download.increments", IDC_EDIT_DAYS);
	
	return CMailNewsPropertyPage::InitDialog();
}

STDMETHODIMP COfflineNewsPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		BOOL    bMode;
		int32	n = 0;
 	 
		PREF_GetBoolPref("offline.news.download.unread_only", &m_bUnreadOnly);
		PREF_GetBoolPref("offline.news.download.by_date", &m_bByDate);
		PREF_GetBoolPref("offline.news.download.use_days", &bMode);
		if (bMode)
			m_nDownloadRadioChoice = 1;
		else
			m_nDownloadRadioChoice = 0;
		PREF_GetIntPref("offline.news.download.days", &n);
		m_nDownLoadDays = (int)n;
		PREF_GetIntPref("offline.news.download.increments", &n);
		m_nDownLoadInterval = (int)n;

		PREF_GetIntPref("offline.news.discussions_count", &n);
		m_nDiscussions = (int)n;
	}

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL COfflineNewsPrefs::DoTransfer(BOOL bSaveAndValidate)
{					   
	CheckBoxTransfer(IDC_UNREAD_ONLY, m_bUnreadOnly, bSaveAndValidate);
	CheckBoxTransfer(IDC_DOWNLOAD_BY_MSG, m_bByMessage, bSaveAndValidate);
	CheckBoxTransfer(IDC_DOWNLOAD_BY_DATE, m_bByDate, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO_FROM, m_nDownloadRadioChoice, bSaveAndValidate);
	ComboBoxTransfer(IDC_COMBO_FROM , m_nDownLoadInterval, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_DAYS, m_nDownLoadDays, bSaveAndValidate);
	return TRUE;
}

BOOL COfflineNewsPrefs::ApplyChanges()
{
	PREF_SetBoolPref("offline.news.download.unread_only", m_bUnreadOnly);

	PREF_SetBoolPref("offline.news.download.by_date", m_bByDate);
	if (m_nDownloadRadioChoice == 0)
		PREF_SetBoolPref("offline.news.download.use_days", FALSE);
	else
		PREF_SetBoolPref("offline.news.download.use_days", TRUE);

	PREF_SetIntPref("offline.news.download.increments", (int32)m_nDownLoadInterval);
	PREF_SetIntPref("offline.news.download.days", (int32)m_nDownLoadDays);
	return TRUE;
}

BOOL COfflineNewsPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_DOWNLOAD_BY_DATE && notifyCode == BN_CLICKED) 
	{
		BOOL bBool = IsDlgButtonChecked(m_hwndDlg, IDC_DOWNLOAD_BY_DATE);
		EnableCheckByDate(bBool);
	}
	else if (id == IDC_EDIT_DAYS && notifyCode == EN_UPDATE) 
	{
		ValidateNumeric(m_hwndDlg, m_hInstance, IDS_DOWNLOAD, 
						id, m_nDownLoadDays);
		return TRUE;
	}
	else if (id == IDC_SELECT_DOWNLOAD && notifyCode == BN_CLICKED) 
	{
		assert(m_pObject);
		if (m_pObject) 
		{
			LPOFFLINEINTERFACE	lpInterface;

			// Request the IMailNewsInterface interface from our data object
			if (SUCCEEDED(m_pObject->QueryInterface(IID_OfflineInterface, 
						                            (void **)&lpInterface))) 
			{
				// Get the number of encodings
				lpInterface->DoSelectDiscussion(GetParent(m_hwndDlg));
				lpInterface->Release();
			}
		}
		return TRUE;

	}

	return CDialog::OnCommand(id, hwndCtl, notifyCode);
}

void COfflineNewsPrefs::EnableCheckByDate(BOOL bEnable)
{
	EnableDlgItem(IDC_RADIO_FROM, bEnable);
	EnableDlgItem(IDC_RADIO_SINCE, bEnable);
	EnableDlgItem(IDC_COMBO_FROM, bEnable);
	EnableDlgItem(IDC_EDIT_DAYS, bEnable);
	EnableDlgItem(IDC_STATIC_DAYS_AGO, bEnable);
}

void COfflineNewsPrefs::SetDiscussionCount(int nCount)
{
	char szFormat[256], szText[256];
	int	nLen = LoadString(m_hInstance, IDS_SELECT_COUNT, szFormat, sizeof(szFormat));
	sprintf(szText, LPCSTR(szFormat), nCount);
	Static_SetText(GetDlgItem(m_hwndDlg, IDC_STATIC_SELECT_NO), 
				   szText);
}
