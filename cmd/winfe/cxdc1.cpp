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

#include "cxdc.h"
#include "cntritem.h"
#include "intlwin.h"
#include "mainfrm.h"
#include "npapi.h"
#include "np.h"
#include "feembed.h"
#include "fmabstra.h"
#include "custom.h"
#include "prefapi.h"
#include "feimage.h"
#include "il_icons.h"
#include "prefinfo.h"

//	Make an array of RGB colors that match the current palette.
//  This array is used for contexts that don't employ DibPalColors.
//  Returns TRUE on success, FALSE otherwise.
static const	RGBQUAD rgbqWhite = {0xFF, 0xFF, 0xFF, 0};
static const	RGBQUAD rgbqBlack = {0, 0, 0, 0};

//#define USE_IDENTITY_PALETTE
#ifdef XP_WIN32
//#define USE_DIB_SECTION
#endif
#ifdef USE_IDENTITY_PALETTE
#    define PALETTEENTRY_FLAGS   PC_NOCOLLAPSE
#else
#    define PALETTEENTRY_FLAGS   0
#endif
extern "C"{
extern int gifAbort;
}

#define ROP_PSDPxax	0x00B8074AL
// Alters the transparent part of the source image (already selected into
// pSrcDC) with the specified brush
//
// Make sure that the WHOLE image is changed and not just the part that
// needs to be displayed; otherwise things will get out of sync

static BOOL
CopyPaletteToRGBArray(HPALETTE pPal, RGBQUAD *pRGBArray) 
{
    PALETTEENTRY *paletteEntries;
    XP_ASSERT(pRGBArray);

    paletteEntries = (PALETTEENTRY *)XP_ALLOC(sizeof(PALETTEENTRY) * 256);
    
    if (!paletteEntries)
        return FALSE;

    // Copy existing palette
    VERIFY(::GetPaletteEntries(pPal, 0, 256, paletteEntries) > 0);
    
    for(int i = 0; i < 256; i++)	{
        pRGBArray[i].rgbRed   = paletteEntries[i].peRed;
        pRGBArray[i].rgbGreen = paletteEntries[i].peGreen;
        pRGBArray[i].rgbBlue  = paletteEntries[i].peBlue;
        pRGBArray[i].rgbReserved = NULL;
    }

    XP_FREE(paletteEntries);
    return TRUE;
}



void CDCCX::ImageComplete(NI_Pixmap* image)
{
	FEBitmapInfo *imageInfo;
	HDC hdc = GetContextDC();
	if (!image || !hdc || IsPrintContext()) return;

	imageInfo = (FEBitmapInfo*) image->client_data;
	if(!imageInfo)
		return;

#ifndef USE_DIB_SECTION
    if(!imageInfo->hBitmap) {
        // Attempt to create a bitmap
		if (!imageInfo->IsMask ) {
			imageInfo->hBitmap = ::CreateDIBitmap(hdc,
                                   &(imageInfo->bmpInfo->bmiHeader),
                                   CBM_INIT, 
                                   image->bits, 
                                   imageInfo->bmpInfo, 
                                   m_bUseDibPalColors ? DIB_PAL_COLORS : DIB_RGB_COLORS);
			if (imageInfo->hBitmap) {
				// Free the image bits since we no longer need them
				CDCCX::HugeFree(image->bits);
				image->bits = NULL;
			}
		}
		else {
			// If there is a mask then create another bitmap
			// Build a BITMAPINFO struct for mask
			char 		 cMask[sizeof(BITMAPINFOHEADER) + (2 * sizeof(RGBQUAD))];
			LPBITMAPINFO pBmInfoMask = (LPBITMAPINFO)cMask;
		
			memcpy(pBmInfoMask, imageInfo->bmpInfo, sizeof(BITMAPINFOHEADER));
			pBmInfoMask->bmiHeader.biBitCount = 1;
			pBmInfoMask->bmiHeader.biCompression = BI_RGB;  // must NOT be BI_BITFIELDS
		
			// Build a color table for monochrome mask. Image lib sets the foreground
			// pixels to 1 and the background pixels to 0. We want the monochrome
			// mask to be the same way
			pBmInfoMask->bmiColors[1] = rgbqBlack;  // background pixels
			pBmInfoMask->bmiColors[0] = rgbqWhite;  // foreground pixels
			// Create the mask. It's important that we use the memory DC, because we
			// want a momochrome bitmap
			imageInfo->hBitmap = ::CreateDIBitmap(m_pImageDC,
				(LPBITMAPINFOHEADER)pBmInfoMask,
				CBM_INIT,
				image->bits,
				pBmInfoMask,
				DIB_RGB_COLORS);

			if (imageInfo->hBitmap) {
				// Free the mask bits since we no longer need them
				CDCCX::HugeFree(image->bits);
				image->bits = NULL;
			}
		}
	}
#else
	HBITMAP oldBmp;
	oldBmp = imageInfo->hBitmap;

	if (!imageInfo->IsMask ) {
		imageInfo->hBitmap = ::CreateDIBitmap(hdc,
                               &(imageInfo->bmpInfo->bmiHeader),
                               CBM_INIT, 
                               image->bits, 
                               imageInfo->bmpInfo, 
                               m_bUseDibPalColors ? DIB_PAL_COLORS : DIB_RGB_COLORS);
	}
	else {
		// If there is a mask then create another bitmap
		// Build a BITMAPINFO struct for mask
		char 		 cMask[sizeof(BITMAPINFOHEADER) + (2 * sizeof(RGBQUAD))];
		LPBITMAPINFO pBmInfoMask = (LPBITMAPINFO)cMask;
	
		memcpy(pBmInfoMask, imageInfo->bmpInfo, sizeof(BITMAPINFOHEADER));
		pBmInfoMask->bmiHeader.biBitCount = 1;
		pBmInfoMask->bmiHeader.biCompression = BI_RGB;  // must NOT be BI_BITFIELDS
	
		// Build a color table for monochrome mask. Image lib sets the foreground
		// pixels to 1 and the background pixels to 0. We want the monochrome
		// mask to be the same way
		pBmInfoMask->bmiColors[1] = rgbqBlack;  // background pixels
		pBmInfoMask->bmiColors[0] = rgbqWhite;  // foreground pixels
		// Create the mask. It's important that we use the memory DC, because we
		// want a momochrome bitmap
		imageInfo->hBitmap = ::CreateDIBitmap(m_pImageDC,
			(LPBITMAPINFOHEADER)pBmInfoMask,
			CBM_INIT,
			image->bits,
			pBmInfoMask,
			DIB_RGB_COLORS);

	}
	::DeleteObject(oldBmp);
	image->bits = NULL;
#endif
	ReleaseContextDC(hdc);
}

static WORD
GetBitCount(BITMAP &bmp)
{
    WORD	nBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel); 

#ifdef XP_WIN32
    if (nBits == 1) 
        nBits = 1; 
    else if (nBits <= 4) 
        nBits = 4; 
    else if (nBits <= 8) 
        nBits = 8; 
    else if (nBits <= 16) 
        nBits = 16; 
    else if (nBits <= 24) 
        nBits = 24; 
    else 
        nBits = 32;
#else
    if (nBits == 1) 
        nBits = 1; 
    else if (nBits <= 4) 
        nBits = 4; 
    else if (nBits <= 8) 
        nBits = 8; 
    else
        nBits = 24; 
#endif

	return nBits;
}

#ifndef XP_WIN32
/* Quaternary raster codes */
#define MAKEROP4(fore,back) (DWORD)((((back) << 8) & 0xFF000000) | (fore))
#endif

BOOL CDCCX::CanWriteBitmapFile(LO_ImageStruct* pLOImage)
{
	ASSERT(pLOImage);
    if(!pLOImage)  {
        return(FALSE);
    }
	IL_Pixmap *pImage = IL_GetImagePixmap(pLOImage->image_req);

    //  Internal icons have no platform data.
	FEBitmapInfo* imageinfo = (FEBitmapInfo*) pImage->client_data;

	// We can't do anything if we don't have all the image bits
	return imageinfo && imageinfo->hBitmap;
}

HANDLE CDCCX::WriteBitmapToMemory(	IL_ImageReq *image_req, LO_Color* bg)
{
	IL_Pixmap *pImage = IL_GetImagePixmap(image_req);
	IL_Pixmap *pMask = IL_GetMaskPixmap(image_req);

	// Windows bitmap file format looks like this:
	//
	// +------------------+
	// | BITMAPFILEHEADER |
	// |------------------|
	// | BITMAPINFOHEADER |
	// |------------------|
	// | color table      |
	// |------------------|
	// | image bits       |
	// +------------------+
	FEBitmapInfo* imageinfo = (FEBitmapInfo*) pImage->client_data;

	FEBitmapInfo* maskinfo = 0;
	if (pMask)
		maskinfo = (FEBitmapInfo*) pMask->client_data;

	// We can't do anything if we don't have all the image bits
	HGLOBAL hDib;
	if (imageinfo && imageinfo->hBitmap) {
		BITMAP				bmp;
		LPBITMAPINFOHEADER	lpBmi;
		WORD				nBitCount;
		LPBYTE				lpBits;
		int					nColorTable;

		// Determine the bits per pixel
		if (!::GetObject(imageinfo->hBitmap, sizeof(bmp), &bmp))
			return FALSE;

		nBitCount = GetBitCount(bmp);

		// We need to know how big the color table is. For 16-bit mode and 32-bit mode, we need to
		// allocate room for 3 double-word color masks
		if (nBitCount == 16 || nBitCount == 32)
			nColorTable = 3;
		else if (nBitCount < 16)
			nColorTable = 1 << nBitCount;
		else {
			ASSERT(nBitCount == 24);
			nColorTable = 0;
		}

		// Allocate space for a BITMAPINFO structure (BITMAPINFOHEADER structure
		// plus space for the color table)
		WORD dwSize = sizeof(BITMAPINFOHEADER) + nColorTable * sizeof(RGBQUAD);
		hDib = GlobalAlloc(GMEM_SHARE, dwSize);
		lpBmi = (LPBITMAPINFOHEADER)GlobalLock(hDib);
		if (!lpBmi)
			return FALSE;

		// Initialize the BITMAPINFOHEADER structure
		lpBmi->biSize = sizeof(BITMAPINFOHEADER);
		lpBmi->biWidth = bmp.bmWidth;
		lpBmi->biHeight = bmp.bmHeight;
		lpBmi->biPlanes = 1;
		lpBmi->biBitCount = nBitCount;

		HDC	hdc = GetContextDC();

		// Note: NT 16-bit video mode won't work unless you use BI_BITFIELDS, and
		// Win 95 probably won't work if you use BI_BITFIELDS. Win 3.x doesn't even
		// support 16-bit DIBs, so it isn't an issue there
	#ifdef XP_WIN32
		if (sysInfo.m_bWinNT && (nBitCount == 16 || nBitCount == 32))
			lpBmi->biCompression = BI_BITFIELDS;
		else
			lpBmi->biCompression = BI_RGB;
	#else
		lpBmi->biCompression = BI_RGB;
	#endif

		// Ask the driver to tell us the number of bits we need to allocate
		::GetDIBits(hdc, imageinfo->hBitmap, 0, (int)lpBmi->biHeight, NULL, (LPBITMAPINFO)lpBmi, DIB_RGB_COLORS);

		if (lpBmi->biSizeImage == 0) {
			// The driver didn't tell us so we need to compute it ourselves
			lpBmi->biSizeImage = ((((lpBmi->biWidth * nBitCount) + 31) & ~31) >> 3) * lpBmi->biHeight;
		}

		GlobalUnlock(hDib);
		hDib = GlobalReAlloc(hDib, lpBmi->biSizeImage + dwSize,0);
		// Allocate space for the bits
 		lpBits = (LPBYTE)GlobalLock(hDib);
		if (!lpBits) {
			free(lpBmi);
			ReleaseContextDC(hdc);
			return FALSE;
		}

		// Windows bitmap files don't allow for a mask. Therefore we have to set the transparent
		// parts of the image to be the appropriate color. This ensures that the bitmap file matches
		// what the user sees on the screen
		//
		// ZZZ: This code should be shared with the code in _StretchBlt()
		COLORREF	bgColor = bg ? RGB(bg->red, bg->green, bg->blue) : m_rgbBackgroundColor;

		if (maskinfo && maskinfo->hBitmap) {
			HBRUSH	hBrush;
			HDC tempDC = ::CreateCompatibleDC(hdc);
			HBITMAP tempBmp = ::CreateCompatibleBitmap(hdc, lpBmi->biWidth, lpBmi->biHeight);
			HBITMAP	hOldBmp = (HBITMAP)::SelectObject(tempDC, tempBmp);
			HBITMAP	hOldBmp1 = (HBITMAP)::SelectObject(m_pImageDC, imageinfo->hBitmap);
		
			if (m_iBitsPerPixel == 16)
				// We don't want a dithered brush
				hBrush = ::CreateSolidBrush(::GetNearestColor(hdc, bgColor));
			else
				hBrush = ::CreateSolidBrush(0x02000000L | bgColor);

			::BitBlt(tempDC, 0, 0,lpBmi->biWidth, lpBmi->biHeight, m_pImageDC, 0, 0, SRCCOPY);

				// Select the mask into the memory DC
			::SelectObject(m_pImageDC, maskinfo->hBitmap);

			// Change the transparent bits of the source bitmap to the desired background
			// color by doing an AND raster op
			//
			// The mask is monochrome and will be converted to color to match the depth of
			// the destination. The foreground pixels in the mask are set to 1, and the
			// background pixels to 0. When converting to a color bitmap, white pixels (1)
			// are set to the background color of the DC, and black pixels (0) are set to
			// the text color of the DC
			//
			// Select the brush
			HBRUSH	hOldBrush = (HBRUSH)::SelectObject(tempDC, hBrush);

			// Draw the brush where the mask is 0
			::BitBlt(tempDC,
						   0,
						   0,
						   lpBmi->biWidth,
						   lpBmi->biHeight,
						   m_pImageDC,
						   0,
						   0,
						   MAKEROP4(SRCPAINT, R2_COPYPEN));

			::SelectObject(tempDC, hOldBrush);

			::DeleteObject(hBrush);
			::GetDIBits(tempDC, tempBmp, 0, (int)lpBmi->biHeight, lpBits + dwSize, (LPBITMAPINFO)lpBmi, DIB_RGB_COLORS);
			::SelectObject(m_pImageDC, hOldBmp1);
			::SelectObject(tempDC, hOldBmp);
			::DeleteObject(tempBmp);
			::DeleteDC(tempDC);
		}
		else
		// This time have the driver give us the color table and the bits
		::GetDIBits(hdc, imageinfo->hBitmap, 0, (int)lpBmi->biHeight, lpBits + dwSize, (LPBITMAPINFO)lpBmi, DIB_RGB_COLORS);
		ReleaseContextDC(hdc);
		GlobalUnlock(hDib);
		return (hDib);

	}
	return (0);
}


BOOL CDCCX::WriteBitmapFile(LPCSTR lpszFileName, LO_ImageStruct* pLOImage)
{
	ASSERT(pLOImage);

    if(!pLOImage)  {
        return(FALSE);
    }
	IL_Pixmap *pImage = IL_GetImagePixmap(pLOImage->image_req);
	IL_Pixmap *pMask = IL_GetMaskPixmap(pLOImage->image_req);

	// Windows bitmap file format looks like this:
	//
	// +------------------+
	// | BITMAPFILEHEADER |
	// |------------------|
	// | BITMAPINFOHEADER |
	// |------------------|
	// | color table      |
	// |------------------|
	// | image bits       |
	// +------------------+
	FEBitmapInfo* imageinfo = (FEBitmapInfo*) pImage->client_data;

	FEBitmapInfo* maskinfo = 0;
	if (pMask)
		maskinfo = (FEBitmapInfo*) pMask->client_data;

	// We can't do anything if we don't have all the image bits
	if (imageinfo && imageinfo->hBitmap) {
		BITMAP				bmp;
		LPBITMAPINFOHEADER	lpBmi;
		WORD				nBitCount;
		LPBYTE				lpBits;
		int					nColorTable;

		// Determine the bits per pixel
		if (!::GetObject(imageinfo->hBitmap, sizeof(bmp), &bmp))
			return FALSE;

		nBitCount = GetBitCount(bmp);

		// We need to know how big the color table is. For 16-bit mode and 32-bit mode, we need to
		// allocate room for 3 double-word color masks
		if (nBitCount == 16 || nBitCount == 32)
			nColorTable = 3;
		else if (nBitCount < 16)
			nColorTable = 1 << nBitCount;
		else {
			ASSERT(nBitCount == 24);
			nColorTable = 0;
		}

		// Allocate space for a BITMAPINFO structure (BITMAPINFOHEADER structure
		// plus space for the color table)
		lpBmi = (LPBITMAPINFOHEADER)calloc(sizeof(BITMAPINFOHEADER) + nColorTable * sizeof(RGBQUAD), 1);

		if (!lpBmi)
			return FALSE;

		// Initialize the BITMAPINFOHEADER structure
		lpBmi->biSize = sizeof(BITMAPINFOHEADER);
		lpBmi->biWidth = bmp.bmWidth;
		lpBmi->biHeight = bmp.bmHeight;
		lpBmi->biPlanes = 1;
		lpBmi->biBitCount = nBitCount;

		HDC	hdc = GetContextDC();

		// Note: NT 16-bit video mode won't work unless you use BI_BITFIELDS, and
		// Win 95 probably won't work if you use BI_BITFIELDS. Win 3.x doesn't even
		// support 16-bit DIBs, so it isn't an issue there
#ifdef XP_WIN32
		if (sysInfo.m_bWinNT && (nBitCount == 16 || nBitCount == 32))
			lpBmi->biCompression = BI_BITFIELDS;
		else
			lpBmi->biCompression = BI_RGB;
#else
		lpBmi->biCompression = BI_RGB;
#endif

		// Ask the driver to tell us the number of bits we need to allocate
		::GetDIBits(hdc, imageinfo->hBitmap, 0, (int)lpBmi->biHeight, NULL, (LPBITMAPINFO)lpBmi, DIB_RGB_COLORS);

		if (lpBmi->biSizeImage == 0) {
			// The driver didn't tell us so we need to compute it ourselves
			lpBmi->biSizeImage = ((((lpBmi->biWidth * nBitCount) + 31) & ~31) >> 3) * lpBmi->biHeight;
		}

		// Allocate space for the bits
        lpBits = (LPBYTE)HugeAlloc(lpBmi->biSizeImage, 1);

		if (!lpBits) {
			free(lpBmi);
			ReleaseContextDC(hdc);
			return FALSE;
		}

		// Windows bitmap files don't allow for a mask. Therefore we have to set the transparent
		// parts of the image to be the appropriate color. This ensures that the bitmap file matches
		// what the user sees on the screen
		//
		// ZZZ: This code should be shared with the code in _StretchBlt()
		LO_Color*	bg = (pLOImage->text_attr && !pLOImage->text_attr->no_background) ? &pLOImage->text_attr->bg : NULL;
		COLORREF	bgColor = bg ? RGB(bg->red, bg->green, bg->blue) : m_rgbBackgroundColor;

        if (maskinfo && maskinfo->hBitmap) {
			HBRUSH	hBrush;
			HDC tempDC = ::CreateCompatibleDC(hdc);
			HBITMAP tempBmp = ::CreateCompatibleBitmap(hdc, lpBmi->biWidth, lpBmi->biHeight);
			HBITMAP	hOldBmp = (HBITMAP)::SelectObject(tempDC, tempBmp);
			HBITMAP	hOldBmp1 = (HBITMAP)::SelectObject(m_pImageDC, imageinfo->hBitmap);
		
			if (m_iBitsPerPixel == 16)
				// We don't want a dithered brush
				hBrush = ::CreateSolidBrush(::GetNearestColor(hdc, bgColor));
			else
				hBrush = ::CreateSolidBrush(0x02000000L | bgColor);
			::BitBlt(tempDC, 0, 0,lpBmi->biWidth, lpBmi->biHeight, m_pImageDC, 0, 0, SRCCOPY);

				// Select the mask into the memory DC
			::SelectObject(m_pImageDC, maskinfo->hBitmap);

			// Change the transparent bits of the source bitmap to the desired background
			// color by doing an AND raster op
			//
			// The mask is monochrome and will be converted to color to match the depth of
			// the destination. The foreground pixels in the mask are set to 1, and the
			// background pixels to 0. When converting to a color bitmap, white pixels (1)
			// are set to the background color of the DC, and black pixels (0) are set to
			// the text color of the DC
			//
			// Select the brush
			HBRUSH	hOldBrush = (HBRUSH)::SelectObject(tempDC, hBrush);

			// Draw the brush where the mask is 0
			::BitBlt(tempDC,
						   0,
						   0,
						   lpBmi->biWidth,
						   lpBmi->biHeight,
						   m_pImageDC,
						   0,
						   0,
						   MAKEROP4(SRCPAINT, R2_COPYPEN));

			::SelectObject(tempDC, hOldBrush);

			::DeleteObject(hBrush);
			::GetDIBits(tempDC, tempBmp, 0, (int)lpBmi->biHeight, lpBits, (LPBITMAPINFO)lpBmi, DIB_RGB_COLORS);
			::SelectObject(m_pImageDC, hOldBmp1);
			::SelectObject(tempDC, hOldBmp);
			::DeleteObject(tempBmp);
			::DeleteDC(tempDC);
		}
		else
		// This time have the driver give us the color table and the bits
		::GetDIBits(hdc, imageinfo->hBitmap, 0, (int)lpBmi->biHeight, lpBits, (LPBITMAPINFO)lpBmi, DIB_RGB_COLORS);
		ReleaseContextDC(hdc);

		BITMAPFILEHEADER  bf;

		// Initialize the BITMAPFILEHEADER struct
		bf.bfType = 0x4D42;  // 'BM'
		bf.bfReserved1 = 0;
		bf.bfReserved2 = 0;

		// Compute the offset to the bits. The bits are after the color table
		bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + nColorTable * sizeof(RGBQUAD);

		// Compute the size of the file
		bf.bfSize = bf.bfOffBits + lpBmi->biSizeImage;

		// Open the file and start writing
		CStdioFile	file;

		TRY {
#ifndef XP_WIN32
			file.Open(lpszFileName, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary);
#else
			file.Open(lpszFileName, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareExclusive);
#endif
			// Write the BITMAPFILEHEADER structure
			file.Write(&bf, sizeof(bf));
	
			// Write the BITMAPINFOHEADER and color table
			file.Write(lpBmi, sizeof(BITMAPINFOHEADER) + nColorTable * sizeof(RGBQUAD));
	
			// Write the bits
#ifdef XP_WIN32
			file.Write(lpBits, lpBmi->biSizeImage);
#else
			file.WriteHuge(lpBits, lpBmi->biSizeImage);
#endif
			file.Close();
		}
		CATCH(CFileException, e) {
			CDCCX::HugeFree(lpBits);
			free(lpBmi);
			return FALSE;
		}
		END_CATCH

		// Free up everything and return success
		CDCCX::HugeFree(lpBits);
		free(lpBmi);
		return TRUE;
	}
	return FALSE;
}


void CDCCX::LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight)	{
	//	Set up the width, height, and margins as defualt values here.
	//	These should have been set up in the Initialize() member of a derived
	//		class.
	if (pContext->grid_children) {
		XP_ListDestroy (pContext->grid_children);
		pContext->grid_children = 0;
	}
	*pWidth = m_lWidth;
	*pHeight = m_lHeight;

	//	No known margins, up to dervied classes to correctly set.
	//	Don't set the margins if non-zero, Layout has it's own ideas about this.

	//	Set ourselves to be at the top of the document.
	m_lOrgX = 0;
	m_lOrgY = 0;

	//	Document has no substance.
	m_lDocWidth = 0;
	m_lDocHeight = 0;


	//	Flush the background color.
	COLORREF rgbColor = prefInfo.m_rgbBackgroundColor;
	FE_SetBackgroundColor(pContext, GetRValue(rgbColor), GetGValue(rgbColor), GetBValue(rgbColor));

	//	We're loading a new page, so flush the previous page's
	//		fonts so that we don't keep building a huge font cache.
	ClearFontCache();

}

#define LIGHT_GRAY	RGB(192, 192, 192)
#define DARK_GRAY	RGB(128, 128, 128)
#define WHITE		RGB(255, 255, 255)
#define BLACK		RGB(0, 0, 0)

static void
GetSystem3DColors(COLORREF rgbBackground, COLORREF& rgbLightColor, COLORREF& rgbDarkColor)
{
#ifdef XP_WIN32
	if (sysInfo.IsWin4_32()) {
		// These are Windows 95 only
		rgbLightColor = ::GetSysColor(COLOR_3DLIGHT);
		rgbDarkColor = ::GetSysColor(COLOR_3DSHADOW);
	} else {
		rgbLightColor = LIGHT_GRAY;
		rgbDarkColor = ::GetSysColor(COLOR_BTNSHADOW);
	}
#else
	rgbLightColor = LIGHT_GRAY;
	rgbDarkColor = ::GetSysColor(COLOR_BTNSHADOW);
#endif

	// We need to make sure that both colors are visible against
	// the background
	if (rgbLightColor == rgbBackground)
		rgbLightColor = rgbBackground == LIGHT_GRAY ? WHITE : LIGHT_GRAY;

	if (rgbDarkColor == rgbBackground)
		rgbDarkColor = rgbBackground == DARK_GRAY ? BLACK : DARK_GRAY;
}

// Constants for calculating Highlight (TS = "TopShadow") and
//   shadow (BS = "BottomShadow") values relative to background
// Taken from UNIX version -- Eric Bina's Visual.c
//
// Bias brightness calculation by standard color-sensitivity values
// (Percents -- UNIX used floats, but we don't need to)
//
#define RED_LUMINOSITY 30
#define GREEN_LUMINOSITY 59
#define BLUE_LUMINOSITY 11

// Percent effect of intensity, light, and luminosity & on brightness,
#define INTENSITY_FACTOR  25
#define LIGHT_FACTOR       0
#define LUMINOSITY_FACTOR 75

// LITE color model percent to interpolate RGB towards black for BS, TS

#define COLOR_LITE_BS_FACTOR   45
#define COLOR_LITE_TS_FACTOR   70

// DARK color model - percent to interpolate RGB towards white for BS, TS

#define COLOR_DARK_BS_FACTOR   30
#define COLOR_DARK_TS_FACTOR   50
#define MAX_COLOR 255

#define COLOR_DARK_THRESHOLD   51
#define COLOR_LIGHT_THRESHOLD  204

void CDCCX::Compute3DColors(COLORREF rgbColor, COLORREF &rgbLight, COLORREF &rgbDark)
{
	unsigned uRed, uGreen, uBlue;
	unsigned uRedBack = GetRValue(rgbColor);
	unsigned uGreenBack = GetGValue(rgbColor);
	unsigned uBlueBack = GetBValue(rgbColor);
	unsigned intensity = (uRedBack + uGreenBack + uBlueBack) / 3;
    unsigned luminosity = ((RED_LUMINOSITY * uRedBack)/ 100)
	                 + ((GREEN_LUMINOSITY * uGreenBack)/ 100)
                     + ((BLUE_LUMINOSITY * uBlueBack)/ 100);
	unsigned backgroundBrightness = ((intensity * INTENSITY_FACTOR) +
				   (luminosity * LUMINOSITY_FACTOR)) / 100;
    unsigned f;
	
    if (backgroundBrightness < COLOR_DARK_THRESHOLD) {
        // Dark Background - interpolate 30% toward black
		uRed =  uRedBack - (COLOR_DARK_BS_FACTOR * uRedBack / 100);
		uGreen = uGreenBack - (COLOR_DARK_BS_FACTOR * uGreenBack / 100);
		uBlue = uBlueBack - (COLOR_DARK_BS_FACTOR * uBlueBack / 100);

        rgbDark = RGB(uRed, uGreen, uBlue);

        // This interpolotes to 50% toward white
		uRed = uRedBack + (COLOR_DARK_TS_FACTOR *
			(MAX_COLOR - uRedBack) / 100);
		uGreen = uGreenBack + (COLOR_DARK_TS_FACTOR *
			(MAX_COLOR - uGreenBack) / 100);

		uBlue = uBlueBack + (COLOR_DARK_TS_FACTOR *
			(MAX_COLOR - uBlueBack) / 100);

	} else if (backgroundBrightness > COLOR_LIGHT_THRESHOLD) {
        // Interpolate 45% toward black
		uRed = uRedBack - (COLOR_LITE_BS_FACTOR * uRedBack / 100);
		uGreen = uGreenBack - (COLOR_LITE_BS_FACTOR * uGreenBack / 100);
		uBlue = uBlueBack - (COLOR_LITE_BS_FACTOR * uBlueBack / 100);
        rgbDark = RGB(uRed, uGreen, uBlue);

	    // Original algorithm (from X source: visual.c) used:
    	// uRed = uRedBack - (COLOR_LITE_TS_FACTOR * uRedBack / 100),
        //   where FACTOR is 20%, but that makes no sense!
        // I think the intention was large interpolation toward white,
        //  so use max of "medium" range (70%) for smooth continuity across threshhold
      uRed = uRedBack + (COLOR_LITE_TS_FACTOR * (MAX_COLOR - uRedBack) / 100);
      uGreen = uGreenBack + (COLOR_LITE_TS_FACTOR * (MAX_COLOR - uGreenBack) / 100);
      uBlue = uBlueBack + (COLOR_LITE_TS_FACTOR * (MAX_COLOR - uBlueBack) / 100);
	
	} else {
        // Medium Background
		f = COLOR_DARK_BS_FACTOR + (backgroundBrightness 
		                * ( COLOR_LITE_BS_FACTOR - COLOR_DARK_BS_FACTOR )
                        / MAX_COLOR);

		uRed = uRedBack - (f * uRedBack / 100);
		uGreen = uGreenBack - (f * uGreenBack / 100);
		uBlue = uBlueBack - (f * uBlueBack / 100);
        rgbDark = RGB(uRed, uGreen, uBlue);

		f = COLOR_DARK_TS_FACTOR + (backgroundBrightness
		               * ( COLOR_LITE_TS_FACTOR - COLOR_DARK_TS_FACTOR )
                       / MAX_COLOR);

        uRed = uRedBack + (f * (MAX_COLOR - uRedBack) / 100);
		uGreen = uGreenBack + (f * (MAX_COLOR - uGreenBack) / 100);
		uBlue = uBlueBack + (f * (MAX_COLOR - uBlueBack) / 100);
    }
    
	// Safety check for upper limit
	uRed = min(MAX_COLOR, uRed);
	uGreen = min(MAX_COLOR, uGreen);
	uBlue = min(MAX_COLOR, uBlue);

    rgbLight = RGB(uRed, uGreen, uBlue);

	// If either of these colors is the same as the background color
	// then use the system 3D element colors instead
	if (rgbLight == m_rgbBackgroundColor || rgbDark == m_rgbBackgroundColor) {
		GetSystem3DColors(m_rgbBackgroundColor, rgbLight, rgbDark);
    }
}

void CDCCX::Set3DColors(COLORREF crBackground)
{
	Compute3DColors(crBackground, m_rgbLightColor, m_rgbDarkColor);
}

void CDCCX::SetBackgroundColor(MWContext *pContext, uint8 uRed, uint8 uGreen, uint8 uBlue)
{
	//	Generate the color reference
    COLORREF crBackground = ResolveBGColor(uRed, uGreen, uBlue);
    
	if( crBackground != m_rgbBackgroundColor ){
        // Recalculate the color only if it has changed
        m_rgbBackgroundColor =  crBackground;
        Set3DColors(m_rgbBackgroundColor);
    }	    
 
	//	Determine transparency.
	ResolveTransparentColor(uRed, uGreen, uBlue);
	
	//	Set the background color in the DC.
	HDC hdc = GetContextDC();
	if (hdc) {
		::SetBkColor(hdc, m_rgbBackgroundColor);
	}
#ifdef DDRAW
	if (GetPrimarySurface()) {
		hdc = GetDispDC();  // set the background color for offscreen surface.
		if (hdc) {
			::SetBkColor(hdc, m_rgbBackgroundColor);
		}
	}
#endif
	ReleaseContextDC(hdc);
	
	//	Have any views (ledges) clear.
	//	Only do the FE_VIEW, since there aren't any other ledges right now.
	FE_ClearView(pContext, FE_VIEW);
}


void CDCCX::SetTransparentColor(BYTE red, BYTE green, BYTE blue)
{
    rgbTransparentColor.red = red;
    rgbTransparentColor.green = green;
    rgbTransparentColor.blue = blue;
}

// Create the initial identity palette that contains just the system colors
// and the  colors.
HPALETTE
CDCCX::InitPalette(HDC hdc) 
{
    int i;
	HPALETTE hPal;
    //	Create an identity palette
    LPLOGPALETTE pLogPal = (LPLOGPALETTE)XP_ALLOC(sizeof(LOGPALETTE)
                                                  + 256 * sizeof(PALETTEENTRY));
    if(pLogPal != NULL)	{
        pLogPal->palVersion = 0x300;
        pLogPal->palNumEntries = 256;
        
        // Get entries 0-9 and 246-255, which are the standard system colors
        GetSystemPaletteEntries(hdc, 0, pLogPal->palNumEntries,
                                pLogPal->palPalEntry);

        // Initialize palette entry flags
        for (i = 10; i < 246; i++) {
            pLogPal->palPalEntry[i].peFlags = PALETTEENTRY_FLAGS;
#ifndef USE_IDENTITY_PALETTE
            // Collapse all unspecified entries to black so that we are
            // friendlier to other applications sharing the palette.
            if(i >= (iLowerColors + MAX_IMAGE_PALETTE_ENTRIES)) {
                pLogPal->palPalEntry[i].peRed   = 0;
                pLogPal->palPalEntry[i].peGreen = 0;
                pLogPal->palPalEntry[i].peBlue  = 0;
            }
#endif
        }

        // Now add in the animation colors
        for(int iLowCnt = iLowerSystemColors, j = 0; iLowCnt <
                iLowerColors; iLowCnt++, j++)	{
            pLogPal->palPalEntry[iLowCnt].peRed   = animationPalette[j].red;
            pLogPal->palPalEntry[iLowCnt].peGreen = animationPalette[j].green;
            pLogPal->palPalEntry[iLowCnt].peBlue  = animationPalette[j].blue;
			pLogPal->palPalEntry[iLowCnt].peFlags = PC_NOCOLLAPSE;
        }
        
        
        if(!(hPal = ::CreatePalette(pLogPal)))	{
            hPal = NULL;
        }

        
        //	Done with this.
        XP_FREE(pLogPal);
    }
	return hPal;
}
HPALETTE CDCCX::CreateColorPalette(HDC hdc, IL_IRGB& transparentColor, int bitsPerPixel)
{
	HPALETTE hPal;
    //	Set up the per-context palette.
    //	This is per-window since not everything goes to the screen, and therefore
    //		won't always have the screen's attributes. (printing, metafiles DCs, etc).
	IL_ColorMap * defaultColorMap =  IL_NewCubeColorMap(NULL, 0,
				   MAX_IMAGE_PALETTE_ENTRIES+1);
	hPal = CDCCX::InitPalette(hdc);
	CDCCX::SetColormap(hdc, defaultColorMap, transparentColor, hPal);
	IL_DestroyColorMap (defaultColorMap);

	return hPal;
}


void CDCCX::SetColormap(HDC hdc, NI_ColorMap *pMap, IL_IRGB& transparentColor, HPALETTE hPal) {

    PALETTEENTRY paletteEntries[256];
    int iExactMatches = 0;      // # of color requests we were able to match
    int iFirstImageColor = iLowerColors;
    int iLastImageColor = iLowerColors + MAX_IMAGE_PALETTE_ENTRIES - 1;
    // Copy existing palette to get system colors and animation colors
    GetPaletteEntries(hPal, 0, 256, paletteEntries);
	if (!pMap->index) {	// setup the index array.
		pMap->index = (unsigned char*)XP_ALLOC(MAX_IMAGE_PALETTE_ENTRIES+1);
		for (int i = 0; i < MAX_IMAGE_PALETTE_ENTRIES; i++) {
			pMap->index[i] = iLowerColors + i;
			paletteEntries[iLowerColors + i].peRed = pMap->map[i].red;
			paletteEntries[iLowerColors + i].peGreen = pMap->map[i].green;
			paletteEntries[iLowerColors + i].peBlue = pMap->map[i].blue;
            paletteEntries[iLowerColors + i].peFlags = PALETTEENTRY_FLAGS;
		}
	}
	paletteEntries[iLowerColors + MAX_IMAGE_PALETTE_ENTRIES].peRed = transparentColor.red;
	paletteEntries[iLowerColors + MAX_IMAGE_PALETTE_ENTRIES].peGreen = transparentColor.green;
	paletteEntries[iLowerColors + MAX_IMAGE_PALETTE_ENTRIES].peBlue = transparentColor.blue;
    paletteEntries[iLowerColors + MAX_IMAGE_PALETTE_ENTRIES].peFlags = PC_NOCOLLAPSE;

	paletteEntries[255].peRed = 0xFF;
	paletteEntries[255].peGreen = 0xFF;
	paletteEntries[255].peBlue = 0xFF;
    paletteEntries[255].peFlags = PALETTEENTRY_FLAGS;
    
    ::SetPaletteEntries(hPal, iFirstImageColor, MAX_IMAGE_PALETTE_ENTRIES,
                            &paletteEntries[iFirstImageColor]);
}

void CDCCX::SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lLength)	{
	//	Set the document height and width.
	m_lDocWidth = lWidth;
	m_lDocHeight = lLength;
}

void CDCCX::GetDocPosition(MWContext *pContext, int iLocation, int32 *lX_p, int32 *lY_p)	{
	*lX_p = m_lOrgX;
	*lY_p = m_lOrgY;
}

BOOL CDCCX::OnOpenDocumentCX(const char *pPathName)	{
	//	Should only happen with window contexts.
	if(IsWindowContext() == FALSE)	{
		return(FALSE);
	}

	//	Make sure there's something specified, otherwise just
	//		return sucess.
	//	Take passing in NULL as a request to initialize, which we do
	//		nothing with.
	if(pPathName != NULL)	{
		//	convert the path name to a URL.
		CString csUrl;
		WFE_ConvertFile2Url(csUrl, pPathName);

		//	Load it.
		GetUrl(NET_CreateURLStruct(csUrl, NET_DONT_RELOAD), FO_CACHE_AND_PRESENT);
	}
	return(TRUE);
}

void CDCCX::ViewImages()	{
	if(IsDestroyed() == FALSE)	{
        // Tell layout that all images are to be force loaded.
        LO_SetForceLoadImage(NULL, TRUE);

		ExplicitlyLoadAllImages();
	}
}

BOOL CDCCX::CanViewImages()	{
	BOOL bRetval;
  	if(prefInfo.m_bAutoLoadImages || IsDestroyed())	{
  		bRetval = FALSE;
  	}
	else	{
		bRetval = TRUE;
	}

	return(bRetval);
}

//	Set up the things that need to be in order to handle an OLE server
//		metafile correctly.
void CDCCX::EnableOleServer()
{
	TRACE("Enabling context as an OLE server\n");

	//	Mark a generic switch that can be checked elsewhere.
	m_bOleServer = TRUE;
}

//	Create a url struct from the history, with appropriate checking
//		for NULL and such, so that we don't have this code scattered
//		throughout the client.
//	bClearStateData is a flag set to erase any data that can't be
//		tossed around without upsetting the client (form data).
//	It's off by default, so be careful out there.
URL_Struct *CDCCX::CreateUrlFromHist(BOOL bClearStateData, SHIST_SavedData *pSavedData, BOOL bWysiwyg)
{
	//	Make sure that we're not destroyed.
	if(IsDestroyed())	{
		return(NULL);
	}

	//	Before we create the URL, we must save any state data of the current page
	//		that is needed to perform the next load (such as position)....
	//	Other's not dealing with history, save their data in the GetUrl call.
    SHIST_SetPositionOfCurrentDoc(&(GetContext()->hist), 0);
    if(GetOriginX() || GetOriginY())    {
#ifdef LAYERS
	    LO_Any *pAny = (LO_Any *)LO_XYToNearestElement(GetDocumentContext(), GetOriginX(), GetOriginY(), NULL);
#else
	    LO_Any *pAny = (LO_Any *)LO_XYToNearestElement(GetDocumentContext(), GetOriginX(), GetOriginY());
#endif  /* LAYERS */
	    if(pAny != NULL)	{
		    TRACE("Remembering document position at element id %ld\n", pAny->ele_id);
		    SHIST_SetPositionOfCurrentDoc(&(GetContext()->hist), pAny->ele_id);
	    }
    }

	//	Call/return the base.
	URL_Struct *pUrl = CStubsCX::CreateUrlFromHist(bClearStateData, pSavedData, bWysiwyg);
	return(pUrl);
}

//
// Make the given form element visible on the screen
//
void CDCCX::DisplayFormElement(MWContext *pContext, int iLocation, LO_FormElementStruct *pFormElement) 
{
    //  Call the base.
    CStubsCX::DisplayFormElement(pContext, iLocation, pFormElement);

    //  Figure the coordinates.
    LTRB Rect;

    // Note that we call this version of ResolveElement since we do
    // something special for form elements. Note also that we don't
    // check the return value since this function may be called before
    // the form element is actually visible, but we have to correctly
    // "display". In other words, this function is only called when the
    // visibility or position of the form element changes and we need
    // make the corresponding changes to the actual widget irrespective
    // of whether it's visible or not.
    ResolveElement(Rect, pFormElement);
    
    SafeSixteen(Rect);
    
    //	Get our front end form element, and have it display itself at the given rectangle.
    CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(GetContext()), pFormElement);
    if(pFormClass != NULL)	{
        pFormClass->DisplayFormElement(Rect);
    }
}

void CDCCX::FormTextIsSubmit(MWContext *pContext, LO_FormElementStruct *pFormElement)  
{
    //  Call the base.
    CStubsCX::FormTextIsSubmit(pContext, pFormElement);

	//	Get our front end form element, and have it do it's thang.
	CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(GetContext()), pFormElement);
	if(pFormClass != NULL)	{
		pFormClass->FormTextIsSubmit();
	}
}

void CDCCX::GetFormElementInfo(MWContext *pContext, LO_FormElementStruct *pFormElement)    
{

    //  Call the base.
    CStubsCX::GetFormElementInfo(pContext, pFormElement);

	//	Get our front end form element, and have it do it's thang.
	CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(GetContext()), pFormElement);
	if(pFormClass != NULL)	{
		pFormClass->GetFormElementInfo();
	}
}


void CDCCX::GetFormElementValue(MWContext *pContext, LO_FormElementStruct *pFormElement, XP_Bool bTurnOff)
{

    //  Call the base.
    CStubsCX::GetFormElementValue(pContext, pFormElement, bTurnOff);
    
	//	Get our front end form element, and have it do it's thang.
	CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(GetContext()), pFormElement);
	if(pFormClass != NULL)	{
		pFormClass->GetFormElementValue(bTurnOff);
	}
}

void CDCCX::ResetFormElement(MWContext *pContext, LO_FormElementStruct *pFormElement)  
{
    //  Call the base.
    CStubsCX::ResetFormElement(pContext, pFormElement);
                              
	//	Get our front end form element, and have it do it's thang.
	CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(GetContext()), pFormElement);
	if(pFormClass != NULL)	{
		pFormClass->ResetFormElement();
	}
} 

void CDCCX::SetFormElementToggle(MWContext *pContext, LO_FormElementStruct *pFormElement, XP_Bool iState)  
{
    //  Call the base.
    CStubsCX::SetFormElementToggle(pContext, pFormElement, iState);

	//	Get our front end form element, and have it do it's thang.
	CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(GetContext()), pFormElement);
	if(pFormClass != NULL)	{
		pFormClass->SetFormElementToggle(iState);
	}
}

//  Do a fill rect type operation.
void CDCCX::FloodRect(LTRB& Rect, HBRUSH hColor)
{
    if(hColor)  {
        HDC hdc = GetContextDC();
        if(hdc) {
            RECT cr;
			::SetRect(&cr, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
            ::FillRect(hdc, &cr, hColor);
            ReleaseContextDC(hdc);
        }
    }
}
BITMAPINFO* CDCCX::FillBitmapInfoHeader(NI_Pixmap* pImage)
{
	BITMAPINFO *pBMInfo;

	NI_PixmapHeader* imageHeader = &pImage->header;
	int pixmap_depth = imageHeader->color_space->pixmap_depth;


	if (pixmap_depth == 24 || pixmap_depth == 32)
        pBMInfo = (BITMAPINFO *)XP_ALLOC(sizeof(BITMAPINFOHEADER)+ (1 * sizeof(RGBQUAD))); // space for header only
	else if (pixmap_depth == 16)
        pBMInfo = (BITMAPINFO *)XP_ALLOC(sizeof(BITMAPINFOHEADER)+ (3 * sizeof(RGBQUAD)) ); // space for header and color masks
    else if (pixmap_depth == 8)
        pBMInfo = (BITMAPINFO *)XP_ALLOC(sizeof(BITMAPINFOHEADER)+ (256 * sizeof(RGBQUAD)) ); // space for header and pallette
    else if (pixmap_depth == 1) 
        pBMInfo = (BITMAPINFO *)XP_ALLOC(sizeof(BITMAPINFOHEADER)+ (2 * sizeof(RGBQUAD)) ); // space for header and pallette
	ASSERT(pixmap_depth < 32);
	if (!pBMInfo) return NULL; // error occor.

    pBMInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pBMInfo->bmiHeader.biWidth = pImage->header.width;
    pBMInfo->bmiHeader.biHeight = pImage->header.height;
    pBMInfo->bmiHeader.biPlanes = 1;
    pBMInfo->bmiHeader.biBitCount = (short)pixmap_depth;

#ifdef XP_WIN32
		//	Don't allow this to happen on OLE servers, as the metafiles can't
		//		handle it.
	if ((pixmap_depth > 1 ) && m_bRGB565) {
		pBMInfo->bmiHeader.biCompression = BI_BITFIELDS;

		// Define the color masks for a RGB565 DIB
	    LPDWORD lpMasks = (LPDWORD)pBMInfo->bmiColors;
		lpMasks[0] = 0xF800;  // red color mask
		lpMasks[1] = 0x07E0;  // green color mask
		lpMasks[2] = 0x001F;  // blue color mask

	} else {
		if (pixmap_depth == 24 || pixmap_depth == 32) {
			LPDWORD lpMasks = (LPDWORD)pBMInfo->bmiColors;
			*lpMasks = NULL;
		}

		pBMInfo->bmiHeader.biCompression = BI_RGB;
	}
#else
	pBMInfo->bmiHeader.biCompression = BI_RGB;
#endif

    //pBMInfo->bmiHeader.biSizeImage = NULL;
	pBMInfo->bmiHeader.biSizeImage = pImage->header.widthBytes * pImage->header.height;
    pBMInfo->bmiHeader.biXPelsPerMeter = 0;
    pBMInfo->bmiHeader.biYPelsPerMeter = 0;

    pBMInfo->bmiHeader.biClrUsed = 0;        // default value    
    pBMInfo->bmiHeader.biClrImportant = 0;   // all important

    if(m_bUseDibPalColors) {
        if (pixmap_depth == 1) {
            // Build an index table for monochrome images. Only XBMs are
			// monochome
            WORD* pPalIndex = (WORD*)(pBMInfo->bmiColors);

            pPalIndex[0] = 255; // palette index for WHITE
            pPalIndex[1] = 0;   // palette index for BLACK

        } else {
			// pImage->depth != 1
            unsigned short nClrUsed = (unsigned short)(1 << pBMInfo->bmiHeader.biBitCount);

			// DIB_PAL_COLORS expects an array of 16-bit unsigned integers that specify
			// an index into the currently realized logical palette
			WORD* pPalIndx = (WORD*)(((unsigned char*)pBMInfo) + pBMInfo->bmiHeader.biSize);
			for (unsigned short index = 0; index < nClrUsed; index++, pPalIndx++)
				*pPalIndx = index;
        }

    } else {
		// Not using DIB_PAL_COLORS
        if (pixmap_depth == 8) {
            RGBQUAD RGBArray[256];
            CopyPaletteToRGBArray(GetPalette(), RGBArray);
            memcpy(&(pBMInfo->bmiColors[0]), RGBArray, (256 * sizeof(RGBQUAD)));



        } else if (pixmap_depth == 1) {
			// Build the color table for monochrome images. Only XBMs are monochrome
            pBMInfo->bmiColors[0].rgbRed      = 255;
            pBMInfo->bmiColors[0].rgbGreen    = 255;
            pBMInfo->bmiColors[0].rgbBlue     = 255;
            pBMInfo->bmiColors[0].rgbReserved = 0;
            pBMInfo->bmiColors[1].rgbRed      = 0;
            pBMInfo->bmiColors[1].rgbGreen    = 0;
            pBMInfo->bmiColors[1].rgbBlue     = 0;
            pBMInfo->bmiColors[1].rgbReserved = 0;
        }
    }
	return pBMInfo;
}

static void replaceColorSpace(NI_Pixmap* pImage, NI_ColorSpace* curColorSpace, int devicePixmap)
{
	NI_PixmapHeader* imageHeader = &pImage->header;  
	NI_ColorSpace* color_space = imageHeader->color_space;  

	ASSERT(color_space->pixmap_depth  < 32);
	if (color_space->pixmap_depth == 1)
		return;
 	if ((color_space->pixmap_depth <= 8) || (devicePixmap < color_space->pixmap_depth)) {
		IL_ReleaseColorSpace(color_space);
		IL_AddRefToColorSpace(curColorSpace);
		imageHeader->color_space = curColorSpace;
	}
}




static BOOL
CanCreateBrush(HBITMAP hBitmap, int nBitsPerPixel)
{
	BITMAP	bitmap;

    // Get the size of the bitmap
	::GetObject(hBitmap, sizeof(bitmap), &bitmap);

	// We can create a brush if the bitmap is exacly 8x8 or we're on NT
    if (sysInfo.m_bWinNT) {
        if (nBitsPerPixel == 24) {
            // There's a reported problem with 24bpp mode on NT, but I haven't
            // seen it on NT 4.0
//            if (sysInfo.m_dwMajor < 4)
                return FALSE;
        }

		// Large brushes on NT are a performance hog, so place an upper
		// limit on the brush size.
        return bitmap.bmWidth <= 64 && bitmap.bmHeight <= 64;
    }

 	return bitmap.bmWidth == 8 && bitmap.bmHeight == 8;
}

HBITMAP CDCCX::CreateBitmap(HDC hTargetDC,  NI_Pixmap *image)
{
	ASSERT(hTargetDC);
	FEBitmapInfo	*imageInfo = (FEBitmapInfo*)image->client_data;
	
	BITMAPINFO *pBmInfo = imageInfo->bmpInfo;

	ASSERT(pBmInfo);

	return ::CreateDIBitmap(hTargetDC, &pBmInfo->bmiHeader, 
				CBM_INIT, image->bits,
				pBmInfo, m_bUseDibPalColors ? DIB_PAL_COLORS : DIB_RGB_COLORS);

}


int CDCCX::DisplayPixmap(NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, LTRB& Rect)
{
	//	If width and height are 0, then we assume the entire width and height.
	//	x and y are relative coords to the top left of the image.
	HDC hdc;
#ifdef DDRAW
	LPDIRECTDRAWSURFACE drawSurface = GetBackSurface(); 
#endif
	hdc = GetDispDC();

	FEBitmapInfo *imageInfo = (FEBitmapInfo*) image->client_data;

	if (!imageInfo) return FALSE;  // there is no image to display.

	// set this flag, so the display boarder will not try to display the icon.
	imageInfo->imageDisplayed = TRUE;
	int	xRepeat, yRepeat;
	xRepeat = (CASTINT(width) + CASTINT(x_offset) + CASTINT(imageInfo->targetWidth) -1) / CASTINT(imageInfo->targetWidth);
	yRepeat = (CASTINT(height) + CASTINT(y_offset) + CASTINT(imageInfo->targetHeight) -1) / CASTINT(imageInfo->targetHeight);
	if (!ResolveElement(Rect, image, x_offset * m_lConvertX, y_offset *m_lConvertY, 
							x * m_lConvertX, y* m_lConvertY, 
							width* m_lConvertX, height* m_lConvertY))
		return FALSE;
	SafeSixteen(Rect);

	FEBitmapInfo *maskInfo = (FEBitmapInfo *)NULL;

	if (mask) maskInfo = (FEBitmapInfo *)mask->client_data;



	x_offset = (x_offset > 0) ? x_offset : 0;
	y_offset = (y_offset > 0) ? y_offset : 0; 

	if ((xRepeat > 1) || (yRepeat > 1)) { // tiled
		TileImage(hdc, Rect, image, mask, x * GetXConvertUnit(), y * GetYConvertUnit());
	}
	else {
		if (maskInfo) { 
			if ((imageInfo->hBitmap) && (maskInfo->hBitmap)) {
#ifdef XP_WIN32
				if (sysInfo.m_bWinNT && (GetContextType() != MetaFile) && !IsPrintContext()) {
					HBITMAP hOldBitmap = (HBITMAP)::SelectObject(m_pImageDC, imageInfo->hBitmap);
					// MaskBlt has two raster ops: one for the foreground pixels (value of 1)
					// and one for the background pixels (value of 0). The raster op is
					// specified using MAKEROP4(fore,back)

#ifdef USE_DIB_SECTION
					if (image->bits)
						::MaskBlt(hdc,
							CASTINT(Rect.left),
							CASTINT(Rect.top),
							CASTINT(Rect.right - Rect.left), 
							CASTINT(Rect.bottom - Rect.top),
							m_pImageDC, x_offset, y_offset,
							maskInfo->hBitmap,
							x_offset, y_offset,
							MAKEROP4(SRCCOPY, 0x00AA0029));  // 0x00AA0029 is destination...

					else 
#endif
						::MaskBlt(hdc,
							CASTINT(Rect.left),
							CASTINT(Rect.top),
							CASTINT(Rect.right - Rect.left), 
							CASTINT(Rect.bottom - Rect.top),
							m_pImageDC, x_offset, y_offset,
							maskInfo->hBitmap,
							x_offset, y_offset,
							MAKEROP4(0x00AA0029, SRCCOPY));  // 0x00AA0029 is destination...

						// Cleanup
					::SelectObject(m_pImageDC, hOldBitmap);
				}
				else
#endif
					StretchMaskBlt(hdc, imageInfo->hBitmap, maskInfo->hBitmap, 
						CASTINT(Rect.left), 	
						CASTINT(Rect.top),
						CASTINT(Rect.right - Rect.left), 
						CASTINT(Rect.bottom - Rect.top),
						x_offset,
						y_offset, 
						width, 
						height);
			}
			else {
				_StretchDIBitsWithMask(hdc,
								CASTINT(Rect.left), 	
								CASTINT(Rect.top),
								CASTINT(Rect.right - Rect.left), 
								CASTINT(Rect.bottom - Rect.top),
								CASTINT(x_offset),
								// the reason for this calculation is that Window's bitmap
								// is reverse.  The first scan line will be at the buttom of
								// the bitmap buffer.
								CASTINT(imageInfo->height - ((height ) + y_offset)),
								CASTINT(width ),
								CASTINT(height),
								image,
								mask);
			}
		}
		else {
			StretchPixmap(hdc, image, 
					CASTINT(Rect.left), 	
					CASTINT(Rect.top),
					CASTINT(Rect.right - Rect.left), 
					CASTINT(Rect.bottom - Rect.top),
					x_offset,
					y_offset, 
					width, 
					height);

		}
	}
	ReleaseContextDC(hdc);


	return 1;
}


 


///////////////////
// Destination coordinates are in the logical units of pDC, and
// the source coordinates are in device units
void CDCCX::StretchPixmap(HDC hTargetDC, NI_Pixmap* pImage, 
								int32 dx, int32 dy, int32 dw, int32 dh,
								int32 sx, int32 sy, int32 sw, int32 sh)
{

	
	// Clamp the width and height if necessary
	if (sx < 0 || sy < 0)
		return;
	FEBitmapInfo	*imageInfo = (FEBitmapInfo*)pImage->client_data;
	
	BITMAPINFO *pBmInfo = imageInfo->bmpInfo;


   if (imageInfo->hBitmap) {
		HBITMAP hOldBitmap;
		
		// If we are using this image as a tile, the bitmap will already be selected
		// into the memory DC for us.
		if (!imageInfo->IsTile)
			hOldBitmap = (HBITMAP)::SelectObject(m_pImageDC, imageInfo->hBitmap);

		::StretchBlt(hTargetDC,
							CASTINT(dx),
							CASTINT(dy),
							CASTINT(dw),
							CASTINT(dh),
							m_pImageDC,
							CASTINT(sx),
							CASTINT(sy),
							CASTINT(sw),
							CASTINT(sh),
     						pBmInfo->bmiHeader.biBitCount == 1 ? SRCAND : SRCCOPY);
		
		// If we are using this image as a tile, the bitmap will be deselected for us.
		if (!imageInfo->IsTile)
			::SelectObject(m_pImageDC, hOldBitmap);
   }
   else {

		// the reason for this calculation is because window's bmp is an
		// upsidedown image.  the bit in the pImage->bits does reflect
		// this.  So we need to sepcify the offset from the bottom up.
		::StretchDIBits( hTargetDC,
							CASTINT(dx),
							CASTINT(dy),
							CASTINT(dw),
							CASTINT(dh),
							CASTINT(sx),
							CASTINT(imageInfo->height - (sh + sy)),
							CASTINT(sw),
							CASTINT(sh),
							pImage->bits,	// address of bitmap bits 
							imageInfo->bmpInfo,	// address of bitmap data 
							CASTUINT(m_bUseDibPalColors ? DIB_PAL_COLORS : DIB_RGB_COLORS),
     						pBmInfo->bmiHeader.biBitCount == 1 ? SRCAND : SRCCOPY);

   }
		
}

HBITMAP CDCCX::CreateMask(HDC hTargetDC, NI_Pixmap* mask) 
{

	ASSERT(hTargetDC);
	FEBitmapInfo	*maskInfo = (FEBitmapInfo*)mask->client_data;

	// Build a BITMAPINFO struct for mask
	char 		 cMask[sizeof(BITMAPINFOHEADER) + (2 * sizeof(RGBQUAD))];
	LPBITMAPINFO pBmInfoMask = (LPBITMAPINFO)cMask;
		
	memcpy(pBmInfoMask, maskInfo->bmpInfo, sizeof(BITMAPINFOHEADER));
	pBmInfoMask->bmiHeader.biBitCount = 1;
	pBmInfoMask->bmiHeader.biCompression = BI_RGB;  // must NOT be BI_BITFIELDS

	pBmInfoMask->bmiColors[1] = rgbqBlack;  // background pixels
	pBmInfoMask->bmiColors[0] = rgbqWhite;  // foreground pixels
	// Create the mask. It's important that we use the memory DC, because we
	// want a momochrome bitmap
	return  ::CreateDIBitmap(hTargetDC,
				(LPBITMAPINFOHEADER)pBmInfoMask,
				CBM_INIT,
				mask->bits,
				pBmInfoMask,
//				m_bUseDibPalColors ? DIB_PAL_COLORS : DIB_RGB_COLORS);
				DIB_RGB_COLORS);
}

// StretchBlt a DIB with a mask
// Destination coordinates are in the logical units of pDC, and
// the source coordinates are in device units
void WFE_StretchDIBitsWithMask(HDC hTargetDC,
							BOOL isDeviceDC,
							HDC hOffscreenDC,
							int dx,
							int dy,
							int dw,
							int dh,
							int sx,
							int sy,
							int sw,
							int sh,
							void XP_HUGE *imageBit,
							BITMAPINFO* lpImageInfo,
							void XP_HUGE *maskBit,
							BOOL bUseDibPalColors,
							COLORREF fillWithBackground
							)
{
		// Tile the shadow bitmap with the backdrop
	HBITMAP pSaveBmp;
	HBITMAP bmpShadow;
	HDC hTempDC;
	int32 targetX, targetY;
	HDC memDC;


	if (isDeviceDC) {

		if (hOffscreenDC)
			memDC = hOffscreenDC;
		else {
			memDC = ::CreateCompatibleDC(hTargetDC);
		}
		if (!(bmpShadow = (HBITMAP)CreateCompatibleBitmap(hTargetDC, dw, dh))) {
			TRACE("_StretchBltWithMask() can't create bitmap!\n");
			return;
		}
		pSaveBmp = (HBITMAP) ::SelectObject(memDC, bmpShadow);
		if (!fillWithBackground) {
			::StretchBlt(memDC,	   // get the screen bit.
				CASTINT(0), CASTINT(0), 
    			CASTINT(dw), CASTINT(dh), 
				hTargetDC, 
				CASTINT(dx), CASTINT(dy), 
				CASTINT(dw), CASTINT(dh), 
				SRCCOPY);
		} 
		else {  // the background is solid color.
			HBRUSH hbr = ::CreateSolidBrush(fillWithBackground);
			RECT tempRect;
			tempRect.left = tempRect.top = 0;
			tempRect.right = dw;
			tempRect.bottom = dh;
			::FillRect(memDC, &tempRect, hbr);
			::DeleteObject(hbr);
		}
		hTempDC = memDC;
		targetX = targetY = 0;
	}
	else  {
		hTempDC =  hTargetDC;
		targetX = dx;
		targetY = dy;
	}

	char 		 cMask[sizeof(BITMAPINFOHEADER) + (2 * sizeof(RGBQUAD))];
	LPBITMAPINFO pBmInfoMask = (LPBITMAPINFO)cMask;

	memcpy(pBmInfoMask, lpImageInfo, sizeof(BITMAPINFOHEADER));
	pBmInfoMask->bmiHeader.biBitCount = 1;
	pBmInfoMask->bmiHeader.biCompression = BI_RGB;  // must NOT be BI_BITFIELDS

	// Build a color table for monochrome mask. The image library sets foreground
	// pixels to 1 and background pixels to 0. We want it the other way around
	// when doing the AND operation
	pBmInfoMask->bmiColors[0] = rgbqWhite;  // background pixels
	pBmInfoMask->bmiColors[1] = rgbqBlack;  // foreground pixels
	pBmInfoMask->bmiHeader.biSizeImage =  0;
	int err = ::StretchDIBits(hTempDC,
				CASTINT(targetX), CASTINT(targetY), 
				CASTINT(dw), CASTINT(dh), 
				CASTINT(sx), CASTINT(sy), 
				CASTINT(sw), CASTINT(sh),
				maskBit,
				pBmInfoMask,
				DIB_RGB_COLORS,
				SRCAND);
	err = ::StretchDIBits(hTempDC,
				CASTINT(targetX), CASTINT(targetY), 
    			CASTINT(dw), CASTINT(dh), 
				CASTINT(sx), CASTINT(sy), 
				CASTINT(sw), CASTINT(sh), 
				imageBit,
				lpImageInfo,
				bUseDibPalColors ? DIB_PAL_COLORS : DIB_RGB_COLORS,
				SRCPAINT);
	if (isDeviceDC) {

		::StretchBlt(hTargetDC,
				 CASTINT(dx), CASTINT(dy),
    			 CASTINT(dw), CASTINT(dh), 
    			 memDC,
				 CASTINT(0), CASTINT(0), 
    			 CASTINT(dw),CASTINT(dh), 
    			 SRCCOPY);

		::SelectObject(memDC, pSaveBmp);
		VERIFY(::DeleteObject( bmpShadow));
		if (!hOffscreenDC) {	// Delete the temporary DC that we created.
			::DeleteDC(memDC);
		}
	}
}

       
// StretchBlt a DIB with a mask
// Destination coordinates are in the logical units of pDC, and
// the source coordinates are in device units
void CDCCX::_StretchDIBitsWithMask(HDC hTargetDC,
							int dx,
							int dy,
							int dw,
							int dh,
							int sx,
							int sy,
							int sw,
							int sh,
							NI_Pixmap *image,
							NI_Pixmap *mask)
{
		// Tile the shadow bitmap with the backdrop
	FEBitmapInfo	*imageInfo = (FEBitmapInfo*)image->client_data;
	FEBitmapInfo	*maskInfo = (FEBitmapInfo*)mask->client_data;
	WFE_StretchDIBitsWithMask(hTargetDC, IsDeviceDC(), m_pImageDC, 
								dx, dy, dw, dh,
								sx, sy, sw, sh, 
								image->bits,
								imageInfo->bmpInfo,
								mask->bits,
								m_bUseDibPalColors);
}

       
void CDCCX::StretchMaskBlt(HDC hTargetDC, HBITMAP theBitmap, HBITMAP theMask, 
								int32 dx, int32 dy, int32 dw, int32 dh,
								int32 sx, int32 sy, int32 sw, int32 sh)
{
		// Tile the shadow bitmap with the backdrop
	HDC		memDC = NULL;
	HBITMAP pSaveBmp;
	HBITMAP bmpShadow;
	HPALETTE hOldPal1 = NULL;
	int32 targetX, targetY;

	if (IsDeviceDC()) {


		if (!(bmpShadow = (HBITMAP)CreateCompatibleBitmap(hTargetDC, CASTINT(dw), CASTINT(dh)))) {
			TRACE("_StretchBltWithMask() can't create bitmap!\n");
			return;
		}
		// Create memory DC
		if (!(memDC=CreateCompatibleDC(hTargetDC))) {
			TRACE("_StretchBltWithMask() can't create compatible memory DC!\n");
			return;
		}
		if (m_bUseDibPalColors && m_pPal)	{
			hOldPal1 = (HPALETTE)::SelectPalette(memDC, m_pPal, FALSE);
		}

	pSaveBmp = (HBITMAP) ::SelectObject(memDC, bmpShadow);
	::StretchBlt(memDC,
				CASTINT(0), CASTINT(0), 
    			CASTINT(dw), CASTINT(dh), 
				hTargetDC, 
				CASTINT(dx), CASTINT(dy), 
				CASTINT(dw), CASTINT(dh), 
				SRCCOPY);
		targetX = targetY = 0;
	}
	else {
		memDC = hTargetDC;
		targetX = dx;
		targetY = dy;
	}
	HBITMAP old = (HBITMAP)::SelectObject(m_pImageDC, theMask);
	::StretchBlt(memDC,
				CASTINT(targetX), CASTINT(targetY), 
				CASTINT(dw), CASTINT(dh), 
				m_pImageDC, 
				CASTINT(sx), CASTINT(sy), 
				CASTINT(sw), CASTINT(sh), 
				SRCAND);

	// load the bitmap into the cached image CDC                                                   
	::SelectObject(m_pImageDC, theBitmap);

	::StretchBlt(memDC,
				CASTINT(targetX), CASTINT(targetY), 
    			CASTINT(dw), CASTINT(dh), 
				m_pImageDC, 
				CASTINT(sx), CASTINT(sy), 
				CASTINT(sw), CASTINT(sh), 
				SRCPAINT);

	if (IsDeviceDC()) {
		::StretchBlt(hTargetDC,
				 CASTINT(dx), CASTINT(dy),
    			 CASTINT(dw), CASTINT(dh), 
    			 memDC,
				 CASTINT(0), CASTINT(0), 
    			 CASTINT(dw),CASTINT(dh), 
    			 SRCCOPY);
		::SelectObject(memDC, pSaveBmp);
		if (m_bUseDibPalColors && hOldPal1)	{
			::SelectPalette(memDC, hOldPal1, FALSE);
		}
		VERIFY(::DeleteDC(memDC));
		VERIFY(::DeleteObject( bmpShadow));
	}
	::SelectObject(m_pImageDC, old);

}
        
// We need to allocate the space for the image decoding ourselves because it 
// depends on what size machine we are on how we want to do it
BITMAPINFO *CDCCX::NewPixmap(NI_Pixmap *pImage, BOOL mask)
{
	if(mask == FALSE) {  
		replaceColorSpace(
            pImage,
            /*this->*/curColorSpace,
            /*this->*/m_iBitsPerPixel);
	}

	NI_PixmapHeader& header = pImage->header;
	NI_ColorSpace *& color_space = header.color_space;  
	uint8& pixmap_depth = color_space->pixmap_depth;

	if(pImage->bits == NULL) {
		header.widthBytes = header.width;
        header.widthBytes *= pixmap_depth;
		if(pixmap_depth < 8) {
            header.widthBytes += 7;
		}
        header.widthBytes /= 8;

		// Make sure image width is 4byte aligned
        int iAlign = CASTINT(header.widthBytes  % 4);
		if(iAlign)	{
			header.widthBytes += 4;
            header.widthBytes -= iAlign;
		}
//#ifndef XP_WIN32
#ifndef USE_DIB_SECTION
		// The image buffer might need to be really big (i.e. over 32K)
		// We need to use a huge memory allocation on the 16-bit version        
		pImage->bits = HugeAlloc(header.widthBytes * header.height, 1);
		// Note: It's possible that the image is so huge that we can't allocate enough
		// memory for it, especially under Win16 (e.g. 18 Mb images)
		if(pImage->bits == NULL) {
			return(NULL);
		}
		return(FillBitmapInfoHeader(pImage));
#else  // for use with createDibSection.
			BITMAPINFO* pbmInfo = FillBitmapInfoHeader(pImage);
			FEBitmapInfo *imageInfo;
			imageInfo = (FEBitmapInfo*) pImage->client_data;
			HDC hdc = GetContextDC();
			imageInfo->hBitmap = CreateDIBSection(hdc,
				pbmInfo,	// pointer to structure containing bitmap size, format, and color data
				m_bUseDibPalColors ? DIB_PAL_COLORS : DIB_RGB_COLORS,
				&pImage->bits,	// pointer to variable to receive a pointer to the bitmap's bit values
				NULL,	// optional handle to a file mapping object
				NULL	// offset to the bitmap bit values within the file mapping object
				);
			ReleaseContextDC(hdc);
#ifdef DEBUG
            // If this assert fires, please be sure to report the following
            // information: pixmap_depth, header.widthBytes, header.height
            // and the current display setting - kevina.
            XP_ASSERT(pImage->bits);

            if (pImage->bits) {
            // Check whether the pointer is currently valid.
            XP_ASSERT(!IsBadWritePtr((void *)pImage->bits,
                                     (uint)(header.widthBytes*header.height)));
            XP_ASSERT(!IsBadReadPtr((void *)pImage->bits,
                                    (uint)(header.widthBytes*header.height)));

            // For testing.  Purify sometimes gets confused and complains that
            // the shared memory pointer returned by CreateDIBSection is
            // invalid, even though the previous assertions don't fire.  If
            // this happens, expect Purify to complain whenever the ImageLib
            // accesses the bits pointer.
//          memset(pImage->bits, ~0, (header.widthBytes * header.height));
            memset(pImage->bits, ~0, 1);
            }
#endif // DEBUG

			if (!imageInfo->hBitmap) {
				return FALSE;
			}
			else
				return pbmInfo;
#endif
        
	}  

	return NULL;
}



// Create a larger tile from a smaller one.  The dimensions of the larger
// tile must be a multiple of the dimensions of the smaller tile.
static HBITMAP CreateLargerTile(HDC pDC, HDC pMemDCNew, HDC pMemDCOld,
								NI_Pixmap *pImage, BOOL bUseDibPalColors,
								int iOldWidth, int iOldHeight,
								int iNewWidth, int iNewHeight)
{
	int x, y;
	HBITMAP pNewTile;
	HGDIOBJ pSavedObj1, pSavedObj2;
	FEBitmapInfo *imageInfo = (FEBitmapInfo *)pImage->client_data;

	pNewTile = CreateCompatibleBitmap(pDC, iNewWidth, iNewHeight);
	if (!pNewTile)
		return NULL;
	pSavedObj1 = (HGDIOBJ)::SelectObject(pMemDCNew, pNewTile);
	if (imageInfo->hBitmap) {
		pSavedObj2 = (HGDIOBJ)::SelectObject(pMemDCOld, imageInfo->hBitmap);
		::BitBlt(pMemDCNew, 0, 0, iOldWidth, iOldHeight, pMemDCOld, 0, 0, SRCCOPY);
	}
	else {
		uint iUsage = bUseDibPalColors ? DIB_PAL_COLORS : DIB_RGB_COLORS;

		::StretchDIBits(pMemDCNew, 0, 0, iOldWidth, iOldHeight, 0, 0, iOldWidth, iOldHeight,
						pImage->bits, imageInfo->bmpInfo, iUsage, SRCCOPY);
	}
	// Fill up the new tile exponentially for speed.
	for	(x = iOldWidth; x < iNewWidth; x *= 2) {
		::BitBlt(pMemDCNew, x, 0, MIN(x,iNewWidth-x), iOldHeight, pMemDCNew, 0, 0, SRCCOPY);
	}
	for	(y = iOldHeight; y < iNewHeight; y *= 2) {
		::BitBlt(pMemDCNew, 0, y, iNewWidth, MIN(y,iNewHeight-y), pMemDCNew, 0, 0, SRCCOPY);
	}
	::SelectObject(pMemDCNew, pSavedObj1);
	if (imageInfo->hBitmap)
		::SelectObject(pMemDCOld, pSavedObj2);

	return pNewTile;
}
#ifdef DDRAW
/*
 * DDColorMatch
 *
 * convert a RGB color to a pysical color.
 *
 * we do this by leting GDI SetPixel() do the color matching
 * then we lock the memory and see what it got mapped to.
 */
static DWORD DDColorMatch(IDirectDrawSurface *pdds, COLORREF rgb)
{
    COLORREF rgbT;
    HDC hdc;
    DWORD dw = CLR_INVALID;
    DDSURFACEDESC ddsd;
    HRESULT hres;

    //
    //  use GDI SetPixel to color match for us
    //
    if (rgb != CLR_INVALID && pdds->GetDC(&hdc) == DD_OK)
    {
        rgbT = GetPixel(hdc, 0, 0);             // save current pixel value
        SetPixel(hdc, 0, 0, rgb);               // set our value
        pdds->ReleaseDC(hdc);
    }

    //
    // now lock the surface so we can read back the converted color
    //
    ddsd.dwSize = sizeof(ddsd);
    while ((hres = pdds->Lock(NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING)
        ;

    if (hres == DD_OK)
    {
        dw  = *(DWORD *)ddsd.lpSurface;                     // get DWORD
        dw &= (1 << ddsd.ddpfPixelFormat.dwRGBBitCount)-1;  // mask it to bpp
        pdds->Unlock(NULL);
    }

    //
    //  now put the color that was there back.
    //
    if (rgb != CLR_INVALID && pdds->GetDC(&hdc) == DD_OK)
    {
        SetPixel(hdc, 0, 0, rgbT);
        pdds->ReleaseDC(hdc);
    }

    return dw;
}
// MWH this funtion only work when there is directDraw support.  It use the transColor for 
// transparent blt.	 if releaseTempSurf == FALSE.  It is caller's responsibility to release
// the surf.
BOOL CDCCX::TransparentBlt(int dx,
							int dy,
							int dw,
							int dh,
							int sx,
							int sy,
							int sw,
							int sh,
							NI_Pixmap *image,
							COLORREF transColor,
							LPDIRECTDRAWSURFACE surf)
{
	if (!GetPrimarySurface()) return FALSE;
	LPDDSURFACEDESC	ddesc = GetSurfDesc();

	RECT rect;
	HRESULT err = 1;
	rect.left = rect.top = 0;
	rect.bottom = dy + dh;
	rect.right = dx + dw;
	BOOL needRelease = TRUE;
	LPDIRECTDRAWSURFACE tempsurf;
	if (!surf) {
		tempsurf = CreateOffscreenSurface(rect);
	}
	else {
		tempsurf = surf;
		needRelease = FALSE;
	}
	HDC tempDC;
	FEBitmapInfo *imageInfo;
	imageInfo = (FEBitmapInfo*) image->client_data;
	if (tempsurf) {
		tempsurf->GetDC(&tempDC);
		::StretchDIBits(tempDC,
					CASTINT(dx), CASTINT(dy), 
    				CASTINT(dw), CASTINT(dh), 
					CASTINT(sx), CASTINT(sy), 
					CASTINT(sw), CASTINT(sh), 
					image->bits,
					imageInfo->bmpInfo,
					DIB_RGB_COLORS,
					SRCCOPY);
		ReleaseOffscreenSurfDC();

		rect.left = dx;
		rect.top = dy;
		tempsurf->ReleaseDC(tempDC);
		DDCOLORKEY  colorKey;
		if (ddesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) { //palette device
			colorKey.dwColorSpaceLowValue = ::GetNearestPaletteIndex(GetPalette(), transColor);
			colorKey.dwColorSpaceHighValue = ::GetNearestPaletteIndex(GetPalette(), transColor);
		}
		else {
			colorKey.dwColorSpaceLowValue = DDColorMatch(tempsurf, transColor);
			colorKey.dwColorSpaceHighValue = DDColorMatch(tempsurf, transColor);
		}
		tempsurf->SetColorKey(DDCKEY_SRCBLT, &colorKey);
		err = GetBackSurface()->Blt(&rect, tempsurf, &rect, DDBLT_KEYSRC , NULL);
		if (err == DDERR_SURFACELOST) {
			RestoreAllDrawSurface();
			err = GetBackSurface()->Blt(&rect, tempsurf, &rect, DDBLT_KEYSRC , NULL);
		}
		if (needRelease) {
			tempsurf->Release();
		}
		LockOffscreenSurfDC();
	}
	if (err != 0) return FALSE;
	else
		return TRUE;
}
#endif

#define ROUNDUP(_x,_toMultipleOf) (((_x)+(_toMultipleOf)-1)/(_toMultipleOf)*(_toMultipleOf))

void	CDCCX::TileImage(HDC hdc, LTRB& Rect, NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y)
{
	int iMinTileWidth, iMinTileHeight, iTileWidth, iTileHeight;
	int iRectWidth, iRectHeight, iOrigWidth, iOrigHeight;
	HBITMAP pTmpImageBitmap = NULL, pTmpMaskBitmap = NULL, pSavedImageBitmap, pSavedMaskBitmap;
	HGDIOBJ pOldObj;
	FEBitmapInfo *imageInfo;
	FEBitmapInfo *maskInfo;
	imageInfo = (FEBitmapInfo*) image->client_data;
	int32 lDrawingOrgX, lDrawingOrgY;
	GetDrawingOrigin(&lDrawingOrgX, &lDrawingOrgY);

	// If we a backdrop image has been explicitly specified and
	// it is transparent, then don't use the NT brush feature,
	// since it doesn't support masks.
	if ((!mask) && (imageInfo->hBitmap) && (CanCreateBrush(imageInfo->hBitmap, m_iBitsPerPixel))) {
		CPoint	brushOrg(CASTINT(lDrawingOrgX - m_lOrgX), CASTINT(lDrawingOrgY - m_lOrgY));
		HBRUSH	hBrush = CreatePatternBrush(imageInfo->hBitmap);
		ASSERT(hBrush);

		// We need to align the brush properly. The values to SetBrushOrg() specifiy
		// where the brush origin (i.e. the point (0, 0) of the brush) will be mapped.
		// The coordinates must be in device units
		::LPtoDP(hdc, &brushOrg, 1);
		brushOrg.x %= (imageInfo->width * GetXConvertUnit());
		brushOrg.y %= (imageInfo->height * GetYConvertUnit());
		POINT tempPoint;
#ifdef _WIN32
		::SetBrushOrgEx(hdc,brushOrg.x, brushOrg.y, &tempPoint);
#else
		::SetBrushOrg( hdc, brushOrg.x, brushOrg.y );
#endif
		// Erase the background
		::FillRect(hdc, (RECT*)&Rect, hBrush);
		VERIFY(::DeleteObject(hBrush));

#ifdef _WIN32
		// Restore the brush origin
		::SetBrushOrgEx(hdc, tempPoint.x, tempPoint.y, NULL);
#endif
		ReleaseContextDC(hdc);
		return;
	}

	if (mask) {
		maskInfo = (FEBitmapInfo*) mask->client_data;
	}

	// If the width (or height) of the image is less than the smaller of
	// 32 and half the width (or height) of the area to be tiled, then tile the
	// image into a larger temporary bitmap and use the temporary bitmap
	// as the tile.	 We also limit the actual area of the temporary tile to 4096.
	iOrigWidth = imageInfo->width * GetXConvertUnit();
	iOrigHeight = imageInfo->height *GetYConvertUnit();
	iRectWidth = Rect.right - Rect.left;
	iRectHeight = Rect.bottom - Rect.top;
	iMinTileWidth = MIN(32,MAX(iRectWidth/2,1));		// Tile must be at least this wide.
	iMinTileHeight = MIN(32,MAX(iRectHeight/2,1));		// Tile must be at least this high.
	iTileWidth = ROUNDUP(iMinTileWidth, iOrigWidth);    // Actual width of temporary tile.
	iTileHeight = ROUNDUP(iMinTileHeight, iOrigHeight);	// Actual height of temporary tile.

	if ((iOrigWidth < iMinTileWidth || iOrigHeight < iMinTileHeight) &&
		(iTileWidth * iTileHeight < 4096)) {
		HDC pTmpDC = NULL;

		// Create a temporary larger tile and use it instead of the original tile.
		HDC hhDC = GetContextDC();
		pTmpDC = CreateCompatibleDC(hhDC);
		ReleaseContextDC(hhDC);
		if (!pTmpDC)
			return;
		HPALETTE  hOldPal;
		if (m_bUseDibPalColors)
			hOldPal = ::SelectPalette(pTmpDC, GetPalette(), FALSE);
		pTmpImageBitmap = CreateLargerTile(hhDC, pTmpDC, m_pImageDC,
										   image, m_bUseDibPalColors,
										   iOrigWidth, iOrigHeight,
										   iTileWidth, iTileHeight);
		if (!pTmpImageBitmap) { // OOM
			if (m_bUseDibPalColors)
				::SelectPalette(pTmpDC, hOldPal, FALSE);
			VERIFY(::DeleteDC(pTmpDC));
			return;
		}
		if (mask) {
			pTmpMaskBitmap = CreateLargerTile(hhDC, pTmpDC, m_pImageDC,
											  mask, m_bUseDibPalColors,
											  iOrigWidth, iOrigHeight,
											  iTileWidth, iTileHeight);
			if (!pTmpMaskBitmap) { // OOM
				if (m_bUseDibPalColors)
					::SelectPalette(pTmpDC, hOldPal, FALSE);
				VERIFY(::DeleteDC(pTmpDC));
				VERIFY(::DeleteObject(pTmpImageBitmap));
				return;
			}
		}
		pSavedImageBitmap = imageInfo->hBitmap;
		imageInfo->hBitmap = pTmpImageBitmap;
		imageInfo->width = iTileWidth;
		imageInfo->height = iTileHeight;
		if (mask) {
			pSavedMaskBitmap = maskInfo->hBitmap;
			maskInfo->hBitmap = pTmpMaskBitmap;
			maskInfo->width = iTileWidth;
			maskInfo->height = iTileHeight;
		}

		if (m_bUseDibPalColors)
			::SelectPalette(pTmpDC, hOldPal, FALSE);
		VERIFY(::DeleteDC(pTmpDC));
	}

	// Set a flag to indicate that we are in the tiling code.
	imageInfo->IsTile = TRUE;
	if (mask)
		maskInfo->IsTile = TRUE;

	// If we are going to call StretchPixmap, avoid selecting the image bitmap
	// into the memory DC each time.
	if (!mask && imageInfo->hBitmap)
		pOldObj = ::SelectObject(m_pImageDC, imageInfo->hBitmap);
	// We need to properly align the backdrop vertically (in logical units)
	int	srcY = CASTINT((((Rect.top - GetTopMargin())/ GetYConvertUnit()) - lDrawingOrgY + m_lOrgY ) % (imageInfo->height));
	int dstHeight = CASTINT((imageInfo->height* GetYConvertUnit()) - (srcY  * GetYConvertUnit()));

	// MWH -- this loop needs a rewrite.  I just do this for first pass on win16 compiler.
	// Should not check for the which Blt we want to use inside the loop.
	// Redraw the backdrop from left to right and top to bottom
	for (int currentY = CASTINT(Rect.top); currentY < CASTINT(Rect.bottom);) {
		// We need to properly align the backdrop horizontally (in logical units)
		int	srcX = CASTINT((((Rect.left - GetLeftMargin()) / GetXConvertUnit()) - lDrawingOrgX + m_lOrgX)% (imageInfo->width));
		int	dstWidth = CASTINT((imageInfo->width * GetXConvertUnit()) - srcX * GetXConvertUnit());

		// Only draw as far down as we've been asked to draw
		if (CASTINT(currentY + dstHeight) > CASTINT(Rect.bottom))
			dstHeight = CASTINT(Rect.bottom - currentY);

	
		// Loop across the row
		for (int currentX = CASTINT(Rect.left); currentX < CASTINT(Rect.right);) {
			// Only draw as far across as we've been asked to draw
			if (currentX + dstWidth > Rect.right)
				dstWidth = CASTINT(Rect.right - currentX);

			// m_pImageDC is in device space, so we need to convert coordinates
			RECT	srcRect;
			::SetRect(&srcRect, srcX, srcY, srcX + (dstWidth/ GetXConvertUnit()), srcY + (dstHeight/ GetYConvertUnit()));
			if (mask) {
				if (imageInfo->hBitmap && maskInfo->hBitmap) {
#ifdef _WIN32
					if (sysInfo.m_bWinNT && (GetContextType() != MetaFile)) {
						HBITMAP hOldBitmap = (HBITMAP)::SelectObject(m_pImageDC, imageInfo->hBitmap);
#ifdef USE_DIB_SECTION
						if (image->bits)
							MaskBlt(hdc, 
								currentX, currentY, 
								dstWidth, 
								dstHeight,
								m_pImageDC, srcRect.left, srcRect.top,
								maskInfo->hBitmap,
								srcRect.left, srcRect.top,
								MAKEROP4(SRCCOPY, 0x00AA0029));  // 0x00AA0029 is destination...
						else
#endif
							MaskBlt(hdc, 
								currentX, currentY, 
								dstWidth, 
								dstHeight,
								m_pImageDC, srcRect.left, srcRect.top,
								maskInfo->hBitmap,
								srcRect.left, srcRect.top,
								MAKEROP4(0x00AA0029, SRCCOPY));  // 0x00AA0029 is destination...
						::SelectObject(m_pImageDC, hOldBitmap);
					}
					else
#endif
						StretchMaskBlt(hdc, 
								imageInfo->hBitmap, maskInfo->hBitmap,
								currentX, currentY, 
								dstWidth, 
								dstHeight,
								srcRect.left, srcRect.top, 
								srcRect.right - srcRect.left, 
								srcRect.bottom - srcRect.top); 
				}
				else {	// blt bitmap with mask.
					_StretchDIBitsWithMask(hdc,
									currentX, 	currentY,
									dstWidth, 
									dstHeight,
									srcRect.left,
									// the reason for this calculation is that Window's bitmap
									// is reverse.  The first scan line will be at the bottom of
									// the bitmap buffer.
									imageInfo->height - srcRect.bottom,
									srcRect.right - srcRect.left,
									srcRect.bottom - srcRect.top,
									image,
									mask);
				}

			}
			else {
				StretchPixmap(hdc, image,
								currentX, currentY, 
								dstWidth, 
								dstHeight,
								srcRect.left, srcRect.top, 
								srcRect.right - srcRect.left, 
								srcRect.bottom - srcRect.top);

			}

			currentX += dstWidth;
			srcX = 0;
			dstWidth = imageInfo->width * GetXConvertUnit();
		}

		currentY += dstHeight;
		srcY = 0;
		dstHeight = imageInfo->height  * GetYConvertUnit();
	}
	// If we called StretchPixmap, restore the memory DC to its previous state.
	if (!mask && imageInfo->hBitmap)
		::SelectObject(m_pImageDC, pOldObj);

	// If we created a temporary tile, then restore the original bitmaps.
	if (pTmpImageBitmap) {
		imageInfo->width = iOrigWidth;
		imageInfo->height = iOrigHeight;
		imageInfo->hBitmap = pSavedImageBitmap;
		VERIFY(::DeleteObject(pTmpImageBitmap));
	}
	if (pTmpMaskBitmap) {
		maskInfo->width = iOrigWidth;
		maskInfo->height = iOrigHeight;
		maskInfo->hBitmap = pSavedMaskBitmap;
		VERIFY(::DeleteObject(pTmpMaskBitmap));
	}

	// Reset the flags to indicate that we are done tiling.
	imageInfo->IsTile = FALSE;
	if (mask)
		maskInfo->IsTile = FALSE;
}


