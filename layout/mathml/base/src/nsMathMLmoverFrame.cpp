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

  mEmbellishData.flags = NS_MATHML_STRETCH_ALL_CHILDREN;

  return rv;
}

NS_IMETHODIMP
nsMathMLmoverFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  nsresult rv;
  rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // check whether or not this is an embellished operator
  EmbellishOperator();

  // set our accent flag
  
  /* The REC says:
  The default value of accent is false, unless overscript
  is an <mo> element or an embellished operator. If overscript is
  an <mo> element, the value of its accent attribute is used as
  the default value of accent. If overscript is an embellished 
  operator, the accent attribute of the <mo> element at its
  core is used as the default value. As with all attributes, an
  explicitly given value overrides the default.

XXX The winner is the outermost in conflicting settings like these:
<mover accent='true'>
  <mi>...</mi>
  <mo accent='false'> ... </mo>
</mover>
   */

  PRInt32 count = 0;
  nsIFrame* baseFrame = nsnull;
  nsIFrame* overscriptFrame = nsnull;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      count++;
      if (1 == count) baseFrame = childFrame;
      if (2 == count) { overscriptFrame = childFrame; break; }
    }
    childFrame->GetNextSibling(&childFrame);
  }

  nsIMathMLFrame* overscriptMathMLFrame = nsnull;
  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsEmbellishData embellishData;
  nsAutoString value;

  mPresentationData.flags &= ~NS_MATHML_MOVABLELIMITS; // default is false
  mPresentationData.flags &= ~NS_MATHML_ACCENTOVER; // default of accent is false

  // see if the baseFrame has movablelimits="true" or if it is an
  // embellished operator whose movablelimits attribute is set to true
  if (baseFrame && NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {
    nsCOMPtr<nsIContent> baseContent;
    baseFrame->GetContent(getter_AddRefs(baseContent));
    if (NS_CONTENT_ATTR_HAS_VALUE == baseContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::movablelimits_, value)) {
      if (value == "true") {
        mPresentationData.flags |= NS_MATHML_MOVABLELIMITS;
      }
    }
    else { // no attribute, get the value from the core
      rv = mEmbellishData.core->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
      if (NS_SUCCEEDED(rv) && aMathMLFrame) {
        aMathMLFrame->GetEmbellishData(embellishData);
        if (NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(embellishData.flags)) {
          mPresentationData.flags |= NS_MATHML_MOVABLELIMITS;
        }
      }
    }
  }

  // see if the overscriptFrame is <mo> or an embellished operator
  if (overscriptFrame) {
    rv = overscriptFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&overscriptMathMLFrame);
    if (NS_SUCCEEDED(rv) && overscriptMathMLFrame) {
      overscriptMathMLFrame->GetEmbellishData(embellishData);
      // core of the overscriptFrame
      if (NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags) && embellishData.core) {
        rv = embellishData.core->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
        if (NS_SUCCEEDED(rv) && aMathMLFrame) {
          aMathMLFrame->GetEmbellishData(embellishData);
          // if we have the accent attribute, tell the core to behave as 
          // requested (otherwise leave the core with its default behavior)
          if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                          nsMathMLAtoms::accent_, value))
          {
            if (value == "true") embellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
            else if (value == "false") embellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENT;
            aMathMLFrame->SetEmbellishData(embellishData);
          }

          // sync the presentation data: record whether we have an accent
          if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags))
            mPresentationData.flags |= NS_MATHML_ACCENTOVER;
        }
      }
    }
  }

  //The REC says:
  /*
  Within overscript, <mover> always sets displaystyle to "false", 
  but increments scriptlevel by 1 only when accent is "false".
  */

  PRInt32 incrementScriptLevel;

  if (overscriptMathMLFrame) {
    incrementScriptLevel = NS_MATHML_IS_ACCENTOVER(mPresentationData.flags)? 0 : 1;
    overscriptMathMLFrame->UpdatePresentationData(incrementScriptLevel, PR_FALSE);
    overscriptMathMLFrame->UpdatePresentationDataFromChildAt(0, incrementScriptLevel, PR_FALSE);
  }

  // switch the style of the overscript
  InsertScriptLevelStyleContext(aPresContext);

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

/*
The REC says:
* If the base is an operator with movablelimits="true" (or an embellished
  operator whose <mo> element core has movablelimits="true"), and
  displaystyle="false", then overscript is drawn in a superscript
  position. In this case, the accent attribute is ignored. This is
  often used for limits on symbols such as &sum;. 

TODO:
 if ( NS_MATHML_IS_MOVABLELIMITS(mPresentationData.flags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like superscript
 }
 else {
  // place like accent 
 }
*/

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
            nsEmbellishData embellishData;
            aMathMLFrame->GetEmbellishData(embellishData);
            embellishData.core->GetRect(rect);
            width[count] = rect.width + embellishData.leftSpace + embellishData.rightSpace;
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

  // XXX Fix me!
  mBoundingMetrics.ascent  = aDesiredSize.ascent;
  mBoundingMetrics.descent = aDesiredSize.descent;
  mBoundingMetrics.width   = aDesiredSize.width;
  return NS_OK;
}
