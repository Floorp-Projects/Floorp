/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgFrame.h"
#include "ImageRegion.h"
#include "ShutdownTracker.h"

#include "prenv.h"

#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"

#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/SourceSurfaceRawData.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/SourceSurfaceVolatileData.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "nsMargin.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace gfx;

namespace image {

static void
ScopedMapRelease(void* aMap)
{
  delete static_cast<DataSourceSurface::ScopedMap*>(aMap);
}

static int32_t
VolatileSurfaceStride(const IntSize& size, SurfaceFormat format)
{
  // Stride must be a multiple of four or cairo will complain.
  return (size.width * BytesPerPixel(format) + 0x3) & ~0x3;
}

static already_AddRefed<DataSourceSurface>
CreateLockedSurface(DataSourceSurface *aSurface,
                    const IntSize& size,
                    SurfaceFormat format)
{
  // Shared memory is never released until the surface itself is released
  if (aSurface->GetType() == SurfaceType::DATA_SHARED) {
    RefPtr<DataSourceSurface> surf(aSurface);
    return surf.forget();
  }

  DataSourceSurface::ScopedMap* smap =
    new DataSourceSurface::ScopedMap(aSurface, DataSourceSurface::READ_WRITE);
  if (smap->IsMapped()) {
    // The ScopedMap is held by this DataSourceSurface.
    RefPtr<DataSourceSurface> surf =
      Factory::CreateWrappingDataSourceSurface(smap->GetData(),
                                               aSurface->Stride(),
                                               size,
                                               format,
                                               &ScopedMapRelease,
                                               static_cast<void*>(smap));
    if (surf) {
      return surf.forget();
    }
  }

  delete smap;
  return nullptr;
}

static bool
ShouldUseHeap(const IntSize& aSize,
              int32_t aStride,
              bool aIsAnimated)
{
  // On some platforms (i.e. Android), a volatile buffer actually keeps a file
  // handle active. We would like to avoid too many since we could easily
  // exhaust the pool. However, other platforms we do not have the file handle
  // problem, and additionally we may avoid a superfluous memset since the
  // volatile memory starts out as zero-filled. Hence the knobs below.

  // For as long as an animated image is retained, its frames will never be
  // released to let the OS purge volatile buffers.
  if (aIsAnimated && gfxPrefs::ImageMemAnimatedUseHeap()) {
    return true;
  }

  // Lets us avoid too many small images consuming all of the handles. The
  // actual allocation checks for overflow.
  int32_t bufferSize = (aStride * aSize.width) / 1024;
  if (bufferSize < gfxPrefs::ImageMemVolatileMinThresholdKB()) {
    return true;
  }

  return false;
}

static already_AddRefed<DataSourceSurface>
AllocateBufferForImage(const IntSize& size,
                       SurfaceFormat format,
                       bool aIsAnimated = false)
{
  int32_t stride = VolatileSurfaceStride(size, format);

  if (ShouldUseHeap(size, stride, aIsAnimated)) {
    RefPtr<SourceSurfaceAlignedRawData> newSurf =
      new SourceSurfaceAlignedRawData();
    if (newSurf->Init(size, format, false, 0, stride)) {
      return newSurf.forget();
    }
  }

  if (!aIsAnimated && gfxVars::GetUseWebRenderOrDefault()
                   && gfxPrefs::ImageMemShared()) {
    RefPtr<SourceSurfaceSharedData> newSurf = new SourceSurfaceSharedData();
    if (newSurf->Init(size, stride, format)) {
      return newSurf.forget();
    }
  } else {
    RefPtr<SourceSurfaceVolatileData> newSurf= new SourceSurfaceVolatileData();
    if (newSurf->Init(size, stride, format)) {
      return newSurf.forget();
    }
  }
  return nullptr;
}

static bool
ClearSurface(DataSourceSurface* aSurface, const IntSize& aSize, SurfaceFormat aFormat)
{
  int32_t stride = aSurface->Stride();
  uint8_t* data = aSurface->GetData();
  MOZ_ASSERT(data);

  if (aFormat == SurfaceFormat::B8G8R8X8) {
    // Skia doesn't support RGBX surfaces, so ensure the alpha value is set
    // to opaque white. While it would be nice to only do this for Skia,
    // imgFrame can run off main thread and past shutdown where
    // we might not have gfxPlatform, so just memset everytime instead.
    memset(data, 0xFF, stride * aSize.height);
  } else if (aSurface->OnHeap()) {
    // We only need to memset it if the buffer was allocated on the heap.
    // Otherwise, it's allocated via mmap and refers to a zeroed page and will
    // be COW once it's written to.
    memset(data, 0, stride * aSize.height);
  }

  return true;
}

// Returns true if an image of aWidth x aHeight is allowed and legal.
static bool
AllowedImageSize(int32_t aWidth, int32_t aHeight)
{
  // reject over-wide or over-tall images
  const int32_t k64KLimit = 0x0000FFFF;
  if (MOZ_UNLIKELY(aWidth > k64KLimit || aHeight > k64KLimit )) {
    NS_WARNING("image too big");
    return false;
  }

  // protect against invalid sizes
  if (MOZ_UNLIKELY(aHeight <= 0 || aWidth <= 0)) {
    return false;
  }

  // check to make sure we don't overflow a 32-bit
  CheckedInt32 requiredBytes = CheckedInt32(aWidth) * CheckedInt32(aHeight) * 4;
  if (MOZ_UNLIKELY(!requiredBytes.isValid())) {
    NS_WARNING("width or height too large");
    return false;
  }
  return true;
}

static bool AllowedImageAndFrameDimensions(const nsIntSize& aImageSize,
                                           const nsIntRect& aFrameRect)
{
  if (!AllowedImageSize(aImageSize.width, aImageSize.height)) {
    return false;
  }
  if (!AllowedImageSize(aFrameRect.Width(), aFrameRect.Height())) {
    return false;
  }
  nsIntRect imageRect(0, 0, aImageSize.width, aImageSize.height);
  if (!imageRect.Contains(aFrameRect)) {
    NS_WARNING("Animated image frame does not fit inside bounds of image");
  }
  return true;
}

imgFrame::imgFrame()
  : mMonitor("imgFrame")
  , mDecoded(0, 0, 0, 0)
  , mLockCount(0)
  , mTimeout(FrameTimeout::FromRawMilliseconds(100))
  , mDisposalMethod(DisposalMethod::NOT_SPECIFIED)
  , mBlendMethod(BlendMethod::OVER)
  , mAborted(false)
  , mFinished(false)
  , mOptimizable(false)
  , mPalettedImageData(nullptr)
  , mPaletteDepth(0)
  , mNonPremult(false)
  , mCompositingFailed(false)
{
}

imgFrame::~imgFrame()
{
#ifdef DEBUG
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mAborted || AreAllPixelsWritten());
  MOZ_ASSERT(mAborted || mFinished);
#endif

  free(mPalettedImageData);
  mPalettedImageData = nullptr;
}

nsresult
imgFrame::InitForDecoder(const nsIntSize& aImageSize,
                         const nsIntRect& aRect,
                         SurfaceFormat aFormat,
                         uint8_t aPaletteDepth /* = 0 */,
                         bool aNonPremult /* = false */,
                         bool aIsAnimated /* = false */)
{
  // Assert for properties that should be verified by decoders,
  // warn for properties related to bad content.
  if (!AllowedImageAndFrameDimensions(aImageSize, aRect)) {
    NS_WARNING("Should have legal image size");
    mAborted = true;
    return NS_ERROR_FAILURE;
  }

  mImageSize = aImageSize;
  mFrameRect = aRect;

  // We only allow a non-trivial frame rect (i.e., a frame rect that doesn't
  // cover the entire image) for paletted animation frames. We never draw those
  // frames directly; we just use FrameAnimator to composite them and produce a
  // BGRA surface that we actually draw. We enforce this here to make sure that
  // imgFrame::Draw(), which is responsible for drawing all other kinds of
  // frames, never has to deal with a non-trivial frame rect.
  if (aPaletteDepth == 0 &&
      !mFrameRect.IsEqualEdges(IntRect(IntPoint(), mImageSize))) {
    MOZ_ASSERT_UNREACHABLE("Creating a non-paletted imgFrame with a "
                           "non-trivial frame rect");
    return NS_ERROR_FAILURE;
  }

  mFormat = aFormat;
  mPaletteDepth = aPaletteDepth;
  mNonPremult = aNonPremult;

  if (aPaletteDepth != 0) {
    // We're creating for a paletted image.
    if (aPaletteDepth > 8) {
      NS_WARNING("Should have legal palette depth");
      NS_ERROR("This Depth is not supported");
      mAborted = true;
      return NS_ERROR_FAILURE;
    }

    // Use the fallible allocator here. Paletted images always use 1 byte per
    // pixel, so calculating the amount of memory we need is straightforward.
    size_t dataSize = PaletteDataLength() + mFrameRect.Area();
    mPalettedImageData = static_cast<uint8_t*>(calloc(dataSize, sizeof(uint8_t)));
    if (!mPalettedImageData) {
      NS_WARNING("Call to calloc for paletted image data should succeed");
    }
    NS_ENSURE_TRUE(mPalettedImageData, NS_ERROR_OUT_OF_MEMORY);
  } else {
    MOZ_ASSERT(!mLockedSurface, "Called imgFrame::InitForDecoder() twice?");

    mRawSurface = AllocateBufferForImage(mFrameRect.Size(), mFormat, aIsAnimated);
    if (!mRawSurface) {
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mLockedSurface = CreateLockedSurface(mRawSurface, mFrameRect.Size(), mFormat);
    if (!mLockedSurface) {
      NS_WARNING("Failed to create LockedSurface");
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!ClearSurface(mRawSurface, mFrameRect.Size(), mFormat)) {
      NS_WARNING("Could not clear allocated buffer");
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

nsresult
imgFrame::InitWithDrawable(gfxDrawable* aDrawable,
                           const nsIntSize& aSize,
                           const SurfaceFormat aFormat,
                           SamplingFilter aSamplingFilter,
                           uint32_t aImageFlags,
                           gfx::BackendType aBackend)
{
  // Assert for properties that should be verified by decoders,
  // warn for properties related to bad content.
  if (!AllowedImageSize(aSize.width, aSize.height)) {
    NS_WARNING("Should have legal image size");
    mAborted = true;
    return NS_ERROR_FAILURE;
  }

  mImageSize = aSize;
  mFrameRect = IntRect(IntPoint(0, 0), aSize);

  mFormat = aFormat;
  mPaletteDepth = 0;

  RefPtr<DrawTarget> target;

  bool canUseDataSurface = Factory::DoesBackendSupportDataDrawtarget(aBackend);
  if (canUseDataSurface) {
    // It's safe to use data surfaces for content on this platform, so we can
    // get away with using volatile buffers.
    MOZ_ASSERT(!mLockedSurface, "Called imgFrame::InitWithDrawable() twice?");

    mRawSurface = AllocateBufferForImage(mFrameRect.Size(), mFormat);
    if (!mRawSurface) {
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mLockedSurface = CreateLockedSurface(mRawSurface, mFrameRect.Size(), mFormat);
    if (!mLockedSurface) {
      NS_WARNING("Failed to create LockedSurface");
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!ClearSurface(mRawSurface, mFrameRect.Size(), mFormat)) {
      NS_WARNING("Could not clear allocated buffer");
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    target = gfxPlatform::CreateDrawTargetForData(
                            mLockedSurface->GetData(),
                            mFrameRect.Size(),
                            mLockedSurface->Stride(),
                            mFormat);
  } else {
    // We can't use data surfaces for content, so we'll create an offscreen
    // surface instead.  This means if someone later calls RawAccessRef(), we
    // may have to do an expensive readback, but we warned callers about that in
    // the documentation for this method.
    MOZ_ASSERT(!mOptSurface, "Called imgFrame::InitWithDrawable() twice?");

    if (gfxPlatform::GetPlatform()->SupportsAzureContentForType(aBackend)) {
      target = gfxPlatform::GetPlatform()->
        CreateDrawTargetForBackend(aBackend, mFrameRect.Size(), mFormat);
    } else {
      target = gfxPlatform::GetPlatform()->
        CreateOffscreenContentDrawTarget(mFrameRect.Size(), mFormat);
    }
  }

  if (!target || !target->IsValid()) {
    mAborted = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Draw using the drawable the caller provided.
  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(target);
  MOZ_ASSERT(ctx);  // Already checked the draw target above.
  gfxUtils::DrawPixelSnapped(ctx, aDrawable, SizeDouble(mFrameRect.Size()),
                             ImageRegion::Create(ThebesRect(mFrameRect)),
                             mFormat, aSamplingFilter, aImageFlags);

  if (canUseDataSurface && !mLockedSurface) {
    NS_WARNING("Failed to create VolatileDataSourceSurface");
    mAborted = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!canUseDataSurface) {
    // We used an offscreen surface, which is an "optimized" surface from
    // imgFrame's perspective.
    mOptSurface = target->Snapshot();
  } else {
    FinalizeSurface();
  }

  // If we reach this point, we should regard ourselves as complete.
  mDecoded = GetRect();
  mFinished = true;

#ifdef DEBUG
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(AreAllPixelsWritten());
#endif

  return NS_OK;
}

nsresult
imgFrame::Optimize(DrawTarget* aTarget)
{
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();

  if (mLockCount > 0 || !mOptimizable) {
    // Don't optimize right now.
    return NS_OK;
  }

  // Check whether image optimization is disabled -- not thread safe!
  static bool gDisableOptimize = false;
  static bool hasCheckedOptimize = false;
  if (!hasCheckedOptimize) {
    if (PR_GetEnv("MOZ_DISABLE_IMAGE_OPTIMIZE")) {
      gDisableOptimize = true;
    }
    hasCheckedOptimize = true;
  }

  // Don't optimize during shutdown because gfxPlatform may not be available.
  if (ShutdownTracker::ShutdownHasStarted()) {
    return NS_OK;
  }

  if (gDisableOptimize) {
    return NS_OK;
  }

  if (mPalettedImageData || mOptSurface) {
    return NS_OK;
  }

  // XXX(seth): It's currently unclear if there's any reason why we can't
  // optimize non-premult surfaces. We should look into removing this.
  if (mNonPremult) {
    return NS_OK;
  }

  mOptSurface = gfxPlatform::GetPlatform()
    ->ScreenReferenceDrawTarget()->OptimizeSourceSurface(mLockedSurface);
  if (mOptSurface == mLockedSurface) {
    mOptSurface = nullptr;
  }

  if (mOptSurface) {
    // There's no reason to keep our original surface around if we have an
    // optimized surface. Release our reference to it. This will leave
    // |mLockedSurface| as the only thing keeping it alive, so it'll get freed
    // below.
    mRawSurface = nullptr;
  }

  // Release all strong references to the surface's memory. If the underlying
  // surface is volatile, this will allow the operating system to free the
  // memory if it needs to.
  mLockedSurface = nullptr;
  mOptimizable = false;

  return NS_OK;
}

DrawableFrameRef
imgFrame::DrawableRef()
{
  return DrawableFrameRef(this);
}

RawAccessFrameRef
imgFrame::RawAccessRef()
{
  return RawAccessFrameRef(this);
}

void
imgFrame::SetRawAccessOnly()
{
  AssertImageDataLocked();

  // Lock our data and throw away the key.
  LockImageData();
}


imgFrame::SurfaceWithFormat
imgFrame::SurfaceForDrawing(bool               aDoPartialDecode,
                            bool               aDoTile,
                            ImageRegion&       aRegion,
                            SourceSurface*     aSurface)
{
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();

  if (!aDoPartialDecode) {
    return SurfaceWithFormat(new gfxSurfaceDrawable(aSurface, mImageSize),
                             mFormat);
  }

  gfxRect available = gfxRect(mDecoded.X(), mDecoded.Y(), mDecoded.Width(),
                              mDecoded.Height());

  if (aDoTile) {
    // Create a temporary surface.
    // Give this surface an alpha channel because there are
    // transparent pixels in the padding or undecoded area
    RefPtr<DrawTarget> target =
      gfxPlatform::GetPlatform()->
        CreateOffscreenContentDrawTarget(mImageSize, SurfaceFormat::B8G8R8A8);
    if (!target) {
      return SurfaceWithFormat();
    }

    SurfacePattern pattern(aSurface,
                           aRegion.GetExtendMode(),
                           Matrix::Translation(mDecoded.X(), mDecoded.Y()));
    target->FillRect(ToRect(aRegion.Intersect(available).Rect()), pattern);

    RefPtr<SourceSurface> newsurf = target->Snapshot();
    return SurfaceWithFormat(new gfxSurfaceDrawable(newsurf, mImageSize),
                             target->GetFormat());
  }

  // Not tiling, and we have a surface, so we can account for
  // a partial decode just by twiddling parameters.
  aRegion = aRegion.Intersect(available);
  IntSize availableSize(mDecoded.Width(), mDecoded.Height());

  return SurfaceWithFormat(new gfxSurfaceDrawable(aSurface, availableSize),
                           mFormat);
}

bool imgFrame::Draw(gfxContext* aContext, const ImageRegion& aRegion,
                    SamplingFilter aSamplingFilter, uint32_t aImageFlags,
                    float aOpacity)
{
  AUTO_PROFILER_LABEL("imgFrame::Draw", GRAPHICS);

  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(!aRegion.Rect().IsEmpty(), "Drawing empty region!");
  NS_ASSERTION(!aRegion.IsRestricted() ||
               !aRegion.Rect().Intersect(aRegion.Restriction()).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");
  MOZ_ASSERT(mFrameRect.IsEqualEdges(IntRect(IntPoint(), mImageSize)),
             "Directly drawing an image with a non-trivial frame rect!");

  if (mPalettedImageData) {
    MOZ_ASSERT_UNREACHABLE("Directly drawing a paletted image!");
    return false;
  }

  MonitorAutoLock lock(mMonitor);

  // Possibly convert this image into a GPU texture, this may also cause our
  // mLockedSurface to be released and the OS to release the underlying memory.
  Optimize(aContext->GetDrawTarget());

  bool doPartialDecode = !AreAllPixelsWritten();

  RefPtr<SourceSurface> surf = GetSourceSurfaceInternal();
  if (!surf) {
    return false;
  }

  gfxRect imageRect(0, 0, mImageSize.width, mImageSize.height);
  bool doTile = !imageRect.Contains(aRegion.Rect()) &&
                !(aImageFlags & imgIContainer::FLAG_CLAMP);

  ImageRegion region(aRegion);
  SurfaceWithFormat surfaceResult =
    SurfaceForDrawing(doPartialDecode, doTile, region, surf);

  if (surfaceResult.IsValid()) {
    gfxUtils::DrawPixelSnapped(aContext, surfaceResult.mDrawable,
                               imageRect.Size(), region, surfaceResult.mFormat,
                               aSamplingFilter, aImageFlags, aOpacity);
  }

  return true;
}

nsresult
imgFrame::ImageUpdated(const nsIntRect& aUpdateRect)
{
  MonitorAutoLock lock(mMonitor);
  return ImageUpdatedInternal(aUpdateRect);
}

nsresult
imgFrame::ImageUpdatedInternal(const nsIntRect& aUpdateRect)
{
  mMonitor.AssertCurrentThreadOwns();

  mDecoded.UnionRect(mDecoded, aUpdateRect);

  // Clamp to the frame rect to ensure that decoder bugs don't result in a
  // decoded rect that extends outside the bounds of the frame rect.
  mDecoded.IntersectRect(mDecoded, mFrameRect);

  // Update our invalidation counters for any consumers watching for changes
  // in the surface.
  if (mRawSurface) {
    mRawSurface->Invalidate();
  }
  if (mLockedSurface && mRawSurface != mLockedSurface) {
    mLockedSurface->Invalidate();
  }
  return NS_OK;
}

void
imgFrame::Finish(Opacity aFrameOpacity /* = Opacity::SOME_TRANSPARENCY */,
                 DisposalMethod aDisposalMethod /* = DisposalMethod::KEEP */,
                 FrameTimeout aTimeout
                   /* = FrameTimeout::FromRawMilliseconds(0) */,
                 BlendMethod aBlendMethod /* = BlendMethod::OVER */,
                 const Maybe<IntRect>& aBlendRect /* = Nothing() */,
                 bool aFinalize /* = true */)
{
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mLockCount > 0, "Image data should be locked");

  mDisposalMethod = aDisposalMethod;
  mTimeout = aTimeout;
  mBlendMethod = aBlendMethod;
  mBlendRect = aBlendRect;
  ImageUpdatedInternal(GetRect());

  if (aFinalize) {
    FinalizeSurfaceInternal();
  }

  mFinished = true;

  // The image is now complete, wake up anyone who's waiting.
  mMonitor.NotifyAll();
}

uint32_t
imgFrame::GetImageBytesPerRow() const
{
  mMonitor.AssertCurrentThreadOwns();

  if (mRawSurface) {
    return mFrameRect.Width() * BytesPerPixel(mFormat);
  }

  if (mPaletteDepth) {
    return mFrameRect.Width();
  }

  return 0;
}

uint32_t
imgFrame::GetImageDataLength() const
{
  return GetImageBytesPerRow() * mFrameRect.Height();
}

void
imgFrame::GetImageData(uint8_t** aData, uint32_t* aLength) const
{
  MonitorAutoLock lock(mMonitor);
  GetImageDataInternal(aData, aLength);
}

void
imgFrame::GetImageDataInternal(uint8_t** aData, uint32_t* aLength) const
{
  mMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(mLockCount > 0, "Image data should be locked");

  if (mLockedSurface) {
    // TODO: This is okay for now because we only realloc shared surfaces on
    // the main thread after decoding has finished, but if animations want to
    // read frame data off the main thread, we will need to reconsider this.
    *aData = mLockedSurface->GetData();
    MOZ_ASSERT(*aData,
      "mLockedSurface is non-null, but GetData is null in GetImageData");
  } else if (mPalettedImageData) {
    *aData = mPalettedImageData + PaletteDataLength();
    MOZ_ASSERT(*aData,
      "mPalettedImageData is non-null, but result is null in GetImageData");
  } else {
    MOZ_ASSERT(false,
      "Have neither mLockedSurface nor mPalettedImageData in GetImageData");
    *aData = nullptr;
  }

  *aLength = GetImageDataLength();
}

uint8_t*
imgFrame::GetImageData() const
{
  uint8_t* data;
  uint32_t length;
  GetImageData(&data, &length);
  return data;
}

bool
imgFrame::GetIsPaletted() const
{
  return mPalettedImageData != nullptr;
}

void
imgFrame::GetPaletteData(uint32_t** aPalette, uint32_t* length) const
{
  AssertImageDataLocked();

  if (!mPalettedImageData) {
    *aPalette = nullptr;
    *length = 0;
  } else {
    *aPalette = (uint32_t*) mPalettedImageData;
    *length = PaletteDataLength();
  }
}

uint32_t*
imgFrame::GetPaletteData() const
{
  uint32_t* data;
  uint32_t length;
  GetPaletteData(&data, &length);
  return data;
}

nsresult
imgFrame::LockImageData()
{
  MonitorAutoLock lock(mMonitor);

  MOZ_ASSERT(mLockCount >= 0, "Unbalanced locks and unlocks");
  if (mLockCount < 0) {
    return NS_ERROR_FAILURE;
  }

  mLockCount++;

  // If we are not the first lock, there's nothing to do.
  if (mLockCount != 1) {
    return NS_OK;
  }

  // If we're the first lock, but have the locked surface, we're OK.
  if (mLockedSurface) {
    return NS_OK;
  }

  // Paletted images don't have surfaces, so there's nothing to do.
  if (mPalettedImageData) {
    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("It's illegal to re-lock an optimized imgFrame");
  return NS_ERROR_FAILURE;
}

void
imgFrame::AssertImageDataLocked() const
{
#ifdef DEBUG
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mLockCount > 0, "Image data should be locked");
#endif
}

nsresult
imgFrame::UnlockImageData()
{
  MonitorAutoLock lock(mMonitor);

  MOZ_ASSERT(mLockCount > 0, "Unlocking an unlocked image!");
  if (mLockCount <= 0) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mLockCount > 1 || mFinished || mAborted,
             "Should have Finish()'d or aborted before unlocking");

  mLockCount--;

  return NS_OK;
}

void
imgFrame::SetOptimizable()
{
  AssertImageDataLocked();
  MonitorAutoLock lock(mMonitor);
  mOptimizable = true;
}

void
imgFrame::FinalizeSurface()
{
  MonitorAutoLock lock(mMonitor);
  FinalizeSurfaceInternal();
}

void
imgFrame::FinalizeSurfaceInternal()
{
  mMonitor.AssertCurrentThreadOwns();

  // Not all images will have mRawSurface to finalize (i.e. paletted images).
  if (!mRawSurface || mRawSurface->GetType() != SurfaceType::DATA_SHARED) {
    return;
  }

  auto sharedSurf = static_cast<SourceSurfaceSharedData*>(mRawSurface.get());
  sharedSurf->Finalize();
}

already_AddRefed<SourceSurface>
imgFrame::GetSourceSurface()
{
  MonitorAutoLock lock(mMonitor);
  return GetSourceSurfaceInternal();
}

already_AddRefed<SourceSurface>
imgFrame::GetSourceSurfaceInternal()
{
  mMonitor.AssertCurrentThreadOwns();

  if (mOptSurface) {
    if (mOptSurface->IsValid()) {
      RefPtr<SourceSurface> surf(mOptSurface);
      return surf.forget();
    } else {
      mOptSurface = nullptr;
    }
  }

  if (mLockedSurface) {
    RefPtr<SourceSurface> surf(mLockedSurface);
    return surf.forget();
  }

  if (!mRawSurface) {
    return nullptr;
  }

  return CreateLockedSurface(mRawSurface, mFrameRect.Size(), mFormat);
}

AnimationData
imgFrame::GetAnimationData() const
{
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mLockCount > 0, "Image data should be locked");

  uint8_t* data;
  if (mPalettedImageData) {
    data = mPalettedImageData;
  } else {
    uint32_t length;
    GetImageDataInternal(&data, &length);
  }

  bool hasAlpha = mFormat == SurfaceFormat::B8G8R8A8;

  return AnimationData(data, PaletteDataLength(), mTimeout, GetRect(),
                       mBlendMethod, mBlendRect, mDisposalMethod, hasAlpha);
}

void
imgFrame::Abort()
{
  MonitorAutoLock lock(mMonitor);

  mAborted = true;

  // Wake up anyone who's waiting.
  mMonitor.NotifyAll();
}

bool
imgFrame::IsAborted() const
{
  MonitorAutoLock lock(mMonitor);
  return mAborted;
}

bool
imgFrame::IsFinished() const
{
  MonitorAutoLock lock(mMonitor);
  return mFinished;
}

void
imgFrame::WaitUntilFinished() const
{
  MonitorAutoLock lock(mMonitor);

  while (true) {
    // Return if we're aborted or complete.
    if (mAborted || mFinished) {
      return;
    }

    // Not complete yet, so we'll have to wait.
    mMonitor.Wait();
  }
}

bool
imgFrame::AreAllPixelsWritten() const
{
  mMonitor.AssertCurrentThreadOwns();
  return mDecoded.IsEqualInterior(mFrameRect);
}

bool imgFrame::GetCompositingFailed() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mCompositingFailed;
}

void
imgFrame::SetCompositingFailed(bool val)
{
  MOZ_ASSERT(NS_IsMainThread());
  mCompositingFailed = val;
}

void
imgFrame::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                 size_t& aHeapSizeOut,
                                 size_t& aNonHeapSizeOut,
                                 size_t& aExtHandlesOut) const
{
  MonitorAutoLock lock(mMonitor);

  if (mPalettedImageData) {
    aHeapSizeOut += aMallocSizeOf(mPalettedImageData);
  }
  if (mLockedSurface) {
    aHeapSizeOut += aMallocSizeOf(mLockedSurface);
  }
  if (mOptSurface) {
    aHeapSizeOut += aMallocSizeOf(mOptSurface);
  }
  if (mRawSurface) {
    aHeapSizeOut += aMallocSizeOf(mRawSurface);
    mRawSurface->AddSizeOfExcludingThis(aMallocSizeOf, aHeapSizeOut,
                                        aNonHeapSizeOut, aExtHandlesOut);
  }
}

} // namespace image
} // namespace mozilla
