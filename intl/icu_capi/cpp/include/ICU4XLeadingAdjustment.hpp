#ifndef ICU4XLeadingAdjustment_HPP
#define ICU4XLeadingAdjustment_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLeadingAdjustment.h"



/**
 * See the [Rust documentation for `LeadingAdjustment`](https://docs.rs/icu/latest/icu/casemap/titlecase/enum.LeadingAdjustment.html) for more information.
 */
enum struct ICU4XLeadingAdjustment {
  Auto = 0,
  None = 1,
  ToCased = 2,
};

#endif
