#ifndef ICU4XLineBreakStrictness_HPP
#define ICU4XLineBreakStrictness_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLineBreakStrictness.h"



/**
 * See the [Rust documentation for `LineBreakStrictness`](https://docs.rs/icu/latest/icu/segmenter/enum.LineBreakStrictness.html) for more information.
 */
enum struct ICU4XLineBreakStrictness {
  Loose = 0,
  Normal = 1,
  Strict = 2,
  Anywhere = 3,
};

#endif
