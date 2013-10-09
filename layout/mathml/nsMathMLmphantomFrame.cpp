/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsMathMLmphantomFrame.h"
#include "mozilla/gfx/2D.h"

//
// <mphantom> -- make content invisible but preserve its size
//

nsIFrame*
NS_NewMathMLmphantomFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmphantomFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmphantomFrame)

nsMathMLmphantomFrame::~nsMathMLmphantomFrame()
{
}

NS_IMETHODIMP
nsMathMLmphantomFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  return NS_OK;
}
