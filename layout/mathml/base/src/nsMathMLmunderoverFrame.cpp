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

#include "nsMathMLmunderoverFrame.h"
#include "nsMathMLmsubsupFrame.h"

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

  mEmbellishData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
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
  
  // see if the overscriptFrame is <mo> or an embellished operator
  if (overscriptFrame) {
    rv = overscriptFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&overscriptMathMLFrame);
    if (NS_SUCCEEDED(rv) && overscriptMathMLFrame) {
      overscriptMathMLFrame->GetEmbellishData(embellishData);
      // core of the overscriptFrame
      if (NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags) && embellishData.core) {
        rv = embellishData.core->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
        if (NS_SUCCEEDED(rv) && aMathMLFrame) {
          aMathMLFrame->GetEmbellishData(embellishData);
          // if we have the accent attribute, tell the core to behave as 
          // requested (otherwise leave the core with its default behavior)
          if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                          nsMathMLAtoms::accent_, value))
          {
            if (value.EqualsWithConversion("true")) embellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
            else if (value.EqualsWithConversion("false")) embellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENT;
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

  /*
     The TeXBook treats 'over' like a superscript, so p.141 or Rule 13a
     say it shouldn't be compressed. However, The TeXBook says
     that math accents and \overline change uncramped styles to their
     cramped counterparts.
  */
  if (overscriptMathMLFrame) 
  {
    PRInt32 increment = NS_MATHML_IS_ACCENTOVER(mPresentationData.flags)
                      ? 0 : 1;
    PRUint32 compress = NS_MATHML_IS_ACCENTOVER(mPresentationData.flags)
                      ? NS_MATHML_COMPRESSED : 0;
    overscriptMathMLFrame->UpdatePresentationData(increment,
      ~NS_MATHML_DISPLAYSTYLE | compress,
       NS_MATHML_DISPLAYSTYLE | compress);
    overscriptMathMLFrame->UpdatePresentationDataFromChildAt(0, -1, increment,
      ~NS_MATHML_DISPLAYSTYLE | compress,
       NS_MATHML_DISPLAYSTYLE | compress);
  }

  /*
     The TeXBook treats 'under' like a subscript, so p.141 or Rule 13a 
     say it should be compressed
  */
  if (underscriptMathMLFrame) {
    PRInt32 increment = NS_MATHML_IS_ACCENTUNDER(mPresentationData.flags)? 0 : 1;
    underscriptMathMLFrame->UpdatePresentationData(increment,
      ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
       NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);
    underscriptMathMLFrame->UpdatePresentationDataFromChildAt(0, -1, increment,
      ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
       NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);
  }

  // switch the style of the underscript and the overscript
  InsertScriptLevelStyleContext(aPresContext);

  return rv;
}

/*
The REC says:
*  If the base is an operator with movablelimits="true" (or an embellished
   operator whose <mo> element core has movablelimits="true"), and
   displaystyle="false", then underscript and overscript are drawn in
   a subscript and superscript position, respectively. In this case, 
   the accent and accentunder attributes are ignored. This is often
   used for limits on symbols such as &sum;.

i.e.,:
 if ( NS_MATHML_IS_MOVABLELIMITS(mPresentationData.flags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like subscript-superscript pair
 }
 else {
  // place like underscript-overscript pair
 }
*/


NS_IMETHODIMP
nsMathMLmunderoverFrame::Place(nsIPresContext*      aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               PRBool               aPlaceOrigin,
                               nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv = NS_OK;

  if ( NS_MATHML_IS_MOVABLELIMITS(mPresentationData.flags) &&
       !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    // place like sub-superscript pair
    return nsMathMLmsubsupFrame::PlaceSubSupScript(aPresContext,
                                                   aRenderingContext,
                                                   aPlaceOrigin,
                                                   aDesiredSize,
                                                   this);
  }

  ////////////////////////////////////
  // Get the children's desired sizes

  PRInt32 count = 0;
  nsBoundingMetrics bmBase, bmUnder, bmOver;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics underSize (nsnull);
  nsHTMLReflowMetrics overSize (nsnull);
  nsIFrame* baseFrame = nsnull;
  nsIFrame* overFrame = nsnull;
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
      else if (2 == count) {
        // over
        overFrame = childFrame;
        GetReflowAndBoundingMetricsFor(overFrame, overSize, bmOver);
      }
      count++;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  if ((3 != count) || !baseFrame || !underFrame || !overFrame) {
#ifdef NS_DEBUG
    printf("munderover: invalid markup\n");
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
  nscoord underDelta1 = 0; // gap between base and underscript
  nscoord underDelta2 = 0; // extra space beneath underscript

  if (!NS_MATHML_IS_ACCENTUNDER(mPresentationData.flags)) {
    // Rule 13a, App. G, TeXbook
    GetItalicCorrection (bmBase, italicCorrection);
    nscoord bigOpSpacing2, bigOpSpacing4, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      dummy, bigOpSpacing2, 
                      dummy, bigOpSpacing4, 
                      bigOpSpacing5);
    underDelta1 = PR_MAX(bigOpSpacing2, (bigOpSpacing4 - bmUnder.ascent));
    underDelta2 = bigOpSpacing5;
  }
  else {
    // No corresponding rule in TeXbook - we are on our own here
    // XXX tune the gap delta between base and underscript 

    // Should we use Rule 10 like \underline does?
    underDelta1 = ruleThickness;
    underDelta2 = ruleThickness;
  }
  // empty under?
  if (0 == (bmUnder.ascent + bmUnder.descent)) underDelta1 = 0;

  nscoord correction = 0;
  nscoord overDelta1 = 0; // gap between base and overscript
  nscoord overDelta2 = 0; // extra space above overscript

  if (!NS_MATHML_IS_ACCENTOVER(mPresentationData.flags)) {    
    // Rule 13a, App. G, TeXbook
    GetItalicCorrection (bmBase, italicCorrection);
    nscoord bigOpSpacing1, bigOpSpacing3, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      bigOpSpacing1, dummy, 
                      bigOpSpacing3, dummy, 
                      bigOpSpacing5);
    overDelta1 = PR_MAX(bigOpSpacing1, (bigOpSpacing3 - bmOver.descent));
    overDelta2 = bigOpSpacing5;

    // XXX This is not a TeX rule... 
    // delta1 (as computed abvove) can become really big when bmOver.descent is
    // negative,  e.g., if the content is &OverBar. In such case, we use the height
    if (bmOver.descent < 0)    
      overDelta1 = PR_MAX(bigOpSpacing1, (bigOpSpacing3 - (bmOver.ascent + bmOver.descent)));
  }
  else {
    // Rule 13, App. G, TeXbook
    GetSkewCorrectionFromChild (aPresContext, baseFrame, correction);
    overDelta1 = ruleThickness;
    if (bmBase.ascent < xHeight) { 
      overDelta1 += xHeight - bmBase.ascent;
    }
    overDelta2 = ruleThickness;
  }
  // empty over?
  if (0 == (bmOver.ascent + bmOver.descent)) overDelta1 = 0;

  mBoundingMetrics.ascent = 
    bmBase.ascent + overDelta1 + bmOver.ascent + bmOver.descent;
  mBoundingMetrics.descent = 
    bmBase.descent + underDelta1 + bmUnder.ascent + bmUnder.descent;
  mBoundingMetrics.width = 
    PR_MAX(bmBase.width/2,PR_MAX((bmUnder.width + italicCorrection/2)/2,(bmOver.width - correction/2)/2)) +
    PR_MAX(bmBase.width/2,PR_MAX((bmUnder.width - italicCorrection/2)/2,(bmOver.width + correction/2)/2));

  nscoord dxBase = (mBoundingMetrics.width - bmBase.width) / 2;
  nscoord dxOver = (mBoundingMetrics.width - (bmOver.width - correction/2)) / 2;
  nscoord dxUnder = (mBoundingMetrics.width - (bmUnder.width + italicCorrection/2)) / 2;

  mBoundingMetrics.leftBearing = 
    PR_MIN(dxBase + bmBase.leftBearing, dxUnder + bmUnder.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(dxBase + bmBase.rightBearing, dxUnder + bmUnder.rightBearing);
  mBoundingMetrics.leftBearing = 
    PR_MIN(mBoundingMetrics.leftBearing, dxOver + bmOver.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(mBoundingMetrics.rightBearing, dxOver + bmOver.rightBearing);

  aDesiredSize.ascent = 
    PR_MAX(mBoundingMetrics.ascent + overDelta2,
           overSize.ascent + bmOver.descent + overDelta1 + bmBase.ascent);
  aDesiredSize.descent = 
    PR_MAX(mBoundingMetrics.descent + underDelta2,
           bmBase.descent + underDelta1 + bmUnder.ascent + underSize.descent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dy;
    // place overscript
    dy = aDesiredSize.ascent - mBoundingMetrics.ascent + bmOver.ascent - overSize.ascent;
    FinishReflowChild (overFrame, aPresContext, overSize, dxOver, dy, 0);
    // place base
    dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, baseSize, dxBase, dy, 0);
    // place underscript
    dy = aDesiredSize.ascent + mBoundingMetrics.descent - bmUnder.descent - underSize.ascent;
    FinishReflowChild (underFrame, aPresContext, underSize, dxUnder, dy, 0);
  }
  return NS_OK;
}
