/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGMASKFRAME_H__
#define __NS_SVGMASKFRAME_H__

#include "mozilla/Attributes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "gfxPattern.h"
#include "gfxMatrix.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class gfxContext;

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsSVGMaskFrame final : public nsSVGContainerFrame {
  friend nsIFrame* NS_NewSVGMaskFrame(mozilla::PresShell* aPresShell,
                                      ComputedStyle* aStyle);

  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::image::imgDrawingParams imgDrawingParams;

 protected:
  explicit nsSVGMaskFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsSVGContainerFrame(aStyle, aPresContext, kClassID), mInUse(false) {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSVGMaskFrame)

  struct MaskParams {
    gfxContext* ctx;
    nsIFrame* maskedFrame;
    const gfxMatrix& toUserSpace;
    float opacity;
    Matrix* maskTransform;
    mozilla::StyleMaskMode maskMode;
    imgDrawingParams& imgParams;

    explicit MaskParams(gfxContext* aCtx, nsIFrame* aMaskedFrame,
                        const gfxMatrix& aToUserSpace, float aOpacity,
                        Matrix* aMaskTransform,
                        mozilla::StyleMaskMode aMaskMode,
                        imgDrawingParams& aImgParams)
        : ctx(aCtx),
          maskedFrame(aMaskedFrame),
          toUserSpace(aToUserSpace),
          opacity(aOpacity),
          maskTransform(aMaskTransform),
          maskMode(aMaskMode),
          imgParams(aImgParams) {}
  };

  // nsSVGMaskFrame method:

  /**
   * Generate a mask surface for the target frame.
   *
   * The return surface can be null, it's the caller's responsibility to
   * null-check before dereferencing.
   */
  already_AddRefed<SourceSurface> GetMaskForMaskedFrame(MaskParams& aParams);

  gfxRect GetMaskArea(nsIFrame* aMaskedFrame);

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override {}

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGMask"), aResult);
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

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;
};

#endif
