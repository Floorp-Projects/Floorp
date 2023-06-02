/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathSkia.h"
#include "HelpersSkia.h"
#include "PathHelpers.h"
#include "mozilla/UniquePtr.h"
#include "skia/include/core/SkPathUtils.h"
#include "skia/src/core/SkGeometry.h"

namespace mozilla::gfx {

already_AddRefed<PathBuilder> PathBuilderSkia::Create(FillRule aFillRule) {
  return MakeAndAddRef<PathBuilderSkia>(aFillRule);
}

PathBuilderSkia::PathBuilderSkia(const Matrix& aTransform, const SkPath& aPath,
                                 FillRule aFillRule)
    : mPath(aPath) {
  SkMatrix matrix;
  GfxMatrixToSkiaMatrix(aTransform, matrix);
  mPath.transform(matrix);
  SetFillRule(aFillRule);
}

PathBuilderSkia::PathBuilderSkia(FillRule aFillRule) { SetFillRule(aFillRule); }

void PathBuilderSkia::SetFillRule(FillRule aFillRule) {
  mFillRule = aFillRule;
  if (mFillRule == FillRule::FILL_WINDING) {
    mPath.setFillType(SkPathFillType::kWinding);
  } else {
    mPath.setFillType(SkPathFillType::kEvenOdd);
  }
}

void PathBuilderSkia::MoveTo(const Point& aPoint) {
  mPath.moveTo(SkFloatToScalar(aPoint.x), SkFloatToScalar(aPoint.y));
  mCurrentPoint = aPoint;
  mBeginPoint = aPoint;
}

void PathBuilderSkia::LineTo(const Point& aPoint) {
  if (!mPath.countPoints()) {
    MoveTo(aPoint);
  } else {
    mPath.lineTo(SkFloatToScalar(aPoint.x), SkFloatToScalar(aPoint.y));
  }
  mCurrentPoint = aPoint;
}

void PathBuilderSkia::BezierTo(const Point& aCP1, const Point& aCP2,
                               const Point& aCP3) {
  if (!mPath.countPoints()) {
    MoveTo(aCP1);
  }
  mPath.cubicTo(SkFloatToScalar(aCP1.x), SkFloatToScalar(aCP1.y),
                SkFloatToScalar(aCP2.x), SkFloatToScalar(aCP2.y),
                SkFloatToScalar(aCP3.x), SkFloatToScalar(aCP3.y));
  mCurrentPoint = aCP3;
}

void PathBuilderSkia::QuadraticBezierTo(const Point& aCP1, const Point& aCP2) {
  if (!mPath.countPoints()) {
    MoveTo(aCP1);
  }
  mPath.quadTo(SkFloatToScalar(aCP1.x), SkFloatToScalar(aCP1.y),
               SkFloatToScalar(aCP2.x), SkFloatToScalar(aCP2.y));
  mCurrentPoint = aCP2;
}

void PathBuilderSkia::Close() {
  mPath.close();
  mCurrentPoint = mBeginPoint;
}

void PathBuilderSkia::Arc(const Point& aOrigin, float aRadius,
                          float aStartAngle, float aEndAngle,
                          bool aAntiClockwise) {
  ArcToBezier(this, aOrigin, Size(aRadius, aRadius), aStartAngle, aEndAngle,
              aAntiClockwise);
}

already_AddRefed<Path> PathBuilderSkia::Finish() {
  RefPtr<Path> path =
      MakeAndAddRef<PathSkia>(mPath, mFillRule, mCurrentPoint, mBeginPoint);
  mCurrentPoint = Point(0.0, 0.0);
  mBeginPoint = Point(0.0, 0.0);
  return path.forget();
}

void PathBuilderSkia::AppendPath(const SkPath& aPath) { mPath.addPath(aPath); }

already_AddRefed<PathBuilder> PathSkia::CopyToBuilder(
    FillRule aFillRule) const {
  return TransformedCopyToBuilder(Matrix(), aFillRule);
}

already_AddRefed<PathBuilder> PathSkia::TransformedCopyToBuilder(
    const Matrix& aTransform, FillRule aFillRule) const {
  RefPtr<PathBuilderSkia> builder =
      MakeAndAddRef<PathBuilderSkia>(aTransform, mPath, aFillRule);

  builder->mCurrentPoint = aTransform.TransformPoint(mCurrentPoint);
  builder->mBeginPoint = aTransform.TransformPoint(mBeginPoint);

  return builder.forget();
}

static bool SkPathContainsPoint(const SkPath& aPath, const Point& aPoint,
                                const Matrix& aTransform) {
  Matrix inverse = aTransform;
  if (!inverse.Invert()) {
    return false;
  }

  SkPoint point = PointToSkPoint(inverse.TransformPoint(aPoint));
  return aPath.contains(point.fX, point.fY);
}

bool PathSkia::ContainsPoint(const Point& aPoint,
                             const Matrix& aTransform) const {
  if (!mPath.isFinite()) {
    return false;
  }

  return SkPathContainsPoint(mPath, aPoint, aTransform);
}

bool PathSkia::GetFillPath(const StrokeOptions& aStrokeOptions,
                           const Matrix& aTransform, SkPath& aFillPath,
                           const Maybe<Rect>& aClipRect) const {
  SkPaint paint;
  if (!StrokeOptionsToPaint(paint, aStrokeOptions)) {
    return false;
  }

  SkMatrix skiaMatrix;
  GfxMatrixToSkiaMatrix(aTransform, skiaMatrix);

  Maybe<SkRect> cullRect;
  if (aClipRect.isSome()) {
    cullRect = Some(RectToSkRect(aClipRect.ref()));
  }

  return skpathutils::FillPathWithPaint(mPath, paint, &aFillPath,
                                        cullRect.ptrOr(nullptr), skiaMatrix);
}

bool PathSkia::StrokeContainsPoint(const StrokeOptions& aStrokeOptions,
                                   const Point& aPoint,
                                   const Matrix& aTransform) const {
  if (!mPath.isFinite()) {
    return false;
  }

  SkPath strokePath;
  if (!GetFillPath(aStrokeOptions, aTransform, strokePath)) {
    return false;
  }

  return SkPathContainsPoint(strokePath, aPoint, aTransform);
}

Rect PathSkia::GetBounds(const Matrix& aTransform) const {
  if (!mPath.isFinite()) {
    return Rect();
  }

  Rect bounds = SkRectToRect(mPath.computeTightBounds());
  return aTransform.TransformBounds(bounds);
}

Rect PathSkia::GetStrokedBounds(const StrokeOptions& aStrokeOptions,
                                const Matrix& aTransform) const {
  if (!mPath.isFinite()) {
    return Rect();
  }

  SkPath fillPath;
  if (!GetFillPath(aStrokeOptions, aTransform, fillPath)) {
    return Rect();
  }

  Rect bounds = SkRectToRect(fillPath.computeTightBounds());
  return aTransform.TransformBounds(bounds);
}

Rect PathSkia::GetFastBounds(const Matrix& aTransform,
                             const StrokeOptions* aStrokeOptions) const {
  if (!mPath.isFinite()) {
    return Rect();
  }
  SkRect bounds = mPath.getBounds();
  if (aStrokeOptions) {
    // If the path is stroked, ensure that the bounds are inflated by any
    // relevant options such as line width. Avoid using dash path effects
    // for performance and to ensure computeFastStrokeBounds succeeds.
    SkPaint paint;
    if (!StrokeOptionsToPaint(paint, *aStrokeOptions, false)) {
      return Rect();
    }
    SkRect outBounds = SkRect::MakeEmpty();
    bounds = paint.computeFastStrokeBounds(bounds, &outBounds);
  }
  return aTransform.TransformBounds(SkRectToRect(bounds));
}

int ConvertConicToQuads(const Point& aP0, const Point& aP1, const Point& aP2,
                        float aWeight, std::vector<Point>& aQuads) {
  SkConic conic(PointToSkPoint(aP0), PointToSkPoint(aP1), PointToSkPoint(aP2),
                aWeight);
  int pow2 = conic.computeQuadPOW2(0.25f);
  aQuads.resize(1 + 2 * (1 << pow2));
  int numQuads =
      conic.chopIntoQuadsPOW2(reinterpret_cast<SkPoint*>(&aQuads[0]), pow2);
  if (numQuads < 1 << pow2) {
    aQuads.resize(1 + 2 * numQuads);
  }
  return numQuads;
}

void PathSkia::StreamToSink(PathSink* aSink) const {
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
        aSink->BezierTo(SkPointToPoint(points[1]), SkPointToPoint(points[2]),
                        SkPointToPoint(points[3]));
        break;
      case SkPath::kQuad_Verb:
        aSink->QuadraticBezierTo(SkPointToPoint(points[1]),
                                 SkPointToPoint(points[2]));
        break;
      case SkPath::kConic_Verb: {
        std::vector<Point> quads;
        int numQuads = ConvertConicToQuads(
            SkPointToPoint(points[0]), SkPointToPoint(points[1]),
            SkPointToPoint(points[2]), iter.conicWeight(), quads);
        for (int i = 0; i < numQuads; i++) {
          aSink->QuadraticBezierTo(quads[2 * i + 1], quads[2 * i + 2]);
        }
        break;
      }
      case SkPath::kClose_Verb:
        aSink->Close();
        break;
      default:
        MOZ_ASSERT(false);
        // Unexpected verb found in path!
    }
  }
}

Maybe<Rect> PathSkia::AsRect() const {
  SkRect rect;
  if (mPath.isRect(&rect)) {
    return Some(SkRectToRect(rect));
  }
  return Nothing();
}

bool PathSkia::IsEmpty() const {
  // Move/Close/Done segments are not included in the mask so as long as any
  // flag is set, we know that the path is non-empty.
  return mPath.getSegmentMasks() == 0;
}

}  // namespace mozilla::gfx
