/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_HELPERSD2D_H_
#define MOZILLA_GFX_HELPERSD2D_H_

#include <d2d1_1.h>

#include <vector>

#include <dwrite.h>
#include <versionhelpers.h>
#include "2D.h"
#include "Logging.h"
#include "ImageScaling.h"

#include "ScaledFontDWrite.h"

#undef min
#undef max

namespace mozilla {
namespace gfx {

RefPtr<ID2D1Factory1> D2DFactory();

static inline D2D1_POINT_2F D2DPoint(const Point& aPoint) {
  return D2D1::Point2F(aPoint.x, aPoint.y);
}

static inline D2D1_SIZE_U D2DIntSize(const IntSize& aSize) {
  return D2D1::SizeU(aSize.width, aSize.height);
}

template <typename T>
static inline D2D1_RECT_F D2DRect(const T& aRect) {
  return D2D1::RectF(aRect.X(), aRect.Y(), aRect.XMost(), aRect.YMost());
}

static inline D2D1_ROUNDED_RECT D2DRoundedRect(const RoundedRect& aRect) {
  return D2D1::RoundedRect(D2DRect(aRect.rect),
                           aRect.corners.BottomLeft().width,
                           aRect.corners.BottomLeft().height);
}

static inline D2D1_EXTEND_MODE D2DExtend(ExtendMode aExtendMode, Axis aAxis) {
  D2D1_EXTEND_MODE extend;
  switch (aExtendMode) {
    case ExtendMode::REPEAT:
      extend = D2D1_EXTEND_MODE_WRAP;
      break;
    case ExtendMode::REPEAT_X: {
      extend = aAxis == Axis::X_AXIS ? D2D1_EXTEND_MODE_WRAP
                                     : D2D1_EXTEND_MODE_CLAMP;
      break;
    }
    case ExtendMode::REPEAT_Y: {
      extend = aAxis == Axis::Y_AXIS ? D2D1_EXTEND_MODE_WRAP
                                     : D2D1_EXTEND_MODE_CLAMP;
      break;
    }
    case ExtendMode::REFLECT:
      extend = D2D1_EXTEND_MODE_MIRROR;
      break;
    default:
      extend = D2D1_EXTEND_MODE_CLAMP;
  }

  return extend;
}

static inline D2D1_BITMAP_INTERPOLATION_MODE D2DFilter(
    const SamplingFilter aSamplingFilter) {
  switch (aSamplingFilter) {
    case SamplingFilter::POINT:
      return D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    default:
      return D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
  }
}

static inline D2D1_INTERPOLATION_MODE D2DInterpolationMode(
    const SamplingFilter aSamplingFilter) {
  switch (aSamplingFilter) {
    case SamplingFilter::POINT:
      return D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    default:
      return D2D1_INTERPOLATION_MODE_LINEAR;
  }
}

static inline D2D1_MATRIX_5X4_F D2DMatrix5x4(const Matrix5x4& aMatrix) {
  return D2D1::Matrix5x4F(aMatrix._11, aMatrix._12, aMatrix._13, aMatrix._14,
                          aMatrix._21, aMatrix._22, aMatrix._23, aMatrix._24,
                          aMatrix._31, aMatrix._32, aMatrix._33, aMatrix._34,
                          aMatrix._41, aMatrix._42, aMatrix._43, aMatrix._44,
                          aMatrix._51, aMatrix._52, aMatrix._53, aMatrix._54);
}

static inline D2D1_VECTOR_3F D2DVector3D(const Point3D& aPoint) {
  return D2D1::Vector3F(aPoint.x, aPoint.y, aPoint.z);
}

static inline D2D1_ANTIALIAS_MODE D2DAAMode(AntialiasMode aMode) {
  switch (aMode) {
    case AntialiasMode::NONE:
      return D2D1_ANTIALIAS_MODE_ALIASED;
    default:
      return D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
  }
}

static inline D2D1_MATRIX_3X2_F D2DMatrix(const Matrix& aTransform) {
  return D2D1::Matrix3x2F(aTransform._11, aTransform._12, aTransform._21,
                          aTransform._22, aTransform._31, aTransform._32);
}

static inline D2D1_COLOR_F D2DColor(const DeviceColor& aColor) {
  return D2D1::ColorF(aColor.r, aColor.g, aColor.b, aColor.a);
}

static inline IntSize ToIntSize(const D2D1_SIZE_U& aSize) {
  return IntSize(aSize.width, aSize.height);
}

static inline SurfaceFormat ToPixelFormat(const DXGI_FORMAT& aFormat) {
  switch (aFormat) {
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_R8_UNORM:
      return SurfaceFormat::A8;
    default:
      return SurfaceFormat::B8G8R8A8;
  }
}

static inline SurfaceFormat ToPixelFormat(const D2D1_PIXEL_FORMAT& aFormat) {
  switch (aFormat.format) {
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_R8_UNORM:
      return SurfaceFormat::A8;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      if (aFormat.alphaMode == D2D1_ALPHA_MODE_IGNORE) {
        return SurfaceFormat::B8G8R8X8;
      } else {
        return SurfaceFormat::B8G8R8A8;
      }
    default:
      return SurfaceFormat::B8G8R8A8;
  }
}

static inline Rect ToRect(const D2D1_RECT_F& aRect) {
  return Rect(aRect.left, aRect.top, aRect.right - aRect.left,
              aRect.bottom - aRect.top);
}

static inline Matrix ToMatrix(const D2D1_MATRIX_3X2_F& aTransform) {
  return Matrix(aTransform._11, aTransform._12, aTransform._21, aTransform._22,
                aTransform._31, aTransform._32);
}

static inline Point ToPoint(const D2D1_POINT_2F& aPoint) {
  return Point(aPoint.x, aPoint.y);
}

static inline DXGI_FORMAT DXGIFormat(SurfaceFormat aFormat) {
  switch (aFormat) {
    case SurfaceFormat::B8G8R8A8:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case SurfaceFormat::B8G8R8X8:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case SurfaceFormat::A8:
      return DXGI_FORMAT_A8_UNORM;
    default:
      return DXGI_FORMAT_UNKNOWN;
  }
}

static inline D2D1_ALPHA_MODE D2DAlphaModeForFormat(SurfaceFormat aFormat) {
  switch (aFormat) {
    case SurfaceFormat::B8G8R8X8:
      return D2D1_ALPHA_MODE_IGNORE;
    default:
      return D2D1_ALPHA_MODE_PREMULTIPLIED;
  }
}

static inline D2D1_PIXEL_FORMAT D2DPixelFormat(SurfaceFormat aFormat) {
  return D2D1::PixelFormat(DXGIFormat(aFormat), D2DAlphaModeForFormat(aFormat));
}

static inline bool D2DSupportsCompositeMode(CompositionOp aOp) {
  switch (aOp) {
    case CompositionOp::OP_OVER:
    case CompositionOp::OP_ADD:
    case CompositionOp::OP_ATOP:
    case CompositionOp::OP_OUT:
    case CompositionOp::OP_IN:
    case CompositionOp::OP_SOURCE:
    case CompositionOp::OP_DEST_IN:
    case CompositionOp::OP_DEST_OUT:
    case CompositionOp::OP_DEST_OVER:
    case CompositionOp::OP_DEST_ATOP:
    case CompositionOp::OP_XOR:
      return true;
    default:
      return false;
  }
}

static inline D2D1_COMPOSITE_MODE D2DCompositionMode(CompositionOp aOp) {
  switch (aOp) {
    case CompositionOp::OP_OVER:
      return D2D1_COMPOSITE_MODE_SOURCE_OVER;
    case CompositionOp::OP_ADD:
      return D2D1_COMPOSITE_MODE_PLUS;
    case CompositionOp::OP_ATOP:
      return D2D1_COMPOSITE_MODE_SOURCE_ATOP;
    case CompositionOp::OP_OUT:
      return D2D1_COMPOSITE_MODE_SOURCE_OUT;
    case CompositionOp::OP_IN:
      return D2D1_COMPOSITE_MODE_SOURCE_IN;
    case CompositionOp::OP_SOURCE:
      return D2D1_COMPOSITE_MODE_SOURCE_COPY;
    case CompositionOp::OP_DEST_IN:
      return D2D1_COMPOSITE_MODE_DESTINATION_IN;
    case CompositionOp::OP_DEST_OUT:
      return D2D1_COMPOSITE_MODE_DESTINATION_OUT;
    case CompositionOp::OP_DEST_OVER:
      return D2D1_COMPOSITE_MODE_DESTINATION_OVER;
    case CompositionOp::OP_DEST_ATOP:
      return D2D1_COMPOSITE_MODE_DESTINATION_ATOP;
    case CompositionOp::OP_XOR:
      return D2D1_COMPOSITE_MODE_XOR;
    default:
      return D2D1_COMPOSITE_MODE_SOURCE_OVER;
  }
}

static inline D2D1_BLEND_MODE D2DBlendMode(CompositionOp aOp) {
  switch (aOp) {
    case CompositionOp::OP_MULTIPLY:
      return D2D1_BLEND_MODE_MULTIPLY;
    case CompositionOp::OP_SCREEN:
      return D2D1_BLEND_MODE_SCREEN;
    case CompositionOp::OP_OVERLAY:
      return D2D1_BLEND_MODE_OVERLAY;
    case CompositionOp::OP_DARKEN:
      return D2D1_BLEND_MODE_DARKEN;
    case CompositionOp::OP_LIGHTEN:
      return D2D1_BLEND_MODE_LIGHTEN;
    case CompositionOp::OP_COLOR_DODGE:
      return D2D1_BLEND_MODE_COLOR_DODGE;
    case CompositionOp::OP_COLOR_BURN:
      return D2D1_BLEND_MODE_COLOR_BURN;
    case CompositionOp::OP_HARD_LIGHT:
      return D2D1_BLEND_MODE_HARD_LIGHT;
    case CompositionOp::OP_SOFT_LIGHT:
      return D2D1_BLEND_MODE_SOFT_LIGHT;
    case CompositionOp::OP_DIFFERENCE:
      return D2D1_BLEND_MODE_DIFFERENCE;
    case CompositionOp::OP_EXCLUSION:
      return D2D1_BLEND_MODE_EXCLUSION;
    case CompositionOp::OP_HUE:
      return D2D1_BLEND_MODE_HUE;
    case CompositionOp::OP_SATURATION:
      return D2D1_BLEND_MODE_SATURATION;
    case CompositionOp::OP_COLOR:
      return D2D1_BLEND_MODE_COLOR;
    case CompositionOp::OP_LUMINOSITY:
      return D2D1_BLEND_MODE_LUMINOSITY;
    default:
      return D2D1_BLEND_MODE_MULTIPLY;
  }
}

static inline bool D2DSupportsPrimitiveBlendMode(CompositionOp aOp) {
  switch (aOp) {
    case CompositionOp::OP_OVER:
      //  case CompositionOp::OP_SOURCE:
      return true;
      //  case CompositionOp::OP_DARKEN:
    case CompositionOp::OP_ADD:
      return IsWindows8Point1OrGreater();
    default:
      return false;
  }
}

static inline D2D1_PRIMITIVE_BLEND D2DPrimitiveBlendMode(CompositionOp aOp) {
  switch (aOp) {
    case CompositionOp::OP_OVER:
      return D2D1_PRIMITIVE_BLEND_SOURCE_OVER;
    // D2D1_PRIMITIVE_BLEND_COPY should leave pixels out of the source's
    // bounds unchanged, but doesn't- breaking unbounded ops.
    // D2D1_PRIMITIVE_BLEND_MIN doesn't quite work like darken either, as it
    // accounts for the source alpha.
    //
    // case CompositionOp::OP_SOURCE:
    //   return D2D1_PRIMITIVE_BLEND_COPY;
    // case CompositionOp::OP_DARKEN:
    //   return D2D1_PRIMITIVE_BLEND_MIN;
    case CompositionOp::OP_ADD:
      return D2D1_PRIMITIVE_BLEND_ADD;
    default:
      return D2D1_PRIMITIVE_BLEND_SOURCE_OVER;
  }
}

static inline bool IsPatternSupportedByD2D(const Pattern& aPattern) {
  if (aPattern.GetType() == PatternType::CONIC_GRADIENT) {
    return false;
  }

  if (aPattern.GetType() != PatternType::RADIAL_GRADIENT) {
    return true;
  }

  const RadialGradientPattern* pat =
      static_cast<const RadialGradientPattern*>(&aPattern);

  if (pat->mRadius1 != 0) {
    return false;
  }

  Point diff = pat->mCenter2 - pat->mCenter1;

  if (sqrt(diff.x * diff.x + diff.y * diff.y) >= pat->mRadius2) {
    // Inner point lies outside the circle.
    return false;
  }

  return true;
}

/**
 * This structure is used to pass rectangles to our shader constant. We can use
 * this for passing rectangular areas to SetVertexShaderConstant. In the format
 * of a 4 component float(x,y,width,height). Our vertex shader can then use
 * this to construct rectangular positions from the 0,0-1,1 quad that we source
 * it with.
 */
struct ShaderConstantRectD3D10 {
  float mX, mY, mWidth, mHeight;
  ShaderConstantRectD3D10(float aX, float aY, float aWidth, float aHeight)
      : mX(aX), mY(aY), mWidth(aWidth), mHeight(aHeight) {}

  // For easy passing to SetVertexShaderConstantF.
  operator float*() { return &mX; }
};

static inline DWRITE_MATRIX DWriteMatrixFromMatrix(Matrix& aMatrix) {
  DWRITE_MATRIX mat;
  mat.m11 = aMatrix._11;
  mat.m12 = aMatrix._12;
  mat.m21 = aMatrix._21;
  mat.m22 = aMatrix._22;
  mat.dx = aMatrix._31;
  mat.dy = aMatrix._32;
  return mat;
}

class AutoDWriteGlyphRun : public DWRITE_GLYPH_RUN {
  static const unsigned kNumAutoGlyphs = 256;

 public:
  AutoDWriteGlyphRun() { glyphCount = 0; }

  ~AutoDWriteGlyphRun() {
    if (glyphCount > kNumAutoGlyphs) {
      delete[] glyphIndices;
      delete[] glyphAdvances;
      delete[] glyphOffsets;
    }
  }

  void allocate(unsigned aNumGlyphs) {
    glyphCount = aNumGlyphs;
    if (aNumGlyphs <= kNumAutoGlyphs) {
      glyphIndices = &mAutoIndices[0];
      glyphAdvances = &mAutoAdvances[0];
      glyphOffsets = &mAutoOffsets[0];
    } else {
      glyphIndices = new UINT16[aNumGlyphs];
      glyphAdvances = new FLOAT[aNumGlyphs];
      glyphOffsets = new DWRITE_GLYPH_OFFSET[aNumGlyphs];
    }
  }

 private:
  DWRITE_GLYPH_OFFSET mAutoOffsets[kNumAutoGlyphs];
  FLOAT mAutoAdvances[kNumAutoGlyphs];
  UINT16 mAutoIndices[kNumAutoGlyphs];
};

static inline void DWriteGlyphRunFromGlyphs(const GlyphBuffer& aGlyphs,
                                            ScaledFontDWrite* aFont,
                                            AutoDWriteGlyphRun* run) {
  run->allocate(aGlyphs.mNumGlyphs);

  FLOAT* advances = const_cast<FLOAT*>(run->glyphAdvances);
  UINT16* indices = const_cast<UINT16*>(run->glyphIndices);
  DWRITE_GLYPH_OFFSET* offsets =
      const_cast<DWRITE_GLYPH_OFFSET*>(run->glyphOffsets);

  memset(advances, 0, sizeof(FLOAT) * aGlyphs.mNumGlyphs);
  for (unsigned int i = 0; i < aGlyphs.mNumGlyphs; i++) {
    indices[i] = aGlyphs.mGlyphs[i].mIndex;
    offsets[i].advanceOffset = aGlyphs.mGlyphs[i].mPosition.x;
    offsets[i].ascenderOffset = -aGlyphs.mGlyphs[i].mPosition.y;
  }

  run->bidiLevel = 0;
  run->fontFace = aFont->mFontFace;
  run->fontEmSize = aFont->GetSize();
  run->glyphCount = aGlyphs.mNumGlyphs;
  run->isSideways = FALSE;
}

static inline already_AddRefed<ID2D1Geometry> ConvertRectToGeometry(
    const D2D1_RECT_F& aRect) {
  RefPtr<ID2D1RectangleGeometry> rectGeom;
  D2DFactory()->CreateRectangleGeometry(&aRect, getter_AddRefs(rectGeom));
  return rectGeom.forget();
}

static inline already_AddRefed<ID2D1Geometry> GetTransformedGeometry(
    ID2D1Geometry* aGeometry, const D2D1_MATRIX_3X2_F& aTransform) {
  RefPtr<ID2D1PathGeometry> tmpGeometry;
  D2DFactory()->CreatePathGeometry(getter_AddRefs(tmpGeometry));
  RefPtr<ID2D1GeometrySink> currentSink;
  tmpGeometry->Open(getter_AddRefs(currentSink));
  aGeometry->Simplify(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
                      aTransform, currentSink);
  currentSink->Close();
  return tmpGeometry.forget();
}

static inline already_AddRefed<ID2D1Geometry> IntersectGeometry(
    ID2D1Geometry* aGeometryA, ID2D1Geometry* aGeometryB) {
  RefPtr<ID2D1PathGeometry> pathGeom;
  D2DFactory()->CreatePathGeometry(getter_AddRefs(pathGeom));
  RefPtr<ID2D1GeometrySink> sink;
  pathGeom->Open(getter_AddRefs(sink));
  aGeometryA->CombineWithGeometry(aGeometryB, D2D1_COMBINE_MODE_INTERSECT,
                                  nullptr, sink);
  sink->Close();

  return pathGeom.forget();
}

static inline already_AddRefed<ID2D1StrokeStyle> CreateStrokeStyleForOptions(
    const StrokeOptions& aStrokeOptions) {
  RefPtr<ID2D1StrokeStyle> style;

  D2D1_CAP_STYLE capStyle;
  D2D1_LINE_JOIN joinStyle;

  switch (aStrokeOptions.mLineCap) {
    case CapStyle::BUTT:
      capStyle = D2D1_CAP_STYLE_FLAT;
      break;
    case CapStyle::ROUND:
      capStyle = D2D1_CAP_STYLE_ROUND;
      break;
    case CapStyle::SQUARE:
      capStyle = D2D1_CAP_STYLE_SQUARE;
      break;
  }

  switch (aStrokeOptions.mLineJoin) {
    case JoinStyle::MITER:
      joinStyle = D2D1_LINE_JOIN_MITER;
      break;
    case JoinStyle::MITER_OR_BEVEL:
      joinStyle = D2D1_LINE_JOIN_MITER_OR_BEVEL;
      break;
    case JoinStyle::ROUND:
      joinStyle = D2D1_LINE_JOIN_ROUND;
      break;
    case JoinStyle::BEVEL:
      joinStyle = D2D1_LINE_JOIN_BEVEL;
      break;
  }

  HRESULT hr;
  // We need to check mDashLength in addition to mDashPattern here since if
  // mDashPattern is set but mDashLength is zero then the stroke will fail to
  // paint.
  if (aStrokeOptions.mDashLength > 0 && aStrokeOptions.mDashPattern) {
    typedef std::vector<Float> FloatVector;
    // D2D "helpfully" multiplies the dash pattern by the line width.
    // That's not what cairo does, or is what <canvas>'s dash wants.
    // So fix the multiplication in advance.
    Float lineWidth = aStrokeOptions.mLineWidth;
    FloatVector dash(aStrokeOptions.mDashPattern,
                     aStrokeOptions.mDashPattern + aStrokeOptions.mDashLength);
    for (FloatVector::iterator it = dash.begin(); it != dash.end(); ++it) {
      *it /= lineWidth;
    }

    hr = D2DFactory()->CreateStrokeStyle(
        D2D1::StrokeStyleProperties(
            capStyle, capStyle, capStyle, joinStyle, aStrokeOptions.mMiterLimit,
            D2D1_DASH_STYLE_CUSTOM, aStrokeOptions.mDashOffset / lineWidth),
        &dash[0],  // data() is not C++98, although it's in recent gcc
                   // and VC10's STL
        dash.size(), getter_AddRefs(style));
  } else {
    hr = D2DFactory()->CreateStrokeStyle(
        D2D1::StrokeStyleProperties(capStyle, capStyle, capStyle, joinStyle,
                                    aStrokeOptions.mMiterLimit),
        nullptr, 0, getter_AddRefs(style));
  }

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create Direct2D stroke style.";
  }

  return style.forget();
}

// This creates a (partially) uploaded bitmap for a DataSourceSurface. It
// uploads the minimum requirement and possibly downscales. It adjusts the
// input Matrix to compensate.
static inline already_AddRefed<ID2D1Bitmap> CreatePartialBitmapForSurface(
    DataSourceSurface* aSurface, const Matrix& aDestinationTransform,
    const IntSize& aDestinationSize, ExtendMode aExtendMode,
    Matrix& aSourceTransform, ID2D1RenderTarget* aRT,
    const IntRect* aSourceRect = nullptr) {
  RefPtr<ID2D1Bitmap> bitmap;

  // This is where things get complicated. The source surface was
  // created for a surface that was too large to fit in a texture.
  // We'll need to figure out if we can work with a partial upload
  // or downsample in software.

  Matrix transform = aDestinationTransform;
  Matrix invTransform = transform = aSourceTransform * transform;
  if (!invTransform.Invert()) {
    // Singular transform, nothing to be drawn.
    return nullptr;
  }

  Rect rect(0, 0, Float(aDestinationSize.width),
            Float(aDestinationSize.height));

  // Calculate the rectangle of the source mapped to our surface.
  rect = invTransform.TransformBounds(rect);
  rect.RoundOut();

  IntSize size = aSurface->GetSize();

  Rect uploadRect(0, 0, Float(size.width), Float(size.height));
  if (aSourceRect) {
    uploadRect = Rect(aSourceRect->X(), aSourceRect->Y(), aSourceRect->Width(),
                      aSourceRect->Height());
  }

  // Limit the uploadRect as much as possible without supporting discontiguous
  // uploads
  //
  // clang-format off
  //                               region we will paint from
  //   uploadRect
  //   .---------------.              .---------------.         resulting uploadRect
  //   |               |rect          |               |
  //   |          .---------.         .----.     .----.          .---------------.
  //   |          |         |  ---->  |    |     |    |   ---->  |               |
  //   |          '---------'         '----'     '----'          '---------------'
  //   '---------------'              '---------------'
  // clang-format on
  //
  //

  int Bpp = BytesPerPixel(aSurface->GetFormat());

  if (uploadRect.Contains(rect)) {
    // Extend mode is irrelevant, the displayed rect is completely contained
    // by the source bitmap.
    uploadRect = rect;
  } else if (aExtendMode == ExtendMode::CLAMP && uploadRect.Intersects(rect)) {
    // Calculate the rectangle on the source bitmap that touches our
    // surface, and upload that, for ExtendMode::CLAMP we can actually guarantee
    // correct behaviour in this case.
    uploadRect = uploadRect.Intersect(rect);

    // We now proceed to check if we can limit at least one dimension of the
    // upload rect safely without looking at extend mode.
  } else if (rect.X() >= 0 && rect.XMost() < size.width) {
    uploadRect.MoveToX(rect.X());
    uploadRect.SetWidth(rect.Width());
  } else if (rect.Y() >= 0 && rect.YMost() < size.height) {
    uploadRect.MoveToY(rect.Y());
    uploadRect.SetHeight(rect.Height());
  }

  if (uploadRect.IsEmpty()) {
    // Nothing to be drawn.
    return nullptr;
  }

  if (uploadRect.Width() <= aRT->GetMaximumBitmapSize() &&
      uploadRect.Height() <= aRT->GetMaximumBitmapSize()) {
    {
      // Scope to auto-Unmap() |mapping|.
      DataSourceSurface::ScopedMap mapping(aSurface, DataSourceSurface::READ);
      if (MOZ2D_WARN_IF(!mapping.IsMapped())) {
        return nullptr;
      }

      // A partial upload will suffice.
      aRT->CreateBitmap(
          D2D1::SizeU(uint32_t(uploadRect.Width()),
                      uint32_t(uploadRect.Height())),
          mapping.GetData() + int(uploadRect.X()) * Bpp +
              int(uploadRect.Y()) * mapping.GetStride(),
          mapping.GetStride(),
          D2D1::BitmapProperties(D2DPixelFormat(aSurface->GetFormat())),
          getter_AddRefs(bitmap));
    }

    aSourceTransform.PreTranslate(uploadRect.X(), uploadRect.Y());

    return bitmap.forget();
  } else {
    if (Bpp != 4) {
      // This shouldn't actually happen in practice!
      MOZ_ASSERT(false);
      return nullptr;
    }

    {
      // Scope to auto-Unmap() |mapping|.
      DataSourceSurface::ScopedMap mapping(aSurface, DataSourceSurface::READ);
      if (MOZ2D_WARN_IF(!mapping.IsMapped())) {
        return nullptr;
      }
      ImageHalfScaler scaler(mapping.GetData(), mapping.GetStride(), size);

      // Calculate the maximum width/height of the image post transform.
      Point topRight = transform.TransformPoint(Point(Float(size.width), 0));
      Point topLeft = transform.TransformPoint(Point(0, 0));
      Point bottomRight = transform.TransformPoint(
          Point(Float(size.width), Float(size.height)));
      Point bottomLeft = transform.TransformPoint(Point(0, Float(size.height)));

      IntSize scaleSize;

      scaleSize.width = int32_t(std::max(Distance(topRight, topLeft),
                                         Distance(bottomRight, bottomLeft)));
      scaleSize.height = int32_t(std::max(Distance(topRight, bottomRight),
                                          Distance(topLeft, bottomLeft)));

      if (unsigned(scaleSize.width) > aRT->GetMaximumBitmapSize()) {
        // Ok, in this case we'd really want a downscale of a part of the
        // bitmap, perhaps we can do this later but for simplicity let's do
        // something different here and assume it's good enough, this should be
        // rare!
        scaleSize.width = 4095;
      }
      if (unsigned(scaleSize.height) > aRT->GetMaximumBitmapSize()) {
        scaleSize.height = 4095;
      }

      scaler.ScaleForSize(scaleSize);

      IntSize newSize = scaler.GetSize();

      if (newSize.IsEmpty()) {
        return nullptr;
      }

      aRT->CreateBitmap(
          D2D1::SizeU(newSize.width, newSize.height), scaler.GetScaledData(),
          scaler.GetStride(),
          D2D1::BitmapProperties(D2DPixelFormat(aSurface->GetFormat())),
          getter_AddRefs(bitmap));

      aSourceTransform.PreScale(Float(size.width) / newSize.width,
                                Float(size.height) / newSize.height);
    }
    return bitmap.forget();
  }
}

static inline void AddRectToSink(ID2D1GeometrySink* aSink,
                                 const D2D1_RECT_F& aRect) {
  aSink->BeginFigure(D2D1::Point2F(aRect.left, aRect.top),
                     D2D1_FIGURE_BEGIN_FILLED);
  aSink->AddLine(D2D1::Point2F(aRect.right, aRect.top));
  aSink->AddLine(D2D1::Point2F(aRect.right, aRect.bottom));
  aSink->AddLine(D2D1::Point2F(aRect.left, aRect.bottom));
  aSink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

class DCCommandSink : public ID2D1CommandSink {
 public:
  explicit DCCommandSink(ID2D1DeviceContext* aCtx) : mCtx(aCtx) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(const IID& aIID, void** aPtr) {
    if (!aPtr) {
      return E_POINTER;
    }

    if (aIID == IID_IUnknown) {
      *aPtr = static_cast<IUnknown*>(this);
      return S_OK;
    } else if (aIID == IID_ID2D1CommandSink) {
      *aPtr = static_cast<ID2D1CommandSink*>(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() { return 1; }

  ULONG STDMETHODCALLTYPE Release() { return 1; }

  STDMETHODIMP BeginDraw() {
    // We don't want to do anything here!
    return S_OK;
  }
  STDMETHODIMP EndDraw() {
    // We don't want to do anything here!
    return S_OK;
  }

  STDMETHODIMP SetAntialiasMode(D2D1_ANTIALIAS_MODE antialiasMode) {
    mCtx->SetAntialiasMode(antialiasMode);
    return S_OK;
  }

  STDMETHODIMP SetTags(D2D1_TAG tag1, D2D1_TAG tag2) {
    mCtx->SetTags(tag1, tag2);
    return S_OK;
  }

  STDMETHODIMP SetTextAntialiasMode(
      D2D1_TEXT_ANTIALIAS_MODE textAntialiasMode) {
    mCtx->SetTextAntialiasMode(textAntialiasMode);
    return S_OK;
  }

  STDMETHODIMP SetTextRenderingParams(
      _In_opt_ IDWriteRenderingParams* textRenderingParams) {
    mCtx->SetTextRenderingParams(textRenderingParams);
    return S_OK;
  }

  STDMETHODIMP SetTransform(_In_ CONST D2D1_MATRIX_3X2_F* transform) {
    mCtx->SetTransform(transform);
    return S_OK;
  }

  STDMETHODIMP SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND primitiveBlend) {
    mCtx->SetPrimitiveBlend(primitiveBlend);
    return S_OK;
  }

  STDMETHODIMP SetUnitMode(D2D1_UNIT_MODE unitMode) {
    mCtx->SetUnitMode(unitMode);
    return S_OK;
  }

  STDMETHODIMP Clear(_In_opt_ CONST D2D1_COLOR_F* color) {
    mCtx->Clear(color);
    return S_OK;
  }

  STDMETHODIMP DrawGlyphRun(
      D2D1_POINT_2F baselineOrigin, _In_ CONST DWRITE_GLYPH_RUN* glyphRun,
      _In_opt_ CONST DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
      _In_ ID2D1Brush* foregroundBrush, DWRITE_MEASURING_MODE measuringMode) {
    mCtx->DrawGlyphRun(baselineOrigin, glyphRun, glyphRunDescription,
                       foregroundBrush, measuringMode);
    return S_OK;
  }

  STDMETHODIMP DrawLine(D2D1_POINT_2F point0, D2D1_POINT_2F point1,
                        _In_ ID2D1Brush* brush, FLOAT strokeWidth,
                        _In_opt_ ID2D1StrokeStyle* strokeStyle) {
    mCtx->DrawLine(point0, point1, brush, strokeWidth, strokeStyle);
    return S_OK;
  }

  STDMETHODIMP DrawGeometry(_In_ ID2D1Geometry* geometry,
                            _In_ ID2D1Brush* brush, FLOAT strokeWidth,
                            _In_opt_ ID2D1StrokeStyle* strokeStyle) {
    mCtx->DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
    return S_OK;
  }

  STDMETHODIMP DrawRectangle(_In_ CONST D2D1_RECT_F* rect,
                             _In_ ID2D1Brush* brush, FLOAT strokeWidth,
                             _In_opt_ ID2D1StrokeStyle* strokeStyle) {
    mCtx->DrawRectangle(rect, brush, strokeWidth, strokeStyle);
    return S_OK;
  }

  STDMETHODIMP DrawBitmap(
      _In_ ID2D1Bitmap* bitmap,
      _In_opt_ CONST D2D1_RECT_F* destinationRectangle, FLOAT opacity,
      D2D1_INTERPOLATION_MODE interpolationMode,
      _In_opt_ CONST D2D1_RECT_F* sourceRectangle,
      _In_opt_ CONST D2D1_MATRIX_4X4_F* perspectiveTransform) {
    mCtx->DrawBitmap(bitmap, destinationRectangle, opacity, interpolationMode,
                     sourceRectangle, perspectiveTransform);
    return S_OK;
  }

  STDMETHODIMP DrawImage(_In_ ID2D1Image* image,
                         _In_opt_ CONST D2D1_POINT_2F* targetOffset,
                         _In_opt_ CONST D2D1_RECT_F* imageRectangle,
                         D2D1_INTERPOLATION_MODE interpolationMode,
                         D2D1_COMPOSITE_MODE compositeMode) {
    mCtx->DrawImage(image, targetOffset, imageRectangle, interpolationMode,
                    compositeMode);
    return S_OK;
  }

  STDMETHODIMP DrawGdiMetafile(_In_ ID2D1GdiMetafile* gdiMetafile,
                               _In_opt_ CONST D2D1_POINT_2F* targetOffset) {
    mCtx->DrawGdiMetafile(gdiMetafile, targetOffset);
    return S_OK;
  }

  STDMETHODIMP FillMesh(_In_ ID2D1Mesh* mesh, _In_ ID2D1Brush* brush) {
    mCtx->FillMesh(mesh, brush);
    return S_OK;
  }

  STDMETHODIMP FillOpacityMask(_In_ ID2D1Bitmap* opacityMask,
                               _In_ ID2D1Brush* brush,
                               _In_opt_ CONST D2D1_RECT_F* destinationRectangle,
                               _In_opt_ CONST D2D1_RECT_F* sourceRectangle) {
    mCtx->FillOpacityMask(opacityMask, brush, destinationRectangle,
                          sourceRectangle);
    return S_OK;
  }

  STDMETHODIMP FillGeometry(_In_ ID2D1Geometry* geometry,
                            _In_ ID2D1Brush* brush,
                            _In_opt_ ID2D1Brush* opacityBrush) {
    mCtx->FillGeometry(geometry, brush, opacityBrush);
    return S_OK;
  }

  STDMETHODIMP FillRectangle(_In_ CONST D2D1_RECT_F* rect,
                             _In_ ID2D1Brush* brush) {
    mCtx->FillRectangle(rect, brush);
    return S_OK;
  }

  STDMETHODIMP PushAxisAlignedClip(_In_ CONST D2D1_RECT_F* clipRect,
                                   D2D1_ANTIALIAS_MODE antialiasMode) {
    mCtx->PushAxisAlignedClip(clipRect, antialiasMode);
    return S_OK;
  }

  STDMETHODIMP PushLayer(_In_ CONST D2D1_LAYER_PARAMETERS1* layerParameters1,
                         _In_opt_ ID2D1Layer* layer) {
    mCtx->PushLayer(layerParameters1, layer);
    return S_OK;
  }

  STDMETHODIMP PopAxisAlignedClip() {
    mCtx->PopAxisAlignedClip();
    return S_OK;
  }

  STDMETHODIMP PopLayer() {
    mCtx->PopLayer();
    return S_OK;
  }

  ID2D1DeviceContext* mCtx;
};

class MOZ_STACK_CLASS AutoRestoreFP final {
 public:
  AutoRestoreFP() {
    // save the current floating point control word
    _controlfp_s(&savedFPSetting, 0, 0);
    UINT unused;
    // set the floating point control word to its default value
    _controlfp_s(&unused, _CW_DEFAULT, MCW_PC);
  }
  ~AutoRestoreFP() {
    UINT unused;
    // restore the saved floating point control word
    _controlfp_s(&unused, savedFPSetting, MCW_PC);
  }

 private:
  UINT savedFPSetting;
};

// Note that overrides of ID2D1SimplifiedGeometrySink methods in this class may
// get called from D2D with nonstandard floating point settings (see comments in
// bug 1134549) - use AutoRestoreFP to reset the floating point control word to
// what we expect
class StreamingGeometrySink : public ID2D1SimplifiedGeometrySink {
 public:
  explicit StreamingGeometrySink(PathSink* aSink) : mSink(aSink) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(const IID& aIID, void** aPtr) {
    if (!aPtr) {
      return E_POINTER;
    }

    if (aIID == IID_IUnknown) {
      *aPtr = static_cast<IUnknown*>(this);
      return S_OK;
    } else if (aIID == IID_ID2D1SimplifiedGeometrySink) {
      *aPtr = static_cast<ID2D1SimplifiedGeometrySink*>(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() { return 1; }

  ULONG STDMETHODCALLTYPE Release() { return 1; }

  // We ignore SetFillMode, this depends on the destination sink.
  STDMETHOD_(void, SetFillMode)(D2D1_FILL_MODE aMode) { return; }
  STDMETHOD_(void, BeginFigure)
  (D2D1_POINT_2F aPoint, D2D1_FIGURE_BEGIN aBegin) {
    AutoRestoreFP resetFloatingPoint;
    mSink->MoveTo(ToPoint(aPoint));
  }
  STDMETHOD_(void, AddLines)(const D2D1_POINT_2F* aLines, UINT aCount) {
    AutoRestoreFP resetFloatingPoint;
    for (UINT i = 0; i < aCount; i++) {
      mSink->LineTo(ToPoint(aLines[i]));
    }
  }
  STDMETHOD_(void, AddBeziers)
  (const D2D1_BEZIER_SEGMENT* aSegments, UINT aCount) {
    AutoRestoreFP resetFloatingPoint;
    for (UINT i = 0; i < aCount; i++) {
      mSink->BezierTo(ToPoint(aSegments[i].point1),
                      ToPoint(aSegments[i].point2),
                      ToPoint(aSegments[i].point3));
    }
  }
  STDMETHOD(Close)() { /* Should never be called! */
    return S_OK;
  }
  STDMETHOD_(void, SetSegmentFlags)
  (D2D1_PATH_SEGMENT aFlags) { /* Should never be called! */
  }

  STDMETHOD_(void, EndFigure)(D2D1_FIGURE_END aEnd) {
    AutoRestoreFP resetFloatingPoint;
    if (aEnd == D2D1_FIGURE_END_CLOSED) {
      return mSink->Close();
    }
  }

 private:
  PathSink* mSink;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_HELPERSD2D_H_ */
