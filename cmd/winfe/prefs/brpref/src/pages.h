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
#include "ibrprefs.h"
#include "intlfont.h"

/////////////////////////////////////////////////////////////////////////////
// Helper functions

// CString interface to PREF_GetCharPref routine
int
PREF_GetStringPref(LPCSTR lpszPref, CString &str);

/////////////////////////////////////////////////////////////////////////////
// CBrowserPropertyPage

// Simple base class that handles reference counting of the DLL object and
// caching the size of the property page. Note: all property pages derived
// from this class MUST be the same size
class CBrowserPropertyPage : public CPropertyPageEx {
	public:
		CBrowserPropertyPage(UINT nTemplateID, LPCSTR lpszHelpTopic);

	protected:
		HRESULT		GetPageSize(SIZE &);

		// Disables the control (specified by ID) and return TRUE if the preference
		// is locked; returns FALSE otherwise
		BOOL		CheckIfLockedPref(LPCSTR lpszPref, UINT nIDCtl);

		// Helper routine to disable all the radio buttons in a group
		void		DisableRadioButtonGroup(UINT nIDButton);

	private:
		CRefDll	m_refDll;

		static SIZE	m_size;  // cached size
};

/////////////////////////////////////////////////////////////////////////////
// CAppearancePrefs

class CAppearancePrefs : public CBrowserPropertyPage {
	public:
		CAppearancePrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL		 InitDialog();

	private:
		BOOL	m_bStartupBrowser;
		BOOL	m_bStartupMail;
		BOOL	m_bStartupNews;
		BOOL	m_bStartupEditor;
		BOOL	m_bStartupNetcaster;
		int		m_nShowToolbarAs;
};

/////////////////////////////////////////////////////////////////////////////
// CFontsPrefs

class CFontsPrefs : public CBrowserPropertyPage {
	public:
		CFontsPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();			
		BOOL		 InitDialog();

		// Override to acquire/release the IBrowserPrefs interface
		// pointer
		STDMETHODIMP SetObjects(ULONG cObjects, LPUNKNOWN FAR* ppunk);
		
		// Event Processing
		LRESULT		 WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
		BOOL		 OnCommand(int id, HWND hwndCtl, UINT notifyCode);
	
	private:
		LPINTLFONT	m_lpIntlFont;
		CString		m_strVariableFaceName;
		CString		m_strFixedFaceName;
		int			m_nVariableSize;
		int			m_nFixedSize;
		DWORD		m_dwEncodings;
		int			m_nCharset;  // charset num for currently selected encoding
		int			m_nUseDocumentFonts;
		int			m_nMaxFontHeight;
		BOOL		m_bDBCSEnabled;

		void	FillFontFace(BOOL bIgnorePitch);
		void	FillEncodingNames();
		void	OnEncodingChanged();
		BOOL	UseSystemFont();
		void    DrawComboBoxItem(LPDRAWITEMSTRUCT lpdis);
		void	MeasureComboBoxItem(LPMEASUREITEMSTRUCT lpmis);
};

/////////////////////////////////////////////////////////////////////////////
// CColorsPrefs

class CColorsPrefs : public CBrowserPropertyPage {
	public:
		CColorsPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL		 InitDialog();

		// Event Processing
		LRESULT	WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:
		BOOL		m_bUnderlineLinks;
		COLORREF	m_rgbVisitedLinks;
		COLORREF	m_rgbUnvisitedLinks;
		BOOL		m_bUseWindowsColors;
		COLORREF	m_rgbTextColor;
		COLORREF	m_rgbBackgroundColor;
		BOOL		m_bOverrideDocumentColors;

		void		EnableColorButtons();
		COLORREF	GetColorButtonColor(UINT nCtlID);
		void		SetColorButtonColor(UINT nCtlID, COLORREF);
};

/////////////////////////////////////////////////////////////////////////////
// CBrowserPrefs

class CBrowserPrefs : public CBrowserPropertyPage {
	public:
		CBrowserPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL		 InitDialog();
		
		// Event Processing
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:
		CString	m_strHomePageURL;
		int		m_nStartsWith;
		int		m_nExpireAfter;
		CString	m_strCurrentPage;
};

/////////////////////////////////////////////////////////////////////////////
// CLanguagesPrefs

class CLanguagesPrefs : public CBrowserPropertyPage {
	public:
		CLanguagesPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL		 InitDialog();
		
		// Event Processing
		BOOL		OnCommand(int id, HWND hwndCtl, UINT notifyCode);
		LRESULT		WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

	private:
		HBITMAP	m_hUpBitmap;
		HBITMAP	m_hDownBitmap;
		HDC		m_hMemDC;
		int		m_nTabStops[2];
		CString	m_strAcceptLangs;

		void	CheckButtons();
		void	SetLBoxItemHeight();
		void	DrawLBoxItem(LPDRAWITEMSTRUCT lpdis);
		void	FillListBox();
};

/////////////////////////////////////////////////////////////////////////////
// CApplicationsPrefs

class CApplicationsPrefs : public CBrowserPropertyPage {
	public:
		CApplicationsPrefs();

	protected:
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		BOOL	ApplyChanges();
		BOOL	InitDialog();
		
		// Override to acquire/release the IBrowserPrefs interface
		// pointer
		STDMETHODIMP SetObjects(ULONG cObjects, LPUNKNOWN FAR* ppunk);
		
		// Event Processing
		BOOL		OnCommand(int id, HWND hwndCtl, UINT notifyCode);
		LRESULT		WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
		
		void		OnNewItem();
		void		OnEditItem();
		void		OnRemoveItem();

	private:
		LPBROWSERPREFS	m_lpBrowserPrefs;
		NET_cdataStruct	m_telnet;
		NET_cdataStruct	m_tn3270;
		CString			m_strTelnet;
		CString			m_strTN3270;

		// Helper routines
		void	DisplayFileDetails();
		void	SetLBoxItemHeight();
		void	DrawLBoxItem(LPDRAWITEMSTRUCT lpdis);
};

/////////////////////////////////////////////////////////////////////////////
// CSmartBrowsingPrefs

class CSmartBrowsingPrefs : public CBrowserPropertyPage {
	public:
		CSmartBrowsingPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL		 InitDialog();

	private:
		BOOL	m_bEnableKeywords;
		CString	m_strExcludedDomains;
};

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPrefs

class CAdvancedPrefs : public CBrowserPropertyPage {
	public:
		CAdvancedPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL		 InitDialog();

	private:
		BOOL	m_bAutoLoadImages;
		BOOL	m_bEnableJava;
		BOOL	m_bEnableJavaScript;
		BOOL	m_bEnableStyleSheets;
		BOOL	m_bSendEmailAddressForFTPPassword;
		BOOL	m_bEnableAutoInstall;
		int		m_nCookieAcceptance;
		BOOL	m_bWarnAboutCookies;
};

/////////////////////////////////////////////////////////////////////////////
// CCachePrefs

class CCachePrefs : public CBrowserPropertyPage {
	public:
		CCachePrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL		 InitDialog();

		// Event Processing
		BOOL		OnCommand(int id, HWND hwndCtl, UINT notifyCode);
	
	private:
		UINT	m_uDiskCacheSize;
		UINT	m_uMemoryCacheSize;
		int		m_nCheckDocFrequency;
		CString	m_strDiskCacheDir;

#ifdef _WIN32
		BOOL		BrowseForCacheFolder(LPSTR lpszPath);
#endif
};

/////////////////////////////////////////////////////////////////////////////
// CProxiesPrefs

class CProxiesPrefs : public CBrowserPropertyPage {
	public:
		CProxiesPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL		 InitDialog();

		// Event Processing
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:
		int		m_nProxyType;
		CString	m_strAutoConfigURL;
		
		void	EnableControls();
};

/////////////////////////////////////////////////////////////////////////////
// CDiskSpacePrefs

class CDiskSpacePrefs : public CBrowserPropertyPage {
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
		BOOL	m_bRemoveBody;
		int		m_nRemoveDays;
};

#endif /* __PAGES_H_ */

