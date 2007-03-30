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

#include "nsMathMLmoverFrame.h"
#include "nsMathMLmsupFrame.h"

//
// <mover> -- attach an overscript to a base - implementation
//

nsIFrame*
NS_NewMathMLmoverFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmoverFrame(aContext);
}

nsMathMLmoverFrame::~nsMathMLmoverFrame()
{
}

NS_IMETHODIMP
nsMathMLmoverFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  if (nsGkAtoms::accent_ == aAttribute) {
    // When we have automatic data to update within ourselves, we ask our
    // parent to re-layout its children
    return ReLayoutChildren(mParent);
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

NS_IMETHODIMP
nsMathMLmoverFrame::UpdatePresentationData(PRInt32         aScriptLevelIncrement,
                                           PRUint32        aFlagsValues,
                                           PRUint32        aFlagsToUpdate)
{
  nsMathMLContainerFrame::UpdatePresentationData(
    aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
  // disable the stretch-all flag if we are going to act like a superscript
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
nsMathMLmoverFrame::UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                                      PRInt32         aLastIndex,
                                                      PRInt32         aScriptLevelIncrement,
                                                      PRUint32        aFlagsValues,
                                                      PRUint32        aFlagsToUpdate)
{
  // mover is special... The REC says:
  // Within overscript, <mover> always sets displaystyle to "false", 
  // but increments scriptlevel by 1 only when accent is "false".
  // This means that
  // 1. don't allow displaystyle to change in the overscript
  // 2. if the value of the accent is changed, we need to recompute the
  //    scriptlevel of the overscript. The problem is that the accent
  //    can change in the <mo> deep down the embellished hierarchy

  // Do #1 here, never allow displaystyle to be changed in the overscript
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
      PropagatePresentationDataFor(childFrame, aScriptLevelIncrement,
                                   aFlagsValues, aFlagsToUpdate);
    }
    index++;
    childFrame = childFrame->GetNextSibling();
  }
  return NS_OK;

  // For #2, changing the accent attribute will trigger a re-build of
  // all automatic data in the embellished hierarchy
}

NS_IMETHODIMP
nsMathMLmoverFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmoverFrame::TransmitAutomaticData()
{
  // At this stage, all our children are in sync and we can fully
  // resolve our own mEmbellishData struct
  //---------------------------------------------------------------------

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

  nsIFrame* overscriptFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (baseFrame)
    overscriptFrame = baseFrame->GetNextSibling();

  // if our base is an embellished operator, let its state bubble to us (in particular,
  // this is where we get the flag for NS_MATHML_EMBELLISH_MOVABLELIMITS). Our flags
  // are reset to the default values of false if the base frame isn't embellished.
  mPresentationData.baseFrame = baseFrame;
  GetEmbellishDataFrom(baseFrame, mEmbellishData);

  // The default value of accent is false, unless the overscript is embellished
  // and its core <mo> is an accent
  nsEmbellishData embellishData;
  GetEmbellishDataFrom(overscriptFrame, embellishData);
  if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags))
    mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTOVER;
  else
    mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTOVER;

  // if we have an accent attribute, it overrides what the overscript said
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_true, &nsGkAtoms::_false, nsnull};
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
     Within overscript, <mover> always sets displaystyle to "false", 
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

  return NS_OK;
}

/*
The REC says:
* If the base is an operator with movablelimits="true" (or an embellished
  operator whose <mo> element core has movablelimits="true"), and
  displaystyle="false", then overscript is drawn in a superscript
  position. In this case, the accent attribute is ignored. This is
  often used for limits on symbols such as &sum;. 

i.e.:
 if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like superscript
 }
 else {
  // place like overscript 
 }
*/

NS_IMETHODIMP
nsMathMLmoverFrame::Place(nsIRenderingContext& aRenderingContext,
                          PRBool               aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{ 
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    // place like superscript
    return nsMathMLmsupFrame::PlaceSuperScript(PresContext(),
                                               aRenderingContext,
                                               aPlaceOrigin,
                                               aDesiredSize,
                                               this, 0, PresContext()->PointsToAppUnits(0.5f));
  }

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmOver;
  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics overSize;
  nsIFrame* overFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (baseFrame)
    overFrame = baseFrame->GetNextSibling();
  if (!baseFrame || !overFrame || overFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return ReflowError(aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
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

  // there are 2 different types of placement depending on 
  // whether we want an accented overscript or not

  nscoord correction = 0;
  GetItalicCorrection (bmBase, correction);

  nscoord delta1 = 0; // gap between base and overscript
  nscoord delta2 = 0; // extra space above overscript
  if (!NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)) {    
    // Rule 13a, App. G, TeXbook
    nscoord bigOpSpacing1, bigOpSpacing3, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      bigOpSpacing1, dummy, 
                      bigOpSpacing3, dummy, 
                      bigOpSpacing5);
    delta1 = PR_MAX(bigOpSpacing1, (bigOpSpacing3 - bmOver.descent));
    delta2 = bigOpSpacing5;

    // XXX This is not a TeX rule... 
    // delta1 (as computed above) can become really big when bmOver.descent is
    // negative,  e.g., if the content is &OverBar. In such case, we use the height
    if (bmOver.descent < 0)    
      delta1 = PR_MAX(bigOpSpacing1, (bigOpSpacing3 - (bmOver.ascent + bmOver.descent)));
  }
  else {
    // Rule 12, App. G, TeXbook
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
    delta1 = ruleThickness + onePixel/2; // we have at least the padding
    if (bmBase.ascent < xHeight) { 
      // also ensure at least x-height above the baseline of the base
      delta1 += xHeight - bmBase.ascent;
    }
    delta2 = ruleThickness;
  }
  // empty over?
  if (!(bmOver.ascent + bmOver.descent)) delta1 = 0;

  nscoord dxBase, dxOver = 0;

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
  dxBase = (mBoundingMetrics.width - bmBase.width) / 2;

  mBoundingMetrics.ascent = 
    bmOver.ascent + bmOver.descent + delta1 + bmBase.ascent;
  mBoundingMetrics.descent = bmBase.descent;
  mBoundingMetrics.leftBearing = 
    PR_MIN(dxBase + bmBase.leftBearing, dxOver + bmOver.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(dxBase + bmBase.rightBearing, dxOver + bmOver.rightBearing);

  aDesiredSize.ascent = 
    PR_MAX(mBoundingMetrics.ascent + delta2,
           overSize.ascent + bmOver.descent + delta1 + bmBase.ascent);
  aDesiredSize.height = aDesiredSize.ascent +
    baseSize.height - baseSize.ascent;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    // place base
    nscoord dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, PresContext(), nsnull, baseSize, dxBase, dy, 0);
    // place overscript
    dy = aDesiredSize.ascent - 
      mBoundingMetrics.ascent + bmOver.ascent - overSize.ascent;
    FinishReflowChild (overFrame, PresContext(), nsnull, overSize, dxOver, dy, 0);
  }
  return NS_OK;
}
