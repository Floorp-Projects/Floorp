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
#include "prefapi.h"
#include "structs.h"
#include "xp_help.h"

#include "nsIDefaultBrowser.h"

/////////////////////////////////////////////////////////////////////////////
// CBasicWindowsPrefs implementation
CBasicWindowsPrefs::CBasicWindowsPrefs(nsIDefaultBrowser*pDefaultBrowser)
	: CPropertyPageEx(CComDll::m_hInstance, IDD_BASICWINPREFS, "" ),
      m_pDefaultBrowser( pDefaultBrowser ) {
	// Set member data using XP preferences
    assert( m_pDefaultBrowser );
}

CBasicWindowsPrefs::~CBasicWindowsPrefs() {
    m_pDefaultBrowser->Release();
}

BOOL CBasicWindowsPrefs::InitDialog()
{
	return CPropertyPageEx::InitDialog();
}

STDMETHODIMP CBasicWindowsPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
        // Get user preferences and remember them.
        nsresult result = m_pDefaultBrowser->GetPreferences( m_originalPrefs );
        assert( result == NS_OK );

        BOOL bIgnore = FALSE;
        PREF_GetBoolPref( "browser.wfe.ignore_def_check", &bIgnore );
        m_bCheckOnStart = !bIgnore;
    
        m_currentPrefs = m_originalPrefs;
	} 

	return CPropertyPageEx::Activate(hwndParent, lprc, bModal);
}

BOOL CBasicWindowsPrefs::DoTransfer(BOOL bSaveAndValidate)
{
    // Synchronize dialog checkboxes from current preferences.
    CheckBoxTransfer( IDC_HANDLEFILES, m_currentPrefs.bHandleFiles, bSaveAndValidate );
    CheckBoxTransfer( IDC_HANDLESHORTCUTS, m_currentPrefs.bHandleShortcuts, bSaveAndValidate );
    CheckBoxTransfer( IDC_INTEGRATEWITHACTIVEDESKTOP, m_currentPrefs.bIntegrateWithActiveDesktop, bSaveAndValidate );
    CheckBoxTransfer( IDC_CHECKONSTART, m_bCheckOnStart, bSaveAndValidate );

	return TRUE;
}

BOOL CBasicWindowsPrefs::ApplyChanges()
{
    // Set "ignore on start" pref based on "check on start" status.
    BOOL bIgnore = !m_bCheckOnStart;
    PREF_SetBoolPref( "browser.wfe.ignore_def_check", bIgnore );

    // If other prefs changed, store them.
    if ( XP_MEMCMP( &m_currentPrefs, &m_originalPrefs, sizeof( nsIDefaultBrowser::Prefs ) ) != 0 ) {
        // Store them in registry.
        nsresult result = m_pDefaultBrowser->SetPreferences( m_currentPrefs );
        assert( result == NS_OK );

        // Apply those preferences.
        result = m_pDefaultBrowser->HandlePerPreferences();
        assert( result == NS_OK );
    }

	return TRUE;
}

LRESULT	CBasicWindowsPrefs::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return CPropertyPageEx::WindowProc(uMsg,wParam,lParam);
}

class CWinPrefDialog : public CDialog {
	public:
		CWinPrefDialog( int id, nsIDefaultBrowser::Prefs &prefs );

	protected:
		BOOL	DoTransfer(BOOL bSaveAndValidate);
		
	private:
        nsIDefaultBrowser::Prefs &m_prefs;
};

CWinPrefDialog::CWinPrefDialog( int id, nsIDefaultBrowser::Prefs &prefs )
    : CDialog( CComDll::m_hInstance, id ), m_prefs( prefs ) {
}

CWinPrefDialog::DoTransfer( BOOL bSaveAndValidate ) {
    switch( m_nTemplateID ) {
        case IDD_WINFILETYPES:
            CheckBoxTransfer( IDC_HANDLEHTML, m_prefs.bHandleHTML, bSaveAndValidate );
            CheckBoxTransfer( IDC_HANDLEJPEG, m_prefs.bHandleJPEG, bSaveAndValidate );
            CheckBoxTransfer( IDC_HANDLEGIF, m_prefs.bHandleGIF, bSaveAndValidate );
            CheckBoxTransfer( IDC_HANDLEJS, m_prefs.bHandleJS, bSaveAndValidate );
            CheckBoxTransfer( IDC_HANDLEXBM, m_prefs.bHandleXBM, bSaveAndValidate );
            CheckBoxTransfer( IDC_HANDLETXT, m_prefs.bHandleTXT, bSaveAndValidate );
            break;

        case IDD_WINSHORTCUTTYPES:
            CheckBoxTransfer( IDC_HANDLEHTTP, m_prefs.bHandleHTTP, bSaveAndValidate );
            CheckBoxTransfer( IDC_HANDLESHTTP, m_prefs.bHandleHTTPS, bSaveAndValidate );
            CheckBoxTransfer( IDC_HANDLEFTP, m_prefs.bHandleFTP, bSaveAndValidate );
            break;
    
        case IDD_WINACTIVEDESKTOPSETTINGS:
            CheckBoxTransfer( IDC_USEINTERNETKEYWORDS, m_prefs.bUseInternetKeywords, bSaveAndValidate );
            CheckBoxTransfer( IDC_USENETCENTERSEARCH, m_prefs.bUseNetcenterSearch, bSaveAndValidate );
            CheckBoxTransfer( IDC_DISABLEACTIVEDESKTOP, m_prefs.bDisableActiveDesktop, bSaveAndValidate );
            break;

        default:
            break;
    }
    return TRUE;
}

BOOL CBasicWindowsPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
    BOOL result = TRUE;
    if ( notifyCode == BN_CLICKED ) {
        switch ( id ) {
            case IDD_WINFILETYPES:
            case IDD_WINSHORTCUTTYPES:
            case IDD_WINACTIVEDESKTOPSETTINGS:
                {
                    CWinPrefDialog dialog( id, m_currentPrefs );
                    dialog.DoModal(GetParent(m_hwndDlg));
                }
                break;
    
            default:
                result = CPropertyPageEx::OnCommand( id, hwndCtl, notifyCode );
                break;
        }
    } else {
        result = CPropertyPageEx::OnCommand( id, hwndCtl, notifyCode );
    }
	return result;
}
