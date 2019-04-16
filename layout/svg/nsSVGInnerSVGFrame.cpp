/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGInnerSVGFrame.h"

#include "mozilla/PresShell.h"

using namespace mozilla;

nsIFrame* NS_NewSVGInnerSVGFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsSVGInnerSVGFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGInnerSVGFrame)

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(nsSVGInnerSVGFrame)
  NS_QUERYFRAME_ENTRY(nsSVGInnerSVGFrame)
  NS_QUERYFRAME_ENTRY(nsISVGSVGFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGViewportFrame)

#ifdef DEBUG
void nsSVGInnerSVGFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                              nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::svg),
               "Content is not an SVG 'svg' element!");

  nsSVGViewportFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */
