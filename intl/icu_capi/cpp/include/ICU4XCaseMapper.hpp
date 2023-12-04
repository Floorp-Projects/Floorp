#ifndef ICU4XCaseMapper_HPP
#define ICU4XCaseMapper_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCaseMapper.h"

class ICU4XDataProvider;
class ICU4XCaseMapper;
#include "ICU4XError.hpp"
class ICU4XLocale;
struct ICU4XTitlecaseOptionsV1;
class ICU4XCodePointSetBuilder;

/**
 * A destruction policy for using ICU4XCaseMapper with std::unique_ptr.
 */
struct ICU4XCaseMapperDeleter {
  void operator()(capi::ICU4XCaseMapper* l) const noexcept {
    capi::ICU4XCaseMapper_destroy(l);
  }
};

/**
 * See the [Rust documentation for `CaseMapper`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html) for more information.
 */
class ICU4XCaseMapper {
 public:

  /**
   * Construct a new ICU4XCaseMapper instance
   * 
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.new) for more information.
   */
  static diplomat::result<ICU4XCaseMapper, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Returns the full lowercase mapping of the given string
   * 
   * See the [Rust documentation for `lowercase`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.lowercase) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> lowercase_to_writeable(const std::string_view s, const ICU4XLocale& locale, W& write) const;

  /**
   * Returns the full lowercase mapping of the given string
   * 
   * See the [Rust documentation for `lowercase`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.lowercase) for more information.
   */
  diplomat::result<std::string, ICU4XError> lowercase(const std::string_view s, const ICU4XLocale& locale) const;

  /**
   * Returns the full uppercase mapping of the given string
   * 
   * See the [Rust documentation for `uppercase`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.uppercase) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> uppercase_to_writeable(const std::string_view s, const ICU4XLocale& locale, W& write) const;

  /**
   * Returns the full uppercase mapping of the given string
   * 
   * See the [Rust documentation for `uppercase`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.uppercase) for more information.
   */
  diplomat::result<std::string, ICU4XError> uppercase(const std::string_view s, const ICU4XLocale& locale) const;

  /**
   * Returns the full titlecase mapping of the given string, performing head adjustment without
   * loading additional data.
   * (if head adjustment is enabled in the options)
   * 
   * The `v1` refers to the version of the options struct, which may change as we add more options
   * 
   * See the [Rust documentation for `titlecase_segment_with_only_case_data`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.titlecase_segment_with_only_case_data) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> titlecase_segment_with_only_case_data_v1_to_writeable(const std::string_view s, const ICU4XLocale& locale, ICU4XTitlecaseOptionsV1 options, W& write) const;

  /**
   * Returns the full titlecase mapping of the given string, performing head adjustment without
   * loading additional data.
   * (if head adjustment is enabled in the options)
   * 
   * The `v1` refers to the version of the options struct, which may change as we add more options
   * 
   * See the [Rust documentation for `titlecase_segment_with_only_case_data`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.titlecase_segment_with_only_case_data) for more information.
   */
  diplomat::result<std::string, ICU4XError> titlecase_segment_with_only_case_data_v1(const std::string_view s, const ICU4XLocale& locale, ICU4XTitlecaseOptionsV1 options) const;

  /**
   * Case-folds the characters in the given string
   * 
   * See the [Rust documentation for `fold`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.fold) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> fold_to_writeable(const std::string_view s, W& write) const;

  /**
   * Case-folds the characters in the given string
   * 
   * See the [Rust documentation for `fold`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.fold) for more information.
   */
  diplomat::result<std::string, ICU4XError> fold(const std::string_view s) const;

  /**
   * Case-folds the characters in the given string
   * using Turkic (T) mappings for dotted/dotless I.
   * 
   * See the [Rust documentation for `fold_turkic`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.fold_turkic) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> fold_turkic_to_writeable(const std::string_view s, W& write) const;

  /**
   * Case-folds the characters in the given string
   * using Turkic (T) mappings for dotted/dotless I.
   * 
   * See the [Rust documentation for `fold_turkic`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.fold_turkic) for more information.
   */
  diplomat::result<std::string, ICU4XError> fold_turkic(const std::string_view s) const;

  /**
   * Adds all simple case mappings and the full case folding for `c` to `builder`.
   * Also adds special case closure mappings.
   * 
   * In other words, this adds all characters that this casemaps to, as
   * well as all characters that may casemap to this one.
   * 
   * Note that since ICU4XCodePointSetBuilder does not contain strings, this will
   * ignore string mappings.
   * 
   * Identical to the similarly named method on `ICU4XCaseMapCloser`, use that if you
   * plan on using string case closure mappings too.
   * 
   * See the [Rust documentation for `add_case_closure_to`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.add_case_closure_to) for more information.
   */
  void add_case_closure_to(char32_t c, ICU4XCodePointSetBuilder& builder) const;

  /**
   * Returns the simple lowercase mapping of the given character.
   * 
   * This function only implements simple and common mappings.
   * Full mappings, which can map one char to a string, are not included.
   * For full mappings, use `ICU4XCaseMapper::lowercase`.
   * 
   * See the [Rust documentation for `simple_lowercase`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_lowercase) for more information.
   */
  char32_t simple_lowercase(char32_t ch) const;

  /**
   * Returns the simple uppercase mapping of the given character.
   * 
   * This function only implements simple and common mappings.
   * Full mappings, which can map one char to a string, are not included.
   * For full mappings, use `ICU4XCaseMapper::uppercase`.
   * 
   * See the [Rust documentation for `simple_uppercase`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_uppercase) for more information.
   */
  char32_t simple_uppercase(char32_t ch) const;

  /**
   * Returns the simple titlecase mapping of the given character.
   * 
   * This function only implements simple and common mappings.
   * Full mappings, which can map one char to a string, are not included.
   * For full mappings, use `ICU4XCaseMapper::titlecase_segment`.
   * 
   * See the [Rust documentation for `simple_titlecase`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_titlecase) for more information.
   */
  char32_t simple_titlecase(char32_t ch) const;

  /**
   * Returns the simple casefolding of the given character.
   * 
   * This function only implements simple folding.
   * For full folding, use `ICU4XCaseMapper::fold`.
   * 
   * See the [Rust documentation for `simple_fold`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_fold) for more information.
   */
  char32_t simple_fold(char32_t ch) const;

  /**
   * Returns the simple casefolding of the given character in the Turkic locale
   * 
   * This function only implements simple folding.
   * For full folding, use `ICU4XCaseMapper::fold_turkic`.
   * 
   * See the [Rust documentation for `simple_fold_turkic`](https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_fold_turkic) for more information.
   */
  char32_t simple_fold_turkic(char32_t ch) const;
  inline const capi::ICU4XCaseMapper* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCaseMapper* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCaseMapper(capi::ICU4XCaseMapper* i) : inner(i) {}
  ICU4XCaseMapper() = default;
  ICU4XCaseMapper(ICU4XCaseMapper&&) noexcept = default;
  ICU4XCaseMapper& operator=(ICU4XCaseMapper&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCaseMapper, ICU4XCaseMapperDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XTitlecaseOptionsV1.hpp"
#include "ICU4XCodePointSetBuilder.hpp"

inline diplomat::result<ICU4XCaseMapper, ICU4XError> ICU4XCaseMapper::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_create(provider.AsFFI());
  diplomat::result<ICU4XCaseMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCaseMapper>(ICU4XCaseMapper(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XCaseMapper::lowercase_to_writeable(const std::string_view s, const ICU4XLocale& locale, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_lowercase(this->inner.get(), s.data(), s.size(), locale.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XCaseMapper::lowercase(const std::string_view s, const ICU4XLocale& locale) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_lowercase(this->inner.get(), s.data(), s.size(), locale.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XCaseMapper::uppercase_to_writeable(const std::string_view s, const ICU4XLocale& locale, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_uppercase(this->inner.get(), s.data(), s.size(), locale.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XCaseMapper::uppercase(const std::string_view s, const ICU4XLocale& locale) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_uppercase(this->inner.get(), s.data(), s.size(), locale.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XCaseMapper::titlecase_segment_with_only_case_data_v1_to_writeable(const std::string_view s, const ICU4XLocale& locale, ICU4XTitlecaseOptionsV1 options, W& write) const {
  ICU4XTitlecaseOptionsV1 diplomat_wrapped_struct_options = options;
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_titlecase_segment_with_only_case_data_v1(this->inner.get(), s.data(), s.size(), locale.AsFFI(), capi::ICU4XTitlecaseOptionsV1{ .leading_adjustment = static_cast<capi::ICU4XLeadingAdjustment>(diplomat_wrapped_struct_options.leading_adjustment), .trailing_case = static_cast<capi::ICU4XTrailingCase>(diplomat_wrapped_struct_options.trailing_case) }, &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XCaseMapper::titlecase_segment_with_only_case_data_v1(const std::string_view s, const ICU4XLocale& locale, ICU4XTitlecaseOptionsV1 options) const {
  ICU4XTitlecaseOptionsV1 diplomat_wrapped_struct_options = options;
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_titlecase_segment_with_only_case_data_v1(this->inner.get(), s.data(), s.size(), locale.AsFFI(), capi::ICU4XTitlecaseOptionsV1{ .leading_adjustment = static_cast<capi::ICU4XLeadingAdjustment>(diplomat_wrapped_struct_options.leading_adjustment), .trailing_case = static_cast<capi::ICU4XTrailingCase>(diplomat_wrapped_struct_options.trailing_case) }, &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XCaseMapper::fold_to_writeable(const std::string_view s, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_fold(this->inner.get(), s.data(), s.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XCaseMapper::fold(const std::string_view s) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_fold(this->inner.get(), s.data(), s.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XCaseMapper::fold_turkic_to_writeable(const std::string_view s, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_fold_turkic(this->inner.get(), s.data(), s.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XCaseMapper::fold_turkic(const std::string_view s) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XCaseMapper_fold_turkic(this->inner.get(), s.data(), s.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline void ICU4XCaseMapper::add_case_closure_to(char32_t c, ICU4XCodePointSetBuilder& builder) const {
  capi::ICU4XCaseMapper_add_case_closure_to(this->inner.get(), c, builder.AsFFIMut());
}
inline char32_t ICU4XCaseMapper::simple_lowercase(char32_t ch) const {
  return capi::ICU4XCaseMapper_simple_lowercase(this->inner.get(), ch);
}
inline char32_t ICU4XCaseMapper::simple_uppercase(char32_t ch) const {
  return capi::ICU4XCaseMapper_simple_uppercase(this->inner.get(), ch);
}
inline char32_t ICU4XCaseMapper::simple_titlecase(char32_t ch) const {
  return capi::ICU4XCaseMapper_simple_titlecase(this->inner.get(), ch);
}
inline char32_t ICU4XCaseMapper::simple_fold(char32_t ch) const {
  return capi::ICU4XCaseMapper_simple_fold(this->inner.get(), ch);
}
inline char32_t ICU4XCaseMapper::simple_fold_turkic(char32_t ch) const {
  return capi::ICU4XCaseMapper_simple_fold_turkic(this->inner.get(), ch);
}
#endif
