#ifndef ICU4XSentenceBreakIteratorUtf16_HPP
#define ICU4XSentenceBreakIteratorUtf16_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XSentenceBreakIteratorUtf16.h"


/**
 * A destruction policy for using ICU4XSentenceBreakIteratorUtf16 with std::unique_ptr.
 */
struct ICU4XSentenceBreakIteratorUtf16Deleter {
  void operator()(capi::ICU4XSentenceBreakIteratorUtf16* l) const noexcept {
    capi::ICU4XSentenceBreakIteratorUtf16_destroy(l);
  }
};

/**
 * See the [Rust documentation for `SentenceBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html) for more information.
 */
class ICU4XSentenceBreakIteratorUtf16 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next) for more information.
   */
  int32_t next();
  inline const capi::ICU4XSentenceBreakIteratorUtf16* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XSentenceBreakIteratorUtf16* AsFFIMut() { return this->inner.get(); }
  inline ICU4XSentenceBreakIteratorUtf16(capi::ICU4XSentenceBreakIteratorUtf16* i) : inner(i) {}
  ICU4XSentenceBreakIteratorUtf16() = default;
  ICU4XSentenceBreakIteratorUtf16(ICU4XSentenceBreakIteratorUtf16&&) noexcept = default;
  ICU4XSentenceBreakIteratorUtf16& operator=(ICU4XSentenceBreakIteratorUtf16&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XSentenceBreakIteratorUtf16, ICU4XSentenceBreakIteratorUtf16Deleter> inner;
};


inline int32_t ICU4XSentenceBreakIteratorUtf16::next() {
  return capi::ICU4XSentenceBreakIteratorUtf16_next(this->inner.get());
}
#endif
