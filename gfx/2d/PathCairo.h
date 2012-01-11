/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_GFX_PATH_CAIRO_H_
#define MOZILLA_GFX_PATH_CAIRO_H_

#include "2D.h"
#include "cairo.h"

namespace mozilla {
namespace gfx {

class DrawTargetCairo;

// A reference to a cairo context that can maintain and set a path.
//
// This class exists to make it possible for us to not construct paths manually
// using cairo_path_t, which in the common case is a speed and memory
// optimization (as the cairo_t maintains the path for us, and we don't have to
// use cairo_append_path). Instead, we can share a cairo_t with a DrawTarget,
// and have it inform us when we need to make a copy of the path.
//
// Exactly one Path* object represents the current path on a given DrawTarget's
// context. That Path* object registers its CairoPathContext with the
// DrawTarget it's associated with. If that DrawTarget is going to change its
// path, it has to tell the CairoPathContext beforehand so the path can be
// saved off.
// The path ownership is transferred to every new instance of CairoPathContext
// in the constructor. We inform the draw target of the new context object,
// which causes us to save off a copy of the path, as we're not going to be
// informed upon changes any more.
// Any transformation on aCtx is not applied to this path, though a path can be
// transformed separately from its context by passing a matrix to the
// constructor.
class CairoPathContext : public RefCounted<CairoPathContext>
{
public:
  // Construct a CairoPathContext and set it to be the path observer of
  // aDrawTarget. Optionally, this path can be transformed by aMatrix.
  CairoPathContext(cairo_t* aCtx, DrawTargetCairo* aDrawTarget,
                   FillRule aFillRule,
                   const Matrix& aMatrix = Matrix());
  ~CairoPathContext();

  // Copy the path on mContext to be the path on aToContext, if they aren't the
  // same.
  void CopyPathTo(cairo_t* aToContext);

  // This method must be called by the draw target before it changes the path
  // currently on the cairo context.
  void PathWillChange();

  // This method must be called by the draw target whenever it is going to
  // change the current transformation on mContext.
  void MatrixWillChange(const Matrix& aMatrix);

  // This method must be called as the draw target is dying. In this case, we
  // forget our reference to the draw target, and become the only reference to
  // our context.
  void ForgetDrawTarget();

  // Create a duplicate context, and copy this path to that context. Optionally,
  // the new context can be transformed.
  void DuplicateContextAndPath(const Matrix& aMatrix = Matrix());

  // Returns true if this CairoPathContext represents path.
  bool ContainsPath(const Path* path);

  cairo_t* GetContext() const { return mContext; }
  DrawTargetCairo* GetDrawTarget() const { return mDrawTarget; }
  Matrix GetTransform() const { return mTransform; }
  FillRule GetFillRule() const { return mFillRule; }

  operator cairo_t* () const { return mContext; }

private: // methods
  CairoPathContext(const CairoPathContext&) MOZ_DELETE;

private: // data
  Matrix mTransform;
  cairo_t* mContext;
  // Not a RefPtr to avoid cycles.
  DrawTargetCairo* mDrawTarget;
  FillRule mFillRule;
};

class PathBuilderCairo : public PathBuilder
{
public:
  // This constructor implicitly takes ownership of aCtx by calling
  // aDrawTarget->SetPathObserver(). Therefore, if the draw target has a path
  // observer, this constructor will cause it to copy out its path.
  // The path currently set on aCtx is not changed.
  PathBuilderCairo(cairo_t* aCtx, DrawTargetCairo* aDrawTarget, FillRule aFillRule);

  // This constructor, called with a CairoPathContext*, implicitly takes
  // ownership of the path, and therefore makes aPathContext copy out its path
  // regardless of whether it has a pointer to a DrawTargetCairo.
  // The path currently set on aPathContext is not changed.
  explicit PathBuilderCairo(CairoPathContext* aPathContext,
                            const Matrix& aTransform = Matrix());

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

  TemporaryRef<CairoPathContext> GetPathContext();

private: // methods
  void SetFillRule(FillRule aFillRule);

private: // data
  RefPtr<CairoPathContext> mPathContext;
  FillRule mFillRule;
};

class PathCairo : public Path
{
public:
  PathCairo(cairo_t* aCtx, DrawTargetCairo* aDrawTarget, FillRule aFillRule, const Matrix& aTransform);

  virtual BackendType GetBackendType() const { return BACKEND_CAIRO; }

  virtual TemporaryRef<PathBuilder> CopyToBuilder(FillRule aFillRule = FILL_WINDING) const;
  virtual TemporaryRef<PathBuilder> TransformedCopyToBuilder(const Matrix &aTransform,
                                                             FillRule aFillRule = FILL_WINDING) const;

  virtual bool ContainsPoint(const Point &aPoint, const Matrix &aTransform) const;

  virtual Rect GetBounds(const Matrix &aTransform = Matrix()) const;

  virtual Rect GetStrokedBounds(const StrokeOptions &aStrokeOptions,
                                const Matrix &aTransform = Matrix()) const;

  virtual FillRule GetFillRule() const { return mFillRule; }

  TemporaryRef<CairoPathContext> GetPathContext();

  // Set this path to be the current path for aContext (if it's not already
  // aContext's path). You must pass the draw target associated with the
  // context as aDrawTarget.
  void CopyPathTo(cairo_t* aContext, DrawTargetCairo* aDrawTarget);

private:
  RefPtr<CairoPathContext> mPathContext;
  FillRule mFillRule;
};

}
}

#endif /* MOZILLA_GFX_PATH_CAIRO_H_ */
