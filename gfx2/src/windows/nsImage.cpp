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

NS_IMPL_ISUPPORTS1(nsImage, nsIImage)

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

#include <windows.h>

void errhandler(char *foo, void *a) {}

PBITMAPINFO CreateBitmapInfoStruct(HWND hwnd, HBITMAP hBmp)
{ 
    BITMAP bmp; 
    PBITMAPINFO pbmi; 
    WORD    cClrBits; 

    // Retrieve the bitmap's color format, width, and height. 
    if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp)) 
        errhandler("GetObject", hwnd); 

    // Convert the color format to a count of bits. 
    cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel); 
    if (cClrBits == 1) 
        cClrBits = 1; 
    else if (cClrBits <= 4) 
        cClrBits = 4; 
    else if (cClrBits <= 8) 
        cClrBits = 8; 
    else if (cClrBits <= 16) 
        cClrBits = 16; 
    else if (cClrBits <= 24) 
        cClrBits = 24; 
    else cClrBits = 32; 

    // Allocate memory for the BITMAPINFO structure. (This structure 
    // contains a BITMAPINFOHEADER structure and an array of RGBQUAD 
    // data structures.) 

     if (cClrBits != 24) 
         pbmi = (PBITMAPINFO) LocalAlloc(LPTR, 
                    sizeof(BITMAPINFOHEADER) + 
                    sizeof(RGBQUAD) * (1<< cClrBits)); 

     // There is no RGBQUAD array for the 24-bit-per-pixel format. 

     else 
         pbmi = (PBITMAPINFO) LocalAlloc(LPTR, 
                    sizeof(BITMAPINFOHEADER)); 

    // Initialize the fields in the BITMAPINFO structure. 

    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
    pbmi->bmiHeader.biWidth = bmp.bmWidth; 
    pbmi->bmiHeader.biHeight = bmp.bmHeight; 
    pbmi->bmiHeader.biPlanes = bmp.bmPlanes; 
    pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel; 
    if (cClrBits < 24) 
        pbmi->bmiHeader.biClrUsed = (1<<cClrBits); 

    // If the bitmap is not compressed, set the BI_RGB flag. 
    pbmi->bmiHeader.biCompression = BI_RGB; 

    // Compute the number of bytes in the array of color 
    // indices and store the result in biSizeImage. 
    // Width must be DWORD aligned unless bitmap is RLE compressed.
    pbmi->bmiHeader.biSizeImage = (pbmi->bmiHeader.biWidth + 15) /16 
                                  * pbmi->bmiHeader.biHeight 
                                  * cClrBits;
    // Set biClrImportant to 0, indicating that all of the 
    // device colors are important. 
     pbmi->bmiHeader.biClrImportant = 0; 
     return pbmi; 
 } 

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


/* readonly attribute PRUint8 bits; */
NS_IMETHODIMP nsImage::GetBits(PRUint8 *aBits)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;


  HWND bg = GetDesktopWindow();
  HDC memDC = GetDC(bg);

//  HBITMAP memBM = CreateCompatibleBitmap(memDC, GFXCoordToIntCeil(mSize.width), GFXCoordToIntCeil(mSize.height));


  LPBITMAPINFOHEADER mBHead = (LPBITMAPINFOHEADER)new char[sizeof(BITMAPINFO)];

  mBHead->biSize = sizeof(BITMAPINFOHEADER);
	mBHead->biWidth = GFXCoordToIntCeil(mSize.width);
	mBHead->biHeight = GFXCoordToIntCeil(mSize.height);
	mBHead->biPlanes = 1;
	mBHead->biBitCount = 24;
	mBHead->biCompression = BI_RGB;
	mBHead->biSizeImage = 0;            // not compressed, so we dont need this to be set
	mBHead->biXPelsPerMeter = 0;
	mBHead->biYPelsPerMeter = 0;
	mBHead->biClrUsed = 0;
	mBHead->biClrImportant = 0;

  HBITMAP memBM = ::CreateDIBitmap(memDC,mBHead,CBM_INIT,mBits,(LPBITMAPINFO)mBHead,
				                      DIB_RGB_COLORS);

  PBITMAPINFO info = CreateBitmapInfoStruct(bg, memBM);


  CreateBMPFile(bg, "c:\\whatever.bmp", info, 
                  memBM, memDC) ;

  DeleteObject(memBM);

  FILE *f = fopen("C:\\image.data", "w");
  fwrite(mBits, mBitsLength, 1, f);
  fclose(f);


  return NS_OK;
}

/* void setBits ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP nsImage::SetBits(const PRUint8 *data, PRUint32 length, PRInt32 offset)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  PRUint32 i;
  PRInt32 off;
  // XXX i could unroll this loop a bit :-)
  for (i=0, off = offset; i <= length; ++i, ++off) {
    mBits[off] = data[i];
  }
  return NS_OK;
}
