// SumDlg.cpp : implementation file
//

#include "stdafx.h"
#include "globals.h"
#include "WizardMachine.h"
#include "SumDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

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

	for(int i=0; i< GlobalArrayIndex; i++)
	{
		str2 = GlobalWidgetArray[i].name;
		// Remove unnecessary keys (customizations not visible) in 
		// Show Configuration Information dialog 
		if ((str2=="Root") || (str2=="inifile") || (str2=="DefaultName") 
			|| (str2=="Organization") || (str2=="Department") 
			|| (str2=="Feedback") || (str2=="FeedbackChoice") 
			|| (str2=="FeedbackButtonName") || (str2=="CustomizationList") 
			|| (str2=="CreateReadmeIcon") || (str2=="CreateLicenseIcon") 
			|| (str2=="IntSetupBgBitmap") || (str2=="Company_Name") 
			|| (str2=="TechSupportNumber") || (str2=="NCIFileList") 
			|| (str2=="NCIFileEditor"))
			continue;
		if (GlobalWidgetArray[i].type != "RadioButton" 
			&& GlobalWidgetArray[i].type != "RadioGroup"
			&& GlobalWidgetArray[i].type != "Text" 
			&& GlobalWidgetArray[i].type != "BoldText" 
			&& GlobalWidgetArray[i].type != "GlobalText" 
			&& GlobalWidgetArray[i].type != "Button" 
			&& GlobalWidgetArray[i].type != "GroupBox" 
			&& GlobalWidgetArray[i].type != "BoldGroup" 
			&& str2.Left(1) != "_")
		{
			if (GlobalWidgetArray[i].name != "")
			{
			// Display only visible customizations in the Show 
			// Configuration Information dialog
			tmp1=GlobalWidgetArray[i].name;
			tmp2=GlobalWidgetArray[i].value;
			str1 += tmp1;
			str1 += "\t=";
			str1 += tmp2;
			str1 += "\r\n";
			}
		}
	}

	
	CEdit *ebptr1 =(CEdit *) GetDlgItem(IDC_EDIT1);
	ebptr1->SetWindowText(str1);
		 

	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
