#ifndef ICU4XFixedDecimalFormatter_HPP
#define ICU4XFixedDecimalFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XFixedDecimalFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
#include "ICU4XFixedDecimalGroupingStrategy.hpp"
class ICU4XFixedDecimalFormatter;
#include "ICU4XError.hpp"
class ICU4XDataStruct;
class ICU4XFixedDecimal;

/**
 * A destruction policy for using ICU4XFixedDecimalFormatter with std::unique_ptr.
 */
struct ICU4XFixedDecimalFormatterDeleter {
  void operator()(capi::ICU4XFixedDecimalFormatter* l) const noexcept {
    capi::ICU4XFixedDecimalFormatter_destroy(l);
  }
};

/**
 * An ICU4X Fixed Decimal Format object, capable of formatting a [`ICU4XFixedDecimal`] as a string.
 * 
 * See the [Rust documentation for `FixedDecimalFormatter`](https://docs.rs/icu/latest/icu/decimal/struct.FixedDecimalFormatter.html) for more information.
 */
class ICU4XFixedDecimalFormatter {
 public:

  /**
   * Creates a new [`ICU4XFixedDecimalFormatter`] from locale data.
   * 
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/latest/icu/decimal/struct.FixedDecimalFormatter.html#method.try_new) for more information.
   */
  static diplomat::result<ICU4XFixedDecimalFormatter, ICU4XError> create_with_grouping_strategy(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XFixedDecimalGroupingStrategy grouping_strategy);

  /**
   * Creates a new [`ICU4XFixedDecimalFormatter`] from preconstructed locale data in the form of an [`ICU4XDataStruct`]
   * constructed from `ICU4XDataStruct::create_decimal_symbols()`.
   * 
   * The contents of the data struct will be consumed: if you wish to use the struct again it will have to be reconstructed.
   * Passing a consumed struct to this method will return an error.
   */
  static diplomat::result<ICU4XFixedDecimalFormatter, ICU4XError> create_with_decimal_symbols_v1(const ICU4XDataStruct& data_struct, ICU4XFixedDecimalGroupingStrategy grouping_strategy);

  /**
   * Formats a [`ICU4XFixedDecimal`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/decimal/struct.FixedDecimalFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_to_writeable(const ICU4XFixedDecimal& value, W& write) const;

  /**
   * Formats a [`ICU4XFixedDecimal`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/decimal/struct.FixedDecimalFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format(const ICU4XFixedDecimal& value) const;
  inline const capi::ICU4XFixedDecimalFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XFixedDecimalFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XFixedDecimalFormatter(capi::ICU4XFixedDecimalFormatter* i) : inner(i) {}
  ICU4XFixedDecimalFormatter() = default;
  ICU4XFixedDecimalFormatter(ICU4XFixedDecimalFormatter&&) noexcept = default;
  ICU4XFixedDecimalFormatter& operator=(ICU4XFixedDecimalFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XFixedDecimalFormatter, ICU4XFixedDecimalFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XDataStruct.hpp"
#include "ICU4XFixedDecimal.hpp"

inline diplomat::result<ICU4XFixedDecimalFormatter, ICU4XError> ICU4XFixedDecimalFormatter::create_with_grouping_strategy(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XFixedDecimalGroupingStrategy grouping_strategy) {
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimalFormatter_create_with_grouping_strategy(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XFixedDecimalGroupingStrategy>(grouping_strategy));
  diplomat::result<ICU4XFixedDecimalFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XFixedDecimalFormatter>(ICU4XFixedDecimalFormatter(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XFixedDecimalFormatter, ICU4XError> ICU4XFixedDecimalFormatter::create_with_decimal_symbols_v1(const ICU4XDataStruct& data_struct, ICU4XFixedDecimalGroupingStrategy grouping_strategy) {
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimalFormatter_create_with_decimal_symbols_v1(data_struct.AsFFI(), static_cast<capi::ICU4XFixedDecimalGroupingStrategy>(grouping_strategy));
  diplomat::result<ICU4XFixedDecimalFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XFixedDecimalFormatter>(ICU4XFixedDecimalFormatter(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XFixedDecimalFormatter::format_to_writeable(const ICU4XFixedDecimal& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimalFormatter_format(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XFixedDecimalFormatter::format(const ICU4XFixedDecimal& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimalFormatter_format(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
