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
#include <stdlib.h>


nsDrawingSurface nsRenderingContextImpl::gBackbuffer = nsnull;
nsRect nsRenderingContextImpl::gBackbufferBounds = nsRect(0, 0, 0, 0);
nsSize nsRenderingContextImpl::gLargestRequestedSize = nsSize(0, 0);


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: nsRenderingContextImpl()
: mTranMatrix(nsnull)
, mLineStyle(nsLineStyle_kSolid)
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


NS_IMETHODIMP nsRenderingContextImpl::GetBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer)
{
  // Default implementation assumes the backbuffer will be cached.
  // If the platform implementation does not require the backbuffer to
  // be cached override this method and make the following call instead:
  // AllocateBackbuffer(aRequestedSize, aMaxSize, aBackbuffer, PR_FALSE);
  return AllocateBackbuffer(aRequestedSize, aMaxSize, aBackbuffer, PR_TRUE);
}

nsresult nsRenderingContextImpl::AllocateBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer, PRBool aCacheBackbuffer)
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

      rv = CreateDrawingSurface(newBounds, 0, gBackbuffer);
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

      float p2t;
      nsCOMPtr<nsIDeviceContext>  dx;
      GetDeviceContext(*getter_AddRefs(dx));
      p2t = dx->DevUnitsToAppUnits();
      nsRect bounds = aRequestedSize;
      bounds *= p2t;

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
  PRInt32 height;
  PRInt32 width;

  nsCOMPtr<nsIDeviceContext>  dx;
  GetDeviceContext(*getter_AddRefs(dx));
  dx->GetDeviceSurfaceDimensions(width, height);

  float devUnits;
  devUnits = dx->DevUnitsToAppUnits();
  PRInt32 screenHeight = NSToIntRound(float( height) / devUnits );
  PRInt32 screenWidth = NSToIntRound(float( width) / devUnits );

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


#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

NS_IMETHODIMP nsRenderingContextImpl::DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect)
{
  nsRect dr = aDestRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  nsRect sr = aSrcRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
  
  if (sr.IsEmpty() || dr.IsEmpty())
    return NS_OK;

  sr.x = aSrcRect.x;
  sr.y = aSrcRect.y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface((void**)&surface);
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

  return img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height,
                   dr.x, dr.y, dr.width, dr.height);
}

/* [noscript] void drawTile (in imgIContainer aImage, in nscoord aXImageStart, in nscoord aYImageStart, [const] in nsRect aTargetRect); */
NS_IMETHODIMP
nsRenderingContextImpl::DrawTile(imgIContainer *aImage,
                                 nscoord aXImageStart, nscoord aYImageStart,
                                 const nsRect * aTargetRect)
{
  nsRect dr(*aTargetRect);
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);
  mTranMatrix->TransformCoord(&aXImageStart, &aYImageStart);

  nscoord width, height;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);

  if (width == 0 || height == 0)
    return PR_FALSE;

  nscoord xOffset = (dr.x - aXImageStart) % width;
  nscoord yOffset = (dr.y - aYImageStart) % height;

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsIDrawingSurface *surface = nsnull;
  GetDrawingSurface((void**)&surface);
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
nsRenderingContextImpl::RenderPostScriptDataFragment(const unsigned char *aData, unsigned long aDatalen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


