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
#include "colorbtn.h"
#include "dlgutil.h"

void WINAPI
DrawColorButtonControl(LPDRAWITEMSTRUCT lpdis, HBRUSH hBrush)
{
	UINT	uState;

#ifdef _WIN32
	uState = DFCS_BUTTONPUSH | DFCS_ADJUSTRECT;

	if (lpdis->itemState & ODS_SELECTED)
		uState |= DFCS_PUSHED;
	DrawFrameControl(lpdis->hDC, &lpdis->rcItem, DFC_BUTTON, uState);
#else
	uState = DPBCS_ADJUSTRECT;
	
	if (lpdis->itemState & ODS_SELECTED)
		uState |= DPBCS_PUSHED;
	DrawPushButtonControl(lpdis->hDC, &lpdis->rcItem, uState);
#endif

	// If the button's selected, then shift everything down and over to the right
	if (lpdis->itemState & ODS_SELECTED)
		OffsetRect(&lpdis->rcItem, 1, 1);

	// The focus rect is inset one pixel on each edge
	//
	// Note: the reason we don't add one to the left and top is that DrawFrameControl()
	// always adjusts the rect by SM_CXEDGE/SM_CYEDGE on all sides even if it only draws
	// a one pixel on the left and top
	lpdis->rcItem.right--;
	lpdis->rcItem.bottom--;
	if ((lpdis->itemAction & ODA_FOCUS) || (lpdis->itemState & ODS_FOCUS))
		DrawFocusRect(lpdis->hDC, &lpdis->rcItem);

	// Draw the specified color framed with a black border just inside the focus rect
	if (!(lpdis->itemState & ODS_DISABLED)) {
		HBRUSH	hOldBrush = SelectBrush(lpdis->hDC, hBrush);
	
		InflateRect(&lpdis->rcItem, -1, -1);
		Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top, lpdis->rcItem.right, lpdis->rcItem.bottom);
		SelectBrush(lpdis->hDC, hOldBrush);
	}
}

