/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is gfx thebes code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Eric Butler <zantifon@gmail.com>
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

#include "gfxBlur.h"

#include "nsMathUtils.h"
#include "nsTArray.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

gfxAlphaBoxBlur::gfxAlphaBoxBlur()
{
}

gfxAlphaBoxBlur::~gfxAlphaBoxBlur()
{
}

gfxContext*
gfxAlphaBoxBlur::Init(const gfxRect& aRect,
                      const gfxIntSize& aSpreadRadius,
                      const gfxIntSize& aBlurRadius,
                      const gfxRect* aDirtyRect,
                      const gfxRect* aSkipRect)
{
    mSpreadRadius = aSpreadRadius;
    mBlurRadius = aBlurRadius;

    gfxRect rect(aRect);
    rect.Outset(aBlurRadius + aSpreadRadius);
    rect.RoundOut();

    if (rect.IsEmpty())
        return nsnull;

    if (aDirtyRect) {
        // If we get passed a dirty rect from layout, we can minimize the
        // shadow size and make painting faster.
        mHasDirtyRect = PR_TRUE;
        mDirtyRect = *aDirtyRect;
        gfxRect requiredBlurArea = mDirtyRect.Intersect(rect);
        requiredBlurArea.Outset(aBlurRadius + aSpreadRadius);
        rect = requiredBlurArea.Intersect(rect);
    } else {
        mHasDirtyRect = PR_FALSE;
    }

    if (aSkipRect) {
        // If we get passed a skip rect, we can lower the amount of
        // blurring/spreading we need to do. We convert it to nsIntRect to avoid
        // expensive int<->float conversions if we were to use gfxRect instead.
        gfxRect skipRect = *aSkipRect;
        skipRect.RoundIn();
        skipRect.Inset(aBlurRadius + aSpreadRadius);
        mSkipRect = gfxThebesUtils::GfxRectToIntRect(skipRect);
        nsIntRect shadowIntRect = gfxThebesUtils::GfxRectToIntRect(rect);
        mSkipRect.IntersectRect(mSkipRect, shadowIntRect);
        if (mSkipRect == shadowIntRect)
          return nsnull;

        mSkipRect -= shadowIntRect.TopLeft();
    } else {
        mSkipRect = nsIntRect(0, 0, 0, 0);
    }

    // Make an alpha-only surface to draw on. We will play with the data after
    // everything is drawn to create a blur effect.
    mImageSurface = new gfxImageSurface(gfxIntSize(static_cast<PRInt32>(rect.Width()), static_cast<PRInt32>(rect.Height())),
                                        gfxASurface::ImageFormatA8);
    if (!mImageSurface || mImageSurface->CairoStatus())
        return nsnull;

    // Use a device offset so callers don't need to worry about translating
    // coordinates, they can draw as if this was part of the destination context
    // at the coordinates of rect.
    mImageSurface->SetDeviceOffset(-rect.TopLeft());

    mContext = new gfxContext(mImageSurface);

    return mContext;
}

void
gfxAlphaBoxBlur::PremultiplyAlpha(gfxFloat alpha)
{
    if (!mImageSurface)
        return;

    unsigned char* data = mImageSurface->Data();
    PRInt32 length = mImageSurface->GetDataSize();

    for (PRInt32 i=0; i<length; ++i)
        data[i] = static_cast<unsigned char>(data[i] * alpha);
}

/**
 * Box blur involves looking at one pixel, and setting its value to the average
 * of its neighbouring pixels.
 * @param aInput The input buffer.
 * @param aOutput The output buffer.
 * @param aLeftLobe The number of pixels to blend on the left.
 * @param aRightLobe The number of pixels to blend on the right.
 * @param aWidth The number of columns in the buffers.
 * @param aRows The number of rows in the buffers.
 * @param aSkipRect An area to skip blurring in.
 * XXX shouldn't we pass stride in separately here?
 */
static void
BoxBlurHorizontal(unsigned char* aInput,
                  unsigned char* aOutput,
                  PRInt32 aLeftLobe,
                  PRInt32 aRightLobe,
                  PRInt32 aWidth,
                  PRInt32 aRows,
                  const nsIntRect& aSkipRect)
{
    PRInt32 boxSize = aLeftLobe + aRightLobe + 1;
    PRBool skipRectCoversWholeRow = 0 >= aSkipRect.x &&
                                    aWidth <= aSkipRect.XMost();

    for (PRInt32 y = 0; y < aRows; y++) {
        // Check whether the skip rect intersects this row. If the skip
        // rect covers the whole surface in this row, we can avoid
        // this row entirely (and any others along the skip rect).
        PRBool inSkipRectY = y >= aSkipRect.y &&
                             y < aSkipRect.YMost();
        if (inSkipRectY && skipRectCoversWholeRow) {
            y = aSkipRect.YMost() - 1;
            continue;
        }

        PRInt32 alphaSum = 0;
        for (PRInt32 i = 0; i < boxSize; i++) {
            PRInt32 pos = i - aLeftLobe;
            pos = NS_MAX(pos, 0);
            pos = NS_MIN(pos, aWidth - 1);
            alphaSum += aInput[aWidth * y + pos];
        }
        for (PRInt32 x = 0; x < aWidth; x++) {
            // Check whether we are within the skip rect. If so, go
            // to the next point outside the skip rect.
            if (inSkipRectY && x >= aSkipRect.x &&
                x < aSkipRect.XMost()) {
                x = aSkipRect.XMost();
                if (x >= aWidth)
                    break;

                // Recalculate the neighbouring alpha values for
                // our new point on the surface.
                alphaSum = 0;
                for (PRInt32 i = 0; i < boxSize; i++) {
                    PRInt32 pos = x + i - aLeftLobe;
                    pos = NS_MAX(pos, 0);
                    pos = NS_MIN(pos, aWidth - 1);
                    alphaSum += aInput[aWidth * y + pos];
                }
            }
            PRInt32 tmp = x - aLeftLobe;
            PRInt32 last = NS_MAX(tmp, 0);
            PRInt32 next = NS_MIN(tmp + boxSize, aWidth - 1);

            aOutput[aWidth * y + x] = alphaSum/boxSize;

            alphaSum += aInput[aWidth * y + next] -
                        aInput[aWidth * y + last];
        }
    }
}

/**
 * Identical to BoxBlurHorizontal, except it blurs top and bottom instead of
 * left and right.
 * XXX shouldn't we pass stride in separately here?
 */
static void
BoxBlurVertical(unsigned char* aInput,
                unsigned char* aOutput,
                PRInt32 aTopLobe,
                PRInt32 aBottomLobe,
                PRInt32 aWidth,
                PRInt32 aRows,
                const nsIntRect& aSkipRect)
{
    PRInt32 boxSize = aTopLobe + aBottomLobe + 1;
    PRBool skipRectCoversWholeColumn = 0 >= aSkipRect.y &&
                                       aRows <= aSkipRect.YMost();

    for (PRInt32 x = 0; x < aWidth; x++) {
        PRBool inSkipRectX = x >= aSkipRect.x &&
                             x < aSkipRect.XMost();
        if (inSkipRectX && skipRectCoversWholeColumn) {
            x = aSkipRect.XMost() - 1;
            continue;
        }

        PRInt32 alphaSum = 0;
        for (PRInt32 i = 0; i < boxSize; i++) {
            PRInt32 pos = i - aTopLobe;
            pos = NS_MAX(pos, 0);
            pos = NS_MIN(pos, aRows - 1);
            alphaSum += aInput[aWidth * pos + x];
        }
        for (PRInt32 y = 0; y < aRows; y++) {
            if (inSkipRectX && y >= aSkipRect.y &&
                y < aSkipRect.YMost()) {
                y = aSkipRect.YMost();
                if (y >= aRows)
                    break;

                alphaSum = 0;
                for (PRInt32 i = 0; i < boxSize; i++) {
                    PRInt32 pos = y + i - aTopLobe;
                    pos = NS_MAX(pos, 0);
                    pos = NS_MIN(pos, aRows - 1);
                    alphaSum += aInput[aWidth * pos + x];
                }
            }
            PRInt32 tmp = y - aTopLobe;
            PRInt32 last = NS_MAX(tmp, 0);
            PRInt32 next = NS_MIN(tmp + boxSize, aRows - 1);

            aOutput[aWidth * y + x] = alphaSum/boxSize;

            alphaSum += aInput[aWidth * next + x] -
                        aInput[aWidth * last + x];
        }
    }
}

static void ComputeLobes(PRInt32 aRadius, PRInt32 aLobes[3][2])
{
    PRInt32 major, minor, final;

    /* See http://www.w3.org/TR/SVG/filters.html#feGaussianBlur for
     * some notes about approximating the Gaussian blur with box-blurs.
     * The comments below are in the terminology of that page.
     */
    PRInt32 z = aRadius/3;
    switch (aRadius % 3) {
    case 0:
        // aRadius = z*3; choose d = 2*z + 1
        major = minor = final = z;
        break;
    case 1:
        // aRadius = z*3 + 1
        // This is a tricky case since there is no value of d which will
        // yield a radius of exactly aRadius. If d is odd, i.e. d=2*k + 1
        // for some integer k, then the radius will be 3*k. If d is even,
        // i.e. d=2*k, then the radius will be 3*k - 1.
        // So we have to choose values that don't match the standard
        // algorithm.
        major = z + 1;
        minor = final = z;
        break;
    case 2:
        // aRadius = z*3 + 2; choose d = 2*z + 2
        major = final = z + 1;
        minor = z;
        break;
    }
    NS_ASSERTION(major + minor + final == aRadius,
                 "Lobes don't sum to the right length");

    aLobes[0][0] = major;
    aLobes[0][1] = minor;
    aLobes[1][0] = minor;
    aLobes[1][1] = major;
    aLobes[2][0] = final;
    aLobes[2][1] = final;
}

static void
SpreadHorizontal(unsigned char* aInput,
                 unsigned char* aOutput,
                 PRInt32 aRadius,
                 PRInt32 aWidth,
                 PRInt32 aRows,
                 PRInt32 aStride,
                 const nsIntRect& aSkipRect)
{
    if (aRadius == 0) {
        memcpy(aOutput, aInput, aStride*aRows);
        return;
    }

    PRBool skipRectCoversWholeRow = 0 >= aSkipRect.x &&
                                    aWidth <= aSkipRect.XMost();
    for (PRInt32 y = 0; y < aRows; y++) {
        // Check whether the skip rect intersects this row. If the skip
        // rect covers the whole surface in this row, we can avoid
        // this row entirely (and any others along the skip rect).
        PRBool inSkipRectY = y >= aSkipRect.y &&
                             y < aSkipRect.YMost();
        if (inSkipRectY && skipRectCoversWholeRow) {
            y = aSkipRect.YMost() - 1;
            continue;
        }

        for (PRInt32 x = 0; x < aWidth; x++) {
            // Check whether we are within the skip rect. If so, go
            // to the next point outside the skip rect.
            if (inSkipRectY && x >= aSkipRect.x &&
                x < aSkipRect.XMost()) {
                x = aSkipRect.XMost();
                if (x >= aWidth)
                    break;
            }

            PRInt32 sMin = PR_MAX(x - aRadius, 0);
            PRInt32 sMax = PR_MIN(x + aRadius, aWidth - 1);
            PRInt32 v = 0;
            for (PRInt32 s = sMin; s <= sMax; ++s) {
                v = PR_MAX(v, aInput[aStride * y + s]);
            }
            aOutput[aStride * y + x] = v;
        }
    }
}

static void
SpreadVertical(unsigned char* aInput,
               unsigned char* aOutput,
               PRInt32 aRadius,
               PRInt32 aWidth,
               PRInt32 aRows,
               PRInt32 aStride,
               const nsIntRect& aSkipRect)
{
    if (aRadius == 0) {
        memcpy(aOutput, aInput, aStride*aRows);
        return;
    }

    PRBool skipRectCoversWholeColumn = 0 >= aSkipRect.y &&
                                       aRows <= aSkipRect.YMost();
    for (PRInt32 x = 0; x < aWidth; x++) {
        PRBool inSkipRectX = x >= aSkipRect.x &&
                             x < aSkipRect.XMost();
        if (inSkipRectX && skipRectCoversWholeColumn) {
            x = aSkipRect.XMost() - 1;
            continue;
        }

        for (PRInt32 y = 0; y < aRows; y++) {
            // Check whether we are within the skip rect. If so, go
            // to the next point outside the skip rect.
            if (inSkipRectX && y >= aSkipRect.y &&
                y < aSkipRect.YMost()) {
                y = aSkipRect.YMost();
                if (y >= aRows)
                    break;
            }

            PRInt32 sMin = PR_MAX(y - aRadius, 0);
            PRInt32 sMax = PR_MIN(y + aRadius, aRows - 1);
            PRInt32 v = 0;
            for (PRInt32 s = sMin; s <= sMax; ++s) {
                v = PR_MAX(v, aInput[aStride * s + x]);
            }
            aOutput[aStride * y + x] = v;
        }
    }
}

void
gfxAlphaBoxBlur::Paint(gfxContext* aDestinationCtx, const gfxPoint& offset)
{
    if (!mContext)
        return;

    unsigned char* boxData = mImageSurface->Data();

    // no need to do all this if not blurring or spreading
    if (mBlurRadius != gfxIntSize(0,0) || mSpreadRadius != gfxIntSize(0,0)) {
        nsTArray<unsigned char> tempAlphaDataBuf;
        PRSize szB = mImageSurface->GetDataSize();
        if (!tempAlphaDataBuf.SetLength(szB))
           return; // OOM

        unsigned char* tmpData = tempAlphaDataBuf.Elements();
        // .SetLength above doesn't initialise the new elements since
        // they are unsigned chars and so have no default constructor.
        // So we have to initialise them by hand.
        memset(tmpData, 0, szB);

        PRInt32 stride = mImageSurface->Stride();
        PRInt32 rows = mImageSurface->Height();
        PRInt32 width = mImageSurface->Width();

        if (mSpreadRadius.width > 0 || mSpreadRadius.height > 0) {
            SpreadHorizontal(boxData, tmpData, mSpreadRadius.width, width, rows, stride, mSkipRect);
            SpreadVertical(tmpData, boxData, mSpreadRadius.height, width, rows, stride, mSkipRect);
        }

        if (mBlurRadius.width > 0) {
            PRInt32 lobes[3][2];
            ComputeLobes(mBlurRadius.width, lobes);
            BoxBlurHorizontal(boxData, tmpData, lobes[0][0], lobes[0][1], stride, rows, mSkipRect);
            BoxBlurHorizontal(tmpData, boxData, lobes[1][0], lobes[1][1], stride, rows, mSkipRect);
            BoxBlurHorizontal(boxData, tmpData, lobes[2][0], lobes[2][1], stride, rows, mSkipRect);
        } else {
            memcpy(tmpData, boxData, stride*rows);
        }

        if (mBlurRadius.height > 0) {
            PRInt32 lobes[3][2];
            ComputeLobes(mBlurRadius.height, lobes);
            BoxBlurVertical(tmpData, boxData, lobes[0][0], lobes[0][1], stride, rows, mSkipRect);
            BoxBlurVertical(boxData, tmpData, lobes[1][0], lobes[1][1], stride, rows, mSkipRect);
            BoxBlurVertical(tmpData, boxData, lobes[2][0], lobes[2][1], stride, rows, mSkipRect);
        } else {
            memcpy(boxData, tmpData, stride*rows);
        }
    }

    // Avoid a semi-expensive clip operation if we can, otherwise
    // clip to the dirty rect
    if (mHasDirtyRect) {
        aDestinationCtx->Save();
        aDestinationCtx->NewPath();
        aDestinationCtx->Rectangle(mDirtyRect);
        aDestinationCtx->Clip();
        aDestinationCtx->Mask(mImageSurface, offset);
        aDestinationCtx->Restore();
    } else {
        aDestinationCtx->Mask(mImageSurface, offset);
    }
}

/**
 * Compute the box blur size (which we're calling the blur radius) from
 * the standard deviation.
 *
 * Much of this, the 3 * sqrt(2 * pi) / 4, is the known value for
 * approximating a Gaussian using box blurs.  This yields quite a good
 * approximation for a Gaussian.  Then we multiply this by 1.5 since our
 * code wants the radius of the entire triple-box-blur kernel instead of
 * the diameter of an individual box blur.  For more details, see:
 *   http://www.w3.org/TR/SVG11/filters.html#feGaussianBlurElement
 *   https://bugzilla.mozilla.org/show_bug.cgi?id=590039#c19
 */
static const gfxFloat GAUSSIAN_SCALE_FACTOR = (3 * sqrt(2 * M_PI) / 4) * 1.5;

gfxIntSize gfxAlphaBoxBlur::CalculateBlurRadius(const gfxPoint& aStd)
{
    return gfxIntSize(
        static_cast<PRInt32>(floor(aStd.x * GAUSSIAN_SCALE_FACTOR + 0.5)),
        static_cast<PRInt32>(floor(aStd.y * GAUSSIAN_SCALE_FACTOR + 0.5)));
}
