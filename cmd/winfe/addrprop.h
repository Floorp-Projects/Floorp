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

#include "addrfrm.h"

#ifdef MOZ_NEWADDR

#ifndef _addrprop_h_
#define _addrprop_h_

// ADDRFRM.H
//
// DESCRIPTION:
//		This file contains the declarations of the various address book related
//		classes.
//

#include "outliner.h"
#include "apimsg.h"
#include "xp_core.h"
#include "abcom.h"
#include "property.h"
#include "abmldlg.h"
#include "mailfrm.h"
#include "mnrccln.h"

// above the range for normal EN_ messages
#define PEN_ILLEGALCHAR     0x8000
			// sent to parent when illegal character hit
			// return 0 if you want parsed edit to beep

/****************************************************************************
*
*	Class: CAddrEditProperties
*
*	DESCRIPTION:
*		This class is the property sheet window for editing all the attributes
*		of the people types in the address book
*
****************************************************************************/
class CAddrEditProperties : public  CNetscapePropertySheet
{
protected:
	ABID	m_entryID;

    CPropertyPage * m_pUserProperties;
	CPropertyPage * m_pContact;
	CPropertyPage * m_pSecurity;
	CPropertyPage * m_pCooltalk;
	CMailNewsResourceSwitcher m_MailNewsResourceSwitcher;
	MSG_Pane * m_pPane;
	//is this a new card
	BOOL m_bNew;

public:
	MWContext* m_context;
	HFONT		m_pFont;

public:
    CAddrEditProperties ( CAddrFrame* frameref, LPCTSTR lpszCaption, 
		CWnd * parent = NULL, MSG_Pane * pane = NULL, 
		MWContext* context = NULL, BOOL bNew = FALSE); 
    virtual ~CAddrEditProperties ( );    
	virtual void OnHelp();
	virtual int DoModal();
	virtual BOOL Create( CWnd* pParentWnd = NULL, DWORD dwStyle = (DWORD)1, DWORD dwExStyle = 0 );

	void SetAttributes(MSG_Pane *pane);
	HFONT GetHFont() { return (m_pFont); }
	void CloseWindow ();

protected:
	CAddrFrame* m_frame;
//    virtual ~CAddrEditProperties ( );    

	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddrEditProperties)
	public:
	virtual void OnOK();
	afx_msg void OnDestroy( );
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrEditProperties)
		// NOTE: the ClassWizard will add member functions here
	afx_msg int OnCreate( LPCREATESTRUCT );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CAddressUser
*
*	DESCRIPTION:
*		This class is the property page for editing the most common attributes
*		of the people types in the address book
*
****************************************************************************/
class CAddressUser : public CNetscapePropertyPage
{

// Construction
public:
	CAddressUser(CWnd *pParent, BOOL bNew);
	virtual ~CAddressUser();

	BOOL	m_bActivated;
	BOOL	m_bNew;
	BOOL	m_bUserChangedDisplay;

// Dialog Data
	//{{AFX_DATA(CAddressUser)
	enum { IDD = IDD_ADDRESS_USER };
	CString	m_address;
	CString	m_description;
	CString	m_firstname;
	CString	m_lastname;
	CString	m_nickname;
	int		m_useHTML;
	CString	m_company;
	CString	m_title;
	CString m_displayname;
	CString m_department;
	//}}AFX_DATA

	void SetAttributes(MSG_Pane *pane);
	BOOL PerformOnOK(MSG_Pane *pane);
	void SetFonts( HFONT pFont );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddressUser)
	public:
	virtual BOOL OnInitDialog();
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddressUser)
		// NOTE: the ClassWizard will add member functions here
		afx_msg void OnCloseWindow();
		afx_msg void OnNameTextChange();
		afx_msg void OnDisplayNameTextChange();
		//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

};


/****************************************************************************
*
*	Class: CAddressContact
*
*	DESCRIPTION:
*		This class is the property page for editing the contact info
*		of the people types in the address book
*
****************************************************************************/
class CAddressContact : public CNetscapePropertyPage
{

// Construction
public:
	CAddressContact(CWnd *pParent);
	virtual ~CAddressContact();

	BOOL	m_bActivated;

// Dialog Data
	//{{AFX_DATA(CAddressContact)
	enum { IDD = IDD_ADDRESS_CONTACT };
	CString m_poaddress;
	CString	m_address;
	CString	m_locality;
	CString	m_region;
	CString	m_zip;
	CString m_country;
	CString	m_work;
	CString	m_fax;
	CString	m_home;
	CString m_pager;
	CString m_cellular;
	//}}AFX_DATA

	void SetAttributes(MSG_Pane *pane);
	BOOL PerformOnOK(MSG_Pane *pane);
	void SetFonts( HFONT pFont );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddressContact)
	public:
	virtual BOOL OnInitDialog();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddressContact)
		// NOTE: the ClassWizard will add member functions here
		afx_msg void OnCloseWindow();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

};

/****************************************************************************
*
*	Class: CAddressCooltalk
*
*	DESCRIPTION:
*		This class is the property page for editing and accessing the cool
*		talk attributes for a person object.
*
****************************************************************************/
class CAddressCooltalk : public CNetscapePropertyPage
{
// Construction
public:
	CAddressCooltalk(CWnd *pParent);
	virtual ~CAddressCooltalk();

	BOOL m_bActivated;

	void SetAttributes(MSG_Pane *pane);
	BOOL PerformOnOK(MSG_Pane *pane);
	void SetExplanationText();
	void SetFonts( HFONT pFont );

// Dialog Data
	//{{AFX_DATA(CAddressCooltalk)
	enum { IDD = IDD_ADDRESS_COOLTALK };
	CString	m_ipaddress;
	CString m_specificDLS;
	CString m_hostorIP;
	int		m_iUseServer;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddressCooltalk)
	public:
	virtual BOOL OnInitDialog();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddressCooltalk)
	afx_msg void OnCloseWindow();
	afx_msg void OnSelendokServer();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

/****************************************************************************
*
*	Class: CAddressSecurity
*
*	DESCRIPTION:
*		This class is the property page for editing and accessing the security
*		attributes for a person object.
*
****************************************************************************/
class CAddressSecurity : public CNetscapePropertyPage
{
// Construction
public:
	CAddressSecurity(CWnd *pParent);   // standard constructor
	virtual ~CAddressSecurity();

	BOOL m_bActivated;

	void SetAttributes(MSG_Pane *pane);
	BOOL PerformOnOK(MSG_Pane *pane);

// Dialog Data
	//{{AFX_DATA(CAddressSecurity)
	enum { IDD = IDD_ADDRESS_SECURITY };
	CString	m_explain1;
	CString	m_explain2;
	CString m_explain3;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddressSecurity)
	public:
	virtual BOOL OnInitDialog();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddressSecurity)
		// NOTE: the ClassWizard will add member functions here
		afx_msg void OnCloseWindow();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/****************************************************************************
*
*	Class: CAddrLDAPProperties
*
*	DESCRIPTION:
*		This class is the property sheet window for editing all the attributes
*		of the people types in the address book
*
****************************************************************************/
class CAddrLDAPProperties : public  CNetscapePropertySheet
{
protected:
    CPropertyPage * m_pLDAPProperties;
	CPropertyPage * m_pOfflineProperties;
	CMailNewsResourceSwitcher m_MailNewsResourceSwitcher;

public:
	DIR_Server* m_pExistServer;
	DIR_Server	m_serverInfo;
	HFONT		m_pFont;
	MWContext*  m_context;

public:
    CAddrLDAPProperties (CWnd * parent,
		MWContext* context,
		DIR_Server*  dir = NULL,
		LPCTSTR lpszCaption = NULL); 
    virtual ~CAddrLDAPProperties ( );    
	virtual void OnHelp();
	virtual int DoModal();
	HFONT GetHFont() { return (m_pFont); }

protected:
	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddrLDAPProperties)
	public:
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrLDAPProperties)
		// NOTE: the ClassWizard will add member functions here
	afx_msg int OnCreate( LPCREATESTRUCT );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

class CServerDialog : public CNetscapePropertyPage 
{
// Attributes
public:

	CServerDialog(CWnd* pParent = NULL, DIR_Server *pExistServer = NULL, DIR_Server *pNewServer = NULL);

    enum { IDD = IDD_PREF_LDAP_SERVER};

	DIR_Server	*m_pExistServer;
	DIR_Server	*m_serverInfo;
	BOOL m_bActivated;

	//{{AFX_VIRTUAL(CServerDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

    BOOL ValidDataInput();

	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CServerDialog)
	public:
	virtual void OnOK();
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

	afx_msg void OnCheckSecure();
	afx_msg void OnEnableLoginLDAP();
	afx_msg void OnHelp();
	DECLARE_MESSAGE_MAP()
};

class CServerOfflineDialog : public CNetscapePropertyPage 
{
// Attributes
public:

	CServerOfflineDialog(CWnd* pParent = NULL, DIR_Server *pServer = NULL, DIR_Server *pNewServer = NULL);

    enum { IDD = IDD_PREF_LDAP_OFFLINE_SETTINGS};

	DIR_Server	*m_pExistServer;
	DIR_Server	*m_serverInfo;
	BOOL m_bActivated;

	//{{AFX_VIRTUAL(CServerOfflineDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CServerOfflineDialog)
	public:
	virtual void OnOK();
	virtual BOOL OnSetActive();
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

	afx_msg void OnUpdateNow();
	afx_msg void OnHelp();
	DECLARE_MESSAGE_MAP()
};


#endif


#else //MOZ_NEWADDR
#ifndef _addrprop_h_
#define _addrprop_h_

// ADDRFRM.H
//
// DESCRIPTION:
//		This file contains the declarations of the various address book related
//		classes.
//

#include "outliner.h"
#include "apimsg.h"
#include "xp_core.h"
#include "addrbook.h"
#include "property.h"
#include "abmldlg.h"
#include "mailfrm.h"
#include "mnrccln.h"

// above the range for normal EN_ messages
#define PEN_ILLEGALCHAR     0x8000
			// sent to parent when illegal character hit
			// return 0 if you want parsed edit to beep

/****************************************************************************
*
*	Class: CAddrEditProperties
*
*	DESCRIPTION:
*		This class is the property sheet window for editing all the attributes
*		of the people types in the address book
*
****************************************************************************/
class CAddrEditProperties : public  CNetscapePropertySheet
{
protected:
	ABID	m_entryID;

    CPropertyPage * m_pUserProperties;
	CPropertyPage * m_pContact;
	CPropertyPage * m_pSecurity;
	CPropertyPage * m_pCooltalk;
	CMailNewsResourceSwitcher m_MailNewsResourceSwitcher;

public:
	DIR_Server* m_dir;
	MWContext* m_context;
	HFONT		m_pFont;
	PersonEntry* m_pPerson;

public:
    CAddrEditProperties ( CAddrFrame* frameref, DIR_Server*  dir,
		LPCTSTR lpszCaption, 
		CWnd * parent = NULL, ABID entryID = NULL, 
		PersonEntry* person = NULL,
		MWContext* context = NULL); 
    virtual ~CAddrEditProperties ( );    
	virtual void OnHelp();
	virtual int DoModal();
	virtual BOOL Create( CWnd* pParentWnd = NULL, DWORD dwStyle = (DWORD)1, DWORD dwExStyle = 0 );

	void SetEntryID(DIR_Server* dir, ABID entryID = NULL, PersonEntry* person = NULL);
	ABID GetEntryID()	{ return (m_entryID); }	
	DIR_Server* GetDirectoryServer() { return (m_dir); }
	HFONT GetHFont() { return (m_pFont); }
	void CloseWindow ();

protected:
	CAddrFrame* m_frame;
//    virtual ~CAddrEditProperties ( );    

	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddrEditProperties)
	public:
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrEditProperties)
		// NOTE: the ClassWizard will add member functions here
	afx_msg int OnCreate( LPCREATESTRUCT );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CAddressUser
*
*	DESCRIPTION:
*		This class is the property page for editing the most common attributes
*		of the people types in the address book
*
****************************************************************************/
class CAddressUser : public CNetscapePropertyPage
{

// Construction
public:
	CAddressUser(CWnd *pParent);
	virtual ~CAddressUser();

	BOOL	m_bActivated;
	DIR_Server* m_dir;

// Dialog Data
	//{{AFX_DATA(CAddressUser)
	enum { IDD = IDD_ADDRESS_USER };
	CString	m_address;
	CString	m_description;
	CString	m_firstname;
	CString	m_lastname;
	CString	m_nickname;
	int		m_useHTML;
	CString	m_company;
	CString	m_title;
	CString m_department;
	CString m_displayname;
	//}}AFX_DATA

	void SetEntryID(DIR_Server* dir, ABID entryID = NULL, PersonEntry* person = NULL);
	BOOL PerformOnOK(PersonEntry* person);
	void SetFonts( HFONT pFont );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddressUser)
	public:
	virtual BOOL OnInitDialog();
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddressUser)
		// NOTE: the ClassWizard will add member functions here
		afx_msg void OnCloseWindow();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

};


/****************************************************************************
*
*	Class: CAddressContact
*
*	DESCRIPTION:
*		This class is the property page for editing the contact info
*		of the people types in the address book
*
****************************************************************************/
class CAddressContact : public CNetscapePropertyPage
{

// Construction
public:
	CAddressContact(CWnd *pParent);
	virtual ~CAddressContact();

	BOOL	m_bActivated;
	DIR_Server* m_dir;

// Dialog Data
	//{{AFX_DATA(CAddressContact)
	enum { IDD = IDD_ADDRESS_CONTACT };
	CString m_poaddress;
	CString	m_address;
	CString	m_locality;
	CString	m_region;
	CString	m_zip;
	CString m_country;
	CString	m_work;
	CString	m_fax;
	CString	m_home;
	CString m_pager;
	CString m_cellular;
	//}}AFX_DATA

	void SetEntryID(DIR_Server* dir, ABID entryID = NULL, PersonEntry* person = NULL);
	BOOL PerformOnOK(PersonEntry* person);
	void SetFonts( HFONT pFont );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddressContact)
	public:
	virtual BOOL OnInitDialog();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddressContact)
		// NOTE: the ClassWizard will add member functions here
		afx_msg void OnCloseWindow();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

};

/****************************************************************************
*
*	Class: CAddressCooltalk
*
*	DESCRIPTION:
*		This class is the property page for editing and accessing the cool
*		talk attributes for a person object.
*
****************************************************************************/
class CAddressCooltalk : public CNetscapePropertyPage
{
// Construction
public:
	CAddressCooltalk(CWnd *pParent);
	virtual ~CAddressCooltalk();

	BOOL m_bActivated;
	DIR_Server* m_dir;

	void SetEntryID(DIR_Server* dir, ABID entryID = NULL, PersonEntry* person = NULL);
	BOOL PerformOnOK(PersonEntry* person);
	void SetExplanationText();
	void SetFonts( HFONT pFont );

// Dialog Data
	//{{AFX_DATA(CAddressCooltalk)
	enum { IDD = IDD_ADDRESS_COOLTALK };
	CString	m_ipaddress;
	CString m_specificDLS;
	CString m_hostorIP;
	int		m_iUseServer;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddressCooltalk)
	public:
	virtual BOOL OnInitDialog();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddressCooltalk)
	afx_msg void OnCloseWindow();
	afx_msg void OnSelendokServer();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


/****************************************************************************
*
*	Class: CAddrLDAPProperties
*
*	DESCRIPTION:
*		This class is the property sheet window for editing all the attributes
*		of the people types in the address book
*
****************************************************************************/
class CAddrLDAPProperties : public  CNetscapePropertySheet
{
protected:
    CPropertyPage * m_pLDAPProperties;
	CPropertyPage * m_pOfflineProperties;
	CMailNewsResourceSwitcher m_MailNewsResourceSwitcher;

public:
	DIR_Server* m_pExistServer;
	DIR_Server	m_serverInfo;
	HFONT		m_pFont;
	MWContext*  m_context;

public:
    CAddrLDAPProperties (CWnd * parent,
		MWContext* context,
		DIR_Server*  dir = NULL,
		LPCTSTR lpszCaption = NULL); 
    virtual ~CAddrLDAPProperties ( );    
	virtual void OnHelp();
	virtual int DoModal();

	HFONT GetHFont() { return (m_pFont); }

protected:
	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAddrLDAPProperties)
	public:
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrLDAPProperties)
		// NOTE: the ClassWizard will add member functions here
	afx_msg int OnCreate( LPCREATESTRUCT );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

class CServerDialog : public CNetscapePropertyPage 
{
// Attributes
public:

	CServerDialog(CWnd* pParent = NULL, DIR_Server *pExistServer = NULL, DIR_Server *pNewServer = NULL);

    enum { IDD = IDD_PREF_LDAP_SERVER};

	DIR_Server	*m_pExistServer;
	DIR_Server	*m_serverInfo;
	BOOL m_bActivated;

	//{{AFX_VIRTUAL(CServerDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

    BOOL ValidDataInput();

	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CServerDialog)
	public:
	virtual void OnOK();
	virtual BOOL OnKillActive();
	virtual BOOL OnSetActive();
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

	afx_msg void OnCheckSecure();
	afx_msg void OnEnableLoginLDAP();
	afx_msg void OnHelp();
	DECLARE_MESSAGE_MAP()
};

class CServerOfflineDialog : public CNetscapePropertyPage 
{
// Attributes
public:

	CServerOfflineDialog(CWnd* pParent = NULL, DIR_Server *pServer = NULL, DIR_Server *pNewServer = NULL);

    enum { IDD = IDD_PREF_LDAP_OFFLINE_SETTINGS};

	DIR_Server	*m_pExistServer;
	DIR_Server	*m_serverInfo;
	BOOL m_bActivated;

	//{{AFX_VIRTUAL(CServerOfflineDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CServerOfflineDialog)
	public:
	virtual void OnOK();
	virtual BOOL OnSetActive();
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

	afx_msg void OnUpdateNow();
	afx_msg void OnHelp();
	DECLARE_MESSAGE_MAP()
};


#endif
#endif // MOZ_NEWADDR
