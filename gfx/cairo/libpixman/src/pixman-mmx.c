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

#include "pixman-mmx.h"

#undef READ
#undef WRITE
#define READ(img,x) *(x)
#define WRITE(img,ptr,v) (*(ptr) = (v));

#define noVERBOSE

#ifdef VERBOSE
#define CHECKPOINT() ErrorF ("at %s %d\n", __FUNCTION__, __LINE__)
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

/* --------------- MMX primitivess ------------------------------------ */

#ifdef __GNUC__
typedef unsigned long long ullong;
typedef ullong mmxdatafield;
#endif
#ifdef _MSC_VER
typedef unsigned __int64 ullong;
typedef __m64 mmxdatafield;
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
} MMXData;

static const MMXData c =
{
#ifdef __GNUC__
    .mmx_4x00ff =			0x00ff00ff00ff00ffULL,
    .mmx_4x0080 =			0x0080008000800080ULL,
    .mmx_565_rgb =			0x000001f0003f001fULL,
    .mmx_565_unpack_multiplier =	0x0000008404100840ULL,
    .mmx_565_r =			0x000000f800000000ULL,
    .mmx_565_g =			0x0000000000fc0000ULL,
    .mmx_565_b =			0x00000000000000f8ULL,
    .mmx_mask_0 =			0xffffffffffff0000ULL,
    .mmx_mask_1 =			0xffffffff0000ffffULL,
    .mmx_mask_2 =			0xffff0000ffffffffULL,
    .mmx_mask_3 =			0x0000ffffffffffffULL,
    .mmx_full_alpha =			0x00ff000000000000ULL,
    .mmx_ffff0000ffff0000 =		0xffff0000ffff0000ULL,
    .mmx_0000ffff00000000 =		0x0000ffff00000000ULL,
    .mmx_000000000000ffff =		0x000000000000ffffULL,
#endif
#ifdef _MSC_VER
    { 0x00ff00ff00ff00ffUI64 },
    { 0x0080008000800080UI64 },
    { 0x000001f0003f001fUI64 },
    { 0x0000008404100840UI64 },
    { 0x000000f800000000UI64 },
    { 0x0000000000fc0000UI64 },
    { 0x00000000000000f8UI64 },
    { 0xffffffffffff0000UI64 },
    { 0xffffffff0000ffffUI64 },
    { 0xffff0000ffffffffUI64 },
    { 0x0000ffffffffffffUI64 },
    { 0x00ff000000000000UI64 },
    { 0xffff0000ffff0000UI64 },
    { 0x0000ffff00000000UI64 },
    { 0x000000000000ffffUI64 },
#endif
};

#ifdef __GNUC__
#    ifdef __ICC
#        define MC(x)  M64(c.mmx_##x)
#    else
#        define MC(x) ((__m64)c.mmx_##x)
#    endif
#    define inline __inline__ __attribute__ ((__always_inline__))
#endif

#ifdef _MSC_VER
#    define MC(x) c.mmx_##x
#    undef inline
#    define inline __forceinline
#endif

static inline __m64
M64 (ullong x)
{
#ifdef __ICC
    return _mm_cvtsi64_m64 (x);
#elif defined (__GNUC__)
    return (__m64)x;
#endif

#ifdef _MSC_VER
    __m64 res;

    res.m64_u64 = x;
    return res;
#endif
}

static inline ullong
ULLONG (__m64 x)
{
#ifdef __ICC
    return _mm_cvtm64_si64 (x);
#elif defined (__GNUC__)
    return (ullong)x;
#endif

#ifdef _MSC_VER
    ullong res;

    res = x.m64_u64;
    return res;
#endif
}

static inline __m64
shift (__m64 v, int s)
{
    if (s > 0)
	return _mm_slli_si64 (v, s);
    else if (s < 0)
	return _mm_srli_si64 (v, -s);
    else
	return v;
}

static inline __m64
negate (__m64 mask)
{
    return _mm_xor_si64 (mask, MC(4x00ff));
}

static inline __m64
pix_multiply (__m64 a, __m64 b)
{
    __m64 res;

    res = _mm_mullo_pi16 (a, b);
    res = _mm_adds_pu16 (res, MC(4x0080));
    res = _mm_adds_pu16 (res, _mm_srli_pi16 (res, 8));
    res = _mm_srli_pi16 (res, 8);

    return res;
}

static inline __m64
pix_add (__m64 a, __m64 b)
{
    return  _mm_adds_pu8 (a, b);
}

static inline __m64
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

static inline __m64
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

static inline __m64
invert_colors (__m64 pixel)
{
    __m64 x, y, z;

    x = y = z = pixel;

    x = _mm_and_si64 (x, MC(ffff0000ffff0000));
    y = _mm_and_si64 (y, MC(000000000000ffff));
    z = _mm_and_si64 (z, MC(0000ffff00000000));

    y = shift (y, 32);
    z = shift (z, -32);

    x = _mm_or_si64 (x, y);
    x = _mm_or_si64 (x, z);

    return x;
}

static inline __m64
over (__m64 src, __m64 srca, __m64 dest)
{
    return  _mm_adds_pu8 (src, pix_multiply(dest, negate(srca)));
}

static inline __m64
over_rev_non_pre (__m64 src, __m64 dest)
{
    __m64 srca = expand_alpha (src);
    __m64 srcfaaa = _mm_or_si64 (srca, MC(full_alpha));

    return over(pix_multiply(invert_colors(src), srcfaaa), srca, dest);
}

static inline __m64
in (__m64 src,
    __m64 mask)
{
    return pix_multiply (src, mask);
}

static inline __m64
in_over_full_src_alpha (__m64 src, __m64 mask, __m64 dest)
{
    src = _mm_or_si64 (src, MC(full_alpha));

    return over(in (src, mask), mask, dest);
}

#ifndef _MSC_VER
static inline __m64
in_over (__m64 src,
	 __m64 srca,
	 __m64 mask,
	 __m64 dest)
{
    return over(in(src, mask), pix_multiply(srca, mask), dest);
}
#else
#define in_over(src, srca, mask, dest) over(in(src, mask), pix_multiply(srca, mask), dest)
#endif

static inline __m64
load8888 (uint32_t v)
{
    return _mm_unpacklo_pi8 (_mm_cvtsi32_si64 (v), _mm_setzero_si64());
}

static inline __m64
pack8888 (__m64 lo, __m64 hi)
{
    return _mm_packs_pu16 (lo, hi);
}

static inline uint32_t
store8888 (__m64 v)
{
    return _mm_cvtsi64_si32(pack8888(v, _mm_setzero_si64()));
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
static inline __m64
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
    p = _mm_and_si64 (p, MC(565_rgb));

    pixel = _mm_mullo_pi16 (p, MC(565_unpack_multiplier));
    return _mm_srli_pi16 (pixel, 8);
}

static inline __m64
expand8888 (__m64 in, int pos)
{
    if (pos == 0)
	return _mm_unpacklo_pi8 (in, _mm_setzero_si64());
    else
	return _mm_unpackhi_pi8 (in, _mm_setzero_si64());
}

static inline __m64
expandx888 (__m64 in, int pos)
{
    return _mm_or_si64 (expand8888 (in, pos), MC(full_alpha));
}

static inline __m64
pack565 (__m64 pixel, __m64 target, int pos)
{
    __m64 p = pixel;
    __m64 t = target;
    __m64 r, g, b;

    r = _mm_and_si64 (p, MC(565_r));
    g = _mm_and_si64 (p, MC(565_g));
    b = _mm_and_si64 (p, MC(565_b));

    r = shift (r, - (32 - 8) + pos * 16);
    g = shift (g, - (16 - 3) + pos * 16);
    b = shift (b, - (0  + 3) + pos * 16);

    if (pos == 0)
	t = _mm_and_si64 (t, MC(mask_0));
    else if (pos == 1)
	t = _mm_and_si64 (t, MC(mask_1));
    else if (pos == 2)
	t = _mm_and_si64 (t, MC(mask_2));
    else if (pos == 3)
	t = _mm_and_si64 (t, MC(mask_3));

    p = _mm_or_si64 (r, t);
    p = _mm_or_si64 (g, p);

    return _mm_or_si64 (b, p);
}

#ifndef _MSC_VER
static inline __m64
pix_add_mul (__m64 x, __m64 a, __m64 y, __m64 b)
{
    x = _mm_mullo_pi16 (x, a);
    y = _mm_mullo_pi16 (y, b);
    x = _mm_adds_pu16 (x, MC(4x0080));
    x = _mm_adds_pu16 (x, y);
    x = _mm_adds_pu16 (x, _mm_srli_pi16 (x, 8));
    x = _mm_srli_pi16 (x, 8);

    return x;
}
#else
#define pix_add_mul(x, a, y, b) \
( x = _mm_mullo_pi16 (x, a), \
  y = _mm_mullo_pi16 (y, b), \
  x = _mm_adds_pu16 (x, MC(4x0080)), \
  x = _mm_adds_pu16 (x, y), \
  x = _mm_adds_pu16 (x, _mm_srli_pi16 (x, 8)), \
  _mm_srli_pi16 (x, 8) )
#endif

/* --------------- MMX code patch for fbcompose.c --------------------- */

static FASTCALL void
mmxCombineMaskU (uint32_t *src, const uint32_t *mask, int width)
{
    const uint32_t *end = mask + width;
    while (mask < end) {
        uint32_t mmask = *mask;
	uint32_t maska = mmask >> 24;
	if (maska == 0) {
	    *src = 0;
	} else if (maska != 0xff) {
	    __m64 a = load8888(mmask);
	    __m64 s = load8888(*src);
	    a = expand_alpha(a);
	    s = pix_multiply(s, a);
	    *src = store8888(s);
	}
	++src;
	++mask;
    }
    _mm_empty();
}


static FASTCALL void
mmxCombineOverU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;

    while (dest < end) {
	uint32_t ssrc = *src;
	uint32_t a = ssrc >> 24;
	if (a == 0xff) {
	    *dest = ssrc;
	} else if (a) {
	    __m64 s, sa;
	    s = load8888(ssrc);
	    sa = expand_alpha(s);
	    *dest = store8888(over(s, sa, load8888(*dest)));
	}
	++dest;
	++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOverReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;

    while (dest < end) {
	__m64 d, da;
	d = load8888(*dest);
	da = expand_alpha(d);
	*dest = store8888(over (d, da, load8888(*src)));
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineInU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;

    while (dest < end) {
        __m64 x, a;
        x = load8888(*src);
        a = load8888(*dest);
        a = expand_alpha(a);
        x = pix_multiply(x, a);
        *dest = store8888(x);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineInReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;

    while (dest < end) {
        __m64 x, a;
        x = load8888(*dest);
        a = load8888(*src);
        a = expand_alpha(a);
        x = pix_multiply(x, a);
        *dest = store8888(x);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOutU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;

    while (dest < end) {
        __m64 x, a;
        x = load8888(*src);
        a = load8888(*dest);
        a = expand_alpha(a);
        a = negate(a);
        x = pix_multiply(x, a);
        *dest = store8888(x);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOutReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;

    while (dest < end) {
        __m64 x, a;
        x = load8888(*dest);
        a = load8888(*src);
        a = expand_alpha(a);
        a = negate(a);
        x = pix_multiply(x, a);
        *dest = store8888(x);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAtopU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;

    while (dest < end) {
        __m64 s, da, d, sia;
        s = load8888(*src);
        d = load8888(*dest);
        sia = expand_alpha(s);
        sia = negate(sia);
        da = expand_alpha(d);
        s = pix_add_mul (s, da, d, sia);
        *dest = store8888(s);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAtopReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end;

    end = dest + width;

    while (dest < end) {
        __m64 s, dia, d, sa;
        s = load8888(*src);
        d = load8888(*dest);
        sa = expand_alpha(s);
        dia = expand_alpha(d);
        dia = negate(dia);
	s = pix_add_mul (s, dia, d, sa);
        *dest = store8888(s);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineXorU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;

    while (dest < end) {
        __m64 s, dia, d, sia;
        s = load8888(*src);
        d = load8888(*dest);
        sia = expand_alpha(s);
        dia = expand_alpha(d);
        sia = negate(sia);
        dia = negate(dia);
	s = pix_add_mul (s, dia, d, sia);
        *dest = store8888(s);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAddU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;
    while (dest < end) {
        __m64 s, d;
        s = load8888(*src);
        d = load8888(*dest);
        s = pix_add(s, d);
        *dest = store8888(s);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineSaturateU (uint32_t *dest, const uint32_t *src, int width)
{
    const uint32_t *end = dest + width;
    while (dest < end) {
        uint32_t s = *src;
        uint32_t d = *dest;
        __m64 ms = load8888(s);
        __m64 md = load8888(d);
        uint32_t sa = s >> 24;
        uint32_t da = ~d >> 24;

        if (sa > da) {
            __m64 msa = load8888(FbIntDiv(da, sa) << 24);
            msa = expand_alpha(msa);
            ms = pix_multiply(ms, msa);
        }
        md = pix_add(md, ms);
        *dest = store8888(md);
        ++src;
        ++dest;
    }
    _mm_empty();
}


static FASTCALL void
mmxCombineSrcC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        s = pix_multiply(s, a);
        *dest = store8888(s);
        ++src;
        ++mask;
        ++dest;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOverC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 sa = expand_alpha(s);

	*dest = store8888(in_over (s, sa, a, d));

        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOverReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);

	*dest = store8888(over (d, da, in (s, a)));

        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}


static FASTCALL void
mmxCombineInC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        s = pix_multiply(s, a);
        s = pix_multiply(s, da);
        *dest = store8888(s);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineInReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 sa = expand_alpha(s);
        a = pix_multiply(a, sa);
        d = pix_multiply(d, a);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOutC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        da = negate(da);
        s = pix_multiply(s, a);
        s = pix_multiply(s, da);
        *dest = store8888(s);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOutReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 sa = expand_alpha(s);
        a = pix_multiply(a, sa);
        a = negate(a);
        d = pix_multiply(d, a);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAtopC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        __m64 sa = expand_alpha(s);
        s = pix_multiply(s, a);
        a = pix_multiply(a, sa);
        a = negate(a);
	d = pix_add_mul (d, a, s, da);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAtopReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        __m64 sa = expand_alpha(s);
        s = pix_multiply(s, a);
        a = pix_multiply(a, sa);
        da = negate(da);
	d = pix_add_mul (d, a, s, da);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineXorC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        __m64 sa = expand_alpha(s);
        s = pix_multiply(s, a);
        a = pix_multiply(a, sa);
        da = negate(da);
        a = negate(a);
	d = pix_add_mul (d, a, s, da);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAddC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    const uint32_t *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        s = pix_multiply(s, a);
        d = pix_add(s, d);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

void
fbComposeSetupMMX(void)
{
    static pixman_bool_t initialized = FALSE;

    if (initialized)
	return;
    
    /* check if we have MMX support and initialize accordingly */
    if (pixman_have_mmx())
    {
        pixman_composeFunctions.combineU[PIXMAN_OP_OVER] = mmxCombineOverU;
        pixman_composeFunctions.combineU[PIXMAN_OP_OVER_REVERSE] = mmxCombineOverReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_IN] = mmxCombineInU;
        pixman_composeFunctions.combineU[PIXMAN_OP_IN_REVERSE] = mmxCombineInReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_OUT] = mmxCombineOutU;
        pixman_composeFunctions.combineU[PIXMAN_OP_OUT_REVERSE] = mmxCombineOutReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ATOP] = mmxCombineAtopU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ATOP_REVERSE] = mmxCombineAtopReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_XOR] = mmxCombineXorU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ADD] = mmxCombineAddU;
        pixman_composeFunctions.combineU[PIXMAN_OP_SATURATE] = mmxCombineSaturateU;

        pixman_composeFunctions.combineC[PIXMAN_OP_SRC] = mmxCombineSrcC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OVER] = mmxCombineOverC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OVER_REVERSE] = mmxCombineOverReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_IN] = mmxCombineInC;
        pixman_composeFunctions.combineC[PIXMAN_OP_IN_REVERSE] = mmxCombineInReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OUT] = mmxCombineOutC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OUT_REVERSE] = mmxCombineOutReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ATOP] = mmxCombineAtopC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ATOP_REVERSE] = mmxCombineAtopReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_XOR] = mmxCombineXorC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ADD] = mmxCombineAddC;

        pixman_composeFunctions.combineMaskU = mmxCombineMaskU;
    }

    initialized = TRUE;
}


/* ------------------ MMX code paths called from fbpict.c ----------------------- */

void
fbCompositeSolid_nx8888mmx (pixman_op_t op,
			    pixman_image_t * pSrc,
			    pixman_image_t * pMask,
			    pixman_image_t * pDst,
			    int16_t	xSrc,
			    int16_t	ySrc,
			    int16_t	xMask,
			    int16_t	yMask,
			    int16_t	xDst,
			    int16_t	yDst,
			    uint16_t	width,
			    uint16_t	height)
{
    uint32_t	src;
    uint32_t	*dstLine, *dst;
    uint16_t	w;
    int	dstStride;
    __m64	vsrc, vsrca;

    CHECKPOINT();

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    if (src >> 24 == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	w = width;

	CHECKPOINT();

	while (w && (unsigned long)dst & 7)
	{
	    *dst = store8888(over(vsrc, vsrca, load8888(*dst)));

	    w--;
	    dst++;
	}

	while (w >= 2)
	{
	    __m64 vdest;
	    __m64 dest0, dest1;

	    vdest = *(__m64 *)dst;

	    dest0 = over(vsrc, vsrca, expand8888(vdest, 0));
	    dest1 = over(vsrc, vsrca, expand8888(vdest, 1));

	    *(__m64 *)dst = pack8888(dest0, dest1);

	    dst += 2;
	    w -= 2;
	}

	CHECKPOINT();

	while (w)
	{
	    *dst = store8888(over(vsrc, vsrca, load8888(*dst)));

	    w--;
	    dst++;
	}
    }

    _mm_empty();
}

void
fbCompositeSolid_nx0565mmx (pixman_op_t op,
			    pixman_image_t * pSrc,
			    pixman_image_t * pMask,
			    pixman_image_t * pDst,
			    int16_t	xSrc,
			    int16_t	ySrc,
			    int16_t	xMask,
			    int16_t	yMask,
			    int16_t	xDst,
			    int16_t	yDst,
			    uint16_t	width,
			    uint16_t	height)
{
    uint32_t	src;
    uint16_t	*dstLine, *dst;
    uint16_t	w;
    int	dstStride;
    __m64	vsrc, vsrca;

    CHECKPOINT();

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    if (src >> 24 == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	w = width;

	CHECKPOINT();

	while (w && (unsigned long)dst & 7)
	{
	    ullong d = *dst;
	    __m64 vdest = expand565 (M64(d), 0);
	    vdest = pack565(over(vsrc, vsrca, vdest), vdest, 0);
	    *dst = ULLONG(vdest);

	    w--;
	    dst++;
	}

	while (w >= 4)
	{
	    __m64 vdest;

	    vdest = *(__m64 *)dst;

	    vdest = pack565 (over(vsrc, vsrca, expand565(vdest, 0)), vdest, 0);
	    vdest = pack565 (over(vsrc, vsrca, expand565(vdest, 1)), vdest, 1);
	    vdest = pack565 (over(vsrc, vsrca, expand565(vdest, 2)), vdest, 2);
	    vdest = pack565 (over(vsrc, vsrca, expand565(vdest, 3)), vdest, 3);

	    *(__m64 *)dst = vdest;

	    dst += 4;
	    w -= 4;
	}

	CHECKPOINT();

	while (w)
	{
	    ullong d = *dst;
	    __m64 vdest = expand565 (M64(d), 0);
	    vdest = pack565(over(vsrc, vsrca, vdest), vdest, 0);
	    *dst = ULLONG(vdest);

	    w--;
	    dst++;
	}
    }

    _mm_empty();
}

void
fbCompositeSolidMask_nx8888x8888Cmmx (pixman_op_t op,
				      pixman_image_t * pSrc,
				      pixman_image_t * pMask,
				      pixman_image_t * pDst,
				      int16_t	xSrc,
				      int16_t	ySrc,
				      int16_t	xMask,
				      int16_t	yMask,
				      int16_t	xDst,
				      int16_t	yDst,
				      uint16_t	width,
				      uint16_t	height)
{
    uint32_t	src, srca;
    uint32_t	*dstLine;
    uint32_t	*maskLine;
    int	dstStride, maskStride;
    __m64	vsrc, vsrca;

    CHECKPOINT();

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint32_t, maskStride, maskLine, 1);

    vsrc = load8888(src);
    vsrca = expand_alpha(vsrc);

    while (height--)
    {
	int twidth = width;
	uint32_t *p = (uint32_t *)maskLine;
	uint32_t *q = (uint32_t *)dstLine;

	while (twidth && (unsigned long)q & 7)
	{
	    uint32_t m = *(uint32_t *)p;

	    if (m)
	    {
		__m64 vdest = load8888(*q);
		vdest = in_over(vsrc, vsrca, load8888(m), vdest);
		*q = store8888(vdest);
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

		dest0 = in_over(vsrc, vsrca, load8888(m0),
				expand8888 (vdest, 0));
		dest1 = in_over(vsrc, vsrca, load8888(m1),
				expand8888 (vdest, 1));

		*(__m64 *)q = pack8888(dest0, dest1);
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
		__m64 vdest = load8888(*q);
		vdest = in_over(vsrc, vsrca, load8888(m), vdest);
		*q = store8888(vdest);
	    }

	    twidth--;
	    p++;
	    q++;
	}

	dstLine += dstStride;
	maskLine += maskStride;
    }

    _mm_empty();
}

void
fbCompositeSrc_8888x8x8888mmx (pixman_op_t op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
			       int16_t	xSrc,
			       int16_t	ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height)
{
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    uint32_t	mask;
    __m64	vmask;
    int	dstStride, srcStride;
    uint16_t	w;
    __m64  srca;

    CHECKPOINT();

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    fbComposeGetSolid (pMask, mask, pDst->bits.format);
    mask = mask | mask >> 8 | mask >> 16 | mask >> 24;
    vmask = load8888 (mask);
    srca = MC(4x00ff);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
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

    _mm_empty();
}

void
fbCompositeSrc_x888xnx8888mmx (pixman_op_t op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
			       int16_t	xSrc,
			       int16_t	ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height)
{
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    uint32_t	mask;
    __m64	vmask;
    int	dstStride, srcStride;
    uint16_t	w;
    __m64  srca;

    CHECKPOINT();

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetSolid (pMask, mask, pDst->bits.format);

    mask = mask | mask >> 8 | mask >> 16 | mask >> 24;
    vmask = load8888 (mask);
    srca = MC(4x00ff);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
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

    _mm_empty();
}

void
fbCompositeSrc_8888x8888mmx (pixman_op_t op,
			     pixman_image_t * pSrc,
			     pixman_image_t * pMask,
			     pixman_image_t * pDst,
			     int16_t	xSrc,
			     int16_t	ySrc,
			     int16_t      xMask,
			     int16_t      yMask,
			     int16_t      xDst,
			     int16_t      yDst,
			     uint16_t     width,
			     uint16_t     height)
{
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    uint32_t    s;
    int	dstStride, srcStride;
    uint8_t     a;
    uint16_t	w;

    CHECKPOINT();

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a == 0xff)
		*dst = s;
	    else if (a) {
		__m64 ms, sa;
		ms = load8888(s);
		sa = expand_alpha(ms);
		*dst = store8888(over(ms, sa, load8888(*dst)));
	    }
	    dst++;
	}
    }
    _mm_empty();
}

void
fbCompositeSrc_8888x0565mmx (pixman_op_t op,
			     pixman_image_t * pSrc,
			     pixman_image_t * pMask,
			     pixman_image_t * pDst,
			     int16_t      xSrc,
			     int16_t      ySrc,
			     int16_t      xMask,
			     int16_t      yMask,
			     int16_t      xDst,
			     int16_t      yDst,
			     uint16_t     width,
			     uint16_t     height)
{
    uint16_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    int	dstStride, srcStride;
    uint16_t	w;

    CHECKPOINT();

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

#if 0
    /* FIXME */
    assert (pSrc->pDrawable == pMask->pDrawable);
#endif

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	CHECKPOINT();

	while (w && (unsigned long)dst & 7)
	{
	    __m64 vsrc = load8888 (*src);
	    ullong d = *dst;
	    __m64 vdest = expand565 (M64(d), 0);

	    vdest = pack565(over(vsrc, expand_alpha(vsrc), vdest), vdest, 0);

	    *dst = ULLONG(vdest);

	    w--;
	    dst++;
	    src++;
	}

	CHECKPOINT();

	while (w >= 4)
	{
	    __m64 vsrc0, vsrc1, vsrc2, vsrc3;
	    __m64 vdest;

	    vsrc0 = load8888(*(src + 0));
	    vsrc1 = load8888(*(src + 1));
	    vsrc2 = load8888(*(src + 2));
	    vsrc3 = load8888(*(src + 3));

	    vdest = *(__m64 *)dst;

	    vdest = pack565(over(vsrc0, expand_alpha(vsrc0), expand565(vdest, 0)), vdest, 0);
	    vdest = pack565(over(vsrc1, expand_alpha(vsrc1), expand565(vdest, 1)), vdest, 1);
	    vdest = pack565(over(vsrc2, expand_alpha(vsrc2), expand565(vdest, 2)), vdest, 2);
	    vdest = pack565(over(vsrc3, expand_alpha(vsrc3), expand565(vdest, 3)), vdest, 3);

	    *(__m64 *)dst = vdest;

	    w -= 4;
	    dst += 4;
	    src += 4;
	}

	CHECKPOINT();

	while (w)
	{
	    __m64 vsrc = load8888 (*src);
	    ullong d = *dst;
	    __m64 vdest = expand565 (M64(d), 0);

	    vdest = pack565(over(vsrc, expand_alpha(vsrc), vdest), vdest, 0);

	    *dst = ULLONG(vdest);

	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty();
}

void
fbCompositeSolidMask_nx8x8888mmx (pixman_op_t op,
				  pixman_image_t * pSrc,
				  pixman_image_t * pMask,
				  pixman_image_t * pDst,
				  int16_t      xSrc,
				  int16_t      ySrc,
				  int16_t      xMask,
				  int16_t      yMask,
				  int16_t      xDst,
				  int16_t      yDst,
				  uint16_t     width,
				  uint16_t     height)
{
    uint32_t	src, srca;
    uint32_t	*dstLine, *dst;
    uint8_t	*maskLine, *mask;
    int	dstStride, maskStride;
    uint16_t	w;
    __m64	vsrc, vsrca;
    ullong	srcsrc;

    CHECKPOINT();

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
	return;

    srcsrc = (ullong)src << 32 | src;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	CHECKPOINT();

	while (w && (unsigned long)dst & 7)
	{
	    ullong m = *mask;

	    if (m)
	    {
		__m64 vdest = in_over(vsrc, vsrca, expand_alpha_rev (M64(m)), load8888(*dst));
		*dst = store8888(vdest);
	    }

	    w--;
	    mask++;
	    dst++;
	}

	CHECKPOINT();

	while (w >= 2)
	{
	    ullong m0, m1;
	    m0 = *mask;
	    m1 = *(mask + 1);

	    if (srca == 0xff && (m0 & m1) == 0xff)
	    {
		*(ullong *)dst = srcsrc;
	    }
	    else if (m0 | m1)
	    {
		__m64 vdest;
		__m64 dest0, dest1;

		vdest = *(__m64 *)dst;

		dest0 = in_over(vsrc, vsrca, expand_alpha_rev (M64(m0)), expand8888(vdest, 0));
		dest1 = in_over(vsrc, vsrca, expand_alpha_rev (M64(m1)), expand8888(vdest, 1));

		*(__m64 *)dst = pack8888(dest0, dest1);
	    }

	    mask += 2;
	    dst += 2;
	    w -= 2;
	}

	CHECKPOINT();

	while (w)
	{
	    ullong m = *mask;

	    if (m)
	    {
		__m64 vdest = load8888(*dst);
		vdest = in_over(vsrc, vsrca, expand_alpha_rev (M64(m)), vdest);
		*dst = store8888(vdest);
	    }

	    w--;
	    mask++;
	    dst++;
	}
    }

    _mm_empty();
}

pixman_bool_t
pixman_fill_mmx (uint32_t *bits,
		 int stride,
		 int bpp,
		 int x,
		 int y,
		 int width,
		 int height,
		 uint32_t xor)
{
    ullong	fill;
    __m64	vfill;
    uint32_t	byte_width;
    uint8_t	*byte_line;
#ifdef __GNUC__
    __m64	v1, v2, v3, v4, v5, v6, v7;
#endif

    if (bpp != 16 && bpp != 32 && bpp != 8)
	return FALSE;

    if (bpp == 16 && (xor >> 16 != (xor & 0xffff)))
	return FALSE;

    if (bpp == 8 &&
	((xor >> 16 != (xor & 0xffff)) ||
	 (xor >> 24 != (xor & 0x00ff) >> 16)))
    {
	return FALSE;
    }
    
    if (bpp == 8)
    {
	stride = stride * (int) sizeof (uint32_t) / 1;
	byte_line = (uint8_t *)(((uint8_t *)bits) + stride * y + x);
	byte_width = width;
	stride *= 1;
    }
    else if (bpp == 16)
    {
	stride = stride * (int) sizeof (uint32_t) / 2;
	byte_line = (uint8_t *)(((uint16_t *)bits) + stride * y + x);
	byte_width = 2 * width;
	stride *= 2;
    }
    else
    {
	stride = stride * (int) sizeof (uint32_t) / 4;
	byte_line = (uint8_t *)(((uint32_t *)bits) + stride * y + x);
	byte_width = 4 * width;
	stride *= 4;
    }

    fill = ((ullong)xor << 32) | xor;
    vfill = M64(fill);

#ifdef __GNUC__
    __asm__ (
	"movq		%7,	%0\n"
	"movq		%7,	%1\n"
	"movq		%7,	%2\n"
	"movq		%7,	%3\n"
	"movq		%7,	%4\n"
	"movq		%7,	%5\n"
	"movq		%7,	%6\n"
	: "=y" (v1), "=y" (v2), "=y" (v3),
	  "=y" (v4), "=y" (v5), "=y" (v6), "=y" (v7)
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

    _mm_empty();
    return TRUE;
}

void
fbCompositeSolidMaskSrc_nx8x8888mmx (pixman_op_t op,
				     pixman_image_t * pSrc,
				     pixman_image_t * pMask,
				     pixman_image_t * pDst,
				     int16_t      xSrc,
				     int16_t      ySrc,
				     int16_t      xMask,
				     int16_t      yMask,
				     int16_t      xDst,
				     int16_t      yDst,
				     uint16_t     width,
				     uint16_t     height)
{
    uint32_t	src, srca;
    uint32_t	*dstLine, *dst;
    uint8_t	*maskLine, *mask;
    int	dstStride, maskStride;
    uint16_t	w;
    __m64	vsrc, vsrca;
    ullong	srcsrc;

    CHECKPOINT();

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
    {
	pixman_fill_mmx (pDst->bits.bits, pDst->bits.rowstride, PIXMAN_FORMAT_BPP (pDst->bits.format),
			 xDst, yDst, width, height, 0);
	return;
    }

    srcsrc = (ullong)src << 32 | src;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	CHECKPOINT();

	while (w && (unsigned long)dst & 7)
	{
	    ullong m = *mask;

	    if (m)
	    {
		__m64 vdest = in(vsrc, expand_alpha_rev (M64(m)));
		*dst = store8888(vdest);
	    }
	    else
	    {
		*dst = 0;
	    }

	    w--;
	    mask++;
	    dst++;
	}

	CHECKPOINT();

	while (w >= 2)
	{
	    ullong m0, m1;
	    m0 = *mask;
	    m1 = *(mask + 1);

	    if (srca == 0xff && (m0 & m1) == 0xff)
	    {
		*(ullong *)dst = srcsrc;
	    }
	    else if (m0 | m1)
	    {
		__m64 vdest;
		__m64 dest0, dest1;

		vdest = *(__m64 *)dst;

		dest0 = in(vsrc, expand_alpha_rev (M64(m0)));
		dest1 = in(vsrc, expand_alpha_rev (M64(m1)));

		*(__m64 *)dst = pack8888(dest0, dest1);
	    }
	    else
	    {
		*(ullong *)dst = 0;
	    }

	    mask += 2;
	    dst += 2;
	    w -= 2;
	}

	CHECKPOINT();

	while (w)
	{
	    ullong m = *mask;

	    if (m)
	    {
		__m64 vdest = load8888(*dst);
		vdest = in(vsrc, expand_alpha_rev (M64(m)));
		*dst = store8888(vdest);
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

    _mm_empty();
}

void
fbCompositeSolidMask_nx8x0565mmx (pixman_op_t op,
				  pixman_image_t * pSrc,
				  pixman_image_t * pMask,
				  pixman_image_t * pDst,
				  int16_t      xSrc,
				  int16_t      ySrc,
				  int16_t      xMask,
				  int16_t      yMask,
				  int16_t      xDst,
				  int16_t      yDst,
				  uint16_t     width,
				  uint16_t     height)
{
    uint32_t	src, srca;
    uint16_t	*dstLine, *dst;
    uint8_t	*maskLine, *mask;
    int	dstStride, maskStride;
    uint16_t	w;
    __m64	vsrc, vsrca, tmp;
    ullong srcsrcsrcsrc, src16;

    CHECKPOINT();

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    tmp = pack565(vsrc, _mm_setzero_si64(), 0);
    src16 = ULLONG(tmp);

    srcsrcsrcsrc = (ullong)src16 << 48 | (ullong)src16 << 32 |
	(ullong)src16 << 16 | (ullong)src16;

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	CHECKPOINT();

	while (w && (unsigned long)dst & 7)
	{
	    ullong m = *mask;

	    if (m)
	    {
		ullong d = *dst;
		__m64 vd = M64(d);
		__m64 vdest = in_over(vsrc, vsrca, expand_alpha_rev (M64 (m)), expand565(vd, 0));
		vd = pack565(vdest, _mm_setzero_si64(), 0);
		*dst = ULLONG(vd);
	    }

	    w--;
	    mask++;
	    dst++;
	}

	CHECKPOINT();

	while (w >= 4)
	{
	    ullong m0, m1, m2, m3;
	    m0 = *mask;
	    m1 = *(mask + 1);
	    m2 = *(mask + 2);
	    m3 = *(mask + 3);

	    if (srca == 0xff && (m0 & m1 & m2 & m3) == 0xff)
	    {
		*(ullong *)dst = srcsrcsrcsrc;
	    }
	    else if (m0 | m1 | m2 | m3)
	    {
		__m64 vdest;
		__m64 vm0, vm1, vm2, vm3;

		vdest = *(__m64 *)dst;

		vm0 = M64(m0);
		vdest = pack565(in_over(vsrc, vsrca, expand_alpha_rev(vm0), expand565(vdest, 0)), vdest, 0);
		vm1 = M64(m1);
		vdest = pack565(in_over(vsrc, vsrca, expand_alpha_rev(vm1), expand565(vdest, 1)), vdest, 1);
		vm2 = M64(m2);
		vdest = pack565(in_over(vsrc, vsrca, expand_alpha_rev(vm2), expand565(vdest, 2)), vdest, 2);
		vm3 = M64(m3);
		vdest = pack565(in_over(vsrc, vsrca, expand_alpha_rev(vm3), expand565(vdest, 3)), vdest, 3);

		*(__m64 *)dst = vdest;
	    }

	    w -= 4;
	    mask += 4;
	    dst += 4;
	}

	CHECKPOINT();

	while (w)
	{
	    ullong m = *mask;

	    if (m)
	    {
		ullong d = *dst;
		__m64 vd = M64(d);
		__m64 vdest = in_over(vsrc, vsrca, expand_alpha_rev (M64(m)), expand565(vd, 0));
		vd = pack565(vdest, _mm_setzero_si64(), 0);
		*dst = ULLONG(vd);
	    }

	    w--;
	    mask++;
	    dst++;
	}
    }

    _mm_empty();
}

void
fbCompositeSrc_8888RevNPx0565mmx (pixman_op_t op,
				  pixman_image_t * pSrc,
				  pixman_image_t * pMask,
				  pixman_image_t * pDst,
				  int16_t      xSrc,
				  int16_t      ySrc,
				  int16_t      xMask,
				  int16_t      yMask,
				  int16_t      xDst,
				  int16_t      yDst,
				  uint16_t     width,
				  uint16_t     height)
{
    uint16_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    int	dstStride, srcStride;
    uint16_t	w;

    CHECKPOINT();

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

#if 0
    /* FIXME */
    assert (pSrc->pDrawable == pMask->pDrawable);
#endif

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	CHECKPOINT();

	while (w && (unsigned long)dst & 7)
	{
	    __m64 vsrc = load8888 (*src);
	    ullong d = *dst;
	    __m64 vdest = expand565 (M64(d), 0);

	    vdest = pack565(over_rev_non_pre(vsrc, vdest), vdest, 0);

	    *dst = ULLONG(vdest);

	    w--;
	    dst++;
	    src++;
	}

	CHECKPOINT();

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
		vdest = pack565(invert_colors(load8888(s0)), _mm_setzero_si64(), 0);
		vdest = pack565(invert_colors(load8888(s1)), vdest, 1);
		vdest = pack565(invert_colors(load8888(s2)), vdest, 2);
		vdest = pack565(invert_colors(load8888(s3)), vdest, 3);

		*(__m64 *)dst = vdest;
	    }
	    else if (a0 | a1 | a2 | a3)
	    {
		__m64 vdest = *(__m64 *)dst;

		vdest = pack565(over_rev_non_pre(load8888(s0), expand565(vdest, 0)), vdest, 0);
	        vdest = pack565(over_rev_non_pre(load8888(s1), expand565(vdest, 1)), vdest, 1);
		vdest = pack565(over_rev_non_pre(load8888(s2), expand565(vdest, 2)), vdest, 2);
		vdest = pack565(over_rev_non_pre(load8888(s3), expand565(vdest, 3)), vdest, 3);

		*(__m64 *)dst = vdest;
	    }

	    w -= 4;
	    dst += 4;
	    src += 4;
	}

	CHECKPOINT();

	while (w)
	{
	    __m64 vsrc = load8888 (*src);
	    ullong d = *dst;
	    __m64 vdest = expand565 (M64(d), 0);

	    vdest = pack565(over_rev_non_pre(vsrc, vdest), vdest, 0);

	    *dst = ULLONG(vdest);

	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty();
}

/* "8888RevNP" is GdkPixbuf's format: ABGR, non premultiplied */

void
fbCompositeSrc_8888RevNPx8888mmx (pixman_op_t op,
				  pixman_image_t * pSrc,
				  pixman_image_t * pMask,
				  pixman_image_t * pDst,
				  int16_t      xSrc,
				  int16_t      ySrc,
				  int16_t      xMask,
				  int16_t      yMask,
				  int16_t      xDst,
				  int16_t      yDst,
				  uint16_t     width,
				  uint16_t     height)
{
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    int	dstStride, srcStride;
    uint16_t	w;

    CHECKPOINT();

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

#if 0
    /* FIXME */
    assert (pSrc->pDrawable == pMask->pDrawable);
#endif

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
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
	    ullong s0, s1;
	    unsigned char a0, a1;
	    __m64 d0, d1;

	    s0 = *src;
	    s1 = *(src + 1);

	    a0 = (s0 >> 24);
	    a1 = (s1 >> 24);

	    if ((a0 & a1) == 0xFF)
	    {
		d0 = invert_colors(load8888(s0));
		d1 = invert_colors(load8888(s1));

		*(__m64 *)dst = pack8888 (d0, d1);
	    }
	    else if (a0 | a1)
	    {
		__m64 vdest = *(__m64 *)dst;

		d0 = over_rev_non_pre (load8888(s0), expand8888 (vdest, 0));
		d1 = over_rev_non_pre (load8888(s1), expand8888 (vdest, 1));

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

    _mm_empty();
}

void
fbCompositeSolidMask_nx8888x0565Cmmx (pixman_op_t op,
				      pixman_image_t * pSrc,
				      pixman_image_t * pMask,
				      pixman_image_t * pDst,
				      int16_t      xSrc,
				      int16_t      ySrc,
				      int16_t      xMask,
				      int16_t      yMask,
				      int16_t      xDst,
				      int16_t      yDst,
				      uint16_t     width,
				      uint16_t     height)
{
    uint32_t	src, srca;
    uint16_t	*dstLine;
    uint32_t	*maskLine;
    int	dstStride, maskStride;
    __m64  vsrc, vsrca;

    CHECKPOINT();

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint32_t, maskStride, maskLine, 1);

    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);

    while (height--)
    {
	int twidth = width;
	uint32_t *p = (uint32_t *)maskLine;
	uint16_t *q = (uint16_t *)dstLine;

	while (twidth && ((unsigned long)q & 7))
	{
	    uint32_t m = *(uint32_t *)p;

	    if (m)
	    {
		ullong d = *q;
		__m64 vdest = expand565 (M64(d), 0);
		vdest = pack565 (in_over (vsrc, vsrca, load8888 (m), vdest), vdest, 0);
		*q = ULLONG(vdest);
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

		vdest = pack565(in_over(vsrc, vsrca, load8888(m0), expand565(vdest, 0)), vdest, 0);
		vdest = pack565(in_over(vsrc, vsrca, load8888(m1), expand565(vdest, 1)), vdest, 1);
		vdest = pack565(in_over(vsrc, vsrca, load8888(m2), expand565(vdest, 2)), vdest, 2);
		vdest = pack565(in_over(vsrc, vsrca, load8888(m3), expand565(vdest, 3)), vdest, 3);

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
		ullong d = *q;
		__m64 vdest = expand565(M64(d), 0);
		vdest = pack565 (in_over(vsrc, vsrca, load8888(m), vdest), vdest, 0);
		*q = ULLONG(vdest);
	    }

	    twidth--;
	    p++;
	    q++;
	}

	maskLine += maskStride;
	dstLine += dstStride;
    }

    _mm_empty ();
}

void
fbCompositeIn_nx8x8mmx (pixman_op_t op,
			pixman_image_t * pSrc,
			pixman_image_t * pMask,
			pixman_image_t * pDst,
			int16_t      xSrc,
			int16_t      ySrc,
			int16_t      xMask,
			int16_t      yMask,
			int16_t      xDst,
			int16_t      yDst,
			uint16_t     width,
			uint16_t     height)
{
    uint8_t	*dstLine, *dst;
    uint8_t	*maskLine, *mask;
    int	dstStride, maskStride;
    uint16_t	w;
    uint32_t	src;
    uint8_t	sa;
    __m64	vsrc, vsrca;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    sa = src >> 24;
    if (sa == 0)
	return;

    vsrc = load8888(src);
    vsrca = expand_alpha(vsrc);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	if ((((unsigned long)pDst & 3) == 0) &&
	    (((unsigned long)pSrc & 3) == 0))
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
	    uint16_t	tmp;
	    uint8_t	a;
	    uint32_t	m, d;
	    uint32_t	r;

	    a = *mask++;
	    d = *dst;

	    m = FbInU (sa, 0, a, tmp);
	    r = FbInU (m, 0, d, tmp);

	    *dst++ = r;
	}
    }

    _mm_empty();
}

void
fbCompositeIn_8x8mmx (pixman_op_t op,
		      pixman_image_t * pSrc,
		      pixman_image_t * pMask,
		      pixman_image_t * pDst,
		      int16_t      xSrc,
		      int16_t      ySrc,
		      int16_t      xMask,
		      int16_t      yMask,
		      int16_t      xDst,
		      int16_t      yDst,
		      uint16_t     width,
		      uint16_t     height)
{
    uint8_t	*dstLine, *dst;
    uint8_t	*srcLine, *src;
    int	srcStride, dstStride;
    uint16_t	w;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	if ((((unsigned long)pDst & 3) == 0) &&
	    (((unsigned long)pSrc & 3) == 0))
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

	    *dst = FbInU (s, 0, d, tmp);

	    src++;
	    dst++;
	}
    }

    _mm_empty ();
}

void
fbCompositeSrcAdd_8888x8x8mmx (pixman_op_t op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
			       int16_t      xSrc,
			       int16_t      ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height)
{
    uint8_t	*dstLine, *dst;
    uint8_t	*maskLine, *mask;
    int	dstStride, maskStride;
    uint16_t	w;
    uint32_t	src;
    uint8_t	sa;
    __m64	vsrc, vsrca;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    sa = src >> 24;
    if (sa == 0)
	return;

    vsrc = load8888(src);
    vsrca = expand_alpha(vsrc);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	if ((((unsigned long)pMask & 3) == 0) &&
	    (((unsigned long)pDst  & 3) == 0))
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
	    uint16_t	tmp;
	    uint16_t	a;
	    uint32_t	m, d;
	    uint32_t	r;

	    a = *mask++;
	    d = *dst;

	    m = FbInU (sa, 0, a, tmp);
	    r = FbAdd (m, d, 0, tmp);

	    *dst++ = r;
	}
    }

    _mm_empty();
}

void
fbCompositeSrcAdd_8000x8000mmx (pixman_op_t op,
				pixman_image_t * pSrc,
				pixman_image_t * pMask,
				pixman_image_t * pDst,
				int16_t      xSrc,
				int16_t      ySrc,
				int16_t      xMask,
				int16_t      yMask,
				int16_t      xDst,
				int16_t      yDst,
				uint16_t     width,
				uint16_t     height)
{
    uint8_t	*dstLine, *dst;
    uint8_t	*srcLine, *src;
    int	dstStride, srcStride;
    uint16_t	w;
    uint8_t	s, d;
    uint16_t	t;

    CHECKPOINT();

    fbComposeGetStart (pSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
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
	    *(__m64*)dst = _mm_adds_pu8(*(__m64*)src, *(__m64*)dst);
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

    _mm_empty();
}

void
fbCompositeSrcAdd_8888x8888mmx (pixman_op_t 	op,
				pixman_image_t *	pSrc,
				pixman_image_t *	pMask,
				pixman_image_t *	 pDst,
				int16_t		 xSrc,
				int16_t      ySrc,
				int16_t      xMask,
				int16_t      yMask,
				int16_t      xDst,
				int16_t      yDst,
				uint16_t     width,
				uint16_t     height)
{
    __m64 dst64;
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    int	dstStride, srcStride;
    uint16_t	w;

    CHECKPOINT();

    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w && (unsigned long)dst & 7)
	{
	    *dst = _mm_cvtsi64_si32(_mm_adds_pu8(_mm_cvtsi32_si64(*src),
						 _mm_cvtsi32_si64(*dst)));
	    dst++;
	    src++;
	    w--;
	}

	while (w >= 2)
	{
	    dst64 = _mm_adds_pu8(*(__m64*)src, *(__m64*)dst);
	    *(ullong*)dst = ULLONG(dst64);
	    dst += 2;
	    src += 2;
	    w -= 2;
	}

	if (w)
	{
	    *dst = _mm_cvtsi64_si32(_mm_adds_pu8(_mm_cvtsi32_si64(*src),
						 _mm_cvtsi32_si64(*dst)));

	}
    }

    _mm_empty();
}

pixman_bool_t
pixman_blt_mmx (uint32_t *src_bits,
		uint32_t *dst_bits,
		int src_stride,
		int dst_stride,
		int src_bpp,
		int dst_bpp,
		int src_x, int src_y,
		int dst_x, int dst_y,
		int width, int height)
{
    uint8_t *	src_bytes;
    uint8_t *	dst_bytes;
    int		byte_width;

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
    } else if (src_bpp == 32) {
	src_stride = src_stride * (int) sizeof (uint32_t) / 4;
	dst_stride = dst_stride * (int) sizeof (uint32_t) / 4;
	src_bytes = (uint8_t *)(((uint32_t *)src_bits) + src_stride * (src_y) + (src_x));
	dst_bytes = (uint8_t *)(((uint32_t *)dst_bits) + dst_stride * (dst_y) + (dst_x));
	byte_width = 4 * width;
	src_stride *= 4;
	dst_stride *= 4;
    } else {
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
#ifdef __GNUC__
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

    _mm_empty();

    return TRUE;
}

void
fbCompositeCopyAreammx (pixman_op_t       op,
			pixman_image_t *	pSrc,
			pixman_image_t *	pMask,
			pixman_image_t *	pDst,
			int16_t		xSrc,
			int16_t		ySrc,
			int16_t		xMask,
			int16_t		yMask,
			int16_t		xDst,
			int16_t		yDst,
			uint16_t		width,
			uint16_t		height)
{
    pixman_blt_mmx (pSrc->bits.bits,
		    pDst->bits.bits,
		    pSrc->bits.rowstride,
		    pDst->bits.rowstride,
		    PIXMAN_FORMAT_BPP (pSrc->bits.format),
		    PIXMAN_FORMAT_BPP (pDst->bits.format),
		    xSrc, ySrc, xDst, yDst, width, height);
}

void
fbCompositeOver_x888x8x8888mmx (pixman_op_t      op,
				pixman_image_t * pSrc,
				pixman_image_t * pMask,
				pixman_image_t * pDst,
				int16_t      xSrc,
				int16_t      ySrc,
				int16_t      xMask,
				int16_t      yMask,
				int16_t      xDst,
				int16_t      yDst,
				uint16_t     width,
				uint16_t     height)
{
    uint32_t	*src, *srcLine;
    uint32_t    *dst, *dstLine;
    uint8_t	*mask, *maskLine;
    int		 srcStride, maskStride, dstStride;
    uint16_t w;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    while (height--)
    {
	src = srcLine;
	srcLine += srcStride;
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;

	w = width;

	while (w--)
	{
	    ullong m = *mask;

	    if (m)
	    {
		__m64 s = load8888 (*src | 0xff000000);

		if (m == 0xff)
		    *dst = store8888 (s);
		else
		{
		    __m64 sa = expand_alpha (s);
		    __m64 vm = expand_alpha_rev (M64(m));
		    __m64 vdest = in_over(s, sa, vm, load8888 (*dst));

		    *dst = store8888 (vdest);
		}
	    }

	    mask++;
	    dst++;
	    src++;
	}
    }

    _mm_empty();
}



#endif /* USE_MMX */
