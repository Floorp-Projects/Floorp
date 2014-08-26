/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgFrame.h"
#include "ImageRegion.h"
#include "DiscardTracker.h"

#include "prenv.h"

#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "gfxAlphaRecovery.h"

static bool gDisableOptimize = false;

#include "GeckoProfiler.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "nsMargin.h"
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

imgFrame::imgFrame() :
  mDecoded(0, 0, 0, 0),
  mDecodedMutex("imgFrame::mDecoded"),
  mPalettedImageData(nullptr),
  mTimeout(100),
  mDisposalMethod(0), /* imgIContainer::kDisposeNotSpecified */
  mLockCount(0),
  mBlendMethod(1), /* imgIContainer::kBlendOver */
  mSinglePixel(false),
  mCompositingFailed(false),
  mNonPremult(false),
  mDiscardable(false),
  mInformedDiscardTracker(false)
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
  moz_free(mPalettedImageData);
  mPalettedImageData = nullptr;

  if (mInformedDiscardTracker) {
    DiscardTracker::InformDeallocation(4 * mSize.height * mSize.width);
  }
}

nsresult imgFrame::Init(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight,
                        SurfaceFormat aFormat, uint8_t aPaletteDepth /* = 0 */)
{
  // assert for properties that should be verified by decoders, warn for properties related to bad content
  if (!AllowedImageSize(aWidth, aHeight)) {
    NS_WARNING("Should have legal image size");
    return NS_ERROR_FAILURE;
  }

  mOffset.MoveTo(aX, aY);
  mSize.SizeTo(aWidth, aHeight);

  mFormat = aFormat;
  mPaletteDepth = aPaletteDepth;

  if (aPaletteDepth != 0) {
    // We're creating for a paletted image.
    if (aPaletteDepth > 8) {
      NS_WARNING("Should have legal palette depth");
      NS_ERROR("This Depth is not supported");
      return NS_ERROR_FAILURE;
    }

    // Use the fallible allocator here
    mPalettedImageData = (uint8_t*)moz_malloc(PaletteDataLength() + GetImageDataLength());
    if (!mPalettedImageData)
      NS_WARNING("moz_malloc for paletted image data should succeed");
    NS_ENSURE_TRUE(mPalettedImageData, NS_ERROR_OUT_OF_MEMORY);
  } else {
    // Inform the discard tracker that we are going to allocate some memory.
    if (!DiscardTracker::TryAllocation(4 * mSize.width * mSize.height)) {
      NS_WARNING("Exceed the hard limit of decode image size");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!mImageSurface) {
      mVBuf = AllocateBufferForImage(mSize, mFormat);
      if (!mVBuf) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      if (mVBuf->OnHeap()) {
        int32_t stride = VolatileSurfaceStride(mSize, mFormat);
        VolatileBufferPtr<uint8_t> ptr(mVBuf);
        memset(ptr, 0, stride * mSize.height);
      }
      mImageSurface = CreateLockedSurface(mVBuf, mSize, mFormat);
    }

    if (!mImageSurface) {
      NS_WARNING("Failed to create VolatileDataSourceSurface");
      // Image surface allocation is failed, need to return
      // the booked buffer size.
      DiscardTracker::InformDeallocation(4 * mSize.width * mSize.height);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mInformedDiscardTracker = true;
  }

  return NS_OK;
}

nsresult imgFrame::Optimize()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gDisableOptimize)
    return NS_OK;

  if (mPalettedImageData || mOptSurface || mSinglePixel)
    return NS_OK;

  // Don't do single-color opts on non-premult data.
  // Cairo doesn't support non-premult single-colors.
  if (mNonPremult)
    return NS_OK;

  /* Figure out if the entire image is a constant color */

  // this should always be true
  if (mImageSurface->Stride() == mSize.width * 4) {
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

        // We just dumped most of our allocated memory, so tell the discard
        // tracker that we're not using any at all.
        if (mInformedDiscardTracker) {
          DiscardTracker::InformDeallocation(4 * mSize.width * mSize.height);
          mInformedDiscardTracker = false;
        }

        return NS_OK;
      }
    }

    // if it's not RGB24/ARGB32, don't optimize, but we never hit this at the moment
  }

#ifdef ANDROID
  SurfaceFormat optFormat =
    gfxPlatform::GetPlatform()->Optimal2DFormatForContent(gfxContentType::COLOR);

  if (!GetHasAlpha() && optFormat == SurfaceFormat::R5G6B5) {
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

  return NS_OK;
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
      target->FillRect(ToRect(aRegion.Rect()), ColorPattern(mSinglePixelColor),
                       DrawOptions(1.0f, CompositionOp::OP_SOURCE));
    } else {
      SurfacePattern pattern(aSurface,
                             ExtendMode::REPEAT,
                             ToMatrix(aContext->CurrentMatrix()));
      target->FillRect(ToRect(aRegion.Rect()), pattern);
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
                    const nsIntMargin& aPadding, GraphicsFilter aFilter,
                    uint32_t aImageFlags)
{
  PROFILER_LABEL("imgFrame", "Draw",
    js::ProfileEntry::Category::GRAPHICS);

  NS_ASSERTION(!aRegion.Rect().IsEmpty(), "Drawing empty region!");
  NS_ASSERTION(!aRegion.IsRestricted() ||
               !aRegion.Rect().Intersect(aRegion.Restriction()).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");
  NS_ASSERTION(!mPalettedImageData, "Directly drawing a paletted image!");

  bool doPadding = aPadding != nsIntMargin(0,0,0,0);
  bool doPartialDecode = !ImageComplete();

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

  gfxRect imageRect(0, 0, mSize.width + aPadding.LeftRight(),
                    mSize.height + aPadding.TopBottom());

  RefPtr<SourceSurface> surf = GetSurface();
  if (!surf) {
    return false;
  }

  bool doTile = !imageRect.Contains(aRegion.Rect()) &&
                !(aImageFlags & imgIContainer::FLAG_CLAMP);
  ImageRegion region(aRegion);
  SurfaceWithFormat surfaceResult =
    SurfaceForDrawing(doPadding, doPartialDecode, doTile, aContext,
                      aPadding, imageRect, region, surf);

  if (surfaceResult.IsValid()) {
    gfxUtils::DrawPixelSnapped(aContext, surfaceResult.mDrawable,
                               imageRect.Size(), region, surfaceResult.mFormat,
                               aFilter, aImageFlags);
  }
  return true;
}

// This can be called from any thread, but not simultaneously.
nsresult imgFrame::ImageUpdated(const nsIntRect &aUpdateRect)
{
  MutexAutoLock lock(mDecodedMutex);

  mDecoded.UnionRect(mDecoded, aUpdateRect);

  // clamp to bounds, in case someone sends a bogus updateRect (I'm looking at
  // you, gif decoder)
  nsIntRect boundsRect(mOffset, nsIntSize(mSize.width, mSize.height));
  mDecoded.IntersectRect(mDecoded, boundsRect);

  return NS_OK;
}

nsIntRect imgFrame::GetRect() const
{
  return nsIntRect(mOffset, nsIntSize(mSize.width, mSize.height));
}

int32_t
imgFrame::GetStride() const
{
  if (mImageSurface) {
    return mImageSurface->Stride();
  }

  return VolatileSurfaceStride(mSize, mFormat);
}

SurfaceFormat imgFrame::GetFormat() const
{
  return mFormat;
}

bool imgFrame::GetNeedsBackground() const
{
  // We need a background painted if we have alpha or we're incomplete.
  return (mFormat == SurfaceFormat::B8G8R8A8 || !ImageComplete());
}

uint32_t imgFrame::GetImageBytesPerRow() const
{
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

void imgFrame::GetImageData(uint8_t **aData, uint32_t *length) const
{
  NS_ABORT_IF_FALSE(mLockCount != 0, "Can't GetImageData unless frame is locked");

  if (mImageSurface)
    *aData = mVBufPtr;
  else if (mPalettedImageData)
    *aData = mPalettedImageData + PaletteDataLength();
  else
    *aData = nullptr;

  *length = GetImageDataLength();
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

bool imgFrame::GetHasAlpha() const
{
  return mFormat == SurfaceFormat::B8G8R8A8;
}

void imgFrame::GetPaletteData(uint32_t **aPalette, uint32_t *length) const
{
  NS_ABORT_IF_FALSE(mLockCount != 0, "Can't GetPaletteData unless frame is locked");

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

nsresult imgFrame::LockImageData()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ABORT_IF_FALSE(mLockCount >= 0, "Unbalanced locks and unlocks");
  if (mLockCount < 0) {
    return NS_ERROR_FAILURE;
  }

  mLockCount++;

  // If we are not the first lock, there's nothing to do.
  if (mLockCount != 1) {
    return NS_OK;
  }

  // Paletted images don't have surfaces, so there's nothing to do.
  if (mPalettedImageData)
    return NS_OK;

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

nsresult imgFrame::UnlockImageData()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ABORT_IF_FALSE(mLockCount != 0, "Unlocking an unlocked image!");
  if (mLockCount == 0) {
    return NS_ERROR_FAILURE;
  }

  mLockCount--;

  NS_ABORT_IF_FALSE(mLockCount >= 0, "Unbalanced locks and unlocks");
  if (mLockCount < 0) {
    return NS_ERROR_FAILURE;
  }

  // If we are not the last lock, there's nothing to do.
  if (mLockCount != 0) {
    return NS_OK;
  }

  // Paletted images don't have surfaces, so there's nothing to do.
  if (mPalettedImageData)
    return NS_OK;

  mVBufPtr = nullptr;
  if (mVBuf && mDiscardable) {
    mImageSurface = nullptr;
  }

  return NS_OK;
}

void imgFrame::SetDiscardable()
{
  MOZ_ASSERT(mLockCount, "Expected to be locked when SetDiscardable is called");
  // Disabled elsewhere due to the cost of calling GetSourceSurfaceForSurface.
#ifdef MOZ_WIDGET_ANDROID
  mDiscardable = true;
#endif
}

TemporaryRef<SourceSurface>
imgFrame::GetSurface()
{
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
  MOZ_ASSERT(mLockCount >= 1, "Should lock before requesting a DrawTarget");

  uint8_t* data = GetImageData();
  if (!data) {
    return nullptr;
  }

  int32_t stride = GetStride();
  return gfxPlatform::GetPlatform()->
    CreateDrawTargetForData(data, mSize, stride, mFormat);
}

int32_t imgFrame::GetRawTimeout() const
{
  return mTimeout;
}

void imgFrame::SetRawTimeout(int32_t aTimeout)
{
  mTimeout = aTimeout;
}

int32_t imgFrame::GetFrameDisposalMethod() const
{
  return mDisposalMethod;
}

void imgFrame::SetFrameDisposalMethod(int32_t aFrameDisposalMethod)
{
  mDisposalMethod = aFrameDisposalMethod;
}

int32_t imgFrame::GetBlendMethod() const
{
  return mBlendMethod;
}

void imgFrame::SetBlendMethod(int32_t aBlendMethod)
{
  mBlendMethod = (int8_t)aBlendMethod;
}

// This can be called from any thread.
bool imgFrame::ImageComplete() const
{
  MutexAutoLock lock(mDecodedMutex);

  return mDecoded.IsEqualInterior(nsIntRect(mOffset.x, mOffset.y,
                                            mSize.width, mSize.height));
}

// A hint from the image decoders that this image has no alpha, even
// though we created is ARGB32.  This changes our format to RGB24,
// which in turn will cause us to Optimize() to RGB24.  Has no effect
// after Optimize() is called, though in all cases it will be just a
// performance win -- the pixels are still correct and have the A byte
// set to 0xff.
void imgFrame::SetHasNoAlpha()
{
  if (mFormat == SurfaceFormat::B8G8R8A8) {
    mFormat = SurfaceFormat::B8G8R8X8;
  }
}

void imgFrame::SetAsNonPremult(bool aIsNonPremult)
{
  mNonPremult = aIsNonPremult;
}

bool imgFrame::GetCompositingFailed() const
{
  return mCompositingFailed;
}

void imgFrame::SetCompositingFailed(bool val)
{
  mCompositingFailed = val;
}

// If |aLocation| indicates this is heap memory, we try to measure things with
// |aMallocSizeOf|.  If that fails (because the platform doesn't support it) or
// it's non-heap memory, we fall back to computing the size analytically.
size_t
imgFrame::SizeOfExcludingThisWithComputedFallbackIfHeap(gfxMemoryLocation aLocation, MallocSizeOf aMallocSizeOf) const
{
  // aMallocSizeOf is only used if aLocation==gfxMemoryLocation::IN_PROCESS_HEAP.  It
  // should be nullptr otherwise.
  NS_ABORT_IF_FALSE(
    (aLocation == gfxMemoryLocation::IN_PROCESS_HEAP &&  aMallocSizeOf) ||
    (aLocation != gfxMemoryLocation::IN_PROCESS_HEAP && !aMallocSizeOf),
    "mismatch between aLocation and aMallocSizeOf");

  size_t n = 0;

  if (mPalettedImageData && aLocation == gfxMemoryLocation::IN_PROCESS_HEAP) {
    size_t n2 = aMallocSizeOf(mPalettedImageData);
    if (n2 == 0) {
      n2 = GetImageDataLength() + PaletteDataLength();
    }
    n += n2;
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
