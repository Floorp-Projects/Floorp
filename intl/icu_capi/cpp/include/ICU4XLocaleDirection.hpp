#ifndef ICU4XLocaleDirection_HPP
#define ICU4XLocaleDirection_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleDirection.h"



/**
 * See the [Rust documentation for `Direction`](https://docs.rs/icu/latest/icu/locid_transform/enum.Direction.html) for more information.
 */
enum struct ICU4XLocaleDirection {
  LeftToRight = 0,
  RightToLeft = 1,
  Unknown = 2,
};

#endif
