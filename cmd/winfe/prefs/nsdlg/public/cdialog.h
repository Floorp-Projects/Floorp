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

#ifndef __CDIALOG_H_
#define __CDIALOG_H_

#include "cstring.h"

// Simple class to encapsulate dialog behavior (modal and modeless)
class CDialog {
	public:
		CDialog(HINSTANCE hInstance, UINT nTemplateID);

		// Create a modeless dialog
		BOOL	Create(HWND hwndOwner);

		// Create a modal dialog
		int		DoModal(HWND hwndOwner);

		// Destroy the window
		BOOL	DestroyWindow();

	protected:
		// Event processing
		virtual BOOL	InitDialog();
		virtual BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

		// Called from member function OnCommand(). OnOK() calls DoTransfer
		// to transfer data from the dialog box. Both member functions call
		// EndDialog() to destroy the dialog box. OnOK() only destroys the
		// dialog box if validation is successful
		virtual void	OnOK();
		virtual void	OnCancel();

		// Dialog data transfer. Override this routine and transfer data to/from
		// the dialog box. Called from member functions InitDialog() and OnOK()
		//
		// When transfering data from the dialog box, return FALSE if validation
		// is unsuccessful
		virtual BOOL	DoTransfer(BOOL bSaveAndValidate) = 0;
		
		// Override this to handle other events yourself. This routine does two
		// things:
		//   - calls the original window procedure (which is DefDlgProc)
		//   - handles WM_COMMAND, splits out the parameters, and calls
		//     member function OnCommand
		virtual	LRESULT	WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

		// Helper functions to assist with data transfer. For radio button
		// transfers, the ID must be the first in a group of auto radio buttons.
		// When transferring a LPSTR from an edit field, space for the string is
		// malloc'd and the caller must free; any existing LPSTR is free'd first
		void	RadioButtonTransfer(int nIDButton, int &value, BOOL bSaveAndValidate);
		void	CheckBoxTransfer(int nIDButton, BOOL &value, BOOL bSaveAndValidate);
		void	EditFieldTransfer(int nIDEdit, CString &str, BOOL bSaveAndValidate);
		void	EditFieldTransfer(int nIDEdit, int &value, BOOL bSaveAndValidate);
		void	EditFieldTransfer(int nIDEdit, UINT &value, BOOL bSaveAndValidate);
		void	ListBoxTransfer(int nIDList, int &index, BOOL bSaveAndValidate);
		void	ListBoxTransfer(int nIDList, CString &str, BOOL bSaveAndValidate);
		void	ComboBoxTransfer(int nIDCombo, int &index, BOOL bSaveAndValidate);
		void	ComboBoxTransfer(int nIDCombo, CString &str, BOOL bSaveAndValidate);

		// Helper routine to enable/disable a control by ID
		void	EnableDlgItem(UINT nIDCtl, BOOL bEnable);
	
	protected:
		HINSTANCE	m_hInstance;
		UINT		m_nTemplateID;
		HWND		m_hwndDlg;

	private:
		WNDPROC	m_wpOriginalProc;	
		friend LRESULT CALLBACK WindowFunc(HWND, UINT, WPARAM, LPARAM);
		friend BOOL CALLBACK DialogFunc(HWND, UINT, WPARAM, LPARAM);
};

#endif /* __CDIALOG_H_ */

