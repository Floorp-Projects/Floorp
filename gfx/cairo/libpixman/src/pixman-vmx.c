/*
 * Copyright © 2007 Luca Barbato
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Luca Barbato not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Luca Barbato makes no representations about the
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
 * Author:  Luca Barbato (lu_zero@gentoo.org)
 *
 * Based on fbmmx.c by Owen Taylor, Søren Sandmann and Nicholas Miell
 */

#include <config.h>
#include "pixman-vmx.h"
#include "pixman-combine32.h"
#include <altivec.h>

#ifdef __GNUC__
#   define inline __inline__ __attribute__ ((__always_inline__))
#endif

static inline vector unsigned int
splat_alpha (vector unsigned int pix) {
    return vec_perm (pix, pix,
    (vector unsigned char)AVV(0x00,0x00,0x00,0x00, 0x04,0x04,0x04,0x04,
                               0x08,0x08,0x08,0x08, 0x0C,0x0C,0x0C,0x0C));
}

static inline vector unsigned int
pix_multiply (vector unsigned int p, vector unsigned int a)
{
    vector unsigned short hi, lo, mod;
    /* unpack to short */
    hi = (vector unsigned short)
                    vec_mergeh ((vector unsigned char)AVV(0),
                                (vector unsigned char)p);
    mod = (vector unsigned short)
                    vec_mergeh ((vector unsigned char)AVV(0),
                                (vector unsigned char)a);

    hi = vec_mladd (hi, mod, (vector unsigned short)
                            AVV(0x0080,0x0080,0x0080,0x0080,
                                 0x0080,0x0080,0x0080,0x0080));

    hi = vec_adds (hi, vec_sr (hi, vec_splat_u16 (8)));

    hi = vec_sr (hi, vec_splat_u16 (8));

    /* unpack to short */
    lo = (vector unsigned short)
                    vec_mergel ((vector unsigned char)AVV(0),
                                (vector unsigned char)p);
    mod = (vector unsigned short)
                    vec_mergel ((vector unsigned char)AVV(0),
                                (vector unsigned char)a);

    lo = vec_mladd (lo, mod, (vector unsigned short)
                            AVV(0x0080,0x0080,0x0080,0x0080,
                                 0x0080,0x0080,0x0080,0x0080));

    lo = vec_adds (lo, vec_sr (lo, vec_splat_u16 (8)));

    lo = vec_sr (lo, vec_splat_u16 (8));

    return (vector unsigned int)vec_packsu (hi, lo);
}

static inline vector unsigned int
pix_add (vector unsigned int a, vector unsigned int b)
{
    return (vector unsigned int)vec_adds ((vector unsigned char)a,
                     (vector unsigned char)b);
}

static inline vector unsigned int
pix_add_mul (vector unsigned int x, vector unsigned int a,
             vector unsigned int y, vector unsigned int b)
{
    vector unsigned short hi, lo, mod, hiy, loy, mody;

    hi = (vector unsigned short)
                    vec_mergeh ((vector unsigned char)AVV(0),
                                (vector unsigned char)x);
    mod = (vector unsigned short)
                    vec_mergeh ((vector unsigned char)AVV(0),
                                (vector unsigned char)a);
    hiy = (vector unsigned short)
                    vec_mergeh ((vector unsigned char)AVV(0),
                                (vector unsigned char)y);
    mody = (vector unsigned short)
                    vec_mergeh ((vector unsigned char)AVV(0),
                                (vector unsigned char)b);

    hi = vec_mladd (hi, mod, (vector unsigned short)
                             AVV(0x0080,0x0080,0x0080,0x0080,
                                  0x0080,0x0080,0x0080,0x0080));

    hi = vec_mladd (hiy, mody, hi);

    hi = vec_adds (hi, vec_sr (hi, vec_splat_u16 (8)));

    hi = vec_sr (hi, vec_splat_u16 (8));

    lo = (vector unsigned short)
                    vec_mergel ((vector unsigned char)AVV(0),
                                (vector unsigned char)x);
    mod = (vector unsigned short)
                    vec_mergel ((vector unsigned char)AVV(0),
                                (vector unsigned char)a);

    loy = (vector unsigned short)
                    vec_mergel ((vector unsigned char)AVV(0),
                                (vector unsigned char)y);
    mody = (vector unsigned short)
                    vec_mergel ((vector unsigned char)AVV(0),
                                (vector unsigned char)b);

    lo = vec_mladd (lo, mod, (vector unsigned short)
                             AVV(0x0080,0x0080,0x0080,0x0080,
                                  0x0080,0x0080,0x0080,0x0080));

    lo = vec_mladd (loy, mody, lo);

    lo = vec_adds (lo, vec_sr (lo, vec_splat_u16 (8)));

    lo = vec_sr (lo, vec_splat_u16 (8));

    return (vector unsigned int)vec_packsu (hi, lo);
}

static inline vector unsigned int
negate (vector unsigned int src)
{
    return vec_nor (src, src);
}
/* dest*~srca + src */
static inline vector unsigned int
over (vector unsigned int src, vector unsigned int srca,
      vector unsigned int dest)
{
    vector unsigned char tmp = (vector unsigned char)
                                pix_multiply (dest, negate (srca));
    tmp = vec_adds ((vector unsigned char)src, tmp);
    return (vector unsigned int)tmp;
}

/* in == pix_multiply */
#define in_over(src, srca, mask, dest) over (pix_multiply (src, mask),\
                                             pix_multiply (srca, mask), dest)


#define COMPUTE_SHIFT_MASK(source) \
    source ## _mask = vec_lvsl (0, source);

#define COMPUTE_SHIFT_MASKS(dest, source) \
    dest ## _mask = vec_lvsl (0, dest); \
    source ## _mask = vec_lvsl (0, source); \
    store_mask = vec_lvsr (0, dest);

#define COMPUTE_SHIFT_MASKC(dest, source, mask) \
    mask ## _mask = vec_lvsl (0, mask); \
    dest ## _mask = vec_lvsl (0, dest); \
    source ## _mask = vec_lvsl (0, source); \
    store_mask = vec_lvsr (0, dest);

/* notice you have to declare temp vars...
 * Note: tmp3 and tmp4 must remain untouched!
 */

#define LOAD_VECTORS(dest, source) \
        tmp1 = (typeof(tmp1))vec_ld(0, source); \
        tmp2 = (typeof(tmp2))vec_ld(15, source); \
        tmp3 = (typeof(tmp3))vec_ld(0, dest); \
        v ## source = (typeof(v ## source)) \
                       vec_perm(tmp1, tmp2, source ## _mask); \
        tmp4 = (typeof(tmp4))vec_ld(15, dest); \
        v ## dest = (typeof(v ## dest)) \
                     vec_perm(tmp3, tmp4, dest ## _mask);

#define LOAD_VECTORSC(dest, source, mask) \
        tmp1 = (typeof(tmp1))vec_ld(0, source); \
        tmp2 = (typeof(tmp2))vec_ld(15, source); \
        tmp3 = (typeof(tmp3))vec_ld(0, dest); \
        v ## source = (typeof(v ## source)) \
                       vec_perm(tmp1, tmp2, source ## _mask); \
        tmp4 = (typeof(tmp4))vec_ld(15, dest); \
        tmp1 = (typeof(tmp1))vec_ld(0, mask); \
        v ## dest = (typeof(v ## dest)) \
                     vec_perm(tmp3, tmp4, dest ## _mask); \
        tmp2 = (typeof(tmp2))vec_ld(15, mask); \
        v ## mask = (typeof(v ## mask)) \
                     vec_perm(tmp1, tmp2, mask ## _mask);
#define STORE_VECTOR(dest) \
        edges = vec_perm (tmp4, tmp3, dest ## _mask); \
        tmp3 = vec_perm ((vector unsigned char)v ## dest, edges, store_mask); \
        tmp1 = vec_perm (edges, (vector unsigned char)v ## dest, store_mask); \
        vec_st ((vector unsigned int) tmp3, 15, dest ); \
        vec_st ((vector unsigned int) tmp1, 0, dest );

static FASTCALL void
vmxCombineMaskU (uint32_t *src, const uint32_t *msk, int width)
{
    int i;
    vector unsigned int  vsrc, vmsk;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         src_mask, msk_mask, store_mask;

    COMPUTE_SHIFT_MASKS(src, msk)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(src, msk)

        vsrc = pix_multiply (vsrc, splat_alpha (vmsk));

        STORE_VECTOR(src)

        msk+=4;
        src+=4;
    }

    for (i = width%4; --i >= 0;) {
        uint32_t a = msk[i] >> 24;
        uint32_t s = src[i];
        FbByteMul (s, a);
        src[i] = s;
    }
}

static FASTCALL void
vmxCombineOverU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = over (vsrc, splat_alpha (vsrc), vdest);

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t ia = Alpha (~s);

        FbByteMulAdd (d, ia, s);
        dest[i] = d;
    }
}


static FASTCALL void
vmxCombineOverReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = over (vdest, splat_alpha (vdest) , vsrc);

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t ia = Alpha (~dest[i]);

        FbByteMulAdd (s, ia, d);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineInU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = pix_multiply (vsrc, splat_alpha (vdest));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {

        uint32_t s = src[i];
        uint32_t a = Alpha (dest[i]);
        FbByteMul (s, a);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineInReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = pix_multiply (vdest, splat_alpha (vsrc));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t d = dest[i];
        uint32_t a = Alpha (src[i]);
        FbByteMul (d, a);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineOutU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = pix_multiply (vsrc, splat_alpha (negate (vdest)));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t s = src[i];
        uint32_t a = Alpha (~dest[i]);
        FbByteMul (s, a);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineOutReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = pix_multiply (vdest, splat_alpha (negate (vsrc)));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t d = dest[i];
        uint32_t a = Alpha (~src[i]);
        FbByteMul (d, a);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineAtopU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = pix_add_mul (vsrc, splat_alpha (vdest),
                            vdest, splat_alpha (negate (vsrc)));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t dest_a = Alpha (d);
        uint32_t src_ia = Alpha (~s);

        FbByteAddMul (s, dest_a, d, src_ia);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineAtopReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = pix_add_mul (vdest, splat_alpha (vsrc),
                            vsrc, splat_alpha (negate (vdest)));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t src_a = Alpha (s);
        uint32_t dest_ia = Alpha (~d);

        FbByteAddMul (s, dest_ia, d, src_a);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineXorU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS (dest, src)

        vdest = pix_add_mul (vsrc, splat_alpha (negate (vdest)),
                            vdest, splat_alpha (negate (vsrc)));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t src_ia = Alpha (~s);
        uint32_t dest_ia = Alpha (~d);

        FbByteAddMul (s, dest_ia, d, src_ia);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineAddU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    vector unsigned int  vdest, vsrc;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKS(dest, src)
    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORS(dest, src)

        vdest = pix_add (vsrc, vdest);

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t s = src[i];
        uint32_t d = dest[i];
        FbByteAdd (d, s);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineSrcC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask);
    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_multiply (vsrc, vmask);

        STORE_VECTOR(dest)

        mask+=4;
        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        FbByteMulC (s, a);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineOverC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask);
    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = in_over (vsrc, splat_alpha (vsrc), vmask, vdest);

        STORE_VECTOR(dest)

        mask+=4;
        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t d = dest[i];
        FbByteMulC (s, a);
        FbByteMulAddC (d, ~a, s);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineOverReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask);
    /* printf("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC (dest, src, mask)

        vdest = over (vdest, splat_alpha (vdest), pix_multiply (vsrc, vmask));

        STORE_VECTOR(dest)

        mask+=4;
        src+=4;
        dest+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t da = Alpha (d);
        FbByteMulC (s, a);
        FbByteMulAddC (s, ~da, d);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineInC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_multiply (pix_multiply (vsrc, vmask), splat_alpha (vdest));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
        mask+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t da = Alpha (dest[i]);
        FbByteMul (s, a);
        FbByteMul (s, da);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineInReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_multiply (vdest, pix_multiply (vmask, splat_alpha (vsrc)));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
        mask+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t d = dest[i];
        uint32_t sa = Alpha (src[i]);
        FbByteMul (a, sa);
        FbByteMulC (d, a);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineOutC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_multiply (pix_multiply (vsrc, vmask), splat_alpha (vdest));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
        mask+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t da = Alpha (~d);
        FbByteMulC (s, a);
        FbByteMulC (s, da);
        dest[i] = s;
    }
}

static FASTCALL void
vmxCombineOutReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_multiply (vdest,
                             negate (pix_multiply (vmask, splat_alpha (vsrc))));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
        mask+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t sa = Alpha (s);
        FbByteMulC (a, sa);
        FbByteMulC (d, ~a);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineAtopC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_add_mul (pix_multiply (vsrc, vmask), splat_alpha (vdest),
                            vdest,
                            negate (pix_multiply (vmask,
                                                splat_alpha (vmask))));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
        mask+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t sa = Alpha (s);
        uint32_t da = Alpha (d);

        FbByteMulC (s, a);
        FbByteMul (a, sa);
        FbByteAddMulC (d, ~a, s, da);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineAtopReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_add_mul (vdest,
                            pix_multiply (vmask, splat_alpha (vsrc)),
                            pix_multiply (vsrc, vmask),
                            negate (splat_alpha (vdest)));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
        mask+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t sa = Alpha (s);
        uint32_t da = Alpha (d);

        FbByteMulC (s, a);
        FbByteMul (a, sa);
        FbByteAddMulC (d, a, s, ~da);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineXorC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_add_mul (vdest,
                            negate (pix_multiply (vmask, splat_alpha (vsrc))),
                            pix_multiply (vsrc, vmask),
                            negate (splat_alpha (vdest)));

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
        mask+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t d = dest[i];
        uint32_t sa = Alpha (s);
        uint32_t da = Alpha (d);

        FbByteMulC (s, a);
        FbByteMul (a, sa);
        FbByteAddMulC (d, ~a, s, ~da);
        dest[i] = d;
    }
}

static FASTCALL void
vmxCombineAddC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    vector unsigned int  vdest, vsrc, vmask;
    vector unsigned char tmp1, tmp2, tmp3, tmp4, edges,
                         dest_mask, mask_mask, src_mask, store_mask;

    COMPUTE_SHIFT_MASKC(dest, src, mask)

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width/4; i > 0; i--) {

        LOAD_VECTORSC(dest, src, mask)

        vdest = pix_add (pix_multiply (vsrc, vmask), vdest);

        STORE_VECTOR(dest)

        src+=4;
        dest+=4;
        mask+=4;
    }

    for (i = width%4; --i >=0;) {
        uint32_t a = mask[i];
        uint32_t s = src[i];
        uint32_t d = dest[i];

        FbByteMulC (s, a);
        FbByteAdd (s, d);
        dest[i] = s;
    }
}


#if 0
void
fbCompositeSolid_nx8888vmx (pixman_operator_t	op,
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
    int	dstStride;

    fbComposeGetSolid (pSrc, pDst, src);

    if (src >> 24 == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	/* XXX vmxCombineOverU (dst, src, width); */
    }
}

void
fbCompositeSolid_nx0565vmx (pixman_operator_t	op,
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

    fbComposeGetSolid (pSrc, pDst, src);

    if (src >> 24 == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
       vmxCombineOverU565(dst, src, width);
    }
}

#endif

void fbComposeSetupVMX (void)
{
    /* check if we have VMX support and initialize accordingly */
    if (pixman_have_vmx ()) {
        pixman_composeFunctions.combineU[PIXMAN_OP_OVER] = vmxCombineOverU;
        pixman_composeFunctions.combineU[PIXMAN_OP_OVER_REVERSE] = vmxCombineOverReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_IN] = vmxCombineInU;
        pixman_composeFunctions.combineU[PIXMAN_OP_IN_REVERSE] = vmxCombineInReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_OUT] = vmxCombineOutU;
        pixman_composeFunctions.combineU[PIXMAN_OP_OUT_REVERSE] = vmxCombineOutReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ATOP] = vmxCombineAtopU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ATOP_REVERSE] = vmxCombineAtopReverseU;
        pixman_composeFunctions.combineU[PIXMAN_OP_XOR] = vmxCombineXorU;
        pixman_composeFunctions.combineU[PIXMAN_OP_ADD] = vmxCombineAddU;

        pixman_composeFunctions.combineC[PIXMAN_OP_SRC] = vmxCombineSrcC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OVER] = vmxCombineOverC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OVER_REVERSE] = vmxCombineOverReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_IN] = vmxCombineInC;
        pixman_composeFunctions.combineC[PIXMAN_OP_IN_REVERSE] = vmxCombineInReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OUT] = vmxCombineOutC;
        pixman_composeFunctions.combineC[PIXMAN_OP_OUT_REVERSE] = vmxCombineOutReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ATOP] = vmxCombineAtopC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ATOP_REVERSE] = vmxCombineAtopReverseC;
        pixman_composeFunctions.combineC[PIXMAN_OP_XOR] = vmxCombineXorC;
        pixman_composeFunctions.combineC[PIXMAN_OP_ADD] = vmxCombineAddC;

        pixman_composeFunctions.combineMaskU = vmxCombineMaskU;
    }
}
