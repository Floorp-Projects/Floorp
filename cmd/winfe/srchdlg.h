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
#ifndef _srchdlg_h_
#define _srchdlg_h_

// SRCHDLG.H
//
// DESCRIPTION:
//		This file contains the declarations for basic/advanced
//		ldap search dialog
//

#ifdef FEATURE_BUTTONPROPERTYPAGE
#include "butprop.h"
#endif

#include "srchobj.h"
#include "mnrccln.h"


#ifndef LDS_GETSEARCHPANE
#define LDS_GETSEARCHPANE	(WM_USER + 225)
#define LDS_GETSERVER		(WM_USER + 226)
#define LDS_RECALC_LAYOUT	(WM_USER + 227)
#endif

/****************************************************************************
*
*	Class: CSearchDialog
*
*	DESCRIPTION:
*		This class is the property sheet window for editing all the attributes
*		of the people types in the address book
*
****************************************************************************/

typedef enum _ButtonPosition	// Normally in butprop.h.
{
	BUTTON_RIGHT,
	BUTTON_BOTTOM,
	BUTTON_NONE
} ButtonPosition;

#ifdef FEATURE_BUTTONPROPERTYPAGE
class CSearchDialog : public CButtonPropertySheet
#else
class CSearchDialog : public CWnd
#endif
{
protected:
#ifdef FEATURE_BUTTONPROPERTYPAGE
    CButtonPropertyPage * m_pAdvancedSearch;
	CButtonPropertyPage * m_pBasicSearch;
#else
	CDialog* m_pAdvancedSearch;
	CDialog* m_pBasicSearch;
public:
	int DoModal() { return 0; }
protected:
#endif

	MSG_Pane* m_pSearchPane;
	DIR_Server* m_pServer;

public:
	
	HFONT		m_pFont;

public:
	CSearchDialog (UINT nIDCaption, 
		MSG_Pane* pSearchPane,
		DIR_Server* pServer,
		CWnd* pParentWnd = NULL,
		UINT numButtons = 0, 
		ButtonPosition buttonPosition = BUTTON_NONE, 
		CUIntArray* buttonLabels = NULL);
    CSearchDialog (
		LPCTSTR lpszCaption,
		MSG_Pane* pSearchPane,
		DIR_Server* pServer,
		CWnd * parent = NULL,
		UINT numButtons = 0, 
		ButtonPosition buttonPosition = BUTTON_NONE,
		CPtrArray* buttonLabels = NULL); 
    virtual ~CSearchDialog ( );    
	virtual void OnHelp();
	virtual void OnButton2();       
	virtual void OnButton3();           
	virtual void OnButton4();       

protected:
	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSearchDialog)
	public:
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSearchDialog)
		// NOTE: the ClassWizard will add member functions here
	afx_msg int OnCreate( LPCREATESTRUCT );
	afx_msg LRESULT OnGetServer(WPARAM, LPARAM);
	afx_msg LRESULT OnGetSearchPane(WPARAM, LPARAM);
	afx_msg LRESULT OnRecalcLayout(WPARAM, LPARAM);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CAdvancedSearch
*
*	DESCRIPTION:
*
****************************************************************************/

#ifdef FEATURE_BUTTONPROPERTYPAGE
class CAdvancedSearch : public CButtonPropertyPage
#else
class CAdvancedSearch : public CDialog
#endif
{

// Construction
public:
	CAdvancedSearch(CWnd *pParent);
	virtual ~CAdvancedSearch();

// Dialog Data
	//{{AFX_DATA(CAddressUser)
	enum { IDD = IDD_ADVANCED_LDAP_SEARCH };

	//}}AFX_DATA

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAdvancedSearch)
	public:
	virtual BOOL OnInitDialog();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Overridables
	public:
	void InitializePrevSearch();


// Implementation
protected:

#ifndef ON_UPDATE_COMMAND_UI_RANGE
	virtual BOOL OnCmdMsg( UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo );  
#endif
	// Generated message map functions
	//{{AFX_MSG(CAdvancedSearch)
		// NOTE: the ClassWizard will add member functions here
	afx_msg void OnMore();
	afx_msg void OnFewer();
	afx_msg void OnSearch();
	afx_msg void OnClearSearch();
	afx_msg void OnAttrib1();
	afx_msg void OnAttrib2();
	afx_msg void OnAttrib3();
	afx_msg void OnAttrib4();
	afx_msg void OnAttrib5();
	afx_msg	void OnAndOr();
	afx_msg void OnOK();
	afx_msg void OnUpdateQuery( CCmdUI *pCmdUI );
	afx_msg void OnEditValueChanged ();
	afx_msg void OnOperatorValueChanged ();
	virtual BOOL OnSetActive();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	void AdjustHeight (int dy);

public:
	BOOL SavePreviousSearch ();

// Data members
protected:
	CSearchObject m_searchObj;
	int		m_iMoreCount;
	BOOL	m_bLogicType;
	BOOL	m_bChanged;
};


/****************************************************************************
*
*	Class: CBasicSearch
*
*	DESCRIPTION:
*
****************************************************************************/
#ifdef FEATURE_BUTTONPROPERTYPAGE
class CBasicSearch : public CButtonPropertyPage
#else
class CBasicSearch : public CDialog
#endif
{

// Construction
public:
	CBasicSearch(CWnd *pParent);
	virtual ~CBasicSearch();

protected:
	void BuildQuery (MSG_Pane* searchPane, BOOL bLogicType);

// Dialog Data
	//{{AFX_DATA(CAddressUser)
	enum { IDD = IDD_BASIC_SEARCH };

	//}}AFX_DATA

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CBasicSearch)
	public:
	virtual BOOL OnInitDialog();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CBasicSearch)
		// NOTE: the ClassWizard will add member functions here
	afx_msg void OnOK();
	afx_msg void OnEditValueChanged ();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

public:
	BOOL SavePreviousSearch ();
	void InitializeSearchValues ();

protected:
	BOOL 	m_bLogicType;
	BOOL	m_bChanged;
};

#endif
