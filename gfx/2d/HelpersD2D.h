/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_HELPERSD2D_H_
#define MOZILLA_GFX_HELPERSD2D_H_

#include "moz-d2d1-1.h"

#include <vector>

#include <dwrite.h>
#include "2D.h"
#include "Logging.h"

#include "ScaledFontDWrite.h"

namespace mozilla {
namespace gfx {

ID2D1Factory* D2DFactory();

static inline D2D1_POINT_2F D2DPoint(const Point &aPoint)
{
  return D2D1::Point2F(aPoint.x, aPoint.y);
}

static inline D2D1_SIZE_U D2DIntSize(const IntSize &aSize)
{
  return D2D1::SizeU(aSize.width, aSize.height);
}

static inline D2D1_RECT_F D2DRect(const Rect &aRect)
{
  return D2D1::RectF(aRect.x, aRect.y, aRect.XMost(), aRect.YMost());
}

static inline D2D1_EXTEND_MODE D2DExtend(ExtendMode aExtendMode)
{
  D2D1_EXTEND_MODE extend;
  switch (aExtendMode) {
  case EXTEND_REPEAT:
    extend = D2D1_EXTEND_MODE_WRAP;
    break;
  case EXTEND_REFLECT:
    extend = D2D1_EXTEND_MODE_MIRROR;
    break;
  default:
    extend = D2D1_EXTEND_MODE_CLAMP;
  }

  return extend;
}

static inline D2D1_BITMAP_INTERPOLATION_MODE D2DFilter(const Filter &aFilter)
{
  switch (aFilter) {
  case FILTER_POINT:
    return D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
  default:
    return D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
  }
}

static inline D2D1_ANTIALIAS_MODE D2DAAMode(AntialiasMode aMode)
{
  switch (aMode) {
  case AA_NONE:
    return D2D1_ANTIALIAS_MODE_ALIASED;
  default:
    return D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
  }
}

static inline D2D1_MATRIX_3X2_F D2DMatrix(const Matrix &aTransform)
{
  return D2D1::Matrix3x2F(aTransform._11, aTransform._12,
                          aTransform._21, aTransform._22,
                          aTransform._31, aTransform._32);
}

static inline D2D1_COLOR_F D2DColor(const Color &aColor)
{
  return D2D1::ColorF(aColor.r, aColor.g, aColor.b, aColor.a);
}

static inline IntSize ToIntSize(const D2D1_SIZE_U &aSize)
{
  return IntSize(aSize.width, aSize.height);
}

static inline SurfaceFormat ToPixelFormat(const D2D1_PIXEL_FORMAT &aFormat)
{
  switch(aFormat.format) {
  case DXGI_FORMAT_A8_UNORM:
    return FORMAT_A8;
  case DXGI_FORMAT_B8G8R8A8_UNORM:
    if (aFormat.alphaMode == D2D1_ALPHA_MODE_IGNORE) {
      return FORMAT_B8G8R8X8;
    } else {
      return FORMAT_B8G8R8A8;
    }
  default:
    return FORMAT_B8G8R8A8;
  }
}

static inline Rect ToRect(const D2D1_RECT_F &aRect)
{
  return Rect(aRect.left, aRect.top, aRect.right - aRect.left, aRect.bottom - aRect.top);
}

static inline DXGI_FORMAT DXGIFormat(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case FORMAT_B8G8R8A8:
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  case FORMAT_B8G8R8X8:
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  case FORMAT_A8:
    return DXGI_FORMAT_A8_UNORM;
  default:
    return DXGI_FORMAT_UNKNOWN;
  }
}

static inline D2D1_ALPHA_MODE AlphaMode(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case FORMAT_B8G8R8X8:
    return D2D1_ALPHA_MODE_IGNORE;
  default:
    return D2D1_ALPHA_MODE_PREMULTIPLIED;
  }
}

static inline D2D1_PIXEL_FORMAT D2DPixelFormat(SurfaceFormat aFormat)
{
  return D2D1::PixelFormat(DXGIFormat(aFormat), AlphaMode(aFormat));
}

static inline bool IsPatternSupportedByD2D(const Pattern &aPattern)
{
  if (aPattern.GetType() != PATTERN_RADIAL_GRADIENT) {
    return true;
  }

  const RadialGradientPattern *pat =
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
struct ShaderConstantRectD3D10
{
  float mX, mY, mWidth, mHeight;
  ShaderConstantRectD3D10(float aX, float aY, float aWidth, float aHeight)
    : mX(aX), mY(aY), mWidth(aWidth), mHeight(aHeight)
  { }

  // For easy passing to SetVertexShaderConstantF.
  operator float* () { return &mX; }
};

static inline DWRITE_MATRIX
DWriteMatrixFromMatrix(Matrix &aMatrix)
{
  DWRITE_MATRIX mat;
  mat.m11 = aMatrix._11;
  mat.m12 = aMatrix._12;
  mat.m21 = aMatrix._21;
  mat.m22 = aMatrix._22;
  mat.dx = aMatrix._31;
  mat.dy = aMatrix._32;
  return mat;
}

class AutoDWriteGlyphRun : public DWRITE_GLYPH_RUN
{
    static const unsigned kNumAutoGlyphs = 256;

public:
    AutoDWriteGlyphRun() {
        glyphCount = 0;
    }

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
    FLOAT               mAutoAdvances[kNumAutoGlyphs];
    UINT16              mAutoIndices[kNumAutoGlyphs];
};

static inline void
DWriteGlyphRunFromGlyphs(const GlyphBuffer &aGlyphs, ScaledFontDWrite *aFont, AutoDWriteGlyphRun *run)
{
  run->allocate(aGlyphs.mNumGlyphs);

  FLOAT *advances = const_cast<FLOAT*>(run->glyphAdvances);
  UINT16 *indices = const_cast<UINT16*>(run->glyphIndices);
  DWRITE_GLYPH_OFFSET *offsets = const_cast<DWRITE_GLYPH_OFFSET*>(run->glyphOffsets);

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

static TemporaryRef<ID2D1Geometry>
ConvertRectToGeometry(const D2D1_RECT_F& aRect)
{
  RefPtr<ID2D1RectangleGeometry> rectGeom;
  D2DFactory()->CreateRectangleGeometry(&aRect, byRef(rectGeom));
  return rectGeom.forget();
}

static TemporaryRef<ID2D1Geometry>
GetTransformedGeometry(ID2D1Geometry *aGeometry, const D2D1_MATRIX_3X2_F &aTransform)
{
  RefPtr<ID2D1PathGeometry> tmpGeometry;
  D2DFactory()->CreatePathGeometry(byRef(tmpGeometry));
  RefPtr<ID2D1GeometrySink> currentSink;
  tmpGeometry->Open(byRef(currentSink));
  aGeometry->Simplify(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
                      aTransform, currentSink);
  currentSink->Close();
  return tmpGeometry;
}

static TemporaryRef<ID2D1Geometry>
IntersectGeometry(ID2D1Geometry *aGeometryA, ID2D1Geometry *aGeometryB)
{
  RefPtr<ID2D1PathGeometry> pathGeom;
  D2DFactory()->CreatePathGeometry(byRef(pathGeom));
  RefPtr<ID2D1GeometrySink> sink;
  pathGeom->Open(byRef(sink));
  aGeometryA->CombineWithGeometry(aGeometryB, D2D1_COMBINE_MODE_INTERSECT, nullptr, sink);
  sink->Close();

  return pathGeom;
}

static TemporaryRef<ID2D1StrokeStyle>
CreateStrokeStyleForOptions(const StrokeOptions &aStrokeOptions)
{
  RefPtr<ID2D1StrokeStyle> style;

  D2D1_CAP_STYLE capStyle;
  D2D1_LINE_JOIN joinStyle;

  switch (aStrokeOptions.mLineCap) {
  case CAP_BUTT:
    capStyle = D2D1_CAP_STYLE_FLAT;
    break;
  case CAP_ROUND:
    capStyle = D2D1_CAP_STYLE_ROUND;
    break;
  case CAP_SQUARE:
    capStyle = D2D1_CAP_STYLE_SQUARE;
    break;
  }

  switch (aStrokeOptions.mLineJoin) {
  case JOIN_MITER:
    joinStyle = D2D1_LINE_JOIN_MITER;
    break;
  case JOIN_MITER_OR_BEVEL:
    joinStyle = D2D1_LINE_JOIN_MITER_OR_BEVEL;
    break;
  case JOIN_ROUND:
    joinStyle = D2D1_LINE_JOIN_ROUND;
    break;
  case JOIN_BEVEL:
    joinStyle = D2D1_LINE_JOIN_BEVEL;
    break;
  }


  HRESULT hr;
  if (aStrokeOptions.mDashPattern) {
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
      D2D1::StrokeStyleProperties(capStyle, capStyle,
                                  capStyle, joinStyle,
                                  aStrokeOptions.mMiterLimit,
                                  D2D1_DASH_STYLE_CUSTOM,
                                  aStrokeOptions.mDashOffset),
      &dash[0], // data() is not C++98, although it's in recent gcc
                // and VC10's STL
      dash.size(),
      byRef(style));
  } else {
    hr = D2DFactory()->CreateStrokeStyle(
      D2D1::StrokeStyleProperties(capStyle, capStyle,
                                  capStyle, joinStyle,
                                  aStrokeOptions.mMiterLimit),
      nullptr, 0, byRef(style));
  }

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create Direct2D stroke style.";
  }

  return style;
}

}
}

#endif /* MOZILLA_GFX_HELPERSD2D_H_ */
