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

/////////////////////////////////////////////////////////////////////////////
// CBasicWindowsPrefs implementation
CBasicWindowsPrefs::CBasicWindowsPrefs()
	: CPropertyPageEx(CComDll::m_hInstance, IDD_BASICWINPREFS, HELP_PREFS_BASICWINPREFS)
{
	// Set member data using XP preferences
}

BOOL CBasicWindowsPrefs::InitDialog()
{
	// Check for locked preferences

	return CPropertyPageEx::InitDialog();
}

STDMETHODIMP CBasicWindowsPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
	} 

	return CPropertyPageEx::Activate(hwndParent, lprc, bModal);
}

BOOL CBasicWindowsPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	return TRUE;
}

BOOL CBasicWindowsPrefs::ApplyChanges()
{
	return TRUE;
}

LRESULT	CBasicWindowsPrefs::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return CPropertyPageEx::WindowProc(uMsg,wParam,lParam);
}

BOOL CBasicWindowsPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	return CPropertyPageEx::OnCommand(id, hwndCtl, notifyCode);
}

/////////////////////////////////////////////////////////////////////////////
// CAdvancedWindowsPrefs implementation
CAdvancedWindowsPrefs::CAdvancedWindowsPrefs()
	: CPropertyPageEx(CComDll::m_hInstance, IDD_ADVANCEDWINPREFS, HELP_PREFS_ADVANCEDWINPREFS)
{
	// Set member data using XP preferences
}

BOOL CAdvancedWindowsPrefs::InitDialog()
{
	// Check for locked preferences

	return CPropertyPageEx::InitDialog();
}

STDMETHODIMP CAdvancedWindowsPrefs::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	if (!m_bHasBeenActivated) {
	} 

	return CPropertyPageEx::Activate(hwndParent, lprc, bModal);
}

BOOL CAdvancedWindowsPrefs::DoTransfer(BOOL bSaveAndValidate)
{
	return TRUE;
}

BOOL CAdvancedWindowsPrefs::ApplyChanges()
{
	return TRUE;
}

LRESULT	CAdvancedWindowsPrefs::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return CPropertyPageEx::WindowProc(uMsg,wParam,lParam);
}

BOOL CAdvancedWindowsPrefs::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	return CPropertyPageEx::OnCommand(id, hwndCtl, notifyCode);
}
