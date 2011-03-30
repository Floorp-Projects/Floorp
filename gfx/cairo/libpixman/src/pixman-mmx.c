/*
 * Copyright © 2004, 2005 Red Hat, Inc.
 * Copyright © 2004 Nicholas Miell
 * Copyright © 2005 Trolltech AS
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Søren Sandmann (sandmann@redhat.com)
 * Minor Improvements: Nicholas Miell (nmiell@gmail.com)
 * MMX code paths for fbcompose.c by Lars Knoll (lars@trolltech.com)
 *
 * Based on work by Owen Taylor
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USE_MMX

#include <mmintrin.h>
#include "pixman-private.h"
#include "pixman-combine32.h"

#define no_vERBOSE

#ifdef VERBOSE
#define CHECKPOINT() error_f ("at %s %d\n", __FUNCTION__, __LINE__)
#else
#define CHECKPOINT()
#endif

/* Notes about writing mmx code
 *
 * give memory operands as the second operand. If you give it as the
 * first, gcc will first load it into a register, then use that
 * register
 *
 *   ie. use
 *
 *         _mm_mullo_pi16 (x, mmx_constant);
 *
 *   not
 *
 *         _mm_mullo_pi16 (mmx_constant, x);
 *
 * Also try to minimize dependencies. i.e. when you need a value, try
 * to calculate it from a value that was calculated as early as
 * possible.
 */

/* --------------- MMX primitives ------------------------------------- */

#ifdef __GNUC__
typedef uint64_t mmxdatafield;
#else
typedef __m64 mmxdatafield;
/* If __m64 is defined as a struct or union, define M64_MEMBER to be the
   name of the member used to access the data */
# ifdef _MSC_VER
#  define M64_MEMBER m64_u64
# elif defined(__SUNPRO_C)
#  define M64_MEMBER l_
# endif
#endif

typedef struct
{
    mmxdatafield mmx_4x00ff;
    mmxdatafield mmx_4x0080;
    mmxdatafield mmx_565_rgb;
    mmxdatafield mmx_565_unpack_multiplier;
    mmxdatafield mmx_565_r;
    mmxdatafield mmx_565_g;
    mmxdatafield mmx_565_b;
    mmxdatafield mmx_mask_0;
    mmxdatafield mmx_mask_1;
    mmxdatafield mmx_mask_2;
    mmxdatafield mmx_mask_3;
    mmxdatafield mmx_full_alpha;
    mmxdatafield mmx_ffff0000ffff0000;
    mmxdatafield mmx_0000ffff00000000;
    mmxdatafield mmx_000000000000ffff;
} mmx_data_t;

#if defined(_MSC_VER)
# define MMXDATA_INIT(field, val) { val ## UI64 }
#elif defined(M64_MEMBER)       /* __m64 is a struct, not an integral type */
# define MMXDATA_INIT(field, val) field =   { val ## ULL }
#else                           /* __m64 is an integral type */
# define MMXDATA_INIT(field, val) field =   val ## ULL
#endif

static const mmx_data_t c =
{
    MMXDATA_INIT (.mmx_4x00ff,                   0x00ff00ff00ff00ff),
    MMXDATA_INIT (.mmx_4x0080,                   0x0080008000800080),
    MMXDATA_INIT (.mmx_565_rgb,                  0x000001f0003f001f),
    MMXDATA_INIT (.mmx_565_unpack_multiplier,    0x0000008404100840),
    MMXDATA_INIT (.mmx_565_r,                    0x000000f800000000),
    MMXDATA_INIT (.mmx_565_g,                    0x0000000000fc0000),
    MMXDATA_INIT (.mmx_565_b,                    0x00000000000000f8),
    MMXDATA_INIT (.mmx_mask_0,                   0xffffffffffff0000),
    MMXDATA_INIT (.mmx_mask_1,                   0xffffffff0000ffff),
    MMXDATA_INIT (.mmx_mask_2,                   0xffff0000ffffffff),
    MMXDATA_INIT (.mmx_mask_3,                   0x0000ffffffffffff),
    MMXDATA_INIT (.mmx_full_alpha,               0x00ff000000000000),
    MMXDATA_INIT (.mmx_ffff0000ffff0000,         0xffff0000ffff0000),
    MMXDATA_INIT (.mmx_0000ffff00000000,         0x0000ffff00000000),
    MMXDATA_INIT (.mmx_000000000000ffff,         0x000000000000ffff),
};

#ifdef __GNUC__
#    ifdef __ICC
#        define MC(x) to_m64 (c.mmx_ ## x)
#    else
#        define MC(x) ((__m64)c.mmx_ ## x)
#    endif
#else
#    define MC(x) c.mmx_ ## x
#endif

static force_inline __m64
to_m64 (uint64_t x)
{
#ifdef __ICC
    return _mm_cvtsi64_m64 (x);
#elif defined M64_MEMBER        /* __m64 is a struct, not an integral type */
    __m64 res;

    res.M64_MEMBER = x;
    return res;
#else                           /* __m64 is an integral type */
    return (__m64)x;
#endif
}

static force_inline uint64_t
to_uint64 (__m64 x)
{
#ifdef __ICC
    return _mm_cvtm64_si64 (x);
#elif defined M64_MEMBER        /* __m64 is a struct, not an integral type */
    uint64_t res = x.M64_MEMBER;
    return res;
#else                           /* __m64 is an integral type */
    return (uint64_t)x;
#endif
}

static force_inline __m64
shift (__m64 v,
       int   s)
{
    if (s > 0)
	return _mm_slli_si64 (v, s);
    else if (s < 0)
	return _mm_srli_si64 (v, -s);
    else
	return v;
}

static force_inline __m64
negate (__m64 mask)
{
    return _mm_xor_si64 (mask, MC (4x00ff));
}

static force_inline __m64
pix_multiply (__m64 a, __m64 b)
{
    __m64 res;

    res = _mm_mullo_pi16 (a, b);
    res = _mm_adds_pu16 (res, MC (4x0080));
    res = _mm_adds_pu16 (res, _mm_srli_pi16 (res, 8));
    res = _mm_srli_pi16 (res, 8);

    return res;
}

static force_inline __m64
pix_add (__m64 a, __m64 b)
{
    return _mm_adds_pu8 (a, b);
}

static force_inline __m64
expand_alpha (__m64 pixel)
{
    __m64 t1, t2;

    t1 = shift (pixel, -48);
    t2 = shift (t1, 16);
    t1 = _mm_or_si64 (t1, t2);
    t2 = shift (t1, 32);
    t1 = _mm_or_si64 (t1, t2);

    return t1;
}

static force_inline __m64
expand_alpha_rev (__m64 pixel)
{
    __m64 t1, t2;

    /* move alpha to low 16 bits and zero the rest */
    t1 = shift (pixel,  48);
    t1 = shift (t1, -48);

    t2 = shift (t1, 16);
    t1 = _mm_or_si64 (t1, t2);
    t2 = shift (t1, 32);
    t1 = _mm_or_si64 (t1, t2);

    return t1;
}

static force_inline __m64
invert_colors (__m64 pixel)
{
    __m64 x, y, z;

    x = y = z = pixel;

    x = _mm_and_si64 (x, MC (ffff0000ffff0000));
    y = _mm_and_si64 (y, MC (000000000000ffff));
    z = _mm_and_si64 (z, MC (0000ffff00000000));

    y = shift (y, 32);
    z = shift (z, -32);

    x = _mm_or_si64 (x, y);
    x = _mm_or_si64 (x, z);

    return x;
}

static force_inline __m64
over (__m64 src,
      __m64 srca,
      __m64 dest)
{
    return _mm_adds_pu8 (src, pix_multiply (dest, negate (srca)));
}

static force_inline __m64
over_rev_non_pre (__m64 src, __m64 dest)
{
    __m64 srca = expand_alpha (src);
    __m64 srcfaaa = _mm_or_si64 (srca, MC (full_alpha));

    return over (pix_multiply (invert_colors (src), srcfaaa), srca, dest);
}

static force_inline __m64
in (__m64 src, __m64 mask)
{
    return pix_multiply (src, mask);
}

static force_inline __m64
in_over_full_src_alpha (__m64 src, __m64 mask, __m64 dest)
{
    src = _mm_or_si64 (src, MC (full_alpha));

    return over (in (src, mask), mask, dest);
}

#ifndef _MSC_VER
static force_inline __m64
in_over (__m64 src, __m64 srca, __m64 mask, __m64 dest)
{
    return over (in (src, mask), pix_multiply (srca, mask), dest);
}

#else

#define in_over(src, srca, mask, dest)					\
    over (in (src, mask), pix_multiply (srca, mask), dest)

#endif

static force_inline __m64
load8888 (uint32_t v)
{
    return _mm_unpacklo_pi8 (_mm_cvtsi32_si64 (v), _mm_setzero_si64 ());
}

static force_inline __m64
pack8888 (__m64 lo, __m64 hi)
{
    return _mm_packs_pu16 (lo, hi);
}

static force_inline uint32_t
store8888 (__m64 v)
{
    return _mm_cvtsi64_si32 (pack8888 (v, _mm_setzero_si64 ()));
}

/* Expand 16 bits positioned at @pos (0-3) of a mmx register into
 *
 *    00RR00GG00BB
 *
 * --- Expanding 565 in the low word ---
 *
 * m = (m << (32 - 3)) | (m << (16 - 5)) | m;
 * m = m & (01f0003f001f);
 * m = m * (008404100840);
 * m = m >> 8;
 *
 * Note the trick here - the top word is shifted by another nibble to
 * avoid it bumping into the middle word
 */
static force_inline __m64
expand565 (__m64 pixel, int pos)
{
    __m64 p = pixel;
    __m64 t1, t2;

    /* move pixel to low 16 bit and zero the rest */
    p = shift (shift (p, (3 - pos) * 16), -48);

    t1 = shift (p, 36 - 11);
    t2 = shift (p, 16 - 5);

    p = _mm_or_si64 (t1, p);
    p = _mm_or_si64 (t2, p);
    p = _mm_and_si64 (p, MC (565_rgb));

    pixel = _mm_mullo_pi16 (p, MC (565_unpack_multiplier));
    return _mm_srli_pi16 (pixel, 8);
}

static force_inline __m64
expand8888 (__m64 in, int pos)
{
    if (pos == 0)
	return _mm_unpacklo_pi8 (in, _mm_setzero_si64 ());
    else
	return _mm_unpackhi_pi8 (in, _mm_setzero_si64 ());
}

static force_inline __m64
expandx888 (__m64 in, int pos)
{
    return _mm_or_si64 (expand8888 (in, pos), MC (full_alpha));
}

static force_inline __m64
pack_565 (__m64 pixel, __m64 target, int pos)
{
    __m64 p = pixel;
    __m64 t = target;
    __m64 r, g, b;

    r = _mm_and_si64 (p, MC (565_r));
    g = _mm_and_si64 (p, MC (565_g));
    b = _mm_and_si64 (p, MC (565_b));

    r = shift (r, -(32 - 8) + pos * 16);
    g = shift (g, -(16 - 3) + pos * 16);
    b = shift (b, -(0  + 3) + pos * 16);

    if (pos == 0)
	t = _mm_and_si64 (t, MC (mask_0));
    else if (pos == 1)
	t = _mm_and_si64 (t, MC (mask_1));
    else if (pos == 2)
	t = _mm_and_si64 (t, MC (mask_2));
    else if (pos == 3)
	t = _mm_and_si64 (t, MC (mask_3));

    p = _mm_or_si64 (r, t);
    p = _mm_or_si64 (g, p);

    return _mm_or_si64 (b, p);
}

#ifndef _MSC_VER

static force_inline __m64
pix_add_mul (__m64 x, __m64 a, __m64 y, __m64 b)
{
    x = pix_multiply (x, a);
    y = pix_multiply (y, b);

    return pix_add (x, y);
}

#else

#define pix_add_mul(x, a, y, b)	 \
    ( x = pix_multiply (x, a),	 \
      y = pix_multiply (y, a),	 \
      pix_add (x, y) )

#endif

/* --------------- MMX code patch for fbcompose.c --------------------- */

static force_inline uint32_t
combine (const uint32_t *src, const uint32_t *mask)
{
    uint32_t ssrc = *src;

    if (mask)
    {
	__m64 m = load8888 (*mask);
	__m64 s = load8888 (ssrc);

	m = expand_alpha (m);
	s = pix_multiply (s, m);

	ssrc = store8888 (s);
    }

    return ssrc;
}

static void
mmx_combine_over_u (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	uint32_t ssrc = combine (src, mask);
	uint32_t a = ssrc >> 24;

	if (a == 0xff)
	{
	    *dest = ssrc;
	}
	else if (ssrc)
	{
	    __m64 s, sa;
	    s = load8888 (ssrc);
	    sa = expand_alpha (s);
	    *dest = store8888 (over (s, sa, load8888 (*dest)));
	}

	++dest;
	++src;
	if (mask)
	    ++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_over_reverse_u (pixman_implementation_t *imp,
                            pixman_op_t              op,
                            uint32_t *               dest,
                            const uint32_t *         src,
                            const uint32_t *         mask,
                            int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	__m64 d, da;
	uint32_t s = combine (src, mask);

	d = load8888 (*dest);
	da = expand_alpha (d);
	*dest = store8888 (over (d, da, load8888 (s)));

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_in_u (pixman_implementation_t *imp,
                  pixman_op_t              op,
                  uint32_t *               dest,
                  const uint32_t *         src,
                  const uint32_t *         mask,
                  int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	__m64 x, a;

	x = load8888 (combine (src, mask));
	a = load8888 (*dest);
	a = expand_alpha (a);
	x = pix_multiply (x, a);

	*dest = store8888 (x);

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_in_reverse_u (pixman_implementation_t *imp,
                          pixman_op_t              op,
                          uint32_t *               dest,
                          const uint32_t *         src,
                          const uint32_t *         mask,
                          int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	__m64 x, a;

	x = load8888 (*dest);
	a = load8888 (combine (src, mask));
	a = expand_alpha (a);
	x = pix_multiply (x, a);
	*dest = store8888 (x);

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_out_u (pixman_implementation_t *imp,
                   pixman_op_t              op,
                   uint32_t *               dest,
                   const uint32_t *         src,
                   const uint32_t *         mask,
                   int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	__m64 x, a;

	x = load8888 (combine (src, mask));
	a = load8888 (*dest);
	a = expand_alpha (a);
	a = negate (a);
	x = pix_multiply (x, a);
	*dest = store8888 (x);

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_out_reverse_u (pixman_implementation_t *imp,
                           pixman_op_t              op,
                           uint32_t *               dest,
                           const uint32_t *         src,
                           const uint32_t *         mask,
                           int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	__m64 x, a;

	x = load8888 (*dest);
	a = load8888 (combine (src, mask));
	a = expand_alpha (a);
	a = negate (a);
	x = pix_multiply (x, a);

	*dest = store8888 (x);

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_atop_u (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	__m64 s, da, d, sia;

	s = load8888 (combine (src, mask));
	d = load8888 (*dest);
	sia = expand_alpha (s);
	sia = negate (sia);
	da = expand_alpha (d);
	s = pix_add_mul (s, da, d, sia);
	*dest = store8888 (s);

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_atop_reverse_u (pixman_implementation_t *imp,
                            pixman_op_t              op,
                            uint32_t *               dest,
                            const uint32_t *         src,
                            const uint32_t *         mask,
                            int                      width)
{
    const uint32_t *end;

    end = dest + width;

    while (dest < end)
    {
	__m64 s, dia, d, sa;

	s = load8888 (combine (src, mask));
	d = load8888 (*dest);
	sa = expand_alpha (s);
	dia = expand_alpha (d);
	dia = negate (dia);
	s = pix_add_mul (s, dia, d, sa);
	*dest = store8888 (s);

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_xor_u (pixman_implementation_t *imp,
                   pixman_op_t              op,
                   uint32_t *               dest,
                   const uint32_t *         src,
                   const uint32_t *         mask,
                   int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	__m64 s, dia, d, sia;

	s = load8888 (combine (src, mask));
	d = load8888 (*dest);
	sia = expand_alpha (s);
	dia = expand_alpha (d);
	sia = negate (sia);
	dia = negate (dia);
	s = pix_add_mul (s, dia, d, sia);
	*dest = store8888 (s);

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_add_u (pixman_implementation_t *imp,
                   pixman_op_t              op,
                   uint32_t *               dest,
                   const uint32_t *         src,
                   const uint32_t *         mask,
                   int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	__m64 s, d;

	s = load8888 (combine (src, mask));
	d = load8888 (*dest);
	s = pix_add (s, d);
	*dest = store8888 (s);

	++dest;
	++src;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_saturate_u (pixman_implementation_t *imp,
                        pixman_op_t              op,
                        uint32_t *               dest,
                        const uint32_t *         src,
                        const uint32_t *         mask,
                        int                      width)
{
    const uint32_t *end = dest + width;

    while (dest < end)
    {
	uint32_t s = combine (src, mask);
	uint32_t d = *dest;
	__m64 ms = load8888 (s);
	__m64 md = load8888 (d);
	uint32_t sa = s >> 24;
	uint32_t da = ~d >> 24;

	if (sa > da)
	{
	    __m64 msa = load8888 (DIV_UN8 (da, sa) << 24);
	    msa = expand_alpha (msa);
	    ms = pix_multiply (ms, msa);
	}

	md = pix_add (md, ms);
	*dest = store8888 (md);

	++src;
	++dest;
	if (mask)
	    mask++;
    }
    _mm_empty ();
}

static void
mmx_combine_src_ca (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);

	s = pix_multiply (s, a);
	*dest = store8888 (s);

	++src;
	++mask;
	++dest;
    }
    _mm_empty ();
}

static void
mmx_combine_over_ca (pixman_implementation_t *imp,
                     pixman_op_t              op,
                     uint32_t *               dest,
                     const uint32_t *         src,
                     const uint32_t *         mask,
                     int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 sa = expand_alpha (s);

	*dest = store8888 (in_over (s, sa, a, d));

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_over_reverse_ca (pixman_implementation_t *imp,
                             pixman_op_t              op,
                             uint32_t *               dest,
                             const uint32_t *         src,
                             const uint32_t *         mask,
                             int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 da = expand_alpha (d);

	*dest = store8888 (over (d, da, in (s, a)));

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_in_ca (pixman_implementation_t *imp,
                   pixman_op_t              op,
                   uint32_t *               dest,
                   const uint32_t *         src,
                   const uint32_t *         mask,
                   int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 da = expand_alpha (d);

	s = pix_multiply (s, a);
	s = pix_multiply (s, da);
	*dest = store8888 (s);

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_in_reverse_ca (pixman_implementation_t *imp,
                           pixman_op_t              op,
                           uint32_t *               dest,
                           const uint32_t *         src,
                           const uint32_t *         mask,
                           int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 sa = expand_alpha (s);

	a = pix_multiply (a, sa);
	d = pix_multiply (d, a);
	*dest = store8888 (d);

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_out_ca (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 da = expand_alpha (d);

	da = negate (da);
	s = pix_multiply (s, a);
	s = pix_multiply (s, da);
	*dest = store8888 (s);

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_out_reverse_ca (pixman_implementation_t *imp,
                            pixman_op_t              op,
                            uint32_t *               dest,
                            const uint32_t *         src,
                            const uint32_t *         mask,
                            int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 sa = expand_alpha (s);

	a = pix_multiply (a, sa);
	a = negate (a);
	d = pix_multiply (d, a);
	*dest = store8888 (d);

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_atop_ca (pixman_implementation_t *imp,
                     pixman_op_t              op,
                     uint32_t *               dest,
                     const uint32_t *         src,
                     const uint32_t *         mask,
                     int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 da = expand_alpha (d);
	__m64 sa = expand_alpha (s);

	s = pix_multiply (s, a);
	a = pix_multiply (a, sa);
	a = negate (a);
	d = pix_add_mul (d, a, s, da);
	*dest = store8888 (d);

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_atop_reverse_ca (pixman_implementation_t *imp,
                             pixman_op_t              op,
                             uint32_t *               dest,
                             const uint32_t *         src,
                             const uint32_t *         mask,
                             int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 da = expand_alpha (d);
	__m64 sa = expand_alpha (s);

	s = pix_multiply (s, a);
	a = pix_multiply (a, sa);
	da = negate (da);
	d = pix_add_mul (d, a, s, da);
	*dest = store8888 (d);

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_xor_ca (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);
	__m64 da = expand_alpha (d);
	__m64 sa = expand_alpha (s);

	s = pix_multiply (s, a);
	a = pix_multiply (a, sa);
	da = negate (da);
	a = negate (a);
	d = pix_add_mul (d, a, s, da);
	*dest = store8888 (d);

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

static void
mmx_combine_add_ca (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    const uint32_t *end = src + width;

    while (src < end)
    {
	__m64 a = load8888 (*mask);
	__m64 s = load8888 (*src);
	__m64 d = load8888 (*dest);

	s = pix_multiply (s, a);
	d = pix_add (s, d);
	*dest = store8888 (d);

	++src;
	++dest;
	++mask;
    }
    _mm_empty ();
}

/* ------------- MMX code paths called from fbpict.c -------------------- */

static void
mmx_composite_over_n_8888 (pixman_implementation_t *imp,
                           pixman_op_t              op,
                           pixman_image_t *         src_image,
                           pixman_image_t *         mask_image,
                           pixman_image_t *         dst_image,
                           int32_t                  src_x,
                           int32_t                  src_y,
                           int32_t                  mask_x,
                           int32_t                  mask_y,
                           int32_t                  dest_x,
                           int32_t                  dest_y,
                           int32_t                  width,
                           int32_t                  height)
{
    uint32_t src;
    uint32_t    *dst_line, *dst;
    int32_t w;
    int dst_stride;
    __m64 vsrc, vsrca;

    CHECKPOINT ();

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	w = width;

	CHECKPOINT ();

	while (w && (unsigned long)dst & 7)
	{
	    *dst = store8888 (over (vsrc, vsrca, load8888 (*dst)));

	    w--;
	    dst++;
	}

	while (w >= 2)
	{
	    __m64 vdest;
	    __m64 dest0, dest1;

	    vdest = *(__m64 *)dst;

	    dest0 = over (vsrc, vsrca, expand8888 (vdest, 0));
	    dest1 = over (vsrc, vsrca, expand8888 (vdest, 1));

	    *(__m64 *)dst = pack8888 (dest0, dest1);

	    dst += 2;
	    w -= 2;
	}

	CHECKPOINT ();

	while (w)
	{
	    *dst = store8888 (over (vsrc, vsrca, load8888 (*dst)));

	    w--;
	    dst++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_n_0565 (pixman_implementation_t *imp,
                           pixman_op_t              op,
                           pixman_image_t *         src_image,
                           pixman_image_t *         mask_image,
                           pixman_image_t *         dst_image,
                           int32_t                  src_x,
                           int32_t                  src_y,
                           int32_t                  mask_x,
                           int32_t                  mask_y,
                           int32_t                  dest_x,
                           int32_t                  dest_y,
                           int32_t                  width,
                           int32_t                  height)
{
    uint32_t src;
    uint16_t    *dst_line, *dst;
    int32_t w;
    int dst_stride;
    __m64 vsrc, vsrca;

    CHECKPOINT ();

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	w = width;

	CHECKPOINT ();

	while (w && (unsigned long)dst & 7)
	{
	    uint64_t d = *dst;
	    __m64 vdest = expand565 (to_m64 (d), 0);

	    vdest = pack_565 (over (vsrc, vsrca, vdest), vdest, 0);
	    *dst = to_uint64 (vdest);

	    w--;
	    dst++;
	}

	while (w >= 4)
	{
	    __m64 vdest;

	    vdest = *(__m64 *)dst;

	    vdest = pack_565 (over (vsrc, vsrca, expand565 (vdest, 0)), vdest, 0);
	    vdest = pack_565 (over (vsrc, vsrca, expand565 (vdest, 1)), vdest, 1);
	    vdest = pack_565 (over (vsrc, vsrca, expand565 (vdest, 2)), vdest, 2);
	    vdest = pack_565 (over (vsrc, vsrca, expand565 (vdest, 3)), vdest, 3);

	    *(__m64 *)dst = vdest;

	    dst += 4;
	    w -= 4;
	}

	CHECKPOINT ();

	while (w)
	{
	    uint64_t d = *dst;
	    __m64 vdest = expand565 (to_m64 (d), 0);

	    vdest = pack_565 (over (vsrc, vsrca, vdest), vdest, 0);
	    *dst = to_uint64 (vdest);

	    w--;
	    dst++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_n_8888_8888_ca (pixman_implementation_t *imp,
                                   pixman_op_t              op,
                                   pixman_image_t *         src_image,
                                   pixman_image_t *         mask_image,
                                   pixman_image_t *         dst_image,
                                   int32_t                  src_x,
                                   int32_t                  src_y,
                                   int32_t                  mask_x,
                                   int32_t                  mask_y,
                                   int32_t                  dest_x,
                                   int32_t                  dest_y,
                                   int32_t                  width,
                                   int32_t                  height)
{
    uint32_t src, srca;
    uint32_t    *dst_line;
    uint32_t    *mask_line;
    int dst_stride, mask_stride;
    __m64 vsrc, vsrca;

    CHECKPOINT ();

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t, mask_stride, mask_line, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	int twidth = width;
	uint32_t *p = (uint32_t *)mask_line;
	uint32_t *q = (uint32_t *)dst_line;

	while (twidth && (unsigned long)q & 7)
	{
	    uint32_t m = *(uint32_t *)p;

	    if (m)
	    {
		__m64 vdest = load8888 (*q);
		vdest = in_over (vsrc, vsrca, load8888 (m), vdest);
		*q = store8888 (vdest);
	    }

	    twidth--;
	    p++;
	    q++;
	}

	while (twidth >= 2)
	{
	    uint32_t m0, m1;
	    m0 = *p;
	    m1 = *(p + 1);

	    if (m0 | m1)
	    {
		__m64 dest0, dest1;
		__m64 vdest = *(__m64 *)q;

		dest0 = in_over (vsrc, vsrca, load8888 (m0),
		                 expand8888 (vdest, 0));
		dest1 = in_over (vsrc, vsrca, load8888 (m1),
		                 expand8888 (vdest, 1));

		*(__m64 *)q = pack8888 (dest0, dest1);
	    }

	    p += 2;
	    q += 2;
	    twidth -= 2;
	}

	while (twidth)
	{
	    uint32_t m = *(uint32_t *)p;

	    if (m)
	    {
		__m64 vdest = load8888 (*q);
		vdest = in_over (vsrc, vsrca, load8888 (m), vdest);
		*q = store8888 (vdest);
	    }

	    twidth--;
	    p++;
	    q++;
	}

	dst_line += dst_stride;
	mask_line += mask_stride;
    }

    _mm_empty ();
}

static void
mmx_composite_over_8888_n_8888 (pixman_implementation_t *imp,
                                pixman_op_t              op,
                                pixman_image_t *         src_image,
                                pixman_image_t *         mask_image,
                                pixman_image_t *         dst_image,
                                int32_t                  src_x,
                                int32_t                  src_y,
                                int32_t                  mask_x,
                                int32_t                  mask_y,
                                int32_t                  dest_x,
                                int32_t                  dest_y,
                                int32_t                  width,
                                int32_t                  height)
{
    uint32_t    *dst_line, *dst;
    uint32_t    *src_line, *src;
    uint32_t mask;
    __m64 vmask;
    int dst_stride, src_stride;
    int32_t w;
    __m64 srca;

    CHECKPOINT ();

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);

    mask = _pixman_image_get_solid (imp, mask_image, dst_image->bits.format);
    mask &= 0xff000000;
    mask = mask | mask >> 8 | mask >> 16 | mask >> 24;
    vmask = load8888 (mask);
    srca = MC (4x00ff);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (unsigned long)dst & 7)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);

	    *dst = store8888 (in_over (s, expand_alpha (s), vmask, d));

	    w--;
	    dst++;
	    src++;
	}

	while (w >= 2)
	{
	    __m64 vs = *(__m64 *)src;
	    __m64 vd = *(__m64 *)dst;
	    __m64 vsrc0 = expand8888 (vs, 0);
	    __m64 vsrc1 = expand8888 (vs, 1);

	    *(__m64 *)dst = pack8888 (
	        in_over (vsrc0, expand_alpha (vsrc0), vmask, expand8888 (vd, 0)),
	        in_over (vsrc1, expand_alpha (vsrc1), vmask, expand8888 (vd, 1)));

	    w -= 2;
	    dst += 2;
	    src += 2;
	}

	while (w)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);

	    *dst = store8888 (in_over (s, expand_alpha (s), vmask, d));

	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_x888_n_8888 (pixman_implementation_t *imp,
                                pixman_op_t              op,
                                pixman_image_t *         src_image,
                                pixman_image_t *         mask_image,
                                pixman_image_t *         dst_image,
                                int32_t                  src_x,
                                int32_t                  src_y,
                                int32_t                  mask_x,
                                int32_t                  mask_y,
                                int32_t                  dest_x,
                                int32_t                  dest_y,
                                int32_t                  width,
                                int32_t                  height)
{
    uint32_t *dst_line, *dst;
    uint32_t *src_line, *src;
    uint32_t mask;
    __m64 vmask;
    int dst_stride, src_stride;
    int32_t w;
    __m64 srca;

    CHECKPOINT ();

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);
    mask = _pixman_image_get_solid (imp, mask_image, dst_image->bits.format);

    mask &= 0xff000000;
    mask = mask | mask >> 8 | mask >> 16 | mask >> 24;
    vmask = load8888 (mask);
    srca = MC (4x00ff);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (unsigned long)dst & 7)
	{
	    __m64 s = load8888 (*src | 0xff000000);
	    __m64 d = load8888 (*dst);

	    *dst = store8888 (in_over (s, srca, vmask, d));

	    w--;
	    dst++;
	    src++;
	}

	while (w >= 16)
	{
	    __m64 vd0 = *(__m64 *)(dst + 0);
	    __m64 vd1 = *(__m64 *)(dst + 2);
	    __m64 vd2 = *(__m64 *)(dst + 4);
	    __m64 vd3 = *(__m64 *)(dst + 6);
	    __m64 vd4 = *(__m64 *)(dst + 8);
	    __m64 vd5 = *(__m64 *)(dst + 10);
	    __m64 vd6 = *(__m64 *)(dst + 12);
	    __m64 vd7 = *(__m64 *)(dst + 14);

	    __m64 vs0 = *(__m64 *)(src + 0);
	    __m64 vs1 = *(__m64 *)(src + 2);
	    __m64 vs2 = *(__m64 *)(src + 4);
	    __m64 vs3 = *(__m64 *)(src + 6);
	    __m64 vs4 = *(__m64 *)(src + 8);
	    __m64 vs5 = *(__m64 *)(src + 10);
	    __m64 vs6 = *(__m64 *)(src + 12);
	    __m64 vs7 = *(__m64 *)(src + 14);

	    vd0 = pack8888 (
	        in_over (expandx888 (vs0, 0), srca, vmask, expand8888 (vd0, 0)),
	        in_over (expandx888 (vs0, 1), srca, vmask, expand8888 (vd0, 1)));

	    vd1 = pack8888 (
	        in_over (expandx888 (vs1, 0), srca, vmask, expand8888 (vd1, 0)),
	        in_over (expandx888 (vs1, 1), srca, vmask, expand8888 (vd1, 1)));

	    vd2 = pack8888 (
	        in_over (expandx888 (vs2, 0), srca, vmask, expand8888 (vd2, 0)),
	        in_over (expandx888 (vs2, 1), srca, vmask, expand8888 (vd2, 1)));

	    vd3 = pack8888 (
	        in_over (expandx888 (vs3, 0), srca, vmask, expand8888 (vd3, 0)),
	        in_over (expandx888 (vs3, 1), srca, vmask, expand8888 (vd3, 1)));

	    vd4 = pack8888 (
	        in_over (expandx888 (vs4, 0), srca, vmask, expand8888 (vd4, 0)),
	        in_over (expandx888 (vs4, 1), srca, vmask, expand8888 (vd4, 1)));

	    vd5 = pack8888 (
	        in_over (expandx888 (vs5, 0), srca, vmask, expand8888 (vd5, 0)),
	        in_over (expandx888 (vs5, 1), srca, vmask, expand8888 (vd5, 1)));

	    vd6 = pack8888 (
	        in_over (expandx888 (vs6, 0), srca, vmask, expand8888 (vd6, 0)),
	        in_over (expandx888 (vs6, 1), srca, vmask, expand8888 (vd6, 1)));

	    vd7 = pack8888 (
	        in_over (expandx888 (vs7, 0), srca, vmask, expand8888 (vd7, 0)),
	        in_over (expandx888 (vs7, 1), srca, vmask, expand8888 (vd7, 1)));

	    *(__m64 *)(dst + 0) = vd0;
	    *(__m64 *)(dst + 2) = vd1;
	    *(__m64 *)(dst + 4) = vd2;
	    *(__m64 *)(dst + 6) = vd3;
	    *(__m64 *)(dst + 8) = vd4;
	    *(__m64 *)(dst + 10) = vd5;
	    *(__m64 *)(dst + 12) = vd6;
	    *(__m64 *)(dst + 14) = vd7;

	    w -= 16;
	    dst += 16;
	    src += 16;
	}

	while (w)
	{
	    __m64 s = load8888 (*src | 0xff000000);
	    __m64 d = load8888 (*dst);

	    *dst = store8888 (in_over (s, srca, vmask, d));

	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_8888_8888 (pixman_implementation_t *imp,
                              pixman_op_t              op,
                              pixman_image_t *         src_image,
                              pixman_image_t *         mask_image,
                              pixman_image_t *         dst_image,
                              int32_t                  src_x,
                              int32_t                  src_y,
                              int32_t                  mask_x,
                              int32_t                  mask_y,
                              int32_t                  dest_x,
                              int32_t                  dest_y,
                              int32_t                  width,
                              int32_t                  height)
{
    uint32_t *dst_line, *dst;
    uint32_t *src_line, *src;
    uint32_t s;
    int dst_stride, src_stride;
    uint8_t a;
    int32_t w;

    CHECKPOINT ();

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;

	    if (a == 0xff)
	    {
		*dst = s;
	    }
	    else if (s)
	    {
		__m64 ms, sa;
		ms = load8888 (s);
		sa = expand_alpha (ms);
		*dst = store8888 (over (ms, sa, load8888 (*dst)));
	    }

	    dst++;
	}
    }
    _mm_empty ();
}

static void
mmx_composite_over_8888_0565 (pixman_implementation_t *imp,
                              pixman_op_t              op,
                              pixman_image_t *         src_image,
                              pixman_image_t *         mask_image,
                              pixman_image_t *         dst_image,
                              int32_t                  src_x,
                              int32_t                  src_y,
                              int32_t                  mask_x,
                              int32_t                  mask_y,
                              int32_t                  dest_x,
                              int32_t                  dest_y,
                              int32_t                  width,
                              int32_t                  height)
{
    uint16_t    *dst_line, *dst;
    uint32_t    *src_line, *src;
    int dst_stride, src_stride;
    int32_t w;

    CHECKPOINT ();

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);

#if 0
    /* FIXME */
    assert (src_image->drawable == mask_image->drawable);
#endif

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	CHECKPOINT ();

	while (w && (unsigned long)dst & 7)
	{
	    __m64 vsrc = load8888 (*src);
	    uint64_t d = *dst;
	    __m64 vdest = expand565 (to_m64 (d), 0);

	    vdest = pack_565 (
		over (vsrc, expand_alpha (vsrc), vdest), vdest, 0);

	    *dst = to_uint64 (vdest);

	    w--;
	    dst++;
	    src++;
	}

	CHECKPOINT ();

	while (w >= 4)
	{
	    __m64 vsrc0, vsrc1, vsrc2, vsrc3;
	    __m64 vdest;

	    vsrc0 = load8888 (*(src + 0));
	    vsrc1 = load8888 (*(src + 1));
	    vsrc2 = load8888 (*(src + 2));
	    vsrc3 = load8888 (*(src + 3));

	    vdest = *(__m64 *)dst;

	    vdest = pack_565 (over (vsrc0, expand_alpha (vsrc0), expand565 (vdest, 0)), vdest, 0);
	    vdest = pack_565 (over (vsrc1, expand_alpha (vsrc1), expand565 (vdest, 1)), vdest, 1);
	    vdest = pack_565 (over (vsrc2, expand_alpha (vsrc2), expand565 (vdest, 2)), vdest, 2);
	    vdest = pack_565 (over (vsrc3, expand_alpha (vsrc3), expand565 (vdest, 3)), vdest, 3);

	    *(__m64 *)dst = vdest;

	    w -= 4;
	    dst += 4;
	    src += 4;
	}

	CHECKPOINT ();

	while (w)
	{
	    __m64 vsrc = load8888 (*src);
	    uint64_t d = *dst;
	    __m64 vdest = expand565 (to_m64 (d), 0);

	    vdest = pack_565 (over (vsrc, expand_alpha (vsrc), vdest), vdest, 0);

	    *dst = to_uint64 (vdest);

	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_n_8_8888 (pixman_implementation_t *imp,
                             pixman_op_t              op,
                             pixman_image_t *         src_image,
                             pixman_image_t *         mask_image,
                             pixman_image_t *         dst_image,
                             int32_t                  src_x,
                             int32_t                  src_y,
                             int32_t                  mask_x,
                             int32_t                  mask_y,
                             int32_t                  dest_x,
                             int32_t                  dest_y,
                             int32_t                  width,
                             int32_t                  height)
{
    uint32_t src, srca;
    uint32_t *dst_line, *dst;
    uint8_t *mask_line, *mask;
    int dst_stride, mask_stride;
    int32_t w;
    __m64 vsrc, vsrca;
    uint64_t srcsrc;

    CHECKPOINT ();

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    srcsrc = (uint64_t)src << 32 | src;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	CHECKPOINT ();

	while (w && (unsigned long)dst & 7)
	{
	    uint64_t m = *mask;

	    if (m)
	    {
		__m64 vdest = in_over (vsrc, vsrca,
				       expand_alpha_rev (to_m64 (m)),
				       load8888 (*dst));

		*dst = store8888 (vdest);
	    }

	    w--;
	    mask++;
	    dst++;
	}

	CHECKPOINT ();

	while (w >= 2)
	{
	    uint64_t m0, m1;

	    m0 = *mask;
	    m1 = *(mask + 1);

	    if (srca == 0xff && (m0 & m1) == 0xff)
	    {
		*(uint64_t *)dst = srcsrc;
	    }
	    else if (m0 | m1)
	    {
		__m64 vdest;
		__m64 dest0, dest1;

		vdest = *(__m64 *)dst;

		dest0 = in_over (vsrc, vsrca, expand_alpha_rev (to_m64 (m0)),
				 expand8888 (vdest, 0));
		dest1 = in_over (vsrc, vsrca, expand_alpha_rev (to_m64 (m1)),
				 expand8888 (vdest, 1));

		*(__m64 *)dst = pack8888 (dest0, dest1);
	    }

	    mask += 2;
	    dst += 2;
	    w -= 2;
	}

	CHECKPOINT ();

	while (w)
	{
	    uint64_t m = *mask;

	    if (m)
	    {
		__m64 vdest = load8888 (*dst);

		vdest = in_over (
		    vsrc, vsrca, expand_alpha_rev (to_m64 (m)), vdest);
		*dst = store8888 (vdest);
	    }

	    w--;
	    mask++;
	    dst++;
	}
    }

    _mm_empty ();
}

pixman_bool_t
pixman_fill_mmx (uint32_t *bits,
                 int       stride,
                 int       bpp,
                 int       x,
                 int       y,
                 int       width,
                 int       height,
                 uint32_t xor)
{
    uint64_t fill;
    __m64 vfill;
    uint32_t byte_width;
    uint8_t     *byte_line;

#ifdef __GNUC__
    __m64 v1, v2, v3, v4, v5, v6, v7;
#endif

    if (bpp != 16 && bpp != 32 && bpp != 8)
	return FALSE;

    if (bpp == 8)
    {
	stride = stride * (int) sizeof (uint32_t) / 1;
	byte_line = (uint8_t *)(((uint8_t *)bits) + stride * y + x);
	byte_width = width;
	stride *= 1;
        xor = (xor & 0xff) * 0x01010101;
    }
    else if (bpp == 16)
    {
	stride = stride * (int) sizeof (uint32_t) / 2;
	byte_line = (uint8_t *)(((uint16_t *)bits) + stride * y + x);
	byte_width = 2 * width;
	stride *= 2;
        xor = (xor & 0xffff) * 0x00010001;
    }
    else
    {
	stride = stride * (int) sizeof (uint32_t) / 4;
	byte_line = (uint8_t *)(((uint32_t *)bits) + stride * y + x);
	byte_width = 4 * width;
	stride *= 4;
    }

    fill = ((uint64_t)xor << 32) | xor;
    vfill = to_m64 (fill);

#ifdef __GNUC__
    __asm__ (
        "movq		%7,	%0\n"
        "movq		%7,	%1\n"
        "movq		%7,	%2\n"
        "movq		%7,	%3\n"
        "movq		%7,	%4\n"
        "movq		%7,	%5\n"
        "movq		%7,	%6\n"
	: "=&y" (v1), "=&y" (v2), "=&y" (v3),
	  "=&y" (v4), "=&y" (v5), "=&y" (v6), "=y" (v7)
	: "y" (vfill));
#endif

    while (height--)
    {
	int w;
	uint8_t *d = byte_line;

	byte_line += stride;
	w = byte_width;

	while (w >= 1 && ((unsigned long)d & 1))
	{
	    *(uint8_t *)d = (xor & 0xff);
	    w--;
	    d++;
	}

	while (w >= 2 && ((unsigned long)d & 3))
	{
	    *(uint16_t *)d = xor;
	    w -= 2;
	    d += 2;
	}

	while (w >= 4 && ((unsigned long)d & 7))
	{
	    *(uint32_t *)d = xor;

	    w -= 4;
	    d += 4;
	}

	while (w >= 64)
	{
#ifdef __GNUC__
	    __asm__ (
	        "movq	%1,	  (%0)\n"
	        "movq	%2,	 8(%0)\n"
	        "movq	%3,	16(%0)\n"
	        "movq	%4,	24(%0)\n"
	        "movq	%5,	32(%0)\n"
	        "movq	%6,	40(%0)\n"
	        "movq	%7,	48(%0)\n"
	        "movq	%8,	56(%0)\n"
		:
		: "r" (d),
		  "y" (vfill), "y" (v1), "y" (v2), "y" (v3),
		  "y" (v4), "y" (v5), "y" (v6), "y" (v7)
		: "memory");
#else
	    *(__m64*) (d +  0) = vfill;
	    *(__m64*) (d +  8) = vfill;
	    *(__m64*) (d + 16) = vfill;
	    *(__m64*) (d + 24) = vfill;
	    *(__m64*) (d + 32) = vfill;
	    *(__m64*) (d + 40) = vfill;
	    *(__m64*) (d + 48) = vfill;
	    *(__m64*) (d + 56) = vfill;
#endif
	    w -= 64;
	    d += 64;
	}

	while (w >= 4)
	{
	    *(uint32_t *)d = xor;

	    w -= 4;
	    d += 4;
	}
	while (w >= 2)
	{
	    *(uint16_t *)d = xor;
	    w -= 2;
	    d += 2;
	}
	while (w >= 1)
	{
	    *(uint8_t *)d = (xor & 0xff);
	    w--;
	    d++;
	}

    }

    _mm_empty ();
    return TRUE;
}

static void
mmx_composite_src_n_8_8888 (pixman_implementation_t *imp,
                            pixman_op_t              op,
                            pixman_image_t *         src_image,
                            pixman_image_t *         mask_image,
                            pixman_image_t *         dst_image,
                            int32_t                  src_x,
                            int32_t                  src_y,
                            int32_t                  mask_x,
                            int32_t                  mask_y,
                            int32_t                  dest_x,
                            int32_t                  dest_y,
                            int32_t                  width,
                            int32_t                  height)
{
    uint32_t src, srca;
    uint32_t    *dst_line, *dst;
    uint8_t     *mask_line, *mask;
    int dst_stride, mask_stride;
    int32_t w;
    __m64 vsrc, vsrca;
    uint64_t srcsrc;

    CHECKPOINT ();

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
    {
	pixman_fill_mmx (dst_image->bits.bits, dst_image->bits.rowstride,
			 PIXMAN_FORMAT_BPP (dst_image->bits.format),
	                 dest_x, dest_y, width, height, 0);
	return;
    }

    srcsrc = (uint64_t)src << 32 | src;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	CHECKPOINT ();

	while (w && (unsigned long)dst & 7)
	{
	    uint64_t m = *mask;

	    if (m)
	    {
		__m64 vdest = in (vsrc, expand_alpha_rev (to_m64 (m)));

		*dst = store8888 (vdest);
	    }
	    else
	    {
		*dst = 0;
	    }

	    w--;
	    mask++;
	    dst++;
	}

	CHECKPOINT ();

	while (w >= 2)
	{
	    uint64_t m0, m1;
	    m0 = *mask;
	    m1 = *(mask + 1);

	    if (srca == 0xff && (m0 & m1) == 0xff)
	    {
		*(uint64_t *)dst = srcsrc;
	    }
	    else if (m0 | m1)
	    {
		__m64 vdest;
		__m64 dest0, dest1;

		vdest = *(__m64 *)dst;

		dest0 = in (vsrc, expand_alpha_rev (to_m64 (m0)));
		dest1 = in (vsrc, expand_alpha_rev (to_m64 (m1)));

		*(__m64 *)dst = pack8888 (dest0, dest1);
	    }
	    else
	    {
		*(uint64_t *)dst = 0;
	    }

	    mask += 2;
	    dst += 2;
	    w -= 2;
	}

	CHECKPOINT ();

	while (w)
	{
	    uint64_t m = *mask;

	    if (m)
	    {
		__m64 vdest = load8888 (*dst);

		vdest = in (vsrc, expand_alpha_rev (to_m64 (m)));
		*dst = store8888 (vdest);
	    }
	    else
	    {
		*dst = 0;
	    }

	    w--;
	    mask++;
	    dst++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_n_8_0565 (pixman_implementation_t *imp,
                             pixman_op_t              op,
                             pixman_image_t *         src_image,
                             pixman_image_t *         mask_image,
                             pixman_image_t *         dst_image,
                             int32_t                  src_x,
                             int32_t                  src_y,
                             int32_t                  mask_x,
                             int32_t                  mask_y,
                             int32_t                  dest_x,
                             int32_t                  dest_y,
                             int32_t                  width,
                             int32_t                  height)
{
    uint32_t src, srca;
    uint16_t *dst_line, *dst;
    uint8_t *mask_line, *mask;
    int dst_stride, mask_stride;
    int32_t w;
    __m64 vsrc, vsrca, tmp;
    uint64_t srcsrcsrcsrc, src16;

    CHECKPOINT ();

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    tmp = pack_565 (vsrc, _mm_setzero_si64 (), 0);
    src16 = to_uint64 (tmp);

    srcsrcsrcsrc =
	(uint64_t)src16 << 48 | (uint64_t)src16 << 32 |
	(uint64_t)src16 << 16 | (uint64_t)src16;

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	CHECKPOINT ();

	while (w && (unsigned long)dst & 7)
	{
	    uint64_t m = *mask;

	    if (m)
	    {
		uint64_t d = *dst;
		__m64 vd = to_m64 (d);
		__m64 vdest = in_over (
		    vsrc, vsrca, expand_alpha_rev (to_m64 (m)), expand565 (vd, 0));

		vd = pack_565 (vdest, _mm_setzero_si64 (), 0);
		*dst = to_uint64 (vd);
	    }

	    w--;
	    mask++;
	    dst++;
	}

	CHECKPOINT ();

	while (w >= 4)
	{
	    uint64_t m0, m1, m2, m3;
	    m0 = *mask;
	    m1 = *(mask + 1);
	    m2 = *(mask + 2);
	    m3 = *(mask + 3);

	    if (srca == 0xff && (m0 & m1 & m2 & m3) == 0xff)
	    {
		*(uint64_t *)dst = srcsrcsrcsrc;
	    }
	    else if (m0 | m1 | m2 | m3)
	    {
		__m64 vdest;
		__m64 vm0, vm1, vm2, vm3;

		vdest = *(__m64 *)dst;

		vm0 = to_m64 (m0);
		vdest = pack_565 (in_over (vsrc, vsrca, expand_alpha_rev (vm0),
					   expand565 (vdest, 0)), vdest, 0);
		vm1 = to_m64 (m1);
		vdest = pack_565 (in_over (vsrc, vsrca, expand_alpha_rev (vm1),
					   expand565 (vdest, 1)), vdest, 1);
		vm2 = to_m64 (m2);
		vdest = pack_565 (in_over (vsrc, vsrca, expand_alpha_rev (vm2),
					   expand565 (vdest, 2)), vdest, 2);
		vm3 = to_m64 (m3);
		vdest = pack_565 (in_over (vsrc, vsrca, expand_alpha_rev (vm3),
					   expand565 (vdest, 3)), vdest, 3);

		*(__m64 *)dst = vdest;
	    }

	    w -= 4;
	    mask += 4;
	    dst += 4;
	}

	CHECKPOINT ();

	while (w)
	{
	    uint64_t m = *mask;

	    if (m)
	    {
		uint64_t d = *dst;
		__m64 vd = to_m64 (d);
		__m64 vdest = in_over (vsrc, vsrca, expand_alpha_rev (to_m64 (m)),
				       expand565 (vd, 0));
		vd = pack_565 (vdest, _mm_setzero_si64 (), 0);
		*dst = to_uint64 (vd);
	    }

	    w--;
	    mask++;
	    dst++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_pixbuf_0565 (pixman_implementation_t *imp,
                                pixman_op_t              op,
                                pixman_image_t *         src_image,
                                pixman_image_t *         mask_image,
                                pixman_image_t *         dst_image,
                                int32_t                  src_x,
                                int32_t                  src_y,
                                int32_t                  mask_x,
                                int32_t                  mask_y,
                                int32_t                  dest_x,
                                int32_t                  dest_y,
                                int32_t                  width,
                                int32_t                  height)
{
    uint16_t    *dst_line, *dst;
    uint32_t    *src_line, *src;
    int dst_stride, src_stride;
    int32_t w;

    CHECKPOINT ();

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);

#if 0
    /* FIXME */
    assert (src_image->drawable == mask_image->drawable);
#endif

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	CHECKPOINT ();

	while (w && (unsigned long)dst & 7)
	{
	    __m64 vsrc = load8888 (*src);
	    uint64_t d = *dst;
	    __m64 vdest = expand565 (to_m64 (d), 0);

	    vdest = pack_565 (over_rev_non_pre (vsrc, vdest), vdest, 0);

	    *dst = to_uint64 (vdest);

	    w--;
	    dst++;
	    src++;
	}

	CHECKPOINT ();

	while (w >= 4)
	{
	    uint32_t s0, s1, s2, s3;
	    unsigned char a0, a1, a2, a3;

	    s0 = *src;
	    s1 = *(src + 1);
	    s2 = *(src + 2);
	    s3 = *(src + 3);

	    a0 = (s0 >> 24);
	    a1 = (s1 >> 24);
	    a2 = (s2 >> 24);
	    a3 = (s3 >> 24);

	    if ((a0 & a1 & a2 & a3) == 0xFF)
	    {
		__m64 vdest;
		vdest = pack_565 (invert_colors (load8888 (s0)), _mm_setzero_si64 (), 0);
		vdest = pack_565 (invert_colors (load8888 (s1)), vdest, 1);
		vdest = pack_565 (invert_colors (load8888 (s2)), vdest, 2);
		vdest = pack_565 (invert_colors (load8888 (s3)), vdest, 3);

		*(__m64 *)dst = vdest;
	    }
	    else if (s0 | s1 | s2 | s3)
	    {
		__m64 vdest = *(__m64 *)dst;

		vdest = pack_565 (over_rev_non_pre (load8888 (s0), expand565 (vdest, 0)), vdest, 0);
		vdest = pack_565 (over_rev_non_pre (load8888 (s1), expand565 (vdest, 1)), vdest, 1);
		vdest = pack_565 (over_rev_non_pre (load8888 (s2), expand565 (vdest, 2)), vdest, 2);
		vdest = pack_565 (over_rev_non_pre (load8888 (s3), expand565 (vdest, 3)), vdest, 3);

		*(__m64 *)dst = vdest;
	    }

	    w -= 4;
	    dst += 4;
	    src += 4;
	}

	CHECKPOINT ();

	while (w)
	{
	    __m64 vsrc = load8888 (*src);
	    uint64_t d = *dst;
	    __m64 vdest = expand565 (to_m64 (d), 0);

	    vdest = pack_565 (over_rev_non_pre (vsrc, vdest), vdest, 0);

	    *dst = to_uint64 (vdest);

	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_pixbuf_8888 (pixman_implementation_t *imp,
                                pixman_op_t              op,
                                pixman_image_t *         src_image,
                                pixman_image_t *         mask_image,
                                pixman_image_t *         dst_image,
                                int32_t                  src_x,
                                int32_t                  src_y,
                                int32_t                  mask_x,
                                int32_t                  mask_y,
                                int32_t                  dest_x,
                                int32_t                  dest_y,
                                int32_t                  width,
                                int32_t                  height)
{
    uint32_t    *dst_line, *dst;
    uint32_t    *src_line, *src;
    int dst_stride, src_stride;
    int32_t w;

    CHECKPOINT ();

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);

#if 0
    /* FIXME */
    assert (src_image->drawable == mask_image->drawable);
#endif

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (unsigned long)dst & 7)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);

	    *dst = store8888 (over_rev_non_pre (s, d));

	    w--;
	    dst++;
	    src++;
	}

	while (w >= 2)
	{
	    uint64_t s0, s1;
	    unsigned char a0, a1;
	    __m64 d0, d1;

	    s0 = *src;
	    s1 = *(src + 1);

	    a0 = (s0 >> 24);
	    a1 = (s1 >> 24);

	    if ((a0 & a1) == 0xFF)
	    {
		d0 = invert_colors (load8888 (s0));
		d1 = invert_colors (load8888 (s1));

		*(__m64 *)dst = pack8888 (d0, d1);
	    }
	    else if (s0 | s1)
	    {
		__m64 vdest = *(__m64 *)dst;

		d0 = over_rev_non_pre (load8888 (s0), expand8888 (vdest, 0));
		d1 = over_rev_non_pre (load8888 (s1), expand8888 (vdest, 1));

		*(__m64 *)dst = pack8888 (d0, d1);
	    }

	    w -= 2;
	    dst += 2;
	    src += 2;
	}

	while (w)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);

	    *dst = store8888 (over_rev_non_pre (s, d));

	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_over_n_8888_0565_ca (pixman_implementation_t *imp,
                                   pixman_op_t              op,
                                   pixman_image_t *         src_image,
                                   pixman_image_t *         mask_image,
                                   pixman_image_t *         dst_image,
                                   int32_t                  src_x,
                                   int32_t                  src_y,
                                   int32_t                  mask_x,
                                   int32_t                  mask_y,
                                   int32_t                  dest_x,
                                   int32_t                  dest_y,
                                   int32_t                  width,
                                   int32_t                  height)
{
    uint32_t src, srca;
    uint16_t    *dst_line;
    uint32_t    *mask_line;
    int dst_stride, mask_stride;
    __m64 vsrc, vsrca;

    CHECKPOINT ();

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t, mask_stride, mask_line, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	int twidth = width;
	uint32_t *p = (uint32_t *)mask_line;
	uint16_t *q = (uint16_t *)dst_line;

	while (twidth && ((unsigned long)q & 7))
	{
	    uint32_t m = *(uint32_t *)p;

	    if (m)
	    {
		uint64_t d = *q;
		__m64 vdest = expand565 (to_m64 (d), 0);
		vdest = pack_565 (in_over (vsrc, vsrca, load8888 (m), vdest), vdest, 0);
		*q = to_uint64 (vdest);
	    }

	    twidth--;
	    p++;
	    q++;
	}

	while (twidth >= 4)
	{
	    uint32_t m0, m1, m2, m3;

	    m0 = *p;
	    m1 = *(p + 1);
	    m2 = *(p + 2);
	    m3 = *(p + 3);

	    if ((m0 | m1 | m2 | m3))
	    {
		__m64 vdest = *(__m64 *)q;

		vdest = pack_565 (in_over (vsrc, vsrca, load8888 (m0), expand565 (vdest, 0)), vdest, 0);
		vdest = pack_565 (in_over (vsrc, vsrca, load8888 (m1), expand565 (vdest, 1)), vdest, 1);
		vdest = pack_565 (in_over (vsrc, vsrca, load8888 (m2), expand565 (vdest, 2)), vdest, 2);
		vdest = pack_565 (in_over (vsrc, vsrca, load8888 (m3), expand565 (vdest, 3)), vdest, 3);

		*(__m64 *)q = vdest;
	    }
	    twidth -= 4;
	    p += 4;
	    q += 4;
	}

	while (twidth)
	{
	    uint32_t m;

	    m = *(uint32_t *)p;
	    if (m)
	    {
		uint64_t d = *q;
		__m64 vdest = expand565 (to_m64 (d), 0);
		vdest = pack_565 (in_over (vsrc, vsrca, load8888 (m), vdest), vdest, 0);
		*q = to_uint64 (vdest);
	    }

	    twidth--;
	    p++;
	    q++;
	}

	mask_line += mask_stride;
	dst_line += dst_stride;
    }

    _mm_empty ();
}

static void
mmx_composite_in_n_8_8 (pixman_implementation_t *imp,
                        pixman_op_t              op,
                        pixman_image_t *         src_image,
                        pixman_image_t *         mask_image,
                        pixman_image_t *         dst_image,
                        int32_t                  src_x,
                        int32_t                  src_y,
                        int32_t                  mask_x,
                        int32_t                  mask_y,
                        int32_t                  dest_x,
                        int32_t                  dest_y,
                        int32_t                  width,
                        int32_t                  height)
{
    uint8_t *dst_line, *dst;
    uint8_t *mask_line, *mask;
    int dst_stride, mask_stride;
    int32_t w;
    uint32_t src;
    uint8_t sa;
    __m64 vsrc, vsrca;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    sa = src >> 24;

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	if ((((unsigned long)dst_image & 3) == 0) &&
	    (((unsigned long)src_image & 3) == 0))
	{
	    while (w >= 4)
	    {
		uint32_t m;
		__m64 vmask;
		__m64 vdest;

		m = 0;

		vmask = load8888 (*(uint32_t *)mask);
		vdest = load8888 (*(uint32_t *)dst);

		*(uint32_t *)dst = store8888 (in (in (vsrca, vmask), vdest));

		dst += 4;
		mask += 4;
		w -= 4;
	    }
	}

	while (w--)
	{
	    uint16_t tmp;
	    uint8_t a;
	    uint32_t m, d;

	    a = *mask++;
	    d = *dst;

	    m = MUL_UN8 (sa, a, tmp);
	    d = MUL_UN8 (m, d, tmp);

	    *dst++ = d;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_in_8_8 (pixman_implementation_t *imp,
                      pixman_op_t              op,
                      pixman_image_t *         src_image,
                      pixman_image_t *         mask_image,
                      pixman_image_t *         dst_image,
                      int32_t                  src_x,
                      int32_t                  src_y,
                      int32_t                  mask_x,
                      int32_t                  mask_y,
                      int32_t                  dest_x,
                      int32_t                  dest_y,
                      int32_t                  width,
                      int32_t                  height)
{
    uint8_t     *dst_line, *dst;
    uint8_t     *src_line, *src;
    int src_stride, dst_stride;
    int32_t w;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t, src_stride, src_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	if ((((unsigned long)dst_image & 3) == 0) &&
	    (((unsigned long)src_image & 3) == 0))
	{
	    while (w >= 4)
	    {
		uint32_t *s = (uint32_t *)src;
		uint32_t *d = (uint32_t *)dst;

		*d = store8888 (in (load8888 (*s), load8888 (*d)));

		w -= 4;
		dst += 4;
		src += 4;
	    }
	}

	while (w--)
	{
	    uint8_t s, d;
	    uint16_t tmp;

	    s = *src;
	    d = *dst;

	    *dst = MUL_UN8 (s, d, tmp);

	    src++;
	    dst++;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_add_n_8_8 (pixman_implementation_t *imp,
			 pixman_op_t              op,
			 pixman_image_t *         src_image,
			 pixman_image_t *         mask_image,
			 pixman_image_t *         dst_image,
			 int32_t                  src_x,
			 int32_t                  src_y,
			 int32_t                  mask_x,
			 int32_t                  mask_y,
			 int32_t                  dest_x,
			 int32_t                  dest_y,
			 int32_t                  width,
			 int32_t                  height)
{
    uint8_t     *dst_line, *dst;
    uint8_t     *mask_line, *mask;
    int dst_stride, mask_stride;
    int32_t w;
    uint32_t src;
    uint8_t sa;
    __m64 vsrc, vsrca;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    src = _pixman_image_get_solid (imp, src_image, dst_image->bits.format);

    sa = src >> 24;

    if (src == 0)
	return;

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	if ((((unsigned long)mask_image & 3) == 0) &&
	    (((unsigned long)dst_image  & 3) == 0))
	{
	    while (w >= 4)
	    {
		__m64 vmask = load8888 (*(uint32_t *)mask);
		__m64 vdest = load8888 (*(uint32_t *)dst);

		*(uint32_t *)dst = store8888 (_mm_adds_pu8 (in (vsrca, vmask), vdest));

		w -= 4;
		dst += 4;
		mask += 4;
	    }
	}

	while (w--)
	{
	    uint16_t tmp;
	    uint16_t a;
	    uint32_t m, d;
	    uint32_t r;

	    a = *mask++;
	    d = *dst;

	    m = MUL_UN8 (sa, a, tmp);
	    r = ADD_UN8 (m, d, tmp);

	    *dst++ = r;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_add_8_8 (pixman_implementation_t *imp,
		       pixman_op_t              op,
		       pixman_image_t *         src_image,
		       pixman_image_t *         mask_image,
		       pixman_image_t *         dst_image,
		       int32_t                  src_x,
		       int32_t                  src_y,
		       int32_t                  mask_x,
		       int32_t                  mask_y,
		       int32_t                  dest_x,
		       int32_t                  dest_y,
		       int32_t                  width,
		       int32_t                  height)
{
    uint8_t *dst_line, *dst;
    uint8_t *src_line, *src;
    int dst_stride, src_stride;
    int32_t w;
    uint8_t s, d;
    uint16_t t;

    CHECKPOINT ();

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (unsigned long)dst & 7)
	{
	    s = *src;
	    d = *dst;
	    t = d + s;
	    s = t | (0 - (t >> 8));
	    *dst = s;

	    dst++;
	    src++;
	    w--;
	}

	while (w >= 8)
	{
	    *(__m64*)dst = _mm_adds_pu8 (*(__m64*)src, *(__m64*)dst);
	    dst += 8;
	    src += 8;
	    w -= 8;
	}

	while (w)
	{
	    s = *src;
	    d = *dst;
	    t = d + s;
	    s = t | (0 - (t >> 8));
	    *dst = s;

	    dst++;
	    src++;
	    w--;
	}
    }

    _mm_empty ();
}

static void
mmx_composite_add_8888_8888 (pixman_implementation_t *imp,
                             pixman_op_t              op,
                             pixman_image_t *         src_image,
                             pixman_image_t *         mask_image,
                             pixman_image_t *         dst_image,
                             int32_t                  src_x,
                             int32_t                  src_y,
                             int32_t                  mask_x,
                             int32_t                  mask_y,
                             int32_t                  dest_x,
                             int32_t                  dest_y,
                             int32_t                  width,
                             int32_t                  height)
{
    __m64 dst64;
    uint32_t    *dst_line, *dst;
    uint32_t    *src_line, *src;
    int dst_stride, src_stride;
    int32_t w;

    CHECKPOINT ();

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (unsigned long)dst & 7)
	{
	    *dst = _mm_cvtsi64_si32 (_mm_adds_pu8 (_mm_cvtsi32_si64 (*src),
	                                           _mm_cvtsi32_si64 (*dst)));
	    dst++;
	    src++;
	    w--;
	}

	while (w >= 2)
	{
	    dst64 = _mm_adds_pu8 (*(__m64*)src, *(__m64*)dst);
	    *(uint64_t*)dst = to_uint64 (dst64);
	    dst += 2;
	    src += 2;
	    w -= 2;
	}

	if (w)
	{
	    *dst = _mm_cvtsi64_si32 (_mm_adds_pu8 (_mm_cvtsi32_si64 (*src),
	                                           _mm_cvtsi32_si64 (*dst)));

	}
    }

    _mm_empty ();
}

static pixman_bool_t
pixman_blt_mmx (uint32_t *src_bits,
                uint32_t *dst_bits,
                int       src_stride,
                int       dst_stride,
                int       src_bpp,
                int       dst_bpp,
                int       src_x,
                int       src_y,
                int       dst_x,
                int       dst_y,
                int       width,
                int       height)
{
    uint8_t *   src_bytes;
    uint8_t *   dst_bytes;
    int byte_width;

    if (src_bpp != dst_bpp)
	return FALSE;

    if (src_bpp == 16)
    {
	src_stride = src_stride * (int) sizeof (uint32_t) / 2;
	dst_stride = dst_stride * (int) sizeof (uint32_t) / 2;
	src_bytes = (uint8_t *)(((uint16_t *)src_bits) + src_stride * (src_y) + (src_x));
	dst_bytes = (uint8_t *)(((uint16_t *)dst_bits) + dst_stride * (dst_y) + (dst_x));
	byte_width = 2 * width;
	src_stride *= 2;
	dst_stride *= 2;
    }
    else if (src_bpp == 32)
    {
	src_stride = src_stride * (int) sizeof (uint32_t) / 4;
	dst_stride = dst_stride * (int) sizeof (uint32_t) / 4;
	src_bytes = (uint8_t *)(((uint32_t *)src_bits) + src_stride * (src_y) + (src_x));
	dst_bytes = (uint8_t *)(((uint32_t *)dst_bits) + dst_stride * (dst_y) + (dst_x));
	byte_width = 4 * width;
	src_stride *= 4;
	dst_stride *= 4;
    }
    else
    {
	return FALSE;
    }

    while (height--)
    {
	int w;
	uint8_t *s = src_bytes;
	uint8_t *d = dst_bytes;
	src_bytes += src_stride;
	dst_bytes += dst_stride;
	w = byte_width;

	while (w >= 2 && ((unsigned long)d & 3))
	{
	    *(uint16_t *)d = *(uint16_t *)s;
	    w -= 2;
	    s += 2;
	    d += 2;
	}

	while (w >= 4 && ((unsigned long)d & 7))
	{
	    *(uint32_t *)d = *(uint32_t *)s;

	    w -= 4;
	    s += 4;
	    d += 4;
	}

	while (w >= 64)
	{
#if defined (__GNUC__) || (defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590))
	    __asm__ (
	        "movq	  (%1),	  %%mm0\n"
	        "movq	 8(%1),	  %%mm1\n"
	        "movq	16(%1),	  %%mm2\n"
	        "movq	24(%1),	  %%mm3\n"
	        "movq	32(%1),	  %%mm4\n"
	        "movq	40(%1),	  %%mm5\n"
	        "movq	48(%1),	  %%mm6\n"
	        "movq	56(%1),	  %%mm7\n"

	        "movq	%%mm0,	  (%0)\n"
	        "movq	%%mm1,	 8(%0)\n"
	        "movq	%%mm2,	16(%0)\n"
	        "movq	%%mm3,	24(%0)\n"
	        "movq	%%mm4,	32(%0)\n"
	        "movq	%%mm5,	40(%0)\n"
	        "movq	%%mm6,	48(%0)\n"
	        "movq	%%mm7,	56(%0)\n"
		:
		: "r" (d), "r" (s)
		: "memory",
		  "%mm0", "%mm1", "%mm2", "%mm3",
		  "%mm4", "%mm5", "%mm6", "%mm7");
#else
	    __m64 v0 = *(__m64 *)(s + 0);
	    __m64 v1 = *(__m64 *)(s + 8);
	    __m64 v2 = *(__m64 *)(s + 16);
	    __m64 v3 = *(__m64 *)(s + 24);
	    __m64 v4 = *(__m64 *)(s + 32);
	    __m64 v5 = *(__m64 *)(s + 40);
	    __m64 v6 = *(__m64 *)(s + 48);
	    __m64 v7 = *(__m64 *)(s + 56);
	    *(__m64 *)(d + 0)  = v0;
	    *(__m64 *)(d + 8)  = v1;
	    *(__m64 *)(d + 16) = v2;
	    *(__m64 *)(d + 24) = v3;
	    *(__m64 *)(d + 32) = v4;
	    *(__m64 *)(d + 40) = v5;
	    *(__m64 *)(d + 48) = v6;
	    *(__m64 *)(d + 56) = v7;
#endif

	    w -= 64;
	    s += 64;
	    d += 64;
	}
	while (w >= 4)
	{
	    *(uint32_t *)d = *(uint32_t *)s;

	    w -= 4;
	    s += 4;
	    d += 4;
	}
	if (w >= 2)
	{
	    *(uint16_t *)d = *(uint16_t *)s;
	    w -= 2;
	    s += 2;
	    d += 2;
	}
    }

    _mm_empty ();

    return TRUE;
}

static void
mmx_composite_copy_area (pixman_implementation_t *imp,
                         pixman_op_t              op,
                         pixman_image_t *         src_image,
                         pixman_image_t *         mask_image,
                         pixman_image_t *         dst_image,
                         int32_t                  src_x,
                         int32_t                  src_y,
                         int32_t                  mask_x,
                         int32_t                  mask_y,
                         int32_t                  dest_x,
                         int32_t                  dest_y,
                         int32_t                  width,
                         int32_t                  height)
{
    pixman_blt_mmx (src_image->bits.bits,
                    dst_image->bits.bits,
                    src_image->bits.rowstride,
                    dst_image->bits.rowstride,
                    PIXMAN_FORMAT_BPP (src_image->bits.format),
                    PIXMAN_FORMAT_BPP (dst_image->bits.format),
                    src_x, src_y, dest_x, dest_y, width, height);
}

#if 0
static void
mmx_composite_over_x888_8_8888 (pixman_implementation_t *imp,
                                pixman_op_t              op,
                                pixman_image_t *         src_image,
                                pixman_image_t *         mask_image,
                                pixman_image_t *         dst_image,
                                int32_t                  src_x,
                                int32_t                  src_y,
                                int32_t                  mask_x,
                                int32_t                  mask_y,
                                int32_t                  dest_x,
                                int32_t                  dest_y,
                                int32_t                  width,
                                int32_t                  height)
{
    uint32_t  *src, *src_line;
    uint32_t  *dst, *dst_line;
    uint8_t  *mask, *mask_line;
    int src_stride, mask_stride, dst_stride;
    int32_t w;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);

    while (height--)
    {
	src = src_line;
	src_line += src_stride;
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	w = width;

	while (w--)
	{
	    uint64_t m = *mask;

	    if (m)
	    {
		__m64 s = load8888 (*src | 0xff000000);

		if (m == 0xff)
		{
		    *dst = store8888 (s);
		}
		else
		{
		    __m64 sa = expand_alpha (s);
		    __m64 vm = expand_alpha_rev (to_m64 (m));
		    __m64 vdest = in_over (s, sa, vm, load8888 (*dst));

		    *dst = store8888 (vdest);
		}
	    }

	    mask++;
	    dst++;
	    src++;
	}
    }

    _mm_empty ();
}
#endif

static const pixman_fast_path_t mmx_fast_paths[] =
{
    PIXMAN_STD_FAST_PATH    (OVER, solid,    a8,       r5g6b5,   mmx_composite_over_n_8_0565       ),
    PIXMAN_STD_FAST_PATH    (OVER, solid,    a8,       b5g6r5,   mmx_composite_over_n_8_0565       ),
    PIXMAN_STD_FAST_PATH    (OVER, solid,    a8,       a8r8g8b8, mmx_composite_over_n_8_8888       ),
    PIXMAN_STD_FAST_PATH    (OVER, solid,    a8,       x8r8g8b8, mmx_composite_over_n_8_8888       ),
    PIXMAN_STD_FAST_PATH    (OVER, solid,    a8,       a8b8g8r8, mmx_composite_over_n_8_8888       ),
    PIXMAN_STD_FAST_PATH    (OVER, solid,    a8,       x8b8g8r8, mmx_composite_over_n_8_8888       ),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid,    a8r8g8b8, a8r8g8b8, mmx_composite_over_n_8888_8888_ca ),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid,    a8r8g8b8, x8r8g8b8, mmx_composite_over_n_8888_8888_ca ),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid,    a8r8g8b8, r5g6b5,   mmx_composite_over_n_8888_0565_ca ),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid,    a8b8g8r8, a8b8g8r8, mmx_composite_over_n_8888_8888_ca ),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid,    a8b8g8r8, x8b8g8r8, mmx_composite_over_n_8888_8888_ca ),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid,    a8b8g8r8, b5g6r5,   mmx_composite_over_n_8888_0565_ca ),
    PIXMAN_STD_FAST_PATH    (OVER, pixbuf,   pixbuf,   a8r8g8b8, mmx_composite_over_pixbuf_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, pixbuf,   pixbuf,   x8r8g8b8, mmx_composite_over_pixbuf_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, pixbuf,   pixbuf,   r5g6b5,   mmx_composite_over_pixbuf_0565    ),
    PIXMAN_STD_FAST_PATH    (OVER, rpixbuf,  rpixbuf,  a8b8g8r8, mmx_composite_over_pixbuf_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, rpixbuf,  rpixbuf,  x8b8g8r8, mmx_composite_over_pixbuf_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, rpixbuf,  rpixbuf,  b5g6r5,   mmx_composite_over_pixbuf_0565    ),
    PIXMAN_STD_FAST_PATH    (OVER, x8r8g8b8, solid,    a8r8g8b8, mmx_composite_over_x888_n_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, x8r8g8b8, solid,    x8r8g8b8, mmx_composite_over_x888_n_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, x8b8g8r8, solid,    a8b8g8r8, mmx_composite_over_x888_n_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, x8b8g8r8, solid,    x8b8g8r8, mmx_composite_over_x888_n_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, a8r8g8b8, solid,    a8r8g8b8, mmx_composite_over_8888_n_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, a8r8g8b8, solid,    x8r8g8b8, mmx_composite_over_8888_n_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, a8b8g8r8, solid,    a8b8g8r8, mmx_composite_over_8888_n_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, a8b8g8r8, solid,    x8b8g8r8, mmx_composite_over_8888_n_8888    ),
#if 0
    /* FIXME: This code is commented out since it's apparently
     * not actually faster than the generic code.
     */
    PIXMAN_STD_FAST_PATH    (OVER, x8r8g8b8, a8,       x8r8g8b8, mmx_composite_over_x888_8_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, x8r8g8b8, a8,       a8r8g8b8, mmx_composite_over_x888_8_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, x8b8r8g8, a8,       x8b8g8r8, mmx_composite_over_x888_8_8888    ),
    PIXMAN_STD_FAST_PATH    (OVER, x8b8r8g8, a8,       a8r8g8b8, mmx_composite_over_x888_8_8888    ),
#endif
    PIXMAN_STD_FAST_PATH    (OVER, solid,    null,     a8r8g8b8, mmx_composite_over_n_8888         ),
    PIXMAN_STD_FAST_PATH    (OVER, solid,    null,     x8r8g8b8, mmx_composite_over_n_8888         ),
    PIXMAN_STD_FAST_PATH    (OVER, solid,    null,     r5g6b5,   mmx_composite_over_n_0565         ),
    PIXMAN_STD_FAST_PATH    (OVER, x8r8g8b8, null,     x8r8g8b8, mmx_composite_copy_area           ),
    PIXMAN_STD_FAST_PATH    (OVER, x8b8g8r8, null,     x8b8g8r8, mmx_composite_copy_area           ),

    PIXMAN_STD_FAST_PATH    (OVER, a8r8g8b8, null,     a8r8g8b8, mmx_composite_over_8888_8888      ),
    PIXMAN_STD_FAST_PATH    (OVER, a8r8g8b8, null,     x8r8g8b8, mmx_composite_over_8888_8888      ),
    PIXMAN_STD_FAST_PATH    (OVER, a8r8g8b8, null,     r5g6b5,   mmx_composite_over_8888_0565      ),
    PIXMAN_STD_FAST_PATH    (OVER, a8b8g8r8, null,     a8b8g8r8, mmx_composite_over_8888_8888      ),
    PIXMAN_STD_FAST_PATH    (OVER, a8b8g8r8, null,     x8b8g8r8, mmx_composite_over_8888_8888      ),
    PIXMAN_STD_FAST_PATH    (OVER, a8b8g8r8, null,     b5g6r5,   mmx_composite_over_8888_0565      ),

    PIXMAN_STD_FAST_PATH    (ADD,  a8r8g8b8, null,     a8r8g8b8, mmx_composite_add_8888_8888       ),
    PIXMAN_STD_FAST_PATH    (ADD,  a8b8g8r8, null,     a8b8g8r8, mmx_composite_add_8888_8888       ),
    PIXMAN_STD_FAST_PATH    (ADD,  a8,       null,     a8,       mmx_composite_add_8_8		   ),
    PIXMAN_STD_FAST_PATH    (ADD,  solid,    a8,       a8,       mmx_composite_add_n_8_8           ),

    PIXMAN_STD_FAST_PATH    (SRC,  solid,    a8,       a8r8g8b8, mmx_composite_src_n_8_8888        ),
    PIXMAN_STD_FAST_PATH    (SRC,  solid,    a8,       x8r8g8b8, mmx_composite_src_n_8_8888        ),
    PIXMAN_STD_FAST_PATH    (SRC,  solid,    a8,       a8b8g8r8, mmx_composite_src_n_8_8888        ),
    PIXMAN_STD_FAST_PATH    (SRC,  solid,    a8,       x8b8g8r8, mmx_composite_src_n_8_8888        ),
    PIXMAN_STD_FAST_PATH    (SRC,  a8r8g8b8, null,     a8r8g8b8, mmx_composite_copy_area           ),
    PIXMAN_STD_FAST_PATH    (SRC,  a8b8g8r8, null,     a8b8g8r8, mmx_composite_copy_area           ),
    PIXMAN_STD_FAST_PATH    (SRC,  a8r8g8b8, null,     x8r8g8b8, mmx_composite_copy_area           ),
    PIXMAN_STD_FAST_PATH    (SRC,  a8b8g8r8, null,     x8b8g8r8, mmx_composite_copy_area           ),
    PIXMAN_STD_FAST_PATH    (SRC,  x8r8g8b8, null,     x8r8g8b8, mmx_composite_copy_area           ),
    PIXMAN_STD_FAST_PATH    (SRC,  x8b8g8r8, null,     x8b8g8r8, mmx_composite_copy_area           ),
    PIXMAN_STD_FAST_PATH    (SRC,  r5g6b5,   null,     r5g6b5,   mmx_composite_copy_area           ),
    PIXMAN_STD_FAST_PATH    (SRC,  b5g6r5,   null,     b5g6r5,   mmx_composite_copy_area           ),

    PIXMAN_STD_FAST_PATH    (IN,   a8,       null,     a8,       mmx_composite_in_8_8              ),
    PIXMAN_STD_FAST_PATH    (IN,   solid,    a8,       a8,       mmx_composite_in_n_8_8            ),

    { PIXMAN_OP_NONE },
};

static pixman_bool_t
mmx_blt (pixman_implementation_t *imp,
         uint32_t *               src_bits,
         uint32_t *               dst_bits,
         int                      src_stride,
         int                      dst_stride,
         int                      src_bpp,
         int                      dst_bpp,
         int                      src_x,
         int                      src_y,
         int                      dst_x,
         int                      dst_y,
         int                      width,
         int                      height)
{
    if (!pixman_blt_mmx (
            src_bits, dst_bits, src_stride, dst_stride, src_bpp, dst_bpp,
            src_x, src_y, dst_x, dst_y, width, height))

    {
	return _pixman_implementation_blt (
	    imp->delegate,
	    src_bits, dst_bits, src_stride, dst_stride, src_bpp, dst_bpp,
	    src_x, src_y, dst_x, dst_y, width, height);
    }

    return TRUE;
}

static pixman_bool_t
mmx_fill (pixman_implementation_t *imp,
          uint32_t *               bits,
          int                      stride,
          int                      bpp,
          int                      x,
          int                      y,
          int                      width,
          int                      height,
          uint32_t xor)
{
    if (!pixman_fill_mmx (bits, stride, bpp, x, y, width, height, xor))
    {
	return _pixman_implementation_fill (
	    imp->delegate, bits, stride, bpp, x, y, width, height, xor);
    }

    return TRUE;
}

pixman_implementation_t *
_pixman_implementation_create_mmx (pixman_implementation_t *fallback)
{
    pixman_implementation_t *imp = _pixman_implementation_create (fallback, mmx_fast_paths);

    imp->combine_32[PIXMAN_OP_OVER] = mmx_combine_over_u;
    imp->combine_32[PIXMAN_OP_OVER_REVERSE] = mmx_combine_over_reverse_u;
    imp->combine_32[PIXMAN_OP_IN] = mmx_combine_in_u;
    imp->combine_32[PIXMAN_OP_IN_REVERSE] = mmx_combine_in_reverse_u;
    imp->combine_32[PIXMAN_OP_OUT] = mmx_combine_out_u;
    imp->combine_32[PIXMAN_OP_OUT_REVERSE] = mmx_combine_out_reverse_u;
    imp->combine_32[PIXMAN_OP_ATOP] = mmx_combine_atop_u;
    imp->combine_32[PIXMAN_OP_ATOP_REVERSE] = mmx_combine_atop_reverse_u;
    imp->combine_32[PIXMAN_OP_XOR] = mmx_combine_xor_u;
    imp->combine_32[PIXMAN_OP_ADD] = mmx_combine_add_u;
    imp->combine_32[PIXMAN_OP_SATURATE] = mmx_combine_saturate_u;

    imp->combine_32_ca[PIXMAN_OP_SRC] = mmx_combine_src_ca;
    imp->combine_32_ca[PIXMAN_OP_OVER] = mmx_combine_over_ca;
    imp->combine_32_ca[PIXMAN_OP_OVER_REVERSE] = mmx_combine_over_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_IN] = mmx_combine_in_ca;
    imp->combine_32_ca[PIXMAN_OP_IN_REVERSE] = mmx_combine_in_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_OUT] = mmx_combine_out_ca;
    imp->combine_32_ca[PIXMAN_OP_OUT_REVERSE] = mmx_combine_out_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_ATOP] = mmx_combine_atop_ca;
    imp->combine_32_ca[PIXMAN_OP_ATOP_REVERSE] = mmx_combine_atop_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_XOR] = mmx_combine_xor_ca;
    imp->combine_32_ca[PIXMAN_OP_ADD] = mmx_combine_add_ca;

    imp->blt = mmx_blt;
    imp->fill = mmx_fill;

    return imp;
}

#endif /* USE_MMX */
