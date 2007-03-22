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

#include "nsMathMLmunderFrame.h"
#include "nsMathMLmsubFrame.h"

//
// <munder> -- attach an underscript to a base - implementation
//

nsIFrame*
NS_NewMathMLmunderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmunderFrame(aContext);
}

nsMathMLmunderFrame::~nsMathMLmunderFrame()
{
}

NS_IMETHODIMP
nsMathMLmunderFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                      nsIAtom*        aAttribute,
                                      PRInt32         aModType)
{
  if (nsGkAtoms::accentunder_ == aAttribute) {
    // When we have automatic data to update within ourselves, we ask our
    // parent to re-layout its children
    return ReLayoutChildren(mParent);
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

NS_IMETHODIMP
nsMathMLmunderFrame::UpdatePresentationData(PRInt32         aScriptLevelIncrement,
                                            PRUint32        aFlagsValues,
                                            PRUint32        aFlagsToUpdate)
{
  nsMathMLContainerFrame::UpdatePresentationData(
    aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
  // disable the stretch-all flag if we are going to act like a subscript
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
nsMathMLmunderFrame::UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                                       PRInt32         aLastIndex,
                                                       PRInt32         aScriptLevelIncrement,
                                                       PRUint32        aFlagsValues,
                                                       PRUint32        aFlagsToUpdate)
{
  // munder is special... The REC says:
  // Within underscript, <munder> always sets displaystyle to "false", 
  // but increments scriptlevel by 1 only when accentunder is "false".
  // This means that
  // 1. don't allow displaystyle to change in the underscript
  // 2. if the value of the accent is changed, we need to recompute the
  //    scriptlevel of the underscript. The problem is that the accent
  //    can change in the <mo> deep down the embellished hierarchy

  // Do #1 here, never allow displaystyle to be changed in the underscript
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
nsMathMLmunderFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderFrame::TransmitAutomaticData()
{
  // At this stage, all our children are in sync and we can fully
  // resolve our own mEmbellishData struct
  //---------------------------------------------------------------------

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

  nsIFrame* underscriptFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (baseFrame)
    underscriptFrame = baseFrame->GetNextSibling();

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

  // if we have an accentunder attribute, it overrides what the underscript said
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_true, &nsGkAtoms::_false, nsnull};
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::accentunder_,
                                    strings, eCaseMatters)) {
    case 0: mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER; break;
    case 1: mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER; break;
  }

  // disable the stretch-all flag if we are going to act like a superscript
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags))
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  // Now transmit any change that we want to our children so that they
  // can update their mPresentationData structs
  //---------------------------------------------------------------------

  /* The REC says:
     Within underscript, <munder> always sets displaystyle to "false", 
     but increments scriptlevel by 1 only when accentunder is "false".

     The TeXBook treats 'under' like a subscript, so p.141 or Rule 13a 
     say it should be compressed
  */
  PRInt32 increment = NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)
    ? 0 : 1;
  PropagatePresentationDataFor(underscriptFrame, increment,
    ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
     NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);

  return NS_OK;
}

/*
The REC says:
* If the base is an operator with movablelimits="true" (or
  an embellished operator whose <mo> element core has
  movablelimits="true"), and displaystyle="false", then
  underscript is drawn in a subscript position. In this case,
  the accentunder attribute is ignored. This is often used
  for limits on symbols such as &sum;. 

i.e.,:
 if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like subscript
 }
 else {
  // place like underscript 
 }
*/

NS_IMETHODIMP
nsMathMLmunderFrame::Place(nsIRenderingContext& aRenderingContext,
                           PRBool               aPlaceOrigin,
                           nsHTMLReflowMetrics& aDesiredSize)
{
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    // place like subscript
    return nsMathMLmsubFrame::PlaceSubScript(GetPresContext(),
                                             aRenderingContext,
                                             aPlaceOrigin,
                                             aDesiredSize,
                                             this, 0, GetPresContext()->PointsToAppUnits(0.5f));
  }

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmUnder;
  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics underSize;
  nsIFrame* underFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (baseFrame)
    underFrame = baseFrame->GetNextSibling();
  if (!baseFrame || !underFrame || underFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return ReflowError(aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(underFrame, underSize, bmUnder);

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
  // whether we want an accented under or not

  nscoord correction = 0;
  GetItalicCorrection (bmBase, correction);

  nscoord delta1 = 0; // gap between base and underscript
  nscoord delta2 = 0; // extra space beneath underscript
  if (!NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {    
    // Rule 13a, App. G, TeXbook
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
    delta1 = ruleThickness + onePixel/2;
    delta2 = ruleThickness;
  }
  // empty under?
  if (!(bmUnder.ascent + bmUnder.descent)) delta1 = 0;

  nscoord dxBase, dxUnder;
  nscoord maxWidth = PR_MAX(bmBase.width, bmUnder.width);
  if (NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {    
    dxUnder = (maxWidth - bmUnder.width)/2;
  }
  else {
    dxUnder = -correction/2 + (maxWidth - bmUnder.width)/2;
  }
  dxBase = (maxWidth - bmBase.width)/2;

  mBoundingMetrics.width =
    PR_MAX(dxBase + bmBase.width, dxUnder + bmUnder.width);
  mBoundingMetrics.ascent = bmBase.ascent;
  mBoundingMetrics.descent = 
    bmBase.descent + delta1 + bmUnder.ascent + bmUnder.descent;
  mBoundingMetrics.leftBearing = 
    PR_MIN(dxBase + bmBase.leftBearing, dxUnder + bmUnder.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(dxBase + bmBase.rightBearing, dxUnder + bmUnder.rightBearing);

  aDesiredSize.ascent = baseSize.ascent;
  aDesiredSize.height = aDesiredSize.ascent +
    PR_MAX(mBoundingMetrics.descent + delta2,
           bmBase.descent + delta1 + bmUnder.ascent +
             underSize.height - underSize.ascent);
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dy = 0;
    // place base
    FinishReflowChild(baseFrame, GetPresContext(), nsnull, baseSize, dxBase, dy, 0);
    // place underscript
    dy = aDesiredSize.ascent + mBoundingMetrics.descent - bmUnder.descent - underSize.ascent;
    FinishReflowChild(underFrame, GetPresContext(), nsnull, underSize, dxUnder, dy, 0);
  }

  return NS_OK;
}

