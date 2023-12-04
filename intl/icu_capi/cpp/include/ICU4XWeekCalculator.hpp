#ifndef ICU4XWeekCalculator_HPP
#define ICU4XWeekCalculator_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XWeekCalculator.h"

class ICU4XDataProvider;
class ICU4XLocale;
class ICU4XWeekCalculator;
#include "ICU4XError.hpp"
#include "ICU4XIsoWeekday.hpp"

/**
 * A destruction policy for using ICU4XWeekCalculator with std::unique_ptr.
 */
struct ICU4XWeekCalculatorDeleter {
  void operator()(capi::ICU4XWeekCalculator* l) const noexcept {
    capi::ICU4XWeekCalculator_destroy(l);
  }
};

/**
 * A Week calculator, useful to be passed in to `week_of_year()` on Date and DateTime types
 * 
 * See the [Rust documentation for `WeekCalculator`](https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html) for more information.
 */
class ICU4XWeekCalculator {
 public:

  /**
   * Creates a new [`ICU4XWeekCalculator`] from locale data.
   * 
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#method.try_new) for more information.
   */
  static diplomat::result<ICU4XWeekCalculator, ICU4XError> create(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * Additional information: [1](https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#structfield.first_weekday), [2](https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#structfield.min_week_days)
   */
  static ICU4XWeekCalculator create_from_first_day_of_week_and_min_week_days(ICU4XIsoWeekday first_weekday, uint8_t min_week_days);

  /**
   * Returns the weekday that starts the week for this object's locale
   * 
   * See the [Rust documentation for `first_weekday`](https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#structfield.first_weekday) for more information.
   */
  ICU4XIsoWeekday first_weekday() const;

  /**
   * The minimum number of days overlapping a year required for a week to be
   * considered part of that year
   * 
   * See the [Rust documentation for `min_week_days`](https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#structfield.min_week_days) for more information.
   */
  uint8_t min_week_days() const;
  inline const capi::ICU4XWeekCalculator* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XWeekCalculator* AsFFIMut() { return this->inner.get(); }
  inline ICU4XWeekCalculator(capi::ICU4XWeekCalculator* i) : inner(i) {}
  ICU4XWeekCalculator() = default;
  ICU4XWeekCalculator(ICU4XWeekCalculator&&) noexcept = default;
  ICU4XWeekCalculator& operator=(ICU4XWeekCalculator&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XWeekCalculator, ICU4XWeekCalculatorDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"

inline diplomat::result<ICU4XWeekCalculator, ICU4XError> ICU4XWeekCalculator::create(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XWeekCalculator_create(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XWeekCalculator, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XWeekCalculator>(ICU4XWeekCalculator(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XWeekCalculator ICU4XWeekCalculator::create_from_first_day_of_week_and_min_week_days(ICU4XIsoWeekday first_weekday, uint8_t min_week_days) {
  return ICU4XWeekCalculator(capi::ICU4XWeekCalculator_create_from_first_day_of_week_and_min_week_days(static_cast<capi::ICU4XIsoWeekday>(first_weekday), min_week_days));
}
inline ICU4XIsoWeekday ICU4XWeekCalculator::first_weekday() const {
  return static_cast<ICU4XIsoWeekday>(capi::ICU4XWeekCalculator_first_weekday(this->inner.get()));
}
inline uint8_t ICU4XWeekCalculator::min_week_days() const {
  return capi::ICU4XWeekCalculator_min_week_days(this->inner.get());
}
#endif
