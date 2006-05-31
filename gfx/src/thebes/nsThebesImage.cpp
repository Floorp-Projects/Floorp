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
    : mWidth(0),
      mHeight(0),
      mDecoded(0,0,0,0),
      mImageComplete(PR_FALSE),
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

    mImageSurface = new gfxImageSurface(format, mWidth, mHeight);
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
    return mStride;
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

    if (mOptSurface)
        return NS_OK;

    mOptSurface = gfxPlatform::GetPlatform()->OptimizeImage(mImageSurface);

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
    if (mOptSurface && !mImageSurface) {
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

    gfxFloat xscale = gfxFloat(aDWidth) / aSWidth;
    gfxFloat yscale = gfxFloat(aDHeight) / aSHeight;

    if (!GetIsImageComplete()) {
      nsRect srcRect(aSX, aSY, aSWidth, aSHeight);
      srcRect.IntersectRect(srcRect, mDecoded);

      aDX += (PRInt32)((srcRect.x - aSX)*xscale);
      aDY += (PRInt32)((srcRect.y - aSY)*yscale);

      // use '+ 1 - *scale' to get rid of rounding errors
      aDWidth  = (PRInt32)((srcRect.width)*xscale + 1 - xscale);
      aDHeight = (PRInt32)((srcRect.height)*yscale + 1 - yscale);

      aSX = srcRect.x;
      aSY = srcRect.y;
    }

    gfxRect dr(aDX, aDY, aDWidth, aDHeight);

    gfxMatrix mat;
    mat.Translate(gfxPoint(aSX, aSY));
    mat.Scale(1.0/xscale, 1.0/yscale);

    nsRefPtr<gfxPattern> pat = new gfxPattern(ThebesSurface());
    pat->SetMatrix(mat);

    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(dr, pat);
    ctx->Fill();

    return NS_OK;
}

/* DrawTile is always relative to the device; never relative to the current
 * transformation matrix on the device.  This is where the IdentityMatrix() bits
 * come from.
 */
/* NB: Still pixels! */
NS_IMETHODIMP
nsThebesImage::DrawTile(nsIRenderingContext &aContext,
                        nsIDrawingSurface *aSurface,
                        PRInt32 aSXOffset, PRInt32 aSYOffset,
                        PRInt32 aPadX, PRInt32 aPadY,
                        const nsRect &aTileRect)
{
    nsThebesRenderingContext *thebesRC = NS_STATIC_CAST(nsThebesRenderingContext*, &aContext);
    gfxContext *ctx = thebesRC->Thebes();

    if (aTileRect.width <= 0 || aTileRect.height <= 0)
        return NS_OK;

    if (aPadX || aPadY)
        fprintf (stderr, "Warning: nsThebesImage::DrawTile given padX(%d)/padY(%d), ignoring\n", aPadX, aPadY);

#if 0
    fprintf (stderr, "****** nsThebesImage::DrawTile: (%d,%d [%d, %d]) -> (%d,%d,%d,%d)\n",
             aSXOffset, aSYOffset, mWidth, mHeight,
             aTileRect.x, aTileRect.y,
             aTileRect.width, aTileRect.height);
#endif

    gfxMatrix savedMatrix = ctx->CurrentMatrix();
    PRBool reallyRepeating = PR_FALSE;

    PRInt32 x0 = aTileRect.x - aSXOffset;
    PRInt32 y0 = aTileRect.y - aSYOffset;

    // Let's figure out if this really needs to repeat,
    // or if we're just drawing a subrect
    if (aSXOffset + aTileRect.width > mWidth ||
        aSYOffset + aTileRect.height > mHeight)
    {
        reallyRepeating = PR_TRUE;
    } else {
        // nope, just drawing a subrect, so let's not set CAIRO_EXTEND_REPEAT
        // so that we don't get screwed by image surface fallbacks due to
        // buggy RENDER implementations
        if (aSXOffset > mWidth)
            aSXOffset = aSXOffset % mWidth;
        if (aSYOffset > mHeight)
            aSYOffset = aSYOffset % mHeight;
    }

    ctx->IdentityMatrix();

    ctx->Translate(gfxPoint(x0, y0));

    nsRefPtr<gfxPattern> pat = new gfxPattern(ThebesSurface());
    if (reallyRepeating)
        pat->SetExtend(CAIRO_EXTEND_REPEAT);

    ctx->NewPath();
    ctx->Rectangle(gfxRect(aSXOffset, aSYOffset,
                           aTileRect.width, aTileRect.height),
                   PR_TRUE);
    ctx->SetPattern(pat);
#if 0
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(aSXOffset, aSYOffset,
                                                    aTileRect.width, aTileRect.height),
                                            pat);
#endif
    ctx->Fill();

    ctx->SetMatrix(savedMatrix);

    return NS_OK;
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

