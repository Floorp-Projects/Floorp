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

#include "nsBlender.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
nsBlender :: nsBlender()
{
  NS_INIT_REFCNT();

  mContext = nsnull;
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
nsBlender::~nsBlender() 
{
  NS_IF_RELEASE(mContext);
}

NS_IMPL_ISUPPORTS1(nsBlender, nsIBlender);

//------------------------------------------------------------

// need to set up some masks for 16 bit blending
// Use compile-time constants where possible for marginally smaller
// footprint (don't need fields in nsBlender) but also for faster code
#ifdef XP_MAC

#define BLEND_RED_MASK        0x7c00
#define BLEND_GREEN_MASK      0x03e0
#define BLEND_BLUE_MASK       0x001f
#define BLEND_RED_SET_MASK    0xf8
#define BLEND_GREEN_SET_MASK  0xf8
#define BLEND_BLUE_SET_MASK   0xf8
#define BLEND_RED_SHIFT       7
#define BLEND_GREEN_SHIFT     2
#define BLEND_BLUE_SHIFT      3

#else  // XP_WIN, XP_UNIX, ???

#define BLEND_RED_MASK        0xf800
#define BLEND_GREEN_MASK      0x07e0
#define BLEND_BLUE_MASK       0x001f
#define BLEND_RED_SET_MASK    0xf8
#define BLEND_GREEN_SET_MASK  0xfC
#define BLEND_BLUE_SET_MASK   0xf8
#define BLEND_RED_SHIFT       8
#define BLEND_GREEN_SHIFT     3
#define BLEND_BLUE_SHIFT      3

#endif

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
NS_IMETHODIMP
nsBlender::Init(nsIDeviceContext *aContext)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);
  
  return NS_OK;
}

static void rangeCheck(nsIDrawingSurface* surface, PRInt32& aX, PRInt32& aY, PRInt32& aWidth, PRInt32& aHeight)
{
  PRUint32 width, height;
  surface->GetDimensions(&width, &height);
  
  // ensure that the origin is within bounds of the drawing surface.
  if (aX < 0)
    aX = 0;
  else if (aX > (PRInt32)width)
    aX = width;
  if (aY < 0)
    aY = 0;
  else if (aY > (PRInt32)height)
    aY = height;
  
  // ensure that the dimensions are within bounds.
  if (aX + aWidth > (PRInt32)width)
    aWidth = width - aX;
  if (aY + aHeight > (PRInt32)height)
    aHeight = height - aY;
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
NS_IMETHODIMP
nsBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsDrawingSurface aSrc,
                 nsDrawingSurface aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                 nsDrawingSurface aSecondSrc, nscolor aSrcBackColor,
                 nscolor aSecondSrcBackColor)
{
  if (aSecondSrc) {
    // the background color options are obsolete and should be removed.
    NS_ASSERTION(aSrcBackColor == NS_RGB(0, 0, 0),
      "Background color for primary source must be black");
    NS_ASSERTION(aSecondSrcBackColor == NS_RGB(255, 255, 255),
      "Background color for secondary source must be white");
    if (aSrcBackColor != NS_RGB(0, 0, 0) ||
        aSecondSrcBackColor != NS_RGB(255, 255, 255)) {
      // disable multi-buffer blending; pretend the primary buffer
      // is all opaque pixels
      aSecondSrc = nsnull;
    }
  }
 
  nsresult result = NS_ERROR_FAILURE;

  nsIDrawingSurface* srcSurface = (nsIDrawingSurface *)aSrc;
  nsIDrawingSurface* destSurface = (nsIDrawingSurface *)aDst;
  nsIDrawingSurface* secondSrcSurface = (nsIDrawingSurface *)aSecondSrc;

  // range check the coordinates in both the source and destination buffers.
  rangeCheck(srcSurface, aSX, aSY, aWidth, aHeight);
  rangeCheck(destSurface, aDX, aDY, aWidth, aHeight);

  PRUint8* srcBytes = nsnull;
  PRUint8* secondSrcBytes = nsnull;
  PRUint8* destBytes = nsnull;
  PRInt32 srcSpan, destSpan, secondSrcSpan;
  PRInt32 srcRowBytes, destRowBytes, secondSrcRowBytes;

  if (NS_OK == srcSurface->Lock(aSX, aSY, aWidth, aHeight, (void**)&srcBytes, &srcRowBytes, &srcSpan, NS_LOCK_SURFACE_READ_ONLY)) {
    if (NS_OK == destSurface->Lock(aDX, aDY, aWidth, aHeight, (void**)&destBytes, &destRowBytes, &destSpan, 0)) {
      NS_ASSERTION(srcSpan == destSpan, "Mismatched bitmap formats (src/dest) in Blender");
      if (srcSpan == destSpan) {

        if (secondSrcSurface) {
          if (NS_OK == secondSrcSurface->Lock(aSX, aSY, aWidth, aHeight, (void**)&secondSrcBytes, &secondSrcRowBytes, &secondSrcSpan, NS_LOCK_SURFACE_READ_ONLY)) {
            NS_ASSERTION(srcSpan == secondSrcSpan && srcRowBytes == secondSrcRowBytes,
                         "Mismatched bitmap formats (src/secondSrc) in Blender");
            if (srcSpan != secondSrcSpan || srcRowBytes != secondSrcRowBytes) {
              // disable second source if there's a format mismatch
              secondSrcBytes = nsnull;
            }
          } else {
            // failed to lock. So, pretend it was never there.
            secondSrcSurface = nsnull;
            secondSrcBytes = nsnull;
          }
        }

        result = Blend(srcBytes, srcRowBytes,
                       destBytes, destRowBytes,
                       secondSrcBytes,
                       srcSpan, aHeight, aSrcOpacity);

        if (secondSrcSurface) {
          secondSrcSurface->Unlock();
        }
      }

      destSurface->Unlock();
    }

    srcSurface->Unlock();
  }

  return result;
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
NS_IMETHODIMP nsBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsIRenderingContext *aSrc,
                               nsIRenderingContext *aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                               nsIRenderingContext *aSecondSrc, nscolor aSrcBackColor,
                               nscolor aSecondSrcBackColor)
{
  // just hand off to the drawing surface blender, to make code easier to maintain.
  nsDrawingSurface srcSurface, destSurface, secondSrcSurface = nsnull;
  aSrc->GetDrawingSurface(&srcSurface);
  aDest->GetDrawingSurface(&destSurface);
  if (aSecondSrc != nsnull)
    aSecondSrc->GetDrawingSurface(&secondSrcSurface);
  return Blend(aSX, aSY, aWidth, aHeight, srcSurface, destSurface,
               aDX, aDY, aSrcOpacity, secondSrcSurface, aSrcBackColor,
               aSecondSrcBackColor);
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
nsresult nsBlender::Blend(PRUint8 *aSrcBits, PRInt32 aSrcStride,
                          PRUint8 *aDestBits, PRInt32 aDestStride,
                          PRUint8 *aSecondSrcBits,
                          PRInt32 aSrcBytes, PRInt32 aLines, float aOpacity)
{
  nsresult result = NS_OK;
  PRUint32 depth;
  mContext->GetDepth(depth);
  // now do the blend
  switch (depth){
    case 32:
        Do32Blend(aOpacity, aLines, aSrcBytes, aSrcBits, aDestBits,
                  aSecondSrcBits, aSrcStride, aDestStride, nsHighQual);
        break;

    case 24:
        Do24Blend(aOpacity, aLines, aSrcBytes, aSrcBits, aDestBits,
                  aSecondSrcBits, aSrcStride, aDestStride, nsHighQual);
        break;

    case 16:
        Do16Blend(aOpacity, aLines, aSrcBytes, aSrcBits, aDestBits,
                  aSecondSrcBits, aSrcStride, aDestStride, nsHighQual);
        break;

    case 8:
    {
#if 0
      IL_ColorSpace *thespace = nsnull;

      if ((result = mContext->GetILColorSpace(thespace)) == NS_OK) {
        Do8Blend(aOpacity, aLines, aSrcBytes, aSrcBits, aDestBits,
          aSecondSrcBits, aSrcStride, aDestStride, thespace,
          nsHighQual);
        IL_ReleaseColorSpace(thespace);
      }
#endif
      break;
    }
  }

  return result;
}

/**
  This is the simple case where the opacity == 1.0. We just copy the pixels.
*/
static void DoOpaqueBlend(PRInt32 aNumLines, PRInt32 aNumBytes,
                          PRUint8 *aSImage, PRUint8 *aDImage,
                          PRInt32 aSLSpan, PRInt32 aDLSpan)
{
  PRIntn y;
  for (y = 0; y < aNumLines; y++) {
    nsCRT::memcpy(aDImage, aSImage, aNumBytes);
    aSImage += aSLSpan;
    aDImage += aDLSpan;
  }
}

/**
   This is the case where we have a single source buffer, all of whose
   pixels have an alpha value of 1.0.

   Here's how to get the formula for calculating new destination pixels:

   There's a destination pixel whose current color is D (0 <= D <= 255).
   We are required to find the new color value for the destination pixel, call it X.
   We are given S, the color value of the source pixel (0 <= S <= 255).
   We are also given P, the opacity to blend with (0 < P < 256).

   Note that we have deliberately defined P's "opaque" range bound to be 256
   and not 255. This considerably speeds up the code.

   Then we have the equation
     X = D*(1 - P/256) + S*(P/256)

   Rearranging gives
     X = D + ((S - D)*P)/256

   This form minimizes the number of integer multiplications, which are much more
   expensive than shifts, adds and subtractions on most processors.
*/
static void DoSingleImageBlend(PRUint32 aOpacity256, PRInt32 aNumLines, PRInt32 aNumBytes,
                               PRUint8 *aSImage, PRUint8 *aDImage,
                               PRInt32 aSLSpan, PRInt32 aDLSpan)
{
  PRIntn y;

  for (y = 0; y < aNumLines; y++) {
    PRUint8 *s2 = aSImage;
    PRUint8 *d2 = aDImage;
    
    PRIntn i;
    for (i = 0; i < aNumBytes; i++) {
      PRUint32 destPix = *d2;
      
      *d2 = (PRUint8)(destPix + (((*s2 - destPix)*aOpacity256) >> 8));
      
      d2++;
      s2++;
    }
    
    aSImage += aSLSpan;
    aDImage += aDLSpan;
  }
}

/**
   After disposing of the simpler cases, here we have two source buffers.

   So here's how to get the formula for calculating new destination pixels:

   If the source pixel is the same in each buffer, then the pixel was painted with
   alpha=1.0 and we use the formula from above to compute the destination pixel.
   However, if the source pixel is different in each buffer (and is not just the
   background color for each buffer, which indicates alpha=0.0), then the pixel was
   painted with some partial alpha value and we need to (at least implicitly) recover
   that alpha value along with the actual color value. So...

   There's a destination pixel whose current color is D (0 <= D <= 255).
   We are required to find the new color value for the destination pixel, call it X.
   We are given S, the color value of the source pixel painted onto black (0 <= S <= 255).
   We are given T, the color value of the source pixel painted onto white (0 <= T <= 255).
   We are also given P, the opacity to blend with (0 < P < 256).

   Let A be the alpha value the source pixel was painted with (0 <= A <= 255).
   Let C be the color value of the source pixel (0 <= C <= 255).

   Note that even though (as above) we set the "opaque" range bound for P at 256,
   we set the "opaque" range bound for A at 255. This turns out to simplify the formulae
   and speed up the code.

   Then we have the equations
     S = C*(A/255)
     T = 255*(1 - A/255) + C*(A/255)
     X = D*(1 - (A/255)*(P/256)) + C*(A/255)*(P/256)

   Rearranging and crunching the algebra gives
     X = D + ((S - (D*(255 + S - T))/255)*P)/256

   This is the simplest form I could find. Apart from the two integer multiplies,
   which I think are minimal, the most troublesome part is the division by 255,
   but I have a fast way to do that (for numbers in the range encountered) using
   two adds and two shifts.
*/
void
nsBlender::Do32Blend(float aOpacity, PRInt32 aNumLines, PRInt32 aNumBytes,
                     PRUint8 *aSImage, PRUint8 *aDImage, PRUint8 *aSecondSImage,
                     PRInt32 aSLSpan, PRInt32 aDLSpan, nsBlendQuality aBlendQuality)
{
  /* Use alpha ranging from 0 to 256 inclusive. This means that we get accurate
     results when we divide by 256. */
  PRUint32 opacity256 = (PRUint32)(aOpacity*256);

  // Handle simpler cases
  if (opacity256 <= 0) {
    return;
  } else if (opacity256 >= 256) {
    DoOpaqueBlend(aNumLines, aNumBytes, aSImage, aDImage, aSLSpan, aDLSpan);
    return;
  } else if (nsnull == aSecondSImage) {
    DoSingleImageBlend(opacity256, aNumLines, aNumBytes, aSImage, aDImage, aSLSpan, aDLSpan);
    return;
  }

  PRIntn numPixels = aNumBytes/4;

  PRIntn y;
  for (y = 0; y < aNumLines; y++) {
    PRUint8 *s2 = aSImage;
    PRUint8 *d2 = aDImage;
    PRUint8 *ss2 = aSecondSImage;

    PRIntn x;
    for (x = 0; x < numPixels; x++) {
      PRUint32 pixSColor  = *((PRUint32*)(s2))&0xFFFFFF;
      PRUint32 pixSSColor = *((PRUint32*)(ss2))&0xFFFFFF;
      
      if ((pixSColor != 0x000000) || (pixSSColor != 0xFFFFFF)) {
        if (pixSColor != pixSSColor) {
          PRIntn i;
          // the original source pixel was alpha-blended into the background.
          // We have to extract the original alpha and color value.
          for (i = 0; i < 4; i++) {
            PRUint32 destPix = *d2;
            PRUint32 onBlack = *s2;
            PRUint32 imageAlphaTimesDestPix = (255 + onBlack - *ss2)*destPix;
            PRUint32 adjustedDestPix;
            FAST_DIVIDE_BY_255(adjustedDestPix, imageAlphaTimesDestPix);
            
            *d2 = (PRUint8)(destPix + (((onBlack - adjustedDestPix)*opacity256) >> 8));
            
            d2++;
            s2++;
            ss2++;
          }
        } else {
          PRIntn i;
          for (i = 0; i < 4; i++) {
            PRUint32 destPix = *d2;
            PRUint32 onBlack = *s2;
            
            *d2 = (PRUint8)(destPix + (((onBlack - destPix)*opacity256) >> 8));
            
            d2++;
            s2++;
          }

          ss2 += 4;
        }
      } else {
        d2 += 4;
        s2 += 4;
        ss2 += 4;
      }
    }
    
    aSImage += aSLSpan;
    aDImage += aDLSpan;
    aSecondSImage += aSLSpan;
  }
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
void
nsBlender::Do24Blend(float aOpacity, PRInt32 aNumLines, PRInt32 aNumBytes,
                     PRUint8 *aSImage, PRUint8 *aDImage, PRUint8 *aSecondSImage,
                     PRInt32 aSLSpan, PRInt32 aDLSpan, nsBlendQuality aBlendQuality)
{
  /* Use alpha ranging from 0 to 256 inclusive. This means that we get accurate
     results when we divide by 256. */
  PRUint32 opacity256 = (PRUint32)(aOpacity*256);

  // Handle simpler cases
  if (opacity256 <= 0) {
    return;
  } else if (opacity256 >= 256) {
    DoOpaqueBlend(aNumLines, aNumBytes, aSImage, aDImage, aSLSpan, aDLSpan);
    return;
  } else if (nsnull == aSecondSImage) {
    DoSingleImageBlend(opacity256, aNumLines, aNumBytes, aSImage, aDImage, aSLSpan, aDLSpan);
    return;
  }

  PRIntn numPixels = aNumBytes/3;

  PRIntn y;
  for (y = 0; y < aNumLines; y++) {
    PRUint8 *s2 = aSImage;
    PRUint8 *d2 = aDImage;
    PRUint8 *ss2 = aSecondSImage;

    PRIntn x;
    for (x = 0; x < numPixels; x++) {
      PRUint32 pixSColor  = s2[0] | (s2[1] << 8) | (s2[2] << 16);
      PRUint32 pixSSColor = ss2[0] | (ss2[1] << 8) | (ss2[2] << 16);
      
      if ((pixSColor != 0x000000) || (pixSSColor != 0xFFFFFF)) {
        if (pixSColor != pixSSColor) {
          PRIntn i;
          // the original source pixel was alpha-blended into the background.
          // We have to extract the original alpha and color value.
          for (i = 0; i < 3; i++) {
            PRUint32 destPix = *d2;
            PRUint32 onBlack = *s2;
            PRUint32 imageAlphaTimesDestPix = (255 + onBlack - *ss2)*destPix;
            PRUint32 adjustedDestPix;
            FAST_DIVIDE_BY_255(adjustedDestPix, imageAlphaTimesDestPix);
            
            *d2 = (PRUint8)(destPix + (((onBlack - adjustedDestPix)*opacity256) >> 8));
            
            d2++;
            s2++;
            ss2++;
          }
        } else {
          PRIntn i;
          for (i = 0; i < 3; i++) {
            PRUint32 destPix = *d2;
            PRUint32 onBlack = *s2;
            
            *d2 = (PRUint8)(destPix + (((onBlack - destPix)*opacity256) >> 8));
            
            d2++;
            s2++;
          }

          ss2 += 3;
        }
      } else {
        d2 += 3;
        s2 += 3;
        ss2 += 3;
      }
    }
    
    aSImage += aSLSpan;
    aDImage += aDLSpan;
    aSecondSImage += aSLSpan;
  }
}

//------------------------------------------------------------



#define RED16(x)    (((x) & BLEND_RED_MASK) >> BLEND_RED_SHIFT)
#define GREEN16(x)  (((x) & BLEND_GREEN_MASK) >> BLEND_GREEN_SHIFT)
#define BLUE16(x)   (((x) & BLEND_BLUE_MASK) << BLEND_BLUE_SHIFT)

#define MAKE16(r, g, b)                                              \
        (PRUint16)(((r) & BLEND_RED_SET_MASK) << BLEND_RED_SHIFT)    \
          | (((g) & BLEND_GREEN_SET_MASK) << BLEND_GREEN_SHIFT)      \
          | (((b) & BLEND_BLUE_SET_MASK) >> BLEND_BLUE_SHIFT)

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
void
nsBlender::Do16Blend(float aOpacity, PRInt32 aNumLines, PRInt32 aNumBytes,
                     PRUint8 *aSImage, PRUint8 *aDImage, PRUint8 *aSecondSImage,
                     PRInt32 aSLSpan, PRInt32 aDLSpan, nsBlendQuality aBlendQuality)
{
  PRUint32 opacity256 = (PRUint32)(aOpacity*256);

  // Handle simpler cases
  if (opacity256 <= 0) {
    return;
  } else if (opacity256 >= 256) {
    DoOpaqueBlend(aNumLines, aNumBytes, aSImage, aDImage, aSLSpan, aDLSpan);
    return;
  }

  PRIntn numPixels = aNumBytes/2;
  
  if (nsnull == aSecondSImage) {
    PRIntn y;
    for (y = 0; y < aNumLines; y++) {
      PRUint16 *s2 = (PRUint16*)aSImage;
      PRUint16 *d2 = (PRUint16*)aDImage;
      
      PRIntn i;
      for (i = 0; i < numPixels; i++) {
        PRUint32 destPix = *d2;
        PRUint32 destPixR = RED16(destPix);
        PRUint32 destPixG = GREEN16(destPix);
        PRUint32 destPixB = BLUE16(destPix);
        PRUint32 srcPix = *s2;
        
        *d2 = MAKE16(destPixR + (((RED16(srcPix) - destPixR)*opacity256) >> 8),
                     destPixG + (((GREEN16(srcPix) - destPixG)*opacity256) >> 8),
                     destPixB + (((BLUE16(srcPix) - destPixB)*opacity256) >> 8));

        d2++;
        s2++;
      }
      
      aSImage += aSLSpan;
      aDImage += aDLSpan;
    }
    return;
  }

  PRUint32 srcBackgroundColor = MAKE16(0x00, 0x00, 0x00);
  PRUint32 src2BackgroundColor = MAKE16(0xFF, 0xFF, 0xFF);

  PRIntn y;
  for (y = 0; y < aNumLines; y++) {
    PRUint16 *s2 = (PRUint16*)aSImage;
    PRUint16 *d2 = (PRUint16*)aDImage;
    PRUint16 *ss2 = (PRUint16*)aSecondSImage;

    PRIntn x;
    for (x = 0; x < numPixels; x++) {
      PRUint32 srcPix = *s2;
      PRUint32 src2Pix = *ss2;

      if ((srcPix != srcBackgroundColor) || (src2Pix != src2BackgroundColor)) {
        PRUint32 destPix = *d2;
        PRUint32 destPixR = RED16(destPix);
        PRUint32 destPixG = GREEN16(destPix);
        PRUint32 destPixB = BLUE16(destPix);
        PRUint32 srcPixR = RED16(srcPix);
        PRUint32 srcPixG = GREEN16(srcPix);
        PRUint32 srcPixB = BLUE16(srcPix);
          
        if (srcPix != src2Pix) {
          PRUint32 imageAlphaTimesDestPixR = (255 + srcPixR - RED16(src2Pix))*destPixR;
          PRUint32 imageAlphaTimesDestPixG = (255 + srcPixG - GREEN16(src2Pix))*destPixG;
          PRUint32 imageAlphaTimesDestPixB = (255 + srcPixB - BLUE16(src2Pix))*destPixB;
          PRUint32 adjustedDestPixR;
          FAST_DIVIDE_BY_255(adjustedDestPixR, imageAlphaTimesDestPixR);
          PRUint32 adjustedDestPixG;
          FAST_DIVIDE_BY_255(adjustedDestPixG, imageAlphaTimesDestPixG);
          PRUint32 adjustedDestPixB;
          FAST_DIVIDE_BY_255(adjustedDestPixB, imageAlphaTimesDestPixB);
            
          *d2 = MAKE16(destPixR + (((srcPixR - adjustedDestPixR)*opacity256) >> 8),
            destPixG + (((srcPixG - adjustedDestPixG)*opacity256) >> 8),
            destPixB + (((srcPixB - adjustedDestPixB)*opacity256) >> 8));
        } else {
          *d2 = MAKE16(destPixR + (((srcPixR - destPixR)*opacity256) >> 8),
            destPixG + (((srcPixG - destPixG)*opacity256) >> 8),
            destPixB + (((srcPixB - destPixB)*opacity256) >> 8));
        }
      }

      d2++;
      s2++;
      ss2++;
    }
    
    aSImage += aSLSpan;
    aDImage += aDLSpan;
    aSecondSImage += aSLSpan;
  }
}


//------------------------------------------------------------

extern void inv_colormap(PRInt16 colors,PRUint8 *aCMap,PRInt16 bits,PRUint32 *dist_buf,PRUint8 *aRGBMap );

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
void
nsBlender::Do8Blend(float aOpacity, PRInt32 aNumlines, PRInt32 aNumbytes,
                    PRUint8 *aSImage, PRUint8 *aDImage, PRUint8 *aSecondSImage,
                    PRInt32 aSLSpan, PRInt32 aDLSpan, IL_ColorSpace *aColorMap,
                    nsBlendQuality aBlendQuality)
{
#if 0
PRUint32  r,g,b,r1,g1,b1,i;
PRUint8   *d1,*d2,*s1,*s2;
PRInt32   x,y,val1,val2,numlines;
PRUint8   *mapptr,*invermap;
PRUint32  *distbuffer;
PRUint32  quantlevel,tnum,num,shiftnum;
NI_RGB    *map;

  if (nsnull == aColorMap->cmap.map) {
    NS_ASSERTION(PR_FALSE, "NULL Colormap during 8-bit blend");
    return;
  } 

  val2 = (PRUint8)(aOpacity*255);
  val1 = 255-val2;

  // build a colormap we can use to get an inverse map
  map = aColorMap->cmap.map+10;
  mapptr = new PRUint8[256*3];
  invermap = mapptr;
  for(i=0;i<216;i++){
    *invermap=map->blue;
    invermap++;
    *invermap=map->green;
    invermap++;
    *invermap=map->red;
    invermap++;
    map++;
  }
  for(i=216;i<256;i++){
    *invermap=0;
    invermap++;
    *invermap=0;
    invermap++;
    *invermap=0;
    invermap++;
  }

  quantlevel = aBlendQuality+2;
  quantlevel = 4;                   // 4                                          5
  shiftnum = (8-quantlevel)+8;      // 12                                         11 
  tnum = 2;                         // 2                                          2
  for(i=1;i<=quantlevel;i++)
    tnum = 2*tnum;                  // 2, 4, 8, tnum = 16                         32

  num = tnum;                       // num = 16                                   32
  for(i=1;i<3;i++)
    num = num*tnum;                 // 256, 4096                                  32768

  distbuffer = new PRUint32[num];   // new PRUint[4096]                           32768
   invermap = new PRUint8[num];
  inv_colormap(216,mapptr,quantlevel,distbuffer,invermap );  // 216,mapptr[],4,distbuffer[4096],invermap[12288])

  // now go thru the image and blend (remember, its bottom upwards)
  s1 = aSImage;
  d1 = aDImage;

  numlines = aNumlines;  

 
  for(y = 0; y < aNumlines; y++){
    s2 = s1;
    d2 = d1;
 
    for(x = 0; x < aNumbytes; x++){
      i = (*d2);
      i-=10;
      r = mapptr[(3 * i) + 2];
      g = mapptr[(3 * i) + 1];
      b = mapptr[(3 * i)];
 
      i =(*s2);
      i-=10;
      r1 = mapptr[(3 * i) + 2];
      g1 = mapptr[(3 * i) + 1];
      b1 = mapptr[(3 * i)];

      r = ((r*val1)+(r1*val2))>>shiftnum;
      //r=r>>quantlevel;
      if(r>tnum)
        r = tnum;

      g = ((g*val1)+(g1*val2))>>shiftnum;
      //g=g>>quantlevel;
      if(g>tnum)
        g = tnum;

      b = ((b*val1)+(b1*val2))>>shiftnum;
      //b=b>>quantlevel;
      if(b>tnum)
        b = tnum;

      r = (r<<(2*quantlevel))+(g<<quantlevel)+b;
      r = (invermap[r]+10);
      (*d2)=r;
      d2++;
      s2++;
    }

    s1 += aSLSpan;
    d1 += aDLSpan;
  }

  delete[] distbuffer;
  delete[] invermap;
#endif
}

//------------------------------------------------------------

static PRInt32   bcenter, gcenter, rcenter;
static PRUint32  gdist, rdist, cdist;
static PRInt32   cbinc, cginc, crinc;
static PRUint32  *gdp, *rdp, *cdp;
static PRUint8   *grgbp, *rrgbp, *crgbp;
static PRInt32   gstride,   rstride;
static PRInt32   x, xsqr, colormax;
static PRInt32   cindex;


static void maxfill(PRUint32 *buffer,PRInt32 side );
static PRInt32 redloop( void );
static PRInt32 greenloop( PRInt32 );
static PRInt32 blueloop( PRInt32 );

/** --------------------------------------------------------------------------
 * Create an inverse colormap for a given color table
 * @param colors -- Number of colors
 * @param aCMap -- The color map
 * @param aBits -- Resolution in bits for the inverse map
 * @param dist_buf -- a buffer to hold the temporary colors in while we calculate the map
 * @param aRGBMap -- the map to put the inverse colors into for the color table
 * @return VOID
 */
void
inv_colormap(PRInt16 aNumColors,PRUint8 *aCMap,PRInt16 aBits,PRUint32 *dist_buf,PRUint8 *aRGBMap )
{
PRInt32         nbits = 8 - aBits;               // 3
PRUint32        r,g,b;

  colormax = 1 << aBits;                        // 32
  x = 1 << nbits;                               // 8
  xsqr = 1 << (2 * nbits);                      // 64   

  // Compute "strides" for accessing the arrays
  gstride = colormax;                           // 32
  rstride = colormax * colormax;                // 1024

  maxfill( dist_buf, colormax );

  for (cindex = 0;cindex < aNumColors;cindex++ ){
    r = aCMap[(3 * cindex) + 2];
    g = aCMap[(3 * cindex) + 1];
    b = aCMap[(3 * cindex)];

	  rcenter = r >> nbits;
	  gcenter = g >> nbits;
	  bcenter = b >> nbits;

	  rdist = r - (rcenter * x + x/2);
	  gdist = g - (gcenter * x + x/2);
	  cdist = b - (bcenter * x + x/2);
	  cdist = rdist*rdist + gdist*gdist + cdist*cdist;

	  crinc = 2 * ((rcenter + 1) * xsqr - (r * x));
	  cginc = 2 * ((gcenter + 1) * xsqr - (g * x));
	  cbinc = 2 * ((bcenter + 1) * xsqr - (b * x));

	  // Array starting points.
	  cdp = dist_buf + rcenter * rstride + gcenter * gstride + bcenter;   // dist 01533120 .. 
	  crgbp = aRGBMap + rcenter * rstride + gcenter * gstride + bcenter;  // aRGBMap 01553168

	  (void)redloop();
  }
}

/** --------------------------------------------------------------------------
 * find a red max or min value in a color cube
 * @return  a red component in a certain span
 */
static PRInt32
redloop()
{
PRInt32        detect,r,first;
PRInt32        txsqr = xsqr + xsqr;
static PRInt32 rxx;

  detect = 0;

  rdist = cdist;   // 48
  rxx = crinc;     // 128
  rdp = cdp;
  rrgbp = crgbp;   // 01553168
  first = 1;
  for (r=rcenter;r<colormax;r++,rdp+=rstride,rrgbp+=rstride,rdist+=rxx,rxx+=txsqr,first=0){
    if ( greenloop( first ) )
      detect = 1;
    else 
      if ( detect )
        break;
  }
   
  rxx=crinc-txsqr;
  rdist = cdist-rxx;
  rdp=cdp-rstride;
  rrgbp=crgbp-rstride;
  first=1;
  for (r=rcenter-1;r>=0;r--,rdp-=rstride,rrgbp-=rstride,rxx-=txsqr,rdist-=rxx,first=0){
    if ( greenloop( first ) )
      detect = 1;
    else 
      if ( detect )
        break;
  }
    
  return detect;
}

/** --------------------------------------------------------------------------
 * find a green max or min value in a color cube
 * @return  a red component in a certain span
 */
static PRInt32
greenloop(PRInt32 aRestart)
{
PRInt32          detect,g,first;
PRInt32          txsqr = xsqr + xsqr;
static  PRInt32  here, min, max;
static  PRInt32  ginc, gxx, gcdist;	
static  PRUint32  *gcdp;
static  PRUint8   *gcrgbp;	

  if(aRestart){
    here = gcenter;
    min = 0;
    max = colormax - 1;
    ginc = cginc;
  }

  detect = 0;

  gcdp=rdp;
  gdp=rdp;
  gcrgbp=rrgbp;
  grgbp=rrgbp;
  gcdist=rdist;
  gdist=rdist;

  // loop up. 
  for(g=here,gxx=ginc,first=1;g<=max;
      g++,gdp+=gstride,gcdp+=gstride,grgbp+=gstride,gcrgbp+=gstride,
      gdist+=gxx,gcdist+=gxx,gxx+=txsqr,first=0){
    if(blueloop(first)){
      if (!detect){
        if (g>here){
	        here = g;
	        rdp = gcdp;
	        rrgbp = gcrgbp;
	        rdist = gcdist;
	        ginc = gxx;
        }
        detect=1;
      }
    }else 
      if (detect){
        break;
      }
  }
    
  // loop down
  gcdist = rdist-gxx;
  gdist = gcdist;
  gdp=rdp-gstride;
  gcdp=gdp;
  grgbp=rrgbp-gstride;
  gcrgbp = grgbp;

  for (g=here-1,gxx=ginc-txsqr,
      first=1;g>=min;g--,gdp-=gstride,gcdp-=gstride,grgbp-=gstride,gcrgbp-=gstride,
      gxx-=txsqr,gdist-=gxx,gcdist-=gxx,first=0){
    if (blueloop(first)){
      if (!detect){
        here = g;
        rdp = gcdp;
        rrgbp = gcrgbp;
        rdist = gcdist;
        ginc = gxx;
        detect = 1;
      }
    } else 
      if ( detect ){
        break;
      }
  }
  return detect;
}

/** --------------------------------------------------------------------------
 * find a blue max or min value in a color cube
 * @return  a red component in a certain span
 */
static PRInt32
blueloop(PRInt32 aRestart )
{
PRInt32  detect,b,i=cindex;
register  PRUint32  *dp;
register  PRUint8   *rgbp;
register  PRInt32   bxx;
PRUint32            bdist;
register  PRInt32   txsqr = xsqr + xsqr;
register  PRInt32   lim;
static    PRInt32   here, min, max;
static    PRInt32   binc;

  if (aRestart){
    here = bcenter;
    min = 0;
    max = colormax - 1;
    binc = cbinc;
  }

  detect = 0;
  bdist = gdist;

// Basic loop, finds first applicable cell.
  for (b=here,bxx=binc,dp=gdp,rgbp=grgbp,lim=max;b<=lim;
      b++,dp++,rgbp++,bdist+=bxx,bxx+=txsqr){
    if(*dp>bdist){
      if(b>here){
        here = b;
        gdp = dp;
        grgbp = rgbp;
        gdist = bdist;
        binc = bxx;
      }
      detect = 1;
      break;
    }
  }

  // Second loop fills in a run of closer cells.
  for (;b<=lim;b++,dp++,rgbp++,bdist+=bxx,bxx+=txsqr){
    if (*dp>bdist){
      *dp = bdist;
      *rgbp = i;
    }
    else{
      break;
    }
  }

// Basic loop down, do initializations here
lim = min;
b = here - 1;
bxx = binc - txsqr;
bdist = gdist - bxx;
dp = gdp - 1;
rgbp = grgbp - 1;

  // The 'find' loop is executed only if we didn't already find something.
  if (!detect)
    for (;b>=lim;b--,dp--,rgbp--,bxx-=txsqr,bdist-=bxx){
      if (*dp>bdist){
        here = b;
        gdp = dp;
        grgbp = rgbp;
        gdist = bdist;
        binc = bxx;
        detect = 1;
        break;
      }
    }

  // Update loop.
  for (;b>=lim;b--,dp--,rgbp--,bxx-=txsqr,bdist-=bxx){
    if (*dp>bdist){
      *dp = bdist;
      *rgbp = i;
    } else{
      break;
      }
  }

  // If we saw something, update the edge trackers.
  return detect;
}

/** --------------------------------------------------------------------------
 * fill in a span with a max value
 * @return  VOID
 */
static void
maxfill(PRUint32 *buffer,PRInt32 side )
{
register PRInt32 maxv = ~0L;
register PRInt32 i;
register PRUint32 *bp;

  for (i=side*side*side,bp=buffer;i>0;i--,bp++)         // side*side*side = 32768
    *bp = maxv;
}

//------------------------------------------------------------

