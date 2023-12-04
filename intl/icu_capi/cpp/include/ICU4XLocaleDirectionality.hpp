#ifndef ICU4XLocaleDirectionality_HPP
#define ICU4XLocaleDirectionality_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleDirectionality.h"

class ICU4XDataProvider;
class ICU4XLocaleDirectionality;
#include "ICU4XError.hpp"
class ICU4XLocaleExpander;
class ICU4XLocale;
#include "ICU4XLocaleDirection.hpp"

/**
 * A destruction policy for using ICU4XLocaleDirectionality with std::unique_ptr.
 */
struct ICU4XLocaleDirectionalityDeleter {
  void operator()(capi::ICU4XLocaleDirectionality* l) const noexcept {
    capi::ICU4XLocaleDirectionality_destroy(l);
  }
};

/**
 * See the [Rust documentation for `LocaleDirectionality`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html) for more information.
 */
class ICU4XLocaleDirectionality {
 public:

  /**
   * Construct a new ICU4XLocaleDirectionality instance
   * 
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.new) for more information.
   */
  static diplomat::result<ICU4XLocaleDirectionality, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Construct a new ICU4XLocaleDirectionality instance with a custom expander
   * 
   * See the [Rust documentation for `new_with_expander`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.new_with_expander) for more information.
   */
  static diplomat::result<ICU4XLocaleDirectionality, ICU4XError> create_with_expander(const ICU4XDataProvider& provider, const ICU4XLocaleExpander& expander);

  /**
   * See the [Rust documentation for `get`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.get) for more information.
   */
  ICU4XLocaleDirection get(const ICU4XLocale& locale) const;

  /**
   * See the [Rust documentation for `is_left_to_right`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.is_left_to_right) for more information.
   */
  bool is_left_to_right(const ICU4XLocale& locale) const;

  /**
   * See the [Rust documentation for `is_right_to_left`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.is_right_to_left) for more information.
   */
  bool is_right_to_left(const ICU4XLocale& locale) const;
  inline const capi::ICU4XLocaleDirectionality* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLocaleDirectionality* AsFFIMut() { return this->inner.get(); }
  inline ICU4XLocaleDirectionality(capi::ICU4XLocaleDirectionality* i) : inner(i) {}
  ICU4XLocaleDirectionality() = default;
  ICU4XLocaleDirectionality(ICU4XLocaleDirectionality&&) noexcept = default;
  ICU4XLocaleDirectionality& operator=(ICU4XLocaleDirectionality&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLocaleDirectionality, ICU4XLocaleDirectionalityDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocaleExpander.hpp"
#include "ICU4XLocale.hpp"

inline diplomat::result<ICU4XLocaleDirectionality, ICU4XError> ICU4XLocaleDirectionality::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleDirectionality_create(provider.AsFFI());
  diplomat::result<ICU4XLocaleDirectionality, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleDirectionality>(ICU4XLocaleDirectionality(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XLocaleDirectionality, ICU4XError> ICU4XLocaleDirectionality::create_with_expander(const ICU4XDataProvider& provider, const ICU4XLocaleExpander& expander) {
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleDirectionality_create_with_expander(provider.AsFFI(), expander.AsFFI());
  diplomat::result<ICU4XLocaleDirectionality, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleDirectionality>(ICU4XLocaleDirectionality(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XLocaleDirection ICU4XLocaleDirectionality::get(const ICU4XLocale& locale) const {
  return static_cast<ICU4XLocaleDirection>(capi::ICU4XLocaleDirectionality_get(this->inner.get(), locale.AsFFI()));
}
inline bool ICU4XLocaleDirectionality::is_left_to_right(const ICU4XLocale& locale) const {
  return capi::ICU4XLocaleDirectionality_is_left_to_right(this->inner.get(), locale.AsFFI());
}
inline bool ICU4XLocaleDirectionality::is_right_to_left(const ICU4XLocale& locale) const {
  return capi::ICU4XLocaleDirectionality_is_right_to_left(this->inner.get(), locale.AsFFI());
}
#endif
