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

#ifndef _FEGUI_H
#define _FEGUI_H

//	Draw helpers.
void WFE_DrawRaisedRect( HDC hDC, LPRECT lpRect );
void WFE_DrawLoweredRect( HDC hDC, LPRECT lpRect );
void WFE_Draw3DButtonRect( HDC hDC, LPRECT lpRect, BOOL bPushed );
void WFE_FillSolidRect(CDC *pDC, CRect crRect, COLORREF rgbFill);

#define WFE_DT_CROPRIGHT	0x0001
#define WFE_DT_CROPLEFT		0x0002
#define WFE_DT_CROPCENTER	0x0004
	
int WFE_DrawTextEx( int iCSID, HDC hdc, LPTSTR lpchText, int cchText, LPRECT lprc, UINT dwDTFormat,
					 UINT dwMoreFormat );

// Bitmap helpers

#define RGB_TRANSPARENT RGB(255,0,255)

void WFE_MakeTransparentBitmap( HDC hDC, HBITMAP hBitmap );
HBITMAP WFE_LoadSysColorBitmap( HINSTANCE hInst, LPCSTR lpszResource );
// Forces bitmap to be compatible with our palette

#endif
