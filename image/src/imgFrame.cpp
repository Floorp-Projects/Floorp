/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgFrame.h"
#include "DiscardTracker.h"

#include "prenv.h"

#include "gfxPlatform.h"
#include "gfxUtils.h"

static bool gDisableOptimize = false;

#include "cairo.h"
#include "GeckoProfiler.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "nsMargin.h"
#include "mozilla/CheckedInt.h"

#if defined(XP_WIN)

#include "gfxWindowsPlatform.h"

/* Whether to use the windows surface; only for desktop win32 */
#define USE_WIN_SURFACE 1

static uint32_t gTotalDDBs = 0;
static uint32_t gTotalDDBSize = 0;
// only use up a maximum of 64MB in DDBs
#define kMaxDDBSize (64*1024*1024)
// and don't let anything in that's bigger than 4MB
#define kMaxSingleDDBSize (4*1024*1024)

#endif

using namespace mozilla;
using namespace mozilla::image;

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

// Returns whether we should, at this time, use image surfaces instead of
// optimized platform-specific surfaces.
static bool ShouldUseImageSurfaces()
{
#if defined(USE_WIN_SURFACE)
  static const DWORD kGDIObjectsHighWaterMark = 7000;

  if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
      gfxWindowsPlatform::RENDER_DIRECT2D) {
    return true;
  }

  // at 7000 GDI objects, stop allocating normal images to make sure
  // we never hit the 10k hard limit.
  // GetCurrentProcess() just returns (HANDLE)-1, it's inlined afaik
  DWORD count = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
  if (count == 0 ||
      count > kGDIObjectsHighWaterMark)
  {
    // either something's broken (count == 0),
    // or we hit our high water mark; disable
    // image allocations for a bit.
    return true;
  }
#endif

  return false;
}

imgFrame::imgFrame() :
  mDecoded(0, 0, 0, 0),
  mDirtyMutex("imgFrame::mDirty"),
  mPalettedImageData(nullptr),
  mSinglePixelColor(0),
  mTimeout(100),
  mDisposalMethod(0), /* imgIContainer::kDisposeNotSpecified */
  mLockCount(0),
  mBlendMethod(1), /* imgIContainer::kBlendOver */
  mSinglePixel(false),
  mFormatChanged(false),
  mCompositingFailed(false),
  mNonPremult(false),
#ifdef USE_WIN_SURFACE
  mIsDDBSurface(false),
#endif
  mInformedDiscardTracker(false),
  mDirty(false)
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
#ifdef USE_WIN_SURFACE
  if (mIsDDBSurface) {
      gTotalDDBs--;
      gTotalDDBSize -= mSize.width * mSize.height * 4;
  }
#endif

  if (mInformedDiscardTracker) {
    DiscardTracker::InformAllocation(-4 * mSize.height * mSize.width);
  }
}

nsresult imgFrame::Init(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight,
                        gfxImageFormat aFormat, uint8_t aPaletteDepth /* = 0 */)
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
    // For Windows, we must create the device surface first (if we're
    // going to) so that the image surface can wrap it.  Can't be done
    // the other way around.
#ifdef USE_WIN_SURFACE
    if (!ShouldUseImageSurfaces()) {
      mWinSurface = new gfxWindowsSurface(gfxIntSize(mSize.width, mSize.height), mFormat);
      if (mWinSurface && mWinSurface->CairoStatus() == 0) {
        // no error
        mImageSurface = mWinSurface->GetAsImageSurface();
      } else {
        mWinSurface = nullptr;
      }
    }
#endif

    // For other platforms we create the image surface first and then
    // possibly wrap it in a device surface.  This branch is also used
    // on Windows if we're not using device surfaces or if we couldn't
    // create one.
    if (!mImageSurface)
      mImageSurface = new gfxImageSurface(gfxIntSize(mSize.width, mSize.height), mFormat);

    if (!mImageSurface || mImageSurface->CairoStatus()) {
      mImageSurface = nullptr;
      // guess
      if (!mImageSurface) {
        NS_WARNING("Allocation of gfxImageSurface should succeed");
      } else if (!mImageSurface->CairoStatus()) {
        NS_WARNING("gfxImageSurface should have good CairoStatus");
      }
      return NS_ERROR_OUT_OF_MEMORY;
    }

#ifdef XP_MACOSX
    if (!ShouldUseImageSurfaces()) {
      mQuartzSurface = new gfxQuartzImageSurface(mImageSurface);
    }
#endif
  }

  // Inform the discard tracker that we've allocated some memory, but only if
  // we're not a paletted image (paletted images are not usually large and are
  // used only for animated frames, which we don't discard).
  if (!mPalettedImageData) {
    DiscardTracker::InformAllocation(4 * mSize.width * mSize.height);
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
    uint32_t *imgData = (uint32_t*) mImageSurface->Data();
    uint32_t firstPixel = * (uint32_t*) imgData;
    uint32_t pixelCount = mSize.width * mSize.height + 1;

    while (--pixelCount && *imgData++ == firstPixel)
      ;

    if (pixelCount == 0) {
      // all pixels were the same
      if (mFormat == gfxImageFormatARGB32 ||
          mFormat == gfxImageFormatRGB24)
      {
        // Should already be premult if desired.
        gfxRGBA::PackedColorType inputType = gfxRGBA::PACKED_XRGB;
        if (mFormat == gfxImageFormatARGB32)
          inputType = gfxRGBA::PACKED_ARGB_PREMULTIPLIED;

        mSinglePixelColor = gfxRGBA(firstPixel, inputType);

        mSinglePixel = true;

        // blow away the older surfaces (if they exist), to release their memory
        mImageSurface = nullptr;
        mOptSurface = nullptr;
#ifdef USE_WIN_SURFACE
        mWinSurface = nullptr;
#endif
#ifdef XP_MACOSX
        mQuartzSurface = nullptr;
#endif

        // We just dumped most of our allocated memory, so tell the discard
        // tracker that we're not using any at all.
        if (mInformedDiscardTracker) {
          DiscardTracker::InformAllocation(-4 * mSize.width * mSize.height);
          mInformedDiscardTracker = false;
        }

        return NS_OK;
      }
    }

    // if it's not RGB24/ARGB32, don't optimize, but we never hit this at the moment
  }

  // if we're being forced to use image surfaces due to
  // resource constraints, don't try to optimize beyond same-pixel.
  if (ShouldUseImageSurfaces())
    return NS_OK;

  mOptSurface = nullptr;

#ifdef USE_WIN_SURFACE
  // we need to special-case windows here, because windows has
  // a distinction between DIB and DDB and we want to use DDBs as much
  // as we can.
  if (mWinSurface) {
    // Don't do DDBs for large images; see bug 359147
    // Note that we bother with DDBs at all because they are much faster
    // on some systems; on others there isn't much of a speed difference
    // between DIBs and DDBs.
    //
    // Originally this just limited to 1024x1024; but that still
    // had us hitting overall total memory usage limits (which was
    // around 220MB on my intel shared memory system with 2GB RAM
    // and 16-128mb in use by the video card, so I can't make
    // heads or tails out of this limit).
    //
    // So instead, we clamp the max size to 64MB (this limit shuld
    // be made dynamic based on.. something.. as soon a we figure
    // out that something) and also limit each individual image to
    // be less than 4MB to keep very large images out of DDBs.

    // assume (almost -- we don't quadword-align) worst-case size
    uint32_t ddbSize = mSize.width * mSize.height * 4;
    if (ddbSize <= kMaxSingleDDBSize &&
        ddbSize + gTotalDDBSize <= kMaxDDBSize)
    {
      nsRefPtr<gfxWindowsSurface> wsurf = mWinSurface->OptimizeToDDB(nullptr, gfxIntSize(mSize.width, mSize.height), mFormat);
      if (wsurf) {
        gTotalDDBs++;
        gTotalDDBSize += ddbSize;
        mIsDDBSurface = true;
        mOptSurface = wsurf;
      }
    }
    if (!mOptSurface && !mFormatChanged) {
      // just use the DIB if the format has not changed
      mOptSurface = mWinSurface;
    }
  }
#endif

#ifdef XP_MACOSX
  if (mQuartzSurface) {
    mQuartzSurface->Flush();
    mOptSurface = mQuartzSurface;
  }
#endif

  if (mOptSurface == nullptr)
    mOptSurface = gfxPlatform::GetPlatform()->OptimizeImage(mImageSurface, mFormat);

  if (mOptSurface) {
    mImageSurface = nullptr;
#ifdef USE_WIN_SURFACE
    mWinSurface = nullptr;
#endif
#ifdef XP_MACOSX
    mQuartzSurface = nullptr;
#endif
  }

  return NS_OK;
}

static void
DoSingleColorFastPath(gfxContext*    aContext,
                      const gfxRGBA& aSinglePixelColor,
                      const gfxRect& aFill)
{
  // if a == 0, it's a noop
  if (aSinglePixelColor.a == 0.0)
    return;

  gfxContext::GraphicsOperator op = aContext->CurrentOperator();
  if (op == gfxContext::OPERATOR_OVER && aSinglePixelColor.a == 1.0) {
    aContext->SetOperator(gfxContext::OPERATOR_SOURCE);
  }

  aContext->SetDeviceColor(aSinglePixelColor);
  aContext->NewPath();
  aContext->Rectangle(aFill);
  aContext->Fill();
  aContext->SetOperator(op);
  aContext->SetDeviceColor(gfxRGBA(0,0,0,0));
}

imgFrame::SurfaceWithFormat
imgFrame::SurfaceForDrawing(bool               aDoPadding,
                            bool               aDoPartialDecode,
                            bool               aDoTile,
                            const nsIntMargin& aPadding,
                            gfxMatrix&         aUserSpaceToImageSpace,
                            gfxRect&           aFill,
                            gfxRect&           aSubimage,
                            gfxRect&           aSourceRect,
                            gfxRect&           aImageRect)
{
  gfxIntSize size(int32_t(aImageRect.Width()), int32_t(aImageRect.Height()));
  if (!aDoPadding && !aDoPartialDecode) {
    NS_ASSERTION(!mSinglePixel, "This should already have been handled");
    return SurfaceWithFormat(new gfxSurfaceDrawable(ThebesSurface(), size), mFormat);
  }

  gfxRect available = gfxRect(mDecoded.x, mDecoded.y, mDecoded.width, mDecoded.height);

  if (aDoTile || mSinglePixel) {
    // Create a temporary surface.
    // Give this surface an alpha channel because there are
    // transparent pixels in the padding or undecoded area
    gfxImageFormat format = gfxImageFormatARGB32;
    nsRefPtr<gfxASurface> surface =
      gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, gfxImageSurface::ContentFromFormat(format));
    if (!surface || surface->CairoStatus())
      return SurfaceWithFormat();

    // Fill 'available' with whatever we've got
    gfxContext tmpCtx(surface);
    tmpCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
    if (mSinglePixel) {
      tmpCtx.SetDeviceColor(mSinglePixelColor);
    } else {
      tmpCtx.SetSource(ThebesSurface(), gfxPoint(aPadding.left, aPadding.top));
    }
    tmpCtx.Rectangle(available);
    tmpCtx.Fill();

    return SurfaceWithFormat(new gfxSurfaceDrawable(surface, size), format);
  }

  // Not tiling, and we have a surface, so we can account for
  // padding and/or a partial decode just by twiddling parameters.
  // First, update our user-space fill rect.
  aSourceRect = aSourceRect.Intersect(available);
  gfxMatrix imageSpaceToUserSpace = aUserSpaceToImageSpace;
  imageSpaceToUserSpace.Invert();
  aFill = imageSpaceToUserSpace.Transform(aSourceRect);

  aSubimage = aSubimage.Intersect(available) - gfxPoint(aPadding.left, aPadding.top);
  aUserSpaceToImageSpace.Multiply(gfxMatrix().Translate(-gfxPoint(aPadding.left, aPadding.top)));
  aSourceRect = aSourceRect - gfxPoint(aPadding.left, aPadding.top);
  aImageRect = gfxRect(0, 0, mSize.width, mSize.height);

  gfxIntSize availableSize(mDecoded.width, mDecoded.height);
  return SurfaceWithFormat(new gfxSurfaceDrawable(ThebesSurface(),
                                                  availableSize),
                           mFormat);
}

void imgFrame::Draw(gfxContext *aContext, GraphicsFilter aFilter,
                    const gfxMatrix &aUserSpaceToImageSpace, const gfxRect& aFill,
                    const nsIntMargin &aPadding, const nsIntRect &aSubimage,
                    uint32_t aImageFlags)
{
  PROFILER_LABEL("image", "imgFrame::Draw");
  NS_ASSERTION(!aFill.IsEmpty(), "zero dest size --- fix caller");
  NS_ASSERTION(!aSubimage.IsEmpty(), "zero source size --- fix caller");
  NS_ASSERTION(!mPalettedImageData, "Directly drawing a paletted image!");

  bool doPadding = aPadding != nsIntMargin(0,0,0,0);
  bool doPartialDecode = !ImageComplete();

  if (mSinglePixel && !doPadding && !doPartialDecode) {
    DoSingleColorFastPath(aContext, mSinglePixelColor, aFill);
    return;
  }

  gfxMatrix userSpaceToImageSpace = aUserSpaceToImageSpace;
  gfxRect sourceRect = userSpaceToImageSpace.TransformBounds(aFill);
  gfxRect imageRect(0, 0, mSize.width + aPadding.LeftRight(),
                    mSize.height + aPadding.TopBottom());
  gfxRect subimage(aSubimage.x, aSubimage.y, aSubimage.width, aSubimage.height);
  gfxRect fill = aFill;

  NS_ASSERTION(!sourceRect.Intersect(subimage).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");

  bool doTile = !imageRect.Contains(sourceRect) &&
                !(aImageFlags & imgIContainer::FLAG_CLAMP);
  SurfaceWithFormat surfaceResult =
    SurfaceForDrawing(doPadding, doPartialDecode, doTile, aPadding,
                      userSpaceToImageSpace, fill, subimage, sourceRect,
                      imageRect);

  if (surfaceResult.IsValid()) {
    gfxUtils::DrawPixelSnapped(aContext, surfaceResult.mDrawable,
                               userSpaceToImageSpace,
                               subimage, sourceRect, imageRect, fill,
                               surfaceResult.mFormat, aFilter, aImageFlags);
  }
}

// This can be called from any thread, but not simultaneously.
nsresult imgFrame::ImageUpdated(const nsIntRect &aUpdateRect)
{
  MutexAutoLock lock(mDirtyMutex);

  mDecoded.UnionRect(mDecoded, aUpdateRect);

  // clamp to bounds, in case someone sends a bogus updateRect (I'm looking at
  // you, gif decoder)
  nsIntRect boundsRect(mOffset, mSize);
  mDecoded.IntersectRect(mDecoded, boundsRect);

  mDirty = true;

  return NS_OK;
}

bool imgFrame::GetIsDirty() const
{
  MutexAutoLock lock(mDirtyMutex);
  return mDirty;
}

nsIntRect imgFrame::GetRect() const
{
  return nsIntRect(mOffset, mSize);
}

gfxImageFormat imgFrame::GetFormat() const
{
  return mFormat;
}

bool imgFrame::GetNeedsBackground() const
{
  // We need a background painted if we have alpha or we're incomplete.
  return (mFormat == gfxImageFormatARGB32 || !ImageComplete());
}

uint32_t imgFrame::GetImageBytesPerRow() const
{
  if (mImageSurface)
    return mImageSurface->Stride();

  if (mPaletteDepth)
    return mSize.width;

  NS_ERROR("GetImageBytesPerRow called with mImageSurface == null and mPaletteDepth == 0");

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
    *aData = mImageSurface->Data();
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
  return mFormat == gfxImageFormatARGB32;
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

  if ((mOptSurface || mSinglePixel) && !mImageSurface) {
    // Recover the pixels
    mImageSurface = new gfxImageSurface(gfxIntSize(mSize.width, mSize.height),
                                        gfxImageFormatARGB32);
    if (!mImageSurface || mImageSurface->CairoStatus())
      return NS_ERROR_OUT_OF_MEMORY;

    gfxContext context(mImageSurface);
    context.SetOperator(gfxContext::OPERATOR_SOURCE);
    if (mSinglePixel)
      context.SetDeviceColor(mSinglePixelColor);
    else
      context.SetSource(mOptSurface);
    context.Paint();

    mOptSurface = nullptr;
#ifdef USE_WIN_SURFACE
    mWinSurface = nullptr;
#endif
#ifdef XP_MACOSX
    mQuartzSurface = nullptr;
#endif
  }

  // We might write to the bits in this image surface, so we need to make the
  // surface ready for that.
  if (mImageSurface)
    mImageSurface->Flush();

#ifdef USE_WIN_SURFACE
  if (mWinSurface)
    mWinSurface->Flush();
#endif

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

  // FIXME: Bug 795737
  // If this image has been drawn since we were locked, it has had snapshots
  // added, and we need to remove them before calling MarkDirty.
  if (mImageSurface)
    mImageSurface->Flush();

#ifdef USE_WIN_SURFACE
  if (mWinSurface)
    mWinSurface->Flush();
#endif

  // Assume we've been written to.
  if (mImageSurface)
    mImageSurface->MarkDirty();

#ifdef USE_WIN_SURFACE
  if (mWinSurface)
    mWinSurface->MarkDirty();
#endif

#ifdef XP_MACOSX
  // The quartz image surface (ab)uses the flush method to get the
  // cairo_image_surface data into a CGImage, so we have to call Flush() here.
  if (mQuartzSurface)
    mQuartzSurface->Flush();
#endif

  return NS_OK;
}

void imgFrame::ApplyDirtToSurfaces()
{
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mDirtyMutex);
  if (mDirty) {
    // FIXME: Bug 795737
    // If this image has been drawn since we were locked, it has had snapshots
    // added, and we need to remove them before calling MarkDirty.
    if (mImageSurface)
      mImageSurface->Flush();

#ifdef USE_WIN_SURFACE
    if (mWinSurface)
      mWinSurface->Flush();
#endif

    if (mImageSurface)
      mImageSurface->MarkDirty();

#ifdef USE_WIN_SURFACE
    if (mWinSurface)
      mWinSurface->MarkDirty();
#endif

#ifdef XP_MACOSX
    // The quartz image surface (ab)uses the flush method to get the
    // cairo_image_surface data into a CGImage, so we have to call Flush() here.
    if (mQuartzSurface)
      mQuartzSurface->Flush();
#endif

    mDirty = false;
  }
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
  MutexAutoLock lock(mDirtyMutex);

  return mDecoded.IsEqualInterior(nsIntRect(mOffset, mSize));
}

// A hint from the image decoders that this image has no alpha, even
// though we created is ARGB32.  This changes our format to RGB24,
// which in turn will cause us to Optimize() to RGB24.  Has no effect
// after Optimize() is called, though in all cases it will be just a
// performance win -- the pixels are still correct and have the A byte
// set to 0xff.
void imgFrame::SetHasNoAlpha()
{
  if (mFormat == gfxImageFormatARGB32) {
      mFormat = gfxImageFormatRGB24;
      mFormatChanged = true;
      ThebesSurface()->SetOpaqueRect(gfxRect(0, 0, mSize.width, mSize.height));
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
imgFrame::SizeOfExcludingThisWithComputedFallbackIfHeap(gfxMemoryLocation aLocation, mozilla::MallocSizeOf aMallocSizeOf) const
{
  // aMallocSizeOf is only used if aLocation==GFX_MEMORY_IN_PROCESS_HEAP.  It
  // should be nullptr otherwise.
  NS_ABORT_IF_FALSE(
    (aLocation == GFX_MEMORY_IN_PROCESS_HEAP &&  aMallocSizeOf) ||
    (aLocation != GFX_MEMORY_IN_PROCESS_HEAP && !aMallocSizeOf),
    "mismatch between aLocation and aMallocSizeOf");

  size_t n = 0;

  if (mPalettedImageData && aLocation == GFX_MEMORY_IN_PROCESS_HEAP) {
    size_t n2 = aMallocSizeOf(mPalettedImageData);
    if (n2 == 0) {
      n2 = GetImageDataLength() + PaletteDataLength();
    }
    n += n2;
  }

#ifdef USE_WIN_SURFACE
  if (mWinSurface && aLocation == mWinSurface->GetMemoryLocation()) {
    n += mWinSurface->KnownMemoryUsed();
  } else
#endif
#ifdef XP_MACOSX
  if (mQuartzSurface && aLocation == GFX_MEMORY_IN_PROCESS_HEAP) {
    n += mSize.width * mSize.height * 4;
  } else
#endif
  if (mImageSurface && aLocation == mImageSurface->GetMemoryLocation()) {
    size_t n2 = 0;
    if (aLocation == GFX_MEMORY_IN_PROCESS_HEAP) { // HEAP: measure
      n2 = mImageSurface->SizeOfIncludingThis(aMallocSizeOf);
    }
    if (n2 == 0) {  // non-HEAP or computed fallback for HEAP
      n2 = mImageSurface->KnownMemoryUsed();
    }
    n += n2;
  }

  if (mOptSurface && aLocation == mOptSurface->GetMemoryLocation()) {
    size_t n2 = 0;
    if (aLocation == GFX_MEMORY_IN_PROCESS_HEAP &&
        mOptSurface->SizeOfIsMeasured()) {
      // HEAP: measure (but only if the sub-class is capable of measuring)
      n2 = mOptSurface->SizeOfIncludingThis(aMallocSizeOf);
    }
    if (n2 == 0) {  // non-HEAP or computed fallback for HEAP
      n2 = mOptSurface->KnownMemoryUsed();
    }
    n += n2;
  }

  return n;
}
