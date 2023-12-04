#ifndef ICU4XIsoDate_HPP
#define ICU4XIsoDate_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XIsoDate.h"

class ICU4XIsoDate;
#include "ICU4XError.hpp"
class ICU4XCalendar;
class ICU4XDate;
#include "ICU4XIsoWeekday.hpp"
class ICU4XWeekCalculator;
struct ICU4XWeekOf;

/**
 * A destruction policy for using ICU4XIsoDate with std::unique_ptr.
 */
struct ICU4XIsoDateDeleter {
  void operator()(capi::ICU4XIsoDate* l) const noexcept {
    capi::ICU4XIsoDate_destroy(l);
  }
};

/**
 * An ICU4X Date object capable of containing a ISO-8601 date
 * 
 * See the [Rust documentation for `Date`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html) for more information.
 */
class ICU4XIsoDate {
 public:

  /**
   * Creates a new [`ICU4XIsoDate`] from the specified date and time.
   * 
   * See the [Rust documentation for `try_new_iso_date`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.try_new_iso_date) for more information.
   */
  static diplomat::result<ICU4XIsoDate, ICU4XError> create(int32_t year, uint8_t month, uint8_t day);

  /**
   * Creates a new [`ICU4XIsoDate`] representing January 1, 1970.
   * 
   * See the [Rust documentation for `unix_epoch`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.unix_epoch) for more information.
   */
  static ICU4XIsoDate create_for_unix_epoch();

  /**
   * Convert this date to one in a different calendar
   * 
   * See the [Rust documentation for `to_calendar`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.to_calendar) for more information.
   */
  ICU4XDate to_calendar(const ICU4XCalendar& calendar) const;

  /**
   * See the [Rust documentation for `to_any`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.to_any) for more information.
   */
  ICU4XDate to_any() const;

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
   * See the [Rust documentation for `month`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month) for more information.
   */
  uint32_t month() const;

  /**
   * Returns the year number for this date
   * 
   * See the [Rust documentation for `year`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year) for more information.
   */
  int32_t year() const;

  /**
   * Returns if the year is a leap year for this date
   * 
   * See the [Rust documentation for `is_in_leap_year`](https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.is_in_leap_year) for more information.
   */
  bool is_in_leap_year() const;

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
  inline const capi::ICU4XIsoDate* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XIsoDate* AsFFIMut() { return this->inner.get(); }
  inline ICU4XIsoDate(capi::ICU4XIsoDate* i) : inner(i) {}
  ICU4XIsoDate() = default;
  ICU4XIsoDate(ICU4XIsoDate&&) noexcept = default;
  ICU4XIsoDate& operator=(ICU4XIsoDate&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XIsoDate, ICU4XIsoDateDeleter> inner;
};

#include "ICU4XCalendar.hpp"
#include "ICU4XDate.hpp"
#include "ICU4XWeekCalculator.hpp"
#include "ICU4XWeekOf.hpp"

inline diplomat::result<ICU4XIsoDate, ICU4XError> ICU4XIsoDate::create(int32_t year, uint8_t month, uint8_t day) {
  auto diplomat_result_raw_out_value = capi::ICU4XIsoDate_create(year, month, day);
  diplomat::result<ICU4XIsoDate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XIsoDate>(ICU4XIsoDate(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XIsoDate ICU4XIsoDate::create_for_unix_epoch() {
  return ICU4XIsoDate(capi::ICU4XIsoDate_create_for_unix_epoch());
}
inline ICU4XDate ICU4XIsoDate::to_calendar(const ICU4XCalendar& calendar) const {
  return ICU4XDate(capi::ICU4XIsoDate_to_calendar(this->inner.get(), calendar.AsFFI()));
}
inline ICU4XDate ICU4XIsoDate::to_any() const {
  return ICU4XDate(capi::ICU4XIsoDate_to_any(this->inner.get()));
}
inline uint32_t ICU4XIsoDate::day_of_month() const {
  return capi::ICU4XIsoDate_day_of_month(this->inner.get());
}
inline ICU4XIsoWeekday ICU4XIsoDate::day_of_week() const {
  return static_cast<ICU4XIsoWeekday>(capi::ICU4XIsoDate_day_of_week(this->inner.get()));
}
inline uint32_t ICU4XIsoDate::week_of_month(ICU4XIsoWeekday first_weekday) const {
  return capi::ICU4XIsoDate_week_of_month(this->inner.get(), static_cast<capi::ICU4XIsoWeekday>(first_weekday));
}
inline diplomat::result<ICU4XWeekOf, ICU4XError> ICU4XIsoDate::week_of_year(const ICU4XWeekCalculator& calculator) const {
  auto diplomat_result_raw_out_value = capi::ICU4XIsoDate_week_of_year(this->inner.get(), calculator.AsFFI());
  diplomat::result<ICU4XWeekOf, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
  capi::ICU4XWeekOf diplomat_raw_struct_out_value = diplomat_result_raw_out_value.ok;
    diplomat_result_out_value = diplomat::Ok<ICU4XWeekOf>(ICU4XWeekOf{ .week = std::move(diplomat_raw_struct_out_value.week), .unit = std::move(static_cast<ICU4XWeekRelativeUnit>(diplomat_raw_struct_out_value.unit)) });
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline uint32_t ICU4XIsoDate::month() const {
  return capi::ICU4XIsoDate_month(this->inner.get());
}
inline int32_t ICU4XIsoDate::year() const {
  return capi::ICU4XIsoDate_year(this->inner.get());
}
inline bool ICU4XIsoDate::is_in_leap_year() const {
  return capi::ICU4XIsoDate_is_in_leap_year(this->inner.get());
}
inline uint8_t ICU4XIsoDate::months_in_year() const {
  return capi::ICU4XIsoDate_months_in_year(this->inner.get());
}
inline uint8_t ICU4XIsoDate::days_in_month() const {
  return capi::ICU4XIsoDate_days_in_month(this->inner.get());
}
inline uint16_t ICU4XIsoDate::days_in_year() const {
  return capi::ICU4XIsoDate_days_in_year(this->inner.get());
}
#endif
