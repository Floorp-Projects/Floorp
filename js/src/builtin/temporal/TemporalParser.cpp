/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/TemporalParser.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/TextUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Duration.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/TimeZone.h"
#include "gc/Barrier.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCAPI.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "util/Text.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/StringType.h"

using namespace js;
using namespace js::temporal;

// TODO: Better error message for empty strings?
// TODO: Add string input to error message?
// TODO: Better error messages, for example display current character?

struct StringName final {
  // Start position and length of this name.
  size_t start = 0;
  size_t length = 0;

  bool present() const { return length > 0; }
};

static JSLinearString* ToString(JSContext* cx, JSString* string,
                                const StringName& name) {
  MOZ_ASSERT(name.present());
  return NewDependentString(cx, string, name.start, name.length);
}

template <typename CharT>
bool EqualCharIgnoreCaseAscii(CharT c1, char c2) {
  if constexpr (sizeof(CharT) > sizeof(char)) {
    if (!mozilla::IsAscii(c1)) {
      return false;
    }
  }

  static constexpr auto toLower = 0x20;
  static_assert('a' - 'A' == toLower);

  // Convert both characters to lower case before the comparison.
  char c = c1;
  if (mozilla::IsAsciiUppercaseAlpha(c1)) {
    c = c + toLower;
  }
  char d = c2;
  if (mozilla::IsAsciiUppercaseAlpha(c2)) {
    d = d + toLower;
  }
  return c == d;
}

using CalendarName = StringName;
using AnnotationKey = StringName;
using AnnotationValue = StringName;
using TimeZoneName = StringName;

struct Annotation final {
  AnnotationKey key;
  AnnotationValue value;
  bool critical = false;
};

struct TimeSpec final {
  PlainTime time;
};

struct TimeZoneOffset final {
  // ±1 for time zones with an offset, otherwise 0.
  int32_t sign = 0;

  // An integer in the range [0, 23].
  int32_t hour = 0;

  // An integer in the range [0, 59].
  int32_t minute = 0;

  // An integer in the range [0, 59].
  int32_t second = 0;

  // An integer in the range [0, 999'999].
  int32_t fractionalPart = 0;
};

/**
 * ParseTimeZoneOffsetString ( offsetString )
 */
static int64_t ParseTimeZoneOffsetString(const TimeZoneOffset& offset) {
  constexpr int64_t nanoPerSec = 1'000'000'000;

  MOZ_ASSERT(offset.sign == -1 || offset.sign == +1);
  MOZ_ASSERT(0 <= offset.hour && offset.hour < 24);
  MOZ_ASSERT(0 <= offset.minute && offset.minute < 60);
  MOZ_ASSERT(0 <= offset.second && offset.second < 60);
  MOZ_ASSERT(0 <= offset.fractionalPart && offset.fractionalPart < nanoPerSec);

  // sign × (((hours × 60 + minutes) × 60 + seconds) × 10^9 + nanoseconds).
  int64_t seconds = (offset.hour * 60 + offset.minute) * 60 + offset.second;
  int64_t nanos = (seconds * nanoPerSec) + offset.fractionalPart;
  int64_t result = offset.sign * nanos;

  MOZ_ASSERT(std::abs(result) < ToNanoseconds(TemporalUnit::Day),
             "time zone offset is less than 24:00 hours");

  return result;
}

/**
 * Struct to hold time zone annotations.
 */
struct TimeZoneAnnotation final {
  // Time zone offset.
  TimeZoneOffset offset;

  // Time zone name.
  TimeZoneName name;

  /**
   * Returns true iff the time zone has an offset part, e.g. "+01:00".
   */
  bool hasOffset() const { return offset.sign != 0; }

  /**
   * Returns true iff the time zone has an IANA name, e.g. "Asia/Tokyo".
   */
  bool hasName() const { return name.present(); }
};

/**
 * Struct to hold any time zone parts of a parsed string.
 */
struct TimeZoneString final {
  // Time zone offset.
  TimeZoneOffset offset;

  // Time zone name.
  TimeZoneName name;

  // Time zone annotation;
  TimeZoneAnnotation annotation;

  // UTC time zone.
  bool utc = false;

  static auto from(TimeZoneOffset offset) {
    TimeZoneString timeZone{};
    timeZone.offset = offset;
    return timeZone;
  }

  static auto from(TimeZoneName name) {
    TimeZoneString timeZone{};
    timeZone.name = name;
    return timeZone;
  }

  static auto UTC() {
    TimeZoneString timeZone{};
    timeZone.utc = true;
    return timeZone;
  }

  /**
   * Returns true iff the time zone has an offset part, e.g. "+01:00".
   */
  bool hasOffset() const { return offset.sign != 0; }

  /**
   * Returns true iff the time zone has an IANA name, e.g. "Asia/Tokyo".
   */
  bool hasName() const { return name.present(); }

  /**
   * Returns true iff the time zone has an annotation.
   */
  bool hasAnnotation() const {
    return annotation.hasName() || annotation.hasOffset();
  }

  /**
   * Returns true iff the time zone uses the "Z" abbrevation to denote UTC time.
   */
  bool isUTC() const { return utc; }
};

/**
 * Struct to hold the parsed date, time, time zone, and calendar components.
 */
struct ZonedDateTimeString final {
  PlainDate date;
  PlainTime time;
  TimeZoneString timeZone;
  CalendarName calendar;
};

template <typename CharT>
static bool IsISO8601Calendar(mozilla::Span<const CharT> calendar) {
  static constexpr std::string_view iso8601 = "iso8601";

  if (calendar.size() != iso8601.length()) {
    return false;
  }

  for (size_t i = 0; i < iso8601.length(); i++) {
    if (!EqualCharIgnoreCaseAscii(calendar[i], iso8601[i])) {
      return false;
    }
  }
  return true;
}

static constexpr int32_t AbsentYear = INT32_MAX;

/**
 * ParseISODateTime ( isoString )
 */
static bool ParseISODateTime(JSContext* cx, const ZonedDateTimeString& parsed,
                             PlainDateTime* result) {
  // Steps 1-6, 8, 10-13 (Not applicable here).

  PlainDateTime dateTime = {parsed.date, parsed.time};

  // NOTE: ToIntegerOrInfinity("") is 0.
  if (dateTime.date.year == AbsentYear) {
    dateTime.date.year = 0;
  }

  // Step 7.
  if (dateTime.date.month == 0) {
    dateTime.date.month = 1;
  }

  // Step 9.
  if (dateTime.date.day == 0) {
    dateTime.date.day = 1;
  }

  // Step 14.
  if (dateTime.time.second == 60) {
    dateTime.time.second = 59;
  }

  // ParseISODateTime, steps 15-16 (Not applicable in our implementation).

  // Call ThrowIfInvalidISODate to report an error if |days| exceeds the number
  // of days in the month. All other values are already in-bounds.
  MOZ_ASSERT(std::abs(dateTime.date.year) <= 999'999);
  MOZ_ASSERT(1 <= dateTime.date.month && dateTime.date.month <= 12);
  MOZ_ASSERT(1 <= dateTime.date.day && dateTime.date.day <= 31);

  // ParseISODateTime, step 17.
  if (!ThrowIfInvalidISODate(cx, dateTime.date)) {
    return false;
  }

  // ParseISODateTime, step 18.
  MOZ_ASSERT(IsValidTime(dateTime.time));

  // Steps 19-25. (Handled in caller.)

  *result = dateTime;
  return true;
}

class ParserError final {
  JSErrNum error_ = JSMSG_NOT_AN_ERROR;

 public:
  constexpr MOZ_IMPLICIT ParserError(JSErrNum error) : error_(error) {}

  constexpr JSErrNum error() const { return error_; }

  constexpr operator JSErrNum() const { return error(); }
};

namespace mozilla::detail {
// Zero is used for tagging, so it mustn't be an error.
static_assert(static_cast<JSErrNum>(0) == JSMSG_NOT_AN_ERROR);

// Ensure efficient packing of the error type.
template <>
struct UnusedZero<::ParserError> {
 private:
  using Error = ::ParserError;
  using ErrorKind = JSErrNum;

 public:
  using StorageType = std::underlying_type_t<ErrorKind>;

  static constexpr bool value = true;
  static constexpr StorageType nullValue = 0;

  static constexpr Error Inspect(const StorageType& aValue) {
    return Error(static_cast<ErrorKind>(aValue));
  }
  static constexpr Error Unwrap(StorageType aValue) {
    return Error(static_cast<ErrorKind>(aValue));
  }
  static constexpr StorageType Store(Error aValue) {
    return static_cast<StorageType>(aValue.error());
  }
};
}  // namespace mozilla::detail

static_assert(mozilla::Result<ZonedDateTimeString, ParserError>::Strategy !=
              mozilla::detail::PackingStrategy::Variant);

template <typename CharT>
class StringReader final {
  mozilla::Span<const CharT> string_;

  // Current position in the string.
  size_t index_ = 0;

 public:
  explicit StringReader(mozilla::Span<const CharT> string) : string_(string) {}

  /**
   * Returns the input string.
   */
  mozilla::Span<const CharT> string() const { return string_; }

  /**
   * Returns a substring of the input string.
   */
  mozilla::Span<const CharT> substring(const StringName& name) const {
    MOZ_ASSERT(name.present());
    return string_.Subspan(name.start, name.length);
  }

  /**
   * Returns the current parse position.
   */
  size_t index() const { return index_; }

  /**
   * Returns the length of the input string-
   */
  size_t length() const { return string_.size(); }

  /**
   * Returns true iff the whole string has been parsed.
   */
  bool atEnd() const { return index() == length(); }

  /**
   * Reset the parser to a previous parse position.
   */
  void reset(size_t index = 0) {
    MOZ_ASSERT(index <= length());
    index_ = index;
  }

  /**
   * Returns true if at least `amount` characters can be read from the current
   * parse position.
   */
  bool hasMore(size_t amount) const { return index() + amount <= length(); }

  /**
   * Advances the parse position by `amount` characters.
   */
  void advance(size_t amount) {
    MOZ_ASSERT(hasMore(amount));
    index_ += amount;
  }

  /**
   * Returns the character at the current parse position.
   */
  CharT current() const { return string()[index()]; }

  /**
   * Returns the character at the next parse position.
   */
  CharT next() const { return string()[index() + 1]; }

  /**
   * Returns the character at position `index`.
   */
  CharT at(size_t index) const { return string()[index]; }
};

template <typename CharT>
class TemporalParser final {
  StringReader<CharT> reader_;

  /**
   * Read exactly `length` digits, returning `Nothing` on failure.
   */
  mozilla::Maybe<int32_t> digits(size_t length) {
    MOZ_ASSERT(length > 0, "can't read zero digits");
    MOZ_ASSERT(length <= std::numeric_limits<int32_t>::digits10,
               "can't read more than digits10 digits without overflow");

    if (!reader_.hasMore(length)) {
      return mozilla::Nothing();
    }
    int32_t num = 0;
    size_t index = reader_.index();
    for (size_t i = 0; i < length; i++) {
      auto ch = reader_.at(index + i);
      if (!mozilla::IsAsciiDigit(ch)) {
        return mozilla::Nothing();
      }
      num = num * 10 + AsciiDigitToNumber(ch);
    }
    reader_.advance(length);
    return mozilla::Some(num);
  }

  // TimeFractionalPart :
  //   Digit{1, 9}
  //
  // Fraction :
  //   DecimalSeparator TimeFractionalPart
  mozilla::Maybe<int32_t> fraction() {
    if (!reader_.hasMore(2)) {
      return mozilla::Nothing();
    }
    if (!hasDecimalSeparator() || !mozilla::IsAsciiDigit(reader_.next())) {
      return mozilla::Nothing();
    }

    // Consume the decimal separator.
    MOZ_ALWAYS_TRUE(decimalSeparator());

    // Maximal nine fractional digits are supported.
    constexpr size_t maxFractions = 9;

    // Read up to |maxFractions| digits.
    int32_t num = 0;
    size_t index = reader_.index();
    size_t i = 0;
    for (; i < std::min(reader_.length() - index, maxFractions); i++) {
      CharT ch = reader_.at(index + i);
      if (!mozilla::IsAsciiDigit(ch)) {
        break;
      }
      num = num * 10 + AsciiDigitToNumber(ch);
    }

    // Skip past the read digits.
    reader_.advance(i);

    // Normalize the fraction to |maxFractions| digits.
    for (; i < maxFractions; i++) {
      num *= 10;
    }
    return mozilla::Some(num);
  }

  /**
   * Returns true iff the current character is `ch`.
   */
  bool hasCharacter(CharT ch) const {
    return reader_.hasMore(1) && reader_.current() == ch;
  }

  /**
   * Consumes the current character if it's equal to `ch` and then returns
   * `true`. Otherwise returns `false`.
   */
  bool character(CharT ch) {
    if (!hasCharacter(ch)) {
      return false;
    }
    reader_.advance(1);
    return true;
  }

  /**
   * Consumes the next characters if they're equal to `str` and then returns
   * `true`. Otherwise returns `false`.
   */
  template <size_t N>
  bool string(const char (&str)[N]) {
    static_assert(N > 2, "use character() for one element strings");

    if (!reader_.hasMore(N - 1)) {
      return false;
    }
    size_t index = reader_.index();
    for (size_t i = 0; i < N - 1; i++) {
      if (reader_.at(index + i) != str[i]) {
        return false;
      }
    }
    reader_.advance(N - 1);
    return true;
  }

  /**
   * Returns true if the next two characters are ASCII alphabetic characters.
   */
  bool hasTwoAsciiAlpha() {
    if (!reader_.hasMore(2)) {
      return false;
    }
    size_t index = reader_.index();
    return mozilla::IsAsciiAlpha(reader_.at(index)) &&
           mozilla::IsAsciiAlpha(reader_.at(index + 1));
  }

  /**
   * Returns true iff the current character is one of `chars`.
   */
  bool hasOneOf(std::initializer_list<char16_t> chars) const {
    if (!reader_.hasMore(1)) {
      return false;
    }
    auto ch = reader_.current();
    return std::find(chars.begin(), chars.end(), ch) != chars.end();
  }

  /**
   * Consumes the current character if it's in `chars` and then returns `true`.
   * Otherwise returns `false`.
   */
  bool oneOf(std::initializer_list<char16_t> chars) {
    if (!hasOneOf(chars)) {
      return false;
    }
    reader_.advance(1);
    return true;
  }

  // Sign :
  //   ASCIISign
  //   U+2212
  //
  // ASCIISign : one of
  //   + -
  bool hasSign() const { return hasOneOf({'+', '-', 0x2212}); }

  /**
   * Consumes the current character, which must be a sign character, and returns
   * its numeric value.
   */
  int32_t sign() {
    MOZ_ASSERT(hasSign());
    int32_t plus = hasCharacter('+');
    reader_.advance(1);
    return plus ? 1 : -1;
  }

  // DecimalSeparator : one of
  //   . ,
  bool hasDecimalSeparator() const { return hasOneOf({'.', ','}); }

  bool decimalSeparator() { return oneOf({'.', ','}); }

  // DateTimeSeparator :
  //   <SP>
  //   T
  //   t
  bool dateTimeSeparator() { return oneOf({' ', 'T', 't'}); }

  // TimeDesignator : one of
  //   T t
  bool hasTimeDesignator() const { return hasOneOf({'T', 't'}); }

  bool timeDesignator() { return oneOf({'T', 't'}); }

  // UTCDesignator : one of
  //   Z z
  bool utcDesignator() { return oneOf({'Z', 'z'}); }

  // TZLeadingChar :
  //   Alpha
  //   .
  //   _
  bool hasTzLeadingChar() const {
    if (!reader_.hasMore(1)) {
      return false;
    }

    CharT ch = reader_.current();
    return mozilla::IsAsciiAlpha(ch) || ch == '.' || ch == '_';
  }

  bool tzLeadingChar() {
    if (!hasTzLeadingChar()) {
      return false;
    }
    reader_.advance(1);
    return true;
  }

  // TZChar :
  //   Alpha
  //   .
  //   -
  //   _
  //   [+Legacy] +
  //   [+Legacy] DecimalDigit
  bool tzCharLegacy() {
    if (!reader_.hasMore(1)) {
      return false;
    }

    CharT ch = reader_.current();
    if (!(mozilla::IsAsciiAlphanumeric(ch) || ch == '.' || ch == '-' ||
          ch == '_' || ch == '+')) {
      return false;
    }

    reader_.advance(1);
    return true;
  }

  // AnnotationCriticalFlag :
  //   !
  bool annotationCriticalFlag() { return character('!'); }

  // AKeyLeadingChar :
  //   LowercaseAlpha
  //   _
  bool aKeyLeadingChar() {
    if (!reader_.hasMore(1)) {
      return false;
    }

    CharT ch = reader_.current();
    if (!(mozilla::IsAsciiLowercaseAlpha(ch) || ch == '_')) {
      return false;
    }

    reader_.advance(1);
    return true;
  }

  // AKeyChar :
  //   AKeyLeadingChar
  //   DecimalDigit
  //   -
  bool aKeyChar() {
    if (!reader_.hasMore(1)) {
      return false;
    }

    CharT ch = reader_.current();
    if (!(mozilla::IsAsciiLowercaseAlpha(ch) || mozilla::IsAsciiDigit(ch) ||
          ch == '-' || ch == '_')) {
      return false;
    }

    reader_.advance(1);
    return true;
  }

  // AValChar :
  //   Alpha
  //   DecimalDigit
  bool aValChar() {
    if (!reader_.hasMore(1)) {
      return false;
    }

    CharT ch = reader_.current();
    if (!mozilla::IsAsciiAlphanumeric(ch)) {
      return false;
    }

    reader_.advance(1);
    return true;
  }

  template <typename T>
  static constexpr bool inBounds(const T& x, const T& min, const T& max) {
    return min <= x && x <= max;
  }

  mozilla::Result<ZonedDateTimeString, ParserError> dateTime();

  mozilla::Result<PlainDate, ParserError> date();

  mozilla::Result<PlainDate, ParserError> dateSpecYearMonth();

  mozilla::Result<PlainDate, ParserError> dateSpecMonthDay();

  mozilla::Result<PlainDate, ParserError> validMonthDay();

  mozilla::Result<PlainTime, ParserError> timeSpec();

  // Return true when |Annotation| can start at the current position.
  bool hasAnnotationStart() const { return hasCharacter('['); }

  // Return true when |TimeZoneAnnotation| can start at the current position.
  bool hasTimeZoneAnnotationStart() const {
    if (!hasCharacter('[')) {
      return false;
    }

    // Ensure no '=' is found before the closing ']', otherwise the opening '['
    // may actually start an |Annotation| instead of a |TimeZoneAnnotation|.
    for (size_t i = reader_.index() + 1; i < reader_.length(); i++) {
      CharT ch = reader_.at(i);
      if (ch == '=') {
        return false;
      }
      if (ch == ']') {
        break;
      }
    }
    return true;
  }

  // Return true when |TimeZoneUTCOffset| can start at the current position.
  bool hasTimeZoneUTCOffsetStart() {
    return hasOneOf({'Z', 'z', '+', '-', 0x2212});
  }

  mozilla::Result<TimeZoneString, ParserError> timeZoneUTCOffset();

  mozilla::Result<TimeZoneOffset, ParserError> utcOffset();

  mozilla::Result<TimeZoneOffset, ParserError> timeZoneUTCOffsetName() {
    // NOTE: Equivalent to the UTCOffset parser production.
    return utcOffset();
  }

  mozilla::Result<TimeZoneAnnotation, ParserError> timeZoneAnnotation();

  bool timeZoneIANANameComponent();
  mozilla::Result<TimeZoneName, ParserError> timeZoneIANAName();

  bool annotationValueComponent();
  mozilla::Result<AnnotationKey, ParserError> annotationKey();
  mozilla::Result<AnnotationValue, ParserError> annotationValue();
  mozilla::Result<Annotation, ParserError> annotation();
  mozilla::Result<CalendarName, ParserError> annotations();

  mozilla::Result<ZonedDateTimeString, ParserError> annotatedTime();

  mozilla::Result<ZonedDateTimeString, ParserError> annotatedDateTime();

  mozilla::Result<ZonedDateTimeString, ParserError>
  annotatedDateTimeTimeRequired();

  mozilla::Result<ZonedDateTimeString, ParserError> annotatedYearMonth();

  mozilla::Result<ZonedDateTimeString, ParserError> annotatedMonthDay();

 public:
  explicit TemporalParser(mozilla::Span<const CharT> str) : reader_(str) {}

  mozilla::Result<ZonedDateTimeString, ParserError>
  parseTemporalInstantString();

  mozilla::Result<ZonedDateTimeString, ParserError>
  parseTemporalTimeZoneString();

  mozilla::Result<TimeZoneOffset, ParserError> parseTimeZoneOffsetString();

  mozilla::Result<ZonedDateTimeString, ParserError>
  parseTemporalCalendarString();

  mozilla::Result<ZonedDateTimeString, ParserError> parseTemporalTimeString();

  mozilla::Result<ZonedDateTimeString, ParserError>
  parseTemporalMonthDayString();

  mozilla::Result<ZonedDateTimeString, ParserError>
  parseTemporalYearMonthString();

  mozilla::Result<ZonedDateTimeString, ParserError>
  parseTemporalDateTimeString();

  mozilla::Result<ZonedDateTimeString, ParserError>
  parseTemporalZonedDateTimeString();
};

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::dateTime() {
  // DateTime :
  //   Date
  //   Date DateTimeSeparator TimeSpec TimeZoneUTCOffset?
  ZonedDateTimeString result = {};

  auto dt = date();
  if (dt.isErr()) {
    return dt.propagateErr();
  }
  result.date = dt.unwrap();

  if (dateTimeSeparator()) {
    auto time = timeSpec();
    if (time.isErr()) {
      return time.propagateErr();
    }
    result.time = time.unwrap();

    if (hasTimeZoneUTCOffsetStart()) {
      auto tz = timeZoneUTCOffset();
      if (tz.isErr()) {
        return tz.propagateErr();
      }
      result.timeZone = tz.unwrap();
    }
  }

  return result;
}

template <typename CharT>
mozilla::Result<PlainDate, ParserError> TemporalParser<CharT>::date() {
  // Date :
  //   DateYear - DateMonth - DateDay
  //   DateYear DateMonth DateDay
  PlainDate result = {};

  // DateYear :
  //  DateFourDigitYear
  //  DateExtendedYear

  // DateFourDigitYear :
  //   DecimalDigit{1,4}
  // DateExtendedYear :
  //   Sign DecimalDigit{1,6}
  if (auto year = digits(4)) {
    result.year = year.value();
  } else if (hasSign()) {
    int32_t yearSign = sign();
    if (auto year = digits(6)) {
      result.year = yearSign * year.value();
      if (yearSign < 0 && result.year == 0) {
        return mozilla::Err(JSMSG_TEMPORAL_PARSER_NEGATIVE_ZERO_YEAR);
      }
    } else {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_EXTENDED_YEAR);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_YEAR);
  }

  // Optional: -
  character('-');

  // DateMonth :
  //   0 NonzeroDigit
  //   10
  //   11
  //   12
  if (auto month = digits(2)) {
    result.month = month.value();
    if (!inBounds(result.month, 1, 12)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_MONTH);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_MONTH);
  }

  // Optional: -
  character('-');

  // DateDay :
  //   0 NonzeroDigit
  //   1 DecimalDigit
  //   2 DecimalDigit
  //   30
  //   31
  if (auto day = digits(2)) {
    result.day = day.value();
    if (!inBounds(result.day, 1, 31)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_DAY);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_DAY);
  }

  return result;
}

template <typename CharT>
mozilla::Result<PlainTime, ParserError> TemporalParser<CharT>::timeSpec() {
  // TimeSpec :
  //   TimeHour
  //   TimeHour : TimeMinute
  //   TimeHour TimeMinute
  //   TimeHour : TimeMinute : TimeSecond TimeFraction?
  //   TimeHour TimeMinute TimeSecond TimeFraction?
  PlainTime result = {};

  // TimeHour :
  //   Hour
  //
  // Hour :
  //   0 DecimalDigit
  //   1 DecimalDigit
  //   20
  //   21
  //   22
  //   23
  if (auto hour = digits(2)) {
    result.hour = hour.value();
    if (!inBounds(result.hour, 0, 23)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_HOUR);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_HOUR);
  }

  // Optional: :
  bool needsMinutes = character(':');

  // TimeMinute :
  //   MinuteSecond
  //
  // MinuteSecond :
  //   0 DecimalDigit
  //   1 DecimalDigit
  //   2 DecimalDigit
  //   3 DecimalDigit
  //   4 DecimalDigit
  //   5 DecimalDigit
  if (auto minute = digits(2)) {
    result.minute = minute.value();
    if (!inBounds(result.minute, 0, 59)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_MINUTE);
    }

    // Optional: :
    bool needsSeconds = needsMinutes && character(':');

    // TimeSecond :
    //   MinuteSecond
    //   60
    if (auto second = digits(2)) {
      result.second = second.value();
      if (!inBounds(result.second, 0, 60)) {
        return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_LEAPSECOND);
      }

      // TimeFraction :
      //   Fraction
      if (auto f = fraction()) {
        int32_t fractionalPart = f.value();
        result.millisecond = fractionalPart / 1'000'000;
        result.microsecond = (fractionalPart % 1'000'000) / 1'000;
        result.nanosecond = fractionalPart % 1'000;
      }
    } else if (needsSeconds) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_SECOND);
    }
  } else if (needsMinutes) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_MINUTE);
  }

  return result;
}

template <typename CharT>
mozilla::Result<TimeZoneString, ParserError>
TemporalParser<CharT>::timeZoneUTCOffset() {
  // TimeZoneUTCOffset :
  //   UTCOffset
  //   UTCDesignator

  if (utcDesignator()) {
    return TimeZoneString::UTC();
  }

  if (hasSign()) {
    auto offset = utcOffset();
    if (offset.isErr()) {
      return offset.propagateErr();
    }
    return TimeZoneString::from(offset.unwrap());
  }

  return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_TIMEZONE);
}

template <typename CharT>
mozilla::Result<TimeZoneOffset, ParserError>
TemporalParser<CharT>::utcOffset() {
  // clang-format off
  //
  // UTCOffset :::
  //   TemporalSign Hour
  //   TemporalSign Hour HourSubcomponents[+Extended]
  //   TemporalSign Hour HourSubcomponents[~Extended]
  //
  // TemporalSign :::
  //   ASCIISign
  //   <MINUS>
  //
  // ASCIISign ::: one of
  //   + -
  //
  // Hour :::
  //   0 DecimalDigit
  //   1 DecimalDigit
  //   20
  //   21
  //   22
  //   23
  //
  // HourSubcomponents[Extended] :::
  //   TimeSeparator[?Extended] MinuteSecond
  //   TimeSeparator[?Extended] MinuteSecond TimeSeparator[?Extended] MinuteSecond TemporalDecimalFraction?
  //
  // TimeSeparator[Extended] :::
  //   [+Extended] :
  //   [~Extended] [empty]
  //
  // MinuteSecond :::
  //   0 DecimalDigit
  //   1 DecimalDigit
  //   2 DecimalDigit
  //   3 DecimalDigit
  //   4 DecimalDigit
  //   5 DecimalDigit
  //
  // TemporalDecimalFraction :::
  //   TemporalDecimalSeparator DecimalDigit{1,9}
  //
  // TemporalDecimalSeparator ::: one of
  //   . ,
  //
  // clang-format on

  TimeZoneOffset result = {};

  if (!hasSign()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_TIMEZONE_SIGN);
  }
  result.sign = sign();

  if (auto hour = digits(2)) {
    result.hour = hour.value();
    if (!inBounds(result.hour, 0, 23)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_HOUR);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_HOUR);
  }

  // Optional: :
  bool needsMinutes = character(':');

  if (auto minute = digits(2)) {
    result.minute = minute.value();
    if (!inBounds(result.minute, 0, 59)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_MINUTE);
    }

    // Optional: :
    bool needsSeconds = needsMinutes && character(':');

    if (auto second = digits(2)) {
      result.second = second.value();
      if (!inBounds(result.second, 0, 59)) {
        return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_SECOND);
      }

      // TemporalDecimalFraction :::
      //   TemporalDecimalSeparator DecimalDigit{1,9}
      if (auto fractionalPart = fraction()) {
        result.fractionalPart = fractionalPart.value();
      }
    } else if (needsSeconds) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_SECOND);
    }
  } else if (needsMinutes) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_MINUTE);
  }

  return result;
}

template <typename CharT>
mozilla::Result<TimeZoneAnnotation, ParserError>
TemporalParser<CharT>::timeZoneAnnotation() {
  // TimeZoneAnnotation :
  //   [ AnnotationCriticalFlag? TimeZoneIdentifier ]
  //
  // TimeZoneIdentifier :
  //   TimeZoneIANAName
  //   TimeZoneUTCOffsetName

  if (!character('[')) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_BRACKET_BEFORE_TIMEZONE);
  }

  // Skip over the optional critical flag.
  annotationCriticalFlag();

  TimeZoneAnnotation result = {};
  if (hasSign()) {
    auto offset = timeZoneUTCOffsetName();
    if (offset.isErr()) {
      return offset.propagateErr();
    }
    result.offset = offset.unwrap();
  } else {
    auto name = timeZoneIANAName();
    if (name.isErr()) {
      return name.propagateErr();
    }
    result.name = name.unwrap();
  }

  if (!character(']')) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_BRACKET_AFTER_TIMEZONE);
  }

  return result;
}

template <typename CharT>
mozilla::Result<TimeZoneName, ParserError>
TemporalParser<CharT>::timeZoneIANAName() {
  // TimeZoneIANAName :
  //   TimeZoneIANANameTail[~Legacy]
  //   TimeZoneIANALegacyName
  //
  // UnpaddedHour :
  //   DecimalDigit
  //   1 DecimalDigit
  //   20
  //   21
  //   22
  //   23
  //
  // TimeZoneIANANameTail[Legacy] :
  //   TimeZoneIANANameComponent[?Legacy]
  //   TimeZoneIANANameComponent[?Legacy] / TimeZoneIANANameTail[?Legacy]
  //
  // TimeZoneIANALegacyName :
  //   Etc/GMT ASCIISign UnpaddedHour
  //   Etc/GMT0
  //   GMT0
  //   GMT-0
  //   GMT+0
  //   EST5EDT
  //   CST6CDT
  //   MST7MDT
  //   PST8PDT

  size_t start = reader_.index();

  // NOTE: Time zone names are parsed with legacy mode always enabled. If the
  // name contains legacy name components, but doesn't match the
  // |TimeZoneIANALegacyName| production, it'll be rejected during the time zone
  // name validation step which happens after parsing.

  do {
    if (!timeZoneIANANameComponent()) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_TIMEZONE_NAME);
    }
  } while (character('/'));

  return TimeZoneName{start, reader_.index() - start};
}

template <typename CharT>
bool TemporalParser<CharT>::timeZoneIANANameComponent() {
  // TimeZoneIANANameComponent[Legacy] :
  //   TZLeadingChar TZChar[?Legacy]{0, 13} but not one of . or ..

  size_t index = reader_.index();

  // Parse TZLeadingChar followed by up to thirteen TZChar[Legacy] characters.
  if (!tzLeadingChar()) {
    return false;
  }
  for (size_t i = 0; i < 13 && tzCharLegacy(); i++) {
  }

  // Reject if the name component matches either "." or "..".
  size_t charactersRead = reader_.index() - index;
  if (charactersRead == 1 && reader_.at(index) == '.') {
    return false;
  }
  if (charactersRead == 2 && reader_.at(index) == '.' &&
      reader_.at(index + 1) == '.') {
    return false;
  }
  return true;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::parseTemporalInstantString() {
  // Initialize all fields to zero.
  ZonedDateTimeString result = {};

  // clang-format off
  //
  // TemporalInstantString :
  //   Date DateTimeSeparator TimeSpec TimeZoneUTCOffset TimeZoneAnnotation? Annotations?
  //
  // clang-format on

  auto dt = date();
  if (dt.isErr()) {
    return dt.propagateErr();
  }
  result.date = dt.unwrap();

  if (!dateTimeSeparator()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_DATE_TIME_SEPARATOR);
  }

  auto time = timeSpec();
  if (time.isErr()) {
    return time.propagateErr();
  }
  result.time = time.unwrap();

  auto tz = timeZoneUTCOffset();
  if (tz.isErr()) {
    return tz.propagateErr();
  }
  result.timeZone = tz.unwrap();

  if (hasTimeZoneAnnotationStart()) {
    auto annotation = timeZoneAnnotation();
    if (annotation.isErr()) {
      return annotation.propagateErr();
    }
    result.timeZone.annotation = annotation.unwrap();
  }

  if (hasAnnotationStart()) {
    if (auto cal = annotations(); cal.isErr()) {
      return cal.propagateErr();
    }
  }

  if (!reader_.atEnd()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_GARBAGE_AFTER_INPUT);
  }

  return result;
}

/**
 * ParseTemporalInstantString ( isoString )
 */
template <typename CharT>
static auto ParseTemporalInstantString(mozilla::Span<const CharT> str) {
  TemporalParser<CharT> parser(str);
  return parser.parseTemporalInstantString();
}

/**
 * ParseTemporalInstantString ( isoString )
 */
static auto ParseTemporalInstantString(Handle<JSLinearString*> str) {
  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    return ParseTemporalInstantString<Latin1Char>(str->latin1Range(nogc));
  }
  return ParseTemporalInstantString<char16_t>(str->twoByteRange(nogc));
}

/**
 * ParseTemporalInstantString ( isoString )
 */
bool js::temporal::ParseTemporalInstantString(JSContext* cx,
                                              Handle<JSString*> str,
                                              PlainDateTime* result,
                                              int64_t* offset) {
  Rooted<JSLinearString*> linear(cx, str->ensureLinear(cx));
  if (!linear) {
    return false;
  }

  // Step 1.
  auto parseResult = ::ParseTemporalInstantString(linear);
  if (parseResult.isErr()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              parseResult.unwrapErr());
    return false;
  }
  ZonedDateTimeString parsed = parseResult.unwrap();

  // Step 2.
  if (!ParseISODateTime(cx, parsed, result)) {
    return false;
  }

  // Steps 3-5.
  if (parsed.timeZone.hasOffset()) {
    *offset = ParseTimeZoneOffsetString(parsed.timeZone.offset);
  } else {
    MOZ_ASSERT(parsed.timeZone.isUTC());
    *offset = 0;
  }

  // Step 6.
  return true;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::parseTemporalTimeZoneString() {
  // TimeZoneIdentifier :
  //   TimeZoneIANAName
  //   TimeZoneUTCOffsetName

  if (hasTzLeadingChar()) {
    if (auto name = timeZoneIANAName(); name.isOk() && reader_.atEnd()) {
      ZonedDateTimeString result = {};
      result.timeZone = TimeZoneString::from(name.unwrap());
      return result;
    }
  } else {
    if (auto offset = timeZoneUTCOffsetName();
        offset.isOk() && reader_.atEnd()) {
      ZonedDateTimeString result = {};
      result.timeZone = TimeZoneString::from(offset.unwrap());
      return result;
    }
  }

  // Try all six parse goals from ParseISODateTime in order.
  //
  // TemporalDateTimeString
  // TemporalInstantString
  // TemporalTimeString
  // TemporalZonedDateTimeString
  // TemporalMonthDayString
  // TemporalYearMonthString

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalDateTimeString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalInstantString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalTimeString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalZonedDateTimeString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalMonthDayString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalYearMonthString(); dt.isOk()) {
    return dt.unwrap();
  } else {
    return dt.propagateErr();
  }
}

/**
 * ParseTemporalTimeZoneString ( isoString )
 */
template <typename CharT>
static auto ParseTemporalTimeZoneString(mozilla::Span<const CharT> str) {
  TemporalParser<CharT> parser(str);
  return parser.parseTemporalTimeZoneString();
}

/**
 * ParseTemporalTimeZoneString ( isoString )
 */
static auto ParseTemporalTimeZoneString(Handle<JSLinearString*> str) {
  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    return ParseTemporalTimeZoneString<Latin1Char>(str->latin1Range(nogc));
  }
  return ParseTemporalTimeZoneString<char16_t>(str->twoByteRange(nogc));
}

/**
 * ParseTemporalTimeZoneString ( isoString )
 */
bool js::temporal::ParseTemporalTimeZoneString(
    JSContext* cx, Handle<JSString*> str, MutableHandle<JSString*> timeZoneName,
    int64_t* offsetNanoseconds) {
  Rooted<JSLinearString*> linear(cx, str->ensureLinear(cx));
  if (!linear) {
    return false;
  }

  // Steps 1-4.
  auto parseResult = ::ParseTemporalTimeZoneString(linear);
  if (parseResult.isErr()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              parseResult.unwrapErr());
    return false;
  }
  ZonedDateTimeString parsed = parseResult.unwrap();
  const auto& timeZone = parsed.timeZone;

  // Step 3.
  PlainDateTime unused;
  if (!ParseISODateTime(cx, parsed, &unused)) {
    return false;
  }

  if (timeZone.annotation.hasOffset()) {
    // Case 1: 19700101Z[+02:00]
    // Case 2: 19700101+00:00[+02:00]
    // Case 3: 19700101[+02:00]

    *offsetNanoseconds = ParseTimeZoneOffsetString(timeZone.annotation.offset);
  } else if (timeZone.annotation.hasName()) {
    MOZ_ASSERT(!timeZone.hasName());

    // Case 1: 19700101Z[Europe/Berlin]
    // Case 2: 19700101+00:00[Europe/Berlin]
    // Case 3: 19700101[Europe/Berlin]

    timeZoneName.set(ToString(cx, linear, timeZone.annotation.name));
    if (!timeZoneName) {
      return false;
    }
  } else if (timeZone.isUTC()) {
    timeZoneName.set(cx->names().UTC);
  } else if (timeZone.hasOffset()) {
    *offsetNanoseconds = ParseTimeZoneOffsetString(timeZone.offset);
  } else if (timeZone.hasName()) {
    timeZoneName.set(ToString(cx, linear, timeZone.name));
    if (!timeZoneName) {
      return false;
    }
  } else {
    // Step 5.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PARSER_MISSING_TIMEZONE);
    return false;
  }

  // Step 6.
  return true;
}

template <typename CharT>
mozilla::Result<TimeZoneOffset, ParserError>
TemporalParser<CharT>::parseTimeZoneOffsetString() {
  auto offset = utcOffset();
  if (offset.isErr()) {
    return offset.propagateErr();
  }
  if (!reader_.atEnd()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_GARBAGE_AFTER_INPUT);
  }
  return offset.unwrap();
}

/**
 * ParseTimeZoneOffsetString ( isoString )
 */
template <typename CharT>
static auto ParseTimeZoneOffsetString(mozilla::Span<const CharT> str) {
  TemporalParser<CharT> parser(str);
  return parser.parseTimeZoneOffsetString();
}

/**
 * ParseTimeZoneOffsetString ( isoString )
 */
static auto ParseTimeZoneOffsetString(Handle<JSLinearString*> str) {
  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    return ParseTimeZoneOffsetString<Latin1Char>(str->latin1Range(nogc));
  }
  return ParseTimeZoneOffsetString<char16_t>(str->twoByteRange(nogc));
}

/**
 * ParseTimeZoneOffsetString ( isoString )
 */
bool js::temporal::ParseTimeZoneOffsetString(JSContext* cx,
                                             Handle<JSString*> str,
                                             int64_t* result) {
  // Step 1. (Not applicable in our implementation.)

  Rooted<JSLinearString*> linear(cx, str->ensureLinear(cx));
  if (!linear) {
    return false;
  }

  // Step 2.
  auto parseResult = ::ParseTimeZoneOffsetString(linear);
  if (parseResult.isErr()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              parseResult.unwrapErr());
    return false;
  }

  // Steps 3-13.
  *result = ParseTimeZoneOffsetString(parseResult.unwrap());
  return true;
}

template <typename CharT>
bool TemporalParser<CharT>::annotationValueComponent() {
  // AnnotationValueComponent :
  //   AValChar AnnotationValueComponent?
  //
  // AValChar :
  //   Alpha
  //   DecimalDigit
  bool hasOne = false;
  while (aValChar()) {
    hasOne = true;
  }
  return hasOne;
}

template <typename CharT>
mozilla::Result<AnnotationKey, ParserError>
TemporalParser<CharT>::annotationKey() {
  // AnnotationKey :
  //   AKeyLeadingChar AnnotationKeyTail?
  //
  // AnnotationKeyTail :
  //   AKeyChar AnnotationKeyTail?

  size_t start = reader_.index();

  if (!aKeyLeadingChar()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_ANNOTATION_KEY);
  }

  // Optionally followed by a sequence of |AKeyChar|.
  while (aKeyChar()) {
  }

  return AnnotationKey{start, reader_.index() - start};
}

template <typename CharT>
mozilla::Result<AnnotationValue, ParserError>
TemporalParser<CharT>::annotationValue() {
  // AnnotationValue :
  //   AnnotationValueTail
  //
  // AnnotationValueTail :
  //   AnnotationValueComponent
  //   AnnotationValueComponent - AnnotationValueTail

  size_t start = reader_.index();

  do {
    if (!annotationValueComponent()) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_ANNOTATION_VALUE);
    }
  } while (character('-'));

  return AnnotationValue{start, reader_.index() - start};
}

template <typename CharT>
mozilla::Result<Annotation, ParserError> TemporalParser<CharT>::annotation() {
  // Annotation :
  //   [ AnnotationCriticalFlag? AnnotationKey = AnnotationValue ]

  if (!character('[')) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_BRACKET_BEFORE_ANNOTATION);
  }

  bool critical = annotationCriticalFlag();

  auto key = annotationKey();
  if (key.isErr()) {
    return key.propagateErr();
  }

  if (!character('=')) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_ASSIGNMENT_IN_ANNOTATION);
  }

  auto value = annotationValue();
  if (value.isErr()) {
    return value.propagateErr();
  }

  if (!character(']')) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_BRACKET_AFTER_ANNOTATION);
  }

  return Annotation{key.unwrap(), value.unwrap(), critical};
}

template <typename CharT>
mozilla::Result<CalendarName, ParserError>
TemporalParser<CharT>::annotations() {
  // Annotations:
  //   Annotation Annotations?

  MOZ_ASSERT(hasAnnotationStart());

  CalendarName cal;
  while (hasAnnotationStart()) {
    auto anno = annotation();
    if (anno.isErr()) {
      return anno.propagateErr();
    }

    // FIXME: spec issue - ignore case for "[u-ca=" to match BCP47?
    // https://github.com/tc39/proposal-temporal/issues/2524

    static constexpr std::string_view ca = "u-ca";

    auto key = anno.unwrap().key;
    auto keySpan = reader_.substring(key);
    if (keySpan.size() == ca.length() &&
        std::equal(ca.begin(), ca.end(), keySpan.data())) {
      if (!cal.present()) {
        cal = anno.unwrap().value;
      }
    } else if (anno.unwrap().critical) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_CRITICAL_ANNOTATION);
    }
  }
  return cal;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::annotatedTime() {
  // clang-format off
  //
  // AnnotatedTime :
  //   TimeDesignator TimeSpec TimeZoneUTCOffset? TimeZoneAnnotation? Annotations?
  //   TimeSpecWithOptionalOffsetNotAmbiguous TimeZoneAnnotation? Annotations?
  //
  // clang-format on

  if (timeDesignator()) {
    ZonedDateTimeString result = {};

    auto time = timeSpec();
    if (time.isErr()) {
      return time.propagateErr();
    }
    result.time = time.unwrap();

    if (hasTimeZoneUTCOffsetStart()) {
      auto tz = timeZoneUTCOffset();
      if (tz.isErr()) {
        return tz.propagateErr();
      }
      result.timeZone = tz.unwrap();
    }

    if (hasTimeZoneAnnotationStart()) {
      auto annotation = timeZoneAnnotation();
      if (annotation.isErr()) {
        return annotation.propagateErr();
      }
      result.timeZone.annotation = annotation.unwrap();
    }

    if (hasAnnotationStart()) {
      auto cal = annotations();
      if (cal.isErr()) {
        return cal.propagateErr();
      }
      result.calendar = cal.unwrap();
    }

    return result;
  }

  // TimeSpecWithOptionalOffsetNotAmbiguous :
  //   TimeSpec TimeZoneUTCOffset? but not one of ValidMonthDay or
  //   DateSpecYearMonth

  size_t start = reader_.index();

  ZonedDateTimeString result = {};

  auto time = timeSpec();
  if (time.isErr()) {
    return time.propagateErr();
  }
  result.time = time.unwrap();

  if (hasTimeZoneUTCOffsetStart()) {
    auto tz = timeZoneUTCOffset();
    if (tz.isErr()) {
      return tz.propagateErr();
    }
    result.timeZone = tz.unwrap();
  }

  size_t end = reader_.index();

  // Reset and check if the input can also be parsed as ValidMonthDay.
  reader_.reset(start);

  if (validMonthDay().isOk()) {
    if (reader_.index() == end) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_AMBIGUOUS_TIME_MONTH_DAY);
    }
  }

  // Reset and check if the input can also be parsed as DateSpecYearMonth.
  reader_.reset(start);

  if (dateSpecYearMonth().isOk()) {
    if (reader_.index() == end) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_AMBIGUOUS_TIME_YEAR_MONTH);
    }
  }

  // Input can neither be parsed as ValidMonthDay nor DateSpecYearMonth.
  reader_.reset(end);

  if (hasTimeZoneAnnotationStart()) {
    auto annotation = timeZoneAnnotation();
    if (annotation.isErr()) {
      return annotation.propagateErr();
    }
    result.timeZone.annotation = annotation.unwrap();
  }

  if (hasAnnotationStart()) {
    auto cal = annotations();
    if (cal.isErr()) {
      return cal.propagateErr();
    }
    result.calendar = cal.unwrap();
  }

  return result;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::annotatedDateTime() {
  // AnnotatedDateTime :
  //   DateTime TimeZoneAnnotation? Annotations?

  auto dt = dateTime();
  if (dt.isErr()) {
    return dt.propagateErr();
  }
  auto result = dt.unwrap();

  if (hasTimeZoneAnnotationStart()) {
    auto annotation = timeZoneAnnotation();
    if (annotation.isErr()) {
      return annotation.propagateErr();
    }
    result.timeZone.annotation = annotation.unwrap();
  }

  if (hasAnnotationStart()) {
    auto cal = annotations();
    if (cal.isErr()) {
      return cal.propagateErr();
    }
    result.calendar = cal.unwrap();
  }

  return result;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::annotatedDateTimeTimeRequired() {
  // clang-format off
  //
  // AnnotatedDateTimeTimeRequired :
  //   Date DateTimeSeparator TimeSpec TimeZoneUTCOffset? TimeZoneAnnotation? Annotations?
  //
  // clang-format on

  ZonedDateTimeString result = {};

  auto dt = date();
  if (dt.isErr()) {
    return dt.propagateErr();
  }
  result.date = dt.unwrap();

  if (!dateTimeSeparator()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_DATE_TIME_SEPARATOR);
  }

  auto time = timeSpec();
  if (time.isErr()) {
    return time.propagateErr();
  }
  result.time = time.unwrap();

  if (hasTimeZoneUTCOffsetStart()) {
    auto tz = timeZoneUTCOffset();
    if (tz.isErr()) {
      return tz.propagateErr();
    }
    result.timeZone = tz.unwrap();
  }

  if (hasTimeZoneAnnotationStart()) {
    auto annotation = timeZoneAnnotation();
    if (annotation.isErr()) {
      return annotation.propagateErr();
    }
    result.timeZone.annotation = annotation.unwrap();
  }

  if (hasAnnotationStart()) {
    auto cal = annotations();
    if (cal.isErr()) {
      return cal.propagateErr();
    }
    result.calendar = cal.unwrap();
  }

  return result;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::annotatedYearMonth() {
  // AnnotatedYearMonth :
  //   DateSpecYearMonth TimeZoneAnnotation? Annotations?

  ZonedDateTimeString result = {};

  auto yearMonth = dateSpecYearMonth();
  if (yearMonth.isErr()) {
    return yearMonth.propagateErr();
  }
  result.date = yearMonth.unwrap();

  if (hasTimeZoneAnnotationStart()) {
    auto annotation = timeZoneAnnotation();
    if (annotation.isErr()) {
      return annotation.propagateErr();
    }
    result.timeZone.annotation = annotation.unwrap();
  }

  if (hasAnnotationStart()) {
    auto cal = annotations();
    if (cal.isErr()) {
      return cal.propagateErr();
    }
    result.calendar = cal.unwrap();
  }

  return result;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::annotatedMonthDay() {
  // AnnotatedMonthDay :
  //   DateSpecMonthDay TimeZoneAnnotation? Annotations?

  ZonedDateTimeString result = {};

  auto monthDay = dateSpecMonthDay();
  if (monthDay.isErr()) {
    return monthDay.propagateErr();
  }
  result.date = monthDay.unwrap();

  if (hasTimeZoneAnnotationStart()) {
    auto annotation = timeZoneAnnotation();
    if (annotation.isErr()) {
      return annotation.propagateErr();
    }
    result.timeZone.annotation = annotation.unwrap();
  }

  if (hasAnnotationStart()) {
    auto cal = annotations();
    if (cal.isErr()) {
      return cal.propagateErr();
    }
    result.calendar = cal.unwrap();
  }

  return result;
}

template <typename CharT>
mozilla::Result<PlainDate, ParserError>
TemporalParser<CharT>::dateSpecYearMonth() {
  // DateSpecYearMonth :
  //   DateYear -? DateMonth
  PlainDate result = {};

  // DateYear :
  //  DateFourDigitYear
  //  DateExtendedYear

  // DateFourDigitYear :
  //   DecimalDigit{1,4}
  // DateExtendedYear :
  //   Sign DecimalDigit{1,6}
  if (auto year = digits(4)) {
    result.year = year.value();
  } else if (hasSign()) {
    int32_t yearSign = sign();
    if (auto year = digits(6)) {
      result.year = yearSign * year.value();
      if (yearSign < 0 && result.year == 0) {
        return mozilla::Err(JSMSG_TEMPORAL_PARSER_NEGATIVE_ZERO_YEAR);
      }
    } else {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_EXTENDED_YEAR);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_YEAR);
  }

  character('-');

  // DateMonth :
  //   0 NonzeroDigit
  //   10
  //   11
  //   12
  if (auto month = digits(2)) {
    result.month = month.value();
    if (!inBounds(result.month, 1, 12)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_MONTH);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_MONTH);
  }

  // Absent days default to 1, cf. ParseISODateTime.
  result.day = 1;

  return result;
}

template <typename CharT>
mozilla::Result<PlainDate, ParserError>
TemporalParser<CharT>::dateSpecMonthDay() {
  // DateSpecMonthDay :
  //   TwoDashes? DateMonth -? DateDay
  //
  // TwoDashes :
  //   --
  PlainDate result = {};

  string("--");

  result.year = AbsentYear;

  // DateMonth :
  //   0 NonzeroDigit
  //   10
  //   11
  //   12
  if (auto month = digits(2)) {
    result.month = month.value();
    if (!inBounds(result.month, 1, 12)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_MONTH);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_MONTH);
  }

  character('-');

  // DateDay :
  //   0 NonzeroDigit
  //   1 DecimalDigit
  //   2 DecimalDigit
  //   30
  //   31
  if (auto day = digits(2)) {
    result.day = day.value();
    if (!inBounds(result.day, 1, 31)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_DAY);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_DAY);
  }

  return result;
}

template <typename CharT>
mozilla::Result<PlainDate, ParserError> TemporalParser<CharT>::validMonthDay() {
  // ValidMonthDay :
  //   DateMonth -? 0 NonZeroDigit
  //   DateMonth -? 1 DecimalDigit
  //   DateMonth -? 2 DecimalDigit
  //   DateMonth -? 30 but not one of 0230 or 02-30
  //   DateMonthWithThirtyOneDays -? 31
  //
  // DateMonthWithThirtyOneDays : one of
  //   01 03 05 07 08 10 12

  PlainDate result = {};

  // DateMonth :
  //   0 NonzeroDigit
  //   10
  //   11
  //   12
  if (auto month = digits(2)) {
    result.month = month.value();
    if (!inBounds(result.month, 1, 12)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_MONTH);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_MONTH);
  }

  character('-');

  if (auto day = digits(2)) {
    result.day = day.value();
    if (!inBounds(result.day, 1, 31)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_DAY);
    }
  } else {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_MISSING_DAY);
  }

  if (result.month == 2 && result.day > 29) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_DAY);
  }

  if (result.day > 30) {
    MOZ_ASSERT(result.day == 31);

    static constexpr int32_t monthsWithThirtyOneDays[] = {
        1, 3, 5, 7, 8, 10, 12,
    };

    if (std::find(std::begin(monthsWithThirtyOneDays),
                  std::end(monthsWithThirtyOneDays),
                  result.month) == std::end(monthsWithThirtyOneDays)) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_INVALID_DAY);
    }
  }

  return result;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::parseTemporalCalendarString() {
  // Handle the common case of a standalone calendar name first.
  //
  // All valid calendar names start with two alphabetic characters and none of
  // the ParseISODateTime parse goals can start with two alphabetic characters.
  // TemporalTimeString can start with 'T', so we can't only check the first
  // character.
  if (hasTwoAsciiAlpha()) {
    auto cal = annotationValue();
    if (cal.isErr()) {
      return cal.propagateErr();
    }
    if (!reader_.atEnd()) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_GARBAGE_AFTER_INPUT);
    }

    ZonedDateTimeString result = {};
    result.calendar = cal.unwrap();
    return result;
  }

  // Try all six parse goals from ParseISODateTime in order.
  //
  // TemporalDateTimeString
  // TemporalInstantString
  // TemporalTimeString
  // TemporalZonedDateTimeString
  // TemporalMonthDayString
  // TemporalYearMonthString

  if (auto dt = parseTemporalDateTimeString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalInstantString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalTimeString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalZonedDateTimeString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalMonthDayString(); dt.isOk()) {
    return dt.unwrap();
  }

  // Restart parsing from the start of the string.
  reader_.reset();

  if (auto dt = parseTemporalYearMonthString(); dt.isOk()) {
    return dt.unwrap();
  } else {
    return dt.propagateErr();
  }
}

/**
 * ParseTemporalCalendarString ( isoString )
 */
template <typename CharT>
static auto ParseTemporalCalendarString(mozilla::Span<const CharT> str) {
  TemporalParser<CharT> parser(str);
  return parser.parseTemporalCalendarString();
}

/**
 * ParseTemporalCalendarString ( isoString )
 */
static auto ParseTemporalCalendarString(Handle<JSLinearString*> str) {
  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    return ParseTemporalCalendarString<Latin1Char>(str->latin1Range(nogc));
  }
  return ParseTemporalCalendarString<char16_t>(str->twoByteRange(nogc));
}

/**
 * ParseTemporalCalendarString ( isoString )
 */
JSLinearString* js::temporal::ParseTemporalCalendarString(
    JSContext* cx, Handle<JSString*> str) {
  Rooted<JSLinearString*> linear(cx, str->ensureLinear(cx));
  if (!linear) {
    return nullptr;
  }

  // Steps 1-3.
  auto parseResult = ::ParseTemporalCalendarString(linear);
  if (parseResult.isErr()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              parseResult.unwrapErr());
    return nullptr;
  }
  ZonedDateTimeString parsed = parseResult.unwrap();

  PlainDateTime unused;
  if (!ParseISODateTime(cx, parsed, &unused)) {
    return nullptr;
  }

  // Step 2.b.
  if (!parsed.calendar.present()) {
    return cx->names().iso8601;
  }

  // Steps 2.c and 3.c
  return ToString(cx, linear, parsed.calendar);
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::parseTemporalTimeString() {
  // TemporalTimeString :
  //   AnnotatedTime
  //   AnnotatedDateTimeTimeRequired

  if (auto time = annotatedTime(); time.isOk() && reader_.atEnd()) {
    return time.unwrap();
  }

  // Reset and try the next option.
  reader_.reset();

  auto dt = annotatedDateTimeTimeRequired();
  if (dt.isErr()) {
    return dt.propagateErr();
  }
  if (!reader_.atEnd()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_GARBAGE_AFTER_INPUT);
  }
  return dt.unwrap();
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::parseTemporalMonthDayString() {
  // TemporalMonthDayString :
  //   AnnotatedMonthDay
  //   AnnotatedDateTime

  if (auto monthDay = annotatedMonthDay(); monthDay.isOk() && reader_.atEnd()) {
    auto result = monthDay.unwrap();

    // FIXME: spec bug - actually needs to check all calendar annotations
    // https://github.com/tc39/proposal-temporal/issues/2538

    // ParseISODateTime, step 3.
    if (result.calendar.present() &&
        !IsISO8601Calendar(reader_.substring(result.calendar))) {
      return mozilla::Err(JSMSG_TEMPORAL_PARSER_MONTH_DAY_CALENDAR_NOT_ISO8601);
    }
    return result;
  }

  // Reset and try the next option.
  reader_.reset();

  auto dt = annotatedDateTime();
  if (dt.isErr()) {
    return dt.propagateErr();
  }
  if (!reader_.atEnd()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_GARBAGE_AFTER_INPUT);
  }
  return dt.unwrap();
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::parseTemporalYearMonthString() {
  // TemporalYearMonthString :
  //   AnnotatedYearMonth
  //   AnnotatedDateTime

  if (auto yearMonth = annotatedYearMonth();
      yearMonth.isOk() && reader_.atEnd()) {
    auto result = yearMonth.unwrap();

    // FIXME: spec bug - actually needs to check all calendar annotations
    // https://github.com/tc39/proposal-temporal/issues/2538

    // ParseISODateTime, step 3.
    if (result.calendar.present() &&
        !IsISO8601Calendar(reader_.substring(result.calendar))) {
      return mozilla::Err(
          JSMSG_TEMPORAL_PARSER_YEAR_MONTH_CALENDAR_NOT_ISO8601);
    }
    return result;
  }

  // Reset and try the next option.
  reader_.reset();

  auto dt = annotatedDateTime();
  if (dt.isErr()) {
    return dt.propagateErr();
  }
  if (!reader_.atEnd()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_GARBAGE_AFTER_INPUT);
  }
  return dt.unwrap();
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::parseTemporalDateTimeString() {
  // TemporalDateTimeString :
  //   AnnotatedDateTime

  auto dateTime = annotatedDateTime();
  if (dateTime.isErr()) {
    return dateTime.propagateErr();
  }
  if (!reader_.atEnd()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_GARBAGE_AFTER_INPUT);
  }
  return dateTime.unwrap();
}

/**
 * ParseTemporalDateTimeString ( isoString )
 */
template <typename CharT>
static auto ParseTemporalDateTimeString(mozilla::Span<const CharT> str) {
  TemporalParser<CharT> parser(str);
  return parser.parseTemporalDateTimeString();
}

/**
 * ParseTemporalDateTimeString ( isoString )
 */
static auto ParseTemporalDateTimeString(Handle<JSLinearString*> str) {
  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    return ParseTemporalDateTimeString<Latin1Char>(str->latin1Range(nogc));
  }
  return ParseTemporalDateTimeString<char16_t>(str->twoByteRange(nogc));
}

/**
 * ParseTemporalDateTimeString ( isoString )
 */
bool js::temporal::ParseTemporalDateTimeString(
    JSContext* cx, Handle<JSString*> str, PlainDateTime* result,
    MutableHandle<JSString*> calendar) {
  Rooted<JSLinearString*> linear(cx, str->ensureLinear(cx));
  if (!linear) {
    return false;
  }

  // Steps 1-2.
  auto parseResult = ::ParseTemporalDateTimeString(linear);
  if (parseResult.isErr()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              parseResult.unwrapErr());
    return false;
  }
  ZonedDateTimeString parsed = parseResult.unwrap();

  // Step 3.
  if (parsed.timeZone.isUTC()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PARSER_INVALID_UTC_DESIGNATOR);
    return false;
  }

  // Step 4.
  if (!ParseISODateTime(cx, parsed, result)) {
    return false;
  }

  if (parsed.calendar.present()) {
    calendar.set(ToString(cx, linear, parsed.calendar));
    if (!calendar) {
      return false;
    }
  }

  return true;
}

/**
 * ParseTemporalDateString ( isoString )
 */
bool js::temporal::ParseTemporalDateString(JSContext* cx, Handle<JSString*> str,
                                           PlainDate* result,
                                           MutableHandle<JSString*> calendar) {
  // Step 1.
  PlainDateTime dateTime;
  if (!ParseTemporalDateTimeString(cx, str, &dateTime, calendar)) {
    return false;
  }

  // Step 2.
  *result = dateTime.date;
  return true;
}

template <typename CharT>
mozilla::Result<ZonedDateTimeString, ParserError>
TemporalParser<CharT>::parseTemporalZonedDateTimeString() {
  // TemporalZonedDateTimeString :
  //   DateTime TimeZoneAnnotation Annotations?

  auto dt = dateTime();
  if (dt.isErr()) {
    return dt.propagateErr();
  }
  auto result = dt.unwrap();

  auto annotation = timeZoneAnnotation();
  if (annotation.isErr()) {
    return annotation.propagateErr();
  }
  result.timeZone.annotation = annotation.unwrap();

  if (hasAnnotationStart()) {
    auto cal = annotations();
    if (cal.isErr()) {
      return cal.propagateErr();
    }
    result.calendar = cal.unwrap();
  }

  if (!reader_.atEnd()) {
    return mozilla::Err(JSMSG_TEMPORAL_PARSER_GARBAGE_AFTER_INPUT);
  }

  return result;
}
