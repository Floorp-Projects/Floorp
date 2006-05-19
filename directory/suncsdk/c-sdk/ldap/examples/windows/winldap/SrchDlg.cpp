/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1996-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

// SrchDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ldap.h"
#include "winldap.h"
#include "SrchDlg.h"

#ifdef _DEBUG
#ifdef _WIN32
#define new DEBUG_NEW
#endif
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// SearchDlg dialog


SearchDlg::SearchDlg(CWnd* pParent /*=NULL*/)
	: CDialog(SearchDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(SearchDlg)
	m_searchBase = _T("");
	m_searchFilter = _T("");
	//}}AFX_DATA_INIT
}


void SearchDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(SearchDlg)
	DDX_Text(pDX, IDC_SEARCH_BASE, m_searchBase);
	DDX_Text(pDX, IDC_SEARCH_FILTER, m_searchFilter);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(SearchDlg, CDialog)
	//{{AFX_MSG_MAP(SearchDlg)
	ON_BN_CLICKED(IDC_SCOPE_BASE, OnScopeBase)
	ON_BN_DOUBLECLICKED(IDC_SCOPE_BASE, OnScopeBase)
	ON_BN_CLICKED(IDC_SCOPE_ONE, OnScopeOne)
	ON_BN_DOUBLECLICKED(IDC_SCOPE_ONE, OnScopeOne)
	ON_BN_CLICKED(IDC_SCOPE_SUB, OnScopeSub)
	ON_BN_DOUBLECLICKED(IDC_SCOPE_SUB, OnScopeSub)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// SearchDlg message handlers

BOOL SearchDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	switch( m_scope )
	{
		case LDAP_SCOPE_SUBTREE:
			((CButton *)GetDlgItem( IDC_SCOPE_SUB ))->SetCheck( 1 );
			break;
		case LDAP_SCOPE_BASE:
			((CButton *)GetDlgItem( IDC_SCOPE_BASE ))->SetCheck( 1 );
			break;
		case LDAP_SCOPE_ONELEVEL:
			((CButton *)GetDlgItem( IDC_SCOPE_ONE ))->SetCheck( 1 );
			break;
	}
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void SearchDlg::OnScopeBase() 
{
	m_scope = LDAP_SCOPE_BASE;
}

void SearchDlg::OnScopeOne() 
{
	m_scope = LDAP_SCOPE_ONELEVEL;
}

void SearchDlg::OnScopeSub() 
{
	m_scope = LDAP_SCOPE_SUBTREE;
}
