/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  mNextLoader = nullptr;
  while (list) {
    nsRefPtr<nsImageLoader> todestroy = list;
    list = todestroy->mNextLoader;
    todestroy->mNextLoader = nullptr;
    todestroy->Destroy();
  }

  if (mRequest && mFrame) {
    nsLayoutUtils::DeregisterImageRequest(mFrame->PresContext(), mRequest,
                                          &mRequestRegistered);
  }

  mFrame = nullptr;

  if (mRequest) {
    mRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
  }

  mRequest = nullptr;
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
  if (mActions & ACTION_REDRAW_ON_DECODE) {
    DoRedraw(nullptr);
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
  if (mActions & ACTION_REDRAW_ON_LOAD) {
    DoRedraw(nullptr);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::FrameChanged(imgIRequest *aRequest,
                                          imgIContainer *aContainer,
                                          const nsIntRect *aDirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  if (!mRequest) {
    // We're in the middle of a paint anyway
    return NS_OK;
  }

  NS_ASSERTION(aRequest == mRequest, "This is a neat trick.");

  nsRect r = aDirtyRect->IsEqualInterior(nsIntRect::GetMaxSizedIntRect()) ?
    nsRect(nsPoint(0, 0), mFrame->GetSize()) :
    aDirtyRect->ToAppUnits(nsPresContext::AppUnitsPerCSSPixel());

  DoRedraw(&r);

  return NS_OK;
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
