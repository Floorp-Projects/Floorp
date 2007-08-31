/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "nsThebesImage.h"
#include "nsThebesRenderingContext.h"

#include "gfxContext.h"
#include "gfxPattern.h"

#include "gfxPlatform.h"

#include "prenv.h"

static PRBool gDisableOptimize = PR_FALSE;

NS_IMPL_ISUPPORTS1(nsThebesImage, nsIImage)

nsThebesImage::nsThebesImage()
    : mFormat(gfxImageSurface::ImageFormatRGB24),
      mWidth(0),
      mHeight(0),
      mDecoded(0,0,0,0),
      mImageComplete(PR_FALSE),
      mSinglePixel(PR_FALSE),
      mAlphaDepth(0)
{
    static PRBool hasCheckedOptimize = PR_FALSE;
    if (!hasCheckedOptimize) {
        if (PR_GetEnv("MOZ_DISABLE_IMAGE_OPTIMIZE")) {
            gDisableOptimize = PR_TRUE;
        }
        hasCheckedOptimize = PR_TRUE;
    }
}

nsresult
nsThebesImage::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
    mWidth = aWidth;
    mHeight = aHeight;

    // Reject over-wide or over-tall images.
    if (!AllowedImageSize(aWidth, aHeight))
        return NS_ERROR_FAILURE;

    gfxImageSurface::gfxImageFormat format;
    switch(aMaskRequirements)
    {
        case nsMaskRequirements_kNeeds1Bit:
            format = gfxImageSurface::ImageFormatARGB32;
            mAlphaDepth = 1;
            break;
        case nsMaskRequirements_kNeeds8Bit:
            format = gfxImageSurface::ImageFormatARGB32;
            mAlphaDepth = 8;
            break;
        default:
            format = gfxImageSurface::ImageFormatRGB24;
            mAlphaDepth = 0;
            break;
    }

    mFormat = format;

#ifdef XP_WIN
    if (!ShouldUseImageSurfaces()) {
        mWinSurface = new gfxWindowsSurface(gfxIntSize(mWidth, mHeight), format);
        if (mWinSurface && mWinSurface->CairoStatus() == 0) {
            // no error
            mImageSurface = mWinSurface->GetImageSurface();
        }
    }

    if (!mImageSurface) {
        mWinSurface = nsnull;
        mImageSurface = new gfxImageSurface(gfxIntSize(mWidth, mHeight), format);
    }
#else
    mImageSurface = new gfxImageSurface(gfxIntSize(mWidth, mHeight), format);
#endif

    if (!mImageSurface || mImageSurface->CairoStatus()) {
        mImageSurface = nsnull;
        // guess
        return NS_ERROR_OUT_OF_MEMORY;
    }

    mStride = mImageSurface->Stride();

    return NS_OK;
}

nsThebesImage::~nsThebesImage()
{
}

PRInt32
nsThebesImage::GetBytesPix()
{
    return 4;
}

PRBool
nsThebesImage::GetIsRowOrderTopToBottom()
{
    return PR_TRUE;
}

PRInt32
nsThebesImage::GetWidth()
{
    return mWidth;
}

PRInt32
nsThebesImage::GetHeight()
{
    return mHeight;
}

PRUint8 *
nsThebesImage::GetBits()
{
    if (mImageSurface)
        return mImageSurface->Data();
    return nsnull;
}

PRInt32
nsThebesImage::GetLineStride()
{
    return mStride;
}

PRBool
nsThebesImage::GetHasAlphaMask()
{
    return mAlphaDepth > 0;
}

PRUint8 *
nsThebesImage::GetAlphaBits()
{
    return nsnull;
}

PRInt32
nsThebesImage::GetAlphaLineStride()
{
    return (mAlphaDepth > 0) ? mStride : 0;
}

void
nsThebesImage::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
    mDecoded.UnionRect(mDecoded, *aUpdateRect);
}

PRBool
nsThebesImage::GetIsImageComplete()
{
    if (!mImageComplete)
        mImageComplete = (mDecoded == nsRect(0, 0, mWidth, mHeight));
    return mImageComplete;
}

nsresult
nsThebesImage::Optimize(nsIDeviceContext* aContext)
{
    if (gDisableOptimize)
        return NS_OK;

    if (mOptSurface || mSinglePixel)
        return NS_OK;

    if (mWidth == 1 && mHeight == 1) {
        // yeah, let's optimize this.
        if (mFormat == gfxImageSurface::ImageFormatARGB32 ||
            mFormat == gfxImageSurface::ImageFormatRGB24)
        {
            PRUint32 pixel = *((PRUint32 *) mImageSurface->Data());

            mSinglePixelColor = gfxRGBA
                (pixel,
                 (mFormat == gfxImageSurface::ImageFormatRGB24 ?
                  gfxRGBA::PACKED_XRGB :
                  gfxRGBA::PACKED_ARGB_PREMULTIPLIED));

            mSinglePixel = PR_TRUE;

            return NS_OK;
        }

        // if it's not RGB24/ARGB32, don't optimize, but we should
        // never hit this.
    }

    // if we're being forced to use image surfaces due to
    // resource constraints, don't try to optimize beyond single-pixel.
    if (ShouldUseImageSurfaces())
        return NS_OK;

#ifdef XP_WIN
    // we need to special-case windows here, because windows has
    // a distinction between DIB and DDB and we want to use DDBs as much
    // as we can.
    if (mWinSurface) {
        // Don't do DDBs for large images; see bug 359147
        // We use 1024 as a reasonable sized maximum; the real fix
        // will be to make sure we don't ever make a DDB that's bigger
        // than the primary screen size (rule of thumb).
        if (mWidth <= 1024 && mHeight <= 1024) {
            nsRefPtr<gfxWindowsSurface> wsurf = mWinSurface->OptimizeToDDB(nsnull, gfxIntSize(mWidth, mHeight), mFormat);
            if (wsurf) {
                mOptSurface = wsurf;
            }
        }

        if (!mOptSurface) {
            // just use the DIB
            mOptSurface = mWinSurface;
        }
    } else {
        mOptSurface = gfxPlatform::GetPlatform()->OptimizeImage(mImageSurface);
    }

    mWinSurface = nsnull;
#else
    mOptSurface = gfxPlatform::GetPlatform()->OptimizeImage(mImageSurface);
#endif

    mImageSurface = nsnull;

    return NS_OK;
}

nsColorMap *
nsThebesImage::GetColorMap()
{
    return NULL;
}

PRInt8
nsThebesImage::GetAlphaDepth()
{
    return mAlphaDepth;
}

void *
nsThebesImage::GetBitInfo()
{
    return NULL;
}

NS_IMETHODIMP
nsThebesImage::LockImagePixels(PRBool aMaskPixels)
{
    if (aMaskPixels)
        return NS_ERROR_NOT_IMPLEMENTED;
    if ((mOptSurface || mSinglePixel) && !mImageSurface) {
        // Recover the pixels
        mImageSurface = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                            gfxImageSurface::ImageFormatARGB32);
        if (!mImageSurface || mImageSurface->CairoStatus())
            return NS_ERROR_OUT_OF_MEMORY;
        nsRefPtr<gfxContext> context = new gfxContext(mImageSurface);
        if (!context) {
            mImageSurface = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        context->SetOperator(gfxContext::OPERATOR_SOURCE);
        if (mSinglePixel)
            context->SetColor(mSinglePixelColor);
        else
            context->SetSource(mOptSurface);
        context->Paint();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsThebesImage::UnlockImagePixels(PRBool aMaskPixels)
{
    if (aMaskPixels)
        return NS_ERROR_NOT_IMPLEMENTED;
    if (mImageSurface && mOptSurface) {
        nsRefPtr<gfxContext> context = new gfxContext(mOptSurface);
        context->SetOperator(gfxContext::OPERATOR_SOURCE);
        context->SetSource(mImageSurface);
        context->Paint();
        // Don't need the pixel data anymore
        mImageSurface = nsnull;
    }
    return NS_OK;
}

/* NB: These are pixels, not twips. */
NS_IMETHODIMP
nsThebesImage::Draw(nsIRenderingContext &aContext,
                    const gfxRect &aSourceRect,
                    const gfxRect &aDestRect)
{
    if (NS_UNLIKELY(aDestRect.IsEmpty())) {
        NS_ERROR("nsThebesImage::Draw zero dest size - please fix caller.");
        return NS_OK;
    }

    nsThebesRenderingContext *thebesRC = static_cast<nsThebesRenderingContext*>(&aContext);
    gfxContext *ctx = thebesRC->Thebes();

#if 0
    fprintf (stderr, "nsThebesImage::Draw src [%f %f %f %f] dest [%f %f %f %f] trans: [%f %f] dec: [%f %f]\n",
             aSourceRect.pos.x, aSourceRect.pos.y, aSourceRect.size.width, aSourceRect.size.height,
             aDestRect.pos.x, aDestRect.pos.y, aDestRect.size.width, aDestRect.size.height,
             ctx->CurrentMatrix().GetTranslation().x, ctx->CurrentMatrix().GetTranslation().y,
             mDecoded.x, mDecoded.y, mDecoded.width, mDecoded.height);
#endif

    if (mSinglePixel) {
        // if a == 0, it's a noop
        if (mSinglePixelColor.a == 0.0)
            return NS_OK;

        // otherwise
        ctx->SetColor(mSinglePixelColor);
        ctx->NewPath();
        ctx->Rectangle(aDestRect, PR_TRUE);
        ctx->Fill();
        return NS_OK;
    }

    gfxFloat xscale = aDestRect.size.width / aSourceRect.size.width;
    gfxFloat yscale = aDestRect.size.height / aSourceRect.size.height;

    gfxRect srcRect(aSourceRect);
    gfxRect destRect(aDestRect);

    if (!GetIsImageComplete()) {
      srcRect = srcRect.Intersect(gfxRect(mDecoded.x, mDecoded.y,
                                          mDecoded.width, mDecoded.height));

      // This happens when mDecoded.width or height is zero. bug 368427.
      if (NS_UNLIKELY(srcRect.size.width == 0 || srcRect.size.height == 0))
          return NS_OK;

      destRect.pos.x += (srcRect.pos.x - aSourceRect.pos.x)*xscale;
      destRect.pos.y += (srcRect.pos.y - aSourceRect.pos.y)*yscale;

      destRect.size.width  = srcRect.size.width * xscale;
      destRect.size.height = srcRect.size.height * yscale;
    }

    // Reject over-wide or over-tall images.
    if (!AllowedImageSize(destRect.size.width + 1, destRect.size.height + 1))
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxPattern> pat;

    /* See bug 364968 to understand the necessity of this goop; we basically
     * have to pre-downscale any image that would fall outside of a scaled 16-bit
     * coordinate space.
     */
    if (aDestRect.pos.x * (1.0 / xscale) >= 32768.0 ||
        aDestRect.pos.y * (1.0 / yscale) >= 32768.0)
    {
        gfxIntSize dim(NS_lroundf(destRect.size.width),
                       NS_lroundf(destRect.size.height));
        nsRefPtr<gfxASurface> temp =
            gfxPlatform::GetPlatform()->CreateOffscreenSurface (dim,  mFormat);
        nsRefPtr<gfxContext> tempctx = new gfxContext(temp);

        nsRefPtr<gfxPattern> srcpat = new gfxPattern(ThebesSurface());
        gfxMatrix mat;
        mat.Translate(srcRect.pos);
        mat.Scale(1.0 / xscale, 1.0 / yscale);
        srcpat->SetMatrix(mat);

        tempctx->SetPattern(srcpat);
        tempctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        tempctx->NewPath();
        tempctx->Rectangle(gfxRect(0.0, 0.0, dim.width, dim.height));
        tempctx->Fill();

        pat = new gfxPattern(temp);

        srcRect.pos.x = 0.0;
        srcRect.pos.y = 0.0;
        srcRect.size.width = dim.width;
        srcRect.size.height = dim.height;

        xscale = 1.0;
        yscale = 1.0;
    }

    if (!pat) {
        pat = new gfxPattern(ThebesSurface());
    }

    gfxMatrix mat;
    mat.Translate(srcRect.pos);
    mat.Scale(1.0/xscale, 1.0/yscale);

    /* Translate the start point of the image (srcRect.pos)
     * to coincide with the destination rectangle origin
     */
    mat.Translate(-destRect.pos);

    pat->SetMatrix(mat);

    // XXX bug 324698
#ifndef XP_MACOSX
    if (xscale > 1.0 || yscale > 1.0) {
        // See bug 324698.  This is a workaround.
        //
        // Set the filter to CAIRO_FILTER_FAST if we're scaling up -- otherwise,
        // pixman's sampling will sample transparency for the outside edges and we'll
        // get blurry edges.  CAIRO_EXTEND_PAD would also work here, but it's not
        // implemented for image sources.
        //
        // This effectively disables smooth upscaling for images.
        pat->SetFilter(0);
    }
#endif

    ctx->NewPath();
    ctx->SetPattern(pat);
    ctx->Rectangle(destRect);
    ctx->Fill();

    return NS_OK;
}

nsresult
nsThebesImage::ThebesDrawTile(gfxContext *thebesContext,
                              nsIDeviceContext* dx,
                              const gfxPoint& offset,
                              const gfxRect& targetRect,
                              const PRInt32 xPadding,
                              const PRInt32 yPadding)
{
    NS_ASSERTION(xPadding >= 0 && yPadding >= 0, "negative padding");

    if (targetRect.size.width <= 0.0 || targetRect.size.height <= 0.0)
        return NS_OK;

    // don't do anything if we have a transparent pixel source
    if (mSinglePixel && mSinglePixelColor.a == 0.0)
        return NS_OK;

    // so we can hold on to this for a bit longer; might not be needed
    nsRefPtr<gfxPattern> pat;

    PRBool doSnap = !(thebesContext->CurrentMatrix().HasNonTranslation());
    PRBool hasPadding = ((xPadding != 0) || (yPadding != 0));

    nsRefPtr<gfxASurface> tmpSurfaceGrip;

    if (mSinglePixel && !hasPadding) {
        thebesContext->SetColor(mSinglePixelColor);
    } else {
        nsRefPtr<gfxASurface> surface;
        PRInt32 width, height;

        if (hasPadding) {
            /* Ugh we have padding; create a temporary surface that's the size of the surface + pad area,
             * and render the image into it first.  Then we'll tile that surface. */
            width = mWidth + xPadding;
            height = mHeight + yPadding;

            // Reject over-wide or over-tall images.
            if (!AllowedImageSize(width, height))
                return NS_ERROR_FAILURE;

            surface = new gfxImageSurface(gfxIntSize(width, height),
                                          gfxASurface::ImageFormatARGB32);
            if (!surface || surface->CairoStatus()) {
                return NS_ERROR_OUT_OF_MEMORY;
            }

            tmpSurfaceGrip = surface;

            nsRefPtr<gfxContext> tmpContext = new gfxContext(surface);
            if (mSinglePixel) {
                tmpContext->SetColor(mSinglePixelColor);
            } else {
                tmpContext->SetSource(ThebesSurface());
            }
            tmpContext->SetOperator(gfxContext::OPERATOR_SOURCE);
            tmpContext->Rectangle(gfxRect(0, 0, mWidth, mHeight));
            tmpContext->Fill();
        } else {
            width = mWidth;
            height = mHeight;
            surface = ThebesSurface();
        }

        gfxMatrix patMat;
        gfxPoint p0;

        p0.x = - floor(offset.x + 0.5);
        p0.y = - floor(offset.y + 0.5);
        // Scale factor to account for CSS pixels; note that the offset (and 
        // therefore p0) is in device pixels, while the width and height are in
        // CSS pixels.
        gfxFloat scale = gfxFloat(dx->AppUnitsPerDevPixel()) /
                         gfxFloat(nsIDeviceContext::AppUnitsPerCSSPixel());
        patMat.Scale(scale, scale);
        patMat.Translate(p0);

        pat = new gfxPattern(surface);
        pat->SetExtend(gfxPattern::EXTEND_REPEAT);
        pat->SetMatrix(patMat);

#ifndef XP_MACOSX
        if (scale < 1.0) {
            // See bug 324698.  This is a workaround.  See comments
            // by the earlier SetFilter call.
            pat->SetFilter(0);
        }
#endif

        thebesContext->SetPattern(pat);
    }

    thebesContext->NewPath();
    thebesContext->Rectangle(targetRect, doSnap);
    thebesContext->Fill();

    thebesContext->SetColor(gfxRGBA(0,0,0,0));

    return NS_OK;
}

PRBool
nsThebesImage::ShouldUseImageSurfaces()
{
#ifdef XP_WIN
    static const DWORD kGDIObjectsHighWaterMark = 7000;

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
        return PR_TRUE;
    }
#endif

    return PR_FALSE;
}

// A hint from the image decoders that this image has no alpha, even
// though we created is ARGB32.  This changes our format to RGB24,
// which in turn will cause us to Optimize() to RGB24.  Has no effect
// after Optimize() is called, though in all cases it will be just a
// performance win -- the pixels are still correct and have the A byte
// set to 0xff.
void
nsThebesImage::SetHasNoAlpha()
{
    if (mFormat == gfxASurface::ImageFormatARGB32)
        mFormat = gfxASurface::ImageFormatRGB24;
}
