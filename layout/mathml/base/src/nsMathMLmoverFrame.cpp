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

#include "nsMathMLmoverFrame.h"
#include "nsMathMLmsupFrame.h"

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

  mEmbellishData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
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
//XXX should sync the presentation data be at this spot, after the if?
      }
    }
  }

  /* The REC says:
     Within overscript, <mover> always sets displaystyle to "false", 
     but increments scriptlevel by 1 only when accent is "false".
  */
  /*
     The TeXBook treats 'over' like a superscript, so p.141 or Rule 13a
     say it shouldn't be compressed. However, The TeXBook says
     that math accents and \overline change uncramped styles to their
     cramped counterparts.
  */
  if (overscriptMathMLFrame) {
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

  // switch the style of the overscript
  InsertScriptLevelStyleContext(aPresContext);

  return rv;
}

/*
The REC says:
* If the base is an operator with movablelimits="true" (or an embellished
  operator whose <mo> element core has movablelimits="true"), and
  displaystyle="false", then overscript is drawn in a superscript
  position. In this case, the accent attribute is ignored. This is
  often used for limits on symbols such as &sum;. 

i.e.:
 if ( NS_MATHML_IS_MOVABLELIMITS(mPresentationData.flags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like superscript
 }
 else {
  // place like overscript 
 }
*/

NS_IMETHODIMP
nsMathMLmoverFrame::Place(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          PRBool               aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{ 
  nsresult rv = NS_OK;
  
  if ( NS_MATHML_IS_MOVABLELIMITS(mPresentationData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    // place like superscript
    return nsMathMLmsupFrame::PlaceSuperScript(aPresContext,
                                               aRenderingContext,
                                               aPlaceOrigin,
                                               aDesiredSize,
                                               this);
  }

  ////////////////////////////////////
  // Get the children's desired sizes

  PRInt32 count = 0;
  nsBoundingMetrics bmBase, bmOver;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics overSize (nsnull);
  nsIFrame* baseFrame = nsnull;
  nsIFrame* overFrame = nsnull;

  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      if (0 == count) {
        // base 
        baseFrame = childFrame;
        GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
      }
      else if (1 == count) {
        // over
        overFrame = childFrame;
        GetReflowAndBoundingMetricsFor(overFrame, overSize, bmOver);
      }
      count++;
    }    
    childFrame->GetNextSibling(&childFrame);
  }
  if ((2 != count) || !baseFrame || !overFrame) {
#ifdef NS_DEBUG
    printf("mover: invalid markup\n");
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
  // whether we want an accented overscript or not

  nscoord correction = 0;
  nscoord delta1 = 0; // gap between base and overscript
  nscoord delta2 = 0; // extra space above overscript
  if (!NS_MATHML_IS_ACCENTOVER(mPresentationData.flags)) {    
    // Rule 13a, App. G, TeXbook
    GetItalicCorrection (bmBase, correction);
    nscoord bigOpSpacing1, bigOpSpacing3, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      bigOpSpacing1, dummy, 
                      bigOpSpacing3, dummy, 
                      bigOpSpacing5);
    delta1 = PR_MAX(bigOpSpacing1, (bigOpSpacing3 - bmOver.descent));
    delta2 = bigOpSpacing5;

    // XXX This is not a TeX rule... 
    // delta1 (as computed abvove) can become really big when bmOver.descent is
    // negative,  e.g., if the content is &OverBar. In such case, we use the height
    if (bmOver.descent < 0)    
      delta1 = PR_MAX(bigOpSpacing1, (bigOpSpacing3 - (bmOver.ascent + bmOver.descent)));
  }
  else {
    // Rule 12, App. G, TeXbook
    GetSkewCorrectionFromChild (aPresContext, baseFrame, correction);
    // We are going to modify this rule to make it more general.
    // The idea behind Rule 12 in the TeXBook is to keep the accent
    // as close to the base as possible, while ensuring that the
    // distance between the *baseline* of the accent char and 
    // the *baseline* of the base is atleast x-height. 
    // The idea is that for normal use, we would like all the accents
    // on a line to line up atleast x-height above the baseline 
    // if possible. 
    // When the ascent of the base is >= x-height, 
    // the baseline of the accent char is placed just above the base
    // (specifically, the baseline of the accent char is placed 
    // above the baseline of the base by the ascent of the base).
    // For ease of implementation, 
    // this assumes that the font-designer designs accents 
    // in such a way that the bottom of the accent is atleast x-height
    // above its baseline, otherwise there will be collisions
    // with the base. Also there should be proper padding between
    // the bottom of the accent char and its baseline.
    // The above rule may not be obvious from a first
    // reading of rule 12 in the TeXBook !!!
    // The mathml <mover> tag can use accent chars that
    // do not follow this convention. So we modify TeX's rule 
    // so that TeX's rule gets subsumed for accents that follow 
    // TeX's convention,
    // while also allowing accents that do not follow the convention :
    // we try to keep the *bottom* of the accent char atleast x-height 
    // from the baseline of the base char. we also slap on an extra
    // padding between the accent and base chars.
    delta1 = ruleThickness; // we have atleast the padding
    if (bmBase.ascent < xHeight) { 
      // also ensure atleast x-height above the baseline of the base
      delta1 += xHeight - bmBase.ascent;
    }
    delta2 = ruleThickness;
  }
  // empty over?
  if (0 == (bmOver.ascent + bmOver.descent)) delta1 = 0;

  mBoundingMetrics.ascent = 
    bmOver.ascent + bmOver.descent + delta1 + bmBase.ascent;

  mBoundingMetrics.descent = bmBase.descent;
  if (NS_MATHML_IS_ACCENTOVER(mPresentationData.flags)) {
    mBoundingMetrics.width = PR_MAX(bmBase.width, bmOver.width); 
  }
  else {
    mBoundingMetrics.width = 
      PR_MAX(bmBase.width/2,(bmOver.width + correction/2)/2) +
      PR_MAX(bmBase.width/2,(bmOver.width - correction/2)/2);
  }

  aDesiredSize.descent = baseSize.descent;
  aDesiredSize.ascent = 
    PR_MAX(mBoundingMetrics.ascent + delta2,
           overSize.ascent + bmOver.descent + delta1 + bmBase.ascent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;

  nscoord dxBase, dyBase;
  nscoord dxOver, dyOver;
  
  dxBase = (mBoundingMetrics.width - bmBase.width) / 2;
  dyBase = aDesiredSize.ascent - baseSize.ascent;

  if (NS_MATHML_IS_ACCENTOVER(mPresentationData.flags)) {
    dxOver = correction + (mBoundingMetrics.width - bmOver.width)/2;
  }
  else {
    dxOver = correction/2 + (mBoundingMetrics.width - bmOver.width)/2;
  }

  dyOver = aDesiredSize.ascent - 
    mBoundingMetrics.ascent + bmOver.ascent - overSize.ascent;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  mBoundingMetrics.leftBearing = 
    PR_MIN(dxBase + bmBase.leftBearing, dxOver + bmOver.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(dxBase + bmBase.rightBearing, dxOver + bmOver.rightBearing);

  if (aPlaceOrigin) {
    // place base
    FinishReflowChild (baseFrame, aPresContext, baseSize, dxBase, dyBase, 0);
    // place overscript
    FinishReflowChild (overFrame, aPresContext, overSize, dxOver, dyOver, 0);
  }
  return NS_OK;
}































