#ifndef ICU4XDateLength_HPP
#define ICU4XDateLength_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDateLength.h"



/**
 * See the [Rust documentation for `Date`](https://docs.rs/icu/latest/icu/datetime/options/length/enum.Date.html) for more information.
 */
enum struct ICU4XDateLength {
  Full = 0,
  Long = 1,
  Medium = 2,
  Short = 3,
};

#endif
