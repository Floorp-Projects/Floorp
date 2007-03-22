// CustomPromptDialog.cpp : implementation file
//

#include "stdafx.h"
#include "mfcembed.h"
#include "CCustomPromptDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCustomPromptDialog dialog


CCustomPromptDialog::CCustomPromptDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CCustomPromptDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CCustomPromptDialog)
	m_CustomText = _T("");
	//}}AFX_DATA_INIT
}


void CCustomPromptDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCustomPromptDialog)
	DDX_Text(pDX, IDC_PROMPT_TEXT, m_CustomText);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCustomPromptDialog, CDialog)
	//{{AFX_MSG_MAP(CCustomPromptDialog)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCustomPromptDialog message handlers
