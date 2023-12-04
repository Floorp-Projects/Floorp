#ifndef ICU4XTrailingCase_HPP
#define ICU4XTrailingCase_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTrailingCase.h"



/**
 * See the [Rust documentation for `TrailingCase`](https://docs.rs/icu/latest/icu/casemap/titlecase/enum.TrailingCase.html) for more information.
 */
enum struct ICU4XTrailingCase {
  Lower = 0,
  Unchanged = 1,
};

#endif
