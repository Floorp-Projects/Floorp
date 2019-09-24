/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Swizzle.h"
#include "Logging.h"
#include "Tools.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"

#ifdef USE_SSE2
#  include "mozilla/SSE.h"
#endif

#ifdef USE_NEON
#  include "mozilla/arm.h"
#endif

namespace mozilla {
namespace gfx {

/**
 * Convenience macros for dispatching to various format combinations.
 */

// Hash the formats to a relatively dense value to optimize jump table
// generation. The first 6 formats in SurfaceFormat are the 32-bit BGRA variants
// and are the most common formats dispatched here. Room is reserved in the
// lowish bits for up to these 6 destination formats. If a destination format is
// >= 6, the 6th bit is set to avoid collisions.
#define FORMAT_KEY(aSrcFormat, aDstFormat) \
  (int(aSrcFormat) * 6 + int(aDstFormat) + (int(int(aDstFormat) >= 6) << 6))

#define FORMAT_CASE_EXPR(aSrcFormat, aDstFormat, ...) \
  case FORMAT_KEY(aSrcFormat, aDstFormat):            \
    __VA_ARGS__;                                      \
    return true;

#define FORMAT_CASE(aSrcFormat, aDstFormat, ...) \
  FORMAT_CASE_EXPR(aSrcFormat, aDstFormat, FORMAT_CASE_CALL(__VA_ARGS__))

#define FORMAT_CASE_ROW(aSrcFormat, aDstFormat, ...) \
  case FORMAT_KEY(aSrcFormat, aDstFormat):           \
    return &__VA_ARGS__;

/**
 * Constexpr functions for analyzing format attributes in templates.
 */

// Whether B comes before R in pixel memory layout.
static constexpr bool IsBGRFormat(SurfaceFormat aFormat) {
  return aFormat == SurfaceFormat::B8G8R8A8 ||
#if MOZ_LITTLE_ENDIAN
         aFormat == SurfaceFormat::R5G6B5_UINT16 ||
#endif
         aFormat == SurfaceFormat::B8G8R8X8 || aFormat == SurfaceFormat::B8G8R8;
}

// Whether the order of B and R need to be swapped to map from src to dst.
static constexpr bool ShouldSwapRB(SurfaceFormat aSrcFormat,
                                   SurfaceFormat aDstFormat) {
  return IsBGRFormat(aSrcFormat) != IsBGRFormat(aDstFormat);
}

// The starting byte of the RGB components in pixel memory.
static constexpr uint32_t RGBByteIndex(SurfaceFormat aFormat) {
  return aFormat == SurfaceFormat::A8R8G8B8 ||
                 aFormat == SurfaceFormat::X8R8G8B8
             ? 1
             : 0;
}

// The byte of the alpha component, which just comes after RGB.
static constexpr uint32_t AlphaByteIndex(SurfaceFormat aFormat) {
  return (RGBByteIndex(aFormat) + 3) % 4;
}

// The endian-dependent bit shift to access RGB of a UINT32 pixel.
static constexpr uint32_t RGBBitShift(SurfaceFormat aFormat) {
#if MOZ_LITTLE_ENDIAN
  return 8 * RGBByteIndex(aFormat);
#else
  return 8 - 8 * RGBByteIndex(aFormat);
#endif
}

// The endian-dependent bit shift to access alpha of a UINT32 pixel.
static constexpr uint32_t AlphaBitShift(SurfaceFormat aFormat) {
  return (RGBBitShift(aFormat) + 24) % 32;
}

// Whether the pixel format should ignore the value of the alpha channel and
// treat it as opaque.
static constexpr bool IgnoreAlpha(SurfaceFormat aFormat) {
  return aFormat == SurfaceFormat::B8G8R8X8 ||
         aFormat == SurfaceFormat::R8G8B8X8 ||
         aFormat == SurfaceFormat::X8R8G8B8;
}

// Whether to force alpha to opaque to map from src to dst.
static constexpr bool ShouldForceOpaque(SurfaceFormat aSrcFormat,
                                        SurfaceFormat aDstFormat) {
  return IgnoreAlpha(aSrcFormat) != IgnoreAlpha(aDstFormat);
}

#ifdef USE_SSE2
/**
 * SSE2 optimizations
 */

template <bool aSwapRB, bool aOpaqueAlpha>
void Premultiply_SSE2(const uint8_t*, int32_t, uint8_t*, int32_t, IntSize);

#  define PREMULTIPLY_SSE2(aSrcFormat, aDstFormat)                     \
    FORMAT_CASE(aSrcFormat, aDstFormat,                                \
                Premultiply_SSE2<ShouldSwapRB(aSrcFormat, aDstFormat), \
                                 ShouldForceOpaque(aSrcFormat, aDstFormat)>)

template <bool aSwapRB, bool aOpaqueAlpha>
void PremultiplyRow_SSE2(const uint8_t*, uint8_t*, int32_t);

#  define PREMULTIPLY_ROW_SSE2(aSrcFormat, aDstFormat)            \
    FORMAT_CASE_ROW(                                              \
        aSrcFormat, aDstFormat,                                   \
        PremultiplyRow_SSE2<ShouldSwapRB(aSrcFormat, aDstFormat), \
                            ShouldForceOpaque(aSrcFormat, aDstFormat)>)

template <bool aSwapRB>
void Unpremultiply_SSE2(const uint8_t*, int32_t, uint8_t*, int32_t, IntSize);

#  define UNPREMULTIPLY_SSE2(aSrcFormat, aDstFormat) \
    FORMAT_CASE(aSrcFormat, aDstFormat,              \
                Unpremultiply_SSE2<ShouldSwapRB(aSrcFormat, aDstFormat)>)

template <bool aSwapRB, bool aOpaqueAlpha>
void Swizzle_SSE2(const uint8_t*, int32_t, uint8_t*, int32_t, IntSize);

#  define SWIZZLE_SSE2(aSrcFormat, aDstFormat)                     \
    FORMAT_CASE(aSrcFormat, aDstFormat,                            \
                Swizzle_SSE2<ShouldSwapRB(aSrcFormat, aDstFormat), \
                             ShouldForceOpaque(aSrcFormat, aDstFormat)>)

template <bool aSwapRB, bool aOpaqueAlpha>
void SwizzleRow_SSE2(const uint8_t*, uint8_t*, int32_t);

#  define SWIZZLE_ROW_SSE2(aSrcFormat, aDstFormat)            \
    FORMAT_CASE_ROW(                                          \
        aSrcFormat, aDstFormat,                               \
        SwizzleRow_SSE2<ShouldSwapRB(aSrcFormat, aDstFormat), \
                        ShouldForceOpaque(aSrcFormat, aDstFormat)>)

template <bool aSwapRB>
void UnpackRowRGB24_SSSE3(const uint8_t*, uint8_t*, int32_t);

#define UNPACK_ROW_RGB_SSSE3(aDstFormat)             \
  FORMAT_CASE_ROW(SurfaceFormat::R8G8B8, aDstFormat, \
                  UnpackRowRGB24_SSSE3<ShouldSwapRB(SurfaceFormat::R8G8B8, aDstFormat)>)

template <bool aSwapRB>
void UnpackRowRGB24_AVX2(const uint8_t*, uint8_t*, int32_t);

#define UNPACK_ROW_RGB_AVX2(aDstFormat)              \
  FORMAT_CASE_ROW(SurfaceFormat::R8G8B8, aDstFormat, \
                  UnpackRowRGB24_AVX2<ShouldSwapRB(SurfaceFormat::R8G8B8, aDstFormat)>)

#endif

#ifdef USE_NEON
/**
 * ARM NEON optimizations
 */

template <bool aSwapRB, bool aOpaqueAlpha>
void Premultiply_NEON(const uint8_t*, int32_t, uint8_t*, int32_t, IntSize);

#  define PREMULTIPLY_NEON(aSrcFormat, aDstFormat)                     \
    FORMAT_CASE(aSrcFormat, aDstFormat,                                \
                Premultiply_NEON<ShouldSwapRB(aSrcFormat, aDstFormat), \
                                 ShouldForceOpaque(aSrcFormat, aDstFormat)>)

template <bool aSwapRB, bool aOpaqueAlpha>
void PremultiplyRow_NEON(const uint8_t*, uint8_t*, int32_t);

#  define PREMULTIPLY_ROW_NEON(aSrcFormat, aDstFormat)            \
    FORMAT_CASE_ROW(                                              \
        aSrcFormat, aDstFormat,                                   \
        PremultiplyRow_NEON<ShouldSwapRB(aSrcFormat, aDstFormat), \
                            ShouldForceOpaque(aSrcFormat, aDstFormat)>)

template <bool aSwapRB>
void Unpremultiply_NEON(const uint8_t*, int32_t, uint8_t*, int32_t, IntSize);

#  define UNPREMULTIPLY_NEON(aSrcFormat, aDstFormat) \
    FORMAT_CASE(aSrcFormat, aDstFormat,              \
                Unpremultiply_NEON<ShouldSwapRB(aSrcFormat, aDstFormat)>)

template <bool aSwapRB, bool aOpaqueAlpha>
void Swizzle_NEON(const uint8_t*, int32_t, uint8_t*, int32_t, IntSize);

#  define SWIZZLE_NEON(aSrcFormat, aDstFormat)                     \
    FORMAT_CASE(aSrcFormat, aDstFormat,                            \
                Swizzle_NEON<ShouldSwapRB(aSrcFormat, aDstFormat), \
                             ShouldForceOpaque(aSrcFormat, aDstFormat)>)

template <bool aSwapRB, bool aOpaqueAlpha>
void SwizzleRow_NEON(const uint8_t*, uint8_t*, int32_t);

#  define SWIZZLE_ROW_NEON(aSrcFormat, aDstFormat)            \
    FORMAT_CASE_ROW(                                          \
        aSrcFormat, aDstFormat,                               \
        SwizzleRow_NEON<ShouldSwapRB(aSrcFormat, aDstFormat), \
                        ShouldForceOpaque(aSrcFormat, aDstFormat)>)
#endif

/**
 * Premultiplying
 */

// Fallback premultiply implementation that uses splayed pixel math to reduce
// the multiplications used. That is, the R and B components are isolated from
// the G and A components, which then can be multiplied as if they were two
// 2-component vectors. Otherwise, an approximation if divide-by-255 is used
// which is faster than an actual division. These optimizations are also used
// for the SSE2 and NEON implementations.
template <bool aSwapRB, bool aOpaqueAlpha, uint32_t aSrcRGBShift,
          uint32_t aSrcAShift, uint32_t aDstRGBShift, uint32_t aDstAShift>
static void PremultiplyChunkFallback(const uint8_t*& aSrc, uint8_t*& aDst,
                                     int32_t aLength) {
  const uint8_t* end = aSrc + 4 * aLength;
  do {
    // Load and process 1 entire pixel at a time.
    uint32_t color = *reinterpret_cast<const uint32_t*>(aSrc);

    uint32_t a = aSrcAShift ? color >> aSrcAShift : color & 0xFF;

    // Isolate the R and B components.
    uint32_t rb = (color >> aSrcRGBShift) & 0x00FF00FF;
    // Swap the order of R and B if necessary.
    if (aSwapRB) {
      rb = (rb >> 16) | (rb << 16);
    }
    // Approximate the multiply by alpha and divide by 255 which is
    // essentially:
    // c = c*a + 255; c = (c + (c >> 8)) >> 8;
    // However, we omit the final >> 8 to fold it with the final shift into
    // place depending on desired output format.
    rb = rb * a + 0x00FF00FF;
    rb = (rb + ((rb >> 8) & 0x00FF00FF)) & 0xFF00FF00;

    // Use same approximation as above, but G is shifted 8 bits left.
    // Alpha is left out and handled separately.
    uint32_t g = color & (0xFF00 << aSrcRGBShift);
    g = g * a + (0xFF00 << aSrcRGBShift);
    g = (g + (g >> 8)) & (0xFF0000 << aSrcRGBShift);

    // The above math leaves RGB shifted left by 8 bits.
    // Shift them right if required for the output format.
    // then combine them back together to produce output pixel.
    // Add the alpha back on if the output format is not opaque.
    *reinterpret_cast<uint32_t*>(aDst) =
        (rb >> (8 - aDstRGBShift)) | (g >> (8 + aSrcRGBShift - aDstRGBShift)) |
        (aOpaqueAlpha ? 0xFF << aDstAShift : a << aDstAShift);

    aSrc += 4;
    aDst += 4;
  } while (aSrc < end);
}

template <bool aSwapRB, bool aOpaqueAlpha, uint32_t aSrcRGBShift,
          uint32_t aSrcAShift, uint32_t aDstRGBShift, uint32_t aDstAShift>
static void PremultiplyRowFallback(const uint8_t* aSrc, uint8_t* aDst,
                                   int32_t aLength) {
  PremultiplyChunkFallback<aSwapRB, aOpaqueAlpha, aSrcRGBShift, aSrcAShift,
                           aDstRGBShift, aDstAShift>(aSrc, aDst, aLength);
}

template <bool aSwapRB, bool aOpaqueAlpha, uint32_t aSrcRGBShift,
          uint32_t aSrcAShift, uint32_t aDstRGBShift, uint32_t aDstAShift>
static void PremultiplyFallback(const uint8_t* aSrc, int32_t aSrcGap,
                                uint8_t* aDst, int32_t aDstGap, IntSize aSize) {
  for (int32_t height = aSize.height; height > 0; height--) {
    PremultiplyChunkFallback<aSwapRB, aOpaqueAlpha, aSrcRGBShift, aSrcAShift,
                             aDstRGBShift, aDstAShift>(aSrc, aDst, aSize.width);
    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

#define PREMULTIPLY_FALLBACK_CASE(aSrcFormat, aDstFormat)                     \
  FORMAT_CASE(                                                                \
      aSrcFormat, aDstFormat,                                                 \
      PremultiplyFallback<ShouldSwapRB(aSrcFormat, aDstFormat),               \
                          ShouldForceOpaque(aSrcFormat, aDstFormat),          \
                          RGBBitShift(aSrcFormat), AlphaBitShift(aSrcFormat), \
                          RGBBitShift(aDstFormat), AlphaBitShift(aDstFormat)>)

#define PREMULTIPLY_FALLBACK(aSrcFormat)                         \
  PREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::B8G8R8A8) \
  PREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::B8G8R8X8) \
  PREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::R8G8B8A8) \
  PREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::R8G8B8X8) \
  PREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::A8R8G8B8) \
  PREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::X8R8G8B8)

#define PREMULTIPLY_ROW_FALLBACK_CASE(aSrcFormat, aDstFormat)             \
  FORMAT_CASE_ROW(aSrcFormat, aDstFormat,                                 \
                  PremultiplyRowFallback<                                 \
                      ShouldSwapRB(aSrcFormat, aDstFormat),               \
                      ShouldForceOpaque(aSrcFormat, aDstFormat),          \
                      RGBBitShift(aSrcFormat), AlphaBitShift(aSrcFormat), \
                      RGBBitShift(aDstFormat), AlphaBitShift(aDstFormat)>)

#define PREMULTIPLY_ROW_FALLBACK(aSrcFormat)                         \
  PREMULTIPLY_ROW_FALLBACK_CASE(aSrcFormat, SurfaceFormat::B8G8R8A8) \
  PREMULTIPLY_ROW_FALLBACK_CASE(aSrcFormat, SurfaceFormat::B8G8R8X8) \
  PREMULTIPLY_ROW_FALLBACK_CASE(aSrcFormat, SurfaceFormat::R8G8B8A8) \
  PREMULTIPLY_ROW_FALLBACK_CASE(aSrcFormat, SurfaceFormat::R8G8B8X8) \
  PREMULTIPLY_ROW_FALLBACK_CASE(aSrcFormat, SurfaceFormat::A8R8G8B8) \
  PREMULTIPLY_ROW_FALLBACK_CASE(aSrcFormat, SurfaceFormat::X8R8G8B8)

// If rows are tightly packed, and the size of the total area will fit within
// the precision range of a single row, then process all the data as if it was
// a single row.
static inline IntSize CollapseSize(const IntSize& aSize, int32_t aSrcStride,
                                   int32_t aDstStride) {
  if (aSrcStride == aDstStride && (aSrcStride & 3) == 0 &&
      aSrcStride / 4 == aSize.width) {
    CheckedInt32 area = CheckedInt32(aSize.width) * CheckedInt32(aSize.height);
    if (area.isValid()) {
      return IntSize(area.value(), 1);
    }
  }
  return aSize;
}

static inline int32_t GetStrideGap(int32_t aWidth, SurfaceFormat aFormat,
                                   int32_t aStride) {
  CheckedInt32 used = CheckedInt32(aWidth) * BytesPerPixel(aFormat);
  if (!used.isValid() || used.value() < 0) {
    return -1;
  }
  return aStride - used.value();
}

bool PremultiplyData(const uint8_t* aSrc, int32_t aSrcStride,
                     SurfaceFormat aSrcFormat, uint8_t* aDst,
                     int32_t aDstStride, SurfaceFormat aDstFormat,
                     const IntSize& aSize) {
  if (aSize.IsEmpty()) {
    return true;
  }
  IntSize size = CollapseSize(aSize, aSrcStride, aDstStride);
  // Find gap from end of row to the start of the next row.
  int32_t srcGap = GetStrideGap(aSize.width, aSrcFormat, aSrcStride);
  int32_t dstGap = GetStrideGap(aSize.width, aDstFormat, aDstStride);
  MOZ_ASSERT(srcGap >= 0 && dstGap >= 0);
  if (srcGap < 0 || dstGap < 0) {
    return false;
  }

#define FORMAT_CASE_CALL(...) __VA_ARGS__(aSrc, srcGap, aDst, dstGap, size)

#ifdef USE_SSE2
  if (mozilla::supports_sse2()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      PREMULTIPLY_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8)
      PREMULTIPLY_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8X8)
      PREMULTIPLY_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      PREMULTIPLY_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
      PREMULTIPLY_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8A8)
      PREMULTIPLY_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8X8)
      PREMULTIPLY_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      PREMULTIPLY_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
      default:
        break;
    }
#endif

#ifdef USE_NEON
  if (mozilla::supports_neon()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      PREMULTIPLY_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8)
      PREMULTIPLY_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8X8)
      PREMULTIPLY_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      PREMULTIPLY_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
      PREMULTIPLY_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8A8)
      PREMULTIPLY_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8X8)
      PREMULTIPLY_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      PREMULTIPLY_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
      default:
        break;
    }
#endif

  switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
    PREMULTIPLY_FALLBACK(SurfaceFormat::B8G8R8A8)
    PREMULTIPLY_FALLBACK(SurfaceFormat::R8G8B8A8)
    PREMULTIPLY_FALLBACK(SurfaceFormat::A8R8G8B8)
    default:
      break;
  }

#undef FORMAT_CASE_CALL

  MOZ_ASSERT(false, "Unsupported premultiply formats");
  return false;
}

SwizzleRowFn PremultiplyRow(SurfaceFormat aSrcFormat,
                            SurfaceFormat aDstFormat) {
#ifdef USE_SSE2
  if (mozilla::supports_sse2()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      PREMULTIPLY_ROW_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8)
      PREMULTIPLY_ROW_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8X8)
      PREMULTIPLY_ROW_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      PREMULTIPLY_ROW_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
      PREMULTIPLY_ROW_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8A8)
      PREMULTIPLY_ROW_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8X8)
      PREMULTIPLY_ROW_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      PREMULTIPLY_ROW_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
      default:
        break;
    }
#endif

#ifdef USE_NEON
  if (mozilla::supports_neon()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      PREMULTIPLY_ROW_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8)
      PREMULTIPLY_ROW_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8X8)
      PREMULTIPLY_ROW_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      PREMULTIPLY_ROW_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
      PREMULTIPLY_ROW_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8A8)
      PREMULTIPLY_ROW_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8X8)
      PREMULTIPLY_ROW_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      PREMULTIPLY_ROW_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
      default:
        break;
    }
#endif

  switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
    PREMULTIPLY_ROW_FALLBACK(SurfaceFormat::B8G8R8A8)
    PREMULTIPLY_ROW_FALLBACK(SurfaceFormat::R8G8B8A8)
    PREMULTIPLY_ROW_FALLBACK(SurfaceFormat::A8R8G8B8)
    default:
      break;
  }

  MOZ_ASSERT_UNREACHABLE("Unsupported premultiply formats");
  return nullptr;
}

/**
 * Unpremultiplying
 */

// Generate a table of 8.16 fixed-point reciprocals representing 1/alpha.
#define UNPREMULQ(x) (0xFF00FFU / (x))
#define UNPREMULQ_2(x) UNPREMULQ(x), UNPREMULQ((x) + 1)
#define UNPREMULQ_4(x) UNPREMULQ_2(x), UNPREMULQ_2((x) + 2)
#define UNPREMULQ_8(x) UNPREMULQ_4(x), UNPREMULQ_4((x) + 4)
#define UNPREMULQ_16(x) UNPREMULQ_8(x), UNPREMULQ_8((x) + 8)
#define UNPREMULQ_32(x) UNPREMULQ_16(x), UNPREMULQ_16((x) + 16)
static const uint32_t sUnpremultiplyTable[256] = {0,
                                                  UNPREMULQ(1),
                                                  UNPREMULQ_2(2),
                                                  UNPREMULQ_4(4),
                                                  UNPREMULQ_8(8),
                                                  UNPREMULQ_16(16),
                                                  UNPREMULQ_32(32),
                                                  UNPREMULQ_32(64),
                                                  UNPREMULQ_32(96),
                                                  UNPREMULQ_32(128),
                                                  UNPREMULQ_32(160),
                                                  UNPREMULQ_32(192),
                                                  UNPREMULQ_32(224)};

// Fallback unpremultiply implementation that uses 8.16 fixed-point reciprocal
// math to eliminate any division by the alpha component. This optimization is
// used for the SSE2 and NEON implementations, with some adaptations. This
// implementation also accesses color components using individual byte accesses
// as this profiles faster than accessing the pixel as a uint32_t and
// shifting/masking to access components.
template <bool aSwapRB, uint32_t aSrcRGBIndex, uint32_t aSrcAIndex,
          uint32_t aDstRGBIndex, uint32_t aDstAIndex>
static void UnpremultiplyFallback(const uint8_t* aSrc, int32_t aSrcGap,
                                  uint8_t* aDst, int32_t aDstGap,
                                  IntSize aSize) {
  for (int32_t height = aSize.height; height > 0; height--) {
    const uint8_t* end = aSrc + 4 * aSize.width;
    do {
      uint8_t r = aSrc[aSrcRGBIndex + (aSwapRB ? 2 : 0)];
      uint8_t g = aSrc[aSrcRGBIndex + 1];
      uint8_t b = aSrc[aSrcRGBIndex + (aSwapRB ? 0 : 2)];
      uint8_t a = aSrc[aSrcAIndex];

      // Access the 8.16 reciprocal from the table based on alpha. Multiply by
      // the reciprocal and shift off the fraction bits to approximate the
      // division by alpha.
      uint32_t q = sUnpremultiplyTable[a];
      aDst[aDstRGBIndex + 0] = (r * q) >> 16;
      aDst[aDstRGBIndex + 1] = (g * q) >> 16;
      aDst[aDstRGBIndex + 2] = (b * q) >> 16;
      aDst[aDstAIndex] = a;

      aSrc += 4;
      aDst += 4;
    } while (aSrc < end);

    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

#define UNPREMULTIPLY_FALLBACK_CASE(aSrcFormat, aDstFormat)             \
  FORMAT_CASE(aSrcFormat, aDstFormat,                                   \
              UnpremultiplyFallback<                                    \
                  ShouldSwapRB(aSrcFormat, aDstFormat),                 \
                  RGBByteIndex(aSrcFormat), AlphaByteIndex(aSrcFormat), \
                  RGBByteIndex(aDstFormat), AlphaByteIndex(aDstFormat)>)

#define UNPREMULTIPLY_FALLBACK(aSrcFormat)                         \
  UNPREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::B8G8R8A8) \
  UNPREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::R8G8B8A8) \
  UNPREMULTIPLY_FALLBACK_CASE(aSrcFormat, SurfaceFormat::A8R8G8B8)

bool UnpremultiplyData(const uint8_t* aSrc, int32_t aSrcStride,
                       SurfaceFormat aSrcFormat, uint8_t* aDst,
                       int32_t aDstStride, SurfaceFormat aDstFormat,
                       const IntSize& aSize) {
  if (aSize.IsEmpty()) {
    return true;
  }
  IntSize size = CollapseSize(aSize, aSrcStride, aDstStride);
  // Find gap from end of row to the start of the next row.
  int32_t srcGap = GetStrideGap(aSize.width, aSrcFormat, aSrcStride);
  int32_t dstGap = GetStrideGap(aSize.width, aDstFormat, aDstStride);
  MOZ_ASSERT(srcGap >= 0 && dstGap >= 0);
  if (srcGap < 0 || dstGap < 0) {
    return false;
  }

#define FORMAT_CASE_CALL(...) __VA_ARGS__(aSrc, srcGap, aDst, dstGap, size)

#ifdef USE_SSE2
  if (mozilla::supports_sse2()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      UNPREMULTIPLY_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8)
      UNPREMULTIPLY_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      UNPREMULTIPLY_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8A8)
      UNPREMULTIPLY_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      default:
        break;
    }
#endif

#ifdef USE_NEON
  if (mozilla::supports_neon()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      UNPREMULTIPLY_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8)
      UNPREMULTIPLY_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      UNPREMULTIPLY_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8A8)
      UNPREMULTIPLY_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      default:
        break;
    }
#endif

  switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
    UNPREMULTIPLY_FALLBACK(SurfaceFormat::B8G8R8A8)
    UNPREMULTIPLY_FALLBACK(SurfaceFormat::R8G8B8A8)
    UNPREMULTIPLY_FALLBACK(SurfaceFormat::A8R8G8B8)
    default:
      break;
  }

#undef FORMAT_CASE_CALL

  MOZ_ASSERT(false, "Unsupported unpremultiply formats");
  return false;
}

/**
 * Swizzling
 */

// Fallback swizzle implementation that uses shifting and masking to reorder
// pixels.
template <bool aSwapRB, bool aOpaqueAlpha, uint32_t aSrcRGBShift,
          uint32_t aSrcAShift, uint32_t aDstRGBShift, uint32_t aDstAShift>
static void SwizzleChunkFallback(const uint8_t*& aSrc, uint8_t*& aDst,
                                 int32_t aLength) {
  const uint8_t* end = aSrc + 4 * aLength;
  do {
    uint32_t rgba = *reinterpret_cast<const uint32_t*>(aSrc);

    if (aSwapRB) {
      // Handle R and B swaps by exchanging words and masking.
      uint32_t rb =
          ((rgba << 16) | (rgba >> 16)) & (0x00FF00FF << aSrcRGBShift);
      uint32_t ga = rgba & ((0xFF << aSrcAShift) | (0xFF00 << aSrcRGBShift));
      rgba = rb | ga;
    }

    // If src and dst shifts differ, rotate left or right to move RGB into
    // place, i.e. ARGB -> RGBA or ARGB -> RGBA.
    if (aDstRGBShift > aSrcRGBShift) {
      rgba = (rgba << 8) | (aOpaqueAlpha ? 0x000000FF : rgba >> 24);
    } else if (aSrcRGBShift > aDstRGBShift) {
      rgba = (rgba >> 8) | (aOpaqueAlpha ? 0xFF000000 : rgba << 24);
    } else if (aOpaqueAlpha) {
      rgba |= 0xFF << aDstAShift;
    }

    *reinterpret_cast<uint32_t*>(aDst) = rgba;

    aSrc += 4;
    aDst += 4;
  } while (aSrc < end);
}

template <bool aSwapRB, bool aOpaqueAlpha, uint32_t aSrcRGBShift,
          uint32_t aSrcAShift, uint32_t aDstRGBShift, uint32_t aDstAShift>
static void SwizzleRowFallback(const uint8_t* aSrc, uint8_t* aDst,
                               int32_t aLength) {
  SwizzleChunkFallback<aSwapRB, aOpaqueAlpha, aSrcRGBShift, aSrcAShift,
                       aDstRGBShift, aDstAShift>(aSrc, aDst, aLength);
}

template <bool aSwapRB, bool aOpaqueAlpha, uint32_t aSrcRGBShift,
          uint32_t aSrcAShift, uint32_t aDstRGBShift, uint32_t aDstAShift>
static void SwizzleFallback(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                            int32_t aDstGap, IntSize aSize) {
  for (int32_t height = aSize.height; height > 0; height--) {
    SwizzleChunkFallback<aSwapRB, aOpaqueAlpha, aSrcRGBShift, aSrcAShift,
                         aDstRGBShift, aDstAShift>(aSrc, aDst, aSize.width);
    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

#define SWIZZLE_FALLBACK(aSrcFormat, aDstFormat)                          \
  FORMAT_CASE(                                                            \
      aSrcFormat, aDstFormat,                                             \
      SwizzleFallback<ShouldSwapRB(aSrcFormat, aDstFormat),               \
                      ShouldForceOpaque(aSrcFormat, aDstFormat),          \
                      RGBBitShift(aSrcFormat), AlphaBitShift(aSrcFormat), \
                      RGBBitShift(aDstFormat), AlphaBitShift(aDstFormat)>)

#define SWIZZLE_ROW_FALLBACK(aSrcFormat, aDstFormat)                         \
  FORMAT_CASE_ROW(                                                           \
      aSrcFormat, aDstFormat,                                                \
      SwizzleRowFallback<ShouldSwapRB(aSrcFormat, aDstFormat),               \
                         ShouldForceOpaque(aSrcFormat, aDstFormat),          \
                         RGBBitShift(aSrcFormat), AlphaBitShift(aSrcFormat), \
                         RGBBitShift(aDstFormat), AlphaBitShift(aDstFormat)>)

// Fast-path for matching formats.
static void SwizzleCopy(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                        int32_t aDstGap, IntSize aSize, int32_t aBPP) {
  if (aSrc != aDst) {
    int32_t rowLength = aBPP * aSize.width;
    for (int32_t height = aSize.height; height > 0; height--) {
      memcpy(aDst, aSrc, rowLength);
      aSrc += rowLength + aSrcGap;
      aDst += rowLength + aDstGap;
    }
  }
}

// Fast-path for conversions that swap all bytes.
template <bool aOpaqueAlpha, uint32_t aSrcAShift, uint32_t aDstAShift>
static void SwizzleChunkSwap(const uint8_t*& aSrc, uint8_t*& aDst,
                             int32_t aLength) {
  const uint8_t* end = aSrc + 4 * aLength;
  do {
    // Use an endian swap to move the bytes, i.e. BGRA -> ARGB.
    uint32_t rgba = *reinterpret_cast<const uint32_t*>(aSrc);
#if MOZ_LITTLE_ENDIAN
    rgba = NativeEndian::swapToBigEndian(rgba);
#else
    rgba = NativeEndian::swapToLittleEndian(rgba);
#endif
    if (aOpaqueAlpha) {
      rgba |= 0xFF << aDstAShift;
    }
    *reinterpret_cast<uint32_t*>(aDst) = rgba;
    aSrc += 4;
    aDst += 4;
  } while (aSrc < end);
}

template <bool aOpaqueAlpha, uint32_t aSrcAShift, uint32_t aDstAShift>
static void SwizzleRowSwap(const uint8_t* aSrc, uint8_t* aDst,
                           int32_t aLength) {
  SwizzleChunkSwap<aOpaqueAlpha, aSrcAShift, aDstAShift>(aSrc, aDst, aLength);
}

template <bool aOpaqueAlpha, uint32_t aSrcAShift, uint32_t aDstAShift>
static void SwizzleSwap(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                        int32_t aDstGap, IntSize aSize) {
  for (int32_t height = aSize.height; height > 0; height--) {
    SwizzleChunkSwap<aOpaqueAlpha, aSrcAShift, aDstAShift>(aSrc, aDst,
                                                           aSize.width);
    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

#define SWIZZLE_SWAP(aSrcFormat, aDstFormat)                 \
  FORMAT_CASE(                                               \
      aSrcFormat, aDstFormat,                                \
      SwizzleSwap<ShouldForceOpaque(aSrcFormat, aDstFormat), \
                  AlphaBitShift(aSrcFormat), AlphaBitShift(aDstFormat)>)

#define SWIZZLE_ROW_SWAP(aSrcFormat, aDstFormat)                \
  FORMAT_CASE_ROW(                                              \
      aSrcFormat, aDstFormat,                                   \
      SwizzleRowSwap<ShouldForceOpaque(aSrcFormat, aDstFormat), \
                     AlphaBitShift(aSrcFormat), AlphaBitShift(aDstFormat)>)

// Fast-path for conversions that force alpha to opaque.
template <uint32_t aDstAShift>
static void SwizzleChunkOpaqueUpdate(uint8_t*& aBuffer, int32_t aLength) {
  const uint8_t* end = aBuffer + 4 * aLength;
  do {
    uint32_t rgba = *reinterpret_cast<const uint32_t*>(aBuffer);
    // Just add on the alpha bits to the source.
    rgba |= 0xFF << aDstAShift;
    *reinterpret_cast<uint32_t*>(aBuffer) = rgba;
    aBuffer += 4;
  } while (aBuffer < end);
}

template <uint32_t aDstAShift>
static void SwizzleChunkOpaqueCopy(const uint8_t*& aSrc, uint8_t* aDst,
                                   int32_t aLength) {
  const uint8_t* end = aSrc + 4 * aLength;
  do {
    uint32_t rgba = *reinterpret_cast<const uint32_t*>(aSrc);
    // Just add on the alpha bits to the source.
    rgba |= 0xFF << aDstAShift;
    *reinterpret_cast<uint32_t*>(aDst) = rgba;
    aSrc += 4;
    aDst += 4;
  } while (aSrc < end);
}

template <uint32_t aDstAShift>
static void SwizzleRowOpaque(const uint8_t* aSrc, uint8_t* aDst,
                             int32_t aLength) {
  if (aSrc == aDst) {
    SwizzleChunkOpaqueUpdate<aDstAShift>(aDst, aLength);
  } else {
    SwizzleChunkOpaqueCopy<aDstAShift>(aSrc, aDst, aLength);
  }
}

template <uint32_t aDstAShift>
static void SwizzleOpaque(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                          int32_t aDstGap, IntSize aSize) {
  if (aSrc == aDst) {
    // Modifying in-place, so just write out the alpha.
    for (int32_t height = aSize.height; height > 0; height--) {
      SwizzleChunkOpaqueUpdate<aDstAShift>(aDst, aSize.width);
      aDst += aDstGap;
    }
  } else {
    for (int32_t height = aSize.height; height > 0; height--) {
      SwizzleChunkOpaqueCopy<aDstAShift>(aSrc, aDst, aSize.width);
      aSrc += aSrcGap;
      aDst += aDstGap;
    }
  }
}

#define SWIZZLE_OPAQUE(aSrcFormat, aDstFormat) \
  FORMAT_CASE(aSrcFormat, aDstFormat, SwizzleOpaque<AlphaBitShift(aDstFormat)>)

#define SWIZZLE_ROW_OPAQUE(aSrcFormat, aDstFormat) \
  FORMAT_CASE_ROW(aSrcFormat, aDstFormat,          \
                  SwizzleRowOpaque<AlphaBitShift(aDstFormat)>)

// Packing of 32-bit formats to RGB565.
template <bool aSwapRB, uint32_t aSrcRGBShift, uint32_t aSrcRGBIndex>
static void PackToRGB565(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                         int32_t aDstGap, IntSize aSize) {
  for (int32_t height = aSize.height; height > 0; height--) {
    const uint8_t* end = aSrc + 4 * aSize.width;
    do {
      uint32_t rgba = *reinterpret_cast<const uint32_t*>(aSrc);

      // Isolate the R, G, and B components and shift to final endian-dependent
      // locations.
      uint16_t rgb565;
      if (aSwapRB) {
        rgb565 = ((rgba & (0xF8 << aSrcRGBShift)) << (8 - aSrcRGBShift)) |
                 ((rgba & (0xFC00 << aSrcRGBShift)) >> (5 + aSrcRGBShift)) |
                 ((rgba & (0xF80000 << aSrcRGBShift)) >> (19 + aSrcRGBShift));
      } else {
        rgb565 = ((rgba & (0xF8 << aSrcRGBShift)) >> (3 + aSrcRGBShift)) |
                 ((rgba & (0xFC00 << aSrcRGBShift)) >> (5 + aSrcRGBShift)) |
                 ((rgba & (0xF80000 << aSrcRGBShift)) >> (8 + aSrcRGBShift));
      }

      *reinterpret_cast<uint16_t*>(aDst) = rgb565;

      aSrc += 4;
      aDst += 2;
    } while (aSrc < end);

    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

// Packing of 32-bit formats to 24-bit formats.
template <bool aSwapRB, uint32_t aSrcRGBShift, uint32_t aSrcRGBIndex>
static void PackToRGB24(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                        int32_t aDstGap, IntSize aSize) {
  for (int32_t height = aSize.height; height > 0; height--) {
    const uint8_t* end = aSrc + 4 * aSize.width;
    do {
      uint8_t r = aSrc[aSrcRGBIndex + (aSwapRB ? 2 : 0)];
      uint8_t g = aSrc[aSrcRGBIndex + 1];
      uint8_t b = aSrc[aSrcRGBIndex + (aSwapRB ? 0 : 2)];

      aDst[0] = r;
      aDst[1] = g;
      aDst[2] = b;

      aSrc += 4;
      aDst += 3;
    } while (aSrc < end);

    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

#define PACK_RGB_CASE(aSrcFormat, aDstFormat, aPackFunc)      \
  FORMAT_CASE(aSrcFormat, aDstFormat,                         \
              aPackFunc<ShouldSwapRB(aSrcFormat, aDstFormat), \
                        RGBBitShift(aSrcFormat), RGBByteIndex(aSrcFormat)>)

#define PACK_RGB(aDstFormat, aPackFunc)                         \
  PACK_RGB_CASE(SurfaceFormat::B8G8R8A8, aDstFormat, aPackFunc) \
  PACK_RGB_CASE(SurfaceFormat::B8G8R8X8, aDstFormat, aPackFunc) \
  PACK_RGB_CASE(SurfaceFormat::R8G8B8A8, aDstFormat, aPackFunc) \
  PACK_RGB_CASE(SurfaceFormat::R8G8B8X8, aDstFormat, aPackFunc) \
  PACK_RGB_CASE(SurfaceFormat::A8R8G8B8, aDstFormat, aPackFunc) \
  PACK_RGB_CASE(SurfaceFormat::X8R8G8B8, aDstFormat, aPackFunc)

// Packing of 32-bit formats to A8.
template <uint32_t aSrcAIndex>
static void PackToA8(const uint8_t* aSrc, int32_t aSrcGap, uint8_t* aDst,
                     int32_t aDstGap, IntSize aSize) {
  for (int32_t height = aSize.height; height > 0; height--) {
    const uint8_t* end = aSrc + 4 * aSize.width;
    do {
      *aDst++ = aSrc[aSrcAIndex];
      aSrc += 4;
    } while (aSrc < end);
    aSrc += aSrcGap;
    aDst += aDstGap;
  }
}

#define PACK_ALPHA_CASE(aSrcFormat, aDstFormat, aPackFunc) \
  FORMAT_CASE(aSrcFormat, aDstFormat, aPackFunc<AlphaByteIndex(aSrcFormat)>)

#define PACK_ALPHA(aDstFormat, aPackFunc)                         \
  PACK_ALPHA_CASE(SurfaceFormat::B8G8R8A8, aDstFormat, aPackFunc) \
  PACK_ALPHA_CASE(SurfaceFormat::R8G8B8A8, aDstFormat, aPackFunc) \
  PACK_ALPHA_CASE(SurfaceFormat::A8R8G8B8, aDstFormat, aPackFunc)

template <bool aSwapRB>
void UnpackRowRGB24(const uint8_t* aSrc, uint8_t* aDst, int32_t aLength) {
  // Because we are expanding, we can only process the data back to front in
  // case we are performing this in place.
  const uint8_t* src = aSrc + 3 * (aLength - 1);
  uint8_t* dst = aDst + 4 * (aLength - 1);
  while (src >= aSrc) {
    uint8_t r = src[aSwapRB ? 2 : 0];
    uint8_t g = src[1];
    uint8_t b = src[aSwapRB ? 0 : 2];

    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = 0xFF;

    src -= 3;
    dst -= 4;
  }
}

// Force instantiation of swizzle variants here.
template void UnpackRowRGB24<false>(const uint8_t*, uint8_t*, int32_t);
template void UnpackRowRGB24<true>(const uint8_t*, uint8_t*, int32_t);

#define UNPACK_ROW_RGB(aDstFormat)                   \
  FORMAT_CASE_ROW(SurfaceFormat::R8G8B8, aDstFormat, \
                  UnpackRowRGB24<ShouldSwapRB(SurfaceFormat::R8G8B8, aDstFormat)>)

bool SwizzleData(const uint8_t* aSrc, int32_t aSrcStride,
                 SurfaceFormat aSrcFormat, uint8_t* aDst, int32_t aDstStride,
                 SurfaceFormat aDstFormat, const IntSize& aSize) {
  if (aSize.IsEmpty()) {
    return true;
  }
  IntSize size = CollapseSize(aSize, aSrcStride, aDstStride);
  // Find gap from end of row to the start of the next row.
  int32_t srcGap = GetStrideGap(aSize.width, aSrcFormat, aSrcStride);
  int32_t dstGap = GetStrideGap(aSize.width, aDstFormat, aDstStride);
  MOZ_ASSERT(srcGap >= 0 && dstGap >= 0);
  if (srcGap < 0 || dstGap < 0) {
    return false;
  }

#define FORMAT_CASE_CALL(...) __VA_ARGS__(aSrc, srcGap, aDst, dstGap, size)

#ifdef USE_SSE2
  if (mozilla::supports_sse2()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      SWIZZLE_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      SWIZZLE_SSE2(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8X8)
      SWIZZLE_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
      SWIZZLE_SSE2(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8A8)
      SWIZZLE_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      SWIZZLE_SSE2(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8X8)
      SWIZZLE_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
      SWIZZLE_SSE2(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8A8)
      default:
        break;
    }
#endif

#ifdef USE_NEON
  if (mozilla::supports_neon()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      SWIZZLE_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      SWIZZLE_NEON(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8X8)
      SWIZZLE_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
      SWIZZLE_NEON(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8A8)
      SWIZZLE_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      SWIZZLE_NEON(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8X8)
      SWIZZLE_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
      SWIZZLE_NEON(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8A8)
      default:
        break;
    }
#endif

  switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
    SWIZZLE_FALLBACK(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
    SWIZZLE_FALLBACK(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8X8)
    SWIZZLE_FALLBACK(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
    SWIZZLE_FALLBACK(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8A8)

    SWIZZLE_FALLBACK(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
    SWIZZLE_FALLBACK(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8X8)
    SWIZZLE_FALLBACK(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
    SWIZZLE_FALLBACK(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8A8)
    SWIZZLE_FALLBACK(SurfaceFormat::R8G8B8A8, SurfaceFormat::A8R8G8B8)
    SWIZZLE_FALLBACK(SurfaceFormat::R8G8B8X8, SurfaceFormat::X8R8G8B8)

    SWIZZLE_FALLBACK(SurfaceFormat::A8R8G8B8, SurfaceFormat::R8G8B8A8)
    SWIZZLE_FALLBACK(SurfaceFormat::X8R8G8B8, SurfaceFormat::R8G8B8X8)
    SWIZZLE_FALLBACK(SurfaceFormat::A8R8G8B8, SurfaceFormat::R8G8B8X8)
    SWIZZLE_FALLBACK(SurfaceFormat::X8R8G8B8, SurfaceFormat::R8G8B8A8)

    SWIZZLE_SWAP(SurfaceFormat::B8G8R8A8, SurfaceFormat::A8R8G8B8)
    SWIZZLE_SWAP(SurfaceFormat::B8G8R8A8, SurfaceFormat::X8R8G8B8)
    SWIZZLE_SWAP(SurfaceFormat::B8G8R8X8, SurfaceFormat::X8R8G8B8)
    SWIZZLE_SWAP(SurfaceFormat::B8G8R8X8, SurfaceFormat::A8R8G8B8)
    SWIZZLE_SWAP(SurfaceFormat::A8R8G8B8, SurfaceFormat::B8G8R8A8)
    SWIZZLE_SWAP(SurfaceFormat::A8R8G8B8, SurfaceFormat::B8G8R8X8)
    SWIZZLE_SWAP(SurfaceFormat::X8R8G8B8, SurfaceFormat::B8G8R8X8)
    SWIZZLE_SWAP(SurfaceFormat::X8R8G8B8, SurfaceFormat::B8G8R8A8)

    SWIZZLE_OPAQUE(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8X8)
    SWIZZLE_OPAQUE(SurfaceFormat::B8G8R8X8, SurfaceFormat::B8G8R8A8)
    SWIZZLE_OPAQUE(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8X8)
    SWIZZLE_OPAQUE(SurfaceFormat::R8G8B8X8, SurfaceFormat::R8G8B8A8)
    SWIZZLE_OPAQUE(SurfaceFormat::A8R8G8B8, SurfaceFormat::X8R8G8B8)
    SWIZZLE_OPAQUE(SurfaceFormat::X8R8G8B8, SurfaceFormat::A8R8G8B8)

    PACK_RGB(SurfaceFormat::R5G6B5_UINT16, PackToRGB565)
    PACK_RGB(SurfaceFormat::B8G8R8, PackToRGB24)
    PACK_RGB(SurfaceFormat::R8G8B8, PackToRGB24)
    PACK_ALPHA(SurfaceFormat::A8, PackToA8)

    default:
      break;
  }

  if (aSrcFormat == aDstFormat) {
    // If the formats match, just do a generic copy.
    SwizzleCopy(aSrc, srcGap, aDst, dstGap, size, BytesPerPixel(aSrcFormat));
    return true;
  }

#undef FORMAT_CASE_CALL

  MOZ_ASSERT(false, "Unsupported swizzle formats");
  return false;
}

SwizzleRowFn SwizzleRow(SurfaceFormat aSrcFormat, SurfaceFormat aDstFormat) {
#ifdef USE_SSE2
  if (mozilla::supports_avx2()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      UNPACK_ROW_RGB_AVX2(SurfaceFormat::R8G8B8X8)
      UNPACK_ROW_RGB_AVX2(SurfaceFormat::R8G8B8A8)
      UNPACK_ROW_RGB_AVX2(SurfaceFormat::B8G8R8X8)
      UNPACK_ROW_RGB_AVX2(SurfaceFormat::B8G8R8A8)
      default:
        break;
    }

  if (mozilla::supports_ssse3()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      UNPACK_ROW_RGB_SSSE3(SurfaceFormat::R8G8B8X8)
      UNPACK_ROW_RGB_SSSE3(SurfaceFormat::R8G8B8A8)
      UNPACK_ROW_RGB_SSSE3(SurfaceFormat::B8G8R8X8)
      UNPACK_ROW_RGB_SSSE3(SurfaceFormat::B8G8R8A8)
      default:
        break;
    }

  if (mozilla::supports_sse2()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      SWIZZLE_ROW_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      SWIZZLE_ROW_SSE2(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8X8)
      SWIZZLE_ROW_SSE2(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
      SWIZZLE_ROW_SSE2(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8A8)
      SWIZZLE_ROW_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      SWIZZLE_ROW_SSE2(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8X8)
      SWIZZLE_ROW_SSE2(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
      SWIZZLE_ROW_SSE2(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8A8)
      default:
        break;
    }
#endif

#ifdef USE_NEON
  if (mozilla::supports_neon()) switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
      SWIZZLE_ROW_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
      SWIZZLE_ROW_NEON(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8X8)
      SWIZZLE_ROW_NEON(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
      SWIZZLE_ROW_NEON(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8A8)
      SWIZZLE_ROW_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
      SWIZZLE_ROW_NEON(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8X8)
      SWIZZLE_ROW_NEON(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
      SWIZZLE_ROW_NEON(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8A8)
      default:
        break;
    }
#endif

  switch (FORMAT_KEY(aSrcFormat, aDstFormat)) {
    SWIZZLE_ROW_FALLBACK(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8)
    SWIZZLE_ROW_FALLBACK(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8X8)
    SWIZZLE_ROW_FALLBACK(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8)
    SWIZZLE_ROW_FALLBACK(SurfaceFormat::B8G8R8X8, SurfaceFormat::R8G8B8A8)

    SWIZZLE_ROW_FALLBACK(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8)
    SWIZZLE_ROW_FALLBACK(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8X8)
    SWIZZLE_ROW_FALLBACK(SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8)
    SWIZZLE_ROW_FALLBACK(SurfaceFormat::R8G8B8X8, SurfaceFormat::B8G8R8A8)

    SWIZZLE_ROW_OPAQUE(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8X8)
    SWIZZLE_ROW_OPAQUE(SurfaceFormat::B8G8R8X8, SurfaceFormat::B8G8R8A8)
    SWIZZLE_ROW_OPAQUE(SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8X8)
    SWIZZLE_ROW_OPAQUE(SurfaceFormat::R8G8B8X8, SurfaceFormat::R8G8B8A8)

    UNPACK_ROW_RGB(SurfaceFormat::R8G8B8X8)
    UNPACK_ROW_RGB(SurfaceFormat::R8G8B8A8)
    UNPACK_ROW_RGB(SurfaceFormat::B8G8R8X8)
    UNPACK_ROW_RGB(SurfaceFormat::B8G8R8A8)

    default:
      break;
  }

  MOZ_ASSERT_UNREACHABLE("Unsupported swizzle formats");
  return nullptr;
}

}  // namespace gfx
}  // namespace mozilla
