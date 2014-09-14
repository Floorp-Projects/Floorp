/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxTypes.h"
#include "gfxPattern.h"
#include "gfxASurface.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "gfxGradientCache.h"
#include "mozilla/gfx/2D.h"

#include "cairo.h"

#include <vector>

using namespace mozilla::gfx;

gfxPattern::gfxPattern(const gfxRGBA& aColor)
  : mExtend(EXTEND_NONE)
{
  mGfxPattern =
    new (mColorPattern.addr()) ColorPattern(Color(aColor.r, aColor.g, aColor.b, aColor.a));
}

// linear
gfxPattern::gfxPattern(gfxFloat x0, gfxFloat y0, gfxFloat x1, gfxFloat y1)
  : mExtend(EXTEND_NONE)
{
  mGfxPattern =
    new (mLinearGradientPattern.addr()) LinearGradientPattern(Point(x0, y0),
                                                              Point(x1, y1),
                                                              nullptr);
}

// radial
gfxPattern::gfxPattern(gfxFloat cx0, gfxFloat cy0, gfxFloat radius0,
                       gfxFloat cx1, gfxFloat cy1, gfxFloat radius1)
  : mExtend(EXTEND_NONE)
{
  mGfxPattern =
    new (mRadialGradientPattern.addr()) RadialGradientPattern(Point(cx0, cy0),
                                                              Point(cx1, cy1),
                                                              radius0, radius1,
                                                              nullptr);
}

// Azure
gfxPattern::gfxPattern(SourceSurface *aSurface, const Matrix &aTransform)
  : mTransform(aTransform)
  , mExtend(EXTEND_NONE)
{
  mGfxPattern = new (mSurfacePattern.addr())
    SurfacePattern(aSurface, ToExtendMode(mExtend), aTransform,
                   mozilla::gfx::Filter::GOOD);
}

gfxPattern::~gfxPattern()
{
  if (mGfxPattern) {
    mGfxPattern->~Pattern();
  }
}

void
gfxPattern::AddColorStop(gfxFloat offset, const gfxRGBA& c)
{
  if (mGfxPattern->GetType() != PatternType::LINEAR_GRADIENT &&
      mGfxPattern->GetType() != PatternType::RADIAL_GRADIENT) {
    return;
  }

  mStops = nullptr;
  gfxRGBA color = c;
  if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
    qcms_transform *transform = gfxPlatform::GetCMSRGBTransform();
    if (transform) {
      gfxPlatform::TransformPixel(color, color, transform);
    }
  }

  GradientStop stop;
  stop.offset = offset;
  stop.color = ToColor(color);
  mStopsList.AppendElement(stop);
}

void
gfxPattern::SetColorStops(GradientStops* aStops)
{
  mStops = aStops;
}

void
gfxPattern::CacheColorStops(DrawTarget *aDT)
{
  mStops = gfxGradientCache::GetOrCreateGradientStops(aDT, mStopsList,
                                                      ToExtendMode(mExtend));
}

void
gfxPattern::SetMatrix(const gfxMatrix& matrix)
{
  mTransform = ToMatrix(matrix);
  // Cairo-pattern matrices specify the conversion from DrawTarget to pattern
  // space. Azure pattern matrices specify the conversion from pattern to
  // DrawTarget space.
  mTransform.Invert();
}

gfxMatrix
gfxPattern::GetMatrix() const
{
  // invert at the higher precision of gfxMatrix
  // cause we need to convert at some point anyways
  gfxMatrix mat = ThebesMatrix(mTransform);
  mat.Invert();
  return mat;
}

gfxMatrix
gfxPattern::GetInverseMatrix() const
{
  return ThebesMatrix(mTransform);
}

Pattern*
gfxPattern::GetPattern(DrawTarget *aTarget, Matrix *aPatternTransform)
{
  if (!mStops &&
      !mStopsList.IsEmpty()) {
    mStops = aTarget->CreateGradientStops(mStopsList.Elements(),
                                          mStopsList.Length(),
                                          ToExtendMode(mExtend));
  }

  Matrix* matrix = nullptr;
  switch (mGfxPattern->GetType()) {
  case PatternType::SURFACE:
    matrix = &mSurfacePattern.addr()->mMatrix;
    mSurfacePattern.addr()->mExtendMode = ToExtendMode(mExtend);
    break;
  case PatternType::LINEAR_GRADIENT:
    matrix = &mLinearGradientPattern.addr()->mMatrix;
    mLinearGradientPattern.addr()->mStops = mStops;
    break;
  case PatternType::RADIAL_GRADIENT:
    matrix = &mRadialGradientPattern.addr()->mMatrix;
    mRadialGradientPattern.addr()->mStops = mStops;
    break;
  default:
    /* Reassure the compiler we are handling all the enum values.  */
    break;
  }

  if (matrix) {
    *matrix = mTransform;
    if (aPatternTransform) {
      AdjustTransformForPattern(*matrix,
                                aTarget->GetTransform(),
                                aPatternTransform);
    }
  }

  return mGfxPattern;
}

void
gfxPattern::SetExtend(GraphicsExtend extend)
{
  mExtend = extend;
  mStops = nullptr;
}

bool
gfxPattern::IsOpaque()
{
  if (mGfxPattern->GetType() != PatternType::SURFACE) {
    return false;
  }

  if (mSurfacePattern.addr()->mSurface->GetFormat() == SurfaceFormat::B8G8R8X8) {
    return true;
  }
  return false;
}

gfxPattern::GraphicsExtend
gfxPattern::Extend() const
{
  return mExtend;
}

void
gfxPattern::SetFilter(GraphicsFilter filter)
{
  if (mGfxPattern->GetType() != PatternType::SURFACE) {
    return;
  }

  mSurfacePattern.addr()->mFilter = ToFilter(filter);
}

GraphicsFilter
gfxPattern::Filter() const
{
  if (mGfxPattern->GetType() != PatternType::SURFACE) {
    return GraphicsFilter::FILTER_GOOD;
  }
  return ThebesFilter(mSurfacePattern.addr()->mFilter);
}

bool
gfxPattern::GetSolidColor(gfxRGBA& aColor)
{
  if (mGfxPattern->GetType() == PatternType::COLOR) {
    aColor = ThebesColor(mColorPattern.addr()->mColor);
    return true;
  }

 return false;
}

gfxPattern::GraphicsPatternType
gfxPattern::GetType() const
{
  return ThebesPatternType(mGfxPattern->GetType());
}

int
gfxPattern::CairoStatus()
{
  return CAIRO_STATUS_SUCCESS;
}

void
gfxPattern::AdjustTransformForPattern(Matrix &aPatternTransform,
                                      const Matrix &aCurrentTransform,
                                      const Matrix *aOriginalTransform)
{
  aPatternTransform.Invert();
  if (!aOriginalTransform) {
    // User space is unchanged, so to get from pattern space to user space,
    // just invert the cairo matrix.
    aPatternTransform.NudgeToIntegers();
    return;
  }
  // aPatternTransform now maps from pattern space to the user space defined
  // by *aOriginalTransform.

  Matrix mat = aCurrentTransform;
  mat.Invert();
  // mat maps from device space to current user space

  // First, transform from pattern space to original user space. Then transform
  // from original user space to device space. Then transform from
  // device space to current user space.
  aPatternTransform = aPatternTransform * *aOriginalTransform * mat;
  aPatternTransform.NudgeToIntegers();
}
