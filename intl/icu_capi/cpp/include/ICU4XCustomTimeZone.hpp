#ifndef ICU4XCustomTimeZone_HPP
#define ICU4XCustomTimeZone_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCustomTimeZone.h"

class ICU4XCustomTimeZone;
#include "ICU4XError.hpp"
class ICU4XIanaToBcp47Mapper;
class ICU4XMetazoneCalculator;
class ICU4XIsoDateTime;

/**
 * A destruction policy for using ICU4XCustomTimeZone with std::unique_ptr.
 */
struct ICU4XCustomTimeZoneDeleter {
  void operator()(capi::ICU4XCustomTimeZone* l) const noexcept {
    capi::ICU4XCustomTimeZone_destroy(l);
  }
};

/**
 * See the [Rust documentation for `CustomTimeZone`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html) for more information.
 */
class ICU4XCustomTimeZone {
 public:

  /**
   * Creates a time zone from an offset string.
   * 
   * See the [Rust documentation for `from_str`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#method.from_str) for more information.
   */
  static diplomat::result<ICU4XCustomTimeZone, ICU4XError> create_from_string(const std::string_view s);

  /**
   * Creates a time zone with no information.
   * 
   * See the [Rust documentation for `new_empty`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#method.new_empty) for more information.
   */
  static ICU4XCustomTimeZone create_empty();

  /**
   * Creates a time zone for UTC.
   * 
   * See the [Rust documentation for `utc`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#method.utc) for more information.
   */
  static ICU4XCustomTimeZone create_utc();

  /**
   * Sets the `gmt_offset` field from offset seconds.
   * 
   * Errors if the offset seconds are out of range.
   * 
   * See the [Rust documentation for `try_from_offset_seconds`](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.try_from_offset_seconds) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html)
   */
  diplomat::result<std::monostate, ICU4XError> try_set_gmt_offset_seconds(int32_t offset_seconds);

  /**
   * Clears the `gmt_offset` field.
   * 
   * See the [Rust documentation for `offset_seconds`](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.offset_seconds) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html)
   */
  void clear_gmt_offset();

  /**
   * Returns the value of the `gmt_offset` field as offset seconds.
   * 
   * Errors if the `gmt_offset` field is empty.
   * 
   * See the [Rust documentation for `offset_seconds`](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.offset_seconds) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html)
   */
  diplomat::result<int32_t, ICU4XError> gmt_offset_seconds() const;

  /**
   * Returns whether the `gmt_offset` field is positive.
   * 
   * Errors if the `gmt_offset` field is empty.
   * 
   * See the [Rust documentation for `is_positive`](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.is_positive) for more information.
   */
  diplomat::result<bool, ICU4XError> is_gmt_offset_positive() const;

  /**
   * Returns whether the `gmt_offset` field is zero.
   * 
   * Errors if the `gmt_offset` field is empty (which is not the same as zero).
   * 
   * See the [Rust documentation for `is_zero`](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.is_zero) for more information.
   */
  diplomat::result<bool, ICU4XError> is_gmt_offset_zero() const;

  /**
   * Returns whether the `gmt_offset` field has nonzero minutes.
   * 
   * Errors if the `gmt_offset` field is empty.
   * 
   * See the [Rust documentation for `has_minutes`](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.has_minutes) for more information.
   */
  diplomat::result<bool, ICU4XError> gmt_offset_has_minutes() const;

  /**
   * Returns whether the `gmt_offset` field has nonzero seconds.
   * 
   * Errors if the `gmt_offset` field is empty.
   * 
   * See the [Rust documentation for `has_seconds`](https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.has_seconds) for more information.
   */
  diplomat::result<bool, ICU4XError> gmt_offset_has_seconds() const;

  /**
   * Sets the `time_zone_id` field from a BCP-47 string.
   * 
   * Errors if the string is not a valid BCP-47 time zone ID.
   * 
   * See the [Rust documentation for `time_zone_id`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.time_zone_id) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneBcp47Id.html)
   */
  diplomat::result<std::monostate, ICU4XError> try_set_time_zone_id(const std::string_view id);

  /**
   * Sets the `time_zone_id` field from an IANA string by looking up
   * the corresponding BCP-47 string.
   * 
   * Errors if the string is not a valid BCP-47 time zone ID.
   * 
   * See the [Rust documentation for `get`](https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47MapperBorrowed.html#method.get) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> try_set_iana_time_zone_id(const ICU4XIanaToBcp47Mapper& mapper, const std::string_view id);

  /**
   * Clears the `time_zone_id` field.
   * 
   * See the [Rust documentation for `time_zone_id`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.time_zone_id) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneBcp47Id.html)
   */
  void clear_time_zone_id();

  /**
   * Writes the value of the `time_zone_id` field as a string.
   * 
   * Errors if the `time_zone_id` field is empty.
   * 
   * See the [Rust documentation for `time_zone_id`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.time_zone_id) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneBcp47Id.html)
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> time_zone_id_to_writeable(W& write) const;

  /**
   * Writes the value of the `time_zone_id` field as a string.
   * 
   * Errors if the `time_zone_id` field is empty.
   * 
   * See the [Rust documentation for `time_zone_id`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.time_zone_id) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneBcp47Id.html)
   */
  diplomat::result<std::string, ICU4XError> time_zone_id() const;

  /**
   * Sets the `metazone_id` field from a string.
   * 
   * Errors if the string is not a valid BCP-47 metazone ID.
   * 
   * See the [Rust documentation for `metazone_id`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.metazone_id) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.MetazoneId.html)
   */
  diplomat::result<std::monostate, ICU4XError> try_set_metazone_id(const std::string_view id);

  /**
   * Clears the `metazone_id` field.
   * 
   * See the [Rust documentation for `metazone_id`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.metazone_id) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.MetazoneId.html)
   */
  void clear_metazone_id();

  /**
   * Writes the value of the `metazone_id` field as a string.
   * 
   * Errors if the `metazone_id` field is empty.
   * 
   * See the [Rust documentation for `metazone_id`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.metazone_id) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.MetazoneId.html)
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> metazone_id_to_writeable(W& write) const;

  /**
   * Writes the value of the `metazone_id` field as a string.
   * 
   * Errors if the `metazone_id` field is empty.
   * 
   * See the [Rust documentation for `metazone_id`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.metazone_id) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.MetazoneId.html)
   */
  diplomat::result<std::string, ICU4XError> metazone_id() const;

  /**
   * Sets the `zone_variant` field from a string.
   * 
   * Errors if the string is not a valid zone variant.
   * 
   * See the [Rust documentation for `zone_variant`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html)
   */
  diplomat::result<std::monostate, ICU4XError> try_set_zone_variant(const std::string_view id);

  /**
   * Clears the `zone_variant` field.
   * 
   * See the [Rust documentation for `zone_variant`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html)
   */
  void clear_zone_variant();

  /**
   * Writes the value of the `zone_variant` field as a string.
   * 
   * Errors if the `zone_variant` field is empty.
   * 
   * See the [Rust documentation for `zone_variant`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html)
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> zone_variant_to_writeable(W& write) const;

  /**
   * Writes the value of the `zone_variant` field as a string.
   * 
   * Errors if the `zone_variant` field is empty.
   * 
   * See the [Rust documentation for `zone_variant`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html)
   */
  diplomat::result<std::string, ICU4XError> zone_variant() const;

  /**
   * Sets the `zone_variant` field to standard time.
   * 
   * See the [Rust documentation for `standard`](https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html#method.standard) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant)
   */
  void set_standard_time();

  /**
   * Sets the `zone_variant` field to daylight time.
   * 
   * See the [Rust documentation for `daylight`](https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html#method.daylight) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant)
   */
  void set_daylight_time();

  /**
   * Returns whether the `zone_variant` field is standard time.
   * 
   * Errors if the `zone_variant` field is empty.
   * 
   * See the [Rust documentation for `standard`](https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html#method.standard) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant)
   */
  diplomat::result<bool, ICU4XError> is_standard_time() const;

  /**
   * Returns whether the `zone_variant` field is daylight time.
   * 
   * Errors if the `zone_variant` field is empty.
   * 
   * See the [Rust documentation for `daylight`](https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html#method.daylight) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant)
   */
  diplomat::result<bool, ICU4XError> is_daylight_time() const;

  /**
   * Sets the metazone based on the time zone and the local timestamp.
   * 
   * See the [Rust documentation for `maybe_calculate_metazone`](https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#method.maybe_calculate_metazone) for more information.
   * 
   * Additional information: [1](https://docs.rs/icu/latest/icu/timezone/struct.MetazoneCalculator.html#method.compute_metazone_from_time_zone)
   */
  void maybe_calculate_metazone(const ICU4XMetazoneCalculator& metazone_calculator, const ICU4XIsoDateTime& local_datetime);
  inline const capi::ICU4XCustomTimeZone* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCustomTimeZone* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCustomTimeZone(capi::ICU4XCustomTimeZone* i) : inner(i) {}
  ICU4XCustomTimeZone() = default;
  ICU4XCustomTimeZone(ICU4XCustomTimeZone&&) noexcept = default;
  ICU4XCustomTimeZone& operator=(ICU4XCustomTimeZone&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCustomTimeZone, ICU4XCustomTimeZoneDeleter> inner;
};

#include "ICU4XIanaToBcp47Mapper.hpp"
#include "ICU4XMetazoneCalculator.hpp"
#include "ICU4XIsoDateTime.hpp"

inline diplomat::result<ICU4XCustomTimeZone, ICU4XError> ICU4XCustomTimeZone::create_from_string(const std::string_view s) {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_create_from_string(s.data(), s.size());
  diplomat::result<ICU4XCustomTimeZone, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCustomTimeZone>(ICU4XCustomTimeZone(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XCustomTimeZone ICU4XCustomTimeZone::create_empty() {
  return ICU4XCustomTimeZone(capi::ICU4XCustomTimeZone_create_empty());
}
inline ICU4XCustomTimeZone ICU4XCustomTimeZone::create_utc() {
  return ICU4XCustomTimeZone(capi::ICU4XCustomTimeZone_create_utc());
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XCustomTimeZone::try_set_gmt_offset_seconds(int32_t offset_seconds) {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_try_set_gmt_offset_seconds(this->inner.get(), offset_seconds);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline void ICU4XCustomTimeZone::clear_gmt_offset() {
  capi::ICU4XCustomTimeZone_clear_gmt_offset(this->inner.get());
}
inline diplomat::result<int32_t, ICU4XError> ICU4XCustomTimeZone::gmt_offset_seconds() const {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_gmt_offset_seconds(this->inner.get());
  diplomat::result<int32_t, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<int32_t>(diplomat_result_raw_out_value.ok);
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<bool, ICU4XError> ICU4XCustomTimeZone::is_gmt_offset_positive() const {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_is_gmt_offset_positive(this->inner.get());
  diplomat::result<bool, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<bool>(diplomat_result_raw_out_value.ok);
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<bool, ICU4XError> ICU4XCustomTimeZone::is_gmt_offset_zero() const {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_is_gmt_offset_zero(this->inner.get());
  diplomat::result<bool, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<bool>(diplomat_result_raw_out_value.ok);
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<bool, ICU4XError> ICU4XCustomTimeZone::gmt_offset_has_minutes() const {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_gmt_offset_has_minutes(this->inner.get());
  diplomat::result<bool, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<bool>(diplomat_result_raw_out_value.ok);
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<bool, ICU4XError> ICU4XCustomTimeZone::gmt_offset_has_seconds() const {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_gmt_offset_has_seconds(this->inner.get());
  diplomat::result<bool, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<bool>(diplomat_result_raw_out_value.ok);
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XCustomTimeZone::try_set_time_zone_id(const std::string_view id) {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_try_set_time_zone_id(this->inner.get(), id.data(), id.size());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XCustomTimeZone::try_set_iana_time_zone_id(const ICU4XIanaToBcp47Mapper& mapper, const std::string_view id) {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_try_set_iana_time_zone_id(this->inner.get(), mapper.AsFFI(), id.data(), id.size());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline void ICU4XCustomTimeZone::clear_time_zone_id() {
  capi::ICU4XCustomTimeZone_clear_time_zone_id(this->inner.get());
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XCustomTimeZone::time_zone_id_to_writeable(W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_time_zone_id(this->inner.get(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XCustomTimeZone::time_zone_id() const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_time_zone_id(this->inner.get(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XCustomTimeZone::try_set_metazone_id(const std::string_view id) {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_try_set_metazone_id(this->inner.get(), id.data(), id.size());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline void ICU4XCustomTimeZone::clear_metazone_id() {
  capi::ICU4XCustomTimeZone_clear_metazone_id(this->inner.get());
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XCustomTimeZone::metazone_id_to_writeable(W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_metazone_id(this->inner.get(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XCustomTimeZone::metazone_id() const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_metazone_id(this->inner.get(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XCustomTimeZone::try_set_zone_variant(const std::string_view id) {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_try_set_zone_variant(this->inner.get(), id.data(), id.size());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline void ICU4XCustomTimeZone::clear_zone_variant() {
  capi::ICU4XCustomTimeZone_clear_zone_variant(this->inner.get());
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XCustomTimeZone::zone_variant_to_writeable(W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_zone_variant(this->inner.get(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XCustomTimeZone::zone_variant() const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_zone_variant(this->inner.get(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline void ICU4XCustomTimeZone::set_standard_time() {
  capi::ICU4XCustomTimeZone_set_standard_time(this->inner.get());
}
inline void ICU4XCustomTimeZone::set_daylight_time() {
  capi::ICU4XCustomTimeZone_set_daylight_time(this->inner.get());
}
inline diplomat::result<bool, ICU4XError> ICU4XCustomTimeZone::is_standard_time() const {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_is_standard_time(this->inner.get());
  diplomat::result<bool, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<bool>(diplomat_result_raw_out_value.ok);
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<bool, ICU4XError> ICU4XCustomTimeZone::is_daylight_time() const {
  auto diplomat_result_raw_out_value = capi::ICU4XCustomTimeZone_is_daylight_time(this->inner.get());
  diplomat::result<bool, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<bool>(diplomat_result_raw_out_value.ok);
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline void ICU4XCustomTimeZone::maybe_calculate_metazone(const ICU4XMetazoneCalculator& metazone_calculator, const ICU4XIsoDateTime& local_datetime) {
  capi::ICU4XCustomTimeZone_maybe_calculate_metazone(this->inner.get(), metazone_calculator.AsFFI(), local_datetime.AsFFI());
}
#endif
