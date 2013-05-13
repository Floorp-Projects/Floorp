/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStyleUtil.h"
#include "nsStyleConsts.h"

#include "nsIContent.h"
#include "nsReadableUtils.h"
#include "nsCSSProps.h"
#include "nsRuleNode.h"

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
            PRUnichar('-')) {
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
                                         PRUnichar quoteChar)
{
  NS_PRECONDITION(quoteChar == '\'' || quoteChar == '"',
                  "CSS strings must be quoted with ' or \"");
  aReturn.Append(quoteChar);

  const PRUnichar* in = aString.BeginReading();
  const PRUnichar* const end = aString.EndReading();
  for (; in != end; in++) {
    if (*in < 0x20 || (*in >= 0x7F && *in < 0xA0)) {
      // Escape U+0000 through U+001F and U+007F through U+009F numerically.
      aReturn.AppendPrintf("\\%hX ", *in);
    } else {
      if (*in == '"' || *in == '\'' || *in == '\\') {
        // Escape backslash and quote characters symbolically.
        // It's not technically necessary to escape the quote
        // character that isn't being used to delimit the string,
        // but we do it anyway because that makes testing simpler.
        aReturn.Append(PRUnichar('\\'));
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
  //   ident    [-]?{nmstart}{nmchar}*
  //   nmstart  [_a-z]|{nonascii}|{escape}
  //   nmchar   [_a-z0-9-]|{nonascii}|{escape}
  //   nonascii [^\0-\177]
  //   escape   {unicode}|\\[^\n\r\f0-9a-f]
  //   unicode  \\[0-9a-f]{1,6}(\r\n|[ \n\r\t\f])?
  // from http://www.w3.org/TR/CSS21/syndata.html#tokenization

  const PRUnichar* in = aIdent.BeginReading();
  const PRUnichar* const end = aIdent.EndReading();

  if (in == end)
    return;

  // A leading dash does not need to be escaped as long as it is not the
  // *only* character in the identifier.
  if (in + 1 != end && *in == '-') {
    aReturn.Append(PRUnichar('-'));
    ++in;
  }

  // Escape a digit at the start (including after a dash),
  // numerically.  If we didn't escape it numerically, it would get
  // interpreted as a numeric escape for the wrong character.
  // A second dash immediately after a leading dash must also be
  // escaped, but this may be done symbolically.
  if (in != end && (*in == '-' ||
                    ('0' <= *in && *in <= '9'))) {
    if (*in == '-') {
      aReturn.Append(PRUnichar('\\'));
      aReturn.Append(PRUnichar('-'));
    } else {
      aReturn.AppendPrintf("\\%hX ", *in);
    }
    ++in;
  }

  for (; in != end; ++in) {
    PRUnichar ch = *in;
    if (ch < 0x20 || (0x7F <= ch && ch < 0xA0)) {
      // Escape U+0000 through U+001F and U+007F through U+009F numerically.
      aReturn.AppendPrintf("\\%hX ", *in);
    } else {
      // Escape ASCII non-identifier printables as a backslash plus
      // the character.
      if (ch < 0x7F &&
          ch != '_' && ch != '-' &&
          (ch < '0' || '9' < ch) &&
          (ch < 'A' || 'Z' < ch) &&
          (ch < 'a' || 'z' < ch)) {
        aReturn.Append(PRUnichar('\\'));
      }
      aReturn.Append(ch);
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
        aResult.Append(PRUnichar(' '));
      }
    }
  }
  NS_ABORT_IF_FALSE(aMaskedValue == 0, "unexpected bit remaining in bitfield");
}

/* static */ void
nsStyleUtil::AppendPaintOrderValue(uint8_t aValue,
                                   nsAString& aResult)
{
  MOZ_STATIC_ASSERT
    (NS_STYLE_PAINT_ORDER_BITWIDTH * NS_STYLE_PAINT_ORDER_LAST_VALUE <= 8,
     "SVGStyleStruct::mPaintOrder and local variables not big enough");

  if (aValue == NS_STYLE_PAINT_ORDER_NORMAL) {
    aResult.AppendLiteral("normal");
    return;
  }

  // Append the minimal value necessary for the given paint order.
  MOZ_STATIC_ASSERT(NS_STYLE_PAINT_ORDER_LAST_VALUE == 3,
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
      aResult.AppendLiteral(" ");
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
      aResult.AppendLiteral(" ");
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
nsStyleUtil::AppendFunctionalAlternates(
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
          aResult.Append(PRUnichar(' '));
        }

        // append the previous functional value
        aResult.Append(funcName);
        aResult.Append(PRUnichar('('));
        aResult.Append(funcParams);
        aResult.Append(PRUnichar(')'));
      }

      // function name
      GetFunctionalAlternatesName(v.alternate, funcName);
      NS_ASSERTION(!funcName.IsEmpty(), "unknown property value name");

      // function params
      funcParams.Assign(v.value);
    } else {
      if (!funcParams.IsEmpty()) {
        funcParams.Append(NS_LITERAL_STRING(", "));
      }
      funcParams.Append(v.value);
    }
  }

    // append the previous functional value
  if (!funcName.IsEmpty() && !funcParams.IsEmpty()) {
    if (!aResult.IsEmpty()) {
      aResult.Append(PRUnichar(' '));
    }

    aResult.Append(funcName);
    aResult.Append(PRUnichar('('));
    aResult.Append(funcParams);
    aResult.Append(PRUnichar(')'));
  }
}

/* static */ void
nsStyleUtil::AppendAlternateValues(const nsCSSValueList* aList,
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
    nsAutoString keywordStr;
    func->Item(0).GetStringValue(keywordStr);
    nsCSSKeyword key = nsCSSKeywords::LookupKeyword(keywordStr);
    NS_ASSERTION(key != eCSSKeyword_UNKNOWN, "unknown alternate property value");

    int32_t alternate;
    if (key == eCSSKeyword_UNKNOWN ||
        !nsCSSProps::FindKeyword(key,
                                 nsCSSProps::kFontVariantAlternatesFuncsKTable,
                                 alternate)) {
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

