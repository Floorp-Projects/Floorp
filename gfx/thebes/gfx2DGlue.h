
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "gfxContext.h"
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
  MOZ_NOT_REACHED("Incomplete switch");
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
  MOZ_NOT_REACHED("Incomplete switch");
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
  MOZ_NOT_REACHED("Incomplete switch");
}

inline gfxMatrix ThebesMatrix(const Matrix &aMatrix)
{
  return gfxMatrix(aMatrix._11, aMatrix._12, aMatrix._21,
                   aMatrix._22, aMatrix._31, aMatrix._32);
}

inline gfxASurface::gfxContentType ContentForFormat(const SurfaceFormat &aFormat)
{
  switch (aFormat) {
  case FORMAT_R5G6B5:
  case FORMAT_B8G8R8X8:
    return gfxASurface::CONTENT_COLOR;
  case FORMAT_A8:
    return gfxASurface::CONTENT_ALPHA;
  case FORMAT_B8G8R8A8:
  default:
    return gfxASurface::CONTENT_COLOR_ALPHA;
  }
}

inline SurfaceFormat FormatForContent(gfxASurface::gfxContentType aContent)
{
  switch (aContent) {
  case gfxASurface::CONTENT_COLOR:
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    return FORMAT_R5G6B5;
#else
    return FORMAT_B8G8R8X8;
#endif
  case gfxASurface::CONTENT_ALPHA:
    return FORMAT_A8;
  default:
    return FORMAT_B8G8R8A8;
  }
}

inline SurfaceFormat SurfaceFormatForImageFormat(gfxASurface::gfxImageFormat aFormat)
{
  return FormatForContent(gfxASurface::ContentFromFormat(aFormat));
}

inline gfxASurface::gfxImageFormat ImageFormatForSurfaceFormat(SurfaceFormat aFormat)
{
  return gfxASurface::FormatFromContent(mozilla::gfx::ContentForFormat(aFormat));
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
  default:
    return gfxContext::OPERATOR_OVER;
  }
}

}
}
