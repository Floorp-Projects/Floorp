/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   emk <VYV03354@nifty.ne.jp>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsImageWin.h"
#include "nsRenderingContextWin.h"
#include "nsDeviceContextWin.h"
#include "imgScaler.h"

static nsresult BuildDIB(LPBITMAPINFOHEADER *aBHead, PRInt32 aWidth,
                         PRInt32 aHeight, PRInt32 aDepth, PRInt8 *aNumBitPix);

static PRInt32 GetPlatform()
{
  OSVERSIONINFO versionInfo;


  versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
  ::GetVersionEx(&versionInfo);
  return versionInfo.dwPlatformId;
}


PRInt32 nsImageWin::gPlatform = GetPlatform();


/** ----------------------------------------------------------------
  * Constructor for nsImageWin
  * @update dc - 11/20/98
  */
nsImageWin::nsImageWin()
  : mImageBits(nsnull)
  , mHBitmap(nsnull)
  , mAlphaBits(nsnull)
  , mColorMap(nsnull)
  , mBHead(nsnull)
  , mDIBTemp(PR_FALSE)
  , mNumBytesPixel(0)
  , mNumPaletteColors(0)
  , mSizeImage(0)
  , mRowBytes(0)
  , mIsOptimized(PR_FALSE)
  , mDecodedX1(PR_INT32_MAX)
  , mDecodedY1(PR_INT32_MAX)
  , mDecodedX2(0)
  , mDecodedY2(0)
  , mIsLocked(PR_FALSE)
  , mAlphaDepth(0)
  , mARowBytes(0)
  , mImageCache(0)
  , mInitialized(PR_FALSE)
{
}


/** ----------------------------------------------------------------
  * destructor for nsImageWin
  * @update dc - 11/20/98
  */
nsImageWin :: ~nsImageWin()
{
  CleanUpDDB();
  CleanUpDIB();

  if (mBHead) {
    delete[] mBHead;
    mBHead = nsnull;
  }
  if (mAlphaBits) {
    delete [] mAlphaBits;
    mAlphaBits = nsnull;
  }
}


NS_IMPL_ISUPPORTS1(nsImageWin, nsIImage)


/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *  @update 3/27/00 dwc
 */
nsresult nsImageWin :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
  if (mInitialized)
    return NS_ERROR_FAILURE;

  if (8 == aDepth) {
    mNumPaletteColors = 256;
    mNumBytesPixel = 1;
  } else if (24 == aDepth) {
    mNumBytesPixel = 3;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }


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
 
    // Need to clear the entire buffer so an incrementally loaded image
    // will not have garbage rendered for the unloaded bits.
/* XXX: Since there is a performance hit for doing the clear we need
   a different solution. For now, we will let garbage be drawn for
   incrementally loaded images. Need a solution where only the portion
   of the image that has been loaded is asked to draw.
    if (mImageBits != nsnull) {
      memset(mImageBits, 128, mSizeImage);
    }
*/


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
        //NS_ASSERTION(nsMaskRequirements_kNeeds8Bit == aMaskRequirements,
        // "unexpected mask depth");
        mARowBytes = aWidth;
        mAlphaDepth = 8;
      }


      // 32-bit align each row
      mARowBytes = (mARowBytes + 3) & ~0x3;


      mAlphaBits = new unsigned char[mARowBytes * aHeight];
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

  mInitialized = PR_TRUE;
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *  @update 3/27/00 dwc
 */
void 
nsImageWin :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
  mDecodedX1 = PR_MIN(mDecodedX1, aUpdateRect->x);
  mDecodedY1 = PR_MIN(mDecodedY1, aUpdateRect->y);

  if (aUpdateRect->YMost() > mDecodedY2)
    mDecodedY2 = aUpdateRect->YMost();
  if (aUpdateRect->XMost() > mDecodedX2)
    mDecodedX2 = aUpdateRect->XMost();
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



static void CompositeBitsInMemory(HDC aTheHDC,
                                  int aDX, int aDY,
                                  int aDWidth, int aDHeight,
                                  int aSX, int aSY,
                                  int aSWidth, int aSHeight,
                                  PRInt32 aSrcy,
                                  PRUint8 *aAlphaBits,
                                  MONOBITMAPINFO *aBMI,
                                  PRUint8* aImageBits,
                                  LPBITMAPINFOHEADER aBHead,
                                  PRInt16 aNumPaletteColors);
// Raster op used with MaskBlt(). Assumes our transparency mask has 0 for the
// opaque pixels and 1 for the transparent pixels. That means we want the
// background raster op (value of 0) to be SRCCOPY, and the foreground raster
// (value of 1) to just use the destination bits
#define MASKBLT_ROP MAKEROP4((DWORD)0x00AA0029, SRCCOPY)


void nsImageWin::CreateImageWithAlphaBits(HDC TheHDC)
{
  unsigned char *imageWithAlphaBits;
  ALPHA32BITMAPINFO bmi(mBHead->biWidth, mBHead->biHeight);
  mHBitmap = ::CreateDIBSection(TheHDC, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS,
                                (LPVOID *)&imageWithAlphaBits, NULL, 0);


  if (!mHBitmap) {
    mIsOptimized = PR_FALSE;
    return;
  }
  
  if (256 == mNumPaletteColors) {
    for (int y = 0; y < mBHead->biHeight; y++) {
      unsigned char *imageWithAlphaRow = imageWithAlphaBits + y * mBHead->biWidth * 4;
      unsigned char *imageRow = mImageBits + y * mRowBytes;
      unsigned char *alphaRow = mAlphaBits + y * mARowBytes;


      for (int x = 0; x < mBHead->biWidth;
           x++, imageWithAlphaRow += 4, imageRow++, alphaRow++) {
        FAST_DIVIDE_BY_255(imageWithAlphaRow[0],
                           mColorMap->Index[3 * (*imageRow)] * *alphaRow);
        FAST_DIVIDE_BY_255(imageWithAlphaRow[1],
                           mColorMap->Index[3 * (*imageRow) + 1] * *alphaRow);
        FAST_DIVIDE_BY_255(imageWithAlphaRow[2],
                           mColorMap->Index[3 * (*imageRow) + 2] * *alphaRow);
        imageWithAlphaRow[3] = *alphaRow;
      }
    }
  } else {
    for (int y = 0; y < mBHead->biHeight; y++) {
      unsigned char *imageWithAlphaRow = imageWithAlphaBits + y * mBHead->biWidth * 4;
      unsigned char *imageRow = mImageBits + y * mRowBytes;
      unsigned char *alphaRow = mAlphaBits + y * mARowBytes;


      for (int x = 0; x < mBHead->biWidth;
          x++, imageWithAlphaRow += 4, imageRow += 3, alphaRow++) {
        FAST_DIVIDE_BY_255(imageWithAlphaRow[0], imageRow[0] * *alphaRow);
        FAST_DIVIDE_BY_255(imageWithAlphaRow[1], imageRow[1] * *alphaRow);
        FAST_DIVIDE_BY_255(imageWithAlphaRow[2], imageRow[2] * *alphaRow);
        imageWithAlphaRow[3] = *alphaRow;
      }
    }
  }
  mIsOptimized = PR_TRUE;
}


/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *  @update 3/27/00 dwc
 */
void 
nsImageWin :: CreateDDB(nsIDrawingSurface* aSurface)
{
  HDC TheHDC;


  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);


  if (TheHDC != NULL){
    // Temporary fix for bug 135226 until we do better decode-time
    // dithering and paletized storage of images. Bail on the 
    // optimization to DDB if we're on a paletted device.
    int rasterCaps = ::GetDeviceCaps(TheHDC, RASTERCAPS);
    if (RC_PALETTE == (rasterCaps & RC_PALETTE)) {
      ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
      return;
    }
    
    if (mSizeImage > 0){
       if (mAlphaDepth == 8) {
         CreateImageWithAlphaBits(TheHDC);
       } else {
         mHBitmap = ::CreateDIBitmap(TheHDC, mBHead, CBM_INIT, mImageBits,
                                     (LPBITMAPINFO)mBHead,
                                     256 == mNumPaletteColors ? DIB_PAL_COLORS : DIB_RGB_COLORS);
         mIsOptimized = (mHBitmap != 0);
       }
      if (mIsOptimized)
        CleanUpDIB();
    }
    ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
  }
}

/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *  @update 3/27/00 dwc
 */
NS_IMETHODIMP 
nsImageWin::Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  if (mBHead == nsnull || 
      aSWidth < 0 || aDWidth < 0 || aSHeight < 0 || aDHeight < 0)
    return NS_ERROR_FAILURE;

  if (0 == aSWidth || 0 == aDWidth || 0 == aSHeight || 0 == aDHeight)
    return NS_OK;

  if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
    return NS_OK;

  PRInt32 origSHeight = aSHeight, origDHeight = aDHeight;
  PRInt32 origSWidth = aSWidth, origDWidth = aDWidth;

  // limit the size of the blit to the amount of the image read in
  if (aSX + aSWidth > mDecodedX2) {
    aDWidth -= ((aSX + aSWidth - mDecodedX2) * origDWidth) / origSWidth;
    aSWidth -= (aSX + aSWidth) - mDecodedX2;
  }
  if (aSX < mDecodedX1) {
    aDX += ((mDecodedX1 - aSX) * origDWidth) / origSWidth;
    aSX = mDecodedX1;
  }

  if (aSY + aSHeight > mDecodedY2) {
    aDHeight -= ((aSY + aSHeight - mDecodedY2) * origDHeight) / origSHeight;
    aSHeight -= (aSY + aSHeight) - mDecodedY2;
  }
  if (aSY < mDecodedY1) {
    aDY += ((mDecodedY1 - aSY) * origDHeight) / origSHeight;
    aSY = mDecodedY1;
  }

  if (aDWidth <= 0 || aDHeight <= 0)
    return NS_OK;

  // Translate to bottom-up coordinates for the source bitmap
  PRInt32 srcy = mBHead->biHeight - (aSY + aSHeight);

  HDC     TheHDC;
  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);
  if (!TheHDC)
    return NS_ERROR_FAILURE;

  // find out if the surface is a printer.
  PRInt32 canRaster;
  ((nsDrawingSurfaceWin *)aSurface)->GetTECHNOLOGY(&canRaster);

  PRBool didComposite = PR_FALSE;
  if (!mIsOptimized || !mHBitmap) {
    DWORD rop = SRCCOPY;

    if (mAlphaBits) {
      if (1 == mAlphaDepth) {
        MONOBITMAPINFO  bmi(mBHead->biWidth, mBHead->biHeight);

        if (canRaster == DT_RASPRINTER) {
          CompositeBitsInMemory(TheHDC, aDX, aDY, aDWidth, aDHeight,
                                aSX, aSY, aSWidth, aSHeight,
                                srcy, mAlphaBits, &bmi, mImageBits, mBHead,
                                mNumPaletteColors);
          didComposite = PR_TRUE;
        } else {
          // Put the mask down
          ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,
                          aSX, srcy, aSWidth, aSHeight, mAlphaBits,
                          (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
          rop = SRCPAINT;
        }
      } else if (8 == mAlphaDepth) {
        nsresult rv = DrawComposited(TheHDC, aDX, aDY, aDWidth, aDHeight,
                                     aSX, srcy, aSWidth, aSHeight,
                                     origDWidth, origDHeight);
        if (NS_FAILED(rv)) {
          ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
          return rv;
        }
        didComposite = PR_TRUE;
      }
    } // mAlphaBits

    // Put the Image down
    if (!didComposite) {
      ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,
                      aSX, srcy, aSWidth, aSHeight, mImageBits,
                      (LPBITMAPINFO)mBHead, 256 == mNumPaletteColors ? 
                      DIB_PAL_COLORS : DIB_RGB_COLORS, rop);
    }
  } else {
    // Optimized.  mHBitmap contains the DDB
    DWORD rop = SRCCOPY;

    if (canRaster == DT_RASPRINTER) {
      // To Printer
      if (mAlphaBits && mAlphaDepth == 1) {
        MONOBITMAPINFO  bmi(mBHead->biWidth, mBHead->biHeight);
        if (mImageBits) {
          CompositeBitsInMemory(TheHDC, aDX, aDY, aDWidth, aDHeight,
                                aSX, aSY, aSWidth, aSHeight, srcy,
                                mAlphaBits, &bmi, mImageBits, mBHead,
                                mNumPaletteColors);
          didComposite = PR_TRUE;  
        } else {
          ConvertDDBtoDIB(); // Create mImageBits
          if (mImageBits) {  
            CompositeBitsInMemory(TheHDC, aDX, aDY, aDWidth, aDHeight,
                                  aSX, aSY, aSWidth, aSHeight, srcy,
                                  mAlphaBits,
                                  &bmi, mImageBits, mBHead,
                                  mNumPaletteColors);
            // Clean up the image bits    
            delete [] mImageBits;
            mImageBits = nsnull;
            didComposite = PR_TRUE;         
          } else {
            NS_WARNING("Could not composite bits in memory because conversion to DIB failed\n");
          }
        } // mImageBits
      } // mAlphaBits && mAlphaDepth == 1

      if (!didComposite && 
          (GetDeviceCaps(TheHDC, RASTERCAPS) & (RC_BITBLT | RC_STRETCHBLT)))
        PrintDDB(aSurface, aDX, aDY, aDWidth, aDHeight,
                 aSX, srcy, aSWidth, aSHeight, rop);

    } else { 
      // we are going to the device that created this DDB

      // Get srcDC from the drawing surface of aContext's (RenderingContext)
      // DeviceContext.  Should be the same each time.
      nsIDeviceContext    *dx;
      aContext.GetDeviceContext(dx);
      
      nsIDrawingSurface*     ds;
      NS_STATIC_CAST(nsDeviceContextWin*, dx)->GetDrawingSurface(aContext, ds);

      nsDrawingSurfaceWin *srcDS = (nsDrawingSurfaceWin *)ds;
      if (!srcDS) {
        NS_RELEASE(dx);
        ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
        return NS_ERROR_FAILURE;
      }      

      HDC                 srcDC;
      srcDS->GetDC(&srcDC);

      // Draw the Alpha/Mask
      if (mAlphaBits && mAlphaDepth == 1) {
        MONOBITMAPINFO  bmi(mBHead->biWidth, mBHead->biHeight);
        ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,
                        aSX, srcy, aSWidth, aSHeight, mAlphaBits,
                        (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
        rop = SRCPAINT;
      }

      // Draw the Image
      HBITMAP oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
      if (8 == mAlphaDepth) {
        BLENDFUNCTION blendFunction;
        blendFunction.BlendOp = AC_SRC_OVER;
        blendFunction.BlendFlags = 0;
        blendFunction.SourceConstantAlpha = 255;
        blendFunction.AlphaFormat = 1;  // AC_SRC_ALPHA
        gAlphaBlend(TheHDC, aDX, aDY, aDWidth, aDHeight, 
                    srcDC, aSX, aSY, aSWidth, aSHeight, blendFunction);
      } else { 
        ::StretchBlt(TheHDC, aDX, aDY, aDWidth, aDHeight, 
                     srcDC, aSX, aSY, aSWidth, aSHeight, rop);
      }
      ::SelectObject(srcDC, oldBits);
      srcDS->ReleaseDC();
      NS_RELEASE(dx);
    }
  }
  ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();

  return NS_OK;
}

/** ---------------------------------------------------
 *  This is a helper routine to do the blending for the DrawComposited method
 *  @update 1/04/02 dwc
 */
void nsImageWin::DrawComposited24(unsigned char *aBits,
                                  PRUint8 *aImageRGB, PRUint32 aStrideRGB,
                                  PRUint8 *aImageAlpha, PRUint32 aStrideAlpha,
                                  int aWidth, int aHeight)
{
  PRInt32 targetRowBytes = ((aWidth * 3) + 3) & ~3;
  for (int y = 0; y < aHeight; y++) {
    unsigned char *targetRow = aBits + y * targetRowBytes;
    unsigned char *imageRow = aImageRGB + y * aStrideRGB;
    unsigned char *alphaRow = aImageAlpha + y * aStrideAlpha;

    for (int x = 0; x < aWidth;
         x++, targetRow += 3, imageRow += 3, alphaRow++) {
      unsigned alpha = *alphaRow;
      MOZ_BLEND(targetRow[0], targetRow[0], imageRow[0], alpha);
      MOZ_BLEND(targetRow[1], targetRow[1], imageRow[1], alpha);
      MOZ_BLEND(targetRow[2], targetRow[2], imageRow[2], alpha);
    }
  }
}


/** ---------------------------------------------------
 *  Blend the image into a 24 bit buffer.. using an 8 bit alpha mask 
 *  @update 1/04/02 dwc
 */
nsresult nsImageWin::DrawComposited(HDC TheHDC, int aDX, int aDY,
                                    int aDWidth, int aDHeight,
                                    int aSX, int aSY, int aSWidth, int aSHeight,
                                    int aOrigDWidth, int aOrigDHeight)
{
  HDC memDC = ::CreateCompatibleDC(TheHDC);
  if (!memDC)
    return NS_ERROR_OUT_OF_MEMORY;
  unsigned char *screenBits;

  PRBool scaling = PR_FALSE;
  /* Both scaled and unscaled images come through this code */
  if ((aDWidth != aSWidth) || (aDHeight != aSHeight)) {
    scaling = PR_TRUE;
    aDWidth = aOrigDWidth;
    aDHeight = aOrigDHeight;
    aSWidth = mBHead->biWidth;
    aSHeight = mBHead->biHeight;
  }

  ALPHA24BITMAPINFO bmi(aDWidth, aDHeight);
  HBITMAP tmpBitmap = ::CreateDIBSection(memDC, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS,
                                         (LPVOID *)&screenBits, NULL, 0);
  if (!tmpBitmap) {
    ::DeleteDC(memDC);
    NS_WARNING("nsImageWin::DrawComposited failed to create tmpBitmap\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }
  HBITMAP oldBitmap = (HBITMAP)::SelectObject(memDC, tmpBitmap);
  if (!oldBitmap || oldBitmap == (HBITMAP)GDI_ERROR) {
    ::DeleteObject(tmpBitmap);
    ::DeleteDC(memDC);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  /* Copy from the HDC */
  BOOL retval = ::BitBlt(memDC, 0, 0, aDWidth, aDHeight,
                         TheHDC, aDX, aDY, SRCCOPY);
  if (!retval) {
    /* select the old object again... */
    ::SelectObject(memDC, oldBitmap);
    ::DeleteObject(tmpBitmap);
    ::DeleteDC(memDC);
    return NS_ERROR_FAILURE;
  }

  PRUint8 *imageRGB, *imageAlpha;
  PRUint32 strideRGB, strideAlpha;

  if (scaling) {
    /* Scale our image to match */
    imageRGB = (PRUint8 *)nsMemory::Alloc(3*aDWidth*aDHeight);
    imageAlpha = (PRUint8 *)nsMemory::Alloc(aDWidth*aDHeight);

    if (!imageRGB || !imageAlpha) {
      if (imageRGB)
        nsMemory::Free(imageRGB);
      if (imageAlpha)
        nsMemory::Free(imageAlpha);
      ::SelectObject(memDC, oldBitmap);
      ::DeleteObject(tmpBitmap);
      ::DeleteDC(memDC);
      return NS_ERROR_FAILURE;
    }

    strideRGB = 3 * aDWidth;
    strideAlpha = aDWidth;
    RectStretch(aSWidth, aSHeight, aDWidth, aDHeight,
                0, 0, aDWidth-1, aDHeight-1,
                mImageBits, mRowBytes, imageRGB, strideRGB, 24);
    RectStretch(aSWidth, aSHeight, aDWidth, aDHeight,
                0, 0, aDWidth-1, aDHeight-1,
                mAlphaBits, mARowBytes, imageAlpha, strideAlpha, 8);
  } else {
    imageRGB = mImageBits + aSY * mRowBytes + aSX * 3;
    imageAlpha = mAlphaBits + aSY * mARowBytes + aSX;
    strideRGB = mRowBytes;
    strideAlpha = mARowBytes;
  }

  /* Do composite */
  DrawComposited24(screenBits, imageRGB, strideRGB, imageAlpha, strideAlpha,
                   aDWidth, aDHeight);

  if (scaling) {
    /* Free scaled images */
    nsMemory::Free(imageRGB);
    nsMemory::Free(imageAlpha);
  }

  /* Copy back to the HDC */
  /* Use StretchBlt instead of BitBlt here so that we get proper dithering */
  if (scaling) {
    /* only copy back the valid portion of the image */
    retval = ::StretchBlt(TheHDC, aDX, aDY,
                          aDWidth, (mDecodedY2*aDHeight + aSHeight - 1)/aSHeight,
                          memDC, 0, 0,
                          aDWidth, (mDecodedY2*aDHeight + aSHeight - 1)/aSHeight,
                          SRCCOPY);
  } else {
    retval = ::StretchBlt(TheHDC, aDX, aDY, aDWidth, aDHeight,
                          memDC, 0, 0, aDWidth, aDHeight, SRCCOPY);
  }

  if (!retval) {
    ::SelectObject(memDC, oldBitmap);
    ::DeleteObject(tmpBitmap);
    ::DeleteDC(memDC);
    return NS_ERROR_FAILURE;
  }

  /* we're done, ignore possible further errors */
  ::SelectObject(memDC, oldBitmap);
  ::DeleteObject(tmpBitmap);
  ::DeleteDC(memDC);
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h
 *  @update 3/27/00 dwc
 */
NS_IMETHODIMP nsImageWin :: Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
         PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return Draw(aContext, aSurface, 0, 0, mBHead->biWidth, mBHead->biHeight, aX, aY, aWidth, aHeight);
}


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
NS_IMETHODIMP nsImageWin::DrawTile(nsIRenderingContext &aContext,
                                   nsIDrawingSurface* aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   PRInt32 aPadX, PRInt32 aPadY,
                                   const nsRect &aDestRect)
{
  NS_ASSERTION(!aDestRect.IsEmpty(), "DrawTile doesn't work with empty rects");
  if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
    return NS_OK;

  float           scale;
  unsigned char   *targetRow,*imageRow,*alphaRow;
  PRInt32         x0, y0, x1, y1, destScaledWidth, destScaledHeight;
  PRInt32         validWidth,validHeight,validX,validY,targetRowBytes;
  PRInt32         x,y,width,height,canRaster;
  nsCOMPtr<nsIDeviceContext> theDeviceContext;
  HDC             theHDC;
  nscoord         ScaledTileWidth,ScaledTileHeight;
  PRBool          padded = (aPadX || aPadY);

  ((nsDrawingSurfaceWin *)aSurface)->GetTECHNOLOGY(&canRaster);
  aContext.GetDeviceContext(*getter_AddRefs(theDeviceContext));

  // We can Progressive Double Blit if we aren't printing to a printer, and
  // we aren't a 256 color image, and we don't have an unoptimized 8 bit alpha.
  if ((canRaster != DT_RASPRINTER) && (256 != mNumPaletteColors) &&
      !(mAlphaDepth == 8 && !mIsOptimized) && !padded)
    if (ProgressiveDoubleBlit(theDeviceContext, aSurface,
                              aSXOffset, aSYOffset, aDestRect))
      return NS_OK;
  theDeviceContext->GetCanonicalPixelScale(scale);

  destScaledWidth  = PR_MAX(PRInt32(mBHead->biWidth*scale), 1);
  destScaledHeight = PR_MAX(PRInt32(mBHead->biHeight*scale), 1);
  
  validX = 0;
  validY = 0;
  validWidth  = mBHead->biWidth;
  validHeight = mBHead->biHeight;
  
  // limit the image rectangle to the size of the image data which
  // has been validated.
  if (mDecodedY2 < mBHead->biHeight) {
    validHeight = mDecodedY2 - mDecodedY1;
    destScaledHeight = PR_MAX(PRInt32(validHeight*scale), 1);
  }
  if (mDecodedX2 < mBHead->biWidth) {
    validWidth = mDecodedX2 - mDecodedX1;
    destScaledWidth = PR_MAX(PRInt32(validWidth*scale), 1);
  }
  if (mDecodedY1 > 0) {   
    validHeight -= mDecodedY1;
    destScaledHeight = PR_MAX(PRInt32(validHeight*scale), 1);
    validY = mDecodedY1;
  }
  if (mDecodedX1 > 0) {
    validWidth -= mDecodedX1;
    destScaledWidth = PR_MAX(PRInt32(validWidth*scale), 1);
    validX = mDecodedX1; 
  }

  // put the DestRect into absolute coordintes of the device
  y0 = aDestRect.y - aSYOffset;
  x0 = aDestRect.x - aSXOffset;
  y1 = aDestRect.y + aDestRect.height;
  x1 = aDestRect.x + aDestRect.width;


  // this is the width and height of the image in pixels
  // we need to map this to the pixel height of the device
  ScaledTileWidth = PR_MAX(PRInt32(mBHead->biWidth*scale), 1);
  ScaledTileHeight = PR_MAX(PRInt32(mBHead->biHeight*scale), 1);

  // do alpha depth equal to 8 here.. this needs some special attention
  if (mAlphaDepth == 8 && !mIsOptimized && !padded) {
    unsigned char *screenBits=nsnull,*adjAlpha,*adjImage,*adjScreen;
    HDC           memDC=nsnull;
    HBITMAP       tmpBitmap=nsnull,oldBitmap;
    unsigned char alpha;
    PRInt32       targetBytesPerPixel,imageBytesPerPixel;

    if (!mImageBits) {
      ConvertDDBtoDIB();
    }

    // draw the alpha and the bitmap to an offscreen buffer.. for the blend.. first 
    ((nsDrawingSurfaceWin *)aSurface)->GetDC(&theHDC);
    if (theHDC) {
    // create a buffer for the blend            
      memDC = CreateCompatibleDC(theHDC);
      width = aDestRect.width;
      height = aDestRect.height;

      ALPHA24BITMAPINFO bmi(width, height);
      tmpBitmap = ::CreateDIBSection(memDC, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, (LPVOID *)&screenBits, NULL, 0);
      oldBitmap = (HBITMAP)::SelectObject(memDC, tmpBitmap);

      // number of bytes in a row on a 32 bit boundary
      targetRowBytes = (bmi.bmiHeader.biWidth * bmi.bmiHeader.biBitCount) >> 5;  // number of 32 bit longs
      if (((PRUint32)bmi.bmiHeader.biWidth * bmi.bmiHeader.biBitCount) & 0x1F) { // make sure its a multiple of 32
        targetRowBytes++;     // or else there will not be enough bytes per line
      }
      targetRowBytes <<= 2;   // divide by 4 to get the number of bytes from the number of 32 bit longs

      targetBytesPerPixel = bmi.bmiHeader.biBitCount/8;
    }

    if (!tmpBitmap) {
      if (memDC) {
        ::DeleteDC(memDC);
      }
      // this failed..and will fall into the slow blitting code
      NS_WARNING("The Creation of the tmpBitmap failed \n");
    } else {
      // Copy from the HDC to the memory DC
      // this will be the image on the screen into a buffer for the blend.
      ::StretchBlt(memDC, 0, 0, width, height,theHDC, aDestRect.x, aDestRect.y, width, height, SRCCOPY);
  
      imageBytesPerPixel = mBHead->biBitCount/8;

      // windows bitmaps start at the bottom.. and go up.  This messes up the offsets for the
      // image and tiles.. so I reverse the the direction.. to go (the normal way) from top to bottom.
      adjScreen = screenBits + ((height-1) * targetRowBytes);
      adjImage = mImageBits + ((validHeight-1) * mRowBytes);
      adjAlpha = mAlphaBits + ((validHeight-1) * mARowBytes);


      for (int y = 0,byw=aSYOffset; y < height; y++,byw++) {

        if (byw >= ScaledTileHeight) {
          byw = 0;
        }

        targetRow = adjScreen - (y * targetRowBytes);
        imageRow = adjImage - (byw * mRowBytes);
        alphaRow = adjAlpha - (byw * mARowBytes);

        // we only need this adjustment at the beginning of each row
        imageRow += (aSXOffset*imageBytesPerPixel);
        alphaRow += aSXOffset;

        for (int x=0,bxw=aSXOffset;x<width;x++,targetRow+=targetBytesPerPixel,imageRow+=imageBytesPerPixel,bxw++, alphaRow++) {
          // if we went past the row width of our buffer.. go back and start again
          if (bxw>=ScaledTileWidth) {
            bxw = 0;
            imageRow = adjImage - (byw * mRowBytes);
            alphaRow = adjAlpha - (byw * mARowBytes);
          }

          alpha = *alphaRow;

          MOZ_BLEND(targetRow[0], targetRow[0], imageRow[0], alpha);
          MOZ_BLEND(targetRow[1], targetRow[1], imageRow[1], alpha);
          MOZ_BLEND(targetRow[2], targetRow[2], imageRow[2], alpha);
        }
      }

      // copy the blended image back to the screen
      ::StretchBlt(theHDC, aDestRect.x, aDestRect.y, width, height,memDC, 0, 0, width, height, SRCCOPY);

      ::SelectObject(memDC, oldBitmap);
      ::DeleteObject(tmpBitmap);
      ::DeleteDC(memDC);
  
    return(NS_OK);
    } 
  }

  // if we got to this point.. everything else failed.. and the slow blit backstop
  // will finish this tiling
  for (y=y0;y<y1;y+=ScaledTileHeight+aPadY*scale) {
    for (x=x0;x<x1;x+=ScaledTileWidth+aPadX*scale) {
    Draw(aContext, aSurface,
         0, 0, PR_MIN(validWidth, x1-x), PR_MIN(validHeight, y1-y),
         x, y, PR_MIN(destScaledWidth, x1-x), PR_MIN(destScaledHeight, y1-y));
    }
  } 
  return(NS_OK);
}

/** ---------------------------------------------------
 *  See documentation in nsImageWin.h
 */
PRBool
nsImageWin::ProgressiveDoubleBlit(nsIDeviceContext *aContext,
                                  nsIDrawingSurface* aSurface,
                                  PRInt32 aSXOffset, PRInt32 aSYOffset,
                                  nsRect aDestRect)
{
  /*
    (aSXOffset, aSYOffset) is the offset into our image that we want to start
    drawing from.  We start drawing at (aDestRect.x, aDestRect.y)

    - Find first full tile
    - progressivly double blit until end of tile area.  What's left will be at
      maximum a strip on the top and a strip on the left side that has not been
      blitted.
    Then, in no particular order:
    - blit the top strip using a row we already put down
    - blit the left strip using a column we already put down
    - blit the very topleft, since it will be left out
  */

  HDC theHDC;
  void *screenBits; // We never use the bits, but we need to pass a variable in
                    // to create a DIB

  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&theHDC);
  if (!theHDC)
    return PR_FALSE;

  // create an imageDC
  HDC imgDC = ::CreateCompatibleDC(theHDC);
  if (!imgDC) {
    ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
    return PR_FALSE;
  }
  
  nsPaletteInfo palInfo;
  aContext->GetPaletteInfo(palInfo);
  if (palInfo.isPaletteDevice && palInfo.palette) {
    ::SetStretchBltMode(imgDC, HALFTONE);
    ::SelectPalette(imgDC, (HPALETTE)palInfo.palette, TRUE);
    ::RealizePalette(imgDC);
  }

  // Create a maskDC, and fill it with mAlphaBits
  HDC maskDC = nsnull;
  HBITMAP oldImgMaskBits = nsnull;
  HBITMAP maskBits;
  HBITMAP mTmpHBitmap = nsnull;
  if (mAlphaDepth == 1) {
    maskDC = ::CreateCompatibleDC(theHDC);
    if (!maskDC) {
      ::DeleteDC(imgDC);
      ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
      return PR_FALSE;
    }

    MONOBITMAPINFO bmi(mBHead->biWidth, mBHead->biHeight);
    maskBits  = ::CreateDIBSection(theHDC, (LPBITMAPINFO)&bmi,
                                   DIB_RGB_COLORS, &screenBits, NULL, 0);
    if (!maskBits) {
      ::DeleteDC(imgDC);
      ::DeleteDC(maskDC);
      ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
      return PR_FALSE;
    }

    oldImgMaskBits = (HBITMAP)::SelectObject(maskDC, maskBits);
    ::SetDIBitsToDevice(maskDC, 0, 0, mBHead->biWidth, mBHead->biHeight,
                        0, 0, 0, mBHead->biHeight, mAlphaBits,
                        (LPBITMAPINFO)&bmi, DIB_RGB_COLORS);
  }


  // Fill imageDC with our image
  HBITMAP oldImgBits = nsnull;
  if (!mIsOptimized || !mHBitmap) {
    // Win 95/98/ME can't do > 0xFF0000 DDBs.
    if (gPlatform == VER_PLATFORM_WIN32_WINDOWS) {
      int bytesPerPix = ::GetDeviceCaps(imgDC, BITSPIXEL) / 8;
      if (mBHead->biWidth * mBHead->biHeight * bytesPerPix > 0xFF0000) {
        ::DeleteDC(imgDC);
        if (maskDC) {
          if (oldImgMaskBits)
            ::SelectObject(maskDC, oldImgMaskBits);
          ::DeleteObject(maskBits);
          ::DeleteDC(maskDC);
        }
        ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
        return PR_FALSE;
      }
    }
    mTmpHBitmap = ::CreateCompatibleBitmap(theHDC, mBHead->biWidth,
                                           mBHead->biHeight);
    if (!mTmpHBitmap) {
      ::DeleteDC(imgDC);
      if (maskDC) {
        if (oldImgMaskBits)
          ::SelectObject(maskDC, oldImgMaskBits);
        ::DeleteObject(maskBits);
        ::DeleteDC(maskDC);
      }
      ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
      return PR_FALSE;
    }
    oldImgBits = (HBITMAP)::SelectObject(imgDC, mTmpHBitmap);
    ::StretchDIBits(imgDC, 0, 0, mBHead->biWidth, mBHead->biHeight,
                    0, 0, mBHead->biWidth, mBHead->biHeight,
                    mImageBits, (LPBITMAPINFO)mBHead,
                    256 == mNumPaletteColors ? DIB_PAL_COLORS : DIB_RGB_COLORS,
                    SRCCOPY);
  } else {
    oldImgBits = (HBITMAP)::SelectObject(imgDC, mHBitmap);
  }

  PRBool result = PR_TRUE;
  PRBool useAlphaBlend = mAlphaDepth == 8 && mIsOptimized;

  PRInt32 firstWidth = mBHead->biWidth - aSXOffset;
  PRInt32 firstHeight = mBHead->biHeight - aSYOffset;

  if (aDestRect.width > firstWidth + mBHead->biWidth ||
      aDestRect.height > firstHeight + mBHead->biHeight) {
    PRInt32 firstPartialWidth = aSXOffset == 0 ? 0 : firstWidth;
    PRInt32 firstPartialHeight = aSYOffset == 0 ? 0 : firstHeight;
    PRBool hasFullImageWidth = aDestRect.width - firstPartialWidth >= mBHead->biWidth;
    PRBool hasFullImageHeight = aDestRect.height - firstPartialHeight >= mBHead->biHeight;

    HDC dstDC = theHDC;               // Defaulting to drawing to theHDC
    HDC dstMaskDC = nsnull;           // No Alpha DC by default
    HBITMAP dstMaskHBitmap = nsnull;  // No Alpha HBITMAP by default either..
    HBITMAP oldDstBits, oldDstMaskBits;
    HBITMAP dstHBitmap;

    do {  // Use a do to help with cleanup
      nsRect storedDstRect;

      // dstDC will only be used if we have an alpha.  It is not needed for
      // images without an alpha because we can directly doubleblit onto theHDC
      if (mAlphaDepth != 0) {
        // We have an alpha, which means we can't p. doubleblit directly onto
        // theHDC.  We must create a temporary DC, p. doubleblit into that, and
        // then draw the whole thing to theHDC

        storedDstRect = aDestRect;
        aDestRect.x = 0;
        aDestRect.y = 0;
        // The last p. doubleblit is very slow.. it's faster to draw to theHDC
        // twice.  So only progressive doubleblit up to 1/2 of the tiling area
        // that requires full tiles.
        if (aDestRect.height - firstPartialHeight >= mBHead->biHeight * 2)
          aDestRect.height = PR_ROUNDUP(PRInt32(ceil(aDestRect.height / 2.0)),
                                        mBHead->biHeight)
                             + firstPartialHeight;

        // Win 95/98/ME can't do > 0xFF0000 DDBs.
        // Note: This should be extremely rare.  We'd need a tile area (before
        //       it was split in 1/2 above) of 2048x4080 @ 4 bytesPerPix before
        //       we hit this.
        if (gPlatform == VER_PLATFORM_WIN32_WINDOWS) {
          int bytesPerPix = (mAlphaDepth == 8) ? 4 :
                            ::GetDeviceCaps(theHDC, BITSPIXEL) / 8;
          if (aDestRect.width * aDestRect.height * bytesPerPix > 0xFF0000) {
            result = PR_FALSE;
            break;
          }
        }

        // make a temporary offscreen DC
        dstDC = ::CreateCompatibleDC(theHDC);
        if (!dstDC) {
          result = PR_FALSE;
          break;
        }

        // Create HBITMAP for the new DC
        if (mAlphaDepth == 8) {
          // Create a bitmap of 1 plane, 32 bit color depth
          // Can't use ::CreateBitmap, since the resulting HBITMAP will only be
          // able to be selected into a compatible HDC (ie. User is @ 24 bit, you
          // can't select the 32 HBITMAP into the HDC.)
          ALPHA32BITMAPINFO bmi(aDestRect.width, aDestRect.height);
          dstHBitmap = ::CreateDIBSection(theHDC, (LPBITMAPINFO)&bmi,
                                          DIB_RGB_COLORS, &screenBits, NULL, 0);
        } else {
          // Create a normal bitmap for the image, and a monobitmap for alpha
          dstHBitmap = ::CreateCompatibleBitmap(theHDC, aDestRect.width,
                                                aDestRect.height);
          if (dstHBitmap) {
            dstMaskDC = ::CreateCompatibleDC(theHDC);
            if (dstMaskDC) {
              MONOBITMAPINFO bmi(aDestRect.width, aDestRect.height);
              dstMaskHBitmap = ::CreateDIBSection(theHDC, (LPBITMAPINFO)&bmi,
                                                  DIB_RGB_COLORS, &screenBits,
                                                  NULL, 0);
              if (dstMaskHBitmap) {
                oldDstMaskBits = (HBITMAP)::SelectObject(dstMaskDC,
                                                         dstMaskHBitmap);
              } else {
                result = PR_FALSE;
                break;
              }
            } // dstMaskDC
          } // dstHBitmap
        }
        if (!dstHBitmap) {
          result = PR_FALSE;
          break;
        }

        oldDstBits = (HBITMAP)::SelectObject(dstDC, dstHBitmap);
      } // (mAlphaDepth != 0)


      PRInt32 imgX = hasFullImageWidth ? 0 : aSXOffset;
      PRInt32 imgW = PR_MIN(mBHead->biWidth - imgX, aDestRect.width);
      PRInt32 imgY = hasFullImageHeight ? 0 : aSYOffset;
      PRInt32 imgH = PR_MIN(mBHead->biHeight - imgY, aDestRect.height);

      // surfaceDestPoint is the first position in the destination where we will
      // be putting (0,0) of our image down.
      nsPoint surfaceDestPoint(aDestRect.x, aDestRect.y);
      if (aSXOffset != 0 && hasFullImageWidth)
        surfaceDestPoint.x += firstWidth;
      if (aSYOffset != 0 && hasFullImageHeight)
        surfaceDestPoint.y += firstHeight;

      // Plunk one down
      BlitImage(dstDC, dstMaskDC,
                surfaceDestPoint.x, surfaceDestPoint.y, imgW, imgH,
                imgDC, maskDC, imgX, imgY, PR_FALSE);
      if (!hasFullImageHeight && imgH != aDestRect.height) {
        // Since we aren't drawing a full image in height, there's an area above
        // that we can blit to too.
        BlitImage(dstDC, dstMaskDC, surfaceDestPoint.x, surfaceDestPoint.y + imgH,
                          imgW, aDestRect.height - imgH,
                  imgDC, maskDC, imgX, 0, PR_FALSE);
        imgH = aDestRect.height;
      }
      if (!hasFullImageWidth && imgW != aDestRect.width) {
        BlitImage(dstDC, dstMaskDC, surfaceDestPoint.x + imgW, surfaceDestPoint.y,
                          aDestRect.width - imgW, imgH,
                  imgDC, maskDC, 0, imgY, PR_FALSE);
        imgW = aDestRect.width;
      }


      nsRect surfaceDestRect;
      nsRect surfaceSrcRect(surfaceDestPoint.x, surfaceDestPoint.y, imgW, imgH);

      // Progressively DoubleBlit a Row
      if (hasFullImageWidth) {
        while (surfaceSrcRect.XMost() < aDestRect.XMost()) {
          surfaceDestRect.x = surfaceSrcRect.XMost();
          surfaceDestRect.width = surfaceSrcRect.width;
          if (surfaceDestRect.XMost() >= aDestRect.XMost())
            surfaceDestRect.width = aDestRect.XMost() - surfaceDestRect.x;

          BlitImage(dstDC, dstMaskDC, surfaceDestRect.x, surfaceSrcRect.y,
                            surfaceDestRect.width, surfaceSrcRect.height,
                    dstDC, dstMaskDC, surfaceSrcRect.x, surfaceSrcRect.y,
                    PR_FALSE);
          surfaceSrcRect.width += surfaceDestRect.width;
        }
      }

      // Progressively DoubleBlit a column
      if (hasFullImageHeight) {
        while (surfaceSrcRect.YMost() < aDestRect.YMost()) {
          surfaceDestRect.y = surfaceSrcRect.YMost();
          surfaceDestRect.height = surfaceSrcRect.height;
          if (surfaceDestRect.YMost() >= aDestRect.YMost())
            surfaceDestRect.height = aDestRect.YMost() - surfaceDestRect.y;

          BlitImage(dstDC, dstMaskDC, surfaceSrcRect.x, surfaceDestRect.y,
                            surfaceSrcRect.width, surfaceDestRect.height,
                    dstDC, dstMaskDC, surfaceSrcRect.x, surfaceSrcRect.y,
                    PR_FALSE);
          surfaceSrcRect.height += surfaceDestRect.height;
        }
      }

      // blit to topleft.  This could be done before p doubleblitting
      if (surfaceDestPoint.y != aDestRect.y && surfaceDestPoint.x != aDestRect.x)
          BlitImage(dstDC, dstMaskDC, aDestRect.x, aDestRect.y,
                    firstWidth, firstHeight,
                    dstDC, dstMaskDC,
                    surfaceDestPoint.x + aSXOffset, surfaceDestPoint.y + aSYOffset,
                    PR_FALSE);
      // blit top row
      if (surfaceDestPoint.y != aDestRect.y)
          BlitImage(dstDC, dstMaskDC, surfaceDestPoint.x, aDestRect.y,
                    surfaceSrcRect.width, firstHeight,
                    dstDC, dstMaskDC, surfaceDestPoint.x, surfaceDestPoint.y + aSYOffset,
                    PR_FALSE);

      // blit left column
      if (surfaceDestPoint.x != aDestRect.x)
        BlitImage(dstDC, dstMaskDC, aDestRect.x, surfaceDestPoint.y,
                  firstWidth, surfaceSrcRect.height,
                  dstDC, dstMaskDC, surfaceDestPoint.x + aSXOffset, surfaceDestPoint.y,
                  PR_FALSE);

      if (mAlphaDepth != 0) {
        // blit temporary dstDC (& dstMaskDC, if one exists) to theHDC
        BlitImage(theHDC, theHDC, storedDstRect.x, storedDstRect.y,
                  aDestRect.width, aDestRect.height,
                  dstDC, dstMaskDC, 0, 0, useAlphaBlend);
        // blit again to 2nd part of tile area
        if (storedDstRect.height > aDestRect.height)
          BlitImage(theHDC, theHDC, storedDstRect.x, storedDstRect.y + aDestRect.height,
                    aDestRect.width, storedDstRect.height - aDestRect.height,
                    dstDC, dstMaskDC, 0, firstPartialHeight, useAlphaBlend);
      }
    } while (PR_FALSE);

    if (mAlphaDepth != 0) {
      if (dstDC) {
        if (dstHBitmap) {
          ::SelectObject(dstDC, oldDstBits);
          ::DeleteObject(dstHBitmap);
        }
        ::DeleteDC(dstDC);
      }
      if (dstMaskDC) {
        if (dstMaskHBitmap) {
          ::SelectObject(dstMaskDC, oldDstMaskBits);
          ::DeleteObject(dstMaskHBitmap);
        }
        ::DeleteDC(dstMaskDC);
      }
    }
  } else {
    // up to 4 image parts.
    // top-left
    BlitImage(theHDC, theHDC,
              aDestRect.x, aDestRect.y, firstWidth, firstHeight,
              imgDC, maskDC, aSXOffset, aSYOffset, useAlphaBlend);

    // bottom-right
    if (aDestRect.width - firstWidth > 0 && aDestRect.height - firstHeight > 0)
      BlitImage(theHDC, theHDC,
                aDestRect.x + firstWidth, aDestRect.y + firstHeight,
                aDestRect.width - firstWidth, aDestRect.height - firstHeight,
                imgDC, maskDC, 0, 0, useAlphaBlend);

    // bottom-left
    if (aDestRect.height - firstHeight > 0)
      BlitImage(theHDC, theHDC, aDestRect.x, aDestRect.y + firstHeight,
                firstWidth, aDestRect.height - firstHeight,
                imgDC, maskDC, aSXOffset, 0, useAlphaBlend);

    // top-right
    if (aDestRect.width - firstWidth > 0)
      BlitImage(theHDC, theHDC, aDestRect.x + firstWidth, aDestRect.y,
                aDestRect.width - firstWidth, firstHeight,
                imgDC, maskDC, 0, aSYOffset, useAlphaBlend);
  }

  if (oldImgBits)
    ::SelectObject(imgDC, oldImgBits);
  if (mTmpHBitmap)
    ::DeleteObject(mTmpHBitmap);
  ::DeleteDC(imgDC);
  if (maskDC) {
    if (oldImgMaskBits)
      ::SelectObject(maskDC, oldImgMaskBits);
    ::DeleteObject(maskBits);
    ::DeleteDC(maskDC);
  }
  ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();

  return result;
}

void
nsImageWin::BlitImage(HDC aDstDC, HDC aDstMaskDC, PRInt32 aDstX, PRInt32 aDstY,
                      PRInt32 aWidth, PRInt32 aHeight,
                      HDC aSrcDC, HDC aSrcMaskDC, PRInt32 aSrcX, PRInt32 aSrcY,
                      PRBool aUseAlphaBlend)
{
  if (aUseAlphaBlend) {
    BLENDFUNCTION blendFunction;
    blendFunction.BlendOp = AC_SRC_OVER;
    blendFunction.BlendFlags = 0;
    blendFunction.SourceConstantAlpha = 255;
    blendFunction.AlphaFormat = 1;
    gAlphaBlend(aDstDC, aDstX, aDstY, aWidth, aHeight,
                aSrcDC, aSrcX, aSrcY, aWidth, aHeight, blendFunction);
  } else {
    if (aSrcMaskDC) {
      if (aDstMaskDC == aDstDC) {
        ::BitBlt(aDstDC, aDstX, aDstY, aWidth, aHeight,
                 aSrcMaskDC, aSrcX, aSrcY, SRCAND);
        ::BitBlt(aDstDC, aDstX, aDstY, aWidth, aHeight,
                 aSrcDC, aSrcX, aSrcY, SRCPAINT);
      } else {
        ::BitBlt(aDstMaskDC, aDstX, aDstY, aWidth, aHeight,
                 aSrcMaskDC, aSrcX, aSrcY, SRCCOPY);
        ::BitBlt(aDstDC, aDstX, aDstY, aWidth, aHeight,
                  aSrcDC, aSrcX, aSrcY, SRCCOPY);
      }
    } else {
      ::BitBlt(aDstDC, aDstX, aDstY, aWidth, aHeight,
               aSrcDC, aSrcX, aSrcY, SRCCOPY);
    }
  }
}


ALPHABLENDPROC nsImageWin::gAlphaBlend = NULL;


PRBool nsImageWin::CanAlphaBlend(void)
{
  static PRBool alreadyChecked = PR_FALSE;

  if (!alreadyChecked) {
    OSVERSIONINFO os;
    
    os.dwOSVersionInfoSize = sizeof(os);
    ::GetVersionEx(&os);
    // If running on Win98, disable using AlphaBlend()
    // to avoid Win98 AlphaBlend() bug
    if (VER_PLATFORM_WIN32_WINDOWS != os.dwPlatformId ||
        os.dwMajorVersion != 4 || os.dwMinorVersion != 10) {
      gAlphaBlend = (ALPHABLENDPROC)::GetProcAddress(::LoadLibrary("msimg32"),
              "AlphaBlend");
    }
    alreadyChecked = PR_TRUE;
  }

  return gAlphaBlend != NULL;
}


/** ----------------------------------------------------------------
 * Create an optimized bitmap
 * @update dc - 11/20/98
 * @param aContext - The device context to use for the optimization
 */
nsresult 
nsImageWin :: Optimize(nsIDeviceContext* aContext)
{
  // we used to set a flag because a valid HDC may not be ready, 
  // like at startup, but now we just roll our own HDC for the given screen.

  //
  // Some images should not be converted to DDB...
  //
  // Windows 95/98/Me: DDB size cannot exceed ~16MB in size.
  // We also can not optimize empty images, or 8-bit alpha depth images on
  // Win98 due to a Windows API bug.
  //
  // Create DDBs only for large (> 128k) images to minimize GDI handle usage.
  // See bug 205893, bug 204374, and bug 216430 for more info on the situation.
  // Bottom-line: we need a better accounting mechanism to avoid exceeding the
  // system's GDI object limit.  The rather arbitrary size limitation imposed
  // here is based on the typical size of the Mozilla memory cache.  This is 
  // most certainly the wrong place to impose this policy, but we do it for
  // now as a stop-gap measure.
  //
  if ((gPlatform == VER_PLATFORM_WIN32_WINDOWS && mSizeImage >= 0xFF0000) ||
      (mAlphaDepth == 8 && !CanAlphaBlend()) ||
      (mSizeImage < 0x20000)) {
    return NS_OK;
  }

  HDC TheHDC = ::CreateCompatibleDC(NULL);
  
  if (TheHDC != NULL){
    // Temporary fix for bug 135226 until we do better decode-time
    // dithering and paletized storage of images. Bail on the 
    // optimization to DDB if we're on a paletted device.
    int rasterCaps = ::GetDeviceCaps(TheHDC, RASTERCAPS);
    if (RC_PALETTE == (rasterCaps & RC_PALETTE)) {
      ::DeleteDC(TheHDC);
      return NS_OK;
    }
  
    // we have to install the correct bitmap to get a good DC going
    int planes = ::GetDeviceCaps(TheHDC, PLANES);
    int bpp = ::GetDeviceCaps(TheHDC, BITSPIXEL);

    HBITMAP  tBitmap = ::CreateBitmap(1,1,planes,bpp,NULL);
    HBITMAP oldbits = (HBITMAP)::SelectObject(TheHDC,tBitmap);

    if (mAlphaDepth == 8) {
      CreateImageWithAlphaBits(TheHDC);
    } else {
      LPVOID bits;
      mHBitmap = ::CreateDIBSection(TheHDC, (LPBITMAPINFO)mBHead,
        256 == mNumPaletteColors ? DIB_PAL_COLORS : DIB_RGB_COLORS,
        &bits, NULL, 0);

      if (mHBitmap) {
        memcpy(bits, mImageBits, mSizeImage);
        mIsOptimized = PR_TRUE;
      } else {
        mIsOptimized = PR_FALSE;
      }
    }
    if (mIsOptimized)
      CleanUpDIB();
    ::SelectObject(TheHDC,oldbits);
    ::DeleteObject(tBitmap);
    ::DeleteDC(TheHDC);
  }

  return NS_OK;
}


/** ----------------------------------------------------------------
 * Calculate the number of bytes in a span for this image
 * @update dc - 11/20/98
 * @param aWidth - The width to calulate the number of bytes from
 * @return Number of bytes for the span
 */
PRInt32  
nsImageWin :: CalcBytesSpan(PRUint32  aWidth)
{
  PRInt32 spanBytes;


  spanBytes = (aWidth * mBHead->biBitCount) >> 5;


  if (((PRUint32)mBHead->biWidth * mBHead->biBitCount) & 0x1F) 
    spanBytes++;


  spanBytes <<= 2;


  return(spanBytes);
}


/** ---------------------------------------------------
 *  See documentation in nsImageWin.h
 *  @update 4/05/00 dwc
 */
void
nsImageWin::CleanUpDIB()
{
  if (mImageBits != nsnull) {
    delete [] mImageBits;
    mImageBits = nsnull;
  }


  // Should be an ISupports, so we can release
  if (mColorMap != nsnull){
    delete [] mColorMap->Index;
    delete mColorMap;
    mColorMap = nsnull;
  }


  //mNumPaletteColors = -1;
  //mNumBytesPixel = 0;
  mSizeImage = 0;


}

/** ---------------------------------------------------
 *  See documentation in nsImageWin.h
 *  @update 4/05/00 dwc
 */
void 
nsImageWin :: CleanUpDDB()
{
  if (mHBitmap != nsnull) {
    ::DeleteObject(mHBitmap);
    mHBitmap = nsnull;
  }
  mIsOptimized = PR_FALSE;
}


/** ----------------------------------------------------------------
 * See documentation in nsIImage.h
 * @update - dwc 5/20/99
 * @return the result of the operation, if NS_OK, then the pixelmap is unoptimized
 */
nsresult 
nsImageWin::PrintDDB(nsIDrawingSurface* aSurface,
                     PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight,
                     PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                     PRUint32 aROP)
{
  HDC   theHDC;
  UINT  palType;


  if (mIsOptimized == PR_TRUE){
    if (mHBitmap != nsnull){
      ConvertDDBtoDIB();
      ((nsDrawingSurfaceWin *)aSurface)->GetDC(&theHDC);


      if (mBHead->biBitCount == 8) {
        palType = DIB_PAL_COLORS;
      } else {
        palType = DIB_RGB_COLORS;
      }


      ::StretchDIBits(theHDC, aDX, aDY, aDWidth, aDHeight,
                      aSX, aSY, aSWidth, aSHeight, mImageBits,
                      (LPBITMAPINFO)mBHead, palType, aROP);


      // we are finished with this, so delete it           
      if (mImageBits != nsnull) {
        delete [] mImageBits;
        mImageBits = nsnull;
      }
    }
  }


  return NS_OK;
}


/** ----------------------------------------------------------------
 * See documentation in nsIImage.h
 * @update - dwc 5/20/99
 * @return the result of the operation, if NS_OK a DIB was created.
 */
nsresult 
nsImageWin::ConvertDDBtoDIB()
{
  PRInt32             numbytes,tWidth,tHeight;
  BITMAP              srcinfo;
  HBITMAP             oldbits;
  HDC                 memPrDC;
  UINT                palType;


  if (mIsOptimized == PR_TRUE) {

    if (mHBitmap != nsnull){
      memPrDC = ::CreateDC("DISPLAY",NULL,NULL,NULL);
      oldbits = (HBITMAP)::SelectObject(memPrDC,mHBitmap);

      numbytes = ::GetObject(mHBitmap,sizeof(BITMAP),&srcinfo);
      
      tWidth = mBHead->biWidth;
      tHeight = mBHead->biHeight;

      if (nsnull != mBHead){
        delete[] mBHead;
      }
      BuildDIB(&mBHead,tWidth,tHeight,srcinfo.bmBitsPixel,&mNumBytesPixel);
      mRowBytes = CalcBytesSpan(mBHead->biWidth);
      mSizeImage = mRowBytes * mBHead->biHeight; // no compression


      // Allocate the image bits
      mImageBits = new unsigned char[mSizeImage];


      if (mBHead->biBitCount == 8) {
        palType = DIB_PAL_COLORS;
      } else {
        palType = DIB_RGB_COLORS;
      }

      numbytes = ::GetDIBits(memPrDC, mHBitmap, 0, srcinfo.bmHeight, mImageBits,
                             (LPBITMAPINFO)mBHead, palType);
      ::SelectObject(memPrDC,oldbits);
      DeleteDC(memPrDC);
    }
  }

  return NS_OK;
}


/** ----------------------------------------------------------------
 * Build A DIB header and allocate memory 
 * @update dc - 11/20/98
 * @return void
 */
nsresult
BuildDIB(LPBITMAPINFOHEADER  *aBHead,PRInt32 aWidth,PRInt32 aHeight,PRInt32 aDepth,PRInt8 *aNumBytesPix)
{
  PRInt16 numPaletteColors;


  if (8 == aDepth) {
    numPaletteColors = 256;
    *aNumBytesPix = 1;
  } else if (16 == aDepth) {
    numPaletteColors = 0;
    *aNumBytesPix = 2; 
  } else if (24 == aDepth) {
    numPaletteColors = 0;
    *aNumBytesPix = 3;
  } else if (32 == aDepth) {
    numPaletteColors = 0;  
    *aNumBytesPix = 4;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }


  if (0 == numPaletteColors) {
    // space for the header only (no color table)
    (*aBHead) = (LPBITMAPINFOHEADER)new char[sizeof(BITMAPINFO)];
  } else {
    // Space for the header and the palette. Since we'll be using DIB_PAL_COLORS
    // the color table is an array of 16-bit unsigned integers that specify an
    // index into the currently realized logical palette
    (*aBHead) = (LPBITMAPINFOHEADER)new char[sizeof(BITMAPINFOHEADER) + (256 * sizeof(WORD))];
  }


  (*aBHead)->biSize = sizeof(BITMAPINFOHEADER);
  (*aBHead)->biWidth = aWidth;
  (*aBHead)->biHeight = aHeight;
  (*aBHead)->biPlanes = 1;
  (*aBHead)->biBitCount = (WORD)aDepth;
  (*aBHead)->biCompression = BI_RGB;
  (*aBHead)->biSizeImage = 0;            // not compressed, so we dont need this to be set
  (*aBHead)->biXPelsPerMeter = 0;
  (*aBHead)->biYPelsPerMeter = 0;
  (*aBHead)->biClrUsed = numPaletteColors;
  (*aBHead)->biClrImportant = numPaletteColors;


  return NS_OK;
}


/** ---------------------------------------------------
 *  Lock down the image pixels
 */
NS_IMETHODIMP
nsImageWin::LockImagePixels(PRBool aMaskPixels)
{
  /*  if (!mHBitmap) return NS_ERROR_NOT_INITIALIZED;
   ... and do Windows locking of image pixels here, if necessary */


  mIsLocked = PR_TRUE;


  return NS_OK;
}


/** ---------------------------------------------------
 *  Unlock the pixels, optimize this nsImageWin
 */
NS_IMETHODIMP
nsImageWin::UnlockImagePixels(PRBool aMaskPixels)
{
  mIsLocked = PR_FALSE;
  // if memory was allocated temporarily by GetBits, it can now be deleted safely
  if (mDIBTemp == PR_TRUE) {
    // get rid of this memory
    if (mImageBits != nsnull) {
      delete [] mImageBits;
      mImageBits = nsnull;
      }


    mDIBTemp = PR_FALSE;
  }


  /* if (!mHBitmap)
    return NS_ERROR_NOT_INITIALIZED;
  
  if (aMaskPixels && !mAlphamHBitmap)
    return NS_ERROR_NOT_INITIALIZED;


   ... and do Windows unlocking of image pixels here, if necessary */


  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsImageWin.h
 *  Get a pointer to the bits for the pixelmap. Will convert to DIB if
 *  only stored in optimized HBITMAP form.  Using this routine will
 *  set the mDIBTemp flag to true so the next unlock will destroy this memory
 *
 * @return address of the DIB pixel array
 */

PRUint8*
nsImageWin::GetBits()
{
  // if mImageBits did not exist.. then
  if (!mImageBits) {
    ConvertDDBtoDIB();
    mDIBTemp = PR_TRUE;   // only set to true if the DIB is being created here as temporary
  }


  return mImageBits;


} // GetBits


/** ---------------------------------------------------
 *  This blends together GIF's for the animation... called by gfxImageFrame   
 *  currently does not support and 8bit blend.  Assumed for animated GIF's only
 *  @update 07/09/2002 dwc
 *  @param aDstImage -- destination where to copy to
 *  @param aDX -- x location of the image
 *  @param aDY -- y location of the image
 *  @param aDWidth -- width of the image
 *  @param aDHeight -- height of the image
 */
NS_IMETHODIMP nsImageWin::DrawToImage(nsIImage* aDstImage, nscoord aDX, nscoord aDY, nscoord aDWidth, nscoord aDHeight)
{
  NS_ASSERTION(mAlphaDepth <= 1, "nsImageWin::DrawToImage can only handle 0 & 1 bit Alpha");

  if (mAlphaDepth > 1)
    return NS_ERROR_UNEXPECTED;

  nsImageWin *dest = NS_STATIC_CAST(nsImageWin *, aDstImage);

  if (!dest)
    return NS_ERROR_FAILURE;

  if (aDX >= dest->mBHead->biWidth || aDY >= dest->mBHead->biHeight)
    return NS_OK;

  if (!dest->mImageBits)
    return NS_ERROR_FAILURE;
     
  if (!dest->mIsOptimized) {
    // set up some local variable to make things run faster in the loop
    PRUint8  *rgbPtr = 0, *alphaPtr = 0;
    PRUint32 rgbStride, alphaStride;
    PRInt32  srcHeight;
    PRUint8  *dstRgbPtr = 0, *dstAlphaPtr = 0;
    PRUint32 dstRgbStride, dstAlphaStride;
    PRInt32  dstHeight;

    rgbPtr = mImageBits;
    rgbStride = mRowBytes;
    alphaPtr = mAlphaBits;
    alphaStride = mARowBytes;
    srcHeight = mBHead->biHeight;

    dstRgbPtr = dest->mImageBits;
    dstRgbStride = dest->mRowBytes;
    dstAlphaPtr = dest->mAlphaBits;
    dstAlphaStride = dest->mARowBytes;
    dstHeight = dest->mBHead->biHeight;


    PRInt32 y;
    PRInt32 ValidWidth = (aDWidth < (dest->mBHead->biWidth - aDX)) ?
                         aDWidth : (dest->mBHead->biWidth - aDX);
    PRInt32 ValidHeight = (aDHeight < (dstHeight - aDY)) ?
                          aDHeight : (dstHeight - aDY);
    PRUint8 *dst;
    PRUint8 *src;

    // now composite the two images together
    switch (mAlphaDepth) {
    case 1:
      {
        PRUint8 *dstAlpha;
        PRUint8 *alpha;
        PRUint8 offset = aDX & 0x7; // x starts at 0


        for (y=0; y<ValidHeight; y++) {
          dst = dstRgbPtr +
                (dstHeight - (aDY+y) - 1) * dstRgbStride +
                (3 * aDX);
          dstAlpha = dstAlphaPtr + (dstHeight - (aDY+y) - 1) * dstAlphaStride;
          src = rgbPtr + (srcHeight - y - 1)*rgbStride;
          alpha = alphaPtr + (srcHeight - y - 1)*alphaStride;
          for (int x = 0;
               x < ValidWidth;
               x += 8, dst +=  3 * 8, src +=  3 * 8) {
            PRUint8 alphaPixels = *alpha++;
            if (alphaPixels == 0) {
              // all 8 transparent; jump forward
              continue;
            }


            // 1 or more bits are set, handle dstAlpha now - may not be aligned.
            // Are all 8 of these alpha pixels used?
            if (x+7 >= ValidWidth) {
              alphaPixels &= 0xff << (8 - (ValidWidth-x)); // no, mask off unused
              if (alphaPixels == 0)
                continue;  // no 1 alpha pixels left
            }
            if (offset == 0) {
              dstAlpha[(aDX+x)>>3] |= alphaPixels; // the cheap aligned case
            } else {
              dstAlpha[(aDX+x)>>3]       |= alphaPixels >> offset;
              // avoid write if no 1's to write - also avoids going past end of array
              PRUint8 alphaTemp = alphaPixels << (8U - offset);
              if (alphaTemp & 0xff)
                dstAlpha[((aDX+x)>>3) + 1] |= alphaTemp;
            }


            if (alphaPixels == 0xff) {
              // fix - could speed up by gathering a run of 0xff's and doing 1 memcpy
              // all 8 pixels set; copy and jump forward
              memcpy(dst,src,8*3);
              continue;
            } else {
              // else mix of 1's and 0's in alphaPixels, do 1 bit at a time
              // Don't go past end of line!
              PRUint8 *d = dst, *s = src;
              for (PRUint8 aMask = 1<<7, j = 0; aMask && j < ValidWidth-x; aMask >>= 1, j++) {
                // if this pixel is opaque then copy into the destination image
                if (alphaPixels & aMask) {
                  // might be faster with *d++ = *s++ 3 times?
                  d[0] = s[0];
                  d[1] = s[1];
                  d[2] = s[2];
                  // dstAlpha bit already set
                }
                d += 3;
                s += 3;
              }
            }
          }
        }
      }
      break;
    case 0:
    default:
      dst = dstRgbPtr + (dstHeight - aDY - 1) * dstRgbStride + 3 * aDX;
      src = rgbPtr + (srcHeight - 1) * rgbStride;

      for (y = 0; y < ValidHeight; y++) {
        memcpy(dst, src,  3 * ValidWidth);
        dst -= dstRgbStride;
        src -= rgbStride;
      }
    }
    nsRect rect(aDX, aDY, ValidWidth, ValidHeight);
    dest->ImageUpdated(nsnull, 0, &rect);
  } else {
    // dst's Image is optimized (dest->mHBitmap is linked to dest->mImageBits), so just paint on it
    if (!dest->mHBitmap)
      return NS_ERROR_UNEXPECTED;
      
    HDC dstMemDC = ::CreateCompatibleDC(nsnull);
    HBITMAP oldDstBits;
    DWORD rop;

    oldDstBits = (HBITMAP)::SelectObject(dstMemDC, dest->mHBitmap);
    rop = SRCCOPY;

    if (mAlphaBits) {
      if (1==mAlphaDepth) {
        MONOBITMAPINFO  bmi(mBHead->biWidth, mBHead->biHeight);

        ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight,
                        0, 0,mBHead->biWidth, mBHead->biHeight, mAlphaBits,
                        (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
        rop = SRCPAINT;
      }
    }

    if (8 == mAlphaDepth) {
      nsresult rv = DrawComposited(dstMemDC, aDX, aDY, aDWidth, aDHeight,
                                   0, 0, mBHead->biWidth, mBHead->biHeight,
                                   aDWidth, aDHeight);
      if (NS_FAILED(rv)) {
        ::SelectObject(dstMemDC, oldDstBits);
        ::DeleteDC(dstMemDC);
        return rv;
      }
    } 
    
    // Copy/Paint our rgb to dest
    if (mIsOptimized && mHBitmap) {
      // We are optimized, use Stretchblt
      HDC srcMemDC = ::CreateCompatibleDC(nsnull);
      HBITMAP oldSrcBits;
      oldSrcBits = (HBITMAP)::SelectObject(srcMemDC, mHBitmap);
       
      ::StretchBlt(dstMemDC, aDX, aDY, aDWidth, aDHeight, srcMemDC, 
                   0, 0, mBHead->biWidth, mBHead->biHeight, rop);
      
      ::SelectObject(srcMemDC, oldSrcBits);
      ::DeleteDC(srcMemDC);
    } else {
      ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight, 
                      0, 0, mBHead->biWidth, mBHead->biHeight, mImageBits,
                      (LPBITMAPINFO)mBHead, DIB_RGB_COLORS, rop);
    }
    ::SelectObject(dstMemDC, oldDstBits);
    ::DeleteDC(dstMemDC);
  }


  return NS_OK;
}

/** ---------------------------------------------------
 *  copy the mask and the image to the passed in DC, do the raster operation in memory before going to the 
 *  printer
 */
void 
CompositeBitsInMemory(HDC aTheHDC, int aDX, int aDY, int aDWidth, int aDHeight,
                      int aSX, int aSY, int aSWidth, int aSHeight,PRInt32 aSrcy,
                      PRUint8 *aAlphaBits, MONOBITMAPINFO *aBMI,
                      PRUint8* aImageBits, LPBITMAPINFOHEADER aBHead,
                      PRInt16 aNumPaletteColors)
{
  unsigned char *screenBits;

  HDC memDC = ::CreateCompatibleDC(NULL);

  if(0!=memDC){
    ALPHA24BITMAPINFO offbmi(aSWidth, aSHeight);
    HBITMAP tmpBitmap = ::CreateDIBSection(memDC, (LPBITMAPINFO)&offbmi, DIB_RGB_COLORS,
                                           (LPVOID *)&screenBits, NULL, 0);

    if(0 != tmpBitmap){
      HBITMAP oldBitmap = (HBITMAP)::SelectObject(memDC, tmpBitmap);

      if(0!=oldBitmap) {
        // pop in the alpha channel
        ::StretchDIBits(memDC, 0, 0, aSWidth, aSHeight,
                        aSX, aSrcy, aSWidth, aSHeight,
                        aAlphaBits, (LPBITMAPINFO)aBMI,
                        DIB_RGB_COLORS, SRCCOPY); 

        // paint in the image
        ::StretchDIBits(memDC, 0, 0, aSWidth, aSHeight,
                        aSX, aSrcy, aSWidth, aSHeight,
                        aImageBits, (LPBITMAPINFO)aBHead,
                        256 == aNumPaletteColors ? DIB_PAL_COLORS : DIB_RGB_COLORS,
                        SRCPAINT);

        // output the composed image
        ::StretchDIBits(aTheHDC, aDX, aDY, aDWidth, aDHeight,
                        aSX, aSrcy, aSWidth, aSHeight,
                        screenBits, (LPBITMAPINFO)&offbmi,
                        256 == aNumPaletteColors ? DIB_PAL_COLORS : DIB_RGB_COLORS,
                        SRCCOPY);

        ::SelectObject(memDC, oldBitmap);
      }
      ::DeleteObject(tmpBitmap);
    }
    ::DeleteDC(memDC);
  }
}
