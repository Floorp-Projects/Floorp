/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include "txXSLTFunctions.h"
#include "nsGkAtoms.h"
#include "txIXPathContext.h"
#include "txStylesheet.h"
#include <math.h>
#include "txNamespaceMap.h"

#include "prdtoa.h"

#define INVALID_PARAM_VALUE \
  NS_LITERAL_STRING("invalid parameter value for function")

const char16_t txFormatNumberFunctionCall::FORMAT_QUOTE = '\'';

/*
 * FormatNumberFunctionCall
 * A representation of the XSLT additional function: format-number()
 */

/*
 * Creates a new format-number function call
 */
txFormatNumberFunctionCall::txFormatNumberFunctionCall(
    txStylesheet* aStylesheet, txNamespaceMap* aMappings)
    : mStylesheet(aStylesheet), mMappings(aMappings) {}

void txFormatNumberFunctionCall::ReportInvalidArg(txIEvalContext* aContext) {
  nsAutoString err(INVALID_PARAM_VALUE);
#ifdef TX_TO_STRING
  err.AppendLiteral(": ");
  toString(err);
#endif
  aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);
}

/*
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param cs the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 */
nsresult txFormatNumberFunctionCall::evaluate(txIEvalContext* aContext,
                                              txAExprResult** aResult) {
  *aResult = nullptr;
  if (!requireParams(2, 3, aContext)) return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

  // Get number and format
  double value;
  txExpandedName formatName;

  nsresult rv = evaluateToNumber(mParams[0], aContext, &value);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString formatStr;
  rv = mParams[1]->evaluateToString(aContext, formatStr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mParams.Length() == 3) {
    nsAutoString formatQName;
    rv = mParams[2]->evaluateToString(aContext, formatQName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = formatName.init(formatQName, mMappings, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  txDecimalFormat* format = mStylesheet->getDecimalFormat(formatName);
  if (!format) {
    nsAutoString err(NS_LITERAL_STRING("unknown decimal format"));
#ifdef TX_TO_STRING
    err.AppendLiteral(" for: ");
    toString(err);
#endif
    aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);
    return NS_ERROR_XPATH_INVALID_ARG;
  }

  // Special cases
  if (mozilla::IsNaN(value)) {
    return aContext->recycler()->getStringResult(format->mNaN, aResult);
  }

  if (value == mozilla::PositiveInfinity<double>()) {
    return aContext->recycler()->getStringResult(format->mInfinity, aResult);
  }

  if (value == mozilla::NegativeInfinity<double>()) {
    nsAutoString res;
    res.Append(format->mMinusSign);
    res.Append(format->mInfinity);
    return aContext->recycler()->getStringResult(res, aResult);
  }

  // Value is a normal finite number
  nsAutoString prefix;
  nsAutoString suffix;
  int minIntegerSize = 0;
  int minFractionSize = 0;
  int maxFractionSize = 0;
  int multiplier = 1;
  int groupSize = -1;

  uint32_t pos = 0;
  uint32_t formatLen = formatStr.Length();
  bool inQuote;

  // Get right subexpression
  inQuote = false;
  if (mozilla::IsNegative(value)) {
    while (pos < formatLen &&
           (inQuote || formatStr.CharAt(pos) != format->mPatternSeparator)) {
      if (formatStr.CharAt(pos) == FORMAT_QUOTE) inQuote = !inQuote;
      pos++;
    }

    if (pos == formatLen) {
      pos = 0;
      prefix.Append(format->mMinusSign);
    } else
      pos++;
  }

  // Parse the format string
  FormatParseState pState = Prefix;
  inQuote = false;

  char16_t c = 0;
  while (pos < formatLen && pState != Finished) {
    c = formatStr.CharAt(pos++);

    switch (pState) {
      case Prefix:
      case Suffix:
        if (!inQuote) {
          if (c == format->mPercent) {
            if (multiplier == 1)
              multiplier = 100;
            else {
              ReportInvalidArg(aContext);
              return NS_ERROR_XPATH_INVALID_ARG;
            }
          } else if (c == format->mPerMille) {
            if (multiplier == 1)
              multiplier = 1000;
            else {
              ReportInvalidArg(aContext);
              return NS_ERROR_XPATH_INVALID_ARG;
            }
          } else if (c == format->mDecimalSeparator ||
                     c == format->mGroupingSeparator ||
                     c == format->mZeroDigit || c == format->mDigit ||
                     c == format->mPatternSeparator) {
            pState = pState == Prefix ? IntDigit : Finished;
            pos--;
            break;
          }
        }

        if (c == FORMAT_QUOTE)
          inQuote = !inQuote;
        else if (pState == Prefix)
          prefix.Append(c);
        else
          suffix.Append(c);
        break;

      case IntDigit:
        if (c == format->mGroupingSeparator)
          groupSize = 0;
        else if (c == format->mDigit) {
          if (groupSize >= 0) groupSize++;
        } else {
          pState = IntZero;
          pos--;
        }
        break;

      case IntZero:
        if (c == format->mGroupingSeparator)
          groupSize = 0;
        else if (c == format->mZeroDigit) {
          if (groupSize >= 0) groupSize++;
          minIntegerSize++;
        } else if (c == format->mDecimalSeparator) {
          pState = FracZero;
        } else {
          pState = Suffix;
          pos--;
        }
        break;

      case FracZero:
        if (c == format->mZeroDigit) {
          maxFractionSize++;
          minFractionSize++;
        } else {
          pState = FracDigit;
          pos--;
        }
        break;

      case FracDigit:
        if (c == format->mDigit)
          maxFractionSize++;
        else {
          pState = Suffix;
          pos--;
        }
        break;

      case Finished:
        break;
    }
  }

  // Did we manage to parse the entire formatstring and was it valid
  if ((c != format->mPatternSeparator && pos < formatLen) || inQuote ||
      groupSize == 0) {
    ReportInvalidArg(aContext);
    return NS_ERROR_XPATH_INVALID_ARG;
  }

  /*
   * FINALLY we're done with the parsing
   * now build the result string
   */

  value = fabs(value) * multiplier;

  // Make sure the multiplier didn't push value to infinity.
  if (value == mozilla::PositiveInfinity<double>()) {
    return aContext->recycler()->getStringResult(format->mInfinity, aResult);
  }

  // Make sure the multiplier didn't push value to infinity.
  if (value == mozilla::PositiveInfinity<double>()) {
    return aContext->recycler()->getStringResult(format->mInfinity, aResult);
  }

  // Prefix
  nsAutoString res(prefix);

  int bufsize;
  if (value > 1)
    bufsize = (int)log10(value) + 30;
  else
    bufsize = 1 + 30;

  auto buf = mozilla::MakeUnique<char[]>(bufsize);
  int bufIntDigits, sign;
  char* endp;
  PR_dtoa(value, 0, 0, &bufIntDigits, &sign, &endp, buf.get(), bufsize - 1);

  int buflen = endp - buf.get();
  int intDigits;
  intDigits = bufIntDigits > minIntegerSize ? bufIntDigits : minIntegerSize;

  if (groupSize < 0) groupSize = intDigits + 10;  // to simplify grouping

  // XXX We shouldn't use SetLength.
  res.SetLength(res.Length() + intDigits +     // integer digits
                1 +                            // decimal separator
                maxFractionSize +              // fractions
                (intDigits - 1) / groupSize);  // group separators

  int32_t i = bufIntDigits + maxFractionSize - 1;
  bool carry = (0 <= i + 1) && (i + 1 < buflen) && (buf[i + 1] >= '5');
  bool hasFraction = false;

  // The number of characters in res that we haven't filled in.
  mozilla::CheckedUint32 resRemain = mozilla::CheckedUint32(res.Length());

#define CHECKED_SET_CHAR(c)                                           \
  --resRemain;                                                        \
  if (!resRemain.isValid() || !res.SetCharAt(c, resRemain.value())) { \
    ReportInvalidArg(aContext);                                       \
    return NS_ERROR_XPATH_INVALID_ARG;                                \
  }

#define CHECKED_TRUNCATE()             \
  --resRemain;                         \
  if (!resRemain.isValid()) {          \
    ReportInvalidArg(aContext);        \
    return NS_ERROR_XPATH_INVALID_ARG; \
  }                                    \
  res.Truncate(resRemain.value());

  // Fractions
  for (; i >= bufIntDigits; --i) {
    int digit;
    if (i >= buflen || i < 0) {
      digit = 0;
    } else {
      digit = buf[i] - '0';
    }

    if (carry) {
      digit = (digit + 1) % 10;
      carry = digit == 0;
    }

    if (hasFraction || digit != 0 || i < bufIntDigits + minFractionSize) {
      hasFraction = true;
      CHECKED_SET_CHAR((char16_t)(digit + format->mZeroDigit));
    } else {
      CHECKED_TRUNCATE();
    }
  }

  // Decimal separator
  if (hasFraction) {
    CHECKED_SET_CHAR(format->mDecimalSeparator);
  } else {
    CHECKED_TRUNCATE();
  }

  // Integer digits
  for (i = 0; i < intDigits; ++i) {
    int digit;
    if (bufIntDigits - i - 1 >= buflen || bufIntDigits - i - 1 < 0) {
      digit = 0;
    } else {
      digit = buf[bufIntDigits - i - 1] - '0';
    }

    if (carry) {
      digit = (digit + 1) % 10;
      carry = digit == 0;
    }

    if (i != 0 && i % groupSize == 0) {
      CHECKED_SET_CHAR(format->mGroupingSeparator);
    }

    CHECKED_SET_CHAR((char16_t)(digit + format->mZeroDigit));
  }

#undef CHECKED_SET_CHAR
#undef CHECKED_TRUNCATE

  if (carry) {
    if (i % groupSize == 0) {
      res.Insert(format->mGroupingSeparator, resRemain.value());
    }
    res.Insert((char16_t)(1 + format->mZeroDigit), resRemain.value());
  }

  if (!hasFraction && !intDigits && !carry) {
    // If we havn't added any characters we add a '0'
    // This can only happen for formats like '##.##'
    res.Append(format->mZeroDigit);
  }

  // Build suffix
  res.Append(suffix);

  return aContext->recycler()->getStringResult(res, aResult);
}  //-- evaluate

Expr::ResultType txFormatNumberFunctionCall::getReturnType() {
  return STRING_RESULT;
}

bool txFormatNumberFunctionCall::isSensitiveTo(ContextSensitivity aContext) {
  return argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void txFormatNumberFunctionCall::appendName(nsAString& aDest) {
  aDest.Append(nsGkAtoms::formatNumber->GetUTF16String());
}
#endif

/*
 * txDecimalFormat
 * A representation of the XSLT element <xsl:decimal-format>
 */

txDecimalFormat::txDecimalFormat()
    : mInfinity(NS_LITERAL_STRING("Infinity")), mNaN(NS_LITERAL_STRING("NaN")) {
  mDecimalSeparator = '.';
  mGroupingSeparator = ',';
  mMinusSign = '-';
  mPercent = '%';
  mPerMille = 0x2030;
  mZeroDigit = '0';
  mDigit = '#';
  mPatternSeparator = ';';
}

bool txDecimalFormat::isEqual(txDecimalFormat* other) {
  return mDecimalSeparator == other->mDecimalSeparator &&
         mGroupingSeparator == other->mGroupingSeparator &&
         mInfinity.Equals(other->mInfinity) &&
         mMinusSign == other->mMinusSign && mNaN.Equals(other->mNaN) &&
         mPercent == other->mPercent && mPerMille == other->mPerMille &&
         mZeroDigit == other->mZeroDigit && mDigit == other->mDigit &&
         mPatternSeparator == other->mPatternSeparator;
}
