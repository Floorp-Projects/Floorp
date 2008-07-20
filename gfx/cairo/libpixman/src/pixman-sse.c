/*
 * Copyright © 2008 Rodrigo Kumpera
 * Copyright © 2008 André Tupinambá
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
 * Author:  Rodrigo Kumpera (kumpera@gmail.com)
 *          André Tupinambá (andrelrt@gmail.com)
 * 
 * Based on work by Owen Taylor and Søren Sandmann
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmintrin.h>
#include <xmmintrin.h> /* for _mm_shuffle_pi16 and _MM_SHUFFLE */
#include <emmintrin.h> /* for SSE2 intrinsics */

#include "pixman-sse.h"

#ifdef USE_SSE2

#ifdef _MSC_VER
#undef inline
#define inline __forceinline
#endif

/* -------------------------------------------------------------------------------------------------
 * Locals
 */

static __m64 xMask0080;
static __m64 xMask00ff;
static __m64 xMask0101;
static __m64 xMaskAlpha;

static __m64 xMask565rgb;
static __m64 xMask565Unpack;

static __m128i Mask0080;
static __m128i Mask00ff;
static __m128i Mask0101;
static __m128i Maskffff;
static __m128i Maskff000000;
static __m128i MaskAlpha;

static __m128i Mask565r;
static __m128i Mask565g1, Mask565g2;
static __m128i Mask565b;
static __m128i MaskRed;
static __m128i MaskGreen;
static __m128i MaskBlue;

/* -------------------------------------------------------------------------------------------------
 * SSE2 Inlines
 */
static inline __m128i
unpack_32_1x128 (uint32_t data)
{
    return _mm_unpacklo_epi8 (_mm_cvtsi32_si128 (data), _mm_setzero_si128());
}

static inline void
unpack_128_2x128 (__m128i data, __m128i* dataLo, __m128i* dataHi)
{
    *dataLo = _mm_unpacklo_epi8 (data, _mm_setzero_si128 ());
    *dataHi = _mm_unpackhi_epi8 (data, _mm_setzero_si128 ());
}

static inline void
unpack565_128_4x128 (__m128i data, __m128i* data0, __m128i* data1, __m128i* data2, __m128i* data3)
{
    __m128i lo, hi;
    __m128i r, g, b;

    lo = _mm_unpacklo_epi16 (data, _mm_setzero_si128 ());
    hi = _mm_unpackhi_epi16 (data, _mm_setzero_si128 ());

    r = _mm_and_si128 (_mm_slli_epi32 (lo, 8), MaskRed);
    g = _mm_and_si128 (_mm_slli_epi32 (lo, 5), MaskGreen);
    b = _mm_and_si128 (_mm_slli_epi32 (lo, 3), MaskBlue);

    lo = _mm_or_si128 (_mm_or_si128 (r, g), b);

    r = _mm_and_si128 (_mm_slli_epi32 (hi, 8), MaskRed);
    g = _mm_and_si128 (_mm_slli_epi32 (hi, 5), MaskGreen);
    b = _mm_and_si128 (_mm_slli_epi32 (hi, 3), MaskBlue);

    hi = _mm_or_si128 (_mm_or_si128 (r, g), b);

    unpack_128_2x128 (lo, data0, data1);
    unpack_128_2x128 (hi, data2, data3);
}

static inline uint16_t
pack565_32_16 (uint32_t pixel)
{
    return (uint16_t) (((pixel>>8) & 0xf800) | ((pixel>>5) & 0x07e0) | ((pixel>>3) & 0x001f));
}

static inline __m128i
pack_2x128_128 (__m128i lo, __m128i hi)
{
    return _mm_packus_epi16 (lo, hi);
}

static inline __m128i
pack565_2x128_128 (__m128i lo, __m128i hi)
{
    __m128i data;
    __m128i r, g1, g2, b;

    data = pack_2x128_128 ( lo, hi );

    r  = _mm_and_si128 (data , Mask565r);
    g1 = _mm_and_si128 (_mm_slli_epi32 (data , 3), Mask565g1);
    g2 = _mm_and_si128 (_mm_srli_epi32 (data , 5), Mask565g2);
    b  = _mm_and_si128 (_mm_srli_epi32 (data , 3), Mask565b);

    return _mm_or_si128 (_mm_or_si128 (_mm_or_si128 (r, g1), g2), b);
}

static inline __m128i
pack565_4x128_128 (__m128i xmm0, __m128i xmm1, __m128i xmm2, __m128i xmm3)
{
    __m128i lo, hi;

    lo = _mm_packus_epi16 (pack565_2x128_128 ( xmm0, xmm1 ), _mm_setzero_si128 ());
    hi = _mm_packus_epi16 (_mm_setzero_si128 (), pack565_2x128_128 ( xmm2, xmm3 ));

    return _mm_or_si128 (lo, hi);
}

static inline uint32_t
packAlpha (__m128i x)
{
    return _mm_cvtsi128_si32 (_mm_packus_epi16 (_mm_packus_epi16 (_mm_srli_epi32 (x, 24),
                                                                  _mm_setzero_si128 ()),
                                                _mm_setzero_si128 ()));
}

static inline __m128i
expandPixel_32_1x128 (uint32_t data)
{
    return _mm_shuffle_epi32 (unpack_32_1x128 (data), _MM_SHUFFLE(1, 0, 1, 0));
}

static inline __m128i
expandAlpha_1x128 (__m128i data)
{
    return _mm_shufflehi_epi16 (_mm_shufflelo_epi16 (data, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
}

static inline void
expandAlpha_2x128 (__m128i dataLo, __m128i dataHi, __m128i* alphaLo, __m128i* alphaHi)
{
    __m128i lo, hi;

    lo = _mm_shufflelo_epi16 (dataLo, _MM_SHUFFLE(3, 3, 3, 3));
    hi = _mm_shufflelo_epi16 (dataHi, _MM_SHUFFLE(3, 3, 3, 3));
    *alphaLo = _mm_shufflehi_epi16 (lo, _MM_SHUFFLE(3, 3, 3, 3));
    *alphaHi = _mm_shufflehi_epi16 (hi, _MM_SHUFFLE(3, 3, 3, 3));
}

static inline void
expandAlphaRev_2x128 (__m128i dataLo, __m128i dataHi, __m128i* alphaLo, __m128i* alphaHi)
{
    __m128i lo, hi;

    lo = _mm_shufflelo_epi16 (dataLo, _MM_SHUFFLE(0, 0, 0, 0));
    hi = _mm_shufflelo_epi16 (dataHi, _MM_SHUFFLE(0, 0, 0, 0));
    *alphaLo = _mm_shufflehi_epi16 (lo, _MM_SHUFFLE(0, 0, 0, 0));
    *alphaHi = _mm_shufflehi_epi16 (hi, _MM_SHUFFLE(0, 0, 0, 0));
}

static inline void
pixMultiply_2x128 (__m128i dataLo, __m128i dataHi, __m128i alphaLo, __m128i alphaHi, __m128i* retLo, __m128i* retHi)
{
    __m128i lo, hi;

    lo = _mm_mullo_epi16 (dataLo, alphaLo);
    hi = _mm_mullo_epi16 (dataHi, alphaHi);
    lo = _mm_adds_epu16 (lo, Mask0080);
    hi = _mm_adds_epu16 (hi, Mask0080);
    *retLo = _mm_mulhi_epu16 (lo, Mask0101);
    *retHi = _mm_mulhi_epu16 (hi, Mask0101);
}

static inline void
pixAddMultiply_2x128 (__m128i srcLo, __m128i srcHi, __m128i alphaDstLo, __m128i alphaDstHi,
                      __m128i dstLo, __m128i dstHi, __m128i alphaSrcLo, __m128i alphaSrcHi,
                      __m128i* retLo, __m128i* retHi)
{
    __m128i lo, hi;
    __m128i mulLo, mulHi;

    lo = _mm_mullo_epi16 (srcLo, alphaDstLo);
    hi = _mm_mullo_epi16 (srcHi, alphaDstHi);
    mulLo = _mm_mullo_epi16 (dstLo, alphaSrcLo);
    mulHi = _mm_mullo_epi16 (dstHi, alphaSrcHi);
    lo = _mm_adds_epu16 (lo, Mask0080);
    hi = _mm_adds_epu16 (hi, Mask0080);
    lo = _mm_adds_epu16 (lo, mulLo);
    hi = _mm_adds_epu16 (hi, mulHi);
    *retLo = _mm_mulhi_epu16 (lo, Mask0101);
    *retHi = _mm_mulhi_epu16 (hi, Mask0101);
}

static inline void
negate_2x128 (__m128i dataLo, __m128i dataHi, __m128i* negLo, __m128i* negHi)
{
    *negLo = _mm_xor_si128 (dataLo, Mask00ff);
    *negHi = _mm_xor_si128 (dataHi, Mask00ff);
}

static inline void
invertColors_2x128 (__m128i dataLo, __m128i dataHi, __m128i* invLo, __m128i* invHi)
{
    __m128i lo, hi;

    lo = _mm_shufflelo_epi16 (dataLo, _MM_SHUFFLE(3, 0, 1, 2));
    hi = _mm_shufflelo_epi16 (dataHi, _MM_SHUFFLE(3, 0, 1, 2));
    *invLo = _mm_shufflehi_epi16 (lo, _MM_SHUFFLE(3, 0, 1, 2));
    *invHi = _mm_shufflehi_epi16 (hi, _MM_SHUFFLE(3, 0, 1, 2));
}

static inline void
over_2x128 (__m128i srcLo, __m128i srcHi, __m128i alphaLo, __m128i alphaHi, __m128i* dstLo, __m128i* dstHi)
{
    negate_2x128 (alphaLo, alphaHi, &alphaLo, &alphaHi);

    pixMultiply_2x128 (*dstLo, *dstHi, alphaLo, alphaHi, dstLo, dstHi);

    *dstLo = _mm_adds_epu8 (srcLo, *dstLo);
    *dstHi = _mm_adds_epu8 (srcHi, *dstHi);
}

static inline void
overRevNonPre_2x128 (__m128i srcLo, __m128i srcHi, __m128i* dstLo, __m128i* dstHi)
{
    __m128i lo, hi;
    __m128i alphaLo, alphaHi;

    expandAlpha_2x128 (srcLo, srcHi, &alphaLo, &alphaHi);

    lo = _mm_or_si128 (alphaLo, MaskAlpha);
    hi = _mm_or_si128 (alphaHi, MaskAlpha);

    invertColors_2x128 (srcLo, srcHi, &srcLo, &srcHi);

    pixMultiply_2x128 (srcLo, srcHi, lo, hi, &lo, &hi);

    over_2x128 (lo, hi, alphaLo, alphaHi, dstLo, dstHi);
}

static inline void
inOver_2x128 (__m128i srcLo,  __m128i srcHi,  __m128i  alphaLo, __m128i  alphaHi,
              __m128i maskLo, __m128i maskHi, __m128i* dstLo,   __m128i* dstHi)
{
    __m128i sLo, sHi;
    __m128i aLo, aHi;

    pixMultiply_2x128 (  srcLo,   srcHi, maskLo, maskHi, &sLo, &sHi);
    pixMultiply_2x128 (alphaLo, alphaHi, maskLo, maskHi, &aLo, &aHi);

    over_2x128 (sLo, sHi, aLo, aHi, dstLo, dstHi);
}

static inline void
cachePrefetch (__m128i* addr)
{
    _mm_prefetch (addr, _MM_HINT_T0);
}

static inline void
cachePrefetchNext (__m128i* addr)
{
    _mm_prefetch (addr + 4, _MM_HINT_T0); // 64 bytes ahead
}

/* load 4 pixels from a 16-byte boundary aligned address */
static inline __m128i
load128Aligned (__m128i* src)
{
    return _mm_load_si128 (src);
}

/* load 4 pixels from a unaligned address */
static inline __m128i
load128Unaligned (__m128i* src)
{
    return _mm_loadu_si128 (src);
}

/* save 4 pixels using Write Combining memory on a 16-byte boundary aligned address */
static inline void
save128WriteCombining (__m128i* dst, __m128i data)
{
    _mm_stream_si128 (dst, data);
}

/* save 4 pixels on a 16-byte boundary aligned address */
static inline void
save128Aligned (__m128i* dst, __m128i data)
{
    _mm_store_si128 (dst, data);
}

/* save 4 pixels on a unaligned address */
static inline void
save128Unaligned (__m128i* dst, __m128i data)
{
    _mm_storeu_si128 (dst, data);
}

/* -------------------------------------------------------------------------------------------------
 * MMX inlines
 */

static inline __m64
unpack_32_1x64 (uint32_t data)
{
    return _mm_unpacklo_pi8 (_mm_cvtsi32_si64 (data), _mm_setzero_si64());
}

static inline __m64
expandAlpha_1x64 (__m64 data)
{
    return _mm_shuffle_pi16 (data, _MM_SHUFFLE(3, 3, 3, 3));
}

static inline __m64
expandAlphaRev_1x64 (__m64 data)
{
    return _mm_shuffle_pi16 (data, _MM_SHUFFLE(0, 0, 0, 0));
}

static inline __m64
expandPixel_8_1x64 (uint8_t data)
{
    return _mm_shuffle_pi16 (unpack_32_1x64 ((uint32_t)data), _MM_SHUFFLE(0, 0, 0, 0));
}

static inline __m64
pixMultiply_1x64 (__m64 data, __m64 alpha)
{
    return _mm_mulhi_pu16 (_mm_adds_pu16 (_mm_mullo_pi16 (data, alpha),
                                          xMask0080),
                           xMask0101);
}

static inline __m64
pixAddMultiply_1x64 (__m64 src, __m64 alphaDst, __m64 dst, __m64 alphaSrc)
{
    return _mm_mulhi_pu16 (_mm_adds_pu16 (_mm_adds_pu16 (_mm_mullo_pi16 (src, alphaDst),
                                                         xMask0080),
                                          _mm_mullo_pi16 (dst, alphaSrc)),
                           xMask0101);
}

static inline __m64
negate_1x64 (__m64 data)
{
    return _mm_xor_si64 (data, xMask00ff);
}

static inline __m64
invertColors_1x64 (__m64 data)
{
    return _mm_shuffle_pi16 (data, _MM_SHUFFLE(3, 0, 1, 2));
}

static inline __m64
over_1x64 (__m64 src, __m64 alpha, __m64 dst)
{
    return _mm_adds_pu8 (src, pixMultiply_1x64 (dst, negate_1x64 (alpha)));
}

static inline __m64
inOver_1x64 (__m64 src, __m64 alpha, __m64 mask, __m64 dst)
{
    return over_1x64 (pixMultiply_1x64 (src, mask),
                      pixMultiply_1x64 (alpha, mask),
                      dst);
}

static inline __m64
overRevNonPre_1x64 (__m64 src, __m64 dst)
{
    __m64 alpha = expandAlpha_1x64 (src);

    return over_1x64 (pixMultiply_1x64 (invertColors_1x64 (src),
                                        _mm_or_si64 (alpha, xMaskAlpha)),
                      alpha,
                      dst);
}

static inline uint32_t
pack_1x64_32( __m64 data )
{
    return _mm_cvtsi64_si32 (_mm_packs_pu16 (data, _mm_setzero_si64()));
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
expand565_16_1x64 (uint16_t pixel)
{
    __m64 p;
    __m64 t1, t2;

    p = _mm_cvtsi32_si64 ((uint32_t) pixel);

    t1 = _mm_slli_si64 (p, 36 - 11);
    t2 = _mm_slli_si64 (p, 16 - 5);

    p = _mm_or_si64 (t1, p);
    p = _mm_or_si64 (t2, p);
    p = _mm_and_si64 (p, xMask565rgb);
    p = _mm_mullo_pi16 (p, xMask565Unpack);

    return _mm_srli_pi16 (p, 8);
}

/* -------------------------------------------------------------------------------------------------
 * Compose Core transformations
 */
static inline uint32_t
coreCombineOverUPixelsse2 (uint32_t src, uint32_t dst)
{
    uint8_t     a;
    __m64       ms;

    a = src >> 24;

    if (a == 0xff)
    {
        return src;
    }
    else if (a)
    {
        ms = unpack_32_1x64 (src);
        return pack_1x64_32 (over_1x64 (ms, expandAlpha_1x64 (ms), unpack_32_1x64 (dst)));
    }

    return dst;
}

static inline void
coreCombineOverUsse2 (uint32_t* pd, const uint32_t* ps, int w)
{
    uint32_t pa;
    uint32_t s, d;

    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmAlphaLo, xmmAlphaHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    /* Align dst on a 16-byte boundary */
    while (w &&
           ((unsigned long)pd & 15))
    {
        d = *pd;
        s = *ps++;

        *pd++ = coreCombineOverUPixelsse2 (s, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        /* I'm loading unaligned because I'm not sure about the address alignment. */
        xmmSrcHi = load128Unaligned ((__m128i*) ps);

        /* Check the alpha channel */
        pa = packAlpha (xmmSrcHi);

        if (pa == 0xffffffff)
        {
            save128Aligned ((__m128i*)pd, xmmSrcHi);
        }
        else if (pa)
        {
            xmmDstHi = load128Aligned ((__m128i*) pd);

            unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
            unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);

            expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaLo, &xmmAlphaHi);

            over_2x128 (xmmSrcLo, xmmSrcHi, xmmAlphaLo, xmmAlphaHi, &xmmDstLo, &xmmDstHi);

            /* rebuid the 4 pixel data and save*/
            save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));
        }

        w -= 4;
        ps += 4;
        pd += 4;
    }

    while (w)
    {
        d = *pd;
        s = *ps++;

        *pd++ = coreCombineOverUPixelsse2 (s, d);
        w--;
    }
}

static inline void
coreCombineOverReverseUsse2 (uint32_t* pd, const uint32_t* ps, int w)
{
    uint32_t s, d;

    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmAlphaLo, xmmAlphaHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    /* Align dst on a 16-byte boundary */
    while (w &&
           ((unsigned long)pd & 15))
    {
        d = *pd;
        s = *ps++;

        *pd++ = coreCombineOverUPixelsse2 (d, s);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        /* I'm loading unaligned because I'm not sure about the address alignment. */
        xmmSrcHi = load128Unaligned ((__m128i*) ps);
        xmmDstHi = load128Aligned ((__m128i*) pd);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);

        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaLo, &xmmAlphaHi);

        over_2x128 (xmmDstLo, xmmDstHi, xmmAlphaLo, xmmAlphaHi, &xmmSrcLo, &xmmSrcHi);

        /* rebuid the 4 pixel data and save*/
        save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmSrcLo, xmmSrcHi));

        w -= 4;
        ps += 4;
        pd += 4;
    }

    while (w)
    {
        d = *pd;
        s = *ps++;

        *pd++ = coreCombineOverUPixelsse2 (d, s);
        w--;
    }
}

static inline uint32_t
coreCombineInUPixelsse2 (uint32_t src, uint32_t dst)
{
    uint32_t maska = src >> 24;

    if (maska == 0)
    {
        return 0;
    }
    else if (maska != 0xff)
    {
        return pack_1x64_32(pixMultiply_1x64 (unpack_32_1x64 (dst), expandAlpha_1x64 (unpack_32_1x64 (src))));
    }

    return dst;
}

static inline void
coreCombineInUsse2 (uint32_t* pd, const uint32_t* ps, int w)
{
    uint32_t s, d;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && ((unsigned long) pd & 15))
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineInUPixelsse2 (d, s);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        xmmDstHi = load128Aligned ((__m128i*) pd);
        xmmSrcHi = load128Unaligned ((__m128i*) ps);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmDstLo, &xmmDstHi);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmDstLo, xmmDstHi, &xmmDstLo, &xmmDstHi);

        save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineInUPixelsse2 (d, s);
        w--;
    }
}

static inline void
coreCombineReverseInUsse2 (uint32_t* pd, const uint32_t* ps, int w)
{
    uint32_t s, d;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && ((unsigned long) pd & 15))
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineInUPixelsse2 (s, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        xmmDstHi = load128Aligned ((__m128i*) pd);
        xmmSrcHi = load128Unaligned ((__m128i*) ps);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmSrcLo, &xmmSrcHi);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        pixMultiply_2x128 (xmmDstLo, xmmDstHi, xmmSrcLo, xmmSrcHi, &xmmDstLo, &xmmDstHi);

        save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineInUPixelsse2 (s, d);
        w--;
    }
}

static inline void
coreCombineReverseOutUsse2 (uint32_t* pd, const uint32_t* ps, int w)
{
    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && ((unsigned long) pd & 15))
    {
        uint32_t s = *ps++;
        uint32_t d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (d), negate_1x64 (expandAlpha_1x64 (unpack_32_1x64 (s)))));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        __m128i xmmSrcLo, xmmSrcHi;
        __m128i xmmDstLo, xmmDstHi;

        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        xmmSrcHi = load128Unaligned ((__m128i*) ps);
        xmmDstHi = load128Aligned ((__m128i*) pd);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        negate_2x128      (xmmSrcLo, xmmSrcHi, &xmmSrcLo, &xmmSrcHi);

        pixMultiply_2x128 (xmmDstLo, xmmDstHi, xmmSrcLo, xmmSrcHi, &xmmDstLo, &xmmDstHi);

        save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        w -= 4;
    }

    while (w)
    {
        uint32_t s = *ps++;
        uint32_t d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (d), negate_1x64 (expandAlpha_1x64 (unpack_32_1x64 (s)))));
        w--;
    }
}

static inline void
coreCombineOutUsse2 (uint32_t* pd, const uint32_t* ps, int w)
{
    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && ((unsigned long) pd & 15))
    {
        uint32_t s = *ps++;
        uint32_t d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (s), negate_1x64 (expandAlpha_1x64 (unpack_32_1x64 (d)))));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        __m128i xmmSrcLo, xmmSrcHi;
        __m128i xmmDstLo, xmmDstHi;

        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        xmmSrcHi = load128Unaligned ((__m128i*) ps);
        xmmDstHi = load128Aligned ((__m128i*) pd);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);

        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmDstLo, &xmmDstHi);
        negate_2x128      (xmmDstLo, xmmDstHi, &xmmDstLo, &xmmDstHi);

        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmDstLo, xmmDstHi, &xmmDstLo, &xmmDstHi);

        save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        w -= 4;
    }

    while (w)
    {
        uint32_t s = *ps++;
        uint32_t d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (s), negate_1x64 (expandAlpha_1x64 (unpack_32_1x64 (d)))));
        w--;
    }
}

static inline uint32_t
coreCombineAtopUPixelsse2 (uint32_t src, uint32_t dst)
{
    __m64 s = unpack_32_1x64 (src);
    __m64 d = unpack_32_1x64 (dst);

    __m64 sa = negate_1x64 (expandAlpha_1x64 (s));
    __m64 da = expandAlpha_1x64 (d);

    return pack_1x64_32 (pixAddMultiply_1x64 (s, da, d, sa));
}

static inline void
coreCombineAtopUsse2 (uint32_t* pd, const uint32_t* ps, int w)
{
    uint32_t s, d;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmAlphaSrcLo, xmmAlphaSrcHi;
    __m128i xmmAlphaDstLo, xmmAlphaDstHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && ((unsigned long) pd & 15))
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineAtopUPixelsse2 (s, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        xmmSrcHi = load128Unaligned ((__m128i*) ps);
        xmmDstHi = load128Aligned ((__m128i*) pd);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaSrcLo, &xmmAlphaSrcHi);
        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        negate_2x128 (xmmAlphaSrcLo, xmmAlphaSrcHi, &xmmAlphaSrcLo, &xmmAlphaSrcHi);

        pixAddMultiply_2x128 ( xmmSrcLo, xmmSrcHi, xmmAlphaDstLo, xmmAlphaDstHi,
                               xmmDstLo, xmmDstHi, xmmAlphaSrcLo, xmmAlphaSrcHi,
                               &xmmDstLo, &xmmDstHi );

        save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineAtopUPixelsse2 (s, d);
        w--;
    }
}

static inline uint32_t
coreCombineReverseAtopUPixelsse2 (uint32_t src, uint32_t dst)
{
    __m64 s = unpack_32_1x64 (src);
    __m64 d = unpack_32_1x64 (dst);

    __m64 sa = expandAlpha_1x64 (s);
    __m64 da = negate_1x64 (expandAlpha_1x64 (d));

    return pack_1x64_32 (pixAddMultiply_1x64 (s, da, d, sa));
}

static inline void
coreCombineReverseAtopUsse2 (uint32_t* pd, const uint32_t* ps, int w)
{
    uint32_t s, d;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmAlphaSrcLo, xmmAlphaSrcHi;
    __m128i xmmAlphaDstLo, xmmAlphaDstHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && ((unsigned long) pd & 15))
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineReverseAtopUPixelsse2 (s, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        xmmSrcHi = load128Unaligned ((__m128i*) ps);
        xmmDstHi = load128Aligned ((__m128i*) pd);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaSrcLo, &xmmAlphaSrcHi);
        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        negate_2x128 (xmmAlphaDstLo, xmmAlphaDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        pixAddMultiply_2x128 ( xmmSrcLo, xmmSrcHi, xmmAlphaDstLo, xmmAlphaDstHi,
                               xmmDstLo, xmmDstHi, xmmAlphaSrcLo, xmmAlphaSrcHi,
                               &xmmDstLo, &xmmDstHi );

        save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineReverseAtopUPixelsse2 (s, d);
        w--;
    }
}

static inline uint32_t
coreCombineXorUPixelsse2 (uint32_t src, uint32_t dst)
{
    __m64 s = unpack_32_1x64 (src);
    __m64 d = unpack_32_1x64 (dst);

    return pack_1x64_32 (pixAddMultiply_1x64 (s, negate_1x64 (expandAlpha_1x64 (d)), d, negate_1x64 (expandAlpha_1x64 (s))));
}

static inline void
coreCombineXorUsse2 (uint32_t* dst, const uint32_t* src, int width)
{
    int w = width;
    uint32_t s, d;
    uint32_t* pd = dst;
    const uint32_t* ps = src;

    __m128i xmmSrc, xmmSrcLo, xmmSrcHi;
    __m128i xmmDst, xmmDstLo, xmmDstHi;
    __m128i xmmAlphaSrcLo, xmmAlphaSrcHi;
    __m128i xmmAlphaDstLo, xmmAlphaDstHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && ((unsigned long) pd & 15))
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineXorUPixelsse2 (s, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        xmmSrc = load128Unaligned ((__m128i*) ps);
        xmmDst = load128Aligned ((__m128i*) pd);

        unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaSrcLo, &xmmAlphaSrcHi);
        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        negate_2x128 (xmmAlphaSrcLo, xmmAlphaSrcHi, &xmmAlphaSrcLo, &xmmAlphaSrcHi);
        negate_2x128 (xmmAlphaDstLo, xmmAlphaDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        pixAddMultiply_2x128 ( xmmSrcLo, xmmSrcHi, xmmAlphaDstLo, xmmAlphaDstHi,
                               xmmDstLo, xmmDstHi, xmmAlphaSrcLo, xmmAlphaSrcHi,
                               &xmmDstLo, &xmmDstHi );

        save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        d = *pd;

        *pd++ = coreCombineXorUPixelsse2 (s, d);
        w--;
    }
}

static inline void
coreCombineAddUsse2 (uint32_t* dst, const uint32_t* src, int width)
{
    int w = width;
    uint32_t s,d;
    uint32_t* pd = dst;
    const uint32_t* ps = src;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        d = *pd;
        *pd++ = _mm_cvtsi64_si32 (_mm_adds_pu8 (_mm_cvtsi32_si64 (s), _mm_cvtsi32_si64 (d)));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        save128Aligned( (__m128i*)pd,
                        _mm_adds_epu8( load128Unaligned((__m128i*)ps),
                                       load128Aligned  ((__m128i*)pd)) );
        pd += 4;
        ps += 4;
        w -= 4;
    }

    while (w--)
    {
        s = *ps++;
        d = *pd;
        *pd++ = _mm_cvtsi64_si32 (_mm_adds_pu8 (_mm_cvtsi32_si64 (s), _mm_cvtsi32_si64 (d)));
    }
}

static inline uint32_t
coreCombineSaturateUPixelsse2 (uint32_t src, uint32_t dst)
{
    __m64 ms = unpack_32_1x64 (src);
    __m64 md = unpack_32_1x64 (dst);
    uint32_t sa = src >> 24;
    uint32_t da = ~dst >> 24;

    if (sa > da)
    {
        ms = pixMultiply_1x64 (ms, expandAlpha_1x64 (unpack_32_1x64 (FbIntDiv(da, sa) << 24)));
    }

    return pack_1x64_32 (_mm_adds_pu16 (md, ms));
}

static inline void
coreCombineSaturateUsse2 (uint32_t *pd, const uint32_t *ps, int w)
{
    uint32_t s,d;

    uint32_t packCmp;
    __m128i xmmSrc, xmmDst;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        d = *pd;
        *pd++ = coreCombineSaturateUPixelsse2 (s, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);

        xmmDst = load128Aligned  ((__m128i*)pd);
        xmmSrc = load128Unaligned((__m128i*)ps);

        packCmp = _mm_movemask_epi8 (_mm_cmpgt_epi32 (_mm_srli_epi32 (xmmSrc, 24),
                                                      _mm_srli_epi32 (_mm_xor_si128 (xmmDst, Maskff000000), 24)));

        /* if some alpha src is grater than respective ~alpha dst */
        if (packCmp)
        {
            s = *ps++;
            d = *pd;
            *pd++ = coreCombineSaturateUPixelsse2 (s, d);

            s = *ps++;
            d = *pd;
            *pd++ = coreCombineSaturateUPixelsse2 (s, d);

            s = *ps++;
            d = *pd;
            *pd++ = coreCombineSaturateUPixelsse2 (s, d);

            s = *ps++;
            d = *pd;
            *pd++ = coreCombineSaturateUPixelsse2 (s, d);
        }
        else
        {
            save128Aligned ((__m128i*)pd, _mm_adds_epu8 (xmmDst, xmmSrc));

            pd += 4;
            ps += 4;
        }

        w -= 4;
    }

    while (w--)
    {
        s = *ps++;
        d = *pd;
        *pd++ = coreCombineSaturateUPixelsse2 (s, d);
    }
}

static inline void
coreCombineSrcCsse2 (uint32_t* pd, const uint32_t* ps, const uint32_t *pm, int w)
{
    uint32_t s, m;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmMaskLo, xmmMaskHi;
    __m128i xmmDstLo, xmmDstHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (s), unpack_32_1x64 (m)));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmMaskLo, xmmMaskHi, &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (s), unpack_32_1x64 (m)));
        w--;
    }
}

static inline uint32_t
coreCombineOverCPixelsse2 (uint32_t src, uint32_t mask, uint32_t dst)
{
    __m64 s = unpack_32_1x64 (src);

    return pack_1x64_32 (inOver_1x64 (s, expandAlpha_1x64 (s), unpack_32_1x64 (mask), unpack_32_1x64 (dst)));
}

static inline void
coreCombineOverCsse2 (uint32_t* pd, const uint32_t* ps, const uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmAlphaLo, xmmAlphaHi;
    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineOverCPixelsse2 (s, m, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaLo, &xmmAlphaHi);

        inOver_2x128 (xmmSrcLo, xmmSrcHi, xmmAlphaLo, xmmAlphaHi, xmmMaskLo, xmmMaskHi, &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineOverCPixelsse2 (s, m, d);
        w--;
    }
}

static inline uint32_t
coreCombineOverReverseCPixelsse2 (uint32_t src, uint32_t mask, uint32_t dst)
{
    __m64 d = unpack_32_1x64 (dst);

	return pack_1x64_32(over_1x64 (d, expandAlpha_1x64 (d), pixMultiply_1x64 (unpack_32_1x64 (src), unpack_32_1x64 (mask))));
}

static inline void
coreCombineOverReverseCsse2 (uint32_t* pd, const uint32_t* ps, const uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmAlphaLo, xmmAlphaHi;
    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineOverReverseCPixelsse2 (s, m, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaLo, &xmmAlphaHi);
        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        over_2x128 (xmmDstLo, xmmDstHi, xmmAlphaLo, xmmAlphaHi, &xmmMaskLo, &xmmMaskHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmMaskLo, xmmMaskHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineOverReverseCPixelsse2 (s, m, d);
        w--;
    }
}

static inline void
coreCombineInCsse2 (uint32_t *pd, uint32_t *ps, uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmAlphaLo, xmmAlphaHi;
    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (pixMultiply_1x64 (unpack_32_1x64 (s), unpack_32_1x64 (m)),
                                                expandAlpha_1x64 (unpack_32_1x64 (d))));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaLo, &xmmAlphaHi);
        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmMaskLo, xmmMaskHi, &xmmDstLo, &xmmDstHi);

        pixMultiply_2x128 (xmmDstLo, xmmDstHi, xmmAlphaLo, xmmAlphaHi, &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (pixMultiply_1x64 (unpack_32_1x64 (s), unpack_32_1x64 (m)),
                                                expandAlpha_1x64 (unpack_32_1x64 (d))));
        w--;
    }
}

static inline void
coreCombineInReverseCsse2 (uint32_t *pd, uint32_t *ps, uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmAlphaLo, xmmAlphaHi;
    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (d),
                                                pixMultiply_1x64 (unpack_32_1x64 (m),
                                                                  expandAlpha_1x64 (unpack_32_1x64 (s)))));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaLo, &xmmAlphaHi);
        pixMultiply_2x128 (xmmMaskLo, xmmMaskHi, xmmAlphaLo, xmmAlphaHi, &xmmAlphaLo, &xmmAlphaHi);

        pixMultiply_2x128 (xmmDstLo, xmmDstHi, xmmAlphaLo, xmmAlphaHi, &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (d),
                                                pixMultiply_1x64 (unpack_32_1x64 (m),
                                                                  expandAlpha_1x64 (unpack_32_1x64 (s)))));
        w--;
    }
}

static inline void
coreCombineOutCsse2 (uint32_t *pd, uint32_t *ps, uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmAlphaLo, xmmAlphaHi;
    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (pixMultiply_1x64 (unpack_32_1x64 (s), unpack_32_1x64 (m)),
                                                negate_1x64 (expandAlpha_1x64 (unpack_32_1x64 (d)))));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaLo, &xmmAlphaHi);
        negate_2x128 (xmmAlphaLo, xmmAlphaHi, &xmmAlphaLo, &xmmAlphaHi);

        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmMaskLo, xmmMaskHi, &xmmDstLo, &xmmDstHi);
        pixMultiply_2x128 (xmmDstLo, xmmDstHi, xmmAlphaLo, xmmAlphaHi, &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (pixMultiply_1x64 (unpack_32_1x64 (s), unpack_32_1x64 (m)),
                                                negate_1x64 (expandAlpha_1x64 (unpack_32_1x64 (d)))));
        w--;
    }
}

static inline void
coreCombineOutReverseCsse2 (uint32_t *pd, uint32_t *ps, uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmAlphaLo, xmmAlphaHi;
    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (d),
                                                negate_1x64 (pixMultiply_1x64 (unpack_32_1x64 (m),
                                                                               expandAlpha_1x64 (unpack_32_1x64 (s))))));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaLo, &xmmAlphaHi);

        pixMultiply_2x128 (xmmMaskLo, xmmMaskHi, xmmAlphaLo, xmmAlphaHi, &xmmMaskLo, &xmmMaskHi);

        negate_2x128 (xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        pixMultiply_2x128 (xmmDstLo, xmmDstHi, xmmMaskLo, xmmMaskHi, &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (d),
                                                negate_1x64 (pixMultiply_1x64 (unpack_32_1x64 (m),
                                                                               expandAlpha_1x64 (unpack_32_1x64 (s))))));
        w--;
    }
}

static inline uint32_t
coreCombineAtopCPixelsse2 (uint32_t src, uint32_t mask, uint32_t dst)
{
    __m64 m = unpack_32_1x64 (mask);
    __m64 s = unpack_32_1x64 (src);
    __m64 d = unpack_32_1x64 (dst);
    __m64 sa = expandAlpha_1x64 (s);
    __m64 da = expandAlpha_1x64 (d);

    s = pixMultiply_1x64 (s, m);
    m = negate_1x64 (pixMultiply_1x64 (m, sa));

    return pack_1x64_32 (pixAddMultiply_1x64 (d, m, s, da));
}

static inline void
coreCombineAtopCsse2 (uint32_t *pd, uint32_t *ps, uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmAlphaSrcLo, xmmAlphaSrcHi;
    __m128i xmmAlphaDstLo, xmmAlphaDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineAtopCPixelsse2 (s, m, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaSrcLo, &xmmAlphaSrcHi);
        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmMaskLo, xmmMaskHi, &xmmSrcLo, &xmmSrcHi);
        pixMultiply_2x128 (xmmMaskLo, xmmMaskHi, xmmAlphaSrcLo, xmmAlphaSrcHi, &xmmMaskLo, &xmmMaskHi);

        negate_2x128 (xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        pixAddMultiply_2x128 (xmmDstLo, xmmDstHi, xmmMaskLo, xmmMaskHi,
                              xmmSrcLo, xmmSrcHi, xmmAlphaDstLo, xmmAlphaDstHi,
                              &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineAtopCPixelsse2 (s, m, d);
        w--;
    }
}

static inline uint32_t
coreCombineReverseAtopCPixelsse2 (uint32_t src, uint32_t mask, uint32_t dst)
{
    __m64 m = unpack_32_1x64 (mask);
    __m64 s = unpack_32_1x64 (src);
    __m64 d = unpack_32_1x64 (dst);

    __m64 da = negate_1x64 (expandAlpha_1x64 (d));
    __m64 sa = expandAlpha_1x64 (s);

    s = pixMultiply_1x64 (s, m);
    m = pixMultiply_1x64 (m, sa);

    return pack_1x64_32 (pixAddMultiply_1x64 (d, m, s, da));
}

static inline void
coreCombineReverseAtopCsse2 (uint32_t *pd, uint32_t *ps, uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmAlphaSrcLo, xmmAlphaSrcHi;
    __m128i xmmAlphaDstLo, xmmAlphaDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineReverseAtopCPixelsse2 (s, m, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaSrcLo, &xmmAlphaSrcHi);
        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmMaskLo, xmmMaskHi, &xmmSrcLo, &xmmSrcHi);
        pixMultiply_2x128 (xmmMaskLo, xmmMaskHi, xmmAlphaSrcLo, xmmAlphaSrcHi, &xmmMaskLo, &xmmMaskHi);

        negate_2x128 (xmmAlphaDstLo, xmmAlphaDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        pixAddMultiply_2x128 (xmmDstLo, xmmDstHi, xmmMaskLo, xmmMaskHi,
                              xmmSrcLo, xmmSrcHi, xmmAlphaDstLo, xmmAlphaDstHi,
                              &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineReverseAtopCPixelsse2 (s, m, d);
        w--;
    }
}

static inline uint32_t
coreCombineXorCPixelsse2 (uint32_t src, uint32_t mask, uint32_t dst)
{
    __m64 a = unpack_32_1x64 (mask);
    __m64 s = unpack_32_1x64 (src);
    __m64 d = unpack_32_1x64 (dst);

    return pack_1x64_32 (pixAddMultiply_1x64 (d,
                                              negate_1x64 (pixMultiply_1x64 (a, expandAlpha_1x64 (s))),
                                              pixMultiply_1x64 (s, a),
                                              negate_1x64 (expandAlpha_1x64 (d))));
}

static inline void
coreCombineXorCsse2 (uint32_t *pd, uint32_t *ps, uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmAlphaSrcLo, xmmAlphaSrcHi;
    __m128i xmmAlphaDstLo, xmmAlphaDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineXorCPixelsse2 (s, m, d);
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmDstHi = load128Aligned ((__m128i*)pd);
        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);

        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);
        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaSrcLo, &xmmAlphaSrcHi);
        expandAlpha_2x128 (xmmDstLo, xmmDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);

        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmMaskLo, xmmMaskHi, &xmmSrcLo, &xmmSrcHi);
        pixMultiply_2x128 (xmmMaskLo, xmmMaskHi, xmmAlphaSrcLo, xmmAlphaSrcHi, &xmmMaskLo, &xmmMaskHi);

        negate_2x128 (xmmAlphaDstLo, xmmAlphaDstHi, &xmmAlphaDstLo, &xmmAlphaDstHi);
        negate_2x128 (xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

        pixAddMultiply_2x128 (xmmDstLo, xmmDstHi, xmmMaskLo, xmmMaskHi,
                              xmmSrcLo, xmmSrcHi, xmmAlphaDstLo, xmmAlphaDstHi,
                              &xmmDstLo, &xmmDstHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = coreCombineXorCPixelsse2 (s, m, d);
        w--;
    }
}

static inline void
coreCombineAddCsse2 (uint32_t *pd, uint32_t *ps, uint32_t *pm, int w)
{
    uint32_t s, m, d;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;
    __m128i xmmMaskLo, xmmMaskHi;

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w && (unsigned long)pd & 15)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (_mm_adds_pu8 (pixMultiply_1x64 (unpack_32_1x64 (s),
                                                              unpack_32_1x64 (m)),
                                            unpack_32_1x64 (d)));
        w--;
    }

    /* call prefetch hint to optimize cache load*/
    cachePrefetch ((__m128i*)ps);
    cachePrefetch ((__m128i*)pd);
    cachePrefetch ((__m128i*)pm);

    while (w >= 4)
    {
        /* fill cache line with next memory */
        cachePrefetchNext ((__m128i*)ps);
        cachePrefetchNext ((__m128i*)pd);
        cachePrefetchNext ((__m128i*)pm);

        xmmSrcHi = load128Unaligned ((__m128i*)ps);
        xmmMaskHi = load128Unaligned ((__m128i*)pm);
        xmmDstHi = load128Aligned ((__m128i*)pd);

        unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);
        unpack_128_2x128 (xmmMaskHi, &xmmMaskLo, &xmmMaskHi);
        unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);

        pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmMaskLo, xmmMaskHi, &xmmSrcLo, &xmmSrcHi);

        save128Aligned( (__m128i*)pd, pack_2x128_128 (_mm_adds_epu8 (xmmSrcLo, xmmDstLo),
                                                      _mm_adds_epu8 (xmmSrcHi, xmmDstHi)));

        ps += 4;
        pd += 4;
        pm += 4;
        w -= 4;
    }

    while (w)
    {
        s = *ps++;
        m = *pm++;
        d = *pd;

        *pd++ = pack_1x64_32 (_mm_adds_pu8 (pixMultiply_1x64 (unpack_32_1x64 (s),
                                                              unpack_32_1x64 (m)),
                                            unpack_32_1x64 (d)));
        w--;
    }
}

/* -------------------------------------------------------------------------------------------------
 * fbComposeSetupSSE
 */
static inline __m64
createMask_16_64 (uint16_t mask)
{
    return _mm_set1_pi16 (mask);
}

static inline __m128i
createMask_16_128 (uint16_t mask)
{
    return _mm_set1_epi16 (mask);
}

static inline __m64
createMask_2x32_64 (uint32_t mask0, uint32_t mask1)
{
    return _mm_set_pi32 (mask0, mask1);
}

static inline __m128i
createMask_2x32_128 (uint32_t mask0, uint32_t mask1)
{
    return _mm_set_epi32 (mask0, mask1, mask0, mask1);
}

/* SSE2 code patch for fbcompose.c */

static FASTCALL void
sse2CombineMaskU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineReverseInUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineOverU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineOverUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineOverReverseU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineOverReverseUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineInU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineInUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineInReverseU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineReverseInUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineOutU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineOutUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineOutReverseU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineReverseOutUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineAtopU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineAtopUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineAtopReverseU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineReverseAtopUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineXorU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineXorUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineAddU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineAddUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineSaturateU (uint32_t *dst, const uint32_t *src, int width)
{
    coreCombineSaturateUsse2 (dst, src, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineSrcC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineSrcCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineOverC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineOverCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineOverReverseC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineOverReverseCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineInC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineInCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineInReverseC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineInReverseCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineOutC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineOutCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineOutReverseC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineOutReverseCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineAtopC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineAtopCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineAtopReverseC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineReverseAtopCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineXorC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineXorCsse2 (dst, src, mask, width);
    _mm_empty();
}

static FASTCALL void
sse2CombineAddC (uint32_t *dst, uint32_t *src, uint32_t *mask, int width)
{
    coreCombineAddCsse2 (dst, src, mask, width);
    _mm_empty();
}

void
fbComposeSetupSSE(void)
{
    static pixman_bool_t initialized = FALSE;

    if (initialized)
	return;
    
    /* check if we have SSE2 support and initialize accordingly */
    if (pixman_have_sse())
    {
        /* SSE2 constants */
        Mask565r  = createMask_2x32_128 (0x00f80000, 0x00f80000);
        Mask565g1 = createMask_2x32_128 (0x00070000, 0x00070000);
        Mask565g2 = createMask_2x32_128 (0x000000e0, 0x000000e0);
        Mask565b  = createMask_2x32_128 (0x0000001f, 0x0000001f);
        MaskRed   = createMask_2x32_128 (0x00f80000, 0x00f80000);
        MaskGreen = createMask_2x32_128 (0x0000fc00, 0x0000fc00);
        MaskBlue  = createMask_2x32_128 (0x000000f8, 0x000000f8);

        Mask0080 = createMask_16_128 (0x0080);
        Mask00ff = createMask_16_128 (0x00ff);
        Mask0101 = createMask_16_128 (0x0101);
        Maskffff = createMask_16_128 (0xffff);
        Maskff000000 = createMask_2x32_128 (0xff000000, 0xff000000);
        MaskAlpha = createMask_2x32_128 (0x00ff0000, 0x00000000);

        /* MMX constants */
        xMask565rgb = createMask_2x32_64 (0x000001f0, 0x003f001f);
        xMask565Unpack = createMask_2x32_64 (0x00000084, 0x04100840);

        xMask0080 = createMask_16_64 (0x0080);
        xMask00ff = createMask_16_64 (0x00ff);
        xMask0101 = createMask_16_64 (0x0101);
        xMaskAlpha = createMask_2x32_64 (0x00ff0000, 0x00000000);

        /* SSE code patch for fbcompose.c */
        pixman_composeFunctions.combineU[PIXMAN_OP_OVER] = sse2CombineOverU;
        pixman_composeFunctions.combineU[PIXMAN_OP_OVER_REVERSE] = sse2CombineOverReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_IN] = sse2CombineInU;
        pixman_composeFunctions.combineU[PIXMAN_OP_IN_REVERSE] = sse2CombineInReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_OUT] = sse2CombineOutU;

        pixman_composeFunctions.combineU[PIXMAN_OP_OUT_REVERSE] = sse2CombineOutReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ATOP] = sse2CombineAtopU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ATOP_REVERSE] = sse2CombineAtopReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_XOR] = sse2CombineXorU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ADD] = sse2CombineAddU;

        pixman_composeFunctions.combineU[PIXMAN_OP_SATURATE] = sse2CombineSaturateU;

        pixman_composeFunctions.combineC[PIXMAN_OP_SRC] = sse2CombineSrcC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OVER] = sse2CombineOverC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OVER_REVERSE] = sse2CombineOverReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_IN] = sse2CombineInC;
        pixman_composeFunctions.combineC[PIXMAN_OP_IN_REVERSE] = sse2CombineInReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OUT] = sse2CombineOutC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OUT_REVERSE] = sse2CombineOutReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ATOP] = sse2CombineAtopC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ATOP_REVERSE] = sse2CombineAtopReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_XOR] = sse2CombineXorC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ADD] = sse2CombineAddC;

        pixman_composeFunctions.combineMaskU = sse2CombineMaskU;
    }

    initialized = TRUE;

    _mm_empty();
}


/* -------------------------------------------------------------------------------------------------
 * fbCompositeSolid_nx8888
 */

void
fbCompositeSolid_nx8888sse2 (pixman_op_t op,
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
    uint32_t	*dstLine, *dst, d;
    uint16_t	w;
    int	dstStride;
    __m128i xmmSrc, xmmAlpha;
    __m128i xmmDst, xmmDstLo, xmmDstHi;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    if (src >> 24 == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);

    xmmSrc = expandPixel_32_1x128 (src);
    xmmAlpha = expandAlpha_1x128 (xmmSrc);

    while (height--)
    {
        dst = dstLine;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)dst);

        dstLine += dstStride;
        w = width;

        while (w && (unsigned long)dst & 15)
        {
            d = *dst;
            *dst++ = pack_1x64_32 (over_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                              _mm_movepi64_pi64 (xmmAlpha),
                                              unpack_32_1x64 (d)));
            w--;
        }

        cachePrefetch ((__m128i*)dst);

        while (w >= 4)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)dst);

            xmmDst = load128Aligned ((__m128i*)dst);

            unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);

            over_2x128 (xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, &xmmDstLo, &xmmDstHi);

            /* rebuid the 4 pixel data and save*/
            save128Aligned ((__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));

            w -= 4;
            dst += 4;
        }

        while (w)
        {
            d = *dst;
            *dst++ = pack_1x64_32 (over_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                              _mm_movepi64_pi64 (xmmAlpha),
                                              unpack_32_1x64 (d)));
            w--;
        }

    }
    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSolid_nx0565
 */
void
fbCompositeSolid_nx0565sse2 (pixman_op_t op,
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
    uint16_t	*dstLine, *dst, d;
    uint16_t	w;
    int	        dstStride;
    __m128i xmmSrc, xmmAlpha;
    __m128i xmmDst, xmmDst0, xmmDst1, xmmDst2, xmmDst3;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    if (src >> 24 == 0)
        return;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);

    xmmSrc = expandPixel_32_1x128 (src);
    xmmAlpha = expandAlpha_1x128 (xmmSrc);

    while (height--)
    {
        dst = dstLine;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)dst);

        dstLine += dstStride;
        w = width;

        while (w && (unsigned long)dst & 15)
        {
            d = *dst;
            *dst++ = pack565_32_16 (pack_1x64_32 (over_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                             _mm_movepi64_pi64 (xmmAlpha),
                                                             expand565_16_1x64 (d))));
            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)dst);

        while (w >= 8)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)dst);

            xmmDst = load128Aligned ((__m128i*)dst);

            unpack565_128_4x128 (xmmDst, &xmmDst0, &xmmDst1, &xmmDst2, &xmmDst3);

            over_2x128 (xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, &xmmDst0, &xmmDst1);
            over_2x128 (xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, &xmmDst2, &xmmDst3);

            xmmDst = pack565_4x128_128 (xmmDst0, xmmDst1, xmmDst2, xmmDst3);

            save128Aligned ((__m128i*)dst, xmmDst);

            dst += 8;
            w -= 8;
        }

        while (w--)
        {
            d = *dst;
            *dst++ = pack565_32_16 (pack_1x64_32 (over_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                             _mm_movepi64_pi64 (xmmAlpha),
                                                             expand565_16_1x64 (d))));
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSolidMask_nx8888x8888C
 */

void
fbCompositeSolidMask_nx8888x8888Csse2 (pixman_op_t op,
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
    uint32_t	*dstLine, d;
    uint32_t	*maskLine, m;
    uint32_t    packCmp;
    int	dstStride, maskStride;

    __m128i xmmSrc, xmmAlpha;
    __m128i xmmDst, xmmDstLo, xmmDstHi;
    __m128i xmmMask, xmmMaskLo, xmmMaskHi;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint32_t, maskStride, maskLine, 1);

    xmmSrc = _mm_unpacklo_epi8 (createMask_2x32_128 (src, src), _mm_setzero_si128 ());
    xmmAlpha = expandAlpha_1x128 (xmmSrc);

    while (height--)
    {
        int w = width;
        uint32_t *pm = (uint32_t *)maskLine;
        uint32_t *pd = (uint32_t *)dstLine;

        dstLine += dstStride;
        maskLine += maskStride;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)pd);
        cachePrefetch ((__m128i*)pm);

        while (w && (unsigned long)pd & 15)
        {
            m = *pm++;

            if (m)
            {
                d = *pd;

                *pd = pack_1x64_32 (inOver_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                 _mm_movepi64_pi64 (xmmAlpha),
                                                 unpack_32_1x64 (m),
                                                 unpack_32_1x64 (d)));
            }

            pd++;
            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)pd);
        cachePrefetch ((__m128i*)pm);

        while (w >= 4)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)pd);
            cachePrefetchNext ((__m128i*)pm);

            xmmMask = load128Unaligned ((__m128i*)pm);

            packCmp = _mm_movemask_epi8 (_mm_cmpeq_epi32 (xmmMask, _mm_setzero_si128()));

            /* if all bits in mask are zero, packCmp are equal to 0xffff */
            if (packCmp != 0xffff)
            {
                xmmDst = load128Aligned ((__m128i*)pd);

                unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);
                unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);

                inOver_2x128 (xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, xmmMaskLo, xmmMaskHi, &xmmDstLo, &xmmDstHi);

                save128Aligned ((__m128i*)pd, pack_2x128_128 (xmmDstLo, xmmDstHi));
            }

            pd += 4;
            pm += 4;
            w -= 4;
        }

        while (w)
        {
            m = *pm++;

            if (m)
            {
                d = *pd;

                *pd = pack_1x64_32 (inOver_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                 _mm_movepi64_pi64 (xmmAlpha),
                                                 unpack_32_1x64 (m),
                                                 unpack_32_1x64 (d)));
            }

            pd++;
            w--;
        }
    }

    _mm_empty();
}


/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrc_8888x8x8888
 */

void
fbCompositeSrc_8888x8x8888sse2 (pixman_op_t op,
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
    uint16_t	w;
    int	dstStride, srcStride;

    __m128i xmmMask;
    __m128i xmmSrc, xmmSrcLo, xmmSrcHi;
    __m128i xmmDst, xmmDstLo, xmmDstHi;
    __m128i xmmAlphaLo, xmmAlphaHi;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetSolid (pMask, mask, pDst->bits.format);

    xmmMask = createMask_16_128 (mask >> 24);

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        src = srcLine;
        srcLine += srcStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)dst);
        cachePrefetch ((__m128i*)src);

        while (w && (unsigned long)dst & 15)
        {
            uint32_t s = *src++;
            uint32_t d = *dst;

            __m64 ms = unpack_32_1x64 (s);

            *dst++ = pack_1x64_32 (inOver_1x64 (ms,
                                                 expandAlpha_1x64 (ms),
                                                 _mm_movepi64_pi64 (xmmMask),
                                                 unpack_32_1x64 (d)));

            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)dst);
        cachePrefetch ((__m128i*)src);

        while (w >= 4)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)dst);
            cachePrefetchNext ((__m128i*)src);

            xmmSrc = load128Unaligned ((__m128i*)src);
            xmmDst = load128Aligned ((__m128i*)dst);

            unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);
            unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);
            expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaLo, &xmmAlphaHi);

            inOver_2x128 (xmmSrcLo, xmmSrcHi, xmmAlphaLo, xmmAlphaHi, xmmMask, xmmMask, &xmmDstLo, &xmmDstHi);

            save128Aligned( (__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));

            dst += 4;
            src += 4;
            w -= 4;
        }

        while (w)
        {
            uint32_t s = *src++;
            uint32_t d = *dst;

            __m64 ms = unpack_32_1x64 (s);

            *dst++ = pack_1x64_32 (inOver_1x64 (ms,
                                                 expandAlpha_1x64 (ms),
                                                 _mm_movepi64_pi64 (xmmMask),
                                                 unpack_32_1x64 (d)));

            w--;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrc_x888xnx8888
 */
void
fbCompositeSrc_x888xnx8888sse2 (pixman_op_t op,
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
    int	dstStride, srcStride;
    uint16_t	w;

    __m128i xmmMask, xmmAlpha;
    __m128i xmmSrc, xmmSrcLo, xmmSrcHi;
    __m128i xmmDst, xmmDstLo, xmmDstHi;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetSolid (pMask, mask, pDst->bits.format);

    xmmMask = createMask_16_128 (mask >> 24);
    xmmAlpha = Mask00ff;

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        src = srcLine;
        srcLine += srcStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)dst);
        cachePrefetch ((__m128i*)src);

        while (w && (unsigned long)dst & 15)
        {
            uint32_t s = (*src++) | 0xff000000;
            uint32_t d = *dst;

            *dst++ = pack_1x64_32 (inOver_1x64 (unpack_32_1x64 (s),
                                                 _mm_movepi64_pi64 (xmmAlpha),
                                                 _mm_movepi64_pi64 (xmmMask),
                                                 unpack_32_1x64 (d)));

            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)dst);
        cachePrefetch ((__m128i*)src);

        while (w >= 4)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)dst);
            cachePrefetchNext ((__m128i*)src);

            xmmSrc = _mm_or_si128 (load128Unaligned ((__m128i*)src), Maskff000000);
            xmmDst = load128Aligned ((__m128i*)dst);

            unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);
            unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);

            inOver_2x128 (xmmSrcLo, xmmSrcHi, xmmAlpha, xmmAlpha, xmmMask, xmmMask, &xmmDstLo, &xmmDstHi);

            save128Aligned( (__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));

            dst += 4;
            src += 4;
            w -= 4;

        }

        while (w)
        {
            uint32_t s = (*src++) | 0xff000000;
            uint32_t d = *dst;

            *dst++ = pack_1x64_32 (inOver_1x64 (unpack_32_1x64 (s),
                                                 _mm_movepi64_pi64 (xmmAlpha),
                                                 _mm_movepi64_pi64 (xmmMask),
                                                 unpack_32_1x64 (d)));

            w--;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrc_8888x8888
 */
void
fbCompositeSrc_8888x8888sse2 (pixman_op_t op,
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
    int	        dstStride, srcStride;
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    dst = dstLine;
    src = srcLine;

    while (height--)
    {
        coreCombineOverUsse2 (dst, src, width);

        dst += dstStride;
        src += srcStride;
    }
    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrc_8888x0565
 */
static inline uint16_t
fbCompositeSrc_8888x0565pixel (uint32_t src, uint16_t dst)
{
    __m64       ms;

    ms = unpack_32_1x64 (src);
    return pack565_32_16( pack_1x64_32 (over_1x64 (ms,
                                                   expandAlpha_1x64 (ms),
                                                   expand565_16_1x64 (dst))));
}

void
fbCompositeSrc_8888x0565sse2 (pixman_op_t op,
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
    uint16_t	*dstLine, *dst, d;
    uint32_t	*srcLine, *src, s;
    int	dstStride, srcStride;
    uint16_t	w;

    __m128i xmmAlphaLo, xmmAlphaHi;
    __m128i xmmSrc, xmmSrcLo, xmmSrcHi;
    __m128i xmmDst, xmmDst0, xmmDst1, xmmDst2, xmmDst3;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

#if 0
    /* FIXME
     *
     * I copy the code from MMX one and keep the fixme.
     * If it's a problem there, probably is a problem here.
     */
    assert (pSrc->pDrawable == pMask->pDrawable);
#endif

    while (height--)
    {
        dst = dstLine;
        src = srcLine;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        dstLine += dstStride;
        srcLine += srcStride;
        w = width;

        /* Align dst on a 16-byte boundary */
        while (w &&
               ((unsigned long)dst & 15))
        {
            s = *src++;
            d = *dst;

            *dst++ = fbCompositeSrc_8888x0565pixel (s, d);
            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        /* It's a 8 pixel loop */
        while (w >= 8)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)src);
            cachePrefetchNext ((__m128i*)dst);

            /* I'm loading unaligned because I'm not sure about the address alignment. */
            xmmSrc = load128Unaligned ((__m128i*) src);
            xmmDst = load128Aligned ((__m128i*) dst);

            /* Unpacking */
            unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);
            unpack565_128_4x128 (xmmDst, &xmmDst0, &xmmDst1, &xmmDst2, &xmmDst3);
            expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaLo, &xmmAlphaHi);

            /* I'm loading next 4 pixels from memory before to optimze the memory read. */
            xmmSrc = load128Unaligned ((__m128i*) (src+4));

            over_2x128 (xmmSrcLo, xmmSrcHi, xmmAlphaLo, xmmAlphaHi, &xmmDst0, &xmmDst1);

            /* Unpacking */
            unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);
            expandAlpha_2x128 (xmmSrcLo, xmmSrcHi, &xmmAlphaLo, &xmmAlphaHi);

            over_2x128 (xmmSrcLo, xmmSrcHi, xmmAlphaLo, xmmAlphaHi, &xmmDst2, &xmmDst3);

            save128Aligned ((__m128i*)dst, pack565_4x128_128 (xmmDst0, xmmDst1, xmmDst2, xmmDst3));

            w -= 8;
            dst += 8;
            src += 8;
        }

        while (w--)
        {
            s = *src++;
            d = *dst;

            *dst++ = fbCompositeSrc_8888x0565pixel (s, d);
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSolidMask_nx8x8888
 */

void
fbCompositeSolidMask_nx8x8888sse2 (pixman_op_t op,
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
    uint32_t m, d;

    __m128i xmmSrc, xmmAlpha, xmmDef;
    __m128i xmmDst, xmmDstLo, xmmDstHi;
    __m128i xmmMask, xmmMaskLo, xmmMaskHi;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    xmmDef = createMask_2x32_128 (src, src);
    xmmSrc = expandPixel_32_1x128 (src);
    xmmAlpha = expandAlpha_1x128 (xmmSrc);

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        mask = maskLine;
        maskLine += maskStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w && (unsigned long)dst & 15)
        {
            uint8_t m = *mask++;

            if (m)
            {
                d = *dst;

                *dst = pack_1x64_32 (inOver_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                  _mm_movepi64_pi64 (xmmAlpha),
                                                  expandPixel_8_1x64 (m),
                                                  unpack_32_1x64 (d)));
            }

            w--;
            dst++;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w >= 4)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)mask);
            cachePrefetchNext ((__m128i*)dst);

            m = *((uint32_t*)mask);

            if (srca == 0xff && m == 0xffffffff)
            {
                save128Aligned ((__m128i*)dst, xmmDef);
            }
            else if (m)
            {
                xmmDst = load128Aligned ((__m128i*) dst);
                xmmMask = unpack_32_1x128 (m);
                xmmMask = _mm_unpacklo_epi8 (xmmMask, _mm_setzero_si128());

                /* Unpacking */
                unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);
                unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);

                expandAlphaRev_2x128 (xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

                inOver_2x128 (xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, xmmMaskLo, xmmMaskHi, &xmmDstLo, &xmmDstHi);

                save128Aligned ((__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));
            }

            w -= 4;
            dst += 4;
            mask += 4;
        }

        while (w)
        {
            uint8_t m = *mask++;

            if (m)
            {
                d = *dst;

                *dst = pack_1x64_32 (inOver_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                  _mm_movepi64_pi64 (xmmAlpha),
                                                  expandPixel_8_1x64 (m),
                                                  unpack_32_1x64 (d)));
            }

            w--;
            dst++;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSolidMask_nx8x8888
 */

pixman_bool_t
pixmanFillsse2 (uint32_t *bits,
		 int stride,
		 int bpp,
		 int x,
		 int y,
		 int width,
		 int height,
		 uint32_t data)
{
    uint32_t	byte_width;
    uint8_t	    *byte_line;

    __m128i xmmDef;

    if (bpp == 16 && (data >> 16 != (data & 0xffff)))
	return FALSE;

    if (bpp != 16 && bpp != 32)
	return FALSE;

    if (bpp == 16)
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

    cachePrefetch ((__m128i*)byte_line);
    xmmDef = createMask_2x32_128 (data, data);

    while (height--)
    {
        int w;
        uint8_t *d = byte_line;
        byte_line += stride;
        w = byte_width;


        cachePrefetchNext ((__m128i*)d);

        while (w >= 2 && ((unsigned long)d & 3))
        {
            *(uint16_t *)d = data;
            w -= 2;
            d += 2;
        }

        while (w >= 4 && ((unsigned long)d & 15))
        {
            *(uint32_t *)d = data;

            w -= 4;
            d += 4;
        }

        cachePrefetchNext ((__m128i*)d);

        while (w >= 128)
        {
            cachePrefetch (((__m128i*)d) + 12);

            save128Aligned ((__m128i*)(d),     xmmDef);
            save128Aligned ((__m128i*)(d+16),  xmmDef);
            save128Aligned ((__m128i*)(d+32),  xmmDef);
            save128Aligned ((__m128i*)(d+48),  xmmDef);
            save128Aligned ((__m128i*)(d+64),  xmmDef);
            save128Aligned ((__m128i*)(d+80),  xmmDef);
            save128Aligned ((__m128i*)(d+96),  xmmDef);
            save128Aligned ((__m128i*)(d+112), xmmDef);

            d += 128;
            w -= 128;
        }

        if (w >= 64)
        {
            cachePrefetch (((__m128i*)d) + 8);

            save128Aligned ((__m128i*)(d),     xmmDef);
            save128Aligned ((__m128i*)(d+16),  xmmDef);
            save128Aligned ((__m128i*)(d+32),  xmmDef);
            save128Aligned ((__m128i*)(d+48),  xmmDef);

            d += 64;
            w -= 64;
        }

        cachePrefetchNext ((__m128i*)d);

        if (w >= 32)
        {
            save128Aligned ((__m128i*)(d),     xmmDef);
            save128Aligned ((__m128i*)(d+16),  xmmDef);

            d += 32;
            w -= 32;
        }

        if (w >= 16)
        {
            save128Aligned ((__m128i*)(d),     xmmDef);

            d += 16;
            w -= 16;
        }

        cachePrefetchNext ((__m128i*)d);

        while (w >= 4)
        {
            *(uint32_t *)d = data;

            w -= 4;
            d += 4;
        }

        if (w >= 2)
        {
            *(uint16_t *)d = data;
            w -= 2;
            d += 2;
        }
    }

    _mm_empty();
    return TRUE;
}

void
fbCompositeSolidMaskSrc_nx8x8888sse2 (pixman_op_t op,
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
    uint32_t    m;

    __m128i xmmSrc, xmmDef;
    __m128i xmmMask, xmmMaskLo, xmmMaskHi;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
    {
        pixmanFillsse2 (pDst->bits.bits, pDst->bits.rowstride,
                        PIXMAN_FORMAT_BPP (pDst->bits.format),
                        xDst, yDst, width, height, 0);
        return;
    }

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    xmmDef = createMask_2x32_128 (src, src);
    xmmSrc = expandPixel_32_1x128 (src);

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        mask = maskLine;
        maskLine += maskStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w && (unsigned long)dst & 15)
        {
            uint8_t m = *mask++;

            if (m)
            {
                *dst = pack_1x64_32 (pixMultiply_1x64 (_mm_movepi64_pi64 (xmmSrc), expandPixel_8_1x64 (m)));
            }
            else
            {
                *dst = 0;
            }

            w--;
            dst++;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w >= 4)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)mask);
            cachePrefetchNext ((__m128i*)dst);

            m = *((uint32_t*)mask);

            if (srca == 0xff && m == 0xffffffff)
            {
                save128Aligned ((__m128i*)dst, xmmDef);
            }
            else if (m)
            {
                xmmMask = unpack_32_1x128 (m);
                xmmMask = _mm_unpacklo_epi8 (xmmMask, _mm_setzero_si128());

                /* Unpacking */
                unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);

                expandAlphaRev_2x128 (xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

                pixMultiply_2x128 (xmmSrc, xmmSrc, xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

                save128Aligned ((__m128i*)dst, pack_2x128_128 (xmmMaskLo, xmmMaskHi));
            }
            else
            {
                save128Aligned ((__m128i*)dst, _mm_setzero_si128());
            }

            w -= 4;
            dst += 4;
            mask += 4;
        }

        while (w)
        {
            uint8_t m = *mask++;

            if (m)
            {
                *dst = pack_1x64_32 (pixMultiply_1x64 (_mm_movepi64_pi64 (xmmSrc), expandPixel_8_1x64 (m)));
            }
            else
            {
                *dst = 0;
            }

            w--;
            dst++;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSolidMask_nx8x0565
 */

void
fbCompositeSolidMask_nx8x0565sse2 (pixman_op_t op,
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
    uint16_t	*dstLine, *dst, d;
    uint8_t	*maskLine, *mask;
    int	dstStride, maskStride;
    uint16_t	w;
    uint32_t m;

    __m128i xmmSrc, xmmAlpha;
    __m128i xmmMask, xmmMaskLo, xmmMaskHi;
    __m128i xmmDst, xmmDst0, xmmDst1, xmmDst2, xmmDst3;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    xmmSrc = expandPixel_32_1x128 (src);
    xmmAlpha = expandAlpha_1x128 (xmmSrc);

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        mask = maskLine;
        maskLine += maskStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w && (unsigned long)dst & 15)
        {
            m = *mask++;

            if (m)
            {
                d = *dst;

                *dst = pack565_32_16 (pack_1x64_32 (inOver_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                                 _mm_movepi64_pi64 (xmmAlpha),
                                                                 expandAlphaRev_1x64 (unpack_32_1x64 (m)),
                                                                 expand565_16_1x64 (d))));
            }

            w--;
            dst++;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w >= 8)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)mask);
            cachePrefetchNext ((__m128i*)dst);

            xmmDst = load128Aligned ((__m128i*) dst);
            unpack565_128_4x128 (xmmDst, &xmmDst0, &xmmDst1, &xmmDst2, &xmmDst3);

            m = *((uint32_t*)mask);
            mask += 4;

            if (m)
            {
                xmmMask = unpack_32_1x128 (m);
                xmmMask = _mm_unpacklo_epi8 (xmmMask, _mm_setzero_si128());

                /* Unpacking */
                unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);

                expandAlphaRev_2x128 (xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);
                inOver_2x128 (xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, xmmMaskLo, xmmMaskHi, &xmmDst0, &xmmDst1);
            }

            m = *((uint32_t*)mask);
            mask += 4;

            if (m)
            {
                xmmMask = unpack_32_1x128 (m);
                xmmMask = _mm_unpacklo_epi8 (xmmMask, _mm_setzero_si128());

                /* Unpacking */
                unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);

                expandAlphaRev_2x128 (xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);
                inOver_2x128 (xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, xmmMaskLo, xmmMaskHi, &xmmDst2, &xmmDst3);
            }

            save128Aligned ((__m128i*)dst, pack565_4x128_128 (xmmDst0, xmmDst1, xmmDst2, xmmDst3));

            w -= 8;
            dst += 8;
        }

        while (w)
        {
            m = *mask++;

            if (m)
            {
                d = *dst;

                *dst = pack565_32_16 (pack_1x64_32 (inOver_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                                 _mm_movepi64_pi64 (xmmAlpha),
                                                                 expandAlphaRev_1x64 (unpack_32_1x64 (m)),
                                                                 expand565_16_1x64 (d))));
            }

            w--;
            dst++;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrc_8888RevNPx0565
 */

void
fbCompositeSrc_8888RevNPx0565sse2 (pixman_op_t op,
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
    uint16_t	*dstLine, *dst, d;
    uint32_t	*srcLine, *src, s;
    int	dstStride, srcStride;
    uint16_t	w;
    uint32_t    packCmp;

    __m64 ms;
    __m128i xmmSrc, xmmSrcLo, xmmSrcHi;
    __m128i xmmDst, xmmDst0, xmmDst1, xmmDst2, xmmDst3;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

#if 0
    /* FIXME
     *
     * I copy the code from MMX one and keep the fixme.
     * If it's a problem there, probably is a problem here.
     */
    assert (pSrc->pDrawable == pMask->pDrawable);
#endif

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        src = srcLine;
        srcLine += srcStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        while (w && (unsigned long)dst & 15)
        {
            s = *src++;
            d = *dst;

            ms = unpack_32_1x64 (s);

            *dst++ = pack565_32_16 (pack_1x64_32 (overRevNonPre_1x64(ms, expand565_16_1x64 (d))));
            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        while (w >= 8)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)src);
            cachePrefetchNext ((__m128i*)dst);

            /* First round */
            xmmSrc = load128Unaligned((__m128i*)src);
            xmmDst = load128Aligned  ((__m128i*)dst);

            packCmp = packAlpha (xmmSrc);

            unpack565_128_4x128 (xmmDst, &xmmDst0, &xmmDst1, &xmmDst2, &xmmDst3);
            unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);

            /* preload next round*/
            xmmSrc = load128Unaligned((__m128i*)(src+4));
            /* preload next round*/

            if (packCmp == 0xffffffff)
            {
                invertColors_2x128 (xmmSrcLo, xmmSrcHi, &xmmDst0, &xmmDst1);
            }
            else if (packCmp)
            {
                overRevNonPre_2x128 (xmmSrcLo, xmmSrcHi, &xmmDst0, &xmmDst1);
            }

            /* Second round */
            packCmp = packAlpha (xmmSrc);

            unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);

            if (packCmp == 0xffffffff)
            {
                invertColors_2x128 (xmmSrcLo, xmmSrcHi, &xmmDst2, &xmmDst3);
            }
            else if (packCmp)
            {
                overRevNonPre_2x128 (xmmSrcLo, xmmSrcHi, &xmmDst2, &xmmDst3);
            }

            save128Aligned ((__m128i*)dst, pack565_4x128_128 (xmmDst0, xmmDst1, xmmDst2, xmmDst3));

            w -= 8;
            src += 8;
            dst += 8;
        }

        while (w)
        {
            s = *src++;
            d = *dst;

            ms = unpack_32_1x64 (s);

            *dst++ = pack565_32_16 (pack_1x64_32 (overRevNonPre_1x64(ms, expand565_16_1x64 (d))));
            w--;
        }
    }

    _mm_empty();
}

/* "8888RevNP" is GdkPixbuf's format: ABGR, non premultiplied */

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrc_8888RevNPx8888
 */

void
fbCompositeSrc_8888RevNPx8888sse2 (pixman_op_t op,
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
    uint32_t	*dstLine, *dst, d;
    uint32_t	*srcLine, *src, s;
    int	dstStride, srcStride;
    uint16_t	w;
    uint32_t    packCmp;

    __m128i xmmSrcLo, xmmSrcHi;
    __m128i xmmDstLo, xmmDstHi;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

#if 0
    /* FIXME
     *
     * I copy the code from MMX one and keep the fixme.
     * If it's a problem there, probably is a problem here.
     */
    assert (pSrc->pDrawable == pMask->pDrawable);
#endif

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        src = srcLine;
        srcLine += srcStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        while (w && (unsigned long)dst & 15)
        {
            s = *src++;
            d = *dst;

            *dst++ = pack_1x64_32 (overRevNonPre_1x64 (unpack_32_1x64 (s), unpack_32_1x64 (d)));

            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        while (w >= 4)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)src);
            cachePrefetchNext ((__m128i*)dst);

            xmmSrcHi = load128Unaligned((__m128i*)src);

            packCmp = packAlpha (xmmSrcHi);

            unpack_128_2x128 (xmmSrcHi, &xmmSrcLo, &xmmSrcHi);

            if (packCmp == 0xffffffff)
            {
                invertColors_2x128( xmmSrcLo, xmmSrcHi, &xmmDstLo, &xmmDstHi);

                save128Aligned ((__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));
            }
            else if (packCmp)
            {
                xmmDstHi = load128Aligned  ((__m128i*)dst);

                unpack_128_2x128 (xmmDstHi, &xmmDstLo, &xmmDstHi);

                overRevNonPre_2x128 (xmmSrcLo, xmmSrcHi, &xmmDstLo, &xmmDstHi);

                save128Aligned ((__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));
            }

            w -= 4;
            dst += 4;
            src += 4;
        }

        while (w)
        {
            s = *src++;
            d = *dst;

            *dst++ = pack_1x64_32 (overRevNonPre_1x64 (unpack_32_1x64 (s), unpack_32_1x64 (d)));

            w--;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSolidMask_nx8888x0565C
 */

void
fbCompositeSolidMask_nx8888x0565Csse2 (pixman_op_t op,
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
    uint16_t	*dstLine, *dst, d;
    uint32_t	*maskLine, *mask, m;
    int	dstStride, maskStride;
    int w;
    uint32_t packCmp;

    __m128i xmmSrc, xmmAlpha;
    __m128i xmmMask, xmmMaskLo, xmmMaskHi;
    __m128i xmmDst, xmmDst0, xmmDst1, xmmDst2, xmmDst3;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (srca == 0)
        return;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint32_t, maskStride, maskLine, 1);

    xmmSrc = expandPixel_32_1x128 (src);
    xmmAlpha = expandAlpha_1x128 (xmmSrc);

    while (height--)
    {
        w = width;
        mask = maskLine;
        dst = dstLine;
        maskLine += maskStride;
        dstLine += dstStride;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w && ((unsigned long)dst & 15))
        {
            m = *(uint32_t *) mask;

            if (m)
            {
                d = *dst;

                *dst = pack565_32_16 (pack_1x64_32 (inOver_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                                   _mm_movepi64_pi64 (xmmAlpha),
                                                                   unpack_32_1x64 (m),
                                                                   expand565_16_1x64 (d))));
            }

            w--;
            dst++;
            mask++;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w >= 8)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)mask);
            cachePrefetchNext ((__m128i*)dst);

            /* First round */
            xmmMask = load128Unaligned((__m128i*)mask);
            xmmDst = load128Aligned((__m128i*)dst);

            packCmp = _mm_movemask_epi8 (_mm_cmpeq_epi32 (xmmMask, _mm_setzero_si128()));

            unpack565_128_4x128 (xmmDst, &xmmDst0, &xmmDst1, &xmmDst2, &xmmDst3);
            unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);

            /* preload next round*/
            xmmMask = load128Unaligned((__m128i*)(mask+4));
            /* preload next round*/

            if (packCmp != 0xffff)
            {
                inOver_2x128(xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, xmmMaskLo, xmmMaskHi, &xmmDst0, &xmmDst1);
            }

            /* Second round */
            packCmp = _mm_movemask_epi8 (_mm_cmpeq_epi32 (xmmMask, _mm_setzero_si128()));

            unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);

            if (packCmp != 0xffff)
            {
                inOver_2x128(xmmSrc, xmmSrc, xmmAlpha, xmmAlpha, xmmMaskLo, xmmMaskHi, &xmmDst2, &xmmDst3);
            }

            save128Aligned ((__m128i*)dst, pack565_4x128_128 (xmmDst0, xmmDst1, xmmDst2, xmmDst3));

            w -= 8;
            dst += 8;
            mask += 8;
        }

        while (w)
        {
            m = *(uint32_t *) mask;

            if (m)
            {
                d = *dst;

                *dst = pack565_32_16 (pack_1x64_32 (inOver_1x64 (_mm_movepi64_pi64 (xmmSrc),
                                                                   _mm_movepi64_pi64 (xmmAlpha),
                                                                   unpack_32_1x64 (m),
                                                                   expand565_16_1x64 (d))));
            }

            w--;
            dst++;
            mask++;
        }
    }

    _mm_empty ();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeIn_nx8x8
 */

void
fbCompositeIn_nx8x8sse2 (pixman_op_t op,
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
    uint16_t	w, d, m;
    uint32_t	src;
    uint8_t	sa;

    __m128i xmmAlpha;
    __m128i xmmMask, xmmMaskLo, xmmMaskHi;
    __m128i xmmDst, xmmDstLo, xmmDstHi;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    sa = src >> 24;
    if (sa == 0)
        return;

    xmmAlpha = expandAlpha_1x128 (expandPixel_32_1x128 (src));

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        mask = maskLine;
        maskLine += maskStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w && ((unsigned long)dst & 15))
        {
            m = (uint32_t) *mask++;
            d = (uint32_t) *dst;

            *dst++ = (uint8_t) pack_1x64_32 (pixMultiply_1x64 (pixMultiply_1x64 (_mm_movepi64_pi64 (xmmAlpha), unpack_32_1x64 (m)),
                                                               unpack_32_1x64 (d)));
            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w >= 16)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)mask);
            cachePrefetchNext ((__m128i*)dst);

            xmmMask = load128Unaligned((__m128i*)mask);
            xmmDst = load128Aligned((__m128i*)dst);

            unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);
            unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);

            pixMultiply_2x128 (xmmAlpha, xmmAlpha, xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);
            pixMultiply_2x128 (xmmMaskLo, xmmMaskHi, xmmDstLo, xmmDstHi, &xmmDstLo, &xmmDstHi);

            save128Aligned ((__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));

            mask += 16;
            dst += 16;
            w -= 16;
        }

        while (w)
        {
            m = (uint32_t) *mask++;
            d = (uint32_t) *dst;

            *dst++ = (uint8_t) pack_1x64_32 (pixMultiply_1x64 (pixMultiply_1x64 (_mm_movepi64_pi64 (xmmAlpha), unpack_32_1x64 (m)),
                                                               unpack_32_1x64 (d)));
            w--;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeIn_8x8
 */

void
fbCompositeIn_8x8sse2 (pixman_op_t op,
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
    uint32_t    s, d;

    __m128i xmmSrc, xmmSrcLo, xmmSrcHi;
    __m128i xmmDst, xmmDstLo, xmmDstHi;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        src = srcLine;
        srcLine += srcStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        while (w && ((unsigned long)dst & 15))
        {
            s = (uint32_t) *src++;
            d = (uint32_t) *dst;

            *dst++ = (uint8_t) pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (s),unpack_32_1x64 (d)));
            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        while (w >= 16)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)src);
            cachePrefetchNext ((__m128i*)dst);

            xmmSrc = load128Unaligned((__m128i*)src);
            xmmDst = load128Aligned((__m128i*)dst);

            unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);
            unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);

            pixMultiply_2x128 (xmmSrcLo, xmmSrcHi, xmmDstLo, xmmDstHi, &xmmDstLo, &xmmDstHi);

            save128Aligned ((__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));

            src += 16;
            dst += 16;
            w -= 16;
        }

        while (w)
        {
            s = (uint32_t) *src++;
            d = (uint32_t) *dst;

            *dst++ = (uint8_t) pack_1x64_32 (pixMultiply_1x64 (unpack_32_1x64 (s),unpack_32_1x64 (d)));
            w--;
        }
    }

    _mm_empty ();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrcAdd_8888x8x8
 */

void
fbCompositeSrcAdd_8888x8x8sse2 (pixman_op_t op,
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
    uint32_t m, d;

    __m128i xmmAlpha;
    __m128i xmmMask, xmmMaskLo, xmmMaskHi;
    __m128i xmmDst, xmmDstLo, xmmDstHi;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    sa = src >> 24;
    if (sa == 0)
        return;

    xmmAlpha = expandAlpha_1x128 (expandPixel_32_1x128 (src));

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        mask = maskLine;
        maskLine += maskStride;
        w = width;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w && ((unsigned long)dst & 15))
        {
            m = (uint32_t) *mask++;
            d = (uint32_t) *dst;

            *dst++ = (uint8_t) pack_1x64_32 (_mm_adds_pu16 (pixMultiply_1x64 (_mm_movepi64_pi64 (xmmAlpha), unpack_32_1x64 (m)),
                                                                              unpack_32_1x64 (d)));
            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)mask);
        cachePrefetch ((__m128i*)dst);

        while (w >= 16)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)mask);
            cachePrefetchNext ((__m128i*)dst);

            xmmMask = load128Unaligned((__m128i*)mask);
            xmmDst = load128Aligned((__m128i*)dst);

            unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);
            unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);

            pixMultiply_2x128 (xmmAlpha, xmmAlpha, xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

            xmmDstLo = _mm_adds_epu16 (xmmMaskLo, xmmDstLo);
            xmmDstHi = _mm_adds_epu16 (xmmMaskHi, xmmDstHi);

            save128Aligned ((__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));

            mask += 16;
            dst += 16;
            w -= 16;
        }

        while (w)
        {
            m = (uint32_t) *mask++;
            d = (uint32_t) *dst;

            *dst++ = (uint8_t) pack_1x64_32 (_mm_adds_pu16 (pixMultiply_1x64 (_mm_movepi64_pi64 (xmmAlpha), unpack_32_1x64 (m)),
                                                                              unpack_32_1x64 (d)));
            w--;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrcAdd_8000x8000
 */

void
fbCompositeSrcAdd_8000x8000sse2 (pixman_op_t op,
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
    uint16_t	t;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);

    while (height--)
    {
        dst = dstLine;
        src = srcLine;

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);

        dstLine += dstStride;
        srcLine += srcStride;
        w = width;

        /* Small head */
        while (w && (unsigned long)dst & 3)
        {
            t = (*dst) + (*src++);
            *dst++ = t | (0 - (t >> 8));
            w--;
        }

        coreCombineAddUsse2 ((uint32_t*)dst, (uint32_t*)src, w >> 2);

        /* Small tail */
        dst += w & 0xfffc;
        src += w & 0xfffc;

        w &= 3;

        while (w)
        {
            t = (*dst) + (*src++);
            *dst++ = t | (0 - (t >> 8));
            w--;
        }
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeSrcAdd_8888x8888
 */
void
fbCompositeSrcAdd_8888x8888sse2 (pixman_op_t 	op,
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
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    int	dstStride, srcStride;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);

    while (height--)
    {
        dst = dstLine;
        dstLine += dstStride;
        src = srcLine;
        srcLine += srcStride;

        coreCombineAddUsse2 (dst, src, width);
    }

    _mm_empty();
}

/* -------------------------------------------------------------------------------------------------
 * fbCompositeCopyAreasse2
 */

pixman_bool_t
pixmanBltsse2 (uint32_t *src_bits,
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

    cachePrefetch ((__m128i*)src_bytes);
    cachePrefetch ((__m128i*)dst_bytes);

    while (height--)
    {
        int w;
        uint8_t *s = src_bytes;
        uint8_t *d = dst_bytes;
        src_bytes += src_stride;
        dst_bytes += dst_stride;
        w = byte_width;

        cachePrefetchNext ((__m128i*)s);
        cachePrefetchNext ((__m128i*)d);

        while (w >= 2 && ((unsigned long)d & 3))
        {
            *(uint16_t *)d = *(uint16_t *)s;
            w -= 2;
            s += 2;
            d += 2;
        }

        while (w >= 4 && ((unsigned long)d & 15))
        {
            *(uint32_t *)d = *(uint32_t *)s;

            w -= 4;
            s += 4;
            d += 4;
        }

        cachePrefetchNext ((__m128i*)s);
        cachePrefetchNext ((__m128i*)d);

        while (w >= 64)
        {
            /* 128 bytes ahead */
            cachePrefetch (((__m128i*)s) + 8);
            cachePrefetch (((__m128i*)d) + 8);

            __m128i xmm0, xmm1, xmm2, xmm3;

            xmm0 = load128Unaligned ((__m128i*)(s));
            xmm1 = load128Unaligned ((__m128i*)(s+16));
            xmm2 = load128Unaligned ((__m128i*)(s+32));
            xmm3 = load128Unaligned ((__m128i*)(s+48));

            save128Aligned ((__m128i*)(d),    xmm0);
            save128Aligned ((__m128i*)(d+16), xmm1);
            save128Aligned ((__m128i*)(d+32), xmm2);
            save128Aligned ((__m128i*)(d+48), xmm3);

            s += 64;
            d += 64;
            w -= 64;
        }

        cachePrefetchNext ((__m128i*)s);
        cachePrefetchNext ((__m128i*)d);

        while (w >= 16)
        {
            save128Aligned ((__m128i*)d, load128Unaligned ((__m128i*)s) );

            w -= 16;
            d += 16;
            s += 16;
        }

        cachePrefetchNext ((__m128i*)s);
        cachePrefetchNext ((__m128i*)d);

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
fbCompositeCopyAreasse2 (pixman_op_t       op,
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
    pixmanBltsse2 (pSrc->bits.bits,
		    pDst->bits.bits,
		    pSrc->bits.rowstride,
		    pDst->bits.rowstride,
		    PIXMAN_FORMAT_BPP (pSrc->bits.format),
		    PIXMAN_FORMAT_BPP (pDst->bits.format),
		    xSrc, ySrc, xDst, yDst, width, height);
}

#if 0
/* This code are buggy in MMX version, now the bug was translated to SSE2 version */
void
fbCompositeOver_x888x8x8888sse2 (pixman_op_t      op,
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
    uint32_t	*src, *srcLine, s;
    uint32_t    *dst, *dstLine, d;
    uint8_t	    *mask, *maskLine;
    uint32_t    m;
    int		 srcStride, maskStride, dstStride;
    uint16_t w;

    __m128i xmmSrc, xmmSrcLo, xmmSrcHi;
    __m128i xmmDst, xmmDstLo, xmmDstHi;
    __m128i xmmMask, xmmMaskLo, xmmMaskHi;

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

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);
        cachePrefetch ((__m128i*)mask);

        while (w && (unsigned long)dst & 15)
        {
            s = 0xff000000 | *src++;
            m = (uint32_t) *mask++;
            d = *dst;

            __m64 ms = unpack_32_1x64 (s);

            if (m != 0xff)
            {
                ms = inOver_1x64 (ms,
                                  xMask00ff,
                                  expandAlphaRev_1x64 (unpack_32_1x64 (m)),
                                  unpack_32_1x64 (d));
            }

            *dst++ = pack_1x64_32 (ms);
            w--;
        }

        /* call prefetch hint to optimize cache load*/
        cachePrefetch ((__m128i*)src);
        cachePrefetch ((__m128i*)dst);
        cachePrefetch ((__m128i*)mask);

        while (w >= 4)
        {
            /* fill cache line with next memory */
            cachePrefetchNext ((__m128i*)src);
            cachePrefetchNext ((__m128i*)dst);
            cachePrefetchNext ((__m128i*)mask);

            m = *(uint32_t*) mask;
            xmmSrc = _mm_or_si128 (load128Unaligned ((__m128i*)src), Maskff000000);

            if (m == 0xffffffff)
            {
                save128Aligned ((__m128i*)dst, xmmSrc);
            }
            else
            {
                xmmDst = load128Aligned ((__m128i*)dst);

                xmmMask = _mm_unpacklo_epi16 (unpack_32_1x128 (m), _mm_setzero_si128());

                unpack_128_2x128 (xmmSrc, &xmmSrcLo, &xmmSrcHi);
                unpack_128_2x128 (xmmMask, &xmmMaskLo, &xmmMaskHi);
                unpack_128_2x128 (xmmDst, &xmmDstLo, &xmmDstHi);

                expandAlphaRev_2x128 (xmmMaskLo, xmmMaskHi, &xmmMaskLo, &xmmMaskHi);

                inOver_2x128 (xmmSrcLo, xmmSrcHi, Mask00ff, Mask00ff, xmmMaskLo, xmmMaskHi, &xmmDstLo, &xmmDstHi);

                save128Aligned( (__m128i*)dst, pack_2x128_128 (xmmDstLo, xmmDstHi));
            }

            src += 4;
            dst += 4;
            mask += 4;
            w -= 4;
        }

        while (w)
        {
            m = (uint32_t) *mask++;

            if (m)
            {
                s = 0xff000000 | *src;

                if (m == 0xff)
                {
                    *dst = s;
                }
                else
                {
                    d = *dst;

                    *dst = pack_1x64_32 (inOver_1x64 (unpack_32_1x64 (s),
                                                      xMask00ff,
                                                      expandAlphaRev_1x64 (unpack_32_1x64 (m)),
                                                      unpack_32_1x64 (d)));
                }

            }

            src++;
            dst++;
            w--;
        }
    }

    _mm_empty();
}
#endif /* #if 0 */

#endif /* USE_SSE2 */
