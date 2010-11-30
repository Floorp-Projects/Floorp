/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * July 16, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Luke Wagner <lw@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jstl_h_
#define jstl_h_

#include "jsbit.h"
#include "jsstaticcheck.h"

#include <new>
#include <string.h>

/* Gross special case for Gecko, which defines malloc/calloc/free. */
#ifdef mozilla_mozalloc_macro_wrappers_h
#  define JSSTL_UNDEFD_MOZALLOC_WRAPPERS
/* The "anti-header" */
#  include "mozilla/mozalloc_undef_macro_wrappers.h"
#endif

namespace js {

/* JavaScript Template Library. */
namespace tl {

/* Compute min/max/clamp. */
template <size_t i, size_t j> struct Min {
    static const size_t result = i < j ? i : j;
};
template <size_t i, size_t j> struct Max {
    static const size_t result = i > j ? i : j;
};
template <size_t i, size_t min, size_t max> struct Clamp {
    static const size_t result = i < min ? min : (i > max ? max : i);
};

/* Compute x^y. */
template <size_t x, size_t y> struct Pow {
    static const size_t result = x * Pow<x, y - 1>::result;
};
template <size_t x> struct Pow<x,0> {
    static const size_t result = 1;
};

/* Compute floor(log2(i)). */
template <size_t i> struct FloorLog2 {
    static const size_t result = 1 + FloorLog2<i / 2>::result;
};
template <> struct FloorLog2<0> { /* Error */ };
template <> struct FloorLog2<1> { static const size_t result = 0; };

/* Compute ceiling(log2(i)). */
template <size_t i> struct CeilingLog2 {
    static const size_t result = FloorLog2<2 * i - 1>::result;
};

/* Round up to the nearest power of 2. */
template <size_t i> struct RoundUpPow2 {
    static const size_t result = 1u << CeilingLog2<i>::result;
};
template <> struct RoundUpPow2<0> {
    static const size_t result = 1;
};

/* Compute the number of bits in the given unsigned type. */
template <class T> struct BitSize {
    static const size_t result = sizeof(T) * JS_BITS_PER_BYTE;
};

/* Allow Assertions by only including the 'result' typedef if 'true'. */
template <bool> struct StaticAssert {};
template <> struct StaticAssert<true> { typedef int result; };

/* Boolean test for whether two types are the same. */
template <class T, class U> struct IsSameType {
    static const bool result = false;
};
template <class T> struct IsSameType<T,T> {
    static const bool result = true;
};

/*
 * Produce an N-bit mask, where N <= BitSize<size_t>::result.  Handle the
 * language-undefined edge case when N = BitSize<size_t>::result.
 */
template <size_t N> struct NBitMask {
    typedef typename StaticAssert<N < BitSize<size_t>::result>::result _;
    static const size_t result = (size_t(1) << N) - 1;
};
template <> struct NBitMask<BitSize<size_t>::result> {
    static const size_t result = size_t(-1);
};

/*
 * For the unsigned integral type size_t, compute a mask M for N such that
 * for all X, !(X & M) implies X * N will not overflow (w.r.t size_t)
 */
template <size_t N> struct MulOverflowMask {
    static const size_t result =
        ~NBitMask<BitSize<size_t>::result - CeilingLog2<N>::result>::result;
};
template <> struct MulOverflowMask<0> { /* Error */ };
template <> struct MulOverflowMask<1> { static const size_t result = 0; };

/*
 * Generate a mask for T such that if (X & sUnsafeRangeSizeMask), an X-sized
 * array of T's is big enough to cause a ptrdiff_t overflow when subtracting
 * a pointer to the end of the array from the beginning.
 */
template <class T> struct UnsafeRangeSizeMask {
    /*
     * The '2' factor means the top bit is clear, sizeof(T) converts from
     * units of elements to bytes.
     */
    static const size_t result = MulOverflowMask<2 * sizeof(T)>::result;
};

/* Return T stripped of any const-ness. */
template <class T> struct StripConst          { typedef T result; };
template <class T> struct StripConst<const T> { typedef T result; };

/*
 * Traits class for identifying POD types. Until C++0x, there is no automatic
 * way to detect PODs, so for the moment it is done manually.
 */
template <class T> struct IsPodType           { static const bool result = false; };
template <> struct IsPodType<char>            { static const bool result = true; };
template <> struct IsPodType<signed char>     { static const bool result = true; };
template <> struct IsPodType<unsigned char>   { static const bool result = true; };
template <> struct IsPodType<short>           { static const bool result = true; };
template <> struct IsPodType<unsigned short>  { static const bool result = true; };
template <> struct IsPodType<int>             { static const bool result = true; };
template <> struct IsPodType<unsigned int>    { static const bool result = true; };
template <> struct IsPodType<long>            { static const bool result = true; };
template <> struct IsPodType<unsigned long>   { static const bool result = true; };
template <> struct IsPodType<float>           { static const bool result = true; };
template <> struct IsPodType<double>          { static const bool result = true; };

/* Return the size/end of an array without using macros. */
template <class T, size_t N> inline T *ArraySize(T (&)[N]) { return N; }
template <class T, size_t N> inline T *ArrayEnd(T (&arr)[N]) { return arr + N; }

} /* namespace tl */

/* Useful for implementing containers that assert non-reentrancy */
class ReentrancyGuard
{
    /* ReentrancyGuard is not copyable. */
    ReentrancyGuard(const ReentrancyGuard &);
    void operator=(const ReentrancyGuard &);

#ifdef DEBUG
    bool &entered;
#endif
  public:
    template <class T>
#ifdef DEBUG
    ReentrancyGuard(T &obj)
      : entered(obj.entered)
#else
    ReentrancyGuard(T &/*obj*/)
#endif
    {
#ifdef DEBUG
        JS_ASSERT(!entered);
        entered = true;
#endif
    }
    ~ReentrancyGuard()
    {
#ifdef DEBUG
        entered = false;
#endif
    }
};

/*
 * Round x up to the nearest power of 2.  This function assumes that the most
 * significant bit of x is not set, which would lead to overflow.
 */
STATIC_POSTCONDITION_ASSUME(return >= x)
JS_ALWAYS_INLINE size_t
RoundUpPow2(size_t x)
{
    size_t log2 = JS_CEILING_LOG2W(x);
    JS_ASSERT(log2 < tl::BitSize<size_t>::result);
    size_t result = size_t(1) << log2;
    return result;
}

/*
 * Safely subtract two pointers when it is known that end > begin.  This avoids
 * the common compiler bug that if (size_t(end) - size_t(begin)) has the MSB
 * set, the unsigned subtraction followed by right shift will produce -1, or
 * size_t(-1), instead of the real difference.
 */
template <class T>
JS_ALWAYS_INLINE size_t
PointerRangeSize(T *begin, T *end)
{
    return (size_t(end) - size_t(begin)) / sizeof(T);
}

/*
 * Allocation policies.  These model the concept:
 *  - public copy constructor, assignment, destructor
 *  - void *malloc(size_t)
 *      Responsible for OOM reporting on NULL return value.
 *  - void *realloc(size_t)
 *      Responsible for OOM reporting on NULL return value.
 *  - void free(void *)
 *  - reportAllocOverflow()
 *      Called on overflow before the container returns NULL.
 */

/* Policy for using system memory functions and doing no error reporting. */
class SystemAllocPolicy
{
  public:
    void *malloc(size_t bytes) { return js_malloc(bytes); }
    void *realloc(void *p, size_t bytes) { return js_realloc(p, bytes); }
    void free(void *p) { js_free(p); }
    void reportAllocOverflow() const {}
};

/*
 * This utility pales in comparison to Boost's aligned_storage. The utility
 * simply assumes that JSUint64 is enough alignment for anyone. This may need
 * to be extended one day...
 *
 * As an important side effect, pulling the storage into this template is
 * enough obfuscation to confuse gcc's strict-aliasing analysis into not giving
 * false negatives when we cast from the char buffer to whatever type we've
 * constructed using the bytes.
 */
template <size_t nbytes>
struct AlignedStorage
{
    union U {
        char bytes[nbytes];
        uint64 _;
    } u;

    const void *addr() const { return u.bytes; }
    void *addr() { return u.bytes; }
};

template <class T>
struct AlignedStorage2
{
    union U {
        char bytes[sizeof(T)];
        uint64 _;
    } u;

    const T *addr() const { return (const T *)u.bytes; }
    T *addr() { return (T *)u.bytes; }
};

/*
 * Small utility for lazily constructing objects without using dynamic storage.
 * When a LazilyConstructed<T> is constructed, it is |empty()|, i.e., no value
 * of T has been constructed and no T destructor will be called when the
 * LazilyConstructed<T> is destroyed. Upon calling |construct|, a T object will
 * be constructed with the given arguments and that object will be destroyed
 * when the owning LazilyConstructed<T> is destroyed.
 *
 * N.B. GCC seems to miss some optimizations with LazilyConstructed and may
 * generate extra branches/loads/stores. Use with caution on hot paths.
 */
template <class T>
class LazilyConstructed
{
    AlignedStorage2<T> storage;
    bool constructed;

    T &asT() { return *storage.addr(); }

  public:
    LazilyConstructed() { constructed = false; }
    ~LazilyConstructed() { if (constructed) asT().~T(); }

    bool empty() const { return !constructed; }

    void construct() {
        JS_ASSERT(!constructed);
        new(storage.addr()) T();
        constructed = true;
    }

    template <class T1>
    void construct(const T1 &t1) {
        JS_ASSERT(!constructed);
        new(storage.addr()) T(t1);
        constructed = true;
    }

    template <class T1, class T2>
    void construct(const T1 &t1, const T2 &t2) {
        JS_ASSERT(!constructed);
        new(storage.addr()) T(t1, t2);
        constructed = true;
    }

    template <class T1, class T2, class T3>
    void construct(const T1 &t1, const T2 &t2, const T3 &t3) {
        JS_ASSERT(!constructed);
        new(storage.addr()) T(t1, t2, t3);
        constructed = true;
    }

    template <class T1, class T2, class T3, class T4>
    void construct(const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4) {
        JS_ASSERT(!constructed);
        new(storage.addr()) T(t1, t2, t3, t4);
        constructed = true;
    }

    T *addr() {
        JS_ASSERT(constructed);
        return &asT();
    }

    T &ref() {
        JS_ASSERT(constructed);
        return asT();
    }

    void destroy() {
        ref().~T();
        constructed = false;
    }
};


/*
 * N.B. GCC seems to miss some optimizations with Conditionally and may
 * generate extra branches/loads/stores. Use with caution on hot paths.
 */
template <class T>
class Conditionally {
    LazilyConstructed<T> t;

  public:
    Conditionally(bool b) { if (b) t.construct(); }

    template <class T1>
    Conditionally(bool b, const T1 &t1) { if (b) t.construct(t1); }
};

template <class T>
class AlignedPtrAndFlag
{
    uintptr_t bits;

  public:
    AlignedPtrAndFlag(T *t, bool flag) {
        JS_ASSERT((uintptr_t(t) & 1) == 0);
        bits = uintptr_t(t) | uintptr_t(flag);
    }

    T *ptr() const {
        return (T *)(bits & ~uintptr_t(1));
    }

    bool flag() const {
        return (bits & 1) != 0;
    }

    void setPtr(T *t) {
        JS_ASSERT((uintptr_t(t) & 1) == 0);
        bits = uintptr_t(t) | uintptr_t(flag());
    }

    void setFlag() {
        bits |= 1;
    }

    void unsetFlag() {
        bits &= ~uintptr_t(1);
    }

    void set(T *t, bool flag) {
        JS_ASSERT((uintptr_t(t) & 1) == 0);
        bits = uintptr_t(t) | flag;
    }
};

template <class T>
static inline void
Reverse(T *beg, T *end)
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

template <typename InputIterT, typename CallableT>
void
ForEach(InputIterT begin, InputIterT end, CallableT f)
{
    for (; begin != end; ++begin)
        f(*begin);
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

/* Allows a const variable to be initialized after its declaration. */
template <class T>
static T&
InitConst(const T &t)
{
    return const_cast<T &>(t);
}

} /* namespace js */

#ifdef JSSTL_UNDEFD_MOZALLOC_WRAPPERS
#  include "mozilla/mozalloc_macro_wrappers.h"
#endif

#endif /* jstl_h_ */
