#ifndef ICU4XFixedDecimal_HPP
#define ICU4XFixedDecimal_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XFixedDecimal.h"

class ICU4XFixedDecimal;
#include "ICU4XError.hpp"
#include "ICU4XFixedDecimalSign.hpp"
#include "ICU4XFixedDecimalSignDisplay.hpp"

/**
 * A destruction policy for using ICU4XFixedDecimal with std::unique_ptr.
 */
struct ICU4XFixedDecimalDeleter {
  void operator()(capi::ICU4XFixedDecimal* l) const noexcept {
    capi::ICU4XFixedDecimal_destroy(l);
  }
};

/**
 * See the [Rust documentation for `FixedDecimal`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html) for more information.
 */
class ICU4XFixedDecimal {
 public:

  /**
   * Construct an [`ICU4XFixedDecimal`] from an integer.
   * 
   * See the [Rust documentation for `FixedDecimal`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html) for more information.
   */
  static ICU4XFixedDecimal create_from_i32(int32_t v);

  /**
   * Construct an [`ICU4XFixedDecimal`] from an integer.
   * 
   * See the [Rust documentation for `FixedDecimal`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html) for more information.
   */
  static ICU4XFixedDecimal create_from_u32(uint32_t v);

  /**
   * Construct an [`ICU4XFixedDecimal`] from an integer.
   * 
   * See the [Rust documentation for `FixedDecimal`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html) for more information.
   */
  static ICU4XFixedDecimal create_from_i64(int64_t v);

  /**
   * Construct an [`ICU4XFixedDecimal`] from an integer.
   * 
   * See the [Rust documentation for `FixedDecimal`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html) for more information.
   */
  static ICU4XFixedDecimal create_from_u64(uint64_t v);

  /**
   * Construct an [`ICU4XFixedDecimal`] from an integer-valued float
   * 
   * See the [Rust documentation for `try_from_f64`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.try_from_f64) for more information.
   * 
   * See the [Rust documentation for `FloatPrecision`](https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FloatPrecision.html) for more information.
   */
  static diplomat::result<ICU4XFixedDecimal, ICU4XError> create_from_f64_with_integer_precision(double f);

  /**
   * Construct an [`ICU4XFixedDecimal`] from an float, with a given power of 10 for the lower magnitude
   * 
   * See the [Rust documentation for `try_from_f64`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.try_from_f64) for more information.
   * 
   * See the [Rust documentation for `FloatPrecision`](https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FloatPrecision.html) for more information.
   */
  static diplomat::result<ICU4XFixedDecimal, ICU4XError> create_from_f64_with_lower_magnitude(double f, int16_t magnitude);

  /**
   * Construct an [`ICU4XFixedDecimal`] from an float, for a given number of significant digits
   * 
   * See the [Rust documentation for `try_from_f64`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.try_from_f64) for more information.
   * 
   * See the [Rust documentation for `FloatPrecision`](https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FloatPrecision.html) for more information.
   */
  static diplomat::result<ICU4XFixedDecimal, ICU4XError> create_from_f64_with_significant_digits(double f, uint8_t digits);

  /**
   * Construct an [`ICU4XFixedDecimal`] from an float, with enough digits to recover
   * the original floating point in IEEE 754 without needing trailing zeros
   * 
   * See the [Rust documentation for `try_from_f64`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.try_from_f64) for more information.
   * 
   * See the [Rust documentation for `FloatPrecision`](https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FloatPrecision.html) for more information.
   */
  static diplomat::result<ICU4XFixedDecimal, ICU4XError> create_from_f64_with_floating_precision(double f);

  /**
   * Construct an [`ICU4XFixedDecimal`] from a string.
   * 
   * See the [Rust documentation for `from_str`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.from_str) for more information.
   */
  static diplomat::result<ICU4XFixedDecimal, ICU4XError> create_from_string(const std::string_view v);

  /**
   * See the [Rust documentation for `digit_at`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.digit_at) for more information.
   */
  uint8_t digit_at(int16_t magnitude) const;

  /**
   * See the [Rust documentation for `magnitude_range`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.magnitude_range) for more information.
   */
  int16_t magnitude_start() const;

  /**
   * See the [Rust documentation for `magnitude_range`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.magnitude_range) for more information.
   */
  int16_t magnitude_end() const;

  /**
   * See the [Rust documentation for `nonzero_magnitude_start`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.nonzero_magnitude_start) for more information.
   */
  int16_t nonzero_magnitude_start() const;

  /**
   * See the [Rust documentation for `nonzero_magnitude_end`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.nonzero_magnitude_end) for more information.
   */
  int16_t nonzero_magnitude_end() const;

  /**
   * See the [Rust documentation for `is_zero`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.is_zero) for more information.
   */
  bool is_zero() const;

  /**
   * Multiply the [`ICU4XFixedDecimal`] by a given power of ten.
   * 
   * See the [Rust documentation for `multiply_pow10`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.multiply_pow10) for more information.
   */
  void multiply_pow10(int16_t power);

  /**
   * See the [Rust documentation for `sign`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.sign) for more information.
   */
  ICU4XFixedDecimalSign sign() const;

  /**
   * Set the sign of the [`ICU4XFixedDecimal`].
   * 
   * See the [Rust documentation for `set_sign`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.set_sign) for more information.
   */
  void set_sign(ICU4XFixedDecimalSign sign);

  /**
   * See the [Rust documentation for `apply_sign_display`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.apply_sign_display) for more information.
   */
  void apply_sign_display(ICU4XFixedDecimalSignDisplay sign_display);

  /**
   * See the [Rust documentation for `trim_start`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.trim_start) for more information.
   */
  void trim_start();

  /**
   * See the [Rust documentation for `trim_end`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.trim_end) for more information.
   */
  void trim_end();

  /**
   * Zero-pad the [`ICU4XFixedDecimal`] on the left to a particular position
   * 
   * See the [Rust documentation for `pad_start`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.pad_start) for more information.
   */
  void pad_start(int16_t position);

  /**
   * Zero-pad the [`ICU4XFixedDecimal`] on the right to a particular position
   * 
   * See the [Rust documentation for `pad_end`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.pad_end) for more information.
   */
  void pad_end(int16_t position);

  /**
   * Truncate the [`ICU4XFixedDecimal`] on the left to a particular position, deleting digits if necessary. This is useful for, e.g. abbreviating years
   * ("2022" -> "22")
   * 
   * See the [Rust documentation for `set_max_position`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.set_max_position) for more information.
   */
  void set_max_position(int16_t position);

  /**
   * See the [Rust documentation for `trunc`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.trunc) for more information.
   */
  void trunc(int16_t position);

  /**
   * See the [Rust documentation for `half_trunc`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_trunc) for more information.
   */
  void half_trunc(int16_t position);

  /**
   * See the [Rust documentation for `expand`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.expand) for more information.
   */
  void expand(int16_t position);

  /**
   * See the [Rust documentation for `half_expand`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_expand) for more information.
   */
  void half_expand(int16_t position);

  /**
   * See the [Rust documentation for `ceil`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.ceil) for more information.
   */
  void ceil(int16_t position);

  /**
   * See the [Rust documentation for `half_ceil`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_ceil) for more information.
   */
  void half_ceil(int16_t position);

  /**
   * See the [Rust documentation for `floor`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.floor) for more information.
   */
  void floor(int16_t position);

  /**
   * See the [Rust documentation for `half_floor`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_floor) for more information.
   */
  void half_floor(int16_t position);

  /**
   * See the [Rust documentation for `half_even`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_even) for more information.
   */
  void half_even(int16_t position);

  /**
   * Concatenates `other` to the end of `self`.
   * 
   * If successful, `other` will be set to 0 and a successful status is returned.
   * 
   * If not successful, `other` will be unchanged and an error is returned.
   * 
   * See the [Rust documentation for `concatenate_end`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.concatenate_end) for more information.
   */
  diplomat::result<std::monostate, std::monostate> concatenate_end(ICU4XFixedDecimal& other);

  /**
   * Format the [`ICU4XFixedDecimal`] as a string.
   * 
   * See the [Rust documentation for `write_to`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.write_to) for more information.
   */
  template<typename W> void to_string_to_writeable(W& to) const;

  /**
   * Format the [`ICU4XFixedDecimal`] as a string.
   * 
   * See the [Rust documentation for `write_to`](https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.write_to) for more information.
   */
  std::string to_string() const;
  inline const capi::ICU4XFixedDecimal* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XFixedDecimal* AsFFIMut() { return this->inner.get(); }
  inline ICU4XFixedDecimal(capi::ICU4XFixedDecimal* i) : inner(i) {}
  ICU4XFixedDecimal() = default;
  ICU4XFixedDecimal(ICU4XFixedDecimal&&) noexcept = default;
  ICU4XFixedDecimal& operator=(ICU4XFixedDecimal&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XFixedDecimal, ICU4XFixedDecimalDeleter> inner;
};


inline ICU4XFixedDecimal ICU4XFixedDecimal::create_from_i32(int32_t v) {
  return ICU4XFixedDecimal(capi::ICU4XFixedDecimal_create_from_i32(v));
}
inline ICU4XFixedDecimal ICU4XFixedDecimal::create_from_u32(uint32_t v) {
  return ICU4XFixedDecimal(capi::ICU4XFixedDecimal_create_from_u32(v));
}
inline ICU4XFixedDecimal ICU4XFixedDecimal::create_from_i64(int64_t v) {
  return ICU4XFixedDecimal(capi::ICU4XFixedDecimal_create_from_i64(v));
}
inline ICU4XFixedDecimal ICU4XFixedDecimal::create_from_u64(uint64_t v) {
  return ICU4XFixedDecimal(capi::ICU4XFixedDecimal_create_from_u64(v));
}
inline diplomat::result<ICU4XFixedDecimal, ICU4XError> ICU4XFixedDecimal::create_from_f64_with_integer_precision(double f) {
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimal_create_from_f64_with_integer_precision(f);
  diplomat::result<ICU4XFixedDecimal, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XFixedDecimal>(ICU4XFixedDecimal(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XFixedDecimal, ICU4XError> ICU4XFixedDecimal::create_from_f64_with_lower_magnitude(double f, int16_t magnitude) {
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimal_create_from_f64_with_lower_magnitude(f, magnitude);
  diplomat::result<ICU4XFixedDecimal, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XFixedDecimal>(ICU4XFixedDecimal(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XFixedDecimal, ICU4XError> ICU4XFixedDecimal::create_from_f64_with_significant_digits(double f, uint8_t digits) {
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimal_create_from_f64_with_significant_digits(f, digits);
  diplomat::result<ICU4XFixedDecimal, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XFixedDecimal>(ICU4XFixedDecimal(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XFixedDecimal, ICU4XError> ICU4XFixedDecimal::create_from_f64_with_floating_precision(double f) {
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimal_create_from_f64_with_floating_precision(f);
  diplomat::result<ICU4XFixedDecimal, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XFixedDecimal>(ICU4XFixedDecimal(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XFixedDecimal, ICU4XError> ICU4XFixedDecimal::create_from_string(const std::string_view v) {
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimal_create_from_string(v.data(), v.size());
  diplomat::result<ICU4XFixedDecimal, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XFixedDecimal>(ICU4XFixedDecimal(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline uint8_t ICU4XFixedDecimal::digit_at(int16_t magnitude) const {
  return capi::ICU4XFixedDecimal_digit_at(this->inner.get(), magnitude);
}
inline int16_t ICU4XFixedDecimal::magnitude_start() const {
  return capi::ICU4XFixedDecimal_magnitude_start(this->inner.get());
}
inline int16_t ICU4XFixedDecimal::magnitude_end() const {
  return capi::ICU4XFixedDecimal_magnitude_end(this->inner.get());
}
inline int16_t ICU4XFixedDecimal::nonzero_magnitude_start() const {
  return capi::ICU4XFixedDecimal_nonzero_magnitude_start(this->inner.get());
}
inline int16_t ICU4XFixedDecimal::nonzero_magnitude_end() const {
  return capi::ICU4XFixedDecimal_nonzero_magnitude_end(this->inner.get());
}
inline bool ICU4XFixedDecimal::is_zero() const {
  return capi::ICU4XFixedDecimal_is_zero(this->inner.get());
}
inline void ICU4XFixedDecimal::multiply_pow10(int16_t power) {
  capi::ICU4XFixedDecimal_multiply_pow10(this->inner.get(), power);
}
inline ICU4XFixedDecimalSign ICU4XFixedDecimal::sign() const {
  return static_cast<ICU4XFixedDecimalSign>(capi::ICU4XFixedDecimal_sign(this->inner.get()));
}
inline void ICU4XFixedDecimal::set_sign(ICU4XFixedDecimalSign sign) {
  capi::ICU4XFixedDecimal_set_sign(this->inner.get(), static_cast<capi::ICU4XFixedDecimalSign>(sign));
}
inline void ICU4XFixedDecimal::apply_sign_display(ICU4XFixedDecimalSignDisplay sign_display) {
  capi::ICU4XFixedDecimal_apply_sign_display(this->inner.get(), static_cast<capi::ICU4XFixedDecimalSignDisplay>(sign_display));
}
inline void ICU4XFixedDecimal::trim_start() {
  capi::ICU4XFixedDecimal_trim_start(this->inner.get());
}
inline void ICU4XFixedDecimal::trim_end() {
  capi::ICU4XFixedDecimal_trim_end(this->inner.get());
}
inline void ICU4XFixedDecimal::pad_start(int16_t position) {
  capi::ICU4XFixedDecimal_pad_start(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::pad_end(int16_t position) {
  capi::ICU4XFixedDecimal_pad_end(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::set_max_position(int16_t position) {
  capi::ICU4XFixedDecimal_set_max_position(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::trunc(int16_t position) {
  capi::ICU4XFixedDecimal_trunc(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::half_trunc(int16_t position) {
  capi::ICU4XFixedDecimal_half_trunc(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::expand(int16_t position) {
  capi::ICU4XFixedDecimal_expand(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::half_expand(int16_t position) {
  capi::ICU4XFixedDecimal_half_expand(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::ceil(int16_t position) {
  capi::ICU4XFixedDecimal_ceil(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::half_ceil(int16_t position) {
  capi::ICU4XFixedDecimal_half_ceil(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::floor(int16_t position) {
  capi::ICU4XFixedDecimal_floor(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::half_floor(int16_t position) {
  capi::ICU4XFixedDecimal_half_floor(this->inner.get(), position);
}
inline void ICU4XFixedDecimal::half_even(int16_t position) {
  capi::ICU4XFixedDecimal_half_even(this->inner.get(), position);
}
inline diplomat::result<std::monostate, std::monostate> ICU4XFixedDecimal::concatenate_end(ICU4XFixedDecimal& other) {
  auto diplomat_result_raw_out_value = capi::ICU4XFixedDecimal_concatenate_end(this->inner.get(), other.AsFFIMut());
  diplomat::result<std::monostate, std::monostate> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err(std::monostate());
  }
  return diplomat_result_out_value;
}
template<typename W> inline void ICU4XFixedDecimal::to_string_to_writeable(W& to) const {
  capi::DiplomatWriteable to_writer = diplomat::WriteableTrait<W>::Construct(to);
  capi::ICU4XFixedDecimal_to_string(this->inner.get(), &to_writer);
}
inline std::string ICU4XFixedDecimal::to_string() const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  capi::ICU4XFixedDecimal_to_string(this->inner.get(), &diplomat_writeable_out);
  return diplomat_writeable_string;
}
#endif
