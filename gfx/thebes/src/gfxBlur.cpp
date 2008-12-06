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
 * The Initial Developer of the Original Code is Mozilla Corporation.
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
                      const gfxIntSize& aBlurRadius,
                      const gfxRect* aDirtyRect)
{
    mBlurRadius = aBlurRadius;

    gfxRect rect(aRect);
    rect.Outset(aBlurRadius.height, aBlurRadius.width,
                aBlurRadius.height, aBlurRadius.width);
    rect.RoundOut();

    if (rect.IsEmpty())
        return nsnull;

    if (aDirtyRect) {
        // If we get passed a dirty rect from layout, we can minimize the
        // shadow size and make painting faster.
        mHasDirtyRect = PR_TRUE;
        mDirtyRect = *aDirtyRect;
        gfxRect requiredBlurArea = mDirtyRect.Intersect(rect);
        requiredBlurArea.Outset(aBlurRadius.height, aBlurRadius.width,
                                aBlurRadius.height, aBlurRadius.width);
        rect = requiredBlurArea.Intersect(rect);
    } else {
        mHasDirtyRect = PR_FALSE;
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
 * @param aStride The stride of the buffers.
 * @param aRows The number of rows in the buffers.
 */
static void
BoxBlurHorizontal(unsigned char* aInput,
                  unsigned char* aOutput,
                  PRInt32 aLeftLobe,
                  PRInt32 aRightLobe,
                  PRInt32 aStride,
                  PRInt32 aRows)
{
    PRInt32 boxSize = aLeftLobe + aRightLobe + 1;

    for (PRInt32 y = 0; y < aRows; y++) {
        PRInt32 alphaSum = 0;
        for (PRInt32 i = 0; i < boxSize; i++) {
            PRInt32 pos = i - aLeftLobe;
            pos = PR_MAX(pos, 0);
            pos = PR_MIN(pos, aStride - 1);
            alphaSum += aInput[aStride * y + pos];
        }
        for (PRInt32 x = 0; x < aStride; x++) {
            PRInt32 tmp = x - aLeftLobe;
            PRInt32 last = PR_MAX(tmp, 0);
            PRInt32 next = PR_MIN(tmp + boxSize, aStride - 1);

            aOutput[aStride * y + x] = alphaSum/boxSize;

            alphaSum += aInput[aStride * y + next] -
                        aInput[aStride * y + last];
        }
    }
}

/**
 * Identical to BoxBlurHorizontal, except it blurs top and bottom instead of
 * left and right.
 */
static void
BoxBlurVertical(unsigned char* aInput,
                unsigned char* aOutput,
                PRInt32 aTopLobe,
                PRInt32 aBottomLobe,
                PRInt32 aStride,
                PRInt32 aRows)
{
    PRInt32 boxSize = aTopLobe + aBottomLobe + 1;

    for (PRInt32 x = 0; x < aStride; x++) {
        PRInt32 alphaSum = 0;
        for (PRInt32 i = 0; i < boxSize; i++) {
            PRInt32 pos = i - aTopLobe;
            pos = PR_MAX(pos, 0);
            pos = PR_MIN(pos, aRows - 1);
            alphaSum += aInput[aStride * pos + x];
        }
        for (PRInt32 y = 0; y < aRows; y++) {
            PRInt32 tmp = y - aTopLobe;
            PRInt32 last = PR_MAX(tmp, 0);
            PRInt32 next = PR_MIN(tmp + boxSize, aRows - 1);

            aOutput[aStride * y + x] = alphaSum/boxSize;

            alphaSum += aInput[aStride * next + x] -
                        aInput[aStride * last + x];
        }
    }
}

void
gfxAlphaBoxBlur::Paint(gfxContext* aDestinationCtx, const gfxPoint& offset)
{
    if (!mContext)
        return;

    unsigned char* boxData = mImageSurface->Data();

    // no need to do all this if not blurring
    if (mBlurRadius.width != 0 || mBlurRadius.height != 0) {
        // A blur radius of 1 achieves nothing (1/2 = 0 in int terms),
        // but we still want a blur!
        // XXX this may not be appropriate... perhaps just use actuall Gaussian here?
        mBlurRadius.width = PR_MAX(mBlurRadius.width, 2);
        mBlurRadius.height = PR_MAX(mBlurRadius.height, 2);

        nsTArray<unsigned char> tempAlphaDataBuf;
        if (!tempAlphaDataBuf.SetLength(mImageSurface->GetDataSize()))
            return; // OOM

        unsigned char* tmpData = tempAlphaDataBuf.Elements();
        PRInt32 stride = mImageSurface->Stride();
        PRInt32 rows = mImageSurface->Height();

        if (mBlurRadius.width > 0) {
            PRInt32 longLobe = mBlurRadius.width / 2;
            PRInt32 shortLobe = (mBlurRadius.width & 1) ? longLobe : longLobe - 1;
            BoxBlurHorizontal(boxData, tmpData, longLobe, shortLobe, stride, rows);
            BoxBlurHorizontal(tmpData, boxData, shortLobe, longLobe, stride, rows);
            BoxBlurHorizontal(boxData, tmpData, longLobe, longLobe, stride, rows);
        }

        if (mBlurRadius.height > 0) {
            PRInt32 longLobe = mBlurRadius.height / 2;
            PRInt32 shortLobe = (mBlurRadius.height & 1) ? longLobe : longLobe - 1;
            BoxBlurVertical(tmpData, boxData, longLobe, shortLobe, stride, rows);
            BoxBlurVertical(boxData, tmpData, shortLobe, longLobe, stride, rows);
            BoxBlurVertical(tmpData, boxData, longLobe, longLobe, stride, rows);
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

static const gfxFloat GAUSSIAN_SCALE_FACTOR = 3 * sqrt(2 * M_PI) / 4;

gfxIntSize gfxAlphaBoxBlur::CalculateBlurRadius(const gfxPoint& aStd)
{
    return gfxIntSize(
        static_cast<PRInt32>(floor(aStd.x * GAUSSIAN_SCALE_FACTOR + 0.5)),
        static_cast<PRInt32>(floor(aStd.y * GAUSSIAN_SCALE_FACTOR + 0.5)));
}
