/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TYPES_H_
#define MOZILLA_GFX_TYPES_H_

#include "mozilla/NullPtr.h"
#include "mozilla/TypedEnum.h"

#include <stddef.h>
#include <stdint.h>

namespace mozilla {
namespace gfx {

typedef float Float;

MOZ_BEGIN_ENUM_CLASS(SurfaceType, int8_t)
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
  RECORDING /* Surface used for recording */
MOZ_END_ENUM_CLASS(SurfaceType)

MOZ_BEGIN_ENUM_CLASS(SurfaceFormat, int8_t)
  B8G8R8A8,
  B8G8R8X8,
  R8G8B8A8,
  R8G8B8X8,
  R5G6B5,
  A8,
  YUV,
  UNKNOWN
MOZ_END_ENUM_CLASS(SurfaceFormat)

MOZ_BEGIN_ENUM_CLASS(FilterType, int8_t)
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
MOZ_END_ENUM_CLASS(FilterType)

MOZ_BEGIN_ENUM_CLASS(BackendType, int8_t)
  NONE = 0,
  DIRECT2D,
  COREGRAPHICS,
  COREGRAPHICS_ACCELERATED,
  CAIRO,
  SKIA,
  RECORDING,
  DIRECT2D1_1
MOZ_END_ENUM_CLASS(BackendType)

MOZ_BEGIN_ENUM_CLASS(FontType, int8_t)
  DWRITE,
  GDI,
  MAC,
  SKIA,
  CAIRO,
  COREGRAPHICS
MOZ_END_ENUM_CLASS(FontType)

MOZ_BEGIN_ENUM_CLASS(NativeSurfaceType, int8_t)
  D3D10_TEXTURE,
  CAIRO_SURFACE,
  CAIRO_CONTEXT,
  CGCONTEXT,
  CGCONTEXT_ACCELERATED,
  OPENGL_TEXTURE
MOZ_END_ENUM_CLASS(NativeSurfaceType)

MOZ_BEGIN_ENUM_CLASS(NativeFontType, int8_t)
  DWRITE_FONT_FACE,
  GDI_FONT_FACE,
  MAC_FONT_FACE,
  SKIA_FONT_FACE,
  CAIRO_FONT_FACE
MOZ_END_ENUM_CLASS(NativeFontType)

MOZ_BEGIN_ENUM_CLASS(FontStyle, int8_t)
  NORMAL,
  ITALIC,
  BOLD,
  BOLD_ITALIC
MOZ_END_ENUM_CLASS(FontStyle)

MOZ_BEGIN_ENUM_CLASS(FontHinting, int8_t)
  NONE,
  LIGHT,
  NORMAL,
  FULL
MOZ_END_ENUM_CLASS(FontHinting)

MOZ_BEGIN_ENUM_CLASS(CompositionOp, int8_t)
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
MOZ_END_ENUM_CLASS(CompositionOp)

MOZ_BEGIN_ENUM_CLASS(ExtendMode, int8_t)
  CLAMP,
  REPEAT,
  REFLECT
MOZ_END_ENUM_CLASS(ExtendMode)

MOZ_BEGIN_ENUM_CLASS(FillRule, int8_t)
  FILL_WINDING,
  FILL_EVEN_ODD
MOZ_END_ENUM_CLASS(FillRule)

MOZ_BEGIN_ENUM_CLASS(AntialiasMode, int8_t)
  NONE,
  GRAY,
  SUBPIXEL,
  DEFAULT
MOZ_END_ENUM_CLASS(AntialiasMode)

MOZ_BEGIN_ENUM_CLASS(Filter, int8_t)
  GOOD,
  LINEAR,
  POINT
MOZ_END_ENUM_CLASS(Filter)

MOZ_BEGIN_ENUM_CLASS(PatternType, int8_t)
  COLOR,
  SURFACE,
  LINEAR_GRADIENT,
  RADIAL_GRADIENT
MOZ_END_ENUM_CLASS(PatternType)

MOZ_BEGIN_ENUM_CLASS(JoinStyle, int8_t)
  BEVEL,
  ROUND,
  MITER,
  MITER_OR_BEVEL
MOZ_END_ENUM_CLASS(JoinStyle)

MOZ_BEGIN_ENUM_CLASS(CapStyle, int8_t)
  BUTT,
  ROUND,
  SQUARE
MOZ_END_ENUM_CLASS(CapStyle)

MOZ_BEGIN_ENUM_CLASS(SamplingBounds, int8_t)
  UNBOUNDED,
  BOUNDED
MOZ_END_ENUM_CLASS(SamplingBounds)

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

  uint32_t ToABGR() const
  {
    return uint32_t(r * 255.0f) | uint32_t(g * 255.0f) << 8 |
           uint32_t(b * 255.0f) << 16 | uint32_t(a * 255.0f) << 24;
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

}
}

#if defined(XP_WIN) && defined(MOZ_GFX)
#ifdef GFX2D_INTERNAL
#define GFX2D_API __declspec(dllexport)
#else
#define GFX2D_API __declspec(dllimport)
#endif
#else
#define GFX2D_API
#endif

// Side constants for use in various places
namespace mozilla {
  namespace css {
    enum Side {eSideTop, eSideRight, eSideBottom, eSideLeft};
  }
}

// XXX - These don't really belong here. But for now this is where they go.
#define NS_SIDE_TOP     mozilla::css::eSideTop
#define NS_SIDE_RIGHT   mozilla::css::eSideRight
#define NS_SIDE_BOTTOM  mozilla::css::eSideBottom
#define NS_SIDE_LEFT    mozilla::css::eSideLeft

#endif /* MOZILLA_GFX_TYPES_H_ */
