// UrlDialog.cpp : implementation file
//

#include "stdafx.h"
#include "Testembed.h"
#include "UrlDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CUrlDialog dialog


CUrlDialog::CUrlDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CUrlDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CUrlDialog)
	m_urlfield = _T("");
	//}}AFX_DATA_INIT
}


void CUrlDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUrlDialog)
	DDX_Text(pDX, IDC_URLFIELD, m_urlfield);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUrlDialog, CDialog)
	//{{AFX_MSG_MAP(CUrlDialog)
	ON_EN_CHANGE(IDC_URLFIELD, OnChangeUrlfield)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUrlDialog message handlers

void CUrlDialog::OnChangeUrlfield() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here

}
