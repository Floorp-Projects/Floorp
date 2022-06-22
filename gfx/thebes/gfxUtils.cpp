/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxUtils.h"

#include "cairo.h"
#include "gfxContext.h"
#include "gfxEnv.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxDrawable.h"
#include "gfxQuad.h"
#include "imgIEncoder.h"
#include "mozilla/Base64.h"
#include "mozilla/StyleColorInlines.h"
#include "mozilla/Components.h"
#include "mozilla/dom/ImageEncoder.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/image/nsBMPEncoder.h"
#include "mozilla/image/nsICOEncoder.h"
#include "mozilla/image/nsJPEGEncoder.h"
#include "mozilla/image/nsPNGEncoder.h"
#include "mozilla/layers/SynchronousTask.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "nsAppRunner.h"
#include "nsComponentManagerUtils.h"
#include "nsIClipboardHelper.h"
#include "nsIFile.h"
#include "nsIGfxInfo.h"
#include "nsMimeTypes.h"
#include "nsPresContext.h"
#include "nsRegion.h"
#include "nsServiceManagerUtils.h"
#include "ImageContainer.h"
#include "ImageRegion.h"
#include "gfx2DGlue.h"

#ifdef XP_WIN
#  include "gfxWindowsPlatform.h"
#endif

using namespace mozilla;
using namespace mozilla::image;
using namespace mozilla::layers;
using namespace mozilla::gfx;

#undef compress
#include "mozilla/Compression.h"

using namespace mozilla::Compression;
extern "C" {

/**
 * Dump a raw image to the default log.  This function is exported
 * from libxul, so it can be called from any library in addition to
 * (of course) from a debugger.
 *
 * Note: this helper currently assumes that all 2-bytepp images are
 * r5g6b5, and that all 4-bytepp images are r8g8b8a8.
 */
NS_EXPORT
void mozilla_dump_image(void* bytes, int width, int height, int bytepp,
                        int strideBytes) {
  if (0 == strideBytes) {
    strideBytes = width * bytepp;
  }
  SurfaceFormat format;
  // TODO more flexible; parse string?
  switch (bytepp) {
    case 2:
      format = SurfaceFormat::R5G6B5_UINT16;
      break;
    case 4:
    default:
      format = SurfaceFormat::R8G8B8A8;
      break;
  }

  RefPtr<DataSourceSurface> surf = Factory::CreateWrappingDataSourceSurface(
      (uint8_t*)bytes, strideBytes, IntSize(width, height), format);
  gfxUtils::DumpAsDataURI(surf);
}
}

static bool MapSrcDest(DataSourceSurface* srcSurf, DataSourceSurface* destSurf,
                       DataSourceSurface::MappedSurface* out_srcMap,
                       DataSourceSurface::MappedSurface* out_destMap) {
  MOZ_ASSERT(srcSurf && destSurf);
  MOZ_ASSERT(out_srcMap && out_destMap);

  if (srcSurf->GetSize() != destSurf->GetSize()) {
    MOZ_ASSERT(false, "Width and height must match.");
    return false;
  }

  if (srcSurf == destSurf) {
    DataSourceSurface::MappedSurface map;
    if (!srcSurf->Map(DataSourceSurface::MapType::READ_WRITE, &map)) {
      NS_WARNING("Couldn't Map srcSurf/destSurf.");
      return false;
    }

    *out_srcMap = map;
    *out_destMap = map;
    return true;
  }

  // Map src for reading.
  DataSourceSurface::MappedSurface srcMap;
  if (!srcSurf->Map(DataSourceSurface::MapType::READ, &srcMap)) {
    NS_WARNING("Couldn't Map srcSurf.");
    return false;
  }

  // Map dest for writing.
  DataSourceSurface::MappedSurface destMap;
  if (!destSurf->Map(DataSourceSurface::MapType::WRITE, &destMap)) {
    NS_WARNING("Couldn't Map aDest.");
    srcSurf->Unmap();
    return false;
  }

  *out_srcMap = srcMap;
  *out_destMap = destMap;
  return true;
}

static void UnmapSrcDest(DataSourceSurface* srcSurf,
                         DataSourceSurface* destSurf) {
  if (srcSurf == destSurf) {
    srcSurf->Unmap();
  } else {
    srcSurf->Unmap();
    destSurf->Unmap();
  }
}

bool gfxUtils::PremultiplyDataSurface(DataSourceSurface* srcSurf,
                                      DataSourceSurface* destSurf) {
  MOZ_ASSERT(srcSurf && destSurf);

  DataSourceSurface::MappedSurface srcMap;
  DataSourceSurface::MappedSurface destMap;
  if (!MapSrcDest(srcSurf, destSurf, &srcMap, &destMap)) return false;

  PremultiplyData(srcMap.mData, srcMap.mStride, srcSurf->GetFormat(),
                  destMap.mData, destMap.mStride, destSurf->GetFormat(),
                  srcSurf->GetSize());

  UnmapSrcDest(srcSurf, destSurf);
  return true;
}

bool gfxUtils::UnpremultiplyDataSurface(DataSourceSurface* srcSurf,
                                        DataSourceSurface* destSurf) {
  MOZ_ASSERT(srcSurf && destSurf);

  DataSourceSurface::MappedSurface srcMap;
  DataSourceSurface::MappedSurface destMap;
  if (!MapSrcDest(srcSurf, destSurf, &srcMap, &destMap)) return false;

  UnpremultiplyData(srcMap.mData, srcMap.mStride, srcSurf->GetFormat(),
                    destMap.mData, destMap.mStride, destSurf->GetFormat(),
                    srcSurf->GetSize());

  UnmapSrcDest(srcSurf, destSurf);
  return true;
}

static bool MapSrcAndCreateMappedDest(
    DataSourceSurface* srcSurf, RefPtr<DataSourceSurface>* out_destSurf,
    DataSourceSurface::MappedSurface* out_srcMap,
    DataSourceSurface::MappedSurface* out_destMap) {
  MOZ_ASSERT(srcSurf);
  MOZ_ASSERT(out_destSurf && out_srcMap && out_destMap);

  // Ok, map source for reading.
  DataSourceSurface::MappedSurface srcMap;
  if (!srcSurf->Map(DataSourceSurface::MapType::READ, &srcMap)) {
    MOZ_ASSERT(false, "Couldn't Map srcSurf.");
    return false;
  }

  // Make our dest surface based on the src.
  RefPtr<DataSourceSurface> destSurf =
      Factory::CreateDataSourceSurfaceWithStride(
          srcSurf->GetSize(), srcSurf->GetFormat(), srcMap.mStride);
  if (NS_WARN_IF(!destSurf)) {
    return false;
  }

  DataSourceSurface::MappedSurface destMap;
  if (!destSurf->Map(DataSourceSurface::MapType::WRITE, &destMap)) {
    MOZ_ASSERT(false, "Couldn't Map destSurf.");
    srcSurf->Unmap();
    return false;
  }

  *out_destSurf = destSurf;
  *out_srcMap = srcMap;
  *out_destMap = destMap;
  return true;
}

already_AddRefed<DataSourceSurface> gfxUtils::CreatePremultipliedDataSurface(
    DataSourceSurface* srcSurf) {
  RefPtr<DataSourceSurface> destSurf;
  DataSourceSurface::MappedSurface srcMap;
  DataSourceSurface::MappedSurface destMap;
  if (!MapSrcAndCreateMappedDest(srcSurf, &destSurf, &srcMap, &destMap)) {
    MOZ_ASSERT(false, "MapSrcAndCreateMappedDest failed.");
    RefPtr<DataSourceSurface> surface(srcSurf);
    return surface.forget();
  }

  PremultiplyData(srcMap.mData, srcMap.mStride, srcSurf->GetFormat(),
                  destMap.mData, destMap.mStride, destSurf->GetFormat(),
                  srcSurf->GetSize());

  UnmapSrcDest(srcSurf, destSurf);
  return destSurf.forget();
}

already_AddRefed<DataSourceSurface> gfxUtils::CreateUnpremultipliedDataSurface(
    DataSourceSurface* srcSurf) {
  RefPtr<DataSourceSurface> destSurf;
  DataSourceSurface::MappedSurface srcMap;
  DataSourceSurface::MappedSurface destMap;
  if (!MapSrcAndCreateMappedDest(srcSurf, &destSurf, &srcMap, &destMap)) {
    MOZ_ASSERT(false, "MapSrcAndCreateMappedDest failed.");
    RefPtr<DataSourceSurface> surface(srcSurf);
    return surface.forget();
  }

  UnpremultiplyData(srcMap.mData, srcMap.mStride, srcSurf->GetFormat(),
                    destMap.mData, destMap.mStride, destSurf->GetFormat(),
                    srcSurf->GetSize());

  UnmapSrcDest(srcSurf, destSurf);
  return destSurf.forget();
}

void gfxUtils::ConvertBGRAtoRGBA(uint8_t* aData, uint32_t aLength) {
  MOZ_ASSERT((aLength % 4) == 0, "Loop below will pass srcEnd!");
  SwizzleData(aData, aLength, SurfaceFormat::B8G8R8A8, aData, aLength,
              SurfaceFormat::R8G8B8A8, IntSize(aLength / 4, 1));
}

#if !defined(MOZ_GFX_OPTIMIZE_MOBILE)
/**
 * This returns the fastest operator to use for solid surfaces which have no
 * alpha channel or their alpha channel is uniformly opaque.
 * This differs per render mode.
 */
static CompositionOp OptimalFillOp() {
#  ifdef XP_WIN
  if (gfxWindowsPlatform::GetPlatform()->IsDirect2DBackend()) {
    // D2D -really- hates operator source.
    return CompositionOp::OP_OVER;
  }
#  endif
  return CompositionOp::OP_SOURCE;
}

// EXTEND_PAD won't help us here; we have to create a temporary surface to hold
// the subimage of pixels we're allowed to sample.
static already_AddRefed<gfxDrawable> CreateSamplingRestrictedDrawable(
    gfxDrawable* aDrawable, gfxContext* aContext, const ImageRegion& aRegion,
    const SurfaceFormat aFormat, bool aUseOptimalFillOp) {
  AUTO_PROFILER_LABEL("CreateSamplingRestrictedDrawable", GRAPHICS);

  DrawTarget* destDrawTarget = aContext->GetDrawTarget();
  if (destDrawTarget->GetBackendType() == BackendType::DIRECT2D1_1) {
    return nullptr;
  }

  gfxRect clipExtents = aContext->GetClipExtents();

  // Inflate by one pixel because bilinear filtering will sample at most
  // one pixel beyond the computed image pixel coordinate.
  clipExtents.Inflate(1.0);

  gfxRect needed = aRegion.IntersectAndRestrict(clipExtents);
  needed.RoundOut();

  // if 'needed' is empty, nothing will be drawn since aFill
  // must be entirely outside the clip region, so it doesn't
  // matter what we do here, but we should avoid trying to
  // create a zero-size surface.
  if (needed.IsEmpty()) return nullptr;

  IntSize size(int32_t(needed.Width()), int32_t(needed.Height()));

  RefPtr<DrawTarget> target =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(size,
                                                                   aFormat);
  if (!target || !target->IsValid()) {
    return nullptr;
  }

  RefPtr<gfxContext> tmpCtx = gfxContext::CreateOrNull(target);
  MOZ_ASSERT(tmpCtx);  // already checked the target above

  if (aUseOptimalFillOp) {
    tmpCtx->SetOp(OptimalFillOp());
  }
  aDrawable->Draw(tmpCtx, needed - needed.TopLeft(), ExtendMode::REPEAT,
                  SamplingFilter::LINEAR, 1.0,
                  gfxMatrix::Translation(needed.TopLeft()));
  RefPtr<SourceSurface> surface = target->Snapshot();

  RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(
      surface, size, gfxMatrix::Translation(-needed.TopLeft()));
  return drawable.forget();
}
#endif  // !MOZ_GFX_OPTIMIZE_MOBILE

/* These heuristics are based on
 * Source/WebCore/platform/graphics/skia/ImageSkia.cpp:computeResamplingMode()
 */
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
static SamplingFilter ReduceResamplingFilter(SamplingFilter aSamplingFilter,
                                             int aImgWidth, int aImgHeight,
                                             float aSourceWidth,
                                             float aSourceHeight) {
  // Images smaller than this in either direction are considered "small" and
  // are not resampled ever (see below).
  const int kSmallImageSizeThreshold = 8;

  // The amount an image can be stretched in a single direction before we
  // say that it is being stretched so much that it must be a line or
  // background that doesn't need resampling.
  const float kLargeStretch = 3.0f;

  if (aImgWidth <= kSmallImageSizeThreshold ||
      aImgHeight <= kSmallImageSizeThreshold) {
    // Never resample small images. These are often used for borders and
    // rules (think 1x1 images used to make lines).
    return SamplingFilter::POINT;
  }

  if (aImgHeight * kLargeStretch <= aSourceHeight ||
      aImgWidth * kLargeStretch <= aSourceWidth) {
    // Large image tiling detected.

    // Don't resample if it is being tiled a lot in only one direction.
    // This is trying to catch cases where somebody has created a border
    // (which might be large) and then is stretching it to fill some part
    // of the page.
    if (fabs(aSourceWidth - aImgWidth) / aImgWidth < 0.5 ||
        fabs(aSourceHeight - aImgHeight) / aImgHeight < 0.5)
      return SamplingFilter::POINT;

    // The image is growing a lot and in more than one direction. Resampling
    // is slow and doesn't give us very much when growing a lot.
    return aSamplingFilter;
  }

  /* Some notes on other heuristics:
     The Skia backend also uses nearest for backgrounds that are stretched by
     a large amount. I'm not sure this is common enough for us to worry about
     now. It also uses nearest for backgrounds/avoids high quality for images
     that are very slightly scaled.  I'm also not sure that very slightly
     scaled backgrounds are common enough us to worry about.

     We don't currently have much support for doing high quality interpolation.
     The only place this currently happens is on Quartz and we don't have as
     much control over it as would be needed. Webkit avoids using high quality
     resampling during load. It also avoids high quality if the transformation
     is not just a scale and translation

     WebKit bug #40045 added code to avoid resampling different parts
     of an image with different methods by using a resampling hint size.
     It currently looks unused in WebKit but it's something to watch out for.
  */

  return aSamplingFilter;
}
#else
static SamplingFilter ReduceResamplingFilter(SamplingFilter aSamplingFilter,
                                             int aImgWidth, int aImgHeight,
                                             int aSourceWidth,
                                             int aSourceHeight) {
  // Just pass the filter through unchanged
  return aSamplingFilter;
}
#endif

#ifdef MOZ_WIDGET_COCOA
// Only prescale a temporary surface if we're going to repeat it often.
// Scaling is expensive on OS X and without prescaling, we'd scale
// every tile of the repeated rect. However, using a temp surface also
// potentially uses more memory if the scaled image is large. So only prescale
// on a temp surface if we know we're going to repeat the image in either the X
// or Y axis multiple times.
static bool ShouldUseTempSurface(Rect aImageRect, Rect aNeededRect) {
  int repeatX = aNeededRect.width / aImageRect.width;
  int repeatY = aNeededRect.height / aImageRect.height;
  return (repeatX >= 5) || (repeatY >= 5);
}

static bool PrescaleAndTileDrawable(gfxDrawable* aDrawable,
                                    gfxContext* aContext,
                                    const ImageRegion& aRegion, Rect aImageRect,
                                    const SamplingFilter aSamplingFilter,
                                    const SurfaceFormat aFormat,
                                    gfxFloat aOpacity, ExtendMode aExtendMode) {
  MatrixScales scaleFactor =
      aContext->CurrentMatrix().ScaleFactors().ConvertTo<float>();
  Matrix scaleMatrix = Matrix::Scaling(scaleFactor.xScale, scaleFactor.yScale);
  const float fuzzFactor = 0.01;

  // If we aren't scaling or translating, don't go down this path
  if ((FuzzyEqual(scaleFactor.xScale, 1.0f, fuzzFactor) &&
       FuzzyEqual(scaleFactor.yScale, 1.0f, fuzzFactor)) ||
      aContext->CurrentMatrix().HasNonAxisAlignedTransform()) {
    return false;
  }

  gfxRect clipExtents = aContext->GetClipExtents();

  // Inflate by one pixel because bilinear filtering will sample at most
  // one pixel beyond the computed image pixel coordinate.
  clipExtents.Inflate(1.0);

  gfxRect needed = aRegion.IntersectAndRestrict(clipExtents);
  Rect scaledNeededRect = scaleMatrix.TransformBounds(ToRect(needed));
  scaledNeededRect.RoundOut();
  if (scaledNeededRect.IsEmpty()) {
    return false;
  }

  Rect scaledImageRect = scaleMatrix.TransformBounds(aImageRect);
  if (!ShouldUseTempSurface(scaledImageRect, scaledNeededRect)) {
    return false;
  }

  IntSize scaledImageSize((int32_t)scaledImageRect.width,
                          (int32_t)scaledImageRect.height);
  if (scaledImageSize.width != scaledImageRect.width ||
      scaledImageSize.height != scaledImageRect.height) {
    // If the scaled image isn't pixel aligned, we'll get artifacts
    // so we have to take the slow path.
    return false;
  }

  RefPtr<DrawTarget> scaledDT =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          scaledImageSize, aFormat);
  if (!scaledDT || !scaledDT->IsValid()) {
    return false;
  }

  RefPtr<gfxContext> tmpCtx = gfxContext::CreateOrNull(scaledDT);
  MOZ_ASSERT(tmpCtx);  // already checked the target above

  scaledDT->SetTransform(scaleMatrix);
  gfxRect gfxImageRect(aImageRect.x, aImageRect.y, aImageRect.width,
                       aImageRect.height);

  // Since this is just the scaled image, we don't want to repeat anything yet.
  aDrawable->Draw(tmpCtx, gfxImageRect, ExtendMode::CLAMP, aSamplingFilter, 1.0,
                  gfxMatrix());

  RefPtr<SourceSurface> scaledImage = scaledDT->Snapshot();

  {
    gfxContextMatrixAutoSaveRestore autoSR(aContext);
    Matrix withoutScale = aContext->CurrentMatrix();
    DrawTarget* destDrawTarget = aContext->GetDrawTarget();

    // The translation still is in scaled units
    withoutScale.PreScale(1.0f / scaleFactor.xScale, 1.0f / scaleFactor.yScale);
    aContext->SetMatrix(withoutScale);

    DrawOptions drawOptions(aOpacity, aContext->CurrentOp(),
                            aContext->CurrentAntialiasMode());

    SurfacePattern scaledImagePattern(scaledImage, aExtendMode, Matrix(),
                                      aSamplingFilter);
    destDrawTarget->FillRect(scaledNeededRect, scaledImagePattern, drawOptions);
  }
  return true;
}
#endif  // MOZ_WIDGET_COCOA

/* static */
void gfxUtils::DrawPixelSnapped(gfxContext* aContext, gfxDrawable* aDrawable,
                                const gfxSize& aImageSize,
                                const ImageRegion& aRegion,
                                const SurfaceFormat aFormat,
                                SamplingFilter aSamplingFilter,
                                uint32_t aImageFlags, gfxFloat aOpacity,
                                bool aUseOptimalFillOp) {
  AUTO_PROFILER_LABEL("gfxUtils::DrawPixelSnapped", GRAPHICS);

  gfxRect imageRect(gfxPoint(0, 0), aImageSize);
  gfxRect region(aRegion.Rect());
  ExtendMode extendMode = aRegion.GetExtendMode();

  RefPtr<gfxDrawable> drawable = aDrawable;

  aSamplingFilter = ReduceResamplingFilter(aSamplingFilter, imageRect.Width(),
                                           imageRect.Height(), region.Width(),
                                           region.Height());

  // OK now, the hard part left is to account for the subimage sampling
  // restriction. If all the transforms involved are just integer
  // translations, then we assume no resampling will occur so there's
  // nothing to do.
  // XXX if only we had source-clipping in cairo!

  if (aContext->CurrentMatrix().HasNonIntegerTranslation()) {
    if ((extendMode != ExtendMode::CLAMP) ||
        !aRegion.RestrictionContains(imageRect)) {
      if (drawable->DrawWithSamplingRect(
              aContext->GetDrawTarget(), aContext->CurrentOp(),
              aContext->CurrentAntialiasMode(), aRegion.Rect(),
              aRegion.Restriction(), extendMode, aSamplingFilter, aOpacity)) {
        return;
      }

#ifdef MOZ_WIDGET_COCOA
      if (PrescaleAndTileDrawable(aDrawable, aContext, aRegion,
                                  ToRect(imageRect), aSamplingFilter, aFormat,
                                  aOpacity, extendMode)) {
        return;
      }
#endif

      // On Mobile, we don't ever want to do this; it has the potential for
      // allocating very large temporary surfaces, especially since we'll
      // do full-page snapshots often (see bug 749426).
#if !defined(MOZ_GFX_OPTIMIZE_MOBILE)
      RefPtr<gfxDrawable> restrictedDrawable = CreateSamplingRestrictedDrawable(
          aDrawable, aContext, aRegion, aFormat, aUseOptimalFillOp);
      if (restrictedDrawable) {
        drawable.swap(restrictedDrawable);

        // We no longer need to tile: Either we never needed to, or we already
        // filled a surface with the tiled pattern; this surface can now be
        // drawn without tiling.
        extendMode = ExtendMode::CLAMP;
      }
#endif
    }
  }

  drawable->Draw(aContext, aRegion.Rect(), extendMode, aSamplingFilter,
                 aOpacity, gfxMatrix());
}

/* static */
int gfxUtils::ImageFormatToDepth(gfxImageFormat aFormat) {
  switch (aFormat) {
    case SurfaceFormat::A8R8G8B8_UINT32:
      return 32;
    case SurfaceFormat::X8R8G8B8_UINT32:
      return 24;
    case SurfaceFormat::R5G6B5_UINT16:
      return 16;
    default:
      break;
  }
  return 0;
}

/*static*/
void gfxUtils::ClipToRegion(gfxContext* aContext, const nsIntRegion& aRegion) {
  aContext->NewPath();
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    const IntRect& r = iter.Get();
    aContext->Rectangle(gfxRect(r.X(), r.Y(), r.Width(), r.Height()));
  }
  aContext->Clip();
}

/*static*/
void gfxUtils::ClipToRegion(DrawTarget* aTarget, const nsIntRegion& aRegion) {
  uint32_t numRects = aRegion.GetNumRects();
  // If there is only one rect, then the region bounds are equivalent to the
  // contents. So just use push a single clip rect with the bounds.
  if (numRects == 1) {
    aTarget->PushClipRect(Rect(aRegion.GetBounds()));
    return;
  }

  // Check if the target's transform will preserve axis-alignment and
  // pixel-alignment for each rect. For now, just handle the common case
  // of integer translations.
  Matrix transform = aTarget->GetTransform();
  if (transform.IsIntegerTranslation()) {
    IntPoint translation = RoundedToInt(transform.GetTranslation());
    AutoTArray<IntRect, 16> rects;
    rects.SetLength(numRects);
    uint32_t i = 0;
    // Build the list of transformed rects by adding in the translation.
    for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
      IntRect rect = iter.Get();
      rect.MoveBy(translation);
      rects[i++] = rect;
    }
    aTarget->PushDeviceSpaceClipRects(rects.Elements(), rects.Length());
  } else {
    // The transform does not produce axis-aligned rects or a rect was not
    // pixel-aligned. So just build a path with all the rects and clip to it
    // instead.
    RefPtr<PathBuilder> pathBuilder = aTarget->CreatePathBuilder();
    for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
      AppendRectToPath(pathBuilder, Rect(iter.Get()));
    }
    RefPtr<Path> path = pathBuilder->Finish();
    aTarget->PushClip(path);
  }
}

/*static*/
float gfxUtils::ClampToScaleFactor(float aVal, bool aRoundDown) {
  // Arbitary scale factor limitation. We can increase this
  // for better scaling performance at the cost of worse
  // quality.
  static const float kScaleResolution = 2;

  // Negative scaling is just a flip and irrelevant to
  // our resolution calculation.
  if (aVal < 0.0) {
    aVal = -aVal;
  }

  bool inverse = false;
  if (aVal < 1.0) {
    inverse = true;
    aVal = 1 / aVal;
  }

  float power = logf(aVal) / logf(kScaleResolution);

  // If power is within 1e-5 of an integer, round to nearest to
  // prevent floating point errors, otherwise round up to the
  // next integer value.
  if (fabs(power - NS_round(power)) < 1e-5) {
    power = NS_round(power);
    // Use floor when we are either inverted or rounding down, but
    // not both.
  } else if (inverse != aRoundDown) {
    power = floor(power);
    // Otherwise, ceil when we are not inverted and not rounding
    // down, or we are inverted and rounding down.
  } else {
    power = ceil(power);
  }

  float scale = powf(kScaleResolution, power);

  if (inverse) {
    scale = 1 / scale;
  }

  return scale;
}

gfxMatrix gfxUtils::TransformRectToRect(const gfxRect& aFrom,
                                        const gfxPoint& aToTopLeft,
                                        const gfxPoint& aToTopRight,
                                        const gfxPoint& aToBottomRight) {
  gfxMatrix m;
  if (aToTopRight.y == aToTopLeft.y && aToTopRight.x == aToBottomRight.x) {
    // Not a rotation, so xy and yx are zero
    m._21 = m._12 = 0.0;
    m._11 = (aToBottomRight.x - aToTopLeft.x) / aFrom.Width();
    m._22 = (aToBottomRight.y - aToTopLeft.y) / aFrom.Height();
    m._31 = aToTopLeft.x - m._11 * aFrom.X();
    m._32 = aToTopLeft.y - m._22 * aFrom.Y();
  } else {
    NS_ASSERTION(
        aToTopRight.y == aToBottomRight.y && aToTopRight.x == aToTopLeft.x,
        "Destination rectangle not axis-aligned");
    m._11 = m._22 = 0.0;
    m._21 = (aToBottomRight.x - aToTopLeft.x) / aFrom.Height();
    m._12 = (aToBottomRight.y - aToTopLeft.y) / aFrom.Width();
    m._31 = aToTopLeft.x - m._21 * aFrom.Y();
    m._32 = aToTopLeft.y - m._12 * aFrom.X();
  }
  return m;
}

Matrix gfxUtils::TransformRectToRect(const gfxRect& aFrom,
                                     const IntPoint& aToTopLeft,
                                     const IntPoint& aToTopRight,
                                     const IntPoint& aToBottomRight) {
  Matrix m;
  if (aToTopRight.y == aToTopLeft.y && aToTopRight.x == aToBottomRight.x) {
    // Not a rotation, so xy and yx are zero
    m._12 = m._21 = 0.0;
    m._11 = (aToBottomRight.x - aToTopLeft.x) / aFrom.Width();
    m._22 = (aToBottomRight.y - aToTopLeft.y) / aFrom.Height();
    m._31 = aToTopLeft.x - m._11 * aFrom.X();
    m._32 = aToTopLeft.y - m._22 * aFrom.Y();
  } else {
    NS_ASSERTION(
        aToTopRight.y == aToBottomRight.y && aToTopRight.x == aToTopLeft.x,
        "Destination rectangle not axis-aligned");
    m._11 = m._22 = 0.0;
    m._21 = (aToBottomRight.x - aToTopLeft.x) / aFrom.Height();
    m._12 = (aToBottomRight.y - aToTopLeft.y) / aFrom.Width();
    m._31 = aToTopLeft.x - m._21 * aFrom.Y();
    m._32 = aToTopLeft.y - m._12 * aFrom.X();
  }
  return m;
}

/* This function is sort of shitty. We truncate doubles
 * to ints then convert those ints back to doubles to make sure that
 * they equal the doubles that we got in. */
bool gfxUtils::GfxRectToIntRect(const gfxRect& aIn, IntRect* aOut) {
  *aOut = IntRect(int32_t(aIn.X()), int32_t(aIn.Y()), int32_t(aIn.Width()),
                  int32_t(aIn.Height()));
  return gfxRect(aOut->X(), aOut->Y(), aOut->Width(), aOut->Height())
      .IsEqualEdges(aIn);
}

/* Clamp r to CAIRO_COORD_MIN .. CAIRO_COORD_MAX
 * these are to be device coordinates.
 *
 * Cairo is currently using 24.8 fixed point,
 * so -2^24 .. 2^24-1 is our valid
 */
/*static*/
void gfxUtils::ConditionRect(gfxRect& aRect) {
#define CAIRO_COORD_MAX (16777215.0)
#define CAIRO_COORD_MIN (-16777216.0)
  // if either x or y is way out of bounds;
  // note that we don't handle negative w/h here
  if (aRect.X() > CAIRO_COORD_MAX) {
    aRect.SetRectX(CAIRO_COORD_MAX, 0.0);
  }

  if (aRect.Y() > CAIRO_COORD_MAX) {
    aRect.SetRectY(CAIRO_COORD_MAX, 0.0);
  }

  if (aRect.X() < CAIRO_COORD_MIN) {
    aRect.SetWidth(aRect.XMost() - CAIRO_COORD_MIN);
    if (aRect.Width() < 0.0) {
      aRect.SetWidth(0.0);
    }
    aRect.MoveToX(CAIRO_COORD_MIN);
  }

  if (aRect.Y() < CAIRO_COORD_MIN) {
    aRect.SetHeight(aRect.YMost() - CAIRO_COORD_MIN);
    if (aRect.Height() < 0.0) {
      aRect.SetHeight(0.0);
    }
    aRect.MoveToY(CAIRO_COORD_MIN);
  }

  if (aRect.XMost() > CAIRO_COORD_MAX) {
    aRect.SetRightEdge(CAIRO_COORD_MAX);
  }

  if (aRect.YMost() > CAIRO_COORD_MAX) {
    aRect.SetBottomEdge(CAIRO_COORD_MAX);
  }
#undef CAIRO_COORD_MAX
#undef CAIRO_COORD_MIN
}

/*static*/
gfxQuad gfxUtils::TransformToQuad(const gfxRect& aRect,
                                  const mozilla::gfx::Matrix4x4& aMatrix) {
  gfxPoint points[4];

  points[0] = aMatrix.TransformPoint(aRect.TopLeft());
  points[1] = aMatrix.TransformPoint(aRect.TopRight());
  points[2] = aMatrix.TransformPoint(aRect.BottomRight());
  points[3] = aMatrix.TransformPoint(aRect.BottomLeft());

  // Could this ever result in lines that intersect? I don't think so.
  return gfxQuad(points[0], points[1], points[2], points[3]);
}

Matrix4x4 gfxUtils::SnapTransformTranslation(const Matrix4x4& aTransform,
                                             Matrix* aResidualTransform) {
  if (aResidualTransform) {
    *aResidualTransform = Matrix();
  }

  Matrix matrix2D;
  if (aTransform.CanDraw2D(&matrix2D) && !matrix2D.HasNonTranslation() &&
      matrix2D.HasNonIntegerTranslation()) {
    return Matrix4x4::From2D(
        SnapTransformTranslation(matrix2D, aResidualTransform));
  }

  return SnapTransformTranslation3D(aTransform, aResidualTransform);
}

Matrix gfxUtils::SnapTransformTranslation(const Matrix& aTransform,
                                          Matrix* aResidualTransform) {
  if (aResidualTransform) {
    *aResidualTransform = Matrix();
  }

  if (!aTransform.HasNonTranslation() &&
      aTransform.HasNonIntegerTranslation()) {
    auto snappedTranslation = IntPoint::Round(aTransform.GetTranslation());
    Matrix snappedMatrix =
        Matrix::Translation(snappedTranslation.x, snappedTranslation.y);
    if (aResidualTransform) {
      // set aResidualTransform so that aResidual * snappedMatrix == matrix2D.
      // (I.e., appying snappedMatrix after aResidualTransform gives the
      // ideal transform.)
      *aResidualTransform =
          Matrix::Translation(aTransform._31 - snappedTranslation.x,
                              aTransform._32 - snappedTranslation.y);
    }
    return snappedMatrix;
  }

  return aTransform;
}

Matrix4x4 gfxUtils::SnapTransformTranslation3D(const Matrix4x4& aTransform,
                                               Matrix* aResidualTransform) {
  if (aTransform.IsSingular() || aTransform.HasPerspectiveComponent() ||
      aTransform.HasNonTranslation() ||
      !aTransform.HasNonIntegerTranslation()) {
    // For a singular transform, there is no reversed matrix, so we
    // don't snap it.
    // For a perspective transform, the content is transformed in
    // non-linear, so we don't snap it too.
    return aTransform;
  }

  // Snap for 3D Transforms

  Point3D transformedOrigin = aTransform.TransformPoint(Point3D());

  // Compute the transformed snap by rounding the values of
  // transformed origin.
  auto transformedSnapXY =
      IntPoint::Round(transformedOrigin.x, transformedOrigin.y);
  Matrix4x4 inverse = aTransform;
  inverse.Invert();
  // see Matrix4x4::ProjectPoint()
  Float transformedSnapZ =
      inverse._33 == 0 ? 0
                       : (-(transformedSnapXY.x * inverse._13 +
                            transformedSnapXY.y * inverse._23 + inverse._43) /
                          inverse._33);
  Point3D transformedSnap =
      Point3D(transformedSnapXY.x, transformedSnapXY.y, transformedSnapZ);
  if (transformedOrigin == transformedSnap) {
    return aTransform;
  }

  // Compute the snap from the transformed snap.
  Point3D snap = inverse.TransformPoint(transformedSnap);
  if (snap.z > 0.001 || snap.z < -0.001) {
    // Allow some level of accumulated computation error.
    MOZ_ASSERT(inverse._33 == 0.0);
    return aTransform;
  }

  // The difference between the origin and snap is the residual transform.
  if (aResidualTransform) {
    // The residual transform is to translate the snap to the origin
    // of the content buffer.
    *aResidualTransform = Matrix::Translation(-snap.x, -snap.y);
  }

  // Translate transformed origin to transformed snap since the
  // residual transform would trnslate the snap to the origin.
  Point3D transformedShift = transformedSnap - transformedOrigin;
  Matrix4x4 result = aTransform;
  result.PostTranslate(transformedShift.x, transformedShift.y,
                       transformedShift.z);

  // For non-2d transform, residual translation could be more than
  // 0.5 pixels for every axis.

  return result;
}

Matrix4x4 gfxUtils::SnapTransform(const Matrix4x4& aTransform,
                                  const gfxRect& aSnapRect,
                                  Matrix* aResidualTransform) {
  if (aResidualTransform) {
    *aResidualTransform = Matrix();
  }

  Matrix matrix2D;
  if (aTransform.Is2D(&matrix2D)) {
    return Matrix4x4::From2D(
        SnapTransform(matrix2D, aSnapRect, aResidualTransform));
  }
  return aTransform;
}

Matrix gfxUtils::SnapTransform(const Matrix& aTransform,
                               const gfxRect& aSnapRect,
                               Matrix* aResidualTransform) {
  if (aResidualTransform) {
    *aResidualTransform = Matrix();
  }

  if (gfxSize(1.0, 1.0) <= aSnapRect.Size() &&
      aTransform.PreservesAxisAlignedRectangles()) {
    auto transformedTopLeft = IntPoint::Round(
        aTransform.TransformPoint(ToPoint(aSnapRect.TopLeft())));
    auto transformedTopRight = IntPoint::Round(
        aTransform.TransformPoint(ToPoint(aSnapRect.TopRight())));
    auto transformedBottomRight = IntPoint::Round(
        aTransform.TransformPoint(ToPoint(aSnapRect.BottomRight())));

    Matrix snappedMatrix = gfxUtils::TransformRectToRect(
        aSnapRect, transformedTopLeft, transformedTopRight,
        transformedBottomRight);

    if (aResidualTransform && !snappedMatrix.IsSingular()) {
      // set aResidualTransform so that aResidual * snappedMatrix == matrix2D.
      // (i.e., appying snappedMatrix after aResidualTransform gives the
      // ideal transform.
      Matrix snappedMatrixInverse = snappedMatrix;
      snappedMatrixInverse.Invert();
      *aResidualTransform = aTransform * snappedMatrixInverse;
    }
    return snappedMatrix;
  }
  return aTransform;
}

/* static */
void gfxUtils::ClearThebesSurface(gfxASurface* aSurface) {
  if (aSurface->CairoStatus()) {
    return;
  }
  cairo_surface_t* surf = aSurface->CairoSurface();
  if (cairo_surface_status(surf)) {
    return;
  }
  cairo_t* ctx = cairo_create(surf);
  cairo_set_source_rgba(ctx, 0.0, 0.0, 0.0, 0.0);
  cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
  IntRect bounds(nsIntPoint(0, 0), aSurface->GetSize());
  cairo_rectangle(ctx, bounds.X(), bounds.Y(), bounds.Width(), bounds.Height());
  cairo_fill(ctx);
  cairo_destroy(ctx);
}

/* static */
already_AddRefed<DataSourceSurface>
gfxUtils::CopySurfaceToDataSourceSurfaceWithFormat(SourceSurface* aSurface,
                                                   SurfaceFormat aFormat) {
  MOZ_ASSERT(aFormat != aSurface->GetFormat(),
             "Unnecessary - and very expersive - surface format conversion");

  Rect bounds(0, 0, aSurface->GetSize().width, aSurface->GetSize().height);

  if (!aSurface->IsDataSourceSurface()) {
    // If the surface is NOT of type DATA then its data is not mapped into main
    // memory. Format conversion is probably faster on the GPU, and by doing it
    // there we can avoid any expensive uploads/readbacks except for (possibly)
    // a single readback due to the unavoidable GetDataSurface() call. Using
    // CreateOffscreenContentDrawTarget ensures the conversion happens on the
    // GPU.
    RefPtr<DrawTarget> dt =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
            aSurface->GetSize(), aFormat);
    if (!dt) {
      gfxWarning() << "gfxUtils::CopySurfaceToDataSourceSurfaceWithFormat "
                      "failed in CreateOffscreenContentDrawTarget";
      return nullptr;
    }

    // Using DrawSurface() here rather than CopySurface() because CopySurface
    // is optimized for memcpy and therefore isn't good for format conversion.
    // Using OP_OVER since in our case it's equivalent to OP_SOURCE and
    // generally more optimized.
    dt->DrawSurface(aSurface, bounds, bounds, DrawSurfaceOptions(),
                    DrawOptions(1.0f, CompositionOp::OP_OVER));
    RefPtr<SourceSurface> surface = dt->Snapshot();
    return surface->GetDataSurface();
  }

  // If the surface IS of type DATA then it may or may not be in main memory
  // depending on whether or not it has been mapped yet. We have no way of
  // knowing, so we can't be sure if it's best to create a data wrapping
  // DrawTarget for the conversion or an offscreen content DrawTarget. We could
  // guess it's not mapped and create an offscreen content DrawTarget, but if
  // it is then we'll end up uploading the surface data, and most likely the
  // caller is going to be accessing the resulting surface data, resulting in a
  // readback (both very expensive operations). Alternatively we could guess
  // the data is mapped and create a data wrapping DrawTarget and, if the
  // surface is not in main memory, then we will incure a readback. The latter
  // of these two "wrong choices" is the least costly (a readback, vs an
  // upload and a readback), and more than likely the DATA surface that we've
  // been passed actually IS in main memory anyway. For these reasons it's most
  // likely best to create a data wrapping DrawTarget here to do the format
  // conversion.
  RefPtr<DataSourceSurface> dataSurface =
      Factory::CreateDataSourceSurface(aSurface->GetSize(), aFormat);
  DataSourceSurface::MappedSurface map;
  if (!dataSurface ||
      !dataSurface->Map(DataSourceSurface::MapType::READ_WRITE, &map)) {
    return nullptr;
  }
  RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForData(
      BackendType::CAIRO, map.mData, dataSurface->GetSize(), map.mStride,
      aFormat);
  if (!dt) {
    dataSurface->Unmap();
    return nullptr;
  }
  // Using DrawSurface() here rather than CopySurface() because CopySurface
  // is optimized for memcpy and therefore isn't good for format conversion.
  // Using OP_OVER since in our case it's equivalent to OP_SOURCE and
  // generally more optimized.
  dt->DrawSurface(aSurface, bounds, bounds, DrawSurfaceOptions(),
                  DrawOptions(1.0f, CompositionOp::OP_OVER));
  dataSurface->Unmap();
  return dataSurface.forget();
}

const uint32_t gfxUtils::sNumFrameColors = 8;

/* static */
const gfx::DeviceColor& gfxUtils::GetColorForFrameNumber(
    uint64_t aFrameNumber) {
  static bool initialized = false;
  static gfx::DeviceColor colors[sNumFrameColors];

  if (!initialized) {
    // This isn't truly device color, but it is just for debug.
    uint32_t i = 0;
    colors[i++] = gfx::DeviceColor::FromABGR(0xffff0000);
    colors[i++] = gfx::DeviceColor::FromABGR(0xffcc00ff);
    colors[i++] = gfx::DeviceColor::FromABGR(0xff0066cc);
    colors[i++] = gfx::DeviceColor::FromABGR(0xff00ff00);
    colors[i++] = gfx::DeviceColor::FromABGR(0xff33ffff);
    colors[i++] = gfx::DeviceColor::FromABGR(0xffff0099);
    colors[i++] = gfx::DeviceColor::FromABGR(0xff0000ff);
    colors[i++] = gfx::DeviceColor::FromABGR(0xff999999);
    MOZ_ASSERT(i == sNumFrameColors);
    initialized = true;
  }

  return colors[aFrameNumber % sNumFrameColors];
}

/* static */
nsresult gfxUtils::EncodeSourceSurface(SourceSurface* aSurface,
                                       const ImageType aImageType,
                                       const nsAString& aOutputOptions,
                                       BinaryOrData aBinaryOrData, FILE* aFile,
                                       nsACString* aStrOut) {
  MOZ_ASSERT(aBinaryOrData == gfxUtils::eDataURIEncode || aFile || aStrOut,
             "Copying binary encoding to clipboard not currently supported");

  const IntSize size = aSurface->GetSize();
  if (size.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  const Size floatSize(size.width, size.height);

  RefPtr<DataSourceSurface> dataSurface;
  if (aSurface->GetFormat() != SurfaceFormat::B8G8R8A8) {
    // FIXME bug 995807 (B8G8R8X8), bug 831898 (R5G6B5)
    dataSurface = gfxUtils::CopySurfaceToDataSourceSurfaceWithFormat(
        aSurface, SurfaceFormat::B8G8R8A8);
  } else {
    dataSurface = aSurface->GetDataSurface();
  }
  if (!dataSurface) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<imgIEncoder> encoder = nullptr;

  switch (aImageType) {
    case ImageType::BMP:
      encoder = MakeRefPtr<nsBMPEncoder>();
      break;

    case ImageType::ICO:
      encoder = MakeRefPtr<nsICOEncoder>();
      break;

    case ImageType::JPEG:
      encoder = MakeRefPtr<nsJPEGEncoder>();
      break;

    case ImageType::PNG:
      encoder = MakeRefPtr<nsPNGEncoder>();
      break;

    default:
      break;
  }

  MOZ_RELEASE_ASSERT(encoder != nullptr);

  nsresult rv = encoder->InitFromData(
      map.mData, BufferSizeFromStrideAndHeight(map.mStride, size.height),
      size.width, size.height, map.mStride, imgIEncoder::INPUT_FORMAT_HOSTARGB,
      aOutputOptions);
  dataSurface->Unmap();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> imgStream(encoder);
  if (!imgStream) {
    return NS_ERROR_FAILURE;
  }

  uint64_t bufSize64;
  rv = imgStream->Available(&bufSize64);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(bufSize64 < UINT32_MAX - 16, NS_ERROR_FAILURE);

  uint32_t bufSize = (uint32_t)bufSize64;

  // ...leave a little extra room so we can call read again and make sure we
  // got everything. 16 bytes for better padding (maybe)
  bufSize += 16;
  uint32_t imgSize = 0;
  Vector<char> imgData;
  if (!imgData.initCapacity(bufSize)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  uint32_t numReadThisTime = 0;
  while ((rv = imgStream->Read(imgData.begin() + imgSize, bufSize - imgSize,
                               &numReadThisTime)) == NS_OK &&
         numReadThisTime > 0) {
    // Update the length of the vector without overwriting the new data.
    if (!imgData.growByUninitialized(numReadThisTime)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    imgSize += numReadThisTime;
    if (imgSize == bufSize) {
      // need a bigger buffer, just double
      bufSize *= 2;
      if (!imgData.resizeUninitialized(bufSize)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!imgData.empty(), NS_ERROR_FAILURE);

  if (aBinaryOrData == gfxUtils::eBinaryEncode) {
    if (aFile) {
      Unused << fwrite(imgData.begin(), 1, imgSize, aFile);
    }
    return NS_OK;
  }

  nsCString stringBuf;
  nsACString& dataURI = aStrOut ? *aStrOut : stringBuf;
  dataURI.AppendLiteral("data:");

  switch (aImageType) {
    case ImageType::BMP:
      dataURI.AppendLiteral(IMAGE_BMP);
      break;

    case ImageType::ICO:
      dataURI.AppendLiteral(IMAGE_ICO_MS);
      break;
    case ImageType::JPEG:
      dataURI.AppendLiteral(IMAGE_JPEG);
      break;

    case ImageType::PNG:
      dataURI.AppendLiteral(IMAGE_PNG);
      break;

    default:
      break;
  }

  dataURI.AppendLiteral(";base64,");
  rv = Base64EncodeAppend(imgData.begin(), imgSize, dataURI);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aFile) {
#ifdef ANDROID
    if (aFile == stdout || aFile == stderr) {
      // ADB logcat cuts off long strings so we will break it down
      const char* cStr = dataURI.BeginReading();
      size_t len = strlen(cStr);
      while (true) {
        printf_stderr("IMG: %.140s\n", cStr);
        if (len <= 140) break;
        len -= 140;
        cStr += 140;
      }
    }
#endif
    fprintf(aFile, "%s", dataURI.BeginReading());
  } else if (!aStrOut) {
    nsCOMPtr<nsIClipboardHelper> clipboard(
        do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv));
    if (clipboard) {
      clipboard->CopyString(NS_ConvertASCIItoUTF16(dataURI));
    }
  }
  return NS_OK;
}

static nsCString EncodeSourceSurfaceAsPNGURI(SourceSurface* aSurface) {
  nsCString string;
  gfxUtils::EncodeSourceSurface(aSurface, ImageType::PNG, u""_ns,
                                gfxUtils::eDataURIEncode, nullptr, &string);
  return string;
}

// https://jdashg.github.io/misc/colors/from-coeffs.html
const float kBT601NarrowYCbCrToRGB_RowMajor[16] = {
    1.16438f,  0.00000f, 1.59603f, -0.87420f, 1.16438f, -0.39176f,
    -0.81297f, 0.53167f, 1.16438f, 2.01723f,  0.00000f, -1.08563f,
    0.00000f,  0.00000f, 0.00000f, 1.00000f};
const float kBT709NarrowYCbCrToRGB_RowMajor[16] = {
    1.16438f,  0.00000f, 1.79274f, -0.97295f, 1.16438f, -0.21325f,
    -0.53291f, 0.30148f, 1.16438f, 2.11240f,  0.00000f, -1.13340f,
    0.00000f,  0.00000f, 0.00000f, 1.00000f};
const float kBT2020NarrowYCbCrToRGB_RowMajor[16] = {
    1.16438f,  0.00000f, 1.67867f, -0.91569f, 1.16438f, -0.18733f,
    -0.65042f, 0.34746f, 1.16438f, 2.14177f,  0.00000f, -1.14815f,
    0.00000f,  0.00000f, 0.00000f, 1.00000f};
const float kIdentityNarrowYCbCrToRGB_RowMajor[16] = {
    0.00000f, 0.00000f, 1.00000f, 0.00000f, 1.00000f, 0.00000f,
    0.00000f, 0.00000f, 0.00000f, 1.00000f, 0.00000f, 0.00000f,
    0.00000f, 0.00000f, 0.00000f, 1.00000f};

/* static */ const float* gfxUtils::YuvToRgbMatrix4x3RowMajor(
    gfx::YUVColorSpace aYUVColorSpace) {
#define X(x) \
  { x[0], x[1], x[2], 0.0f, x[4], x[5], x[6], 0.0f, x[8], x[9], x[10], 0.0f }

  static const float rec601[12] = X(kBT601NarrowYCbCrToRGB_RowMajor);
  static const float rec709[12] = X(kBT709NarrowYCbCrToRGB_RowMajor);
  static const float rec2020[12] = X(kBT2020NarrowYCbCrToRGB_RowMajor);
  static const float identity[12] = X(kIdentityNarrowYCbCrToRGB_RowMajor);

#undef X

  switch (aYUVColorSpace) {
    case gfx::YUVColorSpace::BT601:
      return rec601;
    case gfx::YUVColorSpace::BT709:
      return rec709;
    case gfx::YUVColorSpace::BT2020:
      return rec2020;
    case gfx::YUVColorSpace::Identity:
      return identity;
    default:
      MOZ_CRASH("Bad YUVColorSpace");
  }
}

/* static */ const float* gfxUtils::YuvToRgbMatrix3x3ColumnMajor(
    gfx::YUVColorSpace aYUVColorSpace) {
#define X(x) \
  { x[0], x[4], x[8], x[1], x[5], x[9], x[2], x[6], x[10] }

  static const float rec601[9] = X(kBT601NarrowYCbCrToRGB_RowMajor);
  static const float rec709[9] = X(kBT709NarrowYCbCrToRGB_RowMajor);
  static const float rec2020[9] = X(kBT2020NarrowYCbCrToRGB_RowMajor);
  static const float identity[9] = X(kIdentityNarrowYCbCrToRGB_RowMajor);

#undef X

  switch (aYUVColorSpace) {
    case gfx::YUVColorSpace::BT601:
      return rec601;
    case YUVColorSpace::BT709:
      return rec709;
    case YUVColorSpace::BT2020:
      return rec2020;
    case YUVColorSpace::Identity:
      return identity;
    default:
      MOZ_CRASH("Bad YUVColorSpace");
  }
}

/* static */ const float* gfxUtils::YuvToRgbMatrix4x4ColumnMajor(
    YUVColorSpace aYUVColorSpace) {
#define X(x)                                                             \
  {                                                                      \
    x[0], x[4], x[8], x[12], x[1], x[5], x[9], x[13], x[2], x[6], x[10], \
        x[14], x[3], x[7], x[11], x[15]                                  \
  }

  static const float rec601[16] = X(kBT601NarrowYCbCrToRGB_RowMajor);
  static const float rec709[16] = X(kBT709NarrowYCbCrToRGB_RowMajor);
  static const float rec2020[16] = X(kBT2020NarrowYCbCrToRGB_RowMajor);
  static const float identity[16] = X(kIdentityNarrowYCbCrToRGB_RowMajor);

#undef X

  switch (aYUVColorSpace) {
    case YUVColorSpace::BT601:
      return rec601;
    case YUVColorSpace::BT709:
      return rec709;
    case YUVColorSpace::BT2020:
      return rec2020;
    case YUVColorSpace::Identity:
      return identity;
    default:
      MOZ_CRASH("Bad YUVColorSpace");
  }
}

// Translate from CICP values to the color spaces we support, or return
// Nothing() if there is no appropriate match to let the caller choose
// a default or generate an error.
//
// See Rec. ITU-T H.273 (12/2016) for details on CICP
/* static */ Maybe<gfx::YUVColorSpace> gfxUtils::CicpToColorSpace(
    const CICP::MatrixCoefficients aMatrixCoefficients,
    const CICP::ColourPrimaries aColourPrimaries, LazyLogModule& aLogger) {
  switch (aMatrixCoefficients) {
    case CICP::MatrixCoefficients::MC_BT2020_NCL:
    case CICP::MatrixCoefficients::MC_BT2020_CL:
      return Some(gfx::YUVColorSpace::BT2020);
    case CICP::MatrixCoefficients::MC_BT601:
      return Some(gfx::YUVColorSpace::BT601);
    case CICP::MatrixCoefficients::MC_BT709:
      return Some(gfx::YUVColorSpace::BT709);
    case CICP::MatrixCoefficients::MC_IDENTITY:
      return Some(gfx::YUVColorSpace::Identity);
    case CICP::MatrixCoefficients::MC_CHROMAT_NCL:
    case CICP::MatrixCoefficients::MC_CHROMAT_CL:
    case CICP::MatrixCoefficients::MC_UNSPECIFIED:
      switch (aColourPrimaries) {
        case CICP::ColourPrimaries::CP_BT601:
          return Some(gfx::YUVColorSpace::BT601);
        case CICP::ColourPrimaries::CP_BT709:
          return Some(gfx::YUVColorSpace::BT709);
        case CICP::ColourPrimaries::CP_BT2020:
          return Some(gfx::YUVColorSpace::BT2020);
        default:
          MOZ_LOG(aLogger, LogLevel::Debug,
                  ("Couldn't infer color matrix from primaries: %hhu",
                   aColourPrimaries));
          return {};
      }
    default:
      MOZ_LOG(aLogger, LogLevel::Debug,
              ("Unsupported color matrix value: %hhu", aMatrixCoefficients));
      return {};
  }
}

// Translate from CICP values to the transfer functions we support, or return
// Nothing() if there is no appropriate match.
//
/* static */ Maybe<gfx::TransferFunction> gfxUtils::CicpToTransferFunction(
    const CICP::TransferCharacteristics aTransferCharacteristics) {
  switch (aTransferCharacteristics) {
    case CICP::TransferCharacteristics::TC_BT709:
      return Some(gfx::TransferFunction::BT709);

    case CICP::TransferCharacteristics::TC_SRGB:
      return Some(gfx::TransferFunction::SRGB);

    case CICP::TransferCharacteristics::TC_SMPTE2084:
      return Some(gfx::TransferFunction::PQ);

    case CICP::TransferCharacteristics::TC_HLG:
      return Some(gfx::TransferFunction::HLG);

    default:
      return {};
  }
}

/* static */
void gfxUtils::WriteAsPNG(SourceSurface* aSurface, const nsAString& aFile) {
  WriteAsPNG(aSurface, NS_ConvertUTF16toUTF8(aFile).get());
}

/* static */
void gfxUtils::WriteAsPNG(SourceSurface* aSurface, const char* aFile) {
  FILE* file = fopen(aFile, "wb");

  if (!file) {
    // Maybe the directory doesn't exist; try creating it, then fopen again.
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIFile> comFile = do_CreateInstance("@mozilla.org/file/local;1");
    if (comFile) {
      NS_ConvertUTF8toUTF16 utf16path((nsDependentCString(aFile)));
      rv = comFile->InitWithPath(utf16path);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIFile> dirPath;
        comFile->GetParent(getter_AddRefs(dirPath));
        if (dirPath) {
          rv = dirPath->Create(nsIFile::DIRECTORY_TYPE, 0777);
          if (NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_ALREADY_EXISTS) {
            file = fopen(aFile, "wb");
          }
        }
      }
    }
    if (!file) {
      NS_WARNING("Failed to open file to create PNG!");
      return;
    }
  }

  EncodeSourceSurface(aSurface, ImageType::PNG, u""_ns, eBinaryEncode, file);
  fclose(file);
}

/* static */
void gfxUtils::WriteAsPNG(DrawTarget* aDT, const nsAString& aFile) {
  WriteAsPNG(aDT, NS_ConvertUTF16toUTF8(aFile).get());
}

/* static */
void gfxUtils::WriteAsPNG(DrawTarget* aDT, const char* aFile) {
  RefPtr<SourceSurface> surface = aDT->Snapshot();
  if (surface) {
    WriteAsPNG(surface, aFile);
  } else {
    NS_WARNING("Failed to get surface!");
  }
}

/* static */
void gfxUtils::DumpAsDataURI(SourceSurface* aSurface, FILE* aFile) {
  EncodeSourceSurface(aSurface, ImageType::PNG, u""_ns, eDataURIEncode, aFile);
}

/* static */
nsCString gfxUtils::GetAsDataURI(SourceSurface* aSurface) {
  return EncodeSourceSurfaceAsPNGURI(aSurface);
}

/* static */
void gfxUtils::DumpAsDataURI(DrawTarget* aDT, FILE* aFile) {
  RefPtr<SourceSurface> surface = aDT->Snapshot();
  if (surface) {
    DumpAsDataURI(surface, aFile);
  } else {
    NS_WARNING("Failed to get surface!");
  }
}

/* static */
nsCString gfxUtils::GetAsLZ4Base64Str(DataSourceSurface* aSourceSurface) {
  DataSourceSurface::ScopedMap map(aSourceSurface, DataSourceSurface::READ);
  int32_t dataSize = aSourceSurface->GetSize().height * map.GetStride();
  auto compressedData = MakeUnique<char[]>(LZ4::maxCompressedSize(dataSize));
  if (compressedData) {
    int nDataSize =
        LZ4::compress((char*)map.GetData(), dataSize, compressedData.get());
    if (nDataSize > 0) {
      nsCString string;
      string.AppendPrintf("data:image/lz4bgra;base64,%i,%i,%i,",
                          aSourceSurface->GetSize().width, map.GetStride(),
                          aSourceSurface->GetSize().height);
      nsresult rv = Base64EncodeAppend(compressedData.get(), nDataSize, string);
      if (rv == NS_OK) {
        return string;
      }
    }
  }
  return {};
}

/* static */
nsCString gfxUtils::GetAsDataURI(DrawTarget* aDT) {
  RefPtr<SourceSurface> surface = aDT->Snapshot();
  if (surface) {
    return EncodeSourceSurfaceAsPNGURI(surface);
  } else {
    NS_WARNING("Failed to get surface!");
    return nsCString("");
  }
}

/* static */
void gfxUtils::CopyAsDataURI(SourceSurface* aSurface) {
  EncodeSourceSurface(aSurface, ImageType::PNG, u""_ns, eDataURIEncode,
                      nullptr);
}

/* static */
void gfxUtils::CopyAsDataURI(DrawTarget* aDT) {
  RefPtr<SourceSurface> surface = aDT->Snapshot();
  if (surface) {
    CopyAsDataURI(surface);
  } else {
    NS_WARNING("Failed to get surface!");
  }
}

/* static */ UniquePtr<uint8_t[]> gfxUtils::GetImageBuffer(
    gfx::DataSourceSurface* aSurface, bool aIsAlphaPremultiplied,
    int32_t* outFormat) {
  *outFormat = 0;

  DataSourceSurface::MappedSurface map;
  if (!aSurface->Map(DataSourceSurface::MapType::READ, &map)) return nullptr;

  uint32_t bufferSize =
      aSurface->GetSize().width * aSurface->GetSize().height * 4;
  auto imageBuffer = MakeUniqueFallible<uint8_t[]>(bufferSize);
  if (!imageBuffer) {
    aSurface->Unmap();
    return nullptr;
  }
  memcpy(imageBuffer.get(), map.mData, bufferSize);

  aSurface->Unmap();

  int32_t format = imgIEncoder::INPUT_FORMAT_HOSTARGB;
  if (!aIsAlphaPremultiplied) {
    // We need to convert to INPUT_FORMAT_RGBA, otherwise
    // we are automatically considered premult, and unpremult'd.
    // Yes, it is THAT silly.
    // Except for different lossy conversions by color,
    // we could probably just change the label, and not change the data.
    gfxUtils::ConvertBGRAtoRGBA(imageBuffer.get(), bufferSize);
    format = imgIEncoder::INPUT_FORMAT_RGBA;
  }

  *outFormat = format;
  return imageBuffer;
}

/* static */
nsresult gfxUtils::GetInputStream(gfx::DataSourceSurface* aSurface,
                                  bool aIsAlphaPremultiplied,
                                  const char* aMimeType,
                                  const nsAString& aEncoderOptions,
                                  nsIInputStream** outStream) {
  nsCString enccid("@mozilla.org/image/encoder;2?type=");
  enccid += aMimeType;
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(enccid.get());
  if (!encoder) return NS_ERROR_FAILURE;

  int32_t format = 0;
  UniquePtr<uint8_t[]> imageBuffer =
      GetImageBuffer(aSurface, aIsAlphaPremultiplied, &format);
  if (!imageBuffer) return NS_ERROR_FAILURE;

  return dom::ImageEncoder::GetInputStream(
      aSurface->GetSize().width, aSurface->GetSize().height, imageBuffer.get(),
      format, encoder, aEncoderOptions, outStream);
}

class GetFeatureStatusWorkerRunnable final
    : public dom::WorkerMainThreadRunnable {
 public:
  GetFeatureStatusWorkerRunnable(dom::WorkerPrivate* workerPrivate,
                                 const nsCOMPtr<nsIGfxInfo>& gfxInfo,
                                 int32_t feature, nsACString& failureId,
                                 int32_t* status)
      : WorkerMainThreadRunnable(workerPrivate, "GFX :: GetFeatureStatus"_ns),
        mGfxInfo(gfxInfo),
        mFeature(feature),
        mStatus(status),
        mFailureId(failureId),
        mNSResult(NS_OK) {}

  bool MainThreadRun() override {
    if (mGfxInfo) {
      mNSResult = mGfxInfo->GetFeatureStatus(mFeature, mFailureId, mStatus);
    }
    return true;
  }

  nsresult GetNSResult() const { return mNSResult; }

 protected:
  ~GetFeatureStatusWorkerRunnable() = default;

 private:
  nsCOMPtr<nsIGfxInfo> mGfxInfo;
  int32_t mFeature;
  int32_t* mStatus;
  nsACString& mFailureId;
  nsresult mNSResult;
};

#define GFX_SHADER_CHECK_BUILD_VERSION_PREF "gfx-shader-check.build-version"
#define GFX_SHADER_CHECK_PTR_SIZE_PREF "gfx-shader-check.ptr-size"
#define GFX_SHADER_CHECK_DEVICE_ID_PREF "gfx-shader-check.device-id"
#define GFX_SHADER_CHECK_DRIVER_VERSION_PREF "gfx-shader-check.driver-version"

/* static */
void gfxUtils::RemoveShaderCacheFromDiskIfNecessary() {
  if (!gfxVars::UseWebRenderProgramBinaryDisk()) {
    return;
  }

  nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();

  // Get current values
  nsCString buildID(mozilla::PlatformBuildID());
  int ptrSize = sizeof(void*);
  nsString deviceID, driverVersion;
  gfxInfo->GetAdapterDeviceID(deviceID);
  gfxInfo->GetAdapterDriverVersion(driverVersion);

  // Get pref stored values
  nsAutoCString buildIDChecked;
  Preferences::GetCString(GFX_SHADER_CHECK_BUILD_VERSION_PREF, buildIDChecked);
  int ptrSizeChecked = Preferences::GetInt(GFX_SHADER_CHECK_PTR_SIZE_PREF, 0);
  nsAutoString deviceIDChecked, driverVersionChecked;
  Preferences::GetString(GFX_SHADER_CHECK_DEVICE_ID_PREF, deviceIDChecked);
  Preferences::GetString(GFX_SHADER_CHECK_DRIVER_VERSION_PREF,
                         driverVersionChecked);

  if (buildID == buildIDChecked && ptrSize == ptrSizeChecked &&
      deviceID == deviceIDChecked && driverVersion == driverVersionChecked) {
    return;
  }

  nsAutoString path(gfx::gfxVars::ProfDirectory());

  if (!wr::remove_program_binary_disk_cache(&path)) {
    // Failed to remove program binary disk cache. The disk cache might have
    // invalid data. Disable program binary disk cache usage.
    gfxVars::SetUseWebRenderProgramBinaryDisk(false);
    return;
  }

  Preferences::SetCString(GFX_SHADER_CHECK_BUILD_VERSION_PREF, buildID);
  Preferences::SetInt(GFX_SHADER_CHECK_PTR_SIZE_PREF, ptrSize);
  Preferences::SetString(GFX_SHADER_CHECK_DEVICE_ID_PREF, deviceID);
  Preferences::SetString(GFX_SHADER_CHECK_DRIVER_VERSION_PREF, driverVersion);
}

/* static */
bool gfxUtils::DumpDisplayList() {
  return StaticPrefs::layout_display_list_dump() ||
         (StaticPrefs::layout_display_list_dump_parent() &&
          XRE_IsParentProcess()) ||
         (StaticPrefs::layout_display_list_dump_content() &&
          XRE_IsContentProcess());
}

FILE* gfxUtils::sDumpPaintFile = stderr;

namespace mozilla {
namespace gfx {

DeviceColor ToDeviceColor(const sRGBColor& aColor) {
  // aColor is pass-by-value since to get return value optimization goodness we
  // need to return the same object from all return points in this function. We
  // could declare a local Color variable and use that, but we might as well
  // just use aColor.
  if (gfxPlatform::GetCMSMode() == CMSMode::All) {
    qcms_transform* transform = gfxPlatform::GetCMSRGBTransform();
    if (transform) {
      return gfxPlatform::TransformPixel(aColor, transform);
      // Use the original alpha to avoid unnecessary float->byte->float
      // conversion errors
    }
  }
  return DeviceColor(aColor.r, aColor.g, aColor.b, aColor.a);
}

DeviceColor ToDeviceColor(nscolor aColor) {
  return ToDeviceColor(sRGBColor::FromABGR(aColor));
}

DeviceColor ToDeviceColor(const StyleRGBA& aColor) {
  return ToDeviceColor(aColor.ToColor());
}

}  // namespace gfx
}  // namespace mozilla
