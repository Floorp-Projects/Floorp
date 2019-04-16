/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "mozilla/PresShell.h"
#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsLiteralString.h"
#include "SVGObserverUtils.h"
#include "SVGFilters.h"

using namespace mozilla;

/*
 * This frame is used by filter primitive elements that
 * have special child elements that provide parameters.
 */
class SVGFEContainerFrame final : public nsContainerFrame {
  friend nsIFrame* NS_NewSVGFEContainerFrame(mozilla::PresShell* aPresShell,
                                             ComputedStyle* aStyle);

 protected:
  explicit SVGFEContainerFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID) {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);
  }

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGFEContainerFrame)

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    if (aFlags & eSupportsContainLayoutAndPaint) {
      return false;
    }

    return nsContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGContainer));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGFEContainer"), aResult);
  }
#endif

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override {
    // We don't maintain a visual overflow rect
    return false;
  }
};

nsIFrame* NS_NewSVGFEContainerFrame(PresShell* aPresShell,
                                    ComputedStyle* aStyle) {
  return new (aPresShell)
      SVGFEContainerFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFEContainerFrame)

#ifdef DEBUG
void SVGFEContainerFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                               nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eFILTER),
               "Trying to construct an SVGFEContainerFrame for a "
               "content element that doesn't support the right interfaces");

  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsresult SVGFEContainerFrame::AttributeChanged(int32_t aNameSpaceID,
                                               nsAtom* aAttribute,
                                               int32_t aModType) {
  dom::SVGFE* element = static_cast<dom::SVGFE*>(GetContent());
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    MOZ_ASSERT(
        GetParent()->IsSVGFilterFrame(),
        "Observers observe the filter, so that's what we must invalidate");
    SVGObserverUtils::InvalidateDirectRenderingObservers(GetParent());
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}
