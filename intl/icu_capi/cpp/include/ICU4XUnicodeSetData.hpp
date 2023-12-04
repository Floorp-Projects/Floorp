#ifndef ICU4XUnicodeSetData_HPP
#define ICU4XUnicodeSetData_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XUnicodeSetData.h"

class ICU4XDataProvider;
class ICU4XUnicodeSetData;
#include "ICU4XError.hpp"
class ICU4XLocale;

/**
 * A destruction policy for using ICU4XUnicodeSetData with std::unique_ptr.
 */
struct ICU4XUnicodeSetDataDeleter {
  void operator()(capi::ICU4XUnicodeSetData* l) const noexcept {
    capi::ICU4XUnicodeSetData_destroy(l);
  }
};

/**
 * An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
 * 
 * See the [Rust documentation for `properties`](https://docs.rs/icu/latest/icu/properties/index.html) for more information.
 * 
 * See the [Rust documentation for `UnicodeSetData`](https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetData.html) for more information.
 * 
 * See the [Rust documentation for `UnicodeSetDataBorrowed`](https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html) for more information.
 */
class ICU4XUnicodeSetData {
 public:

  /**
   * Checks whether the string is in the set.
   * 
   * See the [Rust documentation for `contains`](https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html#method.contains) for more information.
   */
  bool contains(const std::string_view s) const;

  /**
   * Checks whether the code point is in the set.
   * 
   * See the [Rust documentation for `contains_char`](https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html#method.contains_char) for more information.
   */
  bool contains_char(char32_t cp) const;

  /**
   * Checks whether the code point (specified as a 32 bit integer, in UTF-32) is in the set.
   */
  bool contains32(uint32_t cp) const;

  /**
   * See the [Rust documentation for `basic_emoji`](https://docs.rs/icu/latest/icu/properties/sets/fn.basic_emoji.html) for more information.
   */
  static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_basic_emoji(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `exemplars_main`](https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_main.html) for more information.
   */
  static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_main(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * See the [Rust documentation for `exemplars_auxiliary`](https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_auxiliary.html) for more information.
   */
  static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_auxiliary(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * See the [Rust documentation for `exemplars_punctuation`](https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_punctuation.html) for more information.
   */
  static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_punctuation(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * See the [Rust documentation for `exemplars_numbers`](https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_numbers.html) for more information.
   */
  static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_numbers(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * See the [Rust documentation for `exemplars_index`](https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_index.html) for more information.
   */
  static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_index(const ICU4XDataProvider& provider, const ICU4XLocale& locale);
  inline const capi::ICU4XUnicodeSetData* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XUnicodeSetData* AsFFIMut() { return this->inner.get(); }
  inline ICU4XUnicodeSetData(capi::ICU4XUnicodeSetData* i) : inner(i) {}
  ICU4XUnicodeSetData() = default;
  ICU4XUnicodeSetData(ICU4XUnicodeSetData&&) noexcept = default;
  ICU4XUnicodeSetData& operator=(ICU4XUnicodeSetData&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XUnicodeSetData, ICU4XUnicodeSetDataDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"

inline bool ICU4XUnicodeSetData::contains(const std::string_view s) const {
  return capi::ICU4XUnicodeSetData_contains(this->inner.get(), s.data(), s.size());
}
inline bool ICU4XUnicodeSetData::contains_char(char32_t cp) const {
  return capi::ICU4XUnicodeSetData_contains_char(this->inner.get(), cp);
}
inline bool ICU4XUnicodeSetData::contains32(uint32_t cp) const {
  return capi::ICU4XUnicodeSetData_contains32(this->inner.get(), cp);
}
inline diplomat::result<ICU4XUnicodeSetData, ICU4XError> ICU4XUnicodeSetData::load_basic_emoji(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XUnicodeSetData_load_basic_emoji(provider.AsFFI());
  diplomat::result<ICU4XUnicodeSetData, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XUnicodeSetData>(ICU4XUnicodeSetData(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XUnicodeSetData, ICU4XError> ICU4XUnicodeSetData::load_exemplars_main(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XUnicodeSetData_load_exemplars_main(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XUnicodeSetData, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XUnicodeSetData>(ICU4XUnicodeSetData(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XUnicodeSetData, ICU4XError> ICU4XUnicodeSetData::load_exemplars_auxiliary(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XUnicodeSetData_load_exemplars_auxiliary(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XUnicodeSetData, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XUnicodeSetData>(ICU4XUnicodeSetData(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XUnicodeSetData, ICU4XError> ICU4XUnicodeSetData::load_exemplars_punctuation(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XUnicodeSetData_load_exemplars_punctuation(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XUnicodeSetData, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XUnicodeSetData>(ICU4XUnicodeSetData(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XUnicodeSetData, ICU4XError> ICU4XUnicodeSetData::load_exemplars_numbers(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XUnicodeSetData_load_exemplars_numbers(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XUnicodeSetData, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XUnicodeSetData>(ICU4XUnicodeSetData(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XUnicodeSetData, ICU4XError> ICU4XUnicodeSetData::load_exemplars_index(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XUnicodeSetData_load_exemplars_index(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XUnicodeSetData, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XUnicodeSetData>(ICU4XUnicodeSetData(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
#endif
