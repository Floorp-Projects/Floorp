/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxUtils.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxDrawable.h"
#include "nsRegion.h"
#include "yuv_convert.h"
#include "ycbcr_to_rgb565.h"
#include "GeckoProfiler.h"
#include "ImageContainer.h"

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;

#include "PremultiplyTables.h"

static const uint8_t PremultiplyValue(uint8_t a, uint8_t v) {
    return gfxUtils::sPremultiplyTable[a*256+v];
}

static const uint8_t UnpremultiplyValue(uint8_t a, uint8_t v) {
    return gfxUtils::sUnpremultiplyTable[a*256+v];
}

void
gfxUtils::PremultiplyImageSurface(gfxImageSurface *aSourceSurface,
                                  gfxImageSurface *aDestSurface)
{
    if (!aDestSurface)
        aDestSurface = aSourceSurface;

    MOZ_ASSERT(aSourceSurface->Format() == aDestSurface->Format() &&
               aSourceSurface->Width()  == aDestSurface->Width() &&
               aSourceSurface->Height() == aDestSurface->Height() &&
               aSourceSurface->Stride() == aDestSurface->Stride(),
               "Source and destination surfaces don't have identical characteristics");

    MOZ_ASSERT(aSourceSurface->Stride() == aSourceSurface->Width() * 4,
               "Source surface stride isn't tightly packed");

    // Only premultiply ARGB32
    if (aSourceSurface->Format() != gfxImageFormatARGB32) {
        if (aDestSurface != aSourceSurface) {
            memcpy(aDestSurface->Data(), aSourceSurface->Data(),
                   aSourceSurface->Stride() * aSourceSurface->Height());
        }
        return;
    }

    uint8_t *src = aSourceSurface->Data();
    uint8_t *dst = aDestSurface->Data();

    uint32_t dim = aSourceSurface->Width() * aSourceSurface->Height();
    for (uint32_t i = 0; i < dim; ++i) {
#ifdef IS_LITTLE_ENDIAN
        uint8_t b = *src++;
        uint8_t g = *src++;
        uint8_t r = *src++;
        uint8_t a = *src++;

        *dst++ = PremultiplyValue(a, b);
        *dst++ = PremultiplyValue(a, g);
        *dst++ = PremultiplyValue(a, r);
        *dst++ = a;
#else
        uint8_t a = *src++;
        uint8_t r = *src++;
        uint8_t g = *src++;
        uint8_t b = *src++;

        *dst++ = a;
        *dst++ = PremultiplyValue(a, r);
        *dst++ = PremultiplyValue(a, g);
        *dst++ = PremultiplyValue(a, b);
#endif
    }
}

void
gfxUtils::UnpremultiplyImageSurface(gfxImageSurface *aSourceSurface,
                                    gfxImageSurface *aDestSurface)
{
    if (!aDestSurface)
        aDestSurface = aSourceSurface;

    MOZ_ASSERT(aSourceSurface->Format() == aDestSurface->Format() &&
               aSourceSurface->Width()  == aDestSurface->Width() &&
               aSourceSurface->Height() == aDestSurface->Height() &&
               aSourceSurface->Stride() == aDestSurface->Stride(),
               "Source and destination surfaces don't have identical characteristics");

    MOZ_ASSERT(aSourceSurface->Stride() == aSourceSurface->Width() * 4,
               "Source surface stride isn't tightly packed");

    // Only premultiply ARGB32
    if (aSourceSurface->Format() != gfxImageFormatARGB32) {
        if (aDestSurface != aSourceSurface) {
            memcpy(aDestSurface->Data(), aSourceSurface->Data(),
                   aSourceSurface->Stride() * aSourceSurface->Height());
        }
        return;
    }

    uint8_t *src = aSourceSurface->Data();
    uint8_t *dst = aDestSurface->Data();

    uint32_t dim = aSourceSurface->Width() * aSourceSurface->Height();
    for (uint32_t i = 0; i < dim; ++i) {
#ifdef IS_LITTLE_ENDIAN
        uint8_t b = *src++;
        uint8_t g = *src++;
        uint8_t r = *src++;
        uint8_t a = *src++;

        *dst++ = UnpremultiplyValue(a, b);
        *dst++ = UnpremultiplyValue(a, g);
        *dst++ = UnpremultiplyValue(a, r);
        *dst++ = a;
#else
        uint8_t a = *src++;
        uint8_t r = *src++;
        uint8_t g = *src++;
        uint8_t b = *src++;

        *dst++ = a;
        *dst++ = UnpremultiplyValue(a, r);
        *dst++ = UnpremultiplyValue(a, g);
        *dst++ = UnpremultiplyValue(a, b);
#endif
    }
}

void
gfxUtils::ConvertBGRAtoRGBA(gfxImageSurface *aSourceSurface,
                            gfxImageSurface *aDestSurface) {
    if (!aDestSurface)
        aDestSurface = aSourceSurface;

    MOZ_ASSERT(aSourceSurface->Format() == aDestSurface->Format() &&
               aSourceSurface->Width()  == aDestSurface->Width() &&
               aSourceSurface->Height() == aDestSurface->Height() &&
               aSourceSurface->Stride() == aDestSurface->Stride(),
               "Source and destination surfaces don't have identical characteristics");

    MOZ_ASSERT(aSourceSurface->Stride() == aSourceSurface->Width() * 4,
               "Source surface stride isn't tightly packed");

    MOZ_ASSERT(aSourceSurface->Format() == gfxImageFormatARGB32 || aSourceSurface->Format() == gfxImageFormatRGB24,
               "Surfaces must be ARGB32 or RGB24");

    uint8_t *src = aSourceSurface->Data();
    uint8_t *dst = aDestSurface->Data();

    uint32_t dim = aSourceSurface->Width() * aSourceSurface->Height();
    uint8_t *srcEnd = src + 4*dim;

    if (src == dst) {
        uint8_t buffer[4];
        for (; src != srcEnd; src += 4) {
            buffer[0] = src[2];
            buffer[1] = src[1];
            buffer[2] = src[0];

            src[0] = buffer[0];
            src[1] = buffer[1];
            src[2] = buffer[2];
        }
    } else {
        for (; src != srcEnd; src += 4, dst += 4) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
        }
    }
}

static bool
IsSafeImageTransformComponent(gfxFloat aValue)
{
  return aValue >= -32768 && aValue <= 32767;
}

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

#ifndef MOZ_GFX_OPTIMIZE_MOBILE
// EXTEND_PAD won't help us here; we have to create a temporary surface to hold
// the subimage of pixels we're allowed to sample.
static already_AddRefed<gfxDrawable>
CreateSamplingRestrictedDrawable(gfxDrawable* aDrawable,
                                 gfxContext* aContext,
                                 const gfxMatrix& aUserSpaceToImageSpace,
                                 const gfxRect& aSourceRect,
                                 const gfxRect& aSubimage,
                                 const gfxImageFormat aFormat)
{
    PROFILER_LABEL("gfxUtils", "CreateSamplingRestricedDrawable");
    gfxRect userSpaceClipExtents = aContext->GetClipExtents();
    // This isn't optimal --- if aContext has a rotation then GetClipExtents
    // will have to do a bounding-box computation, and TransformBounds might
    // too, so we could get a better result if we computed image space clip
    // extents in one go --- but it doesn't really matter and this is easier
    // to understand.
    gfxRect imageSpaceClipExtents =
        aUserSpaceToImageSpace.TransformBounds(userSpaceClipExtents);
    // Inflate by one pixel because bilinear filtering will sample at most
    // one pixel beyond the computed image pixel coordinate.
    imageSpaceClipExtents.Inflate(1.0);

    gfxRect needed = imageSpaceClipExtents.Intersect(aSourceRect);
    needed = needed.Intersect(aSubimage);
    needed.RoundOut();

    // if 'needed' is empty, nothing will be drawn since aFill
    // must be entirely outside the clip region, so it doesn't
    // matter what we do here, but we should avoid trying to
    // create a zero-size surface.
    if (needed.IsEmpty())
        return nullptr;

    nsRefPtr<gfxASurface> temp;
    gfxIntSize size(int32_t(needed.Width()), int32_t(needed.Height()));

    nsRefPtr<gfxImageSurface> image = aDrawable->GetAsImageSurface();
    if (image && gfxRect(0, 0, image->GetSize().width, image->GetSize().height).Contains(needed)) {
      temp = image->GetSubimage(needed);
    } else {
      temp =
          gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, gfxASurface::ContentFromFormat(aFormat));
      if (!temp || temp->CairoStatus())
          return nullptr;

      nsRefPtr<gfxContext> tmpCtx = new gfxContext(temp);
      tmpCtx->SetOperator(OptimalFillOperator());
      aDrawable->Draw(tmpCtx, needed - needed.TopLeft(), true,
                      GraphicsFilter::FILTER_FAST, gfxMatrix().Translate(needed.TopLeft()));
    }

    nsRefPtr<gfxDrawable> drawable = 
        new gfxSurfaceDrawable(temp, size, gfxMatrix().Translate(-needed.TopLeft()));
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
        if (!aSurface || aSurface->GetType() == gfxSurfaceTypeQuartz)
            return;

        if (!IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.xx) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.xy) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.yx) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.yy)) {
            NS_WARNING("Scaling up too much, bailing out");
            mSucceeded = false;
            return;
        }

        if (IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.x0) &&
            IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.y0))
            return;

        // We'll push a group, which will hopefully reduce our transform's
        // translation so it's in bounds.
        gfxMatrix currentMatrix = mContext->CurrentMatrix();
        mContext->Save();

        // Clip the rounded-out-to-device-pixels bounds of the
        // transformed fill area. This is the area for the group we
        // want to push.
        mContext->IdentityMatrix();
        gfxRect bounds = currentMatrix.TransformBounds(aFill);
        bounds.RoundOut();
        mContext->Clip(bounds);
        mContext->SetMatrix(currentMatrix);
        mContext->PushGroup(GFX_CONTENT_COLOR_ALPHA);
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
DeviceToImageTransform(gfxContext* aContext,
                       const gfxMatrix& aUserSpaceToImageSpace)
{
    gfxFloat deviceX, deviceY;
    nsRefPtr<gfxASurface> currentTarget =
        aContext->CurrentSurface(&deviceX, &deviceY);
    gfxMatrix currentMatrix = aContext->CurrentMatrix();
    gfxMatrix deviceToUser = gfxMatrix(currentMatrix).Invert();
    deviceToUser.Translate(-gfxPoint(-deviceX, -deviceY));
    return gfxMatrix(deviceToUser).Multiply(aUserSpaceToImageSpace);
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
gfxUtils::DrawPixelSnapped(gfxContext*      aContext,
                           gfxDrawable*     aDrawable,
                           const gfxMatrix& aUserSpaceToImageSpace,
                           const gfxRect&   aSubimage,
                           const gfxRect&   aSourceRect,
                           const gfxRect&   aImageRect,
                           const gfxRect&   aFill,
                           const gfxImageFormat aFormat,
                           GraphicsFilter aFilter,
                           uint32_t         aImageFlags)
{
    PROFILER_LABEL("gfxUtils", "DrawPixelSnapped");
    bool doTile = !aImageRect.Contains(aSourceRect) &&
                  !(aImageFlags & imgIContainer::FLAG_CLAMP);

    nsRefPtr<gfxASurface> currentTarget = aContext->CurrentSurface();
    gfxMatrix deviceSpaceToImageSpace =
        DeviceToImageTransform(aContext, aUserSpaceToImageSpace);

    AutoCairoPixmanBugWorkaround workaround(aContext, deviceSpaceToImageSpace,
                                            aFill, currentTarget);
    if (!workaround.Succeeded())
        return;

    nsRefPtr<gfxDrawable> drawable = aDrawable;

    aFilter = ReduceResamplingFilter(aFilter, aImageRect.Width(), aImageRect.Height(), aSourceRect.Width(), aSourceRect.Height());

    gfxMatrix userSpaceToImageSpace = aUserSpaceToImageSpace;

    // On Mobile, we don't ever want to do this; it has the potential for
    // allocating very large temporary surfaces, especially since we'll
    // do full-page snapshots often (see bug 749426).
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    // If the pattern translation is large we can get into trouble with pixman's
    // 16 bit coordinate limits. For now, we only do this on platforms where
    // we know we have the pixman limits. 16384.0 is a somewhat arbitrary
    // large number to make sure we avoid the expensive fmod when we can, but
    // still maintain a safe margin from the actual limit
    if (doTile && (userSpaceToImageSpace.y0 > 16384.0 || userSpaceToImageSpace.x0 > 16384.0)) {
        userSpaceToImageSpace.x0 = fmod(userSpaceToImageSpace.x0, aImageRect.width);
        userSpaceToImageSpace.y0 = fmod(userSpaceToImageSpace.y0, aImageRect.height);
    }
#else
    // OK now, the hard part left is to account for the subimage sampling
    // restriction. If all the transforms involved are just integer
    // translations, then we assume no resampling will occur so there's
    // nothing to do.
    // XXX if only we had source-clipping in cairo!
    if (aContext->CurrentMatrix().HasNonIntegerTranslation() ||
        aUserSpaceToImageSpace.HasNonIntegerTranslation()) {
        if (doTile || !aSubimage.Contains(aImageRect)) {
            nsRefPtr<gfxDrawable> restrictedDrawable =
              CreateSamplingRestrictedDrawable(aDrawable, aContext,
                                               aUserSpaceToImageSpace, aSourceRect,
                                               aSubimage, aFormat);
            if (restrictedDrawable) {
                drawable.swap(restrictedDrawable);
            }
        }
        // We no longer need to tile: Either we never needed to, or we already
        // filled a surface with the tiled pattern; this surface can now be
        // drawn without tiling.
        doTile = false;
    }
#endif

    gfxContext::GraphicsOperator op = aContext->CurrentOperator();
    if ((op == gfxContext::OPERATOR_OVER || workaround.PushedGroup()) &&
        aFormat == gfxImageFormatRGB24) {
        aContext->SetOperator(OptimalFillOperator());
    }

    drawable->Draw(aContext, aFill, doTile, aFilter, userSpaceToImageSpace);

    aContext->SetOperator(op);
}

/* static */ int
gfxUtils::ImageFormatToDepth(gfxImageFormat aFormat)
{
    switch (aFormat) {
        case gfxImageFormatARGB32:
            return 32;
        case gfxImageFormatRGB24:
            return 24;
        case gfxImageFormatRGB16_565:
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
PathFromRegionInternal(gfx::DrawTarget* aTarget, const nsIntRegion& aRegion,
                       bool aSnap)
{
  Matrix mat = aTarget->GetTransform();
  const gfxFloat epsilon = 0.000001;
#define WITHIN_E(a,b) (fabs((a)-(b)) < epsilon)
  // We're essentially duplicating the logic in UserToDevicePixelSnapped here.
  bool shouldNotSnap = !aSnap || (WITHIN_E(mat._11,1.0) &&
                                  WITHIN_E(mat._22,1.0) &&
                                  WITHIN_E(mat._12,0.0) &&
                                  WITHIN_E(mat._21,0.0));
#undef WITHIN_E

  RefPtr<PathBuilder> pb = aTarget->CreatePathBuilder();
  nsIntRegionRectIterator iter(aRegion);

  const nsIntRect* r;
  if (shouldNotSnap) {
    while ((r = iter.Next()) != nullptr) {
      pb->MoveTo(Point(r->x, r->y));
      pb->LineTo(Point(r->XMost(), r->y));
      pb->LineTo(Point(r->XMost(), r->YMost()));
      pb->LineTo(Point(r->x, r->YMost()));
      pb->Close();
    }
  } else {
    while ((r = iter.Next()) != nullptr) {
      Rect rect(r->x, r->y, r->width, r->height);

      rect.Round();
      pb->MoveTo(rect.TopLeft());
      pb->LineTo(rect.TopRight());
      pb->LineTo(rect.BottomRight());
      pb->LineTo(rect.BottomLeft());
      pb->Close();
    }
  }
  RefPtr<Path> path = pb->Finish();
  return path;
}

static void
ClipToRegionInternal(gfx::DrawTarget* aTarget, const nsIntRegion& aRegion,
                     bool aSnap)
{
  RefPtr<Path> path = PathFromRegionInternal(aTarget, aRegion, aSnap);
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
  ClipToRegionInternal(aTarget, aRegion, false);
}

/*static*/ void
gfxUtils::ClipToRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aContext, aRegion, true);
}

/*static*/ void
gfxUtils::ClipToRegionSnapped(DrawTarget* aTarget, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aTarget, aRegion, true);
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

  // If power is within 1e-6 of an integer, round to nearest to
  // prevent floating point errors, otherwise round up to the
  // next integer value.
  if (fabs(power - NS_round(power)) < 1e-6) {
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
    m.xy = m.yx = 0.0;
    m.xx = (aToBottomRight.x - aToTopLeft.x)/aFrom.width;
    m.yy = (aToBottomRight.y - aToTopLeft.y)/aFrom.height;
    m.x0 = aToTopLeft.x - m.xx*aFrom.x;
    m.y0 = aToTopLeft.y - m.yy*aFrom.y;
  } else {
    NS_ASSERTION(aToTopRight.y == aToBottomRight.y && aToTopRight.x == aToTopLeft.x,
                 "Destination rectangle not axis-aligned");
    m.xx = m.yy = 0.0;
    m.xy = (aToBottomRight.x - aToTopLeft.x)/aFrom.height;
    m.yx = (aToBottomRight.y - aToTopLeft.y)/aFrom.width;
    m.x0 = aToTopLeft.x - m.xy*aFrom.y;
    m.y0 = aToTopLeft.y - m.yx*aFrom.x;
  }
  return m;
}

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
  gfx::YUVType yuvtype =
    gfx::TypeFromSize(aData.mYSize.width,
                      aData.mYSize.height,
                      aData.mCbCrSize.width,
                      aData.mCbCrSize.height);

  // 'prescale' is true if the scaling is to be done as part of the
  // YCbCr to RGB conversion rather than on the RGB data when rendered.
  bool prescale = aSuggestedSize.width > 0 && aSuggestedSize.height > 0 &&
                    aSuggestedSize != aData.mPicSize;

  if (aSuggestedFormat == gfxImageFormatRGB16_565) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (prescale &&
        !gfx::IsScaleYCbCrToRGB565Fast(aData.mPicX,
                                       aData.mPicY,
                                       aData.mPicSize.width,
                                       aData.mPicSize.height,
                                       aSuggestedSize.width,
                                       aSuggestedSize.height,
                                       yuvtype,
                                       gfx::FILTER_BILINEAR) &&
        gfx::IsConvertYCbCrToRGB565Fast(aData.mPicX,
                                        aData.mPicY,
                                        aData.mPicSize.width,
                                        aData.mPicSize.height,
                                        yuvtype)) {
      prescale = false;
    }
#else
    // yuv2rgb16 function not available
    aSuggestedFormat = gfxImageFormatRGB24;
#endif
  }
  else if (aSuggestedFormat != gfxImageFormatRGB24) {
    // No other formats are currently supported.
    aSuggestedFormat = gfxImageFormatRGB24;
  }
  if (aSuggestedFormat == gfxImageFormatRGB24) {
    /* ScaleYCbCrToRGB32 does not support a picture offset, nor 4:4:4 data.
       See bugs 639415 and 640073. */
    if (aData.mPicX != 0 || aData.mPicY != 0 || yuvtype == gfx::YV24)
      prescale = false;
  }
  if (!prescale) {
    aSuggestedSize = aData.mPicSize;
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
  gfx::YUVType yuvtype =
    gfx::TypeFromSize(aData.mYSize.width,
                      aData.mYSize.height,
                      aData.mCbCrSize.width,
                      aData.mCbCrSize.height);

  // Convert from YCbCr to RGB now, scaling the image if needed.
  if (aDestSize != aData.mPicSize) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (aDestFormat == gfxImageFormatRGB16_565) {
      gfx::ScaleYCbCrToRGB565(aData.mYChannel,
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
                              gfx::FILTER_BILINEAR);
    } else
#endif
      gfx::ScaleYCbCrToRGB32(aData.mYChannel,
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
                             gfx::ROTATE_0,
                             gfx::FILTER_BILINEAR);
  } else { // no prescale
#if defined(HAVE_YCBCR_TO_RGB565)
    if (aDestFormat == gfxImageFormatRGB16_565) {
      gfx::ConvertYCbCrToRGB565(aData.mYChannel,
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
    } else // aDestFormat != gfxImageFormatRGB16_565
#endif
      gfx::ConvertYCbCrToRGB32(aData.mYChannel,
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

#ifdef MOZ_DUMP_PAINTING
/* static */ void
gfxUtils::WriteAsPNG(DrawTarget* aDT, const char* aFile)
{
  aDT->Flush();
  nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(aDT);
  if (surf) {
    surf->WriteAsPNG(aFile);
  } else {
    NS_WARNING("Failed to get Thebes surface!");
  }
}

/* static */ void
gfxUtils::DumpAsDataURL(DrawTarget* aDT)
{
  aDT->Flush();
  nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(aDT);
  if (surf) {
    surf->DumpAsDataURL();
  } else {
    NS_WARNING("Failed to get Thebes surface!");
  }
}

/* static */ void
gfxUtils::CopyAsDataURL(DrawTarget* aDT)
{
  aDT->Flush();
  nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(aDT);
  if (surf) {
    surf->CopyAsDataURL();
  } else {
    NS_WARNING("Failed to get Thebes surface!");
  }
}

bool gfxUtils::sDumpPaintList = getenv("MOZ_DUMP_PAINT_LIST") != 0;
bool gfxUtils::sDumpPainting = getenv("MOZ_DUMP_PAINT") != 0;
bool gfxUtils::sDumpPaintingToFile = getenv("MOZ_DUMP_PAINT_TO_FILE") != 0;
FILE *gfxUtils::sDumpPaintFile = nullptr;
#endif
