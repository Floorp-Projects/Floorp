/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "nsImageWin.h"
#include "nsRenderingContextWin.h"

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

static PRBool
IsWindowsNT()
{
  OSVERSIONINFO versionInfo;

  versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
  ::GetVersionEx(&versionInfo);
  return VER_PLATFORM_WIN32_NT == versionInfo.dwPlatformId;
}

PRBool nsImageWin::gIsWinNT = IsWindowsNT();

/** ----------------------------------------------------------------
  * Constructor for nsImageWin
  * @update dc - 11/20/98
  */
nsImageWin :: nsImageWin()
{
  NS_INIT_REFCNT();

  mImageBits = nsnull;
	mHBitmap = nsnull;
	mAlphaHBitmap = nsnull;
  mAlphaBits = nsnull;
  mColorMap = nsnull;
  mBHead = nsnull;

	CleanUp(PR_TRUE);
}

/** ----------------------------------------------------------------
  * destructor for nsImageWin
  * @update dc - 11/20/98
  */
nsImageWin :: ~nsImageWin()
{
	CleanUp(PR_TRUE);
}

NS_IMPL_ISUPPORTS(nsImageWin, kIImageIID);

/** ----------------------------------------------------------------
 * Initialize the nsImageWin object
 * @update dc - 11/20/98
 * @param aWidth - Width of the image
 * @param aHeight - Height of the image
 * @param aDepth - Depth of the image
 * @param aMaskRequirements - A mask used to specify if alpha is needed.
 * @result NS_OK if the image was initied ok
 */
nsresult nsImageWin :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
	mHBitmap = nsnull;
	mAlphaHBitmap = nsnull;
	CleanUp(PR_TRUE);

  if (8 == aDepth) {
    mNumPaletteColors = 256;
    mNumBytesPixel = 1;
  } else if (24 == aDepth) {
    mNumPaletteColors = 0;
    mNumBytesPixel = 3;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }

  mIsTopToBottom = PR_FALSE;
  if (mNumPaletteColors >= 0){
    // If we have a palette
    if (0 == mNumPaletteColors) {
      // space for the header only (no color table)
      mBHead = (LPBITMAPINFOHEADER)new char[sizeof(BITMAPINFO)];
    } else {
      // Space for the header and the palette. Since we'll be using DIB_PAL_COLORS
      // the color table is an array of 16-bit unsigned integers that specify an
      // index into the currently realized logical palette
      mBHead = (LPBITMAPINFOHEADER)new char[sizeof(BITMAPINFOHEADER) + (256 * sizeof(WORD))];
    }
    mBHead->biSize = sizeof(BITMAPINFOHEADER);
	  mBHead->biWidth = aWidth;
	  mBHead->biHeight = aHeight;
	  mBHead->biPlanes = 1;
	  mBHead->biBitCount = (WORD)aDepth;
	  mBHead->biCompression = BI_RGB;
	  mBHead->biSizeImage = 0;            // not compressed, so we dont need this to be set
	  mBHead->biXPelsPerMeter = 0;
	  mBHead->biYPelsPerMeter = 0;
	  mBHead->biClrUsed = mNumPaletteColors;
	  mBHead->biClrImportant = mNumPaletteColors;

    // Compute the size of the image
    mRowBytes = CalcBytesSpan(mBHead->biWidth);
    mSizeImage = mRowBytes * mBHead->biHeight; // no compression

    // Allocate the image bits
    mImageBits = new unsigned char[mSizeImage];
    // XXX We don't need to waste time initializing the bits. The reason Purify
    // complains about UMR is because when asked to Draw() we ask GDI to render
    // bits that aren't valid yet. We need to fix that...
#if 0
    memset(mImageBits, 128, mSizeImage);
#endif

    if (256 == mNumPaletteColors) {
      // Initialize the array of indexes into the logical palette
      WORD* palIndx = (WORD*)(((LPBYTE)mBHead) + mBHead->biSize);
      for (WORD index = 0; index < 256; index++) {
	*palIndx++ = index;
      }
    }

    // Allocate mask image bits if requested
    if (aMaskRequirements != nsMaskRequirements_kNoMask){
      if (nsMaskRequirements_kNeeds1Bit == aMaskRequirements){
	mARowBytes = (aWidth + 7) / 8;
	mAlphaDepth = 1;
      }else{
	NS_ASSERTION(nsMaskRequirements_kNeeds8Bit == aMaskRequirements,
		     "unexpected mask depth");
	mARowBytes = aWidth;
	mAlphaDepth = 8;
      }

      // 32-bit align each row
      mARowBytes = (mARowBytes + 3) & ~0x3;

      mAlphaBits = new unsigned char[mARowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
    }else{
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
    }

    // XXX Let's only do this if we actually have a palette...
    mColorMap = new nsColorMap;

    if (mColorMap != nsnull){
      mColorMap->NumColors = mNumPaletteColors;
      mColorMap->Index = nsnull;
      if (mColorMap->NumColors > 0) {
	mColorMap->Index = new PRUint8[3 * mColorMap->NumColors];

	// XXX Note: I added this because purify claims that we make a
	// copy of the memory (which we do!). I'm not sure if this
	// matters or not, but this shutup purify.
	memset(mColorMap->Index, 0, sizeof(PRUint8) * (3 * mColorMap->NumColors));
      }
		}
  }

  return NS_OK;
}

/** ----------------------------------------------------------------
  * This routine started out to set up a color table, which as been 
  * outdated, and seemed to have been used for other purposes.  This 
  * routine should be deleted soon -- dc
  * @update dc - 11/20/98
  * @param aContext - a device context to use
  * @param aFlags - flags used for the color manipulation
  * @param aUpdateRect - The update rectangle to use
  * @result void
  */
void nsImageWin :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
  // XXX Any gamma correction should be done in the image library, and not
  // here...
#if 0
  if (aFlags & nsImageUpdateFlags_kColorMapChanged){
    PRUint8 *gamma = aContext->GetGammaTable();

    if (mColorMap->NumColors > 0){
      PRUint8* cpointer = mColorTable;

      for(PRInt32 i = 0; i < mColorMap->NumColors; i++){
		    *cpointer++ = gamma[mColorMap->Index[(3 * i) + 2]];
		    *cpointer++ = gamma[mColorMap->Index[(3 * i) + 1]];
		    *cpointer++ = gamma[mColorMap->Index[(3 * i)]];
		    *cpointer++ = 0;
      }
    }
  }else if ((aFlags & nsImageUpdateFlags_kBitsChanged) &&(nsnull != aUpdateRect)){
    if (0 == mNumPaletteColors){
      PRInt32 x, y, span = CalcBytesSpan(mBHead->biWidth), idx;
      PRUint8 *pixels = mImageBits + 
				(mBHead->biHeight - aUpdateRect->y - aUpdateRect->height) * span + 
				aUpdateRect->x * 3;
      PRUint8 *gamma;
      float    gammaValue;
      aContext->GetGammaTable(gamma);
      aContext->GetGamma(gammaValue);

      // Gamma correct the image
      if (1.0 != gammaValue){
	for (y = 0; y < aUpdateRect->height; y++){
	  for (x = 0, idx = 0; x < aUpdateRect->width; x++){
	    pixels[idx] = gamma[pixels[idx]];
	    idx++;
	    pixels[idx] = gamma[pixels[idx]];
	    idx++;
	    pixels[idx] = gamma[pixels[idx]];
	    idx++;
	  }
	  pixels += span;
	}
      }
    }
  }
#endif
}

//------------------------------------------------------------

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

// Raster op used with MaskBlt(). Assumes our transparency mask has 0 for the
// opaque pixels and 1 for the transparent pixels. That means we want the
// background raster op (value of 0) to be SRCCOPY, and the foreground raster
// (value of 1) to just use the destination bits
#define MASKBLT_ROP MAKEROP4((DWORD)0x00AA0029, SRCCOPY)

/** ----------------------------------------------------------------
  * Create a device dependent windows bitmap
  * @update dc - 11/20/98
  * @param aSurface - The HDC in the form of a drawing surface used to create the DDB
  * @result void
  */
void nsImageWin :: CreateDDB(nsDrawingSurface aSurface)
{
  HDC TheHDC;

  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);

  if (TheHDC != NULL)
  {
    if (mSizeImage > 0)
    {
      mHBitmap = ::CreateDIBitmap(TheHDC, mBHead, CBM_INIT, mImageBits, (LPBITMAPINFO)mBHead,
				  256 == mNumPaletteColors ? DIB_PAL_COLORS : DIB_RGB_COLORS);

      if (nsnull != mAlphaBits)
      {
	// Create a monochrome bitmap
	// XXX Handle the case of 8-bit alpha...
	NS_ASSERTION(1 == mAlphaDepth, "unexpected alpha depth");
	mAlphaHBitmap = ::CreateBitmap(mAlphaWidth, mAlphaHeight, 1, 1, NULL);

	MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);
	::SetDIBits(TheHDC, mAlphaHBitmap, 0, mAlphaHeight, mAlphaBits,
		    (LPBITMAPINFO)&bmi, DIB_RGB_COLORS);
      }

      CleanUp(PR_FALSE);
    }

    ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
  }
}

/** ----------------------------------------------------------------
  * Draw the bitmap, this method has a source and destination coordinates
  * @update dc - 11/20/98
  * @param aContext - the rendering context to draw with
  * @param aSurface - The HDC in a nsDrawingsurfaceWin to copy the bits to.
  * @param aSX - source horizontal location
  * @param aSY - source vertical location
  * @param aSWidth - source width
  * @param aSHeight - source height
  * @param aDX - destination location
  * @param aDY - destination location
  * @param aDWidth - destination width
  * @param aDHeight - destination height
  * @result NS_OK if the draw worked
  */
NS_IMETHODIMP nsImageWin :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
				 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  HDC   TheHDC;

  if (mBHead == nsnull) 
    return NS_ERROR_FAILURE;

  // If the image can be optimized then make sure we've created the DDB
  if (mIsOptimized && (nsnull == mHBitmap))
    CreateDDB(aSurface);

  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);

  if (nsnull != TheHDC)
  {
    if (!IsOptimized())
    {
      DWORD rop = SRCCOPY;

      if (nsnull != mAlphaBits)
      {
	MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

	::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,
			aSX, aSY, aSWidth, aSHeight, mAlphaBits,
			(LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
	rop = SRCPAINT;
      }

      ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,
		      aSX, aSY, aSWidth, aSHeight, mImageBits,
		      (LPBITMAPINFO)mBHead, 256 == mNumPaletteColors ? DIB_PAL_COLORS :
		      DIB_RGB_COLORS, rop);
    }
    else
    {
      nsIDeviceContext    *dx;
      aContext.GetDeviceContext(dx);
      nsDrawingSurface     ds;
      dx->GetDrawingSurface(aContext, ds);
      nsDrawingSurfaceWin *srcDS = (nsDrawingSurfaceWin *)ds;
      HDC                 srcDC;

      if (nsnull != srcDS)
      {
	srcDS->GetDC(&srcDC);

	if (NULL != srcDC)
	{
	  HBITMAP oldbits;

	  if (nsnull == mAlphaHBitmap)
	  {
	    oldbits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
	    ::StretchBlt(TheHDC, aDX, aDY, aDWidth, aDHeight, srcDC, aSX, aSY,
			 aSWidth, aSHeight, SRCCOPY);
	  }
	  else if (gIsWinNT && (aDWidth == aSWidth) && (aDHeight == aSHeight)){
	    oldbits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
	    ::MaskBlt(TheHDC, aDX, aDY, aDWidth, aDHeight,
		      srcDC, aSX, aSY, mAlphaHBitmap, aSX, aSY, MASKBLT_ROP);
	  }
	  else
	  {
	    COLORREF oldTextColor = ::SetTextColor(TheHDC, RGB(0, 0, 0));
	    COLORREF oldBkColor = ::SetBkColor(TheHDC, RGB(255, 255, 255));
	    oldbits = (HBITMAP)::SelectObject(srcDC, mAlphaHBitmap);
	    ::StretchBlt(TheHDC, aDX, aDY, aDWidth, aDHeight, srcDC, aSX, aSY,
			 aSWidth, aSHeight, SRCAND);
	    ::SetTextColor(TheHDC, oldTextColor);
	    ::SetBkColor(TheHDC, oldBkColor);

	    ::SelectObject(srcDC, mHBitmap);
	    ::StretchBlt(TheHDC, aDX, aDY, aDWidth, aDHeight, srcDC, aSX, aSY,
			 aSWidth, aSHeight, SRCPAINT);
	  }

	  ::SelectObject(srcDC, oldbits);

	  srcDS->ReleaseDC();
	}
      }

      NS_RELEASE(dx);
    }

    ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
  }

  return NS_OK;
}

/** ----------------------------------------------------------------
  * Draw the bitmap, defaulting the source to 0,0,source 
  * and destination have the same width and height.
  * @update dc - 11/20/98
  * @param aContext - the rendering context to draw with
  * @param aSurface - The HDC in a nsDrawingsurfaceWin to copy the bits to.
  * @param aX - destination location
  * @param aY - destination location
  * @param aWidth - image width
  * @param aHeight - image height
  * @result NS_OK if the draw worked
  */
NS_IMETHODIMP nsImageWin :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  HDC   TheHDC;

  if (mBHead == nsnull) 
    return NS_ERROR_FAILURE;

  // If the image can be optimized then make sure we've created the DDB
  if (mIsOptimized && (nsnull == mHBitmap))
    CreateDDB(aSurface);

  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);

  if (nsnull != TheHDC)
  {
    if (!IsOptimized())
    {
      DWORD rop = SRCCOPY;

      if (nsnull != mAlphaBits)
      {
	MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

	::StretchDIBits(TheHDC, aX, aY, aWidth, aHeight,
			0, 0, mAlphaWidth, mAlphaHeight, mAlphaBits,
			(LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
	rop = SRCPAINT;
      }

      ::StretchDIBits(TheHDC, aX, aY, aWidth, aHeight,
		      0, 0, mBHead->biWidth, mBHead->biHeight, mImageBits,
		      (LPBITMAPINFO)mBHead, 256 == mNumPaletteColors ? DIB_PAL_COLORS :
		      DIB_RGB_COLORS, rop);
    }
    else
    {
      nsIDeviceContext    *dx;
      aContext.GetDeviceContext(dx);
      nsDrawingSurface     ds;
      dx->GetDrawingSurface(aContext, ds);
      nsDrawingSurfaceWin *srcDS = (nsDrawingSurfaceWin *)ds;
      HDC                 srcDC;

      if (nsnull != srcDS)
      {
	srcDS->GetDC(&srcDC);

	if (NULL != srcDC)
	{
	  HBITMAP oldBits;

	  if (nsnull == mAlphaHBitmap)
	  {
	    oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
	    ::StretchBlt(TheHDC, aX, aY, aWidth, aHeight, srcDC, 0, 0,
			 mBHead->biWidth, mBHead->biHeight, SRCCOPY);

	  }
	  else if (gIsWinNT && (aWidth == mBHead->biWidth) && (aHeight == mBHead->biHeight)){
	    oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
	    ::MaskBlt(TheHDC, aX, aY, aWidth, aHeight,
		      srcDC, 0, 0, mAlphaHBitmap, 0, 0, MASKBLT_ROP);
	  }
	  else
	  {
	    COLORREF oldTextColor = ::SetTextColor(TheHDC, RGB(0, 0, 0));
	    COLORREF oldBkColor = ::SetBkColor(TheHDC, RGB(255, 255, 255));
	    oldBits = (HBITMAP)::SelectObject(srcDC, mAlphaHBitmap);
	    ::StretchBlt(TheHDC, aX, aY, aWidth, aHeight, srcDC, 0, 0,
			 mAlphaWidth, mAlphaHeight, SRCAND);
	    ::SetTextColor(TheHDC, oldTextColor);
	    ::SetBkColor(TheHDC, oldBkColor);

	    ::SelectObject(srcDC, mHBitmap);
	    ::StretchBlt(TheHDC, aX, aY, aWidth, aHeight, srcDC, 0, 0,
			 mBHead->biWidth, mBHead->biHeight, SRCPAINT);
	  }

	  ::SelectObject(srcDC, oldBits);

	  srcDS->ReleaseDC();
	}
      }

      NS_RELEASE(dx);
    }

    ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
  }

  return NS_OK;
}

/** ----------------------------------------------------------------
 * Create an optimezed bitmap, -- this routine may need to be deleted, not really used now
 * @update dc - 11/20/98
 * @param aContext - The device context to use for the optimization
 */
nsresult nsImageWin :: Optimize(nsIDeviceContext* aContext)
{
  // Wait until we're asked to draw before creating the DDB, because
  // we don't have an HDC now
  mIsOptimized = PR_TRUE;
  return NS_OK;
}

/** ----------------------------------------------------------------
 * Calculate the number of bytes in a span for this image
 * @update dc - 11/20/98
 * @param aWidth - The width to calulate the number of bytes from
 * @return Number of bytes for the span
 */
PRInt32  nsImageWin :: CalcBytesSpan(PRUint32  aWidth)
{
  PRInt32 spanBytes;

  spanBytes = (aWidth * mBHead->biBitCount) >> 5;

	if (((PRUint32)mBHead->biWidth * mBHead->biBitCount) & 0x1F) 
		spanBytes++;

	spanBytes <<= 2;

  return(spanBytes);
}

/** ----------------------------------------------------------------
 * Clean up the memory associted with this object.  Has two flavors, 
 * one mode is to clean up everything, the other is to clean up only the DIB information
 * @update dc - 11/20/98
 * @param aWidth - The width to calulate the number of bytes from
 * @return Number of bytes for the span
 */
void nsImageWin :: CleanUp(PRBool aCleanUpAll)
{
	// this only happens when we need to clean up everything
  if (aCleanUpAll == PR_TRUE){
	  if (mHBitmap != nsnull) 
      ::DeleteObject(mHBitmap);
	  if (mAlphaHBitmap != nsnull) 
      ::DeleteObject(mAlphaHBitmap);

    if(mBHead){
      delete[] mBHead;
      mBHead = nsnull;
    }

    mHBitmap = nsnull;
    mAlphaHBitmap = nsnull;

    mIsOptimized = PR_FALSE;
  }

  if (mImageBits != nsnull) {
    delete [] mImageBits;
    mImageBits = nsnull;
  }
  if (mAlphaBits != nsnull) {
    delete [] mAlphaBits;
    mAlphaBits = nsnull;
  }

	// Should be an ISupports, so we can release
  if (mColorMap != nsnull){
    delete [] mColorMap->Index;
    delete mColorMap;
  }

	mNumPaletteColors = -1;
  mNumBytesPixel = 0;
	mSizeImage = 0;
  mImageBits = nsnull;
  mAlphaBits = nsnull;
  mColorMap = nsnull;
}


