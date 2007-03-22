// QaFindDlg.cpp : implementation file
//

#include "stdafx.h"
#include "testembed.h"
#include "QaFindDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CQaFindDlg dialog


CQaFindDlg::CQaFindDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CQaFindDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(QaFindDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_textfield = _T("text");
}


void CQaFindDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CQaFindDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	DDX_Text(pDX, IDC_TEXTFIELD, m_textfield);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CQaFindDlg, CDialog)
	//{{AFX_MSG_MAP(CQaFindDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CQaFindDlg message handlers
