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

#include "srchobj.h"
#include "mnrccln.h"


/****************************************************************************
*
*	Class: CSearchDialog
*
*	DESCRIPTION:
*		This class is the dialog for editing all the attributes
*		of the people types in the address book
*
****************************************************************************/


class CSearchDialog : public CDialog
{
protected:
public:
protected:

	MSG_Pane* m_pSearchPane;
	DIR_Server* m_pServer;

public:
	
	HFONT		m_pFont;
	char *		m_pTitle;
    enum { IDD = IDD_ADVANCED_LDAP_SEARCH };

public:
	CSearchDialog (UINT nIDCaption, 
		MSG_Pane* pSearchPane,
		DIR_Server* pServer,
		CWnd* pParentWnd = NULL);
    CSearchDialog (
		LPCTSTR lpszCaption,
		MSG_Pane* pSearchPane,
		DIR_Server* pServer,
		CWnd * parent = NULL); 
    virtual ~CSearchDialog ( );    

protected:
	CSearchObject m_searchObj;
	int		m_iMoreCount;
	BOOL	m_bLogicType;
	BOOL	m_bChanged;

	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSearchDialog)
	public:
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Implementation
protected:
	#ifndef ON_UPDATE_COMMAND_UI_RANGE
	virtual BOOL OnCmdMsg( UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo );  
#endif

	// Generated message map functions
	//{{AFX_MSG(CSearchDialog)
		// NOTE: the ClassWizard will add member functions here
	afx_msg int OnCreate( LPCREATESTRUCT );
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
	afx_msg void OnUpdateQuery( CCmdUI *pCmdUI );
	afx_msg void OnEditValueChanged ();
	afx_msg void OnOperatorValueChanged ();

	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	void AdjustHeight (int dy);

};


#endif