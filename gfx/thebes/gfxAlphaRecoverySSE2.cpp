/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxAlphaRecovery.h"
#include "gfxImageSurface.h"
#include <emmintrin.h>

// This file should only be compiled on x86 and x64 systems.  Additionally,
// you'll need to compile it with -msse2 if you're using GCC on x86.

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64))
__declspec(align(16)) static uint32_t greenMaski[] =
    { 0x0000ff00, 0x0000ff00, 0x0000ff00, 0x0000ff00 };
__declspec(align(16)) static uint32_t alphaMaski[] =
    { 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
static uint32_t greenMaski[] __attribute__ ((aligned (16))) =
    { 0x0000ff00, 0x0000ff00, 0x0000ff00, 0x0000ff00 };
static uint32_t alphaMaski[] __attribute__ ((aligned (16))) =
    { 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
#elif defined(__SUNPRO_CC) && (defined(__i386) || defined(__x86_64__))
#pragma align 16 (greenMaski, alphaMaski)
static uint32_t greenMaski[] = { 0x0000ff00, 0x0000ff00, 0x0000ff00, 0x0000ff00 };
static uint32_t alphaMaski[] = { 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
#endif

bool
gfxAlphaRecovery::RecoverAlphaSSE2(gfxImageSurface* blackSurf,
                                   const gfxImageSurface* whiteSurf)
{
    gfxIntSize size = blackSurf->GetSize();

    if (size != whiteSurf->GetSize() ||
        (blackSurf->Format() != gfxImageFormat::ARGB32 &&
         blackSurf->Format() != gfxImageFormat::RGB24) ||
        (whiteSurf->Format() != gfxImageFormat::ARGB32 &&
         whiteSurf->Format() != gfxImageFormat::RGB24))
        return false;

    blackSurf->Flush();
    whiteSurf->Flush();

    unsigned char* blackData = blackSurf->Data();
    unsigned char* whiteData = whiteSurf->Data();

    if ((NS_PTR_TO_UINT32(blackData) & 0xf) != (NS_PTR_TO_UINT32(whiteData) & 0xf) ||
        (blackSurf->Stride() - whiteSurf->Stride()) & 0xf) {
        // Cannot keep these in alignment.
        return false;
    }

    __m128i greenMask = _mm_load_si128((__m128i*)greenMaski);
    __m128i alphaMask = _mm_load_si128((__m128i*)alphaMaski);

    for (int32_t i = 0; i < size.height; ++i) {
        int32_t j = 0;
        // Loop single pixels until at 4 byte alignment.
        while (NS_PTR_TO_UINT32(blackData) & 0xf && j < size.width) {
            *((uint32_t*)blackData) =
                RecoverPixel(*reinterpret_cast<uint32_t*>(blackData),
                             *reinterpret_cast<uint32_t*>(whiteData));
            blackData += 4;
            whiteData += 4;
            j++;
        }
        // This extra loop allows the compiler to do some more clever registry
        // management and makes it about 5% faster than with only the 4 pixel
        // at a time loop.
        for (; j < size.width - 8; j += 8) {
            __m128i black1 = _mm_load_si128((__m128i*)blackData);
            __m128i white1 = _mm_load_si128((__m128i*)whiteData);
            __m128i black2 = _mm_load_si128((__m128i*)(blackData + 16));
            __m128i white2 = _mm_load_si128((__m128i*)(whiteData + 16));

            // Execute the same instructions as described in RecoverPixel, only
            // using an SSE2 packed saturated subtract.
            white1 = _mm_subs_epu8(white1, black1);
            white2 = _mm_subs_epu8(white2, black2);
            white1 = _mm_subs_epu8(greenMask, white1);
            white2 = _mm_subs_epu8(greenMask, white2);
            // Producing the final black pixel in an XMM register and storing
            // that is actually faster than doing a masked store since that
            // does an unaligned storage. We have the black pixel in a register
            // anyway.
            black1 = _mm_andnot_si128(alphaMask, black1);
            black2 = _mm_andnot_si128(alphaMask, black2);
            white1 = _mm_slli_si128(white1, 2);
            white2 = _mm_slli_si128(white2, 2);
            white1 = _mm_and_si128(alphaMask, white1);
            white2 = _mm_and_si128(alphaMask, white2);
            black1 = _mm_or_si128(white1, black1);
            black2 = _mm_or_si128(white2, black2);

            _mm_store_si128((__m128i*)blackData, black1);
            _mm_store_si128((__m128i*)(blackData + 16), black2);
            blackData += 32;
            whiteData += 32;
        }
        for (; j < size.width - 4; j += 4) {
            __m128i black = _mm_load_si128((__m128i*)blackData);
            __m128i white = _mm_load_si128((__m128i*)whiteData);

            white = _mm_subs_epu8(white, black);
            white = _mm_subs_epu8(greenMask, white);
            black = _mm_andnot_si128(alphaMask, black);
            white = _mm_slli_si128(white, 2);
            white = _mm_and_si128(alphaMask, white);
            black = _mm_or_si128(white, black);
            _mm_store_si128((__m128i*)blackData, black);
            blackData += 16;
            whiteData += 16;
        }
        // Loop single pixels until we're done.
        while (j < size.width) {
            *((uint32_t*)blackData) =
                RecoverPixel(*reinterpret_cast<uint32_t*>(blackData),
                             *reinterpret_cast<uint32_t*>(whiteData));
            blackData += 4;
            whiteData += 4;
            j++;
        }
        blackData += blackSurf->Stride() - j * 4;
        whiteData += whiteSurf->Stride() - j * 4;
    }

    blackSurf->MarkDirty();

    return true;
}

static int32_t
ByteAlignment(int32_t aAlignToLog2, int32_t aX, int32_t aY=0, int32_t aStride=1)
{
    return (aX + aStride * aY) & ((1 << aAlignToLog2) - 1);
}

/*static*/ mozilla::gfx::IntRect
gfxAlphaRecovery::AlignRectForSubimageRecovery(const mozilla::gfx::IntRect& aRect,
                                               gfxImageSurface* aSurface)
{
    NS_ASSERTION(gfxImageFormat::ARGB32 == aSurface->Format(),
                 "Thebes grew support for non-ARGB32 COLOR_ALPHA?");
    static const int32_t kByteAlignLog2 = GoodAlignmentLog2();
    static const int32_t bpp = 4;
    static const int32_t pixPerAlign = (1 << kByteAlignLog2) / bpp;
    //
    // We're going to create a subimage of the surface with size
    // <sw,sh> for alpha recovery, and want a SIMD fast-path.  The
    // rect <x,y, w,h> /needs/ to be redrawn, but it might not be
    // properly aligned for SIMD.  So we want to find a rect <x',y',
    // w',h'> that's a superset of what needs to be redrawn but is
    // properly aligned.  Proper alignment is
    //
    //   BPP * (x' + y' * sw) \cong 0         (mod ALIGN)
    //   BPP * w'             \cong BPP * sw  (mod ALIGN)
    //
    // (We assume the pixel at surface <0,0> is already ALIGN'd.)
    // That rect (obviously) has to fit within the surface bounds, and
    // we should also minimize the extra pixels redrawn only for
    // alignment's sake.  So we also want
    //
    //  minimize <x',y', w',h'>
    //   0 <= x' <= x
    //   0 <= y' <= y
    //   w <= w' <= sw
    //   h <= h' <= sh
    //
    // This is a messy integer non-linear programming problem, except
    // ... we can assume that ALIGN/BPP is a very small constant.  So,
    // brute force is viable.  The algorithm below will find a
    // solution if one exists, but isn't guaranteed to find the
    // minimum solution.  (For SSE2, ALIGN/BPP = 4, so it'll do at
    // most 64 iterations below).  In what's likely the common case,
    // an already-aligned rectangle, it only needs 1 iteration.
    //
    // Is this alignment worth doing?  Recovering alpha will take work
    // proportional to w*h (assuming alpha recovery computation isn't
    // memory bound).  This analysis can lead to O(w+h) extra work
    // (with small constants).  In exchange, we expect to shave off a
    // ALIGN/BPP constant by using SIMD-ized alpha recovery.  So as
    // w*h diverges from w+h, the win factor approaches ALIGN/BPP.  We
    // only really care about the w*h >> w+h case anyway; others
    // should be fast enough even with the overhead.  (Unless the cost
    // of repainting the expanded rect is high, but in that case
    // SIMD-ized alpha recovery won't make a difference so this code
    // shouldn't be called.)
    //
    gfxIntSize surfaceSize = aSurface->GetSize();
    const int32_t stride = bpp * surfaceSize.width;
    if (stride != aSurface->Stride()) {
        NS_WARNING("Unexpected stride, falling back on slow alpha recovery");
        return aRect;
    }

    const int32_t x = aRect.x, y = aRect.y, w = aRect.width, h = aRect.height;
    const int32_t r = x + w;
    const int32_t sw = surfaceSize.width;
    const int32_t strideAlign = ByteAlignment(kByteAlignLog2, stride);

    // The outer two loops below keep the rightmost (|r| above) and
    // bottommost pixels in |aRect| fixed wrt <x,y>, to ensure that we
    // return only a superset of the original rect.  These loops
    // search for an aligned top-left pixel by trying to expand <x,y>
    // left and up by <dx,dy> pixels, respectively.
    //
    // Then if a properly-aligned top-left pixel is found, the
    // innermost loop tries to find an aligned stride by moving the
    // rightmost pixel rightward by dr.
    int32_t dx, dy, dr;
    for (dy = 0; (dy < pixPerAlign) && (y - dy >= 0); ++dy) {
        for (dx = 0; (dx < pixPerAlign) && (x - dx >= 0); ++dx) {
            if (0 != ByteAlignment(kByteAlignLog2,
                                   bpp * (x - dx), y - dy, stride)) {
                continue;
            }
            for (dr = 0; (dr < pixPerAlign) && (r + dr <= sw); ++dr) {
                if (strideAlign == ByteAlignment(kByteAlignLog2,
                                                 bpp * (w + dr + dx))) {
                    goto FOUND_SOLUTION;
                }
            }
        }
    }

    // Didn't find a solution.
    return aRect;

FOUND_SOLUTION:
    mozilla::gfx::IntRect solution = mozilla::gfx::IntRect(x - dx, y - dy, w + dr + dx, h + dy);
    MOZ_ASSERT(mozilla::gfx::IntRect(0, 0, sw, surfaceSize.height).Contains(solution),
               "'Solution' extends outside surface bounds!");
    return solution;
}
