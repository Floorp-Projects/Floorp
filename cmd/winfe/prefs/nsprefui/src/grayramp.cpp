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

// Fixed-point number used to represent gray scale intensities. There
// are 16-bits for the fractional part and 0 bits for the integral part
typedef WORD	INTENSITY;

// Makes a fixed-point number given a floating-point intensity. Rounds up
#define MAKE_INTENSITY(intensity)\
	((INTENSITY)((intensity) * 65535. + .5))

// The number of displayable colors used. 9 is the largest number of colors
// we can use for the range intensity range .5 .. .75
#define NUM_COLORS	9

static WORD
RGB555(UINT nGray)
{
	// We're passed a gray level that is 8-bit, and we want one that
	// is 5-bit
	WORD	wGray555 = nGray >> 3;

	return (WORD)(wGray555 + (wGray555 << 5) + (wGray555 << 10));
}

// Makes a horizontal gray scale ramp from (128,128,128) to (192,192,192)
// of the specified width and height. Returns a top-down packed DIB with
// a bit-count of 16
LPBITMAPINFOHEADER
MakeGrayScaleRamp(DWORD nWidth, DWORD nHeight)
{
	LPBITMAPINFOHEADER	lpBmInfoHeader;
	LPWORD				lpBits;
	INTENSITY			nLevels[NUM_COLORS];
	INTENSITY			nThresholds[NUM_COLORS - 1];
	WORD				nColors[NUM_COLORS];

	// Each row of the DIB data needs to be a DWORD multiple
	if (nWidth & 0x01)
		nWidth++;  // make the width an even number

	// Allocate a BITMAPINFOHEADER structure, and room for the image data
	lpBmInfoHeader = (LPBITMAPINFOHEADER)HeapAlloc(GetProcessHeap(), 0,
		sizeof(BITMAPINFOHEADER) + nWidth * nHeight * sizeof(WORD));

	if (!lpBmInfoHeader)
		return NULL;
	
	// Initialize the BITMAPINFOHEADER structure
	lpBmInfoHeader->biSize 			= sizeof(BITMAPINFOHEADER);
	lpBmInfoHeader->biWidth 		= nWidth;
	lpBmInfoHeader->biHeight 		= -1 * nHeight;
	lpBmInfoHeader->biPlanes 		= 1;
	lpBmInfoHeader->biBitCount 		= 16;
	lpBmInfoHeader->biCompression 	= BI_RGB;
	lpBmInfoHeader->biSizeImage 	= 0;
	lpBmInfoHeader->biXPelsPerMeter = 0;
	lpBmInfoHeader->biYPelsPerMeter = 0;
	lpBmInfoHeader->biClrUsed 		= 0;
	lpBmInfoHeader->biClrImportant 	= 0;

	// Compute the array of quantized levels
	for (int i = 0; i < NUM_COLORS; i++)
		nLevels[i] = MAKE_INTENSITY(.5) + INTENSITY(DWORD(MAKE_INTENSITY(.25)) * i / (NUM_COLORS - 1));

	// Compute the threshold array
	for (i = 0; i < NUM_COLORS - 1; i++)
		nThresholds[i] = (nLevels[i] + nLevels[i + 1]) / 2;

	// Compute the 5-5-5 colors we're going to use
	for (i = 0; i < NUM_COLORS; i++)
		nColors[i] = RGB555(128 + 64 * i / (NUM_COLORS - 1));
	
	// Get a pointer to the bits. Since this is a packed DIB the bitmap array
	// immediately follows the BITMAPINFO header
	lpBits = (LPWORD)((LPSTR)lpBmInfoHeader + lpBmInfoHeader->biSize);

	// Initialze the gray scale ramp to the desired intensities. Just do the first
	// row and replicate that for each additional row
	for (DWORD x = 0; x < nWidth; x++)
		lpBits[x] = MAKE_INTENSITY(.515625) + INTENSITY(DWORD(MAKE_INTENSITY(.21875)) * x / (nWidth - 1));
		// lpBits[x] = MAKE_INTENSITY(.5) + INTENSITY(DWORD(MAKE_INTENSITY(.25)) * x / (nWidth - 1));

	// Now replicate the other rows
	for (DWORD y = 1; y < nHeight; y++)
		memcpy(lpBits + y * nWidth, lpBits, nWidth * sizeof(WORD));

	// Go a row at a time and convert each pixel to its nearest displayable
	// intensity. Use Floyd Steinberg error diffusion to spread the error (difference
	// between the exact intensity and the displayable intensity) over the nearby pixels
	for (y = 0; y < nHeight; y++) {
		for (x = 0; x < nWidth; x++) {
			DWORD	error;

			// Find which quantized level to use
			for (i = 0; i < NUM_COLORS - 1; i++)
				if (*lpBits < nThresholds[i])
					break;

			error = DWORD(*lpBits - nLevels[i]);
			*lpBits = nColors[i];
			
			// Spread 7/16 of the error to the pixel to the right
			if (x + 1 < nWidth)
				*(lpBits + 1) += INTENSITY(error * 7 >> 4);

			// Don't spread the error downward if this is the bottom row
			if (y + 1 < nHeight) {
				// Spread 3/16 of the error to the pixel below and to the left
				if (x > 0)
					*(lpBits + nWidth - 1) += INTENSITY(error * 3 >> 4);

				// Spread 5/16 of the error to the pixel below
				*(lpBits + nWidth) += INTENSITY(error * 5 >> 4);
	
				// Spread 1/16 of the error below and to the right
				if (x + 1 < nWidth)
					*(lpBits + nWidth + 1) += INTENSITY(error >> 4);
			}

			// Move to the next pixel
			lpBits++;
		}
	}

	return lpBmInfoHeader;
}

