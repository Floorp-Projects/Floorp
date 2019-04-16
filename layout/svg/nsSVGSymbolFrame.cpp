/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGSymbolFrame.h"

#include "mozilla/PresShell.h"

using namespace mozilla;

nsIFrame* NS_NewSVGSymbolFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsSVGSymbolFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGSymbolFrame)

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(nsSVGSymbolFrame)
  NS_QUERYFRAME_ENTRY(nsSVGSymbolFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGViewportFrame)

#ifdef DEBUG
void nsSVGSymbolFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::symbol),
               "Content is not an SVG 'symbol' element!");

  nsSVGViewportFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */
