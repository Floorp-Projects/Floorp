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

// dlgseldg.h : header file
//
#include "apiimg.h"
#include "mailmisc.h"
#include "msgcom.h"
/////////////////////////////////////////////////////////////////////////////
// CFilterList

typedef struct FolderData
{
	MSG_FolderInfo *infoData;
	BOOL bDownLoad;
}FolderData;


class CDiscussionsList: public CMailFolderList
{

private:
	int m_iPosIndex, m_iPosName, m_iPosStatus;
protected:
	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );

	virtual void MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
	{
		DoMeasureItem( m_hWnd, lpMeasureItemStruct );
	}


	afx_msg void OnLButtonDown( UINT nFlags, CPoint point ); 
	DECLARE_MESSAGE_MAP()

public:
	CDWordArray *m_pDWordArray;
	BOOL m_bHasSelectables;
public:
	UINT	ItemFromPoint(CPoint pt, BOOL& bOutside) const;

	//overriding functionality called in CMailFolderHelper
	int  PopulateNews(MSG_Master *pMaster, BOOL bRoots);
	void SubPopulate(int &index, MSG_FolderInfo *folder );

	
	CDiscussionsList();
	~CDiscussionsList();

	void SetColumnPositions(int, int, int);
};






/////////////////////////////////////////////////////////////////////////////
// CDlgSelectGroups dialog

class CDlgSelectGroups : public CDialog
{

public:
	int m_nDiscussionSelectionCount;
	int m_nMailSelectionCount;
	int m_iIndex;
	CDiscussionsList m_DiscussionList;

// Construction
public:
	CDlgSelectGroups(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDlgSelectGroups)
	enum { IDD = IDD_SELECT_DISCUSSIONS };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	int GetSelectionCount() {return m_nDiscussionSelectionCount;};
	int GetMailSelectionCount() {return m_nMailSelectionCount;};

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDlgSelectGroups)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDlgSelectGroups)
	afx_msg void OnButtonSelectAll();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
