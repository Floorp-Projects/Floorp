#ifndef ICU4XLocaleFallbackerWithConfig_HPP
#define ICU4XLocaleFallbackerWithConfig_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleFallbackerWithConfig.h"

class ICU4XLocale;
class ICU4XLocaleFallbackIterator;

/**
 * A destruction policy for using ICU4XLocaleFallbackerWithConfig with std::unique_ptr.
 */
struct ICU4XLocaleFallbackerWithConfigDeleter {
  void operator()(capi::ICU4XLocaleFallbackerWithConfig* l) const noexcept {
    capi::ICU4XLocaleFallbackerWithConfig_destroy(l);
  }
};

/**
 * An object that runs the ICU4X locale fallback algorithm with specific configurations.
 * 
 * See the [Rust documentation for `LocaleFallbacker`](https://docs.rs/icu/latest/icu/locid_transform/fallback/struct.LocaleFallbacker.html) for more information.
 * 
 * See the [Rust documentation for `LocaleFallbackerWithConfig`](https://docs.rs/icu/latest/icu/locid_transform/fallback/struct.LocaleFallbackerWithConfig.html) for more information.
 */
class ICU4XLocaleFallbackerWithConfig {
 public:

  /**
   * Creates an iterator from a locale with each step of fallback.
   * 
   * See the [Rust documentation for `fallback_for`](https://docs.rs/icu/latest/icu/locid_transform/fallback/struct.LocaleFallbacker.html#method.fallback_for) for more information.
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  ICU4XLocaleFallbackIterator fallback_for_locale(const ICU4XLocale& locale) const;
  inline const capi::ICU4XLocaleFallbackerWithConfig* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLocaleFallbackerWithConfig* AsFFIMut() { return this->inner.get(); }
  inline explicit ICU4XLocaleFallbackerWithConfig(capi::ICU4XLocaleFallbackerWithConfig* i) : inner(i) {}
  ICU4XLocaleFallbackerWithConfig() = default;
  ICU4XLocaleFallbackerWithConfig(ICU4XLocaleFallbackerWithConfig&&) noexcept = default;
  ICU4XLocaleFallbackerWithConfig& operator=(ICU4XLocaleFallbackerWithConfig&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLocaleFallbackerWithConfig, ICU4XLocaleFallbackerWithConfigDeleter> inner;
};

#include "ICU4XLocale.hpp"
#include "ICU4XLocaleFallbackIterator.hpp"

inline ICU4XLocaleFallbackIterator ICU4XLocaleFallbackerWithConfig::fallback_for_locale(const ICU4XLocale& locale) const {
  return ICU4XLocaleFallbackIterator(capi::ICU4XLocaleFallbackerWithConfig_fallback_for_locale(this->inner.get(), locale.AsFFI()));
}
#endif
