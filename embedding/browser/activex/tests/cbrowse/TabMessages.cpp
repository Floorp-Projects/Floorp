// TabMessages.cpp : implementation file
//

#include "stdafx.h"
#include "cbrowse.h"
#include "TabMessages.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTabMessages property page

IMPLEMENT_DYNCREATE(CTabMessages, CPropertyPage)

CTabMessages::CTabMessages() : CPropertyPage(CTabMessages::IDD, CTabMessages::IDD)
{
	//{{AFX_DATA_INIT(CTabMessages)
	m_szStatus = _T("");
	//}}AFX_DATA_INIT
}


CTabMessages::~CTabMessages()
{
}


void CTabMessages::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTabMessages)
	DDX_Control(pDX, IDC_PROGRESS, m_pcProgress);
	DDX_Control(pDX, IDC_OUTPUT, m_lbMessages);
	DDX_Text(pDX, IDC_STATUS, m_szStatus);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTabMessages, CPropertyPage)
	//{{AFX_MSG_MAP(CTabMessages)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTabMessages message handlers
