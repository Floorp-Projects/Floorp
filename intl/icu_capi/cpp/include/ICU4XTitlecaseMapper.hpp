#ifndef ICU4XTitlecaseMapper_HPP
#define ICU4XTitlecaseMapper_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTitlecaseMapper.h"

class ICU4XDataProvider;
class ICU4XTitlecaseMapper;
#include "ICU4XError.hpp"
class ICU4XLocale;
struct ICU4XTitlecaseOptionsV1;

/**
 * A destruction policy for using ICU4XTitlecaseMapper with std::unique_ptr.
 */
struct ICU4XTitlecaseMapperDeleter {
  void operator()(capi::ICU4XTitlecaseMapper* l) const noexcept {
    capi::ICU4XTitlecaseMapper_destroy(l);
  }
};

/**
 * See the [Rust documentation for `TitlecaseMapper`](https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html) for more information.
 */
class ICU4XTitlecaseMapper {
 public:

  /**
   * Construct a new `ICU4XTitlecaseMapper` instance
   * 
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html#method.new) for more information.
   */
  static diplomat::result<ICU4XTitlecaseMapper, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Returns the full titlecase mapping of the given string
   * 
   * The `v1` refers to the version of the options struct, which may change as we add more options
   * 
   * See the [Rust documentation for `titlecase_segment`](https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html#method.titlecase_segment) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> titlecase_segment_v1_to_writeable(const std::string_view s, const ICU4XLocale& locale, ICU4XTitlecaseOptionsV1 options, W& write) const;

  /**
   * Returns the full titlecase mapping of the given string
   * 
   * The `v1` refers to the version of the options struct, which may change as we add more options
   * 
   * See the [Rust documentation for `titlecase_segment`](https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html#method.titlecase_segment) for more information.
   */
  diplomat::result<std::string, ICU4XError> titlecase_segment_v1(const std::string_view s, const ICU4XLocale& locale, ICU4XTitlecaseOptionsV1 options) const;
  inline const capi::ICU4XTitlecaseMapper* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XTitlecaseMapper* AsFFIMut() { return this->inner.get(); }
  inline ICU4XTitlecaseMapper(capi::ICU4XTitlecaseMapper* i) : inner(i) {}
  ICU4XTitlecaseMapper() = default;
  ICU4XTitlecaseMapper(ICU4XTitlecaseMapper&&) noexcept = default;
  ICU4XTitlecaseMapper& operator=(ICU4XTitlecaseMapper&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XTitlecaseMapper, ICU4XTitlecaseMapperDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XTitlecaseOptionsV1.hpp"

inline diplomat::result<ICU4XTitlecaseMapper, ICU4XError> ICU4XTitlecaseMapper::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XTitlecaseMapper_create(provider.AsFFI());
  diplomat::result<ICU4XTitlecaseMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XTitlecaseMapper>(ICU4XTitlecaseMapper(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTitlecaseMapper::titlecase_segment_v1_to_writeable(const std::string_view s, const ICU4XLocale& locale, ICU4XTitlecaseOptionsV1 options, W& write) const {
  ICU4XTitlecaseOptionsV1 diplomat_wrapped_struct_options = options;
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTitlecaseMapper_titlecase_segment_v1(this->inner.get(), s.data(), s.size(), locale.AsFFI(), capi::ICU4XTitlecaseOptionsV1{ .leading_adjustment = static_cast<capi::ICU4XLeadingAdjustment>(diplomat_wrapped_struct_options.leading_adjustment), .trailing_case = static_cast<capi::ICU4XTrailingCase>(diplomat_wrapped_struct_options.trailing_case) }, &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTitlecaseMapper::titlecase_segment_v1(const std::string_view s, const ICU4XLocale& locale, ICU4XTitlecaseOptionsV1 options) const {
  ICU4XTitlecaseOptionsV1 diplomat_wrapped_struct_options = options;
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTitlecaseMapper_titlecase_segment_v1(this->inner.get(), s.data(), s.size(), locale.AsFFI(), capi::ICU4XTitlecaseOptionsV1{ .leading_adjustment = static_cast<capi::ICU4XLeadingAdjustment>(diplomat_wrapped_struct_options.leading_adjustment), .trailing_case = static_cast<capi::ICU4XTrailingCase>(diplomat_wrapped_struct_options.trailing_case) }, &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
