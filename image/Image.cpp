/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Image.h"
#include "Layers.h"               // for LayerManager
#include "nsRefreshDriver.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"        // for Tie

namespace mozilla {
namespace image {

///////////////////////////////////////////////////////////////////////////////
// Memory Reporting
///////////////////////////////////////////////////////////////////////////////

ImageMemoryCounter::ImageMemoryCounter(Image* aImage,
                                       SizeOfState& aState,
                                       bool aIsUsed)
  : mIsUsed(aIsUsed)
{
  MOZ_ASSERT(aImage);

  // Extract metadata about the image.
  RefPtr<ImageURL> imageURL(aImage->GetURI());
  if (imageURL) {
    imageURL->GetSpec(mURI);
  }

  int32_t width = 0;
  int32_t height = 0;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);
  mIntrinsicSize.SizeTo(width, height);

  mType = aImage->GetType();

  // Populate memory counters for source and decoded data.
  mValues.SetSource(aImage->SizeOfSourceWithComputedFallback(aState));
  aImage->CollectSizeOfSurfaces(mSurfaces, aState.mMallocSizeOf);

  // Compute totals.
  for (const SurfaceMemoryCounter& surfaceCounter : mSurfaces) {
    mValues += surfaceCounter.Values();
  }
}


///////////////////////////////////////////////////////////////////////////////
// Image Base Types
///////////////////////////////////////////////////////////////////////////////

Pair<DrawResult, RefPtr<layers::Image>>
ImageResource::GetCurrentImage(ImageContainer* aContainer,
                               const IntSize& aSize,
                               uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContainer);

  DrawResult drawResult;
  RefPtr<SourceSurface> surface;
  Tie(drawResult, surface) =
    GetFrameInternal(aSize, FRAME_CURRENT, aFlags | FLAG_ASYNC_NOTIFY);
  if (!surface) {
    // The OS threw out some or all of our buffer. We'll need to wait for the
    // redecode (which was automatically triggered by GetFrame) to complete.
    return MakePair(drawResult, RefPtr<layers::Image>());
  }

  RefPtr<layers::Image> image = new layers::SourceSurfaceImage(surface);
  return MakePair(drawResult, Move(image));
}

already_AddRefed<ImageContainer>
ImageResource::GetImageContainerImpl(LayerManager* aManager,
                                     const IntSize& aSize,
                                     uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT((aFlags & ~(FLAG_SYNC_DECODE |
                         FLAG_SYNC_DECODE_IF_FAST |
                         FLAG_ASYNC_NOTIFY))
               == FLAG_NONE,
             "Unsupported flag passed to GetImageContainer");

  if (!IsImageContainerAvailable(aManager, aFlags)) {
    return nullptr;
  }

  if (mAnimationConsumers == 0) {
    SendOnUnlockedDraw(aFlags);
  }

  RefPtr<layers::ImageContainer> container = mImageContainer.get();

  bool mustRedecode =
    (aFlags & (FLAG_SYNC_DECODE | FLAG_SYNC_DECODE_IF_FAST)) &&
    mLastImageContainerDrawResult != DrawResult::SUCCESS &&
    mLastImageContainerDrawResult != DrawResult::BAD_IMAGE;

  if (container && !mustRedecode) {
    return container.forget();
  }

  // We need a new ImageContainer, so create one.
  container = LayerManager::CreateImageContainer();

  DrawResult drawResult;
  RefPtr<layers::Image> image;
  Tie(drawResult, image) = GetCurrentImage(container, aSize, aFlags);
  if (!image) {
    return nullptr;
  }

#ifdef DEBUG
  NotifyDrawingObservers();
#endif

  // |image| holds a reference to a SourceSurface which in turn holds a lock on
  // the current frame's data buffer, ensuring that it doesn't get freed as
  // long as the layer system keeps this ImageContainer alive.
  AutoTArray<ImageContainer::NonOwningImage, 1> imageList;
  imageList.AppendElement(ImageContainer::NonOwningImage(image, TimeStamp(),
                                                         mLastFrameID++,
                                                         mImageProducerID));
  container->SetCurrentImagesInTransaction(imageList);

  mLastImageContainerDrawResult = drawResult;
  mImageContainer = container;

  return container.forget();
}

void
ImageResource::UpdateImageContainer(const IntSize& aSize)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<layers::ImageContainer> container = mImageContainer.get();
  if (!container) {
    return;
  }

  DrawResult drawResult;
  RefPtr<layers::Image> image;
  Tie(drawResult, image) = GetCurrentImage(container, aSize, FLAG_NONE);
  if (!image) {
    return;
  }

  mLastImageContainerDrawResult = drawResult;
  AutoTArray<ImageContainer::NonOwningImage, 1> imageList;
  imageList.AppendElement(ImageContainer::NonOwningImage(image, TimeStamp(),
                                                         mLastFrameID++,
                                                         mImageProducerID));
  container->SetCurrentImages(imageList);
}

void
ImageResource::ReleaseImageContainer()
{
  MOZ_ASSERT(NS_IsMainThread());
  mImageContainer = nullptr;
}

// Constructor
ImageResource::ImageResource(ImageURL* aURI) :
  mURI(aURI),
  mInnerWindowId(0),
  mAnimationConsumers(0),
  mAnimationMode(kNormalAnimMode),
  mInitialized(false),
  mAnimating(false),
  mError(false),
  mImageProducerID(ImageContainer::AllocateProducerID()),
  mLastFrameID(0),
  mLastImageContainerDrawResult(DrawResult::NOT_READY)
{ }

ImageResource::~ImageResource()
{
  // Ask our ProgressTracker to drop its weak reference to us.
  mProgressTracker->ResetImage();
}

void
ImageResource::IncrementAnimationConsumers()
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only to encourage serialization "
                                "with DecrementAnimationConsumers");
  mAnimationConsumers++;
}

void
ImageResource::DecrementAnimationConsumers()
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only to encourage serialization "
                                "with IncrementAnimationConsumers");
  MOZ_ASSERT(mAnimationConsumers >= 1,
             "Invalid no. of animation consumers!");
  mAnimationConsumers--;
}

nsresult
ImageResource::GetAnimationModeInternal(uint16_t* aAnimationMode)
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_ARG_POINTER(aAnimationMode);

  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

nsresult
ImageResource::SetAnimationModeInternal(uint16_t aAnimationMode)
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(aAnimationMode == kNormalAnimMode ||
               aAnimationMode == kDontAnimMode ||
               aAnimationMode == kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");

  mAnimationMode = aAnimationMode;

  return NS_OK;
}

bool
ImageResource::HadRecentRefresh(const TimeStamp& aTime)
{
  // Our threshold for "recent" is 1/2 of the default refresh-driver interval.
  // This ensures that we allow for frame rates at least as fast as the
  // refresh driver's default rate.
  static TimeDuration recentThreshold =
      TimeDuration::FromMilliseconds(nsRefreshDriver::DefaultInterval() / 2.0);

  if (!mLastRefreshTime.IsNull() &&
      aTime - mLastRefreshTime < recentThreshold) {
    return true;
  }

  // else, we can proceed with a refresh.
  // But first, update our last refresh time:
  mLastRefreshTime = aTime;
  return false;
}

void
ImageResource::EvaluateAnimation()
{
  if (!mAnimating && ShouldAnimate()) {
    nsresult rv = StartAnimation();
    mAnimating = NS_SUCCEEDED(rv);
  } else if (mAnimating && !ShouldAnimate()) {
    StopAnimation();
  }
}

void
ImageResource::SendOnUnlockedDraw(uint32_t aFlags)
{
  if (!mProgressTracker) {
    return;
  }

  if (!(aFlags & FLAG_ASYNC_NOTIFY)) {
    mProgressTracker->OnUnlockedDraw();
  } else {
    NotNull<RefPtr<ImageResource>> image = WrapNotNull(this);
    nsCOMPtr<nsIEventTarget> eventTarget = mProgressTracker->GetEventTarget();
    nsCOMPtr<nsIRunnable> ev = NS_NewRunnableFunction(
      "image::ImageResource::SendOnUnlockedDraw", [=]() -> void {
        RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
        if (tracker) {
          tracker->OnUnlockedDraw();
        }
      });
    eventTarget->Dispatch(ev.forget(), NS_DISPATCH_NORMAL);
  }
}

#ifdef DEBUG
void
ImageResource::NotifyDrawingObservers()
{
  if (!mURI || !NS_IsMainThread()) {
    return;
  }

  bool match = false;
  if ((NS_FAILED(mURI->SchemeIs("resource", &match)) || !match) &&
      (NS_FAILED(mURI->SchemeIs("chrome", &match)) || !match)) {
    return;
  }

  // Record the image drawing for startup performance testing.
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_WARNING_ASSERTION(obs, "Can't get an observer service handle");
  if (obs) {
    nsCOMPtr<nsIURI> imageURI = mURI->ToIURI();
    nsAutoCString spec;
    imageURI->GetSpec(spec);
    obs->NotifyObservers(nullptr, "image-drawing", NS_ConvertUTF8toUTF16(spec).get());
  }
}
#endif

} // namespace image
} // namespace mozilla
