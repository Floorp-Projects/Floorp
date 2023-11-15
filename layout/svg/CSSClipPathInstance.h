/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_CSSCLIPPATHINSTANCE_H_
#define LAYOUT_SVG_CSSCLIPPATHINSTANCE_H_

#include "gfxMatrix.h"
#include "gfxPoint.h"
#include "mozilla/gfx/2D.h"
#include "nsRect.h"
#include "nsStyleStruct.h"

class nsIFrame;
class gfxContext;

namespace mozilla {

class MOZ_STACK_CLASS CSSClipPathInstance {
  using DrawTarget = gfx::DrawTarget;
  using Path = gfx::Path;
  using Rect = gfx::Rect;

 public:
  static void ApplyBasicShapeOrPathClip(gfxContext& aContext, nsIFrame* aFrame,
                                        const gfxMatrix& aTransform);
  static RefPtr<Path> CreateClipPathForFrame(gfx::DrawTarget* aDt,
                                             nsIFrame* aFrame,
                                             const gfxMatrix& aTransform);
  // aPoint is in CSS pixels.
  static bool HitTestBasicShapeOrPathClip(nsIFrame* aFrame,
                                          const gfxPoint& aPoint);

  static Maybe<Rect> GetBoundingRectForBasicShapeOrPathClip(
      nsIFrame* aFrame, const StyleClipPath&);

 private:
  explicit CSSClipPathInstance(nsIFrame* aFrame, const StyleClipPath& aClipPath)
      : mTargetFrame(aFrame), mClipPathStyle(aClipPath) {}

  already_AddRefed<Path> CreateClipPath(DrawTarget* aDrawTarget,
                                        const gfxMatrix& aTransform);

  already_AddRefed<Path> CreateClipPathCircle(DrawTarget* aDrawTarget,
                                              const nsRect& aRefBox);

  already_AddRefed<Path> CreateClipPathEllipse(DrawTarget* aDrawTarget,
                                               const nsRect& aRefBox);

  already_AddRefed<Path> CreateClipPathPolygon(DrawTarget* aDrawTarget,
                                               const nsRect& aRefBox);

  already_AddRefed<Path> CreateClipPathInset(DrawTarget* aDrawTarget,
                                             const nsRect& aRefBox);

  already_AddRefed<Path> CreateClipPathPath(DrawTarget* aDrawTarget,
                                            const nsRect& aRefBox);

  /**
   * The frame for the element that is currently being clipped.
   */
  nsIFrame* mTargetFrame;
  const StyleClipPath& mClipPathStyle;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_CSSCLIPPATHINSTANCE_H_
