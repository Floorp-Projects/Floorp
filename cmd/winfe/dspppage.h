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

// dspppage.h : header file
//
#include "property.h"
#include "msgcom.h"
/////////////////////////////////////////////////////////////////////////////
// CDiskSpacePropertyPage dialog
class CMoreChoicesDlg;
class CNewsFolderPropertySheet;

class CDiskSpacePropertyPage : public CNetscapePropertyPage
{


public:
    CMoreChoicesDlg *m_pMoreDlg;
// Construction
public:
	CDiskSpacePropertyPage(CNewsFolderPropertySheet *pParent,BOOL bDefaultPref=FALSE,int nSelection = 0, int nDays=30, 
		                   int nKeepNew=500,BOOL bKeepUnread=FALSE);
	~CDiskSpacePropertyPage();
	void SetFolderInfo(MSG_FolderInfo *folderInfo);
	virtual BOOL OnSetActive();
	virtual BOOL OnInitDialog();
    void EnableDisableItem(BOOL bState, UINT nIDC);
	void DisableOthers(BOOL bChecked);
// Dialog Data
	//{{AFX_DATA(CDiskSpacePropertyPage)
	enum { IDD = IDD_PP_FOLDER_DISKSPACE };
	int		m_nKeepSelection;
	UINT	m_nDays;
	UINT	m_nKeepNew;
	BOOL	m_bCheckUnread;
	BOOL	m_bCheckDefault;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CDiskSpacePropertyPage)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CNewsFolderPropertySheet *m_pParent;
	MSG_FolderInfo		*m_folderInfo;
	// Generated message map functions
	//{{AFX_MSG(CDiskSpacePropertyPage)
	afx_msg void OnCheckUnread();
	afx_msg void OnDownloadNow();
	afx_msg void OnBtnMore();
	afx_msg void OnRdKeepUnread();
	afx_msg void OnChangeEditDays();
	afx_msg void OnRdKeepDays();
	afx_msg void OnRdKeepNew();
	afx_msg void OnRdKeepall();
	afx_msg void OnChangeEditNewest();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


/////////////////////////////////////////////////////////////////////////////
// CMoreChoicesDlg dialog

class CMoreChoicesDlg : public CDialog
{
public:
	CMoreChoicesDlg(CWnd* pParent = NULL);   // standard constructor
	CDiskSpacePropertyPage *m_pParent; 

	//{{AFX_DATA(CMoreChoicesDlg)
	enum { IDD = IDD_MORE_DISK_CHOICES };
	BOOL	m_bCheck;
	UINT	m_nDays;
	//}}AFX_DATA
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMoreChoicesDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
protected:
	// Generated message map functions
	//{{AFX_MSG(CMoreChoicesDlg)
	afx_msg void OnCheck();
	afx_msg void OnChangeEdit();
	virtual void OnCancel();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};





class CDownLoadPPNews : public CNetscapePropertyPage
{

// Construction
public:
	CDownLoadPPNews(CNewsFolderPropertySheet *pParent);
	~CDownLoadPPNews();

	void DisableOthers(BOOL bChecked);
	void EnableDisableItem(BOOL bState, UINT nIDC);
	void SetFolderInfo(MSG_FolderInfo *folderInfo);
	virtual BOOL OnInitDialog();

	// Dialog Data
	//{{AFX_DATA(CDownLoadPPNews)
	enum { IDD = IDD_PP_DOWNLOAD_DISCUS };
	CString	m_strComboArticles;
	BOOL	m_bCheckArticles;
	BOOL	m_bCheckDefault;
	BOOL	m_bCheckDownLoad;
	BOOL	m_bCheckByDate;
	UINT	m_nDaysAgo;
	int		m_nRadioValue;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CDownLoadPPNews)
	public:
	virtual void OnOK();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	MSG_FolderInfo		*m_folderInfo;
	CNewsFolderPropertySheet *m_pParent;
	// Generated message map functions
	//{{AFX_MSG(CDownLoadPPNews)
	afx_msg void OnCheckDefault();
	afx_msg void OnDownloadNow();
	afx_msg void OnSetfocusCombo2();
	afx_msg void OnSelchangeCombo2();
	afx_msg void OnChangeEditDaysAgo();
	afx_msg void OnRadioSince();
	afx_msg void OnRadioFrom();
	afx_msg void OnCheckByArticles();
	afx_msg void OnCheckByDate();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


class CDownLoadPPMail : public CNetscapePropertyPage
{

// Construction
public:
	CDownLoadPPMail();
	~CDownLoadPPMail();

// Dialog Data
	//{{AFX_DATA(CDownLoadPPMail)
	enum { IDD = IDD_PP_DOWNLOAD_MAIL };
	BOOL m_bCheckDownLoadMail;
	//}}AFX_DATA
	MSG_FolderInfo *m_pfolderInfo;

	void SetFolderInfo(MSG_FolderInfo *pfolderInfo);

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CDownLoadPPMail)
	public:
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CDownLoadPPMail)
	afx_msg void OnCheckDownLoad();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};
