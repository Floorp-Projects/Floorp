/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Image.h"

#include "imgRequest.h"
#include "Layers.h"  // for LayerManager
#include "nsIObserverService.h"
#include "nsRefreshDriver.h"
#include "nsContentUtils.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Services.h"
#include "mozilla/SizeOfState.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"  // for Tie
#include "mozilla/layers/SharedSurfacesChild.h"

namespace mozilla {
namespace image {

///////////////////////////////////////////////////////////////////////////////
// Memory Reporting
///////////////////////////////////////////////////////////////////////////////

ImageMemoryCounter::ImageMemoryCounter(imgRequest* aRequest,
                                       SizeOfState& aState, bool aIsUsed)
    : mProgress(UINT32_MAX),
      mType(UINT16_MAX),
      mIsUsed(aIsUsed),
      mHasError(false),
      mValidating(false) {
  MOZ_ASSERT(aRequest);

  // We don't have the image object yet, but we can get some information.
  nsCOMPtr<nsIURI> imageURL;
  nsresult rv = aRequest->GetURI(getter_AddRefs(imageURL));
  if (NS_SUCCEEDED(rv) && imageURL) {
    imageURL->GetSpec(mURI);
  }

  mType = imgIContainer::TYPE_REQUEST;
  mHasError = NS_FAILED(aRequest->GetImageErrorCode());
  mValidating = !!aRequest->GetValidator();

  RefPtr<ProgressTracker> tracker = aRequest->GetProgressTracker();
  if (tracker) {
    mProgress = tracker->GetProgress();
  }
}

ImageMemoryCounter::ImageMemoryCounter(imgRequest* aRequest, Image* aImage,
                                       SizeOfState& aState, bool aIsUsed)
    : mProgress(UINT32_MAX),
      mType(UINT16_MAX),
      mIsUsed(aIsUsed),
      mHasError(false),
      mValidating(false) {
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aImage);

  // Extract metadata about the image.
  nsCOMPtr<nsIURI> imageURL(aImage->GetURI());
  if (imageURL) {
    imageURL->GetSpec(mURI);
  }

  int32_t width = 0;
  int32_t height = 0;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);
  mIntrinsicSize.SizeTo(width, height);

  mType = aImage->GetType();
  mHasError = aImage->HasError();
  mValidating = !!aRequest->GetValidator();

  RefPtr<ProgressTracker> tracker = aImage->GetProgressTracker();
  if (tracker) {
    mProgress = tracker->GetProgress();
  }

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

bool ImageResource::GetSpecTruncatedTo1k(nsCString& aSpec) const {
  static const size_t sMaxTruncatedLength = 1024;

  mURI->GetSpec(aSpec);
  if (sMaxTruncatedLength >= aSpec.Length()) {
    return true;
  }

  aSpec.Truncate(sMaxTruncatedLength);
  return false;
}

void ImageResource::SetCurrentImage(layers::ImageContainer* aContainer,
                                    gfx::SourceSurface* aSurface,
                                    const Maybe<gfx::IntRect>& aDirtyRect) {
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
  AutoTArray<layers::ImageContainer::NonOwningImage, 1> imageList;
  imageList.AppendElement(layers::ImageContainer::NonOwningImage(
      image, TimeStamp(), mLastFrameID++, mImageProducerID));

  if (aDirtyRect) {
    aContainer->SetCurrentImagesInTransaction(imageList);
  } else {
    aContainer->SetCurrentImages(imageList);
  }

  // If we are animated, then we should request that the image container be
  // treated as such, to avoid display list rebuilding to update frames for
  // WebRender.
  if (mProgressTracker->GetProgress() & FLAG_IS_ANIMATED) {
    if (aDirtyRect) {
      layers::SharedSurfacesChild::UpdateAnimation(aContainer, aSurface,
                                                   aDirtyRect.ref());
    } else {
      gfx::IntRect dirtyRect(gfx::IntPoint(0, 0), aSurface->GetSize());
      layers::SharedSurfacesChild::UpdateAnimation(aContainer, aSurface,
                                                   dirtyRect);
    }
  }
}

ImgDrawResult ImageResource::GetImageContainerImpl(
    layers::LayerManager* aManager, const gfx::IntSize& aSize,
    const Maybe<SVGImageContext>& aSVGContext, uint32_t aFlags,
    layers::ImageContainer** aOutContainer) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(
      (aFlags & ~(FLAG_SYNC_DECODE | FLAG_SYNC_DECODE_IF_FAST |
                  FLAG_ASYNC_NOTIFY | FLAG_HIGH_QUALITY_SCALING)) == FLAG_NONE,
      "Unsupported flag passed to GetImageContainer");

  ImgDrawResult drawResult;
  gfx::IntSize size;
  Tie(drawResult, size) = GetImageContainerSize(aManager, aSize, aFlags);
  if (drawResult != ImgDrawResult::SUCCESS) {
    return drawResult;
  }

  MOZ_ASSERT(!size.IsEmpty());

  if (mAnimationConsumers == 0) {
    SendOnUnlockedDraw(aFlags);
  }

  uint32_t flags = (aFlags & ~(FLAG_SYNC_DECODE | FLAG_SYNC_DECODE_IF_FAST)) |
                   FLAG_ASYNC_NOTIFY;
  RefPtr<layers::ImageContainer> container;
  ImageContainerEntry* entry = nullptr;
  int i = mImageContainers.Length() - 1;
  for (; i >= 0; --i) {
    entry = &mImageContainers[i];
    if (size == entry->mSize && flags == entry->mFlags &&
        aSVGContext == entry->mSVGContext) {
      // Lack of a container is handled below.
      container = RefPtr<layers::ImageContainer>(entry->mContainer);
      break;
    } else if (entry->mContainer.IsDead()) {
      // Stop tracking if our weak pointer to the image container was freed.
      mImageContainers.RemoveElementAt(i);
    }
  }

  if (container) {
    switch (entry->mLastDrawResult) {
      case ImgDrawResult::SUCCESS:
      case ImgDrawResult::BAD_IMAGE:
      case ImgDrawResult::BAD_ARGS:
      case ImgDrawResult::NOT_SUPPORTED:
        container.forget(aOutContainer);
        return entry->mLastDrawResult;
      case ImgDrawResult::NOT_READY:
      case ImgDrawResult::INCOMPLETE:
      case ImgDrawResult::TEMPORARY_ERROR:
        // Temporary conditions where we need to rerequest the frame to recover.
        break;
      case ImgDrawResult::WRONG_SIZE:
        // Unused by GetFrameInternal
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled ImgDrawResult type!");
        container.forget(aOutContainer);
        return entry->mLastDrawResult;
    }
  }

  AutoProfilerImagePaintMarker PROFILER_RAII(this);
#ifdef DEBUG
  NotifyDrawingObservers();
#endif

  gfx::IntSize bestSize;
  RefPtr<gfx::SourceSurface> surface;
  Tie(drawResult, bestSize, surface) = GetFrameInternal(
      size, aSVGContext, FRAME_CURRENT, aFlags | FLAG_ASYNC_NOTIFY);

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
        container = RefPtr<layers::ImageContainer>(entry->mContainer);
        if (container) {
          switch (entry->mLastDrawResult) {
            case ImgDrawResult::SUCCESS:
            case ImgDrawResult::BAD_IMAGE:
            case ImgDrawResult::BAD_ARGS:
            case ImgDrawResult::NOT_SUPPORTED:
              container.forget(aOutContainer);
              return entry->mLastDrawResult;
            case ImgDrawResult::NOT_READY:
            case ImgDrawResult::INCOMPLETE:
            case ImgDrawResult::TEMPORARY_ERROR:
              // Temporary conditions where we need to rerequest the frame to
              // recover. We have already done so!
              break;
            case ImgDrawResult::WRONG_SIZE:
              // Unused by GetFrameInternal
            default:
              MOZ_ASSERT_UNREACHABLE("Unhandled DrawResult type!");
              container.forget(aOutContainer);
              return entry->mLastDrawResult;
          }
        }
        break;
      }
    }
  }

  if (!container) {
    // We need a new ImageContainer, so create one.
    container = layers::LayerManager::CreateImageContainer();

    if (i >= 0) {
      entry->mContainer = container;
    } else {
      entry = mImageContainers.AppendElement(
          ImageContainerEntry(bestSize, aSVGContext, container.get(), flags));
    }
  }

  SetCurrentImage(container, surface, Nothing());
  entry->mLastDrawResult = drawResult;
  container.forget(aOutContainer);
  return drawResult;
}

bool ImageResource::UpdateImageContainer(
    const Maybe<gfx::IntRect>& aDirtyRect) {
  MOZ_ASSERT(NS_IsMainThread());

  for (int i = mImageContainers.Length() - 1; i >= 0; --i) {
    ImageContainerEntry& entry = mImageContainers[i];
    RefPtr<layers::ImageContainer> container(entry.mContainer);
    if (container) {
      gfx::IntSize bestSize;
      RefPtr<gfx::SourceSurface> surface;
      Tie(entry.mLastDrawResult, bestSize, surface) = GetFrameInternal(
          entry.mSize, entry.mSVGContext, FRAME_CURRENT, entry.mFlags);

      // It is possible that this is a factor-of-2 substitution. Since we
      // managed to convert the weak reference into a strong reference, that
      // means that an imagelib user still is holding onto the container. thus
      // we cannot consolidate and must keep updating the duplicate container.
      if (aDirtyRect) {
        SetCurrentImage(container, surface, aDirtyRect);
      } else {
        gfx::IntRect dirtyRect(gfx::IntPoint(0, 0), bestSize);
        SetCurrentImage(container, surface, Some(dirtyRect));
      }
    } else {
      // Stop tracking if our weak pointer to the image container was freed.
      mImageContainers.RemoveElementAt(i);
    }
  }

  return !mImageContainers.IsEmpty();
}

void ImageResource::ReleaseImageContainer() {
  MOZ_ASSERT(NS_IsMainThread());
  mImageContainers.Clear();
}

// Constructor
ImageResource::ImageResource(nsIURI* aURI)
    : mURI(aURI),
      mInnerWindowId(0),
      mAnimationConsumers(0),
      mAnimationMode(kNormalAnimMode),
      mInitialized(false),
      mAnimating(false),
      mError(false),
      mImageProducerID(layers::ImageContainer::AllocateProducerID()),
      mLastFrameID(0) {}

ImageResource::~ImageResource() {
  // Ask our ProgressTracker to drop its weak reference to us.
  mProgressTracker->ResetImage();
}

void ImageResource::IncrementAnimationConsumers() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Main thread only to encourage serialization "
             "with DecrementAnimationConsumers");
  mAnimationConsumers++;
}

void ImageResource::DecrementAnimationConsumers() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Main thread only to encourage serialization "
             "with IncrementAnimationConsumers");
  MOZ_ASSERT(mAnimationConsumers >= 1, "Invalid no. of animation consumers!");
  mAnimationConsumers--;
}

nsresult ImageResource::GetAnimationModeInternal(uint16_t* aAnimationMode) {
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_ARG_POINTER(aAnimationMode);

  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

nsresult ImageResource::SetAnimationModeInternal(uint16_t aAnimationMode) {
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

bool ImageResource::HadRecentRefresh(const TimeStamp& aTime) {
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

void ImageResource::EvaluateAnimation() {
  if (!mAnimating && ShouldAnimate()) {
    nsresult rv = StartAnimation();
    mAnimating = NS_SUCCEEDED(rv);
  } else if (mAnimating && !ShouldAnimate()) {
    StopAnimation();
  }
}

void ImageResource::SendOnUnlockedDraw(uint32_t aFlags) {
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
    eventTarget->Dispatch(CreateMediumHighRunnable(ev.forget()),
                          NS_DISPATCH_NORMAL);
  }
}

#ifdef DEBUG
void ImageResource::NotifyDrawingObservers() {
  if (!mURI || !NS_IsMainThread()) {
    return;
  }

  if (!mURI->SchemeIs("resource") && !mURI->SchemeIs("chrome")) {
    return;
  }

  // Record the image drawing for startup performance testing.
  nsCOMPtr<nsIURI> uri = mURI;
  nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
      "image::ImageResource::NotifyDrawingObservers", [uri]() {
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        NS_WARNING_ASSERTION(obs, "Can't get an observer service handle");
        if (obs) {
          nsAutoCString spec;
          uri->GetSpec(spec);
          obs->NotifyObservers(nullptr, "image-drawing",
                               NS_ConvertUTF8toUTF16(spec).get());
        }
      }));
}
#endif

}  // namespace image
}  // namespace mozilla
