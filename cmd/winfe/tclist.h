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

// TabList.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTCList window
#define MIN_TAB_SIZE		4	// minimum number of tab settings

class CTCList : public CListBox
{
// Construction
public:
	CTCList();

// Attributes
public:
	void	SetColumnSpace(int nSpacing);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTCList)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTCList();

	// Generated message map functions
protected:
	//{{AFX_MSG(CTCList)
	afx_msg LRESULT OnAddString(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnInsertString(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDeleteString(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	// Helper methods
	void	CalculateAvgCharWidth(CDC* pDC);
	void	CalculateTabs();
	BOOL	CalculateColWidths(LPCSTR pString, BOOL bSetWidths = TRUE);
	void	InitColWidth();
	void	Recalc();  


	int			m_nAvgCharWidth;	// avg character width
	int			m_nSpacing;			// number of column spaces
	CUIntArray	m_aTabs;			// array of tab settings
	CUIntArray	m_aColWidth;		// array of maximum column widths
	UINT		m_nTabs;			// number of tabs
	
	// character set used to calculate average character width,
	// only allocate one character set for all instances of classes
	static CString m_strCharSet;
};

/////////////////////////////////////////////////////////////////////////////
