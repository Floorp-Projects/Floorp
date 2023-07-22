/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGPAINTSERVERFRAME_H_
#define LAYOUT_SVG_SVGPAINTSERVERFRAME_H_

#include "gfxRect.h"
#include "mozilla/Attributes.h"
#include "mozilla/SVGContainerFrame.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsIFrame.h"
#include "nsQueryFrame.h"

class gfxContext;
class gfxPattern;

namespace mozilla {
namespace gfx {
class DrawTarget;
}  // namespace gfx

/**
 * RAII class used to temporarily set and remove the
 * NS_FRAME_DRAWING_AS_PAINTSERVER frame state bit while a frame is being
 * drawn as a paint server.
 */
class MOZ_RAII AutoSetRestorePaintServerState {
 public:
  explicit AutoSetRestorePaintServerState(nsIFrame* aFrame) : mFrame(aFrame) {
    mFrame->AddStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);
  }
  ~AutoSetRestorePaintServerState() {
    mFrame->RemoveStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);
  }

 private:
  nsIFrame* mFrame;
};

class SVGPaintServerFrame : public SVGContainerFrame {
 protected:
  using DrawTarget = gfx::DrawTarget;

  SVGPaintServerFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                      ClassID aID)
      : SVGContainerFrame(aStyle, aPresContext, aID) {
    AddStateBits(NS_FRAME_IS_NONDISPLAY |
                 NS_STATE_SVG_RENDERING_OBSERVER_CONTAINER);
  }

 public:
  using imgDrawingParams = image::imgDrawingParams;

  NS_DECL_ABSTRACT_FRAME(SVGPaintServerFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_QUERYFRAME_TARGET(SVGPaintServerFrame)

  /**
   * Constructs a gfxPattern of the paint server rendering.
   *
   * @param aContextMatrix The transform matrix that is currently applied to
   *   the gfxContext that is being drawn to. This is needed by SVG patterns so
   *   that surfaces of the correct size can be created. (SVG gradients are
   *   vector based, so it's not used there.)
   */
  virtual already_AddRefed<gfxPattern> GetPaintServerPattern(
      nsIFrame* aSource, const DrawTarget* aDrawTarget,
      const gfxMatrix& aContextMatrix, StyleSVGPaint nsStyleSVG::*aFillOrStroke,
      float aOpacity, imgDrawingParams& aImgParams,
      const gfxRect* aOverrideBounds = nullptr) = 0;

  // nsIFrame methods:
  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override {}
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGPAINTSERVERFRAME_H_
