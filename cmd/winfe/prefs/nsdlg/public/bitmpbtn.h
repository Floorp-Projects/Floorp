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

#ifndef __BITMPBTN_H_
#define __BITMPBTN_H_

#ifndef _WIN32
// Loads a bitmap and returns the handle. Retrieves the color of the
// first pixel in the image and replaces that entry in the color table
// with COLOR_3DFACE
HBITMAP WINAPI
LoadTransparentBitmap(HINSTANCE hInstance, UINT nResID);
#endif

// Sizes the button to fit the bitmap
void WINAPI
SizeToFitBitmapButton(HWND hwndCtl, HBITMAP hBitmap);

// Draws a bitmap button using the specified bitmap handle and
// memory device
void WINAPI
DrawBitmapButton(LPDRAWITEMSTRUCT lpdis, HBITMAP hBitmap, HDC hMemDC);

#endif /* __BITMPBTN_H_ */

