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

// edtable.h : header file
//
#ifdef EDITOR
#ifndef _EDTABLE_H
#define _EDTABLE_H

//#include "edres1.h"
#include "edtrccln.h"
#include "edprops.h" // For CColorButton

// Limits on table parameters
#define MAX_TABLE_ROWS    100
#define MAX_TABLE_COLUMNS 100
// This is also limit used for cell size, padding, and borders
#define MAX_TABLE_PIXELS  10000

////////////////////////////////////////////////////
// Property Pages for Tabbed Table dialogs
class CTablePage : public CNetscapePropertyPage
{
public:
    CTablePage(CWnd *pParent, MWContext * pMWContext = NULL,
               CEditorResourceSwitcher * pResourceSwitcher = NULL,
               EDT_TableData * pTableData = NULL);
    ~CTablePage();

    void OnOK();

	//{{AFX_DATA(CTablePage)
    enum { IDD = IDD_PAGE_TABLE };   // Put Dialog ID here
	int		m_iRows;
	int		m_iColumns;
	int		m_iAlign;
	int		m_iCaption;
	int		m_iBorderWidth;
	int		m_iCellPadding;
	int     m_iCellSpacing;
	BOOL	m_bColumnHeader;
	BOOL	m_bUseColor;
	BOOL	m_bUseWidth;
	BOOL	m_bUseHeight;
	int		m_iHeightType;
	BOOL	m_bRowHeader;
	int		m_iWidthType;
	CString	m_csBackgroundImage;
    BOOL    m_bNoSave;
    BOOL    m_bBorderWidthDefined;
	//}}AFX_DATA


	// Implementation
protected:              

    CColorButton m_ChooseColorButton;
    BOOL         m_bActivated;
    BOOL         m_bModified;
    BOOL         OnSetActive();
	int		     m_iWidth;
	int		     m_iHeight;

private:
    MWContext		  *m_pMWContext;
    EDT_TableData	  *m_pTableData;
    COLORREF           m_crColor;

    BOOL               m_bImageChanged;
    BOOL               m_bValidImage;

    CColorButton       m_ColorButton;
    BOOL               m_bCustomColor;
    int32              m_iParentWidth;
    int32              m_iParentHeight;
    int                m_iStartColumns;
    int                m_iStartRows;

    // Set before calling UpdateWidthAndHeight()
    //  to not set the associated checkbox
    BOOL               m_bInternalChangeEditbox;

    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;

#ifdef XP_WIN16
    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;
#endif

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTablePage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnKillActive();
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CTablePage)
		// NOTE: the ClassWizard will add member functions here
    afx_msg void OnChangeWidth();
    afx_msg void OnChangeWidthType();
    afx_msg void OnChangeHeight();
    afx_msg void OnChangeHeightType();
    afx_msg void OnOverrideColor();
	afx_msg void OnChooseColor();
	afx_msg void OnExtraHTML();
	afx_msg void OnUseBkgrndImage();
    afx_msg void OnChooseBkgrndImage();
    afx_msg void OnChangeBkgrndImage();
    afx_msg void OnNoSave();
	afx_msg void OnHelp();
    afx_msg void OnChangeBorder();
	afx_msg void EnableApplyButton();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

    void SetModified(BOOL bModified);
};

class CTableCellPage : public CNetscapePropertyPage
{
public:
    CTableCellPage(CWnd *pParent, MWContext * pMWContext = NULL,
                   CEditorResourceSwitcher * pResourceSwitcher = NULL,
                   EDT_TableCellData * pCellData = NULL,
                   UINT nIDCaption = 0);

    ~CTableCellPage();

    void OnOK();

	//{{AFX_DATA(CTableCellPage)
    enum { IDD = IDD_PAGE_TABLE_CELL };   // Put Dialog ID here
	int		m_iAlign;
	int		m_iVAlign;
	int		m_iColumnSpan;
	int		m_iRowSpan;
    int 	m_iUseColor;
	int     m_iUseWidth;
	int 	m_iUseHeight;
	int 	m_iHeader;
	int 	m_iNoWrap;
	int		m_iHeightType;
	int		m_iWidthType;
	CString	m_csBackgroundImage;
    BOOL    m_bNoSave;
	//}}AFX_DATA

// Implementation
protected:              
    CColorButton m_ChooseColorButton;
    BOOL         m_bActivated;
    BOOL         m_bModified;
	int	         m_iWidth;
	int	         m_iHeight;
    BOOL         OnSetActive();

private:
    MWContext         *m_pMWContext;
    EDT_TableCellData *m_pCellData; 
    COLORREF           m_crColor;
    CString            m_csTitle;
    
    // Text on page's tab
    CString            m_csSingleCell;
    CString            m_csSelectedCells;
    CString            m_csSelectedRow;
    CString            m_csSelectedCol;


    BOOL               m_bImageChanged;
    BOOL               m_bValidImage;
    CColorButton       m_ColorButton;
    BOOL               m_bCustomColor;
    ED_HitType         m_iSelectionType;

    int32              m_iParentWidth;
    int32              m_iParentHeight;

    // Set before calling UpdateWidthAndHeight()
    //  to not set the associated checkbox
    BOOL               m_bInternalChangeEditbox;

    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;

    
#ifdef XP_WIN16
    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;
#endif

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTableCellPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnKillActive();
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CTableCellPage)
		// NOTE: the ClassWizard will add member functions here
    afx_msg void OnChangeHAlign();
    afx_msg void OnChangeVAlign();
    afx_msg void OnChangeWidth();
    afx_msg void OnChangeWidthType();
    afx_msg void OnChangeHeight();
    afx_msg void OnChangeHeightType();
	afx_msg void OnOverrideColor();
	afx_msg void OnChooseColor();
	afx_msg void OnExtraHTML();
	afx_msg void OnUseBkgrndImage();
    afx_msg void OnChooseBkgrndImage();
    afx_msg void OnChangeBkgrndImage();
    afx_msg void OnPrevious();
    afx_msg void OnNext();
    afx_msg void OnChangeSelectionType();
	afx_msg void OnHelp();
	afx_msg void EnableApplyButton();
    afx_msg void OnInsert();
    afx_msg void OnDelete();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

    void ChangeSelection(ED_MoveSelType iMoveType);
    // Set 2-state vs. 3-state checkbox and return checkbox state
    int InitCheckbox(UINT nIDCheckbox, ED_CellFormat cf, BOOL bSetState);

    // Init controls separated from OnInitDialog()
    //  to allow switching cells while within the dialog
    void InitPageData();

    void SetModified(BOOL bModified);
};

#endif // _EDTABLE_H
#endif // EDITOR
