#ifndef ICU4XSentenceBreakIteratorUtf8_HPP
#define ICU4XSentenceBreakIteratorUtf8_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XSentenceBreakIteratorUtf8.h"


/**
 * A destruction policy for using ICU4XSentenceBreakIteratorUtf8 with std::unique_ptr.
 */
struct ICU4XSentenceBreakIteratorUtf8Deleter {
  void operator()(capi::ICU4XSentenceBreakIteratorUtf8* l) const noexcept {
    capi::ICU4XSentenceBreakIteratorUtf8_destroy(l);
  }
};

/**
 * See the [Rust documentation for `SentenceBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html) for more information.
 */
class ICU4XSentenceBreakIteratorUtf8 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next) for more information.
   */
  int32_t next();
  inline const capi::ICU4XSentenceBreakIteratorUtf8* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XSentenceBreakIteratorUtf8* AsFFIMut() { return this->inner.get(); }
  inline ICU4XSentenceBreakIteratorUtf8(capi::ICU4XSentenceBreakIteratorUtf8* i) : inner(i) {}
  ICU4XSentenceBreakIteratorUtf8() = default;
  ICU4XSentenceBreakIteratorUtf8(ICU4XSentenceBreakIteratorUtf8&&) noexcept = default;
  ICU4XSentenceBreakIteratorUtf8& operator=(ICU4XSentenceBreakIteratorUtf8&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XSentenceBreakIteratorUtf8, ICU4XSentenceBreakIteratorUtf8Deleter> inner;
};


inline int32_t ICU4XSentenceBreakIteratorUtf8::next() {
  return capi::ICU4XSentenceBreakIteratorUtf8_next(this->inner.get());
}
#endif
