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

// medit.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CURLBar dialog

#ifndef _MEDIT_H_
#define _MEDIT_H_

#ifndef _WIN32
#define GET_WM_COMMAND_CMD(wp, lp)	((UINT)HIWORD(lp))
#endif

class far CNetscapeEdit : public CGenericEdit
{
	DECLARE_DYNAMIC(CNetscapeEdit)

public:
    CNetscapeEdit();

protected:
	MWContext             * m_Context;
	LO_FormElementStruct  * m_Form;
	int             	m_Submit;
	XP_Bool			m_callBase;
#ifdef DEBUG
    XP_Bool m_bOnCreateCalled;
#endif

//	Way to get text data out of a form element if it really has some.
protected:
	lo_FormElementTextData *GetTextData();
	XP_Bool			UpdateEditField();
	void                    UpdateAndCheckForChange();

#ifdef XP_WIN16 
	// 
	// Functions and bookkeeping to set up proper segment locations for
	//   win16 edit elements
	//
protected:
	HINSTANCE              m_hInstance;
	virtual BOOL PreCreateWindow( CREATESTRUCT& cs );
public:
	inline void SetInstance(HINSTANCE hInst) { m_hInstance = hInst; }
#endif

public:
	virtual	void	GetWindowText(CString& rString) { CGenericEdit::GetWindowText(rString); };	// make it virtual so we can overload it in odctrl.h
	virtual	int		GetWindowText(LPTSTR lpszStringBuf, int nMaxCount) { return CGenericEdit::GetWindowText(lpszStringBuf, nMaxCount); };	// make it virtual so we can overload it in odctrl.h

	BOOL SetContext(MWContext * context, LO_FormElementStruct * form = NULL);
	LO_FormElementStruct *GetFormElement();
	void OnEditKeyEvent(CL_EventType type, UINT nChar, UINT nRepCnt, UINT nFlags);

	inline void SetSubmit(int state) { m_Submit = state; }
	inline void RegisterForm(LO_FormElementStruct * form) { m_Form = form; }

protected:
	//{{AFX_MSG(CNetscapeEdit)
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnChar(UINT nChar, UINT nCnt, UINT nFlags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnSetFocus(CWnd * pOldWnd);
	afx_msg void OnKillFocus(CWnd * pNewWnd);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

class CMochaListBox : public CListBox
{
protected:
	MWContext            * m_Context;
	LO_FormElementStruct * m_Form;

public:
	void SetContext(MWContext * context, LO_FormElementStruct * form = NULL);
	inline void RegisterForm(LO_FormElementStruct * form) { m_Form = form; }

protected:
	void CheckForChange();

#if !defined(MSVC4)
	// We can't get OnSelChange() calls so do it by hand
	virtual BOOL OnChildNotify(UINT nMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
#endif	// MSVC4

	//{{AFX_MSG(CMochaListBox)
	afx_msg void OnSetFocus(CWnd * pOldWnd);
	afx_msg void OnKillFocus(CWnd * pNewWnd);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
#if defined(MSVC4)
	afx_msg void OnSelChange();
#endif	// MSVC4
	//}}AFX_MSG

    DECLARE_MESSAGE_MAP()
};

class CMochaComboBox : public CComboBox
{
protected:
	MWContext            * m_Context;
	LO_FormElementStruct * m_Form;

public:
	void SetContext(MWContext * context, LO_FormElementStruct * form = NULL);
	inline void RegisterForm(LO_FormElementStruct * form) { m_Form = form; }

protected:
	void CheckForChange();

#if !defined(MSVC4)
	// We can't get OnSelChange() calls so do it by hand
	virtual BOOL OnChildNotify(UINT nMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
#endif	// MSVC4

	//{{AFX_MSG(CMochaComboBox)
	afx_msg void OnSetFocus(CWnd * pOldWnd);
	afx_msg void OnKillFocus(CWnd * pNewWnd);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
#if defined(MSVC4)
	afx_msg void OnSelChange();
#endif	// MSVC4
	//}}AFX_MSG

    DECLARE_MESSAGE_MAP()
};


#endif // _MEDIT_H_
