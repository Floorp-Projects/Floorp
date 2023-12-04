#ifndef ICU4XLocaleFallbackConfig_HPP
#define ICU4XLocaleFallbackConfig_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleFallbackConfig.h"

#include "ICU4XLocaleFallbackPriority.hpp"
#include "ICU4XLocaleFallbackSupplement.hpp"


/**
 * Collection of configurations for the ICU4X fallback algorithm.
 * 
 * See the [Rust documentation for `LocaleFallbackConfig`](https://docs.rs/icu/latest/icu/locid_transform/fallback/struct.LocaleFallbackConfig.html) for more information.
 */
struct ICU4XLocaleFallbackConfig {
 public:

  /**
   * Choice of priority mode.
   */
  ICU4XLocaleFallbackPriority priority;

  /**
   * An empty string is considered `None`.
   */
  std::string_view extension_key;

  /**
   * Fallback supplement data key to customize fallback rules.
   */
  ICU4XLocaleFallbackSupplement fallback_supplement;
};


#endif
