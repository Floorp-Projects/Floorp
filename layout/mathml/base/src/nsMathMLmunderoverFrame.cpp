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
nsMathMLmunderoverFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                             nsIAtom*        aListName,
                                             nsIFrame*       aChildList)
{
  nsresult rv;
  rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // check whether or not this is an embellished operator
  EmbellishOperator();

  // set our accent and accentunder flags
  /* 
  The REC says:

  The accent and accentunder attributes have the same effect as
  the attributes with the same names on <mover>  and <munder>, 
  respectively. Their default values are also computed in the 
  same manner as described for those elements, with the default
  value of accent depending on overscript and the default value
  of accentunder depending on underscript.
  */

  // get our overscript and underscript frames
  PRInt32 count = 0;
  nsIFrame* baseFrame = nsnull;
  nsIFrame* underscriptFrame = nsnull;
  nsIFrame* overscriptFrame = nsnull;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      count++;
      if (1 == count) baseFrame = childFrame;
      if (2 == count) underscriptFrame = childFrame;
      if (3 == count) { overscriptFrame = childFrame; break; }
    }
    childFrame->GetNextSibling(&childFrame);
  }

  nsIMathMLFrame* underscriptMathMLFrame = nsnull;
  nsIMathMLFrame* overscriptMathMLFrame = nsnull;
  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsEmbellishData embellishData;
  nsAutoString value;

  mPresentationData.flags &= ~NS_MATHML_MOVABLELIMITS; // default is false
  mPresentationData.flags &= ~NS_MATHML_ACCENTUNDER; // default of accentunder is false
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

  // see if the underscriptFrame is <mo> or an embellished operator
  if (underscriptFrame) {
    rv = underscriptFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&underscriptMathMLFrame);
    if (NS_SUCCEEDED(rv) && underscriptMathMLFrame) {
      underscriptMathMLFrame->GetEmbellishData(embellishData);
      // core of the underscriptFrame
      if (NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags) && embellishData.core) {
        rv = embellishData.core->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
        if (NS_SUCCEEDED(rv) && aMathMLFrame) {
          aMathMLFrame->GetEmbellishData(embellishData);
          // if we have the accentunder attribute, tell the core to behave as 
          // requested (otherwise leave the core with its default behavior)
          if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                          nsMathMLAtoms::accentunder_, value))
          {
            if (value == "true") embellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
            else if (value == "false") embellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENT;
            aMathMLFrame->SetEmbellishData(embellishData);
          }

          // sync the presentation data: record whether we have an accentunder
          if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags))
            mPresentationData.flags |= NS_MATHML_ACCENTUNDER;
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
  Within underscript, <munderover> always sets displaystyle to "false",
  but increments scriptlevel by 1 only when accentunder is "false". 

  Within overscript, <munderover> always sets displaystyle to "false", 
  but increments scriptlevel by 1 only when accent is "false".
  */

  PRInt32 incrementScriptLevel;

  if (underscriptMathMLFrame) {
    incrementScriptLevel = NS_MATHML_IS_ACCENTUNDER(mPresentationData.flags)? 0 : 1;
    underscriptMathMLFrame->UpdatePresentationData(incrementScriptLevel, PR_FALSE);
    underscriptMathMLFrame->UpdatePresentationDataFromChildAt(0, incrementScriptLevel, PR_FALSE);
  }
  
  if (overscriptMathMLFrame) 
  {
    incrementScriptLevel = NS_MATHML_IS_ACCENTOVER(mPresentationData.flags)? 0 : 1;
    overscriptMathMLFrame->UpdatePresentationData(incrementScriptLevel, PR_FALSE);
    overscriptMathMLFrame->UpdatePresentationDataFromChildAt(0, incrementScriptLevel, PR_FALSE);
  }
  
  // switch the style of the underscript and the overscript
  InsertScriptLevelStyleContext(aPresContext);

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
  // Reflow children

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

/*
The REC says:
*  If the base is an operator with movablelimits="true" (or an embellished
   operator whose <mo> element core has movablelimits="true"), and
   displaystyle="false", then underscript and overscript are drawn in
   a subscript and superscript position, respectively. In this case, 
   the accent and accentunder attributes are ignored. This is often
   used for limits on symbols such as &sum;.

TODO:
 if ( NS_MATHML_IS_MOVABLELIMITS(mPresentationData.flags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like subscript-superscript pair
 }
 else {
  // place like accentunder-accent pair
 }
*/

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
  mBoundingMetrics.ascent  = aDesiredSize.ascent;
  mBoundingMetrics.descent = aDesiredSize.descent;
  mBoundingMetrics.width   = aDesiredSize.width;

  return NS_OK;
}
