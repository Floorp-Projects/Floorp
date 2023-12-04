#ifndef ICU4XCodePointSetBuilder_HPP
#define ICU4XCodePointSetBuilder_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCodePointSetBuilder.h"

class ICU4XCodePointSetBuilder;
class ICU4XCodePointSetData;

/**
 * A destruction policy for using ICU4XCodePointSetBuilder with std::unique_ptr.
 */
struct ICU4XCodePointSetBuilderDeleter {
  void operator()(capi::ICU4XCodePointSetBuilder* l) const noexcept {
    capi::ICU4XCodePointSetBuilder_destroy(l);
  }
};

/**
 * See the [Rust documentation for `CodePointInversionListBuilder`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html) for more information.
 */
class ICU4XCodePointSetBuilder {
 public:

  /**
   * Make a new set builder containing nothing
   * 
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.new) for more information.
   */
  static ICU4XCodePointSetBuilder create();

  /**
   * Build this into a set
   * 
   * This object is repopulated with an empty builder
   * 
   * See the [Rust documentation for `build`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.build) for more information.
   */
  ICU4XCodePointSetData build();

  /**
   * Complements this set
   * 
   * (Elements in this set are removed and vice versa)
   * 
   * See the [Rust documentation for `complement`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement) for more information.
   */
  void complement();

  /**
   * Returns whether this set is empty
   * 
   * See the [Rust documentation for `is_empty`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.is_empty) for more information.
   */
  bool is_empty() const;

  /**
   * Add a single character to the set
   * 
   * See the [Rust documentation for `add_char`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_char) for more information.
   */
  void add_char(char32_t ch);

  /**
   * Add a single u32 value to the set
   * 
   * See the [Rust documentation for `add_u32`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_u32) for more information.
   */
  void add_u32(uint32_t ch);

  /**
   * Add an inclusive range of characters to the set
   * 
   * See the [Rust documentation for `add_range`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_range) for more information.
   */
  void add_inclusive_range(char32_t start, char32_t end);

  /**
   * Add an inclusive range of u32s to the set
   * 
   * See the [Rust documentation for `add_range_u32`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_range_u32) for more information.
   */
  void add_inclusive_range_u32(uint32_t start, uint32_t end);

  /**
   * Add all elements that belong to the provided set to the set
   * 
   * See the [Rust documentation for `add_set`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_set) for more information.
   */
  void add_set(const ICU4XCodePointSetData& data);

  /**
   * Remove a single character to the set
   * 
   * See the [Rust documentation for `remove_char`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_char) for more information.
   */
  void remove_char(char32_t ch);

  /**
   * Remove an inclusive range of characters from the set
   * 
   * See the [Rust documentation for `remove_range`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_range) for more information.
   */
  void remove_inclusive_range(char32_t start, char32_t end);

  /**
   * Remove all elements that belong to the provided set from the set
   * 
   * See the [Rust documentation for `remove_set`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_set) for more information.
   */
  void remove_set(const ICU4XCodePointSetData& data);

  /**
   * Removes all elements from the set except a single character
   * 
   * See the [Rust documentation for `retain_char`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_char) for more information.
   */
  void retain_char(char32_t ch);

  /**
   * Removes all elements from the set except an inclusive range of characters f
   * 
   * See the [Rust documentation for `retain_range`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_range) for more information.
   */
  void retain_inclusive_range(char32_t start, char32_t end);

  /**
   * Removes all elements from the set except all elements in the provided set
   * 
   * See the [Rust documentation for `retain_set`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_set) for more information.
   */
  void retain_set(const ICU4XCodePointSetData& data);

  /**
   * Complement a single character to the set
   * 
   * (Characters which are in this set are removed and vice versa)
   * 
   * See the [Rust documentation for `complement_char`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_char) for more information.
   */
  void complement_char(char32_t ch);

  /**
   * Complement an inclusive range of characters from the set
   * 
   * (Characters which are in this set are removed and vice versa)
   * 
   * See the [Rust documentation for `complement_range`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_range) for more information.
   */
  void complement_inclusive_range(char32_t start, char32_t end);

  /**
   * Complement all elements that belong to the provided set from the set
   * 
   * (Characters which are in this set are removed and vice versa)
   * 
   * See the [Rust documentation for `complement_set`](https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_set) for more information.
   */
  void complement_set(const ICU4XCodePointSetData& data);
  inline const capi::ICU4XCodePointSetBuilder* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCodePointSetBuilder* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCodePointSetBuilder(capi::ICU4XCodePointSetBuilder* i) : inner(i) {}
  ICU4XCodePointSetBuilder() = default;
  ICU4XCodePointSetBuilder(ICU4XCodePointSetBuilder&&) noexcept = default;
  ICU4XCodePointSetBuilder& operator=(ICU4XCodePointSetBuilder&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCodePointSetBuilder, ICU4XCodePointSetBuilderDeleter> inner;
};

#include "ICU4XCodePointSetData.hpp"

inline ICU4XCodePointSetBuilder ICU4XCodePointSetBuilder::create() {
  return ICU4XCodePointSetBuilder(capi::ICU4XCodePointSetBuilder_create());
}
inline ICU4XCodePointSetData ICU4XCodePointSetBuilder::build() {
  return ICU4XCodePointSetData(capi::ICU4XCodePointSetBuilder_build(this->inner.get()));
}
inline void ICU4XCodePointSetBuilder::complement() {
  capi::ICU4XCodePointSetBuilder_complement(this->inner.get());
}
inline bool ICU4XCodePointSetBuilder::is_empty() const {
  return capi::ICU4XCodePointSetBuilder_is_empty(this->inner.get());
}
inline void ICU4XCodePointSetBuilder::add_char(char32_t ch) {
  capi::ICU4XCodePointSetBuilder_add_char(this->inner.get(), ch);
}
inline void ICU4XCodePointSetBuilder::add_u32(uint32_t ch) {
  capi::ICU4XCodePointSetBuilder_add_u32(this->inner.get(), ch);
}
inline void ICU4XCodePointSetBuilder::add_inclusive_range(char32_t start, char32_t end) {
  capi::ICU4XCodePointSetBuilder_add_inclusive_range(this->inner.get(), start, end);
}
inline void ICU4XCodePointSetBuilder::add_inclusive_range_u32(uint32_t start, uint32_t end) {
  capi::ICU4XCodePointSetBuilder_add_inclusive_range_u32(this->inner.get(), start, end);
}
inline void ICU4XCodePointSetBuilder::add_set(const ICU4XCodePointSetData& data) {
  capi::ICU4XCodePointSetBuilder_add_set(this->inner.get(), data.AsFFI());
}
inline void ICU4XCodePointSetBuilder::remove_char(char32_t ch) {
  capi::ICU4XCodePointSetBuilder_remove_char(this->inner.get(), ch);
}
inline void ICU4XCodePointSetBuilder::remove_inclusive_range(char32_t start, char32_t end) {
  capi::ICU4XCodePointSetBuilder_remove_inclusive_range(this->inner.get(), start, end);
}
inline void ICU4XCodePointSetBuilder::remove_set(const ICU4XCodePointSetData& data) {
  capi::ICU4XCodePointSetBuilder_remove_set(this->inner.get(), data.AsFFI());
}
inline void ICU4XCodePointSetBuilder::retain_char(char32_t ch) {
  capi::ICU4XCodePointSetBuilder_retain_char(this->inner.get(), ch);
}
inline void ICU4XCodePointSetBuilder::retain_inclusive_range(char32_t start, char32_t end) {
  capi::ICU4XCodePointSetBuilder_retain_inclusive_range(this->inner.get(), start, end);
}
inline void ICU4XCodePointSetBuilder::retain_set(const ICU4XCodePointSetData& data) {
  capi::ICU4XCodePointSetBuilder_retain_set(this->inner.get(), data.AsFFI());
}
inline void ICU4XCodePointSetBuilder::complement_char(char32_t ch) {
  capi::ICU4XCodePointSetBuilder_complement_char(this->inner.get(), ch);
}
inline void ICU4XCodePointSetBuilder::complement_inclusive_range(char32_t start, char32_t end) {
  capi::ICU4XCodePointSetBuilder_complement_inclusive_range(this->inner.get(), start, end);
}
inline void ICU4XCodePointSetBuilder::complement_set(const ICU4XCodePointSetData& data) {
  capi::ICU4XCodePointSetBuilder_complement_set(this->inner.get(), data.AsFFI());
}
#endif
