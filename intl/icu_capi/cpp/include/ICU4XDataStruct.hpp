#ifndef ICU4XDataStruct_HPP
#define ICU4XDataStruct_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDataStruct.h"

class ICU4XDataStruct;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XDataStruct with std::unique_ptr.
 */
struct ICU4XDataStructDeleter {
  void operator()(capi::ICU4XDataStruct* l) const noexcept {
    capi::ICU4XDataStruct_destroy(l);
  }
};

/**
 * A generic data struct to be used by ICU4X
 * 
 * This can be used to construct a StructDataProvider.
 */
class ICU4XDataStruct {
 public:

  /**
   * Construct a new DecimalSymbolsV1 data struct.
   * 
   * C++ users: All string arguments must be valid UTF8
   * 
   * See the [Rust documentation for `DecimalSymbolsV1`](https://docs.rs/icu/latest/icu/decimal/provider/struct.DecimalSymbolsV1.html) for more information.
   */
  static diplomat::result<ICU4XDataStruct, ICU4XError> create_decimal_symbols_v1(const std::string_view plus_sign_prefix, const std::string_view plus_sign_suffix, const std::string_view minus_sign_prefix, const std::string_view minus_sign_suffix, const std::string_view decimal_separator, const std::string_view grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, const diplomat::span<const char32_t> digits);
  inline const capi::ICU4XDataStruct* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XDataStruct* AsFFIMut() { return this->inner.get(); }
  inline ICU4XDataStruct(capi::ICU4XDataStruct* i) : inner(i) {}
  ICU4XDataStruct() = default;
  ICU4XDataStruct(ICU4XDataStruct&&) noexcept = default;
  ICU4XDataStruct& operator=(ICU4XDataStruct&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XDataStruct, ICU4XDataStructDeleter> inner;
};


inline diplomat::result<ICU4XDataStruct, ICU4XError> ICU4XDataStruct::create_decimal_symbols_v1(const std::string_view plus_sign_prefix, const std::string_view plus_sign_suffix, const std::string_view minus_sign_prefix, const std::string_view minus_sign_suffix, const std::string_view decimal_separator, const std::string_view grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, const diplomat::span<const char32_t> digits) {
  auto diplomat_result_raw_out_value = capi::ICU4XDataStruct_create_decimal_symbols_v1(plus_sign_prefix.data(), plus_sign_prefix.size(), plus_sign_suffix.data(), plus_sign_suffix.size(), minus_sign_prefix.data(), minus_sign_prefix.size(), minus_sign_suffix.data(), minus_sign_suffix.size(), decimal_separator.data(), decimal_separator.size(), grouping_separator.data(), grouping_separator.size(), primary_group_size, secondary_group_size, min_group_size, digits.data(), digits.size());
  diplomat::result<ICU4XDataStruct, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDataStruct>(ICU4XDataStruct(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
#endif
