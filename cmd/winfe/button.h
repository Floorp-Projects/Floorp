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

// button.h : header file
//

#ifndef _BUTTON_H_
#define _BUTTON_H_

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CNetscapeButton Class

class CNetscapeButton: public CButton
{
	DECLARE_DYNAMIC(CNetscapeButton)

protected:
    MWContext * m_Context;
    LO_FormElementStruct * m_Form;
    XP_Bool m_bDepressed;
    CEdit * m_pwndEdit;
    XP_Bool m_callBase;
    CPaneCX * m_pPaneCX;

    // Construction
public:
    CNetscapeButton(MWContext * context, LO_FormElementStruct * form, CWnd* pParent = NULL);   // standard constructor
	inline void RegisterForm(LO_FormElementStruct * form) { m_Form = form; }
    inline void RegisterEdit(CEdit * pEdit) { m_pwndEdit = pEdit; }
	inline void RegisterContext(MWContext * context)	{ m_Context = context; }

    // Attributes
    LO_FormElementStruct*	GetElement() const {return m_Form;}

    // needs to be seen by closure
    LO_FormElementStruct *m_pLastSelectedRadio;

// Implementation
protected:
    CWnd      * m_Parent;

    // Generated message map functions
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnSetFocus(CWnd * pOldWnd);
    afx_msg void OnKillFocus(CWnd * pNewWnd);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()
	
public:
    void Click(UINT nFlags, BOOL bOnChar /*= FALSE*/, BOOL &bReturnImmediately);
    void OnButtonEvent(CL_EventType type, UINT nFlags, CPoint point);
};

#endif /* _BUTTON_H_ */
