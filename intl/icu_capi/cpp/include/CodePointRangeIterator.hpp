#ifndef CodePointRangeIterator_HPP
#define CodePointRangeIterator_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "CodePointRangeIterator.h"

struct CodePointRangeIteratorResult;

/**
 * A destruction policy for using CodePointRangeIterator with std::unique_ptr.
 */
struct CodePointRangeIteratorDeleter {
  void operator()(capi::CodePointRangeIterator* l) const noexcept {
    capi::CodePointRangeIterator_destroy(l);
  }
};

/**
 * An iterator over code point ranges, produced by `ICU4XCodePointSetData` or
 * one of the `ICU4XCodePointMapData` types
 */
class CodePointRangeIterator {
 public:

  /**
   * Advance the iterator by one and return the next range.
   * 
   * If the iterator is out of items, `done` will be true
   */
  CodePointRangeIteratorResult next();
  inline const capi::CodePointRangeIterator* AsFFI() const { return this->inner.get(); }
  inline capi::CodePointRangeIterator* AsFFIMut() { return this->inner.get(); }
  inline CodePointRangeIterator(capi::CodePointRangeIterator* i) : inner(i) {}
  CodePointRangeIterator() = default;
  CodePointRangeIterator(CodePointRangeIterator&&) noexcept = default;
  CodePointRangeIterator& operator=(CodePointRangeIterator&& other) noexcept = default;
 private:
  std::unique_ptr<capi::CodePointRangeIterator, CodePointRangeIteratorDeleter> inner;
};

#include "CodePointRangeIteratorResult.hpp"

inline CodePointRangeIteratorResult CodePointRangeIterator::next() {
  capi::CodePointRangeIteratorResult diplomat_raw_struct_out_value = capi::CodePointRangeIterator_next(this->inner.get());
  return CodePointRangeIteratorResult{ .start = std::move(diplomat_raw_struct_out_value.start), .end = std::move(diplomat_raw_struct_out_value.end), .done = std::move(diplomat_raw_struct_out_value.done) };
}
#endif
