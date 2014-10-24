/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRenderingContext.h"
#include <string.h>                     // for strlen
#include <algorithm>                    // for min
#include "gfxColor.h"                   // for gfxRGBA
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPoint.h"                   // for gfxPoint, gfxSize
#include "gfxRect.h"                    // for gfxRect
#include "gfxTypes.h"                   // for gfxFloat
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/mozalloc.h"           // for operator delete[], etc
#include "nsBoundingMetrics.h"          // for nsBoundingMetrics
#include "nsCharTraits.h"               // for NS_IS_LOW_SURROGATE
#include "nsDebug.h"                    // for NS_ERROR
#include "nsPoint.h"                    // for nsPoint
#include "nsRect.h"                     // for nsRect, nsIntRect
#include "nsRegion.h"                   // for nsIntRegionRectIterator, etc

// Hard limit substring lengths to 8000 characters ... this lets us statically
// size the cluster buffer array in FindSafeLength
#define MAX_GFX_TEXT_BUF_SIZE 8000

/*static*/ int32_t
nsRenderingContext::FindSafeLength(const char16_t *aString, uint32_t aLength,
                              uint32_t aMaxChunkLength)
{
    if (aLength <= aMaxChunkLength)
        return aLength;

    int32_t len = aMaxChunkLength;

    // Ensure that we don't break inside a surrogate pair
    while (len > 0 && NS_IS_LOW_SURROGATE(aString[len])) {
        len--;
    }
    if (len == 0) {
        // We don't want our caller to go into an infinite loop, so don't
        // return zero. It's hard to imagine how we could actually get here
        // unless there are languages that allow clusters of arbitrary size.
        // If there are and someone feeds us a 500+ character cluster, too
        // bad.
        return aMaxChunkLength;
    }
    return len;
}

//////////////////////////////////////////////////////////////////////
//// nsRenderingContext

void
nsRenderingContext::Init(gfxContext *aThebesContext)
{
    mThebes = aThebesContext;
    mThebes->SetLineWidth(1.0);
}

void
nsRenderingContext::Init(DrawTarget *aDrawTarget)
{
    Init(new gfxContext(aDrawTarget));
}


//
// text
//

void
nsRenderingContext::SetTextRunRTL(bool aIsRTL)
{
    mFontMetrics->SetTextRunRTL(aIsRTL);
}

void
nsRenderingContext::SetFont(nsFontMetrics *aFontMetrics)
{
    mFontMetrics = aFontMetrics;
}

int32_t
nsRenderingContext::GetMaxChunkLength()
{
    return std::min(mFontMetrics->GetMaxStringLength(), MAX_GFX_TEXT_BUF_SIZE);
}

nsBoundingMetrics
nsRenderingContext::GetBoundingMetrics(const char16_t* aString,
                                       uint32_t aLength)
{
    uint32_t maxChunkLength = GetMaxChunkLength();
    int32_t len = FindSafeLength(aString, aLength, maxChunkLength);
    // Assign directly in the first iteration. This ensures that
    // negative ascent/descent can be returned and the left bearing
    // is properly initialized.
    nsBoundingMetrics totalMetrics
        = mFontMetrics->GetBoundingMetrics(aString, len, this);
    aLength -= len;
    aString += len;

    while (aLength > 0) {
        len = FindSafeLength(aString, aLength, maxChunkLength);
        nsBoundingMetrics metrics
            = mFontMetrics->GetBoundingMetrics(aString, len, this);
        totalMetrics += metrics;
        aLength -= len;
        aString += len;
    }
    return totalMetrics;
}
