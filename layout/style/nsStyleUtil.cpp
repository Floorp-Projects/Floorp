/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStyleUtil.h"
#include "nsStyleConsts.h"

#include "mozilla/dom/Document.h"
#include "mozilla/ExpandedPrincipal.h"
#include "nsIContent.h"
#include "nsCSSProps.h"
#include "nsContentUtils.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsStyleStruct.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
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
                                   const nsStringComparator& aComparator) {
  bool result;
  uint32_t selectorLen = aSelectorValue.Length();
  uint32_t attributeLen = aAttributeValue.Length();
  if (selectorLen > attributeLen) {
    result = false;
  } else {
    nsAString::const_iterator iter;
    if (selectorLen != attributeLen &&
        *aAttributeValue.BeginReading(iter).advance(selectorLen) !=
            char16_t('-')) {
      // to match, the aAttributeValue must have a dash after the end of
      // the aSelectorValue's text (unless the aSelectorValue and the
      // aAttributeValue have the same text)
      result = false;
    } else {
      result = StringBeginsWith(aAttributeValue, aSelectorValue, aComparator);
    }
  }
  return result;
}

bool nsStyleUtil::ValueIncludes(const nsAString& aValueList,
                                const nsAString& aValue,
                                const nsStringComparator& aComparator) {
  const char16_t *p = aValueList.BeginReading(),
                 *p_end = aValueList.EndReading();

  while (p < p_end) {
    // skip leading space
    while (p != p_end && nsContentUtils::IsHTMLWhitespace(*p)) ++p;

    const char16_t* val_start = p;

    // look for space or end
    while (p != p_end && !nsContentUtils::IsHTMLWhitespace(*p)) ++p;

    const char16_t* val_end = p;

    if (val_start < val_end &&
        aValue.Equals(Substring(val_start, val_end), aComparator))
      return true;

    ++p;  // we know the next character is not whitespace
  }
  return false;
}

void nsStyleUtil::AppendEscapedCSSString(const nsAString& aString,
                                         nsAString& aReturn,
                                         char16_t quoteChar) {
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

/* static */
void nsStyleUtil::AppendEscapedCSSIdent(const nsAString& aIdent,
                                        nsAString& aReturn) {
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

  if (in == end) return;

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
      if (ch < 0x7F && ch != '_' && ch != '-' && (ch < '0' || '9' < ch) &&
          (ch < 'A' || 'Z' < ch) && (ch < 'a' || 'z' < ch)) {
        aReturn.Append(char16_t('\\'));
      }
      aReturn.Append(ch);
    }
  }
}

/* static */
float nsStyleUtil::ColorComponentToFloat(uint8_t aAlpha) {
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

/* static */
void nsStyleUtil::GetSerializedColorValue(nscolor aColor,
                                          nsAString& aSerializedColor) {
  MOZ_ASSERT(aSerializedColor.IsEmpty());

  const bool hasAlpha = NS_GET_A(aColor) != 255;
  if (hasAlpha) {
    aSerializedColor.AppendLiteral("rgba(");
  } else {
    aSerializedColor.AppendLiteral("rgb(");
  }
  aSerializedColor.AppendInt(NS_GET_R(aColor));
  aSerializedColor.AppendLiteral(", ");
  aSerializedColor.AppendInt(NS_GET_G(aColor));
  aSerializedColor.AppendLiteral(", ");
  aSerializedColor.AppendInt(NS_GET_B(aColor));
  if (hasAlpha) {
    aSerializedColor.AppendLiteral(", ");
    float alpha = nsStyleUtil::ColorComponentToFloat(NS_GET_A(aColor));
    nsStyleUtil::AppendCSSNumber(alpha, aSerializedColor);
  }
  aSerializedColor.AppendLiteral(")");
}

/* static */
bool nsStyleUtil::IsSignificantChild(nsIContent* aChild,
                                     bool aWhitespaceIsSignificant) {
  bool isText = aChild->IsText();

  if (!isText && !aChild->IsComment() && !aChild->IsProcessingInstruction()) {
    return true;
  }

  return isText && aChild->TextLength() != 0 &&
         (aWhitespaceIsSignificant || !aChild->TextIsOnlyWhitespace());
}

/* static */
bool nsStyleUtil::ThreadSafeIsSignificantChild(const nsIContent* aChild,
                                               bool aWhitespaceIsSignificant) {
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
static bool ObjectPositionCoordMightCauseOverflow(
    const LengthPercentage& aCoord) {
  // Any nonzero length in "object-position" can push us to overflow
  // (particularly if our concrete object size is exactly the same size as the
  // replaced element's content-box).
  if (!aCoord.ConvertsToPercentage()) {
    return !aCoord.ConvertsToLength() || aCoord.ToLengthInCSSPixels() != 0.0f;
  }

  // Percentages are interpreted as a fraction of the extra space. So,
  // percentages in the 0-100% range are safe, but values outside of that
  // range could cause overflow.
  float percentage = aCoord.ToPercentage();
  return percentage < 0.0f || percentage > 1.0f;
}

/* static */
bool nsStyleUtil::ObjectPropsMightCauseOverflow(
    const nsStylePosition* aStylePos) {
  auto objectFit = aStylePos->mObjectFit;

  // "object-fit: cover" & "object-fit: none" can give us a render rect that's
  // larger than our container element's content-box.
  if (objectFit == StyleObjectFit::Cover || objectFit == StyleObjectFit::None) {
    return true;
  }
  // (All other object-fit values produce a concrete object size that's no
  // larger than the constraint region.)

  // Check each of our "object-position" coords to see if it could cause
  // overflow in its dimension:
  const Position& objectPosistion = aStylePos->mObjectPosition;
  if (ObjectPositionCoordMightCauseOverflow(objectPosistion.horizontal) ||
      ObjectPositionCoordMightCauseOverflow(objectPosistion.vertical)) {
    return true;
  }

  return false;
}

/* static */
bool nsStyleUtil::CSPAllowsInlineStyle(
    dom::Element* aElement, dom::Document* aDocument,
    nsIPrincipal* aTriggeringPrincipal, uint32_t aLineNumber,
    uint32_t aColumnNumber, const nsAString& aStyleText, nsresult* aRv) {
  nsresult rv;

  if (aRv) {
    *aRv = NS_OK;
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  if (aTriggeringPrincipal && BasePrincipal::Cast(aTriggeringPrincipal)
                                  ->OverridesCSP(aDocument->NodePrincipal())) {
    nsCOMPtr<nsIExpandedPrincipal> ep = do_QueryInterface(aTriggeringPrincipal);
    if (ep) {
      csp = ep->GetCsp();
    }
  } else {
    csp = aDocument->GetCsp();
  }

  if (!csp) {
    // No CSP --> the style is allowed
    return true;
  }

  // Hack to allow Devtools to edit inline styles
  if (csp->GetSkipAllowInlineStyleCheck()) {
    return true;
  }

  // query the nonce
  nsAutoString nonce;
  if (aElement && aElement->NodeInfo()->NameAtom() == nsGkAtoms::style) {
    nsString* cspNonce =
        static_cast<nsString*>(aElement->GetProperty(nsGkAtoms::nonce));
    if (cspNonce) {
      nonce = *cspNonce;
    }
  }

  bool allowInlineStyle = true;
  rv = csp->GetAllowsInline(
      nsIContentSecurityPolicy::STYLE_SRC_DIRECTIVE, nonce,
      false,              // aParserCreated only applies to scripts
      aElement, nullptr,  // nsICSPEventListener
      aStyleText, aLineNumber, aColumnNumber, &allowInlineStyle);
  NS_ENSURE_SUCCESS(rv, false);

  return allowInlineStyle;
}
