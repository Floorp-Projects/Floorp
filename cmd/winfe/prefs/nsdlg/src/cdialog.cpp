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
#ifndef _WIN32
#include <stdlib.h>
#endif
#include <assert.h>
#include "cdialog.h"

// This is the window procedure that we specified when we subclassed the
// dialog. This routine is called to process messages sent to the dialog,
// and is called before DefDlgproc is called
LRESULT CALLBACK
WindowFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CDialog	*pDialog;
	LRESULT	 result;

	// Get the "this" pointer for the dialog, which we saved away in
	// the dialog specific user data
	pDialog = (CDialog*)GetWindowLong(hwndDlg, DWL_USER);
	assert(pDialog);
	assert(pDialog->m_hwndDlg == hwndDlg);

	// Dispatch the message to the dialog object
	result = pDialog->WindowProc(uMsg, wParam, lParam);

	if (uMsg == WM_DESTROY) {
		// Stop subclassing the window
		SubclassWindow(hwndDlg, pDialog->m_wpOriginalProc);
		pDialog->m_wpOriginalProc = NULL;
	}

	return result;
}

// Callback function for processing messages sent to a dialog box.
// The dialog box function is called by the Windows dialog box
// window procedure DefDlgProc
BOOL CALLBACK
DialogFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_INITDIALOG) {
		CDialog	*pDialog;

		// The lParam is the "this" pointer
		pDialog = (CDialog*)lParam;
		pDialog->m_hwndDlg = hwndDlg;

		// Save the "this" pointer in dialog specific user data
		SetWindowLong(hwndDlg, DWL_USER, (LONG)pDialog);

		// Subclass the window
		assert(pDialog->m_wpOriginalProc == NULL);
		pDialog->m_wpOriginalProc = SubclassWindow(hwndDlg, WindowFunc);
		assert(pDialog->m_wpOriginalProc);

		// Let the dialog handle WM_INITDIALOG
		return pDialog->InitDialog();
	}

	// WM_INITDIALOG is the only message we handle here. All others are
	// handled in the window procedure we passed in when we subclassed
	// the window
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CDialog implementation

CDialog::CDialog(HINSTANCE hInstance, UINT nTemplateID)
{
	m_hInstance = hInstance;
	m_nTemplateID = nTemplateID;
	m_hwndDlg = NULL;
	m_wpOriginalProc = NULL;
}

BOOL
CDialog::Create(HWND hwndOwner)
{
	assert(m_hwndDlg == NULL);
	return CreateDialogParam(m_hInstance, MAKEINTRESOURCE(m_nTemplateID), hwndOwner,
		(DLGPROC)DialogFunc, (LPARAM)this) != NULL;
}

int
CDialog::DoModal(HWND hwndOwner)
{
	int	nResult;

	assert(m_hwndDlg == NULL);
	nResult = DialogBoxParam(m_hInstance, MAKEINTRESOURCE(m_nTemplateID), hwndOwner,
		(DLGPROC)DialogFunc, (LPARAM)this);
	m_hwndDlg = NULL;
	return nResult;
}

BOOL
CDialog::DestroyWindow()
{
	assert(m_hwndDlg);
	if (::DestroyWindow(m_hwndDlg)) {
		m_hwndDlg = NULL;
		return TRUE;
	}

	return FALSE;
}

BOOL
CDialog::InitDialog()
{
	// Transfer data to the dialog
	DoTransfer(FALSE);
	return TRUE;
}

BOOL
CDialog::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	if (id == IDOK) {
		OnOK();
		return TRUE;

	} else if (id == IDCANCEL) {
		OnCancel();
		return TRUE;
	}

	return FALSE;
}

void
CDialog::OnOK()
{
	if (DoTransfer(TRUE))
		EndDialog(m_hwndDlg, IDOK);
}

void
CDialog::OnCancel()
{
	EndDialog(m_hwndDlg, IDCANCEL);
}

void
CDialog::EnableDlgItem(UINT nCtlID, BOOL bEnable)
{
	HWND hwndCtl = GetDlgItem(m_hwndDlg, nCtlID);

	assert(hwndCtl);
	EnableWindow(hwndCtl, bEnable);
}

// Data transfer for radio buttons. The ID must be the first in a group of
// auto radio buttons
void
CDialog::RadioButtonTransfer(int nIDButton, int &value, BOOL bSaveAndValidate)
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, nIDButton);

	assert(GetWindowStyle(hwndCtl) & WS_GROUP);
	assert(SendMessage(hwndCtl, WM_GETDLGCODE, 0, 0) & DLGC_RADIOBUTTON);
	
	if (bSaveAndValidate)
		value = -1;  // value if no radio button is set

	// Walk all the controls in the group. Stop when we get to the start of another group
	// or there are no controls left
	int iButton = 0;
	do {
		if (SendMessage(hwndCtl, WM_GETDLGCODE, 0, 0) & DLGC_RADIOBUTTON) {
			// Control is a radio button
			if (bSaveAndValidate) {
				if (Button_GetCheck(hwndCtl) != 0) {
					assert(value == -1);  // only one button in the group should be set
					value = iButton;
				}
			} else {
				// Set the button state
				Button_SetCheck(hwndCtl, iButton == value);
			}

			iButton++;
#ifdef _DEBUG
		} else {
			OutputDebugString("Warning: skipping non-radio button in group.\n");
#endif
		}

		hwndCtl = GetNextSibling(hwndCtl);

	} while (hwndCtl && !(GetWindowStyle(hwndCtl) & WS_GROUP));
}

// Data transfer for a check box
void
CDialog::CheckBoxTransfer(int nIDButton, BOOL &value, BOOL bSaveAndValidate)
{
	if (bSaveAndValidate)
		value = IsDlgButtonChecked(m_hwndDlg, nIDButton);
	else
		CheckDlgButton(m_hwndDlg, nIDButton, value);
}

// Data transfer for a edit field. Transfers the value as a CString
void
CDialog::EditFieldTransfer(int nIDEdit, CString &str, BOOL bSaveAndValidate)
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, nIDEdit);

	assert(hwndCtl);
	if (bSaveAndValidate)
		str.GetWindowText(hwndCtl);
	else
		Edit_SetText(hwndCtl, (const char *)str);
}

// Data transfer for a edit field. Transfers the value as an integer
void
CDialog::EditFieldTransfer(int nIDEdit, int &value, BOOL bSaveAndValidate)
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, nIDEdit);
	char	szBuf[32];

	assert(hwndCtl);
	if (bSaveAndValidate) {
		Edit_GetText(hwndCtl, szBuf, sizeof(szBuf));
		value = atoi(szBuf);

	} else {
		wsprintf(szBuf, "%i", value);
		Edit_SetText(hwndCtl, szBuf);
	}
}

// Data transfer for a edit field. Transfers the value as an unsigned integer
void
CDialog::EditFieldTransfer(int nIDEdit, UINT &value, BOOL bSaveAndValidate)
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, nIDEdit);
	char	szBuf[32];

	assert(hwndCtl);
	if (bSaveAndValidate) {
		Edit_GetText(hwndCtl, szBuf, sizeof(szBuf));
		value = (UINT)strtoul(szBuf, NULL, 10);

	} else {
		wsprintf(szBuf, "%u", value);
		Edit_SetText(hwndCtl, szBuf);
	}
}

void
CDialog::ListBoxTransfer(int nIDList, int &index, BOOL bSaveAndValidate)
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, nIDList);

	if (bSaveAndValidate)
		index = ListBox_GetCurSel(hwndCtl);
	else
		ListBox_SetCurSel(hwndCtl, index);
}

void
CDialog::ListBoxTransfer(int nIDList, CString &str, BOOL bSaveAndValidate)
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, nIDList);

	if (bSaveAndValidate) {
		int	nIndex = ListBox_GetCurSel(hwndCtl);

		if (nIndex == -1)
			str.Empty();
		else
			ListBox_GetText(hwndCtl, nIndex,
				str.BufferSetLength(ListBox_GetTextLen(hwndCtl, nIndex)));
	
	} else {
		// Set the current selection
		if (ListBox_SelectString(hwndCtl, -1, (LPCSTR)str) == LB_ERR) {
#ifdef _DEBUG
			OutputDebugString("Warning: no listbox item selected.\n");
#endif
		}
	}
}

void
CDialog::ComboBoxTransfer(int nIDCombo, int &index, BOOL bSaveAndValidate)
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, nIDCombo);

	if (bSaveAndValidate)
		index = ComboBox_GetCurSel(hwndCtl);
	else
		ComboBox_SetCurSel(hwndCtl, index);
}

void
CDialog::ComboBoxTransfer(int nIDCombo, CString &str, BOOL bSaveAndValidate)
{
	HWND	hwndCtl = GetDlgItem(m_hwndDlg, nIDCombo);

	if (bSaveAndValidate) {
        int nLen = ComboBox_GetTextLength(hwndCtl);

        if (nLen == -1) {
            // GetWindowTextLength() doesn't work for drop down combo boxes
            // on Win 3.1, so assume a max of 255 characters
            nLen = 255;
        }
        
        ComboBox_GetText(hwndCtl, str.BufferSetLength(nLen), nLen + 1);
	
    } else if (ComboBox_SelectString(hwndCtl, -1, (LPARAM)(LPCSTR)str) == CB_ERR) {
		SetWindowText(hwndCtl, (LPCSTR)str);  // just set the edit text
    }
}

LRESULT
CDialog::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_COMMAND:
			if (OnCommand(GET_WM_COMMAND_ID(wParam, lParam),
						  GET_WM_COMMAND_HWND(wParam, lParam),
						  GET_WM_COMMAND_CMD(wParam, lParam)))
				return 0;  // command was handled
			break;

		default:
			break;
	}

	// Call the original window procedure
	assert(m_wpOriginalProc);
	return CallWindowProc((FARPROC)m_wpOriginalProc, m_hwndDlg, uMsg, wParam, lParam);
}

