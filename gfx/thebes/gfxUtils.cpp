/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxUtils.h"

#include "cairo.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxDrawable.h"
#include "imgIEncoder.h"
#include "mozilla/Base64.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Vector.h"
#include "nsComponentManagerUtils.h"
#include "nsIClipboardHelper.h"
#include "nsIFile.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsRegion.h"
#include "nsServiceManagerUtils.h"
#include "yuv_convert.h"
#include "ycbcr_to_rgb565.h"
#include "GeckoProfiler.h"
#include "ImageContainer.h"
#include "ImageRegion.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h"

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif

using namespace mozilla;
using namespace mozilla::image;
using namespace mozilla::layers;
using namespace mozilla::gfx;

#include "DeprecatedPremultiplyTables.h"

static const uint8_t PremultiplyValue(uint8_t a, uint8_t v) {
    return gfxUtils::sPremultiplyTable[a*256+v];
}

static const uint8_t UnpremultiplyValue(uint8_t a, uint8_t v) {
    return gfxUtils::sUnpremultiplyTable[a*256+v];
}

static void
PremultiplyData(const uint8_t* srcData,
                size_t srcStride,  // row-to-row stride in bytes
                uint8_t* destData,
                size_t destStride, // row-to-row stride in bytes
                size_t pixelWidth,
                size_t rowCount)
{
    MOZ_ASSERT(srcData && destData);

    for (size_t y = 0; y < rowCount; ++y) {
        const uint8_t* src  = srcData  + y * srcStride;
        uint8_t* dest       = destData + y * destStride;

        for (size_t x = 0; x < pixelWidth; ++x) {
#ifdef IS_LITTLE_ENDIAN
            uint8_t b = *src++;
            uint8_t g = *src++;
            uint8_t r = *src++;
            uint8_t a = *src++;

            *dest++ = PremultiplyValue(a, b);
            *dest++ = PremultiplyValue(a, g);
            *dest++ = PremultiplyValue(a, r);
            *dest++ = a;
#else
            uint8_t a = *src++;
            uint8_t r = *src++;
            uint8_t g = *src++;
            uint8_t b = *src++;

            *dest++ = a;
            *dest++ = PremultiplyValue(a, r);
            *dest++ = PremultiplyValue(a, g);
            *dest++ = PremultiplyValue(a, b);
#endif
        }
    }
}
static void
UnpremultiplyData(const uint8_t* srcData,
                  size_t srcStride,  // row-to-row stride in bytes
                  uint8_t* destData,
                  size_t destStride, // row-to-row stride in bytes
                  size_t pixelWidth,
                  size_t rowCount)
{
    MOZ_ASSERT(srcData && destData);

    for (size_t y = 0; y < rowCount; ++y) {
        const uint8_t* src  = srcData  + y * srcStride;
        uint8_t* dest       = destData + y * destStride;

        for (size_t x = 0; x < pixelWidth; ++x) {
#ifdef IS_LITTLE_ENDIAN
            uint8_t b = *src++;
            uint8_t g = *src++;
            uint8_t r = *src++;
            uint8_t a = *src++;

            *dest++ = UnpremultiplyValue(a, b);
            *dest++ = UnpremultiplyValue(a, g);
            *dest++ = UnpremultiplyValue(a, r);
            *dest++ = a;
#else
            uint8_t a = *src++;
            uint8_t r = *src++;
            uint8_t g = *src++;
            uint8_t b = *src++;

            *dest++ = a;
            *dest++ = UnpremultiplyValue(a, r);
            *dest++ = UnpremultiplyValue(a, g);
            *dest++ = UnpremultiplyValue(a, b);
#endif
        }
    }
}

static bool
MapSrcDest(DataSourceSurface* srcSurf,
           DataSourceSurface* destSurf,
           DataSourceSurface::MappedSurface* out_srcMap,
           DataSourceSurface::MappedSurface* out_destMap)
{
    MOZ_ASSERT(srcSurf && destSurf);
    MOZ_ASSERT(out_srcMap && out_destMap);

    if (srcSurf->GetFormat()  != SurfaceFormat::B8G8R8A8 ||
        destSurf->GetFormat() != SurfaceFormat::B8G8R8A8)
    {
        MOZ_ASSERT(false, "Only operate on BGRA8 surfs.");
        return false;
    }

    if (srcSurf->GetSize().width  != destSurf->GetSize().width ||
        srcSurf->GetSize().height != destSurf->GetSize().height)
    {
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

static void
UnmapSrcDest(DataSourceSurface* srcSurf,
             DataSourceSurface* destSurf)
{
    if (srcSurf == destSurf) {
        srcSurf->Unmap();
    } else {
        srcSurf->Unmap();
        destSurf->Unmap();
    }
}

bool
gfxUtils::PremultiplyDataSurface(DataSourceSurface* srcSurf,
                                 DataSourceSurface* destSurf)
{
    MOZ_ASSERT(srcSurf && destSurf);

    DataSourceSurface::MappedSurface srcMap;
    DataSourceSurface::MappedSurface destMap;
    if (!MapSrcDest(srcSurf, destSurf, &srcMap, &destMap))
        return false;

    PremultiplyData(srcMap.mData, srcMap.mStride,
                    destMap.mData, destMap.mStride,
                    srcSurf->GetSize().width,
                    srcSurf->GetSize().height);

    UnmapSrcDest(srcSurf, destSurf);
    return true;
}

bool
gfxUtils::UnpremultiplyDataSurface(DataSourceSurface* srcSurf,
                                   DataSourceSurface* destSurf)
{
    MOZ_ASSERT(srcSurf && destSurf);

    DataSourceSurface::MappedSurface srcMap;
    DataSourceSurface::MappedSurface destMap;
    if (!MapSrcDest(srcSurf, destSurf, &srcMap, &destMap))
        return false;

    UnpremultiplyData(srcMap.mData, srcMap.mStride,
                      destMap.mData, destMap.mStride,
                      srcSurf->GetSize().width,
                      srcSurf->GetSize().height);

    UnmapSrcDest(srcSurf, destSurf);
    return true;
}

static bool
MapSrcAndCreateMappedDest(DataSourceSurface* srcSurf,
                          RefPtr<DataSourceSurface>* out_destSurf,
                          DataSourceSurface::MappedSurface* out_srcMap,
                          DataSourceSurface::MappedSurface* out_destMap)
{
    MOZ_ASSERT(srcSurf);
    MOZ_ASSERT(out_destSurf && out_srcMap && out_destMap);

    if (srcSurf->GetFormat() != SurfaceFormat::B8G8R8A8) {
        MOZ_ASSERT(false, "Only operate on BGRA8.");
        return false;
    }

    // Ok, map source for reading.
    DataSourceSurface::MappedSurface srcMap;
    if (!srcSurf->Map(DataSourceSurface::MapType::READ, &srcMap)) {
        MOZ_ASSERT(false, "Couldn't Map srcSurf.");
        return false;
    }

    // Make our dest surface based on the src.
    RefPtr<DataSourceSurface> destSurf =
        Factory::CreateDataSourceSurfaceWithStride(srcSurf->GetSize(),
                                                   srcSurf->GetFormat(),
                                                   srcMap.mStride);
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

TemporaryRef<DataSourceSurface>
gfxUtils::CreatePremultipliedDataSurface(DataSourceSurface* srcSurf)
{
    RefPtr<DataSourceSurface> destSurf;
    DataSourceSurface::MappedSurface srcMap;
    DataSourceSurface::MappedSurface destMap;
    if (!MapSrcAndCreateMappedDest(srcSurf, &destSurf, &srcMap, &destMap)) {
        MOZ_ASSERT(false, "MapSrcAndCreateMappedDest failed.");
        return srcSurf;
    }

    PremultiplyData(srcMap.mData, srcMap.mStride,
                    destMap.mData, destMap.mStride,
                    srcSurf->GetSize().width,
                    srcSurf->GetSize().height);

    UnmapSrcDest(srcSurf, destSurf);
    return destSurf;
}

TemporaryRef<DataSourceSurface>
gfxUtils::CreateUnpremultipliedDataSurface(DataSourceSurface* srcSurf)
{
    RefPtr<DataSourceSurface> destSurf;
    DataSourceSurface::MappedSurface srcMap;
    DataSourceSurface::MappedSurface destMap;
    if (!MapSrcAndCreateMappedDest(srcSurf, &destSurf, &srcMap, &destMap)) {
        MOZ_ASSERT(false, "MapSrcAndCreateMappedDest failed.");
        return srcSurf;
    }

    UnpremultiplyData(srcMap.mData, srcMap.mStride,
                      destMap.mData, destMap.mStride,
                      srcSurf->GetSize().width,
                      srcSurf->GetSize().height);

    UnmapSrcDest(srcSurf, destSurf);
    return destSurf;
}

void
gfxUtils::ConvertBGRAtoRGBA(uint8_t* aData, uint32_t aLength)
{
    MOZ_ASSERT((aLength % 4) == 0, "Loop below will pass srcEnd!");

    uint8_t *src = aData;
    uint8_t *srcEnd = src + aLength;

    uint8_t buffer[4];
    for (; src != srcEnd; src += 4) {
        buffer[0] = src[2];
        buffer[1] = src[1];
        buffer[2] = src[0];

        src[0] = buffer[0];
        src[1] = buffer[1];
        src[2] = buffer[2];
    }
}

static bool
IsSafeImageTransformComponent(gfxFloat aValue)
{
  return aValue >= -32768 && aValue <= 32767;
}

#ifndef MOZ_GFX_OPTIMIZE_MOBILE
/**
 * This returns the fastest operator to use for solid surfaces which have no
 * alpha channel or their alpha channel is uniformly opaque.
 * This differs per render mode.
 */
static gfxContext::GraphicsOperator
OptimalFillOperator()
{
#ifdef XP_WIN
    if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
        gfxWindowsPlatform::RENDER_DIRECT2D) {
        // D2D -really- hates operator source.
        return gfxContext::OPERATOR_OVER;
    } else {
#endif
        return gfxContext::OPERATOR_SOURCE;
#ifdef XP_WIN
    }
#endif
}

// EXTEND_PAD won't help us here; we have to create a temporary surface to hold
// the subimage of pixels we're allowed to sample.
static already_AddRefed<gfxDrawable>
CreateSamplingRestrictedDrawable(gfxDrawable* aDrawable,
                                 gfxContext* aContext,
                                 const ImageRegion& aRegion,
                                 const SurfaceFormat aFormat)
{
    PROFILER_LABEL("gfxUtils", "CreateSamplingRestricedDrawable",
      js::ProfileEntry::Category::GRAPHICS);

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
    if (needed.IsEmpty())
        return nullptr;

    gfxIntSize size(int32_t(needed.Width()), int32_t(needed.Height()));

    RefPtr<DrawTarget> target =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(ToIntSize(size),
                                                                   aFormat);
    if (!target) {
      return nullptr;
    }

    nsRefPtr<gfxContext> tmpCtx = new gfxContext(target);
    tmpCtx->SetOperator(OptimalFillOperator());
    aDrawable->Draw(tmpCtx, needed - needed.TopLeft(), true,
                    GraphicsFilter::FILTER_FAST, 1.0, gfxMatrix::Translation(needed.TopLeft()));
    RefPtr<SourceSurface> surface = target->Snapshot();

    nsRefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(surface, size, gfxMatrix::Translation(-needed.TopLeft()));
    return drawable.forget();
}
#endif // !MOZ_GFX_OPTIMIZE_MOBILE

// working around cairo/pixman bug (bug 364968)
// Our device-space-to-image-space transform may not be acceptable to pixman.
struct MOZ_STACK_CLASS AutoCairoPixmanBugWorkaround
{
    AutoCairoPixmanBugWorkaround(gfxContext*      aContext,
                                 const gfxMatrix& aDeviceSpaceToImageSpace,
                                 const gfxRect&   aFill,
                                 const gfxASurface* aSurface)
     : mContext(aContext), mSucceeded(true), mPushedGroup(false)
    {
        // Quartz's limits for matrix are much larger than pixman
        if (!aSurface || aSurface->GetType() == gfxSurfaceType::Quartz)
            return;

        if (!IsSafeImageTransformComponent(aDeviceSpaceToImageSpace._11) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace._21) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace._12) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace._22)) {
            NS_WARNING("Scaling up too much, bailing out");
            mSucceeded = false;
            return;
        }

        if (IsSafeImageTransformComponent(aDeviceSpaceToImageSpace._31) &&
            IsSafeImageTransformComponent(aDeviceSpaceToImageSpace._32))
            return;

        // We'll push a group, which will hopefully reduce our transform's
        // translation so it's in bounds.
        gfxMatrix currentMatrix = mContext->CurrentMatrix();
        mContext->Save();

        // Clip the rounded-out-to-device-pixels bounds of the
        // transformed fill area. This is the area for the group we
        // want to push.
        mContext->SetMatrix(gfxMatrix());
        gfxRect bounds = currentMatrix.TransformBounds(aFill);
        bounds.RoundOut();
        mContext->Clip(bounds);
        mContext->SetMatrix(currentMatrix);
        mContext->PushGroup(gfxContentType::COLOR_ALPHA);
        mContext->SetOperator(gfxContext::OPERATOR_OVER);

        mPushedGroup = true;
    }

    ~AutoCairoPixmanBugWorkaround()
    {
        if (mPushedGroup) {
            mContext->PopGroupToSource();
            mContext->Paint();
            mContext->Restore();
        }
    }

    bool PushedGroup() { return mPushedGroup; }
    bool Succeeded() { return mSucceeded; }

private:
    gfxContext* mContext;
    bool mSucceeded;
    bool mPushedGroup;
};

static gfxMatrix
DeviceToImageTransform(gfxContext* aContext)
{
    gfxFloat deviceX, deviceY;
    nsRefPtr<gfxASurface> currentTarget =
        aContext->CurrentSurface(&deviceX, &deviceY);
    gfxMatrix deviceToUser = aContext->CurrentMatrix();
    if (!deviceToUser.Invert()) {
        return gfxMatrix(0, 0, 0, 0, 0, 0); // singular
    }
    deviceToUser.Translate(-gfxPoint(-deviceX, -deviceY));
    return deviceToUser;
}

/* These heuristics are based on Source/WebCore/platform/graphics/skia/ImageSkia.cpp:computeResamplingMode() */
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
static GraphicsFilter ReduceResamplingFilter(GraphicsFilter aFilter,
                                             int aImgWidth, int aImgHeight,
                                             float aSourceWidth, float aSourceHeight)
{
    // Images smaller than this in either direction are considered "small" and
    // are not resampled ever (see below).
    const int kSmallImageSizeThreshold = 8;

    // The amount an image can be stretched in a single direction before we
    // say that it is being stretched so much that it must be a line or
    // background that doesn't need resampling.
    const float kLargeStretch = 3.0f;

    if (aImgWidth <= kSmallImageSizeThreshold
        || aImgHeight <= kSmallImageSizeThreshold) {
        // Never resample small images. These are often used for borders and
        // rules (think 1x1 images used to make lines).
        return GraphicsFilter::FILTER_NEAREST;
    }

    if (aImgHeight * kLargeStretch <= aSourceHeight || aImgWidth * kLargeStretch <= aSourceWidth) {
        // Large image tiling detected.

        // Don't resample if it is being tiled a lot in only one direction.
        // This is trying to catch cases where somebody has created a border
        // (which might be large) and then is stretching it to fill some part
        // of the page.
        if (fabs(aSourceWidth - aImgWidth)/aImgWidth < 0.5 || fabs(aSourceHeight - aImgHeight)/aImgHeight < 0.5)
            return GraphicsFilter::FILTER_NEAREST;

        // The image is growing a lot and in more than one direction. Resampling
        // is slow and doesn't give us very much when growing a lot.
        return aFilter;
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

    return aFilter;
}
#else
static GraphicsFilter ReduceResamplingFilter(GraphicsFilter aFilter,
                                             int aImgWidth, int aImgHeight,
                                             int aSourceWidth, int aSourceHeight)
{
    // Just pass the filter through unchanged
    return aFilter;
}
#endif

/* static */ void
gfxUtils::DrawPixelSnapped(gfxContext*         aContext,
                           gfxDrawable*        aDrawable,
                           const gfxSize&      aImageSize,
                           const ImageRegion&  aRegion,
                           const SurfaceFormat aFormat,
                           GraphicsFilter      aFilter,
                           uint32_t            aImageFlags,
                           gfxFloat            aOpacity)
{
    PROFILER_LABEL("gfxUtils", "DrawPixelSnapped",
      js::ProfileEntry::Category::GRAPHICS);

    gfxRect imageRect(gfxPoint(0, 0), aImageSize);
    gfxRect region(aRegion.Rect());

    bool doTile = !imageRect.Contains(region) &&
                  !(aImageFlags & imgIContainer::FLAG_CLAMP);

    nsRefPtr<gfxASurface> currentTarget = aContext->CurrentSurface();
    gfxMatrix deviceSpaceToImageSpace = DeviceToImageTransform(aContext);

    AutoCairoPixmanBugWorkaround workaround(aContext, deviceSpaceToImageSpace,
                                            region, currentTarget);
    if (!workaround.Succeeded())
        return;

    nsRefPtr<gfxDrawable> drawable = aDrawable;

    aFilter = ReduceResamplingFilter(aFilter,
                                     imageRect.Width(), imageRect.Height(),
                                     region.Width(), region.Height());

    // OK now, the hard part left is to account for the subimage sampling
    // restriction. If all the transforms involved are just integer
    // translations, then we assume no resampling will occur so there's
    // nothing to do.
    // XXX if only we had source-clipping in cairo!
    if (aContext->CurrentMatrix().HasNonIntegerTranslation()) {
        if (doTile || !aRegion.RestrictionContains(imageRect)) {
            if (drawable->DrawWithSamplingRect(aContext, aRegion.Rect(), aRegion.Restriction(),
                                               doTile, aFilter, aOpacity)) {
              return;
            }

            // On Mobile, we don't ever want to do this; it has the potential for
            // allocating very large temporary surfaces, especially since we'll
            // do full-page snapshots often (see bug 749426).
#ifndef MOZ_GFX_OPTIMIZE_MOBILE
            nsRefPtr<gfxDrawable> restrictedDrawable =
              CreateSamplingRestrictedDrawable(aDrawable, aContext,
                                               aRegion, aFormat);
            if (restrictedDrawable) {
                drawable.swap(restrictedDrawable);
            }

            // We no longer need to tile: Either we never needed to, or we already
            // filled a surface with the tiled pattern; this surface can now be
            // drawn without tiling.
            doTile = false;
#endif
        }
    }

    drawable->Draw(aContext, aRegion.Rect(), doTile, aFilter, aOpacity);
}

/* static */ int
gfxUtils::ImageFormatToDepth(gfxImageFormat aFormat)
{
    switch (aFormat) {
        case gfxImageFormat::ARGB32:
            return 32;
        case gfxImageFormat::RGB24:
            return 24;
        case gfxImageFormat::RGB16_565:
            return 16;
        default:
            break;
    }
    return 0;
}

static void
PathFromRegionInternal(gfxContext* aContext, const nsIntRegion& aRegion,
                       bool aSnap)
{
  aContext->NewPath();
  nsIntRegionRectIterator iter(aRegion);
  const nsIntRect* r;
  while ((r = iter.Next()) != nullptr) {
    aContext->Rectangle(gfxRect(r->x, r->y, r->width, r->height), aSnap);
  }
}

static void
ClipToRegionInternal(gfxContext* aContext, const nsIntRegion& aRegion,
                     bool aSnap)
{
  PathFromRegionInternal(aContext, aRegion, aSnap);
  aContext->Clip();
}

static TemporaryRef<Path>
PathFromRegionInternal(DrawTarget* aTarget, const nsIntRegion& aRegion)
{
  RefPtr<PathBuilder> pb = aTarget->CreatePathBuilder();
  nsIntRegionRectIterator iter(aRegion);

  const nsIntRect* r;
  while ((r = iter.Next()) != nullptr) {
    pb->MoveTo(Point(r->x, r->y));
    pb->LineTo(Point(r->XMost(), r->y));
    pb->LineTo(Point(r->XMost(), r->YMost()));
    pb->LineTo(Point(r->x, r->YMost()));
    pb->Close();
  }
  RefPtr<Path> path = pb->Finish();
  return path;
}

static void
ClipToRegionInternal(DrawTarget* aTarget, const nsIntRegion& aRegion)
{
  if (!aRegion.IsComplex()) {
    nsIntRect rect = aRegion.GetBounds();
    aTarget->PushClipRect(Rect(rect.x, rect.y, rect.width, rect.height));
    return;
  }

  RefPtr<Path> path = PathFromRegionInternal(aTarget, aRegion);
  aTarget->PushClip(path);
}

/*static*/ void
gfxUtils::ClipToRegion(gfxContext* aContext, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aContext, aRegion, false);
}

/*static*/ void
gfxUtils::ClipToRegion(DrawTarget* aTarget, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aTarget, aRegion);
}

/*static*/ void
gfxUtils::ClipToRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aContext, aRegion, true);
}

/*static*/ gfxFloat
gfxUtils::ClampToScaleFactor(gfxFloat aVal)
{
  // Arbitary scale factor limitation. We can increase this
  // for better scaling performance at the cost of worse
  // quality.
  static const gfxFloat kScaleResolution = 2;

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

  gfxFloat power = log(aVal)/log(kScaleResolution);

  // If power is within 1e-5 of an integer, round to nearest to
  // prevent floating point errors, otherwise round up to the
  // next integer value.
  if (fabs(power - NS_round(power)) < 1e-5) {
    power = NS_round(power);
  } else if (inverse) {
    power = floor(power);
  } else {
    power = ceil(power);
  }

  gfxFloat scale = pow(kScaleResolution, power);

  if (inverse) {
    scale = 1 / scale;
  }

  return scale;
}


/*static*/ void
gfxUtils::PathFromRegion(gfxContext* aContext, const nsIntRegion& aRegion)
{
  PathFromRegionInternal(aContext, aRegion, false);
}

/*static*/ void
gfxUtils::PathFromRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion)
{
  PathFromRegionInternal(aContext, aRegion, true);
}

gfxMatrix
gfxUtils::TransformRectToRect(const gfxRect& aFrom, const gfxPoint& aToTopLeft,
                              const gfxPoint& aToTopRight, const gfxPoint& aToBottomRight)
{
  gfxMatrix m;
  if (aToTopRight.y == aToTopLeft.y && aToTopRight.x == aToBottomRight.x) {
    // Not a rotation, so xy and yx are zero
    m._21 = m._12 = 0.0;
    m._11 = (aToBottomRight.x - aToTopLeft.x)/aFrom.width;
    m._22 = (aToBottomRight.y - aToTopLeft.y)/aFrom.height;
    m._31 = aToTopLeft.x - m._11*aFrom.x;
    m._32 = aToTopLeft.y - m._22*aFrom.y;
  } else {
    NS_ASSERTION(aToTopRight.y == aToBottomRight.y && aToTopRight.x == aToTopLeft.x,
                 "Destination rectangle not axis-aligned");
    m._11 = m._22 = 0.0;
    m._21 = (aToBottomRight.x - aToTopLeft.x)/aFrom.height;
    m._12 = (aToBottomRight.y - aToTopLeft.y)/aFrom.width;
    m._31 = aToTopLeft.x - m._21*aFrom.y;
    m._32 = aToTopLeft.y - m._12*aFrom.x;
  }
  return m;
}

Matrix
gfxUtils::TransformRectToRect(const gfxRect& aFrom, const IntPoint& aToTopLeft,
                              const IntPoint& aToTopRight, const IntPoint& aToBottomRight)
{
  Matrix m;
  if (aToTopRight.y == aToTopLeft.y && aToTopRight.x == aToBottomRight.x) {
    // Not a rotation, so xy and yx are zero
    m._12 = m._21 = 0.0;
    m._11 = (aToBottomRight.x - aToTopLeft.x)/aFrom.width;
    m._22 = (aToBottomRight.y - aToTopLeft.y)/aFrom.height;
    m._31 = aToTopLeft.x - m._11*aFrom.x;
    m._32 = aToTopLeft.y - m._22*aFrom.y;
  } else {
    NS_ASSERTION(aToTopRight.y == aToBottomRight.y && aToTopRight.x == aToTopLeft.x,
                 "Destination rectangle not axis-aligned");
    m._11 = m._22 = 0.0;
    m._21 = (aToBottomRight.x - aToTopLeft.x)/aFrom.height;
    m._12 = (aToBottomRight.y - aToTopLeft.y)/aFrom.width;
    m._31 = aToTopLeft.x - m._21*aFrom.y;
    m._32 = aToTopLeft.y - m._12*aFrom.x;
  }
  return m;
}

/* This function is sort of shitty. We truncate doubles
 * to ints then convert those ints back to doubles to make sure that
 * they equal the doubles that we got in. */
bool
gfxUtils::GfxRectToIntRect(const gfxRect& aIn, nsIntRect* aOut)
{
  *aOut = nsIntRect(int32_t(aIn.X()), int32_t(aIn.Y()),
  int32_t(aIn.Width()), int32_t(aIn.Height()));
  return gfxRect(aOut->x, aOut->y, aOut->width, aOut->height).IsEqualEdges(aIn);
}

void
gfxUtils::GetYCbCrToRGBDestFormatAndSize(const PlanarYCbCrData& aData,
                                         gfxImageFormat& aSuggestedFormat,
                                         gfxIntSize& aSuggestedSize)
{
  YUVType yuvtype =
    TypeFromSize(aData.mYSize.width,
                      aData.mYSize.height,
                      aData.mCbCrSize.width,
                      aData.mCbCrSize.height);

  // 'prescale' is true if the scaling is to be done as part of the
  // YCbCr to RGB conversion rather than on the RGB data when rendered.
  bool prescale = aSuggestedSize.width > 0 && aSuggestedSize.height > 0 &&
                    ToIntSize(aSuggestedSize) != aData.mPicSize;

  if (aSuggestedFormat == gfxImageFormat::RGB16_565) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (prescale &&
        !IsScaleYCbCrToRGB565Fast(aData.mPicX,
                                       aData.mPicY,
                                       aData.mPicSize.width,
                                       aData.mPicSize.height,
                                       aSuggestedSize.width,
                                       aSuggestedSize.height,
                                       yuvtype,
                                       FILTER_BILINEAR) &&
        IsConvertYCbCrToRGB565Fast(aData.mPicX,
                                        aData.mPicY,
                                        aData.mPicSize.width,
                                        aData.mPicSize.height,
                                        yuvtype)) {
      prescale = false;
    }
#else
    // yuv2rgb16 function not available
    aSuggestedFormat = gfxImageFormat::RGB24;
#endif
  }
  else if (aSuggestedFormat != gfxImageFormat::RGB24) {
    // No other formats are currently supported.
    aSuggestedFormat = gfxImageFormat::RGB24;
  }
  if (aSuggestedFormat == gfxImageFormat::RGB24) {
    /* ScaleYCbCrToRGB32 does not support a picture offset, nor 4:4:4 data.
       See bugs 639415 and 640073. */
    if (aData.mPicX != 0 || aData.mPicY != 0 || yuvtype == YV24)
      prescale = false;
  }
  if (!prescale) {
    ToIntSize(aSuggestedSize) = aData.mPicSize;
  }
}

void
gfxUtils::ConvertYCbCrToRGB(const PlanarYCbCrData& aData,
                            const gfxImageFormat& aDestFormat,
                            const gfxIntSize& aDestSize,
                            unsigned char* aDestBuffer,
                            int32_t aStride)
{
  // ConvertYCbCrToRGB et al. assume the chroma planes are rounded up if the
  // luma plane is odd sized.
  MOZ_ASSERT((aData.mCbCrSize.width == aData.mYSize.width ||
              aData.mCbCrSize.width == (aData.mYSize.width + 1) >> 1) &&
             (aData.mCbCrSize.height == aData.mYSize.height ||
              aData.mCbCrSize.height == (aData.mYSize.height + 1) >> 1));
  YUVType yuvtype =
    TypeFromSize(aData.mYSize.width,
                      aData.mYSize.height,
                      aData.mCbCrSize.width,
                      aData.mCbCrSize.height);

  // Convert from YCbCr to RGB now, scaling the image if needed.
  if (ToIntSize(aDestSize) != aData.mPicSize) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (aDestFormat == gfxImageFormat::RGB16_565) {
      ScaleYCbCrToRGB565(aData.mYChannel,
                              aData.mCbChannel,
                              aData.mCrChannel,
                              aDestBuffer,
                              aData.mPicX,
                              aData.mPicY,
                              aData.mPicSize.width,
                              aData.mPicSize.height,
                              aDestSize.width,
                              aDestSize.height,
                              aData.mYStride,
                              aData.mCbCrStride,
                              aStride,
                              yuvtype,
                              FILTER_BILINEAR);
    } else
#endif
      ScaleYCbCrToRGB32(aData.mYChannel,
                             aData.mCbChannel,
                             aData.mCrChannel,
                             aDestBuffer,
                             aData.mPicSize.width,
                             aData.mPicSize.height,
                             aDestSize.width,
                             aDestSize.height,
                             aData.mYStride,
                             aData.mCbCrStride,
                             aStride,
                             yuvtype,
                             ROTATE_0,
                             FILTER_BILINEAR);
  } else { // no prescale
#if defined(HAVE_YCBCR_TO_RGB565)
    if (aDestFormat == gfxImageFormat::RGB16_565) {
      ConvertYCbCrToRGB565(aData.mYChannel,
                                aData.mCbChannel,
                                aData.mCrChannel,
                                aDestBuffer,
                                aData.mPicX,
                                aData.mPicY,
                                aData.mPicSize.width,
                                aData.mPicSize.height,
                                aData.mYStride,
                                aData.mCbCrStride,
                                aStride,
                                yuvtype);
    } else // aDestFormat != gfxImageFormat::RGB16_565
#endif
      ConvertYCbCrToRGB32(aData.mYChannel,
                               aData.mCbChannel,
                               aData.mCrChannel,
                               aDestBuffer,
                               aData.mPicX,
                               aData.mPicY,
                               aData.mPicSize.width,
                               aData.mPicSize.height,
                               aData.mYStride,
                               aData.mCbCrStride,
                               aStride,
                               yuvtype);
  }
}

/* static */ void gfxUtils::ClearThebesSurface(gfxASurface* aSurface,
                                               nsIntRect* aRect,
                                               const gfxRGBA& aColor)
{
  if (aSurface->CairoStatus()) {
    return;
  }
  cairo_surface_t* surf = aSurface->CairoSurface();
  if (cairo_surface_status(surf)) {
    return;
  }
  cairo_t* ctx = cairo_create(surf);
  cairo_set_source_rgba(ctx, aColor.r, aColor.g, aColor.b, aColor.a);
  cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
  nsIntRect bounds;
  if (aRect) {
    bounds = *aRect;
  } else {
    bounds = nsIntRect(nsIntPoint(0, 0), aSurface->GetSize());
  }
  cairo_rectangle(ctx, bounds.x, bounds.y, bounds.width, bounds.height);
  cairo_fill(ctx);
  cairo_destroy(ctx);
}

/* static */ TemporaryRef<DataSourceSurface>
gfxUtils::CopySurfaceToDataSourceSurfaceWithFormat(SourceSurface* aSurface,
                                                   SurfaceFormat aFormat)
{
  MOZ_ASSERT(aFormat != aSurface->GetFormat(),
             "Unnecessary - and very expersive - surface format conversion");

  Rect bounds(0, 0, aSurface->GetSize().width, aSurface->GetSize().height);

  if (aSurface->GetType() != SurfaceType::DATA) {
    // If the surface is NOT of type DATA then its data is not mapped into main
    // memory. Format conversion is probably faster on the GPU, and by doing it
    // there we can avoid any expensive uploads/readbacks except for (possibly)
    // a single readback due to the unavoidable GetDataSurface() call. Using
    // CreateOffscreenContentDrawTarget ensures the conversion happens on the
    // GPU.
    RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->
      CreateOffscreenContentDrawTarget(aSurface->GetSize(), aFormat);
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
  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     map.mData,
                                     dataSurface->GetSize(),
                                     map.mStride,
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

/* static */ const gfx::Color&
gfxUtils::GetColorForFrameNumber(uint64_t aFrameNumber)
{
    static bool initialized = false;
    static gfx::Color colors[sNumFrameColors];

    if (!initialized) {
        uint32_t i = 0;
        colors[i++] = gfx::Color::FromABGR(0xffff0000);
        colors[i++] = gfx::Color::FromABGR(0xffcc00ff);
        colors[i++] = gfx::Color::FromABGR(0xff0066cc);
        colors[i++] = gfx::Color::FromABGR(0xff00ff00);
        colors[i++] = gfx::Color::FromABGR(0xff33ffff);
        colors[i++] = gfx::Color::FromABGR(0xffff0099);
        colors[i++] = gfx::Color::FromABGR(0xff0000ff);
        colors[i++] = gfx::Color::FromABGR(0xff999999);
        MOZ_ASSERT(i == sNumFrameColors);
        initialized = true;
    }

    return colors[aFrameNumber % sNumFrameColors];
}

/* static */ nsresult
gfxUtils::EncodeSourceSurface(SourceSurface* aSurface,
                              const nsACString& aMimeType,
                              const nsAString& aOutputOptions,
                              BinaryOrData aBinaryOrData,
                              FILE* aFile)
{
  MOZ_ASSERT(aBinaryOrData == eDataURIEncode || aFile,
             "Copying binary encoding to clipboard not currently supported");

  const IntSize size = aSurface->GetSize();
  if (size.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  const Size floatSize(size.width, size.height);

  RefPtr<DataSourceSurface> dataSurface;
  if (aSurface->GetFormat() != SurfaceFormat::B8G8R8A8) {
    // FIXME bug 995807 (B8G8R8X8), bug 831898 (R5G6B5)
    dataSurface =
      CopySurfaceToDataSourceSurfaceWithFormat(aSurface,
                                               SurfaceFormat::B8G8R8A8);
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

  nsAutoCString encoderCID(
    NS_LITERAL_CSTRING("@mozilla.org/image/encoder;2?type=") + aMimeType);
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());
  if (!encoder) {
#ifdef DEBUG
    int32_t w = std::min(size.width, 8);
    int32_t h = std::min(size.height, 8);
    printf("Could not create encoder. Top-left %dx%d pixels contain:\n", w, h);
    for (int32_t y = 0; y < h; ++y) {
      for (int32_t x = 0; x < w; ++x) {
        printf("%x ", reinterpret_cast<uint32_t*>(map.mData)[y*map.mStride+x]);
      }
    }
#endif
    dataSurface->Unmap();
    return NS_ERROR_FAILURE;
  }

  nsresult rv = encoder->InitFromData(map.mData,
                                      BufferSizeFromStrideAndHeight(map.mStride, size.height),
                                      size.width,
                                      size.height,
                                      map.mStride,
                                      imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                      aOutputOptions);
  dataSurface->Unmap();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> imgStream;
  CallQueryInterface(encoder.get(), getter_AddRefs(imgStream));
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
  while ((rv = imgStream->Read(imgData.begin() + imgSize,
                               bufSize - imgSize,
                               &numReadThisTime)) == NS_OK && numReadThisTime > 0)
  {
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

  if (aBinaryOrData == eBinaryEncode) {
    if (aFile) {
      fwrite(imgData.begin(), 1, imgSize, aFile);
    }
    return NS_OK;
  }

  // base 64, result will be null-terminated
  nsCString encodedImg;
  rv = Base64Encode(Substring(imgData.begin(), imgSize), encodedImg);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString string("data:");
  string.Append(aMimeType);
  string.Append(";base64,");
  string.Append(encodedImg);

  if (aFile) {
#ifdef ANDROID
    if (aFile == stdout || aFile == stderr) {
      // ADB logcat cuts off long strings so we will break it down
      const char* cStr = string.BeginReading();
      size_t len = strlen(cStr);
      while (true) {
        printf_stderr("IMG: %.140s\n", cStr);
        if (len <= 140)
          break;
        len -= 140;
        cStr += 140;
      }
    }
#endif
    fprintf(aFile, "%s", string.BeginReading());
  } else {
    nsCOMPtr<nsIClipboardHelper> clipboard(do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv));
    if (clipboard) {
      clipboard->CopyString(NS_ConvertASCIItoUTF16(string), nullptr);
    }
  }
  return NS_OK;
}

/* static */ void
gfxUtils::WriteAsPNG(SourceSurface* aSurface, const nsAString& aFile)
{
  WriteAsPNG(aSurface, NS_ConvertUTF16toUTF8(aFile).get());
}

/* static */ void
gfxUtils::WriteAsPNG(SourceSurface* aSurface, const char* aFile)
{
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
      NS_WARNING("Failed to open file to create PNG!\n");
      return;
    }
  }

  EncodeSourceSurface(aSurface, NS_LITERAL_CSTRING("image/png"),
                      EmptyString(), eBinaryEncode, file);
  fclose(file);
}

/* static */ void
gfxUtils::WriteAsPNG(DrawTarget* aDT, const nsAString& aFile)
{
  WriteAsPNG(aDT, NS_ConvertUTF16toUTF8(aFile).get());
}

/* static */ void
gfxUtils::WriteAsPNG(DrawTarget* aDT, const char* aFile)
{
  RefPtr<SourceSurface> surface = aDT->Snapshot();
  if (surface) {
    WriteAsPNG(surface, aFile);
  } else {
    NS_WARNING("Failed to get surface!");
  }
}

/* static */ void
gfxUtils::WriteAsPNG(nsIPresShell* aShell, const char* aFile)
{
  int32_t width = 1000, height = 1000;
  nsRect r(0, 0, aShell->GetPresContext()->DevPixelsToAppUnits(width),
           aShell->GetPresContext()->DevPixelsToAppUnits(height));

  RefPtr<mozilla::gfx::DrawTarget> dt = gfxPlatform::GetPlatform()->
    CreateOffscreenContentDrawTarget(IntSize(width, height),
                                     SurfaceFormat::B8G8R8A8);
  NS_ENSURE_TRUE(dt, /*void*/);

  nsRefPtr<gfxContext> context = new gfxContext(dt);
  aShell->RenderDocument(r, 0, NS_RGB(255, 255, 0), context);
  WriteAsPNG(dt.get(), aFile);
}

/* static */ void
gfxUtils::DumpAsDataURI(SourceSurface* aSurface, FILE* aFile)
{
  EncodeSourceSurface(aSurface, NS_LITERAL_CSTRING("image/png"),
                      EmptyString(), eDataURIEncode, aFile);
}

/* static */ void
gfxUtils::DumpAsDataURI(DrawTarget* aDT, FILE* aFile)
{
  RefPtr<SourceSurface> surface = aDT->Snapshot();
  if (surface) {
    DumpAsDataURI(surface, aFile);
  } else {
    NS_WARNING("Failed to get surface!");
  }
}

/* static */ void
gfxUtils::CopyAsDataURI(SourceSurface* aSurface)
{
  EncodeSourceSurface(aSurface, NS_LITERAL_CSTRING("image/png"),
                      EmptyString(), eDataURIEncode, nullptr);
}

/* static */ void
gfxUtils::CopyAsDataURI(DrawTarget* aDT)
{
  RefPtr<SourceSurface> surface = aDT->Snapshot();
  if (surface) {
    CopyAsDataURI(surface);
  } else {
    NS_WARNING("Failed to get surface!");
  }
}

#ifdef MOZ_DUMP_PAINTING
static bool sDumpPaintList = getenv("MOZ_DUMP_PAINT_LIST") != 0;

/* static */ bool
gfxUtils::DumpPaintList() {
  return sDumpPaintList || gfxPrefs::LayoutDumpDisplayList();
}

bool gfxUtils::sDumpPainting = getenv("MOZ_DUMP_PAINT") != 0;
bool gfxUtils::sDumpPaintingToFile = getenv("MOZ_DUMP_PAINT_TO_FILE") != 0;
FILE *gfxUtils::sDumpPaintFile = nullptr;
#endif

namespace mozilla {
namespace gfx {

Color ToDeviceColor(Color aColor)
{
  // aColor is pass-by-value since to get return value optimization goodness we
  // need to return the same object from all return points in this function. We
  // could declare a local Color variable and use that, but we might as well
  // just use aColor.
  if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
    qcms_transform *transform = gfxPlatform::GetCMSRGBTransform();
    if (transform) {
      gfxPlatform::TransformPixel(aColor, aColor, transform);
      // Use the original alpha to avoid unnecessary float->byte->float
      // conversion errors
    }
  }
  return aColor;
}

Color ToDeviceColor(nscolor aColor)
{
  return ToDeviceColor(Color::FromABGR(aColor));
}

Color ToDeviceColor(const gfxRGBA& aColor)
{
  return ToDeviceColor(ToColor(aColor));
}

} // namespace gfx
} // namespace mozilla
