/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "NumberFormatterSkeleton.h"
#include "NumberFormat.h"

#include "MeasureUnitGenerated.h"

#include "mozilla/RangedPtr.h"

#include <algorithm>
#include <limits>

#include "unicode/unumberrangeformatter.h"

namespace mozilla::intl {

NumberFormatterSkeleton::NumberFormatterSkeleton(
    const NumberFormatOptions& options) {
  if (options.mCurrency.isSome()) {
    if (!currency(options.mCurrency->first) ||
        !currencyDisplay(options.mCurrency->second)) {
      return;
    }
  } else if (options.mUnit.isSome()) {
    if (!unit(options.mUnit->first) || !unitDisplay(options.mUnit->second)) {
      return;
    }
  } else if (options.mPercent) {
    if (!percent()) {
      return;
    }
  }

  if (options.mRoundingIncrement != 1) {
    auto fd = options.mFractionDigits.valueOr(std::pair{0, 0});
    if (!roundingIncrement(options.mRoundingIncrement, fd.first, fd.second,
                           options.mStripTrailingZero)) {
      return;
    }
  } else if (options.mRoundingPriority ==
             NumberFormatOptions::RoundingPriority::Auto) {
    if (options.mFractionDigits.isSome()) {
      if (!fractionDigits(options.mFractionDigits->first,
                          options.mFractionDigits->second,
                          options.mStripTrailingZero)) {
        return;
      }
    }

    if (options.mSignificantDigits.isSome()) {
      if (!significantDigits(options.mSignificantDigits->first,
                             options.mSignificantDigits->second,
                             options.mStripTrailingZero)) {
        return;
      }
    }
  } else {
    MOZ_ASSERT(options.mFractionDigits);
    MOZ_ASSERT(options.mSignificantDigits);

    bool relaxed = options.mRoundingPriority ==
                   NumberFormatOptions::RoundingPriority::MorePrecision;
    if (!fractionWithSignificantDigits(options.mFractionDigits->first,
                                       options.mFractionDigits->second,
                                       options.mSignificantDigits->first,
                                       options.mSignificantDigits->second,
                                       relaxed, options.mStripTrailingZero)) {
      return;
    }
  }

  if (options.mMinIntegerDigits.isSome()) {
    if (!minIntegerDigits(*options.mMinIntegerDigits)) {
      return;
    }
  }

  if (!grouping(options.mGrouping)) {
    return;
  }

  if (!notation(options.mNotation)) {
    return;
  }

  if (!signDisplay(options.mSignDisplay)) {
    return;
  }

  if (!roundingMode(options.mRoundingMode)) {
    return;
  }

  mValidSkeleton = true;
}

bool NumberFormatterSkeleton::currency(std::string_view currency) {
  MOZ_ASSERT(currency.size() == 3,
             "IsWellFormedCurrencyCode permits only length-3 strings");

  char16_t currencyChars[] = {static_cast<char16_t>(currency[0]),
                              static_cast<char16_t>(currency[1]),
                              static_cast<char16_t>(currency[2]), '\0'};
  return append(u"currency/") && append(currencyChars) && append(' ');
}

bool NumberFormatterSkeleton::currencyDisplay(
    NumberFormatOptions::CurrencyDisplay display) {
  switch (display) {
    case NumberFormatOptions::CurrencyDisplay::Code:
      return appendToken(u"unit-width-iso-code");
    case NumberFormatOptions::CurrencyDisplay::Name:
      return appendToken(u"unit-width-full-name");
    case NumberFormatOptions::CurrencyDisplay::Symbol:
      // Default, no additional tokens needed.
      return true;
    case NumberFormatOptions::CurrencyDisplay::NarrowSymbol:
      return appendToken(u"unit-width-narrow");
  }
  MOZ_ASSERT_UNREACHABLE("unexpected currency display type");
  return false;
}

static const SimpleMeasureUnit& FindSimpleMeasureUnit(std::string_view name) {
  const auto* measureUnit = std::lower_bound(
      std::begin(simpleMeasureUnits), std::end(simpleMeasureUnits), name,
      [](const auto& measureUnit, std::string_view name) {
        return name.compare(measureUnit.name) > 0;
      });
  MOZ_ASSERT(measureUnit != std::end(simpleMeasureUnits),
             "unexpected unit identifier: unit not found");
  MOZ_ASSERT(measureUnit->name == name,
             "unexpected unit identifier: wrong unit found");
  return *measureUnit;
}

static constexpr size_t MaxUnitLength() {
  size_t length = 0;
  for (const auto& unit : simpleMeasureUnits) {
    length = std::max(length, std::char_traits<char>::length(unit.name));
  }
  return length * 2 + std::char_traits<char>::length("-per-");
}

bool NumberFormatterSkeleton::unit(std::string_view unit) {
  MOZ_RELEASE_ASSERT(unit.length() <= MaxUnitLength());

  auto appendUnit = [this](const SimpleMeasureUnit& unit) {
    return append(unit.type, strlen(unit.type)) && append('-') &&
           append(unit.name, strlen(unit.name));
  };

  // |unit| can be a compound unit identifier, separated by "-per-".
  static constexpr char separator[] = "-per-";
  size_t separator_len = strlen(separator);
  size_t offset = unit.find(separator);
  if (offset != std::string_view::npos) {
    const auto& numerator = FindSimpleMeasureUnit(unit.substr(0, offset));
    const auto& denominator = FindSimpleMeasureUnit(
        std::string_view(unit.data() + offset + separator_len,
                         unit.length() - offset - separator_len));
    return append(u"measure-unit/") && appendUnit(numerator) && append(' ') &&
           append(u"per-measure-unit/") && appendUnit(denominator) &&
           append(' ');
  }

  const auto& simple = FindSimpleMeasureUnit(unit);
  return append(u"measure-unit/") && appendUnit(simple) && append(' ');
}

bool NumberFormatterSkeleton::unitDisplay(
    NumberFormatOptions::UnitDisplay display) {
  switch (display) {
    case NumberFormatOptions::UnitDisplay::Short:
      return appendToken(u"unit-width-short");
    case NumberFormatOptions::UnitDisplay::Narrow:
      return appendToken(u"unit-width-narrow");
    case NumberFormatOptions::UnitDisplay::Long:
      return appendToken(u"unit-width-full-name");
  }
  MOZ_ASSERT_UNREACHABLE("unexpected unit display type");
  return false;
}

bool NumberFormatterSkeleton::percent() {
  return appendToken(u"percent scale/100");
}

bool NumberFormatterSkeleton::fractionDigits(uint32_t min, uint32_t max,
                                             bool stripTrailingZero) {
  // Note: |min| can be zero here.
  MOZ_ASSERT(min <= max);
  if (!append('.') || !appendN('0', min) || !appendN('#', max - min)) {
    return false;
  }
  if (stripTrailingZero) {
    if (!append(u"/w")) {
      return false;
    }
  }
  return append(' ');
}

bool NumberFormatterSkeleton::fractionWithSignificantDigits(
    uint32_t mnfd, uint32_t mxfd, uint32_t mnsd, uint32_t mxsd, bool relaxed,
    bool stripTrailingZero) {
  // Note: |mnfd| can be zero here.
  MOZ_ASSERT(mnfd <= mxfd);
  MOZ_ASSERT(mnsd > 0);
  MOZ_ASSERT(mnsd <= mxsd);

  if (!append('.') || !appendN('0', mnfd) || !appendN('#', mxfd - mnfd)) {
    return false;
  }
  if (!append('/') || !appendN('@', mnsd) || !appendN('#', mxsd - mnsd)) {
    return false;
  }
  if (!append(relaxed ? 'r' : 's')) {
    return false;
  }
  if (stripTrailingZero) {
    if (!append(u"/w")) {
      return false;
    }
  }
  return append(' ');
}

bool NumberFormatterSkeleton::minIntegerDigits(uint32_t min) {
  MOZ_ASSERT(min > 0);
  return append(u"integer-width/+") && appendN('0', min) && append(' ');
}

bool NumberFormatterSkeleton::significantDigits(uint32_t min, uint32_t max,
                                                bool stripTrailingZero) {
  MOZ_ASSERT(min > 0);
  MOZ_ASSERT(min <= max);
  if (!appendN('@', min) || !appendN('#', max - min)) {
    return false;
  }
  if (stripTrailingZero) {
    if (!append(u"/w")) {
      return false;
    }
  }
  return append(' ');
}

bool NumberFormatterSkeleton::grouping(NumberFormatOptions::Grouping grouping) {
  switch (grouping) {
    case NumberFormatOptions::Grouping::Auto:
      // Default, no additional tokens needed.
      return true;
    case NumberFormatOptions::Grouping::Always:
      return appendToken(u"group-on-aligned");
    case NumberFormatOptions::Grouping::Min2:
      return appendToken(u"group-min2");
    case NumberFormatOptions::Grouping::Never:
      return appendToken(u"group-off");
  }
  MOZ_ASSERT_UNREACHABLE("unexpected grouping mode");
  return false;
}

bool NumberFormatterSkeleton::notation(NumberFormatOptions::Notation style) {
  switch (style) {
    case NumberFormatOptions::Notation::Standard:
      // Default, no additional tokens needed.
      return true;
    case NumberFormatOptions::Notation::Scientific:
      return appendToken(u"scientific");
    case NumberFormatOptions::Notation::Engineering:
      return appendToken(u"engineering");
    case NumberFormatOptions::Notation::CompactShort:
      return appendToken(u"compact-short");
    case NumberFormatOptions::Notation::CompactLong:
      return appendToken(u"compact-long");
  }
  MOZ_ASSERT_UNREACHABLE("unexpected notation style");
  return false;
}

bool NumberFormatterSkeleton::signDisplay(
    NumberFormatOptions::SignDisplay display) {
  switch (display) {
    case NumberFormatOptions::SignDisplay::Auto:
      // Default, no additional tokens needed.
      return true;
    case NumberFormatOptions::SignDisplay::Always:
      return appendToken(u"sign-always");
    case NumberFormatOptions::SignDisplay::Never:
      return appendToken(u"sign-never");
    case NumberFormatOptions::SignDisplay::ExceptZero:
      return appendToken(u"sign-except-zero");
    case NumberFormatOptions::SignDisplay::Negative:
      return appendToken(u"sign-negative");
    case NumberFormatOptions::SignDisplay::Accounting:
      return appendToken(u"sign-accounting");
    case NumberFormatOptions::SignDisplay::AccountingAlways:
      return appendToken(u"sign-accounting-always");
    case NumberFormatOptions::SignDisplay::AccountingExceptZero:
      return appendToken(u"sign-accounting-except-zero");
    case NumberFormatOptions::SignDisplay::AccountingNegative:
      return appendToken(u"sign-accounting-negative");
  }
  MOZ_ASSERT_UNREACHABLE("unexpected sign display type");
  return false;
}

bool NumberFormatterSkeleton::roundingIncrement(uint32_t increment,
                                                uint32_t mnfd, uint32_t mxfd,
                                                bool stripTrailingZero) {
  // Note: |mnfd| can be zero here.
  MOZ_ASSERT(mnfd <= mxfd);
  MOZ_ASSERT(increment > 1);

  // Limit |mxfd| to 100. (20 is the current limit for ECMA-402, but there are
  // plans to change it to 100.)
  constexpr size_t maxFracDigits = 100;
  MOZ_RELEASE_ASSERT(mxfd <= maxFracDigits);

  static constexpr char digits[] = "0123456789";

  // We need enough space to print any uint32_t, which is possibly shifted by
  // |mxfd| decimal places. And additionally we need to reserve space for "0.".
  static_assert(std::numeric_limits<uint32_t>::digits10 + 1 < maxFracDigits);
  constexpr size_t maxLength = maxFracDigits + 2;

  char chars[maxLength];
  RangedPtr<char> ptr(chars + maxLength, chars, maxLength);
  const RangedPtr<char> end = ptr;

  // Convert to a signed integer, so we don't have to worry about underflows.
  int32_t maxFrac = int32_t(mxfd);

  // Write |increment| from back to front.
  while (increment != 0) {
    *--ptr = digits[increment % 10];
    increment /= 10;
    maxFrac -= 1;

    if (maxFrac == 0) {
      *--ptr = '.';
    }
  }

  // Write any remaining zeros from |mxfd| and prepend '0' if we last wrote the
  // decimal point.
  while (maxFrac >= 0) {
    MOZ_ASSERT_IF(maxFrac == 0, *ptr == '.');

    *--ptr = '0';
    maxFrac -= 1;

    if (maxFrac == 0) {
      *--ptr = '.';
    }
  }

  MOZ_ASSERT(ptr < end, "At least one character is written.");
  MOZ_ASSERT(*ptr != '.', "First character is a digit.");

  if (!append(u"precision-increment/") || !append(ptr.get(), end - ptr)) {
    return false;
  }
  if (stripTrailingZero) {
    if (!append(u"/w")) {
      return false;
    }
  }
  return append(' ');
}

bool NumberFormatterSkeleton::roundingMode(
    NumberFormatOptions::RoundingMode rounding) {
  switch (rounding) {
    case NumberFormatOptions::RoundingMode::Ceil:
      return appendToken(u"rounding-mode-ceiling");
    case NumberFormatOptions::RoundingMode::Floor:
      return appendToken(u"rounding-mode-floor");
    case NumberFormatOptions::RoundingMode::Expand:
      return appendToken(u"rounding-mode-up");
    case NumberFormatOptions::RoundingMode::Trunc:
      return appendToken(u"rounding-mode-down");
    case NumberFormatOptions::RoundingMode::HalfCeil:
      return appendToken(u"rounding-mode-half-ceiling");
    case NumberFormatOptions::RoundingMode::HalfFloor:
      return appendToken(u"rounding-mode-half-floor");
    case NumberFormatOptions::RoundingMode::HalfExpand:
      return appendToken(u"rounding-mode-half-up");
    case NumberFormatOptions::RoundingMode::HalfTrunc:
      return appendToken(u"rounding-mode-half-down");
    case NumberFormatOptions::RoundingMode::HalfEven:
      return appendToken(u"rounding-mode-half-even");
    case NumberFormatOptions::RoundingMode::HalfOdd:
      return appendToken(u"rounding-mode-half-odd");
  }
  MOZ_ASSERT_UNREACHABLE("unexpected rounding mode");
  return false;
}

UNumberFormatter* NumberFormatterSkeleton::toFormatter(
    std::string_view locale) {
  if (!mValidSkeleton) {
    return nullptr;
  }

  UErrorCode status = U_ZERO_ERROR;
  UNumberFormatter* nf = unumf_openForSkeletonAndLocale(
      mVector.begin(), mVector.length(), AssertNullTerminatedString(locale),
      &status);
  if (U_FAILURE(status)) {
    return nullptr;
  }
  return nf;
}

static UNumberRangeCollapse ToUNumberRangeCollapse(
    NumberRangeFormatOptions::RangeCollapse collapse) {
  using RangeCollapse = NumberRangeFormatOptions::RangeCollapse;
  switch (collapse) {
    case RangeCollapse::Auto:
      return UNUM_RANGE_COLLAPSE_AUTO;
    case RangeCollapse::None:
      return UNUM_RANGE_COLLAPSE_NONE;
    case RangeCollapse::Unit:
      return UNUM_RANGE_COLLAPSE_UNIT;
    case RangeCollapse::All:
      return UNUM_RANGE_COLLAPSE_ALL;
  }
  MOZ_ASSERT_UNREACHABLE("unexpected range collapse");
  return UNUM_RANGE_COLLAPSE_NONE;
}

static UNumberRangeIdentityFallback ToUNumberRangeIdentityFallback(
    NumberRangeFormatOptions::RangeIdentityFallback identity) {
  using RangeIdentityFallback = NumberRangeFormatOptions::RangeIdentityFallback;
  switch (identity) {
    case RangeIdentityFallback::SingleValue:
      return UNUM_IDENTITY_FALLBACK_SINGLE_VALUE;
    case RangeIdentityFallback::ApproximatelyOrSingleValue:
      return UNUM_IDENTITY_FALLBACK_APPROXIMATELY_OR_SINGLE_VALUE;
    case RangeIdentityFallback::Approximately:
      return UNUM_IDENTITY_FALLBACK_APPROXIMATELY;
    case RangeIdentityFallback::Range:
      return UNUM_IDENTITY_FALLBACK_RANGE;
  }
  MOZ_ASSERT_UNREACHABLE("unexpected range identity fallback");
  return UNUM_IDENTITY_FALLBACK_RANGE;
}

UNumberRangeFormatter* NumberFormatterSkeleton::toRangeFormatter(
    std::string_view locale, NumberRangeFormatOptions::RangeCollapse collapse,
    NumberRangeFormatOptions::RangeIdentityFallback identity) {
  if (!mValidSkeleton) {
    return nullptr;
  }

  UParseError* perror = nullptr;
  UErrorCode status = U_ZERO_ERROR;
  UNumberRangeFormatter* nrf =
      unumrf_openForSkeletonWithCollapseAndIdentityFallback(
          mVector.begin(), mVector.length(), ToUNumberRangeCollapse(collapse),
          ToUNumberRangeIdentityFallback(identity),
          AssertNullTerminatedString(locale), perror, &status);
  if (U_FAILURE(status)) {
    return nullptr;
  }
  return nrf;
}

}  // namespace mozilla::intl
