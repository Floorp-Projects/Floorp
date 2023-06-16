/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoFrameContainer.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "GLImages.h"  // for SurfaceTextureImage
#endif
#include "MediaDecoderOwner.h"
#include "mozilla/Telemetry.h"
#include "mozilla/AbstractThread.h"

using namespace mozilla::layers;

namespace mozilla {
#define NS_DispatchToMainThread(...) CompileError_UseAbstractMainThreadInstead

namespace {
template <Telemetry::HistogramID ID>
class AutoTimer {
  // Set a threshold to reduce performance overhead
  // for we're measuring hot spots.
  static const uint32_t sThresholdMS = 1000;

 public:
  ~AutoTimer() {
    auto end = TimeStamp::Now();
    auto diff = uint32_t((end - mStart).ToMilliseconds());
    if (diff > sThresholdMS) {
      Telemetry::Accumulate(ID, diff);
    }
  }

 private:
  const TimeStamp mStart = TimeStamp::Now();
};
}  // namespace

VideoFrameContainer::VideoFrameContainer(
    MediaDecoderOwner* aOwner, already_AddRefed<ImageContainer> aContainer)
    : mOwner(aOwner),
      mImageContainer(aContainer),
      mMutex("nsVideoFrameContainer"),
      mFrameID(0),
      mPendingPrincipalHandle(PRINCIPAL_HANDLE_NONE),
      mFrameIDForPendingPrincipalHandle(0),
      mMainThread(aOwner->AbstractMainThread()) {
  NS_ASSERTION(aOwner, "aOwner must not be null");
  NS_ASSERTION(mImageContainer, "aContainer must not be null");
}

VideoFrameContainer::~VideoFrameContainer() = default;

PrincipalHandle VideoFrameContainer::GetLastPrincipalHandle() {
  MutexAutoLock lock(mMutex);
  return GetLastPrincipalHandleLocked();
}

PrincipalHandle VideoFrameContainer::GetLastPrincipalHandleLocked() {
  return mLastPrincipalHandle;
}

void VideoFrameContainer::UpdatePrincipalHandleForFrameID(
    const PrincipalHandle& aPrincipalHandle,
    const ImageContainer::FrameID& aFrameID) {
  MutexAutoLock lock(mMutex);
  UpdatePrincipalHandleForFrameIDLocked(aPrincipalHandle, aFrameID);
}

void VideoFrameContainer::UpdatePrincipalHandleForFrameIDLocked(
    const PrincipalHandle& aPrincipalHandle,
    const ImageContainer::FrameID& aFrameID) {
  if (mPendingPrincipalHandle == aPrincipalHandle) {
    return;
  }
  mPendingPrincipalHandle = aPrincipalHandle;
  mFrameIDForPendingPrincipalHandle = aFrameID;
}

#ifdef MOZ_WIDGET_ANDROID
static void NotifySetCurrent(Image* aImage) {
  if (aImage == nullptr) {
    return;
  }

  SurfaceTextureImage* image = aImage->AsSurfaceTextureImage();
  if (image == nullptr) {
    return;
  }

  image->OnSetCurrent();
}
#endif

void VideoFrameContainer::SetCurrentFrame(const gfx::IntSize& aIntrinsicSize,
                                          Image* aImage,
                                          const TimeStamp& aTargetTime) {
#ifdef MOZ_WIDGET_ANDROID
  NotifySetCurrent(aImage);
#endif
  if (aImage) {
    MutexAutoLock lock(mMutex);
    AutoTArray<ImageContainer::NonOwningImage, 1> imageList;
    imageList.AppendElement(
        ImageContainer::NonOwningImage(aImage, aTargetTime, ++mFrameID));
    SetCurrentFramesLocked(aIntrinsicSize, imageList);
  } else {
    ClearCurrentFrame(aIntrinsicSize);
  }
}

void VideoFrameContainer::SetCurrentFrames(
    const gfx::IntSize& aIntrinsicSize,
    const nsTArray<ImageContainer::NonOwningImage>& aImages) {
#ifdef MOZ_WIDGET_ANDROID
  // When there are multiple frames, only the last one is effective
  // (see bug 1299068 comment 4). Here I just count on VideoSink and VideoOutput
  // to send one frame at a time and warn if not.
  Unused << NS_WARN_IF(aImages.Length() > 1);
  for (auto& image : aImages) {
    NotifySetCurrent(image.mImage);
  }
#endif
  MutexAutoLock lock(mMutex);
  SetCurrentFramesLocked(aIntrinsicSize, aImages);
}

void VideoFrameContainer::SetCurrentFramesLocked(
    const gfx::IntSize& aIntrinsicSize,
    const nsTArray<ImageContainer::NonOwningImage>& aImages) {
  mMutex.AssertCurrentThreadOwns();

  if (auto size = Some(aIntrinsicSize); size != mIntrinsicSize) {
    mIntrinsicSize = size;
    mMainThread->Dispatch(NS_NewRunnableFunction(
        "IntrinsicSizeChanged", [this, self = RefPtr(this), size]() {
          mMainThreadState.mNewIntrinsicSize = size;
        }));
  }

  gfx::IntSize oldFrameSize = mImageContainer->GetCurrentSize();

  // When using the OMX decoder, destruction of the current image can indirectly
  //  block on main thread I/O. If we let this happen while holding onto
  //  |mImageContainer|'s lock, then when the main thread then tries to
  //  composite it can then block on |mImageContainer|'s lock, causing a
  //  deadlock. We use this hack to defer the destruction of the current image
  //  until it is safe.
  nsTArray<ImageContainer::OwningImage> oldImages;
  mImageContainer->GetCurrentImages(&oldImages);

  PrincipalHandle principalHandle = PRINCIPAL_HANDLE_NONE;
  ImageContainer::FrameID lastFrameIDForOldPrincipalHandle =
      mFrameIDForPendingPrincipalHandle - 1;
  if (mPendingPrincipalHandle != PRINCIPAL_HANDLE_NONE &&
      ((!oldImages.IsEmpty() &&
        oldImages.LastElement().mFrameID >= lastFrameIDForOldPrincipalHandle) ||
       (!aImages.IsEmpty() &&
        aImages[0].mFrameID > lastFrameIDForOldPrincipalHandle))) {
    // We are releasing the last FrameID prior to
    // `lastFrameIDForOldPrincipalHandle` OR there are no FrameIDs prior to
    // `lastFrameIDForOldPrincipalHandle` in the new set of images. This means
    // that the old principal handle has been flushed out and we can notify our
    // video element about this change.
    principalHandle = mPendingPrincipalHandle;
    mLastPrincipalHandle = mPendingPrincipalHandle;
    mPendingPrincipalHandle = PRINCIPAL_HANDLE_NONE;
    mFrameIDForPendingPrincipalHandle = 0;
  }

  if (aImages.IsEmpty()) {
    mImageContainer->ClearAllImages();
  } else {
    mImageContainer->SetCurrentImages(aImages);
  }
  gfx::IntSize newFrameSize = mImageContainer->GetCurrentSize();
  bool imageSizeChanged = (oldFrameSize != newFrameSize);

  if (principalHandle != PRINCIPAL_HANDLE_NONE || imageSizeChanged) {
    RefPtr<VideoFrameContainer> self = this;
    mMainThread->Dispatch(NS_NewRunnableFunction(
        "PrincipalHandleOrImageSizeChanged",
        [this, self, principalHandle, imageSizeChanged]() {
          mMainThreadState.mImageSizeChanged = imageSizeChanged;
          if (mOwner && principalHandle != PRINCIPAL_HANDLE_NONE) {
            mOwner->PrincipalHandleChangedForVideoFrameContainer(
                this, principalHandle);
          }
        }));
  }
}

void VideoFrameContainer::ClearFutureFrames(TimeStamp aNow) {
  MutexAutoLock lock(mMutex);

  // See comment in SetCurrentFrame for the reasoning behind
  // using a kungFuDeathGrip here.
  AutoTArray<ImageContainer::OwningImage, 10> kungFuDeathGrip;
  mImageContainer->GetCurrentImages(&kungFuDeathGrip);

  if (!kungFuDeathGrip.IsEmpty()) {
    AutoTArray<ImageContainer::NonOwningImage, 1> currentFrame;
    ImageContainer::OwningImage& img = kungFuDeathGrip[0];
    // Find the current image in case there are several.
    for (const auto& image : kungFuDeathGrip) {
      if (image.mTimeStamp > aNow) {
        break;
      }
      img = image;
    }
    currentFrame.AppendElement(ImageContainer::NonOwningImage(
        img.mImage, img.mTimeStamp, img.mFrameID, img.mProducerID));
    mImageContainer->SetCurrentImages(currentFrame);
  }
}

void VideoFrameContainer::ClearCachedResources() {
  MutexAutoLock lock(mMutex);
  mImageContainer->ClearCachedResources();
}

ImageContainer* VideoFrameContainer::GetImageContainer() {
  // Note - you'll need the lock to manipulate this.  The pointer is not
  // modified from multiple threads, just the data pointed to by it.
  return mImageContainer;
}

double VideoFrameContainer::GetFrameDelay() {
  MutexAutoLock lock(mMutex);
  return mImageContainer->GetPaintDelay().ToSeconds();
}

void VideoFrameContainer::InvalidateWithFlags(uint32_t aFlags) {
  NS_ASSERTION(NS_IsMainThread(), "Must call on main thread");

  if (!mOwner) {
    // Owner has been destroyed
    return;
  }

  MediaDecoderOwner::ImageSizeChanged imageSizeChanged{
      mMainThreadState.mImageSizeChanged};
  mMainThreadState.mImageSizeChanged = false;

  auto newIntrinsicSize = std::move(mMainThreadState.mNewIntrinsicSize);

  MediaDecoderOwner::ForceInvalidate forceInvalidate{
      (aFlags & INVALIDATE_FORCE) != 0};
  mOwner->Invalidate(imageSizeChanged, newIntrinsicSize, forceInvalidate);
}

}  // namespace mozilla

#undef NS_DispatchToMainThread
