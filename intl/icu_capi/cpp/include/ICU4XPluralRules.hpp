#ifndef ICU4XPluralRules_HPP
#define ICU4XPluralRules_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XPluralRules.h"

class ICU4XDataProvider;
class ICU4XLocale;
class ICU4XPluralRules;
#include "ICU4XError.hpp"
class ICU4XPluralOperands;
#include "ICU4XPluralCategory.hpp"
struct ICU4XPluralCategories;

/**
 * A destruction policy for using ICU4XPluralRules with std::unique_ptr.
 */
struct ICU4XPluralRulesDeleter {
  void operator()(capi::ICU4XPluralRules* l) const noexcept {
    capi::ICU4XPluralRules_destroy(l);
  }
};

/**
 * FFI version of `PluralRules`.
 * 
 * See the [Rust documentation for `PluralRules`](https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html) for more information.
 */
class ICU4XPluralRules {
 public:

  /**
   * Construct an [`ICU4XPluralRules`] for the given locale, for cardinal numbers
   * 
   * See the [Rust documentation for `try_new_cardinal`](https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.try_new_cardinal) for more information.
   */
  static diplomat::result<ICU4XPluralRules, ICU4XError> create_cardinal(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * Construct an [`ICU4XPluralRules`] for the given locale, for ordinal numbers
   * 
   * See the [Rust documentation for `try_new_ordinal`](https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.try_new_ordinal) for more information.
   */
  static diplomat::result<ICU4XPluralRules, ICU4XError> create_ordinal(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * Get the category for a given number represented as operands
   * 
   * See the [Rust documentation for `category_for`](https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.category_for) for more information.
   */
  ICU4XPluralCategory category_for(const ICU4XPluralOperands& op) const;

  /**
   * Get all of the categories needed in the current locale
   * 
   * See the [Rust documentation for `categories`](https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.categories) for more information.
   */
  ICU4XPluralCategories categories() const;
  inline const capi::ICU4XPluralRules* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XPluralRules* AsFFIMut() { return this->inner.get(); }
  inline ICU4XPluralRules(capi::ICU4XPluralRules* i) : inner(i) {}
  ICU4XPluralRules() = default;
  ICU4XPluralRules(ICU4XPluralRules&&) noexcept = default;
  ICU4XPluralRules& operator=(ICU4XPluralRules&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XPluralRules, ICU4XPluralRulesDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XPluralOperands.hpp"
#include "ICU4XPluralCategories.hpp"

inline diplomat::result<ICU4XPluralRules, ICU4XError> ICU4XPluralRules::create_cardinal(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XPluralRules_create_cardinal(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XPluralRules, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPluralRules>(ICU4XPluralRules(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XPluralRules, ICU4XError> ICU4XPluralRules::create_ordinal(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XPluralRules_create_ordinal(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XPluralRules, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPluralRules>(ICU4XPluralRules(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XPluralCategory ICU4XPluralRules::category_for(const ICU4XPluralOperands& op) const {
  return static_cast<ICU4XPluralCategory>(capi::ICU4XPluralRules_category_for(this->inner.get(), op.AsFFI()));
}
inline ICU4XPluralCategories ICU4XPluralRules::categories() const {
  capi::ICU4XPluralCategories diplomat_raw_struct_out_value = capi::ICU4XPluralRules_categories(this->inner.get());
  return ICU4XPluralCategories{ .zero = std::move(diplomat_raw_struct_out_value.zero), .one = std::move(diplomat_raw_struct_out_value.one), .two = std::move(diplomat_raw_struct_out_value.two), .few = std::move(diplomat_raw_struct_out_value.few), .many = std::move(diplomat_raw_struct_out_value.many), .other = std::move(diplomat_raw_struct_out_value.other) };
}
#endif
