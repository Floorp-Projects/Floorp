// HelpDlg.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "HelpDlg.h"
#include "stdio.h"
#include "fstream.h"
#include "WizHelp.h"
extern NODE *CurrentNode;
extern char iniFilePath[MAX_SIZE];
extern CString iniTracker;
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CHelpDlg dialog

CHelpDlg::CHelpDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CHelpDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CHelpDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
//DoModal();
}

BOOL CHelpDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
//	CWnd ThatWind;
//	char buffer[MAX_SIZE];
	FILE *fPtr;

	CString Helptext = CString(iniFilePath) + "\\" + iniTracker;
//	ThatWind.MessageBox(Helptext,"something",MB_OK);
	if( !( fPtr = fopen(Helptext, "rb") ) )
	{
	    CWnd myWnd;
	
		myWnd.MessageBox("Unable to open file" + Helptext, "ERROR", MB_OK);
//		exit( 3 );
	}

	else 
		fseek(fPtr, 0,2);
		long f_size = ftell(fPtr);
		fseek (fPtr,0,0);
		char *file_buffer;
		file_buffer = (char *) malloc (f_size);
		fread (file_buffer,1,f_size,fPtr);
		file_buffer[f_size]=NULL;

//	ThatWind.MessageBox(file_buffer,"buffer",MB_OK);
	CEdit *ebptr =(CEdit *) GetDlgItem(IDC_EDIT1);
	ebptr->SetWindowText(file_buffer);
	return TRUE;

}
void CHelpDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CHelpDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CHelpDlg, CDialog)
	//{{AFX_MSG_MAP(CHelpDlg)
	ON_WM_CREATE()
	ON_WM_CTLCOLOR()
	ON_WM_PAINT()
	ON_WM_LBUTTONDBLCLK()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHelpDlg message handlers

BOOL CHelpDlg::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::Create(IDD, pParentWnd);
}

BOOL CHelpDlg::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::OnCommand(wParam, lParam);
}

int CHelpDlg::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// TODO: Add your specialized creation code here
	
	return 0;
}

HBRUSH CHelpDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	
	// TODO: Change any attributes of the DC here
	
	// TODO: Return a different brush if the default is not desired
	return hbr;
}

void CHelpDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// TODO: Add your message handler code here
	
	// Do not call CDialog::OnPaint() for painting messages
}


int CHelpDlg::DoModal() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::DoModal();
}

void CHelpDlg::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	
	CDialog::OnLButtonDblClk(nFlags, point);
}
