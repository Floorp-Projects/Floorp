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

#ifndef MUCWIZ_H
#define MUCWIZ_H

#include "property.h"
#include "resource.h"

class CMucViewWizard;
class CNewProfileWizard;

/////////////////////////////////////////////////////////////////////////////
// CMucIntroPage dialog
#ifdef XP_WIN32
class CMucIntroPage : public CNetscapePropertyPage
#else
class CMucIntroPage : public CDialog
#endif XP_WIN32
{
public:

	CMucIntroPage(CWnd* pParent);   // standard constructor
	enum { IDD = IDD_MUCWIZARD_INTRO };
#ifndef XP_WIN32
    BOOL Create(UINT nID, CWnd *pWnd);
	virtual void PostNcDestroy(){delete this;}
#endif
	void SetMove(int x,int y, int nShowCmd);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnMucIntroAcctExist();
	afx_msg void OnMucIntroAcctSys();
	afx_msg void OnMucIntroAcctAdd();
#ifdef XP_WIN32
    virtual BOOL OnSetActive();
#endif
	CNewProfileWizard* m_pParent;
	void MucIntroProc(int);

	DECLARE_MESSAGE_MAP()

private:
	CString         m_tmpProfilePath;
	enum{
		m_OptAdd,
		m_OptExist,
		m_OptSys
	};
	int             m_height;
	int             m_width;

};


/////////////////////////////////////////////////////////////////////////////
// CASWReadyPage  
#ifdef XP_WIN32
class CASWReadyPage : public CNetscapePropertyPage
#else
class CASWReadyPage : public CDialog
#endif
{
public:

    CASWReadyPage(CWnd *pParent);
	enum { IDD = IDD_MUCWIZARD_ASWREADY };
#ifndef XP_WIN32
    BOOL Create(UINT nID, CWnd *pWnd);
	virtual void PostNcDestroy(){delete this;}
#endif
	afx_msg void DoFinish();
	void SetMove(int x,int y,int nShowCmd);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
#ifdef XP_WIN32
    virtual BOOL OnSetActive();
#endif

	CNewProfileWizard* m_pParent;

	DECLARE_MESSAGE_MAP()

private:
	int             m_height;
	int             m_width;

};
/////////////////////////////////////////////////////////////////////////////
// CMucReadyPage  
#ifdef XP_WIN32
class CMucReadyPage : public CNetscapePropertyPage
#else
class CMucReadyPage : public CDialog
#endif
{
public:

    CMucReadyPage(CWnd *pParent);
	enum { IDD = IDD_MUCWIZARD_MUCREADY };
#ifndef XP_WIN32
    BOOL Create(UINT nID, CWnd *pWnd);
	virtual void PostNcDestroy(){delete this;}
#endif
	void SetMove(int x,int y,int nShowCmd);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	CNewProfileWizard* m_pParent;
#ifdef XP_WIN32
    virtual BOOL OnSetActive();
#endif

	DECLARE_MESSAGE_MAP()

private:
	int             m_height;
	int             m_width;
};

/////////////////////////////////////////////////////////////////////////////
// CMucEditPage  
#ifdef XP_WIN32
class CMucEditPage : public CNetscapePropertyPage
#else
class CMucEditPage : public CDialog
#endif
{
public:

    CMucEditPage(CWnd *pParent, BOOL bEditView);
   ~CMucEditPage();
// Dialog Data
	//{{AFX_DATA(CMucEditPage)
	enum { IDD = IDD_MUCWIZARD_EDIT };
	CListBox       *m_acctName;
	//}}AFX_DATA

#ifndef XP_WIN32
    BOOL Create(UINT nID, CWnd *pWnd);
	virtual void PostNcDestroy(){delete this;}
#endif
	void SetMove(int x,int y,int nShowCmd);
	afx_msg void DoFinish();

protected:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMucEditPage)
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	virtual BOOL OnInitDialog();
	afx_msg void OnSelectAcctlist();
	afx_msg void OnCheckDialerFlag();
//      afx_msg void OnSelectModemlist();

	CMucViewWizard*         m_pViewParent;
	CNewProfileWizard*      m_pEditParent;
#ifdef XP_WIN32
    virtual BOOL OnSetActive();
#endif

	DECLARE_MESSAGE_MAP()

protected:
	void CheckConfig();
	void SetViewPageState(BOOL m_bState);
	void UpdateList();

private:
	BOOL                    m_bEditView;
	BOOL                    m_bCheckState;
	CString                 m_acctSelect;
	CString                 m_modemSelect;
	CStringArray    m_acctList;
	CStringArray    m_modemList;
	char            m_tmpProfilePath[MAX_PATH+1];
	int             m_height;
	int             m_width;

};

/////////////////////////////////////////////////////////////////////////////
// CMucViewWizard
#ifdef XP_WIN32
class CMucViewWizard : public CNetscapePropertySheet
{
public:

    CMucViewWizard(CWnd *pParent, CString strProfile, 
				CString strAcct, CString strModem);
    ~CMucViewWizard();

	virtual BOOL OnInitDialog();
	afx_msg void DoFinish();

	CString m_pProfileName;
	CString m_pAcctName;
	CString m_pModemName;

protected:  
	
	CMucEditPage            *m_pMucEditPage;

	DECLARE_MESSAGE_MAP()

};
#else
/////////////////////////////////////////////////////////////////////////////
class CMucViewWizard : public CDialog
{
public:
	CMucViewWizard(CWnd* pParent, CString strProfile, 
					CString strAcct, CString strModem);     

	enum { IDD = IDD_MUCWIZARD_WIN16 };

	CString m_pProfileName;
	CString m_pAcctName;
	CString m_pModemName;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);        // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnMove(int x, int y);

	CMucEditPage            *m_pMucEditPage;

	DECLARE_MESSAGE_MAP()

private:
	int             m_curPage;
	int             m_height;
	int             m_width;
};

#endif XP_WIN32

#endif MUCWIZ_H
