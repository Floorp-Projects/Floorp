/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATHCG_H_
#define MOZILLA_GFX_PATHCG_H_

#include <ApplicationServices/ApplicationServices.h>
#include "2D.h"

namespace mozilla {
namespace gfx {

class PathCG;

class PathBuilderCG : public PathBuilder
{
public:
  // absorbs a reference of aPath
  PathBuilderCG(CGMutablePathRef aPath, FillRule aFillRule)
    : mFigureActive(false)
    , mFillRule(aFillRule)
  {
      mCGPath = aPath;
  }

  PathBuilderCG(FillRule aFillRule)
    : mFigureActive(false)
    , mFillRule(aFillRule)
  {
      mCGPath = CGPathCreateMutable();
  }

  virtual ~PathBuilderCG();

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

private:
  friend class PathCG;

  void EnsureActive(const Point &aPoint);

  CGMutablePathRef mCGPath;
  bool mFigureActive;
  Point mCurrentPoint;
  Point mBeginPoint;
  FillRule mFillRule;
};

class PathCG : public Path
{
public:
  PathCG(CGMutablePathRef aPath, FillRule aFillRule)
    : mPath(aPath)
    , mFillRule(aFillRule)
  {
    CGPathRetain(mPath);
  }
  virtual ~PathCG() { CGPathRelease(mPath); }

  virtual BackendType GetBackendType() const { return BACKEND_COREGRAPHICS; }

  virtual TemporaryRef<PathBuilder> CopyToBuilder(FillRule aFillRule = FILL_WINDING) const;
  virtual TemporaryRef<PathBuilder> TransformedCopyToBuilder(const Matrix &aTransform,
                                                             FillRule aFillRule = FILL_WINDING) const;

  virtual bool ContainsPoint(const Point &aPoint, const Matrix &aTransform) const;
  virtual Rect GetBounds(const Matrix &aTransform = Matrix()) const;
  virtual Rect GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                                const Matrix &aTransform = Matrix()) const;

  virtual FillRule GetFillRule() const { return mFillRule; }

  CGMutablePathRef GetPath() const { return mPath; }

private:
  friend class DrawTargetCG;

  CGMutablePathRef mPath;
  bool mEndedActive;
  Point mEndPoint;
  FillRule mFillRule;
};

}
}

#endif
