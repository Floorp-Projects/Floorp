#ifndef ICU4XCollator_HPP
#define ICU4XCollator_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCollator.h"

class ICU4XDataProvider;
class ICU4XLocale;
struct ICU4XCollatorOptionsV1;
class ICU4XCollator;
#include "ICU4XError.hpp"
#include "ICU4XOrdering.hpp"

/**
 * A destruction policy for using ICU4XCollator with std::unique_ptr.
 */
struct ICU4XCollatorDeleter {
  void operator()(capi::ICU4XCollator* l) const noexcept {
    capi::ICU4XCollator_destroy(l);
  }
};

/**
 * See the [Rust documentation for `Collator`](https://docs.rs/icu/latest/icu/collator/struct.Collator.html) for more information.
 */
class ICU4XCollator {
 public:

  /**
   * Construct a new Collator instance.
   * 
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.try_new) for more information.
   */
  static diplomat::result<ICU4XCollator, ICU4XError> create_v1(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XCollatorOptionsV1 options);

  /**
   * Compare potentially ill-formed UTF-8 strings.
   * 
   * Ill-formed input is compared
   * as if errors had been replaced with REPLACEMENT CHARACTERs according
   * to the WHATWG Encoding Standard.
   * 
   * See the [Rust documentation for `compare_utf8`](https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare_utf8) for more information.
   */
  ICU4XOrdering compare(const std::string_view left, const std::string_view right) const;

  /**
   * Compare guaranteed well-formed UTF-8 strings.
   * 
   * Note: In C++, passing ill-formed UTF-8 strings is undefined behavior
   * (and may be memory-unsafe to do so, too).
   * 
   * See the [Rust documentation for `compare`](https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare) for more information.
   */
  ICU4XOrdering compare_valid_utf8(const std::string_view left, const std::string_view right) const;

  /**
   * Compare potentially ill-formed UTF-16 strings, with unpaired surrogates
   * compared as REPLACEMENT CHARACTER.
   * 
   * See the [Rust documentation for `compare_utf16`](https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare_utf16) for more information.
   */
  ICU4XOrdering compare_utf16(const diplomat::span<const uint16_t> left, const diplomat::span<const uint16_t> right) const;
  inline const capi::ICU4XCollator* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCollator* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCollator(capi::ICU4XCollator* i) : inner(i) {}
  ICU4XCollator() = default;
  ICU4XCollator(ICU4XCollator&&) noexcept = default;
  ICU4XCollator& operator=(ICU4XCollator&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCollator, ICU4XCollatorDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XCollatorOptionsV1.hpp"

inline diplomat::result<ICU4XCollator, ICU4XError> ICU4XCollator::create_v1(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XCollatorOptionsV1 options) {
  ICU4XCollatorOptionsV1 diplomat_wrapped_struct_options = options;
  auto diplomat_result_raw_out_value = capi::ICU4XCollator_create_v1(provider.AsFFI(), locale.AsFFI(), capi::ICU4XCollatorOptionsV1{ .strength = static_cast<capi::ICU4XCollatorStrength>(diplomat_wrapped_struct_options.strength), .alternate_handling = static_cast<capi::ICU4XCollatorAlternateHandling>(diplomat_wrapped_struct_options.alternate_handling), .case_first = static_cast<capi::ICU4XCollatorCaseFirst>(diplomat_wrapped_struct_options.case_first), .max_variable = static_cast<capi::ICU4XCollatorMaxVariable>(diplomat_wrapped_struct_options.max_variable), .case_level = static_cast<capi::ICU4XCollatorCaseLevel>(diplomat_wrapped_struct_options.case_level), .numeric = static_cast<capi::ICU4XCollatorNumeric>(diplomat_wrapped_struct_options.numeric), .backward_second_level = static_cast<capi::ICU4XCollatorBackwardSecondLevel>(diplomat_wrapped_struct_options.backward_second_level) });
  diplomat::result<ICU4XCollator, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCollator>(ICU4XCollator(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline ICU4XOrdering ICU4XCollator::compare(const std::string_view left, const std::string_view right) const {
  return static_cast<ICU4XOrdering>(capi::ICU4XCollator_compare(this->inner.get(), left.data(), left.size(), right.data(), right.size()));
}
inline ICU4XOrdering ICU4XCollator::compare_valid_utf8(const std::string_view left, const std::string_view right) const {
  return static_cast<ICU4XOrdering>(capi::ICU4XCollator_compare_valid_utf8(this->inner.get(), left.data(), left.size(), right.data(), right.size()));
}
inline ICU4XOrdering ICU4XCollator::compare_utf16(const diplomat::span<const uint16_t> left, const diplomat::span<const uint16_t> right) const {
  return static_cast<ICU4XOrdering>(capi::ICU4XCollator_compare_utf16(this->inner.get(), left.data(), left.size(), right.data(), right.size()));
}
#endif
