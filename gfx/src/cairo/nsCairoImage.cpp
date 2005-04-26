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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Joe Hewitt <hewitt@netscape.com>
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
#include "nsCairoImage.h"
#include "nsCairoRenderingContext.h"
#include "nsCairoDrawingSurface.h"

#include "nsIServiceManager.h"

static void ARGBToThreeChannel(PRUint32* aARGB, PRUint8* aData) {
    PRUint8 r = (PRUint8)(*aARGB >> 16);
    PRUint8 g = (PRUint8)(*aARGB >> 8);
    PRUint8 b = (PRUint8)*aARGB;
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
    return (aAlpha << 24) | (r << 16) | (g << 8) | b;
}

nsCairoImage::nsCairoImage()
 : mWidth(0),
   mHeight(0),
   mDecoded(0,0,0,0),
   mImageSurface(nsnull),
   mImageSurfaceBuf(nsnull),
   mImageSurfaceData(nsnull),
   mImageSurfaceAlpha(nsnull),
   mAlphaDepth(0),
   mHadAnyData(PR_FALSE)
{
}

nsCairoImage::~nsCairoImage()
{
    if (mImageSurface)
        cairo_surface_destroy(mImageSurface);
    // XXXX - totally not safe. mImageSurface should own data,
    // but it doesn't.
    if (mImageSurfaceBuf)
        nsMemory::Free(mImageSurfaceBuf);
    if (mImageSurfaceData)
        nsMemory::Free(mImageSurfaceData);
    if (mImageSurfaceAlpha)
        nsMemory::Free(mImageSurfaceAlpha);
}

NS_IMPL_ISUPPORTS1(nsCairoImage, nsIImage)

//////////////////////////////////////////////
//// nsIImage
nsresult
nsCairoImage::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
    mWidth = aWidth;
    mHeight = aHeight;

    switch(aMaskRequirements)
    {
    case nsMaskRequirements_kNeeds1Bit:
        mAlphaDepth = 1;
        mCairoFormat = CAIRO_FORMAT_ARGB32;
        break;
    case nsMaskRequirements_kNeeds8Bit:
        mAlphaDepth = 8;
        mCairoFormat = CAIRO_FORMAT_ARGB32;
        break;
    default:
        mAlphaDepth = 0;
        // this should be able to be RGB24, but that doesn't seem to work
        mCairoFormat = CAIRO_FORMAT_ARGB32;
        break;
    }

    PRInt32 size = aWidth*aHeight*4;
    mImageSurfaceBuf = (PRUint32*)nsMemory::Alloc(size);
    // Fill it in. Setting alpha to FF is important for images with no
    // alpha of their own
    memset(mImageSurfaceBuf, 0xFF, size);

    mImageSurface = cairo_image_surface_create_for_data
        ((char*)mImageSurfaceBuf, mCairoFormat, mWidth, mHeight, aWidth * 4);
    return NS_OK;
}

PRInt32
nsCairoImage::GetBytesPix()
{
    // not including alpha, I guess
    return 3;
}

PRBool
nsCairoImage::GetIsRowOrderTopToBottom()
{
    return PR_FALSE;
}

PRInt32
nsCairoImage::GetWidth()
{
    return mWidth;
}

PRInt32
nsCairoImage::GetHeight()
{
    return mHeight;
}

PRUint8 *
nsCairoImage::GetBits()
{
    return (PRUint8 *) mImageSurfaceData;
}

PRInt32
nsCairoImage::GetLineStride()
{
    return mWidth*3;
}

PRBool
nsCairoImage::GetHasAlphaMask()
{
    return mAlphaDepth > 0;
}

PRUint8 *
nsCairoImage::GetAlphaBits()
{
    return (PRUint8 *) mImageSurfaceAlpha;
}

PRInt32
nsCairoImage::GetAlphaLineStride()
{
    return mAlphaDepth == 1 ? (mWidth+7)/8 : mWidth;
}

void
nsCairoImage::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
    mDecoded.UnionRect(mDecoded, *aUpdateRect);
}

PRBool
nsCairoImage::GetIsImageComplete()
{
    return mDecoded == nsRect(0, 0, mWidth, mHeight);
}

nsresult
nsCairoImage::Optimize(nsIDeviceContext* aContext)
{
    return NS_OK;
}

nsColorMap *
nsCairoImage::GetColorMap()
{
    return NULL;
}

/* WHY ARE THESE HERE?!?!?!?!?!? */
/* INSTEAD OF RENDERINGCONTEXT, WHICH ONLY TAKES IMGICONTAINER?!?!?!? */
NS_IMETHODIMP
nsCairoImage::Draw(nsIRenderingContext &aContext, nsIDrawingSurface *aSurface,
                   PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
    return Draw(aContext, aSurface, 0, 0, mWidth, mHeight, aX, aY, aWidth, aHeight);
}

void
nsCairoImage::UpdateFromImageData()
{
    if (!mImageSurfaceData)
        return;
    
    NS_ASSERTION(mAlphaDepth == 0 || mAlphaDepth == 1 || mAlphaDepth == 8,
                 "Bad alpha depth");

    PRInt32 alphaIndex = 0;
    PRInt32 bufIndex = 0;
    for (PRInt32 row = 0; row < mHeight; ++row) {
        for (PRInt32 col = 0; col < mWidth; ++col) {
            PRUint8 alpha = 0xFF;
            if (mAlphaDepth == 1) {
                PRUint8 mask = 1 << (7 - (col&7));
                if (!(mImageSurfaceAlpha[alphaIndex] & mask)) {
                    alpha = 0;
                }
                if (mask == 0x01) {
                    ++alphaIndex;
                }
            } else if (mAlphaDepth == 8) {
                alpha = mImageSurfaceAlpha[bufIndex];
            }
            mImageSurfaceBuf[bufIndex] = ThreeChannelToARGB(&mImageSurfaceData[bufIndex*3], alpha);
            ++bufIndex;
        }
        if (mAlphaDepth == 1) {
            if (mWidth & 7) {
                ++alphaIndex;
            }
        }
    }

    if (PR_FALSE) { // Enabling this saves memory but can lead to pathological
        // behaviour
        nsMemory::Free(mImageSurfaceData);
        mImageSurfaceData = nsnull;
        if (mImageSurfaceAlpha) {
            nsMemory::Free(mImageSurfaceAlpha);
            mImageSurfaceAlpha = nsnull;
        }

        mHadAnyData = PR_TRUE;
    }
}

NS_IMETHODIMP
nsCairoImage::Draw(nsIRenderingContext &aContext, nsIDrawingSurface *aSurface,
                   PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                   PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
#if 1
    fprintf (stderr, "****** nsCairoImage::Draw: (%d,%d,%d,%d) -> (%d,%d,%d,%d)\n",
             aSX, aSY, aSWidth, aSHeight,
             aDX, aDY, aDWidth, aDHeight);
#endif
    UpdateFromImageData();

    nsCairoDrawingSurface *dstSurf = NS_STATIC_CAST(nsCairoDrawingSurface*, aSurface);
    nsCairoRenderingContext *cairoContext = NS_STATIC_CAST(nsCairoRenderingContext*, &aContext);

    cairo_t *dstCairo = cairoContext->GetCairo();

    cairo_translate(dstCairo, aDX, aDY);

    cairo_pattern_t *pat = cairo_pattern_create_for_surface (mImageSurface);
    cairo_matrix_t* matrix = cairo_matrix_create();
    if (matrix) {
        cairo_matrix_set_affine(matrix, double(aSWidth)/aDWidth, 0,
                                0, double(aSHeight)/aDHeight, aSX, aSY);
        cairo_pattern_set_matrix(pat, matrix);
        cairo_matrix_destroy(matrix);
    }
    cairo_set_pattern (dstCairo, pat);
    cairo_pattern_destroy (pat);

    cairo_new_path(dstCairo);
    cairo_rectangle (dstCairo, 0, 0, aDWidth, aDHeight);
    cairo_fill (dstCairo);
    cairo_translate(dstCairo, -aDX, -aDY);
    
    return NS_OK;
}

NS_IMETHODIMP
nsCairoImage::DrawTile(nsIRenderingContext &aContext,
                       nsIDrawingSurface *aSurface,
                       PRInt32 aSXOffset, PRInt32 aSYOffset,
                       PRInt32 aPadX, PRInt32 aPadY,
                       const nsRect &aTileRect)
{
    UpdateFromImageData();

    if (aPadX || aPadY)
        fprintf (stderr, "Warning: nsCairoImage::DrawTile given padX(%d)/padY(%d), ignoring\n", aPadX, aPadY);

    nsCairoDrawingSurface *dstSurf = NS_STATIC_CAST(nsCairoDrawingSurface*, aSurface);
    nsCairoRenderingContext *cairoContext = NS_STATIC_CAST(nsCairoRenderingContext*, &aContext);

    cairo_t *dstCairo = cairoContext->GetCairo();

    cairo_translate(dstCairo, aTileRect.x, aTileRect.y);

    cairo_pattern_t *pat = cairo_pattern_create_for_surface (mImageSurface);
    cairo_matrix_t* matrix = cairo_matrix_create();
    if (matrix) {
        nsCOMPtr<nsIDeviceContext> dc;
        aContext.GetDeviceContext(*getter_AddRefs(dc));
        float app2dev = dc->AppUnitsToDevUnits();
        cairo_matrix_set_affine(matrix, app2dev, 0, 0, app2dev, aSXOffset, aSYOffset);
        cairo_pattern_set_matrix(pat, matrix);
        cairo_matrix_destroy(matrix);
    }
    cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);
    cairo_set_pattern (dstCairo, pat);
    cairo_pattern_destroy (pat);

    cairo_new_path(dstCairo);
    cairo_rectangle (dstCairo, 0, 0, aTileRect.width, aTileRect.height);
    cairo_fill (dstCairo);
    cairo_translate(dstCairo, -aTileRect.x, -aTileRect.y);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoImage::DrawToImage(nsIImage* aDstImage, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
    UpdateFromImageData();

    nsCairoImage *dstCairoImage = NS_STATIC_CAST(nsCairoImage*, aDstImage);

    cairo_t *dstCairo = cairo_create ();

    cairo_set_target_surface(dstCairo, dstCairoImage->mImageSurface);

    // clip to target
    cairo_rectangle(dstCairo, double(aDX), double(aDY), double(aDWidth), double(aDHeight));
    cairo_clip(dstCairo);

    // scale up to the size difference
    cairo_scale(dstCairo, double(aDWidth)/double(mWidth), double(aDHeight)/double(mHeight));

    // move to where we need to start the image rectangle, so that
    // it gets clipped to the right place
    cairo_translate(dstCairo, double(aDX), double(aDY));

    // show it
    cairo_show_surface (dstCairo, mImageSurface, mWidth, mHeight);

    cairo_destroy (dstCairo);

    return NS_OK;
}

PRInt8
nsCairoImage::GetAlphaDepth()
{
    return mAlphaDepth;
}

void *
nsCairoImage::GetBitInfo()
{
    return NULL;
}

NS_IMETHODIMP
nsCairoImage::LockImagePixels(PRBool aMaskPixels)
{
    if (mImageSurfaceData) {
        return NS_OK;
    }

    if (mAlphaDepth > 0) {
        NS_ASSERTION(mAlphaDepth == 1 || mAlphaDepth == 8,
                     "Bad alpha depth");
        PRUint32 size = mHeight*(mAlphaDepth == 1 ? ((mWidth+7)/8) : mWidth);
        mImageSurfaceAlpha = (PRUint8*)nsMemory::Alloc(size);
        if (!mImageSurfaceAlpha)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    PRUint32 size = mWidth * mHeight * 3;
    mImageSurfaceData = (PRUint8*)nsMemory::Alloc(size);
    if (!mImageSurfaceData)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 count = mWidth*mHeight;
    if (mAlphaDepth > 0 && mHadAnyData) {
        PRInt32 size = mHeight*(mAlphaDepth == 1 ? ((mWidth+7)/8) : mWidth);
        mImageSurfaceAlpha = (PRUint8*)nsMemory::Alloc(size);

        // fill from existing ARGB buffer
        if (mAlphaDepth == 8) {
            for (PRInt32 i = 0; i < count; ++i) {
                mImageSurfaceAlpha[i] = (PRUint8)(mImageSurfaceBuf[i] >> 24);
            }
        } else {
            PRInt32 i = 0;
            for (PRInt32 row = 0; row < mHeight; ++row) {
                PRUint8 alphaBits = 0;
                for (PRInt32 col = 0; col < mWidth; ++col) {
                    PRUint8 mask = 1 << (7 - (col&7));
                    if (mImageSurfaceBuf[row*mWidth + col] & 0xFF000000) {
                        alphaBits |= mask;
                    }
                    if (mask == 0x01) {
                        // This mask byte is complete, write it back
                        mImageSurfaceAlpha[i] = alphaBits;
                        alphaBits = 0;
                        ++i;
                    }
                }
                if (mWidth & 7) {
                    // write back the incomplete alpha mask
                    mImageSurfaceAlpha[i] = alphaBits;
                    ++i;
                }
            }
        }
    }
    
    if (mHadAnyData) {
        // fill from existing ARGB buffer
        for (PRInt32 i = 0; i < count; ++i) {
            ARGBToThreeChannel(&mImageSurfaceBuf[i], &mImageSurfaceData[i*3]);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCairoImage::UnlockImagePixels(PRBool aMaskPixels)
{
    return NS_OK;
}
