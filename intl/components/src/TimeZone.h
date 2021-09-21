/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_TimeZone_h_
#define intl_components_TimeZone_h_

#include "unicode/ucal.h"
#include "unicode/utypes.h"

#include "mozilla/Assertions.h"
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
  explicit TimeZone(UCalendar* aCalendar) : mCalendar(aCalendar) {
    MOZ_ASSERT(aCalendar);
  }

  // Do not allow copy as this class owns the ICU resource. Move is not
  // currently implemented, but a custom move operator could be created if
  // needed.
  TimeZone(const TimeZone&) = delete;
  TimeZone& operator=(const TimeZone&) = delete;

  ~TimeZone();

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
   * Fill the buffer with the system's default IANA time zone identifier, e.g.
   * "America/Chicago".
   */
  template <typename B>
  static ICUResult GetDefaultTimeZone(B& aBuffer) {
    return FillBufferWithICUCall(aBuffer, ucal_getDefaultTimeZone);
  }

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
      //
      // At the time of this writing 32 characters fits every time zone listed
      // in: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
      // https://gist.github.com/gregtatum/f926de157a44e5965864da866fe71e63
      if (!aBuffer.reserve(32)) {
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

 private:
  UCalendar* mCalendar = nullptr;
};

}  // namespace mozilla::intl

#endif
