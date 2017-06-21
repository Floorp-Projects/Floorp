/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGSymbolFrame.h"

nsIFrame*
NS_NewSVGSymbolFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGSymbolFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGSymbolFrame)

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(nsSVGSymbolFrame)
  NS_QUERYFRAME_ENTRY(nsSVGSymbolFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGViewportFrame)

#ifdef DEBUG
void
nsSVGSymbolFrame::Init(nsIContent*       aContent,
                       nsContainerFrame* aParent,
                       nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::symbol),
               "Content is not an SVG 'symbol' element!");

  nsSVGViewportFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */