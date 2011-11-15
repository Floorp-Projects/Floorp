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
#include "nsLayoutUtils.h"

// Paint forcing
#include "prenv.h"

NS_IMPL_ISUPPORTS2(nsImageLoader, imgIDecoderObserver, imgIContainerObserver)

nsImageLoader::nsImageLoader(nsIFrame *aFrame, PRUint32 aActions,
                             nsImageLoader *aNextLoader)
  : mFrame(aFrame),
    mActions(aActions),
    mNextLoader(aNextLoader),
    mRequestRegistered(false)
{
}

nsImageLoader::~nsImageLoader()
{
  Destroy();
}

/* static */ already_AddRefed<nsImageLoader>
nsImageLoader::Create(nsIFrame *aFrame, imgIRequest *aRequest, 
                      PRUint32 aActions, nsImageLoader *aNextLoader)
{
  nsRefPtr<nsImageLoader> loader =
    new nsImageLoader(aFrame, aActions, aNextLoader);

  loader->Load(aRequest);

  return loader.forget();
}

void
nsImageLoader::Destroy()
{
  // Destroy the chain with only one level of recursion.
  nsRefPtr<nsImageLoader> list = mNextLoader;
  mNextLoader = nsnull;
  while (list) {
    nsRefPtr<nsImageLoader> todestroy = list;
    list = todestroy->mNextLoader;
    todestroy->mNextLoader = nsnull;
    todestroy->Destroy();
  }

  if (mRequest && mFrame) {
    nsLayoutUtils::DeregisterImageRequest(mFrame->PresContext(), mRequest,
                                          &mRequestRegistered);
    mRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
  }

  mFrame = nsnull;
  mRequest = nsnull;
}

nsresult
nsImageLoader::Load(imgIRequest *aImage)
{
  NS_ASSERTION(!mRequest, "can't reuse image loaders");
  NS_ASSERTION(mFrame, "not initialized");
  NS_ASSERTION(aImage, "must have non-null image");

  if (!mFrame)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aImage)
    return NS_ERROR_FAILURE;

  // Deregister mRequest from the refresh driver, since it is no longer
  // going to be managed by this nsImageLoader.
  nsPresContext* presContext = mFrame->PresContext();

  nsLayoutUtils::DeregisterImageRequest(presContext, mRequest,
                                        &mRequestRegistered);

  // Make sure to clone into a temporary, then set mRequest, since
  // cloning may notify and we don't want to trigger paints from this
  // code.
  nsCOMPtr<imgIRequest> newRequest;
  nsresult rv = aImage->Clone(this, getter_AddRefs(newRequest));
  mRequest.swap(newRequest);

  if (mRequest) {
    nsLayoutUtils::RegisterImageRequestIfAnimated(presContext, mRequest,
                                                  &mRequestRegistered);
  }

  return rv;
}

NS_IMETHODIMP nsImageLoader::OnStartContainer(imgIRequest *aRequest,
                                              imgIContainer *aImage)
{
  NS_ABORT_IF_FALSE(aImage, "Who's calling us then?");

  /* Get requested animation policy from the pres context:
   *   normal = 0
   *   one frame = 1
   *   one loop = 2
   */
  aImage->SetAnimationMode(mFrame->PresContext()->ImageAnimationMode());

  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnStopFrame(imgIRequest *aRequest,
                                         PRUint32 aFrame)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  if (!mRequest) {
    // We're in the middle of a paint anyway
    return NS_OK;
  }

  // Take requested actions
  if (mActions & ACTION_REFLOW_ON_DECODE) {
    DoReflow();
  }
  if (mActions & ACTION_REDRAW_ON_DECODE) {
    DoRedraw(nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnImageIsAnimated(imgIRequest *aRequest)
{
  // Register with the refresh driver now that we are aware that
  // we are animated.
  nsLayoutUtils::RegisterImageRequest(mFrame->PresContext(),
                                      aRequest, &mRequestRegistered);
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnStopRequest(imgIRequest *aRequest,
                                           bool aLastPart)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  if (!mRequest) {
    // We're in the middle of a paint anyway
    return NS_OK;
  }

  // Take requested actions
  if (mActions & ACTION_REFLOW_ON_LOAD) {
    DoReflow();
  }
  if (mActions & ACTION_REDRAW_ON_LOAD) {
    DoRedraw(nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::FrameChanged(imgIContainer *aContainer,
                                          const nsIntRect *aDirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  if (!mRequest) {
    // We're in the middle of a paint anyway
    return NS_OK;
  }

  nsRect r = aDirtyRect->IsEqualInterior(nsIntRect::GetMaxSizedIntRect()) ?
    nsRect(nsPoint(0, 0), mFrame->GetSize()) :
    aDirtyRect->ToAppUnits(nsPresContext::AppUnitsPerCSSPixel());

  DoRedraw(&r);

  return NS_OK;
}

void
nsImageLoader::DoReflow()
{
  nsIPresShell *shell = mFrame->PresContext()->GetPresShell();
  shell->FrameNeedsReflow(mFrame, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
}

void
nsImageLoader::DoRedraw(const nsRect* aDamageRect)
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
    bounds = mFrame->GetVisualOverflowRect();
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

  if (mFrame->GetStyleVisibility()->IsVisible()) {
    mFrame->Invalidate(bounds);
  }
}
