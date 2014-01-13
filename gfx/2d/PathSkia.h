/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATH_SKIA_H_
#define MOZILLA_GFX_PATH_SKIA_H_

#include "2D.h"
#include "skia/SkPath.h"

namespace mozilla {
namespace gfx {

class PathSkia;

class PathBuilderSkia : public PathBuilder
{
public:
  PathBuilderSkia(const Matrix& aTransform, const SkPath& aPath, FillRule aFillRule);
  PathBuilderSkia(FillRule aFillRule);

  virtual void MoveTo(const Point &aPoint);
  virtual void LineTo(const Point &aPoint);
  virtual void BezierTo(const Point &aCP1,
                        const Point &aCP2,
                        const Point &aCP3);
  virtual void QuadraticBezierTo(const Point &aCP1,
                                 const Point &aCP2);
  virtual void Close();
  virtual void Arc(const Point &aOrigin, float aRadius, float aStartAngle,
                   float aEndAngle, bool aAntiClockwise = false);
  virtual Point CurrentPoint() const;
  virtual TemporaryRef<Path> Finish();

  void AppendPath(const SkPath &aPath);

private:

  void SetFillRule(FillRule aFillRule);

  SkPath mPath;
  FillRule mFillRule;
};

class PathSkia : public Path
{
public:
  PathSkia(SkPath& aPath, FillRule aFillRule)
    : mFillRule(aFillRule)
  {
    mPath.swap(aPath);
  }
  
  virtual BackendType GetBackendType() const { return BackendType::SKIA; }

  virtual TemporaryRef<PathBuilder> CopyToBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const;
  virtual TemporaryRef<PathBuilder> TransformedCopyToBuilder(const Matrix &aTransform,
                                                             FillRule aFillRule = FillRule::FILL_WINDING) const;

  virtual bool ContainsPoint(const Point &aPoint, const Matrix &aTransform) const;
  
  virtual bool StrokeContainsPoint(const StrokeOptions &aStrokeOptions,
                                   const Point &aPoint,
                                   const Matrix &aTransform) const;

  virtual Rect GetBounds(const Matrix &aTransform = Matrix()) const;
  
  virtual Rect GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                                const Matrix &aTransform = Matrix()) const;

  virtual void StreamToSink(PathSink *aSink) const;

  virtual FillRule GetFillRule() const { return mFillRule; }

  const SkPath& GetPath() const { return mPath; }

private:
  friend class DrawTargetSkia;
  
  SkPath mPath;
  FillRule mFillRule;
};

}
}

#endif /* MOZILLA_GFX_PATH_SKIA_H_ */
