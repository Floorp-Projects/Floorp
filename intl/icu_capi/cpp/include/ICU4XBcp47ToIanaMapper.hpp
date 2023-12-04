#ifndef ICU4XBcp47ToIanaMapper_HPP
#define ICU4XBcp47ToIanaMapper_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XBcp47ToIanaMapper.h"

class ICU4XDataProvider;
class ICU4XBcp47ToIanaMapper;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XBcp47ToIanaMapper with std::unique_ptr.
 */
struct ICU4XBcp47ToIanaMapperDeleter {
  void operator()(capi::ICU4XBcp47ToIanaMapper* l) const noexcept {
    capi::ICU4XBcp47ToIanaMapper_destroy(l);
  }
};

/**
 * An object capable of mapping from a BCP-47 time zone ID to an IANA ID.
 * 
 * See the [Rust documentation for `IanaBcp47RoundTripMapper`](https://docs.rs/icu/latest/icu/timezone/struct.IanaBcp47RoundTripMapper.html) for more information.
 */
class ICU4XBcp47ToIanaMapper {
 public:

  /**
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/timezone/struct.IanaBcp47RoundTripMapper.html#method.new) for more information.
   */
  static diplomat::result<ICU4XBcp47ToIanaMapper, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Writes out the canonical IANA time zone ID corresponding to the given BCP-47 ID.
   * 
   * See the [Rust documentation for `bcp47_to_iana`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.IanaBcp47RoundTripMapper.html#method.bcp47_to_iana) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> get_to_writeable(const std::string_view value, W& write) const;

  /**
   * Writes out the canonical IANA time zone ID corresponding to the given BCP-47 ID.
   * 
   * See the [Rust documentation for `bcp47_to_iana`](https://docs.rs/icu/latest/icu/datetime/time_zone/struct.IanaBcp47RoundTripMapper.html#method.bcp47_to_iana) for more information.
   */
  diplomat::result<std::string, ICU4XError> get(const std::string_view value) const;
  inline const capi::ICU4XBcp47ToIanaMapper* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XBcp47ToIanaMapper* AsFFIMut() { return this->inner.get(); }
  inline ICU4XBcp47ToIanaMapper(capi::ICU4XBcp47ToIanaMapper* i) : inner(i) {}
  ICU4XBcp47ToIanaMapper() = default;
  ICU4XBcp47ToIanaMapper(ICU4XBcp47ToIanaMapper&&) noexcept = default;
  ICU4XBcp47ToIanaMapper& operator=(ICU4XBcp47ToIanaMapper&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XBcp47ToIanaMapper, ICU4XBcp47ToIanaMapperDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline diplomat::result<ICU4XBcp47ToIanaMapper, ICU4XError> ICU4XBcp47ToIanaMapper::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XBcp47ToIanaMapper_create(provider.AsFFI());
  diplomat::result<ICU4XBcp47ToIanaMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XBcp47ToIanaMapper>(ICU4XBcp47ToIanaMapper(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XBcp47ToIanaMapper::get_to_writeable(const std::string_view value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XBcp47ToIanaMapper_get(this->inner.get(), value.data(), value.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XBcp47ToIanaMapper::get(const std::string_view value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XBcp47ToIanaMapper_get(this->inner.get(), value.data(), value.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
