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

nsresult
NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmfracFrame* it = new (aPresShell) nsMathMLmfracFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmfracFrame::nsMathMLmfracFrame() : 
  mLineRect()
{
}

nsMathMLmfracFrame::~nsMathMLmfracFrame()
{
}

NS_IMETHODIMP
nsMathMLmfracFrame::Init(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  nsresult rv = NS_OK;

  rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  if (NS_FAILED(rv)) {
    return rv;
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}

nscoord 
nsMathMLmfracFrame::CalcLineThickness(nsIPresContext*  aPresContext,
                                      nsIStyleContext* aStyleContext,
                                      nsString&        aThicknessAttribute,
                                      nscoord          onePixel,
                                      nscoord          aDefaultRuleThickness)
{
  nscoord defaultThickness = aDefaultRuleThickness;
  nscoord lineThickness = aDefaultRuleThickness;
  nscoord minimumThickness = onePixel;

  if (0 < aThicknessAttribute.Length()) {
    if (aThicknessAttribute.EqualsWithConversion("thin")) {
      lineThickness = NSToCoordFloor(defaultThickness * THIN_FRACTION_LINE);
      minimumThickness = onePixel * THIN_FRACTION_LINE_MINIMUM_PIXELS;
      // should visually decrease by at least one pixel, if default is not a pixel
      if (defaultThickness > onePixel && lineThickness > defaultThickness - onePixel)
        lineThickness = defaultThickness - onePixel;
    }
    else if (aThicknessAttribute.EqualsWithConversion("medium")) {
      lineThickness = NSToCoordRound(defaultThickness * MEDIUM_FRACTION_LINE);
      minimumThickness = onePixel * MEDIUM_FRACTION_LINE_MINIMUM_PIXELS;
      // should visually increase by at least one pixel
      if (lineThickness < defaultThickness + onePixel)
        lineThickness = defaultThickness + onePixel;
    }
    else if (aThicknessAttribute.EqualsWithConversion("thick")) {
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
nsMathMLmfracFrame::Paint(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  ////////////
  // paint the fraction line
  if ((NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) &&
      !NS_MATHML_HAS_ERROR(mPresentationData.flags) &&
      !mLineRect.IsEmpty()) {
/*
//  line looking like <hr noshade>
    const nsStyleColor* color;
    nscolor colors[2];
    color = nsStyleUtil::FindNonTransparentBackground(mStyleContext);
    NS_Get3DColors(colors, color->mBackgroundColor);
    aRenderingContext.SetColor(colors[0]);
*/
//  solid line with the current text color
    const nsStyleColor *color = NS_STATIC_CAST(const nsStyleColor*,
      mStyleContext->GetStyleData(eStyleStruct_Color));
    aRenderingContext.SetColor(color->mColor);

//  draw the line, there is 1 pixel padding at either end
    aRenderingContext.FillRect(mLineRect.x, mLineRect.y, 
                               mLineRect.width, mLineRect.height);
  }

  /////////////
  // paint the numerator and denominator
  return nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                       aDirtyRect, aWhichLayer);
}

NS_IMETHODIMP
nsMathMLmfracFrame::Place(nsIPresContext*      aPresContext,
			  nsIRenderingContext& aRenderingContext,
			  PRBool               aPlaceOrigin,
			  nsHTMLReflowMetrics& aDesiredSize)
{
  ////////////////////////////////////
  // Get the children's desired sizes

  PRInt32 count = 0;
  nsBoundingMetrics bmNum, bmDen;
  nsHTMLReflowMetrics sizeNum (nsnull);
  nsHTMLReflowMetrics sizeDen (nsnull);
  nsIFrame* frameNum = nsnull;
  nsIFrame* frameDen = nsnull;

  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (0 == count) {
    	// numerator
      frameNum = childFrame;
      GetReflowAndBoundingMetricsFor(frameNum, sizeNum, bmNum);
    }
    else if (1 == count) {
    	// denominator
      frameDen = childFrame;
      GetReflowAndBoundingMetricsFor(frameDen, sizeDen, bmDen);
    }
    count++;
    
    childFrame->GetNextSibling(&childFrame);
  }
  if (2 != count) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
  }

  //////////////////
  // Get shifts

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
    mStyleContext->GetStyleData(eStyleStruct_Font));
  aRenderingContext.SetFont(font->mFont);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord defaultRuleThickness;
  GetRuleThickness(aRenderingContext, fm, defaultRuleThickness);

  // see if the linethickness attribute is there 
  nsAutoString value;
  GetAttribute(mContent, mPresentationData.mstyle, nsMathMLAtoms::linethickness_, value);
  mLineRect.height = CalcLineThickness(aPresContext, mStyleContext, 
                                       value, onePixel, defaultRuleThickness);
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
  nscoord axisHeight = 0;

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
    GetAxisHeight(aRenderingContext, fm, axisHeight);

    // min clearance between numerator or denominator and middle of bar

    // TeX has a different interpretation of the thickeness.
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

  // there is 1 pixel padding at either end of the fraction
  // XXX Need revisiting the width. TeX uses the exact width
  // e.g. in $$\huge\frac{\displaystyle\int}{i}$$
  nscoord width = 2*onePixel + PR_MAX(bmNum.width, bmDen.width);
  nscoord dxNum = (width - sizeNum.width)/2;
  nscoord dxDen = (width - sizeDen.width)/2;

  // see if the numalign attribute is there 
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle, 
                   nsMathMLAtoms::numalign_, value)) {
    if (value.EqualsWithConversion("left"))
      dxNum = onePixel;
    else if (value.EqualsWithConversion("right"))
      dxNum = width - onePixel - sizeNum.width;
  }
  // see if the denomalign attribute is there 
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle, 
                   nsMathMLAtoms::denomalign_, value)) {
    if (value.EqualsWithConversion("left"))
      dxDen = onePixel;
    else if (value.EqualsWithConversion("right"))
      dxDen = width - onePixel - sizeDen.width;
  }

  mBoundingMetrics.rightBearing =
    PR_MAX(dxNum + bmNum.rightBearing, dxDen + bmDen.rightBearing);
  if (mBoundingMetrics.rightBearing < width - onePixel)
    mBoundingMetrics.rightBearing = width - onePixel;
  mBoundingMetrics.leftBearing =
    PR_MIN(dxNum + bmNum.leftBearing, dxDen + bmDen.leftBearing);
  if (mBoundingMetrics.leftBearing > onePixel)
    mBoundingMetrics.leftBearing = onePixel;
  mBoundingMetrics.ascent = bmNum.ascent + numShift;
  mBoundingMetrics.descent = bmDen.descent + denShift;
  mBoundingMetrics.width = width;

  aDesiredSize.ascent = sizeNum.ascent + numShift;
  aDesiredSize.descent = sizeDen.descent + denShift;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dy;
    // place numerator
    dy = 0;
    FinishReflowChild(frameNum, aPresContext, sizeNum, dxNum, dy, 0);
    // place denominator
    dy = aDesiredSize.height - sizeDen.height;
    FinishReflowChild(frameDen, aPresContext, sizeDen, dxDen, dy, 0);
    // place the fraction bar - dy is top of bar
    dy = aDesiredSize.ascent - (axisHeight + actualRuleThickness/2);
    mLineRect.SetRect(onePixel, dy, width - 2*onePixel, actualRuleThickness);
  }

  return NS_OK;
}
