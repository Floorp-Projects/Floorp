/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Must #include ImageLogging.h before any IPDL-generated files or other files that #include prlog.h
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
#include "nsPresContext.h"
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

// a mask for flags that will affect the decoding
#define DECODE_FLAGS_MASK (imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA | imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION)
#define DECODE_FLAGS_DEFAULT 0

static uint32_t
DecodeFlags(uint32_t aFlags)
{
  return aFlags & DECODE_FLAGS_MASK;
}

/* Accounting for compressed data */
#if defined(PR_LOGGING)
static PRLogModuleInfo *
GetCompressedImageAccountingLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("CompressedImageAccounting");
  return sLog;
}
#else
#define GetCompressedImageAccountingLog()
#endif

// The maximum number of times any one RasterImage was decoded.  This is only
// used for statistics.
static int32_t sMaxDecodeCount = 0;

/* We define our own error checking macros here for 2 reasons:
 *
 * 1) Most of the failures we encounter here will (hopefully) be
 * the result of decoding failures (ie, bad data) and not code
 * failures. As such, we don't want to clutter up debug consoles
 * with spurious messages about NS_ENSURE_SUCCESS failures.
 *
 * 2) We want to set the internal error flag, shutdown properly,
 * and end up in an error state.
 *
 * So this macro should be called when the desired failure behavior
 * is to put the container into an error state and return failure.
 * It goes without saying that macro won't compile outside of a
 * non-static RasterImage method.
 */
#define LOG_CONTAINER_ERROR                      \
  PR_BEGIN_MACRO                                 \
  PR_LOG (GetImgLog(), PR_LOG_ERROR,             \
          ("RasterImage: [this=%p] Error "      \
           "detected at line %u for image of "   \
           "type %s\n", this, __LINE__,          \
           mSourceDataMimeType.get()));          \
  PR_END_MACRO

#define CONTAINER_ENSURE_SUCCESS(status)      \
  PR_BEGIN_MACRO                              \
  nsresult _status = status; /* eval once */  \
  if (NS_FAILED(_status)) {                   \
    LOG_CONTAINER_ERROR;                      \
    DoError();                                \
    return _status;                           \
  }                                           \
 PR_END_MACRO

#define CONTAINER_ENSURE_TRUE(arg, rv)  \
  PR_BEGIN_MACRO                        \
  if (!(arg)) {                         \
    LOG_CONTAINER_ERROR;                \
    DoError();                          \
    return rv;                          \
  }                                     \
  PR_END_MACRO



static int num_containers;
static int num_discardable_containers;
static int64_t total_source_bytes;
static int64_t discardable_source_bytes;

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
              const nsIntSize& aSize,
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
                         RasterSurfaceKey(mDstSize.ToIntSize(), mImageFlags, 0),
                         Lifetime::Transient);

    return true;
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
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
        MOZ_ASSERT(mDstRef->ImageComplete(),
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
                                  RasterSurfaceKey(mDstSize.ToIntSize(),
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
  const nsIntSize      mDstSize;
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
RasterImage::RasterImage(ProgressTracker* aProgressTracker,
                         ImageURL* aURI /* = nullptr */) :
  ImageResource(aURI), // invoke superclass's constructor
  mSize(0,0),
  mFrameDecodeFlags(DECODE_FLAGS_DEFAULT),
  mLockCount(0),
  mDecodeCount(0),
  mRequestedSampleSize(0),
#ifdef DEBUG
  mFramesNotified(0),
#endif
  mDecodingMonitor("RasterImage Decoding Monitor"),
  mDecoder(nullptr),
  mDecodeStatus(DecodeStatus::INACTIVE),
  mFrameCount(0),
  mNotifyProgress(NoProgress),
  mNotifying(false),
  mHasSize(false),
  mDecodeOnDraw(false),
  mTransient(false),
  mDiscardable(false),
  mHasSourceData(false),
  mDecoded(false),
  mHasBeenDecoded(false),
  mPendingAnimation(false),
  mAnimationFinished(false),
  mWantFullDecode(false),
  mPendingError(false)
{
  mProgressTrackerInit = new ProgressTrackerInit(this, aProgressTracker);

  Telemetry::GetHistogramById(Telemetry::IMAGE_DECODE_COUNT)->Add(0);

  // Statistics
  num_containers++;
}

//******************************************************************************
RasterImage::~RasterImage()
{
  // Discardable statistics
  if (mDiscardable) {
    num_discardable_containers--;
    discardable_source_bytes -= mSourceData.Length();

    PR_LOG (GetCompressedImageAccountingLog(), PR_LOG_DEBUG,
            ("CompressedImageAccounting: destroying RasterImage %p.  "
             "Total Containers: %d, Discardable containers: %d, "
             "Total source bytes: %lld, Source bytes for discardable containers %lld",
             this,
             num_containers,
             num_discardable_containers,
             total_source_bytes,
             discardable_source_bytes));
  }

  if (mDecoder) {
    // Kill off our decode request, if it's pending.  (If not, this call is
    // harmless.)
    ReentrantMonitorAutoEnter lock(mDecodingMonitor);
    DecodePool::StopDecoding(this);
    mDecoder = nullptr;
  }

  // Release all frames from the surface cache.
  SurfaceCache::RemoveImage(ImageKey(this));

  mAnim = nullptr;

  // Total statistics
  num_containers--;
  total_source_bytes -= mSourceData.Length();
}

/* static */ void
RasterImage::Initialize()
{
  // Create our singletons now, so we don't have to worry about what thread
  // they're created on.
  DecodePool::Singleton();
}

nsresult
RasterImage::Init(const char* aMimeType,
                  uint32_t aFlags)
{
  // We don't support re-initialization
  if (mInitialized)
    return NS_ERROR_ILLEGAL_VALUE;

  // Not sure an error can happen before init, but be safe
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aMimeType);

  // We must be non-discardable and non-decode-on-draw for
  // transient images.
  MOZ_ASSERT(!(aFlags & INIT_FLAG_TRANSIENT) ||
               (!(aFlags & INIT_FLAG_DISCARDABLE) &&
                !(aFlags & INIT_FLAG_DECODE_ON_DRAW)),
             "Transient images can't be discardable or decode-on-draw");

  // Store initialization data
  mSourceDataMimeType.Assign(aMimeType);
  mDiscardable = !!(aFlags & INIT_FLAG_DISCARDABLE);
  mDecodeOnDraw = !!(aFlags & INIT_FLAG_DECODE_ON_DRAW);
  mTransient = !!(aFlags & INIT_FLAG_TRANSIENT);

  // Statistics
  if (mDiscardable) {
    num_discardable_containers++;
    discardable_source_bytes += mSourceData.Length();
  }

  // Lock this image's surfaces in the SurfaceCache if we're not discardable.
  if (!mDiscardable) {
    SurfaceCache::LockImage(ImageKey(this));
  }

  // Instantiate the decoder
  nsresult rv = InitDecoder(/* aDoSizeDecode = */ true);
  CONTAINER_ENSURE_SUCCESS(rv);

  // If we aren't storing source data, we want to switch from a size decode to
  // a full decode as soon as possible.
  if (!StoringSourceData()) {
    mWantFullDecode = true;
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

    UpdateImageContainer();

    if (mProgressTracker) {
      mProgressTracker->SyncNotifyProgress(NoProgress, res.dirtyRect);
    }
  }

  if (res.animationFinished) {
    mAnimationFinished = true;
    EvaluateAnimation();
  }
}

//******************************************************************************
/* readonly attribute int32_t width; */
NS_IMETHODIMP
RasterImage::GetWidth(int32_t *aWidth)
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
RasterImage::GetHeight(int32_t *aHeight)
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
  if (mError)
    return NS_ERROR_FAILURE;

  *aSize = nsSize(nsPresContext::CSSPixelsToAppUnits(mSize.width),
                  nsPresContext::CSSPixelsToAppUnits(mSize.height));
  return NS_OK;
}

//******************************************************************************
/* [noscript] readonly attribute nsSize intrinsicRatio; */
NS_IMETHODIMP
RasterImage::GetIntrinsicRatio(nsSize* aRatio)
{
  if (mError)
    return NS_ERROR_FAILURE;

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
RasterImage::GetType(uint16_t *aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = GetType();
  return NS_OK;
}

//******************************************************************************
/* [noscript, notxpcom] uint16_t GetType(); */
NS_IMETHODIMP_(uint16_t)
RasterImage::GetType()
{
  return imgIContainer::TYPE_RASTER;
}

DrawableFrameRef
RasterImage::LookupFrameInternal(uint32_t aFrameNum,
                                 const nsIntSize& aSize,
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

  return SurfaceCache::Lookup(ImageKey(this),
                              RasterSurfaceKey(aSize.ToIntSize(),
                                               DecodeFlags(aFlags),
                                               aFrameNum));
}

DrawableFrameRef
RasterImage::LookupFrame(uint32_t aFrameNum,
                         const nsIntSize& aSize,
                         uint32_t aFlags,
                         bool aShouldSyncNotify /* = true */)
{
  MOZ_ASSERT(NS_IsMainThread());

  DrawableFrameRef ref = LookupFrameInternal(aFrameNum, aSize, aFlags);

  if (!ref && IsOpaque() && aFrameNum == 0) {
    // We can use non-premultiplied alpha frames when premultipled alpha is
    // requested, or vice versa, if this image is opaque. Try again with the bit
    // toggled.
    ref = LookupFrameInternal(aFrameNum, aSize,
                              aFlags ^ FLAG_DECODE_NO_PREMULTIPLY_ALPHA);
  }

  if (!ref) {
    // The OS threw this frame away. We need to redecode if we can.
    MOZ_ASSERT(!mAnim, "Animated frames should be locked");

    // Update our state so the decoder knows what to do.
    mFrameDecodeFlags = aFlags & DECODE_FLAGS_MASK;
    mDecoded = false;
    mFrameCount = 0;
    WantDecodedFrames(aFlags, aShouldSyncNotify);

    // See if we managed to redecode enough to get the frame we want.
    ref = LookupFrameInternal(aFrameNum, aSize, aFlags);

    if (!ref) {
      // We didn't successfully redecode, so just fail.
      return DrawableFrameRef();
    }
  }

  if (ref->GetCompositingFailed()) {
    return DrawableFrameRef();
  }

  MOZ_ASSERT(!ref || !ref->GetIsPaletted(), "Should not have paletted frame");

  return ref;
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

nsIntRect
RasterImage::GetFirstFrameRect()
{
  if (mAnim) {
    return mAnim->GetFirstFrameRefreshArea();
  }

  // Fall back to our size. This is implicitly zero-size if !mHasSize.
  return nsIntRect(nsIntPoint(0,0), mSize);
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
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(this, &RasterImage::OnSurfaceDiscarded);
    NS_DispatchToMainThread(runnable);
    return;
  }

  if (mProgressTracker) {
    mProgressTracker->OnDiscard();
  }
}

//******************************************************************************
/* readonly attribute boolean animated; */
NS_IMETHODIMP
RasterImage::GetAnimated(bool *aAnimated)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aAnimated);

  // If we have mAnim, we can know for sure
  if (mAnim) {
    *aAnimated = true;
    return NS_OK;
  }

  // Otherwise, we need to have been decoded to know for sure, since if we were
  // decoded at least once mAnim would have been created for animated images
  if (!mHasBeenDecoded)
    return NS_ERROR_NOT_AVAILABLE;

  // We know for sure
  *aAnimated = false;

  return NS_OK;
}

//******************************************************************************
/* [notxpcom] int32_t getFirstFrameDelay (); */
NS_IMETHODIMP_(int32_t)
RasterImage::GetFirstFrameDelay()
{
  if (mError)
    return -1;

  bool animated = false;
  if (NS_FAILED(GetAnimated(&animated)) || !animated)
    return -1;

  MOZ_ASSERT(mAnim, "Animated images should have a FrameAnimator");
  return mAnim->GetTimeoutForFrame(0);
}

TemporaryRef<SourceSurface>
RasterImage::CopyFrame(uint32_t aWhichFrame,
                       uint32_t aFlags,
                       bool aShouldSyncNotify /* = true */)
{
  if (aWhichFrame > FRAME_MAX_VALUE)
    return nullptr;

  if (mError)
    return nullptr;

  // Get the frame. If it's not there, it's probably the caller's fault for
  // not waiting for the data to be loaded from the network or not passing
  // FLAG_SYNC_DECODE
  DrawableFrameRef frameRef = LookupFrame(GetRequestedFrameIndex(aWhichFrame),
                                          mSize, aFlags, aShouldSyncNotify);
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
  DebugOnly<bool> success =
    surf->Map(DataSourceSurface::MapType::WRITE, &mapping);
  NS_ASSERTION(success, "Failed to map surface");
  RefPtr<DrawTarget> target =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     mapping.mData,
                                     size,
                                     mapping.mStride,
                                     SurfaceFormat::B8G8R8A8);

  nsIntRect intFrameRect = frameRef->GetRect();
  Rect rect(intFrameRect.x, intFrameRect.y,
            intFrameRect.width, intFrameRect.height);
  if (frameRef->IsSinglePixel()) {
    target->FillRect(rect, ColorPattern(frameRef->SinglePixelColor()),
                     DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  } else {
    RefPtr<SourceSurface> srcSurf = frameRef->GetSurface();
    Rect srcRect(0, 0, intFrameRect.width, intFrameRect.height);
    target->DrawSurface(srcSurf, srcRect, rect);
  }

  target->Flush();
  surf->Unmap();

  return surf;
}

//******************************************************************************
/* [noscript] SourceSurface getFrame(in uint32_t aWhichFrame,
 *                                   in uint32_t aFlags); */
NS_IMETHODIMP_(TemporaryRef<SourceSurface>)
RasterImage::GetFrame(uint32_t aWhichFrame,
                      uint32_t aFlags)
{
  return GetFrameInternal(aWhichFrame, aFlags);
}

TemporaryRef<SourceSurface>
RasterImage::GetFrameInternal(uint32_t aWhichFrame,
                              uint32_t aFlags,
                              bool aShouldSyncNotify /* = true */)
{
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE);

  if (aWhichFrame > FRAME_MAX_VALUE)
    return nullptr;

  if (mError)
    return nullptr;

  // Get the frame. If it's not there, it's probably the caller's fault for
  // not waiting for the data to be loaded from the network or not passing
  // FLAG_SYNC_DECODE
  DrawableFrameRef frameRef = LookupFrame(GetRequestedFrameIndex(aWhichFrame),
                                          mSize, aFlags, aShouldSyncNotify);
  if (!frameRef) {
    // The OS threw this frame away and we couldn't redecode it.
    return nullptr;
  }

  // If this frame covers the entire image, we can just reuse its existing
  // surface.
  RefPtr<SourceSurface> frameSurf;
  nsIntRect frameRect = frameRef->GetRect();
  if (frameRect.x == 0 && frameRect.y == 0 &&
      frameRect.width == mSize.width &&
      frameRect.height == mSize.height) {
    frameSurf = frameRef->GetSurface();
  }

  // The image doesn't have a usable surface because it's been optimized away or
  // because it's a partial update frame from an animation. Create one.
  if (!frameSurf) {
    frameSurf = CopyFrame(aWhichFrame, aFlags, aShouldSyncNotify);
  }

  return frameSurf;
}

already_AddRefed<layers::Image>
RasterImage::GetCurrentImage()
{
  RefPtr<SourceSurface> surface =
    GetFrameInternal(FRAME_CURRENT, FLAG_NONE, /* aShouldSyncNotify = */ false);
  if (!surface) {
    // The OS threw out some or all of our buffer. We'll need to wait for the
    // redecode (which was automatically triggered by GetFrame) to complete.
    return nullptr;
  }

  if (!mImageContainer) {
    mImageContainer = LayerManager::CreateImageContainer();
  }

  CairoImage::Data cairoData;
  GetWidth(&cairoData.mSize.width);
  GetHeight(&cairoData.mSize.height);
  cairoData.mSourceSurface = surface;

  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(ImageFormat::CAIRO_SURFACE);
  NS_ASSERTION(image, "Failed to create Image");

  static_cast<CairoImage*>(image.get())->SetData(cairoData);

  return image.forget();
}


NS_IMETHODIMP
RasterImage::GetImageContainer(LayerManager* aManager, ImageContainer **_retval)
{
  int32_t maxTextureSize = aManager->GetMaxTextureSize();
  if (mSize.width > maxTextureSize || mSize.height > maxTextureSize) {
    *_retval = nullptr;
    return NS_OK;
  }

  if (IsUnlocked() && mProgressTracker) {
    mProgressTracker->OnUnlockedDraw();
  }

  if (!mImageContainer) {
    mImageContainer = mImageContainerCache;
  }

  if (mImageContainer) {
    *_retval = mImageContainer;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  nsRefPtr<layers::Image> image = GetCurrentImage();
  if (!image) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mImageContainer->SetCurrentImageInTransaction(image);

  *_retval = mImageContainer;
  NS_ADDREF(*_retval);
  // We only need to be careful about holding on to the image when it is
  // discardable by the OS.
  if (CanDiscard()) {
    mImageContainerCache = mImageContainer;
    mImageContainer = nullptr;
  }

  return NS_OK;
}

void
RasterImage::UpdateImageContainer()
{
  if (!mImageContainer) {
    return;
  }

  nsRefPtr<layers::Image> image = GetCurrentImage();
  if (!image) {
    return;
  }

  mImageContainer->SetCurrentImage(image);
}

size_t
RasterImage::SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const
{
  // n == 0 is possible for two reasons.
  // - This is a zero-length image.
  // - We're on a platform where moz_malloc_size_of always returns 0.
  // In either case the fallback works appropriately.
  size_t n = mSourceData.SizeOfExcludingThis(aMallocSizeOf);
  if (n == 0) {
    n = mSourceData.Length();
  }
  return n;
}

size_t
RasterImage::SizeOfDecoded(gfxMemoryLocation aLocation,
                           MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  n += SurfaceCache::SizeOfSurfaces(ImageKey(this), aLocation, aMallocSizeOf);
  if (mAnim) {
    n += mAnim->SizeOfCompositingFrames(aLocation, aMallocSizeOf);
  }
  return n;
}

class OnAddedFrameRunnable : public nsRunnable
{
public:
  OnAddedFrameRunnable(RasterImage* aImage,
                       uint32_t aNewFrameCount,
                       const nsIntRect& aNewRefreshArea)
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
  nsIntRect mNewRefreshArea;
};

void
RasterImage::OnAddedFrame(uint32_t aNewFrameCount,
                          const nsIntRect& aNewRefreshArea)
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      new OnAddedFrameRunnable(this, aNewFrameCount, aNewRefreshArea);
    NS_DispatchToMainThread(runnable);
    return;
  }

  MOZ_ASSERT((mFrameCount == 1 && aNewFrameCount == 1) ||
             mFrameCount < aNewFrameCount,
             "Frame count running backwards");

  mFrameCount = aNewFrameCount;

  if (aNewFrameCount == 2) {
    // We're becoming animated, so initialize animation stuff.
    MOZ_ASSERT(!mAnim, "Already have animation state?");
    mAnim = MakeUnique<FrameAnimator>(this, mSize.ToIntSize(), mAnimationMode);

    // We don't support discarding animated images (See bug 414259).
    // Lock the image and throw away the key.
    //
    // Note that this is inefficient, since we could get rid of the source data
    // too. However, doing this is actually hard, because we're probably
    // mid-decode, and thus we're decoding out of the source buffer. Since we're
    // going to fix this anyway later, and since we didn't kill the source data
    // in the old world either, locking is acceptable for the moment.
    LockImage();

    if (mPendingAnimation && ShouldAnimate()) {
      StartAnimation();
    }
  }

  if (aNewFrameCount > 1) {
    mAnim->UnionFirstFrameRefreshArea(aNewRefreshArea);
  }
}

nsresult
RasterImage::SetSize(int32_t aWidth, int32_t aHeight, Orientation aOrientation)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDecodingMonitor.AssertCurrentThreadIn();

  if (mError)
    return NS_ERROR_FAILURE;

  // Ensure that we have positive values
  // XXX - Why isn't the size unsigned? Should this be changed?
  if ((aWidth < 0) || (aHeight < 0))
    return NS_ERROR_INVALID_ARG;

  // if we already have a size, check the new size against the old one
  if (mHasSize &&
      ((aWidth != mSize.width) ||
       (aHeight != mSize.height) ||
       (aOrientation != mOrientation))) {
    NS_WARNING("Image changed size on redecode! This should not happen!");

    // Make the decoder aware of the error so that it doesn't try to call
    // FinishInternal during ShutdownDecoder.
    if (mDecoder)
      mDecoder->PostResizeError();

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
RasterImage::DecodingComplete(imgFrame* aFinalFrame)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError) {
    return;
  }

  // Flag that we're done decoding.
  // XXX - these should probably be combined when we fix animated image
  // discarding with bug 500402.
  mDecoded = true;
  mHasBeenDecoded = true;

  // If there's only 1 frame, mark it as optimizable. Optimizing animated images
  // is not supported. Optimizing transient images isn't worth it.
  if (GetNumFrames() == 1 && !mTransient && aFinalFrame) {
    aFinalFrame->SetOptimizable();
  }

  if (mAnim) {
    mAnim->SetDoneDecoding(true);
  }
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
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(ShouldAnimate(), "Should not animate!");

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
  NS_ABORT_IF_FALSE(mAnimating, "Should be animating!");

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
  if (mError)
    return NS_ERROR_FAILURE;

  mPendingAnimation = false;

  if (mAnimationMode == kDontAnimMode || !mAnim ||
      mAnim->GetCurrentAnimationFrameIndex() == 0) {
    return NS_OK;
  }

  mAnimationFinished = false;

  if (mAnimating)
    StopAnimation();

  MOZ_ASSERT(mAnim, "Should have a FrameAnimator");
  mAnim->ResetAnimation();

  UpdateImageContainer();

  // Note - We probably want to kick off a redecode somewhere around here when
  // we fix bug 500402.

  // Update display
  if (mProgressTracker) {
    nsIntRect rect = mAnim->GetFirstFrameRefreshArea();
    mProgressTracker->SyncNotifyProgress(NoProgress, rect);
  }

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
  if (mError || mAnimationMode == kDontAnimMode || mAnimating || !mAnim)
    return;

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
  if (mError)
    return;

  // No need to set this if we're not an animation.
  if (mAnim) {
    mAnim->SetLoopCount(aLoopCount);
  }
}

NS_IMETHODIMP_(nsIntRect)
RasterImage::GetImageSpaceInvalidationRect(const nsIntRect& aRect)
{
  return aRect;
}

nsresult
RasterImage::AddSourceData(const char *aBuffer, uint32_t aCount)
{
  ReentrantMonitorAutoEnter lock(mDecodingMonitor);

  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aBuffer);
  nsresult rv = NS_OK;

  // We should not call this if we're not initialized
  NS_ABORT_IF_FALSE(mInitialized, "Calling AddSourceData() on uninitialized "
                                  "RasterImage!");

  // We should not call this if we're already finished adding source data
  NS_ABORT_IF_FALSE(!mHasSourceData, "Calling AddSourceData() after calling "
                                     "sourceDataComplete()!");

  // Image is already decoded, we shouldn't be getting data, but it could
  // be extra garbage data at the end of a file.
  if (mDecoded) {
    return NS_OK;
  }

  // If we're not storing source data and we've previously gotten the size,
  // write the data directly to the decoder. (If we haven't gotten the size,
  // we'll queue up the data and write it out when we do.)
  if (!StoringSourceData() && mHasSize) {
    rv = WriteToDecoder(aBuffer, aCount);
    CONTAINER_ENSURE_SUCCESS(rv);

    rv = FinishedSomeDecoding();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // Otherwise, we're storing data in the source buffer
  else {

    // Store the data
    char *newElem = mSourceData.AppendElements(aBuffer, aCount);
    if (!newElem)
      return NS_ERROR_OUT_OF_MEMORY;

    if (mDecoder) {
      DecodePool::Singleton()->RequestDecode(this);
    }
  }

  // Statistics
  total_source_bytes += aCount;
  if (mDiscardable)
    discardable_source_bytes += aCount;
  PR_LOG (GetCompressedImageAccountingLog(), PR_LOG_DEBUG,
          ("CompressedImageAccounting: Added compressed data to RasterImage %p (%s). "
           "Total Containers: %d, Discardable containers: %d, "
           "Total source bytes: %lld, Source bytes for discardable containers %lld",
           this,
           mSourceDataMimeType.get(),
           num_containers,
           num_discardable_containers,
           total_source_bytes,
           discardable_source_bytes));

  return NS_OK;
}

/* Note!  buf must be declared as char buf[9]; */
// just used for logging and hashing the header
static void
get_header_str (char *buf, char *data, size_t data_len)
{
  int i;
  int n;
  static char hex[] = "0123456789abcdef";

  n = data_len < 4 ? data_len : 4;

  for (i = 0; i < n; i++) {
    buf[i * 2]     = hex[(data[i] >> 4) & 0x0f];
    buf[i * 2 + 1] = hex[data[i] & 0x0f];
  }

  buf[i * 2] = 0;
}

nsresult
RasterImage::DoImageDataComplete()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError)
    return NS_ERROR_FAILURE;

  // If we've been called before, ignore. Otherwise, flag that we have everything
  if (mHasSourceData)
    return NS_OK;
  mHasSourceData = true;

  // If there's a decoder open, synchronously decode the beginning of the image
  // to check for errors and get the image's size.  (If we already have the
  // image's size, this does nothing.)  Then kick off an async decode of the
  // rest of the image.
  if (mDecoder) {
    nsresult rv = DecodePool::Singleton()->DecodeUntilSizeAvailable(this);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  {
    ReentrantMonitorAutoEnter lock(mDecodingMonitor);

    // Free up any extra space in the backing buffer
    mSourceData.Compact();
  }

  // Log header information
  if (PR_LOG_TEST(GetCompressedImageAccountingLog(), PR_LOG_DEBUG)) {
    char buf[9];
    get_header_str(buf, mSourceData.Elements(), mSourceData.Length());
    PR_LOG (GetCompressedImageAccountingLog(), PR_LOG_DEBUG,
            ("CompressedImageAccounting: RasterImage::SourceDataComplete() - data "
             "is done for container %p (%s) - header %p is 0x%s (length %d)",
             this,
             mSourceDataMimeType.get(),
             mSourceData.Elements(),
             buf,
             mSourceData.Length()));
  }

  return NS_OK;
}

nsresult
RasterImage::OnImageDataComplete(nsIRequest*, nsISupports*, nsresult aStatus, bool aLastPart)
{
  nsresult finalStatus = DoImageDataComplete();

  // Give precedence to Necko failure codes.
  if (NS_FAILED(aStatus))
    finalStatus = aStatus;

  // We just recorded OnStopRequest; we need to inform our listeners.
  {
    ReentrantMonitorAutoEnter lock(mDecodingMonitor);
    FinishedSomeDecoding(ShutdownReason::DONE,
                         LoadCompleteProgress(aLastPart, mError, finalStatus));
  }

  return finalStatus;
}

nsresult
RasterImage::OnImageDataAvailable(nsIRequest*,
                                  nsISupports*,
                                  nsIInputStream* aInStr,
                                  uint64_t,
                                  uint32_t aCount)
{
  nsresult rv;

  // WriteToRasterImage always consumes everything it gets
  // if it doesn't run out of memory
  uint32_t bytesRead;
  rv = aInStr->ReadSegments(WriteToRasterImage, this, aCount, &bytesRead);

  NS_ABORT_IF_FALSE(bytesRead == aCount || HasError() || NS_FAILED(rv),
    "WriteToRasterImage should consume everything if ReadSegments succeeds or "
    "the image must be in error!");

  return rv;
}

/* static */ already_AddRefed<nsIEventTarget>
RasterImage::GetEventTarget()
{
  return DecodePool::Singleton()->GetEventTarget();
}

nsresult
RasterImage::SetSourceSizeHint(uint32_t sizeHint)
{
  if (sizeHint && StoringSourceData())
    return mSourceData.SetCapacity(sizeHint) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/********* Methods to implement lazy allocation of nsIProperties object *************/
NS_IMETHODIMP
RasterImage::Get(const char *prop, const nsIID & iid, void * *result)
{
  if (!mProperties)
    return NS_ERROR_FAILURE;
  return mProperties->Get(prop, iid, result);
}

NS_IMETHODIMP
RasterImage::Set(const char *prop, nsISupports *value)
{
  if (!mProperties)
    mProperties = do_CreateInstance("@mozilla.org/properties;1");
  if (!mProperties)
    return NS_ERROR_OUT_OF_MEMORY;
  return mProperties->Set(prop, value);
}

NS_IMETHODIMP
RasterImage::Has(const char *prop, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (!mProperties) {
    *_retval = false;
    return NS_OK;
  }
  return mProperties->Has(prop, _retval);
}

NS_IMETHODIMP
RasterImage::Undefine(const char *prop)
{
  if (!mProperties)
    return NS_ERROR_FAILURE;
  return mProperties->Undefine(prop);
}

NS_IMETHODIMP
RasterImage::GetKeys(uint32_t *count, char ***keys)
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

  // We should never discard when we have an active decoder
  NS_ABORT_IF_FALSE(!mDecoder, "Asked to discard with open decoder!");

  // As soon as an image becomes animated, it becomes non-discardable and any
  // timers are cancelled.
  NS_ABORT_IF_FALSE(!mAnim, "Asked to discard for animated image!");

  // For post-operation logging
  int old_frame_count = GetNumFrames();

  // Delete all the decoded frames.
  SurfaceCache::RemoveImage(ImageKey(this));

  // Flag that we no longer have decoded frames for this image
  mDecoded = false;
  mFrameCount = 0;

  // Notify that we discarded
  if (mProgressTracker) {
    mProgressTracker->OnDiscard();
  }

  mDecodeStatus = DecodeStatus::INACTIVE;

  // Log
  PR_LOG(GetCompressedImageAccountingLog(), PR_LOG_DEBUG,
         ("CompressedImageAccounting: discarded uncompressed image "
          "data from RasterImage %p (%s) - %d frames (cached count: %d); "
          "Total Containers: %d, Discardable containers: %d, "
          "Total source bytes: %lld, Source bytes for discardable containers %lld",
          this,
          mSourceDataMimeType.get(),
          old_frame_count,
          GetNumFrames(),
          num_containers,
          num_discardable_containers,
          total_source_bytes,
          discardable_source_bytes));
}

bool
RasterImage::CanDiscard() {
  return mHasSourceData &&       // ...have the source data...
         !mDecoder &&            // Can't discard with an open decoder
         !mAnim;                 // Can never discard animated images
}

// Helper method to determine if we're storing the source data in a buffer
// or just writing it directly to the decoder
bool
RasterImage::StoringSourceData() const {
  return !mTransient;
}


// Sets up a decoder for this image. It is an error to call this function
// when decoding is already in process (ie - when mDecoder is non-null).
nsresult
RasterImage::InitDecoder(bool aDoSizeDecode)
{
  // Ensure that the decoder is not already initialized
  NS_ABORT_IF_FALSE(!mDecoder, "Calling InitDecoder() while already decoding!");

  // We shouldn't be firing up a decoder if we already have the frames decoded
  NS_ABORT_IF_FALSE(!mDecoded, "Calling InitDecoder() but already decoded!");

  // Make sure we actually get size before doing a full decode.
  if (!aDoSizeDecode) {
    NS_ABORT_IF_FALSE(mHasSize, "Must do a size decode before a full decode!");
  }

  // Figure out which decoder we want
  eDecoderType type = GetDecoderType(mSourceDataMimeType.get());
  CONTAINER_ENSURE_TRUE(type != eDecoderType_unknown, NS_IMAGELIB_ERROR_NO_DECODER);

  // Instantiate the appropriate decoder
  switch (type) {
    case eDecoderType_png:
      mDecoder = new nsPNGDecoder(*this);
      break;
    case eDecoderType_gif:
      mDecoder = new nsGIFDecoder2(*this);
      break;
    case eDecoderType_jpeg:
      // If we have all the data we don't want to waste cpu time doing
      // a progressive decode
      mDecoder = new nsJPEGDecoder(*this,
                                   mHasBeenDecoded ? Decoder::SEQUENTIAL :
                                                     Decoder::PROGRESSIVE);
      break;
    case eDecoderType_bmp:
      mDecoder = new nsBMPDecoder(*this);
      break;
    case eDecoderType_ico:
      mDecoder = new nsICODecoder(*this);
      break;
    case eDecoderType_icon:
      mDecoder = new nsIconDecoder(*this);
      break;
    default:
      NS_ABORT_IF_FALSE(0, "Shouldn't get here!");
  }

  // Initialize the decoder
  mDecoder->SetSizeDecode(aDoSizeDecode);
  mDecoder->SetDecodeFlags(mFrameDecodeFlags);
  if (!aDoSizeDecode) {
    // We already have the size; tell the decoder so it can preallocate a
    // frame.  By default, we create an ARGB frame with no offset. If decoders
    // need a different type, they need to ask for it themselves.
    mDecoder->SetSize(mSize, mOrientation);
    mDecoder->NeedNewFrame(0, 0, 0, mSize.width, mSize.height,
                           SurfaceFormat::B8G8R8A8);
    mDecoder->AllocateFrame();
  }
  mDecoder->Init();
  CONTAINER_ENSURE_SUCCESS(mDecoder->GetDecoderError());

  if (!aDoSizeDecode) {
    Telemetry::GetHistogramById(Telemetry::IMAGE_DECODE_COUNT)->Subtract(mDecodeCount);
    mDecodeCount++;
    Telemetry::GetHistogramById(Telemetry::IMAGE_DECODE_COUNT)->Add(mDecodeCount);

    if (mDecodeCount > sMaxDecodeCount) {
      // Don't subtract out 0 from the histogram, because that causes its count
      // to go negative, which is not kosher.
      if (sMaxDecodeCount > 0) {
        Telemetry::GetHistogramById(Telemetry::IMAGE_MAX_DECODE_COUNT)->Subtract(sMaxDecodeCount);
      }
      sMaxDecodeCount = mDecodeCount;
      Telemetry::GetHistogramById(Telemetry::IMAGE_MAX_DECODE_COUNT)->Add(sMaxDecodeCount);
    }
  }

  return NS_OK;
}

// Flushes, closes, and nulls-out a decoder. Cleans up any related decoding
// state. It is an error to call this function when there is no initialized
// decoder.
//
// aReason specifies why the shutdown is happening. If aReason is
// ShutdownReason::DONE, an error is flagged if we didn't get what we should
// have out of the decode. If aReason is ShutdownReason::NOT_NEEDED, we don't
// check this. If aReason is ShutdownReason::FATAL_ERROR, we shut down in error
// mode.
nsresult
RasterImage::ShutdownDecoder(ShutdownReason aReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDecodingMonitor.AssertCurrentThreadIn();

  // Ensure that the decoder is initialized
  NS_ABORT_IF_FALSE(mDecoder, "Calling ShutdownDecoder() with no active decoder!");

  // Figure out what kind of decode we were doing before we get rid of our decoder
  bool wasSizeDecode = mDecoder->IsSizeDecode();

  // Finalize the decoder
  // null out mDecoder, _then_ check for errors on the close (otherwise the
  // error routine might re-invoke ShutdownDecoder)
  nsRefPtr<Decoder> decoder = mDecoder;
  mDecoder = nullptr;

  decoder->Finish(aReason);

  // Kill off our decode request, if it's pending.  (If not, this call is
  // harmless.)
  DecodePool::StopDecoding(this);

  nsresult decoderStatus = decoder->GetDecoderError();
  if (NS_FAILED(decoderStatus)) {
    DoError();
    return decoderStatus;
  }

  // We just shut down the decoder. If we didn't get what we want, but expected
  // to, flag an error
  bool succeeded = wasSizeDecode ? mHasSize : mDecoded;
  if ((aReason == ShutdownReason::DONE) && !succeeded) {
    DoError();
    return NS_ERROR_FAILURE;
  }

  // If we finished a full decode, and we're not meant to be storing source
  // data, stop storing it.
  if (!wasSizeDecode && !StoringSourceData()) {
    mSourceData.Clear();
  }

  return NS_OK;
}

// Writes the data to the decoder, updating the total number of bytes written.
nsresult
RasterImage::WriteToDecoder(const char *aBuffer, uint32_t aCount)
{
  mDecodingMonitor.AssertCurrentThreadIn();

  // We should have a decoder
  NS_ABORT_IF_FALSE(mDecoder, "Trying to write to null decoder!");

  // Write
  nsRefPtr<Decoder> kungFuDeathGrip = mDecoder;
  mDecoder->Write(aBuffer, aCount);

  CONTAINER_ENSURE_SUCCESS(mDecoder->GetDecoderError());

  return NS_OK;
}

// This function is called in situations where it's clear that we want the
// frames in decoded form (Draw, LookupFrame, etc).  If we're completely decoded,
// this method resets the discard timer (if we're discardable), since wanting
// the frames now is a good indicator of wanting them again soon. If we're not
// decoded, this method kicks off asynchronous decoding to generate the frames.
nsresult
RasterImage::WantDecodedFrames(uint32_t aFlags, bool aShouldSyncNotify)
{
  // Request a decode, which does nothing if we're already decoded.
  if (aShouldSyncNotify) {
    // We can sync notify, which means we can also sync decode.
    if (aFlags & FLAG_SYNC_DECODE) {
      return SyncDecode();
    }
    return StartDecoding();
  }

  // We can't sync notify, so do an async decode.
  return RequestDecodeCore(ASYNCHRONOUS);
}

//******************************************************************************
/* void requestDecode() */
NS_IMETHODIMP
RasterImage::RequestDecode()
{
  return RequestDecodeCore(SYNCHRONOUS_NOTIFY);
}

/* void startDecode() */
NS_IMETHODIMP
RasterImage::StartDecoding()
{
  if (!NS_IsMainThread()) {
    return NS_DispatchToMainThread(
      NS_NewRunnableMethod(this, &RasterImage::StartDecoding));
  }
  // Here we are explicitly trading off flashing for responsiveness in the case
  // that we're redecoding an image (see bug 845147).
  return RequestDecodeCore(mHasBeenDecoded ?
    SYNCHRONOUS_NOTIFY : SYNCHRONOUS_NOTIFY_AND_SOME_DECODE);
}

bool
RasterImage::IsDecoded()
{
  return mDecoded || mError;
}

NS_IMETHODIMP
RasterImage::RequestDecodeCore(RequestDecodeType aDecodeType)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  if (mError)
    return NS_ERROR_FAILURE;

  // If we're already decoded, there's nothing to do.
  if (mDecoded)
    return NS_OK;

  // If we have a size decoder open, make sure we get the size
  if (mDecoder && mDecoder->IsSizeDecode()) {
    nsresult rv = DecodePool::Singleton()->DecodeUntilSizeAvailable(this);
    CONTAINER_ENSURE_SUCCESS(rv);

    // If we didn't get the size out of the image, we won't until we get more
    // data, so signal that we want a full decode and give up for now.
    if (!mHasSize) {
      mWantFullDecode = true;
      return NS_OK;
    }
  }

  // If the image is waiting for decode work to be notified, go ahead and do that.
  if (mDecodeStatus == DecodeStatus::WORK_DONE &&
      aDecodeType == SYNCHRONOUS_NOTIFY) {
    ReentrantMonitorAutoEnter lock(mDecodingMonitor);
    nsresult rv = FinishedSomeDecoding();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // If we're fully decoded, we have nothing to do. We need this check after
  // DecodeUntilSizeAvailable and FinishedSomeDecoding because they can result
  // in us finishing an in-progress decode (or kicking off and finishing a
  // synchronous decode if we're already waiting on a full decode).
  if (mDecoded) {
    return NS_OK;
  }

  // If we've already got a full decoder running, and have already decoded
  // some bytes, we have nothing to do if we haven't been asked to do some
  // sync decoding
  if (mDecoder && !mDecoder->IsSizeDecode() && mDecoder->BytesDecoded() > 0 &&
      aDecodeType != SYNCHRONOUS_NOTIFY_AND_SOME_DECODE) {
    return NS_OK;
  }

  ReentrantMonitorAutoEnter lock(mDecodingMonitor);

  // If we don't have any bytes to flush to the decoder, we can't do anything.
  // mDecoder->BytesDecoded() can be bigger than mSourceData.Length() if we're
  // not storing the source data.
  if (mDecoder && mDecoder->BytesDecoded() > mSourceData.Length()) {
    return NS_OK;
  }

  // After acquiring the lock we may have finished some more decoding, so
  // we need to repeat the following three checks after getting the lock.

  // If the image is waiting for decode work to be notified, go ahead and do that.
  if (mDecodeStatus == DecodeStatus::WORK_DONE && aDecodeType != ASYNCHRONOUS) {
    nsresult rv = FinishedSomeDecoding();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // If we're fully decoded, we have nothing to do. We need this check after
  // DecodeUntilSizeAvailable and FinishedSomeDecoding because they can result
  // in us finishing an in-progress decode (or kicking off and finishing a
  // synchronous decode if we're already waiting on a full decode).
  if (mDecoded) {
    return NS_OK;
  }

  // If we've already got a full decoder running, and have already
  // decoded some bytes, we have nothing to do.
  if (mDecoder && !mDecoder->IsSizeDecode() && mDecoder->BytesDecoded() > 0) {
    return NS_OK;
  }

  // If we have a size decode open, interrupt it and shut it down; or if
  // the decoder has different flags than what we need
  if (mDecoder && mDecoder->GetDecodeFlags() != mFrameDecodeFlags) {
    nsresult rv = FinishedSomeDecoding(ShutdownReason::NOT_NEEDED);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  // If we don't have a decoder, create one
  if (!mDecoder) {
    rv = InitDecoder(/* aDoSizeDecode = */ false);
    CONTAINER_ENSURE_SUCCESS(rv);

    rv = FinishedSomeDecoding();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  MOZ_ASSERT(mDecoder);

  // If we've read all the data we have, we're done
  if (mHasSourceData && mDecoder->BytesDecoded() == mSourceData.Length()) {
    return NS_OK;
  }

  // If we can do decoding now, do so.  Small images will decode completely,
  // large images will decode a bit and post themselves to the event loop
  // to finish decoding.
  if (!mDecoded && mHasSourceData && aDecodeType == SYNCHRONOUS_NOTIFY_AND_SOME_DECODE) {
    PROFILER_LABEL_PRINTF("RasterImage", "DecodeABitOf",
      js::ProfileEntry::Category::GRAPHICS, "%s", GetURIString().get());

    DecodePool::Singleton()->DecodeABitOf(this);
    return NS_OK;
  }

  if (!mDecoded) {
    // If we get this far, dispatch the worker. We do this instead of starting
    // any immediate decoding to guarantee that all our decode notifications are
    // dispatched asynchronously, and to ensure we stay responsive.
    DecodePool::Singleton()->RequestDecode(this);
  }

  return NS_OK;
}

// Synchronously decodes as much data as possible
nsresult
RasterImage::SyncDecode()
{
  PROFILER_LABEL_PRINTF("RasterImage", "SyncDecode",
    js::ProfileEntry::Category::GRAPHICS, "%s", GetURIString().get());

  // If we have a size decoder open, make sure we get the size
  if (mDecoder && mDecoder->IsSizeDecode()) {
    nsresult rv = DecodePool::Singleton()->DecodeUntilSizeAvailable(this);
    CONTAINER_ENSURE_SUCCESS(rv);

    // If we didn't get the size out of the image, we won't until we get more
    // data, so signal that we want a full decode and give up for now.
    if (!mHasSize) {
      mWantFullDecode = true;
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  ReentrantMonitorAutoEnter lock(mDecodingMonitor);

  // If the image is waiting for decode work to be notified, go ahead and do that.
  if (mDecodeStatus == DecodeStatus::WORK_DONE) {
    nsresult rv = FinishedSomeDecoding();
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  nsresult rv;

  // If we're decoded already, or decoding until the size was available
  // finished us as a side-effect, no worries
  if (mDecoded)
    return NS_OK;

  // If we don't have any bytes to flush to the decoder, we can't do anything.
  // mDecoder->BytesDecoded() can be bigger than mSourceData.Length() if we're
  // not storing the source data.
  if (mDecoder && mDecoder->BytesDecoded() > mSourceData.Length()) {
    return NS_OK;
  }

  // If we have a decoder open with different flags than what we need, shut it
  // down
  if (mDecoder && mDecoder->GetDecodeFlags() != mFrameDecodeFlags) {
    nsresult rv = FinishedSomeDecoding(ShutdownReason::NOT_NEEDED);
    CONTAINER_ENSURE_SUCCESS(rv);

    if (mDecoded && mAnim) {
      // We can't redecode animated images, so we'll have to give up.
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  // If we don't have a decoder, create one
  if (!mDecoder) {
    rv = InitDecoder(/* aDoSizeDecode = */ false);
    CONTAINER_ENSURE_SUCCESS(rv);
  }

  MOZ_ASSERT(mDecoder);

  // Write everything we have
  rv = DecodeSomeData(mSourceData.Length() - mDecoder->BytesDecoded());
  CONTAINER_ENSURE_SUCCESS(rv);

  rv = FinishedSomeDecoding();
  CONTAINER_ENSURE_SUCCESS(rv);
  
  // If our decoder's still open, there's still work to be done.
  if (mDecoder) {
    DecodePool::Singleton()->RequestDecode(this);
  }

  // All good if no errors!
  return mError ? NS_ERROR_FAILURE : NS_OK;
}

bool
RasterImage::CanScale(GraphicsFilter aFilter,
                      const nsIntSize& aSize,
                      uint32_t aFlags)
{
#ifndef MOZ_ENABLE_SKIA
  // The high-quality scaler requires Skia.
  return false;
#else
  // Check basic requirements: HQ downscaling is enabled, we're decoded, the
  // flags allow us to do it, and a 'good' filter is being used. The flags may
  // ask us not to scale because the caller isn't drawing to the window. If
  // we're drawing to something else (e.g. a canvas) we usually have no way of
  // updating what we've drawn, so HQ scaling is useless.
  if (!gfxPrefs::ImageHQDownscalingEnabled() || !mDecoded ||
      !(aFlags & imgIContainer::FLAG_HIGH_QUALITY_SCALING) ||
      aFilter != GraphicsFilter::FILTER_GOOD) {
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
  if (!SurfaceCache::CanHold(aSize.ToIntSize())) {
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

void
RasterImage::NotifyNewScaledFrame()
{
  if (mProgressTracker) {
    // Send an invalidation so observers will repaint and can take advantage of
    // the new scaled frame if possible.
    nsIntRect rect(0, 0, mSize.width, mSize.height);
    mProgressTracker->SyncNotifyProgress(NoProgress, rect);
  }
}

void
RasterImage::RequestScale(imgFrame* aFrame,
                          uint32_t aFlags,
                          const nsIntSize& aSize)
{
  // We don't scale frames which aren't fully decoded.
  if (!aFrame->ImageComplete()) {
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

void
RasterImage::DrawWithPreDownscaleIfNeeded(DrawableFrameRef&& aFrameRef,
                                          gfxContext* aContext,
                                          const nsIntSize& aSize,
                                          const ImageRegion& aRegion,
                                          GraphicsFilter aFilter,
                                          uint32_t aFlags)
{
  DrawableFrameRef frameRef;

  if (CanScale(aFilter, aSize, aFlags)) {
    frameRef =
      SurfaceCache::Lookup(ImageKey(this),
                           RasterSurfaceKey(aSize.ToIntSize(),
                                            DecodeFlags(aFlags),
                                            0));
    if (!frameRef) {
      // We either didn't have a matching scaled frame or the OS threw it away.
      // Request a new one so we'll be ready next time. For now, we'll fall back
      // to aFrameRef below.
      RequestScale(aFrameRef.get(), aFlags, aSize);
    }
    if (frameRef && !frameRef->ImageComplete()) {
      frameRef.reset();  // We're still scaling, so we can't use this yet.
    }
  }

  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  ImageRegion region(aRegion);
  if (!frameRef) {
    frameRef = Move(aFrameRef);
  }

  // By now we may have a frame with the requested size. If not, we need to
  // adjust the drawing parameters accordingly.
  IntSize finalSize = frameRef->GetImageSize();
  if (ThebesIntSize(finalSize) != aSize) {
    gfx::Size scale(double(aSize.width) / finalSize.width,
                    double(aSize.height) / finalSize.height);
    aContext->Multiply(gfxMatrix::Scaling(scale.width, scale.height));
    region.Scale(1.0 / scale.width, 1.0 / scale.height);
  }

  frameRef->Draw(aContext, region, aFilter, aFlags);
}

//******************************************************************************
/* [noscript] void draw(in gfxContext aContext,
 *                      in gfxGraphicsFilter aFilter,
 *                      [const] in gfxMatrix aUserSpaceToImageSpace,
 *                      [const] in gfxRect aFill,
 *                      [const] in nsIntRect aSubimage,
 *                      [const] in nsIntSize aViewportSize,
 *                      [const] in SVGImageContext aSVGContext,
 *                      in uint32_t aWhichFrame,
 *                      in uint32_t aFlags); */
NS_IMETHODIMP
RasterImage::Draw(gfxContext* aContext,
                  const nsIntSize& aSize,
                  const ImageRegion& aRegion,
                  uint32_t aWhichFrame,
                  GraphicsFilter aFilter,
                  const Maybe<SVGImageContext>& /*aSVGContext - ignored*/,
                  uint32_t aFlags)
{
  if (aWhichFrame > FRAME_MAX_VALUE)
    return NS_ERROR_INVALID_ARG;

  if (mError)
    return NS_ERROR_FAILURE;

  // Illegal -- you can't draw with non-default decode flags.
  // (Disabling colorspace conversion might make sense to allow, but
  // we don't currently.)
  if ((aFlags & DECODE_FLAGS_MASK) != DECODE_FLAGS_DEFAULT)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aContext);

  if (IsUnlocked() && mProgressTracker) {
    mProgressTracker->OnUnlockedDraw();
  }

  // We use !mDecoded && mHasSourceData to mean discarded.
  if (!mDecoded && mHasSourceData) {
    mDrawStartTime = TimeStamp::Now();
  }

  // If a synchronous draw is requested, flush anything that might be sitting around
  if (aFlags & FLAG_SYNC_DECODE) {
    nsresult rv = SyncDecode();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // XXX(seth): For now, we deliberately don't look up a frame of size aSize
  // (though DrawWithPreDownscaleIfNeeded will do so later). It doesn't make
  // sense to do so until we support downscale-during-decode. Right now we need
  // to make sure that we always touch an mSize-sized frame so that we have
  // something to HQ scale.
  DrawableFrameRef ref = LookupFrame(GetRequestedFrameIndex(aWhichFrame),
                                     mSize, aFlags);
  if (!ref) {
    // Getting the frame (above) touches the image and kicks off decoding.
    return NS_OK;
  }

  DrawWithPreDownscaleIfNeeded(Move(ref), aContext, aSize,
                               aRegion, aFilter, aFlags);

  if (mDecoded && !mDrawStartTime.IsNull()) {
      TimeDuration drawLatency = TimeStamp::Now() - mDrawStartTime;
      Telemetry::Accumulate(Telemetry::IMAGE_DECODE_ON_DRAW_LATENCY, int32_t(drawLatency.ToMicroseconds()));
      // clear the value of mDrawStartTime
      mDrawStartTime = TimeStamp();
  }

  return NS_OK;
}

//******************************************************************************
/* void lockImage() */
NS_IMETHODIMP
RasterImage::LockImage()
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Main thread to encourage serialization with UnlockImage");
  if (mError)
    return NS_ERROR_FAILURE;

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
  if (mError)
    return NS_ERROR_FAILURE;

  // It's an error to call this function if the lock count is 0
  NS_ABORT_IF_FALSE(mLockCount > 0,
                    "Calling UnlockImage with mLockCount == 0!");
  if (mLockCount == 0)
    return NS_ERROR_ABORT;

  // Decrement our lock count
  mLockCount--;

  // Unlock this image's surfaces in the SurfaceCache.
  if (mLockCount == 0 ) {
    SurfaceCache::UnlockImage(ImageKey(this));
  }

  // If we've decoded this image once before, we're currently decoding again,
  // and our lock count is now zero (so nothing is forcing us to keep the
  // decoded data around), try to cancel the decode and throw away whatever
  // we've decoded.
  if (mHasBeenDecoded && mDecoder && mLockCount == 0 && !mAnim) {
    PR_LOG(GetCompressedImageAccountingLog(), PR_LOG_DEBUG,
           ("RasterImage[0x%p] canceling decode because image "
            "is now unlocked.", this));
    ReentrantMonitorAutoEnter lock(mDecodingMonitor);
    FinishedSomeDecoding(ShutdownReason::NOT_NEEDED);
    return NS_OK;
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
      mDecoded &&          // ...and have something to discard.
      CanDiscard()) {
    Discard();
  }

  return NS_OK;
}

// Flushes up to aMaxBytes to the decoder.
nsresult
RasterImage::DecodeSomeData(size_t aMaxBytes)
{
  MOZ_ASSERT(mDecoder, "Should have a decoder");

  mDecodingMonitor.AssertCurrentThreadIn();

  // If we have nothing else to decode, return.
  if (mDecoder->BytesDecoded() == mSourceData.Length()) {
    return NS_OK;
  }

  MOZ_ASSERT(mDecoder->BytesDecoded() < mSourceData.Length());

  // write the proper amount of data
  size_t bytesToDecode = min(aMaxBytes,
                             mSourceData.Length() - mDecoder->BytesDecoded());
  return WriteToDecoder(mSourceData.Elements() + mDecoder->BytesDecoded(),
                        bytesToDecode);

}

// There are various indicators that tell us we're finished with the decode
// task at hand and can shut down the decoder.
//
// This method may not be called if there is no decoder.
bool
RasterImage::IsDecodeFinished()
{
  // Precondition
  mDecodingMonitor.AssertCurrentThreadIn();
  MOZ_ASSERT(mDecoder, "Should have a decoder");

  // The decode is complete if we got what we wanted.
  if (mDecoder->IsSizeDecode()) {
    if (mDecoder->HasSize()) {
      return true;
    }
  } else if (mDecoder->GetDecodeDone()) {
    return true;
  }

  // Otherwise, if we have all the source data and wrote all the source data,
  // we're done.
  //
  // (NB - This can be the case even for non-erroneous images because
  // Decoder::GetDecodeDone() might not return true until after we call
  // Decoder::Finish() in ShutdownDecoder())
  if (mHasSourceData && (mDecoder->BytesDecoded() == mSourceData.Length())) {
    return true;
  }

  // If we get here, assume it's not finished.
  return false;
}

// Indempotent error flagging routine. If a decoder is open, shuts it down.
void
RasterImage::DoError()
{
  // If we've flagged an error before, we have nothing to do
  if (mError)
    return;

  // We can't safely handle errors off-main-thread, so dispatch a worker to do it.
  if (!NS_IsMainThread()) {
    HandleErrorWorker::DispatchIfNeeded(this);
    return;
  }

  // Calling FinishedSomeDecoding requires us to be in the decoding monitor.
  ReentrantMonitorAutoEnter lock(mDecodingMonitor);

  // If we're mid-decode, shut down the decoder.
  if (mDecoder) {
    FinishedSomeDecoding(ShutdownReason::FATAL_ERROR);
  }

  // Put the container in an error state.
  mError = true;

  // Log our error
  LOG_CONTAINER_ERROR;
}

/* static */ void
RasterImage::HandleErrorWorker::DispatchIfNeeded(RasterImage* aImage)
{
  if (!aImage->mPendingError) {
    aImage->mPendingError = true;
    nsRefPtr<HandleErrorWorker> worker = new HandleErrorWorker(aImage);
    NS_DispatchToMainThread(worker);
  }
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
RasterImage::WriteToRasterImage(nsIInputStream* /* unused */,
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
  nsresult rv = image->AddSourceData(aFromRawSegment, aCount);
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
RasterImage::GetFramesNotified(uint32_t *aFramesNotified)
{
  NS_ENSURE_ARG_POINTER(aFramesNotified);

  *aFramesNotified = mFramesNotified;

  return NS_OK;
}
#endif

nsresult
RasterImage::RequestDecodeIfNeeded(nsresult aStatus,
                                   ShutdownReason aReason,
                                   bool aDone,
                                   bool aWasSize)
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we were a size decode and a full decode was requested, now's the time.
  if (NS_SUCCEEDED(aStatus) &&
      aReason == ShutdownReason::DONE &&
      aDone &&
      aWasSize &&
      mWantFullDecode) {
    mWantFullDecode = false;

    // If we're not meant to be storing source data and we just got the size,
    // we need to synchronously flush all the data we got to a full decoder.
    // When that decoder is shut down, we'll also clear our source data.
    return StoringSourceData() ? RequestDecode()
                               : SyncDecode();
  }

  // We don't need a full decode right now, so just return the existing status.
  return aStatus;
}

nsresult
RasterImage::FinishedSomeDecoding(ShutdownReason aReason /* = ShutdownReason::DONE */,
                                  Progress aProgress /* = NoProgress */)
{
  MOZ_ASSERT(NS_IsMainThread());

  mDecodingMonitor.AssertCurrentThreadIn();

  // Ensure that, if the decoder is the last reference to the image, we don't
  // destroy it by destroying the decoder.
  nsRefPtr<RasterImage> image(this);

  bool done = false;
  bool wasSize = false;
  bool wasDefaultFlags = false;
  nsIntRect invalidRect;
  nsresult rv = NS_OK;
  Progress progress = aProgress;

  if (image->mDecoder) {
    invalidRect = image->mDecoder->TakeInvalidRect();
    progress |= image->mDecoder->TakeProgress();
    wasDefaultFlags = image->mDecoder->GetDecodeFlags() == DECODE_FLAGS_DEFAULT;

    if (!image->mDecoder->IsSizeDecode() && image->mDecoder->ChunkCount()) {
      Telemetry::Accumulate(Telemetry::IMAGE_DECODE_CHUNKS,
                            image->mDecoder->ChunkCount());
    }

    if (!image->mHasSize && image->mDecoder->HasSize()) {
      image->mDecoder->SetSizeOnImage();
    }

    // If the decode finished, or we're specifically being told to shut down,
    // tell the image and shut down the decoder.
    if (image->IsDecodeFinished() || aReason != ShutdownReason::DONE) {
      done = true;

      // Hold on to a reference to the decoder until we're done with it
      nsRefPtr<Decoder> decoder = image->mDecoder;

      wasSize = decoder->IsSizeDecode();

      // Do some telemetry if this isn't a size decode.
      if (!wasSize) {
        Telemetry::Accumulate(Telemetry::IMAGE_DECODE_TIME,
                              int32_t(decoder->DecodeTime().ToMicroseconds()));

        // We record the speed for only some decoders. The rest have
        // SpeedHistogram return HistogramCount.
        Telemetry::ID id = decoder->SpeedHistogram();
        if (id < Telemetry::HistogramCount) {
          int32_t KBps = int32_t(decoder->BytesDecoded() /
                                 (1024 * decoder->DecodeTime().ToSeconds()));
          Telemetry::Accumulate(id, KBps);
        }
      }

      // We need to shut down the decoder first, in order to ensure all
      // decoding routines have been finished.
      rv = image->ShutdownDecoder(aReason);
      if (NS_FAILED(rv)) {
        image->DoError();
      }

      // If there were any final changes, grab them.
      invalidRect.Union(decoder->TakeInvalidRect());
      progress |= decoder->TakeProgress();
    }
  }

  if (GetCurrentFrameIndex() > 0) {
    // Don't send invalidations for animated frames after the first; let
    // RequestRefresh take care of that.
    invalidRect = nsIntRect();
  }
  if (mHasBeenDecoded && !invalidRect.IsEmpty()) {
    // Don't send partial invalidations if we've been decoded before.
    invalidRect = mDecoded ? GetFirstFrameRect()
                           : nsIntRect();
  }
  if (!invalidRect.IsEmpty() && wasDefaultFlags) {
    // Update our image container since we're invalidating.
    UpdateImageContainer();
  }

  if (mNotifying) {
    // Accumulate the progress changes. We don't permit recursive notifications
    // because they cause subtle concurrency bugs, so we'll delay sending out
    // the notifications until we pop back to the lowest invocation of
    // FinishedSomeDecoding on the stack.
    mNotifyProgress |= progress;
    mNotifyInvalidRect.Union(invalidRect);
  } else {
    MOZ_ASSERT(mNotifyProgress == NoProgress && mNotifyInvalidRect.IsEmpty(),
               "Shouldn't have an accumulated change at this point");

    progress = image->mProgressTracker->Difference(progress);

    while (progress != NoProgress || !invalidRect.IsEmpty()) {
      // Tell the observers what happened.
      mNotifying = true;
      image->mProgressTracker->SyncNotifyProgress(progress, invalidRect);
      mNotifying = false;

      // Gather any progress changes that may have occurred as a result of sending
      // out the previous notifications. If there were any, we'll send out
      // notifications for them next.
      progress = image->mProgressTracker->Difference(mNotifyProgress);
      mNotifyProgress = NoProgress;

      invalidRect = mNotifyInvalidRect;
      mNotifyInvalidRect = nsIntRect();
    }
  }

  return RequestDecodeIfNeeded(rv, aReason, done, wasSize);
}

already_AddRefed<imgIContainer>
RasterImage::Unwrap()
{
  nsCOMPtr<imgIContainer> self(this);
  return self.forget();
}

nsIntSize
RasterImage::OptimalImageSizeForDest(const gfxSize& aDest, uint32_t aWhichFrame,
                                     GraphicsFilter aFilter, uint32_t aFlags)
{
  MOZ_ASSERT(aDest.width >= 0 || ceil(aDest.width) <= INT32_MAX ||
             aDest.height >= 0 || ceil(aDest.height) <= INT32_MAX,
             "Unexpected destination size");

  if (mSize.IsEmpty() || aDest.IsEmpty()) {
    return nsIntSize(0, 0);
  }

  nsIntSize destSize(ceil(aDest.width), ceil(aDest.height));

  if (CanScale(aFilter, destSize, aFlags)) {
    DrawableFrameRef frameRef =
      SurfaceCache::Lookup(ImageKey(this),
                           RasterSurfaceKey(destSize.ToIntSize(),
                                            DecodeFlags(aFlags),
                                            0));

    if (frameRef && frameRef->ImageComplete()) {
        return destSize;  // We have an existing HQ scale for this size.
    }
    if (!frameRef) {
      // We could HQ scale to this size, but we haven't. Request a scale now.
      frameRef = LookupFrame(GetRequestedFrameIndex(aWhichFrame),
                             mSize, aFlags);
      if (frameRef) {
        RequestScale(frameRef.get(), aFlags, destSize);
      }
    }
  }

  // We either can't HQ scale to this size or the scaled version isn't ready
  // yet. Use our intrinsic size for now.
  return mSize;
}

} // namespace image
} // namespace mozilla
