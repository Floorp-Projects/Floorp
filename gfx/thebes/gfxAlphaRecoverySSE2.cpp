/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Thebes gfx.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "mozilla/SSE.h"
#include "gfxAlphaRecovery.h"
#include <emmintrin.h>

// This file should only be compiled on x86 and x64 systems.  Additionally,
// you'll need to compile it with -msse2 if you're using GCC on x86.

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64))
__declspec(align(16)) static PRUint32 greenMaski[] =
    { 0x0000ff00, 0x0000ff00, 0x0000ff00, 0x0000ff00 };
__declspec(align(16)) static PRUint32 alphaMaski[] =
    { 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
static PRUint32 greenMaski[] __attribute__ ((aligned (16))) =
    { 0x0000ff00, 0x0000ff00, 0x0000ff00, 0x0000ff00 };
static PRUint32 alphaMaski[] __attribute__ ((aligned (16))) =
    { 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
#elif defined(__SUNPRO_CC) && (defined(__i386) || defined(__x86_64__))
#pragma align 16 (greenMaski, alphaMaski)
static PRUint32 greenMaski[] = { 0x0000ff00, 0x0000ff00, 0x0000ff00, 0x0000ff00 };
static PRUint32 alphaMaski[] = { 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
#endif

PRBool
gfxAlphaRecovery::RecoverAlphaSSE2(gfxImageSurface* blackSurf,
                                   const gfxImageSurface* whiteSurf)
{
    gfxIntSize size = blackSurf->GetSize();

    if (size != whiteSurf->GetSize() ||
        (blackSurf->Format() != gfxASurface::ImageFormatARGB32 &&
         blackSurf->Format() != gfxASurface::ImageFormatRGB24) ||
        (whiteSurf->Format() != gfxASurface::ImageFormatARGB32 &&
         whiteSurf->Format() != gfxASurface::ImageFormatRGB24))
        return PR_FALSE;

    blackSurf->Flush();
    whiteSurf->Flush();

    unsigned char* blackData = blackSurf->Data();
    unsigned char* whiteData = whiteSurf->Data();

    if ((NS_PTR_TO_UINT32(blackData) & 0xf) != (NS_PTR_TO_UINT32(whiteData) & 0xf) ||
        (blackSurf->Stride() - whiteSurf->Stride()) & 0xf) {
        // Cannot keep these in alignment.
        return PR_FALSE;
    }

    __m128i greenMask = _mm_load_si128((__m128i*)greenMaski);
    __m128i alphaMask = _mm_load_si128((__m128i*)alphaMaski);

    for (PRInt32 i = 0; i < size.height; ++i) {
        PRInt32 j = 0;
        // Loop single pixels until at 4 byte alignment.
        while (NS_PTR_TO_UINT32(blackData) & 0xf && j < size.width) {
            *((PRUint32*)blackData) =
                RecoverPixel(*reinterpret_cast<PRUint32*>(blackData),
                             *reinterpret_cast<PRUint32*>(whiteData));
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
            *((PRUint32*)blackData) =
                RecoverPixel(*reinterpret_cast<PRUint32*>(blackData),
                             *reinterpret_cast<PRUint32*>(whiteData));
            blackData += 4;
            whiteData += 4;
            j++;
        }
        blackData += blackSurf->Stride() - j * 4;
        whiteData += whiteSurf->Stride() - j * 4;
    }

    blackSurf->MarkDirty();

    return PR_TRUE;
}
