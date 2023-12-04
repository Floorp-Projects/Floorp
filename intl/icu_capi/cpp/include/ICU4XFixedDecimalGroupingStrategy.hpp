#ifndef ICU4XFixedDecimalGroupingStrategy_HPP
#define ICU4XFixedDecimalGroupingStrategy_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XFixedDecimalGroupingStrategy.h"



/**
 * See the [Rust documentation for `GroupingStrategy`](https://docs.rs/icu/latest/icu/decimal/options/enum.GroupingStrategy.html) for more information.
 */
enum struct ICU4XFixedDecimalGroupingStrategy {
  Auto = 0,
  Never = 1,
  Always = 2,
  Min2 = 3,
};

#endif
