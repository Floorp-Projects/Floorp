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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImageFrame.h"

#include "nsTransform2D.h"

NS_IMPL_ISUPPORTS1(nsImageFrame, gfxIImageFrame)

struct MONOBITMAPINFO {
  BITMAPINFOHEADER  bmiHeader;
  RGBQUAD           bmiColors[2];

  MONOBITMAPINFO(LONG aWidth, LONG aHeight)
  {
    memset(&bmiHeader, 0, sizeof(bmiHeader));
    bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiHeader.biWidth = aWidth;
    bmiHeader.biHeight = aHeight;
    bmiHeader.biPlanes = 1;
    bmiHeader.biBitCount = 1;

    // Note that the palette is being set up so the DIB and the DDB have white and
    // black reversed. This is because we need the mask to have 0 for the opaque
    // pixels of the image, and 1 for the transparent pixels. This way the SRCAND
    // operation sets the opaque pixels to 0, and leaves the transparent pixels
    // undisturbed
    bmiColors[0].rgbBlue = 255;
    bmiColors[0].rgbGreen = 255;
    bmiColors[0].rgbRed = 255;
    bmiColors[0].rgbReserved = 0;
    bmiColors[1].rgbBlue = 0;
    bmiColors[1].rgbGreen = 0;
    bmiColors[1].rgbRed = 0;
    bmiColors[1].rgbReserved = 0;
  }
};


struct ALPHA8BITMAPINFO {
  BITMAPINFOHEADER  bmiHeader;
  RGBQUAD           bmiColors[256];

  ALPHA8BITMAPINFO(LONG aWidth, LONG aHeight)
  {
    memset(&bmiHeader, 0, sizeof(bmiHeader));
    bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiHeader.biWidth = aWidth;
    bmiHeader.biHeight = aHeight;
    bmiHeader.biPlanes = 1;
    bmiHeader.biBitCount = 8;

    /* fill in gray scale palette */
     int i;
     for(i=0; i < 256; i++){
      bmiColors[i].rgbBlue = 255-i;
      bmiColors[i].rgbGreen = 255-i;
      bmiColors[i].rgbRed = 255-i;
      bmiColors[1].rgbReserved = 0;
     }
  }
};

struct ALPHA24BITMAPINFO {
  BITMAPINFOHEADER  bmiHeader;

  ALPHA24BITMAPINFO(LONG aWidth, LONG aHeight)
  {
    memset(&bmiHeader, 0, sizeof(bmiHeader));
    bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiHeader.biWidth = aWidth;
    bmiHeader.biHeight = aHeight;
    bmiHeader.biPlanes = 1;
    bmiHeader.biBitCount = 24;
  }
};

struct ALPHA32BITMAPINFO {
  BITMAPINFOHEADER  bmiHeader;

  ALPHA32BITMAPINFO(LONG aWidth, LONG aHeight)
  {
    memset(&bmiHeader, 0, sizeof(bmiHeader));
    bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiHeader.biWidth = aWidth;
    bmiHeader.biHeight = aHeight;
    bmiHeader.biPlanes = 1;
    bmiHeader.biBitCount = 32;
  }
};







nsImageFrame::nsImageFrame() :
  mTimeout(0),
  mInitalized(PR_FALSE),
  mAlphaData(nsnull)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImageFrame::~nsImageFrame()
{
  /* destructor code */
  delete mAlphaData;
}

/* void init (in nscoord aX, in nscoord aY, in nscoord aWidth, in nscoord aHeight, in gfx_format aFormat); */
NS_IMETHODIMP nsImageFrame::Init(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, gfx_format aFormat)
{
  if (aWidth <= 0 || aHeight <= 0) {
    printf("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  if (mInitalized)
    return NS_ERROR_FAILURE;

  mInitalized = PR_TRUE;

  mRect.SetRect(aX, aY, aWidth, aHeight);
  mFormat = aFormat;

  // XXX this makes an assumption about what values these have and what is between them.. i'm being bad.
  if (mFormat >= gfxIFormats::RGB_A1 && mFormat <= gfxIFormats::BGR_A8)
    mAlphaData = new ImageData;

  switch (aFormat) {
  case gfxIFormats::BGR:
  case gfxIFormats::RGB:
    mImageData.depth = 24;
    break;
  case gfxIFormats::BGRA:
  case gfxIFormats::RGBA:
    mImageData.depth = 32;
    break;

  case gfxIFormats::BGR_A1:
  case gfxIFormats::RGB_A1:
    mImageData.depth = 24;
    mAlphaData->depth = 1;
    mAlphaData->bytesPerRow = (((mRect.width + 7) / 8) + 3) & ~0x3;
//    mAlphaData->header = new MONOBITMAPINFO(mRect.width, mRect.height);
    break;
  case gfxIFormats::BGR_A8:
  case gfxIFormats::RGB_A8:
    mImageData.depth = 24;
    mAlphaData->depth = 8;
    mAlphaData->bytesPerRow = (mRect.width + 3) & ~0x3;
//    mAlphaData->header = new ALPHA8BITMAPINFO(mRect.width, mRect.height);
    break;

  default:
    printf("unsupposed gfx_format\n");
    break;
  }


  mImageData.bytesPerRow = (mRect.width * mImageData.depth) >> 5;

  if ((mRect.width * mImageData.depth) & 0x1F)
    mImageData.bytesPerRow++;
  mImageData.bytesPerRow <<= 2;

  mImageData.length = mImageData.bytesPerRow * mRect.height;
  mImageData.data = new PRUint8[mImageData.length];

  if (mAlphaData) {
    mAlphaData->length = mAlphaData->bytesPerRow * mRect.height;
    mAlphaData->data = new PRUint8[mAlphaData->length];
/*
    mAlphaData->bitmap = ::CreateDIBitmap(NULL, mAlphaData->header, CBM_INIT, NULL, (LPBITMAPINFO)mAlphaData->header,
				                                  DIB_RGB_COLORS);
*/
  }

  mImageData.header = (LPBITMAPINFOHEADER)new char[sizeof(BITMAPINFO)];

  LPBITMAPINFOHEADER header = mImageData.header;

  header->biSize = sizeof(BITMAPINFOHEADER);
	header->biWidth = mRect.width;
	header->biHeight = -mRect.height;
	header->biPlanes = 1;
	header->biBitCount = mImageData.depth;
	header->biCompression = BI_RGB;
	header->biSizeImage = mImageData.length;
	header->biXPelsPerMeter = 0;
	header->biYPelsPerMeter = 0;
	header->biClrUsed = 0;
	header->biClrImportant = 0;

  /*
  mImageData.bitmap = ::CreateDIBitmap(NULL, mImageData.header, CBM_INIT, NULL, (LPBITMAPINFO)mImageData.header,
				                               DIB_RGB_COLORS);
  */
  return NS_OK;
}

/* readonly attribute nscoord x; */
NS_IMETHODIMP nsImageFrame::GetX(nscoord *aX)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aX = mRect.x;
  return NS_OK;
}

/* readonly attribute nscoord y; */
NS_IMETHODIMP nsImageFrame::GetY(nscoord *aY)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aY = mRect.y;
  return NS_OK;
}


/* readonly attribute nscoord width; */
NS_IMETHODIMP nsImageFrame::GetWidth(nscoord *aWidth)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aWidth = mRect.width;
  return NS_OK;
}

/* readonly attribute nscoord height; */
NS_IMETHODIMP nsImageFrame::GetHeight(nscoord *aHeight)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aHeight = mRect.height;
  return NS_OK;
}

/* readonly attribute nsRect rect; */
NS_IMETHODIMP nsImageFrame::GetRect(nsRect **aRect)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aRect = &mRect;
  return NS_OK;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP nsImageFrame::GetFormat(gfx_format *aFormat)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aFormat = mFormat;
  return NS_OK;
}

/* attribute long timeout; */
NS_IMETHODIMP nsImageFrame::GetTimeout(PRInt32 *aTimeout)
{
  *aTimeout = mTimeout;
  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::SetTimeout(PRInt32 aTimeout)
{
  mTimeout = aTimeout;
  return NS_OK;
}

/* readonly attribute unsigned long imageBytesPerRow; */
NS_IMETHODIMP nsImageFrame::GetImageBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mImageData.bytesPerRow;
  return NS_OK;
}

/* readonly attribute unsigned long imageDataLength; */
NS_IMETHODIMP nsImageFrame::GetImageDataLength(PRUint32 *aBitsLength)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mImageData.length;
  return NS_OK;
}

/* void getImageData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP nsImageFrame::GetImageData(PRUint8 **aData, PRUint32 *length)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aData = mImageData.data;
  *length = mImageData.length;

  return NS_OK;
}

/* void setImageData ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP nsImageFrame::SetImageData(const PRUint8 *data, PRUint32 length, PRInt32 offset)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  if (((PRUint32)offset + length) > mImageData.length)
    return NS_ERROR_FAILURE;

  memcpy(mImageData.data + offset, data, length);

  return NS_OK;
}

/* readonly attribute unsigned long alphaBytesPerRow; */
NS_IMETHODIMP nsImageFrame::GetAlphaBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitalized || !mAlphaData)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mAlphaData->bytesPerRow;
  return NS_OK;
}

/* readonly attribute unsigned long alphaDataLength; */
NS_IMETHODIMP nsImageFrame::GetAlphaDataLength(PRUint32 *aBitsLength)
{
  if (!mInitalized || !mAlphaData)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mAlphaData->length;
  return NS_OK;
}

/* void getAlphaData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP nsImageFrame::GetAlphaData(PRUint8 **aBits, PRUint32 *length)
{
  if (!mInitalized || !mAlphaData)
    return NS_ERROR_NOT_INITIALIZED;

  *aBits = mAlphaData->data;
  *length = mAlphaData->length;

  return NS_OK;
}

/* void setAlphaData ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP nsImageFrame::SetAlphaData(const PRUint8 *data, PRUint32 length, PRInt32 offset)
{
  if (!mInitalized || !mAlphaData)
    return NS_ERROR_NOT_INITIALIZED;

  if (((PRUint32)offset + length) > mAlphaData->length)
    return NS_ERROR_FAILURE;

  memcpy(mAlphaData->data + offset, data, length);

  return NS_OK;
}







nsresult nsImageFrame::DrawImage(HDC aDestDC, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  printf("%i, %i, %i, %i    %i, %i\n", aSrcRect->x, aSrcRect->y, aSrcRect->width, aSrcRect->height,
    aDestPoint->x, aDestPoint->y);

  // Translate to bottom-up coordinates for the source bitmap
  nscoord srcY = mRect.height - (aSrcRect->y + aSrcRect->height);

  int rop = SRCCOPY;
  if (mAlphaData && mAlphaData->depth == 1) {
    MONOBITMAPINFO bmi(mRect.width, -mRect.height);
    bmi.bmiHeader.biSizeImage = mAlphaData->length;
    ::StretchDIBits(aDestDC, (aDestPoint->x + aSrcRect->x), (aDestPoint->y + aSrcRect->y),
                    aSrcRect->width, aSrcRect->height,
                    aSrcRect->x, srcY, aSrcRect->width, aSrcRect->height,
                    mAlphaData->data,
                    (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
    rop = SRCPAINT;
  }

  ::StretchDIBits(aDestDC, (aDestPoint->x + aSrcRect->x), (aDestPoint->y + aSrcRect->y),
                  aSrcRect->width, aSrcRect->height,
                  aSrcRect->x, srcY, aSrcRect->width, aSrcRect->height,
                  mImageData.data, 
                  (LPBITMAPINFO)mImageData.header, DIB_RGB_COLORS, rop);

  return NS_OK;
}


nsresult nsImageFrame::DrawScaledImage(HDC aDestDC, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  nsTransform2D trans;
  trans.SetToScale((float(mRect.width) / float(aDestRect->width)), (float(mRect.height) / float(aDestRect->height)));

  nsRect source(*aSrcRect);
  nsRect dest(*aDestRect);

  trans.TransformCoord(&source.x, &source.y, &source.width, &source.height);

  int rop = SRCCOPY;
  if (mAlphaData && mAlphaData->depth == 1) {
    MONOBITMAPINFO bmi(mRect.width, -mRect.width);
    bmi.bmiHeader.biSizeImage = mAlphaData->length;
    ::StretchDIBits(aDestDC, (aDestRect->x + aSrcRect->x), (aDestRect->y + aSrcRect->y), dest.width, dest.height,
                    source.x, source.y, source.width, source.height,
                    mAlphaData->data, 
                    (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
    rop = SRCPAINT;
  }

  ::StretchDIBits(aDestDC, (aDestRect->x + aSrcRect->x), (aDestRect->y + aSrcRect->y), dest.width, dest.height,
                  source.x, source.y, source.width, source.height,
                  mImageData.data, 
                  (LPBITMAPINFO)mImageData.header, DIB_RGB_COLORS, rop);

  return NS_OK;

}

