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

#include "nsMathMLmoverFrame.h"

//
// <mover> -- attach an overscript to a base - implementation
//

nsresult
NS_NewMathMLmoverFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmoverFrame* it = new (aPresShell) nsMathMLmoverFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmoverFrame::nsMathMLmoverFrame()
{
}

nsMathMLmoverFrame::~nsMathMLmoverFrame()
{
}

NS_IMETHODIMP
nsMathMLmoverFrame::Init(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mEmbellish.flags = NS_MATHML_STRETCH_ALL_CHILDREN;

  return rv;
}

NS_IMETHODIMP
nsMathMLmoverFrame::Reflow(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{

  nsresult rv = NS_OK;

  /////////////
  // Reflow children
  ReflowChildren(1, aPresContext, aDesiredSize, aReflowState, aStatus);

  nsIRenderingContext& renderingContext = *aReflowState.rendContext;

  /////////////
  // Ask stretchy children to stretch themselves
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
nsMathMLmoverFrame::Place(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          PRBool               aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{ 
  nscoord count = 0;
  nsRect rect[2];
  nsIFrame* child[2];
  nscoord width[2];
  nscoord maxWidth = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame) && 2 > count) {
      childFrame->GetRect(rect[count]);
      child[count] = childFrame;

      if (aPlaceOrigin) {
        width[count] = rect[count].width;
        if (IsEmbellishOperator(childFrame)) {
          // treated as if embellishments were not there...
          nsIMathMLFrame* aMathMLFrame = nsnull;
          nsresult rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
          NS_ASSERTION(NS_SUCCEEDED(rv) && aMathMLFrame, "Mystery!");
          if (NS_SUCCEEDED(rv) && aMathMLFrame) {
            nsRect rect;
            nsEmbellishState embellishState;
            aMathMLFrame->GetEmbellishState(embellishState);
            embellishState.core->GetRect(rect);
            width[count] = rect.width + embellishState.leftSpace + embellishState.rightSpace;
          }
        }
        maxWidth = PR_MAX(maxWidth, width[count]); // clean up and use "if" later
      }

      count++;
    }
    childFrame->GetNextSibling(&childFrame);
  }

  aDesiredSize.ascent = rect[0].y;
  aDesiredSize.descent = rect[0].x;           
  aDesiredSize.width = PR_MAX(rect[0].width, rect[1].width);

  aDesiredSize.ascent += rect[1].height; 
  aDesiredSize.width = PR_MAX(rect[0].width, rect[1].width);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  if (aPlaceOrigin) {
    rect[0].x = (maxWidth - width[0]) / 2; // center w.r.t largest width
    rect[1].x = (maxWidth - width[1]) / 2;
    rect[1].y = 0;
    rect[0].y = rect[1].height;
  }
//ignore the leading (line spacing) that is attached to the text
  nscoord leading;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  aPresContext->GetMetricsFor(aFont->mFont, getter_AddRefs(fm));
  fm->GetLeading(leading);
  
  aDesiredSize.height -= leading;
  aDesiredSize.ascent -= leading;
  rect[0].y -= leading;
//
  if (aPlaceOrigin) {
    // child[0]->SetRect(aPresContext, rect[0]);
    // child[1]->SetRect(aPresContext, rect[1]);
    nsHTMLReflowMetrics childSize(nsnull);
    for (PRInt32 i=0; i<count; i++) {
      childSize.width = rect[i].width;
      childSize.height = rect[i].height;
      FinishReflowChild(child[i], aPresContext, childSize, rect[i].x, rect[i].y, 0);
    }
  }
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  return NS_OK;
}
