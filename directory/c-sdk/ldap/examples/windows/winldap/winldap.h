// winldap.h : main header file for the WINLDAP application
//

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

#ifndef _LDAP_H
struct LDAP;
#endif

/////////////////////////////////////////////////////////////////////////////
// LdapApp:
// See winldap.cpp for the implementation of this class
//

class LdapApp : public CWinApp
{
public:
	LdapApp();

public:
	BOOL IsConnected() { return m_connected; }
	LDAP *GetConnection() { return m_ld; }

private:
	BOOL m_connected;
	CString	m_dirHost;
	int		m_dirPort;
	CString	m_searchBase;
	CString	m_searchFilter;
	int		m_scope;
	LDAP	*m_ld;


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(LdapApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(LdapApp)
	afx_msg void OnAppAbout();
	afx_msg void OnFileConnect();
	afx_msg void OnUpdateFileConnect(CCmdUI* pCmdUI);
	afx_msg void OnFileDisconnect();
	afx_msg void OnUpdateFileDisconnect(CCmdUI* pCmdUI);
	afx_msg void OnFileSearch();
	afx_msg void OnUpdateFileSearch(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
