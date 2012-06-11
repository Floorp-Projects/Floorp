/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoFrameContainer.h"

#include "nsHTMLMediaElement.h"
#include "nsIFrame.h"
#include "nsDisplayList.h"
#include "nsSVGEffects.h"

using namespace mozilla::layers;

namespace mozilla {

void VideoFrameContainer::SetCurrentFrame(const gfxIntSize& aIntrinsicSize,
                                          Image* aImage,
                                          TimeStamp aTargetTime)
{
  MutexAutoLock lock(mMutex);

  if (aIntrinsicSize != mIntrinsicSize) {
    mIntrinsicSize = aIntrinsicSize;
    mIntrinsicSizeChanged = true;
  }

  gfxIntSize oldFrameSize = mImageContainer->GetCurrentSize();
  TimeStamp lastPaintTime = mImageContainer->GetPaintTime();
  if (!lastPaintTime.IsNull() && !mPaintTarget.IsNull()) {
    mPaintDelay = lastPaintTime - mPaintTarget;
  }
  mImageContainer->SetCurrentImage(aImage);
  gfxIntSize newFrameSize = mImageContainer->GetCurrentSize();
  if (oldFrameSize != newFrameSize) {
    mImageSizeChanged = true;
  }

  mPaintTarget = aTargetTime;
}

double VideoFrameContainer::GetFrameDelay()
{
  MutexAutoLock lock(mMutex);
  return mPaintDelay.ToSeconds();
}

void VideoFrameContainer::Invalidate()
{
  NS_ASSERTION(NS_IsMainThread(), "Must call on main thread");
  if (!mElement) {
    // Element has been destroyed
    return;
  }

  nsIFrame* frame = mElement->GetPrimaryFrame();
  bool invalidateFrame = false;

  {
    MutexAutoLock lock(mMutex);

    // Get mImageContainerSizeChanged while holding the lock.
    invalidateFrame = mImageSizeChanged;
    mImageSizeChanged = false;

    if (mIntrinsicSizeChanged) {
      mElement->UpdateMediaSize(mIntrinsicSize);
      mIntrinsicSizeChanged = false;

      if (frame) {
        nsPresContext* presContext = frame->PresContext();
        nsIPresShell *presShell = presContext->PresShell();
        presShell->FrameNeedsReflow(frame,
                                    nsIPresShell::eStyleChange,
                                    NS_FRAME_IS_DIRTY);
      }
    }
  }

  if (frame) {
    nsRect contentRect = frame->GetContentRect() - frame->GetPosition();
    if (invalidateFrame) {
      frame->Invalidate(contentRect);
    } else {
      frame->InvalidateLayer(contentRect, nsDisplayItem::TYPE_VIDEO);
    }
  }

  nsSVGEffects::InvalidateDirectRenderingObservers(mElement);
}

}
