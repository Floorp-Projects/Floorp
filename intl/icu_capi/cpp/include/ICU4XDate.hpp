#ifndef ICU4XDate_HPP
#define ICU4XDate_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDate.h"

class ICU4XCalendar;
class ICU4XDate;
#include "ICU4XError.hpp"
class ICU4XIsoDate;
#include "ICU4XIsoWeekday.hpp"
class ICU4XWeekCalculator;
struct ICU4XWeekOf;

/**
 * A destruction policy for using ICU4XDate with std::unique_ptr.
 */
struct ICU4XDateDeleter {
  void operator()(capi::ICU4XDate* l) const noexcept {
    capi::ICU4XDate_destroy(l);
  }
};

/**
 * An ICU4X Date object capable of containing a date and time for any calendar.
 * 
 * See the [Rust documentation for `Date`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html) for more information.
 */
class ICU4XDate {
 public:

  /**
   * Creates a new [`ICU4XDate`] representing the ISO date and time
   * given but in a given calendar
   * 
   * See the [Rust documentation for `new_from_iso`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.new_from_iso) for more information.
   */
  static diplomat::result<ICU4XDate, ICU4XError> create_from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, const ICU4XCalendar& calendar);

  /**
   * Creates a new [`ICU4XDate`] from the given codes, which are interpreted in the given calendar system
   * 
   * See the [Rust documentation for `try_new_from_codes`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.try_new_from_codes) for more information.
   */
  static diplomat::result<ICU4XDate, ICU4XError> create_from_codes_in_calendar(const std::string_view era_code, int32_t year, const std::string_view month_code, uint8_t day, const ICU4XCalendar& calendar);

  /**
   * Convert this date to one in a different calendar
   * 
   * See the [Rust documentation for `to_calendar`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.to_calendar) for more information.
   */
  ICU4XDate to_calendar(const ICU4XCalendar& calendar) const;

  /**
   * Converts this date to ISO
   * 
   * See the [Rust documentation for `to_iso`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.to_iso) for more information.
   */
  ICU4XIsoDate to_iso() const;

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
   * See the [Rust documentation for `year`](https://docs.rs/icu/latest/icu/struct.Date.html#method.year) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/types/struct.Era.html)
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> era_to_writeable(W& write) const;

  /**
   * Returns the era for this date,
   * 
   * See the [Rust documentation for `year`](https://docs.rs/icu/latest/icu/struct.Date.html#method.year) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/types/struct.Era.html)
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
  inline const capi::ICU4XDate* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XDate* AsFFIMut() { return this->inner.get(); }
  inline ICU4XDate(capi::ICU4XDate* i) : inner(i) {}
  ICU4XDate() = default;
  ICU4XDate(ICU4XDate&&) noexcept = default;
  ICU4XDate& operator=(ICU4XDate&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XDate, ICU4XDateDeleter> inner;
};

#include "ICU4XCalendar.hpp"
#include "ICU4XIsoDate.hpp"
#include "ICU4XWeekCalculator.hpp"
#include "ICU4XWeekOf.hpp"

inline diplomat::result<ICU4XDate, ICU4XError> ICU4XDate::create_from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, const ICU4XCalendar& calendar) {
  auto diplomat_result_raw_out_value = capi::ICU4XDate_create_from_iso_in_calendar(year, month, day, calendar.AsFFI());
  diplomat::result<ICU4XDate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDate>(ICU4XDate(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XDate, ICU4XError> ICU4XDate::create_from_codes_in_calendar(const std::string_view era_code, int32_t year, const std::string_view month_code, uint8_t day, const ICU4XCalendar& calendar) {
  auto diplomat_result_raw_out_value = capi::ICU4XDate_create_from_codes_in_calendar(era_code.data(), era_code.size(), year, month_code.data(), month_code.size(), day, calendar.AsFFI());
  diplomat::result<ICU4XDate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDate>(ICU4XDate(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XDate ICU4XDate::to_calendar(const ICU4XCalendar& calendar) const {
  return ICU4XDate(capi::ICU4XDate_to_calendar(this->inner.get(), calendar.AsFFI()));
}
inline ICU4XIsoDate ICU4XDate::to_iso() const {
  return ICU4XIsoDate(capi::ICU4XDate_to_iso(this->inner.get()));
}
inline uint32_t ICU4XDate::day_of_month() const {
  return capi::ICU4XDate_day_of_month(this->inner.get());
}
inline ICU4XIsoWeekday ICU4XDate::day_of_week() const {
  return static_cast<ICU4XIsoWeekday>(capi::ICU4XDate_day_of_week(this->inner.get()));
}
inline uint32_t ICU4XDate::week_of_month(ICU4XIsoWeekday first_weekday) const {
  return capi::ICU4XDate_week_of_month(this->inner.get(), static_cast<capi::ICU4XIsoWeekday>(first_weekday));
}
inline diplomat::result<ICU4XWeekOf, ICU4XError> ICU4XDate::week_of_year(const ICU4XWeekCalculator& calculator) const {
  auto diplomat_result_raw_out_value = capi::ICU4XDate_week_of_year(this->inner.get(), calculator.AsFFI());
  diplomat::result<ICU4XWeekOf, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
  capi::ICU4XWeekOf diplomat_raw_struct_out_value = diplomat_result_raw_out_value.ok;
    diplomat_result_out_value = diplomat::Ok<ICU4XWeekOf>(ICU4XWeekOf{ .week = std::move(diplomat_raw_struct_out_value.week), .unit = std::move(static_cast<ICU4XWeekRelativeUnit>(diplomat_raw_struct_out_value.unit)) });
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline uint32_t ICU4XDate::ordinal_month() const {
  return capi::ICU4XDate_ordinal_month(this->inner.get());
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDate::month_code_to_writeable(W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDate_month_code(this->inner.get(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDate::month_code() const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDate_month_code(this->inner.get(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline int32_t ICU4XDate::year_in_era() const {
  return capi::ICU4XDate_year_in_era(this->inner.get());
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDate::era_to_writeable(W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDate_era(this->inner.get(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDate::era() const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDate_era(this->inner.get(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline uint8_t ICU4XDate::months_in_year() const {
  return capi::ICU4XDate_months_in_year(this->inner.get());
}
inline uint8_t ICU4XDate::days_in_month() const {
  return capi::ICU4XDate_days_in_month(this->inner.get());
}
inline uint16_t ICU4XDate::days_in_year() const {
  return capi::ICU4XDate_days_in_year(this->inner.get());
}
inline ICU4XCalendar ICU4XDate::calendar() const {
  return ICU4XCalendar(capi::ICU4XDate_calendar(this->inner.get()));
}
#endif
