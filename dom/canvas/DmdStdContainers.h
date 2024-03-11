/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_canvas_DmdStdContainers_h
#define dom_canvas_DmdStdContainers_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/layers/BuildConstants.h"
#include <unordered_map>
#include <unordered_set>

namespace mozilla::webgl {

// -

template <class T>
class dmd_allocator {
 public:
  using value_type = T;

 private:
  size_t mMallocSize = 0;

 public:
  dmd_allocator() = default;

  // -

  template <class U>
  friend class dmd_allocator;

  template <class U>
  explicit dmd_allocator(const dmd_allocator<U>& rhs) {
    if constexpr (kIsDmd) {
      mMallocSize = rhs.mMallocSize;
    }
  }

  // -

  value_type* allocate(const size_t n) {
    const auto p = std::allocator<value_type>{}.allocate(n);
    if constexpr (kIsDmd) {
      mMallocSize += moz_malloc_size_of(p);
    }
    return p;
  }

  void deallocate(value_type* const p, const size_t n) {
    if constexpr (kIsDmd) {
      mMallocSize -= moz_malloc_size_of(p);
    }
    std::allocator<value_type>{}.deallocate(p, n);
  }

  // -

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf) const {
    return mMallocSize;
  }
};

// -

template <class Key, class T, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = dmd_allocator<std::pair<const Key, T>>,
          class _StdT = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>>
class dmd_unordered_map : public _StdT {
 public:
  using StdT = _StdT;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mso) const {
    const auto& a = StdT::get_allocator();
    return a.SizeOfExcludingThis(mso);
  }
};

// -

template <class Key, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = dmd_allocator<Key>,
          class _StdT = std::unordered_set<Key, Hash, KeyEqual, Allocator>>
class dmd_unordered_set : public _StdT {
 public:
  using StdT = _StdT;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mso) const {
    const auto& a = StdT::get_allocator();
    return a.SizeOfExcludingThis(mso);
  }
};

// -

}  // namespace mozilla::webgl

#endif  // dom_canvas_DmdStdContainers_h
