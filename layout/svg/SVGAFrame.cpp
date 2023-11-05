/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "gfxMatrix.h"
#include "mozilla/PresShell.h"
#include "mozilla/SVGContainerFrame.h"
#include "mozilla/dom/SVGAElement.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "SVGLengthList.h"

nsIFrame* NS_NewSVGAFrame(mozilla::PresShell* aPresShell,
                          mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGAFrame final : public SVGDisplayContainerFrame {
  friend nsIFrame* ::NS_NewSVGAFrame(mozilla::PresShell* aPresShell,
                                     ComputedStyle* aStyle);

 protected:
  explicit SVGAFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGDisplayContainerFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGAFrame)

#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

  // nsIFrame:
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGA"_ns, aResult);
  }
#endif
};

}  // namespace mozilla

//----------------------------------------------------------------------
// Implementation

nsIFrame* NS_NewSVGAFrame(mozilla::PresShell* aPresShell,
                          mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGAFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGAFrame)

//----------------------------------------------------------------------
// nsIFrame methods
#ifdef DEBUG
void SVGAFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                     nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::a),
               "Trying to construct an SVGAFrame for a "
               "content element that doesn't support the right interfaces");

  SVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsresult SVGAFrame::AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                     int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::transform) {
    // We don't invalidate for transform changes (the layers code does that).
    // Also note that SVGTransformableElement::GetAttributeChangeHint will
    // return nsChangeHint_UpdateOverflow for "transform" attribute changes
    // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.
    NotifySVGChanged(TRANSFORM_CHANGED);
  }

  // Currently our SMIL implementation does not modify the DOM attributes. Once
  // we implement the SVG 2 SMIL behaviour this can be removed
  // SVGAElement::SetAttr/UnsetAttr's ResetLinkState() call will be sufficient.
  if (aModType == dom::MutationEvent_Binding::SMIL &&
      aAttribute == nsGkAtoms::href &&
      (aNameSpaceID == kNameSpaceID_None ||
       aNameSpaceID == kNameSpaceID_XLink)) {
    auto* content = static_cast<dom::SVGAElement*>(GetContent());

    // SMIL may change whether an <a> element is a link, in which case we will
    // need to update the link state.
    content->ResetLinkState(true, content->ElementHasHref());
  }

  return NS_OK;
}

}  // namespace mozilla
