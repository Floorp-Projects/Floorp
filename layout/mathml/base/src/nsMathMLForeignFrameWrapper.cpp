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

//
// a helper frame class to wrap non-MathML frames so that foreign elements 
// (e.g., html:img) can mix better with other surrounding MathML markups
//

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsFrame.h"
#include "nsAreaFrame.h"
#include "nsLineLayout.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLForeignFrameWrapper.h"

NS_IMPL_ADDREF_INHERITED(nsMathMLForeignFrameWrapper, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLForeignFrameWrapper, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLForeignFrameWrapper, nsBlockFrame, nsMathMLFrame)

nsresult
NS_NewMathMLForeignFrameWrapper(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLForeignFrameWrapper* it = new (aPresShell) nsMathMLForeignFrameWrapper;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLForeignFrameWrapper::Init(nsIPresContext*  aPresContext,
                                  nsIContent*      aContent,
                                  nsIFrame*        aParent,
                                  nsStyleContext*  aContext,
                                  nsIFrame*        aPrevInFlow)
{
  return nsBlockFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

NS_IMETHODIMP
nsMathMLForeignFrameWrapper::Reflow(nsIPresContext*          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsReflowStatus&          aStatus)
{
  // Let the base class do the reflow
  nsresult rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  // just make-up a bounding metrics
  mBoundingMetrics.Clear();
  mBoundingMetrics.ascent = aDesiredSize.ascent;
  mBoundingMetrics.descent = aDesiredSize.descent;
  mBoundingMetrics.width = aDesiredSize.width;
  mBoundingMetrics.leftBearing = 0;
  mBoundingMetrics.rightBearing = aDesiredSize.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}
