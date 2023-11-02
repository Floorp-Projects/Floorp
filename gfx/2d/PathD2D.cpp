/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathD2D.h"
#include "HelpersD2D.h"
#include <math.h>
#include "DrawTargetD2D1.h"
#include "Logging.h"
#include "PathHelpers.h"

namespace mozilla {
namespace gfx {

already_AddRefed<PathBuilder> PathBuilderD2D::Create(FillRule aFillRule) {
  RefPtr<ID2D1PathGeometry> path;
  HRESULT hr =
      DrawTargetD2D1::factory()->CreatePathGeometry(getter_AddRefs(path));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create Direct2D Path Geometry. Code: "
                 << hexa(hr);
    return nullptr;
  }

  RefPtr<ID2D1GeometrySink> sink;
  hr = path->Open(getter_AddRefs(sink));
  if (FAILED(hr)) {
    gfxWarning() << "Failed to access Direct2D Path Geometry. Code: "
                 << hexa(hr);
    return nullptr;
  }

  if (aFillRule == FillRule::FILL_WINDING) {
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  }

  return MakeAndAddRef<PathBuilderD2D>(sink, path, aFillRule,
                                       BackendType::DIRECT2D1_1);
}

// This class exists as a wrapper for ID2D1SimplifiedGeometry sink, it allows
// a geometry to be duplicated into a geometry sink, while removing the final
// figure end and thus allowing a figure that was implicitly closed to be
// continued.
class OpeningGeometrySink : public ID2D1SimplifiedGeometrySink {
 public:
  explicit OpeningGeometrySink(ID2D1SimplifiedGeometrySink* aSink)
      : mSink(aSink), mNeedsFigureEnded(false) {}

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

  // We ignore SetFillMode, the copier will decide.
  STDMETHOD_(void, SetFillMode)(D2D1_FILL_MODE aMode) {
    EnsureFigureEnded();
    return;
  }
  STDMETHOD_(void, BeginFigure)
  (D2D1_POINT_2F aPoint, D2D1_FIGURE_BEGIN aBegin) {
    EnsureFigureEnded();
    return mSink->BeginFigure(aPoint, aBegin);
  }
  STDMETHOD_(void, AddLines)(const D2D1_POINT_2F* aLines, UINT aCount) {
    EnsureFigureEnded();
    return mSink->AddLines(aLines, aCount);
  }
  STDMETHOD_(void, AddBeziers)
  (const D2D1_BEZIER_SEGMENT* aSegments, UINT aCount) {
    EnsureFigureEnded();
    return mSink->AddBeziers(aSegments, aCount);
  }
  STDMETHOD(Close)() { /* Should never be called! */
    return S_OK;
  }
  STDMETHOD_(void, SetSegmentFlags)(D2D1_PATH_SEGMENT aFlags) {
    return mSink->SetSegmentFlags(aFlags);
  }

  // This function is special - it's the reason this class exists.
  // It needs to intercept the very last endfigure. So that a user can
  // continue writing to this sink as if they never stopped.
  STDMETHOD_(void, EndFigure)(D2D1_FIGURE_END aEnd) {
    if (aEnd == D2D1_FIGURE_END_CLOSED) {
      return mSink->EndFigure(aEnd);
    } else {
      mNeedsFigureEnded = true;
    }
  }

 private:
  void EnsureFigureEnded() {
    if (mNeedsFigureEnded) {
      mSink->EndFigure(D2D1_FIGURE_END_OPEN);
      mNeedsFigureEnded = false;
    }
  }

  ID2D1SimplifiedGeometrySink* mSink;
  bool mNeedsFigureEnded;
};

PathBuilderD2D::~PathBuilderD2D() {}

void PathBuilderD2D::MoveTo(const Point& aPoint) {
  if (mFigureActive) {
    mSink->EndFigure(D2D1_FIGURE_END_OPEN);
    mFigureActive = false;
  }
  EnsureActive(aPoint);
  mCurrentPoint = aPoint;
}

void PathBuilderD2D::LineTo(const Point& aPoint) {
  EnsureActive(aPoint);
  mSink->AddLine(D2DPoint(aPoint));

  mCurrentPoint = aPoint;
  mFigureEmpty = false;
}

void PathBuilderD2D::BezierTo(const Point& aCP1, const Point& aCP2,
                              const Point& aCP3) {
  EnsureActive(aCP1);
  mSink->AddBezier(
      D2D1::BezierSegment(D2DPoint(aCP1), D2DPoint(aCP2), D2DPoint(aCP3)));

  mCurrentPoint = aCP3;
  mFigureEmpty = false;
}

void PathBuilderD2D::QuadraticBezierTo(const Point& aCP1, const Point& aCP2) {
  EnsureActive(aCP1);
  mSink->AddQuadraticBezier(
      D2D1::QuadraticBezierSegment(D2DPoint(aCP1), D2DPoint(aCP2)));

  mCurrentPoint = aCP2;
  mFigureEmpty = false;
}

void PathBuilderD2D::Close() {
  if (mFigureActive) {
    mSink->EndFigure(D2D1_FIGURE_END_CLOSED);

    mFigureActive = false;

    EnsureActive(mBeginPoint);
  }
}

void PathBuilderD2D::Arc(const Point& aOrigin, Float aRadius, Float aStartAngle,
                         Float aEndAngle, bool aAntiClockwise) {
  MOZ_ASSERT(aRadius >= 0);

  // We want aEndAngle to come numerically after aStartAngle when taking into
  // account the sweep direction so that our calculation of the arcSize below
  // (large or small) works.
  Float sweepDirection = aAntiClockwise ? -1.0f : 1.0f;

  Float arcSweepLeft = (aEndAngle - aStartAngle) * sweepDirection;
  if (arcSweepLeft < 0) {
    // This calculation moves aStartAngle by a multiple of 2*Pi so that it is
    // the closest it can be to aEndAngle and still be numerically before
    // aEndAngle when taking into account sweepDirection.
    arcSweepLeft = Float(2.0f * M_PI) + fmodf(arcSweepLeft, Float(2.0f * M_PI));
    aStartAngle = aEndAngle - arcSweepLeft * sweepDirection;
  }

  // XXX - Workaround for now, D2D does not appear to do the desired thing when
  // the angle sweeps a complete circle.
  bool fullCircle = false;
  if (aEndAngle - aStartAngle >= 1.9999 * M_PI) {
    fullCircle = true;
    aEndAngle = Float(aStartAngle + M_PI * 1.9999);
  } else if (aStartAngle - aEndAngle >= 1.9999 * M_PI) {
    fullCircle = true;
    aStartAngle = Float(aEndAngle + M_PI * 1.9999);
  }

  Point startPoint;
  startPoint.x = aOrigin.x + aRadius * cos(aStartAngle);
  startPoint.y = aOrigin.y + aRadius * sin(aStartAngle);

  if (!mFigureActive) {
    EnsureActive(startPoint);
  } else {
    mSink->AddLine(D2DPoint(startPoint));
  }

  Point endPoint;
  endPoint.x = aOrigin.x + aRadius * cosf(aEndAngle);
  endPoint.y = aOrigin.y + aRadius * sinf(aEndAngle);

  D2D1_ARC_SIZE arcSize = D2D1_ARC_SIZE_SMALL;
  D2D1_SWEEP_DIRECTION direction = aAntiClockwise
                                       ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE
                                       : D2D1_SWEEP_DIRECTION_CLOCKWISE;

  // if startPoint and endPoint of our circle are too close there are D2D issues
  // with drawing the circle as a single arc
  const Float kEpsilon = 1e-5f;
  if (!fullCircle || (std::abs(startPoint.x - endPoint.x) +
                          std::abs(startPoint.y - endPoint.y) >
                      kEpsilon)) {
    if (aAntiClockwise) {
      if (aStartAngle - aEndAngle > M_PI) {
        arcSize = D2D1_ARC_SIZE_LARGE;
      }
    } else {
      if (aEndAngle - aStartAngle > M_PI) {
        arcSize = D2D1_ARC_SIZE_LARGE;
      }
    }

    mSink->AddArc(D2D1::ArcSegment(D2DPoint(endPoint),
                                   D2D1::SizeF(aRadius, aRadius), 0.0f,
                                   direction, arcSize));
  } else {
    // our first workaround attempt didn't work, so instead draw the circle as
    // two half-circles
    Float midAngle = aEndAngle > aStartAngle ? Float(aStartAngle + M_PI)
                                             : Float(aEndAngle + M_PI);
    Point midPoint;
    midPoint.x = aOrigin.x + aRadius * cosf(midAngle);
    midPoint.y = aOrigin.y + aRadius * sinf(midAngle);

    mSink->AddArc(D2D1::ArcSegment(D2DPoint(midPoint),
                                   D2D1::SizeF(aRadius, aRadius), 0.0f,
                                   direction, arcSize));

    // if the adjusted endPoint computed above is used here and endPoint !=
    // startPoint then this half of the circle won't render...
    mSink->AddArc(D2D1::ArcSegment(D2DPoint(startPoint),
                                   D2D1::SizeF(aRadius, aRadius), 0.0f,
                                   direction, arcSize));
  }

  mCurrentPoint = endPoint;
  mFigureEmpty = false;
}

void PathBuilderD2D::EnsureActive(const Point& aPoint) {
  if (!mFigureActive) {
    mSink->BeginFigure(D2DPoint(aPoint), D2D1_FIGURE_BEGIN_FILLED);
    mBeginPoint = aPoint;
    mFigureActive = true;
  }
}

already_AddRefed<Path> PathBuilderD2D::Finish() {
  if (mFigureActive) {
    mSink->EndFigure(D2D1_FIGURE_END_OPEN);
  }

  HRESULT hr = mSink->Close();
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to close PathSink. Code: " << hexa(hr);
    return nullptr;
  }

  return MakeAndAddRef<PathD2D>(mGeometry, mFigureActive, mFigureEmpty,
                                mCurrentPoint, mFillRule, mBackendType);
}

already_AddRefed<PathBuilder> PathD2D::CopyToBuilder(FillRule aFillRule) const {
  return TransformedCopyToBuilder(Matrix(), aFillRule);
}

already_AddRefed<PathBuilder> PathD2D::TransformedCopyToBuilder(
    const Matrix& aTransform, FillRule aFillRule) const {
  RefPtr<ID2D1PathGeometry> path;
  HRESULT hr =
      DrawTargetD2D1::factory()->CreatePathGeometry(getter_AddRefs(path));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create PathGeometry. Code: " << hexa(hr);
    return nullptr;
  }

  RefPtr<ID2D1GeometrySink> sink;
  hr = path->Open(getter_AddRefs(sink));
  if (FAILED(hr)) {
    gfxWarning() << "Failed to open Geometry for writing. Code: " << hexa(hr);
    return nullptr;
  }

  if (aFillRule == FillRule::FILL_WINDING) {
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  }

  if (mEndedActive) {
    OpeningGeometrySink wrapSink(sink);
    hr = mGeometry->Simplify(
        D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
        D2DMatrix(aTransform), &wrapSink);
  } else {
    hr = mGeometry->Simplify(
        D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
        D2DMatrix(aTransform), sink);
  }
  if (FAILED(hr)) {
    gfxWarning() << "Failed to simplify PathGeometry to tranformed copy. Code: "
                 << hexa(hr) << " Active: " << mEndedActive;
    return nullptr;
  }

  RefPtr<PathBuilderD2D> pathBuilder =
      new PathBuilderD2D(sink, path, aFillRule, mBackendType);

  pathBuilder->mCurrentPoint = aTransform.TransformPoint(mEndPoint);

  if (mEndedActive) {
    pathBuilder->mFigureActive = true;
  }

  return pathBuilder.forget();
}

void PathD2D::StreamToSink(PathSink* aSink) const {
  HRESULT hr;

  StreamingGeometrySink sink(aSink);

  hr = mGeometry->Simplify(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
                           D2D1::IdentityMatrix(), &sink);

  if (FAILED(hr)) {
    gfxWarning() << "Failed to stream D2D path to sink. Code: " << hexa(hr);
    return;
  }
}

bool PathD2D::ContainsPoint(const Point& aPoint,
                            const Matrix& aTransform) const {
  if (!aTransform.Determinant()) {
    // If the transform is not invertible, then don't consider point inside.
    return false;
  }

  BOOL result;

  HRESULT hr = mGeometry->FillContainsPoint(
      D2DPoint(aPoint), D2DMatrix(aTransform), 0.001f, &result);

  if (FAILED(hr)) {
    // Log
    return false;
  }

  return !!result;
}

bool PathD2D::StrokeContainsPoint(const StrokeOptions& aStrokeOptions,
                                  const Point& aPoint,
                                  const Matrix& aTransform) const {
  if (!aTransform.Determinant()) {
    // If the transform is not invertible, then don't consider point inside.
    return false;
  }

  BOOL result;

  RefPtr<ID2D1StrokeStyle> strokeStyle =
      CreateStrokeStyleForOptions(aStrokeOptions);
  HRESULT hr = mGeometry->StrokeContainsPoint(
      D2DPoint(aPoint), aStrokeOptions.mLineWidth, strokeStyle,
      D2DMatrix(aTransform), &result);

  if (FAILED(hr)) {
    // Log
    return false;
  }

  return !!result;
}

Rect PathD2D::GetBounds(const Matrix& aTransform) const {
  D2D1_RECT_F d2dBounds;

  HRESULT hr = mGeometry->GetBounds(D2DMatrix(aTransform), &d2dBounds);

  Rect bounds = ToRect(d2dBounds);
  if (FAILED(hr) || !bounds.IsFinite()) {
    gfxWarning() << "Failed to get stroked bounds for path. Code: " << hexa(hr);
    return Rect();
  }

  return bounds;
}

Rect PathD2D::GetStrokedBounds(const StrokeOptions& aStrokeOptions,
                               const Matrix& aTransform) const {
  D2D1_RECT_F d2dBounds;

  RefPtr<ID2D1StrokeStyle> strokeStyle =
      CreateStrokeStyleForOptions(aStrokeOptions);
  HRESULT hr =
      mGeometry->GetWidenedBounds(aStrokeOptions.mLineWidth, strokeStyle,
                                  D2DMatrix(aTransform), &d2dBounds);

  Rect bounds = ToRect(d2dBounds);
  if (FAILED(hr) || !bounds.IsFinite()) {
    gfxWarning() << "Failed to get stroked bounds for path. Code: " << hexa(hr);
    return Rect();
  }

  return bounds;
}

}  // namespace gfx
}  // namespace mozilla
