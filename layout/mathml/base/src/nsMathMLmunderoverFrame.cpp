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
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 */


#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsMathMLmunderoverFrame.h"

//
// <munderover> -- attach an underscript-overscript pair to a base - implementation
//

nsresult
NS_NewMathMLmunderoverFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmunderoverFrame* it = new (aPresShell) nsMathMLmunderoverFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmunderoverFrame::nsMathMLmunderoverFrame()
{
}

nsMathMLmunderoverFrame::~nsMathMLmunderoverFrame()
{
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::Init(nsIPresContext*  aPresContext,
                              nsIContent*      aContent,
                              nsIFrame*        aParent,
                              nsIStyleContext* aContext,
                              nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mEmbellishData.flags = NS_MATHML_STRETCH_ALL_CHILDREN;

  return rv;
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::Reflow(nsIPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{

  nsresult rv = NS_OK;

  /////////////
  // Reflow children to stretch themselves

  ReflowChildren(1, aPresContext, aDesiredSize, aReflowState, aStatus);

  /////////////
  // Ask stretchy children to stretch themselves

  nsIRenderingContext& renderingContext = *aReflowState.rendContext;
  nsStretchMetrics containerSize(aDesiredSize);
  nsStretchDirection stretchDir = NS_STRETCH_DIRECTION_HORIZONTAL;

  StretchChildren(aPresContext, renderingContext, stretchDir, containerSize);

  /////////////
  // Place children now by re-adjusting the origins to align the baselines
  FinalizeReflow(1, aPresContext, renderingContext, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::Place(nsIPresContext*      aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               PRBool               aPlaceOrigin,
                               nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord count = 0;
  nsRect rect[3];
  nsIFrame* child[3];
  aDesiredSize.width = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame) && 3 > count) {
      child[count] = childFrame;
      childFrame->GetRect(rect[count]);
      if (aDesiredSize.width < rect[count].width) aDesiredSize.width = rect[count].width;
      count++;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  aDesiredSize.descent = rect[0].x;
  aDesiredSize.ascent = rect[0].y;
        
  aDesiredSize.ascent += rect[2].height;
  aDesiredSize.descent += rect[1].height;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  
  rect[0].x = (aDesiredSize.width - rect[0].width) / 2; // center w.r.t largest width
  rect[1].x = (aDesiredSize.width - rect[1].width) / 2;
  rect[2].x = (aDesiredSize.width - rect[2].width) / 2;
  rect[0].y = rect[2].height;
  rect[1].y = aDesiredSize.height - rect[1].height;
  rect[2].y = 0;

//ignore the leading (line spacing) that is attached to the text
  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  nsCOMPtr<nsIFontMetrics> fm;
  aPresContext->GetMetricsFor(font.mFont, getter_AddRefs(fm));
  nscoord leading;
  fm->GetLeading(leading);

  aDesiredSize.height -= 2*leading;
  aDesiredSize.ascent -= leading;
  aDesiredSize.descent -= leading;
  rect[0].y -= leading;
  rect[1].y -= 2*leading;
//

  if (aPlaceOrigin) {
    // child[0]->SetRect(aPresContext, rect[0]);
    // child[1]->SetRect(aPresContext, rect[1]);
    // child[2]->SetRect(aPresContext, rect[2]);
    nsHTMLReflowMetrics childSize(nsnull);
    for (PRInt32 i=0; i<count; i++) {
      childSize.width = rect[i].width;
      childSize.height = rect[i].height;
      FinishReflowChild(child[i], aPresContext, childSize, rect[i].x, rect[i].y, 0);
    }
  }

  // XXX Fix me!
  mBoundingMetrics.ascent  =  aDesiredSize.ascent;
  mBoundingMetrics.descent = -aDesiredSize.descent;
  mBoundingMetrics.width   =  aDesiredSize.width;

  return NS_OK;
}
