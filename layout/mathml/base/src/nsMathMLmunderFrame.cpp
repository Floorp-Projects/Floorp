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
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsMathMLmunderFrame.h"

//
// <munder> -- attach an underscript to a base - implementation
//

nsresult
NS_NewMathMLmunderFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmunderFrame* it = new (aPresShell) nsMathMLmunderFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmunderFrame::nsMathMLmunderFrame()
{
}

nsMathMLmunderFrame::~nsMathMLmunderFrame()
{
}

NS_IMETHODIMP
nsMathMLmunderFrame::Init(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mEmbellishData.flags = NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}

NS_IMETHODIMP
nsMathMLmunderFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                         nsIAtom*        aListName,
                                         nsIFrame*       aChildList)
{
  nsresult rv;
  rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // check whether or not this is an embellished operator
  EmbellishOperator();

  // set our accentunder flag
  /* The REC says:
  The default value of accentunder is false, unless underscript
  is an <mo> element or an embellished operator.  If underscript is 
  an <mo> element, the value of its accent attribute is used as the
  default value of accentunder. If underscript is an embellished
  operator, the accent attribute of the <mo> element at its
  core is used as the default value. As with all attributes, an
  explicitly given value overrides the default.

XXX The winner is the outermost setting in conflicting settings like these:
<munder accent='true'>
  <mi>...</mi>
  <mo accent='false'> ... </mo>
</munder>
   */

  PRInt32 count = 0;
  nsIFrame* baseFrame = nsnull;
  nsIFrame* underscriptFrame = nsnull;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      count++;
      if (1 == count) baseFrame = childFrame;
      if (2 == count) { underscriptFrame = childFrame; break; }
    }
    childFrame->GetNextSibling(&childFrame);
  }

  nsIMathMLFrame* underscriptMathMLFrame = nsnull;
  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsEmbellishData embellishData;
  nsAutoString value;

  mPresentationData.flags &= ~NS_MATHML_MOVABLELIMITS; // default is false
  mPresentationData.flags &= ~NS_MATHML_ACCENTUNDER; // default of accentunder is false

  // see if the baseFrame has movablelimits="true" or if it is an
  // embellished operator whose movablelimits attribute is set to true
  if (baseFrame && NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {
    nsCOMPtr<nsIContent> baseContent;
    baseFrame->GetContent(getter_AddRefs(baseContent));
    if (NS_CONTENT_ATTR_HAS_VALUE == baseContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::movablelimits_, value)) {
      if (value.EqualsWithConversion("true")) {
        mPresentationData.flags |= NS_MATHML_MOVABLELIMITS;
      }
    }
    else { // no attribute, get the value from the core
      rv = mEmbellishData.core->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
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
    rv = underscriptFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&underscriptMathMLFrame);
    if (NS_SUCCEEDED(rv) && underscriptMathMLFrame) {
      underscriptMathMLFrame->GetEmbellishData(embellishData);
      // core of the underscriptFrame
      if (NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags) && embellishData.core) {
        rv = embellishData.core->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
        if (NS_SUCCEEDED(rv) && aMathMLFrame) {
          aMathMLFrame->GetEmbellishData(embellishData);
          // if we have the accentunder attribute, tell the core to behave as 
          // requested (otherwise leave the core with its default behavior)
          if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                          nsMathMLAtoms::accentunder_, value))
          {
            if (value.EqualsWithConversion("true")) embellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
            else if (value.EqualsWithConversion("false")) embellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENT;
            aMathMLFrame->SetEmbellishData(embellishData);
          }

          // sync the presentation data: record whether we have an accentunder
          if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags))
            mPresentationData.flags |= NS_MATHML_ACCENTUNDER;
        }
      }
    }
  }

  //The REC says:
  /*
  Within underscript, <munder> always sets displaystyle to "false", 
  but increments scriptlevel by 1 only when accentunder is "false".
  */

  PRInt32 incrementScriptLevel;

  if (underscriptMathMLFrame) {
    incrementScriptLevel = NS_MATHML_IS_ACCENTUNDER(mPresentationData.flags)? 0 : 1;
    underscriptMathMLFrame->UpdatePresentationData(incrementScriptLevel, PR_FALSE, PR_FALSE);
    underscriptMathMLFrame->UpdatePresentationDataFromChildAt(0, incrementScriptLevel, PR_FALSE, PR_FALSE);
  }

  // switch the style of the underscript
  InsertScriptLevelStyleContext(aPresContext);

  return rv;
}

/*
The REC says:
* If the base is an operator with movablelimits="true" (or
  an embellished operator whose <mo> element core has
  movablelimits="true"), and displaystyle="false", then
  underscript is drawn in a subscript position. In this case,
  the accentunder attribute is ignored. This is often used
  for limits on symbols such as &sum;. 

TODO:
 if ( NS_MATHML_IS_MOVABLELIMITS(mPresentationData.flags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like subscript
 }
 else {
  // place like accentunder 
 }
*/

NS_IMETHODIMP
nsMathMLmunderFrame::Place(nsIPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           PRBool               aPlaceOrigin,
                           nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv = NS_OK;

  ////////////////////////////////////
  // Get the children's desired sizes

  PRInt32 count = 0;
  nsBoundingMetrics bmBase, bmUnder;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics underSize (nsnull);
  nsIFrame* baseFrame = nsnull;
  nsIFrame* underFrame = nsnull;

  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      if (0 == count) {
        // base 
        baseFrame = childFrame;
        GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
      }
      else if (1 == count) {
        // under
        underFrame = childFrame;
        GetReflowAndBoundingMetricsFor(underFrame, underSize, bmUnder);
      }
      count++;
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  if ((2 != count) || !baseFrame || !underFrame) {
#ifdef NS_DEBUG
    printf("munder: invalid markup");
#endif
    // report an error, encourage people to get their markups in order
    return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
  }

  ////////////////////
  // Place Children

  const nsStyleFont* font =
    (const nsStyleFont*) mStyleContext->GetStyleData (eStyleStruct_Font);
  aRenderingContext.SetFont(font->mFont);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord xHeight = 0;
  fm->GetXHeight (xHeight);

  nscoord ruleThickness;
  GetRuleThickness (aRenderingContext, fm, ruleThickness);

  // there are 2 different types of placement depending on 
  // whether we want an accented under or not

  nscoord italicCorrection = 0;
  nscoord delta1 = 0; // gap between base and underscript
  nscoord delta2 = 0; // extra space beneath underscript
  if (!NS_MATHML_IS_ACCENTUNDER(mPresentationData.flags)) {    
    // Rule 13a, App. G, TeXbook
//    GetItalicCorrectionFromChild (baseFrame, italicCorrection);
    nscoord bigOpSpacing2, bigOpSpacing4, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      dummy, bigOpSpacing2, 
                      dummy, bigOpSpacing4, 
                      bigOpSpacing5);
    delta1 = PR_MAX(bigOpSpacing2, (bigOpSpacing4 - bmUnder.ascent));
    delta2 = bigOpSpacing5;
  }
  else {
    // No corresponding rule in TeXbook - we are on our own here
    // XXX tune the gap delta between base and underscript 

    // Should we use Rule 10 like \underline does?
    delta1 = ruleThickness;
    delta2 = ruleThickness;
  }
  // empty under?
  if (0 == (bmUnder.ascent + bmUnder.descent)) delta1 = 0;

  mBoundingMetrics.ascent = bmBase.ascent;
  mBoundingMetrics.descent = 
    bmBase.descent + delta1 + bmUnder.ascent + bmUnder.descent;
  mBoundingMetrics.width = 
    PR_MAX(bmBase.width/2,(bmUnder.width + italicCorrection/2)/2) +
    PR_MAX(bmBase.width/2,(bmUnder.width - italicCorrection/2)/2);

  nscoord dxBase = (mBoundingMetrics.width - bmBase.width) / 2;
  nscoord dxUnder = (mBoundingMetrics.width - (bmUnder.width + italicCorrection/2)) / 2;
  
  aDesiredSize.ascent = baseSize.ascent;
  aDesiredSize.descent = mBoundingMetrics.descent + delta2;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  mBoundingMetrics.leftBearing = 
    PR_MIN(dxBase + bmBase.leftBearing, dxUnder + bmUnder.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(dxBase + bmBase.rightBearing, dxUnder + bmUnder.rightBearing);

  if (aPlaceOrigin) {
    nscoord dy = 0;
    // place base
    dy = 0;
    FinishReflowChild(baseFrame, aPresContext, baseSize, dxBase, dy, 0);
    // place underscript
    dy = aDesiredSize.ascent + mBoundingMetrics.descent - bmUnder.descent - underSize.ascent;
    FinishReflowChild(underFrame, aPresContext, underSize, dxUnder, dy, 0);
  }

  return NS_OK;
}

