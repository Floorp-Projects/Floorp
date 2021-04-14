/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_Image_h
#define mozilla_image_Image_h

#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/SizeOfState.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"
#include "gfx2DGlue.h"
#include "imgIContainer.h"
#include "ImageContainer.h"
#include "LookupResult.h"
#include "nsStringFwd.h"
#include "ProgressTracker.h"
#include "SurfaceCache.h"

class imgRequest;
class nsIRequest;
class nsIInputStream;

namespace mozilla {
namespace image {

class Image;

///////////////////////////////////////////////////////////////////////////////
// Memory Reporting
///////////////////////////////////////////////////////////////////////////////

struct MemoryCounter {
  MemoryCounter()
      : mSource(0),
        mDecodedHeap(0),
        mDecodedNonHeap(0),
        mDecodedUnknown(0),
        mExternalHandles(0),
        mFrameIndex(0),
        mExternalId(0),
        mSurfaceTypes(0) {}

  void SetSource(size_t aCount) { mSource = aCount; }
  size_t Source() const { return mSource; }
  void SetDecodedHeap(size_t aCount) { mDecodedHeap = aCount; }
  size_t DecodedHeap() const { return mDecodedHeap; }
  void SetDecodedNonHeap(size_t aCount) { mDecodedNonHeap = aCount; }
  size_t DecodedNonHeap() const { return mDecodedNonHeap; }
  void SetDecodedUnknown(size_t aCount) { mDecodedUnknown = aCount; }
  size_t DecodedUnknown() const { return mDecodedUnknown; }
  void SetExternalHandles(size_t aCount) { mExternalHandles = aCount; }
  size_t ExternalHandles() const { return mExternalHandles; }
  void SetFrameIndex(size_t aIndex) { mFrameIndex = aIndex; }
  size_t FrameIndex() const { return mFrameIndex; }
  void SetExternalId(uint64_t aId) { mExternalId = aId; }
  uint64_t ExternalId() const { return mExternalId; }
  void SetSurfaceTypes(uint32_t aTypes) { mSurfaceTypes = aTypes; }
  uint32_t SurfaceTypes() const { return mSurfaceTypes; }

  MemoryCounter& operator+=(const MemoryCounter& aOther) {
    mSource += aOther.mSource;
    mDecodedHeap += aOther.mDecodedHeap;
    mDecodedNonHeap += aOther.mDecodedNonHeap;
    mDecodedUnknown += aOther.mDecodedUnknown;
    mExternalHandles += aOther.mExternalHandles;
    mSurfaceTypes |= aOther.mSurfaceTypes;
    return *this;
  }

 private:
  size_t mSource;
  size_t mDecodedHeap;
  size_t mDecodedNonHeap;
  size_t mDecodedUnknown;
  size_t mExternalHandles;
  size_t mFrameIndex;
  uint64_t mExternalId;
  uint32_t mSurfaceTypes;
};

enum class SurfaceMemoryCounterType { NORMAL, COMPOSITING, COMPOSITING_PREV };

struct SurfaceMemoryCounter {
  SurfaceMemoryCounter(
      const SurfaceKey& aKey, bool aIsLocked, bool aCannotSubstitute,
      bool aIsFactor2, bool aFinished,
      SurfaceMemoryCounterType aType = SurfaceMemoryCounterType::NORMAL)
      : mKey(aKey),
        mType(aType),
        mIsLocked(aIsLocked),
        mCannotSubstitute(aCannotSubstitute),
        mIsFactor2(aIsFactor2),
        mFinished(aFinished) {}

  const SurfaceKey& Key() const { return mKey; }
  MemoryCounter& Values() { return mValues; }
  const MemoryCounter& Values() const { return mValues; }
  SurfaceMemoryCounterType Type() const { return mType; }
  bool IsLocked() const { return mIsLocked; }
  bool CannotSubstitute() const { return mCannotSubstitute; }
  bool IsFactor2() const { return mIsFactor2; }
  bool IsFinished() const { return mFinished; }

 private:
  const SurfaceKey mKey;
  MemoryCounter mValues;
  const SurfaceMemoryCounterType mType;
  const bool mIsLocked;
  const bool mCannotSubstitute;
  const bool mIsFactor2;
  const bool mFinished;
};

struct ImageMemoryCounter {
  ImageMemoryCounter(imgRequest* aRequest, SizeOfState& aState, bool aIsUsed);
  ImageMemoryCounter(imgRequest* aRequest, Image* aImage, SizeOfState& aState,
                     bool aIsUsed);

  nsCString& URI() { return mURI; }
  const nsCString& URI() const { return mURI; }
  const nsTArray<SurfaceMemoryCounter>& Surfaces() const { return mSurfaces; }
  const gfx::IntSize IntrinsicSize() const { return mIntrinsicSize; }
  const MemoryCounter& Values() const { return mValues; }
  uint32_t Progress() const { return mProgress; }
  uint16_t Type() const { return mType; }
  bool IsUsed() const { return mIsUsed; }
  bool HasError() const { return mHasError; }
  bool IsValidating() const { return mValidating; }

  bool IsNotable() const {
    // Errors or requests without images are always notable.
    if (mHasError || mValidating || mProgress == UINT32_MAX ||
        mProgress & FLAG_HAS_ERROR || mType == imgIContainer::TYPE_REQUEST) {
      return true;
    }

    // Sufficiently large images are notable.
    const size_t NotableThreshold = 16 * 1024;
    size_t total = mValues.Source() + mValues.DecodedHeap() +
                   mValues.DecodedNonHeap() + mValues.DecodedUnknown();
    if (total >= NotableThreshold) {
      return true;
    }

    // Incomplete images are always notable as well; the odds of capturing
    // mid-decode should be fairly low.
    for (const auto& surface : mSurfaces) {
      if (!surface.IsFinished()) {
        return true;
      }
    }

    return false;
  }

 private:
  nsCString mURI;
  nsTArray<SurfaceMemoryCounter> mSurfaces;
  gfx::IntSize mIntrinsicSize;
  MemoryCounter mValues;
  uint32_t mProgress;
  uint16_t mType;
  const bool mIsUsed;
  bool mHasError;
  bool mValidating;
};

///////////////////////////////////////////////////////////////////////////////
// Image Base Types
///////////////////////////////////////////////////////////////////////////////

class Image : public imgIContainer {
 public:
  /**
   * Flags for Image initialization.
   *
   * Meanings:
   *
   * INIT_FLAG_NONE: Lack of flags
   *
   * INIT_FLAG_DISCARDABLE: The container should be discardable
   *
   * INIT_FLAG_DECODE_IMMEDIATELY: The container should decode as soon as
   * possible, regardless of what our heuristics say.
   *
   * INIT_FLAG_TRANSIENT: The container is likely to exist for only a short time
   * before being destroyed. (For example, containers for
   * multipart/x-mixed-replace image parts fall into this category.) If this
   * flag is set, INIT_FLAG_DISCARDABLE and INIT_FLAG_DECODE_ONLY_ON_DRAW must
   * not be set.
   *
   * INIT_FLAG_SYNC_LOAD: The container is being loaded synchronously, so
   * it should avoid relying on async workers to get the container ready.
   */
  static const uint32_t INIT_FLAG_NONE = 0x0;
  static const uint32_t INIT_FLAG_DISCARDABLE = 0x1;
  static const uint32_t INIT_FLAG_DECODE_IMMEDIATELY = 0x2;
  static const uint32_t INIT_FLAG_TRANSIENT = 0x4;
  static const uint32_t INIT_FLAG_SYNC_LOAD = 0x8;

  virtual already_AddRefed<ProgressTracker> GetProgressTracker() = 0;
  virtual void SetProgressTracker(ProgressTracker* aProgressTracker) {}

  /**
   * The size, in bytes, occupied by the compressed source data of the image.
   * If MallocSizeOf does not work on this platform, uses a fallback approach to
   * ensure that something reasonable is always returned.
   */
  virtual size_t SizeOfSourceWithComputedFallback(
      SizeOfState& aState) const = 0;

  /**
   * Collect an accounting of the memory occupied by the image's surfaces (which
   * together make up its decoded data). Each surface is recorded as a separate
   * SurfaceMemoryCounter, stored in @aCounters.
   */
  virtual void CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                     MallocSizeOf aMallocSizeOf) const = 0;

  virtual void IncrementAnimationConsumers() = 0;
  virtual void DecrementAnimationConsumers() = 0;
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() = 0;
#endif

  /**
   * Called from OnDataAvailable when the stream associated with the image has
   * received new image data. The arguments are the same as OnDataAvailable's,
   * but by separating this functionality into a different method we don't
   * interfere with subclasses which wish to implement nsIStreamListener.
   *
   * Images should not do anything that could send out notifications until they
   * have received their first OnImageDataAvailable notification; in
   * particular, this means that instantiating decoders should be deferred
   * until OnImageDataAvailable is called.
   */
  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) = 0;

  /**
   * Called from OnStopRequest when the image's underlying request completes.
   *
   * @param aRequest  The completed request.
   * @param aContext  Context from Necko's OnStopRequest.
   * @param aStatus   A success or failure code.
   * @param aLastPart Whether this is the final part of the underlying request.
   */
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext, nsresult aStatus,
                                       bool aLastPart) = 0;

  /**
   * Called when the SurfaceCache discards a surface belonging to this image.
   */
  virtual void OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey) = 0;

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) = 0;
  virtual uint64_t InnerWindowID() const = 0;

  virtual bool HasError() = 0;
  virtual void SetHasError() = 0;

  virtual nsIURI* GetURI() const = 0;

  NS_IMETHOD GetHotspotX(int32_t* aX) override {
    *aX = 0;
    return NS_OK;
  }
  NS_IMETHOD GetHotspotY(int32_t* aY) override {
    *aY = 0;
    return NS_OK;
  }
};

class ImageResource : public Image {
 public:
  already_AddRefed<ProgressTracker> GetProgressTracker() override {
    RefPtr<ProgressTracker> progressTracker = mProgressTracker;
    MOZ_ASSERT(progressTracker);
    return progressTracker.forget();
  }

  void SetProgressTracker(ProgressTracker* aProgressTracker) final {
    MOZ_ASSERT(aProgressTracker);
    MOZ_ASSERT(!mProgressTracker);
    mProgressTracker = aProgressTracker;
  }

  virtual void IncrementAnimationConsumers() override;
  virtual void DecrementAnimationConsumers() override;
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() override {
    return mAnimationConsumers;
  }
#endif

  virtual void OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey) override {}

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) override {
    mInnerWindowId = aInnerWindowId;
  }
  virtual uint64_t InnerWindowID() const override { return mInnerWindowId; }

  virtual bool HasError() override { return mError; }
  virtual void SetHasError() override { mError = true; }

  /*
   * Returns a non-AddRefed pointer to the URI associated with this image.
   * Illegal to use off-main-thread.
   */
  nsIURI* GetURI() const override { return mURI; }

 protected:
  explicit ImageResource(nsIURI* aURI);
  ~ImageResource();

  layers::ContainerProducerID GetImageProducerId() const {
    return mImageProducerID;
  }

  bool GetSpecTruncatedTo1k(nsCString& aSpec) const;

  // Shared functionality for implementors of imgIContainer. Every
  // implementation of attribute animationMode should forward here.
  nsresult GetAnimationModeInternal(uint16_t* aAnimationMode);
  nsresult SetAnimationModeInternal(uint16_t aAnimationMode);

  /**
   * Helper for RequestRefresh.
   *
   * If we've had a "recent" refresh (i.e. if this image is being used in
   * multiple documents & some other document *just* called RequestRefresh() on
   * this image with a timestamp close to aTime), this method returns true.
   *
   * Otherwise, this method updates mLastRefreshTime to aTime & returns false.
   */
  bool HadRecentRefresh(const TimeStamp& aTime);

  /**
   * Decides whether animation should or should not be happening,
   * and makes sure the right thing is being done.
   */
  virtual void EvaluateAnimation();

  /**
   * Extended by child classes, if they have additional
   * conditions for being able to animate.
   */
  virtual bool ShouldAnimate() {
    return mAnimationConsumers > 0 && mAnimationMode != kDontAnimMode;
  }

  virtual nsresult StartAnimation() = 0;
  virtual nsresult StopAnimation() = 0;

  void SendOnUnlockedDraw(uint32_t aFlags);

#ifdef DEBUG
  // Records the image drawing for startup performance testing.
  void NotifyDrawingObservers();
#endif

  // Member data shared by all implementations of this abstract class
  RefPtr<ProgressTracker> mProgressTracker;
  nsCOMPtr<nsIURI> mURI;
  TimeStamp mLastRefreshTime;
  uint64_t mInnerWindowId;
  uint32_t mAnimationConsumers;
  uint16_t mAnimationMode;  // Enum values in imgIContainer
  bool mInitialized : 1;    // Have we been initialized?
  bool mAnimating : 1;      // Are we currently animating?
  bool mError : 1;          // Error handling

  /**
   * Attempt to find a matching cached surface in the SurfaceCache, and if not
   * available, request the production of such a surface (either synchronously
   * or asynchronously).
   *
   * If the draw result is BAD_IMAGE, BAD_ARGS or NOT_READY, the size will be
   * the same as aSize. If it is TEMPORARY_ERROR, INCOMPLETE, or SUCCESS, the
   * size is a hint as to what we expect the surface size to be, once the best
   * fitting size is available. It may or may not match the size of the surface
   * returned at this moment. This is useful for choosing how to store the final
   * result (e.g. if going into an ImageContainer, ideally we would share the
   * same container for many requested sizes, if they all end up with the same
   * best fit size in the end).
   *
   * A valid surface should only be returned for SUCCESS and INCOMPLETE.
   *
   * Any other draw result is invalid.
   */
  virtual Tuple<ImgDrawResult, gfx::IntSize, RefPtr<gfx::SourceSurface>>
  GetFrameInternal(const gfx::IntSize& aSize,
                   const Maybe<SVGImageContext>& aSVGContext,
                   uint32_t aWhichFrame, uint32_t aFlags) {
    return MakeTuple(ImgDrawResult::BAD_IMAGE, aSize,
                     RefPtr<gfx::SourceSurface>());
  }

  /**
   * Calculate the estimated size to use for an image container with the given
   * parameters. It may not be the same as the given size, and it may not be
   * the same as the size of the surface in the image container, but it is the
   * best effort estimate.
   */
  virtual Tuple<ImgDrawResult, gfx::IntSize> GetImageContainerSize(
      layers::LayerManager* aManager, const gfx::IntSize& aSize,
      uint32_t aFlags) {
    return MakeTuple(ImgDrawResult::NOT_SUPPORTED, gfx::IntSize(0, 0));
  }

  ImgDrawResult GetImageContainerImpl(layers::LayerManager* aManager,
                                      const gfx::IntSize& aSize,
                                      const Maybe<SVGImageContext>& aSVGContext,
                                      uint32_t aFlags,
                                      layers::ImageContainer** aContainer);

  /**
   * Re-requests the appropriate frames for each image container using
   * GetFrameInternal.
   * @returns True if any image containers were updated, else false.
   */
  bool UpdateImageContainer(const Maybe<gfx::IntRect>& aDirtyRect);

  void ReleaseImageContainer();

  class MOZ_RAII AutoProfilerImagePaintMarker {
   public:
    explicit AutoProfilerImagePaintMarker(ImageResource* self)
        : mStartTime(TimeStamp::NowUnfuzzed()) {
      nsAutoCString spec;
      if (self->mURI && profiler_can_accept_markers()) {
        static const size_t sMaxTruncatedLength = 1024;
        self->mURI->GetSpec(mSpec);
        if (mSpec.Length() >= sMaxTruncatedLength) {
          mSpec.Truncate(sMaxTruncatedLength);
        }
      }
    }

    ~AutoProfilerImagePaintMarker() {
      if (!mSpec.IsEmpty()) {
        PROFILER_MARKER_TEXT("Image Paint", GRAPHICS,
                             MarkerTiming::IntervalUntilNowFrom(mStartTime),
                             mSpec);
      }
    }

   protected:
    TimeStamp mStartTime;
    nsAutoCString mSpec;
  };

 private:
  void SetCurrentImage(layers::ImageContainer* aContainer,
                       gfx::SourceSurface* aSurface,
                       const Maybe<gfx::IntRect>& aDirtyRect);

  struct ImageContainerEntry {
    ImageContainerEntry(const gfx::IntSize& aSize,
                        const Maybe<SVGImageContext>& aSVGContext,
                        layers::ImageContainer* aContainer, uint32_t aFlags)
        : mSize(aSize),
          mSVGContext(aSVGContext),
          mContainer(aContainer),
          mLastDrawResult(ImgDrawResult::NOT_READY),
          mFlags(aFlags) {}

    gfx::IntSize mSize;
    Maybe<SVGImageContext> mSVGContext;
    // A weak pointer to our ImageContainer, which stays alive only as long as
    // the layer system needs it.
    ThreadSafeWeakPtr<layers::ImageContainer> mContainer;
    // If mContainer is non-null, this contains the ImgDrawResult we obtained
    // the last time we updated it.
    ImgDrawResult mLastDrawResult;
    // Cached flags to use for decoding. FLAG_ASYNC_NOTIFY should always be set
    // but FLAG_HIGH_QUALITY_SCALING may vary.
    uint32_t mFlags;
  };

  AutoTArray<ImageContainerEntry, 1> mImageContainers;
  layers::ImageContainer::ProducerID mImageProducerID;
  layers::ImageContainer::FrameID mLastFrameID;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_Image_h
