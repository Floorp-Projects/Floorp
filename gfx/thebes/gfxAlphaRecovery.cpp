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
                               const gfxImageSurface* whiteSurf)
{
    mozilla::gfx::IntSize size = blackSurf->GetSize();

    if (size != whiteSurf->GetSize() ||
        (blackSurf->Format() != mozilla::gfx::SurfaceFormat::A8R8G8B8_UINT32 &&
         blackSurf->Format() != mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32) ||
        (whiteSurf->Format() != mozilla::gfx::SurfaceFormat::A8R8G8B8_UINT32 &&
         whiteSurf->Format() != mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32))
        return false;

#ifdef MOZILLA_MAY_SUPPORT_SSE2
    if (mozilla::supports_sse2() &&
        RecoverAlphaSSE2(blackSurf, whiteSurf)) {
        return true;
    }
#endif

    blackSurf->Flush();
    whiteSurf->Flush();

    unsigned char* blackData = blackSurf->Data();
    unsigned char* whiteData = whiteSurf->Data();

    for (int32_t i = 0; i < size.height; ++i) {
        uint32_t* blackPixel = reinterpret_cast<uint32_t*>(blackData);
        const uint32_t* whitePixel = reinterpret_cast<uint32_t*>(whiteData);
        for (int32_t j = 0; j < size.width; ++j) {
            uint32_t recovered = RecoverPixel(blackPixel[j], whitePixel[j]);
            blackPixel[j] = recovered;
        }
        blackData += blackSurf->Stride();
        whiteData += whiteSurf->Stride();
    }

    blackSurf->MarkDirty();

    return true;
}
