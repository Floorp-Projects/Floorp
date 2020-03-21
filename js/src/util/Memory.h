/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef util_Memory_h
#define util_Memory_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

static MOZ_ALWAYS_INLINE void* js_memcpy(void* dst_, const void* src_,
                                         size_t len) {
  char* dst = (char*)dst_;
  const char* src = (const char*)src_;
  MOZ_ASSERT_IF(dst >= src, (size_t)(dst - src) >= len);
  MOZ_ASSERT_IF(src >= dst, (size_t)(src - dst) >= len);

  return memcpy(dst, src, len);
}

namespace js {

template <typename T, typename U>
static constexpr U ComputeByteAlignment(T bytes, U alignment) {
  static_assert(std::is_unsigned_v<U>, "alignment amount must be unsigned");

  return (alignment - (bytes % alignment)) % alignment;
}

template <typename T, typename U>
static constexpr T AlignBytes(T bytes, U alignment) {
  static_assert(std::is_unsigned_v<U>, "alignment amount must be unsigned");

  return bytes + ComputeByteAlignment(bytes, alignment);
}

/*****************************************************************************/

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
static void DefaultInitializeElements(void* arrayPtr, size_t length) {
  uintptr_t elem = reinterpret_cast<uintptr_t>(arrayPtr);
  MOZ_ASSERT(elem % alignof(T) == 0);

  for (size_t i = 0; i < length; ++i) {
    new (reinterpret_cast<void*>(elem)) T;
    elem += sizeof(T);
  }
}

} /* namespace js */

#endif /* util_Memory_h */
