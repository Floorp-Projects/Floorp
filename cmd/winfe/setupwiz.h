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

#ifndef SETUPWIZ_H
#define SETUPWIZ_H

#include "property.h"
#include "resource.h"
#ifdef MOZ_MAIL_NEWS
#include "mnwizard.h"
#endif /* MOZ_MAIL_NEWS */
#include "mucwiz.h"

#ifdef XP_WIN32

class CNewProfileWizard;

/////////////////////////////////////////////////////////////////////////////
// CConfirmPage  
class CConfirmPage : public CNetscapePropertyPage
{
public:

    CConfirmPage(CWnd *pParent);
   ~CConfirmPage();
	
	enum { IDD = IDD_SETUPWIZARD_CONFIRMTYPE };

	virtual BOOL OnInitDialog();

	afx_msg int DoFinish();
	//{{AFX_VIRTUAL(CIntroPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


protected: 
	CNewProfileWizard* m_pParent;

    virtual BOOL OnSetActive();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CIntroPage  
class CIntroPage : public CNetscapePropertyPage
{
public:

    CIntroPage(CWnd *pParent);
	
	enum { IDD = IDD_SETUPWIZARD_INTRO };
	virtual BOOL OnInitDialog();

	//{{AFX_VIRTUAL(CIntroPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


protected: 
	CNewProfileWizard* m_pParent;

    virtual BOOL OnSetActive();

	DECLARE_MESSAGE_MAP()
};
/////////////////////////////////////////////////////////////////////////////
// CNamePage  
class CNamePage : public CNetscapePropertyPage
{
public:

    CNamePage(CWnd *pParent);
   ~CNamePage();
	
	enum { IDD = IDD_SETUPWIZARD_NAMEEMAIL };

	virtual BOOL OnInitDialog();
	void ShowHideEmailName();
	afx_msg void DoFinish();

	//{{AFX_VIRTUAL(CProfileNamePage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


protected: 

	CNewProfileWizard* m_pParent;

    virtual BOOL OnSetActive();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CProfileNamePage  
class CProfileNamePage : public CNetscapePropertyPage
{
public:

    CProfileNamePage(CWnd *pParent);
   ~CProfileNamePage();
	
	enum { IDD = IDD_SETUPWIZARD_PROFILENAME };

	void GetProfilePath(char *str);

	virtual BOOL OnInitDialog();

	afx_msg int DoFinish();

	//{{AFX_VIRTUAL(CProfileNamePage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL UpdateData(BOOL bValidate);
	//}}AFX_VIRTUAL


protected: 

	CNewProfileWizard* m_pParent;

    virtual BOOL OnSetActive();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CNewProfileWizard
class CNewProfileWizard : public CNetscapePropertySheet
{
public:

    CNewProfileWizard(CWnd *pPare, BOOL bUpgrade);
    ~CNewProfileWizard();

	afx_msg void DoFinish();
	virtual BOOL OnInitDialog();

	CString         m_pUserAddr;
	CString         m_pFullName;
	CString         m_pProfileName;
	CString         m_pProfilePath;
	BOOL            m_bUpgrade;
	BOOL			m_bExistingDir;

	//PE
	CString         m_pModemName;
	CString         m_pAcctName;
	BOOL            m_bASWEnabled;
	BOOL            m_bMucEnabled;
	CString			m_title;
	// for PE
	afx_msg void DoNext();
	afx_msg void DoBack();
	void GetProfilePath(char *str);

protected:  
	
	CIntroPage                      *m_pIntroPage;
	CNamePage                       *m_pNamePage;
	CProfileNamePage        *m_pProfileNamePage;
	CConfirmPage            *m_pConfirmPage;
#ifdef MOZ_MAIL_NEWS  
	CSendMailPage           *m_pSendMailPage;
	CReceiveMailPage        *m_pReceiveMailPage;
	CReadNewsPage           *m_pReadNewsPage;
#endif /* MOZ_MAIL_NEWS */   
	//PE
	CMucIntroPage           *m_pMucIntroPage;
	CMucEditPage            *m_pMucEditPage;
	CASWReadyPage           *m_pASWReadyPage;
	CMucReadyPage           *m_pMucReadyPage;

	DECLARE_MESSAGE_MAP()
};


#else      //Win16 Code
/////////////////////////////////////////////////////////////////////////////
#define ID_PAGE_INTRO           1
#define ID_PAGE_NAME            2
#define ID_PAGE_PROFILE         3
#define ID_PAGE_CONFIRM         4
#define ID_PAGE_SENDMAIL        5
#define ID_PAGE_RECEIVEMAIL     6
#define ID_PAGE_READNEWS        7
#define ID_PAGE_FINISH          8
#define ID_PEMUC_INTRO			9
#define ID_PEMUC_ASWREADY		10
#define ID_PEMUC_MUCREADY		11
#define ID_PEMUC_MUCEDIT		12

// CMailNewsWizard
class CNewProfileWizard : public CDialog
{
public:

    CNewProfileWizard(CWnd *pPare, BOOL bUpgrade);
   	~CNewProfileWizard();
    enum { IDD = IDD_SETUPWIZARD_WIN16 };

	CString         m_pUserAddr;
	CString         m_pFullName;
	CString         m_pProfileName;
	CString         m_pProfilePath;
	int             m_bFirstProfile;
	int             m_nCurrentPage;
	BOOL            m_bUpgrade;
	BOOL			m_bExistingDir;

	CString m_szFullName;
	CString m_szEmail;
	CString m_szMailServer;
	CString m_szPopName;
	CString m_szInMailServer;
	CString m_szNewsServer;
	XP_Bool m_bUseIMAP;
	XP_Bool m_bLeftOnServer;
	XP_Bool	m_bIsSecure;
	int		m_nPort;

	
	//PE
	CString         m_pModemName;
	CString         m_pAcctName;
	BOOL            m_bASWEnabled;
	BOOL            m_bMucEnabled;

	// for PE
	void GetProfilePath(char *str);


	//{{AFX_VIRTUAL(CMailNewsWizard)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnInitDialog();

protected:

	void InitPrefStrings(); 
	void SetControlText(int nID, int nStringID); 
	void ShowHideIntroPage(int nShowCmd);
	void ShowHideNamePage(int nShowCmd);
	void ShowHideProfilePage(int nShowCmd);
	void ShowHideConfirmPage(int nShowCmd);
	void ShowHideSendPage(int nShowCmd);
	void ShowHideReceivePage(int nShowCmd);
	void ShowHideNewsPage(int nShowCmd);

	//PE
	void ShowHidePEMucIntroPage(int nShowCmd);
	void ShowHidePEMucReadyPage(int nShowCmd);
	void ShowHidePEMucASWReadyPage(int nShowCmd);
	void ShowHidePEMucEditPage(int nShowCmd);

	BOOL DoFinish(); 
	
	afx_msg void DoBack();
	afx_msg void DoNext();
	afx_msg void OnMove(int x, int y);
	afx_msg void OnCheckSecure();

	DECLARE_MESSAGE_MAP()

	//PE
	CMucIntroPage           *m_pMucIntroPage;
	CMucEditPage            *m_pMucEditPage;
	CASWReadyPage           *m_pASWReadyPage;
	CMucReadyPage           *m_pMucReadyPage;

};
#endif XP_WIN32
#endif SETUPWIZ_H

