#ifndef ICU4XLocaleCanonicalizer_HPP
#define ICU4XLocaleCanonicalizer_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleCanonicalizer.h"

class ICU4XDataProvider;
class ICU4XLocaleCanonicalizer;
#include "ICU4XError.hpp"
class ICU4XLocale;
#include "ICU4XTransformResult.hpp"

/**
 * A destruction policy for using ICU4XLocaleCanonicalizer with std::unique_ptr.
 */
struct ICU4XLocaleCanonicalizerDeleter {
  void operator()(capi::ICU4XLocaleCanonicalizer* l) const noexcept {
    capi::ICU4XLocaleCanonicalizer_destroy(l);
  }
};

/**
 * A locale canonicalizer.
 * 
 * See the [Rust documentation for `LocaleCanonicalizer`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html) for more information.
 */
class ICU4XLocaleCanonicalizer {
 public:

  /**
   * Create a new [`ICU4XLocaleCanonicalizer`].
   * 
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.new) for more information.
   */
  static diplomat::result<ICU4XLocaleCanonicalizer, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Create a new [`ICU4XLocaleCanonicalizer`] with extended data.
   * 
   * See the [Rust documentation for `new_with_expander`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.new_with_expander) for more information.
   */
  static diplomat::result<ICU4XLocaleCanonicalizer, ICU4XError> create_extended(const ICU4XDataProvider& provider);

  /**
   * FFI version of `LocaleCanonicalizer::canonicalize()`.
   * 
   * See the [Rust documentation for `canonicalize`](https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.canonicalize) for more information.
   */
  ICU4XTransformResult canonicalize(ICU4XLocale& locale) const;
  inline const capi::ICU4XLocaleCanonicalizer* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLocaleCanonicalizer* AsFFIMut() { return this->inner.get(); }
  inline ICU4XLocaleCanonicalizer(capi::ICU4XLocaleCanonicalizer* i) : inner(i) {}
  ICU4XLocaleCanonicalizer() = default;
  ICU4XLocaleCanonicalizer(ICU4XLocaleCanonicalizer&&) noexcept = default;
  ICU4XLocaleCanonicalizer& operator=(ICU4XLocaleCanonicalizer&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLocaleCanonicalizer, ICU4XLocaleCanonicalizerDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"

inline diplomat::result<ICU4XLocaleCanonicalizer, ICU4XError> ICU4XLocaleCanonicalizer::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleCanonicalizer_create(provider.AsFFI());
  diplomat::result<ICU4XLocaleCanonicalizer, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleCanonicalizer>(ICU4XLocaleCanonicalizer(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XLocaleCanonicalizer, ICU4XError> ICU4XLocaleCanonicalizer::create_extended(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleCanonicalizer_create_extended(provider.AsFFI());
  diplomat::result<ICU4XLocaleCanonicalizer, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleCanonicalizer>(ICU4XLocaleCanonicalizer(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XTransformResult ICU4XLocaleCanonicalizer::canonicalize(ICU4XLocale& locale) const {
  return static_cast<ICU4XTransformResult>(capi::ICU4XLocaleCanonicalizer_canonicalize(this->inner.get(), locale.AsFFIMut()));
}
#endif
