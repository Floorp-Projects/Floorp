/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsMathMLsemanticsFrame.h"

//
// <semantics> -- associate annotations with a MathML expression
//

nsIFrame*
NS_NewMathMLsemanticsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLsemanticsFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLsemanticsFrame)

nsMathMLsemanticsFrame::~nsMathMLsemanticsFrame()
{
}

NS_IMETHODIMP
nsMathMLsemanticsFrame::TransmitAutomaticData()
{
  // The REC defines the following elements to be embellished operators:
  // * one of the elements msub, msup, msubsup, munder, mover, munderover,
  //   mmultiscripts, mfrac, or semantics (Section 5.1 Annotation Framework),
  //   whose first argument exists and is an embellished operator; 
  //
  // If our first child is an embellished operator, its flags bubble to us
  mPresentationData.baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(mPresentationData.baseFrame, mEmbellishData);

  return NS_OK;
}
