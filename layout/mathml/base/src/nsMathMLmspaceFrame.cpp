/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is The University Of
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsIPresContext.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLmspaceFrame.h"


//
// <mspace> -- space - implementation
//

nsresult
NS_NewMathMLmspaceFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmspaceFrame* it = new (aPresShell) nsMathMLmspaceFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmspaceFrame::nsMathMLmspaceFrame()
{
}

nsMathMLmspaceFrame::~nsMathMLmspaceFrame()
{
}

void
nsMathMLmspaceFrame::ProcessAttributes(nsIPresContext* aPresContext)
{
  /*
  parse the attributes

  width  = number h-unit 
  height = number v-unit 
  depth  = number v-unit 
  */

  nsAutoString value;
  nsCSSValue cssValue;

  // width 
  mWidth = 0;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::width_, value)) {
    if ((ParseNumericValue(value, cssValue) ||
         ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue)) &&
         cssValue.IsLengthUnit()) {
      mWidth = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }

  // height
  mHeight = 0;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::height_, value)) {
    if ((ParseNumericValue(value, cssValue) ||
         ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue)) &&
         cssValue.IsLengthUnit()) {
      mHeight = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }

  // depth
  mDepth = 0;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::depth_, value)) {
    if ((ParseNumericValue(value, cssValue) ||
         ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue)) &&
         cssValue.IsLengthUnit()) {
      mDepth = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }
}

NS_IMETHODIMP
nsMathMLmspaceFrame::Reflow(nsIPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  ProcessAttributes(aPresContext);

  mBoundingMetrics.Clear();
  mBoundingMetrics.width = mWidth;
  mBoundingMetrics.ascent = mHeight;
  mBoundingMetrics.descent = mDepth;

  aDesiredSize.ascent = mHeight;
  aDesiredSize.descent = mDepth;
  aDesiredSize.width = mWidth;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = aDesiredSize.width;
  }
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}
