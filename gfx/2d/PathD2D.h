/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATHD2D_H_
#define MOZILLA_GFX_PATHD2D_H_

#include <d2d1.h>

#include "2D.h"

namespace mozilla {
namespace gfx {

class PathD2D;

class PathBuilderD2D : public PathBuilder
{
public:
  PathBuilderD2D(ID2D1GeometrySink *aSink, ID2D1PathGeometry *aGeom, FillRule aFillRule)
    : mSink(aSink)
    , mGeometry(aGeom)
    , mFigureActive(false)
    , mFillRule(aFillRule)
  {
  }
  virtual ~PathBuilderD2D();

  virtual void MoveTo(const Point &aPoint);
  virtual void LineTo(const Point &aPoint);
  virtual void BezierTo(const Point &aCP1,
                        const Point &aCP2,
                        const Point &aCP3);
  virtual void QuadraticBezierTo(const Point &aCP1,
                                 const Point &aCP2);
  virtual void Close();
  virtual void Arc(const Point &aOrigin, Float aRadius, Float aStartAngle,
                   Float aEndAngle, bool aAntiClockwise = false);
  virtual Point CurrentPoint() const;

  virtual TemporaryRef<Path> Finish();

  ID2D1GeometrySink *GetSink() { return mSink; }

private:
  friend class PathD2D;

  void EnsureActive(const Point &aPoint);

  RefPtr<ID2D1GeometrySink> mSink;
  RefPtr<ID2D1PathGeometry> mGeometry;

  bool mFigureActive;
  Point mCurrentPoint;
  Point mBeginPoint;
  FillRule mFillRule;
};

class PathD2D : public Path
{
public:
  PathD2D(ID2D1PathGeometry *aGeometry, bool aEndedActive,
          const Point &aEndPoint, FillRule aFillRule)
    : mGeometry(aGeometry)
    , mEndedActive(aEndedActive)
    , mEndPoint(aEndPoint)
    , mFillRule(aFillRule)
  {}
  
  virtual BackendType GetBackendType() const { return BACKEND_DIRECT2D; }

  virtual TemporaryRef<PathBuilder> CopyToBuilder(FillRule aFillRule = FILL_WINDING) const;
  virtual TemporaryRef<PathBuilder> TransformedCopyToBuilder(const Matrix &aTransform,
                                                             FillRule aFillRule = FILL_WINDING) const;

  virtual bool ContainsPoint(const Point &aPoint, const Matrix &aTransform) const;

  virtual bool StrokeContainsPoint(const StrokeOptions &aStrokeOptions,
                                   const Point &aPoint,
                                   const Matrix &aTransform) const;

  virtual Rect GetBounds(const Matrix &aTransform = Matrix()) const;

  virtual Rect GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                                const Matrix &aTransform = Matrix()) const;

  virtual void StreamToSink(PathSink *aSink) const;

  virtual FillRule GetFillRule() const { return mFillRule; }

  ID2D1Geometry *GetGeometry() { return mGeometry; }

private:
  friend class DrawTargetD2D;
  friend class DrawTargetD2D1;

  mutable RefPtr<ID2D1PathGeometry> mGeometry;
  bool mEndedActive;
  Point mEndPoint;
  FillRule mFillRule;
};

}
}

#endif /* MOZILLA_GFX_PATHD2D_H_ */
