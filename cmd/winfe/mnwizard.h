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

#ifndef MNWIZARD_H
#define MNWIZARD_H

#include "property.h"
#include "resource.h"

class CMailNewsWizard;

#ifdef _WIN32
/////////////////////////////////////////////////////////////////////////////
// CCoverPage  
class CCoverPage : public CNetscapePropertyPage
{
public:

    CCoverPage(CWnd *pParent);
	
	enum { IDD = IDD_WIZARD_COVERPAGE };

	//{{AFX_VIRTUAL(CCoverPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


protected: 
	CMailNewsWizard* m_pParent;

	virtual BOOL OnInitDialog();
    virtual BOOL OnSetActive();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSendMailPage  
class CSendMailPage : public CNetscapePropertyPage
{
public:

    CSendMailPage(CWnd *pParent, BOOL bVerify = FALSE);
	
	enum { IDD = IDD_WIZARD_SENDMAIL };

	virtual BOOL OnInitDialog();

	void DoFinish();

	//{{AFX_VIRTUAL(CSendMailPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


protected: 

	CMailNewsWizard*	m_pParent;
	BOOL				m_bVerify;		

    virtual BOOL OnSetActive();
	virtual LRESULT OnWizardNext();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CReceiveMailPage  
class CReceiveMailPage : public CNetscapePropertyPage
{
public:

    CReceiveMailPage(CWnd *pParent, BOOL bVerify = FALSE);
	
	enum { IDD = IDD_WIZARD_RECEIVEMAIL };

	virtual BOOL OnInitDialog();

	void DoFinish();

	//{{AFX_VIRTUAL(CReceiveMailPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


protected: 

	CMailNewsWizard*	m_pParent;
	BOOL				m_bVerify;		

    virtual BOOL OnSetActive();
	virtual LRESULT OnWizardNext();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CReadNewsPage  
class CReadNewsPage : public CNetscapePropertyPage
{
public:

    CReadNewsPage(CWnd *pParent);
   ~CReadNewsPage();
	
	enum { IDD = IDD_WIZARD_READNEWS };

	virtual BOOL OnInitDialog();

	BOOL DoFinish();

	//PE:
	void SetFinish(BOOL nFinish);

	//{{AFX_VIRTUAL(CReadNewsPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


protected: 

	CMailNewsWizard* m_pParent;

    virtual BOOL OnSetActive();

	afx_msg void OnCheckSecure();
	DECLARE_MESSAGE_MAP()

private:
	BOOL m_bPEFinish;		// PE
};

/////////////////////////////////////////////////////////////////////////////
// CMailNewsWizard
class CMailNewsWizard : public CNetscapePropertySheet
{
public:

    CMailNewsWizard(CWnd *pPare);
    ~CMailNewsWizard();

protected:  
	
	CCoverPage			*m_pCoverPage;
	CSendMailPage		*m_pSendMailPage;
	CReceiveMailPage	*m_pReceiveMailPage;
	CReadNewsPage		*m_pReadNewsPage;

	virtual BOOL OnInitDialog();

	afx_msg void DoFinish();
	DECLARE_MESSAGE_MAP()
};

#else	   //Win16 Code


#define ID_PAGE_COVER		1
#define ID_PAGE_SENDMAIL	2
#define ID_PAGE_READMAIL	3
#define ID_PAGE_READNEWS	4
#define ID_PAGE_FINISH		5

/////////////////////////////////////////////////////////////////////////////
// CMailNewsWizard
class CMailNewsWizard : public CDialog
{
public:

    CMailNewsWizard(CWnd *pPare);
    enum { IDD = IDD_WIZARD_MAILNEWS_WIN16 };

	//{{AFX_VIRTUAL(CMailNewsWizard)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

protected:
	
	int32 m_lPort;
	int m_nCurrentPage;
	int m_nNewsServerLen;
	CString m_szFullName;
	CString m_szEmail;
	CString m_szMailServer;
	CString m_szPopName;
	CString m_szInMailServer;
	CString m_szNewsServer;
	XP_Bool	m_bUseIMAP;
	XP_Bool	m_bIsSecure;

	void SetControlText(int nID, int nStringID); 
	void ShowCoverPage();
	void ShowSendMailPage();
	void ShowHideSendMailControls(int nShowCmd);
	void ShowReadMailPage();
	void ShowHideReadMailControls(int nShowCmd);
	void ShowReadNewsPage();
	void ShowHideReadNewsControls(int nShowCmd);
	BOOL DoFinish(); 
	BOOL CheckValidText(); 
	
	afx_msg void DoBack();
	afx_msg void OnCheckSecure();
	afx_msg void DoNext();
	DECLARE_MESSAGE_MAP()
};

#endif _WIN32

#endif MNWIZARD_H
