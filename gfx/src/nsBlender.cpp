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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#include "nsBlender.h"
#include "nsCRT.h"

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
nsBlender :: nsBlender()
{
  mContext = nsnull;
}

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
nsBlender::~nsBlender() 
{
}

NS_IMPL_ISUPPORTS1(nsBlender, nsIBlender)

//------------------------------------------------------------

// need to set up some masks for 16 bit blending
// Use compile-time constants where possible for marginally smaller
// footprint (don't need fields in nsBlender) but also for faster code
#if defined(XP_MAC) || defined(XP_MACOSX)

#define BLEND_RED_MASK        0x7c00
#define BLEND_GREEN_MASK      0x03e0
#define BLEND_BLUE_MASK       0x001f
#define BLEND_RED_SET_MASK    0xf8
#define BLEND_GREEN_SET_MASK  0xf8
#define BLEND_BLUE_SET_MASK   0xf8
#define BLEND_RED_SHIFT       7
#define BLEND_GREEN_SHIFT     2
#define BLEND_BLUE_SHIFT      3
#define BLEND_GREEN_BITS      5

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
#define BLEND_GREEN_BITS      6

#endif

/** ---------------------------------------------------
 *  See documentation in nsBlender.h
 *	@update 2/25/00 dwc
 */
NS_IMETHODIMP
nsBlender::Init(nsIDeviceContext *aContext)
{
  mContext = aContext;
  
  return NS_OK;
}

#define RED16(x)    (((x) & BLEND_RED_MASK) >> BLEND_RED_SHIFT)
#define GREEN16(x)  (((x) & BLEND_GREEN_MASK) >> BLEND_GREEN_SHIFT)
#define BLUE16(x)   (((x) & BLEND_BLUE_MASK) << BLEND_BLUE_SHIFT)

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
nsBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsIDrawingSurface* aSrc,
                 nsIDrawingSurface* aDst, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                 nsIDrawingSurface* aSecondSrc, nscolor aSrcBackColor,
                 nscolor aSecondSrcBackColor)
{
  NS_ASSERTION(aSrc, "nsBlender::Blend() called with nsnull aSrc");
  NS_ASSERTION(aDst, "nsBlender::Blend() called with nsnull aDst");
  NS_ENSURE_ARG_POINTER(aSrc);
  NS_ENSURE_ARG_POINTER(aDst);

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

  // range check the coordinates in both the source and destination buffers.
  rangeCheck(aSrc, aSX, aSY, aWidth, aHeight);
  rangeCheck(aDst, aDX, aDY, aWidth, aHeight);

  PRUint8* srcBytes = nsnull;
  PRUint8* secondSrcBytes = nsnull;
  PRUint8* destBytes = nsnull;
  PRInt32 srcSpan, destSpan, secondSrcSpan;
  PRInt32 srcRowBytes, destRowBytes, secondSrcRowBytes;

  result = aSrc->Lock(aSX, aSY, aWidth, aHeight, (void**)&srcBytes, &srcRowBytes, &srcSpan, NS_LOCK_SURFACE_READ_ONLY);
  if (NS_SUCCEEDED(result)) {
    result = aDst->Lock(aDX, aDY, aWidth, aHeight, (void**)&destBytes, &destRowBytes, &destSpan, 0);
    if (NS_SUCCEEDED(result)) {
      NS_ASSERTION(srcSpan == destSpan, "Mismatched bitmap formats (src/dest) in Blender");
      if (srcSpan == destSpan) {
        if (aSecondSrc) {
          result = aSecondSrc->Lock(aSX, aSY, aWidth, aHeight, (void**)&secondSrcBytes, &secondSrcRowBytes, &secondSrcSpan, NS_LOCK_SURFACE_READ_ONLY);
          if (NS_SUCCEEDED(result)) {
            NS_ASSERTION(srcSpan == secondSrcSpan && srcRowBytes == secondSrcRowBytes,
                         "Mismatched bitmap formats (src/secondSrc) in Blender");                         
            if (srcSpan == secondSrcSpan && srcRowBytes == secondSrcRowBytes) {
              result = Blend(srcBytes, srcRowBytes,
                             destBytes, destRowBytes,
                             secondSrcBytes,
                             srcSpan, aHeight, aSrcOpacity);
            }
            
            aSecondSrc->Unlock();
          }
        }
        else
        {
          result = Blend(srcBytes, srcRowBytes,
                         destBytes, destRowBytes,
                         secondSrcBytes,
                         srcSpan, aHeight, aSrcOpacity);
        }
      }

      aDst->Unlock();
    }

    aSrc->Unlock();
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
  nsIDrawingSurface* srcSurface, *destSurface, *secondSrcSurface = nsnull;
  aSrc->GetDrawingSurface(&srcSurface);
  aDest->GetDrawingSurface(&destSurface);
  if (aSecondSrc != nsnull)
    aSecondSrc->GetDrawingSurface(&secondSrcSurface);
  return Blend(aSX, aSY, aWidth, aHeight, srcSurface, destSurface,
               aDX, aDY, aSrcOpacity, secondSrcSurface, aSrcBackColor,
               aSecondSrcBackColor);
}

#ifndef MOZ_XUL
NS_IMETHODIMP nsBlender::GetAlphas(const nsRect& aRect, nsIDrawingSurface* aBlack,
                                   nsIDrawingSurface* aWhite, PRUint8** aAlphas) {
  NS_ERROR("GetAlphas not implemented because XUL support not built");
  return NS_ERROR_NOT_IMPLEMENTED;
}
#else
/**
 * Let A be the unknown source pixel's alpha value and let C be its (unknown) color.
 * Let S be the value painted onto black and T be the value painted onto white.
 * Then S = C*(A/255) and T = 255*(1 - A/255) + C*(A/255).
 * Therefore A = 255 - (T - S)
 * This is true no matter what color component we look at.
 */
static void ComputeAlphasByByte(PRInt32 aNumLines, PRInt32 aBytesPerLine,
                                PRInt32 aBytesPerPixel,
                                PRUint8 *aOnBlackImage, PRUint8 *aOnWhiteImage,
                                PRInt32 aBytesLineSpan, PRUint8 *aAlphas,
                                PRUint32 aAlphasSize)
{
  NS_ASSERTION(aBytesPerPixel == 3 || aBytesPerPixel == 4,
               "Only 24 or 32 bits per pixel supported here");

  PRIntn y;
  PRUint8* alphas = aAlphas;
  for (y = 0; y < aNumLines; y++) {
    // Look at component #1. It must be a real color no matter what
    // RGBA ordering is used.
    PRUint8 *s1 = aOnBlackImage + 1;
    PRUint8 *s2 = aOnWhiteImage + 1;
    
    PRIntn i;
    for (i = 1; i < aBytesPerLine; i += aBytesPerPixel) {
      *alphas++ = (PRUint8)(255 - (*s2 - *s1));
      s1 += aBytesPerPixel;
      s2 += aBytesPerPixel;
    }
  
    aOnBlackImage += aBytesLineSpan;
    aOnWhiteImage += aBytesLineSpan;
  }

  NS_ASSERTION(alphas - aAlphas == aAlphasSize, "alpha24/32 calculation error");
}

/**  Use the green channel to work out the alpha value,
     since green has the most bits in most divisions of 16-bit color.

     The green values range from 0 to (1 << BLEND_GREEN_BITS) - 1.
     Therefore we multiply a green value by 255/((1 << BLEND_GREEN_BITS) - 1)
     to get a real alpha value.
*/
static void ComputeAlphas16(PRInt32 aNumLines, PRInt32 aBytesPerLine,
                            PRUint8 *aOnBlackImage, PRUint8 *aOnWhiteImage,
                            PRInt32 aBytesLineSpan, PRUint8 *aAlphas,
                            PRUint32 aAlphasSize)
{
  PRIntn y;
  PRUint8* alphas = aAlphas;
  for (y = 0; y < aNumLines; y++) {
    PRUint16 *s1 = (PRUint16*)aOnBlackImage;
    PRUint16 *s2 = (PRUint16*)aOnWhiteImage;
    
      // GREEN16 returns a value between 0 and 255 representing the
      // green value of the pixel. It only has BLEND_GREEN_BITS of
      // precision (so the values are typically 0, 8, 16, ..., 248). 
      // If we just used the GREEN16 values
      // directly in the same equations that we use for the 24-bit case,
      // we'd lose because (e.g.) a completely transparent pixel would
      // have GREEN16(pix1) = 0, GREEN16(pix2) = 248, and the resulting 
      // alpha value would just be 248, but we need 255. So we need to
      // do some rescaling.
    const PRUint32 SCALE_DENOMINATOR =   // usually 248
      ((1 << BLEND_GREEN_BITS) - 1) << (8 - BLEND_GREEN_BITS);

    PRIntn i;
    for (i = 0; i < aBytesPerLine; i += 2) {
      PRUint32 pix1 = GREEN16(*s1);
      PRUint32 pix2 = GREEN16(*s2);
      *alphas++ = (PRUint8)(255 - ((pix2 - pix1)*255)/SCALE_DENOMINATOR);
      s1++;
      s2++;
    }
    
    aOnBlackImage += aBytesLineSpan;
    aOnWhiteImage += aBytesLineSpan;
  }

  NS_ASSERTION(alphas - aAlphas == aAlphasSize, "alpha16 calculation error");
}

static void ComputeAlphas(PRInt32 aNumLines, PRInt32 aBytesPerLine,
                          PRInt32 aDepth,
                          PRUint8 *aOnBlackImage, PRUint8 *aOnWhiteImage,
                          PRInt32 aBytesLineSpan, PRUint8 *aAlphas,
                          PRUint32 aAlphasSize)
{
  switch (aDepth) {
    case 32:
    case 24:
      ComputeAlphasByByte(aNumLines, aBytesPerLine, aDepth/8,
                          aOnBlackImage, aOnWhiteImage,
                          aBytesLineSpan, aAlphas, aAlphasSize);
      break;

    case 16:
      ComputeAlphas16(aNumLines, aBytesPerLine, aOnBlackImage, aOnWhiteImage,
                      aBytesLineSpan, aAlphas, aAlphasSize);
      break;
    
    default:
      NS_ERROR("Unknown depth for alpha calculation");
      // make them all opaque
      memset(aAlphas, 255, aAlphasSize);
  }
}

NS_IMETHODIMP nsBlender::GetAlphas(const nsRect& aRect, nsIDrawingSurface* aBlack,
                                   nsIDrawingSurface* aWhite, PRUint8** aAlphas) {
  nsresult result;

  nsIDrawingSurface* blackSurface = (nsIDrawingSurface *)aBlack;
  nsIDrawingSurface* whiteSurface = (nsIDrawingSurface *)aWhite;

  nsRect r = aRect;

  rangeCheck(blackSurface, r.x, r.y, r.width, r.height);
  rangeCheck(whiteSurface, r.x, r.y, r.width, r.height);

  PRUint8* blackBytes = nsnull;
  PRUint8* whiteBytes = nsnull;
  PRInt32 blackSpan, whiteSpan;
  PRInt32 blackBytesPerLine, whiteBytesPerLine;

  result = blackSurface->Lock(r.x, r.y, r.width, r.height,
                              (void**)&blackBytes, &blackSpan,
                              &blackBytesPerLine, NS_LOCK_SURFACE_READ_ONLY);
  if (NS_SUCCEEDED(result)) {
    result = whiteSurface->Lock(r.x, r.y, r.width, r.height,
                                (void**)&whiteBytes, &whiteSpan,
                                &whiteBytesPerLine, NS_LOCK_SURFACE_READ_ONLY);
    if (NS_SUCCEEDED(result)) {
      NS_ASSERTION(blackSpan == whiteSpan &&
                   blackBytesPerLine == whiteBytesPerLine,
                   "Mismatched bitmap formats (black/white) in Blender");
      if (blackSpan == whiteSpan && blackBytesPerLine == whiteBytesPerLine) {
        *aAlphas = new PRUint8[r.width*r.height];
        if (*aAlphas) {
          // compute depth like this to make sure it's consistent with the memory layout
          // and work around some GTK bugs. If there are no gfx bugs, then this is correct,
          // if there are gfx bugs, this will prevent a crash.
          PRUint32 depth = (blackBytesPerLine/r.width)*8;
          ComputeAlphas(r.height, blackBytesPerLine, depth,
                        blackBytes, whiteBytes, blackSpan, 
                        *aAlphas, r.width*r.height);
        } else {
          result = NS_ERROR_FAILURE;
        }
      } else {
        result = NS_ERROR_FAILURE;
      }

      whiteSurface->Unlock();
    }

    blackSurface->Unlock();
  }
  
  return result;
}
#endif // MOZ_XUL

/**
  This is a simple case for 8-bit blending. We treat the opacity as binary.
*/
static void Do8Blend(float aOpacity, PRInt32 aNumLines, PRInt32 aNumBytes,
                     PRUint8 *aSImage, PRUint8 *aS2Image, PRUint8 *aDImage,
                     PRInt32 aSLSpan, PRInt32 aDLSpan)
{
  if (aOpacity <= 0.0) {
    return;
  }

  // OK, just do an opaque blend. Assume the rendered image had just
  // 1-bit alpha.
  PRIntn y;
  if (!aS2Image) {
    for (y = 0; y < aNumLines; y++) {
      memcpy(aDImage, aSImage, aNumBytes);
      aSImage += aSLSpan;
      aDImage += aDLSpan;
    }
  } else {
    for (y = 0; y < aNumLines; y++) {
      for (int i = 0; i < aNumBytes; i++) {
        if (aSImage[i] == aS2Image[i]) {
          aDImage[i] = aSImage[i];
        }
      }
      aSImage += aSLSpan;
      aS2Image += aSLSpan;
      aDImage += aDLSpan;
    }
  }
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

    default:
        Do8Blend(aOpacity, aLines, aSrcBytes, aSrcBits, aSecondSrcBits,
               aDestBits, aSrcStride, aDestStride);
        break;
  }

  return result;
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
  }
  if (nsnull == aSecondSImage) {
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
  }
  if (nsnull == aSecondSImage) {
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
