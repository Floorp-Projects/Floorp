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

void
ImageResource::SetCurrentImage(ImageContainer* aContainer,
                               SourceSurface* aSurface,
                               bool aInTransaction)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContainer);

  if (!aSurface) {
    // The OS threw out some or all of our buffer. We'll need to wait for the
    // redecode (which was automatically triggered by GetFrame) to complete.
    return;
  }

  // |image| holds a reference to a SourceSurface which in turn holds a lock on
  // the current frame's data buffer, ensuring that it doesn't get freed as
  // long as the layer system keeps this ImageContainer alive.
  RefPtr<layers::Image> image = new layers::SourceSurfaceImage(aSurface);

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
}

already_AddRefed<ImageContainer>
ImageResource::GetImageContainerImpl(LayerManager* aManager,
                                     const IntSize& aSize,
                                     const Maybe<SVGImageContext>& aSVGContext,
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

  uint32_t flags = (aFlags & ~(FLAG_SYNC_DECODE |
                               FLAG_SYNC_DECODE_IF_FAST)) | FLAG_ASYNC_NOTIFY;
  RefPtr<layers::ImageContainer> container;
  ImageContainerEntry* entry = nullptr;
  int i = mImageContainers.Length() - 1;
  for (; i >= 0; --i) {
    entry = &mImageContainers[i];
    container = entry->mContainer.get();
    if (size == entry->mSize && flags == entry->mFlags &&
        aSVGContext == entry->mSVGContext) {
      // Lack of a container is handled below.
      break;
    } else if (!container) {
      // Stop tracking if our weak pointer to the image container was freed.
      mImageContainers.RemoveElementAt(i);
    } else {
      // It isn't a match, but still valid. Forget the container so we don't
      // try to reuse it below.
      container = nullptr;
    }
  }

  if (container) {
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
  }

#ifdef DEBUG
  NotifyDrawingObservers();
#endif

  DrawResult drawResult;
  IntSize bestSize;
  RefPtr<SourceSurface> surface;
  Tie(drawResult, bestSize, surface) =
    GetFrameInternal(size, aSVGContext, FRAME_CURRENT,
                     aFlags | FLAG_ASYNC_NOTIFY);

  // The requested size might be refused by the surface cache (i.e. due to
  // factor-of-2 mode). In that case we don't want to create an entry for this
  // specific size, but rather re-use the entry for the substituted size.
  if (bestSize != size) {
    MOZ_ASSERT(!bestSize.IsEmpty());

    // We can only remove the entry if we no longer have a container, because if
    // there are strong references to it remaining, we need to still update it
    // in UpdateImageContainer.
    if (i >= 0 && !container) {
      mImageContainers.RemoveElementAt(i);
    }

    // Forget about the stale container, if any. This lets the entry creation
    // logic do its job below, if it turns out there is no existing best entry
    // or the best entry doesn't have a container.
    container = nullptr;

    // We need to do the entry search again for the new size. We skip pruning
    // because we did this above once already, but ImageContainer is threadsafe,
    // so there is a remote possibility it got freed.
    i = mImageContainers.Length() - 1;
    for (; i >= 0; --i) {
      entry = &mImageContainers[i];
      if (bestSize == entry->mSize && flags == entry->mFlags &&
          aSVGContext == entry->mSVGContext) {
        container = entry->mContainer.get();
        if (container) {
          switch (entry->mLastDrawResult) {
            case DrawResult::SUCCESS:
            case DrawResult::BAD_IMAGE:
            case DrawResult::BAD_ARGS:
              return container.forget();
            case DrawResult::NOT_READY:
            case DrawResult::INCOMPLETE:
            case DrawResult::TEMPORARY_ERROR:
              // Temporary conditions where we need to rerequest the frame to
              // recover. We have already done so!
              break;
           case DrawResult::WRONG_SIZE:
              // Unused by GetFrameInternal
            default:
              MOZ_ASSERT_UNREACHABLE("Unhandled DrawResult type!");
              return container.forget();
          }
        }
        break;
      }
    }
  }

  if (!container) {
    // We need a new ImageContainer, so create one.
    container = LayerManager::CreateImageContainer();

    if (i >= 0) {
      entry->mContainer = container;
    } else {
      entry = mImageContainers.AppendElement(
        ImageContainerEntry(bestSize, aSVGContext, container.get(), flags));
    }
  }

  SetCurrentImage(container, surface, true);
  entry->mLastDrawResult = drawResult;
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
      IntSize bestSize;
      RefPtr<SourceSurface> surface;
      Tie(entry.mLastDrawResult, bestSize, surface) =
        GetFrameInternal(entry.mSize, entry.mSVGContext,
                         FRAME_CURRENT, entry.mFlags);

      // It is possible that this is a factor-of-2 substitution. Since we
      // managed to convert the weak reference into a strong reference, that
      // means that an imagelib user still is holding onto the container. thus
      // we cannot consolidate and must keep updating the duplicate container.
      SetCurrentImage(container, surface, false);
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
