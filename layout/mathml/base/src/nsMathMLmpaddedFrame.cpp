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

#include "nsMathMLmpaddedFrame.h"

#include "xp_str.h"  // to get XP_IS_SPACE

//
// <mpadded> -- adjust space around content - implementation
//

#define NS_MATHML_SIGN_INVALID           -1 // if the attribute is not there
#define NS_MATHML_SIGN_UNSPECIFIED        0
#define NS_MATHML_SIGN_MINUS              1
#define NS_MATHML_SIGN_PLUS               2

#define NS_MATHML_PSEUDO_UNIT_UNSPECIFIED 0
#define NS_MATHML_PSEUDO_UNIT_ITSELF      1 // special
#define NS_MATHML_PSEUDO_UNIT_WIDTH       2
#define NS_MATHML_PSEUDO_UNIT_HEIGHT      3
#define NS_MATHML_PSEUDO_UNIT_DEPTH       4
#define NS_MATHML_PSEUDO_UNIT_LSPACE      5
#define NS_MATHML_PSEUDO_UNIT_NAMEDSPACE  6

nsresult
NS_NewMathMLmpaddedFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmpaddedFrame* it = new (aPresShell) nsMathMLmpaddedFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmpaddedFrame::nsMathMLmpaddedFrame()
{
}

nsMathMLmpaddedFrame::~nsMathMLmpaddedFrame()
{
}

NS_IMETHODIMP
nsMathMLmpaddedFrame::Init(nsIPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mEmbellishData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  /*
  parse the attributes

  width = [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | h-unit | namedspace)
  height= [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | v-unit)
  depth = [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | v-unit)
  lspace= [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | h-unit)
  */

  nsAutoString value;

  /* The REC says:
  There is one exceptional element, <mpadded>, whose attributes cannot be 
  set with <mstyle>. When the attributes width, height and depth are specified
  on an <mstyle> element, they apply only to the <mspace/> element. Similarly, 
  when lspace is set with <mstyle>, it applies only to the <mo> element. 
  */

  // See if attributes are local, don't access mstyle !

  // width
  mWidthSign = NS_MATHML_SIGN_INVALID;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, nsnull,
                   nsMathMLAtoms::width_, value)) {
    ParseAttribute(value, mWidthSign, mWidth, mWidthPseudoUnit);
  }

  // height
  mHeightSign = NS_MATHML_SIGN_INVALID;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, nsnull,
                   nsMathMLAtoms::height_, value)) {
    ParseAttribute(value, mHeightSign, mHeight, mHeightPseudoUnit);
  }

  // depth
  mDepthSign = NS_MATHML_SIGN_INVALID;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, nsnull,
                   nsMathMLAtoms::depth_, value)) {
    ParseAttribute(value, mDepthSign, mDepth, mDepthPseudoUnit);
  }

  // lspace
  mLeftSpaceSign = NS_MATHML_SIGN_INVALID;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, nsnull,
                   nsMathMLAtoms::lspace_, value)) {
    ParseAttribute(value, mLeftSpaceSign, mLeftSpace, mLeftSpacePseudoUnit);
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}

PRBool
nsMathMLmpaddedFrame::ParseAttribute(nsString&   aString,
                                     PRInt32&    aSign,
                                     nsCSSValue& aCSSValue,
                                     PRInt32&    aPseudoUnit)
{
  aCSSValue.Reset();
  aString.CompressWhitespace(); // aString is not a const in this code

  PRInt32 stringLength = aString.Length();
  if (!stringLength) return PR_FALSE;

  nsAutoString value;

  //////////////////////
  // see if the sign is there

  PRInt32 i = 0;

  if (aString[0] == '+') {
    aSign = NS_MATHML_SIGN_PLUS;
    i++;
  }
  else if (aString[0] == '-') {
    aSign = NS_MATHML_SIGN_MINUS;
    i++;
  }
  else
    aSign = NS_MATHML_SIGN_UNSPECIFIED;

  // skip any space after the sign
  while (i < stringLength && XP_IS_SPACE(aString[i]))
    i++;

  ///////////////////////
  // get the string that represents the numeric value

  value.SetLength(0);
  while (i < stringLength && !XP_IS_SPACE(aString[i])) {
    value.Append(aString[i]);
    i++;
  }
  // convert to CSS units
  if (!ParseNumericValue(value, aCSSValue)) {
#ifdef NS_DEBUG
    printf("mpadded: attribute with bad numeric value: %s\n",
            NS_LossyConvertUCS2toASCII(aString).get());
#endif
    aSign = NS_MATHML_SIGN_INVALID;
    return PR_FALSE;
  }

  // skip any space in-between
  while (i < stringLength && XP_IS_SPACE(aString[i]))
    i++;

  //////////////////////////
  // get the string that represents the pseudo unit

  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_UNSPECIFIED;
  if (i == stringLength) { // no explicit pseudo-unit ...
    if ((eCSSUnit_Number == aCSSValue.GetUnit()) && aCSSValue.GetFloatValue()) {
      // ... and no explicit CSS unit either

      // In this case, the MathML REC suggests taking ems for
      // h-unit (width, lspace) or exs for v-unit (height, depth).
      // Here, however, we explicitly request authors to specify
      // the unit. This is more in line with the CSS REC (and
      // it allows keeping the code simpler...)

#ifdef NS_DEBUG
      printf("mpadded: attribute with bad numeric value: %s\n",
              NS_LossyConvertUCS2toASCII(aString).get());
#endif
      aCSSValue.Reset();
      aSign = NS_MATHML_SIGN_INVALID;
      return PR_FALSE;
    }

    // else the only other valid possibility here is a percentage value,
    // otherwise ParseNumericValue() would have failed
   
    aPseudoUnit = NS_MATHML_PSEUDO_UNIT_ITSELF;
    return PR_TRUE;
  }

  if (value.EqualsWithConversion("width"))  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_WIDTH;
  else if (value.EqualsWithConversion("height")) aPseudoUnit = NS_MATHML_PSEUDO_UNIT_HEIGHT;
  else if (value.EqualsWithConversion("depth"))  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_DEPTH;
  else if (value.EqualsWithConversion("lspace")) aPseudoUnit = NS_MATHML_PSEUDO_UNIT_LSPACE;
  else 
  {
    nsCSSValue aCSSNamedSpaceValue;
    if (!aCSSValue.IsLengthUnit() &&
         ParseNamedSpaceValue(nsnull, value, aCSSNamedSpaceValue))
    { // XXX nsnull in ParseNamedSpacedValue()? don't access mstyle?

      aPseudoUnit = NS_MATHML_PSEUDO_UNIT_NAMEDSPACE;
      float namedspace = aCSSNamedSpaceValue.GetFloatValue();

      // combine aCSSNamedSpaceValue and aCSSValue since
      // we know that the unit of aCSSNamedSpaceValue is 'em'
      nsCSSUnit unit = aCSSValue.GetUnit();
      if (eCSSUnit_Number == unit) {
        float scaler = aCSSValue.GetFloatValue();
        aCSSValue.SetFloatValue(scaler*namedspace, eCSSUnit_EM);
        return PR_TRUE;
      }
      else if (eCSSUnit_Percent == unit) {
        float scaler = aCSSValue.GetPercentValue();
        aCSSValue.SetFloatValue(scaler*namedspace, eCSSUnit_EM);
        return PR_TRUE;
      }
    }

    // if we reach here, it means we encounter an unexpected pseudo-unit
    aCSSValue.Reset();
    aSign = NS_MATHML_SIGN_INVALID;
    return PR_FALSE;
  }

  if (aCSSValue.IsLengthUnit()) {
    // if we enter here, it means we have both a CSS unit *and* a pseudo-unit,
    // e.g., width="100em height", this an error/ambiguity that the author should fix
#ifdef NS_DEBUG
    printf("mpadded: attribute with bad numeric value: %s\n",
            NS_LossyConvertUCS2toASCII(aString).get());
#endif
    aCSSValue.Reset();
    aSign = NS_MATHML_SIGN_INVALID;
    return PR_FALSE;
  }

  return PR_TRUE;
}

void
nsMathMLmpaddedFrame::UpdateValue(nsIPresContext*      aPresContext,
                                  nsIStyleContext*     aStyleContext,
                                  PRInt32              aSign,
                                  PRInt32              aPseudoUnit,
                                  nsCSSValue&          aCSSValue,
                                  nscoord              aLeftSpace,
                                  nsBoundingMetrics&   aBoundingMetrics,
                                  nscoord&             aValueToUpdate)
{
  nsCSSUnit unit = aCSSValue.GetUnit();
  if (NS_MATHML_SIGN_INVALID != aSign && eCSSUnit_Null != unit) 
  {
    nscoord scaler, amount;

    if (eCSSUnit_Percent == unit || eCSSUnit_Number == unit) 
    {
      switch(aPseudoUnit)
      {
        case NS_MATHML_PSEUDO_UNIT_WIDTH:
             scaler = aBoundingMetrics.width;
             break;

        case NS_MATHML_PSEUDO_UNIT_HEIGHT:
             scaler = aBoundingMetrics.ascent;
             break;

        case NS_MATHML_PSEUDO_UNIT_DEPTH:
             scaler = aBoundingMetrics.descent;
             break;

        case NS_MATHML_PSEUDO_UNIT_LSPACE:
             scaler = aLeftSpace;
             break;

        default:
          // if we ever reach here, it would mean something is wrong 
          // somewhere with the setup and/or the caller
          NS_ASSERTION(0, "Unexpected Pseudo Unit");
          return;
      }
    }

    if (eCSSUnit_Number == unit)
      amount = NSToCoordRound(float(scaler) * aCSSValue.GetFloatValue());
    else if (eCSSUnit_Percent == unit)
      amount = NSToCoordRound(float(scaler) * aCSSValue.GetPercentValue());
    else
      amount = CalcLength(aPresContext, aStyleContext, aCSSValue);

    nscoord oldValue = aValueToUpdate;
    if (NS_MATHML_SIGN_PLUS == aSign)
      aValueToUpdate += amount;
    else if (NS_MATHML_SIGN_MINUS == aSign)
      aValueToUpdate -= amount;
    else
      aValueToUpdate  = amount;

    /* The REC says:
    Dimensions that would be positive if the content was rendered normally
    cannot be made negative using <mpadded>; a positive dimension is set 
    to 0 if it would otherwise become negative. Dimensions which are 
    initially 0 can be made negative
    */
    if (0 < oldValue && 0 > aValueToUpdate) aValueToUpdate = 0;

  }
}

NS_IMETHODIMP
nsMathMLmpaddedFrame::Reflow(nsIPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;

  ///////////////
  // Let the base class format our content like an inferred mrow
  rv = nsMathMLContainerFrame::Reflow(aPresContext, aDesiredSize,
                                      aReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  if (NS_FAILED(rv)) {
    return rv;
  }

  nscoord height = mBoundingMetrics.ascent;
  nscoord depth  = mBoundingMetrics.descent;
  nscoord width  = mBoundingMetrics.width;
  nscoord lspace = 0; // XXX it is unclear from the REC what is the default here 

  PRInt32 pseudoUnit;

  // update width
  pseudoUnit = (mWidthPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_WIDTH : mWidthPseudoUnit;
  UpdateValue(aPresContext, mStyleContext,
              mWidthSign, pseudoUnit, mWidth,
              lspace, mBoundingMetrics, width);

  // update height
  pseudoUnit = (mHeightPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_HEIGHT : mHeightPseudoUnit;
  UpdateValue(aPresContext, mStyleContext,
              mHeightSign, pseudoUnit, mHeight,
              lspace, mBoundingMetrics, height);

  // update depth
  pseudoUnit = (mDepthPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_DEPTH : mDepthPseudoUnit;
  UpdateValue(aPresContext, mStyleContext,
              mDepthSign, pseudoUnit, mDepth,
              lspace, mBoundingMetrics, depth);

  // update lspace -- should be *last* because lspace is overwritten!!
  pseudoUnit = (mLeftSpacePseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_LSPACE : mLeftSpacePseudoUnit;
  UpdateValue(aPresContext, mStyleContext,
              mLeftSpaceSign, pseudoUnit, mLeftSpace,
              lspace, mBoundingMetrics, lspace);

  // do the padding now that we have everything

  if (lspace) { // there was padding on the left
    mBoundingMetrics.leftBearing = 0;
  }

  if (width != mBoundingMetrics.width) { // there was padding on the right
    mBoundingMetrics.rightBearing = lspace + width;
  }

  nscoord dy = height - mBoundingMetrics.ascent;
  nscoord dx = lspace;

  mBoundingMetrics.ascent = height;
  mBoundingMetrics.descent = depth;
  mBoundingMetrics.width = lspace + width;

  aDesiredSize.ascent += dy;
  aDesiredSize.descent += depth - mBoundingMetrics.descent;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  if (dx || dy) {
    nsRect rect;
    nsIFrame* childFrame = mFrames.FirstChild();
    while (childFrame) {
      childFrame->GetRect(rect);
      childFrame->MoveTo(aPresContext, rect.x + dx, rect.y + dy);
      childFrame->GetNextSibling(&childFrame);
    }
  }

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  return NS_OK;
}
