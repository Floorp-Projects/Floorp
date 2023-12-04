#ifndef ICU4XCodePointMapData8_HPP
#define ICU4XCodePointMapData8_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCodePointMapData8.h"

class CodePointRangeIterator;
class ICU4XCodePointSetData;
class ICU4XDataProvider;
class ICU4XCodePointMapData8;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XCodePointMapData8 with std::unique_ptr.
 */
struct ICU4XCodePointMapData8Deleter {
  void operator()(capi::ICU4XCodePointMapData8* l) const noexcept {
    capi::ICU4XCodePointMapData8_destroy(l);
  }
};

/**
 * An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.
 * 
 * For properties whose values fit into 8 bits.
 * 
 * See the [Rust documentation for `properties`](https://docs.rs/icu/latest/icu/properties/index.html) for more information.
 * 
 * See the [Rust documentation for `CodePointMapData`](https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapData.html) for more information.
 * 
 * See the [Rust documentation for `CodePointMapDataBorrowed`](https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html) for more information.
 */
class ICU4XCodePointMapData8 {
 public:

  /**
   * Gets the value for a code point.
   * 
   * See the [Rust documentation for `get`](https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.get) for more information.
   */
  uint8_t get(char32_t cp) const;

  /**
   * Gets the value for a code point (specified as a 32 bit integer, in UTF-32)
   */
  uint8_t get32(uint32_t cp) const;

  /**
   * Converts a general category to its corresponding mask value
   * 
   * Nonexistant general categories will map to the empty mask
   * 
   * See the [Rust documentation for `GeneralCategoryGroup`](https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html) for more information.
   */
  static uint32_t general_category_to_mask(uint8_t gc);

  /**
   * Produces an iterator over ranges of code points that map to `value`
   * 
   * See the [Rust documentation for `iter_ranges_for_value`](https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value) for more information.
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  CodePointRangeIterator iter_ranges_for_value(uint8_t value) const;

  /**
   * Produces an iterator over ranges of code points that do not map to `value`
   * 
   * See the [Rust documentation for `iter_ranges_for_value_complemented`](https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value_complemented) for more information.
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  CodePointRangeIterator iter_ranges_for_value_complemented(uint8_t value) const;

  /**
   * Given a mask value (the nth bit marks property value = n), produce an iterator over ranges of code points
   * whose property values are contained in the mask.
   * 
   * The main mask property supported is that for General_Category, which can be obtained via `general_category_to_mask()` or
   * by using `ICU4XGeneralCategoryNameToMaskMapper`
   * 
   * Should only be used on maps for properties with values less than 32 (like Generak_Category),
   * other maps will have unpredictable results
   * 
   * See the [Rust documentation for `iter_ranges_for_group`](https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_group) for more information.
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  CodePointRangeIterator iter_ranges_for_mask(uint32_t mask) const;

  /**
   * Gets a [`ICU4XCodePointSetData`] representing all entries in this map that map to the given value
   * 
   * See the [Rust documentation for `get_set_for_value`](https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.get_set_for_value) for more information.
   */
  ICU4XCodePointSetData get_set_for_value(uint8_t value) const;

  /**
   * See the [Rust documentation for `general_category`](https://docs.rs/icu/latest/icu/properties/maps/fn.general_category.html) for more information.
   */
  static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_general_category(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `bidi_class`](https://docs.rs/icu/latest/icu/properties/maps/fn.bidi_class.html) for more information.
   */
  static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_bidi_class(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `east_asian_width`](https://docs.rs/icu/latest/icu/properties/maps/fn.east_asian_width.html) for more information.
   */
  static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_east_asian_width(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `indic_syllabic_category`](https://docs.rs/icu/latest/icu/properties/maps/fn.indic_syllabic_category.html) for more information.
   */
  static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_indic_syllabic_category(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `line_break`](https://docs.rs/icu/latest/icu/properties/maps/fn.line_break.html) for more information.
   */
  static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_line_break(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `grapheme_cluster_break`](https://docs.rs/icu/latest/icu/properties/maps/fn.grapheme_cluster_break.html) for more information.
   */
  static diplomat::result<ICU4XCodePointMapData8, ICU4XError> try_grapheme_cluster_break(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `word_break`](https://docs.rs/icu/latest/icu/properties/maps/fn.word_break.html) for more information.
   */
  static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_word_break(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `sentence_break`](https://docs.rs/icu/latest/icu/properties/maps/fn.sentence_break.html) for more information.
   */
  static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_sentence_break(const ICU4XDataProvider& provider);
  inline const capi::ICU4XCodePointMapData8* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCodePointMapData8* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCodePointMapData8(capi::ICU4XCodePointMapData8* i) : inner(i) {}
  ICU4XCodePointMapData8() = default;
  ICU4XCodePointMapData8(ICU4XCodePointMapData8&&) noexcept = default;
  ICU4XCodePointMapData8& operator=(ICU4XCodePointMapData8&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCodePointMapData8, ICU4XCodePointMapData8Deleter> inner;
};

#include "CodePointRangeIterator.hpp"
#include "ICU4XCodePointSetData.hpp"
#include "ICU4XDataProvider.hpp"

inline uint8_t ICU4XCodePointMapData8::get(char32_t cp) const {
  return capi::ICU4XCodePointMapData8_get(this->inner.get(), cp);
}
inline uint8_t ICU4XCodePointMapData8::get32(uint32_t cp) const {
  return capi::ICU4XCodePointMapData8_get32(this->inner.get(), cp);
}
inline uint32_t ICU4XCodePointMapData8::general_category_to_mask(uint8_t gc) {
  return capi::ICU4XCodePointMapData8_general_category_to_mask(gc);
}
inline CodePointRangeIterator ICU4XCodePointMapData8::iter_ranges_for_value(uint8_t value) const {
  return CodePointRangeIterator(capi::ICU4XCodePointMapData8_iter_ranges_for_value(this->inner.get(), value));
}
inline CodePointRangeIterator ICU4XCodePointMapData8::iter_ranges_for_value_complemented(uint8_t value) const {
  return CodePointRangeIterator(capi::ICU4XCodePointMapData8_iter_ranges_for_value_complemented(this->inner.get(), value));
}
inline CodePointRangeIterator ICU4XCodePointMapData8::iter_ranges_for_mask(uint32_t mask) const {
  return CodePointRangeIterator(capi::ICU4XCodePointMapData8_iter_ranges_for_mask(this->inner.get(), mask));
}
inline ICU4XCodePointSetData ICU4XCodePointMapData8::get_set_for_value(uint8_t value) const {
  return ICU4XCodePointSetData(capi::ICU4XCodePointMapData8_get_set_for_value(this->inner.get(), value));
}
inline diplomat::result<ICU4XCodePointMapData8, ICU4XError> ICU4XCodePointMapData8::load_general_category(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCodePointMapData8_load_general_category(provider.AsFFI());
  diplomat::result<ICU4XCodePointMapData8, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCodePointMapData8>(ICU4XCodePointMapData8(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XCodePointMapData8, ICU4XError> ICU4XCodePointMapData8::load_bidi_class(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCodePointMapData8_load_bidi_class(provider.AsFFI());
  diplomat::result<ICU4XCodePointMapData8, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCodePointMapData8>(ICU4XCodePointMapData8(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XCodePointMapData8, ICU4XError> ICU4XCodePointMapData8::load_east_asian_width(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCodePointMapData8_load_east_asian_width(provider.AsFFI());
  diplomat::result<ICU4XCodePointMapData8, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCodePointMapData8>(ICU4XCodePointMapData8(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XCodePointMapData8, ICU4XError> ICU4XCodePointMapData8::load_indic_syllabic_category(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCodePointMapData8_load_indic_syllabic_category(provider.AsFFI());
  diplomat::result<ICU4XCodePointMapData8, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCodePointMapData8>(ICU4XCodePointMapData8(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XCodePointMapData8, ICU4XError> ICU4XCodePointMapData8::load_line_break(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCodePointMapData8_load_line_break(provider.AsFFI());
  diplomat::result<ICU4XCodePointMapData8, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCodePointMapData8>(ICU4XCodePointMapData8(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XCodePointMapData8, ICU4XError> ICU4XCodePointMapData8::try_grapheme_cluster_break(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCodePointMapData8_try_grapheme_cluster_break(provider.AsFFI());
  diplomat::result<ICU4XCodePointMapData8, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCodePointMapData8>(ICU4XCodePointMapData8(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XCodePointMapData8, ICU4XError> ICU4XCodePointMapData8::load_word_break(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCodePointMapData8_load_word_break(provider.AsFFI());
  diplomat::result<ICU4XCodePointMapData8, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCodePointMapData8>(ICU4XCodePointMapData8(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XCodePointMapData8, ICU4XError> ICU4XCodePointMapData8::load_sentence_break(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCodePointMapData8_load_sentence_break(provider.AsFFI());
  diplomat::result<ICU4XCodePointMapData8, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCodePointMapData8>(ICU4XCodePointMapData8(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
#endif
