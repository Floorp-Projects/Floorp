/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfxUtils.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxDrawable.h"
#include "nsRegion.h"

#if defined(XP_WIN) || defined(WINCE)
#include "gfxWindowsPlatform.h"
#endif

static PRUint8 sUnpremultiplyTable[256*256];
static PRUint8 sPremultiplyTable[256*256];
static PRBool sTablesInitialized = PR_FALSE;

static const PRUint8 PremultiplyValue(PRUint8 a, PRUint8 v) {
    return sPremultiplyTable[a*256+v];
}

static const PRUint8 UnpremultiplyValue(PRUint8 a, PRUint8 v) {
    return sUnpremultiplyTable[a*256+v];
}

static void
CalculateTables()
{
    // It's important that the array be indexed first by alpha and then by rgb
    // value.  When we unpremultiply a pixel, we're guaranteed to do three
    // lookups with the same alpha; indexing by alpha first makes it likely that
    // those three lookups will be close to one another in memory, thus
    // increasing the chance of a cache hit.

    // Unpremultiply table

    // a == 0 case
    for (PRUint32 c = 0; c <= 255; c++) {
        sUnpremultiplyTable[c] = c;
    }

    for (int a = 1; a <= 255; a++) {
        for (int c = 0; c <= 255; c++) {
            sUnpremultiplyTable[a*256+c] = (PRUint8)((c * 255) / a);
        }
    }

    // Premultiply table

    for (int a = 0; a <= 255; a++) {
        for (int c = 0; c <= 255; c++) {
            sPremultiplyTable[a*256+c] = (a * c + 254) / 255;
        }
    }

    sTablesInitialized = PR_TRUE;
}

void
gfxUtils::PremultiplyImageSurface(gfxImageSurface *aSourceSurface,
                                  gfxImageSurface *aDestSurface)
{
    if (!aDestSurface)
        aDestSurface = aSourceSurface;

    NS_ASSERTION(aSourceSurface->Format() == aDestSurface->Format() &&
                 aSourceSurface->Width() == aDestSurface->Width() &&
                 aSourceSurface->Height() == aDestSurface->Height() &&
                 aSourceSurface->Stride() == aDestSurface->Stride(),
                 "Source and destination surfaces don't have identical characteristics");

    NS_ASSERTION(aSourceSurface->Stride() == aSourceSurface->Width() * 4,
                 "Source surface stride isn't tightly packed");

    // Only premultiply ARGB32
    if (aSourceSurface->Format() != gfxASurface::ImageFormatARGB32) {
        if (aDestSurface != aSourceSurface) {
            memcpy(aDestSurface->Data(), aSourceSurface->Data(),
                   aSourceSurface->Stride() * aSourceSurface->Height());
        }
        return;
    }

    if (!sTablesInitialized)
        CalculateTables();

    PRUint8 *src = aSourceSurface->Data();
    PRUint8 *dst = aDestSurface->Data();

    PRUint32 dim = aSourceSurface->Width() * aSourceSurface->Height();
    for (PRUint32 i = 0; i < dim; ++i) {
#ifdef IS_LITTLE_ENDIAN
        PRUint8 b = *src++;
        PRUint8 g = *src++;
        PRUint8 r = *src++;
        PRUint8 a = *src++;

        *dst++ = PremultiplyValue(a, b);
        *dst++ = PremultiplyValue(a, g);
        *dst++ = PremultiplyValue(a, r);
        *dst++ = a;
#else
        PRUint8 a = *src++;
        PRUint8 r = *src++;
        PRUint8 g = *src++;
        PRUint8 b = *src++;

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

    NS_ASSERTION(aSourceSurface->Format() == aDestSurface->Format() &&
                 aSourceSurface->Width() == aDestSurface->Width() &&
                 aSourceSurface->Height() == aDestSurface->Height() &&
                 aSourceSurface->Stride() == aDestSurface->Stride(),
                 "Source and destination surfaces don't have identical characteristics");

    NS_ASSERTION(aSourceSurface->Stride() == aSourceSurface->Width() * 4,
                 "Source surface stride isn't tightly packed");

    // Only premultiply ARGB32
    if (aSourceSurface->Format() != gfxASurface::ImageFormatARGB32) {
        if (aDestSurface != aSourceSurface) {
            memcpy(aDestSurface->Data(), aSourceSurface->Data(),
                   aSourceSurface->Stride() * aSourceSurface->Height());
        }
        return;
    }

    if (!sTablesInitialized)
        CalculateTables();

    PRUint8 *src = aSourceSurface->Data();
    PRUint8 *dst = aDestSurface->Data();

    PRUint32 dim = aSourceSurface->Width() * aSourceSurface->Height();
    for (PRUint32 i = 0; i < dim; ++i) {
#ifdef IS_LITTLE_ENDIAN
        PRUint8 b = *src++;
        PRUint8 g = *src++;
        PRUint8 r = *src++;
        PRUint8 a = *src++;

        *dst++ = UnpremultiplyValue(a, b);
        *dst++ = UnpremultiplyValue(a, g);
        *dst++ = UnpremultiplyValue(a, r);
        *dst++ = a;
#else
        PRUint8 a = *src++;
        PRUint8 r = *src++;
        PRUint8 g = *src++;
        PRUint8 b = *src++;

        *dst++ = a;
        *dst++ = UnpremultiplyValue(a, r);
        *dst++ = UnpremultiplyValue(a, g);
        *dst++ = UnpremultiplyValue(a, b);
#endif
    }
}

static PRBool
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

// EXTEND_PAD won't help us here; we have to create a temporary surface to hold
// the subimage of pixels we're allowed to sample.
static already_AddRefed<gfxDrawable>
CreateSamplingRestrictedDrawable(gfxDrawable* aDrawable,
                                 gfxContext* aContext,
                                 const gfxMatrix& aUserSpaceToImageSpace,
                                 const gfxRect& aSourceRect,
                                 const gfxRect& aSubimage,
                                 const gfxImageSurface::gfxImageFormat aFormat)
{
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
    imageSpaceClipExtents.Outset(1.0);

    gfxRect needed = imageSpaceClipExtents.Intersect(aSourceRect);
    needed = needed.Intersect(aSubimage);
    needed.RoundOut();

    // if 'needed' is empty, nothing will be drawn since aFill
    // must be entirely outside the clip region, so it doesn't
    // matter what we do here, but we should avoid trying to
    // create a zero-size surface.
    if (needed.IsEmpty())
        return nsnull;

    gfxIntSize size(PRInt32(needed.Width()), PRInt32(needed.Height()));
    nsRefPtr<gfxASurface> temp =
        gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, gfxASurface::ContentFromFormat(aFormat));
    if (!temp || temp->CairoStatus())
        return nsnull;

    nsRefPtr<gfxContext> tmpCtx = new gfxContext(temp);
    tmpCtx->SetOperator(OptimalFillOperator());
    aDrawable->Draw(tmpCtx, needed - needed.pos, PR_TRUE,
                    gfxPattern::FILTER_FAST, gfxMatrix().Translate(needed.pos));

    nsRefPtr<gfxPattern> resultPattern = new gfxPattern(temp);
    if (!resultPattern)
        return nsnull;

    nsRefPtr<gfxDrawable> drawable = 
        new gfxSurfaceDrawable(temp, size, gfxMatrix().Translate(-needed.pos));
    return drawable.forget();
}

// working around cairo/pixman bug (bug 364968)
// Our device-space-to-image-space transform may not be acceptable to pixman.
struct NS_STACK_CLASS AutoCairoPixmanBugWorkaround
{
    AutoCairoPixmanBugWorkaround(gfxContext*      aContext,
                                 const gfxMatrix& aDeviceSpaceToImageSpace,
                                 const gfxRect&   aFill,
                                 const gfxASurface::gfxSurfaceType& aSurfaceType)
     : mContext(aContext), mSucceeded(PR_TRUE), mPushedGroup(PR_FALSE)
    {
        // Quartz's limits for matrix are much larger than pixman
        if (aSurfaceType == gfxASurface::SurfaceTypeQuartz)
            return;

        if (!IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.xx) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.xy) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.yx) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.yy)) {
            NS_WARNING("Scaling up too much, bailing out");
            mSucceeded = PR_FALSE;
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
        mContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
        mContext->SetOperator(gfxContext::OPERATOR_OVER);

        mPushedGroup = PR_TRUE;
    }

    ~AutoCairoPixmanBugWorkaround()
    {
        if (mPushedGroup) {
            mContext->PopGroupToSource();
            mContext->Paint();
            mContext->Restore();
        }
    }

    PRBool PushedGroup() { return mPushedGroup; }
    PRBool Succeeded() { return mSucceeded; }

private:
    gfxContext* mContext;
    PRPackedBool mSucceeded;
    PRPackedBool mPushedGroup;
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

/* static */ void
gfxUtils::DrawPixelSnapped(gfxContext*      aContext,
                           gfxDrawable*     aDrawable,
                           const gfxMatrix& aUserSpaceToImageSpace,
                           const gfxRect&   aSubimage,
                           const gfxRect&   aSourceRect,
                           const gfxRect&   aImageRect,
                           const gfxRect&   aFill,
                           const gfxImageSurface::gfxImageFormat aFormat,
                           const gfxPattern::GraphicsFilter& aFilter)
{
    PRBool doTile = !aImageRect.Contains(aSourceRect);

    nsRefPtr<gfxASurface> currentTarget = aContext->CurrentSurface();
    gfxASurface::gfxSurfaceType surfaceType = currentTarget->GetType();
    gfxMatrix deviceSpaceToImageSpace =
        DeviceToImageTransform(aContext, aUserSpaceToImageSpace);

    AutoCairoPixmanBugWorkaround workaround(aContext, deviceSpaceToImageSpace,
                                            aFill, surfaceType);
    if (!workaround.Succeeded())
        return;

    nsRefPtr<gfxDrawable> drawable = aDrawable;

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
        doTile = PR_FALSE;
    }

    gfxContext::GraphicsOperator op = aContext->CurrentOperator();
    if ((op == gfxContext::OPERATOR_OVER || workaround.PushedGroup()) &&
        aFormat == gfxASurface::ImageFormatRGB24) {
        aContext->SetOperator(OptimalFillOperator());
    }

    drawable->Draw(aContext, aFill, doTile, aFilter, aUserSpaceToImageSpace);

    aContext->SetOperator(op);
}

/* static */ int
gfxUtils::ImageFormatToDepth(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
        case gfxASurface::ImageFormatARGB32:
            return 32;
        case gfxASurface::ImageFormatRGB24:
            return 24;
        case gfxASurface::ImageFormatRGB16_565:
            return 16;
        default:
            break;
    }
    return 0;
}
static void
ClipToRegionInternal(gfxContext* aContext, const nsIntRegion& aRegion,
                     PRBool aSnap)
{
  aContext->NewPath();
  nsIntRegionRectIterator iter(aRegion);
  const nsIntRect* r;
  while ((r = iter.Next()) != nsnull) {
    aContext->Rectangle(gfxRect(r->x, r->y, r->width, r->height), aSnap);
  }
  aContext->Clip();
}

/*static*/ void
gfxUtils::ClipToRegion(gfxContext* aContext, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aContext, aRegion, PR_FALSE);
}

/*static*/ void
gfxUtils::ClipToRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aContext, aRegion, PR_TRUE);
}
