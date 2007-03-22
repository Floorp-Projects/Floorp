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

#include "nsCOMPtr.h"
#include "nsRenderingContextImpl.h"
#include "nsIDeviceContext.h"
#include "nsIImage.h"
#include "nsTransform2D.h"
#include "nsIRegion.h"
#include "nsIFontMetrics.h"
#include <stdlib.h>


nsIDrawingSurface* nsRenderingContextImpl::gBackbuffer = nsnull;
nsRect nsRenderingContextImpl::gBackbufferBounds = nsRect(0, 0, 0, 0);
nsSize nsRenderingContextImpl::gLargestRequestedSize = nsSize(0, 0);


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: nsRenderingContextImpl()
: mTranMatrix(nsnull)
, mAct(0)
, mActive(nsnull)
, mPenMode(nsPenMode_kNone)
{
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: ~nsRenderingContextImpl()
{


}


NS_IMETHODIMP nsRenderingContextImpl::GetBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, PRBool aForBlending, nsIDrawingSurface* &aBackbuffer)
{
  // Default implementation assumes the backbuffer will be cached.
  // If the platform implementation does not require the backbuffer to
  // be cached override this method and make the following call instead:
  // AllocateBackbuffer(aRequestedSize, aMaxSize, aBackbuffer, PR_FALSE);
  return AllocateBackbuffer(aRequestedSize, aMaxSize, aBackbuffer, PR_TRUE, 0);
}

nsresult nsRenderingContextImpl::AllocateBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsIDrawingSurface* &aBackbuffer, PRBool aCacheBackbuffer, PRUint32 aSurfFlags)
{
  nsRect newBounds;
  nsresult rv = NS_OK;

   if (! aCacheBackbuffer) {
    newBounds = aRequestedSize;
  } else {
    GetDrawingSurfaceSize(aMaxSize, aRequestedSize, newBounds);
  }

  if ((nsnull == gBackbuffer)
      || (gBackbufferBounds.width != newBounds.width)
      || (gBackbufferBounds.height != newBounds.height))
    {
      if (gBackbuffer) {
        //destroy existing DS
        DestroyDrawingSurface(gBackbuffer);
        gBackbuffer = nsnull;
      }

      rv = CreateDrawingSurface(newBounds, aSurfFlags, gBackbuffer);
      //   printf("Allocating a new drawing surface %d %d\n", newBounds.width, newBounds.height);
      if (NS_SUCCEEDED(rv)) {
        gBackbufferBounds = newBounds;
        SelectOffScreenDrawingSurface(gBackbuffer);
      } else {
        gBackbufferBounds.SetRect(0,0,0,0);
        gBackbuffer = nsnull;
      }
    } else {
      SelectOffScreenDrawingSurface(gBackbuffer);

      nsCOMPtr<nsIDeviceContext>  dx;
      GetDeviceContext(*getter_AddRefs(dx));
      nsRect bounds = aRequestedSize;
      bounds *= dx->AppUnitsPerDevPixel();

      SetClipRect(bounds, nsClipCombine_kReplace);
    }

  aBackbuffer = gBackbuffer;
  return rv;
}

NS_IMETHODIMP nsRenderingContextImpl::ReleaseBackbuffer(void)
{
  // If the platform does not require the backbuffer to be cached
  // override this method and call DestroyCachedBackbuffer
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextImpl::DestroyCachedBackbuffer(void)
{
  if (gBackbuffer) {
    DestroyDrawingSurface(gBackbuffer);
    gBackbuffer = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextImpl::UseBackbuffer(PRBool* aUseBackbuffer)
{
  *aUseBackbuffer = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextImpl::PushTranslation(PushedTranslation* aState)
{
  // The transform components are saved and restored instead 
  // of using PushState and PopState because they are too slow
  // because they also save and restore the clip state.
  // Note: Setting a negative translation to restore the 
  // state does not work because the floating point errors can accumulate
  // causing the display of some frames to be off by one pixel. 
  // This happens frequently when running in 120DPI mode where frames are
  // often positioned at 1/2 pixel locations and small floating point errors
  // will cause the frames to vary their pixel x location during scrolling
  // operations causes a single scan line of pixels to be shifted left relative
  // to the other scan lines for the same text. 
  
  // Save the transformation matrix's translation components.
  nsTransform2D *theTransform; 
  GetCurrentTransform(theTransform);
  NS_ASSERTION(theTransform != nsnull, "The rendering context transform is null");
  theTransform->GetTranslation(&aState->mSavedX, &aState->mSavedY);
  
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextImpl::PopTranslation(PushedTranslation* aState)
{
  nsTransform2D *theTransform; 
  GetCurrentTransform(theTransform);
  NS_ASSERTION(theTransform != nsnull, "The rendering context transform is null");
  theTransform->SetTranslation(aState->mSavedX, aState->mSavedY);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextImpl::SetTranslation(nscoord aX, nscoord aY)
{
  nsTransform2D *theTransform; 
  GetCurrentTransform(theTransform);
  NS_ASSERTION(theTransform != nsnull, "The rendering context transform is null");
  theTransform->SetTranslation(aX, aY);
  return NS_OK;
}

PRBool nsRenderingContextImpl::RectFitsInside(const nsRect& aRect, PRInt32 aWidth, PRInt32 aHeight) const
{
  if (aRect.width > aWidth)
    return (PR_FALSE);

  if (aRect.height > aHeight)
    return (PR_FALSE);

  return PR_TRUE;
}

PRBool nsRenderingContextImpl::BothRectsFitInside(const nsRect& aRect1, const nsRect& aRect2, PRInt32 aWidth, PRInt32 aHeight, nsRect& aNewSize) const
{
  if (PR_FALSE == RectFitsInside(aRect1, aWidth, aHeight)) {
    return PR_FALSE;
  }

  if (PR_FALSE == RectFitsInside(aRect2, aWidth, aHeight)) {
    return PR_FALSE;
  }

  aNewSize.width = aWidth;
  aNewSize.height = aHeight;

  return PR_TRUE;
}

void nsRenderingContextImpl::GetDrawingSurfaceSize(const nsRect& aMaxBackbufferSize, const nsRect& aRequestedSize, nsRect& aNewSize) 
{ 
  CalculateDiscreteSurfaceSize(aMaxBackbufferSize, aRequestedSize, aNewSize);
  aNewSize.MoveTo(aRequestedSize.x, aRequestedSize.y);
}

void nsRenderingContextImpl::CalculateDiscreteSurfaceSize(const nsRect& aMaxBackbufferSize, const nsRect& aRequestedSize, nsRect& aSurfaceSize) 
{
  // Get the height and width of the screen
  nscoord height;
  nscoord width;

  nsCOMPtr<nsIDeviceContext>  dx;
  GetDeviceContext(*getter_AddRefs(dx));
  dx->GetDeviceSurfaceDimensions(width, height);

  PRInt32 p2a = dx->AppUnitsPerDevPixel();
  PRInt32 screenHeight = NSAppUnitsToIntPixels(height, p2a);
  PRInt32 screenWidth = NSAppUnitsToIntPixels(width, p2a);

  // These tests must go from smallest rectangle to largest rectangle.

  // 1/8 screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, screenWidth / 8, screenHeight / 8, aSurfaceSize)) {
    return;
  }

  // 1/4 screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, screenWidth / 4, screenHeight / 4, aSurfaceSize)) {
    return;
  }

  // 1/2 screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, screenWidth / 2, screenHeight / 2, aSurfaceSize)) {
    return;
  }

  // 3/4 screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, (screenWidth * 3) / 4, (screenHeight * 3) / 4, aSurfaceSize)) {
    return;
  }

  // 3/4 screen width full screen height
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, (screenWidth * 3) / 4, screenHeight, aSurfaceSize)) {
    return;
  }

  // Full screen
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, screenWidth, screenHeight, aSurfaceSize)) {
    return;
  }

  // Bigger than Full Screen use the largest request every made.
  if (BothRectsFitInside(aRequestedSize, aMaxBackbufferSize, gLargestRequestedSize.width, gLargestRequestedSize.height, aSurfaceSize)) {
    return;
  } else {
    gLargestRequestedSize.width = PR_MAX(aRequestedSize.width, aMaxBackbufferSize.width);
    gLargestRequestedSize.height = PR_MAX(aRequestedSize.height, aMaxBackbufferSize.height);
    aSurfaceSize.width = gLargestRequestedSize.width;
    aSurfaceSize.height = gLargestRequestedSize.height;
    //   printf("Expanding the largested requested size to %d %d\n", gLargestRequestedSize.width, gLargestRequestedSize.height);
  }
}



/**
 * Let the device context know whether we want text reordered with
 * right-to-left base direction
 */
NS_IMETHODIMP
nsRenderingContextImpl::SetRightToLeftText(PRBool aIsRTL)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetRightToLeftText(PRBool* aIsRTL)
{
  *aIsRTL = PR_FALSE;
  return NS_OK;
}

#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#ifndef MOZ_CAIRO_GFX
NS_IMETHODIMP nsRenderingContextImpl::DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect)
{
  nsRect dr = aDestRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  // We should NOT be transforming the source rect (which is based on the image
  // origin) using the rendering context's translation!
  // However, given that we are, remember that the transformation of a
  // height depends on the position, since what we are really doing is
  // transforming the edges.  So transform *with* a translation, based
  // on the origin of the *destination* rect, and then fix up the
  // origin.
  nsRect sr(aDestRect.TopLeft(), aSrcRect.Size());
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
  
  if (sr.IsEmpty() || dr.IsEmpty())
    return NS_OK;

  sr.MoveTo(aSrcRect.TopLeft());
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface(&surface);
  if (!surface) return NS_ERROR_FAILURE;

  // For Bug 87819
  // iframe may want image to start at different position, so adjust
  nsRect iframeRect;
  iframe->GetRect(iframeRect);
  
  if (iframeRect.x > 0) {
    // Adjust for the iframe offset before we do scaling.
    sr.x -= iframeRect.x;

    nscoord scaled_x = sr.x;
    if (dr.width != sr.width) {
      PRFloat64 scale_ratio = PRFloat64(dr.width) / PRFloat64(sr.width);
      scaled_x = NSToCoordRound(scaled_x * scale_ratio);
    }
    if (sr.x < 0) {
      dr.x -= scaled_x;
      sr.width += sr.x;
      dr.width += scaled_x;
      if (sr.width <= 0 || dr.width <= 0)
        return NS_OK;
      sr.x = 0;
    } else if (sr.x > iframeRect.width) {
      return NS_OK;
    }
  }

  if (iframeRect.y > 0) {
    // Adjust for the iframe offset before we do scaling.
    sr.y -= iframeRect.y;

    nscoord scaled_y = sr.y;
    if (dr.height != sr.height) {
      PRFloat64 scale_ratio = PRFloat64(dr.height) / PRFloat64(sr.height);
      scaled_y = NSToCoordRound(scaled_y * scale_ratio);
    }
    if (sr.y < 0) {
      dr.y -= scaled_y;
      sr.height += sr.y;
      dr.height += scaled_y;
      if (sr.height <= 0 || dr.height <= 0)
        return NS_OK;
      sr.y = 0;
    } else if (sr.y > iframeRect.height) {
      return NS_OK;
    }
  }

  // Multiple paint rects may have been coalesced into a bounding box, so
  // ensure that this rect is actually within the clip region before we draw.
  nsCOMPtr<nsIRegion> clipRegion;
  GetClipRegion(getter_AddRefs(clipRegion));
  if (clipRegion && !clipRegion->ContainsRect(dr.x, dr.y, dr.width, dr.height))
    return NS_OK;

  return img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height,
                   dr.x, dr.y, dr.width, dr.height);
}
#endif

/* [noscript] void drawTile (in imgIContainer aImage, in nscoord aXImageStart, in nscoord aYImageStart, [const] in nsRect aTargetRect); */
NS_IMETHODIMP
nsRenderingContextImpl::DrawTile(imgIContainer *aImage,
                                 nscoord aXImageStart, nscoord aYImageStart,
                                 const nsRect * aTargetRect)
{
  nsRect dr(*aTargetRect);
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);
  mTranMatrix->TransformCoord(&aXImageStart, &aYImageStart);

  // may have become empty due to transform shinking small number to 0
  if (dr.IsEmpty())
    return NS_OK;

  nscoord width, height;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);

  if (width == 0 || height == 0)
    return NS_OK;

  nscoord xOffset = (dr.x - aXImageStart) % width;
  nscoord yOffset = (dr.y - aYImageStart) % height;

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface(&surface);
  if (!surface) return NS_ERROR_FAILURE;

  /* bug 113561 - frame can be smaller than container */
  nsRect iframeRect;
  iframe->GetRect(iframeRect);
  PRInt32 padx = width - iframeRect.width;
  PRInt32 pady = height - iframeRect.height;

  return img->DrawTile(*this, surface,
                       xOffset - iframeRect.x, yOffset - iframeRect.y,
                       padx, pady,
                       dr);
}

NS_IMETHODIMP
nsRenderingContextImpl::FlushRect(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::FlushRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetClusterInfo(const PRUnichar *aText,
                                       PRUint32 aLength,
                                       PRUint8 *aClusterStarts)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

PRInt32
nsRenderingContextImpl::GetPosition(const PRUnichar *aText,
                                    PRUint32 aLength,
                                    nsPoint aPt)
{
  return -1;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetRangeWidth(const PRUnichar *aText,
                                      PRUint32 aLength,
                                      PRUint32 aStart,
                                      PRUint32 aEnd,
                                      PRUint32 &aWidth)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetRangeWidth(const char *aText,
                                      PRUint32 aLength,
                                      PRUint32 aStart,
                                      PRUint32 aEnd,
                                      PRUint32 &aWidth)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Hard limit substring lengths to 8000 characters ... this lets us statically
// size the cluster buffer array in FindSafeLength
#define MAX_GFX_TEXT_BUF_SIZE 8000
static PRInt32 GetMaxChunkLength(nsRenderingContextImpl* aContext)
{
  PRInt32 len = aContext->GetMaxStringLength();
  return PR_MIN(len, MAX_GFX_TEXT_BUF_SIZE);
}

static PRInt32 FindSafeLength(nsRenderingContextImpl* aContext,
                              const PRUnichar *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
  if (aLength <= aMaxChunkLength)
    return aLength;
  
  PRUint8 buffer[MAX_GFX_TEXT_BUF_SIZE + 1];
  // Fill in the cluster hint information, if it's available.
  PRUint32 clusterHint;
  aContext->GetHints(clusterHint);
  clusterHint &= NS_RENDERING_HINT_TEXT_CLUSTERS;

  PRInt32 len = aMaxChunkLength;

  if (clusterHint) {
    nsresult rv =
      aContext->GetClusterInfo(aString, aMaxChunkLength + 1, buffer);
    if (NS_FAILED(rv))
      return len;
  }

  // Ensure that we don't break inside a cluster or inside a surrogate pair
  while (len > 0 &&
         (NS_IS_LOW_SURROGATE(aString[len]) || (clusterHint && !buffer[len]))) {
    len--;
  }
  if (len == 0) {
    // We don't want our caller to go into an infinite loop, so don't return
    // zero. It's hard to imagine how we could actually get here unless there
    // are languages that allow clusters of arbitrary size. If there are and
    // someone feeds us a 500+ character cluster, too bad.
    return aMaxChunkLength;
  }
  return len;
}

static PRInt32 FindSafeLength(nsRenderingContextImpl* aContext,
                              const char *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
  // Since it's ASCII, we don't need to worry about clusters or RTL
  return PR_MIN(aLength, aMaxChunkLength);
}

NS_IMETHODIMP
nsRenderingContextImpl::GetWidth(const nsString& aString, nscoord &aWidth,
                                 PRInt32 *aFontID)
{
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextImpl::GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsRenderingContextImpl::DrawString(const nsString& aString, nscoord aX, nscoord aY,
                                   PRInt32 aFontID, const nscoord* aSpacing)
{
  return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP
nsRenderingContextImpl::GetWidth(const char* aString, PRUint32 aLength,
                                 nscoord& aWidth)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  aWidth = 0;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nscoord width;
    nsresult rv = GetWidthInternal(aString, len, width);
    if (NS_FAILED(rv))
      return rv;
    aWidth += width;
    aLength -= len;
    aString += len;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetWidth(const PRUnichar *aString, PRUint32 aLength,
                                 nscoord &aWidth, PRInt32 *aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  aWidth = 0;
  
  if (aFontID) {
    *aFontID = 0;
  }
  
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nscoord width;
    nsresult rv = GetWidthInternal(aString, len, width);
    if (NS_FAILED(rv))
      return rv;
    aWidth += width;
    aLength -= len;
    aString += len;
  }
  return NS_OK;
}  

NS_IMETHODIMP
nsRenderingContextImpl::GetTextDimensions(const char* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength)
    return GetTextDimensionsInternal(aString, aLength, aDimensions);
 
  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsTextDimensions dimensions;
    nsresult rv = GetTextDimensionsInternal(aString, len, dimensions);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsTextDimensions, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned.
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(dimensions);
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions, PRInt32* aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength)
    return GetTextDimensionsInternal(aString, aLength, aDimensions);
    
  if (aFontID) {
    *aFontID = nsnull;
  }
 
  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsTextDimensions dimensions;
    nsresult rv = GetTextDimensionsInternal(aString, len, dimensions);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsTextDimensions, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned.
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(dimensions);
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }
  return NS_OK;  
}

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11) || defined(XP_BEOS)
NS_IMETHODIMP
nsRenderingContextImpl::GetTextDimensions(const char*       aString,
                                          PRInt32           aLength,
                                          PRInt32           aAvailWidth,
                                          PRInt32*          aBreaks,
                                          PRInt32           aNumBreaks,
                                          nsTextDimensions& aDimensions,
                                          PRInt32&          aNumCharsFit,
                                          nsTextDimensions& aLastWordDimensions,
                                          PRInt32*          aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= PRInt32(maxChunkLength))
    return GetTextDimensionsInternal(aString, aLength, aAvailWidth, aBreaks, aNumBreaks,
                                     aDimensions, aNumCharsFit, aLastWordDimensions, aFontID);

  if (aFontID) {
    *aFontID = 0;
  }
  
  // Do a naive implementation based on 3-arg GetTextDimensions
  PRInt32 x = 0;
  PRInt32 wordCount;
  for (wordCount = 0; wordCount < aNumBreaks; ++wordCount) {
    PRInt32 lastBreak = wordCount > 0 ? aBreaks[wordCount - 1] : 0;
    nsTextDimensions dimensions;
    
    NS_ASSERTION(aBreaks[wordCount] > lastBreak, "Breaks must be monotonically increasing");
    NS_ASSERTION(aBreaks[wordCount] <= aLength, "Breaks can't exceed string length");
   
     // Call safe method

    nsresult rv =
      GetTextDimensions(aString + lastBreak, aBreaks[wordCount] - lastBreak,
                        dimensions);
    if (NS_FAILED(rv))
      return rv;
    x += dimensions.width;
    // The first word always "fits"
    if (x > aAvailWidth && wordCount > 0)
      break;
    // aDimensions ascent/descent should exclude the last word (unless there
    // is only one word) so we let it run one word behind
    if (wordCount == 0) {
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(aLastWordDimensions);
    }
    aNumCharsFit = aBreaks[wordCount];
    aLastWordDimensions = dimensions;
  }
  // aDimensions width should include all the text
  aDimensions.width = x;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetTextDimensions(const PRUnichar*  aString,
                                          PRInt32           aLength,
                                          PRInt32           aAvailWidth,
                                          PRInt32*          aBreaks,
                                          PRInt32           aNumBreaks,
                                          nsTextDimensions& aDimensions,
                                          PRInt32&          aNumCharsFit,
                                          nsTextDimensions& aLastWordDimensions,
                                          PRInt32*          aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= PRInt32(maxChunkLength))
    return GetTextDimensionsInternal(aString, aLength, aAvailWidth, aBreaks, aNumBreaks,
                                     aDimensions, aNumCharsFit, aLastWordDimensions, aFontID);

  if (aFontID) {
    *aFontID = 0;
  }

  // Do a naive implementation based on 3-arg GetTextDimensions
  PRInt32 x = 0;
  PRInt32 wordCount;
  for (wordCount = 0; wordCount < aNumBreaks; ++wordCount) {
    PRInt32 lastBreak = wordCount > 0 ? aBreaks[wordCount - 1] : 0;
    
    NS_ASSERTION(aBreaks[wordCount] > lastBreak, "Breaks must be monotonically increasing");
    NS_ASSERTION(aBreaks[wordCount] <= aLength, "Breaks can't exceed string length");
    
    nsTextDimensions dimensions;
    // Call safe method
    nsresult rv =
      GetTextDimensions(aString + lastBreak, aBreaks[wordCount] - lastBreak,
                        dimensions);
    if (NS_FAILED(rv))
      return rv;
    x += dimensions.width;
    // The first word always "fits"
    if (x > aAvailWidth && wordCount > 0)
      break;
    // aDimensions ascent/descent should exclude the last word (unless there
    // is only one word) so we let it run one word behind
    if (wordCount == 0) {
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(aLastWordDimensions);
    }
    aNumCharsFit = aBreaks[wordCount];
    aLastWordDimensions = dimensions;
  }
  // aDimensions width should include all the text
  aDimensions.width = x;
  return NS_OK;
}
#endif

#ifdef MOZ_MATHML
NS_IMETHODIMP
nsRenderingContextImpl::GetBoundingMetrics(const char*        aString,
                                           PRUint32           aLength,
                                           nsBoundingMetrics& aBoundingMetrics)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength)
    return GetBoundingMetricsInternal(aString, aLength, aBoundingMetrics);

  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsBoundingMetrics metrics;
    nsresult rv = GetBoundingMetricsInternal(aString, len, metrics);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsBoundingMetrics, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned and the left bearing is properly initialized.
      aBoundingMetrics = metrics;
    } else {
      aBoundingMetrics += metrics;
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetBoundingMetrics(const PRUnichar*   aString,
                                           PRUint32           aLength,
                                           nsBoundingMetrics& aBoundingMetrics,
                                           PRInt32*           aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength)
    return GetBoundingMetricsInternal(aString, aLength, aBoundingMetrics, aFontID);

  if (aFontID) {
    *aFontID = 0;
  }

  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsBoundingMetrics metrics;
    nsresult rv = GetBoundingMetricsInternal(aString, len, metrics);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsBoundingMetrics, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned and the left bearing is properly initialized.
      aBoundingMetrics = metrics;
    } else {
      aBoundingMetrics += metrics;
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }  
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsRenderingContextImpl::DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsresult rv = DrawStringInternal(aString, len, aX, aY);
    if (NS_FAILED(rv))
      return rv;
    aLength -= len;

    if (aLength > 0) {
      nscoord width;
      rv = GetWidthInternal(aString, len, width);
      if (NS_FAILED(rv))
        return rv;
      aX += width;
      aString += len;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength) {
    return DrawStringInternal(aString, aLength, aX, aY, aFontID, aSpacing);
  }

  PRBool isRTL = PR_FALSE;
  GetRightToLeftText(&isRTL);

  if (isRTL) {
    nscoord totalWidth = 0;
    if (aSpacing) {
      for (PRUint32 i = 0; i < aLength; ++i) {
        totalWidth += aSpacing[i];
      }
    } else {
      nsresult rv = GetWidth(aString, aLength, totalWidth);
      if (NS_FAILED(rv))
        return rv;
    }
    aX += totalWidth;
  }
  
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nscoord width = 0;
    if (aSpacing) {
      for (PRInt32 i = 0; i < len; ++i) {
        width += aSpacing[i];
      }
    } else {
      nsresult rv = GetWidthInternal(aString, len, width);
      if (NS_FAILED(rv))
        return rv;
    }

    if (isRTL) {
      aX -= width;
    }
    nsresult rv = DrawStringInternal(aString, len, aX, aY, aFontID, aSpacing);
    if (NS_FAILED(rv))
      return rv;
    aLength -= len;
    if (!isRTL) {
      aX += width;
    }
    aString += len;
    if (aSpacing) {
      aSpacing += len;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::RenderEPS(const nsRect& aRect, FILE *aDataFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
