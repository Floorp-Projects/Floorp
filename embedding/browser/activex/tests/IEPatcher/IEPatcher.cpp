// IEPatcher.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "IEPatcher.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherApp

BEGIN_MESSAGE_MAP(CIEPatcherApp, CWinApp)
	//{{AFX_MSG_MAP(CIEPatcherApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherApp construction

CIEPatcherApp::CIEPatcherApp()
{
	m_pIEPatcherDlg = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CIEPatcherApp object

CIEPatcherApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherApp initialization

BOOL CIEPatcherApp::InitInstance()
{
	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Parse the command line to see if launched as OLE server
	if (RunEmbedded() || RunAutomated())
	{
		// Register all OLE server (factories) as running.  This enables the
		//  OLE libraries to create objects from other applications.
		COleTemplateServer::RegisterAll();
	}
	else
	{
		// When a server application is launched stand-alone, it is a good idea
		//  to update the system registry in case it has been damaged.
		COleObjectFactory::UpdateRegistryAll();
	}

	// Now the scanner window
	m_pIEPatcherDlg = new CIEPatcherDlg;
	m_pMainWnd = m_pIEPatcherDlg;
	int nResponse = m_pIEPatcherDlg->DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
	delete m_pIEPatcherDlg;

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
