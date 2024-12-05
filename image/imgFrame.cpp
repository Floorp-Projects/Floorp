/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgFrame.h"
#include "ImageRegion.h"
#include "SurfaceCache.h"

#include "prenv.h"

#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

#include "gfxUtils.h"

#include "MainThreadUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsMargin.h"
#include "nsRefreshDriver.h"
#include "nsThreadUtils.h"

#include <algorithm>  // for min, max

namespace mozilla {

using namespace gfx;

namespace image {

/**
 * This class is identical to SourceSurfaceSharedData but returns a different
 * type so that SharedSurfacesChild is aware imagelib wants to recycle this
 * surface for future animation frames.
 */
class RecyclingSourceSurfaceSharedData final : public SourceSurfaceSharedData {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(RecyclingSourceSurfaceSharedData,
                                          override)

  SurfaceType GetType() const override {
    return SurfaceType::DATA_RECYCLING_SHARED;
  }
};

static already_AddRefed<SourceSurfaceSharedData> AllocateBufferForImage(
    const IntSize& size, SurfaceFormat format, bool aShouldRecycle = false) {
  // Stride must be a multiple of four or cairo will complain.
  int32_t stride = (size.width * BytesPerPixel(format) + 0x3) & ~0x3;

  RefPtr<SourceSurfaceSharedData> newSurf;
  if (aShouldRecycle) {
    newSurf = new RecyclingSourceSurfaceSharedData();
  } else {
    newSurf = new SourceSurfaceSharedData();
  }
  if (!newSurf->Init(size, stride, format)) {
    return nullptr;
  }
  return newSurf.forget();
}

static bool GreenSurface(SourceSurfaceSharedData* aSurface,
                         const IntSize& aSize, SurfaceFormat aFormat) {
  int32_t stride = aSurface->Stride();
  uint32_t* surfaceData = reinterpret_cast<uint32_t*>(aSurface->GetData());
  uint32_t surfaceDataLength = (stride * aSize.height) / sizeof(uint32_t);

  // Start by assuming that GG is in the second byte and
  // AA is in the final byte -- the most common case.
  uint32_t color = mozilla::NativeEndian::swapFromBigEndian(0x00FF00FF);

  // We are only going to handle this type of test under
  // certain circumstances.
  MOZ_ASSERT(surfaceData);
  MOZ_ASSERT(aFormat == SurfaceFormat::B8G8R8A8 ||
             aFormat == SurfaceFormat::B8G8R8X8 ||
             aFormat == SurfaceFormat::R8G8B8A8 ||
             aFormat == SurfaceFormat::R8G8B8X8 ||
             aFormat == SurfaceFormat::A8R8G8B8 ||
             aFormat == SurfaceFormat::X8R8G8B8);
  MOZ_ASSERT((stride * aSize.height) % sizeof(uint32_t));

  if (aFormat == SurfaceFormat::A8R8G8B8 ||
      aFormat == SurfaceFormat::X8R8G8B8) {
    color = mozilla::NativeEndian::swapFromBigEndian(0xFF00FF00);
  }

  for (uint32_t i = 0; i < surfaceDataLength; i++) {
    surfaceData[i] = color;
  }

  return true;
}

static bool ClearSurface(SourceSurfaceSharedData* aSurface,
                         const IntSize& aSize, SurfaceFormat aFormat) {
  int32_t stride = aSurface->Stride();
  uint8_t* data = aSurface->GetData();
  MOZ_ASSERT(data);

  if (aFormat == SurfaceFormat::OS_RGBX) {
    // Skia doesn't support RGBX surfaces, so ensure the alpha value is set
    // to opaque white. While it would be nice to only do this for Skia,
    // imgFrame can run off main thread and past shutdown where
    // we might not have gfxPlatform, so just memset every time instead.
    memset(data, 0xFF, stride * aSize.height);
  } else if (aSurface->OnHeap()) {
    // We only need to memset it if the buffer was allocated on the heap.
    // Otherwise, it's allocated via mmap and refers to a zeroed page and will
    // be COW once it's written to.
    memset(data, 0, stride * aSize.height);
  }

  return true;
}

imgFrame::imgFrame()
    : mMonitor("imgFrame"),
      mDecoded(0, 0, 0, 0),
      mAborted(false),
      mFinished(false),
      mShouldRecycle(false),
      mTimeout(FrameTimeout::FromRawMilliseconds(100)),
      mDisposalMethod(DisposalMethod::NOT_SPECIFIED),
      mBlendMethod(BlendMethod::OVER),
      mFormat(SurfaceFormat::UNKNOWN),
      mNonPremult(false) {}

imgFrame::~imgFrame() {
#ifdef DEBUG
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mAborted || AreAllPixelsWritten());
  MOZ_ASSERT(mAborted || mFinished);
#endif
}

nsresult imgFrame::InitForDecoder(const nsIntSize& aImageSize,
                                  SurfaceFormat aFormat, bool aNonPremult,
                                  const Maybe<AnimationParams>& aAnimParams,
                                  bool aShouldRecycle,
                                  uint32_t* aImageDataLength) {
  // Assert for properties that should be verified by decoders,
  // warn for properties related to bad content.
  if (!SurfaceCache::IsLegalSize(aImageSize)) {
    NS_WARNING("Should have legal image size");
    MonitorAutoLock lock(mMonitor);
    mAborted = true;
    return NS_ERROR_FAILURE;
  }

  mImageSize = aImageSize;

  // May be updated shortly after InitForDecoder by BlendAnimationFilter
  // because it needs to take into consideration the previous frames to
  // properly calculate. We start with the whole frame as dirty.
  mDirtyRect = GetRect();

  if (aAnimParams) {
    mBlendRect = aAnimParams->mBlendRect;
    mTimeout = aAnimParams->mTimeout;
    mBlendMethod = aAnimParams->mBlendMethod;
    mDisposalMethod = aAnimParams->mDisposalMethod;
  } else {
    mBlendRect = GetRect();
  }

  if (aShouldRecycle) {
    // If we are recycling then we should always use BGRA for the underlying
    // surface because if we use BGRX, the next frame composited into the
    // surface could be BGRA and cause rendering problems.
    MOZ_ASSERT(aAnimParams);
    mFormat = SurfaceFormat::OS_RGBA;
  } else {
    mFormat = aFormat;
  }

  mNonPremult = aNonPremult;

  MonitorAutoLock lock(mMonitor);
  mShouldRecycle = aShouldRecycle;

  MOZ_ASSERT(!mRawSurface, "Called imgFrame::InitForDecoder() twice?");

  mRawSurface = AllocateBufferForImage(mImageSize, mFormat, mShouldRecycle);
  if (!mRawSurface) {
    mAborted = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (StaticPrefs::browser_measurement_render_anims_and_video_solid() &&
      aAnimParams) {
    mBlankRawSurface = AllocateBufferForImage(mImageSize, mFormat);
    if (!mBlankRawSurface) {
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!ClearSurface(mRawSurface, mImageSize, mFormat)) {
    NS_WARNING("Could not clear allocated buffer");
    mAborted = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mBlankRawSurface) {
    if (!GreenSurface(mBlankRawSurface, mImageSize, mFormat)) {
      NS_WARNING("Could not clear allocated blank buffer");
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (aImageDataLength) {
    *aImageDataLength = GetImageDataLength();
  }

  return NS_OK;
}

nsresult imgFrame::InitForDecoderRecycle(const AnimationParams& aAnimParams,
                                         uint32_t* aImageDataLength) {
  // We want to recycle this frame, but there is no guarantee that consumers are
  // done with it in a timely manner. Let's ensure they are done with it first.
  MonitorAutoLock lock(mMonitor);

  MOZ_ASSERT(mRawSurface);

  if (!mShouldRecycle) {
    // This frame either was never marked as recyclable, or the flag was cleared
    // for a caller which does not support recycling.
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Ensure we account for all internal references to the surface.
  MozRefCountType internalRefs = 1;
  if (mOptSurface == mRawSurface) {
    ++internalRefs;
  }

  if (mRawSurface->refCount() > internalRefs) {
    if (NS_IsMainThread()) {
      // We should never be both decoding and recycling on the main thread. Sync
      // decoding can only be used to produce the first set of frames. Those
      // either never use recycling because advancing was blocked (main thread
      // is busy) or we were auto-advancing (to seek to a frame) and the frames
      // were never accessed (and thus cannot have recycle locks).
      MOZ_ASSERT_UNREACHABLE("Recycling/decoding on the main thread?");
      return NS_ERROR_NOT_AVAILABLE;
    }

    // We don't want to wait forever to reclaim the frame because we have no
    // idea why it is still held. It is possibly due to OMTP. Since we are off
    // the main thread, and we generally have frames already buffered for the
    // animation, we can afford to wait a short period of time to hopefully
    // complete the transaction and reclaim the buffer.
    //
    // We choose to wait for, at most, the refresh driver interval, so that we
    // won't skip more than one frame. If the frame is still in use due to
    // outstanding transactions, we are already skipping frames. If the frame
    // is still in use for some other purpose, it won't be returned to the pool
    // and its owner can hold onto it forever without additional impact here.
    int32_t refreshInterval =
        std::max(std::min(nsRefreshDriver::DefaultInterval(), 20), 4);
    TimeDuration waitInterval =
        TimeDuration::FromMilliseconds(refreshInterval >> 2);
    TimeStamp timeout =
        TimeStamp::Now() + TimeDuration::FromMilliseconds(refreshInterval);
    while (true) {
      mMonitor.Wait(waitInterval);
      if (mRawSurface->refCount() <= internalRefs) {
        break;
      }

      if (timeout <= TimeStamp::Now()) {
        // We couldn't secure the frame for recycling. It will allocate a new
        // frame instead.
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
  }

  mBlendRect = aAnimParams.mBlendRect;
  mTimeout = aAnimParams.mTimeout;
  mBlendMethod = aAnimParams.mBlendMethod;
  mDisposalMethod = aAnimParams.mDisposalMethod;
  mDirtyRect = GetRect();

  if (aImageDataLength) {
    *aImageDataLength = GetImageDataLength();
  }

  return NS_OK;
}

nsresult imgFrame::InitWithDrawable(gfxDrawable* aDrawable,
                                    const nsIntSize& aSize,
                                    const SurfaceFormat aFormat,
                                    SamplingFilter aSamplingFilter,
                                    uint32_t aImageFlags,
                                    gfx::BackendType aBackend) {
  // Assert for properties that should be verified by decoders,
  // warn for properties related to bad content.
  if (!SurfaceCache::IsLegalSize(aSize)) {
    NS_WARNING("Should have legal image size");
    MonitorAutoLock lock(mMonitor);
    mAborted = true;
    return NS_ERROR_FAILURE;
  }

  mImageSize = aSize;
  mFormat = aFormat;

  RefPtr<DrawTarget> target;

  bool canUseDataSurface = Factory::DoesBackendSupportDataDrawtarget(aBackend);
  if (canUseDataSurface) {
    MonitorAutoLock lock(mMonitor);
    // It's safe to use data surfaces for content on this platform, so we can
    // get away with using volatile buffers.
    MOZ_ASSERT(!mRawSurface, "Called imgFrame::InitWithDrawable() twice?");

    mRawSurface = AllocateBufferForImage(mImageSize, mFormat);
    if (!mRawSurface) {
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!ClearSurface(mRawSurface, mImageSize, mFormat)) {
      NS_WARNING("Could not clear allocated buffer");
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    target = gfxPlatform::CreateDrawTargetForData(
        mRawSurface->GetData(), mImageSize, mRawSurface->Stride(), mFormat);
  } else {
    // We can't use data surfaces for content, so we'll create an offscreen
    // surface instead.  This means if someone later calls RawAccessRef(), we
    // may have to do an expensive readback, but we warned callers about that in
    // the documentation for this method.
#ifdef DEBUG
    {
      MonitorAutoLock lock(mMonitor);
      MOZ_ASSERT(!mOptSurface, "Called imgFrame::InitWithDrawable() twice?");
    }
#endif

    if (gfxPlatform::GetPlatform()->SupportsAzureContentForType(aBackend)) {
      target = gfxPlatform::GetPlatform()->CreateDrawTargetForBackend(
          aBackend, mImageSize, mFormat);
    } else {
      target = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          mImageSize, mFormat);
    }
  }

  if (!target || !target->IsValid()) {
    MonitorAutoLock lock(mMonitor);
    mAborted = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Draw using the drawable the caller provided.
  gfxContext ctx(target);

  gfxUtils::DrawPixelSnapped(&ctx, aDrawable, SizeDouble(mImageSize),
                             ImageRegion::Create(ThebesRect(GetRect())),
                             mFormat, aSamplingFilter, aImageFlags);

  MonitorAutoLock lock(mMonitor);
  if (canUseDataSurface && !mRawSurface) {
    NS_WARNING("Failed to create SourceSurfaceSharedData");
    mAborted = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!canUseDataSurface) {
    // We used an offscreen surface, which is an "optimized" surface from
    // imgFrame's perspective.
    mOptSurface = target->Snapshot();
  } else {
    FinalizeSurfaceInternal();
  }

  // If we reach this point, we should regard ourselves as complete.
  mDecoded = GetRect();
  mFinished = true;

  MOZ_ASSERT(AreAllPixelsWritten());

  return NS_OK;
}

DrawableFrameRef imgFrame::DrawableRef() { return DrawableFrameRef(this); }

RawAccessFrameRef imgFrame::RawAccessRef(
    gfx::DataSourceSurface::MapType aMapType) {
  return RawAccessFrameRef(this, aMapType);
}

imgFrame::SurfaceWithFormat imgFrame::SurfaceForDrawing(
    bool aDoPartialDecode, bool aDoTile, ImageRegion& aRegion,
    SourceSurface* aSurface) {
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();

  if (!aDoPartialDecode) {
    return SurfaceWithFormat(new gfxSurfaceDrawable(aSurface, mImageSize),
                             mFormat);
  }

  gfxRect available =
      gfxRect(mDecoded.X(), mDecoded.Y(), mDecoded.Width(), mDecoded.Height());

  if (aDoTile) {
    // Create a temporary surface.
    // Give this surface an alpha channel because there are
    // transparent pixels in the padding or undecoded area
    RefPtr<DrawTarget> target =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
            mImageSize, SurfaceFormat::OS_RGBA);
    if (!target) {
      return SurfaceWithFormat();
    }

    SurfacePattern pattern(aSurface, aRegion.GetExtendMode(),
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
                    float aOpacity) {
  AUTO_PROFILER_LABEL("imgFrame::Draw", GRAPHICS);

  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(!aRegion.Rect().IsEmpty(), "Drawing empty region!");
  NS_ASSERTION(!aRegion.IsRestricted() ||
                   !aRegion.Rect().Intersect(aRegion.Restriction()).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");

  // Perform the draw and freeing of the surface outside the lock. We want to
  // avoid contention with the decoder if we can. The surface may also attempt
  // to relock the monitor if it is freed (e.g. RecyclingSourceSurface).
  RefPtr<SourceSurface> surf;
  SurfaceWithFormat surfaceResult;
  ImageRegion region(aRegion);
  gfxRect imageRect(0, 0, mImageSize.width, mImageSize.height);

  {
    MonitorAutoLock lock(mMonitor);

    bool doPartialDecode = !AreAllPixelsWritten();

    // Most draw targets will just use the surface only during DrawPixelSnapped
    // but captures/recordings will retain a reference outside this stack
    // context. While in theory a decoder thread could be trying to recycle this
    // frame at this very moment, in practice the only way we can get here is if
    // this frame is the current frame of the animation. Since we can only
    // advance on the main thread, we know nothing else will try to use it.
    DrawTarget* drawTarget = aContext->GetDrawTarget();
    bool recording = drawTarget->GetBackendType() == BackendType::RECORDING;
    RefPtr<SourceSurface> surf = GetSourceSurfaceInternal();
    if (!surf) {
      return false;
    }

    bool doTile = !imageRect.Contains(aRegion.Rect()) &&
                  !(aImageFlags & imgIContainer::FLAG_CLAMP);

    surfaceResult = SurfaceForDrawing(doPartialDecode, doTile, region, surf);

    // If we are recording, then we cannot recycle the surface. The blob
    // rasterizer is not properly synchronized for recycling in the compositor
    // process. The easiest thing to do is just mark the frames it consumes as
    // non-recyclable.
    if (recording && surfaceResult.IsValid()) {
      mShouldRecycle = false;
    }
  }

  if (surfaceResult.IsValid()) {
    gfxUtils::DrawPixelSnapped(aContext, surfaceResult.mDrawable,
                               imageRect.Size(), region, surfaceResult.mFormat,
                               aSamplingFilter, aImageFlags, aOpacity);
  }

  return true;
}

nsresult imgFrame::ImageUpdated(const nsIntRect& aUpdateRect) {
  MonitorAutoLock lock(mMonitor);
  return ImageUpdatedInternal(aUpdateRect);
}

nsresult imgFrame::ImageUpdatedInternal(const nsIntRect& aUpdateRect) {
  mMonitor.AssertCurrentThreadOwns();

  // Clamp to the frame rect to ensure that decoder bugs don't result in a
  // decoded rect that extends outside the bounds of the frame rect.
  IntRect updateRect = aUpdateRect.Intersect(GetRect());
  if (updateRect.IsEmpty()) {
    return NS_OK;
  }

  mDecoded.UnionRect(mDecoded, updateRect);

  // Update our invalidation counters for any consumers watching for changes
  // in the surface.
  if (mRawSurface) {
    mRawSurface->Invalidate(updateRect);
  }
  return NS_OK;
}

void imgFrame::Finish(Opacity aFrameOpacity /* = Opacity::SOME_TRANSPARENCY */,
                      bool aFinalize /* = true */,
                      bool aOrientationSwapsWidthAndHeight /* = false */) {
  MonitorAutoLock lock(mMonitor);

  IntRect frameRect(GetRect());
  if (!mDecoded.IsEqualEdges(frameRect)) {
    // The decoder should have produced rows starting from either the bottom or
    // the top of the image. We need to calculate the region for which we have
    // not yet invalidated. And if the orientation swaps width and height then
    // its from the left or right.
    IntRect delta(0, 0, frameRect.width, 0);
    if (!aOrientationSwapsWidthAndHeight) {
      delta.width = frameRect.width;
      if (mDecoded.y == 0) {
        delta.y = mDecoded.height;
        delta.height = frameRect.height - mDecoded.height;
      } else if (mDecoded.y + mDecoded.height == frameRect.height) {
        delta.height = frameRect.height - mDecoded.y;
      } else {
        MOZ_ASSERT_UNREACHABLE("Decoder only updated middle of image!");
        delta = frameRect;
      }
    } else {
      delta.height = frameRect.height;
      if (mDecoded.x == 0) {
        delta.x = mDecoded.width;
        delta.width = frameRect.width - mDecoded.width;
      } else if (mDecoded.x + mDecoded.width == frameRect.width) {
        delta.width = frameRect.width - mDecoded.x;
      } else {
        MOZ_ASSERT_UNREACHABLE("Decoder only updated middle of image!");
        delta = frameRect;
      }
    }

    ImageUpdatedInternal(delta);
  }

  MOZ_ASSERT(mDecoded.IsEqualEdges(frameRect));

  if (aFinalize) {
    FinalizeSurfaceInternal();
  }

  mFinished = true;

  // The image is now complete, wake up anyone who's waiting.
  mMonitor.NotifyAll();
}

uint32_t imgFrame::GetImageBytesPerRow() const {
  mMonitor.AssertCurrentThreadOwns();

  if (mRawSurface) {
    return mImageSize.width * BytesPerPixel(mFormat);
  }

  return 0;
}

uint32_t imgFrame::GetImageDataLength() const {
  return GetImageBytesPerRow() * mImageSize.height;
}

void imgFrame::FinalizeSurface() {
  MonitorAutoLock lock(mMonitor);
  FinalizeSurfaceInternal();
}

void imgFrame::FinalizeSurfaceInternal() {
  mMonitor.AssertCurrentThreadOwns();

  // Not all images will have mRawSurface to finalize (i.e. paletted images).
  if (mShouldRecycle || !mRawSurface ||
      mRawSurface->GetType() != SurfaceType::DATA_SHARED) {
    return;
  }

  auto* sharedSurf = static_cast<SourceSurfaceSharedData*>(mRawSurface.get());
  sharedSurf->Finalize();
}

already_AddRefed<SourceSurface> imgFrame::GetSourceSurface() {
  MonitorAutoLock lock(mMonitor);
  return GetSourceSurfaceInternal();
}

already_AddRefed<SourceSurface> imgFrame::GetSourceSurfaceInternal() {
  mMonitor.AssertCurrentThreadOwns();

  if (mOptSurface) {
    if (mOptSurface->IsValid()) {
      RefPtr<SourceSurface> surf(mOptSurface);
      return surf.forget();
    }
    mOptSurface = nullptr;
  }

  if (mBlankRawSurface) {
    // We are going to return the blank surface because of the flags.
    // We are including comments here that are copied from below
    // just so that we are on the same page!
    RefPtr<SourceSurface> surf(mBlankRawSurface);
    return surf.forget();
  }

  RefPtr<SourceSurface> surf(mRawSurface);
  return surf.forget();
}

void imgFrame::Abort() {
  MonitorAutoLock lock(mMonitor);

  mAborted = true;

  // Wake up anyone who's waiting.
  mMonitor.NotifyAll();
}

bool imgFrame::IsAborted() const {
  MonitorAutoLock lock(mMonitor);
  return mAborted;
}

bool imgFrame::IsFinished() const {
  MonitorAutoLock lock(mMonitor);
  return mFinished;
}

void imgFrame::WaitUntilFinished() const {
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

bool imgFrame::AreAllPixelsWritten() const {
  mMonitor.AssertCurrentThreadOwns();
  return mDecoded.IsEqualInterior(GetRect());
}

void imgFrame::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                      const AddSizeOfCb& aCallback) const {
  MonitorAutoLock lock(mMonitor);

  AddSizeOfCbData metadata;
  metadata.mFinished = mFinished;

  if (mOptSurface) {
    metadata.mHeapBytes += aMallocSizeOf(mOptSurface);

    SourceSurface::SizeOfInfo info;
    mOptSurface->SizeOfExcludingThis(aMallocSizeOf, info);
    metadata.Accumulate(info);
  }
  if (mRawSurface) {
    metadata.mHeapBytes += aMallocSizeOf(mRawSurface);

    SourceSurface::SizeOfInfo info;
    mRawSurface->SizeOfExcludingThis(aMallocSizeOf, info);
    metadata.Accumulate(info);
  }

  aCallback(metadata);
}

}  // namespace image
}  // namespace mozilla
