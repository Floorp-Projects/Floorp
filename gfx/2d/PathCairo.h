/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATH_CAIRO_H_
#define MOZILLA_GFX_PATH_CAIRO_H_

#include "2D.h"
#include "cairo.h"
#include <vector>

namespace mozilla {
namespace gfx {

class DrawTargetCairo;
class PathCairo;

class PathBuilderCairo : public PathBuilder
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathBuilderCairo)
  explicit PathBuilderCairo(FillRule aFillRule);

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

private: // data
  friend class PathCairo;

  FillRule mFillRule;
  std::vector<cairo_path_data_t> mPathData;
  // It's easiest to track this here, parsing the path data to find the current
  // point is a little tricky.
  Point mCurrentPoint;
  Point mBeginPoint;
};

class PathCairo : public Path
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathCairo)
  PathCairo(FillRule aFillRule, std::vector<cairo_path_data_t> &aPathData, const Point &aCurrentPoint);
  explicit PathCairo(cairo_t *aContext);
  ~PathCairo();

  virtual BackendType GetBackendType() const { return BackendType::CAIRO; }

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

  void SetPathOnContext(cairo_t *aContext) const;

  void AppendPathToBuilder(PathBuilderCairo *aBuilder, const Matrix *aTransform = nullptr) const;
private:
  void EnsureContainingContext() const;

  FillRule mFillRule;
  std::vector<cairo_path_data_t> mPathData;
  mutable cairo_t *mContainingContext;
  Point mCurrentPoint;
};

}
}

#endif /* MOZILLA_GFX_PATH_CAIRO_H_ */
