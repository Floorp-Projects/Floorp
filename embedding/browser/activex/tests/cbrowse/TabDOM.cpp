// TabDOM.cpp : implementation file
//

#include "stdafx.h"
#include "cbrowse.h"
#include "TabDOM.h"
#include "CBrowseDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTabDOM property page

IMPLEMENT_DYNCREATE(CTabDOM, CPropertyPage)

CTabDOM::CTabDOM() : CPropertyPage(CTabDOM::IDD, CTabDOM::IDD)
{
	//{{AFX_DATA_INIT(CTabDOM)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


CTabDOM::~CTabDOM()
{
}


void CTabDOM::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTabDOM)
	DDX_Control(pDX, IDC_DOMLIST, m_tcDOM);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTabDOM, CPropertyPage)
	//{{AFX_MSG_MAP(CTabDOM)
	ON_BN_CLICKED(IDC_REFRESHDOM, OnRefreshDOM)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTabDOM message handlers

void CTabDOM::OnRefreshDOM() 
{
	if (m_pBrowseDlg)
	{
		m_pBrowseDlg->OnRefreshDOM();
	}
}


BOOL CTabDOM::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	// Create the DOM tree
	m_tcDOM.SetImageList(&m_pBrowseDlg->m_cImageList, TVSIL_NORMAL);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
