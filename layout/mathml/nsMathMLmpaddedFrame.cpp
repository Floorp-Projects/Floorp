/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsMathMLmpaddedFrame.h"
#include "nsMathMLElement.h"
#include "mozilla/gfx/2D.h"
#include <algorithm>

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

  // width
  mWidthSign = NS_MATHML_SIGN_INVALID;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::width, value);
  if (!value.IsEmpty()) {
    if (!ParseAttribute(value, mWidthSign, mWidth, mWidthPseudoUnit)) {
      ReportParseError(nsGkAtoms::width->GetUTF16String(), value.get());
    }
  }

  // height
  mHeightSign = NS_MATHML_SIGN_INVALID;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::height, value);
  if (!value.IsEmpty()) {
    if (!ParseAttribute(value, mHeightSign, mHeight, mHeightPseudoUnit)) {
      ReportParseError(nsGkAtoms::height->GetUTF16String(), value.get());
    }
  }

  // depth
  mDepthSign = NS_MATHML_SIGN_INVALID;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::depth_, value);
  if (!value.IsEmpty()) {
    if (!ParseAttribute(value, mDepthSign, mDepth, mDepthPseudoUnit)) {
      ReportParseError(nsGkAtoms::depth_->GetUTF16String(), value.get());
    }
  }

  // lspace
  mLeadingSpaceSign = NS_MATHML_SIGN_INVALID;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::lspace_, value);
  if (!value.IsEmpty()) {
    if (!ParseAttribute(value, mLeadingSpaceSign, mLeadingSpace,
                        mLeadingSpacePseudoUnit)) {
      ReportParseError(nsGkAtoms::lspace_->GetUTF16String(), value.get());
    }
  }

  // voffset
  mVerticalOffsetSign = NS_MATHML_SIGN_INVALID;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::voffset_, value);
  if (!value.IsEmpty()) {
    if (!ParseAttribute(value, mVerticalOffsetSign, mVerticalOffset,
                        mVerticalOffsetPseudoUnit)) {
      ReportParseError(nsGkAtoms::voffset_->GetUTF16String(), value.get());
    }
  }

}

// parse an input string in the following format (see bug 148326 for testcases):
// [+|-] unsigned-number (% [pseudo-unit] | pseudo-unit | css-unit | namedspace)
bool
nsMathMLmpaddedFrame::ParseAttribute(nsString&   aString,
                                     int32_t&    aSign,
                                     nsCSSValue& aCSSValue,
                                     int32_t&    aPseudoUnit)
{
  aCSSValue.Reset();
  aSign = NS_MATHML_SIGN_INVALID;
  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_UNSPECIFIED;
  aString.CompressWhitespace(); // aString is not a const in this code

  int32_t stringLength = aString.Length();
  if (!stringLength)
    return false;

  nsAutoString number, unit;

  //////////////////////
  // see if the sign is there

  int32_t i = 0;

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

  // get the number
  bool gotDot = false, gotPercent = false;
  for (; i < stringLength; i++) {
    char16_t c = aString[i];
    if (gotDot && c == '.') {
      // error - two dots encountered
      aSign = NS_MATHML_SIGN_INVALID;
      return false;
    }

    if (c == '.')
      gotDot = true;
    else if (!nsCRT::IsAsciiDigit(c)) {
      break;
    }
    number.Append(c);
  }

  // catch error if we didn't enter the loop above... we could simply initialize
  // floatValue = 1, to cater for cases such as width="height", but that wouldn't
  // be in line with the spec which requires an explicit number
  if (number.IsEmpty()) {
    aSign = NS_MATHML_SIGN_INVALID;
    return false;
  }

  nsresult errorCode;
  float floatValue = number.ToFloat(&errorCode);
  if (NS_FAILED(errorCode)) {
    aSign = NS_MATHML_SIGN_INVALID;
    return false;
  }

  // see if this is a percentage-based value
  if (i < stringLength && aString[i] == '%') {
    i++;
    gotPercent = true;
  }

  // the remainder now should be a css-unit, or a pseudo-unit, or a named-space
  aString.Right(unit, stringLength - i);

  if (unit.IsEmpty()) {
    if (gotPercent) {
      // case ["+"|"-"] unsigned-number "%"
      aCSSValue.SetPercentValue(floatValue / 100.0f);
      aPseudoUnit = NS_MATHML_PSEUDO_UNIT_ITSELF;
      return true;
    } else {
      // case ["+"|"-"] unsigned-number
      // XXXfredw: should we allow non-zero unitless values? See bug 757703.
      if (!floatValue) {
        aCSSValue.SetFloatValue(floatValue, eCSSUnit_Number);
        aPseudoUnit = NS_MATHML_PSEUDO_UNIT_ITSELF;
        return true;
      }
    }
  }
  else if (unit.EqualsLiteral("width"))  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_WIDTH;
  else if (unit.EqualsLiteral("height")) aPseudoUnit = NS_MATHML_PSEUDO_UNIT_HEIGHT;
  else if (unit.EqualsLiteral("depth"))  aPseudoUnit = NS_MATHML_PSEUDO_UNIT_DEPTH;
  else if (!gotPercent) { // percentage can only apply to a pseudo-unit

    // see if the unit is a named-space
    if (nsMathMLElement::ParseNamedSpaceValue(unit, aCSSValue,
                                              nsMathMLElement::
                                              PARSE_ALLOW_NEGATIVE)) {
      // re-scale properly, and we know that the unit of the named-space is 'em'
      floatValue *= aCSSValue.GetFloatValue();
      aCSSValue.SetFloatValue(floatValue, eCSSUnit_EM);
      aPseudoUnit = NS_MATHML_PSEUDO_UNIT_NAMEDSPACE;
      return true;
    }

    // see if the input was just a CSS value
    // We are not supposed to have a unitless, percent, negative or namedspace
    // value here.
    number.Append(unit); // leave the sign out if it was there
    if (nsMathMLElement::ParseNumericValue(number, aCSSValue,
                                           nsMathMLElement::
                                           PARSE_SUPPRESS_WARNINGS, nullptr))
      return true;
  }

  // if we enter here, we have a number that will act as a multiplier on a pseudo-unit
  if (aPseudoUnit != NS_MATHML_PSEUDO_UNIT_UNSPECIFIED) {
    if (gotPercent)
      aCSSValue.SetPercentValue(floatValue / 100.0f);
    else
      aCSSValue.SetFloatValue(floatValue, eCSSUnit_Number);

    return true;
  }


#ifdef DEBUG
  printf("mpadded: attribute with bad numeric value: %s\n",
          NS_LossyConvertUTF16toASCII(aString).get());
#endif
  // if we reach here, it means we encounter an unexpected input
  aSign = NS_MATHML_SIGN_INVALID;
  return false;
}

void
nsMathMLmpaddedFrame::UpdateValue(int32_t                  aSign,
                                  int32_t                  aPseudoUnit,
                                  const nsCSSValue&        aCSSValue,
                                  const ReflowOutput& aDesiredSize,
                                  nscoord&                 aValueToUpdate,
                                  float                aFontSizeInflation) const
{
  nsCSSUnit unit = aCSSValue.GetUnit();
  if (NS_MATHML_SIGN_INVALID != aSign && eCSSUnit_Null != unit) {
    nscoord scaler = 0, amount = 0;

    if (eCSSUnit_Percent == unit || eCSSUnit_Number == unit) {
      switch(aPseudoUnit) {
        case NS_MATHML_PSEUDO_UNIT_WIDTH:
             scaler = aDesiredSize.Width();
             break;

        case NS_MATHML_PSEUDO_UNIT_HEIGHT:
             scaler = aDesiredSize.BlockStartAscent();
             break;

        case NS_MATHML_PSEUDO_UNIT_DEPTH:
             scaler = aDesiredSize.Height() - aDesiredSize.BlockStartAscent();
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
      amount = CalcLength(PresContext(), mStyleContext, aCSSValue,
                          aFontSizeInflation);

    if (NS_MATHML_SIGN_PLUS == aSign)
      aValueToUpdate += amount;
    else if (NS_MATHML_SIGN_MINUS == aSign)
      aValueToUpdate -= amount;
    else
      aValueToUpdate  = amount;
  }
}

void
nsMathMLmpaddedFrame::Reflow(nsPresContext*          aPresContext,
                             ReflowOutput&     aDesiredSize,
                             const ReflowInput& aReflowInput,
                             nsReflowStatus&          aStatus)
{
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  mPresentationData.flags &= ~NS_MATHML_ERROR;
  ProcessAttributes();

  ///////////////
  // Let the base class format our content like an inferred mrow
  nsMathMLContainerFrame::Reflow(aPresContext, aDesiredSize,
                                 aReflowInput, aStatus);
  //NS_ASSERTION(aStatus.IsComplete(), "bad status");
}

/* virtual */ nsresult
nsMathMLmpaddedFrame::Place(DrawTarget*          aDrawTarget,
                            bool                 aPlaceOrigin,
                            ReflowOutput& aDesiredSize)
{
  nsresult rv =
    nsMathMLContainerFrame::Place(aDrawTarget, false, aDesiredSize);
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags) || NS_FAILED(rv)) {
    DidReflowChildren(PrincipalChildList().FirstChild());
    return rv;
  }

  nscoord height = aDesiredSize.BlockStartAscent();
  nscoord depth  = aDesiredSize.Height() - aDesiredSize.BlockStartAscent();
  // The REC says:
  //
  // "The lspace attribute ('leading' space) specifies the horizontal location
  // of the positioning point of the child content with respect to the
  // positioning point of the mpadded element. By default they coincide, and
  // therefore absolute values for lspace have the same effect as relative
  // values."
  //
  // "MathML renderers should ensure that, except for the effects of the
  // attributes, the relative spacing between the contents of the mpadded
  // element and surrounding MathML elements would not be modified by replacing
  // an mpadded element with an mrow element with the same content, even if
  // linebreaking occurs within the mpadded element."
  //
  // (http://www.w3.org/TR/MathML/chapter3.html#presm.mpadded)
  //
  // "In those discussions, the terms leading and trailing are used to specify
  // a side of an object when which side to use depends on the directionality;
  // ie. leading means left in LTR but right in RTL."
  // (http://www.w3.org/TR/MathML/chapter3.html#presm.bidi.math)
  nscoord lspace = 0;
  // In MathML3, "width" will be the bounding box width and "advancewidth" will
  // refer "to the horizontal distance between the positioning point of the
  // mpadded and the positioning point for the following content".  MathML2
  // doesn't make the distinction.
  nscoord width  = aDesiredSize.Width();
  nscoord voffset = 0;

  int32_t pseudoUnit;
  nscoord initialWidth = width;
  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);

  // update width
  pseudoUnit = (mWidthPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_WIDTH : mWidthPseudoUnit;
  UpdateValue(mWidthSign, pseudoUnit, mWidth,
              aDesiredSize, width, fontSizeInflation);
  width = std::max(0, width);

  // update "height" (this is the ascent in the terminology of the REC)
  pseudoUnit = (mHeightPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_HEIGHT : mHeightPseudoUnit;
  UpdateValue(mHeightSign, pseudoUnit, mHeight,
              aDesiredSize, height, fontSizeInflation);
  height = std::max(0, height);

  // update "depth" (this is the descent in the terminology of the REC)
  pseudoUnit = (mDepthPseudoUnit == NS_MATHML_PSEUDO_UNIT_ITSELF)
             ? NS_MATHML_PSEUDO_UNIT_DEPTH : mDepthPseudoUnit;
  UpdateValue(mDepthSign, pseudoUnit, mDepth,
              aDesiredSize, depth, fontSizeInflation);
  depth = std::max(0, depth);

  // update lspace
  if (mLeadingSpacePseudoUnit != NS_MATHML_PSEUDO_UNIT_ITSELF) {
    pseudoUnit = mLeadingSpacePseudoUnit;
    UpdateValue(mLeadingSpaceSign, pseudoUnit, mLeadingSpace,
                aDesiredSize, lspace, fontSizeInflation);
  }

  // update voffset
  if (mVerticalOffsetPseudoUnit != NS_MATHML_PSEUDO_UNIT_ITSELF) {
    pseudoUnit = mVerticalOffsetPseudoUnit;
    UpdateValue(mVerticalOffsetSign, pseudoUnit, mVerticalOffset,
                aDesiredSize, voffset, fontSizeInflation);
  }
  // do the padding now that we have everything
  // The idea here is to maintain the invariant that <mpadded>...</mpadded> (i.e.,
  // with no attributes) looks the same as <mrow>...</mrow>. But when there are
  // attributes, tweak our metrics and move children to achieve the desired visual
  // effects.

  if ((StyleVisibility()->mDirection ?
       mWidthSign : mLeadingSpaceSign) != NS_MATHML_SIGN_INVALID) {
    // there was padding on the left. dismiss the left italic correction now
    // (so that our parent won't correct us)
    mBoundingMetrics.leftBearing = 0;
  }

  if ((StyleVisibility()->mDirection ?
       mLeadingSpaceSign : mWidthSign) != NS_MATHML_SIGN_INVALID) {
    // there was padding on the right. dismiss the right italic correction now
    // (so that our parent won't correct us)
    mBoundingMetrics.width = width;
    mBoundingMetrics.rightBearing = mBoundingMetrics.width;
  }

  nscoord dx = (StyleVisibility()->mDirection ?
                width - initialWidth - lspace : lspace);

  aDesiredSize.SetBlockStartAscent(height);
  aDesiredSize.Width() = mBoundingMetrics.width;
  aDesiredSize.Height() = depth + aDesiredSize.BlockStartAscent();
  mBoundingMetrics.ascent = height;
  mBoundingMetrics.descent = depth;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.BlockStartAscent();

  if (aPlaceOrigin) {
    // Finish reflowing child frames, positioning their origins.
    PositionRowChildFrames(dx, aDesiredSize.BlockStartAscent() - voffset);
  }

  return NS_OK;
}

/* virtual */ nsresult
nsMathMLmpaddedFrame::MeasureForWidth(DrawTarget* aDrawTarget,
                                      ReflowOutput& aDesiredSize)
{
  ProcessAttributes();
  return Place(aDrawTarget, false, aDesiredSize);
}
