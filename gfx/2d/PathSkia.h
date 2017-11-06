/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATH_SKIA_H_
#define MOZILLA_GFX_PATH_SKIA_H_

#include "2D.h"
#include "skia/include/core/SkPath.h"

namespace mozilla {
namespace gfx {

class PathSkia;

class PathBuilderSkia : public PathBuilder
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathBuilderSkia, override)

  PathBuilderSkia(const Matrix& aTransform, const SkPath& aPath, FillRule aFillRule);
  explicit PathBuilderSkia(FillRule aFillRule);

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

  void AppendPath(const SkPath &aPath);

  virtual BackendType GetBackendType() const override { return BackendType::SKIA; }

private:

  void SetFillRule(FillRule aFillRule);

  SkPath mPath;
  FillRule mFillRule;
};

class PathSkia : public Path
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathSkia, override)

  PathSkia(SkPath& aPath, FillRule aFillRule)
    : mFillRule(aFillRule)
  {
    mPath.swap(aPath);
  }

  virtual BackendType GetBackendType() const override { return BackendType::SKIA; }

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

  const SkPath& GetPath() const { return mPath; }

private:
  friend class DrawTargetSkia;
  
  SkPath mPath;
  FillRule mFillRule;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PATH_SKIA_H_ */
