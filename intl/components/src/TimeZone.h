/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_TimeZone_h_
#define intl_components_TimeZone_h_

// ICU doesn't provide a separate C API for time zone functions, but instead
// requires to use UCalendar. This adds a measurable overhead when compared to
// using ICU's C++ TimeZone API, therefore we prefer to use the C++ API when
// possible. Due to the lack of a stable ABI in C++, it's only possible to use
// the C++ API when we use our in-tree ICU copy.
#if !MOZ_SYSTEM_ICU
#  define MOZ_INTL_USE_ICU_CPP_TIMEZONE 1
#else
#  define MOZ_INTL_USE_ICU_CPP_TIMEZONE 0
#endif

#include <stdint.h>
#include <utility>

#include "unicode/ucal.h"
#include "unicode/utypes.h"
#if MOZ_INTL_USE_ICU_CPP_TIMEZONE
#  include "unicode/locid.h"
#  include "unicode/timezone.h"
#  include "unicode/unistr.h"
#endif

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::intl {

/**
 * This component is a Mozilla-focused API for working with time zones in
 * internationalization code. It is used in coordination with other operations
 * such as datetime formatting.
 */
class TimeZone final {
 public:
#if MOZ_INTL_USE_ICU_CPP_TIMEZONE
  explicit TimeZone(UniquePtr<icu::TimeZone> aTimeZone)
      : mTimeZone(std::move(aTimeZone)) {
    MOZ_ASSERT(mTimeZone);
  }
#else
  explicit TimeZone(UCalendar* aCalendar) : mCalendar(aCalendar) {
    MOZ_ASSERT(mCalendar);
  }
#endif

  // Do not allow copy as this class owns the ICU resource. Move is not
  // currently implemented, but a custom move operator could be created if
  // needed.
  TimeZone(const TimeZone&) = delete;
  TimeZone& operator=(const TimeZone&) = delete;

#if MOZ_INTL_USE_ICU_CPP_TIMEZONE
  ~TimeZone() = default;
#else
  ~TimeZone();
#endif

  /**
   * Create a TimeZone.
   */
  static Result<UniquePtr<TimeZone>, ICUError> TryCreate(
      Maybe<Span<const char16_t>> aTimeZoneOverride = Nothing{});

  /**
   * A number indicating the raw offset from GMT in milliseconds.
   */
  Result<int32_t, ICUError> GetRawOffsetMs();

  /**
   * Return the daylight saving offset in milliseconds at the given UTC time.
   */
  Result<int32_t, ICUError> GetDSTOffsetMs(int64_t aUTCMilliseconds);

  /**
   * Return the local offset in milliseconds at the given UTC time.
   */
  Result<int32_t, ICUError> GetOffsetMs(int64_t aUTCMilliseconds);

  /**
   * Return the UTC offset in milliseconds at the given local time.
   */
  Result<int32_t, ICUError> GetUTCOffsetMs(int64_t aLocalMilliseconds);

  enum class DaylightSavings : bool { No, Yes };

  /**
   * Return the display name for this time zone.
   */
  template <typename B>
  ICUResult GetDisplayName(const char* aLocale,
                           DaylightSavings aDaylightSavings, B& aBuffer) {
#if MOZ_INTL_USE_ICU_CPP_TIMEZONE
    icu::UnicodeString displayName;
    mTimeZone->getDisplayName(static_cast<bool>(aDaylightSavings),
                              icu::TimeZone::LONG, icu::Locale(aLocale),
                              displayName);

    int32_t length = displayName.length();
    if (!aBuffer.reserve(AssertedCast<size_t>(length))) {
      return Err(ICUError::OutOfMemory);
    }

    // Copy the display name.
    UErrorCode status = U_ZERO_ERROR;
    int32_t written = displayName.extract(aBuffer.data(), length, status);
    if (!ICUSuccessForStringSpan(status)) {
      return Err(ToICUError(status));
    }
    MOZ_ASSERT(written == length);

    aBuffer.written(written);

    return Ok{};
#else
    return FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          UCalendarDisplayNameType type =
              static_cast<bool>(aDaylightSavings) ? UCAL_DST : UCAL_STANDARD;
          return ucal_getTimeZoneDisplayName(mCalendar, type, aLocale, target,
                                             length, status);
        });
#endif
  }

  /**
   * Fill the buffer with the system's default IANA time zone identifier, e.g.
   * "America/Chicago".
   */
  template <typename B>
  static ICUResult GetDefaultTimeZone(B& aBuffer) {
    return FillBufferWithICUCall(aBuffer, ucal_getDefaultTimeZone);
  }

  /**
   * Fill the buffer with the host system's default IANA time zone identifier,
   * e.g. "America/Chicago".
   *
   * NOTE: This function is not thread-safe.
   */
  template <typename B>
  static ICUResult GetHostTimeZone(B& aBuffer) {
    return FillBufferWithICUCall(aBuffer, ucal_getHostTimeZone);
  }

  /**
   * Set the default time zone.
   */
  static Result<bool, ICUError> SetDefaultTimeZone(Span<const char> aTimeZone);

  /**
   * Set the default time zone using the host system's time zone.
   *
   * NOTE: This function is not thread-safe.
   */
  static ICUResult SetDefaultTimeZoneFromHostTimeZone();

  /**
   * Return the tzdata version.
   *
   * The tzdata version is a string of the form "<year><release>", e.g. "2021a".
   */
  static Result<Span<const char>, ICUError> GetTZDataVersion();

  /**
   * Constant for the typical maximal length of a time zone identifier.
   *
   * At the time of this writing 32 characters fits every supported time zone:
   *
   * Intl.supportedValuesOf("timeZone")
   *     .reduce((acc, v) => Math.max(acc, v.length), 0)
   */
  static constexpr size_t TimeZoneIdentifierLength = 32;

  /**
   * Returns the canonical system time zone ID or the normalized custom time
   * zone ID for the given time zone ID.
   */
  template <typename B>
  static ICUResult GetCanonicalTimeZoneID(Span<const char16_t> inputTimeZone,
                                          B& aBuffer) {
    static_assert(std::is_same_v<typename B::CharType, char16_t>,
                  "Currently only UTF-16 buffers are supported.");

    if (aBuffer.capacity() == 0) {
      // ucal_getCanonicalTimeZoneID differs from other API calls and fails when
      // passed a nullptr or 0 length result. Reserve some space initially so
      // that a real pointer will be used in the API.
      if (!aBuffer.reserve(TimeZoneIdentifierLength)) {
        return Err(ICUError::OutOfMemory);
      }
    }

    return FillBufferWithICUCall(
        aBuffer,
        [&inputTimeZone](UChar* target, int32_t length, UErrorCode* status) {
          return ucal_getCanonicalTimeZoneID(
              inputTimeZone.Elements(),
              static_cast<int32_t>(inputTimeZone.Length()), target, length,
              /* isSystemID */ nullptr, status);
        });
  }

  /**
   * Return an enumeration over all time zones commonly used in the given
   * region.
   */
  static Result<SpanEnumeration<char>, ICUError> GetAvailableTimeZones(
      const char* aRegion);

  /**
   * Return an enumeration over all available time zones.
   */
  static Result<SpanEnumeration<char>, ICUError> GetAvailableTimeZones();

 private:
#if MOZ_INTL_USE_ICU_CPP_TIMEZONE
  UniquePtr<icu::TimeZone> mTimeZone = nullptr;
#else
  UCalendar* mCalendar = nullptr;
#endif
};

}  // namespace mozilla::intl

#endif
