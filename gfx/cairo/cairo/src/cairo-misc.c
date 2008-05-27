/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"


/**
 * cairo_status_to_string:
 * @status: a cairo status
 *
 * Provides a human-readable description of a #cairo_status_t.
 *
 * Returns: a string representation of the status
 */
const char *
cairo_status_to_string (cairo_status_t status)
{
    switch (status) {
    case CAIRO_STATUS_SUCCESS:
	return "success";
    case CAIRO_STATUS_NO_MEMORY:
	return "out of memory";
    case CAIRO_STATUS_INVALID_RESTORE:
	return "cairo_restore without matching cairo_save";
    case CAIRO_STATUS_INVALID_POP_GROUP:
	return "cairo_pop_group without matching cairo_push_group";
    case CAIRO_STATUS_NO_CURRENT_POINT:
	return "no current point defined";
    case CAIRO_STATUS_INVALID_MATRIX:
	return "invalid matrix (not invertible)";
    case CAIRO_STATUS_INVALID_STATUS:
	return "invalid value for an input cairo_status_t";
    case CAIRO_STATUS_NULL_POINTER:
	return "NULL pointer";
    case CAIRO_STATUS_INVALID_STRING:
	return "input string not valid UTF-8";
    case CAIRO_STATUS_INVALID_PATH_DATA:
	return "input path data not valid";
    case CAIRO_STATUS_READ_ERROR:
	return "error while reading from input stream";
    case CAIRO_STATUS_WRITE_ERROR:
	return "error while writing to output stream";
    case CAIRO_STATUS_SURFACE_FINISHED:
	return "the target surface has been finished";
    case CAIRO_STATUS_SURFACE_TYPE_MISMATCH:
	return "the surface type is not appropriate for the operation";
    case CAIRO_STATUS_PATTERN_TYPE_MISMATCH:
	return "the pattern type is not appropriate for the operation";
    case CAIRO_STATUS_INVALID_CONTENT:
	return "invalid value for an input cairo_content_t";
    case CAIRO_STATUS_INVALID_FORMAT:
	return "invalid value for an input cairo_format_t";
    case CAIRO_STATUS_INVALID_VISUAL:
	return "invalid value for an input Visual*";
    case CAIRO_STATUS_FILE_NOT_FOUND:
	return "file not found";
    case CAIRO_STATUS_INVALID_DASH:
	return "invalid value for a dash setting";
    case CAIRO_STATUS_INVALID_DSC_COMMENT:
	return "invalid value for a DSC comment";
    case CAIRO_STATUS_INVALID_INDEX:
	return "invalid index passed to getter";
    case CAIRO_STATUS_CLIP_NOT_REPRESENTABLE:
        return "clip region not representable in desired format";
    case CAIRO_STATUS_TEMP_FILE_ERROR:
	return "error creating or writing to a temporary file";
    case CAIRO_STATUS_INVALID_STRIDE:
	return "invalid value for stride";
    }

    return "<unknown error status>";
}

/**
 * _cairo_operator_bounded_by_mask:
 * @op: a #cairo_operator_t
 *
 * A bounded operator is one where mask pixel
 * of zero results in no effect on the destination image.
 *
 * Unbounded operators often require special handling; if you, for
 * example, draw trapezoids with an unbounded operator, the effect
 * extends past the bounding box of the trapezoids.
 *
 * Return value: %TRUE if the operator is bounded by the mask operand
 **/
cairo_bool_t
_cairo_operator_bounded_by_mask (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_ATOP:
    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;
    }

    ASSERT_NOT_REACHED;
    return FALSE;
}

/**
 * _cairo_operator_bounded_by_source:
 * @op: a #cairo_operator_t
 *
 * A bounded operator is one where source pixels of zero
 * (in all four components, r, g, b and a) effect no change
 * in the resulting destination image.
 *
 * Unbounded operators often require special handling; if you, for
 * example, copy a surface with the SOURCE operator, the effect
 * extends past the bounding box of the source surface.
 *
 * Return value: %TRUE if the operator is bounded by the source operand
 **/
cairo_bool_t
_cairo_operator_bounded_by_source (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_ATOP:
    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    case CAIRO_OPERATOR_CLEAR:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;
    }

    ASSERT_NOT_REACHED;
    return FALSE;
}


void
_cairo_restrict_value (double *value, double min, double max)
{
    if (*value < min)
	*value = min;
    else if (*value > max)
	*value = max;
}

/* This function is identical to the C99 function lround(), except that it
 * performs arithmetic rounding (instead of away-from-zero rounding) and
 * has a valid input range of (INT_MIN, INT_MAX] instead of
 * [INT_MIN, INT_MAX]. It is much faster on both x86 and FPU-less systems
 * than other commonly used methods for rounding (lround, round, rint, lrint
 * or float (d + 0.5)).
 *
 * The reason why this function is much faster on x86 than other
 * methods is due to the fact that it avoids the fldcw instruction.
 * This instruction incurs a large performance penalty on modern Intel
 * processors due to how it prevents efficient instruction pipelining.
 *
 * The reason why this function is much faster on FPU-less systems is for
 * an entirely different reason. All common rounding methods involve multiple
 * floating-point operations. Each one of these operations has to be
 * emulated in software, which adds up to be a large performance penalty.
 * This function doesn't perform any floating-point calculations, and thus
 * avoids this penalty.
  */
int
_cairo_lround (double d)
{
    uint32_t top, shift_amount, output;
    union {
        double d;
        uint64_t ui64;
        uint32_t ui32[2];
    } u;

    u.d = d;

    /* If the integer word order doesn't match the float word order, we swap
     * the words of the input double. This is needed because we will be
     * treating the whole double as a 64-bit unsigned integer. Notice that we
     * use WORDS_BIGENDIAN to detect the integer word order, which isn't
     * exactly correct because WORDS_BIGENDIAN refers to byte order, not word
     * order. Thus, we are making the assumption that the byte order is the
     * same as the integer word order which, on the modern machines that we
     * care about, is OK.
     */
#if ( defined(FLOAT_WORDS_BIGENDIAN) && !defined(WORDS_BIGENDIAN)) || \
    (!defined(FLOAT_WORDS_BIGENDIAN) &&  defined(WORDS_BIGENDIAN))
    {
        uint32_t temp = u.ui32[0];
        u.ui32[0] = u.ui32[1];
        u.ui32[1] = temp;
    }
#endif

#ifdef WORDS_BIGENDIAN
    #define MSW (0) /* Most Significant Word */
    #define LSW (1) /* Least Significant Word */
#else
    #define MSW (1)
    #define LSW (0)
#endif

    /* By shifting the most significant word of the input double to the
     * right 20 places, we get the very "top" of the double where the exponent
     * and sign bit lie.
     */
    top = u.ui32[MSW] >> 20;

    /* Here, we calculate how much we have to shift the mantissa to normalize
     * it to an integer value. We extract the exponent "top" by masking out the
     * sign bit, then we calculate the shift amount by subtracting the exponent
     * from the bias. Notice that the correct bias for 64-bit doubles is
     * actually 1075, but we use 1053 instead for two reasons:
     *
     *  1) To perform rounding later on, we will first need the target
     *     value in a 31.1 fixed-point format. Thus, the bias needs to be one
     *     less: (1075 - 1: 1074).
     *
     *  2) To avoid shifting the mantissa as a full 64-bit integer (which is
     *     costly on certain architectures), we break the shift into two parts.
     *     First, the upper and lower parts of the mantissa are shifted
     *     individually by a constant amount that all valid inputs will require
     *     at the very least. This amount is chosen to be 21, because this will
     *     allow the two parts of the mantissa to later be combined into a
     *     single 32-bit representation, on which the remainder of the shift
     *     will be performed. Thus, we decrease the bias by an additional 21:
     *     (1074 - 21: 1053).
     */
    shift_amount = 1053 - (top & 0x7FF);

    /* We are done with the exponent portion in "top", so here we shift it off
     * the end.
     */
    top >>= 11;

    /* Before we perform any operations on the mantissa, we need to OR in
     * the implicit 1 at the top (see the IEEE-754 spec). We needn't mask
     * off the sign bit nor the exponent bits because these higher bits won't
     * make a bit of difference in the rest of our calculations.
     */
    u.ui32[MSW] |= 0x100000;

    /* If the input double is negative, we have to decrease the mantissa
     * by a hair. This is an important part of performing arithmetic rounding,
     * as negative numbers must round towards positive infinity in the
     * halfwase case of -x.5. Since "top" contains only the sign bit at this
     * point, we can just decrease the mantissa by the value of "top".
     */
    u.ui64 -= top;

    /* By decrementing "top", we create a bitmask with a value of either
     * 0x0 (if the input was negative) or 0xFFFFFFFF (if the input was positive
     * and thus the unsigned subtraction underflowed) that we'll use later.
     */
    top--;

    /* Here, we shift the mantissa by the constant value as described above.
     * We can emulate a 64-bit shift right by 21 through shifting the top 32
     * bits left 11 places and ORing in the bottom 32 bits shifted 21 places
     * to the right. Both parts of the mantissa are now packed into a single
     * 32-bit integer. Although we severely truncate the lower part in the
     * process, we still have enough significant bits to perform the conversion
     * without error (for all valid inputs).
     */
    output = (u.ui32[MSW] << 11) | (u.ui32[LSW] >> 21);

    /* Next, we perform the shift that converts the X.Y fixed-point number
     * currently found in "output" to the desired 31.1 fixed-point format
     * needed for the following rounding step. It is important to consider
     * all possible values for "shift_amount" at this point:
     *
     * - {shift_amount < 0} Since shift_amount is an unsigned integer, it
     *   really can't have a value less than zero. But, if the shift_amount
     *   calculation above caused underflow (which would happen with
     *   input > INT_MAX or input <= INT_MIN) then shift_amount will now be
     *   a very large number, and so this shift will result in complete
     *   garbage. But that's OK, as the input was out of our range, so our
     *   output is undefined.
     *
     * - {shift_amount > 31} If the magnitude of the input was very small
     *   (i.e. |input| << 1.0), shift_amount will have a value greater than
     *   31. Thus, this shift will also result in garbage. After performing
     *   the shift, we will zero-out "output" if this is the case.
     *
     * - {0 <= shift_amount < 32} In this case, the shift will properly convert
     *   the mantissa into a 31.1 fixed-point number.
     */
    output >>= shift_amount;

    /* This is where we perform rounding with the 31.1 fixed-point number.
     * Since what we're after is arithmetic rounding, we simply add the single
     * fractional bit into the integer part of "output", and just keep the
     * integer part.
     */
    output = (output >> 1) + (output & 1);

    /* Here, we zero-out the result if the magnitude if the input was very small
     * (as explained in the section above). Notice that all input out of the
     * valid range is also caught by this condition, which means we produce 0
     * for all invalid input, which is a nice side effect.
     *
     * The most straightforward way to do this would be:
     *
     *      if (shift_amount > 31)
     *          output = 0;
     *
     * But we can use a little trick to avoid the potential branch. The
     * expression (shift_amount > 31) will be either 1 or 0, which when
     * decremented will be either 0x0 or 0xFFFFFFFF (unsigned underflow),
     * which can be used to conditionally mask away all the bits in "output"
     * (in the 0x0 case), effectively zeroing it out. Certain, compilers would
     * have done this for us automatically.
     */
    output &= ((shift_amount > 31) - 1);

    /* If the input double was a negative number, then we have to negate our
     * output. The most straightforward way to do this would be:
     *
     *      if (!top)
     *          output = -output;
     *
     * as "top" at this point is either 0x0 (if the input was negative) or
     * 0xFFFFFFFF (if the input was positive). But, we can use a trick to
     * avoid the branch. Observe that the following snippet of code has the
     * same effect as the reference snippet above:
     *
     *      if (!top)
     *          output = 0 - output;
     *      else
     *          output = output - 0;
     *
     * Armed with the bitmask found in "top", we can condense the two statements
     * into the following:
     *
     *      output = (output & top) - (output & ~top);
     *
     * where, in the case that the input double was negative, "top" will be 0,
     * and the statement will be equivalent to:
     *
     *      output = (0) - (output);
     *
     * and if the input double was positive, "top" will be 0xFFFFFFFF, and the
     * statement will be equivalent to:
     *
     *      output = (output) - (0);
     *
     * Which, as pointed out earlier, is equivalent to the original reference
     * snippet.
     */
    output = (output & top) - (output & ~top);

    return output;
#undef MSW
#undef LSW
}
