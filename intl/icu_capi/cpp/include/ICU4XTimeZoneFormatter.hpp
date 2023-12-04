#ifndef ICU4XTimeZoneFormatter_HPP
#define ICU4XTimeZoneFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTimeZoneFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
class ICU4XTimeZoneFormatter;
#include "ICU4XError.hpp"
struct ICU4XIsoTimeZoneOptions;
class ICU4XCustomTimeZone;

/**
 * A destruction policy for using ICU4XTimeZoneFormatter with std::unique_ptr.
 */
struct ICU4XTimeZoneFormatterDeleter {
  void operator()(capi::ICU4XTimeZoneFormatter* l) const noexcept {
    capi::ICU4XTimeZoneFormatter_destroy(l);
  }
};

/**
 * An ICU4X TimeZoneFormatter object capable of formatting an [`ICU4XCustomTimeZone`] type (and others) as a string
 * 
 * See the [Rust documentation for `TimeZoneFormatter`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html) for more information.
 */
class ICU4XTimeZoneFormatter {
 public:

  /**
   * Creates a new [`ICU4XTimeZoneFormatter`] from locale data.
   * 
   * Uses localized GMT as the fallback format.
   * 
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.try_new) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/datetime/time_zone/enum.FallbackFormat.html)
   */
  static diplomat::result<ICU4XTimeZoneFormatter, ICU4XError> create_with_localized_gmt_fallback(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * Creates a new [`ICU4XTimeZoneFormatter`] from locale data.
   * 
   * Uses ISO-8601 as the fallback format.
   * 
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.try_new) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/datetime/time_zone/enum.FallbackFormat.html)
   */
  static diplomat::result<ICU4XTimeZoneFormatter, ICU4XError> create_with_iso_8601_fallback(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XIsoTimeZoneOptions options);

  /**
   * Loads generic non-location long format. Example: "Pacific Time"
   * 
   * See the [Rust documentation for `include_generic_non_location_long`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_non_location_long) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> load_generic_non_location_long(const ICU4XDataProvider& provider);

  /**
   * Loads generic non-location short format. Example: "PT"
   * 
   * See the [Rust documentation for `include_generic_non_location_short`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_non_location_short) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> load_generic_non_location_short(const ICU4XDataProvider& provider);

  /**
   * Loads specific non-location long format. Example: "Pacific Standard Time"
   * 
   * See the [Rust documentation for `include_specific_non_location_long`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_specific_non_location_long) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> load_specific_non_location_long(const ICU4XDataProvider& provider);

  /**
   * Loads specific non-location short format. Example: "PST"
   * 
   * See the [Rust documentation for `include_specific_non_location_short`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_specific_non_location_short) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> load_specific_non_location_short(const ICU4XDataProvider& provider);

  /**
   * Loads generic location format. Example: "Los Angeles Time"
   * 
   * See the [Rust documentation for `include_generic_location_format`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_location_format) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> load_generic_location_format(const ICU4XDataProvider& provider);

  /**
   * Loads localized GMT format. Example: "GMT-07:00"
   * 
   * See the [Rust documentation for `include_localized_gmt_format`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_localized_gmt_format) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> include_localized_gmt_format();

  /**
   * Loads ISO-8601 format. Example: "-07:00"
   * 
   * See the [Rust documentation for `include_iso_8601_format`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_iso_8601_format) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> load_iso_8601_format(ICU4XIsoTimeZoneOptions options);

  /**
   * Formats a [`ICU4XCustomTimeZone`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.format) for more information.
   * 
   * See the [Rust documentation for `format_to_string`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.format_to_string) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_custom_time_zone_to_writeable(const ICU4XCustomTimeZone& value, W& write) const;

  /**
   * Formats a [`ICU4XCustomTimeZone`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.format) for more information.
   * 
   * See the [Rust documentation for `format_to_string`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.format_to_string) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_custom_time_zone(const ICU4XCustomTimeZone& value) const;
  inline const capi::ICU4XTimeZoneFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XTimeZoneFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XTimeZoneFormatter(capi::ICU4XTimeZoneFormatter* i) : inner(i) {}
  ICU4XTimeZoneFormatter() = default;
  ICU4XTimeZoneFormatter(ICU4XTimeZoneFormatter&&) noexcept = default;
  ICU4XTimeZoneFormatter& operator=(ICU4XTimeZoneFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XTimeZoneFormatter, ICU4XTimeZoneFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XIsoTimeZoneOptions.hpp"
#include "ICU4XCustomTimeZone.hpp"

inline diplomat::result<ICU4XTimeZoneFormatter, ICU4XError> ICU4XTimeZoneFormatter::create_with_localized_gmt_fallback(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_create_with_localized_gmt_fallback(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XTimeZoneFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XTimeZoneFormatter>(ICU4XTimeZoneFormatter(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XTimeZoneFormatter, ICU4XError> ICU4XTimeZoneFormatter::create_with_iso_8601_fallback(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XIsoTimeZoneOptions options) {
  ICU4XIsoTimeZoneOptions diplomat_wrapped_struct_options = options;
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_create_with_iso_8601_fallback(provider.AsFFI(), locale.AsFFI(), capi::ICU4XIsoTimeZoneOptions{ .format = static_cast<capi::ICU4XIsoTimeZoneFormat>(diplomat_wrapped_struct_options.format), .minutes = static_cast<capi::ICU4XIsoTimeZoneMinuteDisplay>(diplomat_wrapped_struct_options.minutes), .seconds = static_cast<capi::ICU4XIsoTimeZoneSecondDisplay>(diplomat_wrapped_struct_options.seconds) });
  diplomat::result<ICU4XTimeZoneFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XTimeZoneFormatter>(ICU4XTimeZoneFormatter(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneFormatter::load_generic_non_location_long(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_load_generic_non_location_long(this->inner.get(), provider.AsFFI());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneFormatter::load_generic_non_location_short(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_load_generic_non_location_short(this->inner.get(), provider.AsFFI());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneFormatter::load_specific_non_location_long(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_load_specific_non_location_long(this->inner.get(), provider.AsFFI());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneFormatter::load_specific_non_location_short(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_load_specific_non_location_short(this->inner.get(), provider.AsFFI());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneFormatter::load_generic_location_format(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_load_generic_location_format(this->inner.get(), provider.AsFFI());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneFormatter::include_localized_gmt_format() {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_include_localized_gmt_format(this->inner.get());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneFormatter::load_iso_8601_format(ICU4XIsoTimeZoneOptions options) {
  ICU4XIsoTimeZoneOptions diplomat_wrapped_struct_options = options;
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_load_iso_8601_format(this->inner.get(), capi::ICU4XIsoTimeZoneOptions{ .format = static_cast<capi::ICU4XIsoTimeZoneFormat>(diplomat_wrapped_struct_options.format), .minutes = static_cast<capi::ICU4XIsoTimeZoneMinuteDisplay>(diplomat_wrapped_struct_options.minutes), .seconds = static_cast<capi::ICU4XIsoTimeZoneSecondDisplay>(diplomat_wrapped_struct_options.seconds) });
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneFormatter::format_custom_time_zone_to_writeable(const ICU4XCustomTimeZone& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_format_custom_time_zone(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeZoneFormatter::format_custom_time_zone(const ICU4XCustomTimeZone& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneFormatter_format_custom_time_zone(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
