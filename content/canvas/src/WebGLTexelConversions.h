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

#ifndef WEBGLTEXELCONVERSIONS_H_
#define WEBGLTEXELCONVERSIONS_H_

#ifdef __SUNPRO_CC
#define __restrict
#endif

#include "WebGLTypes.h"
#include <stdint.h>

#if defined _MSC_VER
#define FORCE_INLINE __forceinline
#elif defined __GNUC__
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

namespace mozilla {

namespace WebGLTexelConversions {

enum WebGLTexelPremultiplicationOp
{
    NoPremultiplicationOp,
    Premultiply,
    Unpremultiply
};

template<int Format>
struct IsFloatFormat
{
    static const bool Value =
        Format == RGBA32F ||
        Format == RGB32F ||
        Format == RA32F ||
        Format == R32F ||
        Format == A32F;
};

template<int Format>
struct Is16bppFormat
{
    static const bool Value =
        Format == RGBA4444 ||
        Format == RGBA5551 ||
        Format == RGB565;
};

template<int Format,
         bool IsFloat = IsFloatFormat<Format>::Value,
         bool Is16bpp = Is16bppFormat<Format>::Value>
struct DataTypeForFormat
{
    typedef uint8_t Type;
};

template<int Format>
struct DataTypeForFormat<Format, true, false>
{
    typedef float Type;
};

template<int Format>
struct DataTypeForFormat<Format, false, true>
{
    typedef uint16_t Type;
};

template<int Format>
struct IntermediateFormat
{
    static const int Value = IsFloatFormat<Format>::Value ? RGBA32F : RGBA8;
};

inline size_t TexelBytesForFormat(int format) {
    switch (format) {
        case WebGLTexelConversions::R8:
        case WebGLTexelConversions::A8:
            return 1;
        case WebGLTexelConversions::RA8:
        case WebGLTexelConversions::RGBA5551:
        case WebGLTexelConversions::RGBA4444:
        case WebGLTexelConversions::RGB565:
        case WebGLTexelConversions::D16:
            return 2;
        case WebGLTexelConversions::RGB8:
            return 3;
        case WebGLTexelConversions::RGBA8:
        case WebGLTexelConversions::BGRA8:
        case WebGLTexelConversions::BGRX8:
        case WebGLTexelConversions::R32F:
        case WebGLTexelConversions::A32F:
        case WebGLTexelConversions::D32:
        case WebGLTexelConversions::D24S8:
            return 4;
        case WebGLTexelConversions::RA32F:
            return 8;
        case WebGLTexelConversions::RGB32F:
            return 12;
        case WebGLTexelConversions::RGBA32F:
            return 16;
        default:
            NS_ABORT_IF_FALSE(false, "Unknown texel format. Coding mistake?");
            return 0;
    }
}

FORCE_INLINE bool HasAlpha(int format) {
    return format == A8 ||
           format == A32F ||
           format == RA8 ||
           format == RA32F ||
           format == RGBA8 ||
           format == BGRA8 ||
           format == RGBA32F ||
           format == RGBA4444 ||
           format == RGBA5551;
}

FORCE_INLINE bool HasColor(int format) {
    return format == R8 ||
           format == R32F ||
           format == RA8 ||
           format == RA32F ||
           format == RGB8 ||
           format == BGRX8 ||
           format == RGB565 ||
           format == RGB32F ||
           format == RGBA8 ||
           format == BGRA8 ||
           format == RGBA32F ||
           format == RGBA4444 ||
           format == RGBA5551;
}


/****** BEGIN CODE SHARED WITH WEBKIT ******/

// the pack/unpack functions here are originally from this file:
//   http://trac.webkit.org/browser/trunk/WebCore/platform/graphics/GraphicsContext3D.cpp

//----------------------------------------------------------------------
// Pixel unpacking routines.

template<int Format, typename SrcType, typename DstType>
FORCE_INLINE void
unpack(const SrcType* __restrict src,
       DstType* __restrict dst)
{
    NS_ABORT_IF_FALSE(false, "Unimplemented texture format conversion");
}

template<> FORCE_INLINE void
unpack<RGBA8, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

template<> FORCE_INLINE void
unpack<RGB8, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = 0xFF;
}

template<> FORCE_INLINE void
unpack<BGRA8, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[2];
    dst[1] = src[1];
    dst[2] = src[0];
    dst[3] = src[3];
}

template<> FORCE_INLINE void
unpack<BGRX8, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[2];
    dst[1] = src[1];
    dst[2] = src[0];
    dst[3] = 0xFF;
}

template<> FORCE_INLINE void
unpack<RGBA5551, uint16_t, uint8_t>(const uint16_t* __restrict src, uint8_t* __restrict dst)
{
    uint16_t packedValue = src[0];
    uint8_t r = (packedValue >> 11) & 0x1F;
    uint8_t g = (packedValue >> 6) & 0x1F;
    uint8_t b = (packedValue >> 1) & 0x1F;
    dst[0] = (r << 3) | (r & 0x7);
    dst[1] = (g << 3) | (g & 0x7);
    dst[2] = (b << 3) | (b & 0x7);
    dst[3] = (packedValue & 0x1) ? 0xFF : 0;
}

template<> FORCE_INLINE void
unpack<RGBA4444, uint16_t, uint8_t>(const uint16_t* __restrict src, uint8_t* __restrict dst)
{
    uint16_t packedValue = src[0];
    uint8_t r = (packedValue >> 12) & 0x0F;
    uint8_t g = (packedValue >> 8) & 0x0F;
    uint8_t b = (packedValue >> 4) & 0x0F;
    uint8_t a = packedValue & 0x0F;
    dst[0] = (r << 4) | r;
    dst[1] = (g << 4) | g;
    dst[2] = (b << 4) | b;
    dst[3] = (a << 4) | a;
}

template<> FORCE_INLINE void
unpack<RGB565, uint16_t, uint8_t>(const uint16_t* __restrict src, uint8_t* __restrict dst)
{
    uint16_t packedValue = src[0];
    uint8_t r = (packedValue >> 11) & 0x1F;
    uint8_t g = (packedValue >> 5) & 0x3F;
    uint8_t b = packedValue & 0x1F;
    dst[0] = (r << 3) | (r & 0x7);
    dst[1] = (g << 2) | (g & 0x3);
    dst[2] = (b << 3) | (b & 0x7);
    dst[3] = 0xFF;
}

template<> FORCE_INLINE void
unpack<R8, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[0];
    dst[2] = src[0];
    dst[3] = 0xFF;
}

template<> FORCE_INLINE void
unpack<RA8, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[0];
    dst[2] = src[0];
    dst[3] = src[1];
}

template<> FORCE_INLINE void
unpack<A8, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = 0;
    dst[1] = 0;
    dst[2] = 0;
    dst[3] = src[0];
}

template<> FORCE_INLINE void
unpack<RGBA32F, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

template<> FORCE_INLINE void
unpack<RGB32F, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = 1.0f;
}

template<> FORCE_INLINE void
unpack<R32F, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[0];
    dst[2] = src[0];
    dst[3] = 1.0f;
}

template<> FORCE_INLINE void
unpack<RA32F, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[0];
    dst[2] = src[0];
    dst[3] = src[1];
}

template<> FORCE_INLINE void
unpack<A32F, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = 0;
    dst[1] = 0;
    dst[2] = 0;
    dst[3] = src[0];
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

template<int Format, int PremultiplicationOp, typename SrcType, typename DstType>
FORCE_INLINE void
pack(const SrcType* __restrict src,
     DstType* __restrict dst)
{
    NS_ABORT_IF_FALSE(false, "Unimplemented texture format conversion");
}

template<> FORCE_INLINE void
pack<A8, NoPremultiplicationOp, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[3];
}

template<> FORCE_INLINE void
pack<A8, Premultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[3];
}

template<> FORCE_INLINE void
pack<A8, Unpremultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[3];
}

template<> FORCE_INLINE void
pack<R8, NoPremultiplicationOp, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
}

template<> FORCE_INLINE void
pack<R8, Premultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    float scaleFactor = src[3] / 255.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    dst[0] = srcR;
}

template<> FORCE_INLINE void
pack<R8, Unpremultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    dst[0] = srcR;
}

template<> FORCE_INLINE void
pack<RA8, NoPremultiplicationOp, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[3];
}

template<> FORCE_INLINE void
pack<RA8, Premultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    float scaleFactor = src[3] / 255.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    dst[0] = srcR;
    dst[1] = src[3];
}

// FIXME: this routine is lossy and must be removed.
template<> FORCE_INLINE void
pack<RA8, Unpremultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    dst[0] = srcR;
    dst[1] = src[3];
}

template<> FORCE_INLINE void
pack<RGB8, NoPremultiplicationOp, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

template<> FORCE_INLINE void
pack<RGB8, Premultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    float scaleFactor = src[3] / 255.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    dst[0] = srcR;
    dst[1] = srcG;
    dst[2] = srcB;
}

template<> FORCE_INLINE void
pack<RGB8, Unpremultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    dst[0] = srcR;
    dst[1] = srcG;
    dst[2] = srcB;
}

template<> FORCE_INLINE void
pack<RGBA8, NoPremultiplicationOp, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

template<> FORCE_INLINE void
pack<RGBA8, Premultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    float scaleFactor = src[3] / 255.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    dst[0] = srcR;
    dst[1] = srcG;
    dst[2] = srcB;
    dst[3] = src[3];
}

// FIXME: this routine is lossy and must be removed.
template<> FORCE_INLINE void
pack<RGBA8, Unpremultiply, uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    dst[0] = srcR;
    dst[1] = srcG;
    dst[2] = srcB;
    dst[3] = src[3];
}

template<> FORCE_INLINE void
pack<RGBA4444, NoPremultiplicationOp, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    *dst = ( ((src[0] & 0xF0) << 8)
           | ((src[1] & 0xF0) << 4)
           | (src[2] & 0xF0)
           | (src[3] >> 4) );
}

template<> FORCE_INLINE void
pack<RGBA4444, Premultiply, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    float scaleFactor = src[3] / 255.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    *dst = ( ((srcR & 0xF0) << 8)
           | ((srcG & 0xF0) << 4)
           | (srcB & 0xF0)
           | (src[3] >> 4));
}

// FIXME: this routine is lossy and must be removed.
template<> FORCE_INLINE void
pack<RGBA4444, Unpremultiply, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    *dst = ( ((srcR & 0xF0) << 8)
           | ((srcG & 0xF0) << 4)
           | (srcB & 0xF0)
           | (src[3] >> 4));
}

template<> FORCE_INLINE void
pack<RGBA5551, NoPremultiplicationOp, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    *dst = ( ((src[0] & 0xF8) << 8)
           | ((src[1] & 0xF8) << 3)
           | ((src[2] & 0xF8) >> 2)
           | (src[3] >> 7));
}

template<> FORCE_INLINE void
pack<RGBA5551, Premultiply, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    float scaleFactor = src[3] / 255.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    *dst = ( ((srcR & 0xF8) << 8)
           | ((srcG & 0xF8) << 3)
           | ((srcB & 0xF8) >> 2)
           | (src[3] >> 7));
}

// FIXME: this routine is lossy and must be removed.
template<> FORCE_INLINE void
pack<RGBA5551, Unpremultiply, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    *dst = ( ((srcR & 0xF8) << 8)
           | ((srcG & 0xF8) << 3)
           | ((srcB & 0xF8) >> 2)
           | (src[3] >> 7));
}

template<> FORCE_INLINE void
pack<RGB565, NoPremultiplicationOp, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    *dst = ( ((src[0] & 0xF8) << 8)
           | ((src[1] & 0xFC) << 3)
           | ((src[2] & 0xF8) >> 3));
}

template<> FORCE_INLINE void
pack<RGB565, Premultiply, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    float scaleFactor = src[3] / 255.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    *dst = ( ((srcR & 0xF8) << 8)
           | ((srcG & 0xFC) << 3)
           | ((srcB & 0xF8) >> 3));
}

// FIXME: this routine is lossy and must be removed.
template<> FORCE_INLINE void
pack<RGB565, Unpremultiply, uint8_t, uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst)
{
    float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
    uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
    uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
    uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
    *dst = ( ((srcR & 0xF8) << 8)
           | ((srcG & 0xFC) << 3)
           | ((srcB & 0xF8) >> 3));
}

template<> FORCE_INLINE void
pack<RGB32F, NoPremultiplicationOp, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

template<> FORCE_INLINE void
pack<RGB32F, Premultiply, float, float>(const float* __restrict src, float* __restrict dst)
{
    float scaleFactor = src[3];
    dst[0] = src[0] * scaleFactor;
    dst[1] = src[1] * scaleFactor;
    dst[2] = src[2] * scaleFactor;
}

template<> FORCE_INLINE void
pack<RGBA32F, NoPremultiplicationOp, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

template<> FORCE_INLINE void
pack<RGBA32F, Premultiply, float, float>(const float* __restrict src, float* __restrict dst)
{
    float scaleFactor = src[3];
    dst[0] = src[0] * scaleFactor;
    dst[1] = src[1] * scaleFactor;
    dst[2] = src[2] * scaleFactor;
    dst[3] = src[3];
}

template<> FORCE_INLINE void
pack<A32F, NoPremultiplicationOp, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[3];
}

template<> FORCE_INLINE void
pack<A32F, Premultiply, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[3];
}

template<> FORCE_INLINE void
pack<R32F, NoPremultiplicationOp, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
}

template<> FORCE_INLINE void
pack<R32F, Premultiply, float, float>(const float* __restrict src, float* __restrict dst)
{
    float scaleFactor = src[3];
    dst[0] = src[0] * scaleFactor;
}

template<> FORCE_INLINE void
pack<RA32F, NoPremultiplicationOp, float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[3];
}

template<> FORCE_INLINE void
pack<RA32F, Premultiply, float, float>(const float* __restrict src, float* __restrict dst)
{
    float scaleFactor = src[3];
    dst[0] = src[0] * scaleFactor;
    dst[1] = scaleFactor;
}

/****** END CODE SHARED WITH WEBKIT ******/

template<typename SrcType, typename DstType> FORCE_INLINE void
convertType(const SrcType* __restrict src, DstType* __restrict dst)
{
    NS_ABORT_IF_FALSE(false, "Unimplemented texture format conversion");
}

template<> FORCE_INLINE void
convertType<uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

template<> FORCE_INLINE void
convertType<float, float>(const float* __restrict src, float* __restrict dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

template<> FORCE_INLINE void
convertType<uint8_t, float>(const uint8_t* __restrict src, float* __restrict dst)
{
    const float scaleFactor = 1.f / 255.0f;
    dst[0] = src[0] * scaleFactor;
    dst[1] = src[1] * scaleFactor;
    dst[2] = src[2] * scaleFactor;
    dst[3] = src[3] * scaleFactor;
}

#undef FORCE_INLINE

} // end namespace WebGLTexelConversions

} // end namespace mozilla

#endif // WEBGLTEXELCONVERSIONS_H_
