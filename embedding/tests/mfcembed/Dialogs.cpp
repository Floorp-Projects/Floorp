/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com> 
 */

#include "stdafx.h"
#include "Dialogs.h"

// File overview....
//
// Mainly contains dialog box code to support Alerts, Prompts such as
// Password prompt and username/password prompts
//

//--------------------------------------------------------------------------//
//				CPromptDialog Stuff
//--------------------------------------------------------------------------//

CPromptDialog::CPromptDialog(CWnd* pParent, const char* pTitle, const char* pText, const char* pDefEditText)
    : CDialog(CPromptDialog::IDD, pParent)
{   
	if(pTitle)
		m_csDialogTitle = pTitle;
	if(pText)
		m_csPromptText = pText;
	if(pDefEditText)
		m_csPromptAnswer = pDefEditText;
}

void CPromptDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CPromptDialog)
    DDX_Text(pDX, IDC_PROMPT_ANSWER, m_csPromptAnswer);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPromptDialog, CDialog)
    //{{AFX_MSG_MAP(CPromptDialog)
		// NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CPromptDialog::OnInitDialog()
{   
   	SetWindowText(m_csDialogTitle);
  
	CWnd *pWnd = GetDlgItem(IDC_PROMPT_TEXT);
	if(pWnd)
		pWnd->SetWindowText(m_csPromptText);

	CEdit *pEdit = (CEdit *)GetDlgItem(IDC_PROMPT_ANSWER);
	if(pEdit) 
	{
		pEdit->SetWindowText(m_csPromptAnswer);
		pEdit->SetFocus();
		pEdit->SetSel(0, -1);

		return 0; // Returning "0" since we're explicitly setting focus
	}

	return TRUE;
}

//--------------------------------------------------------------------------//
//				CPromptPasswordDialog Stuff
//--------------------------------------------------------------------------//

CPromptPasswordDialog::CPromptPasswordDialog(CWnd* pParent, const char* pTitle, const char* pText, const char* pCheckText)
    : CDialog(CPromptPasswordDialog::IDD, pParent)
{   
	if(pTitle)
		m_csDialogTitle = pTitle;
	if(pText)
		m_csPromptText = pText;
	if(pCheckText)
		m_csCheckBoxText = pCheckText;

	m_csPassword = "";
	m_bSavePassword = PR_FALSE;
}

void CPromptPasswordDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CPromptPasswordDialog)
    DDX_Text(pDX, IDC_PASSWORD, m_csPassword);
	DDX_Check(pDX, IDC_CHECK_SAVE_PASSWORD, m_bSavePassword);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPromptPasswordDialog, CDialog)
    //{{AFX_MSG_MAP(CPromptPasswordDialog)
		// NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CPromptPasswordDialog::OnInitDialog()
{   
   	SetWindowText(m_csDialogTitle);
  
	CWnd *pWnd = GetDlgItem(IDC_PROMPT_TEXT);
	if(pWnd)
		pWnd->SetWindowText(m_csPromptText);

	pWnd = GetDlgItem(IDC_CHECK_SAVE_PASSWORD);
	if(pWnd)
	{
		if(!m_csCheckBoxText.IsEmpty())
		{
			pWnd->SetWindowText(m_csCheckBoxText);
		}
		else
		{
			// Hide the check box control if there's no label text
			// This will be the case when we're not using single sign-on
			pWnd->ShowWindow(SW_HIDE); 
		}
	}

	CEdit *pEdit = (CEdit *)GetDlgItem(IDC_PASSWORD);
	if(pEdit) 
	{
		pEdit->SetFocus();

		return 0; // Returning "0" since we're explicitly setting focus
	}

	return TRUE;
}

//--------------------------------------------------------------------------//
//				CPromptUsernamePasswordDialog Stuff
//--------------------------------------------------------------------------//

CPromptUsernamePasswordDialog::CPromptUsernamePasswordDialog(CWnd* pParent, const char* pTitle, const char* pText, 
								   const char* pUserNameLabel, const char* pPasswordLabel,
								   const char* pCheckText)
    : CDialog(CPromptUsernamePasswordDialog::IDD, pParent)
{   
	if(pTitle)
		m_csDialogTitle = pTitle;
	if(pText)
		m_csPromptText = pText;
	if(pUserNameLabel)
		m_csUserNameLabel = pUserNameLabel;
	if(pPasswordLabel)
		m_csPasswordLabel = pPasswordLabel;
	if(pCheckText)
		m_csCheckBoxText = pCheckText;
	m_csUserName = "";
	m_csPassword = "";
	m_bSavePassword = PR_FALSE;
}

void CPromptUsernamePasswordDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CPromptUsernamePasswordDialog)
	DDX_Text(pDX, IDC_USERNAME, m_csUserName);
    DDX_Text(pDX, IDC_PASSWORD, m_csPassword);
	DDX_Check(pDX, IDC_CHECK_SAVE_PASSWORD, m_bSavePassword);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPromptUsernamePasswordDialog, CDialog)
    //{{AFX_MSG_MAP(CPromptUsernamePasswordDialog)
		// NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CPromptUsernamePasswordDialog::OnInitDialog()
{   
   	SetWindowText(m_csDialogTitle);
  
	CWnd *pWnd = GetDlgItem(IDC_PROMPT_TEXT);
	if(pWnd)
		pWnd->SetWindowText(m_csPromptText);

	// This empty check is required since in the case
	// of non single sign-on the interface methods do
	// not specify the label text for username(unlike in
	// in the single sign-on case where they are)
	// In the case where the labels are not specified 
	// we just use whatever is in the dlg resource
	// Ditto for the password label also
	if(! m_csUserNameLabel.IsEmpty())
	{
		pWnd = GetDlgItem(IDC_USERNAME_LABEL);
		if(pWnd)
			pWnd->SetWindowText(m_csUserNameLabel);
	}

	if(! m_csPasswordLabel.IsEmpty())
	{
		pWnd = GetDlgItem(IDC_PASSWORD_LABEL);
		if(pWnd)
			pWnd->SetWindowText(m_csPasswordLabel);
	}

	pWnd = GetDlgItem(IDC_CHECK_SAVE_PASSWORD);
	if(pWnd)
	{
		if(!m_csCheckBoxText.IsEmpty())
		{
			pWnd->SetWindowText(m_csCheckBoxText);
		}
		else
		{
			// Hide the check box control if there's no label text
			// This will be the case when we're not using single sign-on

			pWnd->ShowWindow(SW_HIDE);
		}
	}

	CEdit *pEdit = (CEdit *)GetDlgItem(IDC_USERNAME);
	if(pEdit) 
	{
		pEdit->SetFocus();

		return 0; // Returning "0" since we're explicitly setting focus
	}

	return TRUE;
}
