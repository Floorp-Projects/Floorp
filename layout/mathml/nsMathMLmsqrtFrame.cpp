/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmsqrtFrame.h"
#include "mozilla/gfx/2D.h"

//
// <msqrt> -- form a radical - implementation
//

nsIFrame*
NS_NewMathMLmsqrtFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmsqrtFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmsqrtFrame)

nsMathMLmsqrtFrame::nsMathMLmsqrtFrame(nsStyleContext* aContext) :
  nsMathMLmencloseFrame(aContext)
{
}

nsMathMLmsqrtFrame::~nsMathMLmsqrtFrame()
{
}

void
nsMathMLmsqrtFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
  AllocateMathMLChar(NOTATION_RADICAL);
  mNotationsToDraw |= NOTATION_RADICAL;
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::InheritAutomaticData(nsIFrame* aParent)
{
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  return NS_OK;
}

nsresult
nsMathMLmsqrtFrame::AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType)
{
  return nsMathMLContainerFrame::
    AttributeChanged(aNameSpaceID, aAttribute, aModType);
}
