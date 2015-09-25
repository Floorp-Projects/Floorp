/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoFrameContainer.h"

#include "mozilla/dom/HTMLMediaElement.h"
#include "nsIFrame.h"
#include "nsDisplayList.h"
#include "nsSVGEffects.h"

using namespace mozilla::layers;

namespace mozilla {

VideoFrameContainer::VideoFrameContainer(dom::HTMLMediaElement* aElement,
                                         already_AddRefed<ImageContainer> aContainer)
  : mElement(aElement),
    mImageContainer(aContainer), mMutex("nsVideoFrameContainer"),
    mFrameID(0),
    mIntrinsicSizeChanged(false), mImageSizeChanged(false)
{
  NS_ASSERTION(aElement, "aElement must not be null");
  NS_ASSERTION(mImageContainer, "aContainer must not be null");
}

VideoFrameContainer::~VideoFrameContainer()
{}

void VideoFrameContainer::SetCurrentFrame(const gfx::IntSize& aIntrinsicSize,
                                          Image* aImage,
                                          const TimeStamp& aTargetTime)
{
  if (aImage) {
    MutexAutoLock lock(mMutex);
    nsAutoTArray<ImageContainer::NonOwningImage,1> imageList;
    imageList.AppendElement(
        ImageContainer::NonOwningImage(aImage, aTargetTime, ++mFrameID));
    SetCurrentFramesLocked(aIntrinsicSize, imageList);
  } else {
    ClearCurrentFrame(aIntrinsicSize);
  }
}

void VideoFrameContainer::SetCurrentFrames(const gfx::IntSize& aIntrinsicSize,
                                           const nsTArray<ImageContainer::NonOwningImage>& aImages)
{
  MutexAutoLock lock(mMutex);
  SetCurrentFramesLocked(aIntrinsicSize, aImages);
}

void VideoFrameContainer::SetCurrentFramesLocked(const gfx::IntSize& aIntrinsicSize,
                                                 const nsTArray<ImageContainer::NonOwningImage>& aImages)
{
  mMutex.AssertCurrentThreadOwns();

  if (aIntrinsicSize != mIntrinsicSize) {
    mIntrinsicSize = aIntrinsicSize;
    mIntrinsicSizeChanged = true;
  }

  gfx::IntSize oldFrameSize = mImageContainer->GetCurrentSize();

  // When using the OMX decoder, destruction of the current image can indirectly
  //  block on main thread I/O. If we let this happen while holding onto
  //  |mImageContainer|'s lock, then when the main thread then tries to
  //  composite it can then block on |mImageContainer|'s lock, causing a
  //  deadlock. We use this hack to defer the destruction of the current image
  //  until it is safe.
  nsTArray<ImageContainer::OwningImage> kungFuDeathGrip;
  mImageContainer->GetCurrentImages(&kungFuDeathGrip);

  if (aImages.IsEmpty()) {
    mImageContainer->ClearAllImages();
  } else {
    mImageContainer->SetCurrentImages(aImages);
  }
  gfx::IntSize newFrameSize = mImageContainer->GetCurrentSize();
  if (oldFrameSize != newFrameSize) {
    mImageSizeChanged = true;
  }
}

void VideoFrameContainer::ClearCurrentFrame()
{
  MutexAutoLock lock(mMutex);

  // See comment in SetCurrentFrame for the reasoning behind
  // using a kungFuDeathGrip here.
  nsTArray<ImageContainer::OwningImage> kungFuDeathGrip;
  mImageContainer->GetCurrentImages(&kungFuDeathGrip);

  mImageContainer->ClearAllImages();
}

void VideoFrameContainer::ClearFutureFrames()
{
  MutexAutoLock lock(mMutex);

  // See comment in SetCurrentFrame for the reasoning behind
  // using a kungFuDeathGrip here.
  nsTArray<ImageContainer::OwningImage> kungFuDeathGrip;
  mImageContainer->GetCurrentImages(&kungFuDeathGrip);

  if (!kungFuDeathGrip.IsEmpty()) {
    nsTArray<ImageContainer::NonOwningImage> currentFrame;
    const ImageContainer::OwningImage& img = kungFuDeathGrip[0];
    currentFrame.AppendElement(ImageContainer::NonOwningImage(img.mImage,
        img.mTimeStamp, img.mFrameID, img.mProducerID));
    mImageContainer->SetCurrentImages(currentFrame);
  }
}

ImageContainer* VideoFrameContainer::GetImageContainer() {
  return mImageContainer;
}


double VideoFrameContainer::GetFrameDelay()
{
  return mImageContainer->GetPaintDelay().ToSeconds();
}

void VideoFrameContainer::InvalidateWithFlags(uint32_t aFlags)
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

  bool asyncInvalidate = mImageContainer &&
                         mImageContainer->IsAsync() &&
                         !(aFlags & INVALIDATE_FORCE);

  if (frame) {
    if (invalidateFrame) {
      frame->InvalidateFrame();
    } else {
      frame->InvalidateLayer(nsDisplayItem::TYPE_VIDEO, nullptr, nullptr,
                             asyncInvalidate ? nsIFrame::UPDATE_IS_ASYNC : 0);
    }
  }

  nsSVGEffects::InvalidateDirectRenderingObservers(mElement);
}

} // namespace mozilla
