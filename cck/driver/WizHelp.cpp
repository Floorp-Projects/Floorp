// WizHelp.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "WizHelp.h"
#include "HelpDlg.h"
#include "stdio.h"
#include "fstream.h"
extern NODE *CurrentNode;
extern char iniFilePath[MAX_SIZE];
CString varSection = "Local Variables";
CString navCtrlSection = "Navigation Controls";
CString widgetSection = "Widget";
char buffer[MAX_SIZE];
WID GlobalWid[5];
char *secarray[5]={"Wid1","Wid2","Wid3","Wid4","Wid5"};
char *buttarray[5]={"Butt1","Butt2","Butt3","Butt4","Butt5"};
int number;
CString iniTracker;
CString HelpEdit;
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWizHelp dialog


CWizHelp::CWizHelp(CWnd* pParent /*=NULL*/)
	: CDialog(CWizHelp::IDD, pParent)
{
	//{{AFX_DATA_INIT(CWizHelp)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CWizHelp::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWizHelp)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CWizHelp, CDialog)
	//{{AFX_MSG_MAP(CWizHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWizHelp message handlers

BOOL CWizHelp::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::Create(IDD, pParentWnd);
}

int CWizHelp::DoModal() 
{

	return CDialog::DoModal();
}

BOOL CWizHelp::OnInitDialog() 
{
	CDialog::OnInitDialog();
	FILE *fPtr;
	CWnd newb;
	CString pagename = CurrentNode->localVars->pageName;
	CString fname = pagename + ".ini";
	CString inifilename = CString(iniFilePath) + "\\" + fname;
	CString Hfilename;
	CString Helpfilename;
	GetPrivateProfileString(navCtrlSection, "Help", "", buffer, MAX_SIZE,inifilename );
	Hfilename =buffer;
	Helpfilename = CString(iniFilePath)+"\\" +Hfilename;
//	newb.MessageBox(Helpfilename,inifilename,MB_OK);

			if( !( fPtr = fopen( Helpfilename, "r") ) )
	{
	    CWnd myWnd;
	
		myWnd.MessageBox("Unable to open file" + Helpfilename, "ERROR", MB_OK);
//		exit( 3 );
	}

			else {
	GetPrivateProfileString(varSection, "Number", "", buffer, MAX_SIZE,Helpfilename );
	number = atoi(buffer);
	GetPrivateProfileString(varSection, "HelpText", "", buffer, MAX_SIZE,Helpfilename );
	HelpEdit = CString(buffer);

	FILE *fPtr2;

	CString Helptext = CString(iniFilePath) + "\\" + HelpEdit;
//CWnd newwindow;
//newwindow.MessageBox(HelpEdit,Helptext,MB_OK);
	//	ThatWind.MessageBox(Helptext,"something",MB_OK);
	if( !( fPtr2 = fopen(Helptext, "rb") ) )
	{
	    CWnd myWnd;
	
		myWnd.MessageBox("Unable to open file" + Helptext, "ERROR", MB_OK);
//		exit( 3 );
	}

	else 
		fseek(fPtr2, 0,2);
		long f_size = ftell(fPtr2);
		fseek (fPtr2,0,0);
		char *file_buffer;
		file_buffer = (char *) malloc (f_size);
		fread (file_buffer,1,f_size,fPtr2);
		file_buffer[f_size]=NULL;

//	ThatWind.MessageBox(file_buffer,"buffer",MB_OK);
	CEdit *ebptr =(CEdit *) GetDlgItem(ID_HELP_EDIT);
	ebptr->SetWindowText(file_buffer);

		for (int count =0; count < number; count ++)
	{

		GetPrivateProfileString(secarray[count], "Value", "", buffer, MAX_SIZE,Helpfilename );
		GlobalWid[count].value=buffer;
		GetPrivateProfileString(secarray[count], "Target", "", buffer, MAX_SIZE,Helpfilename );
		GlobalWid[count].target=buffer;
//		newb.MessageBox(GlobalWid[count].value,secarray[count],MB_OK);
//		newb.MessageBox(GlobalWid[count].target,secarray[count],MB_OK);
		GetPrivateProfileString(secarray[count], "Start_x", "", buffer, MAX_SIZE,Helpfilename );
		GlobalWid[count].location.x=atoi(buffer);
//		newb.MessageBox(buffer,secarray[count],MB_OK);
		GetPrivateProfileString(secarray[count], "Start_y", "", buffer, MAX_SIZE,Helpfilename );
		GlobalWid[count].location.y=atoi(buffer);
//		newb.MessageBox(buffer,secarray[count],MB_OK);
		GetPrivateProfileString(secarray[count], "Width", "", buffer, MAX_SIZE,Helpfilename );
		GlobalWid[count].size.width=atoi(buffer);
//		newb.MessageBox(buffer,secarray[count],MB_OK);
		GetPrivateProfileString(secarray[count], "Height", "", buffer, MAX_SIZE,Helpfilename );
		GlobalWid[count].size.height=atoi(buffer);
//		newb.MessageBox(buffer,secarray[count],MB_OK);
		GetPrivateProfileString(secarray[count], "ButtonID", "", buffer, MAX_SIZE,Helpfilename );
		GlobalWid[count].ButtonID=atoi(buffer);

		CButton* Button1;
		Button1= new CButton;

		CRect tmpRect = CRect(GlobalWid[count].location.x, GlobalWid[count].location.y, (GlobalWid[count].location.x + GlobalWid[count].size.width), (GlobalWid[count].location.y + GlobalWid[count].size.height));

//		int k=((CButton*)Button1)->Create("Button1", BS_PUSHBUTTON | WS_TABSTOP|WS_VISIBLE | WS_CHILD, tmpRect, this, ID);
		int k=Button1->Create(GlobalWid[count].value, BS_PUSHBUTTON | WS_TABSTOP|WS_VISIBLE, tmpRect, this, GlobalWid[count].ButtonID);

		
	}
/*		fseek(fPtr, 0,2);
		long f_size = ftell(fPtr);
		fseek (fPtr,0,0);
		char *file_buffer;
		file_buffer = (char *) malloc (f_size);
		fread (file_buffer,1,f_size,fPtr);
		file_buffer[f_size]=NULL;
*/

	//	ReadIniFile
	// TODO: Add extra initialization here
	// TODO: Add your specialized code here and/or call the base class

/*		int s_x = 10;
		int s_y = 10;
		int s_width = 60;
		int s_height = 25;
		int ID = 9999;
//		CWnd* Button1;
		CButton* Button1;
		Button1= new CButton;

		CRect tmpRect = CRect(s_x, s_y, (s_x + s_width), (s_y + s_height));

//		int k=((CButton*)Button1)->Create("Button1", BS_PUSHBUTTON | WS_TABSTOP|WS_VISIBLE | WS_CHILD, tmpRect, this, ID);
		int k=Button1->Create("Button", BS_PUSHBUTTON | WS_TABSTOP|WS_VISIBLE, tmpRect, this, ID);
	
		*/	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


BOOL CWizHelp::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	// TODO: Add your specialized code here and/or call the base class
	for (int integ =0; integ <number; integ ++)
	{
		if (GlobalWid[integ].ButtonID== (int)wParam)
		{//CWnd newwindow;
		//	newwindow.MessageBox(GlobalWid[integ].target,"newbox",MB_OK);
			iniTracker = GlobalWid[integ].target;
		//	newwindow.MessageBox(iniTracker,"newbox",MB_OK);
			CHelpDlg helpdlg;
			helpdlg.DoModal();


		}
	}
	return CDialog::OnCommand(wParam, lParam);
}
