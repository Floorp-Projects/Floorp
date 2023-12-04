#ifndef ICU4XDateTime_HPP
#define ICU4XDateTime_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDateTime.h"

class ICU4XCalendar;
class ICU4XDateTime;
#include "ICU4XError.hpp"
class ICU4XDate;
class ICU4XTime;
class ICU4XIsoDateTime;
#include "ICU4XIsoWeekday.hpp"
class ICU4XWeekCalculator;
struct ICU4XWeekOf;

/**
 * A destruction policy for using ICU4XDateTime with std::unique_ptr.
 */
struct ICU4XDateTimeDeleter {
  void operator()(capi::ICU4XDateTime* l) const noexcept {
    capi::ICU4XDateTime_destroy(l);
  }
};

/**
 * An ICU4X DateTime object capable of containing a date and time for any calendar.
 * 
 * See the [Rust documentation for `DateTime`](https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html) for more information.
 */
class ICU4XDateTime {
 public:

  /**
   * Creates a new [`ICU4XDateTime`] representing the ISO date and time
   * given but in a given calendar
   * 
   * See the [Rust documentation for `new_from_iso`](https://docs.rs/icu/latest/icu/struct.DateTime.html#method.new_from_iso) for more information.
   */
  static diplomat::result<ICU4XDateTime, ICU4XError> create_from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const ICU4XCalendar& calendar);

  /**
   * Creates a new [`ICU4XDateTime`] from the given codes, which are interpreted in the given calendar system
   * 
   * See the [Rust documentation for `try_new_from_codes`](https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.try_new_from_codes) for more information.
   */
  static diplomat::result<ICU4XDateTime, ICU4XError> create_from_codes_in_calendar(const std::string_view era_code, int32_t year, const std::string_view month_code, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const ICU4XCalendar& calendar);

  /**
   * Creates a new [`ICU4XDateTime`] from an [`ICU4XDate`] and [`ICU4XTime`] object
   * 
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.new) for more information.
   */
  static ICU4XDateTime create_from_date_and_time(const ICU4XDate& date, const ICU4XTime& time);

  /**
   * Gets a copy of the date contained in this object
   * 
   * See the [Rust documentation for `date`](https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.date) for more information.
   */
  ICU4XDate date() const;

  /**
   * Gets the time contained in this object
   * 
   * See the [Rust documentation for `time`](https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.time) for more information.
   */
  ICU4XTime time() const;

  /**
   * Converts this date to ISO
   * 
   * See the [Rust documentation for `to_iso`](https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_iso) for more information.
   */
  ICU4XIsoDateTime to_iso() const;

  /**
   * Convert this datetime to one in a different calendar
   * 
   * See the [Rust documentation for `to_calendar`](https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_calendar) for more information.
   */
  ICU4XDateTime to_calendar(const ICU4XCalendar& calendar) const;

  /**
   * Returns the hour in this time
   * 
   * See the [Rust documentation for `hour`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.hour) for more information.
   */
  uint8_t hour() const;

  /**
   * Returns the minute in this time
   * 
   * See the [Rust documentation for `minute`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.minute) for more information.
   */
  uint8_t minute() const;

  /**
   * Returns the second in this time
   * 
   * See the [Rust documentation for `second`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.second) for more information.
   */
  uint8_t second() const;

  /**
   * Returns the nanosecond in this time
   * 
   * See the [Rust documentation for `nanosecond`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.nanosecond) for more information.
   */
  uint32_t nanosecond() const;

  /**
   * Returns the 1-indexed day in the month for this date
   * 
   * See the [Rust documentation for `day_of_month`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.day_of_month) for more information.
   */
  uint32_t day_of_month() const;

  /**
   * Returns the day in the week for this day
   * 
   * See the [Rust documentation for `day_of_week`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.day_of_week) for more information.
   */
  ICU4XIsoWeekday day_of_week() const;

  /**
   * Returns the week number in this month, 1-indexed, based on what
   * is considered the first day of the week (often a locale preference).
   * 
   * `first_weekday` can be obtained via `first_weekday()` on [`ICU4XWeekCalculator`]
   * 
   * See the [Rust documentation for `week_of_month`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.week_of_month) for more information.
   */
  uint32_t week_of_month(ICU4XIsoWeekday first_weekday) const;

  /**
   * Returns the week number in this year, using week data
   * 
   * See the [Rust documentation for `week_of_year`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.week_of_year) for more information.
   */
  diplomat::result<ICU4XWeekOf, ICU4XError> week_of_year(const ICU4XWeekCalculator& calculator) const;

  /**
   * Returns 1-indexed number of the month of this date in its year
   * 
   * Note that for lunar calendars this may not lead to the same month
   * having the same ordinal month across years; use month_code if you care
   * about month identity.
   * 
   * See the [Rust documentation for `month`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month) for more information.
   */
  uint32_t ordinal_month() const;

  /**
   * Returns the month code for this date. Typically something
   * like "M01", "M02", but can be more complicated for lunar calendars.
   * 
   * See the [Rust documentation for `month`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> month_code_to_writeable(W& write) const;

  /**
   * Returns the month code for this date. Typically something
   * like "M01", "M02", but can be more complicated for lunar calendars.
   * 
   * See the [Rust documentation for `month`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month) for more information.
   */
  diplomat::result<std::string, ICU4XError> month_code() const;

  /**
   * Returns the year number in the current era for this date
   * 
   * See the [Rust documentation for `year`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year) for more information.
   */
  int32_t year_in_era() const;

  /**
   * Returns the era for this date,
   * 
   * See the [Rust documentation for `year`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> era_to_writeable(W& write) const;

  /**
   * Returns the era for this date,
   * 
   * See the [Rust documentation for `year`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year) for more information.
   */
  diplomat::result<std::string, ICU4XError> era() const;

  /**
   * Returns the number of months in the year represented by this date
   * 
   * See the [Rust documentation for `months_in_year`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.months_in_year) for more information.
   */
  uint8_t months_in_year() const;

  /**
   * Returns the number of days in the month represented by this date
   * 
   * See the [Rust documentation for `days_in_month`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.days_in_month) for more information.
   */
  uint8_t days_in_month() const;

  /**
   * Returns the number of days in the year represented by this date
   * 
   * See the [Rust documentation for `days_in_year`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.days_in_year) for more information.
   */
  uint16_t days_in_year() const;

  /**
   * Returns the [`ICU4XCalendar`] object backing this date
   * 
   * See the [Rust documentation for `calendar`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.calendar) for more information.
   */
  ICU4XCalendar calendar() const;
  inline const capi::ICU4XDateTime* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XDateTime* AsFFIMut() { return this->inner.get(); }
  inline ICU4XDateTime(capi::ICU4XDateTime* i) : inner(i) {}
  ICU4XDateTime() = default;
  ICU4XDateTime(ICU4XDateTime&&) noexcept = default;
  ICU4XDateTime& operator=(ICU4XDateTime&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XDateTime, ICU4XDateTimeDeleter> inner;
};

#include "ICU4XCalendar.hpp"
#include "ICU4XDate.hpp"
#include "ICU4XTime.hpp"
#include "ICU4XIsoDateTime.hpp"
#include "ICU4XWeekCalculator.hpp"
#include "ICU4XWeekOf.hpp"

inline diplomat::result<ICU4XDateTime, ICU4XError> ICU4XDateTime::create_from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const ICU4XCalendar& calendar) {
  auto diplomat_result_raw_out_value = capi::ICU4XDateTime_create_from_iso_in_calendar(year, month, day, hour, minute, second, nanosecond, calendar.AsFFI());
  diplomat::result<ICU4XDateTime, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDateTime>(ICU4XDateTime(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XDateTime, ICU4XError> ICU4XDateTime::create_from_codes_in_calendar(const std::string_view era_code, int32_t year, const std::string_view month_code, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const ICU4XCalendar& calendar) {
  auto diplomat_result_raw_out_value = capi::ICU4XDateTime_create_from_codes_in_calendar(era_code.data(), era_code.size(), year, month_code.data(), month_code.size(), day, hour, minute, second, nanosecond, calendar.AsFFI());
  diplomat::result<ICU4XDateTime, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDateTime>(ICU4XDateTime(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XDateTime ICU4XDateTime::create_from_date_and_time(const ICU4XDate& date, const ICU4XTime& time) {
  return ICU4XDateTime(capi::ICU4XDateTime_create_from_date_and_time(date.AsFFI(), time.AsFFI()));
}
inline ICU4XDate ICU4XDateTime::date() const {
  return ICU4XDate(capi::ICU4XDateTime_date(this->inner.get()));
}
inline ICU4XTime ICU4XDateTime::time() const {
  return ICU4XTime(capi::ICU4XDateTime_time(this->inner.get()));
}
inline ICU4XIsoDateTime ICU4XDateTime::to_iso() const {
  return ICU4XIsoDateTime(capi::ICU4XDateTime_to_iso(this->inner.get()));
}
inline ICU4XDateTime ICU4XDateTime::to_calendar(const ICU4XCalendar& calendar) const {
  return ICU4XDateTime(capi::ICU4XDateTime_to_calendar(this->inner.get(), calendar.AsFFI()));
}
inline uint8_t ICU4XDateTime::hour() const {
  return capi::ICU4XDateTime_hour(this->inner.get());
}
inline uint8_t ICU4XDateTime::minute() const {
  return capi::ICU4XDateTime_minute(this->inner.get());
}
inline uint8_t ICU4XDateTime::second() const {
  return capi::ICU4XDateTime_second(this->inner.get());
}
inline uint32_t ICU4XDateTime::nanosecond() const {
  return capi::ICU4XDateTime_nanosecond(this->inner.get());
}
inline uint32_t ICU4XDateTime::day_of_month() const {
  return capi::ICU4XDateTime_day_of_month(this->inner.get());
}
inline ICU4XIsoWeekday ICU4XDateTime::day_of_week() const {
  return static_cast<ICU4XIsoWeekday>(capi::ICU4XDateTime_day_of_week(this->inner.get()));
}
inline uint32_t ICU4XDateTime::week_of_month(ICU4XIsoWeekday first_weekday) const {
  return capi::ICU4XDateTime_week_of_month(this->inner.get(), static_cast<capi::ICU4XIsoWeekday>(first_weekday));
}
inline diplomat::result<ICU4XWeekOf, ICU4XError> ICU4XDateTime::week_of_year(const ICU4XWeekCalculator& calculator) const {
  auto diplomat_result_raw_out_value = capi::ICU4XDateTime_week_of_year(this->inner.get(), calculator.AsFFI());
  diplomat::result<ICU4XWeekOf, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
  capi::ICU4XWeekOf diplomat_raw_struct_out_value = diplomat_result_raw_out_value.ok;
    diplomat_result_out_value = diplomat::Ok<ICU4XWeekOf>(ICU4XWeekOf{ .week = std::move(diplomat_raw_struct_out_value.week), .unit = std::move(static_cast<ICU4XWeekRelativeUnit>(diplomat_raw_struct_out_value.unit)) });
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline uint32_t ICU4XDateTime::ordinal_month() const {
  return capi::ICU4XDateTime_ordinal_month(this->inner.get());
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDateTime::month_code_to_writeable(W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDateTime_month_code(this->inner.get(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDateTime::month_code() const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDateTime_month_code(this->inner.get(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline int32_t ICU4XDateTime::year_in_era() const {
  return capi::ICU4XDateTime_year_in_era(this->inner.get());
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDateTime::era_to_writeable(W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDateTime_era(this->inner.get(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDateTime::era() const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDateTime_era(this->inner.get(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline uint8_t ICU4XDateTime::months_in_year() const {
  return capi::ICU4XDateTime_months_in_year(this->inner.get());
}
inline uint8_t ICU4XDateTime::days_in_month() const {
  return capi::ICU4XDateTime_days_in_month(this->inner.get());
}
inline uint16_t ICU4XDateTime::days_in_year() const {
  return capi::ICU4XDateTime_days_in_year(this->inner.get());
}
inline ICU4XCalendar ICU4XDateTime::calendar() const {
  return ICU4XCalendar(capi::ICU4XDateTime_calendar(this->inner.get()));
}
#endif
