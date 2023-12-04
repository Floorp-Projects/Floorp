#ifndef ICU4XGeneralCategoryNameToMaskMapper_HPP
#define ICU4XGeneralCategoryNameToMaskMapper_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XGeneralCategoryNameToMaskMapper.h"

class ICU4XDataProvider;
class ICU4XGeneralCategoryNameToMaskMapper;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XGeneralCategoryNameToMaskMapper with std::unique_ptr.
 */
struct ICU4XGeneralCategoryNameToMaskMapperDeleter {
  void operator()(capi::ICU4XGeneralCategoryNameToMaskMapper* l) const noexcept {
    capi::ICU4XGeneralCategoryNameToMaskMapper_destroy(l);
  }
};

/**
 * A type capable of looking up General Category mask values from a string name.
 * 
 * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html#method.get_name_to_enum_mapper) for more information.
 * 
 * See the [Rust documentation for `PropertyValueNameToEnumMapper`](https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapper.html) for more information.
 */
class ICU4XGeneralCategoryNameToMaskMapper {
 public:

  /**
   * Get the mask value matching the given name, using strict matching
   * 
   * Returns 0 if the name is unknown for this property
   */
  uint32_t get_strict(const std::string_view name) const;

  /**
   * Get the mask value matching the given name, using loose matching
   * 
   * Returns 0 if the name is unknown for this property
   */
  uint32_t get_loose(const std::string_view name) const;

  /**
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XGeneralCategoryNameToMaskMapper, ICU4XError> load(const ICU4XDataProvider& provider);
  inline const capi::ICU4XGeneralCategoryNameToMaskMapper* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XGeneralCategoryNameToMaskMapper* AsFFIMut() { return this->inner.get(); }
  inline ICU4XGeneralCategoryNameToMaskMapper(capi::ICU4XGeneralCategoryNameToMaskMapper* i) : inner(i) {}
  ICU4XGeneralCategoryNameToMaskMapper() = default;
  ICU4XGeneralCategoryNameToMaskMapper(ICU4XGeneralCategoryNameToMaskMapper&&) noexcept = default;
  ICU4XGeneralCategoryNameToMaskMapper& operator=(ICU4XGeneralCategoryNameToMaskMapper&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XGeneralCategoryNameToMaskMapper, ICU4XGeneralCategoryNameToMaskMapperDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline uint32_t ICU4XGeneralCategoryNameToMaskMapper::get_strict(const std::string_view name) const {
  return capi::ICU4XGeneralCategoryNameToMaskMapper_get_strict(this->inner.get(), name.data(), name.size());
}
inline uint32_t ICU4XGeneralCategoryNameToMaskMapper::get_loose(const std::string_view name) const {
  return capi::ICU4XGeneralCategoryNameToMaskMapper_get_loose(this->inner.get(), name.data(), name.size());
}
inline diplomat::result<ICU4XGeneralCategoryNameToMaskMapper, ICU4XError> ICU4XGeneralCategoryNameToMaskMapper::load(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XGeneralCategoryNameToMaskMapper_load(provider.AsFFI());
  diplomat::result<ICU4XGeneralCategoryNameToMaskMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XGeneralCategoryNameToMaskMapper>(ICU4XGeneralCategoryNameToMaskMapper(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
#endif
