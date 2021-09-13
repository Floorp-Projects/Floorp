/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "unicode/ucal.h"
#include "unicode/udat.h"
#include "unicode/udatpg.h"

#include "ScopedICUObject.h"

#include "mozilla/intl/Calendar.h"
#include "mozilla/intl/DateTimeFormat.h"

namespace mozilla::intl {

DateTimeFormat::~DateTimeFormat() {
  MOZ_ASSERT(mDateFormat);
  udat_close(mDateFormat);
}

static UDateFormatStyle ToUDateFormatStyle(DateTimeStyle aStyle) {
  switch (aStyle) {
    case DateTimeStyle::Full:
      return UDAT_FULL;
    case DateTimeStyle::Long:
      return UDAT_LONG;
    case DateTimeStyle::Medium:
      return UDAT_MEDIUM;
    case DateTimeStyle::Short:
      return UDAT_SHORT;
    case DateTimeStyle::None:
      return UDAT_NONE;
  }
  MOZ_ASSERT_UNREACHABLE();
  // Do not use the default: branch so that the enum is exhaustively checked.
  return UDAT_NONE;
}

/**
 * Parse a pattern according to the format specified in
 * <https://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns>.
 */
template <typename CharT>
class PatternIterator {
  CharT* iter_;
  const CharT* const end_;

 public:
  explicit PatternIterator(mozilla::Span<CharT> pattern)
      : iter_(pattern.data()), end_(pattern.data() + pattern.size()) {}

  CharT* next() {
    MOZ_ASSERT(iter_ != nullptr);

    bool inQuote = false;
    while (iter_ < end_) {
      CharT* cur = iter_++;
      if (*cur == '\'') {
        inQuote = !inQuote;
      } else if (!inQuote) {
        return cur;
      }
    }

    iter_ = nullptr;
    return nullptr;
  }
};

/**
 * Return the hour cycle used in the input pattern or Nothing if none was found.
 */
template <typename CharT>
static mozilla::Maybe<HourCycle> HourCycleFromPattern(
    mozilla::Span<const CharT> pattern) {
  PatternIterator<const CharT> iter(pattern);
  while (const auto* ptr = iter.next()) {
    switch (*ptr) {
      case 'K':
        return mozilla::Some(HourCycle::H11);
      case 'h':
        return mozilla::Some(HourCycle::H12);
      case 'H':
        return mozilla::Some(HourCycle::H23);
      case 'k':
        return mozilla::Some(HourCycle::H24);
    }
  }
  return mozilla::Nothing();
}

static bool IsHour12(HourCycle hc) {
  return hc == HourCycle::H11 || hc == HourCycle::H12;
}

static char16_t HourSymbol(HourCycle hc) {
  switch (hc) {
    case HourCycle::H11:
      return 'K';
    case HourCycle::H12:
      return 'h';
    case HourCycle::H23:
      return 'H';
    case HourCycle::H24:
      return 'k';
  }
  MOZ_CRASH("unexpected hour cycle");
}

enum class PatternField { Hour, Minute, Second, Other };

template <typename CharT>
static PatternField ToPatternField(CharT ch) {
  if (ch == 'K' || ch == 'h' || ch == 'H' || ch == 'k' || ch == 'j') {
    return PatternField::Hour;
  }
  if (ch == 'm') {
    return PatternField::Minute;
  }
  if (ch == 's') {
    return PatternField::Second;
  }
  return PatternField::Other;
}

/**
 * Replaces all hour pattern characters in |patternOrSkeleton| to use the
 * matching hour representation for |hourCycle|.
 */
static void ReplaceHourSymbol(mozilla::Span<char16_t> patternOrSkeleton,
                              HourCycle hc) {
  char16_t replacement = HourSymbol(hc);
  PatternIterator<char16_t> iter(patternOrSkeleton);
  while (auto* ptr = iter.next()) {
    auto field = ToPatternField(*ptr);
    if (field == PatternField::Hour) {
      *ptr = replacement;
    }
  }
}

/**
 * Find a matching pattern using the requested hour-12 options.
 *
 * This function is needed to work around the following two issues.
 * - https://unicode-org.atlassian.net/browse/ICU-21023
 * - https://unicode-org.atlassian.net/browse/CLDR-13425
 *
 * We're currently using a relatively simple workaround, which doesn't give the
 * most accurate results. For example:
 *
 * ```
 * var dtf = new Intl.DateTimeFormat("en", {
 *   timeZone: "UTC",
 *   dateStyle: "long",
 *   timeStyle: "long",
 *   hourCycle: "h12",
 * });
 * print(dtf.format(new Date("2020-01-01T00:00Z")));
 * ```
 *
 * Returns the pattern "MMMM d, y 'at' h:mm:ss a z", but when going through
 * |DateTimePatternGenerator::GetSkeleton| and then
 * |DateTimePatternGenerator::GetBestPattern| to find an equivalent pattern for
 * "h23", we'll end up with the pattern "MMMM d, y, HH:mm:ss z", so the
 * combinator element " 'at' " was lost in the process.
 */
template <size_t N>
static bool FindPatternWithHourCycle(JSContext* cx, const char* locale,
                                     FormatBuffer<char16_t, N>& pattern,
                                     bool hour12) {
  SharedIntlData& sharedIntlData = cx->runtime()->sharedIntlData.ref();
  mozilla::intl::DateTimePatternGenerator* gen =
      sharedIntlData.getDateTimePatternGenerator(cx, locale);
  if (!gen) {
    return false;
  }

  FormatBuffer<char16_t, intl::INITIAL_CHAR_BUFFER_SIZE> skeleton(cx);
  auto skelResult =
      mozilla::intl::DateTimePatternGenerator::GetSkeleton(pattern, skeleton);
  if (skelResult.isErr()) {
    intl::ReportInternalError(cx, skelResult.unwrapErr());
    return false;
  }

  // Input skeletons don't differentiate between "K" and "h" resp. "k" and "H".
  ReplaceHourSymbol(skeleton, hour12 ? HourCycle::H12 : HourCycle::H23);

  auto result = gen->GetBestPattern(skeleton, pattern);
  if (result.isErr()) {
    intl::ReportInternalError(cx, result.unwrapErr());
    return false;
  }
  return true;
}

static auto PatternMatchOptions(mozilla::Span<const char16_t> skeleton) {
  // Values for hour, minute, and second are:
  // - absent: 0
  // - numeric: 1
  // - 2-digit: 2
  int32_t hour = 0;
  int32_t minute = 0;
  int32_t second = 0;

  PatternIterator<const char16_t> iter(skeleton);
  while (const auto* ptr = iter.next()) {
    switch (ToPatternField(*ptr)) {
      case PatternField::Hour:
        MOZ_ASSERT(hour < 2);
        hour += 1;
        break;
      case PatternField::Minute:
        MOZ_ASSERT(minute < 2);
        minute += 1;
        break;
      case PatternField::Second:
        MOZ_ASSERT(second < 2);
        second += 1;
        break;
      case PatternField::Other:
        break;
    }
  }

  // Adjust the field length when the user requested '2-digit' representation.
  //
  // We can't just always adjust the field length, because
  // 1. The default value for hour, minute, and second fields is 'numeric'. If
  //    the length is always adjusted, |date.toLocaleTime()| will start to
  //    return strings like "1:5:9 AM" instead of "1:05:09 AM".
  // 2. ICU doesn't support to adjust the field length to 'numeric' in certain
  //    cases. For example when the locale is "de" (German):
  //      a. hour='numeric' and minute='2-digit' will return "1:05".
  //      b. whereas hour='numeric' and minute='numeric' will return "01:05".
  //
  // Therefore we only support adjusting the field length when the user
  // explicitly requested the '2-digit' representation.

  using PatternMatchOption =
      mozilla::intl::DateTimePatternGenerator::PatternMatchOption;
  mozilla::EnumSet<PatternMatchOption> options;
  if (hour == 2) {
    options += PatternMatchOption::HourField;
  }
  if (minute == 2) {
    options += PatternMatchOption::MinuteField;
  }
  if (second == 2) {
    options += PatternMatchOption::SecondField;
  }
  return options;
}

/* static */
Result<UniquePtr<DateTimeFormat>, DateTimeFormat::StyleError>
DateTimeFormat::TryCreateFromStyle(
    Span<const char> aLocale, DateTimeStyle aDateStyle,
    DateTimeStyle aTimeStyle, Maybe<Span<const char16_t>> aTimeZoneOverride) {
  auto dateStyle = ToUDateFormatStyle(aDateStyle);
  auto timeStyle = ToUDateFormatStyle(aTimeStyle);

  if (dateStyle == UDAT_NONE && timeStyle == UDAT_NONE) {
    dateStyle = UDAT_DEFAULT;
    timeStyle = UDAT_DEFAULT;
  }

  // The time zone is optional.
  int32_t tzIDLength = -1;
  const UChar* tzID = nullptr;
  if (aTimeZoneOverride) {
    tzIDLength = static_cast<int32_t>(aTimeZoneOverride->size());
    tzID = aTimeZoneOverride->Elements();
  }

  UErrorCode status = U_ZERO_ERROR;
  UDateFormat* dateFormat =
      udat_open(dateStyle, timeStyle, aLocale.data(), tzID, tzIDLength,
                /* pattern */ nullptr, /* pattern length */ -1, &status);

  if (U_SUCCESS(status)) {
    return UniquePtr<DateTimeFormat>(new DateTimeFormat(dateFormat));
  }

  return Err(DateTimeFormat::StyleError::DateFormatFailure);
}

DateTimeFormat::DateTimeFormat(UDateFormat* aDateFormat) {
  MOZ_RELEASE_ASSERT(aDateFormat, "Expected aDateFormat to not be a nullptr.");
  mDateFormat = aDateFormat;
}

/* static */
Result<UniquePtr<DateTimeFormat>, DateTimeFormat::PatternError>
DateTimeFormat::TryCreateFromPattern(
    Span<const char> aLocale, Span<const char16_t> aPattern,
    Maybe<Span<const char16_t>> aTimeZoneOverride) {
  UErrorCode status = U_ZERO_ERROR;

  // The time zone is optional.
  int32_t tzIDLength = -1;
  const UChar* tzID = nullptr;
  if (aTimeZoneOverride) {
    tzIDLength = static_cast<int32_t>(aTimeZoneOverride->size());
    tzID = aTimeZoneOverride->data();
  }

  // Create the date formatter.
  UDateFormat* dateFormat = udat_open(
      UDAT_PATTERN, UDAT_PATTERN, static_cast<const char*>(aLocale.data()),
      tzID, tzIDLength, aPattern.data(), static_cast<int32_t>(aPattern.size()),
      &status);

  if (U_FAILURE(status)) {
    return Err(PatternError::DateFormatFailure);
  }

  // The DateTimeFormat wrapper will control the life cycle of the ICU
  // dateFormat object.
  return UniquePtr<DateTimeFormat>(new DateTimeFormat(dateFormat));
}

/* static */
Result<UniquePtr<DateTimeFormat>, DateTimeFormat::SkeletonError>
DateTimeFormat::TryCreateFromSkeleton(
    Span<const char> aLocale, Span<const char16_t> aSkeleton,
    Maybe<Span<const char16_t>> aTimeZoneOverride) {
  UErrorCode status = U_ZERO_ERROR;

  // Create a time pattern generator. Its lifetime is scoped to this function.
  UDateTimePatternGenerator* dtpg = udatpg_open(aLocale.data(), &status);
  if (U_FAILURE(status)) {
    return Err(SkeletonError::PatternGeneratorFailure);
  }
  ScopedICUObject<UDateTimePatternGenerator, udatpg_close> datPgToClose(dtpg);

  // Compute the best pattern for the skeleton.
  mozilla::Vector<char16_t, DateTimeFormat::StackU16VectorSize> bestPattern;

  auto result = FillVectorWithICUCall(
      bestPattern,
      [&dtpg, &aSkeleton](UChar* target, int32_t length, UErrorCode* status) {
        return udatpg_getBestPattern(dtpg, aSkeleton.data(),
                                     static_cast<int32_t>(aSkeleton.size()),
                                     target, length, status);
      });

  if (result.isErr()) {
    return Err(SkeletonError::GetBestPatternFailure);
  }

  return DateTimeFormat::TryCreateFromPattern(aLocale, bestPattern,
                                              aTimeZoneOverride)
      .mapErr([](DateTimeFormat::PatternError error) {
        switch (error) {
          case DateTimeFormat::PatternError::DateFormatFailure:
            return SkeletonError::DateFormatFailure;
        }
        // Do not use the default branch, so that the switch is exhaustively
        // checked.
        MOZ_ASSERT_UNREACHABLE();
        return SkeletonError::DateFormatFailure;
      });
}

/* static */
Result<UniquePtr<DateTimeFormat>, DateTimeFormat::SkeletonError>
DateTimeFormat::TryCreateFromSkeleton(
    Span<const char> aLocale, Span<const char> aSkeleton,
    Maybe<Span<const char>> aTimeZoneOverride) {
  // Convert the skeleton to UTF-16.
  mozilla::Vector<char16_t, DateTimeFormat::StackU16VectorSize>
      skeletonUtf16Buffer;

  if (!FillUTF16Vector(aSkeleton, skeletonUtf16Buffer)) {
    return Err(SkeletonError::OutOfMemory);
  }

  // Convert the timezone to UTF-16 if it exists.
  mozilla::Vector<char16_t, DateTimeFormat::StackU16VectorSize> tzUtf16Vec;
  Maybe<Span<const char16_t>> timeZone = Nothing{};
  if (aTimeZoneOverride) {
    if (!FillUTF16Vector(*aTimeZoneOverride, tzUtf16Vec)) {
      return Err(SkeletonError::OutOfMemory);
    };
    timeZone =
        Some(Span<const char16_t>(tzUtf16Vec.begin(), tzUtf16Vec.length()));
  }

  return DateTimeFormat::TryCreateFromSkeleton(aLocale, skeletonUtf16Buffer,
                                               timeZone);
}

void DateTimeFormat::SetStartTimeIfGregorian(double aTime) {
  UErrorCode status = U_ZERO_ERROR;
  UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(mDateFormat));
  ucal_setGregorianChange(cal, aTime, &status);
  // An error here means the calendar is not Gregorian, and can be ignored.
}

/* static */
Result<UniquePtr<Calendar>, InternalError> DateTimeFormat::CloneCalendar(
    double aUnixEpoch) const {
  UErrorCode status = U_ZERO_ERROR;
  UCalendar* calendarRaw = ucal_clone(udat_getCalendar(mDateFormat), &status);
  if (U_FAILURE(status)) {
    return Err(InternalError{});
  }
  auto calendar = MakeUnique<Calendar>(calendarRaw);

  auto setTimeResult = calendar->SetTimeInMs(aUnixEpoch);
  if (setTimeResult.isErr()) {
    return Err(InternalError{});
  }
  return calendar;
}

}  // namespace mozilla::intl
