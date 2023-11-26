/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGFOREIGNOBJECTFRAME_H_
#define LAYOUT_SVG_SVGFOREIGNOBJECTFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/PresShellForwards.h"
#include "mozilla/UniquePtr.h"
#include "nsContainerFrame.h"

class gfxContext;

nsContainerFrame* NS_NewSVGForeignObjectFrame(mozilla::PresShell* aPresShell,
                                              mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGForeignObjectFrame final : public nsContainerFrame,
                                    public ISVGDisplayableFrame {
  friend nsContainerFrame* ::NS_NewSVGForeignObjectFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

 protected:
  explicit SVGForeignObjectFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext);

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGForeignObjectFrame)

  // nsIFrame:
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  bool IsSVGTransformed(Matrix* aOwnTransform,
                        Matrix* aFromParentTransform) const override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGForeignObject"_ns, aResult);
  }
#endif

  // ISVGDisplayableFrame interface:
  void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                imgDrawingParams& aImgParams) override;
  nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;
  SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                              uint32_t aFlags) override;
  bool IsDisplayContainer() override { return true; }

  gfxMatrix GetCanvasTM();

  // Return our ::-moz-svg-foreign-content anonymous box.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

 protected:
  // implementation helpers:
  void DoReflow();
  void RequestReflow(IntrinsicDirty aType);

  // If width or height is less than or equal to zero we must disable rendering
  bool IsDisabled() const { return mRect.width <= 0 || mRect.height <= 0; }

  UniquePtr<gfxMatrix> mCanvasTM;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGFOREIGNOBJECTFRAME_H_
