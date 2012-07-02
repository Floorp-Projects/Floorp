/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "mozilla/Util.h"

#include "nsStyleUtil.h"
#include "nsCRT.h"
#include "nsStyleConsts.h"

#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsTextFormatter.h"
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
  PRUint32 selectorLen = aSelectorValue.Length();
  PRUint32 attributeLen = aAttributeValue.Length();
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

void nsStyleUtil::AppendEscapedCSSString(const nsString& aString,
                                         nsAString& aReturn)
{
  aReturn.Append(PRUnichar('"'));

  const nsString::char_type* in = aString.get();
  const nsString::char_type* const end = in + aString.Length();
  for (; in != end; in++)
  {
    if (*in < 0x20)
    {
     // Escape all characters below 0x20 numerically.
   
     /*
      This is the buffer into which snprintf should write. As the hex. value is,
      for numbers below 0x20, max. 2 characters long, we don't need more than 5
      characters ("\XX "+NUL).
     */
     PRUnichar buf[5];
     nsTextFormatter::snprintf(buf, ArrayLength(buf), NS_LITERAL_STRING("\\%hX ").get(), *in);
     aReturn.Append(buf);
   
    } else switch (*in) {
      // Special characters which should be escaped: Quotes and backslash
      case '\\':
      case '\"':
      case '\'':
       aReturn.Append(PRUnichar('\\'));
      // And now, after the eventual escaping character, the actual one.
      default:
       aReturn.Append(PRUnichar(*in));
    }
  }

  aReturn.Append(PRUnichar('"'));
}

/* static */ void
nsStyleUtil::AppendEscapedCSSIdent(const nsString& aIdent, nsAString& aReturn)
{
  // The relevant parts of the CSS grammar are:
  //   ident    [-]?{nmstart}{nmchar}*
  //   nmstart  [_a-z]|{nonascii}|{escape}
  //   nmchar   [_a-z0-9-]|{nonascii}|{escape}
  //   nonascii [^\0-\177]
  //   escape   {unicode}|\\[^\n\r\f0-9a-f]
  //   unicode  \\[0-9a-f]{1,6}(\r\n|[ \n\r\t\f])?
  // from http://www.w3.org/TR/CSS21/syndata.html#tokenization

  const nsString::char_type* in = aIdent.get();
  const nsString::char_type* const end = in + aIdent.Length();

  // Deal with the leading dash separately so we don't need to
  // unnecessarily escape digits.
  if (in != end && *in == '-') {
    aReturn.Append(PRUnichar('-'));
    ++in;
  }

  bool first = true;
  for (; in != end; ++in, first = false)
  {
    if (*in < 0x20 || (first && '0' <= *in && *in <= '9'))
    {
      // Escape all characters below 0x20, and digits at the start
      // (including after a dash), numerically.  If we didn't escape
      // digits numerically, they'd get interpreted as a numeric escape
      // for the wrong character.

      /*
       This is the buffer into which snprintf should write. As the hex.
       value is, for numbers below 0x7F, max. 2 characters long, we
       don't need more than 5 characters ("\XX "+NUL).
      */
      PRUnichar buf[5];
      nsTextFormatter::snprintf(buf, ArrayLength(buf),
                                NS_LITERAL_STRING("\\%hX ").get(), *in);
      aReturn.Append(buf);
    } else {
      PRUnichar ch = *in;
      if (!((ch == PRUnichar('_')) ||
            (PRUnichar('A') <= ch && ch <= PRUnichar('Z')) ||
            (PRUnichar('a') <= ch && ch <= PRUnichar('z')) ||
            PRUnichar(0x80) <= ch ||
            (!first && ch == PRUnichar('-')) ||
            (PRUnichar('0') <= ch && ch <= PRUnichar('9')))) {
        // Character needs to be escaped
        aReturn.Append(PRUnichar('\\'));
      }
      aReturn.Append(ch);
    }
  }
}

/* static */ void
nsStyleUtil::AppendBitmaskCSSValue(nsCSSProperty aProperty,
                                   PRInt32 aMaskedValue,
                                   PRInt32 aFirstMask,
                                   PRInt32 aLastMask,
                                   nsAString& aResult)
{
  for (PRInt32 mask = aFirstMask; mask <= aLastMask; mask <<= 1) {
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
nsStyleUtil::AppendFontFeatureSettings(const nsTArray<gfxFontFeature>& aFeatures,
                                       nsAString& aResult)
{
  for (PRUint32 i = 0, numFeat = aFeatures.Length(); i < numFeat; i++) {
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

/* static */ float
nsStyleUtil::ColorComponentToFloat(PRUint8 aAlpha)
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

