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
    static const size_t result = ~((size_t(1) << N) - 1);
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
        NBitMask<BitSize<size_t>::result - CeilingLog2<N>::result>::result;
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
#ifdef DEBUG
    bool &entered;
#endif
  public:
    template <class T>
    ReentrancyGuard(T &obj)
#ifdef DEBUG
      : entered(obj.mEntered)
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
static JS_ALWAYS_INLINE size_t
RoundUpPow2(size_t x)
{
    typedef tl::StaticAssert<tl::IsSameType<size_t,JSUword>::result>::result _;
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
static JS_ALWAYS_INLINE size_t
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

/*
 * Policy that calls JSContext:: memory functions and reports errors to the
 * context.  Since the JSContext* given on construction is stored for the
 * lifetime of the container, this policy may only be used for containers whose
 * lifetime is a shorter than the given JSContext.
 */
class ContextAllocPolicy
{
    JSContext *mCx;

  public:
    ContextAllocPolicy(JSContext *cx) : mCx(cx) {}
    JSContext *context() const { return mCx; }

    void *malloc(size_t bytes) { return mCx->malloc(bytes); }
    void free(void *p) { mCx->free(p); }
    void *realloc(void *p, size_t bytes) { return mCx->realloc(p, bytes); }
    void reportAllocOverflow() const { js_ReportAllocationOverflow(mCx); }
};

/* Policy for using system memory functions and doing no error reporting. */
class SystemAllocPolicy
{
  public:
    void *malloc(size_t bytes) { return ::malloc(bytes); }
    void *realloc(void *p, size_t bytes) { return ::realloc(p, bytes); }
    void free(void *p) { ::free(p); }
    void reportAllocOverflow() const {}
};

} /* namespace js */

#endif /* jstl_h_ */
