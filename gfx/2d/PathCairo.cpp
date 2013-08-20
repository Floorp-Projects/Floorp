/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathCairo.h"
#include <math.h>
#include "DrawTargetCairo.h"
#include "Logging.h"
#include "PathHelpers.h"
#include "HelpersCairo.h"

namespace mozilla {
namespace gfx {

CairoPathContext::CairoPathContext(cairo_t* aCtx, DrawTargetCairo* aDrawTarget)
 : mContext(aCtx)
 , mDrawTarget(aDrawTarget)
{
  cairo_reference(mContext);

  // A new path in the DrawTarget's context.
  aDrawTarget->SetPathObserver(this);
  cairo_new_path(mContext);
}

CairoPathContext::CairoPathContext(CairoPathContext& aPathContext)
 : mContext(aPathContext.mContext)
 , mDrawTarget(nullptr)
{
  cairo_reference(mContext);
  DuplicateContextAndPath();
}

CairoPathContext::~CairoPathContext()
{
  if (mDrawTarget) {
    DrawTargetCairo* drawTarget = mDrawTarget;
    ForgetDrawTarget();

    // We need to set mDrawTarget to nullptr before we tell DrawTarget otherwise
    // we will think we need to make a defensive copy of the path.
    drawTarget->SetPathObserver(nullptr);
  }
  cairo_destroy(mContext);
}

void
CairoPathContext::DuplicateContextAndPath()
{
  // Duplicate the path.
  cairo_path_t* path = cairo_copy_path(mContext);

  // Duplicate the context.
  cairo_surface_t* surf = cairo_get_target(mContext);
  cairo_matrix_t matrix;
  cairo_get_matrix(mContext, &matrix);
  cairo_destroy(mContext);

  mContext = cairo_create(surf);

  // Set the matrix to match the source context so that the path is copied in
  // device space. After this point it doesn't matter what the transform is
  // set to because it's always swapped out before use.
  cairo_set_matrix(mContext, &matrix);

  // Add the path, and throw away our duplicate.
  cairo_append_path(mContext, path);
  cairo_path_destroy(path);
}

void
CairoPathContext::ForgetDrawTarget()
{
  // We don't need to set the path observer back to nullptr in this case
  // because ForgetDrawTarget() is trigged when the target has been
  // grabbed by another path observer.
  mDrawTarget = nullptr;
}

void
CairoPathContext::PathWillChange()
{
  // Once we've copied out the context's path, there's no use to holding on to
  // the draw target. Thus, there's nothing for us to do if we're independent
  // of the draw target, since we'll have already copied out the context's
  // path.
  if (mDrawTarget) {
    // The context we point to is going to change from under us. To continue
    // using this path, we need to copy it to a new context.
    DuplicateContextAndPath();
    ForgetDrawTarget();
  }
}

void
CairoPathContext::CopyPathTo(cairo_t* aToContext, Matrix& aTransform)
{
  if (aToContext != mContext) {
    CairoTempMatrix tempMatrix(mContext, aTransform);
    cairo_path_t* path = cairo_copy_path(mContext);
    cairo_new_path(aToContext);
    cairo_append_path(aToContext, path);
    cairo_path_destroy(path);
  }
}

bool
CairoPathContext::ContainsPath(const Path* aPath)
{
  if (aPath->GetBackendType() != BACKEND_CAIRO) {
    return false;
  }

  const PathCairo* path = static_cast<const PathCairo*>(aPath);
  RefPtr<CairoPathContext> ctx = const_cast<PathCairo*>(path)->GetPathContext();
  return ctx == this;
}

PathBuilderCairo::PathBuilderCairo(CairoPathContext* aPathContext,
                                   FillRule aFillRule,
                                   const Matrix& aTransform /* = Matrix() */)
 : mPathContext(aPathContext)
 , mTransform(aTransform)
 , mFillRule(aFillRule)
{}

PathBuilderCairo::PathBuilderCairo(cairo_t* aCtx, DrawTargetCairo* aDrawTarget, FillRule aFillRule)
 : mPathContext(new CairoPathContext(aCtx, aDrawTarget))
 , mTransform(aDrawTarget->GetTransform())
 , mFillRule(aFillRule)
{}

void
PathBuilderCairo::MoveTo(const Point &aPoint)
{
  PrepareForWrite();
  CairoTempMatrix tempMatrix(*mPathContext, mTransform);
  cairo_move_to(*mPathContext, aPoint.x, aPoint.y);
}

void
PathBuilderCairo::LineTo(const Point &aPoint)
{
  PrepareForWrite();
  CairoTempMatrix tempMatrix(*mPathContext, mTransform);
  cairo_line_to(*mPathContext, aPoint.x, aPoint.y);
}

void
PathBuilderCairo::BezierTo(const Point &aCP1,
                           const Point &aCP2,
                           const Point &aCP3)
{
  PrepareForWrite();
  CairoTempMatrix tempMatrix(*mPathContext, mTransform);
  cairo_curve_to(*mPathContext, aCP1.x, aCP1.y, aCP2.x, aCP2.y, aCP3.x, aCP3.y);
}

void
PathBuilderCairo::QuadraticBezierTo(const Point &aCP1,
                                    const Point &aCP2)
{
  PrepareForWrite();
  CairoTempMatrix tempMatrix(*mPathContext, mTransform);

  // We need to elevate the degree of this quadratic Bézier to cubic, so we're
  // going to add an intermediate control point, and recompute control point 1.
  // The first and last control points remain the same.
  // This formula can be found on http://fontforge.sourceforge.net/bezier.html
  Point CP0 = CurrentPoint();
  Point CP1 = (CP0 + aCP1 * 2.0) / 3.0;
  Point CP2 = (aCP2 + aCP1 * 2.0) / 3.0;
  Point CP3 = aCP2;

  cairo_curve_to(*mPathContext, CP1.x, CP1.y, CP2.x, CP2.y, CP3.x, CP3.y);
}

void
PathBuilderCairo::Close()
{
  PrepareForWrite();
  cairo_close_path(*mPathContext);
}

void
PathBuilderCairo::Arc(const Point &aOrigin, float aRadius, float aStartAngle,
                     float aEndAngle, bool aAntiClockwise)
{
  ArcToBezier(this, aOrigin, aRadius, aStartAngle, aEndAngle, aAntiClockwise);
}

Point
PathBuilderCairo::CurrentPoint() const
{
  CairoTempMatrix tempMatrix(*mPathContext, mTransform);
  double x, y;
  cairo_get_current_point(*mPathContext, &x, &y);
  return Point((Float)x, (Float)y);
}

TemporaryRef<Path>
PathBuilderCairo::Finish()
{
  return new PathCairo(mPathContext, mTransform, mFillRule);
}

TemporaryRef<CairoPathContext>
PathBuilderCairo::GetPathContext()
{
  return mPathContext;
}

void
PathBuilderCairo::PrepareForWrite()
{
  // Only PathBuilder and PathCairo maintain references to CairoPathContext.
  // DrawTarget does not. If we're sharing a reference to the context then we
  // need to create a copy that we can modify. This provides copy on write
  // behaviour.
  if (mPathContext->refCount() != 1) {
    mPathContext = new CairoPathContext(*mPathContext);
  }
}

PathCairo::PathCairo(CairoPathContext* aPathContext, Matrix& aTransform,
                     FillRule aFillRule)
 : mPathContext(aPathContext)
 , mTransform(aTransform)
 , mFillRule(aFillRule)
{}

TemporaryRef<PathBuilder>
PathCairo::CopyToBuilder(FillRule aFillRule) const
{
  return new PathBuilderCairo(mPathContext, aFillRule, mTransform);
}

TemporaryRef<PathBuilder>
PathCairo::TransformedCopyToBuilder(const Matrix &aTransform, FillRule aFillRule) const
{
  // We are given the transform we would apply from device space to user space.
  // However in cairo our path is in device space so we view the transform as
  // being the other way round. We therefore need to apply the inverse transform
  // to our current cairo transform.
  Matrix inverse = aTransform;
  inverse.Invert();

  return new PathBuilderCairo(mPathContext, aFillRule, mTransform * inverse);
}

bool
PathCairo::ContainsPoint(const Point &aPoint, const Matrix &aTransform) const
{
  CairoTempMatrix temp(*mPathContext, mTransform);

  Matrix inverse = aTransform;
  inverse.Invert();
  Point transformed = inverse * aPoint;

  // Needs the correct fill rule set.
  cairo_set_fill_rule(*mPathContext, GfxFillRuleToCairoFillRule(mFillRule));
  return cairo_in_fill(*mPathContext, transformed.x, transformed.y);
}

bool
PathCairo::StrokeContainsPoint(const StrokeOptions &aStrokeOptions,
                               const Point &aPoint,
                               const Matrix &aTransform) const
{
  CairoTempMatrix temp(*mPathContext, mTransform);

  Matrix inverse = aTransform;
  inverse.Invert();
  Point transformed = inverse * aPoint;

  SetCairoStrokeOptions(*mPathContext, aStrokeOptions);
  return cairo_in_stroke(*mPathContext, transformed.x, transformed.y);
}

Rect
PathCairo::GetBounds(const Matrix &aTransform) const
{
  CairoTempMatrix temp(*mPathContext, mTransform);

  double x1, y1, x2, y2;

  cairo_path_extents(*mPathContext, &x1, &y1, &x2, &y2);
  Rect bounds(Float(x1), Float(y1), Float(x2 - x1), Float(y2 - y1));
  return aTransform.TransformBounds(bounds);
}

Rect
PathCairo::GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                            const Matrix &aTransform) const
{
  CairoTempMatrix temp(*mPathContext, mTransform);

  double x1, y1, x2, y2;

  SetCairoStrokeOptions(*mPathContext, aStrokeOptions);

  cairo_stroke_extents(*mPathContext, &x1, &y1, &x2, &y2);
  Rect bounds((Float)x1, (Float)y1, (Float)(x2 - x1), (Float)(y2 - y1));
  return aTransform.TransformBounds(bounds);
}

TemporaryRef<CairoPathContext>
PathCairo::GetPathContext()
{
  return mPathContext;
}

void
PathCairo::CopyPathTo(cairo_t* aContext, DrawTargetCairo* aDrawTarget)
{
  mPathContext->CopyPathTo(aContext, mTransform);
  cairo_set_fill_rule(aContext, GfxFillRuleToCairoFillRule(mFillRule));
}

}
}
