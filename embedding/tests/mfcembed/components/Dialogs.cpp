/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
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

CPromptDialog::CPromptDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                             const TCHAR* pInitPromptText,
                             BOOL bHasCheck, const TCHAR* pCheckText, int initCheckVal)
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

CPromptPasswordDialog::CPromptPasswordDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                                             const TCHAR* pInitPasswordText,
                                             BOOL bHasCheck, const TCHAR* pCheckText, int initCheckVal)
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

CPromptUsernamePasswordDialog::CPromptUsernamePasswordDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                                  const TCHAR* pInitUsername, const TCHAR* pInitPassword, 
		                          BOOL bHasCheck, const TCHAR* pCheckText, int initCheckVal)
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

//--------------------------------------------------------------------------//
//				CAlertCheckDialog Stuff
//--------------------------------------------------------------------------//

CAlertCheckDialog::CAlertCheckDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                             const TCHAR* pCheckText, int initCheckVal)
        : CDialog(CAlertCheckDialog::IDD, pParent)
{   
    if(pTitle)
        m_csDialogTitle = pTitle;
    if(pText)
        m_csMsgText = pText;
    if(pCheckText)
        m_csCheckBoxText = pCheckText; 

    m_bCheckBoxValue = initCheckVal;
}

void CAlertCheckDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAlertCheckDialog)
    DDX_Check(pDX, IDC_CHECKBOX, m_bCheckBoxValue);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAlertCheckDialog, CDialog)
    //{{AFX_MSG_MAP(CAlertCheckDialog)
        // NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CAlertCheckDialog::OnInitDialog()
{
    SetWindowText(m_csDialogTitle);

    CWnd *pWnd = GetDlgItem(IDC_MSG_TEXT);
    if(pWnd)
        pWnd->SetWindowText(m_csMsgText);

    CButton *pChk = (CButton *)GetDlgItem(IDC_CHECKBOX);
    if(pChk)
    {
        pChk->SetWindowText(m_csCheckBoxText);
        pChk->SetCheck(m_bCheckBoxValue ? BST_CHECKED : BST_UNCHECKED);
    }

    return TRUE;
}

//--------------------------------------------------------------------------//
//				CConfirmCheckDialog Stuff
//--------------------------------------------------------------------------//

CConfirmCheckDialog::CConfirmCheckDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                            const TCHAR* pCheckText, int initCheckVal,
                            const TCHAR *pBtn1Text, const TCHAR *pBtn2Text, 
                            const TCHAR *pBtn3Text)
            : CDialog(CConfirmCheckDialog::IDD, pParent)
{   
    if(pTitle)
        m_csDialogTitle = pTitle;
    if(pText)
        m_csMsgText = pText;
    if(pCheckText)
        m_csCheckBoxText = pCheckText; 

    m_bCheckBoxValue = initCheckVal;

    if(pBtn1Text)
        m_csBtn1Text = pBtn1Text;
    if(pBtn2Text)
        m_csBtn2Text = pBtn2Text;
    if(pBtn3Text)
        m_csBtn3Text = pBtn3Text;
}

void CConfirmCheckDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CConfirmCheckDialog)
    DDX_Check(pDX, IDC_CHECKBOX, m_bCheckBoxValue);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CConfirmCheckDialog, CDialog)
    //{{AFX_MSG_MAP(CConfirmCheckDialog)
    ON_BN_CLICKED(IDC_BTN1, OnBtn1Clicked)
    ON_BN_CLICKED(IDC_BTN2, OnBtn2Clicked)
    ON_BN_CLICKED(IDC_BTN3, OnBtn3Clicked)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CConfirmCheckDialog::OnInitDialog()
{   
   	SetWindowText(m_csDialogTitle);
  
    CWnd *pWnd = GetDlgItem(IDC_MSG_TEXT);
    if(pWnd)
        pWnd->SetWindowText(m_csMsgText);

    CButton *pChk = (CButton *)GetDlgItem(IDC_CHECKBOX);
    if(pChk)
    {
        if(m_csCheckBoxText.IsEmpty())
        {
            pChk->ShowWindow(SW_HIDE);
        }
        else
        {
            pChk->SetWindowText(m_csCheckBoxText);
            pChk->SetCheck(m_bCheckBoxValue ? BST_CHECKED : BST_UNCHECKED);
        }
    }

    CButton *pBtn1 = (CButton *)GetDlgItem(IDC_BTN1);
    if(pBtn1)
    {
        if(m_csBtn1Text.IsEmpty())
            pBtn1->ShowWindow(SW_HIDE);
        else
            pBtn1->SetWindowText(m_csBtn1Text);
    }

    CButton *pBtn2 = (CButton *)GetDlgItem(IDC_BTN2);
    if(pBtn2)
    {
        if(m_csBtn2Text.IsEmpty())
            pBtn2->ShowWindow(SW_HIDE);
        else
            pBtn2->SetWindowText(m_csBtn2Text);
    }

    CButton *pBtn3 = (CButton *)GetDlgItem(IDC_BTN3);
    if(pBtn3)
    {
        if(m_csBtn3Text.IsEmpty())
            pBtn3->ShowWindow(SW_HIDE);
        else
            pBtn3->SetWindowText(m_csBtn3Text);
    }

    return TRUE;
}

void CConfirmCheckDialog::OnBtn1Clicked()
{
    UpdateData();

    EndDialog(0); // where 0 indicates that the btn pressed was at index 0
}

void CConfirmCheckDialog::OnBtn2Clicked()
{
    UpdateData();

    EndDialog(1); // where 1 indicates that the btn pressed was at index 1
}

void CConfirmCheckDialog::OnBtn3Clicked()
{
    UpdateData();

    EndDialog(2); // where 2 indicates that the btn pressed was at index 2
}
