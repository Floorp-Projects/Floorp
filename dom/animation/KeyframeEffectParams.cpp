/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/KeyframeEffectParams.h"

#include "mozilla/AnimationUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/KeyframeUtils.h"
#include "mozilla/RangedPtr.h"
#include "nsReadableUtils.h"

namespace mozilla {

static inline bool
IsLetter(char16_t aCh)
{
  return (0x41 <= aCh && aCh <= 0x5A) || (0x61 <= aCh && aCh <= 0x7A);
}

static inline bool
IsDigit(char16_t aCh)
{
  return 0x30 <= aCh && aCh <= 0x39;
}

static inline bool
IsNameStartCode(char16_t aCh)
{
  return IsLetter(aCh) || aCh >= 0x80 || aCh == '_';
}

static inline bool
IsNameCode(char16_t aCh)
{
  return IsNameStartCode(aCh) || IsDigit(aCh) || aCh == '-';
}

static inline bool
IsNewLine(char16_t aCh)
{
  // 0x0A (LF), 0x0C (FF), 0x0D (CR), or pairs of CR followed by LF are
  // replaced by LF.
  return aCh == 0x0A || aCh == 0x0C || aCh == 0x0D;
}

static inline bool
IsValidEscape(char16_t aFirst, char16_t aSecond)
{
  return aFirst == '\\' && !IsNewLine(aSecond);
}

static bool
IsIdentStart(RangedPtr<const char16_t> aIter,
             const char16_t* const aEnd)
{
  if (aIter == aEnd) {
    return false;
  }

  if (*aIter == '-') {
    if (aIter + 1 == aEnd) {
      return false;
    }
    char16_t second = *(aIter + 1);
    return IsNameStartCode(second) ||
           second == '-' ||
           (aIter + 2 != aEnd && IsValidEscape(second, *(aIter + 2)));
  }
  return IsNameStartCode(*aIter) ||
         (aIter + 1 != aEnd && IsValidEscape(*aIter, *(aIter + 1)));
}

static void
ConsumeIdentToken(RangedPtr<const char16_t>& aIter,
                  const char16_t* const aEnd,
                  nsAString& aResult)
{
  aResult.Truncate();

  // Check if it starts with an identifier.
  if (!IsIdentStart(aIter, aEnd)) {
    return;
  }

  // Start to consume.
  while (aIter != aEnd) {
    if (IsNameCode(*aIter)) {
      aResult.Append(*aIter);
    } else if (*aIter == '\\') {
      const RangedPtr<const char16_t> secondChar = aIter + 1;
      if (secondChar == aEnd || !IsValidEscape(*aIter, *secondChar)) {
        break;
      }
      // Consume '\\' and append the character following this '\\'.
      ++aIter;
      aResult.Append(*aIter);
    } else {
      break;
    }
    ++aIter;
  }
}

/* static */ void
KeyframeEffectParams::ParseSpacing(const nsAString& aSpacing,
                                   SpacingMode& aSpacingMode,
                                   nsCSSPropertyID& aPacedProperty,
                                   nsAString& aInvalidPacedProperty,
                                   dom::CallerType aCallerType,
                                   ErrorResult& aRv)
{
  aInvalidPacedProperty.Truncate();

  // Ignore spacing if the core API is not enabled since it is not yet ready to
  // ship.
  if (!AnimationUtils::IsCoreAPIEnabledForCaller(aCallerType)) {
    aSpacingMode = SpacingMode::distribute;
    return;
  }

  // Parse spacing.
  // distribute | paced({ident})
  // https://w3c.github.io/web-animations/#dom-keyframeeffectreadonly-spacing
  // 1. distribute spacing.
  if (aSpacing.EqualsLiteral("distribute")) {
    aSpacingMode = SpacingMode::distribute;
    return;
  }

  // 2. paced spacing.
  static const nsLiteralString kPacedPrefix = NS_LITERAL_STRING("paced(");
  if (!StringBeginsWith(aSpacing, kPacedPrefix)) {
    aRv.ThrowTypeError<dom::MSG_INVALID_SPACING_MODE_ERROR>(aSpacing);
    return;
  }

  RangedPtr<const char16_t> iter(aSpacing.Data() + kPacedPrefix.Length(),
                                 aSpacing.Data(), aSpacing.Length());
  const char16_t* const end = aSpacing.EndReading();

  nsAutoString identToken;
  ConsumeIdentToken(iter, end, identToken);
  if (identToken.IsEmpty()) {
    aRv.ThrowTypeError<dom::MSG_INVALID_SPACING_MODE_ERROR>(aSpacing);
    return;
  }

  aPacedProperty =
    nsCSSProps::LookupProperty(identToken, CSSEnabledState::eForAllContent);
  if (aPacedProperty == eCSSProperty_UNKNOWN ||
      aPacedProperty == eCSSPropertyExtra_variable ||
      !KeyframeUtils::IsAnimatableProperty(aPacedProperty)) {
    aPacedProperty = eCSSProperty_UNKNOWN;
    aInvalidPacedProperty = identToken;
  }

  if (end - iter.get() != 1 || *iter != ')') {
    aRv.ThrowTypeError<dom::MSG_INVALID_SPACING_MODE_ERROR>(aSpacing);
    return;
  }

  aSpacingMode = aPacedProperty == eCSSProperty_UNKNOWN
                 ? SpacingMode::distribute
                 : SpacingMode::paced;
}

} // namespace mozilla
