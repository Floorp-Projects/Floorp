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

DrawResult
ImageResource::AddCurrentImage(ImageContainer* aContainer,
                               const IntSize& aSize,
                               uint32_t aFlags,
                               bool aInTransaction)
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
    return drawResult;
  }

  // |image| holds a reference to a SourceSurface which in turn holds a lock on
  // the current frame's data buffer, ensuring that it doesn't get freed as
  // long as the layer system keeps this ImageContainer alive.
  RefPtr<layers::Image> image = new layers::SourceSurfaceImage(surface);

  // We can share the producer ID with other containers because it is only
  // used internally to validate the frames given to a particular container
  // so that another object cannot add its own. Similarly the frame ID is
  // only used internally to ensure it is always increasing, and skipping
  // IDs from an individual container's perspective is acceptable.
  AutoTArray<ImageContainer::NonOwningImage, 1> imageList;
  imageList.AppendElement(ImageContainer::NonOwningImage(image, TimeStamp(),
                                                         mLastFrameID++,
                                                         mImageProducerID));

  if (aInTransaction) {
    aContainer->SetCurrentImagesInTransaction(imageList);
  } else {
    aContainer->SetCurrentImages(imageList);
  }
  return drawResult;
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
                         FLAG_ASYNC_NOTIFY |
                         FLAG_HIGH_QUALITY_SCALING))
               == FLAG_NONE,
             "Unsupported flag passed to GetImageContainer");

  IntSize size = GetImageContainerSize(aManager, aSize, aFlags);
  if (size.IsEmpty()) {
    return nullptr;
  }

  if (mAnimationConsumers == 0) {
    SendOnUnlockedDraw(aFlags);
  }

  RefPtr<layers::ImageContainer> container;
  ImageContainerEntry* entry = nullptr;
  int i = mImageContainers.Length() - 1;
  for (; i >= 0; --i) {
    entry = &mImageContainers[i];
    container = entry->mContainer.get();
    if (size == entry->mSize) {
      // Lack of a container is handled below.
      break;
    } else if (!container) {
      // Stop tracking if our weak pointer to the image container was freed.
      mImageContainers.RemoveElementAt(i);
    }
  }

  if (i >= 0 && container) {
    switch (entry->mLastDrawResult) {
      case DrawResult::SUCCESS:
      case DrawResult::BAD_IMAGE:
      case DrawResult::BAD_ARGS:
        return container.forget();
      case DrawResult::NOT_READY:
      case DrawResult::INCOMPLETE:
      case DrawResult::TEMPORARY_ERROR:
        // Temporary conditions where we need to rerequest the frame to recover.
        break;
      case DrawResult::WRONG_SIZE:
        // Unused by GetFrameInternal
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled DrawResult type!");
        return container.forget();
    }
  } else {
    // We need a new ImageContainer, so create one.
    container = LayerManager::CreateImageContainer();

    if (i >= 0) {
      entry->mContainer = container;
    } else {
      entry = mImageContainers.AppendElement(
        ImageContainerEntry(size, container.get()));
    }
  }

#ifdef DEBUG
  NotifyDrawingObservers();
#endif

  entry->mLastDrawResult =
    AddCurrentImage(container, size, aFlags, true);
  return container.forget();
}

void
ImageResource::UpdateImageContainer()
{
  MOZ_ASSERT(NS_IsMainThread());

  for (int i = mImageContainers.Length() - 1; i >= 0; --i) {
    ImageContainerEntry& entry = mImageContainers[i];
    RefPtr<ImageContainer> container = entry.mContainer.get();
    if (container) {
      entry.mLastDrawResult =
        AddCurrentImage(container, entry.mSize, FLAG_NONE, false);
    } else {
      // Stop tracking if our weak pointer to the image container was freed.
      mImageContainers.RemoveElementAt(i);
    }
  }
}

void
ImageResource::ReleaseImageContainer()
{
  MOZ_ASSERT(NS_IsMainThread());
  mImageContainers.Clear();
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
  mLastFrameID(0)
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
