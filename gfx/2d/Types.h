/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TYPES_H_
#define MOZILLA_GFX_TYPES_H_

#include "mozilla/EndianUtils.h"
#include "mozilla/MacroArgs.h" // for MOZ_CONCAT

#include <stddef.h>
#include <stdint.h>

namespace mozilla {
namespace gfx {

typedef float Float;
typedef double Double;

enum class SurfaceType : int8_t {
  DATA, /* Data surface - bitmap in memory */
  D2D1_BITMAP, /* Surface wrapping a ID2D1Bitmap */
  D2D1_DRAWTARGET, /* Surface made from a D2D draw target */
  CAIRO, /* Surface wrapping a cairo surface */
  CAIRO_IMAGE, /* Data surface wrapping a cairo image surface */
  COREGRAPHICS_IMAGE, /* Surface wrapping a CoreGraphics Image */
  COREGRAPHICS_CGCONTEXT, /* Surface wrapping a CG context */
  SKIA, /* Surface wrapping a Skia bitmap */
  DUAL_DT, /* Snapshot of a dual drawtarget */
  D2D1_1_IMAGE, /* A D2D 1.1 ID2D1Image SourceSurface */
  RECORDING, /* Surface used for recording */
  TILED, /* Surface from a tiled DrawTarget */
  DATA_SHARED, /* Data surface using shared memory */
};

enum class SurfaceFormat : int8_t {
  // The following values are named to reflect layout of colors in memory, from
  // lowest byte to highest byte. The 32-bit value layout depends on machine
  // endianness.
  //               in-memory            32-bit LE value   32-bit BE value
  B8G8R8A8,     // [BB, GG, RR, AA]     0xAARRGGBB        0xBBGGRRAA
  B8G8R8X8,     // [BB, GG, RR, 00]     0x00RRGGBB        0xBBGGRR00
  R8G8B8A8,     // [RR, GG, BB, AA]     0xAABBGGRR        0xRRGGBBAA
  R8G8B8X8,     // [RR, GG, BB, 00]     0x00BBGGRR        0xRRGGBB00
  A8R8G8B8,     // [AA, RR, GG, BB]     0xBBGGRRAA        0xAARRGGBB
  X8R8G8B8,     // [00, RR, GG, BB]     0xBBGGRR00        0x00RRGGBB

  R8G8B8,
  B8G8R8,

  // The _UINT16 suffix here indicates that the name reflects the layout when
  // viewed as a uint16_t value. In memory these values are stored using native
  // endianness.
  R5G6B5_UINT16,                    // 0bRRRRRGGGGGGBBBBB

  // This one is a single-byte, so endianness isn't an issue.
  A8,

  R8G8,

  // These ones are their own special cases.
  YUV,
  NV12,
  YUV422,
  HSV,
  Lab,
  Depth,

  // This represents the unknown format.
  UNKNOWN,

  // The following values are endian-independent synonyms. The _UINT32 suffix
  // indicates that the name reflects the layout when viewed as a uint32_t
  // value.
#if MOZ_LITTLE_ENDIAN
  A8R8G8B8_UINT32 = B8G8R8A8,       // 0xAARRGGBB
  X8R8G8B8_UINT32 = B8G8R8X8        // 0x00RRGGBB
#elif MOZ_BIG_ENDIAN
  A8R8G8B8_UINT32 = A8R8G8B8,       // 0xAARRGGBB
  X8R8G8B8_UINT32 = X8R8G8B8        // 0x00RRGGBB
#else
# error "bad endianness"
#endif
};

inline bool IsOpaque(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case SurfaceFormat::B8G8R8X8:
  case SurfaceFormat::R8G8B8X8:
  case SurfaceFormat::R5G6B5_UINT16:
  case SurfaceFormat::YUV:
  case SurfaceFormat::NV12:
  case SurfaceFormat::YUV422:
    return true;
  default:
    return false;
  }
}

enum class FilterType : int8_t {
  BLEND = 0,
  TRANSFORM,
  MORPHOLOGY,
  COLOR_MATRIX,
  FLOOD,
  TILE,
  TABLE_TRANSFER,
  DISCRETE_TRANSFER,
  LINEAR_TRANSFER,
  GAMMA_TRANSFER,
  CONVOLVE_MATRIX,
  DISPLACEMENT_MAP,
  TURBULENCE,
  ARITHMETIC_COMBINE,
  COMPOSITE,
  DIRECTIONAL_BLUR,
  GAUSSIAN_BLUR,
  POINT_DIFFUSE,
  POINT_SPECULAR,
  SPOT_DIFFUSE,
  SPOT_SPECULAR,
  DISTANT_DIFFUSE,
  DISTANT_SPECULAR,
  CROP,
  PREMULTIPLY,
  UNPREMULTIPLY
};

enum class DrawTargetType : int8_t {
  SOFTWARE_RASTER = 0,
  HARDWARE_RASTER,
  VECTOR
};

enum class BackendType : int8_t {
  NONE = 0,
  DIRECT2D, // Used for version independent D2D objects.
  CAIRO,
  SKIA,
  RECORDING,
  DIRECT2D1_1,

  // Add new entries above this line.
  BACKEND_LAST
};

enum class FontType : int8_t {
  DWRITE,
  GDI,
  MAC,
  SKIA,
  CAIRO,
  COREGRAPHICS,
  FONTCONFIG,
  FREETYPE
};

enum class NativeSurfaceType : int8_t {
  D3D10_TEXTURE,
  CAIRO_CONTEXT,
  CGCONTEXT,
  CGCONTEXT_ACCELERATED,
  OPENGL_TEXTURE
};

enum class NativeFontType : int8_t {
  DWRITE_FONT_FACE,
  GDI_FONT_FACE,
  MAC_FONT_FACE,
  SKIA_FONT_FACE,
  CAIRO_FONT_FACE
};

enum class FontStyle : int8_t {
  NORMAL,
  ITALIC,
  BOLD,
  BOLD_ITALIC
};

enum class FontHinting : int8_t {
  NONE,
  LIGHT,
  NORMAL,
  FULL
};

enum class CompositionOp : int8_t {
  OP_OVER,
  OP_ADD,
  OP_ATOP,
  OP_OUT,
  OP_IN,
  OP_SOURCE,
  OP_DEST_IN,
  OP_DEST_OUT,
  OP_DEST_OVER,
  OP_DEST_ATOP,
  OP_XOR,
  OP_MULTIPLY,
  OP_SCREEN,
  OP_OVERLAY,
  OP_DARKEN,
  OP_LIGHTEN,
  OP_COLOR_DODGE,
  OP_COLOR_BURN,
  OP_HARD_LIGHT,
  OP_SOFT_LIGHT,
  OP_DIFFERENCE,
  OP_EXCLUSION,
  OP_HUE,
  OP_SATURATION,
  OP_COLOR,
  OP_LUMINOSITY,
  OP_COUNT
};

enum class Axis : int8_t {
  X_AXIS,
  Y_AXIS,
  BOTH
};

enum class ExtendMode : int8_t {
  CLAMP,    // Do not repeat
  REPEAT,   // Repeat in both axis
  REPEAT_X, // Only X axis
  REPEAT_Y, // Only Y axis
  REFLECT   // Mirror the image
};

enum class FillRule : int8_t {
  FILL_WINDING,
  FILL_EVEN_ODD
};

enum class AntialiasMode : int8_t {
  NONE,
  GRAY,
  SUBPIXEL,
  DEFAULT
};

// See https://en.wikipedia.org/wiki/Texture_filtering
enum class SamplingFilter : int8_t {
  GOOD,
  LINEAR,
  POINT,
  SENTINEL  // one past the last valid value
};

enum class PatternType : int8_t {
  COLOR,
  SURFACE,
  LINEAR_GRADIENT,
  RADIAL_GRADIENT
};

enum class JoinStyle : int8_t {
  BEVEL,
  ROUND,
  MITER, //!< Mitered if within the miter limit, else, if the backed supports
         //!< it (D2D), the miter is clamped. If the backend does not support
         //!< miter clamping the behavior is as for MITER_OR_BEVEL.
  MITER_OR_BEVEL //!< Mitered if within the miter limit, else beveled.
};

enum class CapStyle : int8_t {
  BUTT,
  ROUND,
  SQUARE
};

enum class SamplingBounds : int8_t {
  UNBOUNDED,
  BOUNDED
};

// Moz2d version for SVG mask types
enum class LuminanceType : int8_t {
  LUMINANCE,
  LINEARRGB,
};

/* Color is stored in non-premultiplied form */
struct Color
{
public:
  Color()
    : r(0.0f), g(0.0f), b(0.0f), a(0.0f)
  {}
  Color(Float aR, Float aG, Float aB, Float aA)
    : r(aR), g(aG), b(aB), a(aA)
  {}
  Color(Float aR, Float aG, Float aB)
    : r(aR), g(aG), b(aB), a(1.0f)
  {}

  static Color FromABGR(uint32_t aColor)
  {
    Color newColor(((aColor >> 0) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 8) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 16) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 24) & 0xff) * (1.0f / 255.0f));

    return newColor;
  }

  // The "Unusual" prefix is to avoid unintentionally using this function when
  // FromABGR(), which is much more common, is needed.
  static Color UnusualFromARGB(uint32_t aColor)
  {
    Color newColor(((aColor >> 16) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 8) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 0) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 24) & 0xff) * (1.0f / 255.0f));

    return newColor;
  }

  uint32_t ToABGR() const
  {
    return uint32_t(r * 255.0f) | uint32_t(g * 255.0f) << 8 |
           uint32_t(b * 255.0f) << 16 | uint32_t(a * 255.0f) << 24;
  }

  // The "Unusual" prefix is to avoid unintentionally using this function when
  // ToABGR(), which is much more common, is needed.
  uint32_t UnusualToARGB() const
  {
    return uint32_t(b * 255.0f) | uint32_t(g * 255.0f) << 8 |
           uint32_t(r * 255.0f) << 16 | uint32_t(a * 255.0f) << 24;
  }

  bool operator==(const Color& aColor) const {
    return r == aColor.r && g == aColor.g && b == aColor.b && a == aColor.a;
  }

  bool operator!=(const Color& aColor) const {
    return !(*this == aColor);
  }

  Float r, g, b, a;
};

struct GradientStop
{
  bool operator<(const GradientStop& aOther) const {
    return offset < aOther.offset;
  }

  Float offset;
  Color color;
};

enum class JobStatus {
    Complete,
    Wait,
    Yield,
    Error
};

} // namespace gfx
} // namespace mozilla

// XXX: temporary
typedef mozilla::gfx::SurfaceFormat gfxImageFormat;

#if defined(XP_WIN) && defined(MOZ_GFX)
#ifdef GFX2D_INTERNAL
#define GFX2D_API __declspec(dllexport)
#else
#define GFX2D_API __declspec(dllimport)
#endif
#else
#define GFX2D_API
#endif

namespace mozilla {

// Side constants for use in various places.
enum Side { eSideTop, eSideRight, eSideBottom, eSideLeft };

enum SideBits {
  eSideBitsNone   = 0,
  eSideBitsTop    = 1 << eSideTop,
  eSideBitsRight  = 1 << eSideRight,
  eSideBitsBottom = 1 << eSideBottom,
  eSideBitsLeft   = 1 << eSideLeft,
  eSideBitsTopBottom = eSideBitsTop  | eSideBitsBottom,
  eSideBitsLeftRight = eSideBitsLeft | eSideBitsRight,
  eSideBitsAll = eSideBitsTopBottom | eSideBitsLeftRight
};

// Creates a for loop that walks over the four mozilla::Side values.
// We use an int32_t helper variable (instead of a Side) for our loop counter,
// to avoid triggering undefined behavior just before we exit the loop (at
// which point the counter is incremented beyond the largest valid Side value).
#define NS_FOR_CSS_SIDES(var_)                                           \
  int32_t MOZ_CONCAT(var_,__LINE__) = mozilla::eSideTop;                 \
  for (mozilla::Side var_;                                               \
       MOZ_CONCAT(var_,__LINE__) <= mozilla::eSideLeft &&                \
         ((var_ = mozilla::Side(MOZ_CONCAT(var_,__LINE__))), true);      \
       ++MOZ_CONCAT(var_,__LINE__))

static inline Side& operator++(Side& side) {
  MOZ_ASSERT(side >= eSideTop && side <= eSideLeft,
             "Out of range side");
  side = Side(side + 1);
  return side;
}

enum Corner {
  // This order is important!
  eCornerTopLeft = 0,
  eCornerTopRight = 1,
  eCornerBottomRight = 2,
  eCornerBottomLeft = 3
};

// RectCornerRadii::radii depends on this value. It is not being added to
// Corner because we want to lift the responsibility to handle it in the
// switch-case.
constexpr int eCornerCount = 4;

// Creates a for loop that walks over the four mozilla::Corner values. This
// implementation uses the same technique as NS_FOR_CSS_SIDES.
#define NS_FOR_CSS_FULL_CORNERS(var_)                                   \
  int32_t MOZ_CONCAT(var_,__LINE__) = mozilla::eCornerTopLeft;          \
  for (mozilla::Corner var_;                                            \
       MOZ_CONCAT(var_,__LINE__) <= mozilla::eCornerBottomLeft &&       \
         (var_ = mozilla::Corner(MOZ_CONCAT(var_,__LINE__)), true);     \
       ++MOZ_CONCAT(var_,__LINE__))

static inline Corner operator++(Corner& aCorner) {
  MOZ_ASSERT(aCorner >= eCornerTopLeft && aCorner <= eCornerBottomLeft,
             "Out of range corner!");
  aCorner = Corner(aCorner + 1);
  return aCorner;
}

// Indices into "half corner" arrays (nsStyleCorners e.g.)
enum HalfCorner {
  // This order is important!
  eCornerTopLeftX = 0,
  eCornerTopLeftY = 1,
  eCornerTopRightX = 2,
  eCornerTopRightY = 3,
  eCornerBottomRightX = 4,
  eCornerBottomRightY = 5,
  eCornerBottomLeftX = 6,
  eCornerBottomLeftY = 7
};

// Creates a for loop that walks over the eight mozilla::HalfCorner values.
// This implementation uses the same technique as NS_FOR_CSS_SIDES.
#define NS_FOR_CSS_HALF_CORNERS(var_)                                   \
  int32_t MOZ_CONCAT(var_,__LINE__) = mozilla::eCornerTopLeftX;         \
  for (mozilla::HalfCorner var_;                                        \
       MOZ_CONCAT(var_,__LINE__) <= mozilla::eCornerBottomLeftY &&      \
         (var_ = mozilla::HalfCorner(MOZ_CONCAT(var_,__LINE__)), true); \
       ++MOZ_CONCAT(var_,__LINE__))

static inline HalfCorner operator++(HalfCorner& aHalfCorner) {
  MOZ_ASSERT(aHalfCorner >= eCornerTopLeftX && aHalfCorner <= eCornerBottomLeftY,
             "Out of range half corner!");
  aHalfCorner = HalfCorner(aHalfCorner + 1);
  return aHalfCorner;
}

// The result of these conversion functions are exhaustively checked in
// nsStyleCoord.cpp, which also serves as usage examples.

constexpr bool HalfCornerIsX(HalfCorner aHalfCorner)
{
  return !(aHalfCorner % 2);
}

constexpr Corner HalfToFullCorner(HalfCorner aHalfCorner)
{
  return Corner(aHalfCorner / 2);
}

constexpr HalfCorner FullToHalfCorner(Corner aCorner, bool aIsVertical)
{
  return HalfCorner(aCorner * 2 + aIsVertical);
}

constexpr bool SideIsVertical(Side aSide)
{
  return aSide % 2;
}

// @param aIsSecond when true, return the clockwise second of the two
// corners associated with aSide. For example, with aSide = eSideBottom the
// result is eCornerBottomRight when aIsSecond is false, and
// eCornerBottomLeft when aIsSecond is true.
constexpr Corner SideToFullCorner(Side aSide, bool aIsSecond)
{
  return Corner((aSide + aIsSecond) % 4);
}

// @param aIsSecond see SideToFullCorner.
// @param aIsParallel return the half-corner that is parallel with aSide
// when aIsParallel is true. For example with aSide=eSideTop, aIsSecond=true
// the result is eCornerTopRightX when aIsParallel is true, and
// eCornerTopRightY when aIsParallel is false (because "X" is parallel with
// eSideTop/eSideBottom, similarly "Y" is parallel with
// eSideLeft/eSideRight)
constexpr HalfCorner SideToHalfCorner(Side aSide, bool aIsSecond,
                                      bool aIsParallel)
{
  return HalfCorner(((aSide + aIsSecond) * 2 + (aSide + !aIsParallel) % 2) % 8);
}

} // namespace mozilla

#endif /* MOZILLA_GFX_TYPES_H_ */
