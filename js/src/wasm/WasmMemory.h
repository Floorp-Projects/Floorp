/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2021 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_memory_h
#define wasm_memory_h

#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"

#include <stdint.h>

#include "js/Value.h"
#include "vm/NativeObject.h"
#include "wasm/WasmConstants.h"
#include "wasm/WasmValType.h"

namespace js {
namespace wasm {

// Limits are parameterized by an IndexType which is used to index the
// underlying resource (either a Memory or a Table). Tables are restricted to
// I32, while memories may use I64 when memory64 is enabled.

enum class IndexType : uint8_t { I32, I64 };

inline ValType ToValType(IndexType it) {
  return it == IndexType::I64 ? ValType::I64 : ValType::I32;
}

extern bool ToIndexType(JSContext* cx, HandleValue value, IndexType* indexType);

extern const char* ToString(IndexType indexType);

// Pages is a typed unit representing a multiple of wasm::PageSize. We
// generally use pages as the unit of length when representing linear memory
// lengths so as to avoid overflow when the specified initial or maximum pages
// would overflow the native word size.
//
// Modules may specify pages up to 2^48 inclusive and so Pages is 64-bit on all
// platforms.
//
// We represent byte lengths using the native word size, as it is assumed that
// consumers of this API will only need byte lengths once it is time to
// allocate memory, at which point the pages will be checked against the
// implementation limits `MaxMemoryPages()` and will then be guaranteed to
// fit in a native word.
struct Pages {
 private:
  // Pages are specified by limit fields, which in general may be up to 2^48,
  // so we must use uint64_t here.
  uint64_t value_;

 public:
  constexpr Pages() : value_(0) {}
  constexpr explicit Pages(uint64_t value) : value_(value) {}

  // Get the wrapped page value. Only use this if you must, prefer to use or
  // add new APIs to Page.
  uint64_t value() const { return value_; }

  // Converts from a byte length to pages, assuming that the length is an
  // exact multiple of the page size.
  static Pages fromByteLengthExact(size_t byteLength) {
    MOZ_ASSERT(byteLength % PageSize == 0);
    return Pages(byteLength / PageSize);
  }

  // Return whether the page length may overflow when converted to a byte
  // length in the native word size.
  bool hasByteLength() const {
    mozilla::CheckedInt<size_t> length(value_);
    length *= PageSize;
    return length.isValid();
  }

  // Converts from pages to byte length in the native word size. Users must
  // check for overflow, or be assured else-how that overflow cannot happen.
  size_t byteLength() const {
    mozilla::CheckedInt<size_t> length(value_);
    length *= PageSize;
    return length.value();
  }

  // Increment this pages by delta and return whether the resulting value
  // did not overflow. If there is no overflow, then this is set to the
  // resulting value.
  bool checkedIncrement(Pages delta) {
    mozilla::CheckedInt<uint64_t> newValue = value_;
    newValue += delta.value_;
    if (!newValue.isValid()) {
      return false;
    }
    value_ = newValue.value();
    return true;
  }

  // Implement pass-through comparison operators so that Pages can be compared.

  bool operator==(Pages other) const { return value_ == other.value_; }
  bool operator!=(Pages other) const { return value_ != other.value_; }
  bool operator<=(Pages other) const { return value_ <= other.value_; }
  bool operator<(Pages other) const { return value_ < other.value_; }
  bool operator>=(Pages other) const { return value_ >= other.value_; }
  bool operator>(Pages other) const { return value_ > other.value_; }
};

// The largest number of pages the application can request.
extern Pages MaxMemoryPages(IndexType t);

// The byte value of MaxMemoryPages(t).
static inline size_t MaxMemoryBytes(IndexType t) {
  return MaxMemoryPages(t).byteLength();
}

// A value at least as large as MaxMemoryBytes(t) representing the largest valid
// bounds check limit on the system.  (It can be larger than MaxMemoryBytes()
// because bounds check limits are rounded up to fit formal requirements on some
// platforms.  Also see ComputeMappedSize().)
extern size_t MaxMemoryBoundsCheckLimit(IndexType t);

static inline uint64_t MaxMemoryLimitField(IndexType indexType) {
  return indexType == IndexType::I32 ? MaxMemory32LimitField
                                     : MaxMemory64LimitField;
}

// Compute the 'clamped' maximum size of a memory. See
// 'WASM Linear Memory structure' in ArrayBufferObject.cpp for background.
extern Pages ClampedMaxPages(IndexType t, Pages initialPages,
                             const mozilla::Maybe<Pages>& sourceMaxPages,
                             bool useHugeMemory);

// For a given WebAssembly/asm.js 'clamped' max pages, return the number of
// bytes to map which will necessarily be a multiple of the system page size and
// greater than clampedMaxPages in bytes.  See "Wasm Linear Memory Structure" in
// vm/ArrayBufferObject.cpp.
extern size_t ComputeMappedSize(Pages clampedMaxPages);

extern size_t GetMaxOffsetGuardLimit(bool hugeMemory);

// Return whether the given immediate satisfies the constraints of the platform.
extern bool IsValidBoundsCheckImmediate(uint32_t i);

// Return whether the given immediate is valid on arm.
extern bool IsValidARMImmediate(uint32_t i);

// Return the next higher valid immediate that satisfies the constraints of the
// platform.
extern uint64_t RoundUpToNextValidBoundsCheckImmediate(uint64_t i);

// Return the next higher valid immediate for arm.
extern uint64_t RoundUpToNextValidARMImmediate(uint64_t i);

#ifdef WASM_SUPPORTS_HUGE_MEMORY
// On WASM_SUPPORTS_HUGE_MEMORY platforms, every asm.js or WebAssembly 32-bit
// memory unconditionally allocates a huge region of virtual memory of size
// wasm::HugeMappedSize. This allows all memory resizing to work without
// reallocation and provides enough guard space for all offsets to be folded
// into memory accesses.  See "Linear memory addresses and bounds checking" in
// wasm/WasmMemory.cpp for more information.

static const uint64_t HugeIndexRange = uint64_t(UINT32_MAX) + 1;
static const uint64_t HugeOffsetGuardLimit = uint64_t(INT32_MAX) + 1;
static const uint64_t HugeUnalignedGuardPage = PageSize;
static const uint64_t HugeMappedSize =
    HugeIndexRange + HugeOffsetGuardLimit + HugeUnalignedGuardPage;
#endif

// The size of the guard page for non huge-memories.
static const size_t GuardSize = PageSize;

}  // namespace wasm
}  // namespace js

#endif  // wasm_memory_h
