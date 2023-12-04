#ifndef ICU4XDateTimeFormatter_HPP
#define ICU4XDateTimeFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDateTimeFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
#include "ICU4XDateLength.hpp"
#include "ICU4XTimeLength.hpp"
class ICU4XDateTimeFormatter;
#include "ICU4XError.hpp"
class ICU4XDateTime;
class ICU4XIsoDateTime;

/**
 * A destruction policy for using ICU4XDateTimeFormatter with std::unique_ptr.
 */
struct ICU4XDateTimeFormatterDeleter {
  void operator()(capi::ICU4XDateTimeFormatter* l) const noexcept {
    capi::ICU4XDateTimeFormatter_destroy(l);
  }
};

/**
 * An ICU4X DateFormatter object capable of formatting a [`ICU4XDateTime`] as a string,
 * using some calendar specified at runtime in the locale.
 * 
 * See the [Rust documentation for `DateTimeFormatter`](https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html) for more information.
 */
class ICU4XDateTimeFormatter {
 public:

  /**
   * Creates a new [`ICU4XDateTimeFormatter`] from locale data.
   * 
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.try_new) for more information.
   */
  static diplomat::result<ICU4XDateTimeFormatter, ICU4XError> create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length);

  /**
   * Formats a [`ICU4XDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const;

  /**
   * Formats a [`ICU4XDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_datetime(const ICU4XDateTime& value) const;

  /**
   * Formats a [`ICU4XIsoDateTime`] to a string.
   * 
   * Will convert to this formatter's calendar first
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const;

  /**
   * Formats a [`ICU4XIsoDateTime`] to a string.
   * 
   * Will convert to this formatter's calendar first
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const;
  inline const capi::ICU4XDateTimeFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XDateTimeFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XDateTimeFormatter(capi::ICU4XDateTimeFormatter* i) : inner(i) {}
  ICU4XDateTimeFormatter() = default;
  ICU4XDateTimeFormatter(ICU4XDateTimeFormatter&&) noexcept = default;
  ICU4XDateTimeFormatter& operator=(ICU4XDateTimeFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XDateTimeFormatter, ICU4XDateTimeFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XDateTime.hpp"
#include "ICU4XIsoDateTime.hpp"

inline diplomat::result<ICU4XDateTimeFormatter, ICU4XError> ICU4XDateTimeFormatter::create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length) {
  auto diplomat_result_raw_out_value = capi::ICU4XDateTimeFormatter_create_with_lengths(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XDateLength>(date_length), static_cast<capi::ICU4XTimeLength>(time_length));
  diplomat::result<ICU4XDateTimeFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDateTimeFormatter>(ICU4XDateTimeFormatter(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDateTimeFormatter::format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDateTimeFormatter_format_datetime(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDateTimeFormatter::format_datetime(const ICU4XDateTime& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDateTimeFormatter_format_datetime(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDateTimeFormatter::format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDateTimeFormatter_format_iso_datetime(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDateTimeFormatter::format_iso_datetime(const ICU4XIsoDateTime& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDateTimeFormatter_format_iso_datetime(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
