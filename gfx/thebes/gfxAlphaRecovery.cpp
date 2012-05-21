/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxAlphaRecovery.h"

#include "gfxImageSurface.h"

#define MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE2
#include "mozilla/SSE.h"

/* static */ bool
gfxAlphaRecovery::RecoverAlpha(gfxImageSurface* blackSurf,
                               const gfxImageSurface* whiteSurf,
                               Analysis* analysis)
{
    gfxIntSize size = blackSurf->GetSize();

    if (size != whiteSurf->GetSize() ||
        (blackSurf->Format() != gfxASurface::ImageFormatARGB32 &&
         blackSurf->Format() != gfxASurface::ImageFormatRGB24) ||
        (whiteSurf->Format() != gfxASurface::ImageFormatARGB32 &&
         whiteSurf->Format() != gfxASurface::ImageFormatRGB24))
        return false;

#ifdef MOZILLA_MAY_SUPPORT_SSE2
    if (!analysis && mozilla::supports_sse2() &&
        RecoverAlphaSSE2(blackSurf, whiteSurf)) {
        return true;
    }
#endif

    blackSurf->Flush();
    whiteSurf->Flush();

    unsigned char* blackData = blackSurf->Data();
    unsigned char* whiteData = whiteSurf->Data();

    /* Get the alpha value of 'first' */
    PRUint32 first;
    if (size.width == 0 || size.height == 0) {
        first = 0;
    } else {
        if (!blackData || !whiteData)
            return false;

        first = RecoverPixel(*reinterpret_cast<PRUint32*>(blackData),
                             *reinterpret_cast<PRUint32*>(whiteData));
    }

    PRUint32 deltas = 0;
    for (PRInt32 i = 0; i < size.height; ++i) {
        PRUint32* blackPixel = reinterpret_cast<PRUint32*>(blackData);
        const PRUint32* whitePixel = reinterpret_cast<PRUint32*>(whiteData);
        for (PRInt32 j = 0; j < size.width; ++j) {
            PRUint32 recovered = RecoverPixel(blackPixel[j], whitePixel[j]);
            blackPixel[j] = recovered;
            deltas |= (first ^ recovered);
        }
        blackData += blackSurf->Stride();
        whiteData += whiteSurf->Stride();
    }

    blackSurf->MarkDirty();
    
    if (analysis) {
        analysis->uniformAlpha = (deltas >> 24) == 0;
        analysis->uniformColor = false;
        if (analysis->uniformAlpha) {
            double d_first_alpha = first >> 24;
            analysis->alpha = d_first_alpha/255.0;
            /* we only set uniformColor when the alpha is already uniform.
               it's only useful in that case ... and if the alpha was nonuniform
               then computing whether the color is uniform would require unpremultiplying
               every pixel */
            analysis->uniformColor = deltas == 0;
            if (analysis->uniformColor) {
                if (d_first_alpha == 0.0) {
                    /* can't unpremultiply, this is OK */
                    analysis->r = analysis->g = analysis->b = 0.0;
                } else {
                    analysis->r = (first & 0xFF)/d_first_alpha;
                    analysis->g = ((first >> 8) & 0xFF)/d_first_alpha;
                    analysis->b = ((first >> 16) & 0xFF)/d_first_alpha;
                }
            }
        }
    }

    return true;
}
