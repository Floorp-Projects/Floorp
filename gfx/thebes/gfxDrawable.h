/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DRAWABLE_H
#define GFX_DRAWABLE_H

#include "gfxRect.h"
#include "gfxMatrix.h"
#include "gfxTypes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "nsISupportsImpl.h"

class gfxContext;
class gfxPattern;

/**
 * gfxDrawable
 * An Interface representing something that has an intrinsic size and can draw
 * itself repeatedly.
 */
class gfxDrawable {
  NS_INLINE_DECL_REFCOUNTING(gfxDrawable)
 public:
  typedef mozilla::gfx::AntialiasMode AntialiasMode;
  typedef mozilla::gfx::CompositionOp CompositionOp;
  typedef mozilla::gfx::DrawTarget DrawTarget;

  explicit gfxDrawable(const mozilla::gfx::IntSize aSize) : mSize(aSize) {}

  /**
   * Draw into aContext filling aFillRect, possibly repeating, using
   * aSamplingFilter. aTransform is a userspace to "image"space matrix. For
   * example, if Draw draws using a gfxPattern, this is the matrix that should
   * be set on the pattern prior to rendering it.
   *  @return whether drawing was successful
   */
  virtual bool Draw(gfxContext* aContext, const gfxRect& aFillRect,
                    mozilla::gfx::ExtendMode aExtendMode,
                    const mozilla::gfx::SamplingFilter aSamplingFilter,
                    gfxFloat aOpacity = 1.0,
                    const gfxMatrix& aTransform = gfxMatrix()) = 0;

  virtual bool DrawWithSamplingRect(
      DrawTarget* aDrawTarget, CompositionOp aOp, AntialiasMode aAntialiasMode,
      const gfxRect& aFillRect, const gfxRect& aSamplingRect,
      mozilla::gfx::ExtendMode aExtendMode,
      const mozilla::gfx::SamplingFilter aSamplingFilter,
      gfxFloat aOpacity = 1.0) {
    return false;
  }

  virtual mozilla::gfx::IntSize Size() { return mSize; }

 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~gfxDrawable() = default;

  const mozilla::gfx::IntSize mSize;
};

/**
 * gfxSurfaceDrawable
 * A convenience implementation of gfxDrawable for surfaces.
 */
class gfxSurfaceDrawable : public gfxDrawable {
 public:
  gfxSurfaceDrawable(mozilla::gfx::SourceSurface* aSurface,
                     const mozilla::gfx::IntSize aSize,
                     const gfxMatrix aTransform = gfxMatrix());
  virtual ~gfxSurfaceDrawable() = default;

  virtual bool Draw(gfxContext* aContext, const gfxRect& aFillRect,
                    mozilla::gfx::ExtendMode aExtendMode,
                    const mozilla::gfx::SamplingFilter aSamplingFilter,
                    gfxFloat aOpacity = 1.0,
                    const gfxMatrix& aTransform = gfxMatrix()) override;

  virtual bool DrawWithSamplingRect(
      DrawTarget* aDrawTarget, CompositionOp aOp, AntialiasMode aAntialiasMode,
      const gfxRect& aFillRect, const gfxRect& aSamplingRect,
      mozilla::gfx::ExtendMode aExtendMode,
      const mozilla::gfx::SamplingFilter aSamplingFilter,
      gfxFloat aOpacity = 1.0) override;

 protected:
  void DrawInternal(DrawTarget* aDrawTarget, CompositionOp aOp,
                    AntialiasMode aAntialiasMode, const gfxRect& aFillRect,
                    const mozilla::gfx::IntRect& aSamplingRect,
                    mozilla::gfx::ExtendMode aExtendMode,
                    const mozilla::gfx::SamplingFilter aSamplingFilter,
                    gfxFloat aOpacity,
                    const gfxMatrix& aTransform = gfxMatrix());

  RefPtr<mozilla::gfx::SourceSurface> mSourceSurface;
  const gfxMatrix mTransform;
};

/**
 * gfxDrawingCallback
 * A simple drawing functor.
 */
class gfxDrawingCallback {
  NS_INLINE_DECL_REFCOUNTING(gfxDrawingCallback)
 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~gfxDrawingCallback() = default;

 public:
  /**
   * Draw into aContext filling aFillRect using aSamplingFilter.
   * aTransform is a userspace to "image"space matrix. For example, if Draw
   * draws using a gfxPattern, this is the matrix that should be set on the
   * pattern prior to rendering it.
   *  @return whether drawing was successful
   */
  virtual bool operator()(gfxContext* aContext, const gfxRect& aFillRect,
                          const mozilla::gfx::SamplingFilter aSamplingFilter,
                          const gfxMatrix& aTransform = gfxMatrix()) = 0;
};

/**
 * gfxCallbackDrawable
 * A convenience implementation of gfxDrawable for callbacks.
 */
class gfxCallbackDrawable : public gfxDrawable {
 public:
  gfxCallbackDrawable(gfxDrawingCallback* aCallback,
                      const mozilla::gfx::IntSize aSize);
  virtual ~gfxCallbackDrawable() = default;

  virtual bool Draw(gfxContext* aContext, const gfxRect& aFillRect,
                    mozilla::gfx::ExtendMode aExtendMode,
                    const mozilla::gfx::SamplingFilter aSamplingFilter,
                    gfxFloat aOpacity = 1.0,
                    const gfxMatrix& aTransform = gfxMatrix()) override;

 protected:
  already_AddRefed<gfxSurfaceDrawable> MakeSurfaceDrawable(
      gfxContext* aContext, mozilla::gfx::SamplingFilter aSamplingFilter =
                                mozilla::gfx::SamplingFilter::LINEAR);

  RefPtr<gfxDrawingCallback> mCallback;
  RefPtr<gfxSurfaceDrawable> mSurfaceDrawable;
};

/**
 * gfxPatternDrawable
 * A convenience implementation of gfxDrawable for patterns.
 */
class gfxPatternDrawable : public gfxDrawable {
 public:
  gfxPatternDrawable(gfxPattern* aPattern, const mozilla::gfx::IntSize aSize);
  virtual ~gfxPatternDrawable();

  virtual bool Draw(gfxContext* aContext, const gfxRect& aFillRect,
                    mozilla::gfx::ExtendMode aExtendMode,
                    const mozilla::gfx::SamplingFilter aSamplingFilter,
                    gfxFloat aOpacity = 1.0,
                    const gfxMatrix& aTransform = gfxMatrix()) override;

 protected:
  already_AddRefed<gfxCallbackDrawable> MakeCallbackDrawable();

  RefPtr<gfxPattern> mPattern;
};

#endif /* GFX_DRAWABLE_H */
