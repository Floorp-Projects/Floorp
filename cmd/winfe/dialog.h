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
	virtual ~CDialogURL();
    void SetContext(MWContext * context) {m_Context = context;}

    enum { IDD = IDD_OPENURL_BOX };

    CString m_csURL;

// Implementation
protected:
    CWnd      * m_Parent;

    virtual void OnOK();
    
    afx_msg void OnBrowseForFile();
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnChangeNavLocation();
	afx_msg void OnSelchangeNavList();
	// Generated message map functions
	//{{AFX_MSG(CDialogURL)
	// NOTE: the ClassWizard will add member functions here
		afx_msg void OnHelp();
	//}}AFX_MSG
    
    BOOL m_bInternalChange;
    BOOL m_bInitNavComboBox;
    CComboBox *GetNavComboBox();
    void SetCaption(char *pPageTitle = NULL);
    
#define MAX_HISTORY_LOCATIONS 10
    char *m_pNavTitleList[MAX_HISTORY_LOCATIONS];

#ifdef EDITOR

    afx_msg void OnOpenInBrowser();
    afx_msg void OnOpenInEditor();
	afx_msg void OnSelchangeComposerList();
    afx_msg void OnChangeComposerLocation();
    char *m_pComposerTitleList[MAX_HISTORY_LOCATIONS];
    BOOL  m_bInitComposerComboBox;
    CComboBox *GetComposerComboBox();
#endif

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

class nsIDefaultBrowser;

class  CDefaultBrowserDlg : public  CDefaultBrowserDlgBase
{
	public:
		CDefaultBrowserDlg(CWnd* pParent = NULL, nsIDefaultBrowser* pDefaultBrowser = NULL);
        CDefaultBrowserDlg(nsIDefaultBrowser* pDefaultBrowser);
        ~CDefaultBrowserDlg();
		 
	// Dialog Data
	//{{AFX_DATA(CDefaultBrowserDlg)
	enum { IDD = IDD_DEFAULT_BROWSER };
	BOOL	m_bPerformCheck;
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
    afx_msg void OnDetails();
	afx_msg void OnPaint();
	//}}AFX_MSG
	
	
	DECLARE_MESSAGE_MAP()

    private:
        nsIDefaultBrowser* m_pDefaultBrowser; // nsIDefaultBrowser interface pointer.

}; // END OF CLASS  CDefaultBrowserDlg()


/////////////////////////////////////////////////////////////////////////////
// basic dialog that knows how to adjust itself to fit its contents

class FAR CSelfAdjustingDialog : public CDialog
{
public:
	CSelfAdjustingDialog(UINT nIDTemplate, CWnd *pParent);
protected:
	virtual void RectForText(CWnd *window, const char *text, LPRECT wrect, LPPOINT diff);
	virtual void ResizeItemToFitText(CWnd *window, const char *text,
		LPPOINT diff);
	virtual void BumpItemIfAfter(CWnd *item, CWnd *afterWind, LPPOINT diff);
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CheckConfirm dialog

class FAR CCheckConfirmDialog : public CSelfAdjustingDialog
{
public:
    CCheckConfirmDialog(CWnd *pParent, const char *pMessage, const char *pCheckMessage,
        const char *pOKMessage, const char *pCancelMessage, BOOL checked);

    enum { IDD = IDD_CHECKCONFIRM_BOX };

	BOOL	DoModal(XP_Bool *checkboxSet);

protected:
    CString mMessage,
			mCheckMessage,
			mOKMessage,
			mCancelMessage;
	int		mCheckState;

	void AdjustButtons(CWnd *okButton, CWnd *cancelButton, LONG expectedMargin);
	void AdjustForItemSize(CWnd *afterWind, LPPOINT diff);
	void CheckOverallSize(LPPOINT diff, BOOL adjust);

	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// Single User Signon User Selection Dialog

class FAR CUserSelectionDialog : public CDialog
{
public:
    CUserSelectionDialog(CWnd *pParent, const char *pMessage,
        const char **pUserList, int16 nUserListCount);
    ~CUserSelectionDialog();

    enum { IDD = IDD_SELECT_BOX };

	BOOL	DoModal(int16 *nSelection);

protected:
    CString mMessage;
	char		**mList;
	int16		mListCount;
	int16		mSelection;

	virtual BOOL OnInitDialog();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
};

#endif /* _DIALOG_H_ */
