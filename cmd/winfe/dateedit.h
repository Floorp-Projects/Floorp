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

#ifndef _DateEdit_h
#define _DateEdit_h

// Requires
#include "widgetry.h"

/////////////////////////////////////////////////////////////////////////////
// CNSDateEdit window

class CNSDateSubedit : public CEdit
{
public:
	CNSDateSubedit();
	virtual ~CNSDateSubedit();
	int SetValue( int nNewValue );
	int GetValue( void );

protected:
	BOOL PreTranslateMessage( MSG* pMsg ); 

	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	DECLARE_MESSAGE_MAP()
};

#define NSDE_RELAYEVENT	(WM_USER+1)

class CNSDateEdit: public CStatic
{
	friend CNSDateSubedit;

public:
	CNSDateEdit();
	virtual ~CNSDateEdit();

	BOOL SetDate( CTime &d );
	BOOL SetDate( int nYear, int nMonth, int nDay );
	// These two methods return FALSE if the current date is invalid
	BOOL GetDate( CTime &d );
	BOOL GetDate( int &nYear, int &nMonth, int &nDay );

protected:

	void			CreateSubWindows( );
	void			LayoutSubWindows( RECT *pRect );
	BOOL			OnKeyPress( UINT nKey, UINT nRepCnt, UINT nFlags );

	BOOL			Validate();

	int				m_nDay, m_nMonth, m_nYear;
	BOOL			m_bNeedControls;
	CNSDateSubedit	m_DayField;
	CNSDateSubedit	m_MonthField;
	CNSDateSubedit	m_YearField;
	CNSDateSubedit *m_pFields[3];
	CSpinButtonCtrl	m_wndSpin;

	int				m_iCurrentField;
	CRect			m_Sep1;
	CRect			m_Sep2;

	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnEnable( BOOL bEnable );
	afx_msg void OnMaxText();
	afx_msg void OnFocusDay();
	afx_msg void OnFocusMonth();
	afx_msg void OnFocusYear();
	afx_msg void OnKillFocusDay();
	afx_msg void OnKillFocusMonth();
	afx_msg void OnKillFocusYear();
	afx_msg LRESULT OnRelayEvent(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif
