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

// edhref.h : header file
//
#ifdef EDITOR
#ifndef _EDHREF_H
#define _EDHREF_H

enum ED_AnchorType{
    ED_ANCHOR_TEXT,
    ED_ANCHOR_IMAGE
};

/////////////////////////////////////////////////////////////////////////////
// CHrefDlg dialog

class CHrefDlg : public CDialog
{
// Construction
public:
    CHrefDlg(CWnd* pParent = NULL,
             MWContext * pMWContext = NULL,
             BOOL bAutoAdjustLinks = TRUE );

// Dialog Data
	//{{AFX_DATA(CHrefDlg)
	enum { IDD = IDD_MAKE_LINK };
	CString	m_csHref;
	CString	m_csAnchorEdit;
	CString	m_csAnchor;
	//}}AFX_DATA

private:
	CString	m_csLastValidHref;

	int		m_nAnchorType;
	BOOL	m_bNewAnchorText;
    BOOL    m_bAutoAdjustLinks;
    
    EDT_ImageData *m_pImageData;

    // This is simply the URL for the current document 
    //  (from History entry)
    char    *m_szBaseDocument;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHrefDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

   	XP_List *    m_pTargetList;

    UINT m_nTargetCount;
    
// Implementation
protected:
    MWContext * m_pMWContext;
    CListBox  * m_pListBox;
    CEdit     * m_pHrefEdit;
    CWnd      * m_pUnlinkButton;
    CWnd      * m_pAnchor;
    CEdit     * m_pAnchorEdit;
    void FillTargetList();
    CNetscapeView* m_pView;
    BOOL        m_bHrefChanged;
    BOOL        m_bValidHref;
    void        ValidateHref();

	// Generated message map functions
	//{{AFX_MSG(CHrefDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnHrefFile();
	afx_msg void OnHrefUnlink();
	afx_msg void OnSelchangeTargetList();
	afx_msg void OnChangeHrefUrl();
	afx_msg void OnKillfocusHrefUrl();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


#endif // _ED_HREF_H
#endif // EDITOR
