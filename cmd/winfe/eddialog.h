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

// eddialog.h : interface of the dialogs used by CNetscapeEditView 
//
/////////////////////////////////////////////////////////////////////////////

#ifndef EDDIALOG_H
#define EDDIALOG_H

#include "edtrccln.h"

/////////////////////////////////////////////////////////////////////////////
// CLoadingImageDlg dialog

class CLoadingImageDlg : public CDialog
{
// Construction
public:
	CLoadingImageDlg(CWnd* pParent = NULL,
                     MWContext * pMWContext = NULL);

// Dialog Data
	//{{AFX_DATA(CLoadingImageDlg)
    enum { IDD = IDD_LOADING_IMAGES };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

    MWContext     *m_pMWContext;
private:
    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLoadingImageDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLoadingImageDlg)
	virtual void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSaveFileDlg dialog
// This is active when images are being saved as part of saving remote file
// It allows user to cancel
// NOTE: This dialog is used during HTTP or FTP upload,
//       so resource is in NETSCAPE.RC, not EDITORXX.DLL
//
class CSaveFileDlg : public CDialog
{
// Construction
public:
	CSaveFileDlg(CWnd* pParent = NULL,
                 MWContext * pMWContext = NULL,
                 int  iFileCount = 1,
                 ED_SaveDialogType saveType = ED_SAVE_DLG_SAVE_LOCAL);

    // Called at the start of each image saved
    void StartFileSave(char * pFilename);

// Dialog Data
	//{{AFX_DATA(CSaveFileDlg)
	enum { IDD = IDD_SAVE_DOCUMENT };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA
    
    // Used to show progress, e.g.: "File 1 of 3"
    int        m_iFileCount;
    int        m_iCurrentFile;
    BOOL       m_bUpload;

private:
    MWContext *m_pMWContext;
    CWnd      *m_pParent;
    UINT       m_hTimer;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSaveFileDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:
	
    // Generated message map functions
	//{{AFX_MSG(CSaveFileDlg)
	virtual void OnCancel();
    afx_msg void OnTimer( UINT  nIDEvent );
    afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CSaveFileOverwriteDlg dialog
// Called for user feedback when saving remote images
//
class CSaveFileOverwriteDlg : public CDialog
{
// Construction
public:
	CSaveFileOverwriteDlg(CWnd* pParent = NULL,
                          char * pFilename = NULL,
                          CSaveFileDlg * pSaveDlg = NULL);
    char * m_pFilename;
    // Put user's choice here
    ED_SaveOption  m_Result;
    CSaveFileDlg * pSaveDlg;

// Dialog Data
	//{{AFX_DATA(CSaveFileOverwriteDlg)
	enum { IDD = IDD_SAVE_IMAGE_OVERWRITE };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

private:
    // Used to cascade this dialog under CSaveFileDlg
    CSaveFileDlg*  m_pSaveFileDlg;	

    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSaveFileOverwriteDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSaveFileOverwriteDlg)
	virtual void OnOK();
	afx_msg void OnDontOverwriteAll();
	afx_msg void OnDontOverwriteOne();
	afx_msg void OnOverwriteAll();
	afx_msg void OnOverwriteOne();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CSaveNewDlg dialog

class CSaveNewDlg : public CDialog
{
// Construction
public:
	CSaveNewDlg(CWnd * pParent = NULL);
	               
// Dialog Data
	//{{AFX_DATA(CSaveNewDlg)
	enum { IDD = IDD_SAVE_NEW_DOCUMENT };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSaveNewDlg)
	protected:
	//}}AFX_VIRTUAL

private:
    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSaveNewDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CPublishDlg dialog

class CPublishDlg : public CDialog
{
// Construction
public:
	CPublishDlg(CWnd *pParent = NULL,
                MWContext *pMWContext = NULL,
                char *pUrl = NULL);

    ~CPublishDlg();

    MWContext   *m_pMWContext;
    
    // The list is first built here and sorted
    // We condense strings in the listbox so filenames show,
    //   so we need to save this list of full URLs
    char      **m_ppImageList;
    // Count of files in m_ppFileList
    int         m_iFileCount;

    // This is list of files returned to NET_PublishDocument()
    char      **m_ppIncludedFiles;
    // The final ftp://username:password@location
    //  is assembled here to be passed back to caller
    char       *m_pFullLocation;

// Dialog Data
	//{{AFX_DATA(CPublishDlg)
	enum { IDD = IDD_PUBLISH };
	CString	m_csUserName;
	CString	m_csPassword;
	BOOL	m_bRememberPassword;
    CString m_csFilename;
    CString m_csTitle;
	//}}AFX_DATA

	CString	m_csLocation;

private:
    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPublishDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
    char      *m_pCurrentUrl;
    char      *m_pCurrentFile;
    char      *m_pCurrentDirectory;
    char      *m_pImageFiles;
    XP_Bool   *m_pSelectedDefault;
    char     **m_ppAllFiles;
    BOOL       m_bIsRootDirectory;
    char     **m_ppUserList;
    char     **m_ppPasswordList;

	// Generated message map functions
	//{{AFX_MSG(CPublishDlg)
	afx_msg void OnHelp();
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	afx_msg void OnSelectAll();
	afx_msg void OnSelectNone();
	afx_msg void OnIncludeAllFiles();
	afx_msg void OnIncludeImageFiles();
	afx_msg void OnKillfocusPublishLocationList();
	afx_msg void OnPublishDefaultLocation();
	afx_msg void OnSelchangePublishLocation();
	//}}AFX_MSG
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CEditHintDlg dialog

class CEditHintDlg : public CDialog
{
// Construction
public:
	CEditHintDlg(CWnd* pParent = NULL,      // standard constructor
                 UINT  nID_Msg = 0,
                 UINT  nID_Caption = 0,
                 BOOL  bYesNo = FALSE);     // Default action is like MB_OK
                                            // If TRUE, action is like MB_YESNO,
                                            // return value: IDOK = "Yes", IDCANCEL = "No"

// Dialog Data
	//{{AFX_DATA(CEditHintDlg)
	enum { IDD = IDD_HINT_DLG };
	BOOL	m_bDontShowAgain;
	CString	m_cHintText;
	//}}AFX_DATA

private:
    BOOL  m_bYesNo;
    UINT  m_nID_Msg;
    UINT  m_nID_Caption;

    // This will change resource hInstance to Editor dll (in constructor)
    // Be sure to call m_ResourceSwitcher.Reset() 
    //   in InitDialog() or OnSetActive() if dialog accesses strings
    //   in NETSCAPE.EXE
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditHintDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEditHintDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnYes();
	afx_msg void OnNo();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CGetLocationDlg dialog

class CGetLocationDlg : public CDialog
{
// Construction
public:
	CGetLocationDlg(CWnd* pParent = NULL,      // standard constructor
                 UINT  nID_Msg = 0,
                 UINT  nID_Caption = 0,
                 UINT  nID_FileCaption = 0);
// Dialog Data
	//{{AFX_DATA(CGetLocationDlg)
	enum { IDD = IDD_LOCATION_DLG };
	CString	m_csLocation;
	//}}AFX_DATA

private:
    UINT  m_nID_Msg;
    UINT  m_nID_Caption;
    UINT  m_nID_FileCaption;

    // This will change resource hInstance to Editor dll (in constructor)
    // Be sure to call m_ResourceSwitcher.Reset() 
    //   in InitDialog() or OnSetActive() if dialog accesses strings
    //   in NETSCAPE.EXE
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGetLocationDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CGetLocationDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
    afx_msg void OnChooseFile();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// COpenTemplateDlg dialog

class COpenTemplateDlg : public CDialog
{
// Construction
public:
	COpenTemplateDlg(CWnd* pParent = NULL);      // standard constructor

// Dialog Data
	//{{AFX_DATA(COpenTemplateDlg)
	enum { IDD = IDD_OPEN_TEMPLATE };
	CString	m_csLocation;
	//}}AFX_DATA

private:
    char  *m_pHistoryBase;
    int    m_iTemplateLocationCount;
    char **m_pLocationList;

    // This will change resource hInstance to Editor dll (in constructor)
    // Be sure to call m_ResourceSwitcher.Reset() 
    //   in InitDialog() or OnSetActive() if dialog accesses strings
    //   in NETSCAPE.EXE
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COpenTemplateDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COpenTemplateDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
    afx_msg void OnChooseFile();
    afx_msg void OnGotoNetscapeTemplates();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CPageTitleDlg dialog

class CPageTitleDlg : public CDialog
{
// Construction
public:
	CPageTitleDlg(CWnd* pParent, char** ppTitle = NULL);

// Dialog Data
	//{{AFX_DATA(CPageTitleDlg)
	enum { IDD = IDD_PAGE_TITLE };
	CString	m_csTitle;
	//}}AFX_DATA

private:
    // Return result here
    char **m_ppTitle;

    // This will change resource hInstance to Editor dll (in constructor)
    // Be sure to call m_ResourceSwitcher.Reset() 
    //   in InitDialog() or OnSetActive() if dialog accesses strings
    //   in NETSCAPE.EXE
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPageTitleDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPageTitleDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

// CPasteSpecialDlg::m_iResult values
enum {
    ED_PASTE_CANCEL,
    ED_PASTE_TEXT,
    ED_PASTE_IMAGE
};

/////////////////////////////////////////////////////////////////////////////
// CPasteSpecialDlg dialog
// Called for user feedback when saving remote images
//
class CPasteSpecialDlg : public CDialog
{
// Construction
public:
	CPasteSpecialDlg(CWnd* pParent);

    // Put user's choice here
    int m_iResult;

// Dialog Data
	//{{AFX_DATA(CSaveFileOverwriteDlg)
	enum { IDD = IDD_PASTE_SPECIAL };
	//}}AFX_DATA

private:
    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSaveFileOverwriteDlg)
	afx_msg void OnPasteText();
	afx_msg void OnPasteImage();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CGetColumnsDlg dialog

class CGetColumnsDlg : public CDialog
{
// Construction
public:
	CGetColumnsDlg(CWnd* pParent = NULL);     // standard constructor

// Dialog Data
	//{{AFX_DATA(CGetColumnsDlg)
	enum { IDD = IDD_GET_TABLE_COLUMNS };
	int  m_iColumns;
	//}}AFX_DATA

    int GetColumns() { return m_iColumns; }

private:

    // This will change resource hInstance to Editor dll (in constructor)
    // Be sure to call m_ResourceSwitcher.Reset() 
    //   in InitDialog() or OnSetActive() if dialog accesses strings
    //   in NETSCAPE.EXE
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGetColumnsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CGetColumnsDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
    
};

#endif // EDDIALOG_H
