/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsImageWin.h"
#include "nsRenderingContextWin.h"


static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

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
  mAlphaBits = nsnull;
  mAlphaDepth = nsnull;
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

/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *	@update 3/27/00 dwc
 */
nsresult nsImageWin :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
	mHBitmap = nsnull;
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

  SetDecodedRect(0,0,0,0);  //init

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
 *	@update 3/27/00 dwc
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

// Raster op used with MaskBlt(). Assumes our transparency mask has 0 for the
// opaque pixels and 1 for the transparent pixels. That means we want the
// background raster op (value of 0) to be SRCCOPY, and the foreground raster
// (value of 1) to just use the destination bits
#define MASKBLT_ROP MAKEROP4((DWORD)0x00AA0029, SRCCOPY)


/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *	@update 3/27/00 dwc
 */
void 
nsImageWin :: CreateDDB(nsDrawingSurface aSurface)
{
  HDC TheHDC;

  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);

  if (TheHDC != NULL){
    if (mSizeImage > 0){
      mHBitmap = ::CreateDIBitmap(TheHDC, mBHead, CBM_INIT, mImageBits, (LPBITMAPINFO)mBHead,
				  256 == mNumPaletteColors ? DIB_PAL_COLORS : DIB_RGB_COLORS);
      mIsOptimized = PR_TRUE;

      CleanUp(PR_FALSE);
    }
    ((nsDrawingSurfaceWin *)aSurface)->ReleaseDC();
  }
}

/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h  
 *	@update 3/27/00 dwc
 */
NS_IMETHODIMP 
nsImageWin :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				          PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
				          PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{

HDC     TheHDC;
PRInt32 canRaster,srcHeight,srcy;
HBITMAP oldBits;
DWORD   rop;

  if (mBHead == nsnull) 
    return NS_ERROR_FAILURE;

  // limit the size of the blit to the amount of the image read in
  srcHeight = mBHead->biHeight;
  srcy = 0;
  if((mDecodedY2 < srcHeight)) {
    aDHeight = PRInt32 (float(mDecodedY2/float(srcHeight))*aDHeight);
    srcHeight = mDecodedY2;
    srcy = (mBHead->biHeight-mDecodedY2);
  }

  // if DC is not for a printer, and the image can be optimized, 
  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);

  // find out if the surface is a printer.
  ((nsDrawingSurfaceWin *)aSurface)->GetTECHNOLOGY(&canRaster);
  if(canRaster != DT_RASPRINTER){
    if (mCanOptimize && (nsnull == mHBitmap))
      CreateDDB(aSurface);
  }

  if (nsnull != TheHDC){
    if (!IsOptimized() || nsnull==mHBitmap){
      rop = SRCCOPY;

      if (nsnull != mAlphaBits){
        if( 1==mAlphaDepth){
	        MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

	        ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,aSX, aSY,aSWidth, aSHeight, mAlphaBits,
			                                    (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
	         rop = SRCPAINT;
        }
        else if( 8==mAlphaDepth){              
            ALPHA8BITMAPINFO bmi(mAlphaWidth, mAlphaHeight);

          	::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,aSX, aSY, aSWidth, aSHeight, mAlphaBits,
	     	    	                            (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
       	     rop = SRCPAINT;

        }
      }

      ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,aSX, srcy, aSWidth, srcHeight, mImageBits,
		                                  (LPBITMAPINFO)mBHead, 256 == mNumPaletteColors ? DIB_PAL_COLORS :
		      DIB_RGB_COLORS, rop);

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
          if( 1==mAlphaDepth){
	          MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

	          ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,aSX, aSY, aSWidth, aSHeight, mAlphaBits,
			                                    (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
	           rop = SRCPAINT;
          }
          else if( 8==mAlphaDepth){              
              ALPHA8BITMAPINFO bmi(mAlphaWidth, mAlphaHeight);

          	  ::StretchDIBits(TheHDC, aDX, aDY, aDWidth, aDHeight,aSX, aSY, aSWidth, aSHeight, mAlphaBits,
	     	    	                                (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
       	       rop = SRCPAINT;

          }
        }
        // if this is for a printer.. we have to convert it back to a DIB
        if(canRaster == DT_RASPRINTER){
          if(!(GetDeviceCaps(TheHDC,RASTERCAPS) &(RC_BITBLT | RC_STRETCHBLT))) {
              // we have an error with the printer not supporting a raster device
          } else {
            // if we did not convert to a DDB already
            if (nsnull == mHBitmap) {
              oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
              ::StretchBlt(TheHDC, aDX, aDY, aDWidth, aDHeight, srcDC, aSX, srcy,aSWidth, srcHeight, rop);
            }else{
              PrintDDB(aSurface,aDX,aDY,aDWidth,aDHeight);
            }
          }
        } else {
          // we are going to the device that created this DDB
          oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
          ::StretchBlt(TheHDC,aDX,aDY,aDWidth,aDHeight,srcDC,aSX,srcy,aSWidth,srcHeight,rop);
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

/** ---------------------------------------------------
 *  See documentation in nsIImageWin.h
 *	@update 3/27/00 dwc
 */
NS_IMETHODIMP nsImageWin :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
HDC     TheHDC;
PRInt32 canRaster,srcHeight,srcy;
HBITMAP oldBits;
DWORD   rop;

  if (mBHead == nsnull) 
    return NS_ERROR_FAILURE;

  // limit the size of the blit to the amount of the image read in
  srcHeight = mBHead->biHeight;
  srcy = 0;
  if((mDecodedY2 < srcHeight)) {
    aHeight = PRInt32 (float(mDecodedY2/float(srcHeight))*aHeight);
    srcHeight = mDecodedY2;
    srcy = (mBHead->biHeight-mDecodedY2);
  }

  // if DC is not for a printer, and the image can be optimized, 
  ((nsDrawingSurfaceWin *)aSurface)->GetDC(&TheHDC);

  // find out if the surface is a printer.
  ((nsDrawingSurfaceWin *)aSurface)->GetTECHNOLOGY(&canRaster);
  if(canRaster != DT_RASPRINTER){
    if (mCanOptimize && (nsnull == mHBitmap))
      CreateDDB(aSurface);
  }

  if (nsnull != TheHDC){
    if (!IsOptimized() || nsnull==mHBitmap){
      rop = SRCCOPY;

      if (nsnull != mAlphaBits){
        if( 1==mAlphaDepth){
	        MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

	        ::StretchDIBits(TheHDC, aX, aY, aWidth, aHeight,0, 0, 
                    mAlphaWidth, mAlphaHeight, mAlphaBits,
			            (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
	         rop = SRCPAINT;
        }
        else if( 8==mAlphaDepth){              
            ALPHA8BITMAPINFO bmi(mAlphaWidth, mAlphaHeight);

          	::StretchDIBits(TheHDC, aX, aY, aWidth, aHeight,
	      	   	0, 0, mAlphaWidth, mAlphaHeight, mAlphaBits,
	     	    	(LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
       	     rop = SRCPAINT;

        }
      }

      ::StretchDIBits(TheHDC, aX, aY, aWidth, aHeight,
		      0, srcy, mBHead->biWidth, srcHeight, mImageBits,
		      (LPBITMAPINFO)mBHead, 256 == mNumPaletteColors ? DIB_PAL_COLORS :
		      DIB_RGB_COLORS, rop);

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
          if( 1==mAlphaDepth){
	          MONOBITMAPINFO  bmi(mAlphaWidth, mAlphaHeight);

	          ::StretchDIBits(TheHDC, aX, aY, aWidth, aHeight,0, 0, 
                      mAlphaWidth, mAlphaHeight, mAlphaBits,
			              (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
	           rop = SRCPAINT;
          }
          else if( 8==mAlphaDepth){              
              ALPHA8BITMAPINFO bmi(mAlphaWidth, mAlphaHeight);

          	  ::StretchDIBits(TheHDC, aX, aY, aWidth, aHeight,
	      	   	  0, 0, mAlphaWidth, mAlphaHeight, mAlphaBits,
	     	    	  (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, SRCAND);
       	       rop = SRCPAINT;

          }
        }
        // if this is for a printer.. we have to convert it back to a DIB
        if(canRaster == DT_RASPRINTER){
          if(!(GetDeviceCaps(TheHDC,RASTERCAPS) &(RC_BITBLT | RC_STRETCHBLT))) {
              // we have an error with the printer not supporting a raster device
          } else {
            // if we did not convert to a DDB already
            if (nsnull == mHBitmap) {
              oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
              ::StretchBlt(TheHDC, aX, aY, aWidth, aHeight, srcDC, 0, srcy,
		           mBHead->biWidth, srcHeight, rop);
            }else{
              PrintDDB(aSurface,aX,aY,aWidth,aHeight);
            }
          }
        } else {
          // we are going to the device that created this DDB
          oldBits = (HBITMAP)::SelectObject(srcDC, mHBitmap);
          ::StretchBlt(TheHDC,aX,aY,aWidth,aHeight,srcDC,0,srcy,mBHead->biWidth,srcHeight,rop);
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

/** ----------------------------------------------------------------
 * Create an optimezed bitmap, -- this routine may need to be deleted, not really used now
 * @update dc - 11/20/98
 * @param aContext - The device context to use for the optimization
 */
nsresult 
nsImageWin :: Optimize(nsIDeviceContext* aContext)
{
  // Just set the flag sinze we may not have a valid HDC, like at startup, but at drawtime we do have a valid HDC
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

/** ----------------------------------------------------------------
 * Clean up the memory associted with this object.  Has two flavors, 
 * one mode is to clean up everything, the other is to clean up only the DIB information
 * @update dc - 11/20/98
 * @param aWidth - The width to calulate the number of bytes from
 * @return Number of bytes for the span
 */
void 
nsImageWin :: CleanUp(PRBool aCleanUpAll)
{
	// this only happens when we need to clean up everything
  if (aCleanUpAll == PR_TRUE){
	  if (mHBitmap != nsnull) {
      ::DeleteObject(mHBitmap);
      mHBitmap = nsnull;
    }

    if(mBHead){
      delete[] mBHead;
      mBHead = nsnull;
    }

    mCanOptimize = PR_FALSE;
    mIsOptimized = PR_FALSE;
  }

  // The following happens whenever Cleanup is called.. this can happen
  // when the entire object is being deleted, or optimized
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

	mNumPaletteColors = -1;
  mNumBytesPixel = 0;
	mSizeImage = 0;
}

/** ----------------------------------------------------------------
 * See documentation in nsIImage.h
 * @update - dwc 5/20/99
 * @return the result of the operation, if NS_OK, then the pixelmap is unoptimized
 */
nsresult 
nsImageWin::PrintDDB(nsDrawingSurface aSurface,PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
HDC theHDC;

  if (mIsOptimized == PR_TRUE){
    if (mHBitmap != nsnull){
      ConvertDDBtoDIB(aWidth,aHeight);
      ((nsDrawingSurfaceWin *)aSurface)->GetDC(&theHDC);
      ::StretchDIBits(theHDC, aX, aY, aWidth, aHeight,
		      0, 0, mBHead->biWidth, mBHead->biHeight, mImageBits,
		      (LPBITMAPINFO)mBHead, 256 == mNumPaletteColors ? DIB_PAL_COLORS :
		      DIB_RGB_COLORS, SRCCOPY);

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
PRInt32             numbytes;
BITMAP              srcinfo;
HDC                 memPrDC;

  if (mIsOptimized == PR_TRUE){

	  if (mHBitmap != nsnull){
      memPrDC = ::CreateDC("DISPLAY",NULL,NULL,NULL);
      ::SelectObject(memPrDC,mHBitmap);

      numbytes = ::GetObject(mHBitmap,sizeof(BITMAP),&srcinfo);

      // put into a DIB
      BuildDIB(&mBHead,mBHead->biWidth,mBHead->biHeight,srcinfo.bmBitsPixel,&mNumBytesPixel);
      mRowBytes = CalcBytesSpan(mBHead->biWidth);
      mSizeImage = mRowBytes * mBHead->biHeight; // no compression

      // Allocate the image bits
      mImageBits = new unsigned char[mSizeImage];
      numbytes = ::GetDIBits(memPrDC,mHBitmap,0,srcinfo.bmHeight,mImageBits,(LPBITMAPINFO)mBHead,DIB_RGB_COLORS);
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
  } else if (24 == aDepth) {
    numPaletteColors = 0;
    *aNumBytesPix = 3;
  } else if (16 == aDepth) {
    *aNumBytesPix = 2;  
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
 *	Lock down the image pixels
 */
NS_IMETHODIMP
nsImageWin::LockImagePixels(PRBool aMaskPixels)
{
    /*
  if (!mHBitmap)
    return NS_ERROR_NOT_INITIALIZED;
  
   ... and do Windows locking of image pixels here, if necessary
*/
	return NS_OK;
}

/** ---------------------------------------------------
 *	Unlock the pixels
 */
NS_IMETHODIMP
nsImageWin::UnlockImagePixels(PRBool aMaskPixels)
{
    /*
  if (!mHBitmap)
    return NS_ERROR_NOT_INITIALIZED;
  
  if (aMaskPixels && !mAlphamHBitmap)
  	return NS_ERROR_NOT_INITIALIZED;

   ... and do Windows unlocking of image pixels here, if necessary
    */
	return NS_OK;
}
/** ---------------------------------------------------
 *	Set the decoded dimens of the image
 */
NS_IMETHODIMP
nsImageWin::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2 )
{
    
  mDecodedX1 = x1; 
  mDecodedY1 = y1; 
  mDecodedX2 = x2; 
  mDecodedY2 = y2; 
  return NS_OK;
}
