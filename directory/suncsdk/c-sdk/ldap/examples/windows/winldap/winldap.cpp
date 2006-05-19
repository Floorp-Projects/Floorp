/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1996-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

// winldap.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "ldap.h"
#include "winldap.h"

#include "MainFrm.h"
#include "LdapDoc.h"
#include "LdapView.h"
#include "ConnDlg.h"
#include "SrchDlg.h"

#ifdef _DEBUG
#ifdef _WIN32
#define new DEBUG_NEW
#endif
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// LdapApp

BEGIN_MESSAGE_MAP(LdapApp, CWinApp)
	//{{AFX_MSG_MAP(LdapApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_CONNECT, OnFileConnect)
	ON_UPDATE_COMMAND_UI(ID_FILE_CONNECT, OnUpdateFileConnect)
	ON_COMMAND(ID_FILE_DISCONNECT, OnFileDisconnect)
	ON_UPDATE_COMMAND_UI(ID_FILE_DISCONNECT, OnUpdateFileDisconnect)
	ON_COMMAND(ID_FILE_SEARCH, OnFileSearch)
	ON_UPDATE_COMMAND_UI(ID_FILE_SEARCH, OnUpdateFileSearch)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LdapApp construction

LdapApp::LdapApp()
{
	m_ld = NULL;
	m_connected = FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// The one and only LdapApp object

LdapApp theApp;

/////////////////////////////////////////////////////////////////////////////
// LdapApp initialization

BOOL LdapApp::InitInstance()
{
	// Standard initialization
#ifdef _WIN32
#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
#endif

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register document templates

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(LdapDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(LdapView));
	AddDocTemplate(pDocTemplate);

#ifdef _WIN32
	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;
#else
	// create a new (empty) document
	OnFileNew();

	if (m_lpCmdLine[0] != '\0')
	{
	}
#endif

	m_dirHost = GetProfileString( "Connection", "host", "localhost" );
	m_dirPort = GetProfileInt( "Connection", "port", 389 );
	m_searchBase = GetProfileString( "Search", "base", "dc=example,dc=com" );

	m_scope = GetProfileInt( "Search", "scope", LDAP_SCOPE_SUBTREE );
	m_searchFilter = GetProfileString( "Search", "filter", "objectclass=*" );

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void LdapApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// LdapApp commands

// Initialize an anonymous connection to a Directory Server
void LdapApp::OnFileConnect() 
{
	ConnDlg dlg;
	dlg.m_dirHost = m_dirHost;
	dlg.m_dirPort = m_dirPort;
	if ( IDOK == dlg.DoModal() )
	{
		m_dirHost = dlg.m_dirHost;
		m_dirPort = dlg.m_dirPort;
		m_ld = ldap_init( m_dirHost, m_dirPort );
		if ( NULL != m_ld )
		{
			// Bind as anonymous
			if ( ldap_bind_s( m_ld, "", "", LDAP_AUTH_SIMPLE )
					!= LDAP_SUCCESS )
			{
				AfxMessageBox( "Error binding!" );
				return;
			}
		}
		else
		{
			AfxMessageBox( "Error connecting!" );
			return;
		}
		m_connected = TRUE;
		LdapView *view = (LdapView *)((CFrameWnd *)AfxGetMainWnd())->GetActiveView();
		if ( view )
		{
			CString title;
			title.Format( "Host %s, port %d", m_dirHost, m_dirPort );
			view->GetDocument()->SetTitle( title );
		}
	}
}

void LdapApp::OnUpdateFileConnect(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( !IsConnected() );
}

// Disconnect (unbind from) a Directory Server
void LdapApp::OnFileDisconnect() 
{
	ldap_unbind( m_ld );
	m_ld = NULL;
	m_connected = FALSE;
	LdapView *view = (LdapView *)((CFrameWnd *)AfxGetMainWnd())->GetActiveView();
	if ( view )
	{
		view->GetDocument()->SetTitle( "" );
	}
}

void LdapApp::OnUpdateFileDisconnect(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( IsConnected() );
}

// Search for and report distinguished names (no attributes)
void LdapApp::OnFileSearch() 
{
	SearchDlg dlg;
	dlg.m_searchBase = m_searchBase;
	dlg.SetScope( m_scope );
	dlg.m_searchFilter = m_searchFilter;
	// Get desired search parameters
	if ( IDOK == dlg.DoModal() )
	{
		m_searchBase = dlg.m_searchBase;
		m_searchFilter = dlg.m_searchFilter;
		m_scope = dlg.GetScope();
		// Clear the result list
		LdapView *view = (LdapView *)((CFrameWnd *)AfxGetMainWnd())->GetActiveView();
		if ( view )
			view->ClearLines();
		char *attrs[2];
		// "dn" is a pseudo-attribute; it is always returned anyway, but not as
		// an attribute
		attrs[0] = "dn";
		attrs[1] = NULL;
		if ( ldap_search( m_ld, m_searchBase, m_scope, m_searchFilter,
				attrs, FALSE ) == -1 )
		{
			AfxMessageBox( "Failed to start asynchronous search" );
			return;
		}
		LDAPMessage *res;
		int rc;
		// Fetch all results as they become available
		while ( (rc = ldap_result( m_ld, LDAP_RES_ANY, 0, NULL, &res ))
			== LDAP_RES_SEARCH_ENTRY )
		{
			LDAPMessage *e = ldap_first_entry( m_ld, res );
			// Get the distinguished name and show it
			char *dn = ldap_get_dn( m_ld, e );
			if ( view )
				view->AddLine( dn, dn );
			ldap_memfree( dn );
			ldap_msgfree( res );
			// Let the view be updated
#ifdef _WIN32
			Sleep( 0 );
#else
			Yield();
#endif
		}
		if ( rc == -1 )
		{
			AfxMessageBox( "Error on ldap_result" );
			return;
	    }
		else if (( rc = ldap_result2error( m_ld, res, 0 )) != LDAP_SUCCESS )
		{
			char *errString = ldap_err2string( rc );
			AfxMessageBox( errString );
	    }
		ldap_msgfree( res );
	}
}

void LdapApp::OnUpdateFileSearch(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( IsConnected() );
}

int LdapApp::ExitInstance() 
{
	WriteProfileString( "Connection", "host", m_dirHost );
	WriteProfileInt( "Connection", "port", m_dirPort );
	WriteProfileString( "Search", "base", m_searchBase );
	WriteProfileInt( "Search", "scope", m_scope );
	WriteProfileString( "Search", "filter", m_searchFilter );
	
	return CWinApp::ExitInstance();
}
