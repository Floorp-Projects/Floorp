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

#include "stdafx.h"
#include "DateEdit.h"


#ifdef DEBUG_mitch
#define DEBUGONLY( stmt ) stmt;
#define DEBUGOUT(s) OutputDebugString(s)
#else
#define DEBUGONLY( stmt ) ;
#define DEBUGOUT(s)
#endif

#ifdef DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define ID_DAY		101
#define ID_MONTH	102
#define ID_YEAR		103

static int _GetMonthDays(int nYear, int nMonth)
{
    static int cDaysInMonth[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if( nYear % 100 != 0 && nYear % 4 == 0 && nMonth == 2)
		return 29;

	return cDaysInMonth[nMonth - 1];
}

/////////////////////////////////////////////////////////////////////////////
// CNSDateEdit public API


//=============================================================== CNSDateEdit
CNSDateEdit::CNSDateEdit()
{
	m_bNeedControls = TRUE;
	m_nDay = m_nMonth = m_nYear = 0;
	m_iCurrentField = 0;
	m_pFields[0] = m_pFields[1] = m_pFields[2] = NULL;
}


//============================================================== ~CNSDateEdit
CNSDateEdit::~CNSDateEdit()
{
}


//=================================================================== SetDate
BOOL CNSDateEdit::SetDate( CTime &d )
{
	return SetDate( d.GetYear(), d.GetMonth(), d.GetDay() );
}


//=================================================================== SetDate
BOOL CNSDateEdit::SetDate( int nYear, int nMonth, int nDay )
{
	m_nDay = nDay;
	m_nMonth = nMonth;
	m_nYear = nYear;
	if ( !m_bNeedControls )
	{
		m_DayField.SetValue( m_nDay );
		m_MonthField.SetValue( m_nMonth );
		m_YearField.SetValue( m_nYear );
	}
	return TRUE;
}


//=================================================================== GetDate
BOOL CNSDateEdit::GetDate( CTime &d )
{
	int nYear, nMonth, nDay;
	if ( !GetDate( nYear, nMonth, nDay ) )
		return FALSE;
	d = CTime( nYear, nMonth, nDay, 0, 0, 0 );
	return TRUE;
}


//=================================================================== GetDate
BOOL CNSDateEdit::GetDate( int &nYear, int &nMonth, int &nDay )
{
	if (Validate()) {
		nYear = m_nYear;
		nMonth = m_nMonth;
		nDay = m_nDay;

		return TRUE;
	}
	return FALSE;
}


/////////////////////////////////////////////////////////////////////////////
// CNSDateEdit message handlers

BEGIN_MESSAGE_MAP(CNSDateEdit, CStatic)
	ON_WM_PAINT()
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_ENABLE()
	ON_WM_GETDLGCODE()
	ON_EN_MAXTEXT(ID_DAY, OnMaxText)
	ON_EN_MAXTEXT(ID_MONTH, OnMaxText)
	ON_EN_MAXTEXT(ID_YEAR, OnMaxText)
	ON_EN_SETFOCUS(ID_DAY, OnFocusDay)
	ON_EN_SETFOCUS(ID_MONTH, OnFocusMonth)
	ON_EN_SETFOCUS(ID_YEAR, OnFocusYear)
	ON_EN_KILLFOCUS(ID_DAY, OnKillFocusDay)
	ON_EN_KILLFOCUS(ID_MONTH, OnKillFocusMonth)
	ON_EN_KILLFOCUS(ID_YEAR, OnKillFocusYear)
	ON_MESSAGE(NSDE_RELAYEVENT, OnRelayEvent)
END_MESSAGE_MAP()


//=================================================================== OnPaint
void CNSDateEdit::OnPaint() 
{
	ASSERT( IsWindow( m_hWnd ) );

	char szSeparator[2];
#ifdef WIN32
	VERIFY( GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_SDATE, szSeparator, 2 ) == 2 );
#else
     static char cName [] = "intl" ;
     GetProfileString (cName, "sDate",  "/", szSeparator,     2) ;
#endif

	BOOL bEnabled = IsWindowEnabled();
	CPaintDC dc(this);
	CBrush winBrush( GetSysColor( bEnabled ? COLOR_WINDOW : COLOR_BTNFACE ) );
	dc.FillRect( &dc.m_ps.rcPaint, &winBrush );

	if ( m_bNeedControls )
		CreateSubWindows( );

	CFont *pOldFont = dc.SelectObject( CFont::FromHandle( (HFONT)::GetStockObject( ANSI_VAR_FONT ) ) );
	int oldMode = dc.SetBkMode( TRANSPARENT );
	COLORREF oldTextColor = dc.SetTextColor( GetSysColor( bEnabled ? COLOR_BTNTEXT : COLOR_GRAYTEXT ) );

	dc.DrawText( szSeparator, -1, m_Sep1, DT_SINGLELINE | DT_CENTER | DT_VCENTER );
	dc.DrawText( szSeparator, -1, m_Sep2, DT_SINGLELINE | DT_CENTER | DT_VCENTER );

	dc.SelectObject( pOldFont );
	dc.SetBkMode( oldMode );
	dc.SetTextColor( oldTextColor );
}


void CNSDateEdit::OnSetFocus(CWnd* pOldWnd) 
{
	if ( m_bNeedControls )
		CreateSubWindows( );

	m_pFields[0]->SetFocus();
	m_iCurrentField = 0;
}

void CNSDateEdit::OnSize( UINT nType, int cx, int cy )
{
	if ( m_bNeedControls )
		CreateSubWindows( );

	if ( nType != SIZE_MINIMIZED ) {
		RECT r;
		::SetRect( &r, 0, 0, cx, cy );
		LayoutSubWindows( &r );
	}
}

void CNSDateEdit::OnEnable( BOOL bEnable )
{
	if ( m_bNeedControls )
		CreateSubWindows( );

	m_YearField.EnableWindow( bEnable );
	m_MonthField.EnableWindow( bEnable );
	m_DayField.EnableWindow( bEnable );

	CStatic::OnEnable( bEnable );

	Invalidate();
}

/////////////////////////////////////////////////////////////////////////////
// CNSDateEdit protected & private methods


//========================================================== CreateSubWindows
void CNSDateEdit::CreateSubWindows( )
{
	m_bNeedControls = FALSE;

	RECT r, rcClient;

	::SetRectEmpty(&r);
	GetClientRect( &rcClient );

	VERIFY( m_YearField.Create( WS_VISIBLE, r, this, (UINT)ID_YEAR ) );
	m_YearField.LimitText( 4 );

	if ( m_nYear != 0 )
		m_YearField.SetValue( m_nYear );
	
	VERIFY( m_MonthField.Create( WS_VISIBLE, r, this, (UINT)ID_MONTH ) );
	m_MonthField.LimitText( 2 );
	if ( m_nMonth != 0 )
		m_MonthField.SetValue( m_nMonth );
	
	VERIFY( m_DayField.Create( WS_VISIBLE, r, this, (UINT)ID_DAY ) );
	m_DayField.LimitText( 2 );
	if ( m_nDay != 0 )
		m_DayField.SetValue( m_nDay );

	m_wndSpin.Create(UDS_WRAP|UDS_ARROWKEYS|UDS_SETBUDDYINT|UDS_NOTHOUSANDS|WS_CHILD|WS_VISIBLE, 
					 CRect(0,0,0,0), this, 104 );

	LayoutSubWindows( &rcClient );

	m_wndSpin.SetBuddy(m_pFields[0]);
	if (m_pFields[0] == &m_DayField) {
		int nDays = _GetMonthDays(m_nYear, m_nMonth);
		m_wndSpin.SetRange(1, nDays);
		m_wndSpin.SetPos(m_nDay);
	} else if (m_pFields[0] == &m_MonthField) {
		m_wndSpin.SetRange(1, 12);
		m_wndSpin.SetPos(m_nMonth);
	} else {
		m_wndSpin.SetRange(1970, 2037);
		m_wndSpin.SetPos(m_nMonth);
	}
}

void CNSDateEdit::LayoutSubWindows( RECT *pRect )
{
	HDC hDC = ::GetDC( m_hWnd );
	ASSERT( hDC );

	int w = pRect->right - pRect->left;
	RECT r, rcSep, rcNum;
	TCHAR szSeparator[2], szFormat[30], *s;
	HFONT hFont =  (HFONT)::GetStockObject( ANSI_VAR_FONT );
	CFont *fontCur = CFont::FromHandle( hFont );

	LOGFONT lf;
	GetObject( hFont, sizeof(lf), &lf);

	// Get locale info
#ifdef WIN32
	VERIFY( GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, szFormat, 30 ) > 0 );
	VERIFY( GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_SDATE, szSeparator, 2 ) > 0 );
#else
	static char cName [] = "intl" ;
	GetProfileString (cName, "sDate",  "/", szSeparator,     2) ;

	int iDate = GetProfileInt (cName, "iDate", 0) ;
    sprintf( szFormat, "%c%s%c%s%c",
             iDate == 1 ? 'd' : iDate == 2 ? 'y' : 'M', szSeparator,
             iDate == 1 ? 'M' : iDate == 2 ? 'M' : 'd', szSeparator,
             iDate == 1 ? 'y' : iDate == 2 ? 'd' : 'y' );
#endif
	
	// Get font info
	::SetRectEmpty(&rcSep);
	::SetRectEmpty(&rcNum);

	DrawText( hDC, szSeparator, -1, &rcSep, 
			  DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_CALCRECT );
	DrawText( hDC, _T("0"), -1, &rcNum, 
			  DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_CALCRECT );
	
	ASSERT( w >= ( rcSep.right * 2 + rcNum.right * 8 ) );

	// Start on the left

	int iHeight = lf.lfHeight;
	int iWidth = 0;

	r.top = pRect->top - ((pRect->top-pRect->bottom)+iHeight)/2 - 1;
	r.bottom = r.top + iHeight + 2;

	r.right = pRect->left + 1;

	// this loop parses the short date format, creating the fields as it goes
	s = strtok( szFormat, szSeparator );
	for( int i = 0; (i < 3) && (s != NULL); i++ )
	{
		switch ( s[0] )
		{
		case 'M':
		case 'm':
			r.left = r.right + 1;
			r.right = r.left + rcNum.right * 2 + iWidth; // room for two characters
			m_MonthField.MoveWindow( &r );
			m_MonthField.SetFont( fontCur );
			m_pFields[i] = &m_MonthField;
			break;
		case 'Y':
		case 'y':
			r.left = r.right + 1;
			r.right = r.left + rcNum.right * 4 + iWidth; // room for four characters
			m_YearField.MoveWindow( &r );
			m_YearField.SetFont( fontCur );
			m_pFields[i] = &m_YearField;
			break;
		case 'D':
		case 'd':
			r.left = r.right + 1;
			r.right = r.left + rcNum.right * 2 + iWidth; // room for two characters
			m_DayField.MoveWindow( &r );
			m_DayField.SetFont( fontCur );
			m_pFields[i] = &m_DayField;
			break;
		default:
			DebugBreak();
		}
		if ( i == 0 )
		{
			r.left = r.right + 1;
			r.right = r.left + rcSep.right;
			m_Sep1 = r;
		}
		else if ( i == 1 )
		{
			r.left = r.right + 1;
			r.right = r.left + rcSep.right;
			m_Sep2 = r;
		}

		s = strtok( NULL, szSeparator );
	}

	r = *pRect;
	r.left = r.right - GetSystemMetrics(SM_CXVSCROLL);

	m_wndSpin.MoveWindow(&r);

	::ReleaseDC( m_hWnd, hDC );
}

//================================================================ Validate
BOOL CNSDateEdit::Validate()
{
	int nYear = m_YearField.GetValue();
	int nMonth = m_MonthField.GetValue();
	int nDay = m_DayField.GetValue();

	BOOL res = TRUE;

	if (nYear >= 1970 && nYear < 2038 && nMonth >= 1 && nMonth <= 12 && nDay >= 1 && nDay <= 31) {
		CTime ctime(nYear, nMonth, nDay, 0, 0, 0);

		res = (ctime.GetYear() == nYear) && 
			  (ctime.GetMonth() == nMonth) && 
			  (ctime.GetDay() == nDay);
	} else {
		res = FALSE;
	}

	if (res) {
		m_nDay = nDay;
		m_nMonth = nMonth;
		m_nYear = nYear;
	} else {
		m_DayField.SetValue(m_nDay);
		m_MonthField.SetValue(m_nMonth);
		m_YearField.SetValue(m_nYear);
	}
	return res;
}

//================================================================ OnKeyPress
BOOL CNSDateEdit::OnKeyPress( UINT nKey, UINT nRepCnt, UINT nFlags )
{
	if (nKey == VK_TAB || nKey == VK_SPACE)
	{
		// Don't move on to the next field/window if the current
		// value is invalid.
		if ( !Validate() )
		{
			MessageBeep( MB_OK );
			m_pFields[m_iCurrentField]->SetSel( 0, -1 );
			return TRUE;
		}
		// Move the focus to the next field or control
		BOOL bShift = GetKeyState( VK_SHIFT ) & 0x8000;

		if (bShift) {
			if (m_iCurrentField > 0 || nKey == VK_SPACE)
				m_iCurrentField += 2;
			else
				return FALSE;
		} else {
			if (m_iCurrentField < 2 || nKey == VK_SPACE)
				m_iCurrentField += 1;
			else
				return FALSE;
		}

		m_iCurrentField %= 3;

		m_pFields[m_iCurrentField]->SetFocus();

		return TRUE; // handled
	}

	return FALSE; // not handled - let the child continue processing this key
}

//============================================================= OnMaxText
void CNSDateEdit::OnMaxText()
{
}

//============================================================= OnFocusDay
void CNSDateEdit::OnFocusDay()
{
	int nDays = _GetMonthDays(m_nYear, m_nMonth);

	for (int i = 0; i < 3; i++) {
		if (m_pFields[i] == &m_DayField)
			m_iCurrentField = i;
	}
	m_wndSpin.SetBuddy(&m_DayField);
	m_wndSpin.SetRange(1, nDays);
	m_wndSpin.SetPos(m_nDay);
}

//============================================================= OnFocusMonth
void CNSDateEdit::OnFocusMonth()
{
	for (int i = 0; i < 3; i++) {
		if (m_pFields[i] == &m_MonthField)
			m_iCurrentField = i;
	}
	m_wndSpin.SetBuddy(&m_MonthField);
	m_wndSpin.SetRange(1, 12);
	m_wndSpin.SetPos(m_nMonth);
}

//============================================================= OnFocusYear
void CNSDateEdit::OnFocusYear()
{
	for (int i = 0; i < 3; i++) {
		if (m_pFields[i] == &m_YearField)
			m_iCurrentField = i;
	}
	m_wndSpin.SetBuddy(&m_YearField);
	m_wndSpin.SetRange(1970, 2037);
	m_wndSpin.SetPos(m_nYear);
}

//============================================================= OnKillFocusDay
void CNSDateEdit::OnKillFocusDay()
{
	if (!Validate()) {
		MessageBeep( MB_OK );
	}
}

//============================================================= OnKillFocusMonth
void CNSDateEdit::OnKillFocusMonth()
{
	if (!Validate()) {
		MessageBeep( MB_OK );
	}
}

//============================================================= OnKillFocusYear
void CNSDateEdit::OnKillFocusYear()
{
	if (!Validate()) {
		MessageBeep( MB_OK );
	}
}

LRESULT CNSDateEdit::OnRelayEvent(WPARAM wParam, LPARAM lParam)
{
	LPMSG pMsg = (LPMSG) lParam; 
	if (pMsg->message == WM_KEYDOWN) {
		return (LRESULT) OnKeyPress( (UINT) pMsg->wParam, (UINT) LOWORD(pMsg->lParam), (UINT) HIWORD(pMsg->lParam));
	}
	return (LRESULT) FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CNSDateSubedit

//============================================================ CNSDateSubedit
CNSDateSubedit::CNSDateSubedit()
{
}


//=========================================================== ~CNSDateSubedit
CNSDateSubedit::~CNSDateSubedit()
{
}

BOOL CNSDateSubedit::PreTranslateMessage( MSG* pMsg )
{
	if (GetParent()->SendMessage(NSDE_RELAYEVENT, (WPARAM) 0, (LPARAM) pMsg))
		return TRUE;

	return CEdit::PreTranslateMessage(pMsg);
}


/////////////////////////////////////////////////////////////////////////////
// CNSDateSubedit message handlers

BEGIN_MESSAGE_MAP(CNSDateSubedit, CEdit)
	ON_WM_CHAR()
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()

//==================================================================== OnChar
void CNSDateSubedit::OnChar( UINT nChar, UINT nRepCnt, UINT nFlags ) 
{
	if ( ((nChar >= '0') && (nChar <= '9')) || (nChar == 0x08) ) {
		CEdit::OnChar(nChar, nRepCnt, nFlags);
	} else if (nChar == ' ') {
		MSG msg;
		msg.message = WM_KEYDOWN;
		msg.wParam = (WPARAM) VK_SPACE;
		GetParent()->SendMessage(NSDE_RELAYEVENT, (WPARAM) 0, (LPARAM) &msg);
	} else {
		MessageBeep( MB_OK );
	}
}

//=============================================================== OnSetFocus
void CNSDateSubedit::OnSetFocus( CWnd* pOldWnd )
{
	CEdit::OnSetFocus( pOldWnd );
	SetSel(0,-1);
}

//================================================================== SetValue
int CNSDateSubedit::SetValue( int nNewValue )
{
	char buff[10];
	SetWindowText( itoa( nNewValue, buff, 10 ) );
	return nNewValue;
}


//================================================================== GetValue
int CNSDateSubedit::GetValue( void )
{
	char buff[10];
	GetWindowText( buff, 10 );
	if ( strlen( buff ) > 0 )
		return atoi( buff );
	return 0;
}
