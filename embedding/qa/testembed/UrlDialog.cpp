// UrlDialog.cpp : implementation file
//

#include "stdafx.h"
#include "Testembed.h"
#include "UrlDialog.h"
#include "QaUtils.h"

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
	m_flagvalue = 0;
	m_flagIndex = -1;
	m_chkValue = FALSE;
	//}}AFX_DATA_INIT
}


void CUrlDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUrlDialog)
	DDX_Control(pDX, IDC_CHKURLFLAG, m_chkFlags);
	DDX_Control(pDX, IDC_COMBO1, m_urlflags);
	DDX_Text(pDX, IDC_URLFIELD, m_urlfield);
	DDX_CBIndex(pDX, IDC_COMBO1, m_flagIndex);
	DDX_Check(pDX, IDC_CHKURLFLAG, m_chkValue);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUrlDialog, CDialog)
	//{{AFX_MSG_MAP(CUrlDialog)
	ON_EN_CHANGE(IDC_URLFIELD, OnChangeUrlfield)
	ON_BN_CLICKED(IDC_CHKURLFLAG, OnChkurlflag)
	ON_EN_CHANGE(IDC_COMBO1, OnChangeUrlfield)
	ON_CBN_SELCHANGE(IDC_COMBO1, OnSelchangeCombo1)
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

void CUrlDialog::OnChkurlflag() 
{
	m_chkValue = ! m_chkValue ;
	m_urlflags.EnableWindow(m_chkValue);
}

BOOL CUrlDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_flagIndex = 0;
	m_urlflags.SetCurSel(m_flagIndex);
	m_urlflags.EnableWindow(m_chkValue);

	
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CUrlDialog::OnSelchangeCombo1() 
{

   CString flagvalue;
// m_urlflags.GetLBText(m_flagIndex,flagvalue);
   m_flagIndex = m_urlflags.GetCurSel();

//   if (flagvalue == "NONE") {
   if (m_flagIndex == 0) {
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_NONE;
		QAOutput("Selected NONE flag.", 1);
   }
   else if (m_flagIndex == 1) {
		QAOutput("Selected MASK flag.", 1);
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_MASK;
   }
   else if (m_flagIndex == 2) {
		QAOutput("Selected IS_LINK flag.", 1);
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_IS_LINK;
   }
   else if (m_flagIndex == 3) {
		QAOutput("Selected BYPASS_HISTORY flag.", 1);
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY;
   }
   else if (m_flagIndex == 4) {
		QAOutput("Selected REPLACE_HISTORY flag.", 1);
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY;
   }
   else if (m_flagIndex == 5) {
		QAOutput("Selected BYPASS_CACHE flag.", 1);
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
   }
   else if (m_flagIndex == 6) {
		QAOutput("Selected BYPASS_PROXY flag.", 1);
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
   }
   else if (m_flagIndex == 7) {
		QAOutput("Selected CHARSET_CHANGE flag.", 1);
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE;	
   }
   else if (m_flagIndex == 8) {
		QAOutput("Selected REFRESH flag.", 1);
		m_flagvalue = nsIWebNavigation::LOAD_FLAGS_IS_REFRESH;	
   }
   else
		QAOutput("NO FLAG!!!.", 1);
}
