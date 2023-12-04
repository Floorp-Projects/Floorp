#ifndef ICU4XWeekOf_HPP
#define ICU4XWeekOf_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XWeekOf.h"

#include "ICU4XWeekRelativeUnit.hpp"


/**
 * See the [Rust documentation for `WeekOf`](https://docs.rs/icu/latest/icu/calendar/week/struct.WeekOf.html) for more information.
 */
struct ICU4XWeekOf {
 public:
  uint16_t week;
  ICU4XWeekRelativeUnit unit;
};


#endif
