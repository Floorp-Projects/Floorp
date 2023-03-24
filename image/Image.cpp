/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Image.h"

#include "imgRequest.h"
#include "WebRenderImageProvider.h"
#include "nsIObserverService.h"
#include "nsRefreshDriver.h"
#include "nsContentUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/SourceSurfaceRawData.h"
#include "mozilla/Services.h"
#include "mozilla/SizeOfState.h"
#include "mozilla/TimeStamp.h"
// for Tie
#include "mozilla/layers/SharedSurfacesChild.h"

namespace mozilla {
namespace image {

WebRenderImageProvider::WebRenderImageProvider(const ImageResource* aImage)
    : mProviderId(aImage->GetImageProviderId()) {}

/* static */ ImageProviderId WebRenderImageProvider::AllocateProviderId() {
  // Callable on all threads.
  static Atomic<ImageProviderId> sProviderId(0u);
  return ++sProviderId;
}

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

void ImageResource::CollectSizeOfSurfaces(
    nsTArray<SurfaceMemoryCounter>& aCounters,
    MallocSizeOf aMallocSizeOf) const {
  SurfaceCache::CollectSizeOfSurfaces(ImageKey(this), aCounters, aMallocSizeOf);
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
      mProviderId(WebRenderImageProvider::AllocateProviderId()) {}

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
    eventTarget->Dispatch(CreateRenderBlockingRunnable(ev.forget()),
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
