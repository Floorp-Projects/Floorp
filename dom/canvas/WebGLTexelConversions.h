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
#  define WEBGLTEXELCONVERSIONS_H_

#  ifdef __SUNPRO_CC
#    define __restrict
#  endif

#  include "WebGLTypes.h"
#  include <stdint.h>
#  include "mozilla/Attributes.h"
#  include "mozilla/Casting.h"

namespace mozilla {

bool ConvertImage(size_t width, size_t height, const void* srcBegin,
                  size_t srcStride, gl::OriginPos srcOrigin,
                  WebGLTexelFormat srcFormat, bool srcPremultiplied,
                  void* dstBegin, size_t dstStride, gl::OriginPos dstOrigin,
                  WebGLTexelFormat dstFormat, bool dstPremultiplied,
                  bool* out_wasTrivial);

//////////////////////////////////////////////////////////////////////////////////////////

// single precision float
// seeeeeeeemmmmmmmmmmmmmmmmmmmmmmm

// half precision float
// seeeeemmmmmmmmmm

// IEEE 16bits floating point:
const uint16_t kFloat16Value_Zero = 0x0000;      // = 0000000000000000b
const uint16_t kFloat16Value_One = 0x3C00;       // = 0011110000000000b
const uint16_t kFloat16Value_Infinity = 0x7C00;  // = 0111110000000000b
const uint16_t kFloat16Value_NaN = 0x7FFF;  // = 011111yyyyyyyyyyb (nonzero y)

MOZ_ALWAYS_INLINE uint16_t packToFloat16(float v) {
  union {
    float f32Value;
    uint32_t f32Bits;
  };

  f32Value = v;

  // pull the sign from v into f16bits
  uint16_t f16Bits = uint16_t(f32Bits >> 16) & 0x8000;
  const uint32_t mantissa = f32Bits & 0x7FFFFF;
  const uint32_t exp = (f32Bits >> 23) & 0xFF;

  // Adapted from: OpenGL ES 2.0 Programming Guide Appx.
  // Converting Float to Half-Float
  // 143 = 255 - 127 + 15
  //     = sp_max - sp_bias + hp_bias
  if (exp >= 143) {
    if (mantissa && exp == 0xFF) {
      // Single precision was NaN
      return f16Bits | kFloat16Value_NaN;
    } else {
      // Outside range, store as infinity
      return f16Bits | kFloat16Value_Infinity;
    }
  }

  // too small, try to make a denormalized number
  // 112 = 255 - 127 - (15 + 1)
  //     = sp_max - sp_bias - (hp_bias + 1)
  if (exp <= 112) {
    return f16Bits | uint16_t(mantissa >> (14 + 112 - exp));
  }

  f16Bits |= uint16_t(exp - 112) << 10;
  f16Bits |= uint16_t(mantissa >> 13) & 0x03FF;

  return f16Bits;
}

MOZ_ALWAYS_INLINE float unpackFromFloat16(uint16_t v) {
  union {
    float f32Value;
    uint32_t f32Bits;
  };

  // grab sign bit
  f32Bits = uint32_t(v & 0x8000) << 16;
  uint16_t exp = (v >> 10) & 0x001F;
  uint16_t mantissa = v & 0x03FF;

  if (!exp) {
    // Handle denormalized numbers
    // Adapted from: OpenGL ES 2.0 Programming Guide Appx.
    // Converting Float to Half-Float
    if (mantissa) {
      exp = 112;  // See packToFloat16
      mantissa <<= 1;
      // For every leading zero, decrement the exponent
      // and shift the mantissa to the left
      while ((mantissa & (1 << 10)) == 0) {
        mantissa <<= 1;
        --exp;
      }
      mantissa &= 0x03FF;

      f32Bits |= (exp << 23) | (mantissa << 13);

      // Denormalized number
      return f32Value;
    }

    // +/- zero
    return f32Value;
  }

  if (exp == 0x001F) {
    if (v & 0x03FF) {
      // this is a NaN
      f32Bits |= 0x7FFFFFFF;
    } else {
      // this is -inf or +inf
      f32Bits |= 0x7F800000;
    }
    return f32Value;
  }

  f32Bits |= uint32_t(exp + (-15 + 127)) << 23;
  f32Bits |= uint32_t(v & 0x03FF) << 13;

  return f32Value;
}

// These routines come from angle/common/mathutil.h
// They are copied here to remove the dependency on ANGLE headers
// included from mathutil.h
MOZ_ALWAYS_INLINE uint16_t packToFloat11(float fp32) {
  const unsigned int float32MantissaMask = 0x7FFFFF;
  const unsigned int float32ExponentMask = 0x7F800000;
  const unsigned int float32SignMask = 0x80000000;
  const unsigned int float32ValueMask = ~float32SignMask;
  const unsigned int float32ExponentFirstBit = 23;
  const unsigned int float32ExponentBias = 127;

  const unsigned short float11Max = 0x7BF;
  const unsigned short float11MantissaMask = 0x3F;
  const unsigned short float11ExponentMask = 0x7C0;
  const unsigned short float11BitMask = 0x7FF;
  const unsigned int float11ExponentBias = 14;

  const unsigned int float32Maxfloat11 = 0x477E0000;
  const unsigned int float32Minfloat11 = 0x38800000;

  const unsigned int float32Bits = BitwiseCast<unsigned int>(fp32);
  const bool float32Sign = (float32Bits & float32SignMask) == float32SignMask;

  unsigned int float32Val = float32Bits & float32ValueMask;

  if ((float32Val & float32ExponentMask) == float32ExponentMask) {
    // INF or NAN
    if ((float32Val & float32MantissaMask) != 0) {
      return float11ExponentMask | (((float32Val >> 17) | (float32Val >> 11) |
                                     (float32Val >> 6) | (float32Val)) &
                                    float11MantissaMask);
    } else if (float32Sign) {
      // -INF is clamped to 0 since float11 is positive only
      return 0;
    } else {
      return float11ExponentMask;
    }
  } else if (float32Sign) {
    // float11 is positive only, so clamp to zero
    return 0;
  } else if (float32Val > float32Maxfloat11) {
    // The number is too large to be represented as a float11, set to max
    return float11Max;
  } else {
    if (float32Val < float32Minfloat11) {
      // The number is too small to be represented as a normalized float11
      // Convert it to a denormalized value.
      const unsigned int shift = (float32ExponentBias - float11ExponentBias) -
                                 (float32Val >> float32ExponentFirstBit);
      float32Val = ((1 << float32ExponentFirstBit) |
                    (float32Val & float32MantissaMask)) >>
                   shift;
    } else {
      // Rebias the exponent to represent the value as a normalized float11
      float32Val += 0xC8000000;
    }

    return ((float32Val + 0xFFFF + ((float32Val >> 17) & 1)) >> 17) &
           float11BitMask;
  }
}

MOZ_ALWAYS_INLINE uint16_t packToFloat10(float fp32) {
  const unsigned int float32MantissaMask = 0x7FFFFF;
  const unsigned int float32ExponentMask = 0x7F800000;
  const unsigned int float32SignMask = 0x80000000;
  const unsigned int float32ValueMask = ~float32SignMask;
  const unsigned int float32ExponentFirstBit = 23;
  const unsigned int float32ExponentBias = 127;

  const unsigned short float10Max = 0x3DF;
  const unsigned short float10MantissaMask = 0x1F;
  const unsigned short float10ExponentMask = 0x3E0;
  const unsigned short float10BitMask = 0x3FF;
  const unsigned int float10ExponentBias = 14;

  const unsigned int float32Maxfloat10 = 0x477C0000;
  const unsigned int float32Minfloat10 = 0x38800000;

  const unsigned int float32Bits = BitwiseCast<unsigned int>(fp32);
  const bool float32Sign = (float32Bits & float32SignMask) == float32SignMask;

  unsigned int float32Val = float32Bits & float32ValueMask;

  if ((float32Val & float32ExponentMask) == float32ExponentMask) {
    // INF or NAN
    if ((float32Val & float32MantissaMask) != 0) {
      return float10ExponentMask | (((float32Val >> 18) | (float32Val >> 13) |
                                     (float32Val >> 3) | (float32Val)) &
                                    float10MantissaMask);
    } else if (float32Sign) {
      // -INF is clamped to 0 since float11 is positive only
      return 0;
    } else {
      return float10ExponentMask;
    }
  } else if (float32Sign) {
    // float10 is positive only, so clamp to zero
    return 0;
  } else if (float32Val > float32Maxfloat10) {
    // The number is too large to be represented as a float11, set to max
    return float10Max;
  } else {
    if (float32Val < float32Minfloat10) {
      // The number is too small to be represented as a normalized float11
      // Convert it to a denormalized value.
      const unsigned int shift = (float32ExponentBias - float10ExponentBias) -
                                 (float32Val >> float32ExponentFirstBit);
      float32Val = ((1 << float32ExponentFirstBit) |
                    (float32Val & float32MantissaMask)) >>
                   shift;
    } else {
      // Rebias the exponent to represent the value as a normalized float11
      float32Val += 0xC8000000;
    }

    return ((float32Val + 0x1FFFF + ((float32Val >> 18) & 1)) >> 18) &
           float10BitMask;
  }
}

enum class WebGLTexelPremultiplicationOp : int {
  None,
  Premultiply,
  Unpremultiply
};

namespace WebGLTexelConversions {

template <WebGLTexelFormat Format>
struct IsFloatFormat {
  static const bool Value =
      Format == WebGLTexelFormat::A32F || Format == WebGLTexelFormat::R32F ||
      Format == WebGLTexelFormat::RA32F || Format == WebGLTexelFormat::RG32F ||
      Format == WebGLTexelFormat::RGB11F11F10F ||
      Format == WebGLTexelFormat::RGB32F || Format == WebGLTexelFormat::RGBA32F;
};

template <WebGLTexelFormat Format>
struct IsHalfFloatFormat {
  static const bool Value =
      Format == WebGLTexelFormat::A16F || Format == WebGLTexelFormat::R16F ||
      Format == WebGLTexelFormat::RA16F || Format == WebGLTexelFormat::RG16F ||
      Format == WebGLTexelFormat::RGB16F || Format == WebGLTexelFormat::RGBA16F;
};

template <WebGLTexelFormat Format>
struct Is16bppFormat {
  static const bool Value = Format == WebGLTexelFormat::RGB565 ||
                            Format == WebGLTexelFormat::RGBA4444 ||
                            Format == WebGLTexelFormat::RGBA5551;
};

template <WebGLTexelFormat Format, bool IsFloat = IsFloatFormat<Format>::Value,
          bool Is16bpp = Is16bppFormat<Format>::Value,
          bool IsHalfFloat = IsHalfFloatFormat<Format>::Value>
struct DataTypeForFormat {
  typedef uint8_t Type;
};

template <WebGLTexelFormat Format>
struct DataTypeForFormat<Format, true, false, false> {
  typedef float Type;
};

template <WebGLTexelFormat Format>
struct DataTypeForFormat<Format, false, true, false> {
  typedef uint16_t Type;
};

template <WebGLTexelFormat Format>
struct DataTypeForFormat<Format, false, false, true> {
  typedef uint16_t Type;
};

template <>
struct DataTypeForFormat<WebGLTexelFormat::RGB11F11F10F, true, false, false> {
  typedef uint32_t Type;
};

template <WebGLTexelFormat Format>
struct IntermediateFormat {
  static const WebGLTexelFormat Value = IsFloatFormat<Format>::Value
                                            ? WebGLTexelFormat::RGBA32F
                                            : IsHalfFloatFormat<Format>::Value
                                                  ? WebGLTexelFormat::RGBA16F
                                                  : WebGLTexelFormat::RGBA8;
};

inline size_t TexelBytesForFormat(WebGLTexelFormat format) {
  switch (format) {
    case WebGLTexelFormat::A8:
    case WebGLTexelFormat::R8:
      return 1;
    case WebGLTexelFormat::A16F:
    case WebGLTexelFormat::R16F:
    case WebGLTexelFormat::RA8:
    case WebGLTexelFormat::RG8:
    case WebGLTexelFormat::RGB565:
    case WebGLTexelFormat::RGBA4444:
    case WebGLTexelFormat::RGBA5551:
      return 2;
    case WebGLTexelFormat::RGB8:
      return 3;
    case WebGLTexelFormat::A32F:
    case WebGLTexelFormat::R32F:
    case WebGLTexelFormat::RA16F:
    case WebGLTexelFormat::RG16F:
    case WebGLTexelFormat::RGB11F11F10F:
    case WebGLTexelFormat::RGBA8:
    case WebGLTexelFormat::BGRX8:
    case WebGLTexelFormat::BGRA8:
      return 4;
    case WebGLTexelFormat::RGB16F:
      return 6;
    case WebGLTexelFormat::RA32F:
    case WebGLTexelFormat::RG32F:
    case WebGLTexelFormat::RGBA16F:
      return 8;
    case WebGLTexelFormat::RGB32F:
      return 12;
    case WebGLTexelFormat::RGBA32F:
      return 16;
    default:
      MOZ_ASSERT(false, "Unknown texel format. Coding mistake?");
      return 0;
  }
}

MOZ_ALWAYS_INLINE bool HasAlpha(WebGLTexelFormat format) {
  return (
      format == WebGLTexelFormat::A8 || format == WebGLTexelFormat::A16F ||
      format == WebGLTexelFormat::A32F || format == WebGLTexelFormat::RA8 ||
      format == WebGLTexelFormat::RA16F || format == WebGLTexelFormat::RA32F ||
      format == WebGLTexelFormat::RGBA4444 ||
      format == WebGLTexelFormat::RGBA5551 ||
      format == WebGLTexelFormat::RGBA8 ||
      format == WebGLTexelFormat::RGBA16F ||
      format == WebGLTexelFormat::RGBA32F || format == WebGLTexelFormat::BGRA8);
}

MOZ_ALWAYS_INLINE bool HasColor(WebGLTexelFormat format) {
  return (
      format == WebGLTexelFormat::R8 || format == WebGLTexelFormat::R16F ||
      format == WebGLTexelFormat::R32F || format == WebGLTexelFormat::RA8 ||
      format == WebGLTexelFormat::RA16F || format == WebGLTexelFormat::RA32F ||
      format == WebGLTexelFormat::RG8 || format == WebGLTexelFormat::RG16F ||
      format == WebGLTexelFormat::RG32F || format == WebGLTexelFormat::RGB565 ||
      format == WebGLTexelFormat::RGB8 ||
      format == WebGLTexelFormat::RGB11F11F10F ||
      format == WebGLTexelFormat::RGB16F ||
      format == WebGLTexelFormat::RGB32F ||
      format == WebGLTexelFormat::RGBA4444 ||
      format == WebGLTexelFormat::RGBA5551 ||
      format == WebGLTexelFormat::RGBA8 ||
      format == WebGLTexelFormat::RGBA16F ||
      format == WebGLTexelFormat::RGBA32F ||
      format == WebGLTexelFormat::BGRX8 || format == WebGLTexelFormat::BGRA8);
}

/****** BEGIN CODE SHARED WITH WEBKIT ******/

// the pack/unpack functions here are originally from this file:
//   http://trac.webkit.org/browser/trunk/WebCore/platform/graphics/GraphicsContext3D.cpp

//----------------------------------------------------------------------
// Pixel unpacking routines.

template <WebGLTexelFormat Format, typename SrcType, typename DstType>
MOZ_ALWAYS_INLINE void unpack(const SrcType* __restrict src,
                              DstType* __restrict dst) {
  MOZ_ASSERT(false, "Unimplemented texture format conversion");
}

////////////////////////////////////////////////////////////////////////////////
// 1-channel formats
template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::A8, uint8_t, uint8_t>(
    const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = 0;
  dst[1] = 0;
  dst[2] = 0;
  dst[3] = src[0];
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::A16F, uint16_t, uint16_t>(
    const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = kFloat16Value_Zero;
  dst[1] = kFloat16Value_Zero;
  dst[2] = kFloat16Value_Zero;
  dst[3] = src[0];
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::A32F, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = 0;
  dst[1] = 0;
  dst[2] = 0;
  dst[3] = src[0];
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::R8, uint8_t, uint8_t>(
    const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[0];
  dst[2] = src[0];
  dst[3] = 0xFF;
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::R16F, uint16_t, uint16_t>(
    const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[0];
  dst[2] = src[0];
  dst[3] = kFloat16Value_One;
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::R32F, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[0];
  dst[2] = src[0];
  dst[3] = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////
// 2-channel formats
template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RA8, uint8_t, uint8_t>(
    const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[0];
  dst[2] = src[0];
  dst[3] = src[1];
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RA16F, uint16_t, uint16_t>(
    const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[0];
  dst[2] = src[0];
  dst[3] = src[1];
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RA32F, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[0];
  dst[2] = src[0];
  dst[3] = src[1];
}

////////////////////////////////////////////////////////////////////////////////
// 3-channel formats
template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGB565, uint16_t, uint8_t>(
    const uint16_t* __restrict src, uint8_t* __restrict dst) {
  uint16_t packedValue = src[0];
  uint8_t r = (packedValue >> 11) & 0x1F;
  uint8_t g = (packedValue >> 5) & 0x3F;
  uint8_t b = packedValue & 0x1F;
  dst[0] = uint8_t(r << 3) | (r & 0x7);
  dst[1] = uint8_t(g << 2) | (g & 0x3);
  dst[2] = uint8_t(b << 3) | (b & 0x7);
  dst[3] = 0xFF;
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGB8, uint8_t, uint8_t>(
    const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = 0xFF;
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGB16F, uint16_t, uint16_t>(
    const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = kFloat16Value_One;
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGB32F, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////
// 4-channel formats
template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGBA4444, uint16_t, uint8_t>(
    const uint16_t* __restrict src, uint8_t* __restrict dst) {
  uint16_t packedValue = src[0];
  uint8_t r = (packedValue >> 12) & 0x0F;
  uint8_t g = (packedValue >> 8) & 0x0F;
  uint8_t b = (packedValue >> 4) & 0x0F;
  uint8_t a = packedValue & 0x0F;
  dst[0] = uint8_t(r << 4) | r;
  dst[1] = uint8_t(g << 4) | g;
  dst[2] = uint8_t(b << 4) | b;
  dst[3] = uint8_t(a << 4) | a;
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGBA5551, uint16_t, uint8_t>(
    const uint16_t* __restrict src, uint8_t* __restrict dst) {
  uint16_t packedValue = src[0];
  uint8_t r = (packedValue >> 11) & 0x1F;
  uint8_t g = (packedValue >> 6) & 0x1F;
  uint8_t b = (packedValue >> 1) & 0x1F;
  dst[0] = uint8_t(r << 3) | (r & 0x7);
  dst[1] = uint8_t(g << 3) | (g & 0x7);
  dst[2] = uint8_t(b << 3) | (b & 0x7);
  dst[3] = (packedValue & 0x1) ? 0xFF : 0;
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGBA8, uint8_t, uint8_t>(
    const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGBA16F, uint16_t, uint16_t>(
    const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::RGBA32F, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

////////////////////////////////////////////////////////////////////////////////
// DOM element formats
template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::BGRX8, uint8_t, uint8_t>(
    const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[2];
  dst[1] = src[1];
  dst[2] = src[0];
  dst[3] = 0xFF;
}

template <>
MOZ_ALWAYS_INLINE void unpack<WebGLTexelFormat::BGRA8, uint8_t, uint8_t>(
    const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[2];
  dst[1] = src[1];
  dst[2] = src[0];
  dst[3] = src[3];
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

template <WebGLTexelFormat Format,
          WebGLTexelPremultiplicationOp PremultiplicationOp, typename SrcType,
          typename DstType>
MOZ_ALWAYS_INLINE void pack(const SrcType* __restrict src,
                            DstType* __restrict dst) {
  MOZ_CRASH("GFX: Unimplemented texture format conversion");
}

////////////////////////////////////////////////////////////////////////////////
// 1-channel formats
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A8, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A8, WebGLTexelPremultiplicationOp::Premultiply, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A8, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A16F, WebGLTexelPremultiplicationOp::None, uint16_t,
     uint16_t>(const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A16F, WebGLTexelPremultiplicationOp::Premultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A16F, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A32F, WebGLTexelPremultiplicationOp::None, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A32F, WebGLTexelPremultiplicationOp::Premultiply, float,
     float>(const float* __restrict src, float* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::A32F, WebGLTexelPremultiplicationOp::Unpremultiply,
     float, float>(const float* __restrict src, float* __restrict dst) {
  dst[0] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R8, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R8, WebGLTexelPremultiplicationOp::Premultiply, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] / 255.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  dst[0] = srcR;
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R8, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  dst[0] = srcR;
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R16F, WebGLTexelPremultiplicationOp::None, uint16_t,
     uint16_t>(const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R16F, WebGLTexelPremultiplicationOp::Premultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float scaleFactor = unpackFromFloat16(src[3]);
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R16F, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float unpackedAlpha = unpackFromFloat16(src[3]);
  float scaleFactor = unpackedAlpha ? 1.0f / unpackedAlpha : 1.0f;
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R32F, WebGLTexelPremultiplicationOp::None, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R32F, WebGLTexelPremultiplicationOp::Premultiply, float,
     float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3];
  dst[0] = src[0] * scaleFactor;
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::R32F, WebGLTexelPremultiplicationOp::Unpremultiply,
     float, float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3] ? 1.0f / src[3] : 1.0f;
  dst[0] = src[0] * scaleFactor;
}

////////////////////////////////////////////////////////////////////////////////
// 2-channel formats
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RA8, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RA8, WebGLTexelPremultiplicationOp::Premultiply, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] / 255.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  dst[0] = srcR;
  dst[1] = src[3];
}

// FIXME: this routine is lossy and must be removed.
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RA8, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  dst[0] = srcR;
  dst[1] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RA16F, WebGLTexelPremultiplicationOp::None, uint16_t,
     uint16_t>(const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RA16F, WebGLTexelPremultiplicationOp::Premultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float scaleFactor = unpackFromFloat16(src[3]);
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
  dst[1] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RA16F, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float unpackedAlpha = unpackFromFloat16(src[3]);
  float scaleFactor = unpackedAlpha ? 1.0f / unpackedAlpha : 1.0f;
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
  dst[1] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void pack<WebGLTexelFormat::RA32F,
                            WebGLTexelPremultiplicationOp::None, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RA32F, WebGLTexelPremultiplicationOp::Premultiply, float,
     float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3];
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RA32F, WebGLTexelPremultiplicationOp::Unpremultiply,
     float, float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3] ? 1.0f / src[3] : 1.0f;
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RG8, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RG8, WebGLTexelPremultiplicationOp::Premultiply, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] / 255.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  dst[0] = srcR;
  dst[1] = srcG;
}

// FIXME: this routine is lossy and must be removed.
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RG8, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  dst[0] = srcR;
  dst[1] = srcG;
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RG16F, WebGLTexelPremultiplicationOp::None, uint16_t,
     uint16_t>(const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RG16F, WebGLTexelPremultiplicationOp::Premultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float scaleFactor = unpackFromFloat16(src[3]);
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
  dst[1] = packToFloat16(unpackFromFloat16(src[1]) * scaleFactor);
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RG16F, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float unpackedAlpha = unpackFromFloat16(src[3]);
  float scaleFactor = unpackedAlpha ? 1.0f / unpackedAlpha : 1.0f;
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
  dst[1] = packToFloat16(unpackFromFloat16(src[1]) * scaleFactor);
}

template <>
MOZ_ALWAYS_INLINE void pack<WebGLTexelFormat::RG32F,
                            WebGLTexelPremultiplicationOp::None, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RG32F, WebGLTexelPremultiplicationOp::Premultiply, float,
     float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3];
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[1] * scaleFactor;
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RG32F, WebGLTexelPremultiplicationOp::Unpremultiply,
     float, float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3] ? 1.0f / src[3] : 1.0f;
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[1] * scaleFactor;
}

////////////////////////////////////////////////////////////////////////////////
// 3-channel formats
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB565, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst) {
  *dst = uint16_t(((src[0] & 0xF8) << 8) | ((src[1] & 0xFC) << 3) |
                  ((src[2] & 0xF8) >> 3));
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB565, WebGLTexelPremultiplicationOp::Premultiply,
     uint8_t, uint16_t>(const uint8_t* __restrict src,
                        uint16_t* __restrict dst) {
  float scaleFactor = src[3] / 255.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  *dst = uint16_t(((srcR & 0xF8) << 8) | ((srcG & 0xFC) << 3) |
                  ((srcB & 0xF8) >> 3));
}

// FIXME: this routine is lossy and must be removed.
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB565, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint16_t>(const uint8_t* __restrict src,
                        uint16_t* __restrict dst) {
  float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  *dst = uint16_t(((srcR & 0xF8) << 8) | ((srcG & 0xFC) << 3) |
                  ((srcB & 0xF8) >> 3));
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB8, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB8, WebGLTexelPremultiplicationOp::Premultiply,
     uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] / 255.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  dst[0] = srcR;
  dst[1] = srcG;
  dst[2] = srcB;
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB8, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  dst[0] = srcR;
  dst[1] = srcG;
  dst[2] = srcB;
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB11F11F10F, WebGLTexelPremultiplicationOp::None, float,
     uint32_t>(const float* __restrict src, uint32_t* __restrict dst) {
  dst[0] = ((packToFloat11(src[0]) << 0) | (packToFloat11(src[1]) << 11) |
            (packToFloat10(src[2]) << 22));
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB11F11F10F, WebGLTexelPremultiplicationOp::Premultiply,
     float, uint32_t>(const float* __restrict src, uint32_t* __restrict dst) {
  float scaleFactor = src[3];
  dst[0] = ((packToFloat11(src[0] * scaleFactor) << 0) |
            (packToFloat11(src[1] * scaleFactor) << 11) |
            (packToFloat10(src[2] * scaleFactor) << 22));
}

template <>
MOZ_ALWAYS_INLINE void pack<WebGLTexelFormat::RGB11F11F10F,
                            WebGLTexelPremultiplicationOp::Unpremultiply, float,
                            uint32_t>(const float* __restrict src,
                                      uint32_t* __restrict dst) {
  float scaleFactor = src[3] ? 1.0f / src[3] : 1.0f;
  dst[0] = ((packToFloat11(src[0] * scaleFactor) << 0) |
            (packToFloat11(src[1] * scaleFactor) << 11) |
            (packToFloat10(src[2] * scaleFactor) << 22));
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB16F, WebGLTexelPremultiplicationOp::None, uint16_t,
     uint16_t>(const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB16F, WebGLTexelPremultiplicationOp::Premultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float scaleFactor = unpackFromFloat16(src[3]);
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
  dst[1] = packToFloat16(unpackFromFloat16(src[1]) * scaleFactor);
  dst[2] = packToFloat16(unpackFromFloat16(src[2]) * scaleFactor);
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB16F, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float unpackedAlpha = unpackFromFloat16(src[3]);
  float scaleFactor = unpackedAlpha ? 1.0f / unpackedAlpha : 1.0f;
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
  dst[1] = packToFloat16(unpackFromFloat16(src[1]) * scaleFactor);
  dst[2] = packToFloat16(unpackFromFloat16(src[2]) * scaleFactor);
}

template <>
MOZ_ALWAYS_INLINE void pack<WebGLTexelFormat::RGB32F,
                            WebGLTexelPremultiplicationOp::None, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB32F, WebGLTexelPremultiplicationOp::Premultiply,
     float, float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3];
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[1] * scaleFactor;
  dst[2] = src[2] * scaleFactor;
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGB32F, WebGLTexelPremultiplicationOp::Unpremultiply,
     float, float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3] ? 1.0f / src[3] : 1.0f;
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[1] * scaleFactor;
  dst[2] = src[2] * scaleFactor;
}

////////////////////////////////////////////////////////////////////////////////
// 4-channel formats
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA4444, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst) {
  *dst = uint16_t(((src[0] & 0xF0) << 8) | ((src[1] & 0xF0) << 4) |
                  (src[2] & 0xF0) | (src[3] >> 4));
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA4444, WebGLTexelPremultiplicationOp::Premultiply,
     uint8_t, uint16_t>(const uint8_t* __restrict src,
                        uint16_t* __restrict dst) {
  float scaleFactor = src[3] / 255.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  *dst = uint16_t(((srcR & 0xF0) << 8) | ((srcG & 0xF0) << 4) | (srcB & 0xF0) |
                  (src[3] >> 4));
}

// FIXME: this routine is lossy and must be removed.
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA4444, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint16_t>(const uint8_t* __restrict src,
                        uint16_t* __restrict dst) {
  float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  *dst = uint16_t(((srcR & 0xF0) << 8) | ((srcG & 0xF0) << 4) | (srcB & 0xF0) |
                  (src[3] >> 4));
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA5551, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint16_t>(const uint8_t* __restrict src, uint16_t* __restrict dst) {
  *dst = uint16_t(((src[0] & 0xF8) << 8) | ((src[1] & 0xF8) << 3) |
                  ((src[2] & 0xF8) >> 2) | (src[3] >> 7));
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA5551, WebGLTexelPremultiplicationOp::Premultiply,
     uint8_t, uint16_t>(const uint8_t* __restrict src,
                        uint16_t* __restrict dst) {
  float scaleFactor = src[3] / 255.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  *dst = uint16_t(((srcR & 0xF8) << 8) | ((srcG & 0xF8) << 3) |
                  ((srcB & 0xF8) >> 2) | (src[3] >> 7));
}

// FIXME: this routine is lossy and must be removed.
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA5551, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint16_t>(const uint8_t* __restrict src,
                        uint16_t* __restrict dst) {
  float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  *dst = uint16_t(((srcR & 0xF8) << 8) | ((srcG & 0xF8) << 3) |
                  ((srcB & 0xF8) >> 2) | (src[3] >> 7));
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA8, WebGLTexelPremultiplicationOp::None, uint8_t,
     uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA8, WebGLTexelPremultiplicationOp::Premultiply,
     uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
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
template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA8, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint8_t, uint8_t>(const uint8_t* __restrict src, uint8_t* __restrict dst) {
  float scaleFactor = src[3] ? 255.0f / src[3] : 1.0f;
  uint8_t srcR = static_cast<uint8_t>(src[0] * scaleFactor);
  uint8_t srcG = static_cast<uint8_t>(src[1] * scaleFactor);
  uint8_t srcB = static_cast<uint8_t>(src[2] * scaleFactor);
  dst[0] = srcR;
  dst[1] = srcG;
  dst[2] = srcB;
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA16F, WebGLTexelPremultiplicationOp::None, uint16_t,
     uint16_t>(const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA16F, WebGLTexelPremultiplicationOp::Premultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float scaleFactor = unpackFromFloat16(src[3]);
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
  dst[1] = packToFloat16(unpackFromFloat16(src[1]) * scaleFactor);
  dst[2] = packToFloat16(unpackFromFloat16(src[2]) * scaleFactor);
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA16F, WebGLTexelPremultiplicationOp::Unpremultiply,
     uint16_t, uint16_t>(const uint16_t* __restrict src,
                         uint16_t* __restrict dst) {
  float unpackedAlpha = unpackFromFloat16(src[3]);
  float scaleFactor = unpackedAlpha ? 1.0f / unpackedAlpha : 1.0f;
  dst[0] = packToFloat16(unpackFromFloat16(src[0]) * scaleFactor);
  dst[1] = packToFloat16(unpackFromFloat16(src[1]) * scaleFactor);
  dst[2] = packToFloat16(unpackFromFloat16(src[2]) * scaleFactor);
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void pack<WebGLTexelFormat::RGBA32F,
                            WebGLTexelPremultiplicationOp::None, float, float>(
    const float* __restrict src, float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA32F, WebGLTexelPremultiplicationOp::Premultiply,
     float, float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3];
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[1] * scaleFactor;
  dst[2] = src[2] * scaleFactor;
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void
pack<WebGLTexelFormat::RGBA32F, WebGLTexelPremultiplicationOp::Unpremultiply,
     float, float>(const float* __restrict src, float* __restrict dst) {
  float scaleFactor = src[3] ? 1.0f / src[3] : 1.0f;
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[1] * scaleFactor;
  dst[2] = src[2] * scaleFactor;
  dst[3] = src[3];
}

/****** END CODE SHARED WITH WEBKIT ******/

template <typename SrcType, typename DstType>
MOZ_ALWAYS_INLINE void convertType(const SrcType* __restrict src,
                                   DstType* __restrict dst) {
  MOZ_ASSERT(false, "Unimplemented texture format conversion");
}

template <>
MOZ_ALWAYS_INLINE void convertType<uint8_t, uint8_t>(
    const uint8_t* __restrict src, uint8_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void convertType<uint16_t, uint16_t>(
    const uint16_t* __restrict src, uint16_t* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void convertType<float, float>(const float* __restrict src,
                                                 float* __restrict dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

template <>
MOZ_ALWAYS_INLINE void convertType<uint8_t, float>(
    const uint8_t* __restrict src, float* __restrict dst) {
  const float scaleFactor = 1.f / 255.0f;
  dst[0] = src[0] * scaleFactor;
  dst[1] = src[1] * scaleFactor;
  dst[2] = src[2] * scaleFactor;
  dst[3] = src[3] * scaleFactor;
}

template <>
MOZ_ALWAYS_INLINE void convertType<uint8_t, uint16_t>(
    const uint8_t* __restrict src, uint16_t* __restrict dst) {
  const float scaleFactor = 1.f / 255.0f;
  dst[0] = packToFloat16(src[0] * scaleFactor);
  dst[1] = packToFloat16(src[1] * scaleFactor);
  dst[2] = packToFloat16(src[2] * scaleFactor);
  dst[3] = packToFloat16(src[3] * scaleFactor);
}

}  // end namespace WebGLTexelConversions

}  // end namespace mozilla

#endif  // WEBGLTEXELCONVERSIONS_H_
