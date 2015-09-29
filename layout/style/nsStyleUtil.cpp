/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStyleUtil.h"
#include "nsStyleConsts.h"

#include "nsIContent.h"
#include "nsCSSProps.h"
#include "nsRuleNode.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsStyleStruct.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIURI.h"
#include "nsPrintfCString.h"

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

void nsStyleUtil::AppendEscapedCSSString(const nsAString& aString,
                                         nsAString& aReturn,
                                         char16_t quoteChar)
{
  NS_PRECONDITION(quoteChar == '\'' || quoteChar == '"',
                  "CSS strings must be quoted with ' or \"");
  aReturn.Append(quoteChar);

  const char16_t* in = aString.BeginReading();
  const char16_t* const end = aString.EndReading();
  for (; in != end; in++) {
    if (*in < 0x20 || (*in >= 0x7F && *in < 0xA0)) {
      // Escape U+0000 through U+001F and U+007F through U+009F numerically.
      aReturn.AppendPrintf("\\%hx ", *in);
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

/* static */ bool
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
    return true;

  // A leading dash does not need to be escaped as long as it is not the
  // *only* character in the identifier.
  if (*in == '-') {
    if (in + 1 == end) {
      aReturn.Append(char16_t('\\'));
      aReturn.Append(char16_t('-'));
      return true;
    }

    aReturn.Append(char16_t('-'));
    ++in;
  }

  // Escape a digit at the start (including after a dash),
  // numerically.  If we didn't escape it numerically, it would get
  // interpreted as a numeric escape for the wrong character.
  if (in != end && ('0' <= *in && *in <= '9')) {
    aReturn.AppendPrintf("\\%hx ", *in);
    ++in;
  }

  for (; in != end; ++in) {
    char16_t ch = *in;
    if (ch == 0x00) {
      return false;
    }
    if (ch < 0x20 || (0x7F <= ch && ch < 0xA0)) {
      // Escape U+0000 through U+001F and U+007F through U+009F numerically.
      aReturn.AppendPrintf("\\%hx ", *in);
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
  return true;
}

// unquoted family names must be a sequence of idents
// so escape any parts that require escaping
static void
AppendUnquotedFamilyName(const nsAString& aFamilyName, nsAString& aResult)
{
  const char16_t *p, *p_end;
  aFamilyName.BeginReading(p);
  aFamilyName.EndReading(p_end);

   bool moreThanOne = false;
   while (p < p_end) {
     const char16_t* identStart = p;
     while (++p != p_end && *p != ' ')
       /* nothing */ ;

     nsDependentSubstring ident(identStart, p);
     if (!ident.IsEmpty()) {
       if (moreThanOne) {
         aResult.Append(' ');
       }
       nsStyleUtil::AppendEscapedCSSIdent(ident, aResult);
       moreThanOne = true;
     }

     ++p;
  }
}

/* static */ void
nsStyleUtil::AppendEscapedCSSFontFamilyList(
  const mozilla::FontFamilyList& aFamilyList,
  nsAString& aResult)
{
  const nsTArray<FontFamilyName>& fontlist = aFamilyList.GetFontlist();
  size_t i, len = fontlist.Length();
  for (i = 0; i < len; i++) {
    if (i != 0) {
      aResult.Append(',');
    }
    const FontFamilyName& name = fontlist[i];
    switch (name.mType) {
      case eFamily_named:
        AppendUnquotedFamilyName(name.mName, aResult);
        break;
      case eFamily_named_quoted:
        AppendEscapedCSSString(name.mName, aResult);
        break;
      default:
        name.AppendToString(aResult);
    }
  }
}


/* static */ void
nsStyleUtil::AppendBitmaskCSSValue(nsCSSProperty aProperty,
                                   int32_t aMaskedValue,
                                   int32_t aFirstMask,
                                   int32_t aLastMask,
                                   nsAString& aResult)
{
  for (int32_t mask = aFirstMask; mask <= aLastMask; mask <<= 1) {
    if (mask & aMaskedValue) {
      AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, mask),
                         aResult);
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
    default: NS_NOTREACHED("unrecognized angle unit");
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
        NS_NOTREACHED("unexpected paint-order component value");
    }
    aValue >>= NS_STYLE_PAINT_ORDER_BITWIDTH;
  }
}

/* static */ void
nsStyleUtil::AppendFontFeatureSettings(const nsTArray<gfxFontFeature>& aFeatures,
                                       nsAString& aResult)
{
  for (uint32_t i = 0, numFeat = aFeatures.Length(); i < numFeat; i++) {
    const gfxFontFeature& feat = aFeatures[i];

    if (i != 0) {
        aResult.AppendLiteral(", ");
    }

    // output tag
    char tag[7];
    tag[0] = '"';
    tag[1] = (feat.mTag >> 24) & 0xff;
    tag[2] = (feat.mTag >> 16) & 0xff;
    tag[3] = (feat.mTag >> 8) & 0xff;
    tag[4] = feat.mTag & 0xff;
    tag[5] = '"';
    tag[6] = 0;
    aResult.AppendASCII(tag);

    // output value, if necessary
    if (feat.mValue == 0) {
      // 0 ==> off
      aResult.AppendLiteral(" off");
    } else if (feat.mValue > 1) {
      aResult.Append(' ');
      aResult.AppendInt(feat.mValue);
    }
    // else, omit value if 1, implied by default
  }
}

/* static */ void
nsStyleUtil::AppendFontFeatureSettings(const nsCSSValue& aSrc,
                                       nsAString& aResult)
{
  nsCSSUnit unit = aSrc.GetUnit();

  if (unit == eCSSUnit_Normal) {
    aResult.AppendLiteral("normal");
    return;
  }

  NS_PRECONDITION(unit == eCSSUnit_PairList || unit == eCSSUnit_PairListDep,
                  "improper value unit for font-feature-settings:");

  nsTArray<gfxFontFeature> featureSettings;
  nsRuleNode::ComputeFontFeatures(aSrc.GetPairListValue(), featureSettings);
  AppendFontFeatureSettings(featureSettings, aResult);
}

/* static */ void
nsStyleUtil::GetFunctionalAlternatesName(int32_t aFeature,
                                         nsAString& aFeatureName)
{
  aFeatureName.Truncate();
  nsCSSKeyword key =
    nsCSSProps::ValueToKeywordEnum(aFeature,
                           nsCSSProps::kFontVariantAlternatesFuncsKTable);

  NS_ASSERTION(key != eCSSKeyword_UNKNOWN, "bad alternate feature type");
  AppendUTF8toUTF16(nsCSSKeywords::GetStringValue(key), aFeatureName);
}

/* static */ void
nsStyleUtil::SerializeFunctionalAlternates(
    const nsTArray<gfxAlternateValue>& aAlternates,
    nsAString& aResult)
{
  nsAutoString funcName, funcParams;
  uint32_t numValues = aAlternates.Length();

  uint32_t feature = 0;
  for (uint32_t i = 0; i < numValues; i++) {
    const gfxAlternateValue& v = aAlternates.ElementAt(i);
    if (feature != v.alternate) {
      feature = v.alternate;
      if (!funcName.IsEmpty() && !funcParams.IsEmpty()) {
        if (!aResult.IsEmpty()) {
          aResult.Append(char16_t(' '));
        }

        // append the previous functional value
        aResult.Append(funcName);
        aResult.Append(char16_t('('));
        aResult.Append(funcParams);
        aResult.Append(char16_t(')'));
      }

      // function name
      GetFunctionalAlternatesName(v.alternate, funcName);
      NS_ASSERTION(!funcName.IsEmpty(), "unknown property value name");

      // function params
      funcParams.Truncate();
      AppendEscapedCSSIdent(v.value, funcParams);
    } else {
      if (!funcParams.IsEmpty()) {
        funcParams.AppendLiteral(", ");
      }
      AppendEscapedCSSIdent(v.value, funcParams);
    }
  }

    // append the previous functional value
  if (!funcName.IsEmpty() && !funcParams.IsEmpty()) {
    if (!aResult.IsEmpty()) {
      aResult.Append(char16_t(' '));
    }

    aResult.Append(funcName);
    aResult.Append(char16_t('('));
    aResult.Append(funcParams);
    aResult.Append(char16_t(')'));
  }
}

/* static */ void
nsStyleUtil::ComputeFunctionalAlternates(const nsCSSValueList* aList,
                                  nsTArray<gfxAlternateValue>& aAlternateValues)
{
  gfxAlternateValue v;

  aAlternateValues.Clear();
  for (const nsCSSValueList* curr = aList; curr != nullptr; curr = curr->mNext) {
    // list contains function units
    if (curr->mValue.GetUnit() != eCSSUnit_Function) {
      continue;
    }

    // element 0 is the propval in ident form
    const nsCSSValue::Array *func = curr->mValue.GetArrayValue();

    // lookup propval
    nsCSSKeyword key = func->Item(0).GetKeywordValue();
    NS_ASSERTION(key != eCSSKeyword_UNKNOWN, "unknown alternate property value");

    int32_t alternate;
    if (key == eCSSKeyword_UNKNOWN ||
        !nsCSSProps::FindKeyword(key,
                                 nsCSSProps::kFontVariantAlternatesFuncsKTable,
                                 alternate)) {
      NS_NOTREACHED("keyword not a font-variant-alternates value");
      continue;
    }
    v.alternate = alternate;

    // other elements are the idents associated with the propval
    // append one alternate value for each one
    uint32_t numElems = func->Count();
    for (uint32_t i = 1; i < numElems; i++) {
      const nsCSSValue& value = func->Item(i);
      NS_ASSERTION(value.GetUnit() == eCSSUnit_Ident,
                   "weird unit found in variant alternate");
      if (value.GetUnit() != eCSSUnit_Ident) {
        continue;
      }
      value.GetStringValue(v.value);
      aAlternateValues.AppendElement(v);
    }
  }
}

static void
AppendSerializedUnicodePoint(uint32_t aCode, nsACString& aBuf)
{
  aBuf.Append(nsPrintfCString("%0X", aCode));
}

// A unicode-range: descriptor is represented as an array of integers,
// to be interpreted as a sequence of pairs: min max min max ...
// It is in source order.  (Possibly it should be sorted and overlaps
// consolidated, but right now we don't do that.)
/* static */ void
nsStyleUtil::AppendUnicodeRange(const nsCSSValue& aValue, nsAString& aResult)
{
  NS_PRECONDITION(aValue.GetUnit() == eCSSUnit_Null ||
                  aValue.GetUnit() == eCSSUnit_Array,
                  "improper value unit for unicode-range:");
  aResult.Truncate();
  if (aValue.GetUnit() != eCSSUnit_Array)
    return;

  nsCSSValue::Array const & sources = *aValue.GetArrayValue();
  nsAutoCString buf;

  MOZ_ASSERT(sources.Count() % 2 == 0,
             "odd number of entries in a unicode-range: array");

  for (uint32_t i = 0; i < sources.Count(); i += 2) {
    uint32_t min = sources[i].GetIntValue();
    uint32_t max = sources[i+1].GetIntValue();

    // We don't try to replicate the U+XX?? notation.
    buf.AppendLiteral("U+");
    AppendSerializedUnicodePoint(min, buf);

    if (min != max) {
      buf.Append('-');
      AppendSerializedUnicodePoint(max, buf);
    }
    buf.AppendLiteral(", ");
  }
  buf.Truncate(buf.Length() - 2); // remove the last comma-space
  CopyASCIItoUTF16(buf, aResult);
}

/* static */ void
nsStyleUtil::AppendSerializedFontSrc(const nsCSSValue& aValue,
                                     nsAString& aResult)
{
  // A src: descriptor is represented as an array value; each entry in
  // the array can be eCSSUnit_URL, eCSSUnit_Local_Font, or
  // eCSSUnit_Font_Format.  Blocks of eCSSUnit_Font_Format may appear
  // only after one of the first two.  (css3-fonts only contemplates
  // annotating URLs with formats, but we handle the general case.)

  NS_PRECONDITION(aValue.GetUnit() == eCSSUnit_Array,
                  "improper value unit for src:");

  const nsCSSValue::Array& sources = *aValue.GetArrayValue();
  size_t i = 0;

  while (i < sources.Count()) {
    nsAutoString formats;

    if (sources[i].GetUnit() == eCSSUnit_URL) {
      aResult.AppendLiteral("url(");
      nsDependentString url(sources[i].GetOriginalURLValue());
      nsStyleUtil::AppendEscapedCSSString(url, aResult);
      aResult.Append(')');
    } else if (sources[i].GetUnit() == eCSSUnit_Local_Font) {
      aResult.AppendLiteral("local(");
      nsDependentString local(sources[i].GetStringBufferValue());
      nsStyleUtil::AppendEscapedCSSString(local, aResult);
      aResult.Append(')');
    } else {
      NS_NOTREACHED("entry in src: descriptor with improper unit");
      i++;
      continue;
    }

    i++;
    formats.Truncate();
    while (i < sources.Count() &&
           sources[i].GetUnit() == eCSSUnit_Font_Format) {
      formats.Append('"');
      formats.Append(sources[i].GetStringBufferValue());
      formats.AppendLiteral("\", ");
      i++;
    }
    if (formats.Length() > 0) {
      formats.Truncate(formats.Length() - 2); // remove the last comma
      aResult.AppendLiteral(" format(");
      aResult.Append(formats);
      aResult.Append(')');
    }
    aResult.AppendLiteral(", ");
  }
  aResult.Truncate(aResult.Length() - 2); // remove the last comma-space
}

/* static */ void
nsStyleUtil::AppendStepsTimingFunction(nsTimingFunction::Type aType,
                                       uint32_t aSteps,
                                       nsTimingFunction::StepSyntax aSyntax,
                                       nsAString& aResult)
{
  MOZ_ASSERT(aType == nsTimingFunction::Type::StepStart ||
             aType == nsTimingFunction::Type::StepEnd);

  if (aSyntax == nsTimingFunction::StepSyntax::Keyword) {
    if (aType == nsTimingFunction::Type::StepStart) {
      aResult.AppendLiteral("step-start");
    } else {
      aResult.AppendLiteral("step-end");
    }
    return;
  }

  aResult.AppendLiteral("steps(");
  aResult.AppendInt(aSteps);
  switch (aSyntax) {
    case nsTimingFunction::StepSyntax::Keyword:
      // handled above
      break;
    case nsTimingFunction::StepSyntax::FunctionalWithStartKeyword:
      aResult.AppendLiteral(", start)");
      break;
    case nsTimingFunction::StepSyntax::FunctionalWithEndKeyword:
      aResult.AppendLiteral(", end)");
      break;
    case nsTimingFunction::StepSyntax::FunctionalWithoutKeyword:
      aResult.Append(')');
      break;
  }
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
nsStyleUtil::IsSignificantChild(nsIContent* aChild, bool aTextIsSignificant,
                                bool aWhitespaceIsSignificant)
{
  NS_ASSERTION(!aWhitespaceIsSignificant || aTextIsSignificant,
               "Nonsensical arguments");

  bool isText = aChild->IsNodeOfType(nsINode::eTEXT);

  if (!isText && !aChild->IsNodeOfType(nsINode::eCOMMENT) &&
      !aChild->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
    return true;
  }

  return aTextIsSignificant && isText && aChild->TextLength() != 0 &&
         (aWhitespaceIsSignificant ||
          !aChild->TextIsOnlyWhitespace());
}

// For a replaced element whose concrete object size is no larger than the
// element's content-box, this method checks whether the given
// "object-position" coordinate might cause overflow in its dimension.
typedef nsStyleBackground::Position::PositionCoord PositionCoord;
static bool
ObjectPositionCoordMightCauseOverflow(const PositionCoord& aCoord)
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
  const nsStyleBackground::Position& objectPosistion = aStylePos->mObjectPosition;
  if (ObjectPositionCoordMightCauseOverflow(objectPosistion.mXPosition) ||
      ObjectPositionCoordMightCauseOverflow(objectPosistion.mYPosition)) {
    return true;
  }

  return false;
}


/* static */ bool
nsStyleUtil::CSPAllowsInlineStyle(nsIContent* aContent,
                                  nsIPrincipal* aPrincipal,
                                  nsIURI* aSourceURI,
                                  uint32_t aLineNumber,
                                  const nsSubstring& aStyleText,
                                  nsresult* aRv)
{
  nsresult rv;

  if (aRv) {
    *aRv = NS_OK;
  }

  MOZ_ASSERT(!aContent || aContent->NodeInfo()->NameAtom() == nsGkAtoms::style,
      "aContent passed to CSPAllowsInlineStyle "
      "for an element that is not <style>");

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = aPrincipal->GetCsp(getter_AddRefs(csp));

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
  if (aContent) {
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::nonce, nonce);
  }

  bool allowInlineStyle = true;
  rv = csp->GetAllowsInline(nsIContentPolicy::TYPE_STYLESHEET,
                            nonce, aStyleText, aLineNumber,
                            &allowInlineStyle);
  NS_ENSURE_SUCCESS(rv, false);

  return allowInlineStyle;
}
