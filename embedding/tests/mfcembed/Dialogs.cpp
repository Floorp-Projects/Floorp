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
#include "BrowserView.h"

// File overview....
//
// contains Find dialog box code
//

//--------------------------------------------------------------------------//
//                CFindDialog Stuff
//--------------------------------------------------------------------------//

CFindDialog::CFindDialog(CString& csSearchStr, PRBool bMatchCase,
                PRBool bMatchWholeWord, PRBool bWrapAround,
                PRBool bSearchBackwards, CBrowserView* pOwner)
                : CFindReplaceDialog()
{
    // Save these initial settings off in member vars
    // We'll use these to initialize the controls
    // in InitDialog()
    m_csSearchStr = csSearchStr;
    m_bMatchCase = bMatchCase;
    m_bMatchWholeWord = bMatchWholeWord;
    m_bWrapAround = bWrapAround;
    m_bSearchBackwards = bSearchBackwards;
    m_pOwner = pOwner;

    // Set up to load our customized Find dialog template
    // rather than the default one MFC provides
    m_fr.Flags |= FR_ENABLETEMPLATE;
    m_fr.hInstance = AfxGetInstanceHandle();
    m_fr.lpTemplateName = MAKEINTRESOURCE(IDD_FINDDLG);
}

BOOL CFindDialog::OnInitDialog() 
{
    CFindReplaceDialog::OnInitDialog();

    CEdit* pEdit = (CEdit *)GetDlgItem(IDC_FIND_EDIT);
    if(pEdit)
        pEdit->SetWindowText(m_csSearchStr);

    CButton* pChk = (CButton *)GetDlgItem(IDC_MATCH_CASE);
    if(pChk)
        pChk->SetCheck(m_bMatchCase);

    pChk = (CButton *)GetDlgItem(IDC_MATCH_WHOLE_WORD);
    if(pChk)
        pChk->SetCheck(m_bMatchWholeWord);

    pChk = (CButton *)GetDlgItem(IDC_WRAP_AROUND);    
    if(pChk)
        pChk->SetCheck(m_bWrapAround);

    pChk = (CButton *)GetDlgItem(IDC_SEARCH_BACKWARDS);
    if(pChk)
        pChk->SetCheck(m_bSearchBackwards);

    return TRUE; 
}

void CFindDialog::PostNcDestroy()    
{
    // Let the owner know we're gone
    if(m_pOwner != NULL)    
        m_pOwner->ClearFindDialog();

    CFindReplaceDialog::PostNcDestroy();
}

BOOL CFindDialog::WrapAround()
{
    CButton* pChk = (CButton *)GetDlgItem(IDC_WRAP_AROUND);

    return pChk ? pChk->GetCheck() : FALSE;
}

BOOL CFindDialog::SearchBackwards()
{
    CButton* pChk = (CButton *)GetDlgItem(IDC_SEARCH_BACKWARDS);

    return pChk ? pChk->GetCheck() : FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CLinkPropertiesDlg dialog
CLinkPropertiesDlg::CLinkPropertiesDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CLinkPropertiesDlg::IDD, pParent)
{
	m_LinkText = _T("");
	m_LinkLocation = _T("");
}

void CLinkPropertiesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_LINK_TEXT, m_LinkText);
	DDX_Text(pDX, IDC_EDIT_LINK_LOCATION, m_LinkLocation);
}


BEGIN_MESSAGE_MAP(CLinkPropertiesDlg, CDialog)
END_MESSAGE_MAP()

void CLinkPropertiesDlg::OnOK() 
{
	UpdateData(TRUE);

	if (m_LinkLocation.IsEmpty() || (m_LinkText.IsEmpty() && m_LinkLocation.IsEmpty()))
	{
		MessageBox(_T("Please enter a Link Location"));
		return;
	}

	if (m_LinkText.IsEmpty())
	{
		m_LinkText = m_LinkLocation;
	}

	EndDialog(IDOK);
}
