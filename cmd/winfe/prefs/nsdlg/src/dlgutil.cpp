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

#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include "dlgutil.h"

#ifndef _WIN32
BOOL
DrawPushButtonControl(HDC hdc, LPRECT lprc, UINT nState)
{
	RECT	r;
	UINT	nWidth, nHeight;
	HBRUSH	hbrFace = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
	HBRUSH	hbrShadow = CreateSolidBrush(GetSysColor(COLOR_BTNSHADOW));

	CopyRect(&r, lprc);
	nWidth = r.right - r.left;
	nHeight = r.bottom - r.top;

	if (nState & DPBCS_PUSHED) {
		// Draw the black highlight on the left and top
		PatBlt(hdc, r.left, r.top, nWidth - 1, 1, BLACKNESS);
		PatBlt(hdc, r.left, r.top + 1, 1, nHeight - 2, BLACKNESS);

		// Draw the white edge along the bottom and right
		PatBlt(hdc, r.left, r.bottom - 1, nWidth - 1, 1, WHITENESS);
		PatBlt(hdc, r.right - 1, r.top, 1, nHeight, WHITENESS);
	
	} else {
		// Draw the white highlight on the left and top
		PatBlt(hdc, r.left, r.top, nWidth - 1, 1, WHITENESS);
		PatBlt(hdc, r.left, r.top + 1, 1, nHeight - 2, WHITENESS);
	
		// Draw the black edge along the bottom and right
		PatBlt(hdc, r.left, r.bottom - 1, nWidth - 1, 1, BLACKNESS);
		PatBlt(hdc, r.right - 1, r.top, 1, nHeight, BLACKNESS);
	}

	// Fill with the button face color
	InflateRect(&r, -1, -1);
	FillRect(hdc, &r, hbrFace);

	// Now draw the button shadow lines
	HBRUSH	hOldBrush = SelectBrush(hdc, hbrShadow);

	if (nState & DPBCS_PUSHED) {
		// Along the left and top
		PatBlt(hdc, r.left, r.top, nWidth - 3, 1, PATCOPY);
		PatBlt(hdc, r.left, r.top + 1, 1, nHeight - 4, PATCOPY);

	} else {
		// Along the bottom and right
		PatBlt(hdc, r.left, r.bottom - 1, nWidth - 2, 1, PATCOPY);
		PatBlt(hdc, r.right - 1, r.top, 1, nHeight - 2, PATCOPY);
	}

	SelectBrush(hdc, hOldBrush);

	// Delete GDI objects
	DeleteBrush(hbrFace);
	DeleteBrush(hbrShadow);

	// Adjust the rect if requested. Note we adjust by 2 pixels on all
	// sides even though we only draw one pixel in some cases. This is
	// for compatibility with the Win32 DrawFrameControl() routine
	if (nState & DPBCS_ADJUSTRECT)
		InflateRect(lprc, -2, -2);

	return TRUE;
}
#endif

