/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImage.h"

#include "nsUnitConverters.h"

NS_IMPL_ISUPPORTS1(nsImage, nsIImage2)

nsImage::nsImage() :
  mBits(nsnull)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImage::~nsImage()
{
  /* destructor code */
  delete[] mBits;
  mBits = nsnull;
}



#include <windows.h>

void errhandler(char *foo, void *a) {}

void CreateBMPFile(HWND hwnd, LPTSTR pszFile, PBITMAPINFO pbi, 
                  HBITMAP hBMP, HDC hDC) 
 { 
     HANDLE hf;                 // file handle 
    BITMAPFILEHEADER hdr;       // bitmap file-header 
    PBITMAPINFOHEADER pbih;     // bitmap info-header 
    LPBYTE lpBits;              // memory pointer 
    DWORD dwTotal;              // total count of bytes 
    DWORD cb;                   // incremental count of bytes 
    BYTE *hp;                   // byte pointer 
    DWORD dwTmp; 

    pbih = (PBITMAPINFOHEADER) pbi; 
    lpBits = (LPBYTE) GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);

    if (!lpBits) 
         errhandler("GlobalAlloc", hwnd); 

    // Retrieve the color table (RGBQUAD array) and the bits 
    // (array of palette indices) from the DIB. 
    if (!GetDIBits(hDC, hBMP, 0, (WORD) pbih->biHeight, lpBits, pbi, 
        DIB_RGB_COLORS)) 
    {
        errhandler("GetDIBits", hwnd); 
    }

    // Create the .BMP file. 
    hf = CreateFile(pszFile, 
                   GENERIC_READ | GENERIC_WRITE, 
                   (DWORD) 0, 
                    NULL, 
                   CREATE_ALWAYS, 
                   FILE_ATTRIBUTE_NORMAL, 
                   (HANDLE) NULL); 
    if (hf == INVALID_HANDLE_VALUE) 
        errhandler("CreateFile", hwnd); 
    hdr.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M" 
    // Compute the size of the entire file. 
    hdr.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) + 
                 pbih->biSize + pbih->biClrUsed 
                 * sizeof(RGBQUAD) + pbih->biSizeImage); 
    hdr.bfReserved1 = 0; 
    hdr.bfReserved2 = 0; 

    // Compute the offset to the array of color indices. 
    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + 
                    pbih->biSize + pbih->biClrUsed 
                    * sizeof (RGBQUAD); 

    // Copy the BITMAPFILEHEADER into the .BMP file. 
    if (!WriteFile(hf, (LPVOID) &hdr, sizeof(BITMAPFILEHEADER), 
        (LPDWORD) &dwTmp,  NULL)) 
    {
       errhandler("WriteFile", hwnd); 
    }

    // Copy the BITMAPINFOHEADER and RGBQUAD array into the file. 
    if (!WriteFile(hf, (LPVOID) pbih, 
                   sizeof(BITMAPINFOHEADER) + pbih->biClrUsed * sizeof (RGBQUAD), 
                  (LPDWORD) &dwTmp,
                  (NULL)
                  ) 
       )
        errhandler("WriteFile", hwnd); 

    // Copy the array of color indices into the .BMP file. 
    dwTotal = cb = pbih->biSizeImage; 
    hp = lpBits; 
    if (!WriteFile(hf, (LPSTR) hp, (int) cb, (LPDWORD) &dwTmp,NULL)) 
           errhandler("WriteFile", hwnd); 

    // Close the .BMP file. 
     if (!CloseHandle(hf)) 
           errhandler("CloseHandle", hwnd); 

    // Free memory. 
    GlobalFree((HGLOBAL)lpBits);
}




/* void init (in gfx_dimension aWidth, in gfx_dimension aHeight, in gfx_format aFormat); */
NS_IMETHODIMP nsImage::Init(gfx_dimension aWidth, gfx_dimension aHeight, gfx_format aFormat)
{
  if (aWidth <= 0 || aHeight <= 0) {
    printf("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  delete[] mBits;

  mSize.SizeTo(aWidth, aHeight);
  mFormat = aFormat;

  switch (aFormat) {
  case nsIGFXFormat::RGB:
  case nsIGFXFormat::RGB_A1:
  case nsIGFXFormat::RGB_A8:
    mDepth = 24;
    break;
  case nsIGFXFormat::RGBA:
    mDepth = 32;
    break;
  default:
    printf("unsupposed gfx_format\n");
    break;
  }

  PRInt32 ceilWidth(GFXCoordToIntCeil(mSize.width));

  mBytesPerRow = (ceilWidth * mDepth) >> 5;

  if ((ceilWidth * mDepth) & 0x1F)
    mBytesPerRow++;
  mBytesPerRow <<= 2;

  mBitsLength = mBytesPerRow * GFXCoordToIntCeil(mSize.height);

  mBits = new PRUint8[mBitsLength];

  return NS_OK;
}

/* void initFromDrawable (in nsIDrawable aDrawable, in gfx_coord aX, in gfx_coord aY, in gfx_dimension aWidth, in gfx_dimension aHeight); */
NS_IMETHODIMP nsImage::InitFromDrawable(nsIDrawable *aDrawable, gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_dimension width; */
NS_IMETHODIMP nsImage::GetWidth(gfx_dimension *aWidth)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute gfx_dimension height; */
NS_IMETHODIMP nsImage::GetHeight(gfx_dimension *aHeight)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aHeight = mSize.height;
  return NS_OK;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP nsImage::GetFormat(gfx_format *aFormat)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;



  HWND bg = GetDesktopWindow();
  HDC memDC = GetDC(NULL);

  LPBITMAPINFOHEADER mBHead = (LPBITMAPINFOHEADER)new char[sizeof(BITMAPINFO)];

  mBHead->biSize = sizeof(BITMAPINFOHEADER);
	mBHead->biWidth = GFXCoordToIntCeil(mSize.width);
	mBHead->biHeight = -GFXCoordToIntCeil(mSize.height);
	mBHead->biPlanes = 1;
	mBHead->biBitCount = 24;
	mBHead->biCompression = BI_RGB;
	mBHead->biSizeImage = mBitsLength;            // not compressed, so we dont need this to be set
	mBHead->biXPelsPerMeter = 0;
	mBHead->biYPelsPerMeter = 0;
	mBHead->biClrUsed = 0;
	mBHead->biClrImportant = 0;

  HBITMAP memBM = ::CreateDIBitmap(memDC,mBHead,CBM_INIT,mBits,(LPBITMAPINFO)mBHead,
				                           DIB_RGB_COLORS);

  SelectObject(memDC, memBM);


	mBHead->biHeight = -mBHead->biHeight;

  CreateBMPFile(bg, "c:\\whatever.bmp", (LPBITMAPINFO)mBHead, 
                memBM, memDC) ;

  ReleaseDC(NULL, memDC);

  DeleteObject(memBM);


  *aFormat = mFormat;
  return NS_OK;
}

/* readonly attribute unsigned long bytesPerRow; */
NS_IMETHODIMP nsImage::GetBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mBytesPerRow;
  return NS_OK;
}

/* readonly attribute unsigned long bitsLength; */
NS_IMETHODIMP nsImage::GetBitsLength(PRUint32 *aBitsLength)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mBitsLength;
  return NS_OK;
}

/* void getBits([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP nsImage::GetBits(PRUint8 **aBits, PRUint32 *length)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aBits = mBits;
  *length = mBitsLength;

  return NS_OK;
}

/* void setBits ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP nsImage::SetBits(const PRUint8 *data, PRUint32 length, PRInt32 offset)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  memcpy(mBits + offset, data, length);

  return NS_OK;
}
