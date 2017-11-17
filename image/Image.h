/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_Image_h
#define mozilla_image_Image_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/Tuple.h"
#include "mozilla/TimeStamp.h"
#include "gfx2DGlue.h"
#include "imgIContainer.h"
#include "ImageURL.h"
#include "ImageContainer.h"
#include "LookupResult.h"
#include "nsStringFwd.h"
#include "ProgressTracker.h"
#include "SurfaceCache.h"

class nsIRequest;
class nsIInputStream;

namespace mozilla {
namespace image {

class Image;

///////////////////////////////////////////////////////////////////////////////
// Memory Reporting
///////////////////////////////////////////////////////////////////////////////

struct MemoryCounter
{
  MemoryCounter()
    : mSource(0)
    , mDecodedHeap(0)
    , mDecodedNonHeap(0)
    , mSharedHandles(0)
  { }

  void SetSource(size_t aCount) { mSource = aCount; }
  size_t Source() const { return mSource; }
  void SetDecodedHeap(size_t aCount) { mDecodedHeap = aCount; }
  size_t DecodedHeap() const { return mDecodedHeap; }
  void SetDecodedNonHeap(size_t aCount) { mDecodedNonHeap = aCount; }
  size_t DecodedNonHeap() const { return mDecodedNonHeap; }
  void SetSharedHandles(size_t aCount) { mSharedHandles = aCount; }
  size_t SharedHandles() const { return mSharedHandles; }

  MemoryCounter& operator+=(const MemoryCounter& aOther)
  {
    mSource += aOther.mSource;
    mDecodedHeap += aOther.mDecodedHeap;
    mDecodedNonHeap += aOther.mDecodedNonHeap;
    mSharedHandles += aOther.mSharedHandles;
    return *this;
  }

private:
  size_t mSource;
  size_t mDecodedHeap;
  size_t mDecodedNonHeap;
  size_t mSharedHandles;
};

enum class SurfaceMemoryCounterType
{
  NORMAL,
  COMPOSITING,
  COMPOSITING_PREV
};

struct SurfaceMemoryCounter
{
  SurfaceMemoryCounter(const SurfaceKey& aKey,
                       bool aIsLocked,
                       bool aCannotSubstitute,
                       bool aIsFactor2,
                       SurfaceMemoryCounterType aType =
                         SurfaceMemoryCounterType::NORMAL)
    : mKey(aKey)
    , mType(aType)
    , mIsLocked(aIsLocked)
    , mCannotSubstitute(aCannotSubstitute)
    , mIsFactor2(aIsFactor2)
  { }

  const SurfaceKey& Key() const { return mKey; }
  MemoryCounter& Values() { return mValues; }
  const MemoryCounter& Values() const { return mValues; }
  SurfaceMemoryCounterType Type() const { return mType; }
  bool IsLocked() const { return mIsLocked; }
  bool CannotSubstitute() const { return mCannotSubstitute; }
  bool IsFactor2() const { return mIsFactor2; }

private:
  const SurfaceKey mKey;
  MemoryCounter mValues;
  const SurfaceMemoryCounterType mType;
  const bool mIsLocked;
  const bool mCannotSubstitute;
  const bool mIsFactor2;
};

struct ImageMemoryCounter
{
  ImageMemoryCounter(Image* aImage, SizeOfState& aState, bool aIsUsed);

  nsCString& URI() { return mURI; }
  const nsCString& URI() const { return mURI; }
  const nsTArray<SurfaceMemoryCounter>& Surfaces() const { return mSurfaces; }
  const gfx::IntSize IntrinsicSize() const { return mIntrinsicSize; }
  const MemoryCounter& Values() const { return mValues; }
  uint16_t Type() const { return mType; }
  bool IsUsed() const { return mIsUsed; }

  bool IsNotable() const
  {
    const size_t NotableThreshold = 16 * 1024;
    size_t total = mValues.Source() + mValues.DecodedHeap()
                                    + mValues.DecodedNonHeap();
    return total >= NotableThreshold;
  }

private:
  nsCString mURI;
  nsTArray<SurfaceMemoryCounter> mSurfaces;
  gfx::IntSize mIntrinsicSize;
  MemoryCounter mValues;
  uint16_t mType;
  const bool mIsUsed;
};


///////////////////////////////////////////////////////////////////////////////
// Image Base Types
///////////////////////////////////////////////////////////////////////////////

class Image : public imgIContainer
{
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
  static const uint32_t INIT_FLAG_NONE                     = 0x0;
  static const uint32_t INIT_FLAG_DISCARDABLE              = 0x1;
  static const uint32_t INIT_FLAG_DECODE_IMMEDIATELY       = 0x2;
  static const uint32_t INIT_FLAG_TRANSIENT                = 0x4;
  static const uint32_t INIT_FLAG_SYNC_LOAD                = 0x8;

  virtual already_AddRefed<ProgressTracker> GetProgressTracker() = 0;
  virtual void SetProgressTracker(ProgressTracker* aProgressTracker) {}

  /**
   * The size, in bytes, occupied by the compressed source data of the image.
   * If MallocSizeOf does not work on this platform, uses a fallback approach to
   * ensure that something reasonable is always returned.
   */
  virtual size_t
    SizeOfSourceWithComputedFallback(SizeOfState& aState) const = 0;

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
                                       nsISupports* aContext,
                                       nsresult aStatus,
                                       bool aLastPart) = 0;

  /**
   * Called when the SurfaceCache discards a surface belonging to this image.
   */
  virtual void OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey) = 0;

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) = 0;
  virtual uint64_t InnerWindowID() const = 0;

  virtual bool HasError() = 0;
  virtual void SetHasError() = 0;

  virtual ImageURL* GetURI() = 0;

  virtual void ReportUseCounters() { }
};

class ImageResource : public Image
{
public:
  already_AddRefed<ProgressTracker> GetProgressTracker() override
  {
    RefPtr<ProgressTracker> progressTracker = mProgressTracker;
    MOZ_ASSERT(progressTracker);
    return progressTracker.forget();
  }

  void SetProgressTracker(
                       ProgressTracker* aProgressTracker) override final
  {
    MOZ_ASSERT(aProgressTracker);
    MOZ_ASSERT(!mProgressTracker);
    mProgressTracker = aProgressTracker;
  }

  virtual void IncrementAnimationConsumers() override;
  virtual void DecrementAnimationConsumers() override;
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() override
  {
    return mAnimationConsumers;
  }
#endif

  virtual void OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey) override { }

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) override
  {
    mInnerWindowId = aInnerWindowId;
  }
  virtual uint64_t InnerWindowID() const override { return mInnerWindowId; }

  virtual bool HasError() override    { return mError; }
  virtual void SetHasError() override { mError = true; }

  /*
   * Returns a non-AddRefed pointer to the URI associated with this image.
   * Illegal to use off-main-thread.
   */
  virtual ImageURL* GetURI() override { return mURI.get(); }

protected:
  explicit ImageResource(ImageURL* aURI);
  ~ImageResource();

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
  RefPtr<ProgressTracker>     mProgressTracker;
  RefPtr<ImageURL>            mURI;
  TimeStamp                     mLastRefreshTime;
  uint64_t                      mInnerWindowId;
  uint32_t                      mAnimationConsumers;
  uint16_t                      mAnimationMode; // Enum values in imgIContainer
  bool                          mInitialized:1; // Have we been initalized?
  bool                          mAnimating:1;   // Are we currently animating?
  bool                          mError:1;       // Error handling

  virtual Tuple<DrawResult, gfx::IntSize, RefPtr<gfx::SourceSurface>>
    GetFrameInternal(const gfx::IntSize& aSize,
                     const Maybe<SVGImageContext>& aSVGContext,
                     uint32_t aWhichFrame,
                     uint32_t aFlags)
  {
    return MakeTuple(DrawResult::BAD_IMAGE, aSize,
                     RefPtr<gfx::SourceSurface>());
  }

  /**
   * Calculate the estimated size to use for an image container with the given
   * parameters. It may not be the same as the given size, and it may not be
   * the same as the size of the surface in the image container, but it is the
   * best effort estimate.
   */
  virtual gfx::IntSize GetImageContainerSize(layers::LayerManager* aManager,
                                             const gfx::IntSize& aSize,
                                             uint32_t aFlags)
  {
    return gfx::IntSize(0, 0);
  }

  already_AddRefed<layers::ImageContainer>
    GetImageContainerImpl(layers::LayerManager* aManager,
                          const gfx::IntSize& aSize,
                          const Maybe<SVGImageContext>& aSVGContext,
                          uint32_t aFlags);

  void UpdateImageContainer();

  void ReleaseImageContainer();

private:
  void SetCurrentImage(layers::ImageContainer* aContainer,
                       gfx::SourceSurface* aSurface,
                       bool aInTransaction);

  struct ImageContainerEntry {
    ImageContainerEntry(const gfx::IntSize& aSize,
                        const Maybe<SVGImageContext>& aSVGContext,
                        layers::ImageContainer* aContainer,
                        uint32_t aFlags)
      : mSize(aSize)
      , mSVGContext(aSVGContext)
      , mContainer(aContainer)
      , mLastDrawResult(DrawResult::NOT_READY)
      , mFlags(aFlags)
    { }

    gfx::IntSize                        mSize;
    Maybe<SVGImageContext>              mSVGContext;
    // A weak pointer to our ImageContainer, which stays alive only as long as
    // the layer system needs it.
    WeakPtr<layers::ImageContainer>     mContainer;
    // If mContainer is non-null, this contains the DrawResult we obtained
    // the last time we updated it.
    DrawResult                          mLastDrawResult;
    // Cached flags to use for decoding. FLAG_ASYNC_NOTIFY should always be set
    // but FLAG_HIGH_QUALITY_SCALING may vary.
    uint32_t                            mFlags;
  };

  AutoTArray<ImageContainerEntry, 1> mImageContainers;
  layers::ImageContainer::ProducerID mImageProducerID;
  layers::ImageContainer::FrameID mLastFrameID;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_Image_h
