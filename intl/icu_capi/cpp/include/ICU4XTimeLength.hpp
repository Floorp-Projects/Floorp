#ifndef ICU4XTimeLength_HPP
#define ICU4XTimeLength_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTimeLength.h"



/**
 * See the [Rust documentation for `Time`](https://docs.rs/icu/latest/icu/datetime/options/length/enum.Time.html) for more information.
 */
enum struct ICU4XTimeLength {
  Full = 0,
  Long = 1,
  Medium = 2,
  Short = 3,
};

#endif
