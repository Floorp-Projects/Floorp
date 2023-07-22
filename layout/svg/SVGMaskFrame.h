/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGMASKFRAME_H_
#define LAYOUT_SVG_SVGMASKFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SVGContainerFrame.h"
#include "mozilla/gfx/2D.h"
#include "gfxPattern.h"
#include "gfxMatrix.h"

class gfxContext;

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGMaskFrame(mozilla::PresShell* aPresShell,
                             mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGMaskFrame final : public SVGContainerFrame {
  friend nsIFrame* ::NS_NewSVGMaskFrame(mozilla::PresShell* aPresShell,
                                        ComputedStyle* aStyle);

  using Matrix = gfx::Matrix;
  using SourceSurface = gfx::SourceSurface;
  using imgDrawingParams = image::imgDrawingParams;

 protected:
  explicit SVGMaskFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGContainerFrame(aStyle, aPresContext, kClassID), mInUse(false) {
    AddStateBits(NS_FRAME_IS_NONDISPLAY |
                 NS_STATE_SVG_RENDERING_OBSERVER_CONTAINER);
  }

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGMaskFrame)

  struct MaskParams {
    gfx::DrawTarget* dt;
    nsIFrame* maskedFrame;
    const gfxMatrix& toUserSpace;
    float opacity;
    StyleMaskMode maskMode;
    imgDrawingParams& imgParams;

    explicit MaskParams(gfx::DrawTarget* aDt, nsIFrame* aMaskedFrame,
                        const gfxMatrix& aToUserSpace, float aOpacity,
                        StyleMaskMode aMaskMode, imgDrawingParams& aImgParams)
        : dt(aDt),
          maskedFrame(aMaskedFrame),
          toUserSpace(aToUserSpace),
          opacity(aOpacity),
          maskMode(aMaskMode),
          imgParams(aImgParams) {}
  };

  // SVGMaskFrame method:

  /**
   * Generate a mask surface for the target frame.
   *
   * The return surface can be null, it's the caller's responsibility to
   * null-check before dereferencing.
   */
  already_AddRefed<SourceSurface> GetMaskForMaskedFrame(MaskParams& aParams);

  gfxRect GetMaskArea(nsIFrame* aMaskedFrame);

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override {}

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGMask"_ns, aResult);
  }
#endif

 private:
  /**
   * If the mask element transforms its children due to
   * maskContentUnits="objectBoundingBox" being set on it, this function
   * returns the resulting transform.
   */
  gfxMatrix GetMaskTransform(nsIFrame* aMaskedFrame);

  gfxMatrix mMatrixForChildren;
  // recursion prevention flag
  bool mInUse;

  // SVGContainerFrame methods:
  gfxMatrix GetCanvasTM() override;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGMASKFRAME_H_
