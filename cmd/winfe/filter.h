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

#ifndef __Filter_h
#define __Filter_h

#include "msg_filt.h"
#include "apiimg.h"
#include "mailmisc.h"
#ifndef _WIN32
#include "ctl3d.h"
#endif

//
// filter.h : header file
//


/////////////////////////////////////////////////////////////////////////////
// CFilterList

class CFilterList: public CListBox {
private:
	int m_iPosIndex, m_iPosName, m_iPosStatus;
	LPIMAGEMAP m_pIImageMap;
	LPUNKNOWN m_pIImageUnk;

protected:
	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );

	afx_msg void OnLButtonDown( UINT nFlags, CPoint point ); 
	afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );

	DECLARE_MESSAGE_MAP()

public:
	UINT	ItemFromPoint(CPoint pt, BOOL& bOutside) const;		

	CFilterList();
	~CFilterList();

	void SetColumnPositions(int, int, int);
};

/////////////////////////////////////////////////////////////////////////////
// CFilterDialog dialog

class CFilterPickerDialog : public CDialog
{
protected:
	MSG_Pane *m_pPane;
	CFilterList m_FilterList;

	CBitmapButton m_UpButton;
	CBitmapButton m_DownButton;

// Construction
public:
	CFilterPickerDialog(MSG_Pane *pPane, CWnd* pParent = NULL);

// Dialog Data
	//{{AFX_DATA(CFilterPickerDialog)
	enum { IDD = IDD_FILTER_PICKER };
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFilterPickerDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	
	void UpdateList();

	MSG_FilterList *m_pFilterList;
	MSG_Filter *m_pFilter;
	int m_iIndex;

	int32 m_iFilterCount;
	XP_Bool m_bLoggingEnabled;

	// Generated message map functions
	//{{AFX_MSG(CFilterPickerDialog)
	afx_msg void OnDblclkListFilter();
	afx_msg void OnSelchangeListFilter();
	afx_msg void OnSelcancelListFilter();
	afx_msg void OnOK();
	afx_msg void OnCancel();
	afx_msg void OnNew();
	afx_msg void OnEdit();
	afx_msg void OnUpdateEdit( CCmdUI *pCmdUI );
	afx_msg void OnDelete();
	afx_msg void OnUpdateDelete( CCmdUI *pCmdUI );
	afx_msg void OnUp();
	afx_msg void OnUpdateUp( CCmdUI *pCmdUI );
	afx_msg void OnDown();
	afx_msg void OnUpdateDown( CCmdUI *pCmdUI );
	afx_msg BOOL OnInitDialog();
	afx_msg void OnHelp();
	afx_msg void OnView();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CFilterDialog dialog

class CCustomHeadersDlg;

class CFilterDialog : public CDialog
{
protected:
	CMailFolderCombo m_FolderCombo;
	MSG_Pane *m_pPane;
	MSG_Filter **m_hFilter;

// Construction
public:
	CFilterDialog(MSG_Pane *pPane, MSG_Filter **filter, CWnd* pParent = NULL);

	CCustomHeadersDlg *m_pCustomHeadersDlg;
	int m_iRowIndex;
	BOOL m_bLogicType;
	int m_iRowSelected;

	void UpdateOpList(int iRow);
	void UpdateDestList();
	void UpdateColumn1Attributes();
	void EditHeaders(int iRow);

	int GetNumTerms();

	void GetTerm(int iRow,
				 MSG_SearchAttribute *attrib,
				 MSG_SearchOperator *op,
				 MSG_SearchValue *value,
				 char *pszArbitraryHeader);

	void SetTerm(int iRow,
				 MSG_SearchAttribute attrib,
				 MSG_SearchOperator op,
				 MSG_SearchValue value,
				 char *pszArbitraryHeader);

	void GetAction(MSG_RuleActionType *action, void **value);
	void SetAction(MSG_RuleActionType action, void *value);
	int  ChangeLogicText();

// Dialog Data
	//{{AFX_DATA(CFilterDialog)
	enum { IDD = IDD_MAILFILTER };
	int		m_iActive;
	CString	m_lpszFilterName;
	CString	m_lpszFilterDesc;
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFilterDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	void SlideBottomControls(int);

	// Generated message map functions
	//{{AFX_MSG(CFilterDialog)
	afx_msg void OnUpdateDest();
	afx_msg void OnAction();
	afx_msg void OnAttrib1();
	afx_msg void OnAttrib2();
	afx_msg void OnAttrib3();
	afx_msg void OnAttrib4();
	afx_msg void OnAttrib5();
	afx_msg void OnMore();
	afx_msg void OnFewer();
	afx_msg void OnHelp();
	afx_msg void OnAndOr();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg LONG OnFinishedHeaders(WPARAM wParam, LPARAM lParam );
	afx_msg	void OnNewFolder();


	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
#ifndef _WIN32
	afx_msg LRESULT OnDlgSubclass(WPARAM wParam, LPARAM lParam);
#endif
};

#endif
