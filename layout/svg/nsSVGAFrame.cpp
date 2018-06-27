/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "gfxMatrix.h"
#include "mozilla/dom/SVGAElement.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsAutoPtr.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "SVGLengthList.h"

using namespace mozilla;

class nsSVGAFrame : public nsSVGDisplayContainerFrame
{
  friend nsIFrame*
  NS_NewSVGAFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle);
protected:
  explicit nsSVGAFrame(ComputedStyle* aStyle)
    : nsSVGDisplayContainerFrame(aStyle, kClassID)
  {}

public:
  NS_DECL_FRAMEARENA_HELPERS(nsSVGAFrame)

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

  // nsIFrame:
  virtual nsresult  AttributeChanged(int32_t         aNameSpaceID,
                                     nsAtom*        aAttribute,
                                     int32_t         aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGA"), aResult);
  }
#endif
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGAFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) nsSVGAFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGAFrame)

//----------------------------------------------------------------------
// nsIFrame methods
#ifdef DEBUG
void
nsSVGAFrame::Init(nsIContent*       aContent,
                  nsContainerFrame* aParent,
                  nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::a),
               "Trying to construct an SVGAFrame for a "
               "content element that doesn't support the right interfaces");

  nsSVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsresult
nsSVGAFrame::AttributeChanged(int32_t         aNameSpaceID,
                              nsAtom*        aAttribute,
                              int32_t         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::transform) {
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

    dom::SVGAElement* content = static_cast<dom::SVGAElement*>(GetContent());

    // SMIL may change whether an <a> element is a link, in which case we will
    // need to update the link state.
    content->ResetLinkState(true, content->ElementHasHref());
  }

 return NS_OK;
}
