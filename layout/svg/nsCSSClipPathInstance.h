/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_CSSCLIPPATHINSTANCE_H__
#define __NS_CSSCLIPPATHINSTANCE_H__

#include "nsStyleStruct.h"
#include "nsRect.h"
#include "mozilla/gfx/2D.h"

class nsIFrame;
class nsRenderingContext;

namespace mozilla {

class nsCSSClipPathInstance
{
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Path Path;

public:
  static void ApplyBasicShapeClip(gfxContext& aContext,
                                  nsIFrame* aFrame);
  // aPoint is in CSS pixels.
  static bool HitTestBasicShapeClip(nsIFrame* aFrame,
                                    const gfxPoint& aPoint);
private:
  explicit nsCSSClipPathInstance(nsIFrame* aFrame,
                                 const StyleClipPath aClipPathStyle)
    : mTargetFrame(aFrame)
    , mClipPathStyle(aClipPathStyle)
  {
  }

  already_AddRefed<Path> CreateClipPath(DrawTarget* aDrawTarget);

  already_AddRefed<Path> CreateClipPathCircle(DrawTarget* aDrawTarget,
                                              const nsRect& aRefBox);

  already_AddRefed<Path> CreateClipPathEllipse(DrawTarget* aDrawTarget,
                                               const nsRect& aRefBox);

  already_AddRefed<Path> CreateClipPathPolygon(DrawTarget* aDrawTarget,
                                               const nsRect& aRefBox);

  /**
   * The frame for the element that is currently being clipped.
   */
  nsIFrame* mTargetFrame;
  StyleClipPath mClipPathStyle;
};

} // namespace mozilla

#endif
