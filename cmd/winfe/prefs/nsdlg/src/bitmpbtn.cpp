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
#include "bitmpbtn.h"
#include "dlgutil.h"

#ifndef _WIN32
// Loads a bitmap and returns the handle. Retrieves the color of the
// first pixel in the image and replaces that entry in the color table
// with COLOR_3DFACE
HBITMAP WINAPI
LoadTransparentBitmap(HINSTANCE hInstance, UINT nResID)
{
	// Find the resource
	HRSRC	hrsrc = FindResource(hInstance, MAKEINTRESOURCE(nResID), RT_BITMAP);

	assert(hrsrc);
	if (!hrsrc)
		return NULL;

	// Get a handle for the resource
	HGLOBAL	hglb = LoadResource(hInstance, hrsrc);
	if (!hglb)
		return NULL;

	// Get a pointer to the BITMAPINFOHEADER
	LPBITMAPINFOHEADER	lpbi = (LPBITMAPINFOHEADER)LockResource(hglb);

	// We expect a 4-bpp image only
	assert(lpbi->biBitCount == 4);
	if (lpbi->biBitCount != 4) {
		UnlockResource(hglb);
		FreeResource(hglb);
		return NULL;
	}

	// Get a pointer to the color table
	LPRGBQUAD	pColors = (LPRGBQUAD)((LPSTR)lpbi + (WORD)lpbi->biSize);

	// Look at the first pixel and get the palette index
	UINT	nClrUsed = lpbi->biClrUsed == 0 ? 16 : (UINT)lpbi->biClrUsed;
	LPBYTE	lpBits = (LPBYTE)(pColors + nClrUsed);

	// Munge the color table entry
	COLORREF	clrBtnFace = GetSysColor(COLOR_BTNFACE);
	int			nIndex = *lpBits & 0xF;

	pColors[nIndex].rgbRed = GetRValue(clrBtnFace);
	pColors[nIndex].rgbGreen = GetGValue(clrBtnFace);
	pColors[nIndex].rgbBlue = GetBValue(clrBtnFace);

	// Create the DDB
	HBITMAP		hBitmap;
	HDC			hDC = GetDC(NULL);

	hBitmap = CreateDIBitmap(hDC, lpbi, CBM_INIT, lpBits,
		(LPBITMAPINFO)lpbi, DIB_RGB_COLORS);
	ReleaseDC(NULL, hDC);

	// Release the resource
	UnlockResource(hglb);
	FreeResource(hglb);

	return hBitmap;
}
#endif

// Sizes the button to fit the bitmap
void WINAPI
SizeToFitBitmapButton(HWND hwndCtl, HBITMAP hBitmap)
{
	BITMAP	bm;

	GetObject(hBitmap, sizeof(bm), &bm);
	SetWindowPos(hwndCtl, NULL, 0, 0, bm.bmWidth + 4, bm.bmHeight + 4,
		SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

#define ROP_PSDPxax  0x00B8074AL

// Draws the bitmap in a disabled appearance. This routine assumes the
// bitmap has already been selected into the memory DC
static void NEAR
DisabledBitBlt(HDC hDestDC, RECT &r, HDC hMemDC)
{
	HDC			hMonoDC;
	HBITMAP		hMonoBitmap, hOldBitmap;
	HBRUSH		hOldBrush;
	int			nWidth = r.right - r.left;
	int			nHeight = r.bottom - r.top;
#ifndef _WIN32
	HBRUSH		hShadowBrush = CreateSolidBrush(GetSysColor(COLOR_BTNSHADOW));
#endif

	// Create a monochrome bitmap and HDC
	hMonoBitmap = CreateBitmap(nWidth, nHeight, 1, 1, NULL);
	hMonoDC = CreateCompatibleDC(NULL);
	hOldBitmap = SelectBitmap(hMonoDC, hMonoBitmap);

	// Create a mask where the button face color is 1 and everything else is 0
	SetBkColor(hMemDC, GetSysColor(COLOR_BTNFACE));
	BitBlt(hMonoDC, 0, 0, nWidth, nHeight, hMemDC, 0, 0, SRCCOPY);

	SetTextColor(hDestDC, 0L);                  // 0's in mono -> 0 (for ROP)
	SetBkColor(hDestDC, (COLORREF)0x00FFFFFFL); // 1's in mono -> 1
	
	// Every place the mask has a 0 pixel draw with button highlight color
#ifdef _WIN32
	hOldBrush = SelectBrush(hDestDC, GetSysColorBrush(COLOR_BTNHILIGHT));
#else
	hOldBrush = SelectBrush(hDestDC, GetStockObject(WHITE_BRUSH));
#endif
	BitBlt(hDestDC, r.left + 1, r.top + 1, nWidth - 1, nHeight - 1, hMonoDC,
		0, 0, ROP_PSDPxax);

	// Every place the mask has a 0 pixel draw with button shadow color
#ifdef _WIN32
	SelectBrush(hDestDC, GetSysColorBrush(COLOR_BTNSHADOW));
#else
	SelectBrush(hDestDC, hShadowBrush);
#endif
	BitBlt(hDestDC, r.left, r.top, nWidth, nHeight, hMonoDC,
		0, 0, ROP_PSDPxax);

	// Restore the destination HDC
	SelectBrush(hDestDC, hOldBrush);
#ifndef _WIN32
	DeleteObject(hShadowBrush);
#endif

	// Destroy the GDI objects
	SelectBitmap(hMonoDC, hOldBitmap);
	DeleteBitmap(hMonoBitmap);
	DeleteDC(hMonoDC);
}

void WINAPI
DrawBitmapButton(LPDRAWITEMSTRUCT lpdis, HBITMAP hBitmap, HDC hMemDC)
{
	UINT	uState;
	
	// Draw the button borders
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
	
	// Draw the bitmap
	HBITMAP	hOldBitmap = SelectBitmap(hMemDC, hBitmap);

	if (lpdis->itemState & ODS_DISABLED)
		DisabledBitBlt(lpdis->hDC, lpdis->rcItem, hMemDC);
	else
		BitBlt(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top, lpdis->rcItem.right -
			lpdis->rcItem.left, lpdis->rcItem.bottom - lpdis->rcItem.top, hMemDC,
			0, 0, SRCCOPY);
	
	// Resore the memory DC
	SelectBitmap(hMemDC, hOldBitmap);

	// Draw the focus rect if requested
	if ((lpdis->itemAction & ODA_FOCUS) || (lpdis->itemState & ODS_FOCUS)) {
		lpdis->rcItem.right--;
		lpdis->rcItem.bottom--;
		DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
	}
}
