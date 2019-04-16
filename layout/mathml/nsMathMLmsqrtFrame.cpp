/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmsqrtFrame.h"

#include "mozilla/PresShell.h"
#include "mozilla/gfx/2D.h"

//
// <msqrt> -- form a radical - implementation
//

using namespace mozilla;

nsIFrame* NS_NewMathMLmsqrtFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsMathMLmsqrtFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmsqrtFrame)

nsMathMLmsqrtFrame::nsMathMLmsqrtFrame(ComputedStyle* aStyle,
                                       nsPresContext* aPresContext)
    : nsMathMLmencloseFrame(aStyle, aPresContext, kClassID) {}

nsMathMLmsqrtFrame::~nsMathMLmsqrtFrame() {}

void nsMathMLmsqrtFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                              nsIFrame* aPrevInFlow) {
  nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
  AllocateMathMLChar(NOTATION_RADICAL);
  mNotationsToDraw += NOTATION_RADICAL;
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::InheritAutomaticData(nsIFrame* aParent) {
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  return NS_OK;
}

nsresult nsMathMLmsqrtFrame::AttributeChanged(int32_t aNameSpaceID,
                                              nsAtom* aAttribute,
                                              int32_t aModType) {
  return nsMathMLContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                                  aModType);
}
