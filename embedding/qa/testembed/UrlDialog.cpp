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
	m_urlflag = 0;
	//}}AFX_DATA_INIT
}


void CUrlDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUrlDialog)
	DDX_Text(pDX, IDC_URLFIELD, m_urlfield);
	DDX_CBIndex(pDX, IDC_COMBO1, m_urlflag);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUrlDialog, CDialog)
	//{{AFX_MSG_MAP(CUrlDialog)
	ON_EN_CHANGE(IDC_URLFIELD, OnChangeUrlfield)
	ON_EN_CHANGE(IDC_COMBO1, OnChangeUrlfield)
	ON_CBN_EDITCHANGE(IDC_COMBO1, OnEditchangeCombo1)
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

int CUrlDialog::OnEditchangeCombo1() 
{
	// TODO: Add your control notification handler code here

	int loadFlag = 0;
/*
	if (m_urlflag == NONE);
		loadFlag = nsIWebNavigation::LOAD_FLAGS_NONE;
	else if (m_urlflag == MASK);
		loadFlag = nsIWebNavigation::LOAD_FLAGS_MASK;
	else if (m_urlflag == IS_LINK);
		loadFlag = nsIWebNavigation::LOAD_FLAGS_IS_LINK;
	else if (m_urlflag == BYPASS_HISTORY);
		loadFlag = nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY;
	else if (m_urlflag == REPLACE_HISTORY);
		loadFlag = nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY;
	else if (m_urlflag == BYPASS_CACHE);
		loadFlag = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
	else if (m_urlflag == BYPASS_PROXY);
		loadFlag = nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
	else if (m_urlflag == CHARSET_CHANGE);
		loadFlag = nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE;	
*/
	return loadFlag;

}
