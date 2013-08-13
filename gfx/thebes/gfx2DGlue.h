/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_2D_GLUE_H
#define GFX_2D_GLUE_H


#include "gfxPlatform.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "gfx3DMatrix.h"
#include "gfxContext.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
class SourceSurface;
class ScaledFont;
}
}

namespace mozilla {
namespace gfx {

inline Rect ToRect(const gfxRect &aRect)
{
  return Rect(Float(aRect.x), Float(aRect.y),
              Float(aRect.width), Float(aRect.height));
}

inline IntRect ToIntRect(const nsIntRect &aRect)
{
  return IntRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline Color ToColor(const gfxRGBA &aRGBA)
{
  return Color(Float(aRGBA.r), Float(aRGBA.g),
               Float(aRGBA.b), Float(aRGBA.a));
}

inline Matrix ToMatrix(const gfxMatrix &aMatrix)
{
  return Matrix(Float(aMatrix.xx), Float(aMatrix.yx), Float(aMatrix.xy),
                Float(aMatrix.yy), Float(aMatrix.x0), Float(aMatrix.y0));
}

inline Point ToPoint(const gfxPoint &aPoint)
{
  return Point(Float(aPoint.x), Float(aPoint.y));
}

inline Size ToSize(const gfxSize &aSize)
{
  return Size(Float(aSize.width), Float(aSize.height));
}

inline IntSize ToIntSize(const gfxIntSize &aSize)
{
  return IntSize(aSize.width, aSize.height);
}

inline Filter ToFilter(gfxPattern::GraphicsFilter aFilter)
{
  switch (aFilter) {
  case gfxPattern::FILTER_NEAREST:
    return FILTER_POINT;
  default:
    return FILTER_LINEAR;
  }
}

inline gfxPattern::GraphicsFilter ThebesFilter(Filter aFilter)
{
  switch (aFilter) {
  case FILTER_POINT:
    return gfxPattern::FILTER_NEAREST;
  default:
    return gfxPattern::FILTER_BEST;
  }
}

inline ExtendMode ToExtendMode(gfxPattern::GraphicsExtend aExtend)
{
  switch (aExtend) {
  case gfxPattern::EXTEND_REPEAT:
    return EXTEND_REPEAT;
  case gfxPattern::EXTEND_REFLECT:
    return EXTEND_REFLECT;
  default:
    return EXTEND_CLAMP;
  }
}

inline gfxPattern::GraphicsExtend ThebesExtend(ExtendMode aExtend)
{
  switch (aExtend) {
  case EXTEND_REPEAT:
    return gfxPattern::EXTEND_REPEAT;
  case EXTEND_REFLECT:
    return gfxPattern::EXTEND_REFLECT;
  default:
    return gfxPattern::EXTEND_PAD;
  }
}

inline gfxPoint ThebesPoint(const Point &aPoint)
{
  return gfxPoint(aPoint.x, aPoint.y);
}

inline gfxSize ThebesSize(const Size &aSize)
{
  return gfxSize(aSize.width, aSize.height);
}

inline gfxIntSize ThebesIntSize(const IntSize &aSize)
{
  return gfxIntSize(aSize.width, aSize.height);
}

inline gfxRect ThebesRect(const Rect &aRect)
{
  return gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline nsIntRect ThebesIntRect(const IntRect &aRect)
{
  return nsIntRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline gfxRGBA ThebesRGBA(const Color &aColor)
{
  return gfxRGBA(aColor.r, aColor.g, aColor.b, aColor.a);
}

inline gfxContext::GraphicsLineCap ThebesLineCap(CapStyle aStyle)
{
  switch (aStyle) {
  case CAP_BUTT:
    return gfxContext::LINE_CAP_BUTT;
  case CAP_ROUND:
    return gfxContext::LINE_CAP_ROUND;
  case CAP_SQUARE:
    return gfxContext::LINE_CAP_SQUARE;
  }
  MOZ_CRASH("Incomplete switch");
}

inline CapStyle ToCapStyle(gfxContext::GraphicsLineCap aStyle)
{
  switch (aStyle) {
  case gfxContext::LINE_CAP_BUTT:
    return CAP_BUTT;
  case gfxContext::LINE_CAP_ROUND:
    return CAP_ROUND;
  case gfxContext::LINE_CAP_SQUARE:
    return CAP_SQUARE;
  }
  MOZ_CRASH("Incomplete switch");
}

inline gfxContext::GraphicsLineJoin ThebesLineJoin(JoinStyle aStyle)
{
  switch (aStyle) {
  case JOIN_MITER:
    return gfxContext::LINE_JOIN_MITER;
  case JOIN_BEVEL:
    return gfxContext::LINE_JOIN_BEVEL;
  case JOIN_ROUND:
    return gfxContext::LINE_JOIN_ROUND;
  default:
    return gfxContext::LINE_JOIN_MITER;
  }
}

inline JoinStyle ToJoinStyle(gfxContext::GraphicsLineJoin aStyle)
{
  switch (aStyle) {
  case gfxContext::LINE_JOIN_MITER:
    return JOIN_MITER;
  case gfxContext::LINE_JOIN_BEVEL:
    return JOIN_BEVEL;
  case gfxContext::LINE_JOIN_ROUND:
    return JOIN_ROUND;
  }
  MOZ_CRASH("Incomplete switch");
}

inline gfxMatrix ThebesMatrix(const Matrix &aMatrix)
{
  return gfxMatrix(aMatrix._11, aMatrix._12, aMatrix._21,
                   aMatrix._22, aMatrix._31, aMatrix._32);
}

inline gfxASurface::gfxImageFormat SurfaceFormatToImageFormat(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case FORMAT_B8G8R8A8:
    return gfxASurface::ImageFormatARGB32;
  case FORMAT_B8G8R8X8:
    return gfxASurface::ImageFormatRGB24;
  case FORMAT_R5G6B5:
    return gfxASurface::ImageFormatRGB16_565;
  case FORMAT_A8:
    return gfxASurface::ImageFormatA8;
  default:
    return gfxASurface::ImageFormatUnknown;
  }
}

inline SurfaceFormat ImageFormatToSurfaceFormat(gfxASurface::gfxImageFormat aFormat)
{
  switch (aFormat) {
  case gfxASurface::ImageFormatARGB32:
    return FORMAT_B8G8R8A8;
  case gfxASurface::ImageFormatRGB24:
    return FORMAT_B8G8R8X8;
  case gfxASurface::ImageFormatRGB16_565:
    return FORMAT_R5G6B5;
  case gfxASurface::ImageFormatA8:
    return FORMAT_A8;
  default:
  case gfxASurface::ImageFormatUnknown:
    return FORMAT_B8G8R8A8;
  }
}

inline gfxASurface::gfxContentType ContentForFormat(const SurfaceFormat &aFormat)
{
  switch (aFormat) {
  case FORMAT_R5G6B5:
  case FORMAT_B8G8R8X8:
  case FORMAT_R8G8B8X8:
    return gfxASurface::CONTENT_COLOR;
  case FORMAT_A8:
    return gfxASurface::CONTENT_ALPHA;
  case FORMAT_B8G8R8A8:
  case FORMAT_R8G8B8A8:
  default:
    return gfxASurface::CONTENT_COLOR_ALPHA;
  }
}

inline CompositionOp CompositionOpForOp(gfxContext::GraphicsOperator aOp)
{
  switch (aOp) {
  case gfxContext::OPERATOR_ADD:
    return OP_ADD;
  case gfxContext::OPERATOR_ATOP:
    return OP_ATOP;
  case gfxContext::OPERATOR_IN:
    return OP_IN;
  case gfxContext::OPERATOR_OUT:
    return OP_OUT;
  case gfxContext::OPERATOR_SOURCE:
    return OP_SOURCE;
  case gfxContext::OPERATOR_DEST_IN:
    return OP_DEST_IN;
  case gfxContext::OPERATOR_DEST_OUT:
    return OP_DEST_OUT;
  case gfxContext::OPERATOR_DEST_ATOP:
    return OP_DEST_ATOP;
  case gfxContext::OPERATOR_XOR:
    return OP_XOR;
  case gfxContext::OPERATOR_MULTIPLY:
    return OP_MULTIPLY;
  case gfxContext::OPERATOR_SCREEN:
    return OP_SCREEN;
  case gfxContext::OPERATOR_OVERLAY:
    return OP_OVERLAY;
  case gfxContext::OPERATOR_DARKEN:
    return OP_DARKEN;
  case gfxContext::OPERATOR_LIGHTEN:
    return OP_LIGHTEN;
  case gfxContext::OPERATOR_COLOR_DODGE:
    return OP_COLOR_DODGE;
  case gfxContext::OPERATOR_COLOR_BURN:
    return OP_COLOR_BURN;
  case gfxContext::OPERATOR_HARD_LIGHT:
    return OP_HARD_LIGHT;
  case gfxContext::OPERATOR_SOFT_LIGHT:
    return OP_SOFT_LIGHT;
  case gfxContext::OPERATOR_DIFFERENCE:
    return OP_DIFFERENCE;
  case gfxContext::OPERATOR_EXCLUSION:
    return OP_EXCLUSION;
  case gfxContext::OPERATOR_HUE:
    return OP_HUE;
  case gfxContext::OPERATOR_SATURATION:
    return OP_SATURATION;
  case gfxContext::OPERATOR_COLOR:
    return OP_COLOR;
  case gfxContext::OPERATOR_LUMINOSITY:
    return OP_LUMINOSITY;
  default:
    return OP_OVER;
  }
}

inline gfxContext::GraphicsOperator ThebesOp(CompositionOp aOp)
{
  switch (aOp) {
  case OP_ADD:
    return gfxContext::OPERATOR_ADD;
  case OP_ATOP:
    return gfxContext::OPERATOR_ATOP;
  case OP_IN:
    return gfxContext::OPERATOR_IN;
  case OP_OUT:
    return gfxContext::OPERATOR_OUT;
  case OP_SOURCE:
    return gfxContext::OPERATOR_SOURCE;
  case OP_DEST_IN:
    return gfxContext::OPERATOR_DEST_IN;
  case OP_DEST_OUT:
    return gfxContext::OPERATOR_DEST_OUT;
  case OP_DEST_ATOP:
    return gfxContext::OPERATOR_DEST_ATOP;
  case OP_XOR:
    return gfxContext::OPERATOR_XOR;
  case OP_MULTIPLY:
    return gfxContext::OPERATOR_MULTIPLY;
  case OP_SCREEN:
    return gfxContext::OPERATOR_SCREEN;
  case OP_OVERLAY:
    return gfxContext::OPERATOR_OVERLAY;
  case OP_DARKEN:
    return gfxContext::OPERATOR_DARKEN;
  case OP_LIGHTEN:
    return gfxContext::OPERATOR_LIGHTEN;
  case OP_COLOR_DODGE:
    return gfxContext::OPERATOR_COLOR_DODGE;
  case OP_COLOR_BURN:
    return gfxContext::OPERATOR_COLOR_BURN;
  case OP_HARD_LIGHT:
    return gfxContext::OPERATOR_HARD_LIGHT;
  case OP_SOFT_LIGHT:
    return gfxContext::OPERATOR_SOFT_LIGHT;
  case OP_DIFFERENCE:
    return gfxContext::OPERATOR_DIFFERENCE;
  case OP_EXCLUSION:
    return gfxContext::OPERATOR_EXCLUSION;
  case OP_HUE:
    return gfxContext::OPERATOR_HUE;
  case OP_SATURATION:
    return gfxContext::OPERATOR_SATURATION;
  case OP_COLOR:
    return gfxContext::OPERATOR_COLOR;
  case OP_LUMINOSITY:
    return gfxContext::OPERATOR_LUMINOSITY;
  default:
    return gfxContext::OPERATOR_OVER;
  }
}

inline void
ToMatrix4x4(const gfx3DMatrix& aIn, Matrix4x4& aOut)
{
  aOut._11 = aIn._11;
  aOut._12 = aIn._12;
  aOut._13 = aIn._13;
  aOut._14 = aIn._14;
  aOut._21 = aIn._21;
  aOut._22 = aIn._22;
  aOut._23 = aIn._23;
  aOut._24 = aIn._24;
  aOut._31 = aIn._31;
  aOut._32 = aIn._32;
  aOut._33 = aIn._33;
  aOut._34 = aIn._34;
  aOut._41 = aIn._41;
  aOut._42 = aIn._42;
  aOut._43 = aIn._43;
  aOut._44 = aIn._44;
}

inline void
To3DMatrix(const Matrix4x4& aIn, gfx3DMatrix& aOut)
{
  aOut._11 = aIn._11;
  aOut._12 = aIn._12;
  aOut._13 = aIn._13;
  aOut._14 = aIn._14;
  aOut._21 = aIn._21;
  aOut._22 = aIn._22;
  aOut._23 = aIn._23;
  aOut._24 = aIn._24;
  aOut._31 = aIn._31;
  aOut._32 = aIn._32;
  aOut._33 = aIn._33;
  aOut._34 = aIn._34;
  aOut._41 = aIn._41;
  aOut._42 = aIn._42;
  aOut._43 = aIn._43;
  aOut._44 = aIn._44;
}

}
}

#endif
