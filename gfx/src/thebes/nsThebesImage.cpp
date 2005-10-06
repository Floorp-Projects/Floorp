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

#include "nsMemory.h"
#include "nsColor.h"

#include "nsThebesDeviceContext.h"
#include "nsThebesRenderingContext.h"
#include "nsThebesDrawingSurface.h"
#include "nsThebesImage.h"

#include "gfxContext.h"
#include "gfxPattern.h"

#ifdef MOZ_ENABLE_GTK2
#include <gdk/gdkx.h>
#include "gfxXlibSurface.h"

#ifdef MOZ_ENABLE_GLITZ
#include "glitz-glx.h"
#include "gfxGlitzSurface.h"
#endif
#endif

static PRUint8 Unpremultiply(PRUint8 aVal, PRUint8 aAlpha);
static void ARGBToThreeChannel(PRUint32* aARGB, PRUint8* aData);
static PRUint8 Premultiply(PRUint8 aVal, PRUint8 aAlpha);
static PRUint32 ThreeChannelToARGB(PRUint8* aData, PRUint8 aAlpha);

NS_IMPL_ISUPPORTS1(nsThebesImage, nsIImage)

nsThebesImage::nsThebesImage()
    : mWidth(0),
      mHeight(0),
      mDecoded(0,0,0,0),
      mLockedData(nsnull),
      mLockedAlpha(nsnull),
      mAlphaDepth(0),
      mLocked(PR_FALSE),
      mHadAnyData(PR_FALSE),
      mUpToDate(PR_FALSE)
{
}

nsresult
nsThebesImage::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
    NS_ASSERTION(aDepth == 24, "nsThebesImage::Init called with depth != 24");

    gfxImageSurface::gfxImageFormat format;

    mWidth = aWidth;
    mHeight = aHeight;

    switch(aMaskRequirements)
    {
        case nsMaskRequirements_kNeeds1Bit:
            mAlphaDepth = 1;
            format = gfxImageSurface::ImageFormatARGB32;
            break;
        case nsMaskRequirements_kNeeds8Bit:
            mAlphaDepth = 8;
            format = gfxImageSurface::ImageFormatARGB32;
            break;
        default:
            mAlphaDepth = 0;
            format = gfxImageSurface::ImageFormatARGB32;
            break;
    }

    mImageSurface = new gfxImageSurface (format, mWidth, mHeight);
    memset(mImageSurface->Data(), 0xFF, mHeight * mImageSurface->Stride());

    return NS_OK;
}

nsThebesImage::~nsThebesImage()
{
    if (mLockedData)
        nsMemory::Free(mLockedData);
    if (mLockedAlpha)
        nsMemory::Free(mLockedAlpha);
}

PRInt32
nsThebesImage::GetBytesPix()
{
    // not including alpha
    return 3;
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
    //NS_ASSERTION(mLocked == PR_TRUE, "GetBits called outside of Lock!");
    return mLockedData;
}

PRInt32
nsThebesImage::GetLineStride()
{
    return mWidth * 3;
}

PRBool
nsThebesImage::GetHasAlphaMask()
{
    return mAlphaDepth > 0;
}

PRUint8 *
nsThebesImage::GetAlphaBits()
{
    //NS_ASSERTION(mLocked == PR_TRUE, "GetAlphaBits called outside of Lock!");
    return (PRUint8 *) mLockedAlpha;
}

PRInt32
nsThebesImage::GetAlphaLineStride()
{
    return mAlphaDepth == 1 ? (mWidth+7)/8 : mWidth;
}

void
nsThebesImage::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
    mDecoded.UnionRect(mDecoded, *aUpdateRect);
}

PRBool
nsThebesImage::GetIsImageComplete()
{
    return mDecoded == nsRect(0, 0, mWidth, mHeight);
}

nsresult
nsThebesImage::Optimize(nsIDeviceContext* aContext)
{
    UpdateFromLockedData();

    if (!mOptSurface) {
#ifdef MOZ_ENABLE_GTK2
        if (!nsThebesDrawingSurface::UseGlitz()) {
            XRenderPictFormat *fmt = nsnull;

            if (mRealAlphaDepth == 0) {
                fmt = gfxXlibSurface::FindRenderFormat(GDK_DISPLAY(), gfxASurface::ImageFormatRGB24);
            } else if (mRealAlphaDepth == 1) {
                fmt = gfxXlibSurface::FindRenderFormat(GDK_DISPLAY(), gfxASurface::ImageFormatARGB32);
            } else if (mRealAlphaDepth == 8) {
                fmt = gfxXlibSurface::FindRenderFormat(GDK_DISPLAY(), gfxASurface::ImageFormatARGB32);
            }

            /* XXX handle mRealAlphaDepth = 1, and create separate mask */

            if (fmt) {
                mOptSurface = new gfxXlibSurface(GDK_DISPLAY(),
                                                 fmt,
                                                 mWidth, mHeight);
            }
        } else {
# ifdef MOZ_ENABLE_GLITZ
            // glitz
            nsThebesDeviceContext *tdc = NS_STATIC_CAST(nsThebesDeviceContext*, aContext);
            glitz_drawable_format_t *gdf = (glitz_drawable_format_t*) tdc->GetGlitzDrawableFormat();
            glitz_drawable_t *gdraw = glitz_glx_create_pbuffer_drawable (GDK_DISPLAY(),
                                                                         DefaultScreen(GDK_DISPLAY()),
                                                                         gdf,
                                                                         mWidth, mHeight);
            glitz_format_t *gf =
                glitz_find_standard_format (gdraw, GLITZ_STANDARD_ARGB32);
            glitz_surface_t *gsurf =
                glitz_surface_create (gdraw,
                                      gf,
                                      mWidth,
                                      mHeight,
                                      0,
                                      NULL);

            glitz_surface_attach (gsurf, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);
            mOptSurface = new gfxGlitzSurface (gdraw, gsurf, PR_TRUE);
# endif
        }
#endif
    }

    if (mOptSurface) {
        nsRefPtr<gfxContext> tmpCtx(new gfxContext(mOptSurface));
        tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
        tmpCtx->SetSource(mImageSurface);
#if 0
        PRUint32 k = (PRUint32) this;
        k |= 0xff000000;
        tmpCtx->SetColor(gfxRGBA(nscolor(k)));
#endif
        tmpCtx->Paint();
#if 0
        tmpCtx->NewPath();
        tmpCtx->Rectangle(gfxRect(0, 0, mWidth, mHeight));
        tmpCtx->Fill();
#endif
    }

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
    //NS_ASSERTION(!mLocked, "LockImagePixels called on already locked image!");
    PRUint32 tmpSize;

    // if we already have locked data allocated,
    // and we have some data, and we're not up to date,
    // then don't bother updating frmo the image surface
    // to the 2 buffers -- the 2 buffers are more current,
    // because UpdateFromLockedData() hasn't been called yet.
    //
    // UpdateFromLockedData() is only called right before we
    // actually try to draw, because the image decoders
    // will call Lock/Unlock pretty frequently as they decode
    // rows of the image.

    if (mLockedData && mHadAnyData && !mUpToDate)
        return NS_OK;

    if (mAlphaDepth > 0) {
        NS_ASSERTION(mAlphaDepth == 1 || mAlphaDepth == 8,
                     "Bad alpha depth");
        tmpSize = mHeight*(mAlphaDepth == 1 ? ((mWidth+7)/8) : mWidth);
        if (!mLockedAlpha)
            mLockedAlpha = (PRUint8*)nsMemory::Alloc(tmpSize);
        if (!mLockedAlpha)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    tmpSize = mWidth * mHeight * 3;
    if (!mLockedData)
        mLockedData = (PRUint8*)nsMemory::Alloc(tmpSize);
    if (!mLockedData)
        return NS_ERROR_OUT_OF_MEMORY;

    if (mAlphaDepth > 0 && mHadAnyData) {
        // fill from existing ARGB buffer
        if (mAlphaDepth == 8) {
            PRInt32 destCount = 0;

            for (PRInt32 row = 0; row < mHeight; ++row) {
                PRUint32* srcRow = (PRUint32*) (mImageSurface->Data() + (mImageSurface->Stride()*row));

                for (PRInt32 col = 0; col < mWidth; ++col) {
                    mLockedAlpha[destCount++] = (PRUint8)(srcRow[col] >> 24);
                }
            }
        } else {
            PRInt32 i = 0;
            for (PRInt32 row = 0; row < mHeight; ++row) {
                PRUint32* srcRow = (PRUint32*) (mImageSurface->Data() + (mImageSurface->Stride()*row));
                PRUint8 alphaBits = 0;

                for (PRInt32 col = 0; col < mWidth; ++col) {
                    PRUint8 mask = 1 << (7 - (col&7));

                    if (srcRow[col] & 0xFF000000) {
                        alphaBits |= mask;
                    }
                    if (mask == 0x01) {
                        // This mask byte is complete, write it back
                        mLockedAlpha[i] = alphaBits;
                        alphaBits = 0;
                        ++i;
                    }
                }
                if (mWidth & 7) {
                    // write back the incomplete alpha mask
                    mLockedAlpha[i] = alphaBits;
                    ++i;
                }
            }
        }
    }
    
    if (mHadAnyData) {
        int destCount = 0;

        for (PRInt32 row = 0; row < mHeight; ++row) {
            PRUint32* srcRow = (PRUint32*) (mImageSurface->Data() + (mImageSurface->Stride()*row));

            for (PRInt32 col = 0; col < mWidth; ++col) {
                ARGBToThreeChannel(&srcRow[col], &mLockedData[destCount*3]);
                destCount++;
            }
        }
    }

    mLocked = PR_TRUE;

    mOptSurface = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsThebesImage::UnlockImagePixels(PRBool aMaskPixels)
{
    //NS_ASSERTION(mLocked, "UnlockImagePixels called on unlocked image!");
        
    mLocked = PR_FALSE;
    mUpToDate = PR_FALSE;

    return NS_OK;
}

void
nsThebesImage::UpdateFromLockedData()
{
    if (!mLockedData || mUpToDate)
        return;

    mUpToDate = PR_TRUE;

    NS_ASSERTION(mAlphaDepth == 0 || mAlphaDepth == 1 || mAlphaDepth == 8,
                 "Bad alpha depth");

    mRealAlphaDepth = 0;

    PRInt32 alphaIndex = 0;
    PRInt32 lockedBufIndex = 0;
    for (PRInt32 row = 0; row < mHeight; ++row) {
        PRUint32 *destPtr = (PRUint32*) (mImageSurface->Data() + (row * mImageSurface->Stride()));

        for (PRInt32 col = 0; col < mWidth; ++col) {
            PRUint8 alpha = 0xFF;

            if (mAlphaDepth == 1) {
                PRUint8 mask = 1 << (7 - (col&7));
                if (!(mLockedAlpha[alphaIndex] & mask)) {
                    alpha = 0;
                }
                if (mask == 0x01) {
                    ++alphaIndex;
                }
            } else if (mAlphaDepth == 8) {
                alpha = mLockedAlpha[lockedBufIndex];
            }

            if (mRealAlphaDepth == 0 && alpha != 0xff)
                mRealAlphaDepth = mAlphaDepth;

            *destPtr++ =
                ThreeChannelToARGB(&mLockedData[lockedBufIndex*3], alpha);

            ++lockedBufIndex;
        }
        if (mAlphaDepth == 1) {
            if (mWidth & 7) {
                ++alphaIndex;
            }
        }
    }

    if (PR_FALSE) { // Enabling this saves memory but can lead to pathologically bad
        // performance. Would also cause problems with premultiply/unpremultiply too
        nsMemory::Free(mLockedData);
        mLockedData = nsnull;
        if (mLockedAlpha) {
            nsMemory::Free(mLockedAlpha);
            mLockedAlpha = nsnull;
        }
    }

    mHadAnyData = PR_TRUE;
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
    UpdateFromLockedData();

    nsThebesRenderingContext *thebesRC = NS_STATIC_CAST(nsThebesRenderingContext*, &aContext);
    gfxContext *ctx = thebesRC->Thebes();

#if 0
    fprintf (stderr, "nsThebesImage::Draw src [%d %d %d %d] dest [%d %d %d %d] tx [%f %f]\n",
             aSX, aSY, aSWidth, aSHeight, aDX, aDY, aDWidth, aDHeight,
             ctx->CurrentMatrix().GetTranslation().x, ctx->CurrentMatrix().GetTranslation().y);
#endif

    gfxRect sr(aSX, aSY, aSWidth, aSHeight);
    gfxRect dr(aDX, aDY, aDWidth, aDHeight);

    gfxMatrix mat;
    mat.Translate(gfxPoint(aSX, aSY));
    mat.Scale(double(aSWidth)/aDWidth, double(aSHeight)/aDHeight);

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
    UpdateFromLockedData();

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

    // Let's figure out if this really is repeating, or if we're just drawing a subrect
    if (aTileRect.width > mWidth ||
        aTileRect.height > mHeight)
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
    UpdateFromLockedData();

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
    dst->Fill();

    return NS_OK;
}

/** Image conversion utils **/

static PRUint8 Unpremultiply(PRUint8 aVal, PRUint8 aAlpha) {
    return (aVal*255)/aAlpha;
}

static void ARGBToThreeChannel(PRUint32* aARGB, PRUint8* aData) {
    PRUint8 a = (PRUint8)(*aARGB >> 24);
    PRUint8 r = (PRUint8)(*aARGB >> 16);
    PRUint8 g = (PRUint8)(*aARGB >> 8);
    PRUint8 b = (PRUint8)(*aARGB >> 0);

    if (a != 0xFF) {
        if (a == 0) {
            r = 0;
            g = 0;
            b = 0;
        } else {
            r = Unpremultiply(r, a);
            g = Unpremultiply(g, a);
            b = Unpremultiply(b, a);
        }
    }

#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
    // BGR format; assume little-endian system
#ifndef IS_LITTLE_ENDIAN
#error Strange big-endian/OS combination
#endif
    // BGR, blue byte first
    aData[0] = b; aData[1] = g; aData[2] = r;
#else
    // RGB, red byte first
    aData[0] = r; aData[1] = g; aData[2] = b;
#endif
}

static PRUint8 Premultiply(PRUint8 aVal, PRUint8 aAlpha) {
    PRUint32 r;
    FAST_DIVIDE_BY_255(r, aVal*aAlpha);
    return (PRUint8)r;
}

static PRUint32 ThreeChannelToARGB(PRUint8* aData, PRUint8 aAlpha) {
    PRUint8 r, g, b;
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
    // BGR format; assume little-endian system
#ifndef IS_LITTLE_ENDIAN
#error Strange big-endian/OS combination
#endif
    // BGR, blue byte first
    b = aData[0]; g = aData[1]; r = aData[2];
#else
    // RGB, red byte first
    r = aData[0]; g = aData[1]; b = aData[2];
#endif
    if (aAlpha != 0xFF) {
        if (aAlpha == 0) {
            r = 0;
            g = 0;
            b = 0;
        } else {
            r = Premultiply(r, aAlpha);
            g = Premultiply(g, aAlpha);
            b = Premultiply(b, aAlpha);
        }
    }
    return (aAlpha << 24) | (r << 16) | (g << 8) | b;
}
