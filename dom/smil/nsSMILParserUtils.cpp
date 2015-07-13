/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILParserUtils.h"
#include "nsSMILKeySpline.h"
#include "nsISMILAttr.h"
#include "nsSMILValue.h"
#include "nsSMILTimeValue.h"
#include "nsSMILTimeValueSpecParams.h"
#include "nsSMILTypes.h"
#include "nsSMILRepeatCount.h"
#include "nsContentUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
//------------------------------------------------------------------------------
// Helper functions and Constants

namespace {

const uint32_t MSEC_PER_SEC  = 1000;
const uint32_t MSEC_PER_MIN  = 1000 * 60;
const uint32_t MSEC_PER_HOUR = 1000 * 60 * 60;

#define ACCESSKEY_PREFIX_LC NS_LITERAL_STRING("accesskey(") // SMIL2+
#define ACCESSKEY_PREFIX_CC NS_LITERAL_STRING("accessKey(") // SVG/SMIL ANIM
#define REPEAT_PREFIX    NS_LITERAL_STRING("repeat(")
#define WALLCLOCK_PREFIX NS_LITERAL_STRING("wallclock(")

inline bool
SkipWhitespace(RangedPtr<const char16_t>& aIter,
               const RangedPtr<const char16_t>& aEnd)
{
  while (aIter != aEnd) {
    if (!IsSVGWhitespace(*aIter)) {
      return true;
    }
    ++aIter;
  }
  return false;
}

inline bool
ParseColon(RangedPtr<const char16_t>& aIter,
           const RangedPtr<const char16_t>& aEnd)
{
  if (aIter == aEnd || *aIter != ':') {
    return false;
  }
  ++aIter;
  return true;
}

/*
 * Exactly two digits in the range 00 - 59 are expected.
 */
bool
ParseSecondsOrMinutes(RangedPtr<const char16_t>& aIter,
                      const RangedPtr<const char16_t>& aEnd,
                      uint32_t& aValue)
{
  if (aIter == aEnd || !SVGContentUtils::IsDigit(*aIter)) {
    return false;
  }

  RangedPtr<const char16_t> iter(aIter);

  if (++iter == aEnd || !SVGContentUtils::IsDigit(*iter)) {
     return false;
  }

  uint32_t value = 10 * SVGContentUtils::DecimalDigitValue(*aIter) +
                   SVGContentUtils::DecimalDigitValue(*iter);
  if (value > 59) {
    return false;
  }
  if (++iter != aEnd && SVGContentUtils::IsDigit(*iter)) {
    return false;
  }

  aValue = value;
  aIter = iter;
  return true;
}

inline bool
ParseClockMetric(RangedPtr<const char16_t>& aIter,
                 const RangedPtr<const char16_t>& aEnd,
                 uint32_t& aMultiplier)
{
  if (aIter == aEnd) {
    aMultiplier = MSEC_PER_SEC;
    return true;
  }

  switch (*aIter) {
  case 'h':
    if (++aIter == aEnd) {
      aMultiplier = MSEC_PER_HOUR;
      return true;
    }
    return false;
  case 'm':
    {
      const nsAString& metric = Substring(aIter.get(), aEnd.get());
      if (metric.EqualsLiteral("min")) {
        aMultiplier = MSEC_PER_MIN;
        aIter = aEnd;
        return true;
      }
      if (metric.EqualsLiteral("ms")) {
        aMultiplier = 1;
        aIter = aEnd;
        return true;
      }
    }
    return false;
  case 's':
    if (++aIter == aEnd) {
      aMultiplier = MSEC_PER_SEC;
      return true;
    }
  }
  return false;
}

/**
 * See http://www.w3.org/TR/SVG/animate.html#ClockValueSyntax
 */
bool
ParseClockValue(RangedPtr<const char16_t>& aIter,
                const RangedPtr<const char16_t>& aEnd,
                nsSMILTimeValue* aResult)
{
  if (aIter == aEnd) {
    return false;
  }

  // TIMECOUNT_VALUE     ::= Timecount ("." Fraction)? (Metric)?
  // PARTIAL_CLOCK_VALUE ::= Minutes ":" Seconds ("." Fraction)?
  // FULL_CLOCK_VALUE    ::= Hours ":" Minutes ":" Seconds ("." Fraction)?
  enum ClockType {
    TIMECOUNT_VALUE,
    PARTIAL_CLOCK_VALUE,
    FULL_CLOCK_VALUE
  };

  int32_t clockType = TIMECOUNT_VALUE;

  RangedPtr<const char16_t> iter(aIter);

  // Determine which type of clock value we have by counting the number
  // of colons in the string.
  do {
    switch (*iter) {
    case ':':
       if (clockType == FULL_CLOCK_VALUE) {
         return false;
       }
       ++clockType;
       break;
    case 'e':
    case 'E':
    case '-':
    case '+':
      // Exclude anything invalid (for clock values)
      // that number parsing might otherwise allow.
      return false;
    }
    ++iter;
  } while (iter != aEnd);

  iter = aIter;

  int32_t hours = 0, timecount;
  double fraction = 0.0;
  uint32_t minutes, seconds, multiplier;

  switch (clockType) {
    case FULL_CLOCK_VALUE:
      if (!SVGContentUtils::ParseInteger(iter, aEnd, hours) ||
          !ParseColon(iter, aEnd)) {
        return false;
      }
      // intentional fall through
    case PARTIAL_CLOCK_VALUE:
      if (!ParseSecondsOrMinutes(iter, aEnd, minutes) ||
          !ParseColon(iter, aEnd) ||
          !ParseSecondsOrMinutes(iter, aEnd, seconds)) {
        return false;
      }
      if (iter != aEnd &&
          (*iter != '.' ||
           !SVGContentUtils::ParseNumber(iter, aEnd, fraction))) {
        return false;
      }
      aResult->SetMillis(nsSMILTime(hours) * MSEC_PER_HOUR +
                         minutes * MSEC_PER_MIN +
                         seconds * MSEC_PER_SEC +
                         NS_round(fraction * MSEC_PER_SEC));
      aIter = iter;
      return true;
    case TIMECOUNT_VALUE:
      if (!SVGContentUtils::ParseInteger(iter, aEnd, timecount)) {
        return false;
      }
      if (iter != aEnd && *iter == '.' &&
          !SVGContentUtils::ParseNumber(iter, aEnd, fraction)) {
        return false;
      }
      if (!ParseClockMetric(iter, aEnd, multiplier)) {
        return false;
      }
      aResult->SetMillis(nsSMILTime(timecount) * multiplier +
                         NS_round(fraction * multiplier));
      aIter = iter;
      return true;
  }

  return false;
}

bool
ParseOffsetValue(RangedPtr<const char16_t>& aIter,
                 const RangedPtr<const char16_t>& aEnd,
                 nsSMILTimeValue* aResult)
{
  RangedPtr<const char16_t> iter(aIter);

  int32_t sign;
  if (!SVGContentUtils::ParseOptionalSign(iter, aEnd, sign) ||
      !SkipWhitespace(iter, aEnd) ||
      !ParseClockValue(iter, aEnd, aResult)) {
    return false;
  }
  if (sign == -1) {
    aResult->SetMillis(-aResult->GetMillis());
  }
  aIter = iter;
  return true;
}

bool
ParseOffsetValue(const nsAString& aSpec,
                 nsSMILTimeValue* aResult)
{
  RangedPtr<const char16_t> iter(SVGContentUtils::GetStartRangedPtr(aSpec));
  const RangedPtr<const char16_t> end(SVGContentUtils::GetEndRangedPtr(aSpec));

  return ParseOffsetValue(iter, end, aResult) && iter == end;
}

bool
ParseOptionalOffset(RangedPtr<const char16_t>& aIter,
                    const RangedPtr<const char16_t>& aEnd,
                    nsSMILTimeValue* aResult)
{
  if (aIter == aEnd) {
    aResult->SetMillis(0L);
    return true;
  }

  return SkipWhitespace(aIter, aEnd) &&
         ParseOffsetValue(aIter, aEnd, aResult);
}

bool
ParseAccessKey(const nsAString& aSpec, nsSMILTimeValueSpecParams& aResult)
{
  MOZ_ASSERT(StringBeginsWith(aSpec, ACCESSKEY_PREFIX_CC) ||
             StringBeginsWith(aSpec, ACCESSKEY_PREFIX_LC),
             "Calling ParseAccessKey on non-accesskey-type spec");

  nsSMILTimeValueSpecParams result;
  result.mType = nsSMILTimeValueSpecParams::ACCESSKEY;

  MOZ_ASSERT(ACCESSKEY_PREFIX_LC.Length() == ACCESSKEY_PREFIX_CC.Length(),
             "Case variations for accesskey prefix differ in length");

  RangedPtr<const char16_t> iter(SVGContentUtils::GetStartRangedPtr(aSpec));
  RangedPtr<const char16_t> end(SVGContentUtils::GetEndRangedPtr(aSpec));

  iter += ACCESSKEY_PREFIX_LC.Length();

  // Expecting at least <accesskey> + ')'
  if (end - iter < 2)
    return false;

  uint32_t c = *iter++;

  // Process 32-bit codepoints
  if (NS_IS_HIGH_SURROGATE(c)) {
    if (end - iter < 2) // Expecting at least low-surrogate + ')'
      return false;
    uint32_t lo = *iter++;
    if (!NS_IS_LOW_SURROGATE(lo))
      return false;
    c = SURROGATE_TO_UCS4(c, lo);
  // XML 1.1 says that 0xFFFE and 0xFFFF are not valid characters
  } else if (NS_IS_LOW_SURROGATE(c) || c == 0xFFFE || c == 0xFFFF) {
    return false;
  }

  result.mRepeatIterationOrAccessKey = c;

  if (*iter++ != ')')
    return false;

  if (!ParseOptionalOffset(iter, end, &result.mOffset) || iter != end) {
    return false;
  }
  aResult = result;
  return true;
}

void
MoveToNextToken(RangedPtr<const char16_t>& aIter,
                const RangedPtr<const char16_t>& aEnd,
                bool aBreakOnDot,
                bool& aIsAnyCharEscaped)
{
  aIsAnyCharEscaped = false;

  bool isCurrentCharEscaped = false;

  while (aIter != aEnd && !IsSVGWhitespace(*aIter)) {
    if (isCurrentCharEscaped) {
      isCurrentCharEscaped = false;
    } else {
      if (*aIter == '+' || *aIter == '-' ||
          (aBreakOnDot && *aIter == '.')) {
        break;
      }
      if (*aIter == '\\') {
        isCurrentCharEscaped = true;
        aIsAnyCharEscaped = true;
      }
    }
    ++aIter;
  }
}

already_AddRefed<nsIAtom>
ConvertUnescapedTokenToAtom(const nsAString& aToken)
{
  // Whether the token is an id-ref or event-symbol it should be a valid NCName
  if (aToken.IsEmpty() || NS_FAILED(nsContentUtils::CheckQName(aToken, false)))
    return nullptr;
  return do_GetAtom(aToken);
}
    
already_AddRefed<nsIAtom>
ConvertTokenToAtom(const nsAString& aToken,
                   bool aUnescapeToken)
{
  // Unescaping involves making a copy of the string which we'd like to avoid if possible
  if (!aUnescapeToken) {
    return ConvertUnescapedTokenToAtom(aToken);
  }

  nsAutoString token(aToken);

  const char16_t* read = token.BeginReading();
  const char16_t* const end = token.EndReading();
  char16_t* write = token.BeginWriting();
  bool escape = false;

  while (read != end) {
    MOZ_ASSERT(write <= read, "Writing past where we've read");
    if (!escape && *read == '\\') {
      escape = true;
      ++read;
    } else {
      *write++ = *read++;
      escape = false;
    }
  }
  token.Truncate(write - token.BeginReading());

  return ConvertUnescapedTokenToAtom(token);
}

bool
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

  RangedPtr<const char16_t> start(SVGContentUtils::GetStartRangedPtr(aSpec));
  RangedPtr<const char16_t> end(SVGContentUtils::GetEndRangedPtr(aSpec));

  if (start == end) {
    return false;
  }

  RangedPtr<const char16_t> tokenEnd(start);

  bool requiresUnescaping;
  MoveToNextToken(tokenEnd, end, true, requiresUnescaping);

  nsRefPtr<nsIAtom> atom =
    ConvertTokenToAtom(Substring(start.get(), tokenEnd.get()),
                       requiresUnescaping);
  if (atom == nullptr) {
    return false;
  }

  // Parse the second token if there is one
  if (tokenEnd != end && *tokenEnd == '.') {
    result.mDependentElemID = atom;

    ++tokenEnd;
    start = tokenEnd;
    MoveToNextToken(tokenEnd, end, false, requiresUnescaping);

    const nsAString& token2 = Substring(start.get(), tokenEnd.get());

    // element-name.begin
    if (token2.EqualsLiteral("begin")) {
      result.mType = nsSMILTimeValueSpecParams::SYNCBASE;
      result.mSyncBegin = true;
    // element-name.end
    } else if (token2.EqualsLiteral("end")) {
      result.mType = nsSMILTimeValueSpecParams::SYNCBASE;
      result.mSyncBegin = false;
    // element-name.repeat(digit+)
    } else if (StringBeginsWith(token2, REPEAT_PREFIX)) {
      start += REPEAT_PREFIX.Length();
      int32_t repeatValue;
      if (start == tokenEnd || *start == '+' || *start == '-' ||
          !SVGContentUtils::ParseInteger(start, tokenEnd, repeatValue)) {
        return false;
      }
      if (start == tokenEnd || *start != ')') {
        return false;
      }
      result.mType = nsSMILTimeValueSpecParams::REPEAT;
      result.mRepeatIterationOrAccessKey = repeatValue;
    // element-name.event-symbol
    } else {
      atom = ConvertTokenToAtom(token2, requiresUnescaping);
      if (atom == nullptr) {
        return false;
      }
      result.mType = nsSMILTimeValueSpecParams::EVENT;
      result.mEventSymbol = atom;
    }
  } else {
    // event-symbol
    result.mType = nsSMILTimeValueSpecParams::EVENT;
    result.mEventSymbol = atom;
  }

  // We've reached the end of the token, so we should now be either looking at
  // a '+', '-' (possibly with whitespace before it), or the end.
  if (!ParseOptionalOffset(tokenEnd, end, &result.mOffset) || tokenEnd != end) {
    return false;
  }
  aResult = result;
  return true;
}

} // namespace

//------------------------------------------------------------------------------
// Implementation

const nsDependentSubstring
nsSMILParserUtils::TrimWhitespace(const nsAString& aString)
{
  nsAString::const_iterator start, end;

  aString.BeginReading(start);
  aString.EndReading(end);

  // Skip whitespace characters at the beginning
  while (start != end && IsSVGWhitespace(*start)) {
    ++start;
  }

  // Skip whitespace characters at the end.
  while (end != start) {
    --end;

    if (!IsSVGWhitespace(*end)) {
      // Step back to the last non-whitespace character.
      ++end;

      break;
    }
  }

  return Substring(start, end);
}

bool
nsSMILParserUtils::ParseKeySplines(const nsAString& aSpec,
                                   FallibleTArray<nsSMILKeySpline>& aKeySplines)
{
  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace> controlPointTokenizer(aSpec, ';');
  while (controlPointTokenizer.hasMoreTokens()) {

    nsCharSeparatedTokenizerTemplate<IsSVGWhitespace> 
      tokenizer(controlPointTokenizer.nextToken(), ',',
                nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);

    double values[4];
    for (int i = 0 ; i < 4; i++) {
      if (!tokenizer.hasMoreTokens() ||
          !SVGContentUtils::ParseNumber(tokenizer.nextToken(), values[i]) ||
          values[i] > 1.0 || values[i] < 0.0) {
        return false;
      }
    }
    if (tokenizer.hasMoreTokens() ||
        tokenizer.separatorAfterCurrentToken() ||
        !aKeySplines.AppendElement(nsSMILKeySpline(values[0],
                                                   values[1],
                                                   values[2],
                                                   values[3]),
                                   fallible)) {
      return false;
    }
  }

  return !aKeySplines.IsEmpty();
}

bool
nsSMILParserUtils::ParseSemicolonDelimitedProgressList(const nsAString& aSpec,
                                                       bool aNonDecreasing,
                                                       FallibleTArray<double>& aArray)
{
  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace> tokenizer(aSpec, ';');

  double previousValue = -1.0;

  while (tokenizer.hasMoreTokens()) {
    double value;
    if (!SVGContentUtils::ParseNumber(tokenizer.nextToken(), value)) {
      return false;
    }

    if (value > 1.0 || value < 0.0 ||
        (aNonDecreasing && value < previousValue)) {
      return false;
    }

    if (!aArray.AppendElement(value, fallible)) {
      return false;
    }
    previousValue = value;
  }

  return !aArray.IsEmpty();
}

// Helper class for ParseValues
class MOZ_STACK_CLASS SMILValueParser :
  public nsSMILParserUtils::GenericValueParser
{
public:
  SMILValueParser(const SVGAnimationElement* aSrcElement,
                  const nsISMILAttr* aSMILAttr,
                  FallibleTArray<nsSMILValue>* aValuesArray,
                  bool* aPreventCachingOfSandwich) :
    mSrcElement(aSrcElement),
    mSMILAttr(aSMILAttr),
    mValuesArray(aValuesArray),
    mPreventCachingOfSandwich(aPreventCachingOfSandwich)
  {}

  virtual bool Parse(const nsAString& aValueStr) override {
    nsSMILValue newValue;
    bool tmpPreventCachingOfSandwich = false;
    if (NS_FAILED(mSMILAttr->ValueFromString(aValueStr, mSrcElement, newValue,
                                             tmpPreventCachingOfSandwich)))
      return false;

    if (!mValuesArray->AppendElement(newValue, fallible)) {
      return false;
    }
    if (tmpPreventCachingOfSandwich) {
      *mPreventCachingOfSandwich = true;
    }
    return true;
  }
protected:
  const SVGAnimationElement* mSrcElement;
  const nsISMILAttr* mSMILAttr;
  FallibleTArray<nsSMILValue>* mValuesArray;
  bool* mPreventCachingOfSandwich;
};

bool
nsSMILParserUtils::ParseValues(const nsAString& aSpec,
                               const SVGAnimationElement* aSrcElement,
                               const nsISMILAttr& aAttribute,
                               FallibleTArray<nsSMILValue>& aValuesArray,
                               bool& aPreventCachingOfSandwich)
{
  // Assume all results can be cached, until we find one that can't.
  aPreventCachingOfSandwich = false;
  SMILValueParser valueParser(aSrcElement, &aAttribute,
                              &aValuesArray, &aPreventCachingOfSandwich);
  return ParseValuesGeneric(aSpec, valueParser);
}

bool
nsSMILParserUtils::ParseValuesGeneric(const nsAString& aSpec,
                                      GenericValueParser& aParser)
{
  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace> tokenizer(aSpec, ';');
  if (!tokenizer.hasMoreTokens()) { // Empty list
    return false;
  }

  while (tokenizer.hasMoreTokens()) {
    if (!aParser.Parse(tokenizer.nextToken())) {
      return false;
    }
  }

  return true;
}

bool
nsSMILParserUtils::ParseRepeatCount(const nsAString& aSpec,
                                    nsSMILRepeatCount& aResult)
{
  const nsAString& spec =
    nsSMILParserUtils::TrimWhitespace(aSpec);

  if (spec.EqualsLiteral("indefinite")) {
    aResult.SetIndefinite();
    return true;
  }

  double value;
  if (!SVGContentUtils::ParseNumber(spec, value) || value <= 0.0) {
    return false;
  }
  aResult = value;
  return true;
}

bool
nsSMILParserUtils::ParseTimeValueSpecParams(const nsAString& aSpec,
                                            nsSMILTimeValueSpecParams& aResult)
{
  const nsAString& spec = TrimWhitespace(aSpec);

  if (spec.EqualsLiteral("indefinite")) {
     aResult.mType = nsSMILTimeValueSpecParams::INDEFINITE;
     return true;
  }
  
  // offset type
  if (ParseOffsetValue(spec, &aResult.mOffset)) {
    aResult.mType = nsSMILTimeValueSpecParams::OFFSET;
    return true;
  }

  // wallclock type
  if (StringBeginsWith(spec, WALLCLOCK_PREFIX)) {
    return false; // Wallclock times not implemented
  }

  // accesskey type
  if (StringBeginsWith(spec, ACCESSKEY_PREFIX_LC) ||
      StringBeginsWith(spec, ACCESSKEY_PREFIX_CC)) {
    return ParseAccessKey(spec, aResult);
  }

  // event, syncbase, or repeat
  return ParseElementBaseTimeValueSpec(spec, aResult);
}

bool
nsSMILParserUtils::ParseClockValue(const nsAString& aSpec,
                                   nsSMILTimeValue* aResult)
{
  RangedPtr<const char16_t> iter(SVGContentUtils::GetStartRangedPtr(aSpec));
  RangedPtr<const char16_t> end(SVGContentUtils::GetEndRangedPtr(aSpec));

  return ::ParseClockValue(iter, end, aResult) && iter == end;
}

int32_t
nsSMILParserUtils::CheckForNegativeNumber(const nsAString& aStr)
{
  int32_t absValLocation = -1;

  nsAString::const_iterator start, end;
  aStr.BeginReading(start);
  aStr.EndReading(end);

  // Skip initial whitespace
  while (start != end && IsSVGWhitespace(*start)) {
    ++start;
  }

  // Check for dash
  if (start != end && *start == '-') {
    ++start;
    // Check for numeric character
    if (start != end && SVGContentUtils::IsDigit(*start)) {
      absValLocation = start.get() - start.start();
    }
  }
  return absValLocation;
}
