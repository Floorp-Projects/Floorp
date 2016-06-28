/*
 * Copyright © 2013 Soren Sandmann Pedersen
 * Copyright © 2013 Red Hat, Inc.
 * Copyright © 2016 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Soren Sandmann (soren.sandmann@gmail.com)
 *         Jeff Muizelaar (jmuizelaar@mozilla.com)
 */

/* This has been adapted from the ssse3 code from pixman. It's currently
 * a mess as I want to try it out in practice before finalizing the details.
 */

#include <stdlib.h>
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <stdint.h>
#include <assert.h>

typedef int32_t                 pixman_fixed_16_16_t;
typedef pixman_fixed_16_16_t    pixman_fixed_t;
#define pixman_fixed_1                  (pixman_int_to_fixed(1))
#define pixman_fixed_to_int(f)          ((int) ((f) >> 16))
#define pixman_int_to_fixed(i)          ((pixman_fixed_t) ((i) << 16))
#define pixman_double_to_fixed(d)       ((pixman_fixed_t) ((d) * 65536.0))
typedef struct pixman_vector pixman_vector_t;

typedef int pixman_bool_t;
typedef int64_t                 pixman_fixed_32_32_t;
typedef pixman_fixed_32_32_t    pixman_fixed_48_16_t;
typedef struct { pixman_fixed_48_16_t v[3]; } pixman_vector_48_16_t;

struct pixman_vector
{
    pixman_fixed_t      vector[3];
};
typedef struct pixman_transform pixman_transform_t;

struct pixman_transform
{
    pixman_fixed_t      matrix[3][3];
};

#ifdef _MSC_VER
#define force_inline __forceinline
#else
#define force_inline __inline__ __attribute__((always_inline))
#endif

#define BILINEAR_INTERPOLATION_BITS 6

static force_inline int
pixman_fixed_to_bilinear_weight (pixman_fixed_t x)
{
    return (x >> (16 - BILINEAR_INTERPOLATION_BITS)) &
                               ((1 << BILINEAR_INTERPOLATION_BITS) - 1);
}

static void
pixman_transform_point_31_16_3d (const pixman_transform_t    *t,
                                 const pixman_vector_48_16_t *v,
                                 pixman_vector_48_16_t       *result)
{
    int i;
    int64_t tmp[3][2];

    /* input vector values must have no more than 31 bits (including sign)
     * in the integer part */
    assert (v->v[0] <   ((pixman_fixed_48_16_t)1 << (30 + 16)));
    assert (v->v[0] >= -((pixman_fixed_48_16_t)1 << (30 + 16)));
    assert (v->v[1] <   ((pixman_fixed_48_16_t)1 << (30 + 16)));
    assert (v->v[1] >= -((pixman_fixed_48_16_t)1 << (30 + 16)));
    assert (v->v[2] <   ((pixman_fixed_48_16_t)1 << (30 + 16)));
    assert (v->v[2] >= -((pixman_fixed_48_16_t)1 << (30 + 16)));

    for (i = 0; i < 3; i++)
    {
        tmp[i][0] = (int64_t)t->matrix[i][0] * (v->v[0] >> 16);
        tmp[i][1] = (int64_t)t->matrix[i][0] * (v->v[0] & 0xFFFF);
        tmp[i][0] += (int64_t)t->matrix[i][1] * (v->v[1] >> 16);
        tmp[i][1] += (int64_t)t->matrix[i][1] * (v->v[1] & 0xFFFF);
        tmp[i][0] += (int64_t)t->matrix[i][2] * (v->v[2] >> 16);
        tmp[i][1] += (int64_t)t->matrix[i][2] * (v->v[2] & 0xFFFF);
    }

    result->v[0] = tmp[0][0] + ((tmp[0][1] + 0x8000) >> 16);
    result->v[1] = tmp[1][0] + ((tmp[1][1] + 0x8000) >> 16);
    result->v[2] = tmp[2][0] + ((tmp[2][1] + 0x8000) >> 16);
}

static pixman_bool_t
pixman_transform_point_3d (const struct pixman_transform *transform,
                           struct pixman_vector *         vector)
{
    pixman_vector_48_16_t tmp;
    tmp.v[0] = vector->vector[0];
    tmp.v[1] = vector->vector[1];
    tmp.v[2] = vector->vector[2];

    pixman_transform_point_31_16_3d (transform, &tmp, &tmp);

    vector->vector[0] = tmp.v[0];
    vector->vector[1] = tmp.v[1];
    vector->vector[2] = tmp.v[2];

    return vector->vector[0] == tmp.v[0] &&
           vector->vector[1] == tmp.v[1] &&
           vector->vector[2] == tmp.v[2];
}


struct bits_image_t
{
    uint32_t *                 bits;
    int                        rowstride;
    pixman_transform_t *transform;
};

typedef struct bits_image_t bits_image_t;
typedef struct {
    int unused;
} pixman_iter_info_t;

typedef struct pixman_iter_t pixman_iter_t;
typedef void      (* pixman_iter_fini_t)         (pixman_iter_t *iter);

struct pixman_iter_t
{
    int x, y;
    pixman_iter_fini_t          fini;
    bits_image_t *image;
    uint32_t *                  buffer;
    int width;
    int height;
    void *                      data;
};

typedef struct
{
    int		y;
    uint64_t *	buffer;
} line_t;

typedef struct
{
    line_t		lines[2];
    pixman_fixed_t	y;
    pixman_fixed_t	x;
    uint64_t		data[1];
} bilinear_info_t;

static void
ssse3_fetch_horizontal (bits_image_t *image, line_t *line,
			int y, pixman_fixed_t x, pixman_fixed_t ux, int n)
{
    uint32_t *bits = image->bits + y * image->rowstride;
    __m128i vx = _mm_set_epi16 (
	- (x + 1), x, - (x + 1), x,
	- (x + ux + 1), x + ux,  - (x + ux + 1), x + ux);
    __m128i vux = _mm_set_epi16 (
	- 2 * ux, 2 * ux, - 2 * ux, 2 * ux,
	- 2 * ux, 2 * ux, - 2 * ux, 2 * ux);
    __m128i vaddc = _mm_set_epi16 (1, 0, 1, 0, 1, 0, 1, 0);
    __m128i *b = (__m128i *)line->buffer;
    __m128i vrl0, vrl1;

    while ((n -= 2) >= 0)
    {
        __m128i vw, vr, s;
#ifdef HACKY_PADDING
        if (pixman_fixed_to_int(x + ux) >= image->rowstride) {
            vrl1 = _mm_setzero_si128();
            printf("overread 2loop\n");
         } else {
                 if (pixman_fixed_to_int(x + ux) < 0)
                         printf("underflow\n");
        vrl1 = _mm_loadl_epi64(
            (__m128i *)(bits + (pixman_fixed_to_int(x + ux) < 0 ? 0 : pixman_fixed_to_int(x + ux))));
        }
#else
        vrl1 = _mm_loadl_epi64(
            (__m128i *)(bits + pixman_fixed_to_int(x + ux)));
#endif
	/* vrl1: R1, L1 */

    final_pixel:
#ifdef HACKY_PADDING
	vrl0 = _mm_loadl_epi64 (
	    (__m128i *)(bits + (pixman_fixed_to_int (x) < 0 ? 0 : pixman_fixed_to_int (x))));
#else
        vrl0 = _mm_loadl_epi64 (
	    (__m128i *)(bits + pixman_fixed_to_int (x)));
#endif
        /* vrl0: R0, L0 */

	/* The weights are based on vx which is a vector of 
	 *
	 *    - (x + 1), x, - (x + 1), x,
	 *          - (x + ux + 1), x + ux, - (x + ux + 1), x + ux
	 *
	 * so the 16 bit weights end up like this:
	 *
	 *    iw0, w0, iw0, w0, iw1, w1, iw1, w1
	 *
	 * and after shifting and packing, we get these bytes:
	 *
	 *    iw0, w0, iw0, w0, iw1, w1, iw1, w1,
	 *        iw0, w0, iw0, w0, iw1, w1, iw1, w1,
	 *
	 * which means the first and the second input pixel 
	 * have to be interleaved like this:
	 *
	 *    la0, ra0, lr0, rr0, la1, ra1, lr1, rr1,
	 *        lg0, rg0, lb0, rb0, lg1, rg1, lb1, rb1
	 *
	 * before maddubsw can be used.
	 */

	vw = _mm_add_epi16 (
	    vaddc, _mm_srli_epi16 (vx, 16 - BILINEAR_INTERPOLATION_BITS));
	/* vw: iw0, w0, iw0, w0, iw1, w1, iw1, w1
	 */

	vw = _mm_packus_epi16 (vw, vw);
	/* vw: iw0, w0, iw0, w0, iw1, w1, iw1, w1,
	 *         iw0, w0, iw0, w0, iw1, w1, iw1, w1
	 */
	vx = _mm_add_epi16 (vx, vux);

	x += 2 * ux;

	vr = _mm_unpacklo_epi16 (vrl1, vrl0);
	/* vr: rar0, rar1, rgb0, rgb1, lar0, lar1, lgb0, lgb1 */

	s = _mm_shuffle_epi32 (vr, _MM_SHUFFLE (1, 0, 3, 2));
	/* s:  lar0, lar1, lgb0, lgb1, rar0, rar1, rgb0, rgb1 */

	vr = _mm_unpackhi_epi8 (vr, s);
	/* vr: la0, ra0, lr0, rr0, la1, ra1, lr1, rr1,
	 *         lg0, rg0, lb0, rb0, lg1, rg1, lb1, rb1
	 */

	vr = _mm_maddubs_epi16 (vr, vw);

	/* When the weight is 0, the inverse weight is
	 * 128 which can't be represented in a signed byte.
	 * As a result maddubsw computes the following:
	 *
	 *     r = l * -128 + r * 0
	 *
	 * rather than the desired
	 *
	 *     r = l * 128 + r * 0
	 *
	 * We fix this by taking the absolute value of the
	 * result.
	 */
        // we can drop this if we use lower precision

	vr = _mm_shuffle_epi32 (vr, _MM_SHUFFLE (2, 0, 3, 1));
	/* vr: A0, R0, A1, R1, G0, B0, G1, B1 */
	_mm_store_si128 (b++, vr);
    }

    if (n == -1)
    {
	vrl1 = _mm_setzero_si128();
	goto final_pixel;
    }

    line->y = y;
}

// scale a line of destination pixels
static uint32_t *
ssse3_fetch_bilinear_cover (pixman_iter_t *iter, const uint32_t *mask)
{
    pixman_fixed_t fx, ux;
    bilinear_info_t *info = iter->data;
    line_t *line0, *line1;
    int y0, y1;
    int32_t dist_y;
    __m128i vw, uvw;
    int i;

    fx = info->x;
    ux = iter->image->transform->matrix[0][0];

    y0 = pixman_fixed_to_int (info->y);
    if (y0 < 0)
        *(volatile char*)0 = 9;
    y1 = y0 + 1;

    // clamping in y direction
    if (y1 >= iter->height) {
        y1 = iter->height - 1;
    }

    line0 = &info->lines[y0 & 0x01];
    line1 = &info->lines[y1 & 0x01];

    if (line0->y != y0)
    {
	ssse3_fetch_horizontal (
	    iter->image, line0, y0, fx, ux, iter->width);
    }

    if (line1->y != y1)
    {
	ssse3_fetch_horizontal (
	    iter->image, line1, y1, fx, ux, iter->width);
    }

#ifdef PIXMAN_STYLE_INTERPOLATION
    dist_y = pixman_fixed_to_bilinear_weight (info->y);
    dist_y <<= (16 - BILINEAR_INTERPOLATION_BITS);

    vw = _mm_set_epi16 (
	dist_y, dist_y, dist_y, dist_y, dist_y, dist_y, dist_y, dist_y);

#else
    // setup the weights for the top (vw) and bottom (uvw) lines
    dist_y = pixman_fixed_to_bilinear_weight (info->y);
    // we use 15 instead of 16 because we need an extra bit to handle when the weights are 0 and 1
    dist_y <<= (15 - BILINEAR_INTERPOLATION_BITS);

    vw = _mm_set_epi16 (
	dist_y, dist_y, dist_y, dist_y, dist_y, dist_y, dist_y, dist_y);


    dist_y = (1 << BILINEAR_INTERPOLATION_BITS) - pixman_fixed_to_bilinear_weight (info->y);
    dist_y <<= (15 - BILINEAR_INTERPOLATION_BITS);
    uvw = _mm_set_epi16 (
	dist_y, dist_y, dist_y, dist_y, dist_y, dist_y, dist_y, dist_y);
#endif

    for (i = 0; i + 3 < iter->width; i += 4)
    {
	__m128i top0 = _mm_load_si128 ((__m128i *)(line0->buffer + i));
	__m128i bot0 = _mm_load_si128 ((__m128i *)(line1->buffer + i));
	__m128i top1 = _mm_load_si128 ((__m128i *)(line0->buffer + i + 2));
	__m128i bot1 = _mm_load_si128 ((__m128i *)(line1->buffer + i + 2));
#ifdef PIXMAN_STYLE_INTERPOLATION
	__m128i r0, r1, tmp, p;

        r0 = _mm_mulhi_epu16 (
	    _mm_sub_epi16 (bot0, top0), vw);
	tmp = _mm_cmplt_epi16 (bot0, top0);
	tmp = _mm_and_si128 (tmp, vw);
	r0 = _mm_sub_epi16 (r0, tmp);
	r0 = _mm_add_epi16 (r0, top0);
	r0 = _mm_srli_epi16 (r0, BILINEAR_INTERPOLATION_BITS);
	/* r0:  A0 R0 A1 R1 G0 B0 G1 B1 */
        //r0 = _mm_shuffle_epi32 (r0, _MM_SHUFFLE (2, 0, 3, 1));
	/* r0:  A1 R1 G1 B1 A0 R0 G0 B0 */

        // tmp = bot1 < top1 ? vw : 0;
        // r1 = (bot1 - top1)*vw + top1 - tmp
        // r1 = bot1*vw - vw*top1 + top1 - tmp
        // r1 = bot1*vw + top1 - vw*top1 - tmp
        // r1 = bot1*vw + top1*(1 - vw) - tmp
	r1 = _mm_mulhi_epu16 (
	    _mm_sub_epi16 (bot1, top1), vw);
	tmp = _mm_cmplt_epi16 (bot1, top1);
	tmp = _mm_and_si128 (tmp, vw);
	r1 = _mm_sub_epi16 (r1, tmp);
	r1 = _mm_add_epi16 (r1, top1);
	r1 = _mm_srli_epi16 (r1, BILINEAR_INTERPOLATION_BITS);
	//r1 = _mm_shuffle_epi32 (r1, _MM_SHUFFLE (2, 0, 3, 1));
	/* r1: A3 R3 G3 B3 A2 R2 G2 B2 */
#else
	__m128i r0, r1, p;
        top0 = _mm_mulhi_epu16 (top0, uvw);
        bot0 = _mm_mulhi_epu16 (bot0, vw);
        r0 = _mm_add_epi16(top0, bot0);
        r0 = _mm_srli_epi16(r0, BILINEAR_INTERPOLATION_BITS-1);

        top1 = _mm_mulhi_epu16 (top1, uvw);
        bot1 = _mm_mulhi_epu16 (bot1, vw);
        r1 = _mm_add_epi16(top1, bot1);
        r1 = _mm_srli_epi16(r1, BILINEAR_INTERPOLATION_BITS-1);
#endif

	p = _mm_packus_epi16 (r0, r1);
	_mm_storeu_si128 ((__m128i *)(iter->buffer + i), p);
    }

    while (i < iter->width)
    {
	__m128i top0 = _mm_load_si128 ((__m128i *)(line0->buffer + i));
	__m128i bot0 = _mm_load_si128 ((__m128i *)(line1->buffer + i));

#ifdef PIXMAN_STYLE_INTERPOLATION
	__m128i r0, tmp, p;
	r0 = _mm_mulhi_epu16 (
	    _mm_sub_epi16 (bot0, top0), vw);
	tmp = _mm_cmplt_epi16 (bot0, top0);
	tmp = _mm_and_si128 (tmp, vw);
	r0 = _mm_sub_epi16 (r0, tmp);
	r0 = _mm_add_epi16 (r0, top0);
	r0 = _mm_srli_epi16 (r0, BILINEAR_INTERPOLATION_BITS);
	/* r0:  A0 R0 A1 R1 G0 B0 G1 B1 */
	r0 = _mm_shuffle_epi32 (r0, _MM_SHUFFLE (2, 0, 3, 1));
	/* r0:  A1 R1 G1 B1 A0 R0 G0 B0 */
#else
	__m128i r0, p;
        top0 = _mm_mulhi_epu16 (top0, uvw);
        bot0 = _mm_mulhi_epu16 (bot0, vw);
        r0 = _mm_add_epi16(top0, bot0);
        r0 = _mm_srli_epi16(r0, BILINEAR_INTERPOLATION_BITS-1);
#endif

	p = _mm_packus_epi16 (r0, r0);

	if (iter->width - i == 1)
	{
	    *(uint32_t *)(iter->buffer + i) = _mm_cvtsi128_si32 (p);
	    i++;
	}
	else
	{
	    _mm_storel_epi64 ((__m128i *)(iter->buffer + i), p);
	    i += 2;
	}
    }

    info->y += iter->image->transform->matrix[1][1];

    return iter->buffer;
}

static void
ssse3_bilinear_cover_iter_fini (pixman_iter_t *iter)
{
    free (iter->data);
}

static void
ssse3_bilinear_cover_iter_init (pixman_iter_t *iter)
{
    int width = iter->width;
    bilinear_info_t *info;
    pixman_vector_t v;

    /* Reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (iter->x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (iter->y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (iter->image->transform, &v))
	goto fail;

    info = malloc (sizeof (*info) + (2 * width - 1) * sizeof (uint64_t) + 64);
    if (!info)
	goto fail;

    info->x = v.vector[0] - pixman_fixed_1 / 2;
    info->y = v.vector[1] - pixman_fixed_1 / 2;

#define ALIGN(addr)							\
    ((void *)((((uintptr_t)(addr)) + 15) & (~15)))

    /* It is safe to set the y coordinates to -1 initially
     * because COVER_CLIP_BILINEAR ensures that we will only
     * be asked to fetch lines in the [0, height) interval
     */
    info->lines[0].y = -1;
    info->lines[0].buffer = ALIGN (&(info->data[0]));
    info->lines[1].y = -1;
    info->lines[1].buffer = ALIGN (info->lines[0].buffer + width);

    iter->fini = ssse3_bilinear_cover_iter_fini;

    iter->data = info;
    return;

fail:
    /* Something went wrong, either a bad matrix or OOM; in such cases,
     * we don't guarantee any particular rendering.
     */
    iter->fini = NULL;
}

/* scale the src from src_width/height to dest_width/height drawn
 * into the rectangle x,y width,height
 * src_stride and dst_stride are 4 byte units */
void ssse3_scale_data(uint32_t *src, int src_width, int src_height, int src_stride,
                      uint32_t *dest, int dest_width, int dest_height,
                      int dest_stride,
                      int x, int y,
                      int width, int height)
{
    //XXX: assert(src_width > 1)
    pixman_transform_t transform = {
        { { pixman_fixed_1, 0, 0 },
            { 0, pixman_fixed_1, 0 },
            { 0, 0, pixman_fixed_1 } }
    };
    double width_scale = ((double)src_width)/dest_width;
    double height_scale = ((double)src_height)/dest_height;
#define AVOID_PADDING
#ifdef AVOID_PADDING
    // scale up by enough that we don't read outside of the bounds of the source surface
    // currently this is required to avoid reading out of bounds.
    if (width_scale < 1) {
        width_scale = (double)(src_width-1)/dest_width;
        transform.matrix[0][2] = pixman_fixed_1/2;
    }
    if (height_scale < 1) {
        height_scale = (double)(src_height-1)/dest_height;
        transform.matrix[1][2] = pixman_fixed_1/2;
    }
#endif
    transform.matrix[0][0] = pixman_double_to_fixed(width_scale);
    transform.matrix[1][1] = pixman_double_to_fixed(height_scale);
    transform.matrix[2][2] = pixman_fixed_1;

    bits_image_t image;
    image.bits = src;
    image.transform = &transform;
    image.rowstride = src_stride;

    pixman_iter_t iter;
    iter.image = &image;
    iter.x = x;
    iter.y = y;
    iter.width = width;
    iter.height = src_height;
    iter.buffer = dest;
    iter.data = NULL;

    ssse3_bilinear_cover_iter_init(&iter);
    if (iter.data) {
        for (int iy = 0; iy < height; iy++) {
            ssse3_fetch_bilinear_cover(&iter, NULL);
            iter.buffer += dest_stride;
        }
        ssse3_bilinear_cover_iter_fini(&iter);
    }
}
