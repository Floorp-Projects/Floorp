// SumDlg.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "SumDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
extern WIDGET GlobalWidgetArray[1000];

extern int GlobalArrayIndex;


CString stringerx1 ="name1";

CString stringer2x1 ="value1";

CString stringerx2 ="name2";

CString stringer2x2 ="value2";

/////////////////////////////////////////////////////////////////////////////
// CSumDlg dialog


CSumDlg::CSumDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSumDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSumDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CSumDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSumDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSumDlg, CDialog)
	//{{AFX_MSG_MAP(CSumDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSumDlg message handlers

int CSumDlg::DoModal() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::DoModal();
}

BOOL CSumDlg::OnInitDialog() 
{
	CString tmp1,tmp2,str1,str2;
	CDialog::OnInitDialog();
	for (int i = 0; i <= GlobalArrayIndex; i++)
	{
		if (GlobalWidgetArray[i].type == "EditBox") {
			tmp1=GlobalWidgetArray[i].name;
			tmp2=GlobalWidgetArray[i].value;
			str1 += "\t" + tmp1;
			str1 += "\t=";
			str1 += tmp2;
			str1 += "\r\n";

//			tmp2=GlobalWidgetArray[i].value;
//			str2 += tmp2;
//			str2 += "\r\n";

//			CWnd Mywnd;
//			Mywnd.MessageBox(Valuebox,"hello",MB_OK);

		}
	}

	
	
	CEdit *ebptr1 =(CEdit *) GetDlgItem(IDC_EDIT1);
	ebptr1->SetWindowText(str1);
		 
//	CEdit *ebptr2 =(CEdit *) GetDlgItem(IDC_EDIT3);
//	ebptr2->SetWindowText(str2);
	
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
