#ifndef ICU4XDateFormatter_HPP
#define ICU4XDateFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDateFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
#include "ICU4XDateLength.hpp"
class ICU4XDateFormatter;
#include "ICU4XError.hpp"
class ICU4XDate;
class ICU4XIsoDate;
class ICU4XDateTime;
class ICU4XIsoDateTime;

/**
 * A destruction policy for using ICU4XDateFormatter with std::unique_ptr.
 */
struct ICU4XDateFormatterDeleter {
  void operator()(capi::ICU4XDateFormatter* l) const noexcept {
    capi::ICU4XDateFormatter_destroy(l);
  }
};

/**
 * An ICU4X DateFormatter object capable of formatting a [`ICU4XDate`] as a string,
 * using some calendar specified at runtime in the locale.
 * 
 * See the [Rust documentation for `DateFormatter`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html) for more information.
 */
class ICU4XDateFormatter {
 public:

  /**
   * Creates a new [`ICU4XDateFormatter`] from locale data.
   * 
   * See the [Rust documentation for `try_new_with_length`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.try_new_with_length) for more information.
   */
  static diplomat::result<ICU4XDateFormatter, ICU4XError> create_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length);

  /**
   * Formats a [`ICU4XDate`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_date_to_writeable(const ICU4XDate& value, W& write) const;

  /**
   * Formats a [`ICU4XDate`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_date(const ICU4XDate& value) const;

  /**
   * Formats a [`ICU4XIsoDate`] to a string.
   * 
   * Will convert to this formatter's calendar first
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_date_to_writeable(const ICU4XIsoDate& value, W& write) const;

  /**
   * Formats a [`ICU4XIsoDate`] to a string.
   * 
   * Will convert to this formatter's calendar first
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_iso_date(const ICU4XIsoDate& value) const;

  /**
   * Formats a [`ICU4XDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const;

  /**
   * Formats a [`ICU4XDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_datetime(const ICU4XDateTime& value) const;

  /**
   * Formats a [`ICU4XIsoDateTime`] to a string.
   * 
   * Will convert to this formatter's calendar first
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const;

  /**
   * Formats a [`ICU4XIsoDateTime`] to a string.
   * 
   * Will convert to this formatter's calendar first
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const;
  inline const capi::ICU4XDateFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XDateFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XDateFormatter(capi::ICU4XDateFormatter* i) : inner(i) {}
  ICU4XDateFormatter() = default;
  ICU4XDateFormatter(ICU4XDateFormatter&&) noexcept = default;
  ICU4XDateFormatter& operator=(ICU4XDateFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XDateFormatter, ICU4XDateFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XDate.hpp"
#include "ICU4XIsoDate.hpp"
#include "ICU4XDateTime.hpp"
#include "ICU4XIsoDateTime.hpp"

inline diplomat::result<ICU4XDateFormatter, ICU4XError> ICU4XDateFormatter::create_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length) {
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_create_with_length(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XDateLength>(date_length));
  diplomat::result<ICU4XDateFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDateFormatter>(ICU4XDateFormatter(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDateFormatter::format_date_to_writeable(const ICU4XDate& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_format_date(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDateFormatter::format_date(const ICU4XDate& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_format_date(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDateFormatter::format_iso_date_to_writeable(const ICU4XIsoDate& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_format_iso_date(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDateFormatter::format_iso_date(const ICU4XIsoDate& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_format_iso_date(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDateFormatter::format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_format_datetime(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDateFormatter::format_datetime(const ICU4XDateTime& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_format_datetime(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDateFormatter::format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_format_iso_datetime(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDateFormatter::format_iso_datetime(const ICU4XIsoDateTime& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDateFormatter_format_iso_datetime(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
