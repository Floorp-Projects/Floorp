#ifndef ICU4XRegionDisplayNames_HPP
#define ICU4XRegionDisplayNames_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XRegionDisplayNames.h"

class ICU4XDataProvider;
class ICU4XLocale;
class ICU4XRegionDisplayNames;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XRegionDisplayNames with std::unique_ptr.
 */
struct ICU4XRegionDisplayNamesDeleter {
  void operator()(capi::ICU4XRegionDisplayNames* l) const noexcept {
    capi::ICU4XRegionDisplayNames_destroy(l);
  }
};

/**
 * See the [Rust documentation for `RegionDisplayNames`](https://docs.rs/icu/latest/icu/displaynames/struct.RegionDisplayNames.html) for more information.
 */
class ICU4XRegionDisplayNames {
 public:

  /**
   * Creates a new `RegionDisplayNames` from locale data and an options bag.
   * 
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/latest/icu/displaynames/struct.RegionDisplayNames.html#method.try_new) for more information.
   */
  static diplomat::result<ICU4XRegionDisplayNames, ICU4XError> create(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * Returns the locale specific display name of a region.
   * Note that the funtion returns an empty string in case the display name for a given
   * region code is not found.
   * 
   * See the [Rust documentation for `of`](https://docs.rs/icu/latest/icu/displaynames/struct.RegionDisplayNames.html#method.of) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> of_to_writeable(const std::string_view region, W& write) const;

  /**
   * Returns the locale specific display name of a region.
   * Note that the funtion returns an empty string in case the display name for a given
   * region code is not found.
   * 
   * See the [Rust documentation for `of`](https://docs.rs/icu/latest/icu/displaynames/struct.RegionDisplayNames.html#method.of) for more information.
   */
  diplomat::result<std::string, ICU4XError> of(const std::string_view region) const;
  inline const capi::ICU4XRegionDisplayNames* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XRegionDisplayNames* AsFFIMut() { return this->inner.get(); }
  inline ICU4XRegionDisplayNames(capi::ICU4XRegionDisplayNames* i) : inner(i) {}
  ICU4XRegionDisplayNames() = default;
  ICU4XRegionDisplayNames(ICU4XRegionDisplayNames&&) noexcept = default;
  ICU4XRegionDisplayNames& operator=(ICU4XRegionDisplayNames&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XRegionDisplayNames, ICU4XRegionDisplayNamesDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"

inline diplomat::result<ICU4XRegionDisplayNames, ICU4XError> ICU4XRegionDisplayNames::create(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XRegionDisplayNames_create(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XRegionDisplayNames, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XRegionDisplayNames>(ICU4XRegionDisplayNames(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XRegionDisplayNames::of_to_writeable(const std::string_view region, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XRegionDisplayNames_of(this->inner.get(), region.data(), region.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XRegionDisplayNames::of(const std::string_view region) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XRegionDisplayNames_of(this->inner.get(), region.data(), region.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
