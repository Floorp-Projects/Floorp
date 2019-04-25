/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_CSSCLIPPATHINSTANCE_H__
#define __NS_CSSCLIPPATHINSTANCE_H__

#include "gfxMatrix.h"
#include "gfxPoint.h"
#include "mozilla/gfx/2D.h"
#include "nsRect.h"
#include "nsStyleStruct.h"

class nsIFrame;
class gfxContext;

namespace mozilla {

class nsCSSClipPathInstance {
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::Rect Rect;

 public:
  static void ApplyBasicShapeOrPathClip(gfxContext& aContext, nsIFrame* aFrame,
                                        const gfxMatrix& aTransform);
  // aPoint is in CSS pixels.
  static bool HitTestBasicShapeOrPathClip(nsIFrame* aFrame,
                                          const gfxPoint& aPoint);

  static Rect GetBoundingRectForBasicShapeOrPathClip(
      nsIFrame* aFrame, const StyleShapeSource& aClipPathStyle);

 private:
  explicit nsCSSClipPathInstance(nsIFrame* aFrame,
                                 const StyleShapeSource& aClipPathStyle)
      : mTargetFrame(aFrame), mClipPathStyle(aClipPathStyle) {}

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

  already_AddRefed<Path> CreateClipPathPath(DrawTarget* aDrawTarget);

  /**
   * The frame for the element that is currently being clipped.
   */
  nsIFrame* mTargetFrame;
  StyleShapeSource mClipPathStyle;
};

}  // namespace mozilla

#endif
