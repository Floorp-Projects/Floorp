/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
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

/* class to notify frames of background image loads */

#include "nsImageLoader.h"

#include "imgILoader.h"

#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"

#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"

#include "imgIContainer.h"

#include "nsStyleContext.h"
#include "nsGkAtoms.h"

// Paint forcing
#include "prenv.h"

NS_IMPL_ISUPPORTS2(nsImageLoader, imgIDecoderObserver, imgIContainerObserver)

nsImageLoader::nsImageLoader() :
  mFrame(nsnull), mPresContext(nsnull)
{
}

nsImageLoader::~nsImageLoader()
{
  mFrame = nsnull;
  mPresContext = nsnull;

  if (mRequest) {
    mRequest->Cancel(NS_ERROR_FAILURE);
  }
}


void
nsImageLoader::Init(nsIFrame *aFrame, nsPresContext *aPresContext)
{
  mFrame = aFrame;
  mPresContext = aPresContext;
}

void
nsImageLoader::Destroy()
{
  mFrame = nsnull;
  mPresContext = nsnull;

  if (mRequest) {
    mRequest->Cancel(NS_ERROR_FAILURE);
  }

  mRequest = nsnull;
}

nsresult
nsImageLoader::Load(imgIRequest *aImage)
{
  if (!mFrame)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aImage)
    return NS_ERROR_FAILURE;

  if (mRequest) {
    nsCOMPtr<nsIURI> oldURI;
    mRequest->GetURI(getter_AddRefs(oldURI));
    nsCOMPtr<nsIURI> newURI;
    aImage->GetURI(getter_AddRefs(newURI));
    PRBool eq = PR_FALSE;
    nsresult rv = newURI->Equals(oldURI, &eq);
    if (NS_SUCCEEDED(rv) && eq) {
      return NS_OK;
    }

    // Now cancel the old request so it won't hold a stale ref to us.
    mRequest->Cancel(NS_ERROR_FAILURE);
    mRequest = nsnull;
  }

  // Make sure to clone into a temporary, then set mRequest, since
  // cloning may notify and we don't want to trigger paints from this
  // code.
  nsCOMPtr<imgIRequest> newRequest;
  nsresult rv = aImage->Clone(this, getter_AddRefs(newRequest));
  mRequest.swap(newRequest);
  return rv;
}

                    

NS_IMETHODIMP nsImageLoader::OnStartContainer(imgIRequest *aRequest,
                                              imgIContainer *aImage)
{
  if (aImage)
  {
    /* Get requested animation policy from the pres context:
     *   normal = 0
     *   one frame = 1
     *   one loop = 2
     */
    aImage->SetAnimationMode(mPresContext->ImageAnimationMode());
    // Ensure the animation (if any) is started.
    aImage->StartAnimation();
  }
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnStopFrame(imgIRequest *aRequest,
                                         gfxIImageFrame *aFrame)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;
  
#ifdef NS_DEBUG
// Make sure the image request status's STATUS_FRAME_COMPLETE flag has been set to ensure
// the image will be painted when invalidated
  if (aRequest) {
   PRUint32 status = imgIRequest::STATUS_ERROR;
   nsresult rv = aRequest->GetImageStatus(&status);
   if (NS_SUCCEEDED(rv)) {
     NS_ASSERTION((status & imgIRequest::STATUS_FRAME_COMPLETE), "imgIRequest::STATUS_FRAME_COMPLETE not set");
   }
  }
#endif

  if (!mRequest) {
    // We're in the middle of a paint anyway
    return NS_OK;
  }
  
  // Draw the background image
  RedrawDirtyFrame(nsnull);
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::FrameChanged(imgIContainer *aContainer,
                                          gfxIImageFrame *newframe,
                                          nsRect * dirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  if (!mRequest) {
    // We're in the middle of a paint anyway
    return NS_OK;
  }
  
  nsRect r(*dirtyRect);

  r.x = nsPresContext::CSSPixelsToAppUnits(r.x);
  r.y = nsPresContext::CSSPixelsToAppUnits(r.y);
  r.width = nsPresContext::CSSPixelsToAppUnits(r.width);
  r.height = nsPresContext::CSSPixelsToAppUnits(r.height);

  RedrawDirtyFrame(&r);

  return NS_OK;
}


void
nsImageLoader::RedrawDirtyFrame(const nsRect* aDamageRect)
{
  // NOTE: It is not sufficient to invalidate only the size of the image:
  //       the image may be tiled! 
  //       The best option is to call into the frame, however lacking this
  //       we have to at least invalidate the frame's bounds, hence
  //       as long as we have a frame we'll use its size.
  //

  // Invalidate the entire frame
  // XXX We really only need to invalidate the client area of the frame...    

  nsRect bounds(nsPoint(0, 0), mFrame->GetSize());

  if (mFrame->GetType() == nsGkAtoms::canvasFrame) {
    // The canvas's background covers the whole viewport.
    bounds = mFrame->GetOverflowRect();
  }

  // XXX this should be ok, but there is some crappy ass bug causing it not to work
  // XXX seems related to the "body fixup rule" dealing with the canvas and body frames...
#if 0
  // Invalidate the entire frame only if the frame has a tiled background
  // image, otherwise just invalidate the intersection of the frame's bounds
  // with the damaged rect.
  nsStyleContext* styleContext;
  mFrame->GetStyleContext(&styleContext);
  const nsStyleBackground* bg = styleContext->GetStyleBackground();

  if ((bg->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE) ||
      (bg->mBackgroundRepeat == NS_STYLE_BG_REPEAT_OFF)) {
    // The frame does not have a background image so we are free
    // to invalidate only the intersection of the damage rect and
    // the frame's bounds.

    if (aDamageRect) {
      bounds.IntersectRect(*aDamageRect, bounds);
    }
  }

#endif

  mFrame->Invalidate(bounds, PR_FALSE);
}
