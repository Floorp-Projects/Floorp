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
#include "nsPresContext.h"
#include "SourceBuffer.h"
#include "SurfaceCache.h"
#include "FrameAnimator.h"

#include "nsPNGDecoder.h"
#include "nsGIFDecoder2.h"
#include "nsJPEGDecoder.h"
#include "nsBMPDecoder.h"
#include "nsICODecoder.h"
#include "nsIconDecoder.h"

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

class ScaleRunner : public nsRunnable
{
  enum ScaleState
  {
    eNew,
    eReady,
    eFinish,
    eFinishWithError
  };

public:
  ScaleRunner(RasterImage* aImage,
              uint32_t aImageFlags,
              const IntSize& aSize,
              RawAccessFrameRef&& aSrcRef)
    : mImage(aImage)
    , mSrcRef(Move(aSrcRef))
    , mDstSize(aSize)
    , mImageFlags(aImageFlags)
    , mState(eNew)
  {
    MOZ_ASSERT(!mSrcRef->GetIsPaletted());
    MOZ_ASSERT(aSize.width > 0 && aSize.height > 0);
  }

  bool Init()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mState == eNew, "Calling Init() twice?");

    // We'll need a destination frame. It's unconditionally ARGB32 because
    // that's what the scaler outputs.
    nsRefPtr<imgFrame> tentativeDstFrame = new imgFrame();
    nsresult rv =
      tentativeDstFrame->InitForDecoder(mDstSize, SurfaceFormat::B8G8R8A8);
    if (NS_FAILED(rv)) {
      return false;
    }

    // We need a strong reference to the raw data for the destination frame.
    // (We already got one for the source frame in the constructor.)
    RawAccessFrameRef tentativeDstRef = tentativeDstFrame->RawAccessRef();
    if (!tentativeDstRef) {
      return false;
    }

    // Everything worked, so commit to these objects and mark ourselves ready.
    mDstRef = Move(tentativeDstRef);
    mState = eReady;

    // Insert the new surface into the cache immediately. We need to do this so
    // that we won't start multiple scaling jobs for the same size.
    SurfaceCache::Insert(mDstRef.get(), ImageKey(mImage.get()),
                         RasterSurfaceKey(mDstSize, mImageFlags, 0),
                         Lifetime::Transient);

    return true;
  }

  NS_IMETHOD Run() override
  {
    if (mState == eReady) {
      // Collect information from the frames that we need to scale.
      ScalingData srcData = mSrcRef->GetScalingData();
      ScalingData dstData = mDstRef->GetScalingData();

      // Actually do the scaling.
      bool succeeded =
        gfx::Scale(srcData.mRawData, srcData.mSize.width, srcData.mSize.height,
                   srcData.mBytesPerRow, dstData.mRawData, mDstSize.width,
                   mDstSize.height, dstData.mBytesPerRow, srcData.mFormat);

      if (succeeded) {
        // Mark the frame as complete and discardable.
        mDstRef->ImageUpdated(mDstRef->GetRect());
        MOZ_ASSERT(mDstRef->IsImageComplete(),
                   "Incomplete, but just updated the entire frame");
      }

      // We need to send notifications and release our references on the main
      // thread, so finish up there.
      mState = succeeded ? eFinish : eFinishWithError;
      NS_DispatchToMainThread(this);
    } else if (mState == eFinish) {
      MOZ_ASSERT(NS_IsMainThread());
      MOZ_ASSERT(mDstRef, "Should have a valid scaled frame");

      // Notify, so observers can redraw.
      nsRefPtr<RasterImage> image = mImage.get();
      if (image) {
        image->NotifyNewScaledFrame();
      }

      // We're done, so release everything.
      mSrcRef.reset();
      mDstRef.reset();
    } else if (mState == eFinishWithError) {
      MOZ_ASSERT(NS_IsMainThread());
      NS_WARNING("HQ scaling failed");

      // Remove the frame from the cache since we know we don't need it.
      SurfaceCache::RemoveSurface(ImageKey(mImage.get()),
                                  RasterSurfaceKey(mDstSize,
                                                   mImageFlags, 0));

      // Release everything we're holding, too.
      mSrcRef.reset();
      mDstRef.reset();
    } else {
      // mState must be eNew, which is invalid in Run().
      MOZ_ASSERT(false, "Need to call Init() before dispatching");
    }

    return NS_OK;
  }

private:
  virtual ~ScaleRunner()
  {
    MOZ_ASSERT(!mSrcRef && !mDstRef,
               "Should have released strong refs in Run()");
  }

  WeakPtr<RasterImage> mImage;
  RawAccessFrameRef    mSrcRef;
  RawAccessFrameRef    mDstRef;
  const IntSize      mDstSize;
  uint32_t             mImageFlags;
  ScaleState           mState;
};

static nsCOMPtr<nsIThread> sScaleWorkerThread = nullptr;

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

  NS_ENSURE_ARG_POINTER(aMimeType);

  // We want to avoid redecodes for transient images.
  MOZ_ASSERT(!(aFlags & INIT_FLAG_TRANSIENT) ||
               (!(aFlags & INIT_FLAG_DISCARDABLE) &&
                !(aFlags & INIT_FLAG_DOWNSCALE_DURING_DECODE)),
             "Illegal init flags for transient image");

  // Store initialization data
  mSourceDataMimeType.Assign(aMimeType);
  mDiscardable = !!(aFlags & INIT_FLAG_DISCARDABLE);
  mWantFullDecode = !!(aFlags & INIT_FLAG_DECODE_IMMEDIATELY);
  mTransient = !!(aFlags & INIT_FLAG_TRANSIENT);
  mDownscaleDuringDecode = !!(aFlags & INIT_FLAG_DOWNSCALE_DURING_DECODE);
  mSyncLoad = !!(aFlags & INIT_FLAG_SYNC_LOAD);

#ifndef MOZ_ENABLE_SKIA
  // Downscale-during-decode requires Skia.
  mDownscaleDuringDecode = false;
#endif

  // Lock this image's surfaces in the SurfaceCache if we're not discardable.
  if (!mDiscardable) {
    mLockCount++;
    SurfaceCache::LockImage(ImageKey(this));
  }

  if (mSyncLoad) {
    // We'll create a sync size decoder in OnImageDataComplete. For now, just
    // verify that we *could* create such a decoder.
    eDecoderType type = GetDecoderType(mSourceDataMimeType.get());
    if (type == eDecoderType_unknown) {
      return NS_ERROR_FAILURE;
    }
  } else {
    // Create an async size decoder and verify that we succeed in doing so.
    nsresult rv = Decode(Nothing(), DECODE_FLAGS_DEFAULT);
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
/* readonly attribute int32_t width; */
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
/* readonly attribute int32_t height; */
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
/* [noscript] readonly attribute nsSize intrinsicSize; */
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
/* [noscript] readonly attribute nsSize intrinsicRatio; */
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
/* unsigned short GetType(); */
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
    MOZ_ASSERT(DecodeFlags(aFlags) == DECODE_FLAGS_DEFAULT,
               "Can't composite frames with non-default decode flags");
    return mAnim->GetCompositedFrame(aFrameNum);
  }

  Maybe<uint32_t> alternateFlags;
  if (IsOpaque()) {
    // If we're opaque, we can always substitute a frame that was decoded with a
    // different decode flag for premultiplied alpha, because that can only
    // matter for frames with transparency.
    alternateFlags = Some(aFlags ^ FLAG_DECODE_NO_PREMULTIPLY_ALPHA);
  }

  // We don't want any substitution for sync decodes (except the premultiplied
  // alpha optimization above), so we use SurfaceCache::Lookup in this case.
  if (aFlags & FLAG_SYNC_DECODE) {
    return SurfaceCache::Lookup(ImageKey(this),
                                RasterSurfaceKey(aSize,
                                                 DecodeFlags(aFlags),
                                                 aFrameNum),
                                alternateFlags);
  }

  // We'll return the best match we can find to the requested frame.
  return SurfaceCache::LookupBestMatch(ImageKey(this),
                                       RasterSurfaceKey(aSize,
                                                        DecodeFlags(aFlags),
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
    MOZ_ASSERT(!mAnim, "Animated frames should be locked");

    Decode(Some(requestedSize), aFlags);

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
  if (mAnim) {
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
/* readonly attribute boolean animated; */
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
  // decoded at least once mAnim would have been created for animated images
  if (!mHasBeenDecoded) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We know for sure
  *aAnimated = false;

  return NS_OK;
}

//******************************************************************************
/* [notxpcom] int32_t getFirstFrameDelay (); */
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
      RecoverFromLossOfFrames(mSize, aFlags);
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
  return GetFrameInternal(aWhichFrame, aFlags).second().forget();
}

Pair<DrawResult, RefPtr<SourceSurface>>
RasterImage::GetFrameInternal(uint32_t aWhichFrame, uint32_t aFlags)
{
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE);

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
    LookupFrame(GetRequestedFrameIndex(aWhichFrame), mSize, aFlags);
  if (!frameRef) {
    // The OS threw this frame away and we couldn't redecode it.
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }

  // If this frame covers the entire image, we can just reuse its existing
  // surface.
  RefPtr<SourceSurface> frameSurf;
  IntRect frameRect = frameRef->GetRect();
  if (frameRect.x == 0 && frameRect.y == 0 &&
      frameRect.width == mSize.width &&
      frameRect.height == mSize.height) {
    frameSurf = frameRef->GetSurface();
  }

  // The image doesn't have a usable surface because it's been optimized away or
  // because it's a partial update frame from an animation. Create one.
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

  auto result = GetFrameInternal(FRAME_CURRENT, aFlags | FLAG_ASYNC_NOTIFY);
  if (!result.second()) {
    // The OS threw out some or all of our buffer. We'll need to wait for the
    // redecode (which was automatically triggered by GetFrame) to complete.
    return MakePair(result.first(), nsRefPtr<layers::Image>());
  }

  CairoImage::Data cairoData;
  GetWidth(&cairoData.mSize.width);
  GetHeight(&cairoData.mSize.height);
  cairoData.mSourceSurface = result.second();

  nsRefPtr<layers::Image> image =
    aContainer->CreateImage(ImageFormat::CAIRO_SURFACE);
  MOZ_ASSERT(image);

  static_cast<CairoImage*>(image.get())->SetData(cairoData);

  return MakePair(result.first(), Move(image));
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

  auto result = GetCurrentImage(container, aFlags);
  if (!result.second()) {
    // We couldn't get an Image.
    return nullptr;
  }

  // |result.second()| holds a reference to a SourceSurface which in turn holds
  // a lock on the current frame's VolatileBuffer, ensuring that it doesn't get
  // freed as long as the layer system keeps this ImageContainer alive.
  container->SetCurrentImageInTransaction(result.second());

  mLastImageContainerDrawResult = result.first();
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

  auto result = GetCurrentImage(container, FLAG_NONE);
  if (!result.second()) {
    // We couldn't get an Image.
    return;
  }

  mLastImageContainerDrawResult = result.first();
  nsAutoTArray<ImageContainer::NonOwningImage,1> imageList;
  imageList.AppendElement(
      ImageContainer::NonOwningImage(result.second()));
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
      // We're becoming animated, so initialize animation stuff.
      MOZ_ASSERT(!mAnim, "Already have animation state?");
      mAnim = MakeUnique<FrameAnimator>(this, mSize, mAnimationMode);

      // We don't support discarding animated images (See bug 414259).
      // Lock the image and throw away the key.
      //
      // Note that this is inefficient, since we could get rid of the source
      // data too. However, doing this is actually hard, because we're probably
      // mid-decode, and thus we're decoding out of the source buffer. Since
      // we're going to fix this anyway later, and since we didn't kill the
      // source data in the old world either, locking is acceptable for the
      // moment.
      LockImage();

      if (mPendingAnimation && ShouldAnimate()) {
        StartAnimation();
      }
    }
    if (aNewFrameCount > 1) {
      mAnim->UnionFirstFrameRefreshArea(aNewRefreshArea);
    }
  }
}

nsresult
RasterImage::SetSize(int32_t aWidth, int32_t aHeight, Orientation aOrientation)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError) {
    return NS_ERROR_FAILURE;
  }

  // Ensure that we have positive values
  // XXX - Why isn't the size unsigned? Should this be changed?
  if ((aWidth < 0) || (aHeight < 0)) {
    return NS_ERROR_INVALID_ARG;
  }

  // if we already have a size, check the new size against the old one
  if (mHasSize &&
      ((aWidth != mSize.width) ||
       (aHeight != mSize.height) ||
       (aOrientation != mOrientation))) {
    NS_WARNING("Image changed size on redecode! This should not happen!");
    DoError();
    return NS_ERROR_UNEXPECTED;
  }

  // Set the size and flag that we have it
  mSize.SizeTo(aWidth, aHeight);
  mOrientation = aOrientation;
  mHasSize = true;

  return NS_OK;
}

void
RasterImage::OnDecodingComplete(bool aIsAnimated)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError) {
    return;
  }

  // Flag that we've been decoded before.
  mHasBeenDecoded = true;

  if (aIsAnimated) {
    if (mAnim) {
      mAnim->SetDoneDecoding(true);
    } else {
      // The OnAddedFrame event that will create mAnim is still in the event
      // queue. Wait for it.
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethod(this, &RasterImage::MarkAnimationDecoded);
      NS_DispatchToMainThread(runnable);
    }
  }
}

void
RasterImage::MarkAnimationDecoded()
{
  MOZ_ASSERT(mAnim, "Should have an animation now");
  if (!mAnim) {
    return;
  }

  mAnim->SetDoneDecoding(true);
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
/* void StartAnimation () */
nsresult
RasterImage::StartAnimation()
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(ShouldAnimate(), "Should not animate!");

  // If we don't have mAnim yet, then we're not ready to animate.  Setting
  // mPendingAnimation will cause us to start animating as soon as we have a
  // second frame, which causes mAnim to be constructed.
  mPendingAnimation = !mAnim;
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
/* void stopAnimation (); */
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
/* void resetAnimation (); */
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

void
RasterImage::SetLoopCount(int32_t aLoopCount)
{
  if (mError) {
    return;
  }

  // No need to set this if we're not an animation.
  if (mAnim) {
    mAnim->SetLoopCount(aLoopCount);
  }
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

  // Allow a synchronous size decode if mSyncLoad was set, or if we're running
  // on a single thread (in which case waiting for the async size decoder could
  // delay this image's load event quite a bit), or if this image is transient.
  bool canSyncSizeDecode = mSyncLoad || mTransient ||
                           DecodePool::NumberOfCores() < 2;

  if (canSyncSizeDecode && !mHasSize) {
    // We're loading this image synchronously, so it needs to be usable after
    // this call returns.  Since we haven't gotten our size yet, we need to do a
    // synchronous size decode here.
    Decode(Nothing(), FLAG_SYNC_DECODE);
  }

  // Determine our final status, giving precedence to Necko failure codes. We
  // check after running the size decode above in case it triggered an error.
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
    MOZ_ASSERT(!canSyncSizeDecode, "Firing load async but canSyncSizeDecode?");
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
                                  nsIInputStream* aInStr,
                                  uint64_t aOffset,
                                  uint32_t aCount)
{
  nsresult rv;

  // WriteToSourceBuffer always consumes everything it gets if it doesn't run
  // out of memory.
  uint32_t bytesRead;
  rv = aInStr->ReadSegments(WriteToSourceBuffer, this, aCount, &bytesRead);

  MOZ_ASSERT(bytesRead == aCount || HasError() || NS_FAILED(rv),
    "WriteToSourceBuffer should consume everything if ReadSegments succeeds or "
    "the image must be in error!");

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

// Sets up a decoder for this image.
already_AddRefed<Decoder>
RasterImage::CreateDecoder(const Maybe<IntSize>& aSize, uint32_t aFlags)
{
  // Make sure we actually get size before doing a full decode.
  if (aSize) {
    MOZ_ASSERT(mHasSize, "Must do a size decode before a full decode!");
    MOZ_ASSERT(mDownscaleDuringDecode || *aSize == mSize,
               "Can only decode to our intrinsic size if we're not allowed to "
               "downscale-during-decode");
  } else {
    MOZ_ASSERT(!mHasSize, "Should not do unnecessary size decodes");
  }

  // Figure out which decoder we want.
  eDecoderType type = GetDecoderType(mSourceDataMimeType.get());
  if (type == eDecoderType_unknown) {
    return nullptr;
  }

  // Instantiate the appropriate decoder.
  nsRefPtr<Decoder> decoder;
  switch (type) {
    case eDecoderType_png:
      decoder = new nsPNGDecoder(this);
      break;
    case eDecoderType_gif:
      decoder = new nsGIFDecoder2(this);
      break;
    case eDecoderType_jpeg:
      // If we have all the data we don't want to waste cpu time doing
      // a progressive decode.
      decoder = new nsJPEGDecoder(this,
                                  mHasBeenDecoded ? Decoder::SEQUENTIAL :
                                                    Decoder::PROGRESSIVE);
      break;
    case eDecoderType_bmp:
      decoder = new nsBMPDecoder(this);
      break;
    case eDecoderType_ico:
      decoder = new nsICODecoder(this);
      break;
    case eDecoderType_icon:
      decoder = new nsIconDecoder(this);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown decoder type");
  }

  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetSizeDecode(!aSize);
  decoder->SetSendPartialInvalidations(!mHasBeenDecoded);
  decoder->SetImageIsTransient(mTransient);
  decoder->SetFlags(aFlags);

  if (!mHasBeenDecoded && aSize) {
    // Lock the image while we're decoding, so that it doesn't get evicted from
    // the SurfaceCache before we have a chance to realize that it's animated.
    // The corresponding unlock happens in FinalizeDecoder.
    LockImage();
    decoder->SetImageIsLocked();
  }

  decoder->SetIterator(mSourceBuffer->Iterator());

  // Set a target size for downscale-during-decode if applicable.
  if (mDownscaleDuringDecode && aSize && *aSize != mSize) {
    DebugOnly<nsresult> rv = decoder->SetTargetSize(*aSize);
    MOZ_ASSERT(nsresult(rv) != NS_ERROR_NOT_AVAILABLE,
               "We're downscale-during-decode but decoder doesn't support it?");
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Bad downscale-during-decode target size?");
  }

  decoder->Init();

  if (NS_FAILED(decoder->GetDecoderError())) {
    return nullptr;
  }

  if (aSize) {
    // Add a placeholder for the first frame to the SurfaceCache so we won't
    // trigger any more decoders with the same parameters.
    InsertOutcome outcome =
      SurfaceCache::InsertPlaceholder(ImageKey(this),
                                      RasterSurfaceKey(*aSize,
                                                       decoder->GetDecodeFlags(),
                                                       /* aFrameNum = */ 0));
    if (outcome != InsertOutcome::SUCCESS) {
      return nullptr;
    }
  }

  if (!aSize) {
    Telemetry::GetHistogramById(
      Telemetry::IMAGE_DECODE_COUNT)->Subtract(mDecodeCount);
    mDecodeCount++;
    Telemetry::GetHistogramById(
      Telemetry::IMAGE_DECODE_COUNT)->Add(mDecodeCount);

    if (mDecodeCount > sMaxDecodeCount) {
      // Don't subtract out 0 from the histogram, because that causes its count
      // to go negative, which is not kosher.
      if (sMaxDecodeCount > 0) {
        Telemetry::GetHistogramById(
          Telemetry::IMAGE_MAX_DECODE_COUNT)->Subtract(sMaxDecodeCount);
      }
      sMaxDecodeCount = mDecodeCount;
      Telemetry::GetHistogramById(
        Telemetry::IMAGE_MAX_DECODE_COUNT)->Add(sMaxDecodeCount);
    }
  }

  return decoder.forget();
}

//******************************************************************************
/* void requestDecode() */
NS_IMETHODIMP
RasterImage::RequestDecode()
{
  return RequestDecodeForSize(mSize, DECODE_FLAGS_DEFAULT);
}

/* void startDecode() */
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

NS_IMETHODIMP
RasterImage::Decode(const Maybe<IntSize>& aSize, uint32_t aFlags)
{
  MOZ_ASSERT(!aSize || NS_IsMainThread());

  if (mError) {
    return NS_ERROR_FAILURE;
  }

  // If we don't have a size yet, we can't do any other decoding.
  if (!mHasSize && aSize) {
    mWantFullDecode = true;
    return NS_OK;
  }

  if (mDownscaleDuringDecode && aSize) {
    // We're about to decode again, which may mean that some of the previous
    // sizes we've decoded at aren't useful anymore. We can allow them to
    // expire from the cache by unlocking them here. When the decode finishes,
    // it will send an invalidation that will cause all instances of this image
    // to redraw. If this image is locked, any surfaces that are still useful
    // will become locked again when LookupFrame touches them, and the remainder
    // will eventually expire.
    SurfaceCache::UnlockSurfaces(ImageKey(this));
  }

  // Create a decoder.
  nsRefPtr<Decoder> decoder = CreateDecoder(aSize, aFlags);
  if (!decoder) {
    return NS_ERROR_FAILURE;
  }

  if (mHasSourceData) {
    // If we have all the data, we can sync decode if requested.
    if (aFlags & FLAG_SYNC_DECODE) {
      PROFILER_LABEL_PRINTF("DecodePool", "SyncDecodeIfPossible",
        js::ProfileEntry::Category::GRAPHICS, "%s", GetURIString().get());
      DecodePool::Singleton()->SyncDecodeIfPossible(decoder);
      return NS_OK;
    }

    if (aFlags & FLAG_SYNC_DECODE_IF_FAST) {
      PROFILER_LABEL_PRINTF("DecodePool", "SyncDecodeIfSmall",
        js::ProfileEntry::Category::GRAPHICS, "%s", GetURIString().get());
      DecodePool::Singleton()->SyncDecodeIfSmall(decoder);
      return NS_OK;
    }
  }

  // Perform an async decode. We also take this path if we don't have all the
  // source data yet, since sync decoding is impossible in that situation.
  DecodePool::Singleton()->AsyncDecode(decoder);
  return NS_OK;
}

void
RasterImage::RecoverFromLossOfFrames(const IntSize& aSize, uint32_t aFlags)
{
  if (!mHasSize) {
    return;
  }

  NS_WARNING("An imgFrame became invalid. Attempting to recover...");

  // Discard all existing frames, since they're probably all now invalid.
  SurfaceCache::RemoveImage(ImageKey(this));

  // Animated images require some special handling, because we normally require
  // that they never be discarded.
  if (mAnim) {
    Decode(Some(mSize), aFlags | FLAG_SYNC_DECODE);
    ResetAnimation();
    return;
  }

  // For non-animated images, it's fine to recover using an async decode.
  Decode(Some(aSize), aFlags);
}

bool
RasterImage::CanScale(GraphicsFilter aFilter,
                      const IntSize& aSize,
                      uint32_t aFlags)
{
#ifndef MOZ_ENABLE_SKIA
  // The high-quality scaler requires Skia.
  return false;
#else
  // Check basic requirements: HQ downscaling is enabled, we have all the source
  // data and know our size, the flags allow us to do it, and a 'good' filter is
  // being used. The flags may ask us not to scale because the caller isn't
  // drawing to the window. If we're drawing to something else (e.g. a canvas)
  // we usually have no way of updating what we've drawn, so HQ scaling is
  // useless.
  if (!gfxPrefs::ImageHQDownscalingEnabled() || !mHasSize || !mHasSourceData ||
      !(aFlags & imgIContainer::FLAG_HIGH_QUALITY_SCALING) ||
      aFilter != GraphicsFilter::FILTER_GOOD) {
    return false;
  }

  // We don't HQ scale images that we can downscale during decode.
  if (mDownscaleDuringDecode) {
    return false;
  }

  // We don't use the scaler for animated or transient images to avoid doing a
  // bunch of work on an image that just gets thrown away.
  if (mAnim || mTransient) {
    return false;
  }

  // If target size is 1:1 with original, don't scale.
  if (aSize == mSize) {
    return false;
  }

  // To save memory, don't quality upscale images bigger than the limit.
  if (aSize.width > mSize.width || aSize.height > mSize.height) {
    uint32_t scaledSize = static_cast<uint32_t>(aSize.width * aSize.height);
    if (scaledSize > gfxPrefs::ImageHQUpscalingMaxSize()) {
      return false;
    }
  }

  // There's no point in scaling if we can't store the result.
  if (!SurfaceCache::CanHold(aSize)) {
    return false;
  }

  // XXX(seth): It's not clear what this check buys us over
  // gfxPrefs::ImageHQUpscalingMaxSize().
  // The default value of this pref is 1000, which means that we never upscale.
  // If that's all it's getting us, I'd rather we just forbid that explicitly.
  gfx::Size scale(double(aSize.width) / mSize.width,
                  double(aSize.height) / mSize.height);
  gfxFloat minFactor = gfxPrefs::ImageHQDownscalingMinFactor() / 1000.0;
  return (scale.width < minFactor || scale.height < minFactor);
#endif
}

bool
RasterImage::CanDownscaleDuringDecode(const IntSize& aSize, uint32_t aFlags)
{
  // Check basic requirements: downscale-during-decode is enabled for this
  // image, we have all the source data and know our size, the flags allow us to
  // do it, and a 'good' filter is being used.
  if (!mDownscaleDuringDecode || !mHasSize ||
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

void
RasterImage::NotifyNewScaledFrame()
{
  // Send an invalidation so observers will repaint and can take advantage of
  // the new scaled frame if possible.
  NotifyProgress(NoProgress, IntRect(0, 0, mSize.width, mSize.height));
}

void
RasterImage::RequestScale(imgFrame* aFrame,
                          uint32_t aFlags,
                          const IntSize& aSize)
{
  // We don't scale frames which aren't fully decoded.
  if (!aFrame->IsImageComplete()) {
    return;
  }

  // We can't scale frames that need padding or are single pixel.
  if (aFrame->NeedsPadding() || aFrame->IsSinglePixel()) {
    return;
  }

  // We also can't scale if we can't lock the image data for this frame.
  RawAccessFrameRef frameRef = aFrame->RawAccessRef();
  if (!frameRef) {
    return;
  }

  nsRefPtr<ScaleRunner> runner =
    new ScaleRunner(this, DecodeFlags(aFlags), aSize, Move(frameRef));
  if (runner->Init()) {
    if (!sScaleWorkerThread) {
      NS_NewNamedThread("Image Scaler", getter_AddRefs(sScaleWorkerThread));
      ClearOnShutdown(&sScaleWorkerThread);
    }

    sScaleWorkerThread->Dispatch(runner, NS_DISPATCH_NORMAL);
  }
}

DrawResult
RasterImage::DrawWithPreDownscaleIfNeeded(DrawableFrameRef&& aFrameRef,
                                          gfxContext* aContext,
                                          const IntSize& aSize,
                                          const ImageRegion& aRegion,
                                          GraphicsFilter aFilter,
                                          uint32_t aFlags)
{
  DrawableFrameRef frameRef;

  if (CanScale(aFilter, aSize, aFlags)) {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(this),
                           RasterSurfaceKey(aSize,
                                            DecodeFlags(aFlags),
                                            0));
    if (!result) {
      // We either didn't have a matching scaled frame or the OS threw it away.
      // Request a new one so we'll be ready next time. For now, we'll fall back
      // to aFrameRef below.
      RequestScale(aFrameRef.get(), aFlags, aSize);
    }
    if (result && result.DrawableRef()->IsImageComplete()) {
      frameRef = Move(result.DrawableRef());  // The scaled version is ready.
    }
  }

  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  ImageRegion region(aRegion);
  bool frameIsComplete = true;  // We already checked HQ scaled frames.
  if (!frameRef) {
    // There's no HQ scaled frame available, so we'll have to use the frame
    // provided by the caller.
    frameRef = Move(aFrameRef);
    frameIsComplete = frameRef->IsImageComplete();
  }

  // By now we may have a frame with the requested size. If not, we need to
  // adjust the drawing parameters accordingly.
  IntSize finalSize = frameRef->GetImageSize();
  bool couldRedecodeForBetterFrame = false;
  if (finalSize != aSize) {
    gfx::Size scale(double(aSize.width) / finalSize.width,
                    double(aSize.height) / finalSize.height);
    aContext->Multiply(gfxMatrix::Scaling(scale.width, scale.height));
    region.Scale(1.0 / scale.width, 1.0 / scale.height);

    couldRedecodeForBetterFrame = mDownscaleDuringDecode &&
                                  CanDownscaleDuringDecode(aSize, aFlags);
  }

  if (!frameRef->Draw(aContext, region, aFilter, aFlags)) {
    RecoverFromLossOfFrames(aSize, aFlags);
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
  if (DecodeFlags(aFlags) != DECODE_FLAGS_DEFAULT) {
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

  auto result = DrawWithPreDownscaleIfNeeded(Move(ref), aContext, aSize,
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
/* void lockImage() */
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
/* void unlockImage() */
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
/* void requestDiscard() */
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

// nsIInputStream callback to copy the incoming image data directly to the
// RasterImage without processing. The RasterImage is passed as the closure.
// Always reads everything it gets, even if the data is erroneous.
NS_METHOD
RasterImage::WriteToSourceBuffer(nsIInputStream* /* unused */,
                                 void*          aClosure,
                                 const char*    aFromRawSegment,
                                 uint32_t       /* unused */,
                                 uint32_t       aCount,
                                 uint32_t*      aWriteCount)
{
  // Retrieve the RasterImage
  RasterImage* image = static_cast<RasterImage*>(aClosure);

  // Copy the source data. Unless we hit OOM, we squelch the return value
  // here, because returning an error means that ReadSegments stops
  // reading data, violating our invariant that we read everything we get.
  // If we hit OOM then we fail and the load is aborted.
  nsresult rv = image->mSourceBuffer->Append(aFromRawSegment, aCount);
  if (rv == NS_ERROR_OUT_OF_MEMORY) {
    image->DoError();
    return rv;
  }

  // We wrote everything we got
  *aWriteCount = aCount;

  return NS_OK;
}

bool
RasterImage::ShouldAnimate()
{
  return ImageResource::ShouldAnimate() && GetNumFrames() >= 2 &&
         !mAnimationFinished;
}

/* readonly attribute uint32_t framesNotified; */
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
                            uint32_t aFlags /* = DECODE_FLAGS_DEFAULT */)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that we stay alive long enough to finish notifying.
  nsRefPtr<RasterImage> image(this);

  bool wasDefaultFlags = aFlags == DECODE_FLAGS_DEFAULT;

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
  MOZ_ASSERT(mError || mHasSize || !aDecoder->HasSize(),
             "Should have handed off size by now");

  // Send out any final notifications.
  NotifyProgress(aDecoder->TakeProgress(),
                 aDecoder->TakeInvalidRect(),
                 aDecoder->GetDecodeFlags());

  bool wasSize = aDecoder->IsSizeDecode();
  bool done = aDecoder->GetDecodeDone();

  if (!wasSize && aDecoder->ChunkCount()) {
    Telemetry::Accumulate(Telemetry::IMAGE_DECODE_CHUNKS,
                          aDecoder->ChunkCount());
  }

  if (done) {
    // Do some telemetry if this isn't a size decode.
    if (!wasSize) {
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
    } else if (wasSize && !mHasSize) {
      DoError();
    }

    // If we were waiting to fire the load event, go ahead and fire it now.
    if (mLoadProgress && wasSize) {
      NotifyForLoadEvent(*mLoadProgress);
      mLoadProgress = Nothing();
      NotifyProgress(FLAG_ONLOAD_UNBLOCKED);
    }
  }

  if (aDecoder->ImageIsLocked()) {
    // Unlock the image, balancing the LockImage call we made in CreateDecoder.
    UnlockImage();
  }

  // If we were a size decode and a full decode was requested, now's the time.
  if (done && wasSize && mWantFullDecode) {
    mWantFullDecode = false;
    RequestDecode();
  }
}

already_AddRefed<imgIContainer>
RasterImage::Unwrap()
{
  nsCOMPtr<imgIContainer> self(this);
  return self.forget();
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
  } else if (CanScale(aFilter, destSize, aFlags)) {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(this),
                           RasterSurfaceKey(destSize,
                                            DecodeFlags(aFlags),
                                            0));

    if (result && result.DrawableRef()->IsImageComplete()) {
      return destSize;  // We have an existing HQ scale for this size.
    }
    if (!result) {
      // We could HQ scale to this size, but we haven't. Request a scale now.
      DrawableFrameRef ref = LookupFrame(GetRequestedFrameIndex(aWhichFrame),
                                         mSize, aFlags);
      if (ref) {
        RequestScale(ref.get(), aFlags, destSize);
      }
    }
  }

  // We either can't HQ scale to this size or the scaled version isn't ready
  // yet. Use our intrinsic size for now.
  return mSize;
}

} // namespace image
} // namespace mozilla
