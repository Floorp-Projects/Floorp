/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Must #include ImageLogging.h before any IPDL-generated files or other files
// that #include prlog.h
#include "ImageLogging.h"

#include "RasterImage.h"

#include "base/histogram.h"
#include "gfxPlatform.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "Decoder.h"
#include "nsAutoPtr.h"
#include "prenv.h"
#include "prsystem.h"
#include "ImageContainer.h"
#include "ImageRegion.h"
#include "Layers.h"
#include "LookupResult.h"
#include "nsIConsoleService.h"
#include "nsIInputStream.h"
#include "nsIScriptError.h"
#include "nsISupportsPrimitives.h"
#include "nsPresContext.h"
#include "SourceBuffer.h"
#include "SurfaceCache.h"
#include "FrameAnimator.h"

#include "gfxContext.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Move.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Services.h"
#include <stdint.h>
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/gfx/Scale.h"

#include "GeckoProfiler.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h"
#include <algorithm>

namespace mozilla {

using namespace gfx;
using namespace layers;

namespace image {

using std::ceil;
using std::min;

// The maximum number of times any one RasterImage was decoded.  This is only
// used for statistics.
static int32_t sMaxDecodeCount = 0;

#ifndef DEBUG
NS_IMPL_ISUPPORTS(RasterImage, imgIContainer, nsIProperties)
#else
NS_IMPL_ISUPPORTS(RasterImage, imgIContainer, nsIProperties,
                  imgIContainerDebug)
#endif

//******************************************************************************
RasterImage::RasterImage(ImageURL* aURI /* = nullptr */) :
  ImageResource(aURI), // invoke superclass's constructor
  mSize(0,0),
  mLockCount(0),
  mDecodeCount(0),
  mRequestedSampleSize(0),
  mLastImageContainerDrawResult(DrawResult::NOT_READY),
#ifdef DEBUG
  mFramesNotified(0),
#endif
  mSourceBuffer(new SourceBuffer()),
  mFrameCount(0),
  mHasSize(false),
  mTransient(false),
  mSyncLoad(false),
  mDiscardable(false),
  mHasSourceData(false),
  mHasBeenDecoded(false),
  mDownscaleDuringDecode(false),
  mPendingAnimation(false),
  mAnimationFinished(false),
  mWantFullDecode(false)
{
  Telemetry::GetHistogramById(Telemetry::IMAGE_DECODE_COUNT)->Add(0);
}

//******************************************************************************
RasterImage::~RasterImage()
{
  // Make sure our SourceBuffer is marked as complete. This will ensure that any
  // outstanding decoders terminate.
  if (!mSourceBuffer->IsComplete()) {
    mSourceBuffer->Complete(NS_ERROR_ABORT);
  }

  // Release all frames from the surface cache.
  SurfaceCache::RemoveImage(ImageKey(this));
}

nsresult
RasterImage::Init(const char* aMimeType,
                  uint32_t aFlags)
{
  // We don't support re-initialization
  if (mInitialized) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Not sure an error can happen before init, but be safe
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  // We want to avoid redecodes for transient images.
  MOZ_ASSERT(!(aFlags & INIT_FLAG_TRANSIENT) ||
               (!(aFlags & INIT_FLAG_DISCARDABLE) &&
                !(aFlags & INIT_FLAG_DOWNSCALE_DURING_DECODE)),
             "Illegal init flags for transient image");

  // Store initialization data
  mDiscardable = !!(aFlags & INIT_FLAG_DISCARDABLE);
  mWantFullDecode = !!(aFlags & INIT_FLAG_DECODE_IMMEDIATELY);
  mTransient = !!(aFlags & INIT_FLAG_TRANSIENT);
  mDownscaleDuringDecode = !!(aFlags & INIT_FLAG_DOWNSCALE_DURING_DECODE);
  mSyncLoad = !!(aFlags & INIT_FLAG_SYNC_LOAD);

#ifndef MOZ_ENABLE_SKIA
  // Downscale-during-decode requires Skia.
  mDownscaleDuringDecode = false;
#endif

  // Use the MIME type to select a decoder type, and make sure there *is* a
  // decoder for this MIME type.
  NS_ENSURE_ARG_POINTER(aMimeType);
  mDecoderType = DecoderFactory::GetDecoderType(aMimeType);
  if (mDecoderType == DecoderType::UNKNOWN) {
    return NS_ERROR_FAILURE;
  }

  // Lock this image's surfaces in the SurfaceCache if we're not discardable.
  if (!mDiscardable) {
    mLockCount++;
    SurfaceCache::LockImage(ImageKey(this));
  }

  if (!mSyncLoad) {
    // Create an async metadata decoder and verify we succeed in doing so.
    nsresult rv = DecodeMetadata(DECODE_FLAGS_DEFAULT);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Mark us as initialized
  mInitialized = true;

  return NS_OK;
}

//******************************************************************************
// [notxpcom] void requestRefresh ([const] in TimeStamp aTime);
NS_IMETHODIMP_(void)
RasterImage::RequestRefresh(const TimeStamp& aTime)
{
  if (HadRecentRefresh(aTime)) {
    return;
  }

  EvaluateAnimation();

  if (!mAnimating) {
    return;
  }

  FrameAnimator::RefreshResult res;
  if (mAnim) {
    res = mAnim->RequestRefresh(aTime);
  }

  if (res.frameAdvanced) {
    // Notify listeners that our frame has actually changed, but do this only
    // once for all frames that we've now passed (if AdvanceFrame() was called
    // more than once).
    #ifdef DEBUG
      mFramesNotified++;
    #endif

    NotifyProgress(NoProgress, res.dirtyRect);
  }

  if (res.animationFinished) {
    mAnimationFinished = true;
    EvaluateAnimation();
  }
}

//******************************************************************************
NS_IMETHODIMP
RasterImage::GetWidth(int32_t* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);

  if (mError) {
    *aWidth = 0;
    return NS_ERROR_FAILURE;
  }

  *aWidth = mSize.width;
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
RasterImage::GetHeight(int32_t* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);

  if (mError) {
    *aHeight = 0;
    return NS_ERROR_FAILURE;
  }

  *aHeight = mSize.height;
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
RasterImage::GetIntrinsicSize(nsSize* aSize)
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  *aSize = nsSize(nsPresContext::CSSPixelsToAppUnits(mSize.width),
                  nsPresContext::CSSPixelsToAppUnits(mSize.height));
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
RasterImage::GetIntrinsicRatio(nsSize* aRatio)
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  *aRatio = nsSize(mSize.width, mSize.height);
  return NS_OK;
}

NS_IMETHODIMP_(Orientation)
RasterImage::GetOrientation()
{
  return mOrientation;
}

//******************************************************************************
NS_IMETHODIMP
RasterImage::GetType(uint16_t* aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = imgIContainer::TYPE_RASTER;
  return NS_OK;
}

LookupResult
RasterImage::LookupFrameInternal(uint32_t aFrameNum,
                                 const IntSize& aSize,
                                 uint32_t aFlags)
{
  if (!mAnim) {
    NS_ASSERTION(aFrameNum == 0,
                 "Don't ask for a frame > 0 if we're not animated!");
    aFrameNum = 0;
  }

  if (mAnim && aFrameNum > 0) {
    MOZ_ASSERT(ToSurfaceFlags(aFlags) == DefaultSurfaceFlags(),
               "Can't composite frames with non-default surface flags");
    return mAnim->GetCompositedFrame(aFrameNum);
  }

  Maybe<SurfaceFlags> alternateFlags;
  if (IsOpaque()) {
    // If we're opaque, we can always substitute a frame that was decoded with a
    // different decode flag for premultiplied alpha, because that can only
    // matter for frames with transparency.
    alternateFlags.emplace(ToSurfaceFlags(aFlags) ^
                             SurfaceFlags::NO_PREMULTIPLY_ALPHA);
  }

  // We don't want any substitution for sync decodes (except the premultiplied
  // alpha optimization above), so we use SurfaceCache::Lookup in this case.
  if (aFlags & FLAG_SYNC_DECODE) {
    return SurfaceCache::Lookup(ImageKey(this),
                                RasterSurfaceKey(aSize,
                                                 ToSurfaceFlags(aFlags),
                                                 aFrameNum),
                                alternateFlags);
  }

  // We'll return the best match we can find to the requested frame.
  return SurfaceCache::LookupBestMatch(ImageKey(this),
                                       RasterSurfaceKey(aSize,
                                                        ToSurfaceFlags(aFlags),
                                                        aFrameNum),
                                       alternateFlags);
}

DrawableFrameRef
RasterImage::LookupFrame(uint32_t aFrameNum,
                         const IntSize& aSize,
                         uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());

  IntSize requestedSize = CanDownscaleDuringDecode(aSize, aFlags)
                        ? aSize : mSize;

  LookupResult result = LookupFrameInternal(aFrameNum, requestedSize, aFlags);

  if (!result && !mHasSize) {
    // We can't request a decode without knowing our intrinsic size. Give up.
    return DrawableFrameRef();
  }

  if (result.Type() == MatchType::NOT_FOUND ||
      result.Type() == MatchType::SUBSTITUTE_BECAUSE_NOT_FOUND ||
      ((aFlags & FLAG_SYNC_DECODE) && !result)) {
    // We don't have a copy of this frame, and there's no decoder working on
    // one. (Or we're sync decoding and the existing decoder hasn't even started
    // yet.) Trigger decoding so it'll be available next time.
    MOZ_ASSERT(!mAnim || GetNumFrames() < 1, "Animated frames should be locked");

    Decode(requestedSize, aFlags);

    // If we can sync decode, we should already have the frame.
    if (aFlags & FLAG_SYNC_DECODE) {
      result = LookupFrameInternal(aFrameNum, requestedSize, aFlags);
    }
  }

  if (!result) {
    // We still weren't able to get a frame. Give up.
    return DrawableFrameRef();
  }

  if (result.DrawableRef()->GetCompositingFailed()) {
    return DrawableFrameRef();
  }

  MOZ_ASSERT(!result.DrawableRef()->GetIsPaletted(),
             "Should not have a paletted frame");

  // Sync decoding guarantees that we got the frame, but if it's owned by an
  // async decoder that's currently running, the contents of the frame may not
  // be available yet. Make sure we get everything.
  if (mHasSourceData && (aFlags & FLAG_SYNC_DECODE)) {
    result.DrawableRef()->WaitUntilComplete();
  }

  return Move(result.DrawableRef());
}

uint32_t
RasterImage::GetCurrentFrameIndex() const
{
  if (mAnim) {
    return mAnim->GetCurrentAnimationFrameIndex();
  }

  return 0;
}

uint32_t
RasterImage::GetRequestedFrameIndex(uint32_t aWhichFrame) const
{
  return aWhichFrame == FRAME_FIRST ? 0 : GetCurrentFrameIndex();
}

IntRect
RasterImage::GetFirstFrameRect()
{
  if (mAnim && mHasBeenDecoded) {
    return mAnim->GetFirstFrameRefreshArea();
  }

  // Fall back to our size. This is implicitly zero-size if !mHasSize.
  return IntRect(IntPoint(0,0), mSize);
}

NS_IMETHODIMP_(bool)
RasterImage::IsOpaque()
{
  if (mError) {
    return false;
  }

  Progress progress = mProgressTracker->GetProgress();

  // If we haven't yet finished decoding, the safe answer is "not opaque".
  if (!(progress & FLAG_DECODE_COMPLETE)) {
    return false;
  }

  // Other, we're opaque if FLAG_HAS_TRANSPARENCY is not set.
  return !(progress & FLAG_HAS_TRANSPARENCY);
}

void
RasterImage::OnSurfaceDiscarded()
{
  MOZ_ASSERT(mProgressTracker);

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(mProgressTracker, &ProgressTracker::OnDiscard);
  NS_DispatchToMainThread(runnable);
}

//******************************************************************************
NS_IMETHODIMP
RasterImage::GetAnimated(bool* aAnimated)
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_ARG_POINTER(aAnimated);

  // If we have mAnim, we can know for sure
  if (mAnim) {
    *aAnimated = true;
    return NS_OK;
  }

  // Otherwise, we need to have been decoded to know for sure, since if we were
  // decoded at least once mAnim would have been created for animated images.
  // This is true even though we check for animation during the metadata decode,
  // because we may still discover animation only during the full decode for
  // corrupt images.
  if (!mHasBeenDecoded) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We know for sure
  *aAnimated = false;

  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP_(int32_t)
RasterImage::GetFirstFrameDelay()
{
  if (mError) {
    return -1;
  }

  bool animated = false;
  if (NS_FAILED(GetAnimated(&animated)) || !animated) {
    return -1;
  }

  MOZ_ASSERT(mAnim, "Animated images should have a FrameAnimator");
  return mAnim->GetTimeoutForFrame(0);
}

already_AddRefed<SourceSurface>
RasterImage::CopyFrame(uint32_t aWhichFrame, uint32_t aFlags)
{
  if (aWhichFrame > FRAME_MAX_VALUE) {
    return nullptr;
  }

  if (mError) {
    return nullptr;
  }

  // Get the frame. If it's not there, it's probably the caller's fault for
  // not waiting for the data to be loaded from the network or not passing
  // FLAG_SYNC_DECODE
  DrawableFrameRef frameRef =
    LookupFrame(GetRequestedFrameIndex(aWhichFrame), mSize, aFlags);
  if (!frameRef) {
    // The OS threw this frame away and we couldn't redecode it right now.
    return nullptr;
  }

  // Create a 32-bit image surface of our size, but draw using the frame's
  // rect, implicitly padding the frame out to the image's size.

  IntSize size(mSize.width, mSize.height);
  RefPtr<DataSourceSurface> surf =
    Factory::CreateDataSourceSurface(size,
                                     SurfaceFormat::B8G8R8A8,
                                     /* aZero = */ true);
  if (NS_WARN_IF(!surf)) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface mapping;
  if (!surf->Map(DataSourceSurface::MapType::WRITE, &mapping)) {
    gfxCriticalError() << "RasterImage::CopyFrame failed to map surface";
    return nullptr;
  }

  RefPtr<DrawTarget> target =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     mapping.mData,
                                     size,
                                     mapping.mStride,
                                     SurfaceFormat::B8G8R8A8);
  if (!target) {
    gfxWarning() << "RasterImage::CopyFrame failed in CreateDrawTargetForData";
    return nullptr;
  }

  IntRect intFrameRect = frameRef->GetRect();
  Rect rect(intFrameRect.x, intFrameRect.y,
            intFrameRect.width, intFrameRect.height);
  if (frameRef->IsSinglePixel()) {
    target->FillRect(rect, ColorPattern(frameRef->SinglePixelColor()),
                     DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  } else {
    RefPtr<SourceSurface> srcSurf = frameRef->GetSurface();
    if (!srcSurf) {
      RecoverFromInvalidFrames(mSize, aFlags);
      return nullptr;
    }

    Rect srcRect(0, 0, intFrameRect.width, intFrameRect.height);
    target->DrawSurface(srcSurf, srcRect, rect);
  }

  target->Flush();
  surf->Unmap();

  return surf.forget();
}

//******************************************************************************
/* [noscript] SourceSurface getFrame(in uint32_t aWhichFrame,
 *                                   in uint32_t aFlags); */
NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
RasterImage::GetFrame(uint32_t aWhichFrame,
                      uint32_t aFlags)
{
  return GetFrameInternal(mSize, aWhichFrame, aFlags).second().forget();
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
RasterImage::GetFrameAtSize(const IntSize& aSize,
                            uint32_t aWhichFrame,
                            uint32_t aFlags)
{
  return GetFrameInternal(aSize, aWhichFrame, aFlags).second().forget();
}

Pair<DrawResult, RefPtr<SourceSurface>>
RasterImage::GetFrameInternal(const IntSize& aSize,
                              uint32_t aWhichFrame,
                              uint32_t aFlags)
{
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE);

  if (aSize.IsEmpty()) {
    return MakePair(DrawResult::BAD_ARGS, RefPtr<SourceSurface>());
  }

  if (aWhichFrame > FRAME_MAX_VALUE) {
    return MakePair(DrawResult::BAD_ARGS, RefPtr<SourceSurface>());
  }

  if (mError) {
    return MakePair(DrawResult::BAD_IMAGE, RefPtr<SourceSurface>());
  }

  // Get the frame. If it's not there, it's probably the caller's fault for
  // not waiting for the data to be loaded from the network or not passing
  // FLAG_SYNC_DECODE
  DrawableFrameRef frameRef =
    LookupFrame(GetRequestedFrameIndex(aWhichFrame), aSize, aFlags);
  if (!frameRef) {
    // The OS threw this frame away and we couldn't redecode it.
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }

  // If this frame covers the entire image, we can just reuse its existing
  // surface.
  RefPtr<SourceSurface> frameSurf;
  if (!frameRef->NeedsPadding() &&
      frameRef->GetSize() == aSize) {
    frameSurf = frameRef->GetSurface();
  }

  // The image doesn't have a usable surface because it's been optimized away or
  // because it's a partial update frame from an animation. Create one. (In this
  // case we fall back to returning a surface at our intrinsic size, even if a
  // different size was originally specified.)
  if (!frameSurf) {
    frameSurf = CopyFrame(aWhichFrame, aFlags);
  }

  if (!frameRef->IsImageComplete()) {
    return MakePair(DrawResult::INCOMPLETE, Move(frameSurf));
  }

  return MakePair(DrawResult::SUCCESS, Move(frameSurf));
}

Pair<DrawResult, nsRefPtr<layers::Image>>
RasterImage::GetCurrentImage(ImageContainer* aContainer, uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContainer);

  DrawResult drawResult;
  RefPtr<SourceSurface> surface;
  Tie(drawResult, surface) =
    GetFrameInternal(mSize, FRAME_CURRENT, aFlags | FLAG_ASYNC_NOTIFY);
  if (!surface) {
    // The OS threw out some or all of our buffer. We'll need to wait for the
    // redecode (which was automatically triggered by GetFrame) to complete.
    return MakePair(drawResult, nsRefPtr<layers::Image>());
  }

  CairoImage::Data cairoData;
  GetWidth(&cairoData.mSize.width);
  GetHeight(&cairoData.mSize.height);
  cairoData.mSourceSurface = surface;

  nsRefPtr<layers::Image> image =
    aContainer->CreateImage(ImageFormat::CAIRO_SURFACE);
  MOZ_ASSERT(image);

  static_cast<CairoImage*>(image.get())->SetData(cairoData);

  return MakePair(drawResult, Move(image));
}

NS_IMETHODIMP_(bool)
RasterImage::IsImageContainerAvailable(LayerManager* aManager, uint32_t aFlags)
{
  int32_t maxTextureSize = aManager->GetMaxTextureSize();
  if (!mHasSize ||
      mSize.width > maxTextureSize ||
      mSize.height > maxTextureSize) {
    return false;
  }

  return true;
}

NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
RasterImage::GetImageContainer(LayerManager* aManager, uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT((aFlags & ~(FLAG_SYNC_DECODE |
                         FLAG_SYNC_DECODE_IF_FAST |
                         FLAG_ASYNC_NOTIFY))
               == FLAG_NONE,
             "Unsupported flag passed to GetImageContainer");

  int32_t maxTextureSize = aManager->GetMaxTextureSize();
  if (!mHasSize ||
      mSize.width > maxTextureSize ||
      mSize.height > maxTextureSize) {
    return nullptr;
  }

  if (IsUnlocked() && mProgressTracker) {
    mProgressTracker->OnUnlockedDraw();
  }

  nsRefPtr<layers::ImageContainer> container = mImageContainer.get();

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
  nsRefPtr<layers::Image> image;
  Tie(drawResult, image) = GetCurrentImage(container, aFlags);
  if (!image) {
    return nullptr;
  }

  // |image| holds a reference to a SourceSurface which in turn holds a lock on
  // the current frame's VolatileBuffer, ensuring that it doesn't get freed as
  // long as the layer system keeps this ImageContainer alive.
  container->SetCurrentImageInTransaction(image);

  mLastImageContainerDrawResult = drawResult;
  mImageContainer = container;

  return container.forget();
}

void
RasterImage::UpdateImageContainer()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<layers::ImageContainer> container = mImageContainer.get();
  if (!container) {
    return;
  }

  DrawResult drawResult;
  nsRefPtr<layers::Image> image;
  Tie(drawResult, image) = GetCurrentImage(container, FLAG_NONE);
  if (!image) {
    return;
  }

  mLastImageContainerDrawResult = drawResult;
  nsAutoTArray<ImageContainer::NonOwningImage, 1> imageList;
  imageList.AppendElement(ImageContainer::NonOwningImage(image));
  container->SetCurrentImages(imageList);
}

size_t
RasterImage::SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const
{
  return mSourceBuffer->SizeOfIncludingThisWithComputedFallback(aMallocSizeOf);
}

void
RasterImage::CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                   MallocSizeOf aMallocSizeOf) const
{
  SurfaceCache::CollectSizeOfSurfaces(ImageKey(this), aCounters, aMallocSizeOf);
  if (mAnim) {
    mAnim->CollectSizeOfCompositingSurfaces(aCounters, aMallocSizeOf);
  }
}

class OnAddedFrameRunnable : public nsRunnable
{
public:
  OnAddedFrameRunnable(RasterImage* aImage,
                       uint32_t aNewFrameCount,
                       const IntRect& aNewRefreshArea)
    : mImage(aImage)
    , mNewFrameCount(aNewFrameCount)
    , mNewRefreshArea(aNewRefreshArea)
  {
    MOZ_ASSERT(aImage);
  }

  NS_IMETHOD Run()
  {
    mImage->OnAddedFrame(mNewFrameCount, mNewRefreshArea);
    return NS_OK;
  }

private:
  nsRefPtr<RasterImage> mImage;
  uint32_t mNewFrameCount;
  IntRect mNewRefreshArea;
};

void
RasterImage::OnAddedFrame(uint32_t aNewFrameCount,
                          const IntRect& aNewRefreshArea)
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      new OnAddedFrameRunnable(this, aNewFrameCount, aNewRefreshArea);
    NS_DispatchToMainThread(runnable);
    return;
  }

  MOZ_ASSERT(aNewFrameCount <= mFrameCount + 1, "Skipped a frame?");

  if (mError) {
    return;  // We're in an error state, possibly due to OOM. Bail.
  }

  if (aNewFrameCount > mFrameCount) {
    mFrameCount = aNewFrameCount;

    if (aNewFrameCount == 2) {
      MOZ_ASSERT(mAnim, "Should already have animation state");

      // We may be able to start animating.
      if (mPendingAnimation && ShouldAnimate()) {
        StartAnimation();
      }
    }
    if (aNewFrameCount > 1) {
      mAnim->UnionFirstFrameRefreshArea(aNewRefreshArea);
    }
  }
}

bool
RasterImage::SetMetadata(const ImageMetadata& aMetadata,
                         bool aFromMetadataDecode)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError) {
    return true;
  }

  if (aMetadata.HasSize()) {
    IntSize size = aMetadata.GetSize();
    if (size.width < 0 || size.height < 0) {
      NS_WARNING("Image has negative intrinsic size");
      DoError();
      return true;
    }

    MOZ_ASSERT(aMetadata.HasOrientation());
    Orientation orientation = aMetadata.GetOrientation();

    // If we already have a size, check the new size against the old one.
    if (mHasSize && (size != mSize || orientation != mOrientation)) {
      NS_WARNING("Image changed size or orientation on redecode! "
                 "This should not happen!");
      DoError();
      return true;
    }

    // Set the size and flag that we have it.
    mSize = size;
    mOrientation = orientation;
    mHasSize = true;
  }

  if (mHasSize && aMetadata.HasAnimation() && !mAnim) {
    // We're becoming animated, so initialize animation stuff.
    mAnim = MakeUnique<FrameAnimator>(this, mSize, mAnimationMode);

    // We don't support discarding animated images (See bug 414259).
    // Lock the image and throw away the key.
    LockImage();

    if (!aFromMetadataDecode) {
      // The metadata decode reported that this image isn't animated, but we
      // discovered that it actually was during the full decode. This is a
      // rare failure that only occurs for corrupt images. To recover, we need
      // to discard all existing surfaces and redecode.
      return false;
    }
  }

  if (mAnim) {
    mAnim->SetLoopCount(aMetadata.GetLoopCount());
    mAnim->SetFirstFrameTimeout(aMetadata.GetFirstFrameTimeout());
  }

  if (aMetadata.HasHotspot()) {
    IntPoint hotspot = aMetadata.GetHotspot();

    nsCOMPtr<nsISupportsPRUint32> intwrapx =
      do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID);
    nsCOMPtr<nsISupportsPRUint32> intwrapy =
      do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID);
    intwrapx->SetData(hotspot.x);
    intwrapy->SetData(hotspot.y);

    Set("hotspotX", intwrapx);
    Set("hotspotY", intwrapy);
  }

  return true;
}

NS_IMETHODIMP
RasterImage::SetAnimationMode(uint16_t aAnimationMode)
{
  if (mAnim) {
    mAnim->SetAnimationMode(aAnimationMode);
  }
  return SetAnimationModeInternal(aAnimationMode);
}

//******************************************************************************

nsresult
RasterImage::StartAnimation()
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(ShouldAnimate(), "Should not animate!");

  // If we're not ready to animate, then set mPendingAnimation, which will cause
  // us to start animating if and when we do become ready.
  mPendingAnimation = !mAnim || GetNumFrames() < 2;
  if (mPendingAnimation) {
    return NS_OK;
  }

  // A timeout of -1 means we should display this frame forever.
  if (mAnim->GetTimeoutForFrame(GetCurrentFrameIndex()) < 0) {
    mAnimationFinished = true;
    return NS_ERROR_ABORT;
  }

  // We need to set the time that this initial frame was first displayed, as
  // this is used in AdvanceFrame().
  mAnim->InitAnimationFrameTimeIfNecessary();

  return NS_OK;
}

//******************************************************************************
nsresult
RasterImage::StopAnimation()
{
  MOZ_ASSERT(mAnimating, "Should be animating!");

  nsresult rv = NS_OK;
  if (mError) {
    rv = NS_ERROR_FAILURE;
  } else {
    mAnim->SetAnimationFrameTime(TimeStamp());
  }

  mAnimating = false;
  return rv;
}

//******************************************************************************
NS_IMETHODIMP
RasterImage::ResetAnimation()
{
  if (mError) {
    return NS_ERROR_FAILURE;
    }

  mPendingAnimation = false;

  if (mAnimationMode == kDontAnimMode || !mAnim ||
      mAnim->GetCurrentAnimationFrameIndex() == 0) {
    return NS_OK;
  }

  mAnimationFinished = false;

  if (mAnimating) {
    StopAnimation();
  }

  MOZ_ASSERT(mAnim, "Should have a FrameAnimator");
  mAnim->ResetAnimation();

  NotifyProgress(NoProgress, mAnim->GetFirstFrameRefreshArea());

  // Start the animation again. It may not have been running before, if
  // mAnimationFinished was true before entering this function.
  EvaluateAnimation();

  return NS_OK;
}

//******************************************************************************
// [notxpcom] void setAnimationStartTime ([const] in TimeStamp aTime);
NS_IMETHODIMP_(void)
RasterImage::SetAnimationStartTime(const TimeStamp& aTime)
{
  if (mError || mAnimationMode == kDontAnimMode || mAnimating || !mAnim) {
    return;
  }

  mAnim->SetAnimationFrameTime(aTime);
}

NS_IMETHODIMP_(float)
RasterImage::GetFrameIndex(uint32_t aWhichFrame)
{
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE, "Invalid argument");
  return (aWhichFrame == FRAME_FIRST || !mAnim)
         ? 0.0f
         : mAnim->GetCurrentAnimationFrameIndex();
}

NS_IMETHODIMP_(IntRect)
RasterImage::GetImageSpaceInvalidationRect(const IntRect& aRect)
{
  return aRect;
}

nsresult
RasterImage::OnImageDataComplete(nsIRequest*, nsISupports*, nsresult aStatus,
                                 bool aLastPart)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Record that we have all the data we're going to get now.
  mHasSourceData = true;

  // Let decoders know that there won't be any more data coming.
  mSourceBuffer->Complete(aStatus);

  // Allow a synchronous metadata decode if mSyncLoad was set, or if we're
  // running on a single thread (in which case waiting for the async metadata
  // decoder could delay this image's load event quite a bit), or if this image
  // is transient.
  bool canSyncDecodeMetadata = mSyncLoad || mTransient ||
                               DecodePool::NumberOfCores() < 2;

  if (canSyncDecodeMetadata && !mHasSize) {
    // We're loading this image synchronously, so it needs to be usable after
    // this call returns.  Since we haven't gotten our size yet, we need to do a
    // synchronous metadata decode here.
    DecodeMetadata(FLAG_SYNC_DECODE);
  }

  // Determine our final status, giving precedence to Necko failure codes. We
  // check after running the metadata decode in case it triggered an error.
  nsresult finalStatus = mError ? NS_ERROR_FAILURE : NS_OK;
  if (NS_FAILED(aStatus)) {
    finalStatus = aStatus;
  }

  // If loading failed, report an error.
  if (NS_FAILED(finalStatus)) {
    DoError();
  }

  Progress loadProgress = LoadCompleteProgress(aLastPart, mError, finalStatus);

  if (!mHasSize && !mError) {
    // We don't have our size yet, so we'll fire the load event in SetSize().
    MOZ_ASSERT(!canSyncDecodeMetadata,
               "Firing load async after metadata sync decode?");
    NotifyProgress(FLAG_ONLOAD_BLOCKED);
    mLoadProgress = Some(loadProgress);
    return finalStatus;
  }

  NotifyForLoadEvent(loadProgress);

  return finalStatus;
}

void
RasterImage::NotifyForLoadEvent(Progress aProgress)
{
  MOZ_ASSERT(mHasSize || mError, "Need to know size before firing load event");
  MOZ_ASSERT(!mHasSize ||
             (mProgressTracker->GetProgress() & FLAG_SIZE_AVAILABLE),
             "Should have notified that the size is available if we have it");

  // If we encountered an error, make sure we notify for that as well.
  if (mError) {
    aProgress |= FLAG_HAS_ERROR;
  }

  // Notify our listeners, which will fire this image's load event.
  NotifyProgress(aProgress);
}

nsresult
RasterImage::OnImageDataAvailable(nsIRequest*,
                                  nsISupports*,
                                  nsIInputStream* aInputStream,
                                  uint64_t,
                                  uint32_t aCount)
{
  nsresult rv = mSourceBuffer->AppendFromInputStream(aInputStream, aCount);
  MOZ_ASSERT(rv == NS_OK || rv == NS_ERROR_OUT_OF_MEMORY);

  if (MOZ_UNLIKELY(rv == NS_ERROR_OUT_OF_MEMORY)) {
    DoError();
  }

  return rv;
}

nsresult
RasterImage::SetSourceSizeHint(uint32_t aSizeHint)
{
  return mSourceBuffer->ExpectLength(aSizeHint);
}

/********* Methods to implement lazy allocation of nsIProperties object *******/
NS_IMETHODIMP
RasterImage::Get(const char* prop, const nsIID& iid, void** result)
{
  if (!mProperties) {
    return NS_ERROR_FAILURE;
  }
  return mProperties->Get(prop, iid, result);
}

NS_IMETHODIMP
RasterImage::Set(const char* prop, nsISupports* value)
{
  if (!mProperties) {
    mProperties = do_CreateInstance("@mozilla.org/properties;1");
  }
  if (!mProperties) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return mProperties->Set(prop, value);
}

NS_IMETHODIMP
RasterImage::Has(const char* prop, bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (!mProperties) {
    *_retval = false;
    return NS_OK;
  }
  return mProperties->Has(prop, _retval);
}

NS_IMETHODIMP
RasterImage::Undefine(const char* prop)
{
  if (!mProperties) {
    return NS_ERROR_FAILURE;
  }
  return mProperties->Undefine(prop);
}

NS_IMETHODIMP
RasterImage::GetKeys(uint32_t* count, char*** keys)
{
  if (!mProperties) {
    *count = 0;
    *keys = nullptr;
    return NS_OK;
  }
  return mProperties->GetKeys(count, keys);
}

void
RasterImage::Discard()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(CanDiscard(), "Asked to discard but can't");
  MOZ_ASSERT(!mAnim, "Asked to discard for animated image");

  // Delete all the decoded frames.
  SurfaceCache::RemoveImage(ImageKey(this));

  // Notify that we discarded.
  if (mProgressTracker) {
    mProgressTracker->OnDiscard();
  }
}

bool
RasterImage::CanDiscard() {
  return mHasSourceData &&       // ...have the source data...
         !mAnim;                 // Can never discard animated images
}

//******************************************************************************

NS_IMETHODIMP
RasterImage::RequestDecode()
{
  return RequestDecodeForSize(mSize, DECODE_FLAGS_DEFAULT);
}


NS_IMETHODIMP
RasterImage::StartDecoding()
{
  return RequestDecodeForSize(mSize, FLAG_SYNC_DECODE_IF_FAST);
}

NS_IMETHODIMP
RasterImage::RequestDecodeForSize(const IntSize& aSize, uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError) {
    return NS_ERROR_FAILURE;
  }

  if (!mHasSize) {
    mWantFullDecode = true;
    return NS_OK;
  }

  // Fall back to our intrinsic size if we don't support
  // downscale-during-decode.
  IntSize targetSize = mDownscaleDuringDecode ? aSize : mSize;

  // Decide whether to sync decode images we can decode quickly. Here we are
  // explicitly trading off flashing for responsiveness in the case that we're
  // redecoding an image (see bug 845147).
  bool shouldSyncDecodeIfFast =
    !mHasBeenDecoded && (aFlags & FLAG_SYNC_DECODE_IF_FAST);

  uint32_t flags = shouldSyncDecodeIfFast
                 ? aFlags
                 : aFlags & ~FLAG_SYNC_DECODE_IF_FAST;

  // Look up the first frame of the image, which will implicitly start decoding
  // if it's not available right now.
  LookupFrame(0, targetSize, flags);

  return NS_OK;
}

static void
LaunchDecoder(Decoder* aDecoder,
              RasterImage* aImage,
              uint32_t aFlags,
              bool aHaveSourceData)
{
  if (aHaveSourceData) {
    // If we have all the data, we can sync decode if requested.
    if (aFlags & imgIContainer::FLAG_SYNC_DECODE) {
      PROFILER_LABEL_PRINTF("DecodePool", "SyncDecodeIfPossible",
        js::ProfileEntry::Category::GRAPHICS,
        "%s", aImage->GetURIString().get());
      DecodePool::Singleton()->SyncDecodeIfPossible(aDecoder);
      return;
    }

    if (aFlags & imgIContainer::FLAG_SYNC_DECODE_IF_FAST) {
      PROFILER_LABEL_PRINTF("DecodePool", "SyncDecodeIfSmall",
        js::ProfileEntry::Category::GRAPHICS,
        "%s", aImage->GetURIString().get());
      DecodePool::Singleton()->SyncDecodeIfSmall(aDecoder);
      return;
    }
  }

  // Perform an async decode. We also take this path if we don't have all the
  // source data yet, since sync decoding is impossible in that situation.
  DecodePool::Singleton()->AsyncDecode(aDecoder);
}

NS_IMETHODIMP
RasterImage::Decode(const IntSize& aSize, uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError) {
    return NS_ERROR_FAILURE;
  }

  // If we don't have a size yet, we can't do any other decoding.
  if (!mHasSize) {
    mWantFullDecode = true;
    return NS_OK;
  }

  if (mDownscaleDuringDecode) {
    // We're about to decode again, which may mean that some of the previous
    // sizes we've decoded at aren't useful anymore. We can allow them to
    // expire from the cache by unlocking them here. When the decode finishes,
    // it will send an invalidation that will cause all instances of this image
    // to redraw. If this image is locked, any surfaces that are still useful
    // will become locked again when LookupFrame touches them, and the remainder
    // will eventually expire.
    SurfaceCache::UnlockSurfaces(ImageKey(this));
  }

  MOZ_ASSERT(mDownscaleDuringDecode || aSize == mSize,
             "Can only decode to our intrinsic size if we're not allowed to "
             "downscale-during-decode");

  Maybe<IntSize> targetSize = mSize != aSize ? Some(aSize) : Nothing();

  // Determine which flags we need to decode this image with.
  DecoderFlags decoderFlags = DefaultDecoderFlags();
  if (aFlags & FLAG_ASYNC_NOTIFY) {
    decoderFlags |= DecoderFlags::ASYNC_NOTIFY;
  }
  if (mTransient) {
    decoderFlags |= DecoderFlags::IMAGE_IS_TRANSIENT;
  }
  if (mHasBeenDecoded) {
    decoderFlags |= DecoderFlags::IS_REDECODE;
  }

  // Create a decoder.
  nsRefPtr<Decoder> decoder;
  if (mAnim) {
    decoder = DecoderFactory::CreateAnimationDecoder(mDecoderType, this,
                                                     mSourceBuffer, decoderFlags,
                                                     ToSurfaceFlags(aFlags),
                                                     mRequestedResolution);
  } else {
    decoder = DecoderFactory::CreateDecoder(mDecoderType, this, mSourceBuffer,
                                            targetSize, decoderFlags,
                                            ToSurfaceFlags(aFlags),
                                            mRequestedSampleSize,
                                            mRequestedResolution);
  }

  // Make sure DecoderFactory was able to create a decoder successfully.
  if (!decoder) {
    return NS_ERROR_FAILURE;
  }

  // Add a placeholder for the first frame to the SurfaceCache so we won't
  // trigger any more decoders with the same parameters.
  InsertOutcome outcome =
    SurfaceCache::InsertPlaceholder(ImageKey(this),
                                    RasterSurfaceKey(aSize,
                                                     decoder->GetSurfaceFlags(),
                                                     /* aFrameNum = */ 0));
  if (outcome != InsertOutcome::SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  // Report telemetry.
  Telemetry::GetHistogramById(Telemetry::IMAGE_DECODE_COUNT)
    ->Subtract(mDecodeCount);
  mDecodeCount++;
  Telemetry::GetHistogramById(Telemetry::IMAGE_DECODE_COUNT)
    ->Add(mDecodeCount);

  if (mDecodeCount > sMaxDecodeCount) {
    // Don't subtract out 0 from the histogram, because that causes its count
    // to go negative, which is not kosher.
    if (sMaxDecodeCount > 0) {
      Telemetry::GetHistogramById(Telemetry::IMAGE_MAX_DECODE_COUNT)
        ->Subtract(sMaxDecodeCount);
    }
    sMaxDecodeCount = mDecodeCount;
    Telemetry::GetHistogramById(Telemetry::IMAGE_MAX_DECODE_COUNT)
      ->Add(sMaxDecodeCount);
  }

  // We're ready to decode; start the decoder.
  LaunchDecoder(decoder, this, aFlags, mHasSourceData);
  return NS_OK;
}

NS_IMETHODIMP
RasterImage::DecodeMetadata(uint32_t aFlags)
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!mHasSize, "Should not do unnecessary metadata decodes");

  // Create a decoder.
  nsRefPtr<Decoder> decoder =
    DecoderFactory::CreateMetadataDecoder(mDecoderType, this, mSourceBuffer,
                                          mRequestedSampleSize,
                                          mRequestedResolution);

  // Make sure DecoderFactory was able to create a decoder successfully.
  if (!decoder) {
    return NS_ERROR_FAILURE;
  }

  // We're ready to decode; start the decoder.
  LaunchDecoder(decoder, this, aFlags, mHasSourceData);
  return NS_OK;
}

void
RasterImage::RecoverFromInvalidFrames(const IntSize& aSize, uint32_t aFlags)
{
  if (!mHasSize) {
    return;
  }

  NS_WARNING("A RasterImage's frames became invalid. Attempting to recover...");

  // Discard all existing frames, since they're probably all now invalid.
  SurfaceCache::RemoveImage(ImageKey(this));

  // Relock the image if it's supposed to be locked.
  if (mLockCount > 0) {
    SurfaceCache::LockImage(ImageKey(this));
  }

  // Animated images require some special handling, because we normally require
  // that they never be discarded.
  if (mAnim) {
    Decode(mSize, aFlags | FLAG_SYNC_DECODE);
    ResetAnimation();
    return;
  }

  // For non-animated images, it's fine to recover using an async decode.
  Decode(aSize, aFlags);
}

bool
RasterImage::CanDownscaleDuringDecode(const IntSize& aSize, uint32_t aFlags)
{
  // Check basic requirements: downscale-during-decode is enabled for this
  // image, we have all the source data and know our size, the flags allow us to
  // do it, and a 'good' filter is being used.
  if (!mDownscaleDuringDecode || !mHasSize ||
      !gfxPrefs::ImageDownscaleDuringDecodeEnabled() ||
      !(aFlags & imgIContainer::FLAG_HIGH_QUALITY_SCALING)) {
    return false;
  }

  // We don't downscale animated images during decode.
  if (mAnim) {
    return false;
  }

  // Never upscale.
  if (aSize.width >= mSize.width || aSize.height >= mSize.height) {
    return false;
  }

  // Zero or negative width or height is unacceptable.
  if (aSize.width < 1 || aSize.height < 1) {
    return false;
  }

  // There's no point in scaling if we can't store the result.
  if (!SurfaceCache::CanHold(aSize)) {
    return false;
  }

  return true;
}

DrawResult
RasterImage::DrawInternal(DrawableFrameRef&& aFrameRef,
                          gfxContext* aContext,
                          const IntSize& aSize,
                          const ImageRegion& aRegion,
                          GraphicsFilter aFilter,
                          uint32_t aFlags)
{
  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  ImageRegion region(aRegion);
  bool frameIsComplete = aFrameRef->IsImageComplete();

  // By now we may have a frame with the requested size. If not, we need to
  // adjust the drawing parameters accordingly.
  IntSize finalSize = aFrameRef->GetImageSize();
  bool couldRedecodeForBetterFrame = false;
  if (finalSize != aSize) {
    gfx::Size scale(double(aSize.width) / finalSize.width,
                    double(aSize.height) / finalSize.height);
    aContext->Multiply(gfxMatrix::Scaling(scale.width, scale.height));
    region.Scale(1.0 / scale.width, 1.0 / scale.height);

    couldRedecodeForBetterFrame = mDownscaleDuringDecode &&
                                  CanDownscaleDuringDecode(aSize, aFlags);
  }

  if (!aFrameRef->Draw(aContext, region, aFilter, aFlags)) {
    RecoverFromInvalidFrames(aSize, aFlags);
    return DrawResult::TEMPORARY_ERROR;
  }
  if (!frameIsComplete) {
    return DrawResult::INCOMPLETE;
  }
  if (couldRedecodeForBetterFrame) {
    return DrawResult::WRONG_SIZE;
  }
  return DrawResult::SUCCESS;
}

//******************************************************************************
/* [noscript] void draw(in gfxContext aContext,
 *                      in gfxGraphicsFilter aFilter,
 *                      [const] in gfxMatrix aUserSpaceToImageSpace,
 *                      [const] in gfxRect aFill,
 *                      [const] in IntRect aSubimage,
 *                      [const] in IntSize aViewportSize,
 *                      [const] in SVGImageContext aSVGContext,
 *                      in uint32_t aWhichFrame,
 *                      in uint32_t aFlags); */
NS_IMETHODIMP_(DrawResult)
RasterImage::Draw(gfxContext* aContext,
                  const IntSize& aSize,
                  const ImageRegion& aRegion,
                  uint32_t aWhichFrame,
                  GraphicsFilter aFilter,
                  const Maybe<SVGImageContext>& /*aSVGContext - ignored*/,
                  uint32_t aFlags)
{
  if (aWhichFrame > FRAME_MAX_VALUE) {
    return DrawResult::BAD_ARGS;
  }

  if (mError) {
    return DrawResult::BAD_IMAGE;
  }

  // Illegal -- you can't draw with non-default decode flags.
  // (Disabling colorspace conversion might make sense to allow, but
  // we don't currently.)
  if (ToSurfaceFlags(aFlags) != DefaultSurfaceFlags()) {
    return DrawResult::BAD_ARGS;
  }

  if (!aContext) {
    return DrawResult::BAD_ARGS;
  }

  if (IsUnlocked() && mProgressTracker) {
    mProgressTracker->OnUnlockedDraw();
  }

  // If we're not using GraphicsFilter::FILTER_GOOD, we shouldn't high-quality
  // scale or downscale during decode.
  uint32_t flags = aFilter == GraphicsFilter::FILTER_GOOD
                 ? aFlags
                 : aFlags & ~FLAG_HIGH_QUALITY_SCALING;

  DrawableFrameRef ref =
    LookupFrame(GetRequestedFrameIndex(aWhichFrame), aSize, flags);
  if (!ref) {
    // Getting the frame (above) touches the image and kicks off decoding.
    if (mDrawStartTime.IsNull()) {
      mDrawStartTime = TimeStamp::Now();
    }
    return DrawResult::NOT_READY;
  }

  bool shouldRecordTelemetry = !mDrawStartTime.IsNull() &&
                               ref->IsImageComplete();

  auto result = DrawInternal(Move(ref), aContext, aSize,
                             aRegion, aFilter, flags);

  if (shouldRecordTelemetry) {
      TimeDuration drawLatency = TimeStamp::Now() - mDrawStartTime;
      Telemetry::Accumulate(Telemetry::IMAGE_DECODE_ON_DRAW_LATENCY,
                            int32_t(drawLatency.ToMicroseconds()));
      mDrawStartTime = TimeStamp();
  }

  return result;
}

//******************************************************************************

NS_IMETHODIMP
RasterImage::LockImage()
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Main thread to encourage serialization with UnlockImage");
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  // Increment the lock count
  mLockCount++;

  // Lock this image's surfaces in the SurfaceCache.
  if (mLockCount == 1) {
    SurfaceCache::LockImage(ImageKey(this));
  }

  return NS_OK;
}

//******************************************************************************

NS_IMETHODIMP
RasterImage::UnlockImage()
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Main thread to encourage serialization with LockImage");
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  // It's an error to call this function if the lock count is 0
  MOZ_ASSERT(mLockCount > 0,
             "Calling UnlockImage with mLockCount == 0!");
  if (mLockCount == 0) {
    return NS_ERROR_ABORT;
  }

  // Decrement our lock count
  mLockCount--;

  // Unlock this image's surfaces in the SurfaceCache.
  if (mLockCount == 0 ) {
    SurfaceCache::UnlockImage(ImageKey(this));
  }

  return NS_OK;
}

//******************************************************************************

NS_IMETHODIMP
RasterImage::RequestDiscard()
{
  if (mDiscardable &&      // Enabled at creation time...
      mLockCount == 0 &&   // ...not temporarily disabled...
      CanDiscard()) {
    Discard();
  }

  return NS_OK;
}

// Indempotent error flagging routine. If a decoder is open, shuts it down.
void
RasterImage::DoError()
{
  // If we've flagged an error before, we have nothing to do
  if (mError) {
    return;
  }

  // We can't safely handle errors off-main-thread, so dispatch a worker to
  // do it.
  if (!NS_IsMainThread()) {
    HandleErrorWorker::DispatchIfNeeded(this);
    return;
  }

  // Put the container in an error state.
  mError = true;

  // Stop animation and release our FrameAnimator.
  if (mAnimating) {
    StopAnimation();
  }
  mAnim.release();

  // Release all locks.
  mLockCount = 0;
  SurfaceCache::UnlockImage(ImageKey(this));

  // Release all frames from the surface cache.
  SurfaceCache::RemoveImage(ImageKey(this));

  // Invalidate to get rid of any partially-drawn image content.
  NotifyProgress(NoProgress, IntRect(0, 0, mSize.width, mSize.height));

  MOZ_LOG(GetImgLog(), LogLevel::Error,
          ("RasterImage: [this=%p] Error detected for image\n", this));
}

/* static */ void
RasterImage::HandleErrorWorker::DispatchIfNeeded(RasterImage* aImage)
{
  nsRefPtr<HandleErrorWorker> worker = new HandleErrorWorker(aImage);
  NS_DispatchToMainThread(worker);
}

RasterImage::HandleErrorWorker::HandleErrorWorker(RasterImage* aImage)
  : mImage(aImage)
{
  MOZ_ASSERT(mImage, "Should have image");
}

NS_IMETHODIMP
RasterImage::HandleErrorWorker::Run()
{
  mImage->DoError();

  return NS_OK;
}

bool
RasterImage::ShouldAnimate()
{
  return ImageResource::ShouldAnimate() && GetNumFrames() >= 2 &&
         !mAnimationFinished;
}

#ifdef DEBUG
NS_IMETHODIMP
RasterImage::GetFramesNotified(uint32_t* aFramesNotified)
{
  NS_ENSURE_ARG_POINTER(aFramesNotified);

  *aFramesNotified = mFramesNotified;

  return NS_OK;
}
#endif

void
RasterImage::NotifyProgress(Progress aProgress,
                            const IntRect& aInvalidRect /* = IntRect() */,
                            SurfaceFlags aSurfaceFlags
                              /* = DefaultSurfaceFlags() */)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that we stay alive long enough to finish notifying.
  nsRefPtr<RasterImage> image(this);

  bool wasDefaultFlags = aSurfaceFlags == DefaultSurfaceFlags();

  if (!aInvalidRect.IsEmpty() && wasDefaultFlags) {
    // Update our image container since we're invalidating.
    UpdateImageContainer();
  }

  // Tell the observers what happened.
  image->mProgressTracker->SyncNotifyProgress(aProgress, aInvalidRect);
}

void
RasterImage::FinalizeDecoder(Decoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDecoder);
  MOZ_ASSERT(aDecoder->HasError() || !aDecoder->InFrame(),
             "Finalizing a decoder in the middle of a frame");

  bool wasMetadata = aDecoder->IsMetadataDecode();
  bool done = aDecoder->GetDecodeDone();

  // If the decoder detected an error, log it to the error console.
  if (aDecoder->ShouldReportError() && !aDecoder->WasAborted()) {
    ReportDecoderError(aDecoder);
  }

  // Record all the metadata the decoder gathered about this image.
  bool metadataOK = SetMetadata(aDecoder->GetImageMetadata(), wasMetadata);
  if (!metadataOK) {
    // This indicates a serious error that requires us to discard all existing
    // surfaces and redecode to recover. We'll drop the results from this
    // decoder on the floor, since they aren't valid.
    aDecoder->TakeProgress();
    aDecoder->TakeInvalidRect();
    RecoverFromInvalidFrames(mSize,
                             FromSurfaceFlags(aDecoder->GetSurfaceFlags()));
    return;
  }

  MOZ_ASSERT(mError || mHasSize || !aDecoder->HasSize(),
             "SetMetadata should've gotten a size");

  if (!wasMetadata && aDecoder->GetDecodeDone() && !aDecoder->WasAborted()) {
    // Flag that we've been decoded before.
    mHasBeenDecoded = true;
    if (mAnim) {
      mAnim->SetDoneDecoding(true);
    }
  }

  // Send out any final notifications.
  NotifyProgress(aDecoder->TakeProgress(),
                 aDecoder->TakeInvalidRect(),
                 aDecoder->GetSurfaceFlags());

  if (!wasMetadata && aDecoder->ChunkCount()) {
    Telemetry::Accumulate(Telemetry::IMAGE_DECODE_CHUNKS,
                          aDecoder->ChunkCount());
  }

  if (done) {
    // Do some telemetry if this isn't a metadata decode.
    if (!wasMetadata) {
      Telemetry::Accumulate(Telemetry::IMAGE_DECODE_TIME,
                            int32_t(aDecoder->DecodeTime().ToMicroseconds()));

      // We record the speed for only some decoders. The rest have
      // SpeedHistogram return HistogramCount.
      Telemetry::ID id = aDecoder->SpeedHistogram();
      if (id < Telemetry::HistogramCount) {
        int32_t KBps = int32_t(aDecoder->BytesDecoded() /
                               (1024 * aDecoder->DecodeTime().ToSeconds()));
        Telemetry::Accumulate(id, KBps);
      }
    }

    // Detect errors.
    if (aDecoder->HasError() && !aDecoder->WasAborted()) {
      DoError();
    } else if (wasMetadata && !mHasSize) {
      DoError();
    }

    // If we were waiting to fire the load event, go ahead and fire it now.
    if (mLoadProgress && wasMetadata) {
      NotifyForLoadEvent(*mLoadProgress);
      mLoadProgress = Nothing();
      NotifyProgress(FLAG_ONLOAD_UNBLOCKED);
    }
  }

  // If we were a metadata decode and a full decode was requested, do it.
  if (done && wasMetadata && mWantFullDecode) {
    mWantFullDecode = false;
    RequestDecode();
  }
}

void
RasterImage::ReportDecoderError(Decoder* aDecoder)
{
  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);

  if (consoleService && errorObject && !aDecoder->HasDecoderError()) {
    nsAutoString msg(NS_LITERAL_STRING("Image corrupt or truncated."));
    nsAutoString src;
    if (GetURI()) {
      nsCString uri;
      if (GetURI()->GetSpecTruncatedTo1k(uri) == ImageURL::TruncatedTo1k) {
        msg += NS_LITERAL_STRING(" URI in this note truncated due to length.");
      }
      src = NS_ConvertUTF8toUTF16(uri);
    }
    if (NS_SUCCEEDED(errorObject->InitWithWindowID(
                       msg,
                       src,
                       EmptyString(), 0, 0, nsIScriptError::errorFlag,
                       "Image", InnerWindowID()
                     ))) {
      consoleService->LogMessage(errorObject);
    }
  }
}

already_AddRefed<imgIContainer>
RasterImage::Unwrap()
{
  nsCOMPtr<imgIContainer> self(this);
  return self.forget();
}

void
RasterImage::PropagateUseCounters(nsIDocument*)
{
  // No use counters.
}

IntSize
RasterImage::OptimalImageSizeForDest(const gfxSize& aDest, uint32_t aWhichFrame,
                                     GraphicsFilter aFilter, uint32_t aFlags)
{
  MOZ_ASSERT(aDest.width >= 0 || ceil(aDest.width) <= INT32_MAX ||
             aDest.height >= 0 || ceil(aDest.height) <= INT32_MAX,
             "Unexpected destination size");

  if (mSize.IsEmpty() || aDest.IsEmpty()) {
    return IntSize(0, 0);
  }

  IntSize destSize(ceil(aDest.width), ceil(aDest.height));

  if (aFilter == GraphicsFilter::FILTER_GOOD &&
      CanDownscaleDuringDecode(destSize, aFlags)) {
    return destSize;
  }

  // We can't scale to this size. Use our intrinsic size for now.
  return mSize;
}

} // namespace image
} // namespace mozilla
