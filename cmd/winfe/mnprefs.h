/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _MNPREFS_H
#define _MNPREFS_H

#ifdef MOZ_MAIL_NEWS
#include "property.h"
#include "dirprefs.h"
#include "mailmisc.h"

#define MAX_HOSTNAME_LEN	 256	//include '\0'
#define MAX_DESCRIPTION_LEN	1024	//include '\0'
#define MAX_PORT_NUMBER		65535

typedef enum
{
  FROM_SUBSCRIBEUI,
  FROM_FOLDERPANE,
  FROM_PREFERENCE
};

class CMailFolderCombo;
class CMailServerPropertySheet;

class CChooseFolderDialog : public CDialog 
{
// Attributes
public:

	CString m_szFolder;
	CString m_szServer;
	CString m_szPrefUrl;

	CChooseFolderDialog(CWnd* pParent = NULL, char *pFolderPath = NULL, int nType = 0 );

    enum { IDD = IDD_PREF_CHOOSE_FOLDER};


	//{{AFX_VIRTUAL(CChooseFolderDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
 
	int m_nDefaultID;
	CMailFolderCombo m_FolderCombo;
	CMailFolderCombo m_ServerCombo;
	char* m_pFolderPath;

    virtual void OnOK();
	virtual BOOL OnInitDialog();

	afx_msg void OnNewFolder();
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CGeneralServerPage  
//
class CGeneralServerPage : public CPropertyPage
{
public:

    CGeneralServerPage(CWnd *pParent, const char* pName);
	
	enum { IDD = IDD_MAILSERVER_GENERAL };


	CMailServerPropertySheet* m_pParent;

	BOOL ProcessOK();

	//{{AFX_VIRTUAL(CGeneralServerPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

protected: 

	CString m_szServerName;

	afx_msg void OnChangeServerType();
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CPopServerPage  
class CPopServerPage : public CPropertyPage
{
public:

    CPopServerPage(CWnd *pParent);
	
	enum { IDD = IDD_MAILSERVER_POP };

	BOOL ProcessOK();

	//{{AFX_VIRTUAL(CPopServerPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

protected: 

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CIMAPServerPage  
class CIMAPServerPage : public CPropertyPage
{
public:

    CIMAPServerPage(CWnd *pParent, const char* pName);
	
	enum { IDD = IDD_MAILSERVER_IMAP };

	CMailServerPropertySheet* m_pParent;
	XP_Bool	GetUseSSL();
	BOOL ProcessOK();

	//{{AFX_VIRTUAL(CIMAPServerPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

protected: 

	CString m_szServerName;

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CIMAPAdvancedPage  
class CIMAPAdvancedPage : public CPropertyPage
{
public:

    CIMAPAdvancedPage(CWnd *pParent, const char* pName);
	
	enum { IDD = IDD_MAILSERVER_ADVANCED };

	CMailServerPropertySheet* m_pParent;

	void GetPersonalDir(char* pDir, int nLen);
	void GetPublicDir(char* pDir, int nLen);
	void GetOthersDir(char* pDir, int nLen);
	XP_Bool GetOverrideNameSpaces();
	BOOL ProcessOK();


	//{{AFX_VIRTUAL(CIMAPAdvancedPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

protected: 

	CString m_szServerName;

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CMailServerPropertySheet
//
class CMailServerPropertySheet : public CPropertySheet
{
public:

    CMailServerPropertySheet(CWnd *pParent, const char* pTitle, const char* pName, 
					int nType = -1, BOOL bEdit = FALSE, BOOL bBothType = FALSE);
    ~CMailServerPropertySheet();
	
	BOOL	IsPopServer() { return m_bPop; }
	BOOL	EditServer() { return m_bEdit; }
	BOOL	AllowBothTypes() { return m_bBothType; }
	char*	GetMailHostName() { return m_hostName; }
	void	SetMailHostName(char* pName);
	XP_Bool	GetIMAPUseSSL();
	void	GetIMAPPersonalDir(char* pDir, int nLen);
	void	GetIMAPPublicDir(char* pDir, int nLen);
	void	GetIMAPOthersDir(char* pDir, int nLen);
	XP_Bool GetIMAPOverrideNameSpaces();

	void	ShowHidePages(int nShowType);

	//In Win16, GetActivePage() is a protected
	CPropertyPage*  GetCurrentPage() 
		{ return (CPropertyPage*)GetActivePage();	}

	virtual void OnHelp();

protected:

	BOOL m_bPop;
	BOOL m_bEdit;
	BOOL m_bBothType;
	CGeneralServerPage* m_pGeneralPage;
	CPopServerPage* m_pPopPage;
	CIMAPServerPage* m_pIMAPPage;
	CIMAPAdvancedPage* m_pAdvancedPage;
	char m_hostName[MAX_HOSTNAME_LEN];

	BOOL IsPageValid(CPropertyPage* pPage);

#ifdef _WIN32
	virtual BOOL OnInitDialog();
#else
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
#endif
	afx_msg void OnOK();
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CNewsServerDialog
//
class CNewsServerDialog : public CDialog 
{
// Attributes
public:

	CNewsServerDialog(CWnd* pParent, const char* pName, int nFromWhere, MSG_NewsHost *pHost = NULL);

    enum { IDD = IDD_NEWSGROUP_ADDSERVER };

	char* GetNewsHostName() { return m_hostName; }
	XP_Bool GetSecure() { return m_bIsSecure; }
	XP_Bool GetAuthentication() { return m_bAuthentication; }
	int32 GetNewsHostPort() { return m_lPort; }

	//{{AFX_VIRTUAL(CNewNewsgroupsPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

	// Implementation
protected:
 
	char	m_hostName[MSG_MAXGROUPNAMELENGTH];
	XP_Bool	m_bIsSecure;
	XP_Bool	m_bAuthentication;
	int 	m_nFromWhere;
	int32	m_lPort;
	MSG_NewsHost* m_pEditHost;

	BOOL NewsHostExists();
	BOOL IsSameServer(MSG_Host *pHost);
	int32 GetPortNumber();

	afx_msg void OnOK();
	afx_msg void OnCheckSecure();
	afx_msg void OnHelp();
	DECLARE_MESSAGE_MAP()
};

#endif /* MOZ_MAIL_NEWS */

class CDirDialog : public CFileDialog 
{
// Attributes
public:

	CDirDialog(CWnd* pParent, LPCTSTR pInitDir);	

	//{{AFX_VIRTUAL(CDirDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	char m_szIniFile[1024];

	DECLARE_MESSAGE_MAP()
};

#endif // _MNPREFS_H
