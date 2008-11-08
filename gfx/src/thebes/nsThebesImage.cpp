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

#ifdef XP_WIN
static PRUint32 gTotalDDBs = 0;
static PRUint32 gTotalDDBSize = 0;
// only use up a maximum of 64MB in DDBs
#define kMaxDDBSize (64*1024*1024)
// and don't let anything in that's bigger than 4MB
#define kMaxSingleDDBSize (4*1024*1024)
#endif

NS_IMPL_ISUPPORTS1(nsThebesImage, nsIImage)

nsThebesImage::nsThebesImage()
    : mFormat(gfxImageSurface::ImageFormatRGB24),
      mWidth(0),
      mHeight(0),
      mDecoded(0,0,0,0),
      mImageComplete(PR_FALSE),
      mSinglePixel(PR_FALSE),
      mFormatChanged(PR_FALSE),
      mAlphaDepth(0)
{
    static PRBool hasCheckedOptimize = PR_FALSE;
    if (!hasCheckedOptimize) {
        if (PR_GetEnv("MOZ_DISABLE_IMAGE_OPTIMIZE")) {
            gDisableOptimize = PR_TRUE;
        }
        hasCheckedOptimize = PR_TRUE;
    }

#ifdef XP_WIN
    mIsDDBSurface = PR_FALSE;
#endif
}

nsresult
nsThebesImage::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
    mWidth = aWidth;
    mHeight = aHeight;

    // Reject over-wide or over-tall images.
    if (!AllowedImageSize(aWidth, aHeight))
        return NS_ERROR_FAILURE;

    // Check to see if we are running OOM
    nsCOMPtr<nsIMemory> mem;
    NS_GetMemoryManager(getter_AddRefs(mem));
    if (!mem)
        return NS_ERROR_UNEXPECTED;

    PRBool lowMemory;
    mem->IsLowMemory(&lowMemory);
    if (lowMemory)
        return NS_ERROR_OUT_OF_MEMORY;

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

    if (!mImageSurface)
        mWinSurface = nsnull;
#endif

    if (!mImageSurface)
        mImageSurface = new gfxImageSurface(gfxIntSize(mWidth, mHeight), format);

    if (!mImageSurface || mImageSurface->CairoStatus()) {
        mImageSurface = nsnull;
        // guess
        return NS_ERROR_OUT_OF_MEMORY;
    }

#ifdef XP_MACOSX
    mQuartzSurface = new gfxQuartzImageSurface(mImageSurface);
#endif

    mStride = mImageSurface->Stride();

    return NS_OK;
}

nsThebesImage::~nsThebesImage()
{
#ifdef XP_WIN
    if (mIsDDBSurface) {
        gTotalDDBs--;
        gTotalDDBSize -= mWidth*mHeight*4;
    }
#endif
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

nsresult
nsThebesImage::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
    // Check to see if we are running OOM
    nsCOMPtr<nsIMemory> mem;
    NS_GetMemoryManager(getter_AddRefs(mem));
    if (!mem)
        return NS_ERROR_UNEXPECTED;

    PRBool lowMemory;
    mem->IsLowMemory(&lowMemory);
    if (lowMemory)
        return NS_ERROR_OUT_OF_MEMORY;

    mDecoded.UnionRect(mDecoded, *aUpdateRect);
#ifdef XP_MACOSX
    if (mQuartzSurface)
        mQuartzSurface->Flush();
#endif
    return NS_OK;
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

    /* Figure out if the entire image is a constant color */

    // this should always be true
    if (mStride == mWidth * 4) {
        PRUint32 *imgData = (PRUint32*) mImageSurface->Data();
        PRUint32 firstPixel = * (PRUint32*) imgData;
        PRUint32 pixelCount = mWidth * mHeight + 1;

        while (--pixelCount && *imgData++ == firstPixel)
            ;

        if (pixelCount == 0) {
            // all pixels were the same
            if (mFormat == gfxImageSurface::ImageFormatARGB32 ||
                mFormat == gfxImageSurface::ImageFormatRGB24)
            {
                mSinglePixelColor = gfxRGBA
                    (firstPixel,
                     (mFormat == gfxImageSurface::ImageFormatRGB24 ?
                      gfxRGBA::PACKED_XRGB :
                      gfxRGBA::PACKED_ARGB_PREMULTIPLIED));

                mSinglePixel = PR_TRUE;

                // blow away the older surfaces, to release data

                mImageSurface = nsnull;
                mOptSurface = nsnull;
#ifdef XP_WIN
                mWinSurface = nsnull;
#endif
#ifdef XP_MACOSX
                mQuartzSurface = nsnull;
#endif
                return NS_OK;
            }
        }

        // if it's not RGB24/ARGB32, don't optimize, but we never hit this at the moment
    }

    // if we're being forced to use image surfaces due to
    // resource constraints, don't try to optimize beyond same-pixel.
    if (ShouldUseImageSurfaces())
        return NS_OK;

    mOptSurface = nsnull;

#ifdef XP_WIN
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
        PRUint32 ddbSize = mWidth * mHeight * 4;
        if (ddbSize <= kMaxSingleDDBSize &&
            ddbSize + gTotalDDBSize <= kMaxDDBSize)
        {
            nsRefPtr<gfxWindowsSurface> wsurf = mWinSurface->OptimizeToDDB(nsnull, gfxIntSize(mWidth, mHeight), mFormat);
            if (wsurf) {
                gTotalDDBs++;
                gTotalDDBSize += ddbSize;
                mIsDDBSurface = PR_TRUE;
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

    if (mOptSurface == nsnull)
        mOptSurface = gfxPlatform::GetPlatform()->OptimizeImage(mImageSurface, mFormat);

    if (mOptSurface) {
        mImageSurface = nsnull;
#ifdef XP_WIN
        mWinSurface = nsnull;
#endif
#ifdef XP_MACOSX
        mQuartzSurface = nsnull;
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
    if (aMaskPixels)
        return NS_ERROR_NOT_IMPLEMENTED;
    if ((mOptSurface || mSinglePixel) && !mImageSurface) {
        // Recover the pixels
        mImageSurface = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                            gfxImageSurface::ImageFormatARGB32);
        if (!mImageSurface || mImageSurface->CairoStatus())
            return NS_ERROR_OUT_OF_MEMORY;
        gfxContext context(mImageSurface);
        context.SetOperator(gfxContext::OPERATOR_SOURCE);
        if (mSinglePixel)
            context.SetDeviceColor(mSinglePixelColor);
        else
            context.SetSource(mOptSurface);
        context.Paint();

#ifdef XP_WIN
        mWinSurface = nsnull;
#endif
#ifdef XP_MACOSX
        mQuartzSurface = nsnull;
#endif
    }

    return NS_OK;
}

NS_IMETHODIMP
nsThebesImage::UnlockImagePixels(PRBool aMaskPixels)
{
    if (aMaskPixels)
        return NS_ERROR_NOT_IMPLEMENTED;
    mOptSurface = nsnull;
#ifdef XP_MACOSX
    if (mQuartzSurface)
        mQuartzSurface->Flush();
#endif
    return NS_OK;
}

static PRBool
IsSafeImageTransformComponent(gfxFloat aValue)
{
    return aValue >= -32768 && aValue <= 32767;
}

void
nsThebesImage::Draw(gfxContext*        aContext,
                    const gfxMatrix&   aUserSpaceToImageSpace,
                    const gfxRect&     aFill,
                    const nsIntMargin& aPadding,
                    const nsIntRect&   aSubimage)
{
    NS_ASSERTION(!aFill.IsEmpty(), "zero dest size --- fix caller");
    NS_ASSERTION(!aSubimage.IsEmpty(), "zero source size --- fix caller");

    PRBool doPadding = aPadding != nsIntMargin(0,0,0,0);
    PRBool doPartialDecode = !GetIsImageComplete();
    gfxContext::GraphicsOperator op = aContext->CurrentOperator();

    if (mSinglePixel && !doPadding && !doPartialDecode) {
        // Single-color fast path
        // if a == 0, it's a noop
        if (mSinglePixelColor.a == 0.0)
            return;

        if (op == gfxContext::OPERATOR_OVER && mSinglePixelColor.a == 1.0)
            aContext->SetOperator(gfxContext::OPERATOR_SOURCE);

        aContext->SetDeviceColor(mSinglePixelColor);
        aContext->NewPath();
        aContext->Rectangle(aFill);
        aContext->Fill();
        aContext->SetOperator(op);
        aContext->SetDeviceColor(gfxRGBA(0,0,0,0));
        return;
    }

    gfxMatrix userSpaceToImageSpace = aUserSpaceToImageSpace;
    gfxRect sourceRect = userSpaceToImageSpace.Transform(aFill);
    gfxRect imageRect(0, 0, mWidth + aPadding.LeftRight(), mHeight + aPadding.TopBottom());
    gfxRect subimage(aSubimage.x, aSubimage.y, aSubimage.width, aSubimage.height);
    gfxRect fill = aFill;
    nsRefPtr<gfxASurface> surface;
    gfxImageSurface::gfxImageFormat format;

    PRBool doTile = !imageRect.Contains(sourceRect);
    if (doPadding || doPartialDecode) {
        gfxRect available = gfxRect(mDecoded.x, mDecoded.y, mDecoded.width, mDecoded.height) +
            gfxPoint(aPadding.left, aPadding.top);
  
        if (!doTile && !mSinglePixel) {
            // Not tiling, and we have a surface, so we can account for
            // padding and/or a partial decode just by twiddling parameters.
            // First, update our user-space fill rect.
            sourceRect = sourceRect.Intersect(available);
            gfxMatrix imageSpaceToUserSpace = userSpaceToImageSpace;
            imageSpaceToUserSpace.Invert();
            fill = imageSpaceToUserSpace.Transform(sourceRect);
  
            surface = ThebesSurface();
            format = mFormat;
            subimage = subimage.Intersect(available) - gfxPoint(aPadding.left, aPadding.top);
            userSpaceToImageSpace.Multiply(
                gfxMatrix().Translate(-gfxPoint(aPadding.left, aPadding.top)));
            sourceRect = sourceRect - gfxPoint(aPadding.left, aPadding.top);
            imageRect = gfxRect(0, 0, mWidth, mHeight);
        } else {
            // Create a temporary surface
            gfxIntSize size(PRInt32(imageRect.Width()),
                            PRInt32(imageRect.Height()));
            // Give this surface an alpha channel because there are
            // transparent pixels in the padding or undecoded area
            format = gfxASurface::ImageFormatARGB32;
            surface = gfxPlatform::GetPlatform()->CreateOffscreenSurface(size,
                format);
            if (!surface || surface->CairoStatus() != 0)
                return;
  
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
        }
    } else {
        NS_ASSERTION(!mSinglePixel, "This should already have been handled");
        surface = ThebesSurface();
        format = mFormat;
    }
    // At this point, we've taken care of mSinglePixel images, images with
    // aPadding, and partially-decoded images.

    if (!AllowedImageSize(fill.size.width + 1, fill.size.height + 1)) {
        NS_WARNING("Destination area too large, bailing out");
        return;
    }

    // BEGIN working around cairo/pixman bug (bug 364968)
    // Compute device-space-to-image-space transform. We need to sanity-
    // check it to work around a pixman bug :-(
    // XXX should we only do this for certain surface types?
    gfxFloat deviceX, deviceY;
    nsRefPtr<gfxASurface> currentTarget =
        aContext->CurrentSurface(&deviceX, &deviceY);
    gfxMatrix currentMatrix = aContext->CurrentMatrix();
    gfxMatrix deviceToUser = currentMatrix;
    deviceToUser.Invert();
    deviceToUser.Translate(-gfxPoint(-deviceX, -deviceY));
    gfxMatrix deviceToImage = deviceToUser;
    deviceToImage.Multiply(userSpaceToImageSpace);
  
    // Our device-space-to-image-space transform may not be acceptable to pixman.
    if (!IsSafeImageTransformComponent(deviceToImage.xx) ||
        !IsSafeImageTransformComponent(deviceToImage.xy) ||
        !IsSafeImageTransformComponent(deviceToImage.yx) ||
        !IsSafeImageTransformComponent(deviceToImage.yy)) {
        NS_WARNING("Scaling up too much, bailing out");
        return;
    }

    PRBool pushedGroup = PR_FALSE;
    if (!IsSafeImageTransformComponent(deviceToImage.x0) ||
        !IsSafeImageTransformComponent(deviceToImage.y0)) {
        // We'll push a group, which will hopefully reduce our transform's
        // translation so it's in bounds
        aContext->Save();
  
        // Clip the rounded-out-to-device-pixels bounds of the
        // transformed fill area. This is the area for the group we
        // want to push.
        aContext->IdentityMatrix();
        gfxRect bounds = currentMatrix.TransformBounds(fill);
        bounds.RoundOut();
        aContext->Clip(bounds);
        aContext->SetMatrix(currentMatrix);
  
        aContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
        aContext->SetOperator(gfxContext::OPERATOR_OVER);
        pushedGroup = PR_TRUE;
    }
    // END working around cairo/pixman bug (bug 364968)
  
    nsRefPtr<gfxPattern> pattern = new gfxPattern(surface);
    pattern->SetMatrix(userSpaceToImageSpace);

    // OK now, the hard part left is to account for the subimage sampling
    // restriction. If all the transforms involved are just integer
    // translations, then we assume no resampling will occur so there's
    // nothing to do.
    // XXX if only we had source-clipping in cairo!
    if (!currentMatrix.HasNonIntegerTranslation() &&
        !userSpaceToImageSpace.HasNonIntegerTranslation()) {
        if (doTile) {
            pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
        }
    } else {
        if (doTile || !subimage.Contains(imageRect)) {
            // EXTEND_PAD won't help us here; we have to create a temporary
            // surface to hold the subimage of pixels we're allowed to
            // sample
            gfxRect needed = subimage.Intersect(sourceRect);
            needed.RoundOut();
            gfxIntSize size(PRInt32(needed.Width()), PRInt32(needed.Height()));
            nsRefPtr<gfxASurface> temp =
                gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, format);
            if (temp && temp->CairoStatus() == 0) {
                gfxContext tmpCtx(temp);
                tmpCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
                nsRefPtr<gfxPattern> tmpPattern = new gfxPattern(surface);
                if (tmpPattern) {
                    tmpPattern->SetExtend(gfxPattern::EXTEND_REPEAT);
                    tmpPattern->SetMatrix(gfxMatrix().Translate(needed.pos));
                    tmpCtx.SetPattern(tmpPattern);
                    tmpCtx.Paint();
                    tmpPattern = new gfxPattern(temp);
                    if (tmpPattern) {
                        pattern.swap(tmpPattern);
                        pattern->SetMatrix(
                            gfxMatrix(userSpaceToImageSpace).Multiply(gfxMatrix().Translate(-needed.pos)));
                    }
                }
            }
        }
  
        // In theory we can handle this using cairo's EXTEND_PAD,
        // but implementation limitations mean we have to consult
        // the surface type.
        switch (currentTarget->GetType()) {
        case gfxASurface::SurfaceTypeXlib:
        case gfxASurface::SurfaceTypeXcb:
            // See bug 324698.  This is a workaround for EXTEND_PAD not being
            // implemented correctly on linux in the X server.
            //
            // Set the filter to CAIRO_FILTER_FAST --- otherwise,
            // pixman's sampling will sample transparency for the outside edges and we'll
            // get blurry edges.  CAIRO_EXTEND_PAD would also work here, if
            // available
            //
            // This effectively disables smooth upscaling for images.
            pattern->SetFilter(0);
            break;
  
        case gfxASurface::SurfaceTypeQuartz:
        case gfxASurface::SurfaceTypeQuartzImage:
            // Do nothing, Mac seems to be OK. Really?
            break;

        default:
            // turn on EXTEND_PAD.
            // This is what we really want for all surface types, if the
            // implementation was universally good.
            pattern->SetExtend(gfxPattern::EXTEND_PAD);
            break;
        }
    }

    if ((op == gfxContext::OPERATOR_OVER || pushedGroup) &&
        format == gfxASurface::ImageFormatRGB24) {
        aContext->SetOperator(gfxContext::OPERATOR_SOURCE);
    }

    // Phew! Now we can actually draw this image
    aContext->NewPath();
    aContext->SetPattern(pattern);
    aContext->Rectangle(fill);
    aContext->Fill();
  
    aContext->SetOperator(op);
    if (pushedGroup) {
        aContext->PopGroupToSource();
        aContext->Paint();
        aContext->Restore();
    }
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
    if (mFormat == gfxASurface::ImageFormatARGB32) {
        mFormat = gfxASurface::ImageFormatRGB24;
        mFormatChanged = PR_TRUE;
    }
}
