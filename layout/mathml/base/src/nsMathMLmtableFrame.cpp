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
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsAreaFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleChangeList.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsMathMLmtableFrame.h"

//
// <mtable> -- table or matrix - implementation
//

// --------
// implementation of nsMathMLmtableOuterFrame

nsresult
NS_NewMathMLmtableOuterFrame (nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtableOuterFrame* it = new (aPresShell) nsMathMLmtableOuterFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
}

nsMathMLmtableOuterFrame::nsMathMLmtableOuterFrame()
  :nsTableOuterFrame()
{
}

nsMathMLmtableOuterFrame::~nsMathMLmtableOuterFrame()
{
}

NS_INTERFACE_MAP_BEGIN(nsMathMLmtableOuterFrame)
  NS_INTERFACE_MAP_ENTRY(nsIMathMLFrame)
NS_INTERFACE_MAP_END_INHERITING(nsTableOuterFrame)

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLmtableOuterFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLmtableOuterFrame::Release(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Init(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParent,
                               nsIStyleContext* aContext,
                               nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsTableOuterFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  return rv;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Reflow(nsIPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  // we want to return a table that is centered about the axis
  nsresult rv = nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  nsCOMPtr<nsIFontMetrics> fm;
  aPresContext->GetMetricsFor(font.mFont, getter_AddRefs(fm));

  nscoord axisHeight;
  GetAxisHeight(fm, axisHeight);

  // center about the axis
  aDesiredSize.ascent = aDesiredSize.height/2 + axisHeight;
  aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;

  // just make-up a bounding metrics
  mBoundingMetrics.Clear();
  mBoundingMetrics.ascent = aDesiredSize.ascent;
  mBoundingMetrics.descent = aDesiredSize.descent;
  mBoundingMetrics.width = aDesiredSize.width;
  mBoundingMetrics.leftBearing = 0;
  mBoundingMetrics.rightBearing = aDesiredSize.width;

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  return rv;
}

// --------
// implementation of nsMathMLmtdFrame

nsresult
NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtdFrame* it = new (aPresShell) nsMathMLmtdFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
// XXX RBS -  what about mFlags ?
// it->SetFlags(NS_AREA_WRAP_SIZE); ?
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmtdFrame::nsMathMLmtdFrame()
{
}

nsMathMLmtdFrame::~nsMathMLmtdFrame()
{
}

NS_IMETHODIMP
nsMathMLmtdFrame::Reflow(nsIPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)

{
  nsresult  rv = NS_OK;
   
  // Let the base class do the reflow
  rv = nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // more about <maligngroup/> and <malignmark/> later
  // ...
  
  return rv;
}
