// ConnDlg.cpp : implementation file
//

#include "stdafx.h"
#include "winldap.h"
#include "ConnDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// ConnDlg dialog


ConnDlg::ConnDlg(CWnd* pParent /*=NULL*/)
	: CDialog(ConnDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(ConnDlg)
	m_dirHost = _T("");
	m_dirPort = 0;
	//}}AFX_DATA_INIT
}


void ConnDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(ConnDlg)
	DDX_Text(pDX, IDC_DIR_HOST, m_dirHost);
	DDX_Text(pDX, IDC_DIR_PORT, m_dirPort);
	DDV_MinMaxInt(pDX, m_dirPort, 1, 32000);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(ConnDlg, CDialog)
	//{{AFX_MSG_MAP(ConnDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ConnDlg message handlers
