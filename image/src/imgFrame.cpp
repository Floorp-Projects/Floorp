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
#include "gfxUtils.h"
#include "gfxAlphaRecovery.h"

static bool gDisableOptimize = false;

#include "GeckoProfiler.h"
#include "mozilla/Likely.h"
#include "MainThreadUtils.h"
#include "mozilla/MemoryReporting.h"
#include "nsMargin.h"
#include "nsThreadUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/gfx/Tools.h"


namespace mozilla {

using namespace gfx;

namespace image {

static UserDataKey kVolatileBuffer;

static void
VolatileBufferRelease(void *vbuf)
{
  delete static_cast<VolatileBufferPtr<unsigned char>*>(vbuf);
}

static int32_t
VolatileSurfaceStride(const IntSize& size, SurfaceFormat format)
{
  // Stride must be a multiple of four or cairo will complain.
  return (size.width * BytesPerPixel(format) + 0x3) & ~0x3;
}

static TemporaryRef<DataSourceSurface>
CreateLockedSurface(VolatileBuffer *vbuf,
                    const IntSize& size,
                    SurfaceFormat format)
{
  VolatileBufferPtr<unsigned char> *vbufptr =
    new VolatileBufferPtr<unsigned char>(vbuf);
  MOZ_ASSERT(!vbufptr->WasBufferPurged(), "Expected image data!");

  int32_t stride = VolatileSurfaceStride(size, format);
  RefPtr<DataSourceSurface> surf =
    Factory::CreateWrappingDataSourceSurface(*vbufptr, stride, size, format);
  if (!surf) {
    delete vbufptr;
    return nullptr;
  }

  surf->AddUserData(&kVolatileBuffer, vbufptr, VolatileBufferRelease);
  return surf;
}

static TemporaryRef<VolatileBuffer>
AllocateBufferForImage(const IntSize& size, SurfaceFormat format)
{
  int32_t stride = VolatileSurfaceStride(size, format);
  RefPtr<VolatileBuffer> buf = new VolatileBuffer();
  if (buf->Init(stride * size.height,
                1 << gfxAlphaRecovery::GoodAlignmentLog2()))
    return buf;

  return nullptr;
}

// Returns true if an image of aWidth x aHeight is allowed and legal.
static bool AllowedImageSize(int32_t aWidth, int32_t aHeight)
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
#if defined(XP_MACOSX)
  // CoreGraphics is limited to images < 32K in *height*, so clamp all surfaces on the Mac to that height
  if (MOZ_UNLIKELY(aHeight > SHRT_MAX)) {
    NS_WARNING("image too big");
    return false;
  }
#endif
  return true;
}

static bool AllowedImageAndFrameDimensions(const nsIntSize& aImageSize,
                                           const nsIntRect& aFrameRect)
{
  if (!AllowedImageSize(aImageSize.width, aImageSize.height)) {
    return false;
  }
  if (!AllowedImageSize(aFrameRect.width, aFrameRect.height)) {
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
  , mTimeout(100)
  , mDisposalMethod(DisposalMethod::NOT_SPECIFIED)
  , mBlendMethod(BlendMethod::OVER)
  , mHasNoAlpha(false)
  , mAborted(false)
  , mPalettedImageData(nullptr)
  , mPaletteDepth(0)
  , mNonPremult(false)
  , mSinglePixel(false)
  , mCompositingFailed(false)
  , mOptimizable(false)
{
  static bool hasCheckedOptimize = false;
  if (!hasCheckedOptimize) {
    if (PR_GetEnv("MOZ_DISABLE_IMAGE_OPTIMIZE")) {
      gDisableOptimize = true;
    }
    hasCheckedOptimize = true;
  }
}

imgFrame::~imgFrame()
{
#ifdef DEBUG
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mAborted || IsImageCompleteInternal());
#endif

  moz_free(mPalettedImageData);
  mPalettedImageData = nullptr;
}

nsresult
imgFrame::ReinitForDecoder(const nsIntSize& aImageSize,
                           const nsIntRect& aRect,
                           SurfaceFormat aFormat,
                           uint8_t aPaletteDepth /* = 0 */,
                           bool aNonPremult /* = false */)
{
  MonitorAutoLock lock(mMonitor);

  if (mDecoded.x != 0 || mDecoded.y != 0 ||
      mDecoded.width != 0 || mDecoded.height != 0) {
    MOZ_ASSERT_UNREACHABLE("Shouldn't reinit after write");
    return NS_ERROR_FAILURE;
  }
  if (mAborted) {
    MOZ_ASSERT_UNREACHABLE("Shouldn't reinit if aborted");
    return NS_ERROR_FAILURE;
  }
  if (mLockCount < 1) {
    MOZ_ASSERT_UNREACHABLE("Shouldn't reinit unless locked");
    return NS_ERROR_FAILURE;
  }

  // Restore everything (except mLockCount, which we need to keep) to how it was
  // when we were first created.
  // XXX(seth): This is probably a little excessive, but I want to be *really*
  // sure that nothing got missed.
  mDecoded = nsIntRect(0, 0, 0, 0);
  mTimeout = 100;
  mDisposalMethod = DisposalMethod::NOT_SPECIFIED;
  mBlendMethod = BlendMethod::OVER;
  mHasNoAlpha = false;
  mAborted = false;
  mPaletteDepth = 0;
  mNonPremult = false;
  mSinglePixel = false;
  mCompositingFailed = false;
  mOptimizable = false;
  mImageSize = IntSize();
  mSize = IntSize();
  mOffset = nsIntPoint();
  mSinglePixelColor = Color();

  // Release all surfaces.
  mImageSurface = nullptr;
  mOptSurface = nullptr;
  mVBuf = nullptr;
  mVBufPtr = nullptr;
  moz_free(mPalettedImageData);
  mPalettedImageData = nullptr;

  // Reinitialize.
  nsresult rv = InitForDecoder(aImageSize, aRect, aFormat,
                               aPaletteDepth, aNonPremult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We were locked before; perform the same actions we would've performed when
  // we originally got locked.
  if (mImageSurface) {
    mVBufPtr = mVBuf;
    return NS_OK;
  }

  if (!mPalettedImageData) {
    MOZ_ASSERT_UNREACHABLE("We got optimized somehow during reinit");
    return NS_ERROR_FAILURE;
  }

  // Paletted images don't have surfaces, so there's nothing to do.
  return NS_OK;
}

nsresult
imgFrame::InitForDecoder(const nsIntSize& aImageSize,
                         const nsIntRect& aRect,
                         SurfaceFormat aFormat,
                         uint8_t aPaletteDepth /* = 0 */,
                         bool aNonPremult /* = false */)
{
  // Assert for properties that should be verified by decoders,
  // warn for properties related to bad content.
  if (!AllowedImageAndFrameDimensions(aImageSize, aRect)) {
    NS_WARNING("Should have legal image size");
    mAborted = true;
    return NS_ERROR_FAILURE;
  }

  mImageSize = aImageSize.ToIntSize();
  mOffset.MoveTo(aRect.x, aRect.y);
  mSize.SizeTo(aRect.width, aRect.height);

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
    mPalettedImageData =
      static_cast<uint8_t*>(moz_malloc(PaletteDataLength() +
                                       (mSize.width * mSize.height)));
    if (!mPalettedImageData)
      NS_WARNING("moz_malloc for paletted image data should succeed");
    NS_ENSURE_TRUE(mPalettedImageData, NS_ERROR_OUT_OF_MEMORY);
  } else {
    MOZ_ASSERT(!mImageSurface, "Called imgFrame::InitForDecoder() twice?");

    mVBuf = AllocateBufferForImage(mSize, mFormat);
    if (!mVBuf) {
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (mVBuf->OnHeap()) {
      int32_t stride = VolatileSurfaceStride(mSize, mFormat);
      VolatileBufferPtr<uint8_t> ptr(mVBuf);
      memset(ptr, 0, stride * mSize.height);
    }
    mImageSurface = CreateLockedSurface(mVBuf, mSize, mFormat);

    if (!mImageSurface) {
      NS_WARNING("Failed to create VolatileDataSourceSurface");
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
                           GraphicsFilter aFilter,
                           uint32_t aImageFlags)
{
  // Assert for properties that should be verified by decoders,
  // warn for properties related to bad content.
  if (!AllowedImageSize(aSize.width, aSize.height)) {
    NS_WARNING("Should have legal image size");
    mAborted = true;
    return NS_ERROR_FAILURE;
  }

  mImageSize = aSize.ToIntSize();
  mOffset.MoveTo(0, 0);
  mSize.SizeTo(aSize.width, aSize.height);

  mFormat = aFormat;
  mPaletteDepth = 0;

  RefPtr<DrawTarget> target;

  bool canUseDataSurface =
    gfxPlatform::GetPlatform()->CanRenderContentToDataSurface();

  if (canUseDataSurface) {
    // It's safe to use data surfaces for content on this platform, so we can
    // get away with using volatile buffers.
    MOZ_ASSERT(!mImageSurface, "Called imgFrame::InitWithDrawable() twice?");

    mVBuf = AllocateBufferForImage(mSize, mFormat);
    if (!mVBuf) {
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    int32_t stride = VolatileSurfaceStride(mSize, mFormat);
    VolatileBufferPtr<uint8_t> ptr(mVBuf);
    if (!ptr) {
      mAborted = true;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (mVBuf->OnHeap()) {
      memset(ptr, 0, stride * mSize.height);
    }
    mImageSurface = CreateLockedSurface(mVBuf, mSize, mFormat);

    target = gfxPlatform::GetPlatform()->
      CreateDrawTargetForData(ptr, mSize, stride, mFormat);
  } else {
    // We can't use data surfaces for content, so we'll create an offscreen
    // surface instead.  This means if someone later calls RawAccessRef(), we
    // may have to do an expensive readback, but we warned callers about that in
    // the documentation for this method.
    MOZ_ASSERT(!mOptSurface, "Called imgFrame::InitWithDrawable() twice?");

    target = gfxPlatform::GetPlatform()->
        CreateOffscreenContentDrawTarget(mSize, mFormat);
  }

  if (!target) {
    mAborted = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Draw using the drawable the caller provided.
  nsIntRect imageRect(0, 0, mSize.width, mSize.height);
  nsRefPtr<gfxContext> ctx = new gfxContext(target);
  gfxUtils::DrawPixelSnapped(ctx, aDrawable, ThebesIntSize(mSize),
                             ImageRegion::Create(imageRect),
                             mFormat, aFilter, aImageFlags);

  if (canUseDataSurface && !mImageSurface) {
    NS_WARNING("Failed to create VolatileDataSourceSurface");
    mAborted = true;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!canUseDataSurface) {
    // We used an offscreen surface, which is an "optimized" surface from
    // imgFrame's perspective.
    mOptSurface = target->Snapshot();
  }

  // If we reach this point, we should regard ourselves as complete.
  mDecoded = GetRect();
  MOZ_ASSERT(IsImageComplete());

  return NS_OK;
}

nsresult imgFrame::Optimize()
{
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(mLockCount == 1,
             "Should only optimize when holding the lock exclusively");

  // Don't optimize during shutdown because gfxPlatform may not be available.
  if (ShutdownTracker::ShutdownHasStarted())
    return NS_OK;

  if (!mOptimizable || gDisableOptimize)
    return NS_OK;

  if (mPalettedImageData || mOptSurface || mSinglePixel)
    return NS_OK;

  // Don't do single-color opts on non-premult data.
  // Cairo doesn't support non-premult single-colors.
  if (mNonPremult)
    return NS_OK;

  /* Figure out if the entire image is a constant color */

  if (gfxPrefs::ImageSingleColorOptimizationEnabled() &&
      mImageSurface->Stride() == mSize.width * 4) {
    uint32_t *imgData = (uint32_t*) ((uint8_t *)mVBufPtr);
    uint32_t firstPixel = * (uint32_t*) imgData;
    uint32_t pixelCount = mSize.width * mSize.height + 1;

    while (--pixelCount && *imgData++ == firstPixel)
      ;

    if (pixelCount == 0) {
      // all pixels were the same
      if (mFormat == SurfaceFormat::B8G8R8A8 ||
          mFormat == SurfaceFormat::B8G8R8X8) {
        mSinglePixel = true;
        mSinglePixelColor.a = ((firstPixel >> 24) & 0xFF) * (1.0f / 255.0f);
        mSinglePixelColor.r = ((firstPixel >> 16) & 0xFF) * (1.0f / 255.0f);
        mSinglePixelColor.g = ((firstPixel >>  8) & 0xFF) * (1.0f / 255.0f);
        mSinglePixelColor.b = ((firstPixel >>  0) & 0xFF) * (1.0f / 255.0f);
        mSinglePixelColor.r /= mSinglePixelColor.a;
        mSinglePixelColor.g /= mSinglePixelColor.a;
        mSinglePixelColor.b /= mSinglePixelColor.a;

        // blow away the older surfaces (if they exist), to release their memory
        mVBuf = nullptr;
        mVBufPtr = nullptr;
        mImageSurface = nullptr;
        mOptSurface = nullptr;

        return NS_OK;
      }
    }

    // if it's not RGB24/ARGB32, don't optimize, but we never hit this at the moment
  }

#ifdef ANDROID
  SurfaceFormat optFormat =
    gfxPlatform::GetPlatform()->Optimal2DFormatForContent(gfxContentType::COLOR);

  if (mFormat != SurfaceFormat::B8G8R8A8 &&
      optFormat == SurfaceFormat::R5G6B5) {
    RefPtr<VolatileBuffer> buf =
      AllocateBufferForImage(mSize, optFormat);
    if (!buf)
      return NS_OK;

    RefPtr<DataSourceSurface> surf =
      CreateLockedSurface(buf, mSize, optFormat);
    if (!surf)
      return NS_ERROR_OUT_OF_MEMORY;

    DataSourceSurface::MappedSurface mapping;
    DebugOnly<bool> success =
      surf->Map(DataSourceSurface::MapType::WRITE, &mapping);
    NS_ASSERTION(success, "Failed to map surface");
    RefPtr<DrawTarget> target =
      Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                       mapping.mData,
                                       mSize,
                                       mapping.mStride,
                                       optFormat);

    Rect rect(0, 0, mSize.width, mSize.height);
    target->DrawSurface(mImageSurface, rect, rect);
    target->Flush();
    surf->Unmap();

    mImageSurface = surf;
    mVBuf = buf;
    mFormat = optFormat;
  }
#else
  mOptSurface = gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget()->OptimizeSourceSurface(mImageSurface);
  if (mOptSurface == mImageSurface)
    mOptSurface = nullptr;
#endif

  if (mOptSurface) {
    mVBuf = nullptr;
    mVBufPtr = nullptr;
    mImageSurface = nullptr;
  }

#ifdef MOZ_WIDGET_ANDROID
  // On Android, free mImageSurface unconditionally if we're discardable. This
  // allows the operating system to free our volatile buffer.
  // XXX(seth): We'd eventually like to do this on all platforms, but right now
  // converting raw memory to a SourceSurface is expensive on some backends.
  mImageSurface = nullptr;
#endif

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
imgFrame::SurfaceForDrawing(bool               aDoPadding,
                            bool               aDoPartialDecode,
                            bool               aDoTile,
                            gfxContext*        aContext,
                            const nsIntMargin& aPadding,
                            gfxRect&           aImageRect,
                            ImageRegion&       aRegion,
                            SourceSurface*     aSurface)
{
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();

  IntSize size(int32_t(aImageRect.Width()), int32_t(aImageRect.Height()));
  if (!aDoPadding && !aDoPartialDecode) {
    NS_ASSERTION(!mSinglePixel, "This should already have been handled");
    return SurfaceWithFormat(new gfxSurfaceDrawable(aSurface, ThebesIntSize(size)), mFormat);
  }

  gfxRect available = gfxRect(mDecoded.x, mDecoded.y, mDecoded.width, mDecoded.height);

  if (aDoTile || mSinglePixel) {
    // Create a temporary surface.
    // Give this surface an alpha channel because there are
    // transparent pixels in the padding or undecoded area
    RefPtr<DrawTarget> target =
      gfxPlatform::GetPlatform()->
        CreateOffscreenContentDrawTarget(size, SurfaceFormat::B8G8R8A8);
    if (!target)
      return SurfaceWithFormat();

    // Fill 'available' with whatever we've got
    if (mSinglePixel) {
      target->FillRect(ToRect(aRegion.Intersect(available).Rect()),
                       ColorPattern(mSinglePixelColor),
                       DrawOptions(1.0f, CompositionOp::OP_SOURCE));
    } else {
      SurfacePattern pattern(aSurface,
                             ExtendMode::REPEAT,
                             Matrix::Translation(mDecoded.x, mDecoded.y));
      target->FillRect(ToRect(aRegion.Intersect(available).Rect()), pattern);
    }

    RefPtr<SourceSurface> newsurf = target->Snapshot();
    return SurfaceWithFormat(new gfxSurfaceDrawable(newsurf, ThebesIntSize(size)), target->GetFormat());
  }

  // Not tiling, and we have a surface, so we can account for
  // padding and/or a partial decode just by twiddling parameters.
  gfxPoint paddingTopLeft(aPadding.left, aPadding.top);
  aRegion = aRegion.Intersect(available) - paddingTopLeft;
  aContext->Multiply(gfxMatrix::Translation(paddingTopLeft));
  aImageRect = gfxRect(0, 0, mSize.width, mSize.height);

  gfxIntSize availableSize(mDecoded.width, mDecoded.height);
  return SurfaceWithFormat(new gfxSurfaceDrawable(aSurface, availableSize),
                           mFormat);
}

bool imgFrame::Draw(gfxContext* aContext, const ImageRegion& aRegion,
                    GraphicsFilter aFilter, uint32_t aImageFlags)
{
  PROFILER_LABEL("imgFrame", "Draw",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(!aRegion.Rect().IsEmpty(), "Drawing empty region!");
  NS_ASSERTION(!aRegion.IsRestricted() ||
               !aRegion.Rect().Intersect(aRegion.Restriction()).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");
  NS_ASSERTION(!mPalettedImageData, "Directly drawing a paletted image!");

  MonitorAutoLock lock(mMonitor);

  nsIntMargin padding(mOffset.y,
                      mImageSize.width - (mOffset.x + mSize.width),
                      mImageSize.height - (mOffset.y + mSize.height),
                      mOffset.x);

  bool doPadding = padding != nsIntMargin(0,0,0,0);
  bool doPartialDecode = !IsImageCompleteInternal();

  if (mSinglePixel && !doPadding && !doPartialDecode) {
    if (mSinglePixelColor.a == 0.0) {
      return true;
    }
    RefPtr<DrawTarget> dt = aContext->GetDrawTarget();
    dt->FillRect(ToRect(aRegion.Rect()),
                 ColorPattern(mSinglePixelColor),
                 DrawOptions(1.0f,
                             CompositionOpForOp(aContext->CurrentOperator())));
    return true;
  }

  RefPtr<SourceSurface> surf = GetSurfaceInternal();
  if (!surf && !mSinglePixel) {
    return false;
  }

  gfxRect imageRect(0, 0, mImageSize.width, mImageSize.height);
  bool doTile = !imageRect.Contains(aRegion.Rect()) &&
                !(aImageFlags & imgIContainer::FLAG_CLAMP);
  ImageRegion region(aRegion);
  // SurfaceForDrawing changes the current transform, and we need it to still
  // be changed when we call gfxUtils::DrawPixelSnapped. We still need to
  // restore it before returning though.
  // XXXjwatt In general having functions require someone further up the stack
  // to undo transform changes that they make is bad practice. We should
  // change how this code works.
  gfxContextMatrixAutoSaveRestore autoSR(aContext);
  SurfaceWithFormat surfaceResult =
    SurfaceForDrawing(doPadding, doPartialDecode, doTile, aContext,
                      padding, imageRect, region, surf);

  if (surfaceResult.IsValid()) {
    gfxUtils::DrawPixelSnapped(aContext, surfaceResult.mDrawable,
                               imageRect.Size(), region, surfaceResult.mFormat,
                               aFilter, aImageFlags);
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

  // clamp to bounds, in case someone sends a bogus updateRect (I'm looking at
  // you, gif decoder)
  nsIntRect boundsRect(mOffset, nsIntSize(mSize.width, mSize.height));
  mDecoded.IntersectRect(mDecoded, boundsRect);

  // If the image is now complete, wake up anyone who's waiting.
  if (IsImageCompleteInternal()) {
    mMonitor.NotifyAll();
  }

  return NS_OK;
}

void
imgFrame::Finish(Opacity aFrameOpacity /* = Opacity::SOME_TRANSPARENCY */,
                 DisposalMethod aDisposalMethod /* = DisposalMethod::KEEP */,
                 int32_t aRawTimeout /* = 0 */,
                 BlendMethod aBlendMethod /* = BlendMethod::OVER */)
{
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mLockCount > 0, "Image data should be locked");

  if (aFrameOpacity == Opacity::OPAQUE) {
    mHasNoAlpha = true;
  }

  mDisposalMethod = aDisposalMethod;
  mTimeout = aRawTimeout;
  mBlendMethod = aBlendMethod;
  ImageUpdatedInternal(GetRect());
}

nsIntRect imgFrame::GetRect() const
{
  return nsIntRect(mOffset, nsIntSize(mSize.width, mSize.height));
}

int32_t
imgFrame::GetStride() const
{
  mMonitor.AssertCurrentThreadOwns();

  if (mImageSurface) {
    return mImageSurface->Stride();
  }

  return VolatileSurfaceStride(mSize, mFormat);
}

SurfaceFormat imgFrame::GetFormat() const
{
  MonitorAutoLock lock(mMonitor);
  return mFormat;
}

uint32_t imgFrame::GetImageBytesPerRow() const
{
  mMonitor.AssertCurrentThreadOwns();

  if (mVBuf)
    return mSize.width * BytesPerPixel(mFormat);

  if (mPaletteDepth)
    return mSize.width;

  return 0;
}

uint32_t imgFrame::GetImageDataLength() const
{
  return GetImageBytesPerRow() * mSize.height;
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

  if (mImageSurface) {
    *aData = mVBufPtr;
    MOZ_ASSERT(*aData, "mImageSurface is non-null, but mVBufPtr is null in GetImageData");
  } else if (mPalettedImageData) {
    *aData = mPalettedImageData + PaletteDataLength();
    MOZ_ASSERT(*aData, "mPalettedImageData is non-null, but result is null in GetImageData");
  } else {
    MOZ_ASSERT(false, "Have neither mImageSurface nor mPalettedImageData in GetImageData");
    *aData = nullptr;
  }

  *aLength = GetImageDataLength();
}

uint8_t* imgFrame::GetImageData() const
{
  uint8_t *data;
  uint32_t length;
  GetImageData(&data, &length);
  return data;
}

bool imgFrame::GetIsPaletted() const
{
  return mPalettedImageData != nullptr;
}

void imgFrame::GetPaletteData(uint32_t **aPalette, uint32_t *length) const
{
  AssertImageDataLocked();

  if (!mPalettedImageData) {
    *aPalette = nullptr;
    *length = 0;
  } else {
    *aPalette = (uint32_t *) mPalettedImageData;
    *length = PaletteDataLength();
  }
}

uint32_t* imgFrame::GetPaletteData() const
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

  // If we're the first lock, but have an image surface, we're OK.
  if (mImageSurface) {
    mVBufPtr = mVBuf;
    return NS_OK;
  }

  // Paletted images don't have surfaces, so there's nothing to do.
  if (mPalettedImageData) {
    return NS_OK;
  }

  return Deoptimize();
}

nsresult
imgFrame::Deoptimize()
{
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mImageSurface);

  if (!mImageSurface) {
    if (mVBuf) {
      VolatileBufferPtr<uint8_t> ref(mVBuf);
      if (ref.WasBufferPurged())
        return NS_ERROR_FAILURE;

      mImageSurface = CreateLockedSurface(mVBuf, mSize, mFormat);
      if (!mImageSurface)
        return NS_ERROR_OUT_OF_MEMORY;
    }
    if (mOptSurface || mSinglePixel || mFormat == SurfaceFormat::R5G6B5) {
      SurfaceFormat format = mFormat;
      if (mFormat == SurfaceFormat::R5G6B5)
        format = SurfaceFormat::B8G8R8A8;

      // Recover the pixels
      RefPtr<VolatileBuffer> buf =
        AllocateBufferForImage(mSize, format);
      if (!buf) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      RefPtr<DataSourceSurface> surf =
        CreateLockedSurface(buf, mSize, format);
      if (!surf)
        return NS_ERROR_OUT_OF_MEMORY;

      DataSourceSurface::MappedSurface mapping;
      DebugOnly<bool> success =
        surf->Map(DataSourceSurface::MapType::WRITE, &mapping);
      NS_ASSERTION(success, "Failed to map surface");
      RefPtr<DrawTarget> target =
        Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                         mapping.mData,
                                         mSize,
                                         mapping.mStride,
                                         format);

      Rect rect(0, 0, mSize.width, mSize.height);
      if (mSinglePixel)
        target->FillRect(rect, ColorPattern(mSinglePixelColor),
                         DrawOptions(1.0f, CompositionOp::OP_SOURCE));
      else if (mFormat == SurfaceFormat::R5G6B5)
        target->DrawSurface(mImageSurface, rect, rect);
      else
        target->DrawSurface(mOptSurface, rect, rect,
                            DrawSurfaceOptions(),
                            DrawOptions(1.0f, CompositionOp::OP_SOURCE));
      target->Flush();
      surf->Unmap();

      mFormat = format;
      mVBuf = buf;
      mImageSurface = surf;
      mOptSurface = nullptr;
    }
  }

  mVBufPtr = mVBuf;
  return NS_OK;
}

void
imgFrame::AssertImageDataLocked() const
{
#ifdef DEBUG
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mLockCount > 0, "Image data should be locked");
#endif
}

class UnlockImageDataRunnable : public nsRunnable
{
public:
  explicit UnlockImageDataRunnable(imgFrame* aTarget)
    : mTarget(aTarget)
  {
    MOZ_ASSERT(mTarget);
  }

  NS_IMETHOD Run() { return mTarget->UnlockImageData(); }

private:
  nsRefPtr<imgFrame> mTarget;
};

nsresult
imgFrame::UnlockImageData()
{
  MonitorAutoLock lock(mMonitor);

  MOZ_ASSERT(mLockCount > 0, "Unlocking an unlocked image!");
  if (mLockCount <= 0) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mLockCount > 1 || IsImageCompleteInternal() || mAborted,
             "Should have marked complete or aborted before unlocking");

  // If we're about to become unlocked, we don't need to hold on to our data
  // surface anymore. (But we don't need to do anything for paletted images,
  // which don't have surfaces.)
  if (mLockCount == 1 && !mPalettedImageData) {
    // We can't safely optimize off-main-thread, so create a runnable to do it.
    if (!NS_IsMainThread()) {
      nsCOMPtr<nsIRunnable> runnable = new UnlockImageDataRunnable(this);
      NS_DispatchToMainThread(runnable);
      return NS_OK;
    }

    // If we're using a surface format with alpha but the image has no alpha,
    // change the format. This doesn't change the underlying data at all, but
    // allows DrawTargets to avoid blending when drawing known opaque images.
    if (mHasNoAlpha && mFormat == SurfaceFormat::B8G8R8A8 && mImageSurface) {
      mFormat = SurfaceFormat::B8G8R8X8;
      mImageSurface = CreateLockedSurface(mVBuf, mSize, mFormat);
    }

    // Convert the data surface to a GPU surface or a single color if possible.
    // This will also release mImageSurface if possible.
    Optimize();
    
    // Allow the OS to release our data surface.
    mVBufPtr = nullptr;
  }

  mLockCount--;

  return NS_OK;
}

void
imgFrame::SetOptimizable()
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertImageDataLocked();
  mOptimizable = true;
}

Color
imgFrame::SinglePixelColor() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mSinglePixelColor;
}

bool
imgFrame::IsSinglePixel() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mSinglePixel;
}

TemporaryRef<SourceSurface>
imgFrame::GetSurface()
{
  MonitorAutoLock lock(mMonitor);
  return GetSurfaceInternal();
}

TemporaryRef<SourceSurface>
imgFrame::GetSurfaceInternal()
{
  mMonitor.AssertCurrentThreadOwns();

  if (mOptSurface) {
    if (mOptSurface->IsValid())
      return mOptSurface;
    else
      mOptSurface = nullptr;
  }

  if (mImageSurface)
    return mImageSurface;

  if (!mVBuf)
    return nullptr;

  VolatileBufferPtr<char> buf(mVBuf);
  if (buf.WasBufferPurged())
    return nullptr;

  return CreateLockedSurface(mVBuf, mSize, mFormat);
}

TemporaryRef<DrawTarget>
imgFrame::GetDrawTarget()
{
  MonitorAutoLock lock(mMonitor);

  uint8_t* data;
  uint32_t length;
  GetImageDataInternal(&data, &length);
  if (!data) {
    return nullptr;
  }

  int32_t stride = GetStride();
  return gfxPlatform::GetPlatform()->
    CreateDrawTargetForData(data, mSize, stride, mFormat);
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
                       mBlendMethod, mDisposalMethod, hasAlpha);
}

ScalingData
imgFrame::GetScalingData() const
{
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mLockCount > 0, "Image data should be locked");
  MOZ_ASSERT(!GetIsPaletted(), "GetScalingData can't handle paletted images");

  uint8_t* data;
  uint32_t length;
  GetImageDataInternal(&data, &length);

  return ScalingData(data, mSize, GetImageBytesPerRow(), mFormat);
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
imgFrame::IsImageComplete() const
{
  MonitorAutoLock lock(mMonitor);
  return IsImageCompleteInternal();
}

void
imgFrame::WaitUntilComplete() const
{
  MonitorAutoLock lock(mMonitor);

  while (true) {
    // Return if we're aborted or complete.
    if (mAborted || IsImageCompleteInternal()) {
      return;
    }

    // Not complete yet, so we'll have to wait.
    mMonitor.Wait();
  }
}

bool
imgFrame::IsImageCompleteInternal() const
{
  mMonitor.AssertCurrentThreadOwns();
  return mDecoded.IsEqualInterior(nsIntRect(mOffset.x, mOffset.y,
                                            mSize.width, mSize.height));
}

bool imgFrame::GetCompositingFailed() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mCompositingFailed;
}

void imgFrame::SetCompositingFailed(bool val)
{
  MOZ_ASSERT(NS_IsMainThread());
  mCompositingFailed = val;
}

size_t
imgFrame::SizeOfExcludingThis(gfxMemoryLocation aLocation,
                              MallocSizeOf aMallocSizeOf) const
{
  MonitorAutoLock lock(mMonitor);

  // aMallocSizeOf is only used if aLocation==gfxMemoryLocation::IN_PROCESS_HEAP.  It
  // should be nullptr otherwise.
  MOZ_ASSERT(
    (aLocation == gfxMemoryLocation::IN_PROCESS_HEAP &&  aMallocSizeOf) ||
    (aLocation != gfxMemoryLocation::IN_PROCESS_HEAP && !aMallocSizeOf),
    "mismatch between aLocation and aMallocSizeOf");

  size_t n = 0;

  if (mPalettedImageData && aLocation == gfxMemoryLocation::IN_PROCESS_HEAP) {
    n += aMallocSizeOf(mPalettedImageData);
  }
  if (mImageSurface && aLocation == gfxMemoryLocation::IN_PROCESS_HEAP) {
    n += aMallocSizeOf(mImageSurface);
  }
  if (mOptSurface && aLocation == gfxMemoryLocation::IN_PROCESS_HEAP) {
    n += aMallocSizeOf(mOptSurface);
  }

  if (mVBuf && aLocation == gfxMemoryLocation::IN_PROCESS_HEAP) {
    n += aMallocSizeOf(mVBuf);
    n += mVBuf->HeapSizeOfExcludingThis(aMallocSizeOf);
  }

  if (mVBuf && aLocation == gfxMemoryLocation::IN_PROCESS_NONHEAP) {
    n += mVBuf->NonHeapSizeOfExcludingThis();
  }

  return n;
}

} // namespace image
} // namespace mozilla
