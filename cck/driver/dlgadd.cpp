// DlgAdd.cpp : implementation file
//

#include "stdafx.h"
#include "DlgAdd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgAdd dialog


CDlgAdd::CDlgAdd(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgAdd::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgAdd)
	m_strPrefDesc = _T("");
	m_strPrefName = _T("");
	m_intPrefType = 0;
	//}}AFX_DATA_INIT
}


void CDlgAdd::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgAdd)
	DDX_Text(pDX, IDC_PREFDESC, m_strPrefDesc);
	DDX_Text(pDX, IDC_PREFNAME, m_strPrefName);
	DDX_Radio(pDX, IDC_PREFTYPE_STRING, m_intPrefType);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgAdd, CDialog)
	//{{AFX_MSG_MAP(CDlgAdd)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgAdd message handlers
