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
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLmfencedFrame.h"
#include "nsMathMLmfracFrame.h"

//
// <mfrac> -- form a fraction from two subexpressions - implementation
//

// various fraction line thicknesses (multiplicative values of the default rule thickness)

#define THIN_FRACTION_LINE                   0.5f
#define THIN_FRACTION_LINE_MINIMUM_PIXELS    1  // minimum of 1 pixel

#define MEDIUM_FRACTION_LINE                 1.5f
#define MEDIUM_FRACTION_LINE_MINIMUM_PIXELS  2  // minimum of 2 pixels

#define THICK_FRACTION_LINE                  2.0f
#define THICK_FRACTION_LINE_MINIMUM_PIXELS   4  // minimum of 4 pixels

// additional style context to be used by our MathMLChar.
#define NS_SLASH_CHAR_STYLE_CONTEXT_INDEX   0

static const PRUnichar kSlashChar = PRUnichar('/');

nsIFrame*
NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmfracFrame(aContext);
}

nsMathMLmfracFrame::~nsMathMLmfracFrame()
{
  if (mSlashChar) {
    delete mSlashChar;
    mSlashChar = nsnull;
  }
}

PRBool
nsMathMLmfracFrame::IsBevelled()
{
  nsAutoString value;
  GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::bevelled_,
               value);
  return value.EqualsLiteral("true");
}

NS_IMETHODIMP
nsMathMLmfracFrame::Init(nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);

  if (IsBevelled()) {
    // enable the bevelled rendering
    mSlashChar = new nsMathMLChar();
    if (mSlashChar) {
      nsPresContext* presContext = PresContext();
    
      nsAutoString slashChar; slashChar.Assign(kSlashChar);
      mSlashChar->SetData(presContext, slashChar);
      ResolveMathMLCharStyle(presContext, mContent, mStyleContext, mSlashChar, PR_TRUE);
    }
  }

  return rv;
}

eMathMLFrameType
nsMathMLmfracFrame::GetMathMLFrameType()
{
  // frac is "inner" in TeXBook, Appendix G, rule 15e. See also page 170.
  return eMathMLFrameType_Inner;
}

NS_IMETHODIMP
nsMathMLmfracFrame::TransmitAutomaticData()
{
  // 1. The REC says:
  //    The <mfrac> element sets displaystyle to "false", or if it was already
  //    false increments scriptlevel by 1, within numerator and denominator.
  // 2. The TeXbook (Ch 17. p.141) says the numerator inherits the compression
  //    while the denominator is compressed
  PRInt32 increment =
     NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags) ? 0 : 1;
  mInnerScriptLevel = mPresentationData.scriptLevel + increment;
  UpdatePresentationDataFromChildAt(0, -1, increment,
    ~NS_MATHML_DISPLAYSTYLE,
     NS_MATHML_DISPLAYSTYLE);
  UpdatePresentationDataFromChildAt(1,  1, 0,
     NS_MATHML_COMPRESSED,
     NS_MATHML_COMPRESSED);

  // if our numerator is an embellished operator, let its state bubble to us
  GetEmbellishDataFrom(mFrames.FirstChild(), mEmbellishData);
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {
    // even when embellished, we need to record that <mfrac> won't fire
    // Stretch() on its embellished child
    mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  }

  return NS_OK;
}

nscoord 
nsMathMLmfracFrame::CalcLineThickness(nsPresContext*  aPresContext,
                                      nsStyleContext*  aStyleContext,
                                      nsString&        aThicknessAttribute,
                                      nscoord          onePixel,
                                      nscoord          aDefaultRuleThickness)
{
  nscoord defaultThickness = aDefaultRuleThickness;
  nscoord lineThickness = aDefaultRuleThickness;
  nscoord minimumThickness = onePixel;

  if (!aThicknessAttribute.IsEmpty()) {
    if (aThicknessAttribute.EqualsLiteral("thin")) {
      lineThickness = NSToCoordFloor(defaultThickness * THIN_FRACTION_LINE);
      minimumThickness = onePixel * THIN_FRACTION_LINE_MINIMUM_PIXELS;
      // should visually decrease by at least one pixel, if default is not a pixel
      if (defaultThickness > onePixel && lineThickness > defaultThickness - onePixel)
        lineThickness = defaultThickness - onePixel;
    }
    else if (aThicknessAttribute.EqualsLiteral("medium")) {
      lineThickness = NSToCoordRound(defaultThickness * MEDIUM_FRACTION_LINE);
      minimumThickness = onePixel * MEDIUM_FRACTION_LINE_MINIMUM_PIXELS;
      // should visually increase by at least one pixel
      if (lineThickness < defaultThickness + onePixel)
        lineThickness = defaultThickness + onePixel;
    }
    else if (aThicknessAttribute.EqualsLiteral("thick")) {
      lineThickness = NSToCoordCeil(defaultThickness * THICK_FRACTION_LINE);
      minimumThickness = onePixel * THICK_FRACTION_LINE_MINIMUM_PIXELS;
      // should visually increase by at least two pixels
      if (lineThickness < defaultThickness + 2*onePixel)
        lineThickness = defaultThickness + 2*onePixel;
    }
    else { // see if it is a plain number, or a percentage, or a h/v-unit like 1ex, 2px, 1em
      nsCSSValue cssValue;
      if (ParseNumericValue(aThicknessAttribute, cssValue)) {
        nsCSSUnit unit = cssValue.GetUnit();
        if (eCSSUnit_Number == unit)
          lineThickness = nscoord(float(defaultThickness) * cssValue.GetFloatValue());
        else if (eCSSUnit_Percent == unit)
          lineThickness = nscoord(float(defaultThickness) * cssValue.GetPercentValue());
        else if (eCSSUnit_Null != unit)
          lineThickness = CalcLength(aPresContext, aStyleContext, cssValue);
      }
    }
  }

  // use minimum if the lineThickness is a non-zero value less than minimun
  if (lineThickness && lineThickness < minimumThickness) 
    lineThickness = minimumThickness;

  return lineThickness;
}

NS_IMETHODIMP
nsMathMLmfracFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  /////////////
  // paint the numerator and denominator
  nsresult rv = nsMathMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  /////////////
  // paint the fraction line
  if (mSlashChar) {
    // bevelled rendering
    rv = mSlashChar->Display(aBuilder, this, aLists);
  } else {
    rv = DisplayBar(aBuilder, this, mLineRect, aLists);
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLmfracFrame::Reflow(nsPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  if (mSlashChar) {
    // bevelled rendering
    return nsMathMLmfencedFrame::doReflow(aPresContext, aReflowState,
                                          aDesiredSize, aStatus, this,
                                          nsnull, nsnull, mSlashChar, 1);
  }

  // default rendering
  return nsMathMLContainerFrame::Reflow(aPresContext, aDesiredSize,
                                        aReflowState, aStatus);
}

nscoord
nsMathMLmfracFrame::FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord gap = nsMathMLContainerFrame::FixInterFrameSpacing(aDesiredSize);
  if (!gap) return 0;

  if (mSlashChar) {
    nsRect rect;
    mSlashChar->GetRect(rect);
    rect.MoveBy(gap, 0);
    mSlashChar->SetRect(rect);
  }
  else {
    mLineRect.MoveBy(gap, 0);
  }
  return gap;
}

NS_IMETHODIMP
nsMathMLmfracFrame::Place(nsIRenderingContext& aRenderingContext,
                          PRBool               aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{
  ////////////////////////////////////
  // Get the children's desired sizes
  nsBoundingMetrics bmNum, bmDen;
  nsHTMLReflowMetrics sizeNum;
  nsHTMLReflowMetrics sizeDen;
  nsIFrame* frameDen = nsnull;
  nsIFrame* frameNum = mFrames.FirstChild();
  if (frameNum) 
    frameDen = frameNum->GetNextSibling();
  if (!frameNum || !frameDen || frameDen->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return ReflowError(aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(frameNum, sizeNum, bmNum);
  GetReflowAndBoundingMetricsFor(frameDen, sizeDen, bmDen);

  //////////////////
  // Get shifts

  nsPresContext* presContext = PresContext();
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  aRenderingContext.SetFont(GetStyleFont()->mFont, nsnull);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord defaultRuleThickness, axisHeight;
  GetRuleThickness(aRenderingContext, fm, defaultRuleThickness);
  GetAxisHeight(aRenderingContext, fm, axisHeight);

  // by default, leave at least one-pixel padding at either end, or use
  // lspace & rspace that may come from <mo> if we are an embellished container
  // (we fetch values from the core since they may use units that depend
  // on style data, and style changes could have occurred in the core since
  // our last visit there)
  nsEmbellishData coreData;
  GetEmbellishDataFrom(mEmbellishData.coreFrame, coreData);
  nscoord leftSpace = PR_MAX(onePixel, coreData.leftSpace);
  nscoord rightSpace = PR_MAX(onePixel, coreData.rightSpace);

  // see if the linethickness attribute is there 
  nsAutoString value;
  GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::linethickness_, value);
  mLineRect.height = CalcLineThickness(presContext, mStyleContext, value,
                                       onePixel, defaultRuleThickness);
  nscoord numShift = 0;
  nscoord denShift = 0;

  // Rule 15b, App. G, TeXbook
  nscoord numShift1, numShift2, numShift3;
  nscoord denShift1, denShift2;

  GetNumeratorShifts(fm, numShift1, numShift2, numShift3);
  GetDenominatorShifts(fm, denShift1, denShift2);
  if (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    // C > T
    numShift = numShift1;
    denShift = denShift1;
  }
  else {
    numShift = (0 < mLineRect.height) ? numShift2 : numShift3;
    denShift = denShift2;
  }

  nscoord minClearance = 0;
  nscoord actualClearance = 0;

  nscoord actualRuleThickness =  mLineRect.height;

  if (0 == actualRuleThickness) {
    // Rule 15c, App. G, TeXbook

    // min clearance between numerator and denominator
    minClearance = (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) ?
      7 * defaultRuleThickness : 3 * defaultRuleThickness;
    actualClearance =
      (numShift - bmNum.descent) - (bmDen.ascent - denShift);
    // actualClearance should be >= minClearance
    if (actualClearance < minClearance) {
      nscoord halfGap = (minClearance - actualClearance)/2;
      numShift += halfGap;
      denShift += halfGap;
    }
  }
  else {
    // Rule 15d, App. G, TeXbook

    // min clearance between numerator or denominator and middle of bar

    // TeX has a different interpretation of the thickness.
    // Try $a \above10pt b$ to see. Here is what TeX does:
//     minClearance = (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) ?
//      3 * actualRuleThickness : actualRuleThickness;
 
    // we slightly depart from TeX here. We use the defaultRuleThickness instead
    // of the value coming from the linethickness attribute, i.e., we recover what
    // TeX does if the user hasn't set linethickness. But when the linethickness
    // is set, we avoid the wide gap problem.
     minClearance = (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) ?
      3 * defaultRuleThickness : defaultRuleThickness + onePixel;

    // adjust numShift to maintain minClearance if needed
    actualClearance =
      (numShift - bmNum.descent) - (axisHeight + actualRuleThickness/2);
    if (actualClearance < minClearance) {
      numShift += (minClearance - actualClearance);
    }
    // adjust denShift to maintain minClearance if needed
    actualClearance =
      (axisHeight - actualRuleThickness/2) - (bmDen.ascent - denShift);
    if (actualClearance < minClearance) {
      denShift += (minClearance - actualClearance);
    }
  }

  //////////////////
  // Place Children

  // XXX Need revisiting the width. TeX uses the exact width
  // e.g. in $$\huge\frac{\displaystyle\int}{i}$$
  nscoord width = PR_MAX(bmNum.width, bmDen.width);
  nscoord dxNum = leftSpace + (width - sizeNum.width)/2;
  nscoord dxDen = leftSpace + (width - sizeDen.width)/2;
  width += leftSpace + rightSpace;

  // see if the numalign attribute is there 
  GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::numalign_,
               value);
  if (value.EqualsLiteral("left"))
    dxNum = leftSpace;
  else if (value.EqualsLiteral("right"))
    dxNum = width - rightSpace - sizeNum.width;

  // see if the denomalign attribute is there 
  GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::denomalign_,
               value);
  if (value.EqualsLiteral("left"))
    dxDen = leftSpace;
  else if (value.EqualsLiteral("right"))
    dxDen = width - rightSpace - sizeDen.width;

  mBoundingMetrics.rightBearing =
    PR_MAX(dxNum + bmNum.rightBearing, dxDen + bmDen.rightBearing);
  if (mBoundingMetrics.rightBearing < width - rightSpace)
    mBoundingMetrics.rightBearing = width - rightSpace;
  mBoundingMetrics.leftBearing =
    PR_MIN(dxNum + bmNum.leftBearing, dxDen + bmDen.leftBearing);
  if (mBoundingMetrics.leftBearing > leftSpace)
    mBoundingMetrics.leftBearing = leftSpace;
  mBoundingMetrics.ascent = bmNum.ascent + numShift;
  mBoundingMetrics.descent = bmDen.descent + denShift;
  mBoundingMetrics.width = width;

  aDesiredSize.ascent = sizeNum.ascent + numShift;
  aDesiredSize.height = aDesiredSize.ascent +
                        sizeDen.height - sizeDen.ascent + denShift;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dy;
    // place numerator
    dy = 0;
    FinishReflowChild(frameNum, presContext, nsnull, sizeNum, dxNum, dy, 0);
    // place denominator
    dy = aDesiredSize.height - sizeDen.height;
    FinishReflowChild(frameDen, presContext, nsnull, sizeDen, dxDen, dy, 0);
    // place the fraction bar - dy is top of bar
    dy = aDesiredSize.ascent - (axisHeight + actualRuleThickness/2);
    mLineRect.SetRect(leftSpace, dy, width - (leftSpace + rightSpace), actualRuleThickness);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmfracFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  if (nsGkAtoms::bevelled_ == aAttribute) {
    if (!IsBevelled()) {
      // disable the bevelled rendering
      if (mSlashChar) {
        delete mSlashChar;
        mSlashChar = nsnull;
      }
    }
    else {
      // enable the bevelled rendering
      if (!mSlashChar) {
        mSlashChar = new nsMathMLChar();
        if (mSlashChar) {
          nsPresContext* presContext = PresContext();
          nsAutoString slashChar; slashChar.Assign(kSlashChar);
          mSlashChar->SetData(presContext, slashChar);
          ResolveMathMLCharStyle(presContext, mContent, mStyleContext, mSlashChar, PR_TRUE);
        }
      }
    }
  }
  return nsMathMLContainerFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

NS_IMETHODIMP
nsMathMLmfracFrame::UpdatePresentationData(PRInt32         aScriptLevelIncrement,
                                           PRUint32        aFlagsValues,
                                           PRUint32        aFlagsToUpdate)
{
  // mfrac is special... The REC says:
  // The <mfrac> element sets displaystyle to "false", or if it was already
  // false increments scriptlevel by 1, within numerator and denominator.
  // @see similar peculiarities for <mover>, <munder>, <munderover>

  // This means that
  // 1. If our displaystyle is being changed from true to false, we have
  //    to propagate an inner scriptlevel increment to our children
  // 2. If the displaystyle is changed from false to true, we have to undo
  //    any incrementation that was done on the inner scriptlevel

  if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
    if (mInnerScriptLevel > mPresentationData.scriptLevel) {
      // we get here if our displaystyle is currently false
      NS_ASSERTION(!NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags), "out of sync");
      if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsValues)) {
        // ...and is being set to true, so undo the inner increment now
        mInnerScriptLevel = mPresentationData.scriptLevel;
        UpdatePresentationDataFromChildAt(0, -1, -1, 0, 0);
      }
    }
    else {
      // case of mInnerScriptLevel == mPresentationData.scriptLevel, our
      // current displaystyle is true; we increment the inner scriptlevel if
      // our displaystyle is about to be set to false; since mInnerScriptLevel
      // is changed, we can only get here once
      NS_ASSERTION(NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags), "out of sync");
      if (!NS_MATHML_IS_DISPLAYSTYLE(aFlagsValues)) {
        mInnerScriptLevel = mPresentationData.scriptLevel + 1;
        UpdatePresentationDataFromChildAt(0, -1, 1, 0, 0);
      }
    }
  }

  mInnerScriptLevel += aScriptLevelIncrement;
  return nsMathMLContainerFrame::
    UpdatePresentationData(aScriptLevelIncrement, aFlagsValues,
                           aFlagsToUpdate);
}

NS_IMETHODIMP
nsMathMLmfracFrame::UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                                      PRInt32         aLastIndex,
                                                      PRInt32         aScriptLevelIncrement,
                                                      PRUint32        aFlagsValues,
                                                      PRUint32        aFlagsToUpdate)
{
  // The REC says "The <mfrac> element sets displaystyle to "false" within
  // numerator and denominator"
#if 0
  // At one point I thought that it meant that the displaystyle state of
  // the numerator and denominator cannot be modified by an ancestor, i.e.,
  // to change the displaystlye, one has to use displaystyle="true" with mstyle: 
  // <mfrac> <mstyle>numerator</mstyle> <mstyle>denominator</mstyle> </mfrac>

  // Commenting out for now until it is clear what the intention really is.
  // See also the variants for <mover>, <munder>, <munderover>

  aFlagsToUpdate &= ~NS_MATHML_DISPLAYSTYLE;
  aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
#endif
  return nsMathMLContainerFrame::
    UpdatePresentationDataFromChildAt(aFirstIndex, aLastIndex,
      aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
}

// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
nsStyleContext*
nsMathMLmfracFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  if (!mSlashChar) {
    return nsnull;
  }
  switch (aIndex) {
  case NS_SLASH_CHAR_STYLE_CONTEXT_INDEX:
    return mSlashChar->GetStyleContext();
    break;
  default:
    return nsnull;
  }
}

void
nsMathMLmfracFrame::SetAdditionalStyleContext(PRInt32          aIndex, 
                                              nsStyleContext*  aStyleContext)
{
  if (!mSlashChar) {
    return;
  }
  switch (aIndex) {
  case NS_SLASH_CHAR_STYLE_CONTEXT_INDEX:
    mSlashChar->SetStyleContext(aStyleContext);
    break;
  }
}
