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
#include "widgetry.h"

class CProgressMeterClass
{
protected:
 	HWND	m_hWnd;
    int		cubeHeight;
    int		cubeWidth;
    int		cubeCount;
    int 	percent;     
    int		lastUpdate;

	HBRUSH	m_hbrushBlue;

    void PaintCube(HDC hdc, int position);

public:
	static void RegisterClass();

	CProgressMeterClass(HWND hwnd);
	~CProgressMeterClass();

    void OnPaint();
	void OnStep();
	void OnStepTo(int newPercent);
};

//////////////////////////////////////////////////////////////////////////////
// CProgressMeter
// The progress meter draws a 3D progress status inside the static 
// control

#define EDGE_WIDTH  2       // default bevel edge width

CProgressMeterClass::CProgressMeterClass(HWND hwnd)
{
	m_hWnd = hwnd;
	lastUpdate = percent = cubeWidth = cubeHeight = 0;        
	cubeCount = 1;
	m_hbrushBlue = ::CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
}

CProgressMeterClass::~CProgressMeterClass()
{
	VERIFY(::DeleteObject(m_hbrushBlue));
}


void CProgressMeterClass::OnPaint()
{
	PAINTSTRUCT		 	paint;
	BeginPaint(m_hWnd, &paint);
	HDC hdc = paint.hdc;
    RECT rect;                      // get the entire area of the static region
    ::GetClientRect(m_hWnd, &rect);           

	int nCubes, oldmod = 21;

	// try 20 to 30 cubes and select the best fit (least modulo)
	for ( nCubes = 30; nCubes <= 50; nCubes++ )
	{
		int extra = ( rect.right - 4 ) / nCubes;
		int val = ( ( rect.right - 4 ) + ( extra / 2 ) ) % nCubes;

		// is this a better fit than the last try?
		if ( val < oldmod )
		{
			oldmod = val;           // yes, save count
			cubeCount = nCubes;

			// compute the cube width and height for the control
			cubeWidth = ( ( ( rect.right - 4 ) + ( extra / 2 ) ) ) / nCubes;
			cubeHeight = rect.bottom - 4;

			// if this is a perfect fit, look no further
			if ( !val )
				break;
		}
	}		

	WFE_DrawLoweredRect(hdc, &rect);
		
    // draw the center of the control in the button face color
	::InflateRect(&rect, -EDGE_WIDTH, -EDGE_WIDTH);
    ::FillRect( hdc, &rect, sysInfo.m_hbrBtnFace );

    // paint all the cubes that have been painted already (this is for refresh)
    long i;
    long interval = 1000 / cubeCount;
    if ( !interval )
        interval = 1;

    if (percent > 100)
		percent = 100;
	if (percent < 0)
		percent = 0;

    for ( i = 0; i < long(percent) * 1000L / interval / 100; i++ )
        PaintCube( hdc, CASTINT(i) );

	EndPaint(m_hWnd, &paint);
}

// Paint a single cube in the progress control

void CProgressMeterClass::PaintCube ( HDC hdc, int position )
{
    if ( position <= cubeCount )
    {
        RECT MeterRect;
        ::GetClientRect(m_hWnd, &MeterRect);
        RECT rect;

        // cubes get painted 1 pixel less than the inside area with
        // 2 pixel spaces between cubes
        rect.left = EDGE_WIDTH + 2 + ( position * cubeWidth );
        rect.top = EDGE_WIDTH + 3;
        rect.bottom = cubeHeight;
        rect.right = ( rect.left + cubeWidth ) - 2;

        if (rect.right < MeterRect.right)
            ::FillRect(hdc, &rect, m_hbrushBlue);
    }
}

void CProgressMeterClass::OnStepTo(int newPercent)
{
	if (newPercent < percent) {
		lastUpdate = percent = 0;
		RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
	}
    while (percent<newPercent) 
        OnStep();
}

// increase the gauge by 1% and update the cubes if required

void CProgressMeterClass::OnStep()
{
	HDC hdc = GetDC(m_hWnd);

    // did we overstep?
    if ( percent < 100 )
    {
        // get the spacing
        long interval = 1000 / cubeCount;
        if ( !interval )
            interval = 1;
        percent++;

        // time to draw another cube?
        if (long(percent) * 1000L / interval / 100 > lastUpdate)
        {   
            PaintCube ( hdc, lastUpdate );
            lastUpdate = CASTINT(long(percent) * 1000L / interval  / 100);
        }
    }
	ReleaseDC(m_hWnd, hdc);
}

LRESULT CALLBACK EXPORT
ProgressMeterWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	CProgressMeterClass *pObject;

	// If we've been created then create our C++ object
	if (iMsg == WM_CREATE) {
		pObject = new CProgressMeterClass(hwnd);
		if (!pObject)
			return -1;

		SetWindowLong(hwnd, 0, (LONG) pObject);
		return 0;
	}

	// Get the pointer to our C++ object
	pObject = (CProgressMeterClass *)GetWindowLong(hwnd, 0);
	if (!pObject)
		return DefWindowProc(hwnd, iMsg, wParam, lParam);

	switch (iMsg) {
		case WM_DESTROY:
			delete pObject;
			SetWindowLong(hwnd, 0, 0L);
			break;

		case WM_PAINT:
			pObject->OnPaint();
			break;

		case NSPM_STEP:
			pObject->OnStep();
			break;

		case NSPM_STEPTO:
			pObject->OnStepTo((int) wParam);
			break;

		default:
			return DefWindowProc(hwnd, iMsg, wParam, lParam);
	}

	return 0;
}

// Initialization routine

void CProgressMeterClass::RegisterClass()
{
	WNDCLASS	wndclass;

	wndclass.style         = 0;
	wndclass.lpfnWndProc   = ProgressMeterWndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof(CProgressMeterClass *);
	wndclass.hInstance     = AfxGetInstanceHandle();
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = WC_NSPROGRESS;

	::RegisterClass(&wndclass);
}

CProgressMeter::CProgressMeter()
{
	m_hWnd = NULL;
}

BOOL CProgressMeter::SubclassDlgItem(UINT nID, CWnd *pParent)
{
	HWND hwnd = pParent->GetDlgItem(nID)->GetSafeHwnd();
	RECT rcWindow;
	GetWindowRect(hwnd, &rcWindow);
	MapWindowPoints(NULL, pParent->GetSafeHwnd(), (LPPOINT) &rcWindow, 2);

	m_hWnd = ::CreateWindow(WC_NSPROGRESS, NULL, WS_CHILD|WS_VISIBLE,
						    rcWindow.left, rcWindow.top,
						    rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top,
							pParent->GetSafeHwnd(),
						    (HMENU) ::GetDlgCtrlID(hwnd),
						    AfxGetInstanceHandle(),
						    NULL);

	::DestroyWindow(hwnd);
	return m_hWnd != NULL;
}

void CProgressMeter::StepIt()
{
	if (m_hWnd)
		::SendMessage(m_hWnd, NSPM_STEP, (WPARAM) 0, (LPARAM) 0);
}

void CProgressMeter::StepItTo(int newPercent)
{
	if (m_hWnd)
		::SendMessage(m_hWnd, NSPM_STEPTO, (WPARAM) newPercent, (LPARAM) 0);
}

#ifndef _WIN32X

/////////////////////////////////////////////////////////////////////
// CUpDownClass

#define UDT_PAUSE	1
#define UDT_REFRESH	2

static const char szProp[] = "NSUpDown";

class CUpDownClass {
private:
	HWND m_hWnd, m_hwndBuddy;
	UINT m_nBase;
	int m_nPos, m_nLower, m_nUpper;
	DWORD m_dwStyle;
	UINT m_idTimer;
	int m_iHit, m_iState;
	HBITMAP m_hbmArrows;

	enum { udsNone, udsUp, udsDown };

	void _UpdateValue(int nPos);
	int _GetValue();

	void _Up();
	void _Down();

	void _SetState(int iState);

public:
	WNDPROC m_pfSuperProc;

	CUpDownClass(HWND hwnd);
	~CUpDownClass();

	BOOL HandleEvent(UINT iMsg, WPARAM wParam, LPARAM lParam, LRESULT *lResult);

	void OnEnable(BOOL bEnable);
	void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	void OnLButtonDown(UINT nFlags, POINT point);
	void OnLButtonUp(UINT nFlags, POINT point);
	void OnPaint();
	void OnTimer(UINT nID);

	BOOL SetAccel(int nAccel, UDACCEL* pAccel);
	UINT GetAccel(int nAccel, UDACCEL* pAccel) const;
	UINT SetBase(UINT nBase);
	UINT GetBase() const;
	HWND SetBuddy(HWND hwndBuddy);
	HWND GetBuddy() const;
	int SetPos(int nPos);
	int GetPos() const;
	void SetRange(int nLower, int nUpper);
	DWORD GetRange() const;

	static void RegisterClass();
};

// WNDPROCs

LRESULT CALLBACK
UpDownWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	CUpDownClass *pObject;
	POINT pt;

	// If we've been created then create our C++ object
	if (iMsg == WM_CREATE) {
		pObject = new CUpDownClass(hwnd);
		if (!pObject)
			return -1;

		SetWindowLong(hwnd, 0, (LONG) pObject);
		return 0;
	}

	// Get the pointer to our C++ object
	pObject = (CUpDownClass *)GetWindowLong(hwnd, 0);
	if (!pObject)
		return DefWindowProc(hwnd, iMsg, wParam, lParam);

	switch (iMsg) {
		case WM_DESTROY:
			delete pObject;
			SetWindowLong(hwnd, 0, 0L);
			break;

		case WM_ENABLE:
			pObject->OnEnable((BOOL) wParam);
			break;

		case WM_KEYDOWN:
			pObject->OnKeyDown((UINT) wParam, (UINT) LOWORD(lParam), (UINT) HIWORD(lParam));
			break;

		case WM_KEYUP:
			pObject->OnKeyUp((UINT) wParam, (UINT) LOWORD(lParam), (UINT) HIWORD(lParam));
			break;

		case WM_LBUTTONDOWN:
			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);
			pObject->OnLButtonDown((UINT) wParam, pt);
			break;

		case WM_LBUTTONUP:
			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);
			pObject->OnLButtonUp((UINT) wParam, pt);

		case WM_PAINT:
			pObject->OnPaint();
			break;

		case WM_TIMER:
			pObject->OnTimer((UINT) wParam);
			break;

		case UDM_SETRANGE:
			pObject->SetRange((int) HIWORD(lParam), (int) LOWORD(lParam));
			break;

		case UDM_GETRANGE:
			return (LRESULT) pObject->GetRange();

		case UDM_SETPOS:
			pObject->SetPos((int) wParam);

		case UDM_GETPOS:
			return (LRESULT) pObject->GetPos();

		case UDM_SETBUDDY:
			pObject->SetBuddy((HWND) wParam);
			break;

		case UDM_GETBUDDY:
			return (LRESULT) pObject->GetBuddy();

		case UDM_SETACCEL:
			return (LPARAM) pObject->SetAccel((int) wParam, (LPUDACCEL) lParam);

		case UDM_GETACCEL:
			return (LPARAM) pObject->GetAccel((int) wParam, (LPUDACCEL) lParam);
			break;

		case UDM_SETBASE:
			pObject->SetBase((UINT) wParam);
			break;

		case UDM_GETBASE:
			return (LRESULT) pObject->GetBase();

		default:
			return DefWindowProc(hwnd, iMsg, wParam, lParam);
	}

	return 0;
}

LRESULT CALLBACK UpDownSubWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndUpDown = (HWND) ::GetProp(hwnd, szProp);
	if (hwndUpDown) {
		CUpDownClass *pObject = (CUpDownClass *) GetWindowLong(hwndUpDown, 0);
		if (pObject) {
			LRESULT lResult;
			if (!pObject->HandleEvent(iMsg, wParam, lParam, &lResult))
				return CallWindowProc(pObject->m_pfSuperProc, hwnd, iMsg, wParam, lParam );
			else
				return lResult;
		}
	}
	// Tragedy, punt
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

// Constructor/Destructor

static WORD awArrows[] = { 0xef, 0xc7, 0x83, 0x01, 0x83, 0xc7, 0xef, 0x00 };

CUpDownClass::CUpDownClass(HWND hwnd)
{
	m_hWnd = hwnd;
	m_dwStyle = ::GetWindowLong(m_hWnd, GWL_STYLE);
	m_iState = m_iHit = udsNone;

	m_hbmArrows = ::CreateBitmap(8, 8, 1, 1, awArrows);

	m_nPos = 1;
	m_nLower = 1;
	m_nUpper = 100;

	m_pfSuperProc = NULL;
}

CUpDownClass::~CUpDownClass()
{
	if (m_idTimer)
		KillTimer(m_hWnd, m_idTimer);

	SetBuddy(NULL);

	VERIFY(::DeleteObject(m_hbmArrows));
}

// Private methods

void CUpDownClass::_UpdateValue(int nPos)
{
	m_nPos = nPos;
	if (m_hwndBuddy && (m_dwStyle & UDS_SETBUDDYINT)) {
		char szText[17];
		_itoa(m_nPos, szText, 10);
		::SendMessage(m_hwndBuddy, WM_SETTEXT, (WPARAM) 0, (LPARAM) szText);
	}
}

int CUpDownClass::_GetValue()
{
	int nPos = m_nPos;
	if (m_hwndBuddy && (m_dwStyle & UDS_SETBUDDYINT)) {
		char szText[17];
		::SendMessage(m_hwndBuddy, WM_GETTEXT, (WPARAM) 17, (LPARAM) szText);
		int nVal = atoi(szText);
		if (nVal >= m_nLower && nVal <= m_nUpper)
			nPos = nVal;
	}

	return nPos;
}

void CUpDownClass::_Up()
{
	int nPos = _GetValue();
	if (nPos < m_nUpper) {
		nPos++;
	} else if (m_dwStyle & UDS_WRAP) {
		nPos = m_nLower;
	}
	_UpdateValue(nPos);
}

void CUpDownClass::_Down()
{
	int nPos = _GetValue();
	if (nPos > m_nLower) {
		nPos--;
	} else if (m_dwStyle & UDS_WRAP) {
		nPos = m_nUpper;
	}
	_UpdateValue(nPos);
}

void CUpDownClass::_SetState(int iState)
{
	if (m_iState != iState) {
		m_iState = iState;
		::InvalidateRect(m_hWnd, NULL, TRUE);
		::UpdateWindow(m_hWnd);
	}
}

BOOL CUpDownClass::HandleEvent(UINT iMsg, WPARAM wParam, LPARAM lParam, LRESULT *lResult)
{
	*lResult = 0;

	if ((iMsg == WM_KEYDOWN || iMsg == WM_KEYUP) && m_dwStyle & UDS_ARROWKEYS) {
		if (((UINT) wParam) == VK_UP || ((UINT) wParam) == VK_DOWN) {
			if (iMsg == WM_KEYDOWN) {
				OnKeyDown((UINT) wParam, LOWORD(lParam), HIWORD(lParam));
			} else {
				OnKeyUp((UINT) wParam, LOWORD(lParam), HIWORD(lParam));
			}
			return TRUE;
		}
	}

	return FALSE;
}

// WM_ methods

void CUpDownClass::OnEnable(BOOL bEnable)
{
	::InvalidateRect(m_hWnd, NULL, TRUE);
	::UpdateWindow(m_hWnd);
}

void CUpDownClass::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (m_dwStyle & UDS_ARROWKEYS) {
		if (nChar == VK_UP) {
			_Up();
			_SetState(udsUp);
		} else if (nChar = VK_DOWN) {
			_Down();
			_SetState(udsDown);
		}
	}
}

void CUpDownClass::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	_SetState(udsNone);
}

void CUpDownClass::OnLButtonDown(UINT nFlags, POINT point)
{
	::SetCapture(m_hWnd);
	RECT rcClient;
	::GetClientRect(m_hWnd, &rcClient);
	if (point.y < (rcClient.bottom / 2)) {
		m_iHit = udsUp;
		_Up();
	} else {
		m_iHit = udsDown;
		_Down();
	}
	m_idTimer = ::SetTimer(m_hWnd, UDT_PAUSE, 500, NULL);

	_SetState(m_iHit);
}

void CUpDownClass::OnLButtonUp(UINT nFlags, POINT point)
{
	if (::GetCapture() == m_hWnd)
		::ReleaseCapture();

	if (m_idTimer) {
		::KillTimer(m_hWnd, m_idTimer);
		m_idTimer = 0;
	}
	m_iHit = udsNone;
	_SetState(udsNone);
}

void CUpDownClass::OnPaint()
{
	PAINTSTRUCT		 	paint;
	BeginPaint(m_hWnd, &paint);
	HDC hdc = paint.hdc;

	HDC hdcArrows = ::CreateCompatibleDC(hdc);
	HBITMAP hbmOld = (HBITMAP) ::SelectObject(hdcArrows, m_hbmArrows);
	COLORREF oldBk = ::SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
	COLORREF oldText = ::SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));

	RECT rcTop, rcBottom;
	::GetClientRect(m_hWnd, &rcTop);
	rcBottom = rcTop;
	rcTop.bottom = rcTop.bottom / 2;
	rcBottom.top = rcTop.bottom + 1;

	int ux, uy, dx, dy;
	ux = (rcTop.right + 1) / 2 - 4;
	uy = (rcTop.bottom + 1) / 2 - 2;
	dx = ux;
	dy = rcBottom.bottom - uy - 3;

	if (m_iState == udsUp) {
		ux++;
		uy++;
	} else if (m_iState == udsDown) {
		dx++;
		dy++;
	}
	::BitBlt(hdc, ux, uy, 8, 3, hdcArrows, 0, 0, SRCCOPY);
	::BitBlt(hdc, dx, dy, 8, 3, hdcArrows, 0, 4, SRCCOPY);

	WFE_Draw3DButtonRect(hdc, &rcTop, m_iState == udsUp);
	WFE_Draw3DButtonRect(hdc, &rcBottom, m_iState == udsDown);

	::SelectObject(hdcArrows, hbmOld);
	VERIFY(::DeleteDC(hdcArrows));
	::SetBkColor(hdc, oldBk);
	::SetTextColor(hdc, oldText);

	EndPaint(m_hWnd, &paint);
}

void CUpDownClass::OnTimer(UINT nID)
{
	int iState = udsNone;

	if (::GetCapture() == m_hWnd) {
		if (nID == UDT_PAUSE) {
			::KillTimer(m_hWnd, m_idTimer);
			m_idTimer = ::SetTimer(m_hWnd, UDT_REFRESH, 100, NULL);
		}
		
		RECT rcClient;
		::GetClientRect(m_hWnd, &rcClient);
		POINT pt;
		::GetCursorPos(&pt);
		::ScreenToClient(m_hWnd, &pt);

		if (::PtInRect(&rcClient, pt)) {
			iState = (pt.y < rcClient.bottom / 2) ? udsUp : udsDown;
			if (iState == m_iHit) {
				if (iState == udsUp) {
					_Up();
				} else {
					_Down();
				}
			}
		}
	} else {
		::KillTimer(m_hWnd, m_idTimer);
		m_idTimer = 0;
	}
	_SetState(iState);
}

// UDM_ methods

BOOL CUpDownClass::SetAccel(int nAccel, UDACCEL* pAccel)
{
	ASSERT(0);
	return FALSE;
}

UINT CUpDownClass::GetAccel(int nAccel, UDACCEL* pAccel) const
{
	ASSERT(0);
	return 0;
}

UINT CUpDownClass::SetBase(UINT nBase)
{
	int res = m_nBase;
	m_nBase = nBase;
	return res;
}

UINT CUpDownClass::GetBase() const
{
	return m_nBase;
}

HWND CUpDownClass::SetBuddy(HWND hwndBuddy)
{
	HWND res = m_hwndBuddy;

	if (::IsWindow(m_hwndBuddy)) {
		// Unsubclass the parent window        
		if (GetWindowLong(m_hwndBuddy, GWL_WNDPROC) == (LONG) UpDownSubWndProc) {       
			::SetWindowLong(m_hwndBuddy, GWL_WNDPROC, (LONG) m_pfSuperProc);
		}
		VERIFY(RemoveProp(m_hwndBuddy, szProp));
		m_hwndBuddy = NULL;
	}
	if (::IsWindow(hwndBuddy)) {
		m_hwndBuddy = hwndBuddy;
		VERIFY(SetProp(m_hwndBuddy, szProp, (HANDLE) m_hWnd));
		m_pfSuperProc = (WNDPROC) ::SetWindowLong(m_hwndBuddy, GWL_WNDPROC, (LONG) UpDownSubWndProc);
	}
	return res;
}

HWND CUpDownClass::GetBuddy() const
{
	return m_hwndBuddy;
}

int CUpDownClass::SetPos(int nPos)
{
	int res = m_nPos;
	m_nPos = nPos;
	return res;
}

int CUpDownClass::GetPos() const
{
	return m_nPos;
}

void CUpDownClass::SetRange(int nLower, int nUpper)
{
	m_nLower = nLower;
	m_nUpper = nUpper;
}


DWORD CUpDownClass::GetRange() const
{
	return MAKELONG(m_nLower, m_nUpper);
}

void CUpDownClass::RegisterClass()
{
	WNDCLASS	wndclass;

	wndclass.style         = CS_PARENTDC|CS_HREDRAW|CS_VREDRAW;
	wndclass.lpfnWndProc   = UpDownWndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof(CUpDownClass *);
	wndclass.hInstance     = AfxGetInstanceHandle();
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = NSUPDOWN_CLASS;

	::RegisterClass(&wndclass);
}

#endif

#ifdef FEATURE_SPIN_BUTTON
#include "widgetry.i01"
#endif

/////////////////////////////////////////////////////////////////////
// AssortedWidgetInit

void AssortedWidgetInit()
{
	CProgressMeterClass::RegisterClass();
#ifndef _WIN32X
	CUpDownClass::RegisterClass();
#endif
}
