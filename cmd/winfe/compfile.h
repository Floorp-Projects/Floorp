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

#ifndef __COMPFILE_H
#define __COMPFILE_H

#include "msgcom.h"
#include "winproto.h"

class CNSAttachDropTarget;

class CNSAttachmentList  :   public  CListBox
{
public:
	HFONT m_cfTextFont;
    CNSAttachmentList(MSG_Pane * pPane = NULL);
    ~CNSAttachmentList();
    
    CNSAttachDropTarget * m_pDropTarget;
    MSG_Pane * m_pPane;
    MSG_Pane * GetMsgPane() { return m_pPane; }
    
    void AttachFile();
    void AttachUrl(char * pUrl = NULL);
    void RemoveAttachment();
    void UpdateAttachments();
    char * GetAttachmentName(
        char * prompt, int type = ALL, XP_Bool bMustExist = TRUE, BOOL * pOpenIntoEditor = NULL);
    void Cleanup();
    void AddAttachment(char * pName);
    BOOL Create(CWnd * pWnd, UINT id);
    BOOL ProcessDropTarget(COleDataObject * pDataObject);

	 virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	 virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	 virtual void DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct);

    afx_msg void    OnDropFiles( HDROP hDropInfo );
protected:
	 UINT	ItemFromPoint(CPoint pt, BOOL& bOutside) const;		
    
    afx_msg int     OnCreate( LPCREATESTRUCT lpCreateStruct );
    afx_msg void    OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) ;
    afx_msg void    OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void    OnDelete();
    afx_msg void    OnDestroy();
    afx_msg void OnUpdateDelete(CCmdUI * pCmdUI);
    
	 DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CLocationDlg dialog

class CLocationDlg : public CDialog
{
// Construction
public:
	CLocationDlg(char * pUrl = NULL, CWnd* pParent = NULL);   // standard constructor

    char * m_pUrl;
// Dialog Data
	//{{AFX_DATA(CLocationDlg)
	enum { IDD = IDD_ATTACHLOCATION };
	CEdit	m_LocationBox;
	CString	m_Location;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLocationDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLocationDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
#endif
