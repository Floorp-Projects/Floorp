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
