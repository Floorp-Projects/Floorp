// WebProgDlg.cpp : implementation file
//

#include "stdafx.h"
#include "testembed.h"
#include "WebProgDlg.h"
#include "QaUtils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWebProgDlg dialog


CWebProgDlg::CWebProgDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CWebProgDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CWebProgDlg)
	m_wpFlagValue = 0;
	m_wpFlagIndex = -1;
	//}}AFX_DATA_INIT
}


void CWebProgDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWebProgDlg)
	DDX_Control(pDX, IDC_WPCOMBO, m_webProgFlags);
	DDX_CBIndex(pDX, IDC_WPCOMBO, m_wpFlagIndex);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CWebProgDlg, CDialog)
	//{{AFX_MSG_MAP(CWebProgDlg)
	ON_CBN_SELCHANGE(IDC_WPCOMBO, OnSelectWPCombo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWebProgDlg message handlers

BOOL CWebProgDlg::OnInitWPDialog()
{
	CDialog::OnInitDialog();

	m_wpFlagIndex = 0;
	m_webProgFlags.SetCurSel(m_wpFlagIndex);

	return TRUE;
}
void CWebProgDlg::OnSelectWPCombo()
{
	m_wpFlagIndex = m_webProgFlags.GetCurSel();	

	if (m_wpFlagIndex == 0) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_STATE_REQUEST;
		QAOutput("Selected NOTIFY_STATE_REQUEST flag.", 1);
	}
	else if (m_wpFlagIndex == 1) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_STATE_DOCUMENT;
		QAOutput("Selected NOTIFY_STATE_DOCUMENT flag.", 1);
	}
	else if (m_wpFlagIndex == 2) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_STATE_NETWORK;
		QAOutput("Selected NOTIFY_STATE_NETWORK flag.", 1);
	}
	else if (m_wpFlagIndex == 3) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_STATE_WINDOW;
		QAOutput("Selected NOTIFY_STATE_WINDOW flag.", 1);
	}
	else if (m_wpFlagIndex == 4) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_STATE_ALL;
		QAOutput("Selected NOTIFY_STATE_ALL flag.", 1);
	}
	else if (m_wpFlagIndex == 5) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_PROGRESS;
		QAOutput("Selected NOTIFY_PROGRESS flag.", 1);
	}
	else if (m_wpFlagIndex == 6) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_STATUS;
		QAOutput("Selected NOTIFY_STATUS flag.", 1);
	}
	else if (m_wpFlagIndex == 7) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_SECURITY;
		QAOutput("Selected NOTIFY_SECURITY flag.", 1);
	}
	else if (m_wpFlagIndex == 8) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_LOCATION;
		QAOutput("Selected NOTIFY_LOCATION flag.", 1);
	}
	else if (m_wpFlagIndex == 9) {
		m_wpFlagValue = nsIWebProgress::NOTIFY_ALL;
		QAOutput("Selected NOTIFY_ALL flag.", 1);
	}
	else
		QAOutput("NO FLAG!!!.", 1);
}
