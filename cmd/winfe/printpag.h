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

#ifndef __PRINTPAG_H
#define __PRINTPAG_H

// printpag.h : header file
//

class CMarginEdit   : public CGenericEdit
{
public:
    CMarginEdit ( ) : CGenericEdit ( ) { };
    DECLARE_MESSAGE_MAP ( );
    afx_msg void OnChar( UINT nChar, UINT nRepCnt, UINT nFlags );
};

class CPagePreview  : public CStatic
{
public:
    CPagePreview ( ) : CStatic ( ) { };
    DECLARE_MESSAGE_MAP ( );
    afx_msg void OnPaint ( );
};

/////////////////////////////////////////////////////////////////////////////
// CPrintPageSetup dialog

class CPrintPageSetup : public CDialog
{
// Construction
public:
	CPrintPageSetup(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CPrintPageSetup)
#ifdef XP_WIN32
	enum { IDD = IDD_PAGESETUP };
#else
	enum { IDD = IDD_PAGESETUP16 };
#endif
    CButton	m_MarginBox;
	CMarginEdit	m_BottomMargin;
	CMarginEdit	m_LeftMargin;
	CMarginEdit	m_RightMargin;
	CMarginEdit	m_TopMargin;
	BOOL	m_bLinesBlack;
	BOOL	m_bTextBlack;
	BOOL	m_bDate;
	BOOL	m_bPageNo;
	BOOL	m_bSolidLines;
	BOOL	m_bTitle;
	BOOL	m_bTotal;
	BOOL	m_bURL;
    BOOL    m_bReverseOrder;
	BOOL    m_bPrintBkImage;
    long    m_InitialTop,m_InitialBottom,m_InitialLeft,m_InitialRight;
	//}}AFX_DATA
    CPagePreview m_PagePreview;

	void ShowPagePreview ( CClientDC & pdc );
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPrintPageSetup)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:
    long lWidth, lHeight;

	int UpdateMargins ( BOOL check = FALSE );
	char * TwipsToString ( long twips, long conversion );	
	char * TwipsToLocaleUnit ( long twips );
	long StringToTwips ( char * szBuffer, long conversion );
	long LocaleUnitToTwips ( char * szBuffer );
	void SetLocaleUnit ( void );
    void MarginError ( char * );

	// Generated message map functions
	//{{AFX_MSG(CPrintPageSetup)
	virtual void OnOK();
    virtual void OnCancel();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif
