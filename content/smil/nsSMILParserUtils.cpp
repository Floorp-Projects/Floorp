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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#include "nsSMILParserUtils.h"
#include "nsISMILAttr.h"
#include "nsSMILValue.h"
#include "nsSMILTimeValue.h"
#include "nsSMILTimeValueSpecParams.h"
#include "nsSMILTypes.h"
#include "nsSMILRepeatCount.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "prdtoa.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "prlong.h"
#include "nsCharSeparatedTokenizer.h"

//------------------------------------------------------------------------------
// Helper functions and Constants

namespace {

const PRUint32 MSEC_PER_SEC  = 1000;
const PRUint32 MSEC_PER_MIN  = 1000 * 60;
const PRUint32 MSEC_PER_HOUR = 1000 * 60 * 60;
const PRInt32  DECIMAL_BASE  = 10;

#define ACCESSKEY_PREFIX_LC NS_LITERAL_STRING("accesskey(") // SMIL2+
#define ACCESSKEY_PREFIX_CC NS_LITERAL_STRING("accessKey(") // SVG/SMIL ANIM
#define REPEAT_PREFIX    NS_LITERAL_STRING("repeat(")
#define WALLCLOCK_PREFIX NS_LITERAL_STRING("wallclock(")

// NS_IS_SPACE relies on isspace which may return true for \xB and \xC but
// SMILANIM does not consider these characters to be whitespace.
inline bool
IsSpace(const PRUnichar c)
{
  return (c == 0x9 || c == 0xA || c == 0xD || c == 0x20);
}

template<class T>
inline void
SkipBeginWsp(T& aStart, T aEnd)
{
  while (aStart != aEnd && IsSpace(*aStart)) {
    ++aStart;
  }
}

inline void
SkipBeginEndWsp(const PRUnichar*& aStart, const PRUnichar*& aEnd)
{
  SkipBeginWsp(aStart, aEnd);
  while (aEnd != aStart && IsSpace(*(aEnd - 1))) {
    --aEnd;
  }
}

double
GetFloat(const char*& aStart, const char* aEnd, nsresult* aErrorCode)
{
  char* floatEnd;
  double value = PR_strtod(aStart, &floatEnd);

  nsresult rv;

  if (floatEnd == aStart || floatEnd > aEnd) {
    rv = NS_ERROR_FAILURE;
  } else {
    aStart = floatEnd;
    rv = NS_OK;
  }

  if (aErrorCode) {
    *aErrorCode = rv;
  }

  return value;
}

size_t
GetUnsignedInt(const nsAString& aStr, PRUint32& aResult)
{
  NS_ConvertUTF16toUTF8 cstr(aStr);
  const char* str = cstr.get();

  char* rest;
  PRInt32 value = strtol(str, &rest, DECIMAL_BASE);

  if (rest == str || value < 0)
    return 0;

  aResult = static_cast<PRUint32>(value);
  return rest - str;
}

bool
GetUnsignedIntAndEndParen(const nsAString& aStr, PRUint32& aResult)
{
  size_t intLen = GetUnsignedInt(aStr, aResult);

  const PRUnichar* start = aStr.BeginReading();
  const PRUnichar* end = aStr.EndReading();

  // Make sure the string is only digit+')'
  if (intLen == 0 || start + intLen + 1 != end || *(start + intLen) != ')')
    return PR_FALSE;

  return PR_TRUE;
}

inline bool
ConsumeSubstring(const char*& aStart, const char* aEnd, const char* aSubstring)
{
  size_t substrLen = PL_strlen(aSubstring);

  if (static_cast<size_t>(aEnd - aStart) < substrLen)
    return PR_FALSE;

  bool result = false;

  if (PL_strstr(aStart, aSubstring) == aStart) {
    aStart += substrLen;
    result = PR_TRUE;
  }

  return result;
}

bool
ParseClockComponent(const char*& aStart,
                    const char* aEnd,
                    double& aResult,
                    bool& aIsReal,
                    bool& aCouldBeMin,
                    bool& aCouldBeSec)
{
  nsresult rv;
  const char* begin = aStart;
  double value = GetFloat(aStart, aEnd, &rv);

  // Check a number was found
  if (NS_FAILED(rv))
    return PR_FALSE;

  // Check that it's not expressed in exponential form
  size_t len = aStart - begin;
  bool isExp = (PL_strnpbrk(begin, "eE", len) != nsnull);
  if (isExp)
    return PR_FALSE;

  // Don't allow real numbers of the form "23."
  if (*(aStart - 1) == '.')
    return PR_FALSE;

  // Number looks good
  aResult = value;

  // Set some flags so we can check this number is valid once we know
  // whether it's an hour, minute string etc.
  aIsReal = (PL_strnchr(begin, '.', len) != nsnull);
  aCouldBeMin = (value < 60.0 && (len == 2));
  aCouldBeSec = (value < 60.0 ||
      (value == 60.0 && begin[0] == '5')); // Take care of rounding error
  aCouldBeSec &= (len >= 2 &&
      (begin[2] == '\0' || begin[2] == '.' || IsSpace(begin[2])));

  return PR_TRUE;
}

bool
ParseMetricMultiplicand(const char*& aStart,
                        const char* aEnd,
                        PRInt32& multiplicand)
{
  bool result = false;

  size_t len = aEnd - aStart;
  const char* cur = aStart;

  if (len) {
    switch (*cur++)
    {
      case 'h':
        multiplicand = MSEC_PER_HOUR;
        result = PR_TRUE;
        break;
      case 'm':
        if (len >= 2) {
          if (*cur == 's') {
            ++cur;
            multiplicand = 1;
            result = PR_TRUE;
          } else if (len >= 3 && *cur++ == 'i' && *cur++ == 'n') {
            multiplicand = MSEC_PER_MIN;
            result = PR_TRUE;
          }
        }
        break;
      case 's':
        multiplicand = MSEC_PER_SEC;
        result = PR_TRUE;
        break;
    }
  }

  if (result) {
    aStart = cur;
  }

  return result;
}

nsresult
ParseOptionalOffset(const nsAString& aSpec, nsSMILTimeValueSpecParams& aResult)
{
  if (aSpec.IsEmpty()) {
    aResult.mOffset.SetMillis(0);
    return NS_OK;
  }

  if (aSpec.First() != '+' && aSpec.First() != '-')
    return NS_ERROR_FAILURE;

  return nsSMILParserUtils::ParseClockValue(aSpec, &aResult.mOffset, PR_TRUE);
}

nsresult
ParseAccessKey(const nsAString& aSpec, nsSMILTimeValueSpecParams& aResult)
{
  NS_ABORT_IF_FALSE(StringBeginsWith(aSpec, ACCESSKEY_PREFIX_CC) ||
      StringBeginsWith(aSpec, ACCESSKEY_PREFIX_LC),
      "Calling ParseAccessKey on non-accesskey-type spec");

  nsSMILTimeValueSpecParams result;
  result.mType = nsSMILTimeValueSpecParams::ACCESSKEY;

  NS_ABORT_IF_FALSE(
      ACCESSKEY_PREFIX_LC.Length() == ACCESSKEY_PREFIX_CC.Length(),
      "Case variations for accesskey prefix differ in length");
  const PRUnichar* start = aSpec.BeginReading() + ACCESSKEY_PREFIX_LC.Length();
  const PRUnichar* end = aSpec.EndReading();

  // Expecting at least <accesskey> + ')'
  if (end - start < 2)
    return NS_ERROR_FAILURE;

  PRUint32 c = *start++;

  // Process 32-bit codepoints
  if (NS_IS_HIGH_SURROGATE(c)) {
    if (end - start < 2) // Expecting at least low-surrogate + ')'
      return NS_ERROR_FAILURE;
    PRUint32 lo = *start++;
    if (!NS_IS_LOW_SURROGATE(lo))
      return NS_ERROR_FAILURE;
    c = SURROGATE_TO_UCS4(c, lo);
  // XML 1.1 says that 0xFFFE and 0xFFFF are not valid characters
  } else if (NS_IS_LOW_SURROGATE(c) || c == 0xFFFE || c == 0xFFFF) {
    return NS_ERROR_FAILURE;
  }

  result.mRepeatIterationOrAccessKey = c;

  if (*start++ != ')')
    return NS_ERROR_FAILURE;

  SkipBeginWsp(start, end);

  nsresult rv = ParseOptionalOffset(Substring(start, end), result);
  if (NS_FAILED(rv))
    return rv;

  aResult = result;

  return NS_OK;
}

const PRUnichar*
GetTokenEnd(const nsAString& aStr, bool aBreakOnDot)
{
  const PRUnichar* tokenEnd = aStr.BeginReading();
  const PRUnichar* const end = aStr.EndReading();
  bool escape = false;
  while (tokenEnd != end) {
    PRUnichar c = *tokenEnd;
    if (IsSpace(c) ||
       (!escape && (c == '+' || c == '-' || (aBreakOnDot && c == '.')))) {
      break;
    }
    escape = (!escape && c == '\\');
    ++tokenEnd;
  }
  return tokenEnd;
}

void
Unescape(nsAString& aStr)
{
  const PRUnichar* read = aStr.BeginReading();
  const PRUnichar* const end = aStr.EndReading();
  PRUnichar* write = aStr.BeginWriting();
  bool escape = false;

  while (read != end) {
    NS_ABORT_IF_FALSE(write <= read, "Writing past where we've read");
    if (!escape && *read == '\\') {
      escape = PR_TRUE;
      ++read;
    } else {
      *write++ = *read++;
      escape = PR_FALSE;
    }
  }

  aStr.SetLength(write - aStr.BeginReading());
}

nsresult
ParseElementBaseTimeValueSpec(const nsAString& aSpec,
                              nsSMILTimeValueSpecParams& aResult)
{
  nsSMILTimeValueSpecParams result;

  //
  // The spec will probably look something like one of these
  //
  // element-name.begin
  // element-name.event-name
  // event-name
  // element-name.repeat(3)
  // event\.name
  //
  // Technically `repeat(3)' is permitted but the behaviour in this case is not
  // defined (for SMIL Animation) so we don't support it here.
  //

  const PRUnichar* tokenStart = aSpec.BeginReading();
  const PRUnichar* tokenEnd = GetTokenEnd(aSpec, PR_TRUE);
  nsAutoString token(Substring(tokenStart, tokenEnd));
  Unescape(token);

  if (token.IsEmpty())
    return NS_ERROR_FAILURE;

  // Whether the token is an id-ref or event-symbol it should be a valid NCName
  if (NS_FAILED(nsContentUtils::CheckQName(token, PR_FALSE)))
    return NS_ERROR_FAILURE;

  // Parse the second token if there is one
  if (tokenEnd != aSpec.EndReading() && *tokenEnd == '.') {
    result.mDependentElemID = do_GetAtom(token);

    tokenStart = ++tokenEnd;
    tokenEnd = GetTokenEnd(Substring(tokenStart, aSpec.EndReading()), PR_FALSE);

    // Don't unescape the token unless we need to and not until after we've
    // tested it
    const nsAString& rawToken2 = Substring(tokenStart, tokenEnd);

    // element-name.begin
    if (rawToken2.Equals(NS_LITERAL_STRING("begin"))) {
      result.mType = nsSMILTimeValueSpecParams::SYNCBASE;
      result.mSyncBegin = PR_TRUE;
    // element-name.end
    } else if (rawToken2.Equals(NS_LITERAL_STRING("end"))) {
      result.mType = nsSMILTimeValueSpecParams::SYNCBASE;
      result.mSyncBegin = PR_FALSE;
    // element-name.repeat(digit+)
    } else if (StringBeginsWith(rawToken2, REPEAT_PREFIX)) {
      result.mType = nsSMILTimeValueSpecParams::REPEAT;
      if (!GetUnsignedIntAndEndParen(
            Substring(tokenStart + REPEAT_PREFIX.Length(), tokenEnd),
            result.mRepeatIterationOrAccessKey))
        return NS_ERROR_FAILURE;
    // element-name.event-symbol
    } else {
      nsAutoString token2(rawToken2);
      Unescape(token2);
      result.mType = nsSMILTimeValueSpecParams::EVENT;
      if (token2.IsEmpty() ||
          NS_FAILED(nsContentUtils::CheckQName(token2, PR_FALSE)))
        return NS_ERROR_FAILURE;
      result.mEventSymbol = do_GetAtom(token2);
    }
  } else {
    // event-symbol
    result.mType = nsSMILTimeValueSpecParams::EVENT;
    result.mEventSymbol = do_GetAtom(token);
  }

  // We've reached the end of the token, so we should now be either looking at
  // a '+', '-', or the end.
  const PRUnichar* specEnd = aSpec.EndReading();
  SkipBeginWsp(tokenEnd, specEnd);

  nsresult rv = ParseOptionalOffset(Substring(tokenEnd, specEnd), result);
  if (NS_SUCCEEDED(rv)) {
    aResult = result;
  }

  return rv;
}

} // end anonymous namespace block

//------------------------------------------------------------------------------
// Implementation

nsresult
nsSMILParserUtils::ParseKeySplines(const nsAString& aSpec,
                                   nsTArray<double>& aSplineArray)
{
  nsresult rv = NS_OK;

  NS_ConvertUTF16toUTF8 spec(aSpec);
  const char* start = spec.BeginReading();
  const char* end = spec.EndReading();

  SkipBeginWsp(start, end);

  int i = 0;

  while (start != end)
  {
    double value = GetFloat(start, end, &rv);
    if (NS_FAILED(rv))
      break;

    if (value > 1.0 || value < 0.0) {
      rv = NS_ERROR_FAILURE;
      break;
    }

    if (!aSplineArray.AppendElement(value)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }

    ++i;

    SkipBeginWsp(start, end);
    if (start == end)
      break;

    if (i % 4) {
      if (*start == ',') {
        ++start;
      }
    } else {
      if (*start != ';') {
        rv = NS_ERROR_FAILURE;
        break;
      }
      ++start;
    }

    SkipBeginWsp(start, end);
  }

  if (i % 4) {
    rv = NS_ERROR_FAILURE; // wrong number of points
  }

  return rv;
}

nsresult
nsSMILParserUtils::ParseSemicolonDelimitedProgressList(const nsAString& aSpec,
                                                       bool aNonDecreasing,
                                                       nsTArray<double>& aArray)
{
  nsresult rv = NS_OK;

  NS_ConvertUTF16toUTF8 spec(aSpec);
  const char* start = spec.BeginReading();
  const char* end = spec.EndReading();

  SkipBeginWsp(start, end);

  double previousValue = -1.0;

  while (start != end) {
    double value = GetFloat(start, end, &rv);
    if (NS_FAILED(rv))
      break;

    if (value > 1.0 || value < 0.0 ||
        (aNonDecreasing && value < previousValue)) {
      rv = NS_ERROR_FAILURE;
      break;
    }

    if (!aArray.AppendElement(value)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
    previousValue = value;

    SkipBeginWsp(start, end);
    if (start == end)
      break;

    if (*start++ != ';') {
      rv = NS_ERROR_FAILURE;
      break;
    }

    SkipBeginWsp(start, end);
  }

  return rv;
}

// Helper class for ParseValues
class SMILValueParser : public nsSMILParserUtils::GenericValueParser
{
public:
  SMILValueParser(const nsISMILAnimationElement* aSrcElement,
                  const nsISMILAttr* aSMILAttr,
                  nsTArray<nsSMILValue>* aValuesArray,
                  bool* aPreventCachingOfSandwich) :
    mSrcElement(aSrcElement),
    mSMILAttr(aSMILAttr),
    mValuesArray(aValuesArray),
    mPreventCachingOfSandwich(aPreventCachingOfSandwich)
  {}

  virtual nsresult Parse(const nsAString& aValueStr) {
    nsSMILValue newValue;
    bool tmpPreventCachingOfSandwich;
    nsresult rv = mSMILAttr->ValueFromString(aValueStr, mSrcElement, newValue,
                                             tmpPreventCachingOfSandwich);
    if (NS_FAILED(rv))
      return rv;

    if (!mValuesArray->AppendElement(newValue)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (tmpPreventCachingOfSandwich) {
      *mPreventCachingOfSandwich = PR_TRUE;
    }
    return NS_OK;
  }
protected:
  const nsISMILAnimationElement* mSrcElement;
  const nsISMILAttr* mSMILAttr;
  nsTArray<nsSMILValue>* mValuesArray;
  bool* mPreventCachingOfSandwich;
};

nsresult
nsSMILParserUtils::ParseValues(const nsAString& aSpec,
                               const nsISMILAnimationElement* aSrcElement,
                               const nsISMILAttr& aAttribute,
                               nsTArray<nsSMILValue>& aValuesArray,
                               bool& aPreventCachingOfSandwich)
{
  // Assume all results can be cached, until we find one that can't.
  aPreventCachingOfSandwich = PR_FALSE;
  SMILValueParser valueParser(aSrcElement, &aAttribute,
                              &aValuesArray, &aPreventCachingOfSandwich);
  return ParseValuesGeneric(aSpec, valueParser);
}

nsresult
nsSMILParserUtils::ParseValuesGeneric(const nsAString& aSpec,
                                      GenericValueParser& aParser)
{
  nsCharSeparatedTokenizer tokenizer(aSpec, ';');
  if (!tokenizer.hasMoreTokens()) { // Empty list
    return NS_ERROR_FAILURE;
  }

  while (tokenizer.hasMoreTokens()) {
    nsresult rv = aParser.Parse(tokenizer.nextToken());
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Disallow ;-terminated values lists.
  if (tokenizer.lastTokenEndedWithSeparator()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsSMILParserUtils::ParseRepeatCount(const nsAString& aSpec,
                                    nsSMILRepeatCount& aResult)
{
  nsresult rv = NS_OK;

  NS_ConvertUTF16toUTF8 spec(aSpec);
  const char* start = spec.BeginReading();
  const char* end = spec.EndReading();

  SkipBeginWsp(start, end);

  if (start != end)
  {
    if (ConsumeSubstring(start, end, "indefinite")) {
      aResult.SetIndefinite();
    } else {
      double value = GetFloat(start, end, &rv);

      if (NS_SUCCEEDED(rv))
      {
        /* Repeat counts must be > 0 */
        if (value <= 0.0) {
          rv = NS_ERROR_FAILURE;
        } else {
          aResult = value;
        }
      }
    }

    /* Check for trailing junk */
    SkipBeginWsp(start, end);
    if (start != end) {
      rv = NS_ERROR_FAILURE;
    }
  } else {
    /* Empty spec */
    rv = NS_ERROR_FAILURE;
  }

  if (NS_FAILED(rv)) {
    aResult.Unset();
  }

  return rv;
}

nsresult
nsSMILParserUtils::ParseTimeValueSpecParams(const nsAString& aSpec,
                                            nsSMILTimeValueSpecParams& aResult)
{
  nsresult rv = NS_ERROR_FAILURE;

  const PRUnichar* start = aSpec.BeginReading();
  const PRUnichar* end = aSpec.EndReading();

  SkipBeginEndWsp(start, end);
  if (start == end)
    return rv;

  const nsAString &spec = Substring(start, end);

  // offset type
  if (*start == '+' || *start == '-' || NS_IsAsciiDigit(*start)) {
    rv = ParseClockValue(spec, &aResult.mOffset, PR_TRUE);
    if (NS_SUCCEEDED(rv)) {
      aResult.mType = nsSMILTimeValueSpecParams::OFFSET;
    }
  }

  // indefinite
  else if (spec.Equals(NS_LITERAL_STRING("indefinite"))) {
    aResult.mType = nsSMILTimeValueSpecParams::INDEFINITE;
    rv = NS_OK;
  }

  // wallclock type
  else if (StringBeginsWith(spec, WALLCLOCK_PREFIX)) {
    rv = NS_ERROR_NOT_IMPLEMENTED;
  }

  // accesskey type
  else if (StringBeginsWith(spec, ACCESSKEY_PREFIX_LC) ||
           StringBeginsWith(spec, ACCESSKEY_PREFIX_CC)) {
    rv = ParseAccessKey(spec, aResult);
  }

  // event, syncbase, or repeat
  else {
    rv = ParseElementBaseTimeValueSpec(spec, aResult);
  }

  return rv;
}

nsresult
nsSMILParserUtils::ParseClockValue(const nsAString& aSpec,
                                   nsSMILTimeValue* aResult,
                                   PRUint32 aFlags,   // = 0
                                   bool* aIsMedia)  // = nsnull
{
  nsSMILTime offset = 0L;
  double component = 0.0;

  PRInt8 sign = 0;
  PRUint8 colonCount = 0;

  bool started = false;
  bool isValid = true;

  PRInt32 metricMultiplicand = MSEC_PER_SEC;

  bool numIsReal = false;
  bool prevNumCouldBeMin = false;
  bool numCouldBeMin = false;
  bool numCouldBeSec = false;
  bool isIndefinite = false;

  if (aIsMedia) {
    *aIsMedia = PR_FALSE;
  }

  NS_ConvertUTF16toUTF8 spec(aSpec);
  const char* start = spec.BeginReading();
  const char* end = spec.EndReading();

  while (start != end) {
    if (IsSpace(*start)) {
      if (started) {
        ++start;
        break;
      }
      // else, we haven't started yet, ignore initial whitespace
      ++start;

    } else if ((aFlags & kClockValueAllowSign)
               && (*start == '+' || *start == '-')) {
      if (sign != 0) {
        // sign has already been set
        isValid = PR_FALSE;
        break;
      }

      if (started) {
        // sign appears in the middle of the string
        isValid = PR_FALSE;
        break;
      }

      sign = (*start == '+') ? 1 : -1;
      ++start;
    // The NS_IS_DIGIT etc. macros are not locale-specific
    } else if (NS_IS_DIGIT(*start)) {
      prevNumCouldBeMin = numCouldBeMin;

      if (!ParseClockComponent(start, end, component, numIsReal, numCouldBeMin,
                               numCouldBeSec)) {
        isValid = PR_FALSE;
        break;
      }

      started = PR_TRUE;
    } else if (*start == ':') {
      ++colonCount;

      // Neither minutes nor hours can be reals
      if (numIsReal) {
        isValid = PR_FALSE;
        break;
      }

      // Clock value can't start with a ':'
      if (!started) {
        isValid = PR_FALSE;
        break;
      }

      // Can't have more than two colons
      if (colonCount > 2) {
        isValid = PR_FALSE;
        break;
      }

      // Multiply the offset by 60 and add the last accumulated component
      offset = offset * 60 + PRInt64(component);

      component = 0.0;
      ++start;
    } else if (NS_IS_ALPHA(*start)) {
      if (colonCount > 0) {
        isValid = PR_FALSE;
        break;
      }

      if ((aFlags & kClockValueAllowIndefinite)
          && ConsumeSubstring(start, end, "indefinite")) {
        // We set a separate flag because we don't know what the state of the
        // passed in time value is and we shouldn't change it in the case of a
        // bad input string (so we can't initialise it to 0ms for example).
        isIndefinite = PR_TRUE;
        if (aResult) {
          aResult->SetIndefinite();
        }
      } else if (aIsMedia && ConsumeSubstring(start, end, "media")) {
        *aIsMedia = PR_TRUE;
      } else if (!ParseMetricMultiplicand(start, end, metricMultiplicand)) {
        isValid = PR_FALSE;
        break;
      }

      // Nothing must come after the string except whitespace
      break;
    } else {
      isValid = PR_FALSE;
      break;
    }
  }

  if (!started) {
    isValid = PR_FALSE;
  }

  // Process remainder of string (if any) to ensure it is only trailing
  // whitespace (embedded whitespace is not allowed)
  SkipBeginWsp(start, end);
  if (start != end) {
    isValid = PR_FALSE;
  }

  // No more processing required if the value was "indefinite" or "media".
  if (isIndefinite || (aIsMedia && *aIsMedia))
    return NS_OK;

  // If there is more than one colon then the previous component must be a
  // correctly formatted minute (i.e. two digits between 00 and 59) and the
  // latest component must be a correctly formatted second (i.e. two digits
  // before the .)
  if (colonCount > 0 && (!prevNumCouldBeMin || !numCouldBeSec)) {
    isValid = PR_FALSE;
  }

  if (isValid) {
    // Tack on the last component
    if (colonCount > 0) {
      offset = offset * 60 * 1000;
      component *= 1000;
      // rounding
      component = (component >= 0) ? component + 0.5 : component - 0.5;
      offset += PRInt64(component);
    } else {
      component *= metricMultiplicand;
      // rounding
      component = (component >= 0) ? component + 0.5 : component - 0.5;
      offset = PRInt64(component);
    }

    if (aResult) {
      nsSMILTime millis = offset;

      if (sign == -1) {
        millis = -offset;
      }

      aResult->SetMillis(millis);
    }
  }

  return (isValid) ? NS_OK : NS_ERROR_FAILURE;
}

PRInt32
nsSMILParserUtils::CheckForNegativeNumber(const nsAString& aStr)
{
  PRInt32 absValLocation = -1;

  nsAString::const_iterator start, end;
  aStr.BeginReading(start);
  aStr.EndReading(end);

  // Skip initial whitespace
  SkipBeginWsp(start, end);

  // Check for dash
  if (start != end && *start == '-') {
    ++start;
    // Check for numeric character
    if (start != end && NS_IS_DIGIT(*start)) {
      absValLocation = start.get() - start.start();
    }
  }
  return absValLocation;
}
