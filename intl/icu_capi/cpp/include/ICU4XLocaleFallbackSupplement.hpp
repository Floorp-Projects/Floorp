#ifndef ICU4XLocaleFallbackSupplement_HPP
#define ICU4XLocaleFallbackSupplement_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleFallbackSupplement.h"



/**
 * What additional data is required to load when performing fallback.
 * 
 * See the [Rust documentation for `LocaleFallbackSupplement`](https://docs.rs/icu/latest/icu/locid_transform/fallback/enum.LocaleFallbackSupplement.html) for more information.
 */
enum struct ICU4XLocaleFallbackSupplement {
  None = 0,
  Collation = 1,
};

#endif
