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
#include "resource.h"
#include "xp_core.h"
#include "xp_help.h"
#include "prefapi.h"
#include "brprefid.h"
#include "csid.h"

#include "fe_proto.h"


/////////////////////////////////////////////////////////////////////////////
// CAppearancePrefs implementation

CAppearancePrefs::CAppearancePrefs()
	: CBrowserPropertyPage(IDD_APPEARANCE, HELP_PREFS_APPEARANCE)
{
}

BOOL
CAppearancePrefs::InitDialog()
{
	// Only enable the Use Current Page button if we were given a URL
	//EnableDlgItem(IDC_CHECK5, FE_IsNetcasterInstalled());

	BOOL result = CBrowserPropertyPage::InitDialog();

	EnableDlgItem(IDC_CHECK1, !PREF_PrefIsLocked("general.startup.browser"));
#ifdef MOZ_MAIL_NEWS
	EnableDlgItem(IDC_CHECK2, !PREF_PrefIsLocked("general.startup.mail"));
	EnableDlgItem(IDC_CHECK3, !PREF_PrefIsLocked("general.startup.news"));
#ifdef EDITOR
	EnableDlgItem(IDC_CHECK4, !PREF_PrefIsLocked("general.startup.editor"));
#endif /* EDITOR */
#endif /* MOZ_MAIL_NEWS */
#if !defined(WIN16)   
	EnableDlgItem(IDC_CHECK5, !PREF_PrefIsLocked("general.startup.netcaster"));
#endif   
	if (PREF_PrefIsLocked("browser.chrome.button_style"))
	{
		DisableRadioButtonGroup(IDC_RADIO1);
	}
	return result;
	
	
}

// Initialize member data using XP preferences
STDMETHODIMP
CAppearancePrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		// Startup mode
		PREF_GetBoolPref("general.startup.browser", &m_bStartupBrowser);
		PREF_GetBoolPref("general.startup.mail", &m_bStartupMail);
		PREF_GetBoolPref("general.startup.news", &m_bStartupNews);
		PREF_GetBoolPref("general.startup.editor", &m_bStartupEditor);
#if !defined(WIN16)      
		PREF_GetBoolPref("general.startup.netcaster", &m_bStartupNetcaster);
#endif      

		if (!m_bStartupMail && !m_bStartupNews && !m_bStartupEditor && !m_bStartupNetcaster)
			m_bStartupBrowser = TRUE;  // make sure something is set

		// Toolbar style
		int32	nToolbarStyle;
		
		PREF_GetIntPref("browser.chrome.button_style", &nToolbarStyle);
		m_nShowToolbarAs = (int)nToolbarStyle;
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CAppearancePrefs::DoTransfer(BOOL bSaveAndValidate)
{
	CheckBoxTransfer(IDC_CHECK1, m_bStartupBrowser, bSaveAndValidate);
#ifdef MOZ_MAIL_NEWS
	CheckBoxTransfer(IDC_CHECK2, m_bStartupMail, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK3, m_bStartupNews, bSaveAndValidate);
#endif /* MOZ_MAIL_NEWS */
#ifdef EDITOR
	CheckBoxTransfer(IDC_CHECK4, m_bStartupEditor, bSaveAndValidate);
#endif /* EDITOR */
#if !defined(WIN16)
	CheckBoxTransfer(IDC_CHECK5, m_bStartupNetcaster, bSaveAndValidate);
#endif   
	RadioButtonTransfer(IDC_RADIO1, m_nShowToolbarAs, bSaveAndValidate);
	return TRUE;
}

// Apply changes using XP preferences
BOOL
CAppearancePrefs::ApplyChanges()
{
	if (!m_bStartupMail && !m_bStartupNews && !m_bStartupEditor && !m_bStartupNetcaster)
		m_bStartupBrowser = TRUE;  // make sure something is set
	
	PREF_SetBoolPref("general.startup.browser", m_bStartupBrowser);
#ifdef MOZ_MAIL_NEWS
	PREF_SetBoolPref("general.startup.mail", m_bStartupMail);
	PREF_SetBoolPref("general.startup.news", m_bStartupNews);
#endif /* MOZ_MAIL_NEWS */
#ifdef EDITOR
	PREF_SetBoolPref("general.startup.editor", m_bStartupEditor);
#endif /* EDITOR */
#if !defined(WIN16)
	PREF_SetBoolPref("general.startup.netcaster", m_bStartupNetcaster);
#endif   

	PREF_SetIntPref("browser.chrome.button_style", (int32)m_nShowToolbarAs);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CFontsPrefs implementation

CFontsPrefs::CFontsPrefs()
	: CBrowserPropertyPage(IDD_FONTS, HELP_PREFS_APPEARANCE_FONTS)
{
	m_lpIntlFont = NULL;
	m_nMaxFontHeight = 0;
	m_bDBCSEnabled = GetSystemMetrics(SM_DBCSENABLED);
}



// Override SetObjects() member function to acquire/release the IIntlFont
// interface pointer
STDMETHODIMP
CFontsPrefs::SetObjects(ULONG cObjects, LPUNKNOWN FAR* ppunk)
{
	HRESULT	hres = CBrowserPropertyPage::SetObjects(cObjects, ppunk);

	if (SUCCEEDED(hres)) {
		if (cObjects == 0) {
			assert(m_lpIntlFont);
			if (m_lpIntlFont) {
				// Release the interface pointer
				m_lpIntlFont->Release();
				m_lpIntlFont = NULL;
			}

		} else {
			assert(!m_lpIntlFont);
			if (!m_lpIntlFont) {
				m_pObject->QueryInterface(IID_IIntlFont, (void **)&m_lpIntlFont);
				assert(m_lpIntlFont);
			}
		}
	}

	return hres;
}

// Initialize member data using XP preferences
STDMETHODIMP
CFontsPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		// Webfont preference
		int32	nUseDocFonts;
		
		PREF_GetIntPref("browser.use_document_fonts", &nUseDocFonts);
		m_nUseDocumentFonts = (int)nUseDocFonts;

		// Font encoding
		assert(m_lpIntlFont);
		if (m_lpIntlFont) {
			// Get the number of encodings
			m_lpIntlFont->GetNumEncodings(&m_dwEncodings);

			// Get the current charset num
			DWORD	dwCharsetNum;

			m_lpIntlFont->GetCurrentCharset(&dwCharsetNum);
			m_nCharset = (int)dwCharsetNum;
		}
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

// Called whenever the value of the encoding combo box is changed.
// Also explicitly called from InitDialog()
void
CFontsPrefs::OnEncodingChanged()
{
	// Get the font and font sizes to use for this encoding
	assert(m_lpIntlFont);
	if (m_lpIntlFont) {
		ENCODINGINFO	info;

		// Get the face name and size to use for this encoding
		m_lpIntlFont->GetEncodingInfo(m_nCharset, &info);
		m_strVariableFaceName = info.szVariableWidthFont;
		m_nVariableSize = info.nVariableWidthSize;
		m_strFixedFaceName = info.szFixedWidthFont;
		m_nFixedSize = info.nFixedWidthSize;
		
		// Update the list of fonts from which to choose. The reason we do this
		// here is that for some encodings we show both proportional and fixed
		// fonts in both font lists. The reason for this is that multi-byte fonts
		// are huge and some people don't have both a proportional and fixed font
		FillFontFace(info.bIgnorePitch);
	}
}

int CALLBACK
EnumVariableWidthFonts(LPLOGFONT lpnlf, LPTEXTMETRIC lpntm, int nFontType, LPARAM lParam)
{
	if (lpntm->tmPitchAndFamily & TMPF_FIXED_PITCH) {
		// Ignore fonts with SYMBOL charset and ignore raster fonts
		if ((lpntm->tmCharSet != SYMBOL_CHARSET) && (lpntm->tmPitchAndFamily & (TMPF_VECTOR|TMPF_DEVICE|TMPF_TRUETYPE))) {
			ComboBox_AddString((HWND)lParam, lpnlf->lfFaceName);
		}
	}
	
	return TRUE;
}

int CALLBACK
EnumFixedWidthFonts(LPLOGFONT lpnlf, LPTEXTMETRIC lpntm, int nFontType, LPARAM lParam)
{
	if ((lpntm->tmPitchAndFamily & TMPF_FIXED_PITCH) == 0) {
		// Ignore fonts with SYMBOL charset and ignore raster fonts
		if ((lpntm->tmCharSet != SYMBOL_CHARSET) && (lpntm->tmPitchAndFamily & (TMPF_VECTOR|TMPF_DEVICE|TMPF_TRUETYPE))) {
			ComboBox_AddString((HWND)lParam, lpnlf->lfFaceName);
		}
	}

	return TRUE;
}

int CALLBACK
EnumFontsIgnorePitch(LPLOGFONT lpnlf, LPTEXTMETRIC lpntm, int nFontType, LPARAM lParam)
{
	// Ignore fonts with SYMBOL charset and ignore raster fonts
	if ((lpntm->tmCharSet != SYMBOL_CHARSET) && (lpntm->tmPitchAndFamily & (TMPF_VECTOR|TMPF_DEVICE|TMPF_TRUETYPE)))
		ComboBox_AddString((HWND)lParam, lpnlf->lfFaceName);
	
	return TRUE;
}

static void
FillFontSizes(HWND hCombo)
{
	static int	nSizes[] = {8,9,10,11,12,14,16,18,20,22,24,26,28,36,48,72,-1};

	for (int *pSize = nSizes; *pSize != -1; pSize++) {
		char	szBuf[16];

		wsprintf(szBuf, "%i", *pSize);
		ComboBox_AddString(hCombo, szBuf);
	}
}

// Ignore pitch if we are doing this for Multibyte text. Multibyte fonts are huge
// and so some people only have one font for both proportional and fixed
void CFontsPrefs::FillFontFace(BOOL bIgnorePitch)
{
	// Load up the list of variable-width fonts
	HDC				hdc = GetDC(m_hwndDlg);
	HWND			hCombo;
	FONTENUMPROC	lpfnEnumProc;
	
	// Fill the list of variable width fonts
	lpfnEnumProc = bIgnorePitch ? (FONTENUMPROC)EnumFontsIgnorePitch :
		(FONTENUMPROC)EnumVariableWidthFonts;

	// Clear out the contents of the combo box
	hCombo = GetDlgItem(m_hwndDlg, IDC_COMBO2);
	ComboBox_ResetContent(hCombo);

#ifdef _WIN32
	EnumFontFamilies(hdc, NULL, lpfnEnumProc, (LPARAM)hCombo);
#else
	EnumFontFamilies(hdc, NULL, lpfnEnumProc, (LPSTR)hCombo);
#endif

	// Fill the list of fixed-width fonts
	lpfnEnumProc = bIgnorePitch ? (FONTENUMPROC)EnumFontsIgnorePitch :
		(FONTENUMPROC)EnumFixedWidthFonts;
	
	hCombo = GetDlgItem(m_hwndDlg, IDC_COMBO4);
	ComboBox_ResetContent(hCombo);

#ifdef _WIN32
	EnumFontFamilies(hdc, NULL, lpfnEnumProc, (LPARAM)hCombo);
#else
	EnumFontFamilies(hdc, NULL, lpfnEnumProc, (LPSTR)hCombo);
#endif
	
	ReleaseDC(m_hwndDlg, hdc);
}

// If we're on a double-byte character system and the dialog font
// is "MS Sans Serif" or "Helv" then use the system font
BOOL
CFontsPrefs::UseSystemFont()
{
	if (m_bDBCSEnabled) {
		HFONT	hFont = GetWindowFont(m_hwndDlg);
		LOGFONT	font;
	
		font.lfFaceName[0] = '\0';
		GetObject(hFont, sizeof(font), &font);
		return lstrcmp(font.lfFaceName, "MS Sans Serif") == 0 ||
			   lstrcmp(font.lfFaceName, "Helv") == 0;
	}

	return FALSE;
}

void
CFontsPrefs::FillEncodingNames()
{
	assert(m_lpIntlFont);
	if (m_lpIntlFont) {
		HWND	hwndCombo = GetDlgItem(m_hwndDlg, IDC_COMBO1);

		// NOTE: we retrieved the number of encodings in member function
		// Activate()
		for (DWORD i = 0; i < m_dwEncodings; i++) {
			LPOLESTR	lpoleName;
			LPCSTR		lpszName;

			m_lpIntlFont->GetEncodingName(i, &lpoleName);
			lpszName = AllocTaskAnsiString(lpoleName);  // UNICODE -> ANSI
			CoTaskMemFree(lpoleName);
			ComboBox_AddString(hwndCombo, lpszName);
		}
	}
}

BOOL
CFontsPrefs::InitDialog()
{
	// See if we should use the system font for the two font name
	// combo boxes
	if (UseSystemFont()) {
		HFONT	hFont = NULL;

#ifdef _WIN32
		hFont = GetStockFont(DEFAULT_GUI_FONT);
#endif

		if (!hFont)
			hFont = GetStockFont(SYSTEM_FONT);

		SetWindowFont(GetDlgItem(m_hwndDlg, IDC_COMBO2), hFont, FALSE);
		SetWindowFont(GetDlgItem(m_hwndDlg, IDC_COMBO4), hFont, FALSE);
	}

	// We don't want the height of the edit-field part of the owner draw
	// combo boxes to be less than the edit-field part of the encoding combo box
	m_nMaxFontHeight = (int)SendMessage(GetDlgItem(m_hwndDlg, IDC_COMBO1),
		CB_GETITEMHEIGHT, (WPARAM)(int)-1, 0);

	// Fill in the list of font sizes
	FillFontSizes(GetDlgItem(m_hwndDlg, IDC_COMBO3));
	FillFontSizes(GetDlgItem(m_hwndDlg, IDC_COMBO5));

	// Fill the combo box with the list of encoding names
	FillEncodingNames();

	// Update the font names and sizes for this encoding. This will
	// also fill the combo boxes that display the list of proportional
	// and fixed fonts from which to choose
	OnEncodingChanged();
	
	// Set the height of the selection field for the proportional and fixed
	// face combo boxes
	SendMessage(GetDlgItem(m_hwndDlg, IDC_COMBO2), CB_SETITEMHEIGHT, (WPARAM)(int)-1,
		(LPARAM)m_nMaxFontHeight);
	SendMessage(GetDlgItem(m_hwndDlg, IDC_COMBO4), CB_SETITEMHEIGHT, (WPARAM)(int)-1,
		(LPARAM)m_nMaxFontHeight);

	// See if the preference for using WebFonts is locked
	if (PREF_PrefIsLocked("browser.use_document_fonts")) {
		// Disable all the radio buttons in the group
		DisableRadioButtonGroup(IDC_RADIO1);
	}

	return CBrowserPropertyPage::InitDialog();
}

BOOL
CFontsPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	if (bSaveAndValidate) {
		CString	strFontSize;

		ComboBoxTransfer(IDC_COMBO3, strFontSize, bSaveAndValidate);
		m_nVariableSize = atoi((LPCSTR)strFontSize);

		ComboBoxTransfer(IDC_COMBO5, strFontSize, bSaveAndValidate);
		m_nFixedSize = atoi((LPCSTR)strFontSize);

	} else {
		char	szBuf[32];

		wsprintf(szBuf, "%i", m_nVariableSize);
		ComboBox_SelectString(GetDlgItem(m_hwndDlg, IDC_COMBO3), -1, szBuf);

		wsprintf(szBuf, "%i", m_nFixedSize);
		ComboBox_SelectString(GetDlgItem(m_hwndDlg, IDC_COMBO5), -1, szBuf);
	}

	ComboBoxTransfer(IDC_COMBO1, m_nCharset, bSaveAndValidate);
	ComboBoxTransfer(IDC_COMBO2, m_strVariableFaceName, bSaveAndValidate);
	ComboBoxTransfer(IDC_COMBO4, m_strFixedFaceName, bSaveAndValidate);
	RadioButtonTransfer(IDC_RADIO1, m_nUseDocumentFonts, bSaveAndValidate);
	return TRUE;
}

BOOL
CFontsPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDC_COMBO1 && notifyCode == CBN_SELCHANGE) {
		// Get the new encoding
		ComboBoxTransfer(IDC_COMBO1, m_nCharset, TRUE);

		// Update the font names and sizes for this encoding
		OnEncodingChanged();
		DoTransfer(FALSE);
		return TRUE;
	}

	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

#define LEFT_PADDING	2

void
CFontsPrefs::MeasureComboBoxItem(LPMEASUREITEMSTRUCT lpmis)
{
	LOGFONT		lf;
	int			nLen;
	SIZE		size;
	HWND		hwndCtl = GetDlgItem(m_hwndDlg, lpmis->CtlID);
	HFONT		hFont, hOldFont;
	HDC			hDC = GetDC(hwndCtl);
	TEXTMETRIC	tm;

	assert(lpmis->itemID != -1);
	if (lpmis->itemID == -1)
		return;
	
	// Initialize the LOGFONT structure
	memset(&lf, 0, sizeof(lf));

	// Get the font face name
	nLen = ComboBox_GetLBText(hwndCtl, lpmis->itemID, lf.lfFaceName);

	if (m_bDBCSEnabled) {
		// Just use the combo box font
		hFont = GetWindowFont(hwndCtl);

	} else {
		// Assume an 8-point font
		lf.lfHeight = (LONG)-MulDiv(8, GetDeviceCaps(hDC, LOGPIXELSY), 72);
		
		// Create a logical font for this face name
		lf.lfWeight = FW_NORMAL;
		lf.lfCharSet = DEFAULT_CHARSET;
		lf.lfQuality = PROOF_QUALITY;
		hFont = CreateFontIndirect(&lf);
	}
	assert(hFont);

	// Measure the font face name
	hOldFont = SelectFont(hDC, hFont);
#ifdef _WIN32
	GetTextExtentPoint32(hDC, lf.lfFaceName, nLen, &size);
#else
	GetTextExtentPoint(hDC, lf.lfFaceName, nLen, &size);
#endif
	lpmis->itemWidth = (UINT)size.cx;

	GetTextMetrics(hDC, &tm);
	lpmis->itemHeight = tm.tmHeight;

	// We need to keep track of the largest font height so we can size the
	// combo box accordingly
	if ((int)lpmis->itemHeight > m_nMaxFontHeight)
		m_nMaxFontHeight = (int)lpmis->itemHeight;

	// Cleanup
	SelectFont(hDC, hOldFont);
	if (!m_bDBCSEnabled)
		DeleteFont(hFont);
	ReleaseDC(hwndCtl, hDC);
}

void
CFontsPrefs::DrawComboBoxItem(LPDRAWITEMSTRUCT lpdis)
{
	LOGFONT	lf;
	HFONT	hFont, hOldFont;
	int		nLen;
    SIZE    size;
    int     y;

	switch (lpdis->itemAction) {
		case ODA_DRAWENTIRE:
        case ODA_SELECT:
			if (lpdis->itemID == (UINT)-1)
                break;

			// Set up the text and background colors
			SetTextColor(lpdis->hDC, GetSysColor(lpdis->itemState & ODS_SELECTED ?
				COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));

			SetBkColor(lpdis->hDC, GetSysColor(lpdis->itemState & ODS_SELECTED ?
				COLOR_HIGHLIGHT : COLOR_WINDOW));
			SetBkMode(lpdis->hDC, TRANSPARENT);
	
			// Initialize the LOGFONT structure
			memset(&lf, 0, sizeof(lf));
			
			// Get the string to display
			nLen = ComboBox_GetLBText(lpdis->hwndItem, lpdis->itemID, lf.lfFaceName);

			if (m_bDBCSEnabled) {
				hFont = GetWindowFont(lpdis->hwndItem);

			} else {
				// Assume an 8-point font
				lf.lfHeight = (LONG)-MulDiv(8, GetDeviceCaps(lpdis->hDC, LOGPIXELSY), 72);
				
				// Create the font
				lf.lfWeight = FW_NORMAL;
				lf.lfCharSet = DEFAULT_CHARSET;
				lf.lfQuality = PROOF_QUALITY;
				hFont = CreateFontIndirect(&lf);
			}
			assert(hFont);
			
			// Draw the text centered vertically in the bounding rect
			hOldFont = SelectFont(lpdis->hDC, hFont);
#ifdef _WIN32
            GetTextExtentPoint32(lpdis->hDC, lf.lfFaceName, nLen, &size);
#else
            GetTextExtentPoint(lpdis->hDC, lf.lfFaceName, nLen, &size);
#endif
            y = ((lpdis->rcItem.bottom - lpdis->rcItem.top) - size.cy) / 2;
			ExtTextOut(lpdis->hDC, lpdis->rcItem.left + LEFT_PADDING, lpdis->rcItem.top + y,
				ETO_OPAQUE, &lpdis->rcItem, lf.lfFaceName, nLen, NULL);

			// Cleanup the HDC
			SelectFont(lpdis->hDC, hOldFont);
			if (!m_bDBCSEnabled)
				DeleteFont(hFont);
			
			// Draw the focus rect if necessary
			if (lpdis->itemState & ODS_FOCUS)
				DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
			break;

		case ODA_FOCUS:
			DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
			break;
	}
}

LRESULT
CFontsPrefs::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPDRAWITEMSTRUCT	lpdis;
	LPMEASUREITEMSTRUCT	lpmis;

	switch (uMsg) {
		case WM_MEASUREITEM:
			lpmis = (LPMEASUREITEMSTRUCT)lParam;

			if (lpmis->CtlID == IDC_COMBO2 || lpmis->CtlID == IDC_COMBO4)
				MeasureComboBoxItem(lpmis);
			return TRUE;

		case WM_DRAWITEM:
			lpdis = (LPDRAWITEMSTRUCT)lParam;

			if (lpdis->CtlID == IDC_COMBO2 || lpdis->CtlID == IDC_COMBO4)
				DrawComboBoxItem(lpdis);
			return TRUE;
	}

	return CBrowserPropertyPage::WindowProc(uMsg, wParam, lParam);
}

// Apply changes using XP preferences
BOOL
CFontsPrefs::ApplyChanges()
{
	PREF_SetIntPref("browser.use_document_fonts", (int32)m_nUseDocumentFonts);
	
	// Request the IIntlFont interface from our data object
	assert(m_pObject);
	if (m_pObject) {
		LPINTLFONT	lpIntlFont;
		
		if (SUCCEEDED(m_pObject->QueryInterface(IID_IIntlFont, (void **)&lpIntlFont))) {
			ENCODINGINFO	info;
	
			info.nVariableWidthSize = m_nVariableSize;
			lstrcpy(info.szVariableWidthFont, (LPCSTR)m_strVariableFaceName);
			info.nFixedWidthSize = m_nFixedSize;
			lstrcpy(info.szFixedWidthFont, (LPCSTR)m_strFixedFaceName);
	
			lpIntlFont->SetEncodingFonts(m_nCharset, &info);
			lpIntlFont->Release();
		}
	}
	
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CColorsPrefs implementation

CColorsPrefs::CColorsPrefs()
	: CBrowserPropertyPage(IDD_COLORS, HELP_PREFS_APPEARANCE_COLORS)
{
}

// Initialize member data using XP preferences
STDMETHODIMP
CColorsPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		PREF_GetBoolPref("browser.underline_anchors", &m_bUnderlineLinks);
		PREF_GetColorPrefDWord("browser.anchor_color", &m_rgbUnvisitedLinks);
		PREF_GetColorPrefDWord("browser.visited_color", &m_rgbVisitedLinks);
		PREF_GetBoolPref("browser.wfe.use_windows_colors", &m_bUseWindowsColors);
		PREF_GetColorPrefDWord("browser.foreground_color", &m_rgbTextColor);
		PREF_GetColorPrefDWord("browser.background_color", &m_rgbBackgroundColor);
		
		BOOL	bUseDocumentColors;

		PREF_GetBoolPref("browser.use_document_colors", &bUseDocumentColors);
		m_bOverrideDocumentColors = !bUseDocumentColors;
	}

	return CBrowserPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CColorsPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	CheckBoxTransfer(IDC_CHECK1, m_bUseWindowsColors, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK2, m_bUnderlineLinks, bSaveAndValidate);
	CheckBoxTransfer(IDC_CHECK3, m_bOverrideDocumentColors, bSaveAndValidate);

	if (!bSaveAndValidate)
		EnableColorButtons();

	return TRUE;
}

void
CColorsPrefs::EnableColorButtons()
{
	BOOL	bUseWindowsColors;

	// Disable the foreground/background color buttons if they're
	// using the Windows colors
	CheckBoxTransfer(IDC_CHECK1, bUseWindowsColors, TRUE);

	EnableDlgItem(IDC_BUTTON1, !bUseWindowsColors);
	EnableDlgItem(IDC_BUTTON2, !bUseWindowsColors);
}

// Apply changes using XP preferences
BOOL
CColorsPrefs::ApplyChanges()
{
	PREF_SetBoolPref("browser.underline_anchors", m_bUnderlineLinks);
	PREF_SetColorPrefDWord("browser.anchor_color", m_rgbUnvisitedLinks);
	PREF_SetColorPrefDWord("browser.visited_color", m_rgbVisitedLinks);
	PREF_SetBoolPref("browser.wfe.use_windows_colors", m_bUseWindowsColors);
	PREF_SetColorPrefDWord("browser.foreground_color", m_rgbTextColor);
	PREF_SetColorPrefDWord("browser.background_color", m_rgbBackgroundColor);
	PREF_SetBoolPref("browser.use_document_colors", !m_bOverrideDocumentColors);
	return TRUE;
}

BOOL
CColorsPrefs::InitDialog()
{
	BOOL result = CBrowserPropertyPage::InitDialog();

	// Enable controls based on which prefs are locked.
	EnableDlgItem(IDC_CHECK2, !PREF_PrefIsLocked("browser.underline_anchors"));
	EnableDlgItem(IDC_BUTTON3, !PREF_PrefIsLocked("browser.anchor_color"));
	EnableDlgItem(IDC_BUTTON4, !PREF_PrefIsLocked("browser.visited_color"));
	EnableDlgItem(IDC_CHECK1, !PREF_PrefIsLocked("browser.wfe.use_windows_colors"));
	EnableDlgItem(IDC_BUTTON1, !PREF_PrefIsLocked("browser.foreground_color"));
	EnableDlgItem(IDC_BUTTON2, !PREF_PrefIsLocked("browser.background_color"));
	EnableDlgItem(IDC_CHECK3, !PREF_PrefIsLocked("browser.use_document_colors"));

	return result; 
}

COLORREF
CColorsPrefs::GetColorButtonColor(UINT nCtlID)
{
	if (nCtlID == IDC_BUTTON1)
		return m_rgbTextColor;
	else if (nCtlID == IDC_BUTTON2)
		return m_rgbBackgroundColor;
	else if (nCtlID == IDC_BUTTON3)
		return m_rgbUnvisitedLinks;
	else if (nCtlID == IDC_BUTTON4)
		return m_rgbVisitedLinks;

	assert(FALSE);
	return 0;
}

void
CColorsPrefs::SetColorButtonColor(UINT nCtlID, COLORREF cr)
{
	if (nCtlID == IDC_BUTTON1)
		m_rgbTextColor = cr;
	else if (nCtlID == IDC_BUTTON2)
		m_rgbBackgroundColor = cr;
	else if (nCtlID == IDC_BUTTON3)
		m_rgbUnvisitedLinks = cr;
	else if (nCtlID == IDC_BUTTON4)
		m_rgbVisitedLinks = cr;
	else
		assert(FALSE);
}

LRESULT
CColorsPrefs::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HBRUSH	hBrush;

	switch (uMsg) {
		case WM_DRAWITEM:
			hBrush = CreateSolidBrush(GetColorButtonColor(((LPDRAWITEMSTRUCT)lParam)->CtlID));
			DrawColorButtonControl((LPDRAWITEMSTRUCT)lParam, hBrush);
			DeleteBrush(hBrush);
			return TRUE;

		default:
			return CBrowserPropertyPage::WindowProc(uMsg, wParam, lParam);
	}
}

BOOL
CColorsPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	switch (id) {
		case IDC_CHECK1:
			if (notifyCode == BN_CLICKED)
				EnableColorButtons();
			break;

		case IDC_BUTTON1:
		case IDC_BUTTON2:
		case IDC_BUTTON3:
		case IDC_BUTTON4:
			if (notifyCode == BN_CLICKED) {
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

				if (ChooseColor(&cc) == IDOK) {
					SetColorButtonColor(id, cc.rgbResult);
					InvalidateRect(hwndCtl, NULL, FALSE);
				}

				return TRUE;
			}
			break;
	}

	return CBrowserPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}

