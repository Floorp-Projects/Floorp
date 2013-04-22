/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NumericConversions_h___
#define NumericConversions_h___

#include "mozilla/FloatingPoint.h"

#include "jscpucfg.h"

#include <math.h>

/* A NaN whose bit pattern conforms to JS::Value's bit pattern restrictions. */
extern double js_NaN;

namespace js {

namespace detail {

union DoublePun {
    struct {
#if defined(IS_LITTLE_ENDIAN)
        uint32_t lo, hi;
#else
        uint32_t hi, lo;
#endif
    } s;
    uint64_t u64;
    double d;
};

} /* namespace detail */

/* Numeric Conversion base. Round doubles to Ints according to ECMA or WEBIDL standards. */
template<size_t width, typename ResultType>
inline ResultType
ToIntWidth(double d)
{
#if defined(__i386__) || defined(__i386) || defined(__x86_64__) || \
    defined(_M_IX86) || defined(_M_X64)
    detail::DoublePun du, duh, twoWidth;
    uint32_t di_h, u_tmp, expon, shift_amount;
    int32_t mask32;

    /*
     * Algorithm Outline
     *  Step 1. If d is NaN, +/-Inf or |d|>=2^(width + 52) or |d|<1, then return 0
     *          All of this is implemented based on an exponent comparison,
     *          since anything with a higher exponent is either not finite, or
     *          going to round to 0..
     *  Step 2. If |d|<2^(width - 1), then return (int)d
     *          The cast to integer (conversion in RZ mode) returns the correct result.
     *  Step 3. If |d|>=2^width, d:=fmod(d, 2^width) is taken  -- but without a call
     *  Step 4. If |d|>=2^(width - 1), then the fractional bits are cleared before
     *          applying the correction by 2^width:  d - sign(d)*2^width
     *  Step 5. Return (int)d
     */

    du.d = d;
    di_h = du.s.hi;

    u_tmp = (di_h & 0x7ff00000) - 0x3ff00000;
    if (u_tmp >= ((width + 52) << 20)) {
        // d is Nan, +/-Inf or +/-0, or |d|>=2^(width+52) or |d|<1, in which case result=0
        // If we need to shift by more than (width + 52), there are no data bits
        // to preserve, and the mod will turn out 0.
        return 0;
    }

    if (u_tmp < ((width - 1) << 20)) {
        // |d|<2^(width - 1)
        return ResultType(d);
    }

    if (u_tmp > ((width - 1) << 20)) {
        // |d|>=2^width
        // Throw away multiples of 2^width.
        //
        // That is, compute du.d = the value in (-2^width, 2^width)
        // that has the same sign as d and is equal to d modulo 2^width.
        //
        // This can't be done simply by masking away bits of du because
        // the implicit one-bit of the mantissa is one of the ones we want to
        // eliminate. So instead we compute duh.d = the appropriate multiple
        // of 2^width, which *can* be computed by masking, and then we
        // subtract that from du.d.
        expon = u_tmp >> 20;
        shift_amount = expon - (width - 11);
        mask32 = 0x80000000;
        if (shift_amount < 32) {
            // Shift only affects top word.
            mask32 >>= shift_amount;
            duh.s.hi = du.s.hi & mask32;
            duh.s.lo = 0;
        } else {
            // Top word all 1s, shift affects bottom word.
            mask32 >>= (shift_amount-32);
            duh.s.hi = du.s.hi;
            duh.s.lo = du.s.lo & mask32;
        }
        du.d -= duh.d;
    }

    di_h = du.s.hi;

    // Eliminate fractional bits
    u_tmp = (di_h & 0x7ff00000);
    if (u_tmp >= (0x3ff00000 + ((width - 1) << 20))) {
        // |d|>=2^(width - 1)
        expon = u_tmp >> 20;

        // Same idea as before, except save everything non-fractional.
        shift_amount = expon - (0x3ff - 11);
        mask32 = 0x80000000;
        if (shift_amount < 32) {
            // Top word only
            mask32 >>= shift_amount;
            du.s.hi &= mask32;
            du.s.lo = 0;
        } else {
            // Bottom word. Top word all 1s.
            mask32 >>= (shift_amount-32);
            du.s.lo &= mask32;
        }
        // Apply step 4's 2^width correction.
        twoWidth.s.hi = (0x3ff00000 + (width << 20)) ^ (du.s.hi & 0x80000000);
        twoWidth.s.lo = 0;
        du.d -= twoWidth.d;
    }

    return ResultType(du.d);
#else
    double twoWidth, twoWidthMin1;

    if (!MOZ_DOUBLE_IS_FINITE(d))
        return 0;

    /* FIXME: This relies on undefined behavior; see bug 667739. */
    ResultType i = (ResultType) d;
    if ((double) i == d)
        return ResultType(i);

    twoWidth = width == 32 ? 4294967296.0 : 18446744073709551616.0;
    twoWidthMin1 = width == 32 ? 2147483648.0 : 9223372036854775808.0;
    d = fmod(d, twoWidth);
    d = (d >= 0) ? floor(d) : ceil(d) + twoWidth;
    return (ResultType) (d >= twoWidthMin1 ? d - twoWidth : d);
#endif
}

/* ES5 9.5 ToInt32 (specialized for doubles). */
inline int32_t
ToInt32(double d)
{
#if defined (__arm__) && defined (__GNUC__)
    int32_t i;
    uint32_t    tmp0;
    uint32_t    tmp1;
    uint32_t    tmp2;
    asm (
    // We use a pure integer solution here. In the 'softfp' ABI, the argument
    // will start in r0 and r1, and VFP can't do all of the necessary ECMA
    // conversions by itself so some integer code will be required anyway. A
    // hybrid solution is faster on A9, but this pure integer solution is
    // notably faster for A8.

    // %0 is the result register, and may alias either of the %[QR]1 registers.
    // %Q4 holds the lower part of the mantissa.
    // %R4 holds the sign, exponent, and the upper part of the mantissa.
    // %1, %2 and %3 are used as temporary values.

    // Extract the exponent.
"   mov     %1, %R4, LSR #20\n"
"   bic     %1, %1, #(1 << 11)\n"  // Clear the sign.

    // Set the implicit top bit of the mantissa. This clobbers a bit of the
    // exponent, but we have already extracted that.
"   orr     %R4, %R4, #(1 << 20)\n"

    // Special Cases
    //   We should return zero in the following special cases:
    //    - Exponent is 0x000 - 1023: +/-0 or subnormal.
    //    - Exponent is 0x7ff - 1023: +/-INFINITY or NaN
    //      - This case is implicitly handled by the standard code path anyway,
    //        as shifting the mantissa up by the exponent will result in '0'.
    //
    // The result is composed of the mantissa, prepended with '1' and
    // bit-shifted left by the (decoded) exponent. Note that because the r1[20]
    // is the bit with value '1', r1 is effectively already shifted (left) by
    // 20 bits, and r0 is already shifted by 52 bits.

    // Adjust the exponent to remove the encoding offset. If the decoded
    // exponent is negative, quickly bail out with '0' as such values round to
    // zero anyway. This also catches +/-0 and subnormals.
"   sub     %1, %1, #0xff\n"
"   subs    %1, %1, #0x300\n"
"   bmi     8f\n"

    //  %1 = (decoded) exponent >= 0
    //  %R4 = upper mantissa and sign

    // ---- Lower Mantissa ----
"   subs    %3, %1, #52\n"         // Calculate exp-52
"   bmi     1f\n"

    // Shift r0 left by exp-52.
    // Ensure that we don't overflow ARM's 8-bit shift operand range.
    // We need to handle anything up to an 11-bit value here as we know that
    // 52 <= exp <= 1024 (0x400). Any shift beyond 31 bits results in zero
    // anyway, so as long as we don't touch the bottom 5 bits, we can use
    // a logical OR to push long shifts into the 32 <= (exp&0xff) <= 255 range.
"   bic     %2, %3, #0xff\n"
"   orr     %3, %3, %2, LSR #3\n"
    // We can now perform a straight shift, avoiding the need for any
    // conditional instructions or extra branches.
"   mov     %Q4, %Q4, LSL %3\n"
"   b       2f\n"
"1:\n" // Shift r0 right by 52-exp.
    // We know that 0 <= exp < 52, and we can shift up to 255 bits so 52-exp
    // will always be a valid shift and we can sk%3 the range check for this case.
"   rsb     %3, %1, #52\n"
"   mov     %Q4, %Q4, LSR %3\n"

    //  %1 = (decoded) exponent
    //  %R4 = upper mantissa and sign
    //  %Q4 = partially-converted integer

"2:\n"
    // ---- Upper Mantissa ----
    // This is much the same as the lower mantissa, with a few different
    // boundary checks and some masking to hide the exponent & sign bit in the
    // upper word.
    // Note that the upper mantissa is pre-shifted by 20 in %R4, but we shift
    // it left more to remove the sign and exponent so it is effectively
    // pre-shifted by 31 bits.
"   subs    %3, %1, #31\n"          // Calculate exp-31
"   mov     %1, %R4, LSL #11\n"     // Re-use %1 as a temporary register.
"   bmi     3f\n"

    // Shift %R4 left by exp-31.
    // Avoid overflowing the 8-bit shift range, as before.
"   bic     %2, %3, #0xff\n"
"   orr     %3, %3, %2, LSR #3\n"
    // Perform the shift.
"   mov     %2, %1, LSL %3\n"
"   b       4f\n"
"3:\n" // Shift r1 right by 31-exp.
    // We know that 0 <= exp < 31, and we can shift up to 255 bits so 31-exp
    // will always be a valid shift and we can skip the range check for this case.
"   rsb     %3, %3, #0\n"          // Calculate 31-exp from -(exp-31)
"   mov     %2, %1, LSR %3\n"      // Thumb-2 can't do "LSR %3" in "orr".

    //  %Q4 = partially-converted integer (lower)
    //  %R4 = upper mantissa and sign
    //  %2 = partially-converted integer (upper)

"4:\n"
    // Combine the converted parts.
"   orr     %Q4, %Q4, %2\n"
    // Negate the result if we have to, and move it to %0 in the process. To
    // avoid conditionals, we can do this by inverting on %R4[31], then adding
    // %R4[31]>>31.
"   eor     %Q4, %Q4, %R4, ASR #31\n"
"   add     %0, %Q4, %R4, LSR #31\n"
"   b       9f\n"
"8:\n"
    // +/-INFINITY, +/-0, subnormals, NaNs, and anything else out-of-range that
    // will result in a conversion of '0'.
"   mov     %0, #0\n"
"9:\n"
    : "=r" (i), "=&r" (tmp0), "=&r" (tmp1), "=&r" (tmp2), "=&r" (d)
    : "4" (d)
    : "cc"
        );
    return i;
#else
    return ToIntWidth<32, int32_t>(d);
#endif
}

/* ES5 9.6 (specialized for doubles). */
inline uint32_t
ToUint32(double d)
{
    return uint32_t(ToInt32(d));
}

/* WEBIDL 4.2.10 */
inline int64_t
ToInt64(double d)
{
    return ToIntWidth<64, int64_t>(d);
}

/* WEBIDL 4.2.11 */
inline uint64_t
ToUint64(double d)
{
    return uint64_t(ToInt64(d));
}

/* ES5 9.4 ToInteger (specialized for doubles). */
inline double
ToInteger(double d)
{
    if (d == 0)
        return d;

    if (!MOZ_DOUBLE_IS_FINITE(d)) {
        if (MOZ_DOUBLE_IS_NaN(d))
            return 0;
        return d;
    }

    bool neg = (d < 0);
    d = floor(neg ? -d : d);
    return neg ? -d : d;
}

} /* namespace js */

#endif /* NumericConversions_h__ */
