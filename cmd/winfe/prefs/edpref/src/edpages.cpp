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
#include "structs.h"
#include "xp_core.h"
#include "xp_help.h"
#include "prefapi.h"

/////////////////////////////////////////////////////////////////////////////
// Helper functions

static void NEAR
EnableDlgItem(HWND hdlg, UINT nID, BOOL bEnable)
{
	HWND hCtl = GetDlgItem(hdlg, nID);

	assert(hCtl);
	EnableWindow(hCtl, bEnable);
}

/////////////////////////////////////////////////////////////////////////////
// CEditorPrefs implementation

CEditorPrefs::CEditorPrefs()
	: CEditorPropertyPage(IDD_PREF_EDITOR, HELP_EDIT_PREFS_EDITOR_GENERAL)
{
}

STDMETHODIMP
CEditorPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
	    PREF_GetStringPref("editor.author",m_strAuthor);
	    PREF_GetStringPref("editor.html_editor",m_strHTML_Editor);
	    PREF_GetStringPref("editor.image_editor",m_strImageEditor);
        int32 n;
        PREF_GetIntPref("editor.fontsize_mode", &n);
        m_iFontSizeMode = (int)n;

	    PREF_GetIntPref("editor.auto_save_delay",&n);
        m_iAutoSaveMinutes = (int)n;

        m_bAutoSave = n > 0;
	}

	return CEditorPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CEditorPrefs::InitDialog()
{
	// Composer preferences
	EnableDlgItem(IDC_HTML_EDITOR, !PREF_PrefIsLocked("editor.html_editor"));
	EnableDlgItem(IDC_CHOOSE_HTML_EDITOR, !PREF_PrefIsLocked("editor.html_editor"));
	EnableDlgItem(IDC_IMAGE_EDITOR, !PREF_PrefIsLocked("editor.image_editor"));
	EnableDlgItem(IDC_CHOOSE_IMAGE_EDITOR, !PREF_PrefIsLocked("editor.image_editor"));
	EnableDlgItem(IDC_AUTO_SAVE, !PREF_PrefIsLocked("editor.auto_save"));
	EnableDlgItem(IDC_AUTO_SAVE_MINUTES, !PREF_PrefIsLocked("editor.auto_save_delay"));
	EnableDlgItem(IDC_FONTSIZE_MODE, !PREF_PrefIsLocked("editor.fontsize_mode"));
	EnableDlgItem(IDC_FONTSIZE_MODE2, !PREF_PrefIsLocked("editor.fontsize_mode"));
	EnableDlgItem(IDC_FONTSIZE_MODE3, !PREF_PrefIsLocked("editor.fontsize_mode"));
	
	return CEditorPropertyPage::InitDialog();;
}

BOOL
CEditorPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	EditFieldTransfer(IDC_EDIT_AUTHOR, m_strAuthor, bSaveAndValidate);
	EditFieldTransfer(IDC_HTML_EDITOR, m_strHTML_Editor, bSaveAndValidate);
	EditFieldTransfer(IDC_IMAGE_EDITOR, m_strImageEditor, bSaveAndValidate);
	CheckBoxTransfer(IDC_AUTO_SAVE, m_bAutoSave, bSaveAndValidate);
	RadioButtonTransfer(IDC_FONTSIZE_MODE, m_iFontSizeMode, bSaveAndValidate);
	EditFieldTransfer(IDC_AUTO_SAVE_MINUTES, m_iAutoSaveMinutes, bSaveAndValidate);
	return TRUE;
}

BOOL
CEditorPrefs::ApplyChanges()
{
    // TODO: Need TrimLeft() and TrimRight() or just Trim() functions for CString
    PREF_SetCharPref("editor.author",LPCSTR(m_strAuthor));
	PREF_SetCharPref("editor.html_editor",LPCSTR(m_strHTML_Editor));
	PREF_SetCharPref("editor.image_editor",LPCSTR(m_strImageEditor));
    
    if ( m_bAutoSave ){
		PREF_SetIntPref("editor.auto_save_delay",m_iAutoSaveMinutes);
    } else {
		PREF_SetIntPref("editor.auto_save_delay",0);
    }
    PREF_SetIntPref("editor.fontsize_mode", (int32)m_iFontSizeMode);
    return TRUE;
}

BOOL
CEditorPrefs::GetApplicationFilename(CString& strFilename, UINT nIDCaption)
{
    OPENFILENAME fname;

    // Dialog caption
    CString  strCaption;
    strCaption.LoadString(m_hInstance, nIDCaption);

    // space for the full path name    
    char szFullPath[_MAX_PATH];
    szFullPath[0] = '\0';

    // Filter string
    char     szFilter[256];
    ::LoadString(m_hInstance, IDS_APP_FILTER, szFilter, 256);
    char * szTemp = szFilter;
    // Replace '\n' with '\0' in filter
    while( *szTemp != '\0' ) {
        if( *szTemp == '\n' ) *szTemp = '\0';
        szTemp++;
    }
    
    // set up the entries
    fname.lStructSize = sizeof(OPENFILENAME);
    fname.hwndOwner = m_hwndDlg;
    fname.lpstrFilter = szFilter;
    fname.lpstrCustomFilter = NULL;
    fname.nFilterIndex = 0;
    fname.lpstrFile = szFullPath;
    fname.nMaxFile = _MAX_PATH;
    fname.lpstrFileTitle = NULL;
    fname.nMaxFileTitle = 0;
    fname.lpstrInitialDir = NULL;
    fname.lpstrTitle = LPCSTR(strCaption);
    fname.lpstrDefExt = NULL;
    fname.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
    if( GetOpenFileName(&fname) ){
        strFilename = szFullPath;
        return TRUE;
    }
    return FALSE;
}

BOOL 
CEditorPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
    // Get current data from dialog
    if(notifyCode == BN_CLICKED){
        DoTransfer(TRUE);

	    if (id == IDC_CHOOSE_HTML_EDITOR &&
            GetApplicationFilename(m_strHTML_Editor, IDS_CHOOSE_HTML_EDITOR) ){
		    DoTransfer(FALSE);
		    return TRUE;
	    } else if (id == IDC_CHOOSE_IMAGE_EDITOR &&
            GetApplicationFilename(m_strImageEditor, IDS_CHOOSE_IMAGE_EDITOR) ){
		    DoTransfer(FALSE);
		    return TRUE;
        }
    }
	return CEditorPropertyPage::OnCommand(id, hwndCtl, notifyCode);
}


/////////////////////////////////////////////////////////////////////////////
// CPublishPrefs implementation

CPublishPrefs::CPublishPrefs()
	: CEditorPropertyPage(IDD_PREF_PUBLISH, HELP_EDIT_PREFS_EDITOR_PUBLISH)
{
}

BOOL
CPublishPrefs::InitDialog()
{

	// Composer/Publishing Preferences
	EnableDlgItem(IDC_AUTOADJUST_LINKS, !PREF_PrefIsLocked("editor.publish_keep_links"));
	EnableDlgItem(IDC_KEEP_IMAGE_WITH_DOC, !PREF_PrefIsLocked("editor.publish_keep_images"));
	EnableDlgItem(IDC_PUBLISH_FTP, !PREF_PrefIsLocked("editor.publish_location"));
	EnableDlgItem(IDC_PUBLISH_HTTP, !PREF_PrefIsLocked("editor.publish_browse_location"));
	
	return CEditorPropertyPage::InitDialog();;
}

STDMETHODIMP
CPublishPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		PREF_GetBoolPref("editor.publish_keep_links",&m_bAutoAdjustLinks);
    	PREF_GetBoolPref("editor.publish_keep_images",&m_bKeepImagesWithDoc);
	    PREF_GetStringPref("editor.publish_location",m_strPublishLocation);
	    PREF_GetStringPref("editor.publish_browse_location",m_strBrowseLocation);
	}

	return CEditorPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CPublishPrefs::DoTransfer(BOOL bSaveAndValidate)
{
    CheckBoxTransfer(IDC_AUTOADJUST_LINKS, m_bAutoAdjustLinks, bSaveAndValidate);
    CheckBoxTransfer(IDC_KEEP_IMAGE_WITH_DOC, m_bKeepImagesWithDoc, bSaveAndValidate);
	EditFieldTransfer(IDC_PUBLISH_FTP, m_strPublishLocation, bSaveAndValidate);
	EditFieldTransfer(IDC_PUBLISH_HTTP, m_strBrowseLocation, bSaveAndValidate);
	return TRUE;
}

BOOL
CPublishPrefs::ApplyChanges()
{
    PREF_SetBoolPref("editor.publish_keep_links",(XP_Bool)m_bAutoAdjustLinks);
    PREF_SetBoolPref("editor.publish_keep_images",(XP_Bool)m_bKeepImagesWithDoc);
    PREF_SetCharPref("editor.publish_location",LPCSTR(m_strPublishLocation));
	PREF_SetCharPref("editor.publish_browse_location",LPCSTR(m_strBrowseLocation));
	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
// CEditorPrefs2 implementation

CEditorPrefs2::CEditorPrefs2()
	: CEditorPropertyPage(IDD_PREF_EDITOR2, HELP_EDIT_PREFS_EDITOR_PUBLISH)
{
}

BOOL
CEditorPrefs2::InitDialog()
{
	// Composer/Publishing Preferences
    EnableDlgItem(IDC_PAGEUPDOWN, !PREF_PrefIsLocked("editor.page_updown_move_cursor"));
	return CEditorPropertyPage::InitDialog();;
}

STDMETHODIMP
CEditorPrefs2::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
		PREF_GetBoolPref("editor.page_updown_move_cursor",&m_bUpDownMoveCursor);
	}

	return CEditorPropertyPage::Activate(hwndParent, lprc, bModal);
}

BOOL
CEditorPrefs2::DoTransfer(BOOL bSaveAndValidate)
{
    CheckBoxTransfer(IDC_PAGEUPDOWN, m_bUpDownMoveCursor, bSaveAndValidate);
	return TRUE;
}

BOOL
CEditorPrefs2::ApplyChanges()
{
    PREF_SetBoolPref("editor.page_updown_move_cursor",(XP_Bool)m_bUpDownMoveCursor);
	return TRUE;
}

