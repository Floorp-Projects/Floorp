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

#ifndef __PAGES_H_
#define __PAGES_H_

#include "cppageex.h"
#include "xp_list.h"

//also in brpref\src\advpages.cpp, winfe\mnprefs.cpp
#define MNPREF_PrefIsLocked(pPrefName) PREF_PrefIsLocked(pPrefName)

/////////////////////////////////////////////////////////////////////////////
// Helper functions

// CString interface to PREF_GetCharPref routine
int
PREF_GetStringPref(LPCSTR lpszPref, CString &str);

/////////////////////////////////////////////////////////////////////////////
// CMailNewsPrefs

// Simple base class that handles reference counting of the DLL object and
// caching the size of the property page. Note: all property pages derived
// from this class MUST be the same size
class CMailNewsPropertyPage : public CPropertyPageEx {
	public:
		CMailNewsPropertyPage(UINT nTemplateID, LPCSTR lpszHelpTopic);

	protected:
		HRESULT	GetPageSize(SIZE &);

		// Disables the control (specified by ID) and return TRUE if the preference
		// is locked; returns FALSE otherwise
		BOOL		CheckIfLockedPref(LPCSTR lpszPref, UINT nIDCtl);

	private:
		CRefDll	m_refDll;

		static SIZE	m_size;  // cached size
};

/////////////////////////////////////////////////////////////////////////////
// CMailNewsPrefs

class CMailNewsPrefs : public CMailNewsPropertyPage {
	public:
		CMailNewsPrefs();

	protected:
		BOOL		 InitDialog();
 		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();

		// Event Processing
		LRESULT	WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:
		CString		m_strFontStyle;
		int			m_nFontStyle;
		int			m_nFontSize;
		int			m_nFontColor;
		COLORREF	m_rgbQuotedColor;

		int			m_nWidthRadio;
		BOOL		m_bFixWidthFont;
		BOOL		m_bEnableSound;
		BOOL		m_bUseMapi;
		BOOL		m_bRemeberLastMsg;
		BOOL		m_bConfirmOnDelete;

		COLORREF	GetColorButtonColor(UINT nCtlID);
		void		SetColorButtonColor(UINT nCtlID, COLORREF);
};

/////////////////////////////////////////////////////////////////////////////
// CIdentityPrefs

class CIdentityPrefs : public CMailNewsPropertyPage {
	public:
		CIdentityPrefs();

	protected:
		BOOL	InitDialog();
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();			
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:
		CString	m_strName;
		CString	m_strEmail;
		CString	m_strReply;
		CString	m_strOrganization;
		CString	m_strSigFile;
		BOOL	m_bAttchVCard;
};

/////////////////////////////////////////////////////////////////////////////
// CMessagesPrefs

class CMessagesPrefs : public CMailNewsPropertyPage {
	public:
		CMessagesPrefs();

	protected:
		BOOL	InitDialog();
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:

		void	ValidateNonZero();

		int		m_nForwardChoice;
		BOOL	m_bAutoQuote;
		int		m_nRadioQuoteType;
		BOOL	m_bWarpIncoming;
		int		m_nWrapLen;
		int		m_nRadio8bits;
};

/////////////////////////////////////////////////////////////////////////////
// CMailServerPrefs

class CMailServerPrefs : public CMailNewsPropertyPage {
	public:
		CMailServerPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	InitDialog();
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:

		int		m_nServerType;
		int		m_nUseSSL;
		CString m_strPopServer;
		CString	m_strUserName;
		CString	m_strOutServer;
		CString	m_strMailDir;
		BOOL	m_bSetDefault;

		void GetPopServerName();
		void GetMailServerType();
		void GetIMAPServers(HWND hwndList);
		void CheckButtons(HWND hwndList);
		void DoDefaultServer(HWND hwndList);
};

/////////////////////////////////////////////////////////////////////////////
// CNewsServerPrefs

class CNewsServerPrefs : public CMailNewsPropertyPage {
	public:
		CNewsServerPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	InitDialog();
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();
		void	DoDefaultServer(HWND hwndList);
		int		GetDefaultServerIndex(HWND hwndList);
		void	SetServerName(HWND hwndList, int nIndex, CString& name);
		void	CheckButtons(HWND hwndList);

		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);
		
	private:
		CString	m_strNewsServer;
		CString	m_strNewsDir;
		BOOL	m_bNotify;
		BOOL	m_bSecure;
		int		m_nMaxMsgs;
		int		m_nPort;
		LPVOID	m_pDefaultServer;
};

/////////////////////////////////////////////////////////////////////////////
// CMailReceiptPrefs

class CMailReceiptPrefs : public CMailNewsPropertyPage {
	public:
		CMailReceiptPrefs();

	protected:
		BOOL	InitDialog();
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();
		
		// Event Processing
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:
		int		m_nRequestReceipt;
		int		m_nReceiptArrive;
		int		m_nEnableRespond;
};

/////////////////////////////////////////////////////////////////////////////
// CHTMLFormatPrefs

class CHTMLFormatPrefs : public CMailNewsPropertyPage {
	public:
		CHTMLFormatPrefs();

	protected:
		BOOL	InitDialog();
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();
		
	private:
		BOOL	m_nRadioCompose;
		int		m_nRadioHTML;
};

/////////////////////////////////////////////////////////////////////////////
// COutgoingMsgPrefs

class COutgoingMsgPrefs : public CMailNewsPropertyPage {
	public:
		COutgoingMsgPrefs();

	protected:
		BOOL	InitDialog();
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();

		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:

		BOOL	m_bMailCcSelf;
		CString	m_strMailDefaultCc;
		BOOL	m_bNewsCcSelf;
		CString	m_strNewsDefaultCc;
		BOOL	m_bMailUseFcc;
		CString	m_strMailFccFolder;
		BOOL	m_bNewsUseFcc;
		CString	m_strNewsFccFolder;
		BOOL	m_bUseDrafts;
		CString	m_strDraftFolder;
		BOOL	m_bUseTemplates;
		CString	m_strTemplateFolder;
		
		BOOL	m_bMailImapFcc;
		BOOL	m_bNewsImapFcc;

		void	GetCheckText(CString& szFolderPath, int nType, int nCtlID, int nStrID);
		void	ChooseFolder(CString& szFolderPath, int nType, int nCtlID, int nStrID);
		void	SetCheckText(const char* pFolder, const char* pServer, int nCtlID, int nStrID);
};

/////////////////////////////////////////////////////////////////////////////
// CAddressingPrefs

class CAddressingPrefs : public CMailNewsPropertyPage {
	public:
		CAddressingPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();
		BOOL	InitDialog();
		
		// Event Processing
		BOOL		OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:

		void	FillDirComboBox();
		void	SetDirAutoComplete(BOOL bOnOrOff);

		int		m_nRadioSortChoice;
		int		m_nRadioListChoice;
		BOOL	m_bInAddressBook;
		BOOL	m_bInDirectory;
		BOOL	m_bMatchPabName;
		CString	m_strLdapDir;
};

/////////////////////////////////////////////////////////////////////////////
// CDiskSpacePrefs

class CDiskSpacePrefs : public CMailNewsPropertyPage {
	public:
		CDiskSpacePrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 InitDialog();
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();

		// Event Processing
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:

		BOOL	m_bLimitSize;
		int		m_nLimitSize;
		BOOL	m_bPromptPurge;
		int		m_nPurgeSize;
		int		m_nKeepMethod;
		int		m_nKeepDays;
		int		m_nKeepCounts;
		BOOL	m_bKeepUnread;
		int		m_nRemoveDays;
		BOOL	m_bRemoveBody;
};

/////////////////////////////////////////////////////////////////////////////
// COfflinePrefs

class COfflinePrefs : public CMailNewsPropertyPage {
	public:
		COfflinePrefs();

	protected:
		BOOL	InitDialog();
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();			

	private:

		int		m_nModeRadioChoice;
		int		m_nOnlineRadioChoice;
		BOOL	m_bPromptSynchronize;
};

/////////////////////////////////////////////////////////////////////////////
// COfflineNewsPrefs

class COfflineNewsPrefs : public CMailNewsPropertyPage {
	public:
		COfflineNewsPrefs();

	protected:
		BOOL	InitDialog();
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:

		void	EnableCheckByDate(BOOL bEnable);
		void	SetDiscussionCount(int nCount);

		int		m_nDownLoadMsg;
		int		m_nDownloadRadioChoice;		
		int		m_nDownLoadInterval;
		int		m_nDownLoadDays;
		int		m_nDiscussions;

		BOOL	m_bByMessage;
		BOOL	m_bUnreadOnly;
		BOOL	m_bByDate;
		
};

#endif /* __NSPROP_H_ */

