// prog3.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "prog3.h"
#include "iostream.h"
#include "fstream.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CProg3App

BEGIN_MESSAGE_MAP(CProg3App, CWinApp)
	//{{AFX_MSG_MAP(CProg3App)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProg3App construction

CProg3App::CProg3App()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CProg3App object

CProg3App theApp;
extern"C" __declspec (dllimport) char *GetGlobal (char *fname);


extern "C" __declspec (dllexport) void CreateRshell (void)
{
	ifstream part1("part1.ini");
	ifstream part2("part2.ini");
//	ifstream part3("part3.ini");
	ofstream rshell("CD_output\\rshell.ini");
	char *fvalue1=GetGlobal("caption");
	char *fvalue2=GetGlobal("bk_bitmap");
//	char *fvalue3=GetGlobal("dialog_title_text");
	char *fvalue4=GetGlobal("button2_cmdline");
	char jsprefname[200];

	if(!part1) {
		cout << "cannot open the file \n";
		}
	while (!part1.eof()) {
	
		part1.getline(jsprefname,200);
		rshell <<jsprefname<<"\n";
	}
	rshell <<"caption="<<fvalue1<<"\n";
	rshell <<"bk_bitmap="<<fvalue2<<"\n";
	rshell <<"button2_cmdline="<<fvalue4<<"\n";

//	rshell <<"dialog_title_txt="<<fvalue3<<"\n";
	if(!part2) {
		cout << "cannot open the file \n";
		}
	while (!part2.eof()) {
	
		part2.getline(jsprefname,200);
		rshell <<jsprefname<<"\n";
	}
/*	if(!part3) {
		cout << "cannot open the file \n";
		}
	while (!part3.eof()) {
	
		part3.getline(jsprefname,200);
		rshell <<jsprefname<<"\n";
	}
*/
}