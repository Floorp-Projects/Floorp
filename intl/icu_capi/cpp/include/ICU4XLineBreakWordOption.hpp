#ifndef ICU4XLineBreakWordOption_HPP
#define ICU4XLineBreakWordOption_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLineBreakWordOption.h"



/**
 * See the [Rust documentation for `LineBreakWordOption`](https://docs.rs/icu/latest/icu/segmenter/enum.LineBreakWordOption.html) for more information.
 */
enum struct ICU4XLineBreakWordOption {
  Normal = 0,
  BreakAll = 1,
  KeepAll = 2,
};

#endif
