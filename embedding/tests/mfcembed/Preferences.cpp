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
