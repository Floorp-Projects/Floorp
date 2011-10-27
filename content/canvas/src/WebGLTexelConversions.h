/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// the pixel conversions code here is originally from this file:
//   http://trac.webkit.org/browser/trunk/WebCore/platform/graphics/GraphicsContext3D.cpp

// Keep as much as possible unchanged to ease sharing code with the WebKit guys.
// Changes:
//  * added BGR8 path, we need it in Mozilla to load textures from DOMElements
//  * enclosing in a namespace WebGLTexelConversions to make it clear it is, in profilers and in symbol table dumps
//  * added __restrict keywords. Although non-standard, this is very well supported across all compilers
//    that I know of (GCC/LLVM/MSC/ICC/XLC...)
//  * optimized scaleFactor computation in Unmultiply functions (1 div instead of 2)

#ifndef WEBGLTEXELCONVERSIONS_H_
#define WEBGLTEXELCONVERSIONS_H_

#ifdef __SUNPRO_CC
#define __restrict
#endif

#include "WebGLContext.h"

#if defined _MSC_VER
#define FORCE_INLINE __forceinline
#elif defined __GNUC__
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

namespace mozilla {

typedef PRUint8  uint8_t;
typedef PRUint16 uint16_t;

namespace WebGLTexelConversions {

/****** BEGIN CODE SHARED WITH WEBKIT ******/

//----------------------------------------------------------------------
// Pixel unpacking routines.

FORCE_INLINE void unpackRGBA8ToRGBA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = source[3];
}

FORCE_INLINE void unpackRGB8ToRGBA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = 0xFF;
}

FORCE_INLINE void unpackBGRA8ToRGBA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[2];
    destination[1] = source[1];
    destination[2] = source[0];
    destination[3] = source[3];
}

FORCE_INLINE void unpackBGR8ToRGBA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[2];
    destination[1] = source[1];
    destination[2] = source[0];
    destination[3] = 0xFF;
}

FORCE_INLINE void unpackRGBA5551ToRGBA8(const uint16_t* __restrict source, uint8_t* __restrict destination)
{
    uint16_t packedValue = source[0];
    uint8_t r = packedValue >> 11;
    uint8_t g = (packedValue >> 6) & 0x1F;
    uint8_t b = (packedValue >> 1) & 0x1F;
    destination[0] = (r << 3) | (r & 0x7);
    destination[1] = (g << 3) | (g & 0x7);
    destination[2] = (b << 3) | (b & 0x7);
    destination[3] = (packedValue & 0x1) ? 0xFF : 0x0;
}

FORCE_INLINE void unpackRGBA4444ToRGBA8(const uint16_t* __restrict source, uint8_t* __restrict destination)
{
    uint16_t packedValue = source[0];
    uint8_t r = packedValue >> 12;
    uint8_t g = (packedValue >> 8) & 0x0F;
    uint8_t b = (packedValue >> 4) & 0x0F;
    uint8_t a = packedValue & 0x0F;
    destination[0] = r << 4 | r;
    destination[1] = g << 4 | g;
    destination[2] = b << 4 | b;
    destination[3] = a << 4 | a;
}

FORCE_INLINE void unpackRGB565ToRGBA8(const uint16_t* __restrict source, uint8_t* __restrict destination)
{
    uint16_t packedValue = source[0];
    uint8_t r = packedValue >> 11;
    uint8_t g = (packedValue >> 5) & 0x3F;
    uint8_t b = packedValue & 0x1F;
    destination[0] = (r << 3) | (r & 0x7);
    destination[1] = (g << 2) | (g & 0x3);
    destination[2] = (b << 3) | (b & 0x7);
    destination[3] = 0xFF;
}

FORCE_INLINE void unpackR8ToRGBA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = 0xFF;
}

FORCE_INLINE void unpackRA8ToRGBA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
}

FORCE_INLINE void unpackA8ToRGBA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = 0x0;
    destination[1] = 0x0;
    destination[2] = 0x0;
    destination[3] = source[0];
}

FORCE_INLINE void unpackRGB32FToRGBA32F(const float* __restrict source, float* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = 1;
}

FORCE_INLINE void unpackR32FToRGBA32F(const float* __restrict source, float* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = 1;
}

FORCE_INLINE void unpackRA32FToRGBA32F(const float* __restrict source, float* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
}

FORCE_INLINE void unpackA32FToRGBA32F(const float* __restrict source, float* __restrict destination)
{
    destination[0] = 0;
    destination[1] = 0;
    destination[2] = 0;
    destination[3] = source[0];
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

FORCE_INLINE void packRGBA8ToA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[3];
}

FORCE_INLINE void packRGBA8ToR8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[0];
}

FORCE_INLINE void packRGBA8ToR8Premultiply(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    destination[0] = sourceR;
}

// FIXME: this routine is lossy and must be removed.
FORCE_INLINE void packRGBA8ToR8Unmultiply(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    destination[0] = sourceR;
}

FORCE_INLINE void packRGBA8ToRA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[3];
}

FORCE_INLINE void packRGBA8ToRA8Premultiply(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = source[3];
}

// FIXME: this routine is lossy and must be removed.
FORCE_INLINE void packRGBA8ToRA8Unmultiply(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = source[3];
}

FORCE_INLINE void packRGBA8ToRGB8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

FORCE_INLINE void packRGBA8ToRGB8Premultiply(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = sourceG;
    destination[2] = sourceB;
}

// FIXME: this routine is lossy and must be removed.
FORCE_INLINE void packRGBA8ToRGB8Unmultiply(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = sourceG;
    destination[2] = sourceB;
}

// This is only used when the source format is different than kSourceFormatRGBA8.
FORCE_INLINE void packRGBA8ToRGBA8(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = source[3];
}

FORCE_INLINE void packRGBA8ToRGBA8Premultiply(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = sourceG;
    destination[2] = sourceB;
    destination[3] = source[3];
}

// FIXME: this routine is lossy and must be removed.
FORCE_INLINE void packRGBA8ToRGBA8Unmultiply(const uint8_t* __restrict source, uint8_t* __restrict destination)
{
    float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = sourceG;
    destination[2] = sourceB;
    destination[3] = source[3];
}

FORCE_INLINE void packRGBA8ToUnsignedShort4444(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    *destination = (((source[0] & 0xF0) << 8)
                    | ((source[1] & 0xF0) << 4)
                    | (source[2] & 0xF0)
                    | (source[3] >> 4));
}

FORCE_INLINE void packRGBA8ToUnsignedShort4444Premultiply(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF0) << 8)
                    | ((sourceG & 0xF0) << 4)
                    | (sourceB & 0xF0)
                    | (source[3] >> 4));
}

// FIXME: this routine is lossy and must be removed.
FORCE_INLINE void packRGBA8ToUnsignedShort4444Unmultiply(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF0) << 8)
                    | ((sourceG & 0xF0) << 4)
                    | (sourceB & 0xF0)
                    | (source[3] >> 4));
}

FORCE_INLINE void packRGBA8ToUnsignedShort5551(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    *destination = (((source[0] & 0xF8) << 8)
                    | ((source[1] & 0xF8) << 3)
                    | ((source[2] & 0xF8) >> 2)
                    | (source[3] >> 7));
}

FORCE_INLINE void packRGBA8ToUnsignedShort5551Premultiply(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF8) << 8)
                    | ((sourceG & 0xF8) << 3)
                    | ((sourceB & 0xF8) >> 2)
                    | (source[3] >> 7));
}

// FIXME: this routine is lossy and must be removed.
FORCE_INLINE void packRGBA8ToUnsignedShort5551Unmultiply(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF8) << 8)
                    | ((sourceG & 0xF8) << 3)
                    | ((sourceB & 0xF8) >> 2)
                    | (source[3] >> 7));
}

FORCE_INLINE void packRGBA8ToUnsignedShort565(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    *destination = (((source[0] & 0xF8) << 8)
                    | ((source[1] & 0xFC) << 3)
                    | ((source[2] & 0xF8) >> 3));
}

FORCE_INLINE void packRGBA8ToUnsignedShort565Premultiply(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF8) << 8)
                    | ((sourceG & 0xFC) << 3)
                    | ((sourceB & 0xF8) >> 3));
}

// FIXME: this routine is lossy and must be removed.
FORCE_INLINE void packRGBA8ToUnsignedShort565Unmultiply(const uint8_t* __restrict source, uint16_t* __restrict destination)
{
    float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF8) << 8)
                    | ((sourceG & 0xFC) << 3)
                    | ((sourceB & 0xF8) >> 3));
}

FORCE_INLINE void packRGBA32FToRGB32F(const float* __restrict source, float* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

FORCE_INLINE void packRGBA32FToRGB32FPremultiply(const float* __restrict source, float* __restrict destination)
{
    float scaleFactor = source[3];
    destination[0] = source[0] * scaleFactor;
    destination[1] = source[1] * scaleFactor;
    destination[2] = source[2] * scaleFactor;
}

FORCE_INLINE void packRGBA32FToRGBA32FPremultiply(const float* __restrict source, float* __restrict destination)
{
    float scaleFactor = source[3];
    destination[0] = source[0] * scaleFactor;
    destination[1] = source[1] * scaleFactor;
    destination[2] = source[2] * scaleFactor;
    destination[3] = source[3];
}

FORCE_INLINE void packRGBA32FToA32F(const float* __restrict source, float* __restrict destination)
{
    destination[0] = source[3];
}

// identical to above, to avoid special-casing
FORCE_INLINE void packRGBA32FToA32FPremultiply(const float* __restrict source, float* __restrict destination)
{
    destination[0] = source[3];
}

FORCE_INLINE void packRGBA32FToR32F(const float* __restrict source, float* __restrict destination)
{
    destination[0] = source[0];
}

FORCE_INLINE void packRGBA32FToR32FPremultiply(const float* __restrict source, float* __restrict destination)
{
    float scaleFactor = source[3];
    destination[0] = source[0] * scaleFactor;
}


FORCE_INLINE void packRGBA32FToRA32F(const float* __restrict source, float* __restrict destination)
{
    destination[0] = source[0];
    destination[1] = source[3];
}

FORCE_INLINE void packRGBA32FToRA32FPremultiply(const float* __restrict source, float* __restrict destination)
{
    float scaleFactor = source[3];
    destination[0] = source[0] * scaleFactor;
    destination[1] = scaleFactor;
}

/****** END CODE SHARED WITH WEBKIT ******/

} // end namespace WebGLTexelConversions

} // end namespace mozilla

#endif // WEBGLTEXELCONVERSIONS_H_
