/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com> 
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"
#include "Dialogs.h"

// File overview....
//
// Contains dialog box code to support Alerts, Prompts such as
// password prompt and username/password prompts
//

//--------------------------------------------------------------------------//
//				CPromptDialog Stuff
//--------------------------------------------------------------------------//

CPromptDialog::CPromptDialog(CWnd* pParent, const char* pTitle, const char* pText,
                             const char* pInitPromptText,
                             BOOL bHasCheck, const char* pCheckText, int initCheckVal)
    : CDialog(CPromptDialog::IDD, pParent),
    m_bHasCheckBox(bHasCheck)
{   
	if(pTitle)
		m_csDialogTitle = pTitle;
	if(pText)
		m_csPromptText = pText;
	if(pInitPromptText)
		m_csPromptAnswer = pInitPromptText;
	if(pCheckText)
	    m_csCheckBoxText = pCheckText; 
}

void CPromptDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CPromptDialog)
    DDX_Text(pDX, IDC_PROMPT_ANSWER, m_csPromptAnswer);
    DDX_Check(pDX, IDC_CHECK_SAVE_PASSWORD, m_bCheckBoxValue);
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

  CButton *pChk = (CButton *)GetDlgItem(IDC_CHECK_SAVE_PASSWORD);
	if(pChk)
	{
	    if (m_bHasCheckBox)
	    {
		    if(!m_csCheckBoxText.IsEmpty())
			    pChk->SetWindowText(m_csCheckBoxText);
			pChk->SetCheck(m_bCheckBoxValue ? BST_CHECKED : BST_UNCHECKED);
		}
		else
		{
			// Hide the check box control if there's no label text
			// This will be the case when we're not using single sign-on
			pChk->ShowWindow(SW_HIDE); 
		}
	}

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

CPromptPasswordDialog::CPromptPasswordDialog(CWnd* pParent, const char* pTitle, const char* pText,
                                             const char* pInitPasswordText,
                                             BOOL bHasCheck, const char* pCheckText, int initCheckVal)
    : CDialog(CPromptPasswordDialog::IDD, pParent),
    m_bHasCheckBox(bHasCheck), m_bCheckBoxValue(initCheckVal)
{   
	if(pTitle)
		m_csDialogTitle = pTitle;
	if(pText)
		m_csPromptText = pText;
	if(pInitPasswordText)
	    m_csPassword = pInitPasswordText;
	if(pCheckText)
		m_csCheckBoxText = pCheckText;
}

void CPromptPasswordDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CPromptPasswordDialog)
    DDX_Text(pDX, IDC_PASSWORD, m_csPassword);
	  DDX_Check(pDX, IDC_CHECK_SAVE_PASSWORD, m_bCheckBoxValue);
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

	CButton *pChk = (CButton *)GetDlgItem(IDC_CHECK_SAVE_PASSWORD);
	if(pChk)
	{
	  if (m_bHasCheckBox)
	  {
		  if(!m_csCheckBoxText.IsEmpty())
			  pChk->SetWindowText(m_csCheckBoxText);
			pChk->SetCheck(m_bCheckBoxValue ? BST_CHECKED : BST_UNCHECKED);
		}
		else
		{
			// Hide the check box control if there's no label text
			// This will be the case when we're not using single sign-on
			pChk->ShowWindow(SW_HIDE); 
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
                                  const char* pInitUsername, const char* pInitPassword, 
		                          BOOL bHasCheck, const char* pCheckText, int initCheckVal)
    : CDialog(CPromptUsernamePasswordDialog::IDD, pParent),
    m_bHasCheckBox(bHasCheck), m_bCheckBoxValue(initCheckVal)
{
	if(pTitle)
		m_csDialogTitle = pTitle;
	if(pText)
		m_csPromptText = pText;
	if(pInitUsername)
		m_csUserName = pInitUsername;
	if(pInitPassword)
		m_csPassword = pInitPassword;
	if(pCheckText)
		m_csCheckBoxText = pCheckText;
}

void CPromptUsernamePasswordDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CPromptUsernamePasswordDialog)
	DDX_Text(pDX, IDC_USERNAME, m_csUserName);
    DDX_Text(pDX, IDC_PASSWORD, m_csPassword);
	DDX_Check(pDX, IDC_CHECK_SAVE_PASSWORD, m_bCheckBoxValue);
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

	CButton *pChk = (CButton *)GetDlgItem(IDC_CHECK_SAVE_PASSWORD);
	if(pChk)
	{
		if(m_bHasCheckBox)
		{
		    if (!m_csCheckBoxText.IsEmpty())
			    pChk->SetWindowText(m_csCheckBoxText);
			pChk->SetCheck(m_bCheckBoxValue ? BST_CHECKED : BST_UNCHECKED);
		}
		else
		{
			pChk->ShowWindow(SW_HIDE);
		}
	}

	CEdit *pEdit = (CEdit *)GetDlgItem(IDC_PASSWORD);
	if(pEdit) 
	{
		pEdit->SetWindowText(m_csPassword);
	}

	pEdit = (CEdit *)GetDlgItem(IDC_USERNAME);
	if(pEdit) 
	{
		pEdit->SetWindowText(m_csUserName);
		pEdit->SetSel(0, -1);

		pEdit->SetFocus();

		return 0; // Returning "0" since we're explicitly setting focus
	}

	return TRUE;
}
