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

  bool annotationValueComponent();
  mozilla::Result<AnnotationValue, ParserError> annotationValue();

 public:
  explicit TemporalParser(mozilla::Span<const CharT> str) : reader_(str) {}

  mozilla::Result<ZonedDateTimeString, ParserError>
  parseTemporalCalendarString();
};

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

  MOZ_CRASH("NYI");
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
