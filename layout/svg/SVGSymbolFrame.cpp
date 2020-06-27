/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGSymbolFrame.h"

#include "mozilla/PresShell.h"

nsIFrame* NS_NewSVGSymbolFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGSymbolFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGSymbolFrame)

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(SVGSymbolFrame)
  NS_QUERYFRAME_ENTRY(SVGSymbolFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGViewportFrame)

#ifdef DEBUG
void SVGSymbolFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                          nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::symbol),
               "Content is not an SVG 'symbol' element!");

  SVGViewportFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

}  // namespace mozilla
