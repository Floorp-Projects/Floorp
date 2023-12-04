#ifndef ICU4XReorderedIndexMap_HPP
#define ICU4XReorderedIndexMap_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XReorderedIndexMap.h"


/**
 * A destruction policy for using ICU4XReorderedIndexMap with std::unique_ptr.
 */
struct ICU4XReorderedIndexMapDeleter {
  void operator()(capi::ICU4XReorderedIndexMap* l) const noexcept {
    capi::ICU4XReorderedIndexMap_destroy(l);
  }
};

/**
 * Thin wrapper around a vector that maps visual indices to source indices
 * 
 * `map[visualIndex] = sourceIndex`
 * 
 * Produced by `reorder_visual()` on [`ICU4XBidi`].
 */
class ICU4XReorderedIndexMap {
 public:

  /**
   * Get this as a slice/array of indices
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  const diplomat::span<const size_t> as_slice() const;

  /**
   * The length of this map
   */
  size_t len() const;

  /**
   * Get element at `index`. Returns 0 when out of bounds
   * (note that 0 is also a valid in-bounds value, please use `len()`
   * to avoid out-of-bounds)
   */
  size_t get(size_t index) const;
  inline const capi::ICU4XReorderedIndexMap* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XReorderedIndexMap* AsFFIMut() { return this->inner.get(); }
  inline ICU4XReorderedIndexMap(capi::ICU4XReorderedIndexMap* i) : inner(i) {}
  ICU4XReorderedIndexMap() = default;
  ICU4XReorderedIndexMap(ICU4XReorderedIndexMap&&) noexcept = default;
  ICU4XReorderedIndexMap& operator=(ICU4XReorderedIndexMap&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XReorderedIndexMap, ICU4XReorderedIndexMapDeleter> inner;
};


inline const diplomat::span<const size_t> ICU4XReorderedIndexMap::as_slice() const {
  capi::DiplomatUsizeView diplomat_slice_raw_out_value = capi::ICU4XReorderedIndexMap_as_slice(this->inner.get());
  diplomat::span<const size_t> slice(diplomat_slice_raw_out_value.data, diplomat_slice_raw_out_value.len);
  return slice;
}
inline size_t ICU4XReorderedIndexMap::len() const {
  return capi::ICU4XReorderedIndexMap_len(this->inner.get());
}
inline size_t ICU4XReorderedIndexMap::get(size_t index) const {
  return capi::ICU4XReorderedIndexMap_get(this->inner.get(), index);
}
#endif
