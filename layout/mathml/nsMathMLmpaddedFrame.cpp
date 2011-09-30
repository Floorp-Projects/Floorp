/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
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
#include "nsCRT.h"  // to get NS_IS_SPACE
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"

#include "nsMathMLmpaddedFrame.h"

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
#define NS_MATHML_PSEUDO_UNIT_NAMEDSPACE  5

nsIFrame*
NS_NewMathMLmpaddedFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmpaddedFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmpaddedFrame)

nsMathMLmpaddedFrame::~nsMathMLmpaddedFrame()
{
}

NS_IMETHODIMP
nsMathMLmpaddedFrame::InheritAutomaticData(nsIFrame* aParent) 
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  return NS_OK;
}

void
nsMathMLmpaddedFrame::ProcessAttributes()
{
  /*
  parse the attributes

  width  = [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | h-unit | namedspace)
  height = [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | v-unit | namedspace)
  depth  = [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | v-unit | namedspace)
  lspace = [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | h-unit | namedspace)
  voffset= [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | v-unit | namedspace)
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
  GetAttribute(mContent, nsnull, nsGkAtoms::width, value);
  if (!value.IsEmpty()) {
    ParseAttribute(value, mWidthSign, mWidth, mWidthPseudoUnit);
  }

  // height
  mHeightSign = NS_MATHML_SIGN_INVALID;
  GetAttribute(mContent, nsnull, nsGkAtoms::height, value);
  if (!value.IsEmpty()) {
    ParseAttribute(value, mHeightSign, mHeight, mHeightPseudoUnit);
  }

  // depth
  mDepthSign = NS_MATHML_SIGN_INVALID;
  GetAttribute(mContent, nsnull, nsGkAtoms::depth_, value);
  if (!value.IsEmpty()) {
    ParseAttribute(value, mDepthSign, mDepth, mDepthPseudoUnit);
  }

  // lspace
  mLeftSpaceSign = NS_MATHML_SIGN_INVALID;
  GetAttribute(mContent, nsnull, nsGkAtoms::lspace_, value);
  if (!value.IsEmpty()) {
    ParseAttribute(value, mLeftSpaceSign, mLeftSpace, mLeftSpacePseudoUnit);
  }

  // voffset
  mVerticalOffsetSign = NS_MATHML_SIGN_INVALID;
  GetAttribute(mContent, nsnull, nsGkAtoms::voffset_, value);
  if (!value.IsEmpty()) {
    ParseAttribute(value, mVerticalOffsetSign, mVerticalOffset, 
                   mVerticalOffsetPseudoUnit);
  }
  
}

// parse an input string in the following format (see bug 148326 for testcases):
// [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | css-unit | namedspace)
bool
nsMathMLmpaddedFrame::ParseAttribute(nsString&   aString,
                                     PRInt32&    aSign,
                                     nsCSSValue& aCSSValue,
                                     PRInt32&    aPseudoUnit)
{
  aCSSValue.Reset();
  aSign = NS_MATHML_SIGN_INVALID;
  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_UNSPECIFIED;
  aString.CompressWhitespace(); // aString is not a const in this code

  PRInt32 stringLength = aString.Length();
  if (!stringLength)
    return PR_FALSE;

  nsAutoString number, unit;

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
  if (i < stringLength && nsCRT::IsAsciiSpace(aString[i]))
    i++;

  // get the number
  bool gotDot = false, gotPercent = false;
  for (; i < stringLength; i++) {
    PRUnichar c = aString[i];
    if (gotDot && c == '.') {
      // error - two dots encountered
      aSign = NS_MATHML_SIGN_INVALID;
      return PR_FALSE;
    }

    if (c == '.')
      gotDot = PR_TRUE;
    else if (!nsCRT::IsAsciiDigit(c)) {
      break;
    }
    number.Append(c);
  }

  // catch error if we didn't enter the loop above... we could simply initialize
  // floatValue = 1, to cater for cases such as width="height", but that wouldn't
  // be in line with the spec which requires an explicit number
  if (number.IsEmpty()) {
#ifdef NS_DEBUG
    printf("mpadded: attribute with bad numeric value: %s\n",
            NS_LossyConvertUTF16toASCII(aString).get());
#endif
    aSign = NS_MATHML_SIGN_INVALID;
    return PR_FALSE;
  }

  PRInt32 errorCode;
  float floatValue = number.ToFloat(&errorCode);
  if (errorCode) {
    aSign = NS_MATHML_SIGN_INVALID;
    return PR_FALSE;
  }

  // skip any space after the number
  if (i < stringLength && nsCRT::IsAsciiSpace(aString[i]))
    i++;

  // see if this is a percentage-based value
  if (i < stringLength && aString[i] == '%') {
    i++;
    gotPercent = PR_TRUE;

    // skip any space after the '%' sign
    if (i < stringLength && nsCRT::IsAsciiSpace(aString[i]))
      i++;
  }

  // the remainder now should be a css-unit, or a pseudo-unit, or a named-space
  aString.Right(unit, stringLength - i);

  if (unit.IsEmpty()) {
    // also cater for the edge case of "0" for which the unit is optional
    if (gotPercent || !floatValue) {
      aCSSValue.SetPercentValue(floatValue / 100.0f);
      aPseudoUnit = NS_MATHML_PSEUDO_UNIT_ITSELF;
      return PR_TRUE;
    }
    /*
    else {
      // no explicit CSS unit and no explicit pseudo-unit...
      // In this case, the MathML REC suggests taking ems for
      // h-unit (width, lspace) or exs for v-unit (height, depth).
      // Here, however, we explicitly request authors to specify
      // the unit. This is more in line with the CSS REC (and
      // it allows keeping the code simpler...)
    }
    */
  }
  else if (unit.EqualsLiteral("width"))  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_WIDTH;
  else if (unit.EqualsLiteral("height")) aPseudoUnit = NS_MATHML_PSEUDO_UNIT_HEIGHT;
  else if (unit.EqualsLiteral("depth"))  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_DEPTH;
  else if (!gotPercent) { // percentage can only apply to a pseudo-unit

    // see if the unit is a named-space
    // XXX nsnull in ParseNamedSpacedValue()? don't access mstyle?
    if (ParseNamedSpaceValue(nsnull, unit, aCSSValue)) {
      // re-scale properly, and we know that the unit of the named-space is 'em'
      floatValue *= aCSSValue.GetFloatValue();
      aCSSValue.SetFloatValue(floatValue, eCSSUnit_EM);
      aPseudoUnit = NS_MATHML_PSEUDO_UNIT_NAMEDSPACE;
      return PR_TRUE;
    }

    // see if the input was just a CSS value
    number.Append(unit); // leave the sign out if it was there
    if (ParseNumericValue(number, aCSSValue))
      return PR_TRUE;
  }

  // if we enter here, we have a number that will act as a multiplier on a pseudo-unit
  if (aPseudoUnit != NS_MATHML_PSEUDO_UNIT_UNSPECIFIED) {
    if (gotPercent)
      aCSSValue.SetPercentValue(floatValue / 100.0f);
    else
      aCSSValue.SetFloatValue(floatValue, eCSSUnit_Number);

    return PR_TRUE;
  }


#ifdef NS_DEBUG
  printf("mpadded: attribute with bad numeric value: %s\n",
          NS_LossyConvertUTF16toASCII(aString).get());
#endif
  // if we reach here, it means we encounter an unexpected input
  aSign = NS_MATHML_SIGN_INVALID;
  return PR_FALSE;
}

void
nsMathMLmpaddedFrame::UpdateValue(PRInt32                  aSign,
                                  PRInt32                  aPseudoUnit,
                                  const nsCSSValue&        aCSSValue,
                                  const nsBoundingMetrics& aBoundingMetrics,
                                  nscoord&                 aValueToUpdate) const
{
  nsCSSUnit unit = aCSSValue.GetUnit();
  if (NS_MATHML_SIGN_INVALID != aSign && eCSSUnit_Null != unit) {
    nscoord scaler = 0, amount = 0;

    if (eCSSUnit_Percent == unit || eCSSUnit_Number == unit) {
      switch(aPseudoUnit) {
        case NS_MATHML_PSEUDO_UNIT_WIDTH:
             scaler = aBoundingMetrics.width;
             break;

        case NS_MATHML_PSEUDO_UNIT_HEIGHT:
             scaler = aBoundingMetrics.ascent;
             break;

        case NS_MATHML_PSEUDO_UNIT_DEPTH:
             scaler = aBoundingMetrics.descent;
             break;

        default:
          // if we ever reach here, it would mean something is wrong 
          // somewhere with the setup and/or the caller
          NS_ERROR("Unexpected Pseudo Unit");
          return;
      }
    }

    if (eCSSUnit_Number == unit)
      amount = NSToCoordRound(float(scaler) * aCSSValue.GetFloatValue());
    else if (eCSSUnit_Percent == unit)
      amount = NSToCoordRound(float(scaler) * aCSSValue.GetPercentValue());
    else
      amount = CalcLength(PresContext(), mStyleContext, aCSSValue);

    if (NS_MATHML_SIGN_PLUS == aSign)
      aValueToUpdate += amount;
    else if (NS_MATHML_SIGN_MINUS == aSign)
      aValueToUpdate -= amount;
    else
      aValueToUpdate  = amount;
  }
}

NS_IMETHODIMP
nsMathMLmpaddedFrame::Reflow(nsPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  ProcessAttributes();

  ///////////////
  // Let the base class format our content like an inferred mrow
  nsresult rv = nsMathMLContainerFrame::Reflow(aPresContext, aDesiredSize,
                                               aReflowState, aStatus);
  //NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  return rv;
}

/* virtual */ nsresult
nsMathMLmpaddedFrame::Place(nsRenderingContext& aRenderingContext,
                            bool                 aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv =
    nsMathMLContainerFrame::Place(aRenderingContext, PR_FALSE, aDesiredSize);
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags) || NS_FAILED(rv)) {
    DidReflowChildren(GetFirstPrincipalChild());
    return rv;
  }

  nscoord height = mBoundingMetrics.ascent;
  nscoord depth  = mBoundingMetrics.descent;
  // In MathML2 (http://www.w3.org/TR/MathML2/chapter3.html#presm.mpadded),
  // lspace is "the amount of space between the left edge of a bounding box
  // and the start of the rendering of its contents' bounding box" and the
  // default is zero.
  //
  // In MathML3 draft
  // http://www.w3.org/TR/2007/WD-MathML3-20070427/chapter3.html#id.3.3.6.2,
  // lspace is "the amount of space between the left edge of the bounding box
  // and the positioning poin [sic] of the mpadded element" and the default is
  // "same as content".
  //
  // In both cases, "MathML renderers should ensure that, except for the
  // effects of the attributes, relative spacing between the contents of
  // mpadded and surrounding MathML elements is not modified by replacing an
  // mpadded element with an mrow element with the same content."
  nscoord lspace = 0;
  // In MATHML3, "width" will be the bounding box width and "advancewidth" will
  // refer "to the horizontal distance between the positioning point of the
  // mpadded and the positioning point for the following content".  MathML2
  // doesn't make the distinction.
  nscoord width  = mBoundingMetrics.width;
  nscoord voffset = 0;

  PRInt32 pseudoUnit;

  // update width
  pseudoUnit = (mWidthPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_WIDTH : mWidthPseudoUnit;
  UpdateValue(mWidthSign, pseudoUnit, mWidth,
              mBoundingMetrics, width);
  width = NS_MAX(0, width);

  // update "height" (this is the ascent in the terminology of the REC)
  pseudoUnit = (mHeightPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_HEIGHT : mHeightPseudoUnit;
  UpdateValue(mHeightSign, pseudoUnit, mHeight,
              mBoundingMetrics, height);
  height = NS_MAX(0, height);

  // update "depth" (this is the descent in the terminology of the REC)
  pseudoUnit = (mDepthPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_DEPTH : mDepthPseudoUnit;
  UpdateValue(mDepthSign, pseudoUnit, mDepth,
              mBoundingMetrics, depth);
  depth = NS_MAX(0, depth);

  // update lspace
  if (mLeftSpacePseudoUnit != NS_MATHML_PSEUDO_UNIT_ITSELF) {
    pseudoUnit = mLeftSpacePseudoUnit;
    UpdateValue(mLeftSpaceSign, pseudoUnit, mLeftSpace,
                mBoundingMetrics, lspace);
  }

  // update voffset
  if (mVerticalOffsetPseudoUnit != NS_MATHML_PSEUDO_UNIT_ITSELF) {
    pseudoUnit = mVerticalOffsetPseudoUnit;
    UpdateValue(mVerticalOffsetSign, pseudoUnit, mVerticalOffset,
                mBoundingMetrics, voffset);
  }
  // do the padding now that we have everything
  // The idea here is to maintain the invariant that <mpadded>...</mpadded> (i.e.,
  // with no attributes) looks the same as <mrow>...</mrow>. But when there are
  // attributes, tweak our metrics and move children to achieve the desired visual
  // effects.

  if (mLeftSpaceSign != NS_MATHML_SIGN_INVALID) { // there was padding on the left
    // dismiss the left italic correction now (so that our parent won't correct us)
    mBoundingMetrics.leftBearing = 0;
  }

  if (mWidthSign != NS_MATHML_SIGN_INVALID) { // there was padding on the right
    // dismiss the right italic correction now (so that our parent won't correct us)
    mBoundingMetrics.width = width;
    mBoundingMetrics.rightBearing = mBoundingMetrics.width;
  }

  nscoord dy = height - mBoundingMetrics.ascent;
  nscoord dx = lspace;

  aDesiredSize.ascent += dy;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.height += dy + depth - mBoundingMetrics.descent;
  mBoundingMetrics.ascent = height;
  mBoundingMetrics.descent = depth;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    // Finish reflowing child frames, positioning their origins.
    PositionRowChildFrames(dx, aDesiredSize.ascent - voffset);
  }

  return NS_OK;
}
