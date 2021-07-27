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

#include "wasm/WasmMemory.h"

#include "mozilla/MathAlgorithms.h"
#include "vm/ArrayBufferObject.h"

using mozilla::IsPowerOfTwo;

using namespace js;
using namespace js::wasm;

const char* wasm::ToString(IndexType indexType) {
  switch (indexType) {
    case IndexType::I32:
      return "i32";
    case IndexType::I64:
      return "i64";
    default:
      MOZ_CRASH();
  }
}

bool wasm::ToIndexType(JSContext* cx, HandleValue value, IndexType* indexType) {
  RootedString typeStr(cx, ToString(cx, value));
  if (!typeStr) {
    return false;
  }

  RootedLinearString typeLinearStr(cx, typeStr->ensureLinear(cx));
  if (!typeLinearStr) {
    return false;
  }

  if (StringEqualsLiteral(typeLinearStr, "i32")) {
    *indexType = IndexType::I32;
  } else if (StringEqualsLiteral(typeLinearStr, "i64")) {
    *indexType = IndexType::I64;
  } else {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_STRING_IDX_TYPE);
    return false;
  }
  return true;
}

#ifdef JS_64BIT
#  ifdef ENABLE_WASM_CRANELIFT
// TODO (large ArrayBuffer): Cranelift needs to be updated to use more than the
// low 32 bits of the boundsCheckLimit, so for now we limit its heap size to
// something that satisfies the 32-bit invariants.
//
// The "-2" here accounts for the !huge-memory case in CreateSpecificWasmBuffer,
// which is guarding against an overflow.  Also see
// WasmMemoryObject::boundsCheckLimit() for related assertions.
wasm::Pages wasm::MaxMemoryPages() {
  size_t desired = MaxMemory32LimitField - 2;
  size_t actual = ArrayBufferObject::maxBufferByteLength() / PageSize;
  return wasm::Pages(std::min(desired, actual));
}

size_t wasm::MaxMemoryBoundsCheckLimit() {
  return UINT32_MAX - 2 * PageSize + 1;
}
#  else
wasm::Pages wasm::MaxMemoryPages() {
  size_t desired = MaxMemory32LimitField;
  size_t actual = ArrayBufferObject::maxBufferByteLength() / PageSize;
  return wasm::Pages(std::min(desired, actual));
}

size_t wasm::MaxMemoryBoundsCheckLimit() { return size_t(UINT32_MAX) + 1; }
#  endif
#else
wasm::Pages wasm::MaxMemoryPages() {
  MOZ_ASSERT(ArrayBufferObject::maxBufferByteLength() >= INT32_MAX / PageSize);
  return wasm::Pages(INT32_MAX / PageSize);
}

size_t wasm::MaxMemoryBoundsCheckLimit() { return size_t(INT32_MAX) + 1; }
#endif

// Because ARM has a fixed-width instruction encoding, ARM can only express a
// limited subset of immediates (in a single instruction).

static const uint64_t HighestValidARMImmediate = 0xff000000;

//  Heap length on ARM should fit in an ARM immediate. We approximate the set
//  of valid ARM immediates with the predicate:
//    2^n for n in [16, 24)
//  or
//    2^24 * n for n >= 1.
bool wasm::IsValidARMImmediate(uint32_t i) {
  bool valid = (IsPowerOfTwo(i) || (i & 0x00ffffff) == 0);

  MOZ_ASSERT_IF(valid, i % PageSize == 0);

  return valid;
}

uint64_t wasm::RoundUpToNextValidARMImmediate(uint64_t i) {
  MOZ_ASSERT(i <= HighestValidARMImmediate);
  static_assert(HighestValidARMImmediate == 0xff000000,
                "algorithm relies on specific constant");

  if (i <= 16 * 1024 * 1024) {
    i = i ? mozilla::RoundUpPow2(i) : 0;
  } else {
    i = (i + 0x00ffffff) & ~0x00ffffff;
  }

  MOZ_ASSERT(IsValidARMImmediate(i));

  return i;
}

Pages wasm::ClampedMaxPages(Pages initialPages,
                            const Maybe<Pages>& sourceMaxPages,
                            bool useHugeMemory) {
  Pages clampedMaxPages;

  if (sourceMaxPages.isSome()) {
    // There is a specified maximum, clamp it to the implementation limit of
    // maximum pages
    clampedMaxPages = std::min(*sourceMaxPages, wasm::MaxMemoryPages());

#if defined(JS_64BIT) && defined(ENABLE_WASM_CRANELIFT)
    // On 64-bit platforms when we aren't using huge memory and we're using
    // Cranelift, we must satisfy the 32-bit invariants that:
    //    clampedMaxSize + wasm::PageSize < UINT32_MAX
    // This is ensured by clamping sourceMaxPages to wasm::MaxMemoryPages(),
    // which has special logic for cranelift.
    MOZ_ASSERT_IF(!useHugeMemory,
                  clampedMaxPages.byteLength() + wasm::PageSize < UINT32_MAX);
#endif

#ifndef JS_64BIT
    static_assert(sizeof(uintptr_t) == 4, "assuming not 64 bit implies 32 bit");

    // On 32-bit platforms, prevent applications specifying a large max
    // (like MaxMemory32Pages) from unintentially OOMing the browser: they just
    // want "a lot of memory". Maintain the invariant that
    // initialPages <= clampedMaxPages.
    static const uint64_t OneGib = 1 << 30;
    static const Pages OneGibPages = Pages(OneGib >> wasm::PageBits);
    static_assert(HighestValidARMImmediate > OneGib,
                  "computing mapped size on ARM requires clamped max size");

    Pages clampedPages = std::max(OneGibPages, initialPages);
    clampedMaxPages = std::min(clampedPages, clampedMaxPages);
#endif
  } else {
    // There is not a specified maximum, fill it in with the implementation
    // limit of maximum pages
    clampedMaxPages = wasm::MaxMemoryPages();
  }

  // Double-check our invariants
  MOZ_RELEASE_ASSERT(sourceMaxPages.isNothing() ||
                     clampedMaxPages <= *sourceMaxPages);
  MOZ_RELEASE_ASSERT(clampedMaxPages <= wasm::MaxMemoryPages());
  MOZ_RELEASE_ASSERT(initialPages <= clampedMaxPages);

  return clampedMaxPages;
}

size_t wasm::ComputeMappedSize(wasm::Pages clampedMaxPages) {
  // Caller is responsible to ensure that clampedMaxPages has been clamped to
  // implementation limits.
  size_t maxSize = clampedMaxPages.byteLength();

  // It is the bounds-check limit, not the mapped size, that gets baked into
  // code. Thus round up the maxSize to the next valid immediate value
  // *before* adding in the guard page.

  uint64_t boundsCheckLimit = RoundUpToNextValidBoundsCheckImmediate(maxSize);
  MOZ_ASSERT(IsValidBoundsCheckImmediate(boundsCheckLimit));

  MOZ_ASSERT(boundsCheckLimit % gc::SystemPageSize() == 0);
  MOZ_ASSERT(GuardSize % gc::SystemPageSize() == 0);
  return boundsCheckLimit + GuardSize;
}

bool wasm::IsValidBoundsCheckImmediate(uint32_t i) {
#ifdef JS_CODEGEN_ARM
  return IsValidARMImmediate(i);
#else
  return true;
#endif
}

uint64_t wasm::RoundUpToNextValidBoundsCheckImmediate(uint64_t i) {
#ifdef JS_CODEGEN_ARM
  return RoundUpToNextValidARMImmediate(i);
#else
  return i;
#endif
}
