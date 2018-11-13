/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStyleUtil.h"
#include "nsStyleConsts.h"

#include "mozilla/FontPropertyTypes.h"
#include "nsIContent.h"
#include "nsCSSProps.h"
#include "nsContentUtils.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsStyleStruct.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIURI.h"
#include "nsISupportsPrimitives.h"
#include "nsLayoutUtils.h"
#include "nsPrintfCString.h"
#include <cctype>

using namespace mozilla;

//------------------------------------------------------------------------------
// Font Algorithm Code
//------------------------------------------------------------------------------

// Compare two language strings
bool nsStyleUtil::DashMatchCompare(const nsAString& aAttributeValue,
                                     const nsAString& aSelectorValue,
                                     const nsStringComparator& aComparator)
{
  bool result;
  uint32_t selectorLen = aSelectorValue.Length();
  uint32_t attributeLen = aAttributeValue.Length();
  if (selectorLen > attributeLen) {
    result = false;
  }
  else {
    nsAString::const_iterator iter;
    if (selectorLen != attributeLen &&
        *aAttributeValue.BeginReading(iter).advance(selectorLen) !=
            char16_t('-')) {
      // to match, the aAttributeValue must have a dash after the end of
      // the aSelectorValue's text (unless the aSelectorValue and the
      // aAttributeValue have the same text)
      result = false;
    }
    else {
      result = StringBeginsWith(aAttributeValue, aSelectorValue, aComparator);
    }
  }
  return result;
}

bool
nsStyleUtil::ValueIncludes(const nsAString& aValueList,
                           const nsAString& aValue,
                           const nsStringComparator& aComparator)
{
  const char16_t *p = aValueList.BeginReading(),
              *p_end = aValueList.EndReading();

  while (p < p_end) {
    // skip leading space
    while (p != p_end && nsContentUtils::IsHTMLWhitespace(*p))
      ++p;

    const char16_t *val_start = p;

    // look for space or end
    while (p != p_end && !nsContentUtils::IsHTMLWhitespace(*p))
      ++p;

    const char16_t *val_end = p;

    if (val_start < val_end &&
        aValue.Equals(Substring(val_start, val_end), aComparator))
      return true;

    ++p; // we know the next character is not whitespace
  }
  return false;
}

void nsStyleUtil::AppendEscapedCSSString(const nsAString& aString,
                                         nsAString& aReturn,
                                         char16_t quoteChar)
{
  MOZ_ASSERT(quoteChar == '\'' || quoteChar == '"',
             "CSS strings must be quoted with ' or \"");

  aReturn.Append(quoteChar);

  const char16_t* in = aString.BeginReading();
  const char16_t* const end = aString.EndReading();
  for (; in != end; in++) {
    if (*in < 0x20 || *in == 0x7F) {
      // Escape U+0000 through U+001F and U+007F numerically.
      aReturn.AppendPrintf("\\%x ", *in);
    } else {
      if (*in == '"' || *in == '\'' || *in == '\\') {
        // Escape backslash and quote characters symbolically.
        // It's not technically necessary to escape the quote
        // character that isn't being used to delimit the string,
        // but we do it anyway because that makes testing simpler.
        aReturn.Append(char16_t('\\'));
      }
      aReturn.Append(*in);
    }
  }

  aReturn.Append(quoteChar);
}

/* static */ void
nsStyleUtil::AppendEscapedCSSIdent(const nsAString& aIdent, nsAString& aReturn)
{
  // The relevant parts of the CSS grammar are:
  //   ident    ([-]?{nmstart}|[-][-]){nmchar}*
  //   nmstart  [_a-z]|{nonascii}|{escape}
  //   nmchar   [_a-z0-9-]|{nonascii}|{escape}
  //   nonascii [^\0-\177]
  //   escape   {unicode}|\\[^\n\r\f0-9a-f]
  //   unicode  \\[0-9a-f]{1,6}(\r\n|[ \n\r\t\f])?
  // from http://www.w3.org/TR/CSS21/syndata.html#tokenization but
  // modified for idents by
  // http://dev.w3.org/csswg/cssom/#serialize-an-identifier and
  // http://dev.w3.org/csswg/css-syntax/#would-start-an-identifier

  const char16_t* in = aIdent.BeginReading();
  const char16_t* const end = aIdent.EndReading();

  if (in == end)
    return;

  // A leading dash does not need to be escaped as long as it is not the
  // *only* character in the identifier.
  if (*in == '-') {
    if (in + 1 == end) {
      aReturn.Append(char16_t('\\'));
      aReturn.Append(char16_t('-'));
      return;
    }

    aReturn.Append(char16_t('-'));
    ++in;
  }

  // Escape a digit at the start (including after a dash),
  // numerically.  If we didn't escape it numerically, it would get
  // interpreted as a numeric escape for the wrong character.
  if (in != end && ('0' <= *in && *in <= '9')) {
    aReturn.AppendPrintf("\\%x ", *in);
    ++in;
  }

  for (; in != end; ++in) {
    char16_t ch = *in;
    if (ch == 0x00) {
      aReturn.Append(char16_t(0xFFFD));
    } else if (ch < 0x20 || 0x7F == ch) {
      // Escape U+0000 through U+001F and U+007F numerically.
      aReturn.AppendPrintf("\\%x ", *in);
    } else {
      // Escape ASCII non-identifier printables as a backslash plus
      // the character.
      if (ch < 0x7F &&
          ch != '_' && ch != '-' &&
          (ch < '0' || '9' < ch) &&
          (ch < 'A' || 'Z' < ch) &&
          (ch < 'a' || 'z' < ch)) {
        aReturn.Append(char16_t('\\'));
      }
      aReturn.Append(ch);
    }
  }
}

/* static */ void
nsStyleUtil::AppendBitmaskCSSValue(const nsCSSKTableEntry aTable[],
                                   int32_t aMaskedValue,
                                   int32_t aFirstMask,
                                   int32_t aLastMask,
                                   nsAString& aResult)
{
  for (int32_t mask = aFirstMask; mask <= aLastMask; mask <<= 1) {
    if (mask & aMaskedValue) {
      AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(mask, aTable), aResult);
      aMaskedValue &= ~mask;
      if (aMaskedValue) { // more left
        aResult.Append(char16_t(' '));
      }
    }
  }
  MOZ_ASSERT(aMaskedValue == 0, "unexpected bit remaining in bitfield");
}

/* static */ void
nsStyleUtil::AppendAngleValue(const nsStyleCoord& aAngle, nsAString& aResult)
{
  MOZ_ASSERT(aAngle.IsAngleValue(), "Should have angle value");

  // Append number.
  AppendCSSNumber(aAngle.GetAngleValue(), aResult);

  // Append unit.
  switch (aAngle.GetUnit()) {
    case eStyleUnit_Degree: aResult.AppendLiteral("deg");  break;
    case eStyleUnit_Grad:   aResult.AppendLiteral("grad"); break;
    case eStyleUnit_Radian: aResult.AppendLiteral("rad");  break;
    case eStyleUnit_Turn:   aResult.AppendLiteral("turn"); break;
    default: MOZ_ASSERT_UNREACHABLE("unrecognized angle unit");
  }
}

/* static */ void
nsStyleUtil::AppendPaintOrderValue(uint8_t aValue,
                                   nsAString& aResult)
{
  static_assert
    (NS_STYLE_PAINT_ORDER_BITWIDTH * NS_STYLE_PAINT_ORDER_LAST_VALUE <= 8,
     "SVGStyleStruct::mPaintOrder and local variables not big enough");

  if (aValue == NS_STYLE_PAINT_ORDER_NORMAL) {
    aResult.AppendLiteral("normal");
    return;
  }

  // Append the minimal value necessary for the given paint order.
  static_assert(NS_STYLE_PAINT_ORDER_LAST_VALUE == 3,
                "paint-order values added; check serialization");

  // The following relies on the default order being the order of the
  // constant values.

  const uint8_t MASK = (1 << NS_STYLE_PAINT_ORDER_BITWIDTH) - 1;

  uint32_t lastPositionToSerialize = 0;
  for (uint32_t position = NS_STYLE_PAINT_ORDER_LAST_VALUE - 1;
       position > 0;
       position--) {
    uint8_t component =
      (aValue >> (position * NS_STYLE_PAINT_ORDER_BITWIDTH)) & MASK;
    uint8_t earlierComponent =
      (aValue >> ((position - 1) * NS_STYLE_PAINT_ORDER_BITWIDTH)) & MASK;
    if (component < earlierComponent) {
      lastPositionToSerialize = position - 1;
      break;
    }
  }

  for (uint32_t position = 0; position <= lastPositionToSerialize; position++) {
    if (position > 0) {
      aResult.Append(' ');
    }
    uint8_t component = aValue & MASK;
    switch (component) {
      case NS_STYLE_PAINT_ORDER_FILL:
        aResult.AppendLiteral("fill");
        break;

      case NS_STYLE_PAINT_ORDER_STROKE:
        aResult.AppendLiteral("stroke");
        break;

      case NS_STYLE_PAINT_ORDER_MARKERS:
        aResult.AppendLiteral("markers");
        break;

      default:
        MOZ_ASSERT_UNREACHABLE("unexpected paint-order component value");
    }
    aValue >>= NS_STYLE_PAINT_ORDER_BITWIDTH;
  }
}

/* static */ void
nsStyleUtil::AppendStepsTimingFunction(nsTimingFunction::Type aType,
                                       uint32_t aSteps,
                                       nsAString& aResult)
{
  MOZ_ASSERT(aType == nsTimingFunction::Type::StepStart ||
             aType == nsTimingFunction::Type::StepEnd);

  aResult.AppendLiteral("steps(");
  aResult.AppendInt(aSteps);
  if (aType == nsTimingFunction::Type::StepStart) {
    aResult.AppendLiteral(", start)");
  } else {
    aResult.AppendLiteral(")");
  }
}

/* static */ void
nsStyleUtil::AppendFramesTimingFunction(uint32_t aFrames,
                                        nsAString& aResult)
{
  aResult.AppendLiteral("frames(");
  aResult.AppendInt(aFrames);
  aResult.AppendLiteral(")");
}

/* static */ void
nsStyleUtil::AppendCubicBezierTimingFunction(float aX1, float aY1,
                                             float aX2, float aY2,
                                             nsAString& aResult)
{
  // set the value from the cubic-bezier control points
  // (We could try to regenerate the keywords if we want.)
  aResult.AppendLiteral("cubic-bezier(");
  aResult.AppendFloat(aX1);
  aResult.AppendLiteral(", ");
  aResult.AppendFloat(aY1);
  aResult.AppendLiteral(", ");
  aResult.AppendFloat(aX2);
  aResult.AppendLiteral(", ");
  aResult.AppendFloat(aY2);
  aResult.Append(')');
}

/* static */ void
nsStyleUtil::AppendCubicBezierKeywordTimingFunction(
    nsTimingFunction::Type aType,
    nsAString& aResult)
{
  switch (aType) {
    case nsTimingFunction::Type::Ease:
    case nsTimingFunction::Type::Linear:
    case nsTimingFunction::Type::EaseIn:
    case nsTimingFunction::Type::EaseOut:
    case nsTimingFunction::Type::EaseInOut: {
      nsCSSKeyword keyword = nsCSSProps::ValueToKeywordEnum(
          static_cast<int32_t>(aType),
          nsCSSProps::kTransitionTimingFunctionKTable);
      AppendASCIItoUTF16(nsCSSKeywords::GetStringValue(keyword),
                         aResult);
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected aType");
      break;
  }
}

/* static */ float
nsStyleUtil::ColorComponentToFloat(uint8_t aAlpha)
{
  // Alpha values are expressed as decimals, so we should convert
  // back, using as few decimal places as possible for
  // round-tripping.
  // First try two decimal places:
  float rounded = NS_roundf(float(aAlpha) * 100.0f / 255.0f) / 100.0f;
  if (FloatToColorComponent(rounded) != aAlpha) {
    // Use three decimal places.
    rounded = NS_roundf(float(aAlpha) * 1000.0f / 255.0f) / 1000.0f;
  }
  return rounded;
}

/* static */ bool
nsStyleUtil::IsSignificantChild(nsIContent* aChild,
                                bool aWhitespaceIsSignificant)
{
  bool isText = aChild->IsText();

  if (!isText && !aChild->IsComment() && !aChild->IsProcessingInstruction()) {
    return true;
  }

  return isText && aChild->TextLength() != 0 &&
         (aWhitespaceIsSignificant ||
          !aChild->TextIsOnlyWhitespace());
}

/* static */ bool
nsStyleUtil::ThreadSafeIsSignificantChild(const nsIContent* aChild,
                                          bool aWhitespaceIsSignificant)
{
  bool isText = aChild->IsText();

  if (!isText && !aChild->IsComment() && !aChild->IsProcessingInstruction()) {
    return true;
  }

  return isText && aChild->TextLength() != 0 &&
         (aWhitespaceIsSignificant ||
          !aChild->ThreadSafeTextIsOnlyWhitespace());
}

// For a replaced element whose concrete object size is no larger than the
// element's content-box, this method checks whether the given
// "object-position" coordinate might cause overflow in its dimension.
static bool
ObjectPositionCoordMightCauseOverflow(const Position::Coord& aCoord)
{
  // Any nonzero length in "object-position" can push us to overflow
  // (particularly if our concrete object size is exactly the same size as the
  // replaced element's content-box).
  if (aCoord.mLength != 0) {
    return true;
  }

  // Percentages are interpreted as a fraction of the extra space. So,
  // percentages in the 0-100% range are safe, but values outside of that
  // range could cause overflow.
  if (aCoord.mHasPercent &&
      (aCoord.mPercent < 0.0f || aCoord.mPercent > 1.0f)) {
    return true;
  }
  return false;
}


/* static */ bool
nsStyleUtil::ObjectPropsMightCauseOverflow(const nsStylePosition* aStylePos)
{
  auto objectFit = aStylePos->mObjectFit;

  // "object-fit: cover" & "object-fit: none" can give us a render rect that's
  // larger than our container element's content-box.
  if (objectFit == NS_STYLE_OBJECT_FIT_COVER ||
      objectFit == NS_STYLE_OBJECT_FIT_NONE) {
    return true;
  }
  // (All other object-fit values produce a concrete object size that's no larger
  // than the constraint region.)

  // Check each of our "object-position" coords to see if it could cause
  // overflow in its dimension:
  const Position& objectPosistion = aStylePos->mObjectPosition;
  if (ObjectPositionCoordMightCauseOverflow(objectPosistion.mXPosition) ||
      ObjectPositionCoordMightCauseOverflow(objectPosistion.mYPosition)) {
    return true;
  }

  return false;
}


/* static */ bool
nsStyleUtil::CSPAllowsInlineStyle(Element* aElement,
                                  nsIPrincipal* aPrincipal,
                                  nsIPrincipal* aTriggeringPrincipal,
                                  nsIURI* aSourceURI,
                                  uint32_t aLineNumber,
                                  uint32_t aColumnNumber,
                                  const nsAString& aStyleText,
                                  nsresult* aRv)
{
  nsresult rv;

  if (aRv) {
    *aRv = NS_OK;
  }

  nsIPrincipal* principal = aPrincipal;
  if (aTriggeringPrincipal &&
      BasePrincipal::Cast(aTriggeringPrincipal)->OverridesCSP(aPrincipal)) {
    principal = aTriggeringPrincipal;
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = principal->GetCsp(getter_AddRefs(csp));

  if (NS_FAILED(rv)) {
    if (aRv)
      *aRv = rv;
    return false;
  }

  if (!csp) {
    // No CSP --> the style is allowed
    return true;
  }

  // query the nonce
  nsAutoString nonce;
  if (aElement && aElement->NodeInfo()->NameAtom() == nsGkAtoms::style) {
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::nonce, nonce);
  }

  bool allowInlineStyle = true;
  rv = csp->GetAllowsInline(nsIContentPolicy::TYPE_STYLESHEET,
                            nonce,
                            false, // aParserCreated only applies to scripts
                            aElement, aStyleText, aLineNumber, aColumnNumber,
                            &allowInlineStyle);
  NS_ENSURE_SUCCESS(rv, false);

  return allowInlineStyle;
}

void
nsStyleUtil::AppendFontSlantStyle(const FontSlantStyle& aStyle, nsAString& aOut)
{
  if (aStyle.IsNormal()) {
    aOut.AppendLiteral("normal");
  } else if (aStyle.IsItalic()) {
    aOut.AppendLiteral("italic");
  } else {
    aOut.AppendLiteral("oblique");
    auto angle = aStyle.ObliqueAngle();
    if (angle != FontSlantStyle::kDefaultAngle) {
      aOut.AppendLiteral(" ");
      AppendAngleValue(nsStyleCoord(angle, eStyleUnit_Degree), aOut);
    }
  }
}
