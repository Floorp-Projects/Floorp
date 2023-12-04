#ifndef ICU4XLocaleExpander_HPP
#define ICU4XLocaleExpander_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleExpander.h"

class ICU4XDataProvider;
class ICU4XLocaleExpander;
#include "ICU4XError.hpp"
class ICU4XLocale;
#include "ICU4XTransformResult.hpp"

/**
 * A destruction policy for using ICU4XLocaleExpander with std::unique_ptr.
 */
struct ICU4XLocaleExpanderDeleter {
  void operator()(capi::ICU4XLocaleExpander* l) const noexcept {
    capi::ICU4XLocaleExpander_destroy(l);
  }
};

/**
 * A locale expander.
 * 
 * See the [Rust documentation for `LocaleExpander`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html) for more information.
 */
class ICU4XLocaleExpander {
 public:

  /**
   * Create a new [`ICU4XLocaleExpander`].
   * 
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html#method.new) for more information.
   */
  static diplomat::result<ICU4XLocaleExpander, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Create a new [`ICU4XLocaleExpander`] with extended data.
   * 
   * See the [Rust documentation for `new_extended`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html#method.new_extended) for more information.
   */
  static diplomat::result<ICU4XLocaleExpander, ICU4XError> create_extended(const ICU4XDataProvider& provider);

  /**
   * FFI version of `LocaleExpander::maximize()`.
   * 
   * See the [Rust documentation for `maximize`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html#method.maximize) for more information.
   */
  ICU4XTransformResult maximize(ICU4XLocale& locale) const;

  /**
   * FFI version of `LocaleExpander::minimize()`.
   * 
   * See the [Rust documentation for `minimize`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html#method.minimize) for more information.
   */
  ICU4XTransformResult minimize(ICU4XLocale& locale) const;
  inline const capi::ICU4XLocaleExpander* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLocaleExpander* AsFFIMut() { return this->inner.get(); }
  inline ICU4XLocaleExpander(capi::ICU4XLocaleExpander* i) : inner(i) {}
  ICU4XLocaleExpander() = default;
  ICU4XLocaleExpander(ICU4XLocaleExpander&&) noexcept = default;
  ICU4XLocaleExpander& operator=(ICU4XLocaleExpander&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLocaleExpander, ICU4XLocaleExpanderDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"

inline diplomat::result<ICU4XLocaleExpander, ICU4XError> ICU4XLocaleExpander::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleExpander_create(provider.AsFFI());
  diplomat::result<ICU4XLocaleExpander, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleExpander>(ICU4XLocaleExpander(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XLocaleExpander, ICU4XError> ICU4XLocaleExpander::create_extended(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleExpander_create_extended(provider.AsFFI());
  diplomat::result<ICU4XLocaleExpander, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleExpander>(ICU4XLocaleExpander(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XTransformResult ICU4XLocaleExpander::maximize(ICU4XLocale& locale) const {
  return static_cast<ICU4XTransformResult>(capi::ICU4XLocaleExpander_maximize(this->inner.get(), locale.AsFFIMut()));
}
inline ICU4XTransformResult ICU4XLocaleExpander::minimize(ICU4XLocale& locale) const {
  return static_cast<ICU4XTransformResult>(capi::ICU4XLocaleExpander_minimize(this->inner.get(), locale.AsFFIMut()));
}
#endif
