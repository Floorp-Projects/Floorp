/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * PR assertion checker.
 */

#ifndef jsutil_h
#define jsutil_h

#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/PodOperations.h"

#include <limits.h>

#include "js/Initialization.h"
#include "js/Utility.h"
#include "js/Value.h"

#if defined(JS_DEBUG)
# define JS_DIAGNOSTICS_ASSERT(expr) MOZ_ASSERT(expr)
#elif defined(JS_CRASH_DIAGNOSTICS)
# define JS_DIAGNOSTICS_ASSERT(expr) do { if (MOZ_UNLIKELY(!(expr))) MOZ_CRASH(); } while(0)
#else
# define JS_DIAGNOSTICS_ASSERT(expr) ((void) 0)
#endif

static MOZ_ALWAYS_INLINE void*
js_memcpy(void* dst_, const void* src_, size_t len)
{
    char* dst = (char*) dst_;
    const char* src = (const char*) src_;
    MOZ_ASSERT_IF(dst >= src, (size_t) (dst - src) >= len);
    MOZ_ASSERT_IF(src >= dst, (size_t) (src - dst) >= len);

    return memcpy(dst, src, len);
}

namespace js {

// An internal version of JS_IsInitialized() that returns whether SpiderMonkey
// is currently initialized or is in the process of being initialized.
inline bool
IsInitialized()
{
    using namespace JS::detail;
    return libraryInitState == InitState::Initializing ||
           libraryInitState == InitState::Running;
}

template <class T>
static inline void
Reverse(T* beg, T* end)
{
    while (beg != end) {
        if (--end == beg)
            return;
        T tmp = *beg;
        *beg = *end;
        *end = tmp;
        ++beg;
    }
}

template <class T, class Pred>
static inline T*
RemoveIf(T* begin, T* end, Pred pred)
{
    T* result = begin;
    for (T* p = begin; p != end; p++) {
        if (!pred(*p))
            *result++ = *p;
    }
    return result;
}

template <class Container, class Pred>
static inline size_t
EraseIf(Container& c, Pred pred)
{
    auto newEnd = RemoveIf(c.begin(), c.end(), pred);
    size_t removed = c.end() - newEnd;
    c.shrinkBy(removed);
    return removed;
}

template <class T>
static inline T*
Find(T* beg, T* end, const T& v)
{
    for (T* p = beg; p != end; ++p) {
        if (*p == v)
            return p;
    }
    return end;
}

template <class Container>
static inline typename Container::ElementType*
Find(Container& c, const typename Container::ElementType& v)
{
    return Find(c.begin(), c.end(), v);
}

template <typename InputIterT, typename CallableT>
void
ForEach(InputIterT begin, InputIterT end, CallableT f)
{
    for (; begin != end; ++begin)
        f(*begin);
}

template <class Container1, class Container2>
static inline bool
EqualContainers(const Container1& lhs, const Container2& rhs)
{
    if (lhs.length() != rhs.length())
        return false;
    for (size_t i = 0, n = lhs.length(); i < n; i++) {
        if (lhs[i] != rhs[i])
            return false;
    }
    return true;
}

template <class Container>
static inline HashNumber
AddContainerToHash(const Container& c, HashNumber hn = 0)
{
    for (size_t i = 0; i < c.length(); i++)
        hn = mozilla::AddToHash(hn, HashNumber(c[i]));
    return hn;
}

template <class T>
static inline T
Min(T t1, T t2)
{
    return t1 < t2 ? t1 : t2;
}

template <class T>
static inline T
Max(T t1, T t2)
{
    return t1 > t2 ? t1 : t2;
}

template <typename T, typename U>
static inline U
ComputeByteAlignment(T bytes, U alignment)
{
    static_assert(mozilla::IsUnsigned<U>::value,
                  "alignment amount must be unsigned");

    MOZ_ASSERT(mozilla::IsPowerOfTwo(alignment));
    return (alignment - (bytes % alignment)) % alignment;
}

template <typename T, typename U>
static inline T
AlignBytes(T bytes, U alignment)
{
    static_assert(mozilla::IsUnsigned<U>::value,
                  "alignment amount must be unsigned");

    return bytes + ComputeByteAlignment(bytes, alignment);
}

/*****************************************************************************/

/* A bit array is an array of bits represented by an array of words (size_t). */

static const size_t BitArrayElementBits = sizeof(size_t) * CHAR_BIT;

static inline unsigned
NumWordsForBitArrayOfLength(size_t length)
{
    return (length + (BitArrayElementBits - 1)) / BitArrayElementBits;
}

static inline unsigned
BitArrayIndexToWordIndex(size_t length, size_t bitIndex)
{
    unsigned wordIndex = bitIndex / BitArrayElementBits;
    MOZ_ASSERT(wordIndex < length);
    return wordIndex;
}

static inline size_t
BitArrayIndexToWordMask(size_t i)
{
    return size_t(1) << (i % BitArrayElementBits);
}

static inline bool
IsBitArrayElementSet(const size_t* array, size_t length, size_t i)
{
    return array[BitArrayIndexToWordIndex(length, i)] & BitArrayIndexToWordMask(i);
}

static inline bool
IsAnyBitArrayElementSet(const size_t* array, size_t length)
{
    unsigned numWords = NumWordsForBitArrayOfLength(length);
    for (unsigned i = 0; i < numWords; ++i) {
        if (array[i])
            return true;
    }
    return false;
}

static inline void
SetBitArrayElement(size_t* array, size_t length, size_t i)
{
    array[BitArrayIndexToWordIndex(length, i)] |= BitArrayIndexToWordMask(i);
}

static inline void
ClearBitArrayElement(size_t* array, size_t length, size_t i)
{
    array[BitArrayIndexToWordIndex(length, i)] &= ~BitArrayIndexToWordMask(i);
}

static inline void
ClearAllBitArrayElements(size_t* array, size_t length)
{
    for (unsigned i = 0; i < length; ++i)
        array[i] = 0;
}

}  /* namespace js */

namespace mozilla {

/**
 * Set the first |aNElem| T elements in |aDst| to |aSrc|.
 */
template<typename T>
static MOZ_ALWAYS_INLINE void
PodSet(T* aDst, const T& aSrc, size_t aNElem)
{
    for (const T* dstend = aDst + aNElem; aDst < dstend; ++aDst)
        *aDst = aSrc;
}

} /* namespace mozilla */

/*
 * Patterns used by SpiderMonkey to overwrite unused memory. If you are
 * accessing an object with one of these pattern, you probably have a dangling
 * pointer. These values should be odd, see the comment in IsThingPoisoned.
 *
 * Note: new patterns should also be added to the array in IsThingPoisoned!
 */
const uint8_t JS_FRESH_NURSERY_PATTERN     = 0x2F;
const uint8_t JS_SWEPT_NURSERY_PATTERN     = 0x2B;
const uint8_t JS_ALLOCATED_NURSERY_PATTERN = 0x2D;
const uint8_t JS_FRESH_TENURED_PATTERN     = 0x4F;
const uint8_t JS_MOVED_TENURED_PATTERN     = 0x49;
const uint8_t JS_SWEPT_TENURED_PATTERN     = 0x4B;
const uint8_t JS_ALLOCATED_TENURED_PATTERN = 0x4D;
const uint8_t JS_FREED_HEAP_PTR_PATTERN    = 0x6B;
const uint8_t JS_FREED_CHUNK_PATTERN       = 0x8B;
#define JS_SWEPT_TI_PATTERN 0x6F

/*
 * Ensure JS_SWEPT_CODE_PATTERN is a byte pattern that will crash immediately
 * when executed, so either an undefined instruction or an instruction that's
 * illegal in user mode.
 */
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_NONE)
# define JS_SWEPT_CODE_PATTERN 0xED // IN instruction, crashes in user mode.
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
# define JS_SWEPT_CODE_PATTERN 0xA3 // undefined instruction
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
# define JS_SWEPT_CODE_PATTERN 0x01 // undefined instruction
#else
# error "JS_SWEPT_CODE_PATTERN not defined for this platform"
#endif

enum class MemCheckKind : uint8_t {
    // Marks a region as poisoned. Memory sanitizers like ASan will crash when
    // accessing it (both reads and writes).
    MakeNoAccess,

    // Marks a region as having undefined contents. In ASan builds this just
    // unpoisons the memory. MSan and Valgrind can also use this to find
    // reads of uninitialized memory.
    MakeUndefined,
};

static MOZ_ALWAYS_INLINE void
SetMemCheckKind(void* ptr, size_t bytes, MemCheckKind kind)
{
    switch (kind) {
      case MemCheckKind::MakeUndefined:
        MOZ_MAKE_MEM_UNDEFINED(ptr, bytes);
        return;
      case MemCheckKind::MakeNoAccess:
        MOZ_MAKE_MEM_NOACCESS(ptr, bytes);
        return;
    }
    MOZ_CRASH("Invalid kind");
}

namespace js {

static inline void
AlwaysPoison(void* ptr, uint8_t value, size_t num, MemCheckKind kind)
{
    // Without a valid Value tag, a poisoned Value may look like a valid
    // floating point number. To ensure that we crash more readily when
    // observing a poisoned Value, we make the poison an invalid ObjectValue.
    // Unfortunately, this adds about 2% more overhead, so we can only enable
    // it in debug.
#if defined(DEBUG)
    uintptr_t poison;
    memset(&poison, value, sizeof(poison));
# if defined(JS_PUNBOX64)
    poison = poison & ((uintptr_t(1) << JSVAL_TAG_SHIFT) - 1);
# endif
    JS::Value v = js::PoisonedObjectValue(poison);

    size_t value_count = num / sizeof(v);
    size_t byte_count = num % sizeof(v);
    mozilla::PodSet(reinterpret_cast<JS::Value*>(ptr), v, value_count);
    if (byte_count) {
        uint8_t* bytes = static_cast<uint8_t*>(ptr);
        uint8_t* end = bytes + num;
        mozilla::PodSet(end - byte_count, value, byte_count);
    }
#else // !DEBUG
    memset(ptr, value, num);
#endif // !DEBUG

    SetMemCheckKind(ptr, num, kind);
}

static inline void
Poison(void* ptr, uint8_t value, size_t num, MemCheckKind kind)
{
    static bool disablePoison = bool(getenv("JSGC_DISABLE_POISONING"));
    if (!disablePoison)
        AlwaysPoison(ptr, value, num, kind);
}

} // namespace js

/* Crash diagnostics by default in debug and on nightly channel. */
#if defined(DEBUG) || defined(NIGHTLY_BUILD)
# define JS_CRASH_DIAGNOSTICS 1
#endif

/* Enable poisoning in crash-diagnostics and zeal builds. */
#if defined(JS_CRASH_DIAGNOSTICS) || defined(JS_GC_ZEAL)
# define JS_POISON(p, val, size, kind) js::Poison(p, val, size, kind)
# define JS_GC_POISONING 1
#else
# define JS_POISON(p, val, size, kind) ((void) 0)
#endif

/* Enable even more poisoning in purely debug builds. */
#if defined(DEBUG)
# define JS_EXTRA_POISON(p, val, size, kind) js::Poison(p, val, size, kind)
#else
# define JS_EXTRA_POISON(p, val, size, kind) ((void) 0)
#endif

#endif /* jsutil_h */
