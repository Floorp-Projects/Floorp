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

#include "gfxImageFrameWin.h"

NS_IMPL_ISUPPORTS2(gfxImageFrameWin, gfxIImageFrame, gfxIImageFrameWin)


#define NOT_SETUP 0x33
static PRBool gGotOSVersion = NOT_SETUP; // win2k, win xp, etc
static OSVERSIONINFO gOSVerInfo;

gfxImageFrameWin::gfxImageFrameWin() :
  mImageRowSpan(0),
  mAlphaRowSpan(0),
  mInitalized(PR_FALSE),
  mMutable(PR_TRUE),
  mHasBackgroundColor(PR_FALSE),
  mHasTransparentColor(PR_FALSE),
  mAlphaDepth(0),
  mTimeout(100),
  mBackgroundColor(0),
  mTransparentColor(0),
  mDisposalMethod(0)
{
  NS_INIT_ISUPPORTS();

  if (gGotOSVersion == NOT_SETUP) {
    gGotOSVersion = PR_TRUE;
    gOSVerInfo.dwOSVersionInfoSize = sizeof(gOSVerInfo);
    ::GetVersionEx(&gOSVerInfo);
  }

  /* member initializers and constructor code */
}

gfxImageFrameWin::~gfxImageFrameWin()
{
  /* destructor code */
}

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

  ALPHA8BITMAPINFO(LONG aWidth, LONG aHeight)
  {
    memset(&bmiHeader, 0, sizeof(bmiHeader));
    bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiHeader.biWidth = aWidth;
    bmiHeader.biHeight = aHeight;
    bmiHeader.biPlanes = 1;
    bmiHeader.biBitCount = 8;
  }
};

/* void init (in nscoord aX, in nscoord aY, in nscoord aWidth, in nscoord aHeight, in gfx_format aFormat); */
NS_IMETHODIMP gfxImageFrameWin::Init(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, gfx_format aFormat)
{
  if (mInitalized)
    return NS_ERROR_FAILURE;

  if (aWidth <= 0 || aHeight <= 0) {
    NS_ASSERTION(0, "error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  mInitalized = PR_TRUE;

  mOffset.MoveTo(aX, aY);
  mSize.SizeTo(aWidth, aHeight);

  mFormat = aFormat;

  mImage.mBMI = (LPBITMAPINFOHEADER)new BITMAPINFO;

  mImage.mBMI->biSize = sizeof(BITMAPINFOHEADER);
  mImage.mBMI->biWidth = aWidth;
  mImage.mBMI->biHeight = aHeight;
  mImage.mBMI->biPlanes = 1;
  mImage.mBMI->biBitCount = (WORD)24;
  mImage.mBMI->biCompression = BI_RGB;
  mImage.mBMI->biSizeImage = 0;
  mImage.mBMI->biXPelsPerMeter = 0;
  mImage.mBMI->biYPelsPerMeter = 0;
  mImage.mBMI->biClrUsed = 0;
  mImage.mBMI->biClrImportant = 0;

  DWORD ignoredOffset = 0;
  mImage.mDIB = ::CreateDIBSection(nsnull, (const BITMAPINFO*)mImage.mBMI, DIB_RGB_COLORS, &mImage.mDIBBits,
                                   nsnull, ignoredOffset);
  if (!mImage.mDIB)
    return NS_ERROR_FAILURE;

  mImageRowSpan = ((aWidth * 3) + 3) & ~3;

  if (aFormat == gfxIFormats::RGB_A1 || aFormat == gfxIFormats::BGR_A1) {
    mAlphaRowSpan = (((aWidth + 7) / 8) + 3) & ~0x3;
    mAlphaDepth = 1;
  } else if (aFormat == gfxIFormats::RGB_A8 || aFormat == gfxIFormats::BGR_A8) {
    mAlphaRowSpan = (aWidth + 3) & ~0x3;
    mAlphaDepth = 8;
  }

  if (mAlphaRowSpan > 0) {
    if (mAlphaDepth == 1) {
      mAlphaImage.mBMI = (LPBITMAPINFOHEADER)new MONOBITMAPINFO(aWidth, aHeight);
    } else if (mAlphaDepth == 8) {
      mAlphaImage.mBMI = (LPBITMAPINFOHEADER)new ALPHA8BITMAPINFO(aWidth, aHeight);
    }

    DWORD ignoredOffset = 0;
    mAlphaImage.mDIB = ::CreateDIBSection(nsnull, (const BITMAPINFO*)mAlphaImage.mBMI,
                                          DIB_RGB_COLORS, &mAlphaImage.mDIBBits,
                                          nsnull, ignoredOffset);

    // don't error if we can't create an alpha pixmap... we'll survive and draw what we can...
  }

  return NS_OK;
}


/* attribute boolean mutable */
NS_IMETHODIMP gfxImageFrameWin::GetMutable(PRBool *aMutable)
{
  NS_ASSERTION(mInitalized, "gfxImageFrameWin::GetMutable called on non-inited gfxImageFrameWin");
  *aMutable = mMutable;
  return NS_OK;
}

NS_IMETHODIMP gfxImageFrameWin::SetMutable(PRBool aMutable)
{
  // even though the decoder will never need the data again, we will if it has 8bit alpha
  if (mAlphaDepth != 8)
    mMutable = aMutable;
  return NS_OK;
}

/* readonly attribute nscoord x; */
NS_IMETHODIMP gfxImageFrameWin::GetX(nscoord *aX)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aX = mOffset.x;
  return NS_OK;
}

/* readonly attribute nscoord y; */
NS_IMETHODIMP gfxImageFrameWin::GetY(nscoord *aY)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aY = mOffset.y;
  return NS_OK;
}


/* readonly attribute nscoord width; */
NS_IMETHODIMP gfxImageFrameWin::GetWidth(nscoord *aWidth)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute nscoord height; */
NS_IMETHODIMP gfxImageFrameWin::GetHeight(nscoord *aHeight)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aHeight = mSize.height;
  return NS_OK;
}

/* void getRect(in nsRectRef rect); */
NS_IMETHODIMP gfxImageFrameWin::GetRect(nsRect &aRect)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  aRect.SetRect(mOffset.x, mOffset.y, mSize.width, mSize.height);

  return NS_OK;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP gfxImageFrameWin::GetFormat(gfx_format *aFormat)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aFormat = mFormat;
  return NS_OK;
}

/* readonly attribute unsigned long imageBytesPerRow; */
NS_IMETHODIMP gfxImageFrameWin::GetImageBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mImageRowSpan;
  return NS_OK;
}

/* readonly attribute unsigned long imageDataLength; */
NS_IMETHODIMP gfxImageFrameWin::GetImageDataLength(PRUint32 *aBitsLength)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mImageRowSpan * mSize.height;
  return NS_OK;
}

/* void getImageData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP gfxImageFrameWin::GetImageData(PRUint8 **aData, PRUint32 *length)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to get data on a mutable frame");

  *aData = (PRUint8*)mImage.mDIBBits;
  *length = mImageRowSpan * mSize.height;

  return NS_OK;
}

/* void setImageData ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP gfxImageFrameWin::SetImageData(const PRUint8 *aData, PRUint32 aLength, PRInt32 aOffset)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to set data on a mutable frame");
  if (!mMutable)
    return NS_ERROR_FAILURE;

  PRInt32 row_stride = mImageRowSpan;
  PRUint8 *imgData = (PRUint8*)mImage.mDIBBits;
  PRInt32 imgLen = mImageRowSpan * mSize.height;

  if (((aOffset + (PRInt32)aLength) > imgLen) || !imgData) {
    return NS_ERROR_FAILURE;
  }

  // on WinNT, Win2k, WinXP (and greater) we need to flush before we can use the bits from the DIBSection
  if (gOSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    ::GdiFlush();
  }

  PRInt32 newOffset;
#ifdef XP_PC
  newOffset = ((mSize.height - 1) * row_stride) - aOffset;
#else
  newOffset = aOffset;
#endif

  memcpy(imgData + newOffset, aData, aLength);

  /*
  PRInt32 row = (aOffset / row_stride);

  
  PRInt32 decY2 = mImage->GetDecodedY2();
  if (decY2 != mSize.height) {
    mImage->SetDecodedRect(0, 0, mSize.width, row + 1);
  }
  
  PRInt32 numnewrows = (aLength / row_stride);
  nsRect r(0, row, mSize.width, numnewrows);
  mImage->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);
  */
  return NS_OK;
}

/* void lockImageData (); */
NS_IMETHODIMP gfxImageFrameWin::LockImageData()
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  return NS_OK;
}

/* void unlockImageData (); */
NS_IMETHODIMP gfxImageFrameWin::UnlockImageData()
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  return NS_OK;
}

/* readonly attribute unsigned long alphaBytesPerRow; */
NS_IMETHODIMP gfxImageFrameWin::GetAlphaBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mAlphaRowSpan;
  return NS_OK;
}

/* readonly attribute unsigned long alphaDataLength; */
NS_IMETHODIMP gfxImageFrameWin::GetAlphaDataLength(PRUint32 *aBitsLength)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mAlphaRowSpan * mSize.height;
  return NS_OK;
}

/* void getAlphaData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP gfxImageFrameWin::GetAlphaData(PRUint8 **aData, PRUint32 *length)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to get data on a mutable frame");

  *aData = (PRUint8*)mAlphaImage.mDIBBits;
  *length = mAlphaRowSpan * mSize.height;

  return NS_OK;
}

/* void setAlphaData ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP gfxImageFrameWin::SetAlphaData(const PRUint8 *aData, PRUint32 aLength, PRInt32 aOffset)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to set data on a mutable frame");
  if (!mMutable)
    return NS_ERROR_FAILURE;


  PRInt32 row_stride = mAlphaRowSpan;
  PRUint8 *imgData = (PRUint8*)mAlphaImage.mDIBBits;
  PRInt32 imgLen = mAlphaRowSpan * mSize.height;

  if (((aOffset + (PRInt32)aLength) > imgLen) || !imgData) {
    return NS_ERROR_FAILURE;
  }

  // on WinNT, Win2k, WinXP (and greater) we need to flush before we can use the bits from the DIBSection
  if (gOSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    ::GdiFlush();
  }

  PRInt32 newOffset;
#ifdef XP_PC
  newOffset = ((mSize.height - 1) * row_stride) - aOffset;
#else
  newOffset = aOffset;
#endif

  memcpy(imgData + newOffset, aData, aLength);

  return NS_OK;
}

/* void lockAlphaData (); */
NS_IMETHODIMP gfxImageFrameWin::LockAlphaData()
{
//  if (!mInitalized || !mImage->GetHasAlphaMask())
    return NS_ERROR_NOT_INITIALIZED;

  return NS_OK;
}

/* void unlockAlphaData (); */
NS_IMETHODIMP gfxImageFrameWin::UnlockAlphaData()
{
//  if (!mInitalized || !mImage->GetHasAlphaMask())
    return NS_ERROR_NOT_INITIALIZED;

  return NS_OK;
}





/* void drawTo */
NS_IMETHODIMP gfxImageFrameWin::DrawTo(gfxIImageFrame *aDstFrame, nscoord aDX, nscoord aDY, nscoord aDWidth, nscoord aDHeight)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  // Create a memory DC that is compatible with the screen
  HDC dstMemDC = ::CreateCompatibleDC(nsnull);

  gfxImageFrameWin *imgWin = NS_STATIC_CAST(gfxImageFrameWin*, aDstFrame); 

  HBITMAP dstImage = imgWin->mImage.mDDB ? imgWin->mImage.mDDB : imgWin->mImage.mDIB;

  HBITMAP oldDstBits = (HBITMAP)::SelectObject(dstMemDC, dstImage);

  DWORD rop = SRCCOPY;

  if (mAlphaDepth == 1) {
    ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight, 0, 0, mSize.width, mSize.height, mAlphaImage.mDIBBits,
                    (LPBITMAPINFO)mAlphaImage.mBMI, DIB_RGB_COLORS, SRCAND);
    rop = SRCPAINT;
  }

  ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight, 0, 0, mSize.width, mSize.height, mImage.mDIBBits,
                  (LPBITMAPINFO)mImage.mBMI, DIB_RGB_COLORS, rop);

  ::SelectObject(dstMemDC, oldDstBits);
  ::DeleteDC(dstMemDC);
  
  return NS_OK;
}


/* attribute long timeout; */
NS_IMETHODIMP gfxImageFrameWin::GetTimeout(PRInt32 *aTimeout)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  if (mTimeout == 0)
    *aTimeout = 100; // Ensure a minimal time between updates so we don't throttle the UI thread.
  else
    *aTimeout = mTimeout;
  return NS_OK;
}

NS_IMETHODIMP gfxImageFrameWin::SetTimeout(PRInt32 aTimeout)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  mTimeout = aTimeout;
  return NS_OK;
}

/* attribute long frameDisposalMethod; */
NS_IMETHODIMP gfxImageFrameWin::GetFrameDisposalMethod(PRInt32 *aFrameDisposalMethod)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aFrameDisposalMethod = mDisposalMethod;
  return NS_OK;
}
NS_IMETHODIMP gfxImageFrameWin::SetFrameDisposalMethod(PRInt32 aFrameDisposalMethod)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  mDisposalMethod = aFrameDisposalMethod;
  return NS_OK;
}

/* attribute gfx_color backgroundColor; */
NS_IMETHODIMP gfxImageFrameWin::GetBackgroundColor(gfx_color *aBackgroundColor)
{
  if (!mInitalized || !mHasBackgroundColor)
    return NS_ERROR_NOT_INITIALIZED;

  *aBackgroundColor = mBackgroundColor;
  return NS_OK;
}
NS_IMETHODIMP gfxImageFrameWin::SetBackgroundColor(gfx_color aBackgroundColor)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  mBackgroundColor = aBackgroundColor;
  mHasBackgroundColor = PR_TRUE;
  return NS_OK;
}

/* attribute gfx_color transparentColor; */
NS_IMETHODIMP gfxImageFrameWin::GetTransparentColor(gfx_color *aTransparentColor)
{
  if (!mInitalized || !mHasTransparentColor)
    return NS_ERROR_NOT_INITIALIZED;
    
  *aTransparentColor = mTransparentColor;
  return NS_OK;
}
NS_IMETHODIMP gfxImageFrameWin::SetTransparentColor(gfx_color aTransparentColor)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  mTransparentColor = aTransparentColor;
  mHasTransparentColor = PR_TRUE;
  return NS_OK;
}





NS_IMETHODIMP gfxImageFrameWin::GetDIB(HBITMAP *aBitmap, HBITMAP *aMask)
{
  if (!mImage.mDIB) {
    *aBitmap = nsnull;
    return NS_ERROR_FAILURE;
  }

  *aBitmap = mImage.mDIB;
  *aMask = mAlphaImage.mDIB;

  return NS_OK;
}

NS_IMETHODIMP gfxImageFrameWin::GetDDB(HBITMAP *aBitmap, HBITMAP *aMask)
{

  if (!mImage.mDDB) {
    *aBitmap = nsnull;
    return NS_ERROR_FAILURE;
  }

  *aBitmap = mImage.mDDB;
  *aMask = mAlphaImage.mDIB; // XXX convert to ddb

  return NS_OK;
}

NS_IMETHODIMP gfxImageFrameWin::GetBitmap(HDC aHDC, HBITMAP *aBitmap, HBITMAP *aMask, gfx_depth *aAlphaDepth)
{
  if (mImage.mDDB) {
    // XXX check and see if our DDB is compatible with aHDC

    *aBitmap = mImage.mDDB;
    *aMask = mAlphaImage.mDDB ? mAlphaImage.mDDB : mAlphaImage.mDIB;
    *aAlphaDepth = mAlphaDepth;

    return NS_OK;

  } else if (mImage.mDIB && !mMutable) {

    // win95/98/me only support up to 16MB DDBs.
    if (gOSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
      if ((mImageRowSpan * mSize.height) > 16000000) {
        *aBitmap = mImage.mDIB;
        *aMask = mAlphaImage.mDIB;
        *aAlphaDepth = mAlphaDepth;
        return NS_OK;
      }
    }

    // Convert the DIB to a DDB and throw out the DIB

    // on WinNT, Win2k, WinXP (and greater) we need to flush before we can use the bits from the DIBSection
    if (gOSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
      ::GdiFlush();
    }

    // Create the DDB from the DIB
    mImage.mDDB = ::CreateDIBitmap(aHDC, mImage.mBMI, CBM_INIT, mImage.mDIBBits, (LPBITMAPINFO)mImage.mBMI,
                                   DIB_RGB_COLORS);

    if (!mImage.mDDB) {
      *aBitmap = mImage.mDIB;
      *aMask = mAlphaImage.mDIB;
      *aAlphaDepth = mAlphaDepth;
      return NS_OK;
    }

    // throw away the DIB
    ::DeleteObject(mImage.mDIB);
    mImage.mDIB = nsnull;
    mImage.mDIBBits = nsnull;
    delete[] mImage.mBMI;
    mImage.mBMI = nsnull;

/*
    // we may need to use a different DC for 1bit things?
    if (mAlphaImage.mBMI) {
      // Create the DDB from the alpha DIB
      mAlphaImage.mDDB = ::CreateDIBitmap(aHDC, mAlphaImage.mBMI, CBM_INIT, mAlphaImage.mDIBBits, (LPBITMAPINFO)mAlphaImage.mBMI,
                                          DIB_RGB_COLORS);

      if (!mAlphaImage.mDDB) {
        // Unable to create a DDB.. :(
        *aBitmap = mImage.mDDB;
        *aMask = mAlphaImage.mDIB;
        *aAlphaDepth = mAlphaDepth;
        return NS_OK;
      }

      if (mAlphaImage.mDIB) {
        ::DeleteObject(mAlphaImage.mDIB);
        mAlphaImage.mDIB = nsnull;
        mAlphaImage.mDIBBits = nsnull;
      }
      delete[] mAlphaImage.mBMI;
      mAlphaImage.mBMI = nsnull;
    }
*/

    *aBitmap = mImage.mDDB;
    *aMask = mAlphaImage.mDDB ? mAlphaImage.mDDB : mAlphaImage.mDIB;
    *aAlphaDepth = mAlphaDepth;

    return NS_OK;
  }

  *aBitmap = mImage.mDIB;
  *aMask = mAlphaImage.mDIB;
  *aAlphaDepth = mAlphaDepth;

  return NS_OK;
}

NS_IMETHODIMP gfxImageFrameWin::GetAlphaStuff(HBITMAP *aBitmap, LPBITMAPINFOHEADER *aInfoHeader, void **aBits)
{
  if (!mAlphaImage.mDIB)
    return NS_ERROR_FAILURE;

  *aBitmap = mAlphaImage.mDIB;
  *aInfoHeader = mAlphaImage.mBMI;
  *aBits = mAlphaImage.mDIBBits;

  return NS_OK;
}
