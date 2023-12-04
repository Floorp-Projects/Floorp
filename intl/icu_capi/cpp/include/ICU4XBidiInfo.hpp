#ifndef ICU4XBidiInfo_HPP
#define ICU4XBidiInfo_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XBidiInfo.h"

class ICU4XBidiParagraph;

/**
 * A destruction policy for using ICU4XBidiInfo with std::unique_ptr.
 */
struct ICU4XBidiInfoDeleter {
  void operator()(capi::ICU4XBidiInfo* l) const noexcept {
    capi::ICU4XBidiInfo_destroy(l);
  }
};

/**
 * An object containing bidi information for a given string, produced by `for_text()` on `ICU4XBidi`
 * 
 * See the [Rust documentation for `BidiInfo`](https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.BidiInfo.html) for more information.
 */
class ICU4XBidiInfo {
 public:

  /**
   * The number of paragraphs contained here
   */
  size_t paragraph_count() const;

  /**
   * Get the nth paragraph, returning `None` if out of bounds
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  std::optional<ICU4XBidiParagraph> paragraph_at(size_t n) const;

  /**
   * The number of bytes in this full text
   */
  size_t size() const;

  /**
   * Get the BIDI level at a particular byte index in the full text.
   * This integer is conceptually a `unicode_bidi::Level`,
   * and can be further inspected using the static methods on ICU4XBidi.
   * 
   * Returns 0 (equivalent to `Level::ltr()`) on error
   */
  uint8_t level_at(size_t pos) const;
  inline const capi::ICU4XBidiInfo* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XBidiInfo* AsFFIMut() { return this->inner.get(); }
  inline ICU4XBidiInfo(capi::ICU4XBidiInfo* i) : inner(i) {}
  ICU4XBidiInfo() = default;
  ICU4XBidiInfo(ICU4XBidiInfo&&) noexcept = default;
  ICU4XBidiInfo& operator=(ICU4XBidiInfo&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XBidiInfo, ICU4XBidiInfoDeleter> inner;
};

#include "ICU4XBidiParagraph.hpp"

inline size_t ICU4XBidiInfo::paragraph_count() const {
  return capi::ICU4XBidiInfo_paragraph_count(this->inner.get());
}
inline std::optional<ICU4XBidiParagraph> ICU4XBidiInfo::paragraph_at(size_t n) const {
  auto diplomat_optional_raw_out_value = capi::ICU4XBidiInfo_paragraph_at(this->inner.get(), n);
  std::optional<ICU4XBidiParagraph> diplomat_optional_out_value;
  if (diplomat_optional_raw_out_value != nullptr) {
    diplomat_optional_out_value = ICU4XBidiParagraph(diplomat_optional_raw_out_value);
  } else {
    diplomat_optional_out_value = std::nullopt;
  }
  return diplomat_optional_out_value;
}
inline size_t ICU4XBidiInfo::size() const {
  return capi::ICU4XBidiInfo_size(this->inner.get());
}
inline uint8_t ICU4XBidiInfo::level_at(size_t pos) const {
  return capi::ICU4XBidiInfo_level_at(this->inner.get(), pos);
}
#endif
