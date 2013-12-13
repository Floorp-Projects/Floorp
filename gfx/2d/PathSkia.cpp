/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathSkia.h"
#include <math.h>
#include "DrawTargetSkia.h"
#include "Logging.h"
#include "HelpersSkia.h"
#include "PathHelpers.h"

namespace mozilla {
namespace gfx {

PathBuilderSkia::PathBuilderSkia(const Matrix& aTransform, const SkPath& aPath, FillRule aFillRule)
  : mPath(aPath)
{
  SkMatrix matrix;
  GfxMatrixToSkiaMatrix(aTransform, matrix);
  mPath.transform(matrix);
  SetFillRule(aFillRule);
}

PathBuilderSkia::PathBuilderSkia(FillRule aFillRule)
{
  SetFillRule(aFillRule);
}

void
PathBuilderSkia::SetFillRule(FillRule aFillRule)
{
  mFillRule = aFillRule;
  if (mFillRule == FILL_WINDING) {
    mPath.setFillType(SkPath::kWinding_FillType);
  } else {
    mPath.setFillType(SkPath::kEvenOdd_FillType);
  }
}

void
PathBuilderSkia::MoveTo(const Point &aPoint)
{
  mPath.moveTo(SkFloatToScalar(aPoint.x), SkFloatToScalar(aPoint.y));
}

void
PathBuilderSkia::LineTo(const Point &aPoint)
{
  if (!mPath.countPoints()) {
    MoveTo(aPoint);
  } else {
    mPath.lineTo(SkFloatToScalar(aPoint.x), SkFloatToScalar(aPoint.y));
  }
}

void
PathBuilderSkia::BezierTo(const Point &aCP1,
                          const Point &aCP2,
                          const Point &aCP3)
{
  if (!mPath.countPoints()) {
    MoveTo(aCP1);
  }
  mPath.cubicTo(SkFloatToScalar(aCP1.x), SkFloatToScalar(aCP1.y),
                SkFloatToScalar(aCP2.x), SkFloatToScalar(aCP2.y),
                SkFloatToScalar(aCP3.x), SkFloatToScalar(aCP3.y));
}

void
PathBuilderSkia::QuadraticBezierTo(const Point &aCP1,
                                   const Point &aCP2)
{
  if (!mPath.countPoints()) {
    MoveTo(aCP1);
  }
  mPath.quadTo(SkFloatToScalar(aCP1.x), SkFloatToScalar(aCP1.y),
               SkFloatToScalar(aCP2.x), SkFloatToScalar(aCP2.y));
}

void
PathBuilderSkia::Close()
{
  mPath.close();
}

void
PathBuilderSkia::Arc(const Point &aOrigin, float aRadius, float aStartAngle,
                     float aEndAngle, bool aAntiClockwise)
{
  ArcToBezier(this, aOrigin, aRadius, aStartAngle, aEndAngle, aAntiClockwise);
}

Point
PathBuilderSkia::CurrentPoint() const
{
  int pointCount = mPath.countPoints();
  if (!pointCount) {
    return Point(0, 0);
  }
  SkPoint point = mPath.getPoint(pointCount - 1);
  return Point(SkScalarToFloat(point.fX), SkScalarToFloat(point.fY));
}

TemporaryRef<Path>
PathBuilderSkia::Finish()
{
  RefPtr<PathSkia> path = new PathSkia(mPath, mFillRule);
  return path;
}

void
PathBuilderSkia::AppendPath(const SkPath &aPath)
{
  mPath.addPath(aPath);
}

TemporaryRef<PathBuilder>
PathSkia::CopyToBuilder(FillRule aFillRule) const
{
  return TransformedCopyToBuilder(Matrix(), aFillRule);
}

TemporaryRef<PathBuilder>
PathSkia::TransformedCopyToBuilder(const Matrix &aTransform, FillRule aFillRule) const
{
  RefPtr<PathBuilderSkia> builder = new PathBuilderSkia(aTransform, mPath, aFillRule);
  return builder;
}

bool
PathSkia::ContainsPoint(const Point &aPoint, const Matrix &aTransform) const
{
  Matrix inverse = aTransform;
  inverse.Invert();
  Point transformed = inverse * aPoint;

  Rect bounds = GetBounds(aTransform);

  if (aPoint.x < bounds.x || aPoint.y < bounds.y ||
      aPoint.x > bounds.XMost() || aPoint.y > bounds.YMost()) {
    return false;
  }

  SkRegion pointRect;
  pointRect.setRect(int32_t(SkFloatToScalar(transformed.x - 1)),
                    int32_t(SkFloatToScalar(transformed.y - 1)),
                    int32_t(SkFloatToScalar(transformed.x + 1)),
                    int32_t(SkFloatToScalar(transformed.y + 1)));

  SkRegion pathRegion;
  
  return pathRegion.setPath(mPath, pointRect);
}

static Rect SkRectToRect(const SkRect& aBounds)
{
  return Rect(SkScalarToFloat(aBounds.fLeft),
              SkScalarToFloat(aBounds.fTop),
              SkScalarToFloat(aBounds.fRight - aBounds.fLeft),
              SkScalarToFloat(aBounds.fBottom - aBounds.fTop));
}

bool
PathSkia::StrokeContainsPoint(const StrokeOptions &aStrokeOptions,
                              const Point &aPoint,
                              const Matrix &aTransform) const
{
  Matrix inverse = aTransform;
  inverse.Invert();
  Point transformed = inverse * aPoint;

  SkPaint paint;
  StrokeOptionsToPaint(paint, aStrokeOptions);

  SkPath strokePath;
  paint.getFillPath(mPath, &strokePath);

  Rect bounds = aTransform.TransformBounds(SkRectToRect(strokePath.getBounds()));

  if (aPoint.x < bounds.x || aPoint.y < bounds.y ||
      aPoint.x > bounds.XMost() || aPoint.y > bounds.YMost()) {
    return false;
  }

  SkRegion pointRect;
  pointRect.setRect(int32_t(SkFloatToScalar(transformed.x - 1)),
                    int32_t(SkFloatToScalar(transformed.y - 1)),
                    int32_t(SkFloatToScalar(transformed.x + 1)),
                    int32_t(SkFloatToScalar(transformed.y + 1)));

  SkRegion pathRegion;
  
  return pathRegion.setPath(strokePath, pointRect);
}

Rect
PathSkia::GetBounds(const Matrix &aTransform) const
{
  Rect bounds = SkRectToRect(mPath.getBounds());
  return aTransform.TransformBounds(bounds);
}

Rect
PathSkia::GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                           const Matrix &aTransform) const
{
  SkPaint paint;
  StrokeOptionsToPaint(paint, aStrokeOptions);
  
  SkPath result;
  paint.getFillPath(mPath, &result);

  Rect bounds = SkRectToRect(result.getBounds());
  return aTransform.TransformBounds(bounds);
}

void
PathSkia::StreamToSink(PathSink *aSink) const
{
  SkPath::RawIter iter(mPath);

  SkPoint points[4];
  SkPath::Verb currentVerb;
  while ((currentVerb = iter.next(points)) != SkPath::kDone_Verb) {
    switch (currentVerb) {
    case SkPath::kMove_Verb:
      aSink->MoveTo(SkPointToPoint(points[0]));
      break;
    case SkPath::kLine_Verb:
      aSink->LineTo(SkPointToPoint(points[1]));
      break;
    case SkPath::kCubic_Verb:
      aSink->BezierTo(SkPointToPoint(points[1]),
                      SkPointToPoint(points[2]),
                      SkPointToPoint(points[3]));
      break;
    case SkPath::kQuad_Verb:
      aSink->QuadraticBezierTo(SkPointToPoint(points[1]),
                               SkPointToPoint(points[2]));
      break;
    case SkPath::kClose_Verb:
      aSink->Close();
      break;
    default:
      MOZ_ASSERT(false);
      // Unexpected verb found in path!
    }
  }
}

}
}
