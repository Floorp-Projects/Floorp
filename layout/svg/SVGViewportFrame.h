/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGVIEWPORTFRAME_H_
#define LAYOUT_SVG_SVGVIEWPORTFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/ISVGSVGFrame.h"
#include "mozilla/SVGContainerFrame.h"

class gfxContext;

namespace mozilla {

/**
 * Superclass for inner SVG frames and symbol frames.
 */
class SVGViewportFrame : public SVGDisplayContainerFrame, public ISVGSVGFrame {
 protected:
  SVGViewportFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                   nsIFrame::ClassID aID)
      : SVGDisplayContainerFrame(aStyle, aPresContext, aID) {}

 public:
  NS_DECL_ABSTRACT_FRAME(SVGViewportFrame)

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  // ISVGDisplayableFrame interface:
  void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                imgDrawingParams& aImgParams) override;
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;
  SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                              uint32_t aFlags) override;
  nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;

  // SVGContainerFrame methods:
  bool HasChildrenOnlyTransform(Matrix* aTransform) const override;

  // ISVGSVGFrame interface:
  void NotifyViewportOrTransformChanged(uint32_t aFlags) override;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGVIEWPORTFRAME_H_
