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
    mWinSurface = new gfxWindowsSurface(mWidth, mHeight, format);
    mImageSurface = mWinSurface->GetImageSurface();

    if (!mImageSurface) {
        mWinSurface = nsnull;
        mImageSurface = new gfxImageSurface(format, mWidth, mHeight);
    }
#else
    mImageSurface = new gfxImageSurface(format, mWidth, mHeight);
#endif
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
            nsRefPtr<gfxWindowsSurface> wsurf = mWinSurface->OptimizeToDDB(nsnull, mFormat, mWidth, mHeight);
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
        mImageSurface = new gfxImageSurface(gfxImageSurface::ImageFormatARGB32,
                                            mWidth, mHeight);
        if (!mImageSurface)
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
        // Don't need the pixel data anymore
        mImageSurface = nsnull;
    }
    return NS_OK;
}

/* NB: These are pixels, not twips. */
NS_IMETHODIMP
nsThebesImage::Draw(nsIRenderingContext &aContext, nsIDrawingSurface *aSurface,
                    PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
    return Draw(aContext, aSurface, 0, 0, mWidth, mHeight, aX, aY, aWidth, aHeight);
}

/* NB: These are pixels, not twips. */
/* BUT nsRenderingContextImpl's DrawImage calls this with twips. */
NS_IMETHODIMP
nsThebesImage::Draw(nsIRenderingContext &aContext, nsIDrawingSurface *aSurface,
                   PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                   PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
    nsThebesRenderingContext *thebesRC = NS_STATIC_CAST(nsThebesRenderingContext*, &aContext);
    gfxContext *ctx = thebesRC->Thebes();

#if 0
    fprintf (stderr, "nsThebesImage::Draw src [%d %d %d %d] dest [%d %d %d %d] tx [%f %f] dec [%d %d %d %d]\n",
             aSX, aSY, aSWidth, aSHeight, aDX, aDY, aDWidth, aDHeight,
             ctx->CurrentMatrix().GetTranslation().x, ctx->CurrentMatrix().GetTranslation().y,
             mDecoded.x, mDecoded.y, mDecoded.width, mDecoded.height);
#endif

    gfxMatrix savedCTM(ctx->CurrentMatrix());
    PRBool doSnap = !(savedCTM.HasNonTranslation());

    if (mSinglePixel) {
        // if a == 0, it's a noop
        if (mSinglePixelColor.a == 0.0)
            return NS_OK;

        // otherwise
        ctx->SetColor(mSinglePixelColor);
        ctx->NewPath();
        ctx->Rectangle(gfxRect(aDX, aDY, aDWidth, aDHeight), PR_TRUE);
        ctx->Fill();
        return NS_OK;
    }

    // See comment inside ThebesDrawTile
    if (doSnap) {
        gfxMatrix roundedCTM(savedCTM);
        roundedCTM.x0 = ::floor(roundedCTM.x0 + 0.5);
        roundedCTM.y0 = ::floor(roundedCTM.y0 + 0.5);
        ctx->SetMatrix(roundedCTM);
    }

    gfxFloat xscale = gfxFloat(aDWidth) / aSWidth;
    gfxFloat yscale = gfxFloat(aDHeight) / aSHeight;

    if (!GetIsImageComplete()) {
      nsRect srcRect(aSX, aSY, aSWidth, aSHeight);
      srcRect.IntersectRect(srcRect, mDecoded);

      // This happens when mDecoded.width or height is zero. bug 368427.
      if (NS_UNLIKELY(srcRect.width == 0 || srcRect.height == 0))
          return NS_OK;

      aDX += (PRInt32)((srcRect.x - aSX)*xscale);
      aDY += (PRInt32)((srcRect.y - aSY)*yscale);

      // use '+ 1 - *scale' to get rid of rounding errors
      aDWidth  = (PRInt32)((srcRect.width)*xscale + 1 - xscale);
      aDHeight = (PRInt32)((srcRect.height)*yscale + 1 - yscale);

      aSX = srcRect.x;
      aSY = srcRect.y;
    }

    // Reject over-wide or over-tall images.
    if (!AllowedImageSize(aDWidth, aDHeight))
        return NS_ERROR_FAILURE;

    gfxRect dr(aDX, aDY, aDWidth, aDHeight);

    gfxMatrix mat;
    mat.Translate(gfxPoint(aSX, aSY));
    mat.Scale(1.0/xscale, 1.0/yscale);

    /* Translate the start point of the image (the aSX,aSY point)
     * to coincide with the destination rectangle origin
     */
    mat.Translate(gfxPoint(-aDX, -aDY));

    nsRefPtr<gfxPattern> pat = new gfxPattern(ThebesSurface());
    pat->SetMatrix(mat);

    ctx->NewPath();
    ctx->SetPattern(pat);
    ctx->Rectangle(dr, doSnap);
    ctx->Fill();

    if (doSnap)
        ctx->SetMatrix(savedCTM);

    return NS_OK;
}

nsresult
nsThebesImage::ThebesDrawTile(gfxContext *thebesContext,
                              const gfxPoint& offset,
                              const gfxRect& targetRect,
                              const PRInt32 xPadding,
                              const PRInt32 yPadding)
{
    if (targetRect.size.width <= 0.0 || targetRect.size.height <= 0.0)
        return NS_OK;

    // don't do anything if we have a transparent pixel source
    if (mSinglePixel && mSinglePixelColor.a == 0.0)
        return NS_OK;

    // so we can hold on to this for a bit longer; might not be needed
    nsRefPtr<gfxPattern> pat;

    gfxMatrix savedCTM(thebesContext->CurrentMatrix());
    PRBool doSnap = !(savedCTM.HasNonTranslation());
    PRBool hasPadding = ((xPadding != 0) || (yPadding != 0));

    // If we need to snap, we need to round the CTM as well;
    // otherwise, we may have non-integer pixels in the translation,
    // which will affect the rendering of images (since the current CTM
    // is what's used at the time of a SetPattern call).
    if (doSnap) {
        gfxMatrix roundedCTM(savedCTM);
        roundedCTM.x0 = ::floor(roundedCTM.x0 + 0.5);
        roundedCTM.y0 = ::floor(roundedCTM.y0 + 0.5);
        thebesContext->SetMatrix(roundedCTM);
    }

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

            surface = new gfxImageSurface(gfxASurface::ImageFormatARGB32,
                                          width, height);
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

        if (offset.x > width || offset.y > height) {
            p0.x = - floor(fmod(offset.x, gfxFloat(width)) + 0.5);
            p0.y = - floor(fmod(offset.y, gfxFloat(height)) + 0.5);
        } else {
            p0.x = - floor(offset.x + 0.5);
            p0.y = - floor(offset.y + 0.5);
        }

        patMat.Translate(p0);

        pat = new gfxPattern(surface);
        pat->SetExtend(gfxPattern::EXTEND_REPEAT);
        pat->SetMatrix(patMat);

        thebesContext->SetPattern(pat);
    }

    thebesContext->NewPath();
    thebesContext->Rectangle(targetRect, doSnap);
    thebesContext->Fill();

    thebesContext->SetColor(gfxRGBA(0,0,0,0));
    if (doSnap)
        thebesContext->SetMatrix(savedCTM);

    return NS_OK;
}

/* This function is going away; it's been replaced by ThebesDrawTile above. */
NS_IMETHODIMP
nsThebesImage::DrawTile(nsIRenderingContext &aContext,
                        nsIDrawingSurface *aSurface,
                        PRInt32 aSXOffset, PRInt32 aSYOffset,
                        PRInt32 aPadX, PRInt32 aPadY,
                        const nsRect &aTileRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* This is only used by the GIF decoder, via gfxImageFrame::DrawTo */
NS_IMETHODIMP
nsThebesImage::DrawToImage(nsIImage* aDstImage, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
    nsThebesImage *dstThebesImage = NS_STATIC_CAST(nsThebesImage*, aDstImage);

    nsRefPtr<gfxContext> dst = new gfxContext(dstThebesImage->ThebesSurface());

    dst->NewPath();
    // We don't use PixelSnappedRectangleAndSetPattern because if
    // these coords aren't already pixel aligned, we've lost
    // before we've even begun.
    dst->Translate(gfxPoint(aDX, aDY));
    dst->Rectangle(gfxRect(0, 0, aDWidth, aDHeight), PR_TRUE);
    dst->Scale(double(aDWidth)/mWidth, double(aDHeight)/mHeight);

    dst->SetSource(ThebesSurface());
    dst->Paint();

    return NS_OK;
}
