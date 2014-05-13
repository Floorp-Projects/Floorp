/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmspaceFrame.h"
#include "nsMathMLElement.h"
#include "mozilla/gfx/2D.h"
#include <algorithm>


//
// <mspace> -- space - implementation
//

nsIFrame*
NS_NewMathMLmspaceFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmspaceFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmspaceFrame)

nsMathMLmspaceFrame::~nsMathMLmspaceFrame()
{
}

bool
nsMathMLmspaceFrame::IsLeaf() const
{
  return true;
}

void
nsMathMLmspaceFrame::ProcessAttributes(nsPresContext* aPresContext)
{
  nsAutoString value;

  // width 
  //
  // "Specifies the desired width of the space."
  //
  // values: length
  // default: 0em
  //
  // The default value is "0em", so unitless values can be ignored.
  // <mspace/> is listed among MathML elements allowing negative spacing and
  // the MathML test suite contains "Presentation/TokenElements/mspace/mspace2" 
  // as an example. Hence we allow negative values.
  //
  mWidth = 0;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::width, value);
  if (!value.IsEmpty()) {
    ParseNumericValue(value, &mWidth,
                      nsMathMLElement::PARSE_ALLOW_NEGATIVE,
                      aPresContext, mStyleContext);
  }

  // height
  //
  // "Specifies the desired height (above the baseline) of the space."
  //
  // values: length
  // default: 0ex
  //
  // The default value is "0ex", so unitless values can be ignored.
  // We do not allow negative values. See bug 716349.
  //
  mHeight = 0;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::height, value);
  if (!value.IsEmpty()) {
    ParseNumericValue(value, &mHeight, 0,
                      aPresContext, mStyleContext);
  }

  // depth
  //
  // "Specifies the desired depth (below the baseline) of the space."
  //
  // values: length
  // default: 0ex
  //
  // The default value is "0ex", so unitless values can be ignored.
  // We do not allow negative values. See bug 716349.
  //
  mDepth = 0;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::depth_, value);
  if (!value.IsEmpty()) {
    ParseNumericValue(value, &mDepth, 0,
                      aPresContext, mStyleContext);
  }
}

void
nsMathMLmspaceFrame::Reflow(nsPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  ProcessAttributes(aPresContext);

  mBoundingMetrics = nsBoundingMetrics();
  mBoundingMetrics.width = mWidth;
  mBoundingMetrics.ascent = mHeight;
  mBoundingMetrics.descent = mDepth;
  mBoundingMetrics.leftBearing = 0;
  mBoundingMetrics.rightBearing = mBoundingMetrics.width;

  aDesiredSize.SetTopAscent(mHeight);
  aDesiredSize.Width() = std::max(0, mBoundingMetrics.width);
  aDesiredSize.Height() = aDesiredSize.TopAscent() + mDepth;
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

/* virtual */ nsresult
nsMathMLmspaceFrame::MeasureForWidth(nsRenderingContext& aRenderingContext,
                                     nsHTMLReflowMetrics& aDesiredSize)
{
  ProcessAttributes(PresContext());
  mBoundingMetrics = nsBoundingMetrics();
  mBoundingMetrics.width = mWidth;
  aDesiredSize.Width() = std::max(0, mBoundingMetrics.width);
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}
