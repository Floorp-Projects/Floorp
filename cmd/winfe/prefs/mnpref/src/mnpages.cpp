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

#include "xp_help.h"
#include "pch.h"
#include <assert.h>
#include "dllcom.h"
#include "pages.h"
#include "resource.h"
#include "structs.h"
#include "xp_core.h"
#include "prefapi.h"
#include "colorbtn.h"
#include "bitmpbtn.h"
#include "mninterf.h"
#include "mnprefid.h"
#include "msgcom.h"

//#define MOZ_NO_LDAP    1
#undef MOZ_LDAP

#include "dirprefs.h"

extern "C" BOOL IsNumeric(char* pStr)
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

/////////////////////////////////////////////////////////////////////////////
// CMailNewsPrefs implementation

CMailNewsPrefs::CMailNewsPrefs()
	: CMailNewsPropertyPage(IDD_MAILNEWS, HELP_PREFS_MAILNEWS_MAIN_PANE)
{
	// Set member data using XP preferences
	m_rgbQuotedColor = RGB(0,0,0);
}

BOOL CMailNewsPrefs::InitDialog()
{
	char	szBuf[256];
	HWND hwndCombo = GetDlgItem(m_hwndDlg, IDC_FONT_STYLE);
	for (int i = IDS_PLAIN; i <= IDS_BOLD_ITALIC; i++) 
	{
		int	nLen = LoadString(m_hInstance, i, szBuf, sizeof(szBuf));
		
		assert(nLen > 0);
		ComboBox_AddString(hwndCombo, szBuf);
	}

	// Fill in the list of font sizes
	hwndCombo = GetDlgItem(m_hwndDlg, IDC_FONT_SIZE);
	for (i = IDS_FONT_SIZE_FIRST; i <= IDS_FONT_SIZE_LAST; i++) 
	{
		int	nLen = LoadString(m_hInstance, i, szBuf, sizeof(szBuf));
		
		assert(nLen > 0);
		ComboBox_AddString(hwndCombo, szBuf);
	}

	// Check for locked preferences
	CheckIfLockedPref("mail.quoted_style", IDC_FONT_STYLE);
	CheckIfLockedPref("mail.quoted_size", IDC_FONT_SIZE);
	CheckIfLockedPref("mail.citation_color", IDC_COLOR);

	if (MNPREF_PrefIsLocked("mail.fixed_width_messages")) 
	{
		EnableDlgItem(IDC_RADIO_FIXED, FALSE);
		EnableDlgItem(IDC_RADIO_VARIABLE, FALSE);
	}
									 
	CheckIfLockedPref("mail.play_sound", IDC_ENABLE_SOUND);
	CheckIfLockedPref("mailnews.confirm.moveFoldersToTrash", IDC_REMEMBER_LAST_MESSAGE);
	CheckIfLockedPref("mailnews.remember_selected_message", IDC_CONFIRM_ON_DELETE);
	CheckIfLockedPref("mail.use_mapi_server", IDC_USE_MAPI);

	return CMailNewsPropertyPage::InitDialog();
}

// Initialize member data using XP preferences
STDMETHODIMP CMailNewsPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		int32 n;

		PREF_GetIntPref("mail.quoted_style", &n);
		m_nFontStyle = (int)n;
		PREF_GetIntPref("mail.quoted_size", &n);
		m_nFontSize = (int)n;
		PREF_GetColorPrefDWord("mail.citation_color", &m_rgbQuotedColor);
		PREF_GetBoolPref("mail.fixed_width_messages", &m_bFixWidthFont);
		if (m_bFixWidthFont)
			m_nWidthRadio = 0;
		else
			m_nWidthRadio = 1;
		PREF_GetBoolPref("mail.play_sound", &m_bEnableSound);
		PREF_GetBoolPref("mailnews.remember_selected_message", &m_bRemeberLastMsg);
		PREF_GetBoolPref("mailnews.confirm.moveFoldersToTrash", &m_bConfirmOnDelete);
		PREF_GetBoolPref("mail.use_mapi_server", &m_bUseMapi);
	}

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CMailNewsPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	ComboBoxTransfer(IDC_FONT_STYLE , m_nFontStyle, bSaveAndValidate);
	ComboBoxTransfer(IDC_FONT_SIZE, m_nFontSize, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO_FIXED, m_nWidthRadio, bSaveAndValidate);
	CheckBoxTransfer(IDC_ENABLE_SOUND, m_bEnableSound, bSaveAndValidate);
	CheckBoxTransfer(IDC_REMEMBER_LAST_MESSAGE, m_bRemeberLastMsg, bSaveAndValidate);
	CheckBoxTransfer(IDC_CONFIRM_ON_DELETE, m_bConfirmOnDelete, bSaveAndValidate);
	CheckBoxTransfer(IDC_USE_MAPI, m_bUseMapi, bSaveAndValidate);
	return TRUE;
}

BOOL CMailNewsPrefs::ApplyChanges()
{
	// Apply changes using XP preferences
	PREF_SetIntPref("mail.quoted_style", (int32)m_nFontStyle);
	PREF_SetIntPref("mail.quoted_size", (int32)m_nFontSize);
	PREF_SetColorPrefDWord("mail.citation_color", m_rgbQuotedColor);
	if (m_nWidthRadio == 0)
		PREF_SetBoolPref("mail.fixed_width_messages", TRUE);
	else
		PREF_SetBoolPref("mail.fixed_width_messages", FALSE);
	PREF_SetBoolPref("mail.play_sound", m_bEnableSound);
	PREF_SetBoolPref("mailnews.remember_selected_message", m_bRemeberLastMsg);
	PREF_SetBoolPref("mailnews.confirm.moveFoldersToTrash", m_bConfirmOnDelete);
	PREF_SetBoolPref("mail.use_mapi_server", m_bUseMapi);
	return TRUE;
}

COLORREF CMailNewsPrefs::GetColorButtonColor(UINT nCtlID)
{
	if (nCtlID == IDC_COLOR)
		return m_rgbQuotedColor;

	assert(FALSE);
	return 0;
}

void CMailNewsPrefs::SetColorButtonColor(UINT nCtlID, COLORREF cr)
{
	if (nCtlID == IDC_COLOR)
		m_rgbQuotedColor = cr;
	else
		assert(FALSE);
}

LRESULT CMailNewsPrefs::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HBRUSH	hBrush;

	switch (uMsg) {
		case WM_DRAWITEM:
			hBrush = CreateSolidBrush(GetColorButtonColor(((LPDRAWITEMSTRUCT)lParam)->CtlID));
			DrawColorButtonControl((LPDRAWITEMSTRUCT)lParam, hBrush);
			DeleteBrush(hBrush);
			return TRUE;

		default:
			return CMailNewsPropertyPage::WindowProc(uMsg, wParam, lParam);
	}
}

BOOL CMailNewsPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	switch (id)
	{
		case IDC_COLOR:
			if (notifyCode == BN_CLICKED) 
			{
				CHOOSECOLOR	cc;
				COLORREF	custColors[16];

				// Initialize the structure
				memset(&cc, 0, sizeof(cc));
				cc.lStructSize = sizeof(cc);
				cc.rgbResult = GetColorButtonColor(id);
				cc.hwndOwner = GetParent(m_hwndDlg);
				cc.lpCustColors = custColors;
				cc.Flags = CC_RGBINIT;

				// Initialize the custom colors to white
				for (int i = 0; i < 16; i++)
					custColors[i] = RGB(255,255,255);

				if (ChooseColor(&cc) == IDOK) 
				{
					SetColorButtonColor(id, cc.rgbResult);
					InvalidateRect(hwndCtl, NULL, FALSE);
				}

				return TRUE;
			}
			break;
	}
	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CIdentityPrefs implementation

CIdentityPrefs::CIdentityPrefs()
#ifndef MOZ_MAIL_NEWS
	: CMailNewsPropertyPage(IDD_IDENTITY, HELP_PREFS_IDENTITY)
#else
	: CMailNewsPropertyPage(IDD_IDENTITY, HELP_PREFS_MAILNEWS_IDENTITY)
#endif /* MOZ_MAIL_NEWS */
{
	// Set member data using XP preferences
}

BOOL CIdentityPrefs::InitDialog()
{
	// Check for locked preferences
	CheckIfLockedPref("mail.identity.username", IDC_EDIT_NAME);
	CheckIfLockedPref("mail.identity.useremail", IDC_EDIT_EMAIL);
	CheckIfLockedPref("mail.identity.reply_to", IDC_EDIT_REPLYTO);
	CheckIfLockedPref("mail.identity.organization", IDC_EDIT_ORGANIZATION);
	if (CheckIfLockedPref("mail.signature_file", IDC_EDIT_SIGFILE))
		EnableDlgItem(IDC_BUTTON_BROWSE, FALSE);
	if (CheckIfLockedPref("mail.attach_vcard", IDC_ATTACH_VCARD))
		EnableDlgItem(IDC_BUTTON_EDITCARD, FALSE);

	return CMailNewsPropertyPage::InitDialog();
}

STDMETHODIMP CIdentityPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		PREF_GetStringPref("mail.identity.username", m_strName);
		PREF_GetStringPref("mail.identity.useremail", m_strEmail);
		PREF_GetStringPref("mail.identity.reply_to", m_strReply);
		PREF_GetStringPref("mail.identity.organization", m_strOrganization);
		PREF_GetStringPref("mail.signature_file", m_strSigFile);
		PREF_GetBoolPref("mail.attach_vcard", &m_bAttchVCard);
	} 

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CIdentityPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	EditFieldTransfer(IDC_EDIT_NAME, m_strName, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_EMAIL, m_strEmail, bSaveAndValidate);
#ifdef MOZ_MAIL_NEWS
	EditFieldTransfer(IDC_EDIT_REPLYTO, m_strReply, bSaveAndValidate);
#endif   
	EditFieldTransfer(IDC_EDIT_ORGANIZATION, m_strOrganization, bSaveAndValidate);
#ifdef MOZ_MAIL_NEWS   
	EditFieldTransfer(IDC_EDIT_SIGFILE, m_strSigFile, bSaveAndValidate);
	CheckBoxTransfer(IDC_ATTACH_VCARD, m_bAttchVCard, bSaveAndValidate);
#endif
	return TRUE;
}

BOOL CIdentityPrefs::ApplyChanges()
{
	PREF_SetCharPref("mail.identity.username", m_strName);
	PREF_SetCharPref("mail.identity.useremail", m_strEmail);
	PREF_SetCharPref("mail.identity.reply_to", m_strReply);
	PREF_SetCharPref("mail.identity.organization", m_strOrganization);
	if (m_strSigFile.IsEmpty())
		PREF_SetBoolPref("mail.use_signature_file", FALSE);
	else
		PREF_SetBoolPref("mail.use_signature_file", TRUE);
	PREF_SetCharPref("mail.signature_file", m_strSigFile);
	PREF_SetBoolPref("mail.attach_vcard",m_bAttchVCard);
	return TRUE;
}

BOOL CIdentityPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	switch (id)
	{
		case IDC_BUTTON_BROWSE:
			if (notifyCode == BN_CLICKED) 
			{
				assert(m_pObject);
				if (m_pObject) 
				{
					LPMAILNEWSINTERFACE	lpInterface;

					// Request the IMailNewsInterface interface from our data object
					if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
															(void **)&lpInterface))) 
					{
						LPOLESTR	lpoleSigFile = NULL;
						 
						lpInterface->GetSigFile(GetParent(m_hwndDlg), &lpoleSigFile);
						if (lpoleSigFile) 
						{
							m_strSigFile = lpoleSigFile;
							CoTaskMemFree(lpoleSigFile);
							Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT_SIGFILE), 
								         (LPCSTR)m_strSigFile);
						}
						lpInterface->Release();
					}
				}
			}
			break;

		case IDC_ATTACH_VCARD:
			if (notifyCode == BN_CLICKED) 
			{
				if (IsDlgButtonChecked(m_hwndDlg, IDC_ATTACH_VCARD))
				{
					assert(m_pObject);
					if (m_pObject) 
					{
						LPMAILNEWSINTERFACE	lpInterface;

						// Request the IMailNewsInterface interface from our data object
						if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
																(void **)&lpInterface))) 
						{
							if (lpInterface->CreateAddressBookCard(GetParent(m_hwndDlg)))
							{
								m_bAttchVCard = TRUE;
							}
							else
							{
								m_bAttchVCard = FALSE;
								CheckDlgButton(m_hwndDlg, IDC_ATTACH_VCARD, FALSE);
							}
							// Get the number of encodings
							lpInterface->Release();
						}
					}

				}
				return TRUE;
			}
			break;
		case IDC_BUTTON_EDITCARD:
			if (notifyCode == BN_CLICKED) 
			{
				assert(m_pObject);
				if (m_pObject) 
				{
					LPMAILNEWSINTERFACE	lpInterface;

					// Request the IMailNewsInterface interface from our data object
					if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
						                                    (void **)&lpInterface))) 
					{
						// Get the number of encodings
						lpInterface->EditAddressBookCard(GetParent(m_hwndDlg));
						lpInterface->Release();
					}
				}
			}
			return TRUE;
	}
	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CMessagesPrefs implementation

CMessagesPrefs::CMessagesPrefs()
	: CMailNewsPropertyPage(IDD_MESSAGES, HELP_PREFS_MAILNEWS_MESSAGES)
{											  
}

BOOL CMessagesPrefs::InitDialog()
{
	if(MNPREF_PrefIsLocked("mail.forward_message_mode"))
	{
		EnableDlgItem(IDC_COMBO_FORWARD, FALSE);
	}

	char	szBuf[256];

	HWND hwndCombo = GetDlgItem(m_hwndDlg, IDC_COMBO_FORWARD);

	for(int i = IDS_FORWARDINLINE; i <= IDS_FORWARDATTACHMENT; i++)
	{
		int	nLen = LoadString(m_hInstance, i, szBuf, sizeof(szBuf));
		assert(nLen > 0);
		ComboBox_AddString(hwndCombo, szBuf);
	}


	CheckIfLockedPref("mail.auto_quote", IDC_CHECK_AUTO_QUOTE);


	if (MNPREF_PrefIsLocked("mailnews.reply_on_top")) 
	{
		EnableDlgItem(IDC_COMBO_REPLY, FALSE);
	}

	hwndCombo = GetDlgItem(m_hwndDlg, IDC_COMBO_REPLY);

	for(int j = IDS_REPLYABOVE; j <= IDS_REPLYSELECT; j++)
	{
		int	nLen = LoadString(m_hInstance, j, szBuf, sizeof(szBuf));
		assert(nLen > 0);
		ComboBox_AddString(hwndCombo, szBuf);
	}

	CheckIfLockedPref("mail.wrap_long_lines", IDC_WRAP_INCOMING);

	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_WRAP_LEN), 5);
	CheckIfLockedPref("mailnews.wraplength", IDC_EDIT_WRAP_LEN);

	if (MNPREF_PrefIsLocked("mail.strictly_mime")) 
	{
		EnableDlgItem(IDC_RADIO_8BITS, FALSE);
		EnableDlgItem(IDC_RADIO_MIME, FALSE);
	}

	return CMailNewsPropertyPage::InitDialog();
}

STDMETHODIMP
CMessagesPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		int32 n;

		PREF_GetIntPref("mail.forward_message_mode", &n);

		if(n == 0)
			m_nForwardChoice = 2;
		else if(n==1)
			m_nForwardChoice = 1;
		else
			m_nForwardChoice = 0;

		PREF_GetBoolPref("mail.auto_quote", &m_bAutoQuote);

		PREF_GetIntPref("mailnews.reply_on_top", &n);
		if (n == 0)
			m_nRadioQuoteType = 1;
		else if (n == 1)
			m_nRadioQuoteType = 0;
		else
			m_nRadioQuoteType = 2;

		PREF_GetBoolPref("mail.wrap_long_lines", &m_bWarpIncoming);

		PREF_GetIntPref("mailnews.wraplength", &n);
		if (n == 0)
			PREF_GetDefaultIntPref("mailnews.wraplength", &n);
		else if (n < 10)
			n = 10;
		m_nWrapLen = (int)n;

		BOOL bPref;
		PREF_GetBoolPref("mail.strictly_mime", &bPref);
		if (bPref)
			m_nRadio8bits = 1;
		else
			m_nRadio8bits = 0;

	}

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CMessagesPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	ComboBoxTransfer(IDC_COMBO_FORWARD, m_nForwardChoice, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_AUTO_QUOTE, m_bAutoQuote, bSaveAndValidate);
	ComboBoxTransfer(IDC_COMBO_REPLY, m_nRadioQuoteType, bSaveAndValidate);
	CheckBoxTransfer(IDC_WRAP_INCOMING, m_bWarpIncoming, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_WRAP_LEN, m_nWrapLen, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO_8BITS, m_nRadio8bits, bSaveAndValidate);

	return TRUE;
}

BOOL CMessagesPrefs::ApplyChanges()
{
	if(m_nForwardChoice == 0)
		PREF_SetIntPref("mail.forward_message_mode", (int32)2);
	else if(m_nForwardChoice == 1)
		PREF_SetIntPref("mail.forward_message_mode", (int32)1);
	else
		PREF_SetIntPref("mail.forward_message_mode", (int32)0);

	PREF_SetBoolPref("mail.auto_quote", m_bAutoQuote);
	if (m_nRadioQuoteType == 0)
		PREF_SetIntPref("mailnews.reply_on_top", (int32)1);
	else if (m_nRadioQuoteType == 1)
		PREF_SetIntPref("mailnews.reply_on_top", (int32)0);
	else  
		PREF_SetIntPref("mailnews.reply_on_top", (int32)2);

	PREF_SetBoolPref("mail.wrap_long_lines", m_bWarpIncoming);

	PREF_SetIntPref("mailnews.wraplength", (int32)m_nWrapLen);
	if (m_nRadio8bits == 0)
		PREF_SetBoolPref("mail.strictly_mime", FALSE);
	else
		PREF_SetBoolPref("mail.strictly_mime", TRUE);

	return TRUE;
}

BOOL CMessagesPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_EDIT_WRAP_LEN && notifyCode == EN_UPDATE)
	{
		ValidateNumeric(m_hwndDlg, m_hInstance, IDS_MESSAGES, 
						id, m_nWrapLen);
		ValidateNonZero();
		return TRUE;
	}
	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

void CMessagesPrefs::ValidateNonZero()
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, IDC_EDIT_WRAP_LEN);
	char	szBuf[32];
	DWORD	dwStart, dwEnd;

	assert(hwndCtl);
	Edit_GetText(hwndCtl, szBuf, sizeof(szBuf));
	SendMessage(hwndCtl, EM_GETSEL, (WPARAM)(LPDWORD)&dwStart, 
			    (LPARAM)(LPDWORD)&dwEnd);

	if (szBuf[0] == '\0' || szBuf[0] == '0')
	{
		CString	strText, strCaption;

		strText.LoadString(m_hInstance, IDS_NON_ZERO);
		strCaption.LoadString(m_hInstance, IDS_MESSAGES);

		MessageBeep(MB_ICONHAND);
		MessageBox(GetParent(m_hwndDlg), (LPCSTR)strText, (LPCSTR)strCaption, 
			       MB_OK | MB_ICONSTOP);
		if (m_nWrapLen == 0)
		{
			int32 n;
			PREF_GetDefaultIntPref("mailnews.wraplength", &n);
			m_nWrapLen = (int)n;
		}
		wsprintf(szBuf, "%i", m_nWrapLen);
		Edit_SetText(hwndCtl, szBuf);
		Edit_SetSel(hwndCtl, dwStart - 1, dwEnd - 1);
	}
	else
	{
		m_nWrapLen = atoi(szBuf);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMailServerPrefs implementation

CMailServerPrefs::CMailServerPrefs()
	: CMailNewsPropertyPage(IDD_MAILSERVER, HELP_PREFS_MAILNEWS_MAILSERVER)
{
	m_nServerType = -1;
	m_bSetDefault = FALSE;
}

STDMETHODIMP CMailServerPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		int32 n;

		GetMailServerType();

		PREF_GetIntPref("mail.smtp.ssl", &n);
		m_nUseSSL = (int)n;

		GetPopServerName();

		PREF_GetStringPref("mail.pop_name", m_strUserName);
		PREF_GetStringPref("network.hosts.smtp_server", m_strOutServer);
		PREF_GetStringPref("mail.directory", m_strMailDir);
	}

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CMailServerPrefs::InitDialog()
{
	HWND hwndList = GetDlgItem(m_hwndDlg, IDC_SERVERS_LIST);
	if (m_nServerType == TYPE_POP)
	{	 
		ListBox_AddString(hwndList, (LPCSTR)m_strPopServer);   
	}
	else if (m_nServerType == TYPE_IMAP)
	{ 
		GetIMAPServers(hwndList);
	}
	if (ListBox_GetCount(hwndList))
	{
		ListBox_SetCurSel(hwndList, 0);
		EnableDlgItem(IDC_DEFAULT, FALSE);
	}

	if (CheckIfLockedPref("network.hosts.pop_server", IDC_SERVERS_LIST))
	{
		EnableDlgItem(IDC_NEW, FALSE);
		EnableDlgItem(IDC_EDIT, FALSE);
		EnableDlgItem(IDC_DELETE, FALSE);
	}
	CheckIfLockedPref("mail.pop_name", IDC_EDIT_NAME);
	CheckIfLockedPref("network.hosts.smtp_server", IDC_EDIT_SMTP_SERVER);
	if (MNPREF_PrefIsLocked("mail.smtp.ssl")) 
	{
		EnableDlgItem(IDC_RADIO_NEVER, FALSE);
		EnableDlgItem(IDC_RADIO_POSSIBLE, FALSE);
		EnableDlgItem(IDC_RADIO_ALWAYS, FALSE);
	}

	if (CheckIfLockedPref("mail.directory", IDC_EDIT_MAIL_DIR))
		EnableDlgItem(IDC_BUTTON_BROWSE, FALSE);

	return CMailNewsPropertyPage::InitDialog();
}

BOOL CMailServerPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	if (m_nServerType == TYPE_POP)
		ListBoxTransfer(IDC_SERVERS_LIST, m_strPopServer, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_NAME, m_strUserName, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_SMTP_SERVER, m_strOutServer, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO_NEVER, m_nUseSSL, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_MAIL_DIR, m_strMailDir, bSaveAndValidate);
	return TRUE;
}

BOOL CMailServerPrefs::ApplyChanges()
{
	PREF_SetIntPref("mail.server_type", (int32)m_nServerType);
	PREF_SetCharPref("mail.pop_name", m_strUserName);
	PREF_SetCharPref("network.hosts.smtp_server", m_strOutServer);
	PREF_SetIntPref("mail.smtp.ssl", (int32)m_nUseSSL);
	PREF_SetCharPref("mail.directory", m_strMailDir);

	if (!m_bSetDefault)
		return TRUE;
	assert(m_pObject);
	if (m_pObject) 
	{
		LPMAILNEWSINTERFACE	lpInterface;

		// Request the IMailNewsInterface interface from our data object
		if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
												(void **)&lpInterface))) 
		{
			HWND hwndList = GetDlgItem(m_hwndDlg, IDC_SERVERS_LIST);
			if (m_nServerType == TYPE_IMAP)
				lpInterface->SetImapDefaultServer(hwndList);
			lpInterface->Release();
		}
	}
	return TRUE;
}

void CMailServerPrefs::GetPopServerName()
{
	PREF_GetStringPref("network.hosts.pop_server", m_strPopServer);
}

void CMailServerPrefs::GetMailServerType()
{
	int32 n;

	PREF_GetIntPref("mail.server_type", &n);
	m_nServerType = (int)n;
}

void CMailServerPrefs::CheckButtons(HWND hwndList)
{
	int nIndex = ListBox_GetCurSel(hwndList);
	if (ListBox_GetCount(hwndList) && nIndex >= 0)
	{
		EnableDlgItem(IDC_EDIT, TRUE);
		EnableDlgItem(IDC_DELETE, TRUE);
		if (nIndex == 0)
			EnableDlgItem(IDC_DEFAULT, FALSE);
		else
			EnableDlgItem(IDC_DEFAULT, TRUE);
	}
	else
	{
		EnableDlgItem(IDC_EDIT, FALSE);
		EnableDlgItem(IDC_DELETE, FALSE);
		EnableDlgItem(IDC_DEFAULT, FALSE);
		SetFocus(GetDlgItem(m_hwndDlg, IDC_NEW));
	}
}

void CMailServerPrefs::DoDefaultServer(HWND hwndList)
{
	if (m_nServerType == TYPE_IMAP)
	{
		CString	strNewServer, strDefault, strOldServer, strServer;
		int	nListCount = ListBox_GetCount(hwndList);
		int	nCurSel = ListBox_GetCurSel(hwndList);
		int	nLen = ListBox_GetTextLen(hwndList, nCurSel);

		ListBox_GetText(hwndList, nCurSel, strNewServer.BufferSetLength(nLen));
		strDefault.LoadString(m_hInstance, IDS_DEFAULT);
		strNewServer += strDefault;

		nLen = ListBox_GetTextLen(hwndList, 0);
		ListBox_GetText(hwndList, 0, strOldServer.BufferSetLength(nLen));
		strServer = strOldServer.Left(strOldServer.GetLength() - strDefault.GetLength());
		ListBox_DeleteString(hwndList, 0);
		ListBox_InsertString(hwndList, 0, (LPCSTR)strServer);
		ListBox_DeleteString(hwndList, nCurSel);
		ListBox_InsertString(hwndList, 0, (LPCSTR)strNewServer);
		ListBox_SetCurSel(hwndList, 0);
	}
}

void CMailServerPrefs::GetIMAPServers(HWND hwndList)
{
	char serverStr[512];
	int nLen = 511;

	if (PREF_NOERROR == PREF_GetCharPref("network.hosts.imap_servers", serverStr, &nLen))
	{
		int	nListCount = ListBox_GetCount(hwndList);
		int	nOldSel = ListBox_GetCurSel(hwndList);
		if (nListCount)
			ListBox_ResetContent(hwndList);

		char name[128];
		char* pTemp = serverStr;
		nLen = strlen(serverStr);
		if (nLen)
		{
			int j = 0;
			BOOL nFirst = TRUE;
			for (int i = 0; i <= nLen; i++)
			{
				if (*pTemp != ',' && *pTemp != '\0')
					name[j++] = *pTemp;
				else
				{	
					name[j] = '\0';
					CString	strText = name;
					if (nFirst)
					{
						nFirst = FALSE;

						CString	strDefault;
						strDefault.LoadString(m_hInstance, IDS_DEFAULT);
						strText += strDefault;
					}
					ListBox_AddString(hwndList, (LPCSTR)strText); 
					j = 0;
				}
				pTemp++;
			}
			int nTotal = ListBox_GetCount(hwndList);
			if (nOldSel < nTotal)
				nOldSel = ListBox_SetCurSel(hwndList, nOldSel);
			else if (nTotal)
				ListBox_SetCurSel(hwndList, 0);
		}
	}
}

BOOL CMailServerPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{  	
	if (id == IDC_SERVERS_LIST && notifyCode == LBN_SELCHANGE)
	{
		CheckButtons(GetDlgItem(m_hwndDlg, IDC_SERVERS_LIST));
		return TRUE;
	}
	if (notifyCode == BN_CLICKED && 
		(id == IDC_NEW || id == IDC_EDIT ||
		 id == IDC_DELETE || id == IDC_BUTTON_BROWSE
		 || id == IDC_DEFAULT))
	{
		assert(m_pObject);
		if (m_pObject) 
		{
			LPMAILNEWSINTERFACE	lpInterface;

			// Request the IMailNewsInterface interface from our data object
			if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
													(void **)&lpInterface))) 
			{
				HWND hwndList = GetDlgItem(m_hwndDlg, IDC_SERVERS_LIST);
				BOOL bAllowBothTypes = FALSE;

				switch (id)
				{  
					case IDC_NEW:
						if (ListBox_GetCount(hwndList) == 0)
							bAllowBothTypes = TRUE;
						if (lpInterface->AddMailServer(GetParent(m_hwndDlg), 
										hwndList, bAllowBothTypes, (DWORD)m_nServerType))
						{
							GetMailServerType();
							if (m_nServerType == TYPE_IMAP)
							{
								GetIMAPServers(hwndList);
							}
							else
								GetPopServerName();	
						}
						break;
					case IDC_EDIT:
						if (ListBox_GetCount(hwndList) <= 1)
							bAllowBothTypes = TRUE;
						if (lpInterface->EditMailServer(GetParent(m_hwndDlg), 
									hwndList, bAllowBothTypes, (DWORD)m_nServerType))
						{
							GetMailServerType();
							if (m_nServerType == TYPE_IMAP)
							{
								GetIMAPServers(hwndList);
							}
							else
								GetPopServerName();	
						}
						break;
					case IDC_DELETE:
						if (lpInterface->DeleteMailServer(GetParent(m_hwndDlg), hwndList,
													(DWORD)m_nServerType))
						{
							if (m_nServerType == TYPE_IMAP)
							{
								GetIMAPServers(hwndList);
							}
							else
							{	//only support one pop server now
								m_strPopServer = "";
							}
						}
						break;
					case IDC_BUTTON_BROWSE:
						{
							LPOLESTR	lpoleDirFile = NULL;
							 
							lpInterface->ShowDirectoryPicker(GetParent(m_hwndDlg), 
								(LPOLESTR)LPCSTR(m_strMailDir), &lpoleDirFile);
							if (lpoleDirFile) 
							{
								m_strMailDir = lpoleDirFile;
								CoTaskMemFree(lpoleDirFile);
								Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT_MAIL_DIR), 
											 (LPCSTR)m_strMailDir);
							}
						}
						break;
					case IDC_DEFAULT:
						m_bSetDefault = TRUE;
						DoDefaultServer(hwndList);
						break;
				}
				lpInterface->Release();
				CheckButtons(hwndList);
				return TRUE;
			}
		}
	}

	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CNewsServerPrefs implementation

CNewsServerPrefs::CNewsServerPrefs()
	: CMailNewsPropertyPage(IDD_NEWSSERVER, HELP_PREFS_MAILNEWS_GROUPSERVER)
{
	// Set member data using XP preferences
}

STDMETHODIMP CNewsServerPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) 
	{
		int32 n;
 
		PREF_GetStringPref("network.hosts.nntp_server", m_strNewsServer);
		PREF_GetStringPref("news.directory", m_strNewsDir);
		PREF_GetBoolPref("news.notify.on", &m_bNotify);
		PREF_GetBoolPref("news.server_is_secure", &m_bSecure);
		PREF_GetIntPref("news.max_articles", &n);
		m_nMaxMsgs = (int)n;
		PREF_GetIntPref("news.server_port", &n);
		m_nPort = (int)n;
	}

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CNewsServerPrefs::InitDialog()
{
	// Limit the length of the string the user can type
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_MSG_COUNT), 5);

	HWND hwndList = GetDlgItem(m_hwndDlg, IDC_SERVERS_LIST);
	if (0 == ListBox_GetCount(hwndList))
		EnableDlgItem(IDC_DEFAULT, FALSE);

	if (CheckIfLockedPref("network.hosts.nntp_server", IDC_SERVERS_LIST))
	{
		EnableDlgItem(IDC_NEW, FALSE);
		EnableDlgItem(IDC_EDIT, FALSE);
		EnableDlgItem(IDC_DELETE, FALSE);
	}
	if (CheckIfLockedPref("news.directory", IDC_EDIT_NEWS_DIR))
		EnableDlgItem(IDC_BUTTON_BROWSE, FALSE);
	CheckIfLockedPref("news.notify.on", IDC_CHECK_NOTIFY);
	CheckIfLockedPref("news.max_articles", IDC_EDIT_MSG_COUNT);
	
	assert(m_pObject);
	if (m_pObject) 
	{
		LPMAILNEWSINTERFACE	lpInterface;

		// Request the IMailNewsInterface interface from our data object
		if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
												(void **)&lpInterface))) 
		{
			CString	strNewServer, strDefault;

			HWND hwndList = GetDlgItem(m_hwndDlg, IDC_SERVERS_LIST);
			lpInterface->FillNewsServerList(hwndList, (LPOLESTR)LPCSTR(m_strNewsServer),
				(LPVOID FAR*)&m_pDefaultServer);
			lpInterface->Release();

			int nServerIndex = GetDefaultServerIndex(hwndList);
			if (nServerIndex != -1)
			{
				strNewServer = m_strNewsServer;
				strDefault.LoadString(m_hInstance, IDS_DEFAULT);
				strNewServer += strDefault;

				SetServerName(hwndList, nServerIndex, strNewServer);
				ListBox_SetCurSel(hwndList, nServerIndex);
			}
		}
	}   
	return CMailNewsPropertyPage::InitDialog();
}

BOOL CNewsServerPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	EditFieldTransfer(IDC_EDIT_NEWS_DIR, m_strNewsDir, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_NOTIFY, m_bNotify, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_MSG_COUNT, m_nMaxMsgs, bSaveAndValidate);
	return TRUE;
}

BOOL CNewsServerPrefs::ApplyChanges()
{
    int32 tempInt;
	PREF_SetCharPref("network.hosts.nntp_server", m_strNewsServer);
	PREF_SetCharPref("news.directory", m_strNewsDir);
	PREF_SetBoolPref("news.notify.on", m_bNotify);
	PREF_SetIntPref("news.max_articles", (int32)m_nMaxMsgs);
	PREF_GetIntPref("news.server_change_xaction", &tempInt);
	PREF_SetIntPref("news.server_change_xaction", ++tempInt);
	return TRUE;
}

void CNewsServerPrefs::CheckButtons(HWND hwndList)
{
	int nIndex = ListBox_GetCurSel(hwndList);
	if (ListBox_GetCount(hwndList) && nIndex >= 0)
	{
		EnableDlgItem(IDC_EDIT, TRUE);
		EnableDlgItem(IDC_DELETE, TRUE);
		int	nCurSel = ListBox_GetCurSel(hwndList);
		if (nCurSel != GetDefaultServerIndex(hwndList))
			EnableDlgItem(IDC_DEFAULT, TRUE);
		else
			EnableDlgItem(IDC_DEFAULT, FALSE);
	}
	else
	{
		EnableDlgItem(IDC_EDIT, FALSE);
		EnableDlgItem(IDC_DELETE, FALSE);
		EnableDlgItem(IDC_DEFAULT, FALSE);
		SetFocus(GetDlgItem(m_hwndDlg, IDC_NEW));
	}
}

void CNewsServerPrefs::SetServerName(HWND hwndList, int nIndex, CString& name)
{
	LPVOID pServer =  (LPVOID)ListBox_GetItemData(hwndList, nIndex);
	ListBox_DeleteString(hwndList, nIndex);
	ListBox_InsertString(hwndList, nIndex, (LPCSTR)name);
	ListBox_SetItemData(hwndList, nIndex, pServer);
}

int CNewsServerPrefs::GetDefaultServerIndex(HWND hwndList)
{
	LPVOID pServer = NULL;
	int	nListCount = ListBox_GetCount(hwndList);
	for (int i = 0; i < nListCount; i++)
	{
		pServer =  (LPVOID)ListBox_GetItemData(hwndList, i);
		if (m_pDefaultServer == pServer)
		{
			return i;
		}
	} 
	return -1;
}

void CNewsServerPrefs::DoDefaultServer(HWND hwndList)
{
	CString	strNewServer, strDefault, strOldServer, strServer;
	int	nListCount = ListBox_GetCount(hwndList);
	int	nCurSel = ListBox_GetCurSel(hwndList);
	int	nLen = ListBox_GetTextLen(hwndList, nCurSel);

	ListBox_GetText(hwndList, nCurSel, strNewServer.BufferSetLength(nLen));
	m_strNewsServer = strNewServer;
	strDefault.LoadString(m_hInstance, IDS_DEFAULT);
	strNewServer += strDefault;

	SetServerName(hwndList, nCurSel, strNewServer);

	int nServerIndex = GetDefaultServerIndex(hwndList);
	if (nServerIndex >= 0)
	{
		nLen = ListBox_GetTextLen(hwndList, nServerIndex);
		ListBox_GetText(hwndList, nServerIndex, strOldServer.BufferSetLength(nLen));
		strServer = strOldServer.Left(strOldServer.GetLength() - strDefault.GetLength());
		SetServerName(hwndList, nServerIndex, strServer);
	}

	m_pDefaultServer = (LPVOID)ListBox_GetItemData(hwndList, nCurSel);
	ListBox_SetCurSel(hwndList, nCurSel);
}

BOOL CNewsServerPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_EDIT_MSG_COUNT && notifyCode == EN_UPDATE)
	{
		ValidateNumeric(m_hwndDlg, m_hInstance, IDS_GROUPS_SERVER, 
						id, m_nMaxMsgs);
		return TRUE;
	}
	if (id == IDC_SERVERS_LIST && notifyCode == LBN_SELCHANGE)
	{
		CheckButtons(GetDlgItem(m_hwndDlg, IDC_SERVERS_LIST));
		return TRUE;
	}
	if (notifyCode == BN_CLICKED && 
		(id == IDC_NEW || id == IDC_EDIT ||	id == IDC_DELETE ||
		 id == IDC_DEFAULT || id == IDC_BUTTON_BROWSE))
	{
		assert(m_pObject);
		if (m_pObject) 
		{
			LPMAILNEWSINTERFACE	lpInterface;

			// Request the IMailNewsInterface interface from our data object
			if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
													(void **)&lpInterface))) 
			{
				HWND hwndList = GetDlgItem(m_hwndDlg, IDC_SERVERS_LIST);
				BOOL bChanged = FALSE;
				int nIndex = -1;
				LPVOID lpOldDefault = m_pDefaultServer;

				switch (id)
				{  
					case IDC_NEW:
						lpInterface->AddNewsServer(GetParent(m_hwndDlg), hwndList);
						break;
					case IDC_EDIT:
						lpInterface->EditNewsServer(GetParent(m_hwndDlg), hwndList, &bChanged);
						break;
					case IDC_DELETE:
						lpInterface->DeleteNewsServer(GetParent(m_hwndDlg), hwndList, &m_pDefaultServer);
						if (m_pDefaultServer != lpOldDefault)
						{
							CString	strNewServer, strDefault;
							int	nIndex = GetDefaultServerIndex(hwndList);
							int	nLen = ListBox_GetTextLen(hwndList, nIndex);

							ListBox_GetText(hwndList, nIndex, strNewServer.BufferSetLength(nLen));
							m_strNewsServer = strNewServer;
							strDefault.LoadString(m_hInstance, IDS_DEFAULT);
							strNewServer += strDefault;

							SetServerName(hwndList, nIndex, strNewServer);
							ListBox_SetCurSel(hwndList, nIndex);
						}
						break;
					case IDC_DEFAULT:
						DoDefaultServer(hwndList);
						break;
					case IDC_BUTTON_BROWSE:
						LPOLESTR	lpoleDirFile = NULL;
						 
						lpInterface->ShowDirectoryPicker(GetParent(m_hwndDlg), 
							(LPOLESTR)LPCSTR(m_strNewsDir), &lpoleDirFile);
						if (lpoleDirFile) 
						{
							m_strNewsDir = lpoleDirFile;
							CoTaskMemFree(lpoleDirFile);
							Edit_SetText(GetDlgItem(m_hwndDlg, IDC_EDIT_NEWS_DIR), 
										 (LPCSTR)m_strNewsDir);
						}
						break;
				}
				lpInterface->Release();
				CheckButtons(hwndList);

				return TRUE;
			}
		}
	}
	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CCustomizedDialog implementation

class CCustomizedDialog : public CDialog {
	public:
		CCustomizedDialog();

	protected:
		BOOL	InitDialog();
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		
		// Event processing
		void	OnOK();

	private:

		int		m_nCheckGeneral;
		int		m_nCheckOtherDomain;
		int		m_nCheckNotOnList;
};

CCustomizedDialog::CCustomizedDialog()
	: CDialog(CComDll::m_hInstance, IDD_RECEIPT_CUSTOMIZE)
{
	m_nCheckGeneral = 0;
	m_nCheckOtherDomain = 0;
	m_nCheckNotOnList = 0;

	int32 n = 0;
	PREF_GetIntPref("mail.mdn.report.other", &n);
	m_nCheckGeneral = (int)n;
	PREF_GetIntPref("mail.mdn.report.outside_domain", &n);
	m_nCheckOtherDomain = (int)n;
	PREF_GetIntPref("mail.mdn.report.not_in_to_cc", &n);
	m_nCheckNotOnList = (int)n;

}

BOOL CCustomizedDialog::InitDialog()
{
	char	szBuf[256];	   
	//make sure the control IDs and string IDs are continuous number
	for (int j = IDC_NOT_TOCC; j <= IDC_GENERAL; j++) 
	{
		HWND hwndCombo = GetDlgItem(m_hwndDlg, j);
		for (int i = IDS_NEVER; i <= IDS_ASK_ME; i++) 
		{
			int	nLen = LoadString(m_hInstance, i, szBuf, sizeof(szBuf));
			
			assert(nLen > 0);
			ComboBox_AddString(hwndCombo, szBuf);
		}
	}

	if (MNPREF_PrefIsLocked("mail.mdn.report.other")) 
	{
		EnableDlgItem(IDC_STATIC1, FALSE);
		EnableDlgItem(IDC_GENERAL, FALSE);
	}
	if (MNPREF_PrefIsLocked("mail.mdn.report.outside_domain")) 
	{
		EnableDlgItem(IDC_STATIC2, FALSE);
		EnableDlgItem(IDC_OUTSIDE_DOMAIN, FALSE);
	}
	if (MNPREF_PrefIsLocked("mail.mdn.report.not_in_to_cc")) 
	{
		EnableDlgItem(IDC_STATIC3, FALSE);
		EnableDlgItem(IDC_NOT_TOCC, FALSE);
	}

	return CDialog::InitDialog();
}

BOOL CCustomizedDialog::DoTransfer(BOOL bSaveAndValidate)
{
	ComboBoxTransfer(IDC_GENERAL , m_nCheckGeneral, bSaveAndValidate);
	ComboBoxTransfer(IDC_OUTSIDE_DOMAIN , m_nCheckOtherDomain, bSaveAndValidate);
	ComboBoxTransfer(IDC_NOT_TOCC , m_nCheckNotOnList, bSaveAndValidate);
	return TRUE;
}

void CCustomizedDialog::OnOK()
{
	// Transfer data from the dialog and then destroy the dialog window
	CDialog::OnOK();

	// Apply changes using XP preferences

	PREF_SetIntPref("mail.mdn.report.other", (int32)m_nCheckGeneral);
	PREF_SetIntPref("mail.mdn.report.outside_domain", (int32)m_nCheckOtherDomain);
	PREF_SetIntPref("mail.mdn.report.not_in_to_cc", (int32)m_nCheckNotOnList);
}

/////////////////////////////////////////////////////////////////////////////
// CMailReceiptPrefs implementation

CMailReceiptPrefs::CMailReceiptPrefs()
	: CMailNewsPropertyPage(IDD_READRECEIPT, HELP_PREFS_MAILNEWS_RECEIPTS)
{
	m_nRequestReceipt = 0;
	m_nReceiptArrive = 0;
	m_nEnableRespond = 0;
}

BOOL CMailReceiptPrefs::InitDialog()
{
	if (MNPREF_PrefIsLocked("mail.request.return_receipt")) 
	{
		EnableDlgItem(IDC_RADIO_SERVER_ONLY, FALSE);
		EnableDlgItem(IDC_RADIO_CLIENT_ONLY, FALSE);
		EnableDlgItem(IDC_RADIO_BOTH, FALSE);
	}
	if (MNPREF_PrefIsLocked("mail.incorporate.return_receipt")) 
	{
		EnableDlgItem(IDC_RADIO_IN_INBOX, FALSE);
		EnableDlgItem(IDC_RADIO_TO_SENT, FALSE);
	}
	if (MNPREF_PrefIsLocked("mail.mdn.report.enabled")) 
	{
		EnableDlgItem(IDC_RADIO_NO_RECEIPT, FALSE);
		EnableDlgItem(IDC_RADIO_CUSTOMIZE_RECEIPT, FALSE);
	}

	return CMailNewsPropertyPage::InitDialog();
}

STDMETHODIMP CMailReceiptPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) 
	{
		int32 n = 0;

		PREF_GetIntPref("mail.request.return_receipt", &n);
		m_nRequestReceipt = (int)(n-1);
		PREF_GetIntPref("mail.incorporate.return_receipt", &n);
		m_nReceiptArrive = (int)n;

		BOOL bPref = FALSE;
		PREF_GetBoolPref("mail.mdn.report.enabled", &bPref);
		if (bPref)
			m_nEnableRespond = 1;
		else
			m_nEnableRespond = 0;
	}
	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CMailReceiptPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	RadioButtonTransfer(IDC_RADIO_SERVER_ONLY, m_nRequestReceipt, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO_IN_INBOX, m_nReceiptArrive, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO_NO_RECEIPT, m_nEnableRespond, bSaveAndValidate);
	return TRUE;
}

BOOL CMailReceiptPrefs::ApplyChanges()
{
	// Apply changes using XP preferences
	PREF_SetIntPref("mail.request.return_receipt", (int32)(m_nRequestReceipt+1));
	PREF_SetIntPref("mail.incorporate.return_receipt", (int32)m_nReceiptArrive);
	if (m_nEnableRespond == 0)
		PREF_SetBoolPref("mail.mdn.report.enabled", FALSE);
	else
		PREF_SetBoolPref("mail.mdn.report.enabled", TRUE);
	return TRUE;
}

BOOL CMailReceiptPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_CUSTOMIZE && notifyCode == BN_CLICKED) 
	{
		CCustomizedDialog customDialog;
		customDialog.DoModal(GetParent(m_hwndDlg));
		return TRUE;
	}

	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CHTMLFormatPrefs implementation

CHTMLFormatPrefs::CHTMLFormatPrefs()
	: CMailNewsPropertyPage(IDD_HTML_FORMATTING, HELP_PREFS_MAILNEWS_FORMATTING)
{
}

BOOL CHTMLFormatPrefs::InitDialog()
{
	CheckIfLockedPref("mail.html_compose", IDC_HTMLCOMPOSE);
	CheckIfLockedPref("mail.html_compose", IDC_PLAINCOMPOSE);

	if (MNPREF_PrefIsLocked("mail.default_html_action")) 
	{
		EnableDlgItem(IDC_ASK, FALSE);
		EnableDlgItem(IDC_PLAIN, FALSE);
		EnableDlgItem(IDC_ANYWAY, FALSE);
		EnableDlgItem(IDC_BOTH, FALSE);
	}

	return CMailNewsPropertyPage::InitDialog();
}

STDMETHODIMP CHTMLFormatPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) 
	{
		BOOL bHtmlCompose;
		int32 n = 0;

		PREF_GetBoolPref("mail.html_compose", &bHtmlCompose);
		if (bHtmlCompose)
			m_nRadioCompose	= 0;
		else
			m_nRadioCompose	= 1;
		PREF_GetIntPref("mail.default_html_action", &n);
		m_nRadioHTML = (int)n;
	}
	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CHTMLFormatPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	RadioButtonTransfer(IDC_HTMLCOMPOSE, m_nRadioCompose, bSaveAndValidate);
	RadioButtonTransfer(IDC_ASK, m_nRadioHTML, bSaveAndValidate);
	return TRUE;
}

BOOL CHTMLFormatPrefs::ApplyChanges()
{
	// Apply changes using XP preferences
	if (m_nRadioCompose == 0)
		PREF_SetBoolPref("mail.html_compose", TRUE);
	else
		PREF_SetBoolPref("mail.html_compose", FALSE);
	PREF_SetIntPref("mail.default_html_action", (int32)m_nRadioHTML);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// COutgoingMsgPrefs implementation

COutgoingMsgPrefs::COutgoingMsgPrefs()
	: CMailNewsPropertyPage(IDD_OUTGOING_MSG, HELP_PREFS_MAILNEWS_COPIES)
{											  
}

BOOL COutgoingMsgPrefs::InitDialog()
{
	CheckIfLockedPref("mail.cc_self", IDC_CHECK_MAIL_SELF);
	CheckIfLockedPref("mail.default_cc", IDC_EDIT_MAIL_TOADDRESS);
	CheckIfLockedPref("news.cc_self", IDC_CHECK_NEWS_SELF);
	CheckIfLockedPref("news.default_cc", IDC_EDIT_NEWS_TOADDRESS);
	CheckIfLockedPref("mail.use_fcc", IDC_CHECK_MAIL_FOLDER);
	CheckIfLockedPref("mail.default_fcc", IDC_CHECK_MAIL_FOLDER);
	CheckIfLockedPref("news.use_fcc", IDC_CHECK_NEWS_FOLDER);
	CheckIfLockedPref("news.default_fcc", IDC_CHECK_NEWS_FOLDER);
	CheckIfLockedPref("mail.default_drafts", IDC_STATIC_DRAFT);
	CheckIfLockedPref("mail.default_templates", IDC_STATIC_TEMPLATE);

	if (m_bMailImapFcc)
		PREF_GetStringPref("mail.imap_sentmail_path", m_strMailFccFolder);
	else
		PREF_GetStringPref("mail.default_fcc", m_strMailFccFolder);
	if (m_bNewsImapFcc)
		PREF_GetStringPref("news.imap_sentmail_path", m_strNewsFccFolder);
	else
		PREF_GetStringPref("news.default_fcc", m_strNewsFccFolder);
	PREF_GetStringPref("mail.default_drafts", m_strDraftFolder);
	PREF_GetStringPref("mail.default_templates", m_strTemplateFolder);
	GetCheckText(m_strMailFccFolder, TYPE_SENTMAIL, IDC_CHECK_MAIL_FOLDER, 
		IDS_FOLDER_MAIL);
	GetCheckText(m_strNewsFccFolder, TYPE_SENTNEWS, IDC_CHECK_NEWS_FOLDER, 
		IDS_FOLDER_NEWS);
	GetCheckText(m_strDraftFolder, TYPE_DRAFT, IDC_STATIC_DRAFT, 
		IDS_FOLDER_DRAFT);
	GetCheckText(m_strTemplateFolder, TYPE_TEMPLATE, IDC_STATIC_TEMPLATE,
		IDS_FOLDER_TEMPLATE);

	return CMailNewsPropertyPage::InitDialog();
}

STDMETHODIMP
COutgoingMsgPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {

		PREF_GetBoolPref("mail.cc_self", &m_bMailCcSelf);
		PREF_GetStringPref("mail.default_cc", m_strMailDefaultCc);
		PREF_GetBoolPref("news.cc_self", &m_bNewsCcSelf);
		PREF_GetStringPref("news.default_cc", m_strNewsDefaultCc);

		PREF_GetBoolPref("mail.use_fcc", &m_bMailUseFcc);
		PREF_GetBoolPref("news.use_fcc", &m_bNewsUseFcc);

		PREF_GetBoolPref("mail.use_imap_sentmail", &m_bMailImapFcc);
		PREF_GetBoolPref("news.use_imap_sentmail", &m_bNewsImapFcc);
	}

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL COutgoingMsgPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	CheckBoxTransfer(IDC_CHECK_MAIL_SELF, m_bMailCcSelf, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_MAIL_TOADDRESS, m_strMailDefaultCc, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_NEWS_SELF, m_bNewsCcSelf, bSaveAndValidate);
	EditFieldTransfer(IDC_EDIT_NEWS_TOADDRESS, m_strNewsDefaultCc, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_MAIL_FOLDER, m_bMailUseFcc, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_NEWS_FOLDER, m_bNewsUseFcc, bSaveAndValidate);

	return TRUE;
}

BOOL COutgoingMsgPrefs::ApplyChanges()
{
	CString tmpStr;
	PREF_SetBoolPref("mail.cc_self", m_bMailCcSelf);
	PREF_SetCharPref("mail.default_cc", m_strMailDefaultCc);
	PREF_SetBoolPref("news.cc_self", m_bNewsCcSelf);
	PREF_SetCharPref("news.default_cc", m_strNewsDefaultCc);

	PREF_SetBoolPref("mail.use_fcc", m_bMailUseFcc);
	tmpStr = m_strMailFccFolder;
	tmpStr.MakeLower();
	if (strstr(tmpStr, "imap://"))
	{
		PREF_SetBoolPref("mail.use_imap_sentmail", TRUE);
		PREF_SetCharPref("mail.imap_sentmail_path", m_strMailFccFolder);
	}
	else
	{
		PREF_SetBoolPref("mail.use_imap_sentmail", FALSE);
		PREF_SetCharPref("mail.default_fcc", m_strMailFccFolder);
	}
	PREF_SetBoolPref("news.use_fcc", m_bNewsUseFcc);
	tmpStr = m_strNewsFccFolder;
	tmpStr.MakeLower();
	if (strstr(tmpStr, "imap://"))
	{
		PREF_SetBoolPref("news.use_imap_sentmail", TRUE);
		PREF_SetCharPref("news.imap_sentmail_path", m_strNewsFccFolder);
	}
	else
	{
		PREF_SetBoolPref("news.use_imap_sentmail", FALSE);
		PREF_SetCharPref("news.default_fcc", m_strNewsFccFolder);
	}

	PREF_SetCharPref("mail.default_drafts", m_strDraftFolder);
	PREF_SetCharPref("mail.default_templates", m_strTemplateFolder);
	return TRUE;
}

BOOL COutgoingMsgPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (notifyCode == BN_CLICKED) 
	{
		switch (id)
		{
			case IDC_CHOOSE_FOLDER_MAIL:
				ChooseFolder(m_strMailFccFolder, TYPE_SENTMAIL, 
					IDC_CHECK_MAIL_FOLDER, IDS_FOLDER_MAIL);
				return TRUE;
			case IDC_CHOOSE_FOLDER_NEWS:
				ChooseFolder(m_strNewsFccFolder, TYPE_SENTNEWS, 
					IDC_CHECK_NEWS_FOLDER, IDS_FOLDER_NEWS);
				return TRUE;
			case IDC_CHOOSE_FOLDER_DRAFT:
				ChooseFolder(m_strDraftFolder, TYPE_DRAFT, 
					IDC_STATIC_DRAFT, IDS_FOLDER_DRAFT);
				return TRUE;
			case IDC_CHOOSE_FOLDER_TEMPLATE:
				ChooseFolder(m_strTemplateFolder, TYPE_TEMPLATE, 
					IDC_STATIC_TEMPLATE, IDS_FOLDER_TEMPLATE);
				return TRUE;
		}
	}

	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

void COutgoingMsgPrefs::GetCheckText
(CString& szFolderPath, int nType, int nCtlID, int nStrID)
{
	assert(m_pObject);
	if (m_pObject) 
	{
		LPMAILNEWSINTERFACE	lpInterface;

		// Request the IMailNewsInterface interface from our data object
		if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
												(void **)&lpInterface))) 
		{
			LPOLESTR	lpoleFolder = NULL;
			LPOLESTR	lpoleServer = NULL;
 			lpInterface->GetFolderServer((LPOLESTR)LPCSTR(szFolderPath), (DWORD)nType,
										  &lpoleFolder, &lpoleServer);
			if (lpoleFolder && lpoleServer) 
			{
				CString folder = lpoleFolder;
				CString server = lpoleServer;
				SetCheckText(LPCSTR(folder), LPCSTR(server), nCtlID, nStrID);
				CoTaskMemFree(lpoleFolder);
				CoTaskMemFree(lpoleServer);
 			}
			lpInterface->Release();
		}
	}
}

void COutgoingMsgPrefs::ChooseFolder
(CString& szFolderPath, int nType, int nCtlID, int nStrID)
{
	assert(m_pObject);
	if (m_pObject) 
	{
		LPMAILNEWSINTERFACE	lpInterface;

		// Request the IMailNewsInterface interface from our data object
		if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
												(void **)&lpInterface))) 
		{
			LPOLESTR	lpoleFolder = NULL;
			LPOLESTR	lpoleServer = NULL;
			LPOLESTR	lpolePref = NULL;
			lpInterface->ShowChooseFolder(GetParent(m_hwndDlg), 
										  (LPOLESTR)LPCSTR(szFolderPath), (DWORD)nType,
										  &lpoleFolder, &lpoleServer, &lpolePref);
			if (lpoleFolder && lpoleServer) 
			{
				CString folder = lpoleFolder;
				CString server = lpoleServer;
				SetCheckText(LPCSTR(folder), LPCSTR(server), nCtlID, nStrID);
				CoTaskMemFree(lpoleFolder);
				CoTaskMemFree(lpoleServer);
 			}
			if (lpolePref)
			{
				szFolderPath = lpolePref;
				CoTaskMemFree(lpolePref);
 			}
			lpInterface->Release();
		}
	}
}

void COutgoingMsgPrefs::SetCheckText(const char* pFolder, const char* pServer, int nCtlID, int nStrID)
{
	char szFormat[256], szText[256];
	int	nLen = LoadString(m_hInstance, nStrID, szFormat, sizeof(szFormat));
	sprintf(szText, LPCSTR(szFormat), pFolder, pServer);
	if (nCtlID == IDC_STATIC_DRAFT || nCtlID == IDC_STATIC_TEMPLATE)
		Static_SetText(GetDlgItem(m_hwndDlg, nCtlID), szText);
	else	
		Button_SetText(GetDlgItem(m_hwndDlg, nCtlID), szText);
}


/////////////////////////////////////////////////////////////////////////////
// CAddressingPrefs implementation

CAddressingPrefs::CAddressingPrefs()
	: CMailNewsPropertyPage(IDD_ADDRESSING, HELP_PREFS_MAILNEWS_ADDRESSING)
{
	m_bInAddressBook = FALSE;
	m_bInDirectory = FALSE;
	m_bMatchPabName = FALSE;
}

STDMETHODIMP CAddressingPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) 
	{
		PREF_GetBoolPref("ldap_1.autoComplete.useAddressBooks", &m_bInAddressBook);
		PREF_GetBoolPref("ldap_1.autoComplete.useDirectory", &m_bInDirectory);
		PREF_GetBoolPref("ldap_1.autoComplete.skipDirectoryIfLocalMatchFound", &m_bMatchPabName);
		BOOL bTemp;
		PREF_GetBoolPref("ldap_1.autoComplete.showDialogForMultipleMatches", &bTemp);
		if (bTemp)
			m_nRadioListChoice = 0;
		else
			m_nRadioListChoice = 1;
		PREF_GetBoolPref("mail.addr_book.lastnamefirst", &bTemp);
		if (bTemp)
			m_nRadioSortChoice = 1;
		else
			m_nRadioSortChoice = 0;
	}
	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CAddressingPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	CheckBoxTransfer(IDC_CHECK_IN_PAB, m_bInAddressBook, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_IN_DIR, m_bInDirectory, bSaveAndValidate);
	ComboBoxTransfer(IDC_COMBO_DIRS, m_strLdapDir, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK_PAB_FIRST, m_bMatchPabName, bSaveAndValidate);

	RadioButtonTransfer(IDC_RADIO_SHOW, m_nRadioListChoice, bSaveAndValidate);
	RadioButtonTransfer(IDC_FIRSTNAME_FIRST, m_nRadioSortChoice, bSaveAndValidate);
	return TRUE;
}

BOOL CAddressingPrefs::ApplyChanges()
{
	// Apply changes using XP preferences
	PREF_SetBoolPref("ldap_1.autoComplete.useAddressBooks", m_bInAddressBook);
	PREF_SetBoolPref("ldap_1.autoComplete.useDirectory", m_bInDirectory);
	PREF_SetBoolPref("ldap_1.autoComplete.skipDirectoryIfLocalMatchFound", m_bMatchPabName);
	if (m_nRadioListChoice == 0)
		PREF_SetBoolPref("ldap_1.autoComplete.showDialogForMultipleMatches", TRUE);
	else
		PREF_SetBoolPref("ldap_1.autoComplete.showDialogForMultipleMatches", FALSE);

	if (m_nRadioSortChoice == 0)
		PREF_SetBoolPref("mail.addr_book.lastnamefirst", FALSE);
	else
		PREF_SetBoolPref("mail.addr_book.lastnamefirst", TRUE);

	//we need to save the changes made to which directory to autocomplete from
	assert(m_pObject);
	if (m_pObject) 
	{
		LPMAILNEWSINTERFACE	lpInterface;

		// Request the IMailNewsInterface interface from our data object
		if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
												(void **)&lpInterface))) 
		{
			lpInterface->SaveLdapDirAutoComplete();
			lpInterface->Release();
		}
	}   

	return TRUE;
}

BOOL CAddressingPrefs::InitDialog()
{
	// Fill the list box with the list of accept languages
	FillDirComboBox();

	if (m_bInAddressBook && m_bInDirectory)
		EnableDlgItem(IDC_CHECK_PAB_FIRST, TRUE);
	else
		EnableDlgItem(IDC_CHECK_PAB_FIRST, FALSE);

	if (MNPREF_PrefIsLocked("ldap_1.autoComplete.useAddressBooks"))
		EnableDlgItem(IDC_CHECK_IN_PAB, FALSE);
	if (MNPREF_PrefIsLocked("ldap_1.autoComplete.useDirectory"))
	{
		EnableDlgItem(IDC_CHECK_IN_DIR, FALSE);	 
		EnableDlgItem(IDC_COMBO_DIRS, FALSE);	  
	}
	if (MNPREF_PrefIsLocked("ldap_1.autoComplete.skipDirectoryIfLocalMatchFound"))
		EnableDlgItem(IDC_CHECK_PAB_FIRST, FALSE);
	if (MNPREF_PrefIsLocked("ldap_1.autoComplete.showDialogForMultipleMatches"))
	{
		EnableDlgItem(IDC_RADIO_SHOW, FALSE);
		EnableDlgItem(IDC_RADIO_NO_SHOW, FALSE);
	}
	if (MNPREF_PrefIsLocked("mail.addr_book.lastnamefirst"))
	{
		EnableDlgItem(IDC_FIRSTNAME_FIRST, FALSE);
		EnableDlgItem(IDC_LASTNAME_FIRST, FALSE);
	}
	
	return CMailNewsPropertyPage::InitDialog();
}

void CAddressingPrefs::FillDirComboBox()
{
	assert(m_pObject);
	if (m_pObject) 
	{
		LPMAILNEWSINTERFACE	lpInterface;

		// Request the IMailNewsInterface interface from our data object
		if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
												(void **)&lpInterface))) 
		{
			char szDir[256];
			HWND hwndList = GetDlgItem(m_hwndDlg, IDC_COMBO_DIRS);
			lpInterface->FillLdapDirList(hwndList, NULL);
			int index = ComboBox_GetCurSel(hwndList);
			ComboBox_GetLBText(hwndList, index, szDir);
			m_strLdapDir = szDir;
			lpInterface->Release();
		}
	}   
}

void CAddressingPrefs::SetDirAutoComplete(BOOL bOnOrOff)
{
	assert(m_pObject);
	if (m_pObject) 
	{
		LPMAILNEWSINTERFACE	lpInterface;

		// Request the IMailNewsInterface interface from our data object
		if (SUCCEEDED(m_pObject->QueryInterface(IID_MailNewsInterface, 
												(void **)&lpInterface))) 
		{
			HWND hwndList = GetDlgItem(m_hwndDlg, IDC_COMBO_DIRS);
			lpInterface->SetLdapDirAutoComplete(hwndList, bOnOrOff);
			lpInterface->Release();
		}
	} 
}

BOOL CAddressingPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if ((id == IDC_CHECK_IN_PAB || id == IDC_CHECK_IN_DIR) && 
		notifyCode == BN_CLICKED) 
	{
		if (IsDlgButtonChecked(m_hwndDlg, IDC_CHECK_IN_PAB) && 
			IsDlgButtonChecked(m_hwndDlg, IDC_CHECK_IN_DIR))
			EnableDlgItem(IDC_CHECK_PAB_FIRST, TRUE);
		else
			EnableDlgItem(IDC_CHECK_PAB_FIRST, FALSE);
	}

	if (id == IDC_CHECK_IN_DIR && notifyCode == BN_CLICKED) 
	{
		if (IsDlgButtonChecked(m_hwndDlg, IDC_CHECK_IN_DIR))
			SetDirAutoComplete(TRUE);
		else
			SetDirAutoComplete(FALSE);
	}
	if (id == IDC_COMBO_DIRS && notifyCode == CBN_SELCHANGE) 
	{
		if (IsDlgButtonChecked(m_hwndDlg, IDC_CHECK_IN_DIR))
			SetDirAutoComplete(TRUE);

		char szDir[256];
		HWND hwndList = GetDlgItem(m_hwndDlg, IDC_COMBO_DIRS);
		int index = ComboBox_GetCurSel(hwndList);
		ComboBox_GetLBText(hwndList, index, szDir);
		m_strLdapDir = szDir;
	}
	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CDiskSpacePrefs implementation

CDiskSpacePrefs::CDiskSpacePrefs()
	: CMailNewsPropertyPage(IDD_DISK_SPACE, HELP_PREFS_ADVANCED_DISK_SPACE)
{
}

// Initialize member data using XP preferences
STDMETHODIMP CDiskSpacePrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
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

	return CMailNewsPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL CDiskSpacePrefs::InitDialog()
{
	// Limit the length of the string the user can type
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_MSG_SIZE), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_COMPACT_SIZE), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_MSG_DAYS), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_MSG_COUNT), 5);
	Edit_LimitText(GetDlgItem(m_hwndDlg, IDC_EDIT_REMOVE_DAYS), 5);

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
		EnableDlgItem(IDC_KEEP_MSG_DAYS, FALSE);
		EnableDlgItem(IDC_KEEP_ALL, FALSE);
		EnableDlgItem(IDC_KEEP_COUNT, FALSE);
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
		EnableDlgItem(IDC_EDIT_REMOVE_DAYS, FALSE);

	return CMailNewsPropertyPage::InitDialog();
}

BOOL CDiskSpacePrefs::DoTransfer(BOOL bSaveAndValidate)
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
	EditFieldTransfer(IDC_EDIT_REMOVE_DAYS, m_nRemoveDays, bSaveAndValidate);
	return TRUE;
}

// Apply changes using XP preferences
BOOL CDiskSpacePrefs::ApplyChanges()
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

	// Apply changes using XP preferences
	return TRUE;
}

BOOL CDiskSpacePrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
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
		case IDC_EDIT_REMOVE_DAYS:
			if (notifyCode == EN_UPDATE)
				ValidateNumeric(m_hwndDlg, m_hInstance, IDS_DISK_SPACE, 
				                id, m_nRemoveDays);
			return TRUE;
	}
	return CMailNewsPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}
