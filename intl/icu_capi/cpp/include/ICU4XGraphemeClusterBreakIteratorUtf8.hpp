#ifndef ICU4XGraphemeClusterBreakIteratorUtf8_HPP
#define ICU4XGraphemeClusterBreakIteratorUtf8_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XGraphemeClusterBreakIteratorUtf8.h"


/**
 * A destruction policy for using ICU4XGraphemeClusterBreakIteratorUtf8 with std::unique_ptr.
 */
struct ICU4XGraphemeClusterBreakIteratorUtf8Deleter {
  void operator()(capi::ICU4XGraphemeClusterBreakIteratorUtf8* l) const noexcept {
    capi::ICU4XGraphemeClusterBreakIteratorUtf8_destroy(l);
  }
};

/**
 * See the [Rust documentation for `GraphemeClusterBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html) for more information.
 */
class ICU4XGraphemeClusterBreakIteratorUtf8 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next) for more information.
   */
  int32_t next();
  inline const capi::ICU4XGraphemeClusterBreakIteratorUtf8* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XGraphemeClusterBreakIteratorUtf8* AsFFIMut() { return this->inner.get(); }
  inline ICU4XGraphemeClusterBreakIteratorUtf8(capi::ICU4XGraphemeClusterBreakIteratorUtf8* i) : inner(i) {}
  ICU4XGraphemeClusterBreakIteratorUtf8() = default;
  ICU4XGraphemeClusterBreakIteratorUtf8(ICU4XGraphemeClusterBreakIteratorUtf8&&) noexcept = default;
  ICU4XGraphemeClusterBreakIteratorUtf8& operator=(ICU4XGraphemeClusterBreakIteratorUtf8&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XGraphemeClusterBreakIteratorUtf8, ICU4XGraphemeClusterBreakIteratorUtf8Deleter> inner;
};


inline int32_t ICU4XGraphemeClusterBreakIteratorUtf8::next() {
  return capi::ICU4XGraphemeClusterBreakIteratorUtf8_next(this->inner.get());
}
#endif
