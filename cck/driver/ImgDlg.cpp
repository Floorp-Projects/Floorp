// ImgDlg.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "ImgDlg.h"
#include "HelpDlg.h"
#include "WizHelp.h"
#include "interpret.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CImgDlg dialog
extern CInterpret *theInterpreter;
extern char iniFilePath[MAX_SIZE];
extern char imagesPath[MAX_SIZE];
extern CString iniTracker;
CString imageTitle;

CImgDlg::CImgDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CImgDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CImgDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


CImgDlg::CImgDlg(CString theIniFileName, CWnd* pParent /*=NULL*/)
	: CDialog(CImgDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CImageDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	imageSectionName = "IMAGE";

	// All the images to be displayed on button clicks are 
	// in this iniFile. So iniFileName is initialized thus.
	iniFileName = CString(iniFilePath) + theIniFileName;
	ReadImageFromIniFile();
}


void CImgDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CImgDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CImgDlg, CDialog)
	//{{AFX_MSG_MAP(CImgDlg)
	ON_BN_CLICKED(IDC_HELP_BUTTON, OnHelpButton)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_CTLCOLOR()
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CImgDlg message handlers

void CImgDlg::OnHelpButton() 
{
	// TODO: Add your control notification handler code here
//		CWnd Mywnd;
//		Mywnd.MessageBox("hello","hello",MB_OK);
	CString helpvalue = iniTracker;
	CString helpvar = helpvalue.Left(6);
	CString htmlfile ="";
	if (helpvar.CompareNoCase("Online")== 0)
	{
		helpvalue.Delete(0,7);
//		AfxMessageBox("online",MB_OK);
		htmlfile = theInterpreter->replaceVars((char*)(LPCTSTR)helpvalue, NULL);
		theInterpreter->OpenBrowser((char*)(LPCTSTR)htmlfile);
	}
	else 
	{
		CWizHelp hlpdlg;
		hlpdlg.DoModal();
	}

}

int CImgDlg::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// TODO: Add your specialized creation code here
	
	return 0;
}

void CImgDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// TODO: Add your message handler code here

	CRect rect(0, 0, 4, 8);
	MapDialogRect(&rect);

	int baseWidth = rect.Width();
	int baseHeight = rect.Height();

	CClientDC cdc(this);
	HBITMAP hbmpOld;
	CDC dcMem;

	dcMem.CreateCompatibleDC(&cdc);

	hbmpOld = (HBITMAP)::SelectObject(dcMem, image.hBitmap);

	dc.BitBlt((int)((float)(image.location.x) * (float)baseWidth / 4.0),
		(int)((float)(image.location.y) * (float)baseHeight / 8.0),
		(int)((float)(image.size.width) * (float)baseWidth / 4.0),
		(int)((float)(image.size.height) * (float)baseHeight / 8.0),
		&dcMem,  
		0, 
		0, 
		SRCCOPY);
	
	// Do not call CDialog::OnPaint() for painting messages
}

void CImgDlg::ReadImageFromIniFile()
{
	char buffer[500];
	GetPrivateProfileString(imageSectionName, "Name", "", buffer, 250, iniFileName);
	image.name = CString(imagesPath) + CString(buffer);

	GetPrivateProfileString(imageSectionName, "Title", "", buffer, 250, iniFileName);
	imageTitle = CString(buffer);

	GetPrivateProfileString(imageSectionName, "start_X", "", buffer, 250, iniFileName);
	image.location.x = atoi(buffer);

	GetPrivateProfileString(imageSectionName, "start_Y", "", buffer, 250, iniFileName);
	image.location.y = atoi(buffer);

	GetPrivateProfileString(imageSectionName, "width", "", buffer, 250, iniFileName);
	image.size.width = atoi(buffer);

	GetPrivateProfileString(imageSectionName, "height", "", buffer, 250, iniFileName);
	image.size.height = atoi(buffer);

	image.hBitmap = (HBITMAP)LoadImage(NULL, image.name, IMAGE_BITMAP, 0, 0,
											LR_LOADFROMFILE|LR_CREATEDIBSECTION);
	GetPrivateProfileString(imageSectionName, "Help", "", buffer, 250, iniFileName);
	iniTracker = CString(buffer);

}

BOOL CImgDlg::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::OnCommand(wParam, lParam);
}

int CImgDlg::DoModal() 
{
	// TODO: Add your specialized code here and/or call the base class

	return CDialog::DoModal();
}

BOOL CImgDlg::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
	return CDialog::Create(IDD, pParentWnd);
}

HBRUSH CImgDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	
	// TODO: Change any attributes of the DC here
	
	// TODO: Return a different brush if the default is not desired
	return hbr;
}

BOOL CImgDlg::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	// TODO: Add your message handler code here and/or call default
	
	return CDialog::OnHelpInfo(pHelpInfo);
}

BOOL CImgDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	SetWindowText(imageTitle);
	SetWindowPos(&wndTop,0,0,4,8,SWP_NOSIZE);	
// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
