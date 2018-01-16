/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATH_CAIRO_H_
#define MOZILLA_GFX_PATH_CAIRO_H_

#include "2D.h"
#include "cairo.h"
#include <vector>

namespace mozilla {
namespace gfx {

class PathCairo;

class PathBuilderCairo : public PathBuilder
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathBuilderCairo, override)

  explicit PathBuilderCairo(FillRule aFillRule);

  virtual void MoveTo(const Point &aPoint) override;
  virtual void LineTo(const Point &aPoint) override;
  virtual void BezierTo(const Point &aCP1,
                        const Point &aCP2,
                        const Point &aCP3) override;
  virtual void QuadraticBezierTo(const Point &aCP1,
                                 const Point &aCP2) override;
  virtual void Close() override;
  virtual void Arc(const Point &aOrigin, float aRadius, float aStartAngle,
                   float aEndAngle, bool aAntiClockwise = false) override;
  virtual Point CurrentPoint() const override;
  virtual already_AddRefed<Path> Finish() override;

  virtual BackendType GetBackendType() const override { return BackendType::CAIRO; }

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
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathCairo, override)

  PathCairo(FillRule aFillRule, std::vector<cairo_path_data_t> &aPathData, const Point &aCurrentPoint);
  explicit PathCairo(cairo_t *aContext);
  ~PathCairo();

  virtual BackendType GetBackendType() const override { return BackendType::CAIRO; }

  virtual already_AddRefed<PathBuilder> CopyToBuilder(FillRule aFillRule) const override;
  virtual already_AddRefed<PathBuilder> TransformedCopyToBuilder(const Matrix &aTransform,
                                                             FillRule aFillRule) const override;

  virtual bool ContainsPoint(const Point &aPoint, const Matrix &aTransform) const override;

  virtual bool StrokeContainsPoint(const StrokeOptions &aStrokeOptions,
                                   const Point &aPoint,
                                   const Matrix &aTransform) const override;

  virtual Rect GetBounds(const Matrix &aTransform = Matrix()) const override;

  virtual Rect GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                                const Matrix &aTransform = Matrix()) const override;

  virtual void StreamToSink(PathSink *aSink) const override;

  virtual FillRule GetFillRule() const override { return mFillRule; }

  void SetPathOnContext(cairo_t *aContext) const;

  void AppendPathToBuilder(PathBuilderCairo *aBuilder, const Matrix *aTransform = nullptr) const;
private:
  void EnsureContainingContext(const Matrix &aTransform) const;

  FillRule mFillRule;
  std::vector<cairo_path_data_t> mPathData;
  mutable cairo_t *mContainingContext;
  mutable Matrix mContainingTransform;
  Point mCurrentPoint;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PATH_CAIRO_H_ */
