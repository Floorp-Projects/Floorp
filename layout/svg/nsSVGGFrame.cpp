/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGGFrame.h"

// Keep others in (case-insensitive) order:
#include "mozilla/PresShell.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "SVGGraphicsElement.h"
#include "SVGTransformableElement.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
// Implementation

nsIFrame* NS_NewSVGGFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsSVGGFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGGFrame)

#ifdef DEBUG
void nsSVGGFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                       nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement() &&
                   static_cast<SVGElement*>(aContent)->IsTransformable(),
               "The element doesn't support nsIDOMSVGTransformable");

  nsSVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

//----------------------------------------------------------------------
// nsSVGDisplayableFrame methods

nsresult nsSVGGFrame::AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                       int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::transform) {
    // We don't invalidate for transform changes (the layers code does that).
    // Also note that SVGTransformableElement::GetAttributeChangeHint will
    // return nsChangeHint_UpdateOverflow for "transform" attribute changes
    // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.
    NotifySVGChanged(TRANSFORM_CHANGED);
  }

  return NS_OK;
}
