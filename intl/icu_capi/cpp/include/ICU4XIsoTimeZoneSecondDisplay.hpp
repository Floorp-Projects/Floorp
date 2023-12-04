#ifndef ICU4XIsoTimeZoneSecondDisplay_HPP
#define ICU4XIsoTimeZoneSecondDisplay_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XIsoTimeZoneSecondDisplay.h"



/**
 * See the [Rust documentation for `IsoSeconds`](https://docs.rs/icu/latest/icu/datetime/time_zone/enum.IsoSeconds.html) for more information.
 */
enum struct ICU4XIsoTimeZoneSecondDisplay {
  Optional = 0,
  Never = 1,
};

#endif
