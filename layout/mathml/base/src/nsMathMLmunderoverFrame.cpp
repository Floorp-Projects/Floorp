/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLmunderoverFrame.h"
#include "nsMathMLmsubsupFrame.h"

//
// <munderover> -- attach an underscript-overscript pair to a base - implementation
//

nsIFrame*
NS_NewMathMLmunderoverFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmunderoverFrame(aContext);
}

nsMathMLmunderoverFrame::~nsMathMLmunderoverFrame()
{
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                          nsIAtom*        aAttribute,
                                          PRInt32         aModType)
{
  if (nsGkAtoms::accent_ == aAttribute ||
      nsGkAtoms::accentunder_ == aAttribute) {
    // When we have automatic data to update within ourselves, we ask our
    // parent to re-layout its children
    return ReLayoutChildren(mParent);
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::UpdatePresentationData(PRInt32         aScriptLevelIncrement,
                                                PRUint32        aFlagsValues,
                                                PRUint32        aFlagsToUpdate)
{
  nsMathMLContainerFrame::UpdatePresentationData(aScriptLevelIncrement,
    aFlagsValues, aFlagsToUpdate);
  // disable the stretch-all flag if we are going to act like a subscript-superscript pair
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }
  else {
    mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                                           PRInt32         aLastIndex,
                                                           PRInt32         aScriptLevelIncrement,
                                                           PRUint32        aFlagsValues,
                                                           PRUint32        aFlagsToUpdate)
{
  // munderover is special... The REC says:
  // Within underscript, <munder> always sets displaystyle to "false", 
  // but increments scriptlevel by 1 only when accentunder is "false".
  // Within underscript, <munderover> always sets displaystyle to "false",
  // but increments scriptlevel by 1 only when accentunder is "false". 
  // This means that
  // 1. don't allow displaystyle to change in the underscript & overscript
  // 2a if the value of the accent is changed, we need to recompute the
  //    scriptlevel of the underscript. The problem is that the accent
  //    can change in the <mo> deep down the embellished hierarchy
  // 2b if the value of the accent is changed, we need to recompute the
  //    scriptlevel of the overscript. The problem is that the accent
  //    can change in the <mo> deep down the embellished hierarchy

  // Do #1 here, prevent displaystyle to be changed in the underscript & overscript
  PRInt32 index = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if ((index >= aFirstIndex) &&
        ((aLastIndex <= 0) || ((aLastIndex > 0) && (index <= aLastIndex)))) {
      if (index > 0) {
        // disable the flag
        aFlagsToUpdate &= ~NS_MATHML_DISPLAYSTYLE;
        aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
      }
      PropagatePresentationDataFor(childFrame,
        aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
    }
    index++;
    childFrame = childFrame->GetNextSibling();
  }
  return NS_OK;

  // For #2, changing the accent attribute will trigger a re-build of
  // all automatic data in the embellished hierarchy
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::TransmitAutomaticData()
{
  // At this stage, all our children are in sync and we can fully
  // resolve our own mEmbellishData struct
  //---------------------------------------------------------------------

  /* 
  The REC says:

  The accent and accentunder attributes have the same effect as
  the attributes with the same names on <mover>  and <munder>, 
  respectively. Their default values are also computed in the 
  same manner as described for those elements, with the default
  value of accent depending on overscript and the default value
  of accentunder depending on underscript.
  */

  nsIFrame* overscriptFrame = nsnull;
  nsIFrame* underscriptFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (baseFrame)
    underscriptFrame = baseFrame->GetNextSibling();
  if (underscriptFrame)
    overscriptFrame = underscriptFrame->GetNextSibling();

  // if our base is an embellished operator, let its state bubble to us (in particular,
  // this is where we get the flag for NS_MATHML_EMBELLISH_MOVABLELIMITS). Our flags
  // are reset to the default values of false if the base frame isn't embellished.
  mPresentationData.baseFrame = baseFrame;
  GetEmbellishDataFrom(baseFrame, mEmbellishData);

  // The default value of accentunder is false, unless the underscript is embellished
  // and its core <mo> is an accent
  nsEmbellishData embellishData;
  GetEmbellishDataFrom(underscriptFrame, embellishData);
  if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags))
    mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER;
  else
    mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER;

  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_true, &nsGkAtoms::_false, nsnull};

  // if we have an accentunder attribute, it overrides what the underscript said
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::accentunder_,
                                    strings, eCaseMatters)) {
    case 0: mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER; break;
    case 1: mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER; break;
  }

  // The default value of accent is false, unless the overscript is embellished
  // and its core <mo> is an accent
  GetEmbellishDataFrom(overscriptFrame, embellishData);
  if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags))
    mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTOVER;
  else
    mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTOVER;

  // if we have an accent attribute, it overrides what the overscript said
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::accent_,
                                    strings, eCaseMatters)) {
    case 0: mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTOVER; break;
    case 1: mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTOVER; break;
  }

  // disable the stretch-all flag if we are going to act like a superscript
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags))
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  // Now transmit any change that we want to our children so that they
  // can update their mPresentationData structs
  //---------------------------------------------------------------------

  /* The REC says:
     Within underscript, <munderover> always sets displaystyle to "false",
     but increments scriptlevel by 1 only when accentunder is "false". 

     Within overscript, <munderover> always sets displaystyle to "false", 
     but increments scriptlevel by 1 only when accent is "false".
 
     The TeXBook treats 'over' like a superscript, so p.141 or Rule 13a
     say it shouldn't be compressed. However, The TeXBook says
     that math accents and \overline change uncramped styles to their
     cramped counterparts.
  */
  PRInt32 increment = NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)
    ? 0 : 1;
  PRUint32 compress = NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)
    ? NS_MATHML_COMPRESSED : 0;
  PropagatePresentationDataFor(overscriptFrame, increment,
    ~NS_MATHML_DISPLAYSTYLE | compress,
     NS_MATHML_DISPLAYSTYLE | compress);

  /*
     The TeXBook treats 'under' like a subscript, so p.141 or Rule 13a 
     say it should be compressed
  */
  increment = NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)
    ? 0 : 1;
  PropagatePresentationDataFor(underscriptFrame, increment,
    ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
     NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);

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

i.e.,:
 if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishDataflags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like subscript-superscript pair
 }
 else {
  // place like underscript-overscript pair
 }
*/

NS_IMETHODIMP
nsMathMLmunderoverFrame::Place(nsIRenderingContext& aRenderingContext,
                               PRBool               aPlaceOrigin,
                               nsHTMLReflowMetrics& aDesiredSize)
{
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    // place like sub-superscript pair
    return nsMathMLmsubsupFrame::PlaceSubSupScript(GetPresContext(),
                                                   aRenderingContext,
                                                   aPlaceOrigin,
                                                   aDesiredSize,
                                                   this, 0, 0, GetPresContext()->PointsToAppUnits(0.5f));
  }

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmUnder, bmOver;
  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics underSize;
  nsHTMLReflowMetrics overSize;
  nsIFrame* overFrame = nsnull;
  nsIFrame* underFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (baseFrame)
    underFrame = baseFrame->GetNextSibling();
  if (underFrame)
    overFrame = underFrame->GetNextSibling();
  if (!baseFrame || !underFrame || !overFrame || overFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return ReflowError(aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(underFrame, underSize, bmUnder);
  GetReflowAndBoundingMetricsFor(overFrame, overSize, bmOver);

  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  ////////////////////
  // Place Children

  aRenderingContext.SetFont(GetStyleFont()->mFont, nsnull);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord xHeight = 0;
  fm->GetXHeight (xHeight);

  nscoord ruleThickness;
  GetRuleThickness (aRenderingContext, fm, ruleThickness);

  nscoord correction = 0;
  GetItalicCorrection (bmBase, correction);

  // there are 2 different types of placement depending on 
  // whether we want an accented under or not

  nscoord underDelta1 = 0; // gap between base and underscript
  nscoord underDelta2 = 0; // extra space beneath underscript

  if (!NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {
    // Rule 13a, App. G, TeXbook
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
    underDelta1 = ruleThickness + onePixel/2;
    underDelta2 = ruleThickness;
  }
  // empty under?
  if (!(bmUnder.ascent + bmUnder.descent)) underDelta1 = 0;

  nscoord overDelta1 = 0; // gap between base and overscript
  nscoord overDelta2 = 0; // extra space above overscript

  if (!NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)) {    
    // Rule 13a, App. G, TeXbook
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
    // Rule 12, App. G, TeXbook
    overDelta1 = ruleThickness + onePixel/2;
    if (bmBase.ascent < xHeight) { 
      overDelta1 += xHeight - bmBase.ascent;
    }
    overDelta2 = ruleThickness;
  }
  // empty over?
  if (!(bmOver.ascent + bmOver.descent)) overDelta1 = 0;

  nscoord dxBase, dxOver = 0, dxUnder = 0;

  //////////
  // pass 1, do what <mover> does: attach the overscript on the base

  // Ad-hoc - This is to override fonts which have ready-made _accent_
  // glyphs with negative lbearing and rbearing. We want to position
  // the overscript ourselves
  nscoord overWidth = bmOver.width;
  if (!overWidth && (bmOver.rightBearing - bmOver.leftBearing > 0)) {
    overWidth = bmOver.rightBearing - bmOver.leftBearing;
    dxOver = -bmOver.leftBearing;
  }

  if (NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)) {
    mBoundingMetrics.width = bmBase.width; 
    dxOver += correction + (mBoundingMetrics.width - overWidth)/2;
  }
  else {
    mBoundingMetrics.width = PR_MAX(bmBase.width, overWidth);
    dxOver += correction/2 + (mBoundingMetrics.width - overWidth)/2;
  }
  dxBase = (mBoundingMetrics.width - bmBase.width)/2;

  mBoundingMetrics.ascent = 
    bmBase.ascent + overDelta1 + bmOver.ascent + bmOver.descent;
  mBoundingMetrics.descent = 
    bmBase.descent + underDelta1 + bmUnder.ascent + bmUnder.descent;
  mBoundingMetrics.leftBearing = 
    PR_MIN(dxBase + bmBase.leftBearing, dxOver + bmOver.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(dxBase + bmBase.rightBearing, dxOver + bmOver.rightBearing);

  //////////
  // pass 2, do what <munder> does: attach the underscript on the previous
  // result. We conceptually view the previous result as an "anynomous base" 
  // from where to attach the underscript. Hence if the underscript is empty,
  // we should end up like <mover>. If the overscript is empty, we should
  // end up like <munder>.

  nsBoundingMetrics bmAnonymousBase = mBoundingMetrics;
  nscoord ascentAnonymousBase =
    PR_MAX(mBoundingMetrics.ascent + overDelta2,
           overSize.ascent + bmOver.descent + overDelta1 + bmBase.ascent);

  GetItalicCorrection(bmAnonymousBase, correction);

  nscoord maxWidth = PR_MAX(bmAnonymousBase.width, bmUnder.width);
  if (NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {    
    dxUnder = (maxWidth - bmUnder.width)/2;;
  }
  else {
    dxUnder = -correction/2 + (maxWidth - bmUnder.width)/2;
  }
  nscoord dxAnonymousBase = (maxWidth - bmAnonymousBase.width)/2;

  // adjust the offsets of the real base and overscript since their
  // final offsets should be relative to us...
  dxOver += dxAnonymousBase;
  dxBase += dxAnonymousBase;

  mBoundingMetrics.width =
    PR_MAX(dxAnonymousBase + bmAnonymousBase.width, dxUnder + bmUnder.width);
  mBoundingMetrics.leftBearing =
    PR_MIN(dxAnonymousBase + bmAnonymousBase.leftBearing, dxUnder + bmUnder.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(dxAnonymousBase + bmAnonymousBase.rightBearing, dxUnder + bmUnder.rightBearing);

  aDesiredSize.ascent = ascentAnonymousBase;
  aDesiredSize.height = aDesiredSize.ascent +
    PR_MAX(mBoundingMetrics.descent + underDelta2,
           bmAnonymousBase.descent + underDelta1 + bmUnder.ascent +
             underSize.height - underSize.ascent);
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dy;
    // place overscript
    dy = aDesiredSize.ascent - mBoundingMetrics.ascent + bmOver.ascent - overSize.ascent;
    FinishReflowChild (overFrame, GetPresContext(), nsnull, overSize, dxOver, dy, 0);
    // place base
    dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, GetPresContext(), nsnull, baseSize, dxBase, dy, 0);
    // place underscript
    dy = aDesiredSize.ascent + mBoundingMetrics.descent - bmUnder.descent - underSize.ascent;
    FinishReflowChild (underFrame, GetPresContext(), nsnull, underSize, dxUnder, dy, 0);
  }
  return NS_OK;
}
