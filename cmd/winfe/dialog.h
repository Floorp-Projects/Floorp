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

#ifndef _DIALOG_H_
#define _DIALOG_H_

/////////////////////////////////////////////////////////////////////////////
// CDialogURL dialog

class FAR CDialogURL : public CDialog
{

protected:
    MWContext * m_Context;

public:
    CDialogURL(CWnd *pParent, MWContext * context = NULL);
    void SetContext(MWContext * context) {m_Context = context;}

    enum { IDD = IDD_OPENURL_BOX };

    CString m_csURL;

// Implementation
protected:
    CWnd      * m_Parent;

    virtual void OnOK();
    
    afx_msg void OnBrowseForFile();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	// Generated message map functions
	//{{AFX_MSG(CDialogURL)
	// NOTE: the ClassWizard will add member functions here
		afx_msg void OnHelp();
	//}}AFX_MSG

	BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogLicense dialog

class FAR CDialogLicense : public CDialog
{
// Construction
public:
    CDialogLicense(CWnd* pParent = NULL);   // standard constructor
    int DoModal();

// Dialog Data
    enum { IDD = IDD_LICENSE };

// Text Font
    CFont m_cfTextFont;
// Implementation
protected:
	afx_msg int  OnInitDialog();
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogSecurity dialog

class FAR CDialogSecurity : public CDialog
{
// Construction
public: 
	int m_Type;
	XP_Bool *returnpref;
    CDialogSecurity(int myType, XP_Bool *returnPref, CWnd* pParent = NULL);   // standard constructor
    int DoModal();

// Dialog Data
    enum { IDD = IDD_SECURITY };

// Implementation
protected:
	afx_msg int  OnInitDialog();
	afx_msg void OnOK();
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogPRMT dialog

class FAR CDialogPRMT : public CDialog
{
protected:
	CString m_csCaption;

// Construction
public:
    CDialogPRMT(CWnd* pParent = NULL);  // standard constructor
    char * DoModal(const char * Msg, const char * Dflt, const char *pszCaption = NULL);

    void SetSecureTitle( CString &csTitle ) { m_csTitle = csTitle; }
    
// Dialog Data
    //{{AFX_DATA(CDialogPRMT)
    enum { IDD = IDD_PROMPT };
    CString m_csAsk;
    CString m_csAns;
    //}}AFX_DATA

    CString m_csTitle;
    
// Implementation
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg int  OnInitDialog();
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogPASS dialog

class FAR CDialogPASS : public CDialog
{
// Construction
public:
    CDialogPASS(CWnd* pParent = NULL);  // standard constructor
    char * DoModal(const char * Msg);

    void SetSecureTitle( CString &csTitle ) { m_csTitle = csTitle; }
    
// Dialog Data
    //{{AFX_DATA(CDialogPASS)
    enum { IDD = IDD_PROMPT_PASSWD };
    CString m_csAsk;
    CString m_csAns;
    //}}AFX_DATA

    CString m_csTitle;
    
// Implementation
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	BOOL OnInitDialog();

    // Generated message map functions
    //{{AFX_MSG(CDialogPASS)
        // NOTE: the ClassWizard will add member functions here
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CDialogUPass dialog

class CDialogUPass : public CDialog
{
// Construction
public:
	CDialogUPass(CWnd* pParent = NULL);	// standard constructor
	int DoModal(char * message, char ** user, char ** passwd);

    void SetSecureTitle( CString &csTitle ) { m_csTitle = csTitle; }
    
// Dialog Data
	//{{AFX_DATA(CDialogUPass)
	enum { IDD = IDD_USERPASS };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

    CString m_csTitle;
    
// Implementation
protected:

	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	
	CString m_csUser, m_csPasswd, m_csMessage;

	// Generated message map functions
	//{{AFX_MSG(CDialogUPass)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	afx_msg int  OnInitDialog();    
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CUnknownTypeDlg dialog

class CUnknownTypeDlg : public CDialog
{
// Construction
public:
	CUnknownTypeDlg(CWnd* pParent = NULL, char * filetype = NULL, CHelperApp * app = NULL);	

// Dialog Data
	//{{AFX_DATA(CUnknownTypeDlg)
	enum { IDD = IDD_UNKNOWNTYPE };
	CString	m_FileType;
	//}}AFX_DATA
	CHelperApp * m_app;

// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Generated message map functions
	//{{AFX_MSG(CUnknownTypeDlg)
	afx_msg void OnConfigureviewer();
	afx_msg void OnSavetodisk();
	afx_msg void OnMoreInfo();
	virtual void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CNewMimeType dialog

class CNewMimeType : public CDialog
{
// Construction
public:
	CNewMimeType(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CNewMimeType)
	enum { IDD = IDD_NEWMIMETYPE };
	CString	m_MimeSubtype;
	CString	m_MimeType;
	//}}AFX_DATA

// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Generated message map functions
	//{{AFX_MSG(CNewMimeType)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CConfigureViewerSmall dialog

class CConfigureViewerSmall : public CDialog
{
// Construction
public:
	CConfigureViewerSmall(CWnd* pParent = NULL, const char * filetype = NULL,CHelperApp * app = NULL);

// Dialog Data
	//{{AFX_DATA(CConfigureViewerSmall)
	enum { IDD = IDD_CONFIGUREVIEWER_SMALL };
	CString	m_MimeType;	
	CString	m_AppName;
	//}}AFX_DATA
	CHelperApp * m_app;
	
// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Generated message map functions
	//{{AFX_MSG(CConfigureViewerSmall)
	afx_msg void OnHelperBrowse();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CDefaultBrowserDlg
*
*	DESCRIPTION:
*		This provides a dialog for notifying the user that another application
*		has made themselves the "default browser" by changing our registry
*		entries.
*
****************************************************************************/

#define CDefaultBrowserDlgBase	CDialog

class  CDefaultBrowserDlg : public  CDefaultBrowserDlgBase
{
	public:
		CDefaultBrowserDlg(CWnd* pParent = NULL);
		 
	// Dialog Data
	//{{AFX_DATA(CDefaultBrowserDlg)
	enum { IDD = IDD_DEFAULT_BROWSER };
	BOOL	m_bIgnore;
	CListBox	m_Listbox;
	//}}AFX_DATA
		
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDefaultBrowserDlg)
	protected:
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual BOOL OnInitDialog();
		virtual void OnOK( );

	//}}AFX_VIRTUAL
	
	protected:
	
	// Generated message map functions
	//{{AFX_MSG(CDefaultBrowserDlg)
	afx_msg void OnNo();
	afx_msg void OnPaint();
	//}}AFX_MSG
	
	
	DECLARE_MESSAGE_MAP()

}; // END OF CLASS  CDefaultBrowserDlg()


#endif /* _DIALOG_H_ */
