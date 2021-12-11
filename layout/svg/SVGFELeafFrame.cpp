/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "mozilla/PresShell.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/dom/SVGFilters.h"
#include "ComputedStyle.h"
#include "nsContainerFrame.h"
#include "nsIFrame.h"
#include "nsGkAtoms.h"

nsIFrame* NS_NewSVGFELeafFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle);
namespace mozilla {

/*
 * This frame is used by filter primitive elements that don't
 * have special child elements that provide parameters.
 */
class SVGFELeafFrame final : public nsIFrame {
  friend nsIFrame* ::NS_NewSVGFELeafFrame(mozilla::PresShell* aPresShell,
                                          ComputedStyle* aStyle);

 protected:
  explicit SVGFELeafFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsIFrame(aStyle, aPresContext, kClassID) {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);
  }

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGFELeafFrame)

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    if (aFlags & eSupportsContainLayoutAndPaint) {
      return false;
    }

    return nsIFrame::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGFELeaf"_ns, aResult);
  }
#endif

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual bool ComputeCustomOverflow(OverflowAreas& aOverflowAreas) override {
    // We don't maintain a ink overflow rect
    return false;
  }
};

}  // namespace mozilla

nsIFrame* NS_NewSVGFELeafFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGFELeafFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGFELeafFrame)

#ifdef DEBUG
void SVGFELeafFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                          nsIFrame* aPrevInFlow) {
  nsCOMPtr<SVGFE> filterPrimitive = do_QueryInterface(aContent);
  NS_ASSERTION(filterPrimitive,
               "Trying to construct an SVGFELeafFrame for a "
               "content element that doesn't support the right interfaces");

  nsIFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsresult SVGFELeafFrame::AttributeChanged(int32_t aNameSpaceID,
                                          nsAtom* aAttribute,
                                          int32_t aModType) {
  auto* element = static_cast<mozilla::dom::SVGFE*>(GetContent());
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    MOZ_ASSERT(
        GetParent()->IsSVGFilterFrame(),
        "Observers observe the filter, so that's what we must invalidate");
    SVGObserverUtils::InvalidateDirectRenderingObservers(GetParent());
  }

  return nsIFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

}  // namespace mozilla
