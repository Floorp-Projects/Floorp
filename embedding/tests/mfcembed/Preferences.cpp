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
#include "Preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////
// CPreferences

IMPLEMENT_DYNAMIC(CPreferences, CPropertySheet)

CPreferences::CPreferences(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
    :CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
    AddPage(&m_startupPage);
}

CPreferences::~CPreferences()
{
}

BEGIN_MESSAGE_MAP(CPreferences, CPropertySheet)
    //{{AFX_MSG_MAP(CPreferences)
        // NOTE - the ClassWizard will add and remove mapping macros here.
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CPreferences::OnInitDialog() 
{
    BOOL bResult = CPropertySheet::OnInitDialog();
    
    // Hide the Apply button
    CWnd* pApplyButton = GetDlgItem(ID_APPLY_NOW);
    ASSERT(pApplyButton);
    pApplyButton->ShowWindow(SW_HIDE);

    return bResult;
}


/////////////////////////////////////////////////////////////////
// CStartupPrefsPage property page

IMPLEMENT_DYNCREATE(CStartupPrefsPage, CPropertyPage)

CStartupPrefsPage::CStartupPrefsPage() : CPropertyPage(CStartupPrefsPage::IDD)
{
    //{{AFX_DATA_INIT(CStartupPrefsPage)
    m_strHomePage = _T("");
    m_iStartupPage = -1;
    //}}AFX_DATA_INIT
}

CStartupPrefsPage::~CStartupPrefsPage()
{
}

void CStartupPrefsPage::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CStartupPrefsPage)
    DDX_Control(pDX, IDC_EDIT_HOMEPAGE, m_HomePage);
    DDX_Text(pDX, IDC_EDIT_HOMEPAGE, m_strHomePage);
    DDX_Radio(pDX, IDC_RADIO_BLANK_PAGE, m_iStartupPage);
    //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CStartupPrefsPage, CPropertyPage)
    //{{AFX_MSG_MAP(CStartupPrefsPage)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()
