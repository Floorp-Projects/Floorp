/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsImageWin.h"
#include "nsRenderingContextWin.h"

#define MAX_BUFFER_WIDTH        128
#define MAX_BUFFER_HEIGHT       128

static nsresult BuildDIB(LPBITMAPINFOHEADER  *aBHead,PRInt32 aWidth,PRInt32 aHeight,PRInt32 aDepth,PRInt8  *aNumBitPix);

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
  mDIBSection = nsnull;
  mAlphaBits = nsnull;
  mAlphaDepth = nsnull;
  mColorMap = nsnull;
  mBHead = nsnull;

  mNaturalWidth = 0;
  mNaturalHeight = 0;
  mIsLocked = PR_FALSE;
  mDIBTemp = PR_FALSE;

  //CleanUp(PR_TRUE);
  CleanUpDIBSection();
  CleanUpDDB();
  CleanUpDIB();

}

/** ----------------------------------------------------------------
  * destructor for nsImageWin
  * @update dc - 11/20/98
  */
nsImageWin :: ~nsImageWin()
{
  //CleanUp(PR_TRUE);
  CleanUpDIBSection();
  CleanUpDDB();
  CleanUpDIB();
}

NS_IMPL_ISUPPORTS1(nsImageWin, nsIImage);

/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *  @update 3/27/00 dwc
 */
nsresult nsImageWin :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
  mHBitmap = nsnull;
  //CleanUp(PR_TRUE);
  CleanUpDIBSection();
  CleanUpDDB();
  CleanUpDIB();

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

  SetDecodedRect(0,0,0,0);  //init

  SetNaturalWidth(0);
  SetNaturalHeight(0);

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
    if (mAlphaBits)
      delete[] mAlphaBits;
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

/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *  @update 3/27/00 dwc
 */
void 
nsImageWin :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
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


static void CompositeBitsInMemory(HDC aTheHDC, int aDX, int aDY, int aDWidth, int aDHeight,
                    int aSX, int aSY, int aSWidth, int aSHeight,PRInt32 aSrcy,
                    PRUint8 *aAlphaBits,MONOBITMAPINFO  *aBMI,PRUint8* aImageBits,LPBITMAPINFOHEADER aBHead,PRInt16 aNumPaletteColors);


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
}

/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *  @update 3/27/00 dwc
 */
void 
nsImageWin :: CreateDDB(nsDrawingSurface aSurface)
{
  HDC TheHDC;

  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);

  if (TheHDC != NULL){
    if (mSizeImage > 0){
       if (mAlphaDepth == 8) {
         CreateImageWithAlphaBits(TheHDC);
       } else {
         mHBitmap = ::CreateDIBitmap(TheHDC,mBHead,CBM_INIT,mImageBits,(LPBITMAPINFO)mBHead,
                  256==mNumPaletteColors?DIB_PAL_COLORS:DIB_RGB_COLORS);
       }
      mIsOptimized = PR_TRUE;
      //CleanUp(PR_FALSE);
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
nsImageWin :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                  PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
  HDC     TheHDC;
  PRInt32 canRaster,srcy;
  HBITMAP oldBits;
  DWORD   rop;
  PRInt32 origSHeight = aSHeight, origDHeight = aDHeight;
  PRInt32 origSWidth = aSWidth, origDWidth = aDWidth;

  if (mBHead == nsnull || aSWidth < 0 || aDWidth < 0 || aSHeight < 0 || aDHeight < 0) 
    return NS_ERROR_FAILURE;

  if (0 == aSWidth || 0 == aDWidth || 0 == aSHeight || 0 == aDHeight)
    return NS_OK;

  // limit the size of the blit to the amount of the image read in
  if (aSX + aSWidth > mDecodedX2) {
    aDWidth -= ((aSX + aSWidth - mDecodedX2)*origDWidth)/origSWidth;
    aSWidth -= (aSX + aSWidth) - mDecodedX2;
  }
  if (aSX < mDecodedX1) {
    aDX += ((mDecodedX1 - aSX)*origDWidth)/origSWidth;
    aSX = mDecodedX1;
  }

  if (aSY + aSHeight > mDecodedY2) {
    aDHeight -= ((aSY + aSHeight - mDecodedY2)*origDHeight)/origSHeight;
    aSHeight -= (aSY + aSHeight) - mDecodedY2;
  }
  if (aSY < mDecodedY1) {
    aDY += ((mDecodedY1 - aSY)*origDHeight)/origSHeight;
    aSY = mDecodedY1;
  }

  if (aDWidth <= 0 || aDHeight <= 0)
    return NS_OK;

  // Translate to bottom-up coordinates for the source bitmap
  srcy = mBHead->biHeight - (aSY + aSHeight);

  // if DC is not for a printer, and the image can be optimized, 
  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);

  // find out if the surface is a printer.
  ((nsDrawingSurfaceWin *)aSurface)->GetTECHNOLOGY(&canRaster);
  if (canRaster != DT_RASPRINTER){
    if ((PR_TRUE==mCanOptimize) && (nsnull == mHBitmap))
      CreateDDB(aSurface);
  }

  if (nsnull != TheHDC){
    if (!IsOptimized() || nsnull==mHBitmap){
      PRBool  didComposite = PR_FALSE;

      rop = SRCCOPY;

      if (nsnull != mAlphaBits){
        if (1==mAlphaDepth){
          if(canRaster == DT_RASPRINTER){
            MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);
              CompositeBitsInMemory(TheHDC,aDX,aDY,aDWidth,aDHeight,aSX,aSY,aSWidth,aSHeight,srcy,
                    mAlphaBits,&bmi,mImageBits,mBHead,mNumPaletteColors);
            didComposite = PR_TRUE;
          } else {

            MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

            ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,aSX, srcy,aSWidth, aSHeight, mAlphaBits,
                                            (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
             rop = SRCPAINT;
          }  
		}
      }

      
      if (PR_FALSE == didComposite){
        if (8==mAlphaDepth) {              
           DrawComposited(TheHDC, aDX, aDY, aDWidth, aDHeight,
             aSX, srcy, aSWidth, aSHeight);
        } else {
          ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,aSX, srcy, aSWidth, aSHeight, mImageBits,
            (LPBITMAPINFO)mBHead, 256 == mNumPaletteColors ? DIB_PAL_COLORS :
            DIB_RGB_COLORS, rop);
        }
      }

    }else{
      nsIDeviceContext    *dx;
      aContext.GetDeviceContext(dx);
      nsDrawingSurface     ds;
      dx->GetDrawingSurface(aContext, ds);
      nsDrawingSurfaceWin *srcDS = (nsDrawingSurfaceWin *)ds;
      HDC                 srcDC;

      if (nsnull != srcDS){
        srcDS->GetDC(&srcDC);

        rop = SRCCOPY;

        if (nsnull != mAlphaBits){
          if (1==mAlphaDepth){
            MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

            ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,aSX, srcy, aSWidth, aSHeight, mAlphaBits,
                                          (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
             rop = SRCPAINT;
             //rop = SRCCOPY;
          }
        }
        // if this is for a printer.. we have to convert it back to a DIB
        if (canRaster == DT_RASPRINTER){
          if (!(GetDeviceCaps(TheHDC,RASTERCAPS) &(RC_BITBLT | RC_STRETCHBLT))) {
            // we have an error with the printer not supporting a raster device
          } else {
            // if we did not convert to a DDB already
            if (nsnull == mHBitmap) {
              oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
              if (8 == mAlphaDepth) {
                BLENDFUNCTION blendFunction;
                blendFunction.BlendOp = AC_SRC_OVER;
                blendFunction.BlendFlags = 0;
                blendFunction.SourceConstantAlpha = 255;
                blendFunction.AlphaFormat = 1 /*AC_SRC_ALPHA*/;
                gAlphaBlend(TheHDC, aDX, aDY, aDWidth, aDHeight, srcDC, aSX, srcy, aSWidth, aSHeight, blendFunction);
              } else {
                ::StretchBlt(TheHDC, aDX, aDY, aDWidth, aDHeight, srcDC, aSX, aSY,aSWidth, aSHeight, rop);
              }
            }else{
              PrintDDB(aSurface,aDX,aDY,aDWidth,aDHeight,rop);
            }
          }
        } else {
          // we are going to the device that created this DDB
          oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
          if (8 == mAlphaDepth) {
            BLENDFUNCTION blendFunction;
            blendFunction.BlendOp = AC_SRC_OVER;
            blendFunction.BlendFlags = 0;
            blendFunction.SourceConstantAlpha = 255;
            blendFunction.AlphaFormat = 1 /*AC_SRC_ALPHA*/;
            gAlphaBlend(TheHDC, aDX, aDY, aDWidth, aDHeight, srcDC, aSX, srcy, aSWidth, aSHeight, blendFunction);
          } else {
            ::StretchBlt(TheHDC,aDX,aDY,aDWidth,aDHeight,srcDC,aSX,aSY,aSWidth,aSHeight,rop);
          }
        }

        ::SelectObject(srcDC, oldBits);
        srcDS->ReleaseDC();
      }
      NS_RELEASE(dx);
    }
    ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
  }

  return NS_OK;
}

void nsImageWin::DrawComposited24(unsigned char *aBits, int aX, int aY, int aWidth, int aHeight)
{
  PRInt32 targetRowBytes = ((aWidth * 3) + 3) & ~3;
  for (int y = 0; y < aHeight; y++) {
    unsigned char *targetRow = aBits + y * targetRowBytes;
    unsigned char *imageRow = mImageBits + (y + aY) * mRowBytes + 3 * aX;
    unsigned char *alphaRow = mAlphaBits + (y + aY) * mARowBytes + aX;

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
 *  Do alpha blending by hand
 */
void nsImageWin::DrawComposited(HDC TheHDC, int aDX, int aDY, int aDWidth, int aDHeight,
                    int aSX, int aSY, int aSWidth, int aSHeight)
{
  HDC memDC = CreateCompatibleDC(TheHDC);
  unsigned char *screenBits;
  ALPHA24BITMAPINFO bmi(aSWidth, aSHeight);
  HBITMAP tmpBitmap = ::CreateDIBSection(memDC, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS,
                                         (LPVOID *)&screenBits, NULL, 0);
  HBITMAP oldBitmap = (HBITMAP)::SelectObject(memDC, tmpBitmap);

  /* Copy from the screen */
  ::StretchBlt(memDC, 0, 0, aSWidth, aSHeight,
               TheHDC, aDX, aDY, aDWidth, aDHeight, SRCCOPY);

  /* Do composite */
  DrawComposited24(screenBits, aSX, aSY, aSWidth, aSHeight);

  /* Copy back to the screen */
  ::StretchBlt(TheHDC, aDX, aDY, aDWidth, aDHeight,
               memDC, 0, 0, aSWidth, aSHeight, SRCCOPY);

  ::SelectObject(memDC, oldBitmap);
  ::DeleteObject(tmpBitmap);
  ::DeleteObject(memDC);
}

/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h
 *  @update 3/27/00 dwc
 */
NS_IMETHODIMP nsImageWin :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
         PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return Draw(aContext, aSurface, 0, 0, mNaturalWidth, mNaturalHeight, aX, aY, aWidth, aHeight);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *  @update 3/16/00 dwc
 */
NS_IMETHODIMP nsImageWin::DrawTile(nsIRenderingContext &aContext,
                                   nsDrawingSurface aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   const nsRect &aTileRect)
{
  PRInt32
    validX = 0,
    validY = 0,
    validWidth  = mBHead->biWidth,
    validHeight = mBHead->biHeight;
  
  // limit the image rectangle to the size of the image data which
  // has been validated.
  if (mDecodedY2 < mBHead->biHeight) {
    validHeight = mDecodedY2 - mDecodedY1;
  }
  if (mDecodedX2 < mBHead->biWidth) {
    validWidth = mDecodedX2 - mDecodedX1;
  }
  if (mDecodedY1 > 0) {   
    validHeight -= mDecodedY1;
    validY = mDecodedY1;
  }
  if (mDecodedX1 > 0) {
    validWidth -= mDecodedX1;
    validX = mDecodedX1; 
  }

  PRInt32 aY0 = aTileRect.y - aSYOffset,
          aX0 = aTileRect.x - aSXOffset,
          aY1 = aTileRect.y + aTileRect.height,
          aX1 = aTileRect.x + aTileRect.width;

  nscoord aWidth = mBHead->biWidth;
  nscoord aHeight = mBHead->biHeight;

  nscoord tileWidth = aTileRect.width;
  nscoord tileHeight = aTileRect.height;

  nsRect              destRect,srcRect,tvrect;
  HDC                 TheHDC,offDC,maskDC;
  PRInt32             x,y,width,height,canRaster,TileBufferWidth,TileBufferHeight;
  HBITMAP             maskBits,tileBits,oldBits,oldMaskBits; 


  // The slower tiling will need to be used for the following cases:
  // 1.) Printers 2.) When in 256 color mode 3.) when the tile is larger than the buffer
  tvrect.SetRect(0,0,aX1-aX0,aY1-aY0);
  ((nsDrawingSurfaceWin *)aSurface)->GetTECHNOLOGY(&canRaster);

  // we have to use the old way.. for 256 color mode and printing.. slow, but will always work.
  if ((mAlphaDepth>1) || (canRaster==DT_RASPRINTER) || (256==mNumPaletteColors)
      || (aWidth>MAX_BUFFER_WIDTH) || (aHeight>MAX_BUFFER_HEIGHT)){
    for(y=aY0;y<aY1;y+=aHeight){
      for(x=aX0;x<aX1;x+=aWidth){
      Draw(aContext, aSurface,
           0, 0, PR_MIN(validWidth, aX1-x), PR_MIN(validHeight, aY1-y),
           x, y, PR_MIN(validWidth, aX1-x), PR_MIN(validHeight, aY1-y));
      }
    } 
    return(PR_TRUE);
  }
  
  // create a larger tile from the smaller one
  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);
  if (NULL == TheHDC){
    return (PR_FALSE);
  }

  // can not create a compatible bitmap with an offscreen DC (it will be black and white)
  // so we will create a screen compatible bitmap and then install this into the offscreen DC
  tvrect.SetRect(0,0,aX1-aX0,aY1-aY0);
  offDC = ::CreateCompatibleDC(TheHDC);
  if (NULL ==offDC){
    return (PR_FALSE);
  }

  if (tileWidth < tvrect.width){
    TileBufferWidth = MAX_BUFFER_WIDTH;
  } else {
    TileBufferWidth = tileWidth;
  }

  if (tileHeight < tvrect.height){
    TileBufferHeight = MAX_BUFFER_HEIGHT;
  } else {
    TileBufferHeight = tileHeight;
  }

  tileBits = ::CreateCompatibleBitmap(TheHDC, TileBufferWidth,TileBufferHeight);

  if (NULL == tileBits){
    ::DeleteObject(offDC);
    return (PR_FALSE);
  }
  oldBits =(HBITMAP) ::SelectObject(offDC,tileBits);


  if (1==mAlphaDepth) {
    // larger tile for mask
    maskDC = ::CreateCompatibleDC(TheHDC);
    if (NULL ==maskDC){
      ::SelectObject(offDC,oldBits);
      ::DeleteObject(tileBits);
      ::DeleteObject(offDC);
      return (PR_FALSE);
    }
    maskBits = ::CreateCompatibleBitmap(TheHDC, TileBufferWidth, TileBufferHeight);
    if (NULL ==maskBits){
      ::SelectObject(offDC,oldBits);
      ::DeleteObject(tileBits);
      ::DeleteObject(offDC);
      ::DeleteObject(maskDC);
      return (PR_FALSE);
    }

    oldMaskBits = (HBITMAP)::SelectObject(maskDC,maskBits);

    // get the mask into our new tiled mask
    MONOBITMAPINFO  bmi(mAlphaWidth,mAlphaHeight);
    ::StretchDIBits(maskDC, 0, 0, aWidth, aHeight,0, 0, 
                    mAlphaWidth, mAlphaHeight, mAlphaBits,(LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCCOPY);

    ::BitBlt(maskDC,0,0,aWidth,aHeight,maskDC,0,0,SRCCOPY);
    srcRect.SetRect(0,0,aWidth,aHeight);
    BuildTile(maskDC,srcRect,TileBufferWidth/2,TileBufferHeight/2,SRCCOPY);
  }

  // put the initial tile of background image into the offscreen
  if (!IsOptimized() || nsnull==mHBitmap){
    ::StretchDIBits(offDC, 0, 0, aWidth, aHeight,0, 0, aWidth, aHeight, mImageBits,
                    (LPBITMAPINFO)mBHead, 256 == mNumPaletteColors ? DIB_PAL_COLORS:DIB_RGB_COLORS,
                    SRCCOPY);
  }else{
    // need to select install this bitmap into this DC first
    HBITMAP oldBits;

    oldBits = (HBITMAP)::SelectObject(TheHDC, mHBitmap);
    ::BitBlt(offDC,0,0,aWidth,aHeight,TheHDC,0,0,SRCCOPY);
    ::SelectObject(TheHDC, oldBits);
  }

  srcRect.SetRect(0,0,aWidth,aHeight);
  BuildTile(offDC,srcRect,TileBufferWidth/2,TileBufferHeight/2,SRCCOPY);

  // now duplicate our tile into the background
  destRect = srcRect;
  width = destRect.width;
  height = destRect.height;

  if (1!=mAlphaDepth) {
    for(y=aY0;y<aY1;y+=srcRect.height){
      for(x=aX0;x<aX1;x+=srcRect.width){
        destRect.x = x;
        destRect.y = y;
        ::BitBlt(TheHDC,x,y,width,height,offDC,0,0,SRCCOPY);
      }
    } 
  } else {
    for(y=aY0;y<aY1;y+=srcRect.height){
      for(x=aX0;x<aX1;x+=srcRect.width){
        destRect.x = x;
        destRect.y = y;
        ::BitBlt(TheHDC,x,y,width,height,maskDC,0,0,SRCAND);
        ::BitBlt(TheHDC,x,y,width,height,offDC,0,0,SRCPAINT);
      }
    } 
    ::SelectObject(maskDC,oldMaskBits);
    ::DeleteObject(maskBits);
    ::DeleteObject(maskDC);
  }

  ::SelectObject(offDC,oldBits);
  ::DeleteObject(tileBits);
  ::DeleteObject(offDC);

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *  @update 3/16/00 dwc
 */
PRBool 
nsImageWin :: PatBltTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                                nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,
                                nscoord aWidth, nscoord aHeight)
{
  HDC     TheHDC;
  HBRUSH  hBrush,oldBrush;
  BOOL    success,problem=FALSE;
  DWORD   rop;
  HBITMAP theBits;
  POINT   originalPoint;

  if (PR_FALSE==mCanOptimize) {
    return (PR_FALSE);
  }
  
  if (nsnull==mHBitmap) {
    CreateDDB(aSurface);
  }

  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);
  if (NULL == TheHDC){
    return (PR_FALSE);
  }

  // default copy mode
  rop = PATCOPY;


  // we have to reset the origin here..
  ::SetBrushOrgEx(TheHDC,aX0,aY0,&originalPoint);

  // if there is an alpha layer, lay down the mask first
  if (1==mAlphaDepth){
    ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);
    theBits = ::CreateBitmap(mAlphaWidth,mAlphaHeight,1,1,NULL);
    MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);
    SetDIBits(TheHDC,theBits,0,mAlphaHeight,mAlphaBits,(LPBITMAPINFO)&bmi,DIB_RGB_COLORS);
    hBrush = CreatePatternBrush(theBits);
    oldBrush = (HBRUSH)SelectObject(TheHDC,hBrush);
    success = PatBlt( TheHDC, aX0,aY0,aX1-aX0,aY1-aY0,0xA000C9);
    SelectObject(TheHDC,oldBrush);
    DeleteObject(hBrush);
    DeleteObject(theBits);
    rop = 0xFA0089;
  }

  // do a pattern blit
  if (mHBitmap == NULL) {
    problem = TRUE;
  }

  hBrush = CreatePatternBrush(mHBitmap);
  oldBrush = (HBRUSH)SelectObject(TheHDC,hBrush);
  success = PatBlt( TheHDC, aX0,aY0,aX1-aX0,aY1-aY0,rop);
  SelectObject(TheHDC,oldBrush);
  DeleteObject(hBrush);

  ::SetBrushOrgEx(TheHDC,originalPoint.x,originalPoint.y,NULL);

  return (PR_TRUE);
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
 * Create an optimized bitmap, -- this routine may need to be deleted, not really used now
 * @update dc - 11/20/98
 * @param aContext - The device context to use for the optimization
 */
nsresult 
nsImageWin :: Optimize(nsIDeviceContext* aContext)
{
  // Just set the flag sinze we may not have a valid HDC, like at startup, but at drawtime we do have a valid HDC
  if ((8 != mAlphaDepth) || CanAlphaBlend())
    mCanOptimize = PR_TRUE;      
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

void
nsImageWin::CleanUpDIBSection()
{
  if (mDIBSection != nsnull) {
    ::DeleteObject(mDIBSection);
    mDIBSection = nsnull;
    mImageBits = nsnull;
  }
  if (mAlphaBits != nsnull) {
    delete [] mAlphaBits;
    mAlphaBits = nsnull;
  }
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

  if (mBHead) {
    delete[] mBHead;
    mBHead = nsnull;
  }

  mCanOptimize = PR_FALSE;
  mIsOptimized = PR_FALSE;

}

/** ----------------------------------------------------------------
 * See documentation in nsIImage.h
 * @update - dwc 5/20/99
 * @return the result of the operation, if NS_OK, then the pixelmap is unoptimized
 */
nsresult 
nsImageWin::PrintDDB(nsDrawingSurface aSurface,PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,PRUint32 aROP)
{
  HDC   theHDC;
  UINT  palType;

  if (mIsOptimized == PR_TRUE){
    if (mHBitmap != nsnull){
      ConvertDDBtoDIB(aWidth,aHeight);
      ((nsDrawingSurfaceWin *)aSurface)->GetDC(&theHDC);

      if (mBHead->biBitCount == 8) {
        palType = DIB_PAL_COLORS;
      } else {
        palType = DIB_RGB_COLORS;
      }

      ::StretchDIBits(theHDC, aX, aY, aWidth, aHeight,
          0, 0, mBHead->biWidth, mBHead->biHeight, mImageBits,(LPBITMAPINFO)mBHead,palType, aROP);

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
 * @return the result of the operation, if NS_OK, then the pixelmap is unoptimized
 */
nsresult 
nsImageWin::ConvertDDBtoDIB(PRInt32 aWidth, PRInt32 aHeight)
{
  PRInt32             numbytes,tWidth,tHeight;
  BITMAP              srcinfo;
  HBITMAP             oldbits;
  HDC                 memPrDC;
  UINT                palType;

  if (mIsOptimized == PR_TRUE){

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

      numbytes = ::GetDIBits(memPrDC,mHBitmap,0,srcinfo.bmHeight,mImageBits,(LPBITMAPINFO)mBHead,palType);
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
 *  Set the decoded dimens of the image
 */
NS_IMETHODIMP
nsImageWin::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2)
{
  mDecodedX1 = x1; 
  mDecodedY1 = y1; 
  mDecodedX2 = x2; 
  mDecodedY2 = y2; 
  return NS_OK;
}

/** ---------------------------------------------------
 *  A bit blitter to tile images to the background recursively
 *  @update 9/9/2000 dwc
 *  @param aTheHDC -- HDC to render to
 *  @param aSrcRect -- the size of the tile we are building
 *  @param aWidth -- max width allowed for the  HDC
 *  @param aHeight -- max height allowed for the HDC
 */
void
nsImageWin::BuildTile(HDC TheHDC,nsRect &aSrcRect,PRInt16 aWidth,PRInt16 aHeight,PRInt32  aCopyMode)
{
  nsRect  destRect;
  
  if (aSrcRect.width < aWidth) {
    // width is less than double so double our source bitmap width
    destRect = aSrcRect;
    destRect.x += aSrcRect.width;
    ::BitBlt(TheHDC,destRect.x,destRect.y,destRect.width,destRect.height,TheHDC,aSrcRect.x,aSrcRect.y,aCopyMode);
    aSrcRect.width*=2;
    this->BuildTile(TheHDC,aSrcRect,aWidth,aHeight,aCopyMode);
  }else if (aSrcRect.height < aHeight) {
    // height is less than double so double our source bitmap height
    destRect = aSrcRect;
    destRect.y += aSrcRect.height;
    ::BitBlt(TheHDC,destRect.x,destRect.y,destRect.width,destRect.height,TheHDC,aSrcRect.x,aSrcRect.y,aCopyMode);
    aSrcRect.height*=2;
    this->BuildTile(TheHDC,aSrcRect,aWidth,aHeight,aCopyMode);
  } 
}

/**
 * Get a pointer to the bits for the pixelmap. Will convert to DIB if
 * only stored in optimized HBITMAP form.  Using this routine will
 * set the mDIBTemp flag to true so the next unlock will destroy this memory
 *
 * @return address of the DIB pixel array
 */

PRUint8*
nsImageWin::GetBits()
{
  // if mImageBits did not exist.. then
  if (!mImageBits) {
    ConvertDDBtoDIB(GetWidth(), GetHeight());
    mDIBTemp = PR_TRUE;   // only set to true if the DIB is being created here as temporary
  }

  return mImageBits;

} // GetBits

#ifdef USE_IMG2
NS_IMETHODIMP nsImageWin::DrawToImage(nsIImage* aDstImage, nscoord aDX, nscoord aDY, nscoord aDWidth, nscoord aDHeight)
{
  PRInt32 canRaster,srcy;
  HBITMAP oldSrcBits, oldDstBits;
  DWORD   rop;
  PRInt32 origDHeight = aDHeight;
  PRInt32 origDWidth = aDWidth;

  if (mBHead == nsnull || aDWidth < 0 || aDHeight < 0) 
    return NS_ERROR_FAILURE;

  if (0 == aDWidth || 0 == aDHeight)
    return NS_OK;

  // Translate to bottom-up coordinates for the source bitmap
  srcy = mBHead->biHeight - mNaturalHeight;

  // Create a memory DC that is compatible with the screen
  HDC dstMemDC = ::CreateCompatibleDC(nsnull);

  nsImageWin* imgWin = NS_STATIC_CAST(nsImageWin*, aDstImage); 
  if (!imgWin->mHBitmap)
  {
    if (imgWin->mSizeImage > 0) {
      void* bits;
      imgWin->mHBitmap = CreateDIBSection(dstMemDC,(LPBITMAPINFO)imgWin->mBHead,DIB_RGB_COLORS,&bits,nsnull,nsnull);
      imgWin->mDIBSection = imgWin->mHBitmap;
      oldDstBits = (HBITMAP)::SelectObject(dstMemDC, imgWin->mHBitmap);

      rop = SRCCOPY;

      if (nsnull != imgWin->mAlphaBits) {
        if (1==imgWin->mAlphaDepth) {
          MONOBITMAPINFO  bmi(imgWin->mAlphaWidth, imgWin->mAlphaHeight);

          ::StretchDIBits(dstMemDC, 0, 0, aDWidth, aDHeight,0, 0,mNaturalWidth, mNaturalHeight, imgWin->mAlphaBits,
                                        (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
           rop = SRCPAINT;
        }
      }

      if (8==imgWin->mAlphaDepth) {              
        DrawComposited(dstMemDC, aDX, aDY, aDWidth, aDHeight,0, 0, mNaturalHeight, mNaturalWidth);
      } else {
        ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight,0, 0, mNaturalWidth, mNaturalHeight, imgWin->mImageBits,
          (LPBITMAPINFO)imgWin->mBHead, DIB_RGB_COLORS, rop);
      }
      imgWin->mIsOptimized = PR_TRUE;
      imgWin->CleanUpDIB();
      imgWin->mImageBits = (PRUint8*)bits;
    }
  } else
    oldDstBits = (HBITMAP)::SelectObject(dstMemDC, imgWin->mHBitmap);

#if 0
  if (!IsOptimized() || nsnull==mHBitmap)
#endif
  {
    rop = SRCCOPY;

    if (nsnull != mAlphaBits) {
      if (1==mAlphaDepth) {
        MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

        ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight,0, 0,mNaturalWidth, mNaturalHeight, mAlphaBits,
                        (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
        rop = SRCPAINT;
      }
    }

    if (8==mAlphaDepth) {
      DrawComposited(dstMemDC, aDX, aDY, aDWidth, aDHeight, 0, 0, mNaturalHeight, mNaturalWidth);
    } else {
      ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight,0, 0, mNaturalWidth, mNaturalHeight, mImageBits,
                      (LPBITMAPINFO)mBHead, DIB_RGB_COLORS, rop);
    }
  }
#if 0
  else{
    rop = SRCCOPY;
    if (1==mAlphaDepth && mAlphaBits){
      MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

      ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight,0, 0, mNaturalWidth, mNaturalHeight, mAlphaBits,
                      (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
      rop = SRCPAINT;
      ::StretchDIBits(dstMemDC, aDX, aDY, aDWidth, aDHeight,0, 0, mNaturalWidth, mNaturalHeight, mHBitmap,
                      (LPBITMAPINFO)mBHead, DIB_RGB_COLORS, rop);
    } else if (8 == mAlphaDepth && mAlphaBits) {
      BLENDFUNCTION blendFunction;
      blendFunction.BlendOp = AC_SRC_OVER;
      blendFunction.BlendFlags = 0;
      blendFunction.SourceConstantAlpha = 255;
      blendFunction.AlphaFormat = 1; //AC_SRC_ALPHA*
      gAlphaBlend(dstMemDC, aDX, aDY, aDWidth, aDHeight, dstMemDC, 0, 0, mNaturalWidth, mNaturalHeight, blendFunction);
    } else {
      ::StretchBlt(dstMemDC, aDX, aDY, aDWidth, aDHeight, dstMemDC, 0, 0, mNaturalWidth, mNaturalHeight, rop);
    }
  }
#endif

  ::SelectObject(dstMemDC, oldDstBits);
  ::DeleteDC(dstMemDC);

  return NS_OK;
}

#endif


/** ---------------------------------------------------
 *  copy the mask and the image to the passed in DC, do the raster operation in memory before going to the 
 *  printer
 */
void 
CompositeBitsInMemory(HDC aTheHDC, int aDX, int aDY, int aDWidth, int aDHeight,
                    int aSX, int aSY, int aSWidth, int aSHeight,PRInt32 aSrcy,
                    PRUint8 *aAlphaBits,MONOBITMAPINFO  *aBMI,PRUint8* aImageBits,LPBITMAPINFOHEADER aBHead,PRInt16 aNumPaletteColors)
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
        ::StretchDIBits(memDC, 0, 0, aSWidth, aSHeight,aSX, aSrcy,aSWidth, aSHeight, aAlphaBits,
                                                (LPBITMAPINFO)aBMI, DIB_RGB_COLORS, SRCCOPY); 

        // paint in the image
        ::StretchDIBits(memDC, 0, 0, aSWidth, aSHeight,aSX, aSrcy, aSWidth, aSHeight, aImageBits,
                (LPBITMAPINFO)aBHead, 256 == aNumPaletteColors ? DIB_PAL_COLORS :DIB_RGB_COLORS, SRCPAINT);



        // output the composed image
        ::StretchDIBits(aTheHDC, aDX, aDY, aDWidth, aDHeight,aSX, aSrcy, aSWidth, aSHeight, screenBits,
                (LPBITMAPINFO)&offbmi, 256 == aNumPaletteColors ? DIB_PAL_COLORS : DIB_RGB_COLORS, SRCCOPY);


        ::SelectObject(memDC, oldBitmap);
      }
      ::DeleteObject(tmpBitmap);
    }
    ::DeleteObject(memDC);
  }
}
