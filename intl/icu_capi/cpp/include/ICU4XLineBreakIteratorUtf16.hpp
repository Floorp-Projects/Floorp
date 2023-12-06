#ifndef ICU4XLineBreakIteratorUtf16_HPP
#define ICU4XLineBreakIteratorUtf16_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLineBreakIteratorUtf16.h"


/**
 * A destruction policy for using ICU4XLineBreakIteratorUtf16 with std::unique_ptr.
 */
struct ICU4XLineBreakIteratorUtf16Deleter {
  void operator()(capi::ICU4XLineBreakIteratorUtf16* l) const noexcept {
    capi::ICU4XLineBreakIteratorUtf16_destroy(l);
  }
};

/**
 * See the [Rust documentation for `LineBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html) for more information.
 * 
 * Additional information: [1](https://docs.rs/icu/latest/icu/segmenter/type.LineBreakIteratorUtf16.html)
 */
class ICU4XLineBreakIteratorUtf16 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html#method.next) for more information.
   */
  int32_t next();
  inline const capi::ICU4XLineBreakIteratorUtf16* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLineBreakIteratorUtf16* AsFFIMut() { return this->inner.get(); }
  inline explicit ICU4XLineBreakIteratorUtf16(capi::ICU4XLineBreakIteratorUtf16* i) : inner(i) {}
  ICU4XLineBreakIteratorUtf16() = default;
  ICU4XLineBreakIteratorUtf16(ICU4XLineBreakIteratorUtf16&&) noexcept = default;
  ICU4XLineBreakIteratorUtf16& operator=(ICU4XLineBreakIteratorUtf16&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLineBreakIteratorUtf16, ICU4XLineBreakIteratorUtf16Deleter> inner;
};


inline int32_t ICU4XLineBreakIteratorUtf16::next() {
  return capi::ICU4XLineBreakIteratorUtf16_next(this->inner.get());
}
#endif
