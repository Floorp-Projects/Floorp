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

// NSAdrNam.h : header file
//

#ifndef __NSADRNAM_H__
#define __NSADRNAM_H__

#include "apiaddr.h"
/////////////////////////////////////////////////////////////////////////////
// CNSAddressNameEditField window
#include "genedit.h"
class CNSAddressNameEditField : public CGenericEdit
{
// Construction
public:
	CNSAddressNameEditField(CNSAddressList *pParentAddressList);
    HFONT m_cfTextFont;
	BOOL m_bNameCompletion;
	BOOL m_bAttemptNameCompletion;
	int  m_iTextHeight;
	UINT m_uTypedownClock;
	BOOL m_bSetTimerForCompletion;
	MSG_Pane *m_pPickerPane;
	CNSAddressList *m_pParentAddressList;

	void StartNameCompletion(void);
	void StopNameCompletion(void);
	void DrawNameCompletion(void);
    void SetNameCompletionFlag(BOOL bComplete) { m_bAttemptNameCompletion = bComplete; }
	BOOL Create( CWnd *pParent, MWContext *pContext );


  void Paste();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNSAddressNameEditField)
	//}}AFX_VIRTUAL

// Implementation
public:
    LPADDRESSPARENT m_pIAddressParent;

    void  SetControlParent(LPADDRESSPARENT pIAddressParent) 
        { m_pIAddressParent = pIAddressParent; }
	virtual ~CNSAddressNameEditField();

    void  SetCSID(int16 csid);
	// Generated message map functions
protected:

	//{{AFX_MSG(CNSAddressNameEditField)
	afx_msg void OnKillFocus(CWnd * pNewWnd);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnTimer( UINT  nIDEvent );
	//}}AFX_MSG

    afx_msg BOOL PreTranslateMessage( MSG* pMsg );

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif __NSADRNAM_H__	// end define of CNSAddressNameEditField
