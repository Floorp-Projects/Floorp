/*
 * Copyright © 2009-2010 Nokia Corporation
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
 * Author:  Siarhei Siamashka (siarhei.siamashka@nokia.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include "pixman-private.h"
#include "pixman-combine32.h"

/*
 * Functions, which implement the core inner loops for the nearest neighbour
 * scaled fastpath compositing operations. They do not need to do clipping
 * checks, also the loops are unrolled to process two pixels per iteration
 * for better performance on most CPU architectures (superscalar processors
 * can issue several operations simultaneously, other processors can hide
 * instructions latencies by pipelining operations). Unrolling more
 * does not make much sense because the compiler will start running out
 * of spare registers soon.
 */

static void
fast_composite_scale_nearest_over_8888_0565 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint16_t *dst_line;
    uint32_t *src_first_line;
    uint32_t  d;
    uint32_t  s1, s2;
    uint8_t   a1, a2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;

    uint32_t *src;
    uint16_t *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint16_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src_first_line, 1);

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;

	if ((y < 0) || (y >= src_image->bits.height))
	{
	    continue;
	}

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    s2 = src[x2];

	    a1 = s1 >> 24;
	    a2 = s2 >> 24;

	    if (a1 == 0xff)
	    {
		*dst = CONVERT_8888_TO_0565 (s1);
	    }
	    else if (s1)
	    {
		d = CONVERT_0565_TO_0888 (*dst);
		a1 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a1, s1);
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;

	    if (a2 == 0xff)
	    {
		*dst = CONVERT_8888_TO_0565 (s2);
	    }
	    else if (s2)
	    {
		d = CONVERT_0565_TO_0888 (*dst);
		a2 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a2, s2);
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];

	    a1 = s1 >> 24;
	    if (a1 == 0xff)
	    {
		*dst = CONVERT_8888_TO_0565 (s1);
	    }
	    else if (s1)
	    {
		d = CONVERT_0565_TO_0888 (*dst);
		a1 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a1, s1);
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_scale_nearest_normal_repeat_over_8888_0565 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint16_t *dst_line;
    uint32_t *src_first_line;
    uint32_t  d;
    uint32_t  s1, s2;
    uint8_t   a1, a2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;
    int32_t   max_vx, max_vy;

    uint32_t *src;
    uint16_t *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint16_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src_first_line, 1);

    max_vx = src_image->bits.width << 16;
    max_vy = src_image->bits.height << 16;

    while (orig_vx < 0) orig_vx += max_vx;
    while (vy < 0) vy += max_vy;
    while (orig_vx >= max_vx) orig_vx -= max_vx;
    while (vy >= max_vy) vy -= max_vy;

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;
	while (vy >= max_vy) vy -= max_vy;

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s2 = src[x2];

	    a1 = s1 >> 24;
	    a2 = s2 >> 24;

	    if (a1 == 0xff)
	    {
		*dst = CONVERT_8888_TO_0565 (s1);
	    }
	    else if (s1)
	    {
		d = CONVERT_0565_TO_0888 (*dst);
		a1 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a1, s1);
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;

	    if (a2 == 0xff)
	    {
		*dst = CONVERT_8888_TO_0565 (s2);
	    }
	    else if (s2)
	    {
		d = CONVERT_0565_TO_0888 (*dst);
		a2 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a2, s2);
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    a1 = s1 >> 24;
	    if (a1 == 0xff)
	    {
		*dst = CONVERT_8888_TO_0565 (s1);
	    }
	    else if (s1)
	    {
		d = CONVERT_0565_TO_0888 (*dst);
		a1 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a1, s1);
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_scale_nearest_over_8888_8888 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint32_t *dst_line;
    uint32_t *src_first_line;
    uint32_t  d;
    uint32_t  s1, s2;
    uint8_t   a1, a2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;

    uint32_t *src, *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint32_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src_first_line, 1);

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;

	if ((y < 0) || (y >= src_image->bits.height))
	{
	    continue;
	}

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    s2 = src[x2];

	    a1 = s1 >> 24;
	    a2 = s2 >> 24;

	    if (a1 == 0xff)
	    {
		*dst = s1;
	    }
	    else if (s1)
	    {
		d = *dst;
		a1 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a1, s1);
		*dst = d;
	    }
	    dst++;

	    if (a2 == 0xff)
	    {
		*dst = s2;
	    }
	    else if (s2)
	    {
		d = *dst;
		a2 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a2, s2);
		*dst = d;
	    }
	    dst++;
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];

	    a1 = s1 >> 24;
	    if (a1 == 0xff)
	    {
		*dst = s1;
	    }
	    else if (s1)
	    {
		d = *dst;
		a1 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a1, s1);
		*dst = d;
	    }
	    dst++;
	}
    }
}

static void
fast_composite_scale_nearest_normal_repeat_over_8888_8888 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint32_t *dst_line;
    uint32_t *src_first_line;
    uint32_t  d;
    uint32_t  s1, s2;
    uint8_t   a1, a2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;
    int32_t   max_vx, max_vy;

    uint32_t *src, *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint32_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src_first_line, 1);

    max_vx = src_image->bits.width << 16;
    max_vy = src_image->bits.height << 16;

    while (orig_vx < 0) orig_vx += max_vx;
    while (vy < 0) vy += max_vy;
    while (orig_vx >= max_vx) orig_vx -= max_vx;
    while (vy >= max_vy) vy -= max_vy;

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;
	while (vy >= max_vy) vy -= max_vy;

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s2 = src[x2];

	    a1 = s1 >> 24;
	    a2 = s2 >> 24;

	    if (a1 == 0xff)
	    {
		*dst = s1;
	    }
	    else if (s1)
	    {
		d = *dst;
		a1 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a1, s1);
		*dst = d;
	    }
	    dst++;

	    if (a2 == 0xff)
	    {
		*dst = s2;
	    }
	    else if (s2)
	    {
		d = *dst;
		a2 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a2, s2);
		*dst = d;
	    }
	    dst++;
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    a1 = s1 >> 24;
	    if (a1 == 0xff)
	    {
		*dst = s1;
	    }
	    else if (s1)
	    {
		d = *dst;
		a1 ^= 0xff;
		UN8x4_MUL_UN8_ADD_UN8x4 (d, a1, s1);
		*dst = d;
	    }
	    dst++;
	}
    }
}

static void
fast_composite_scale_nearest_src_8888_8888 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint32_t *dst_line;
    uint32_t *src_first_line;
    uint32_t  s1, s2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;

    uint32_t *src, *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint32_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src_first_line, 1);

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;

	if ((y < 0) || (y >= src_image->bits.height))
	{
	    memset (dst, 0, width * sizeof(*dst));
	    continue;
	}

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    s2 = src[x2];

	    *dst++ = s1;
	    *dst++ = s2;
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];
	    *dst++ = s1;
	}
    }
}

static void
fast_composite_scale_nearest_normal_repeat_src_8888_8888 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint32_t *dst_line;
    uint32_t *src_first_line;
    uint32_t  s1, s2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;
    int32_t   max_vx, max_vy;

    uint32_t *src, *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint32_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src_first_line, 1);

    max_vx = src_image->bits.width << 16;
    max_vy = src_image->bits.height << 16;

    while (orig_vx < 0) orig_vx += max_vx;
    while (vy < 0) vy += max_vy;
    while (orig_vx >= max_vx) orig_vx -= max_vx;
    while (vy >= max_vy) vy -= max_vy;

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;
	while (vy >= max_vy) vy -= max_vy;

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s2 = src[x2];

	    *dst++ = s1;
	    *dst++ = s2;
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    *dst++ = s1;
	}
    }
}

static void
fast_composite_scale_nearest_src_0565_0565 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint16_t *dst_line;
    uint16_t *src_first_line;
    uint16_t  s1, s2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;

    uint16_t *src, *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint16_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint16_t, src_stride, src_first_line, 1);

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;

	if ((y < 0) || (y >= src_image->bits.height))
	{
	    memset (dst, 0, width * sizeof(*dst));
	    continue;
	}

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    s2 = src[x2];

	    *dst++ = s1;
	    *dst++ = s2;
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];
	    *dst++ = s1;
	}
    }
}

static void
fast_composite_scale_nearest_normal_repeat_src_0565_0565 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint16_t *dst_line;
    uint16_t *src_first_line;
    uint16_t  s1, s2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;
    int32_t   max_vx, max_vy;

    uint16_t *src, *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint16_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint16_t, src_stride, src_first_line, 1);

    max_vx = src_image->bits.width << 16;
    max_vy = src_image->bits.height << 16;

    while (orig_vx < 0) orig_vx += max_vx;
    while (vy < 0) vy += max_vy;
    while (orig_vx >= max_vx) orig_vx -= max_vx;
    while (vy >= max_vy) vy -= max_vy;

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;
	while (vy >= max_vy) vy -= max_vy;

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s2 = src[x2];

	    *dst++ = s1;
	    *dst++ = s2;
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    *dst++ = s1;
	}
    }
}

static void
fast_composite_scale_nearest_src_8888_0565 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint16_t *dst_line;
    uint32_t *src_first_line;
    uint32_t  s1, s2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;

    uint32_t *src;
    uint16_t *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint16_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src_first_line, 1);

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;

	if ((y < 0) || (y >= src_image->bits.height))
	{
	    memset (dst, 0, width * sizeof(*dst));
	    continue;
	}

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    s2 = src[x2];

	    *dst++ = CONVERT_8888_TO_0565 (s1);
	    *dst++ = CONVERT_8888_TO_0565 (s2);
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    s1 = src[x1];
	    *dst++ = CONVERT_8888_TO_0565 (s1);
	}
    }
}

static void
fast_composite_scale_nearest_normal_repeat_src_8888_0565 (
    pixman_image_t *src_image,
    pixman_image_t *dst_image,
    int             src_x,
    int             src_y,
    int             dst_x,
    int             dst_y,
    int             width,
    int             height,
    int32_t         vx,
    int32_t         vy,
    int32_t         unit_x,
    int32_t         unit_y)
{
    uint16_t *dst_line;
    uint32_t *src_first_line;
    uint32_t  s1, s2;
    int       w;
    int       x1, x2, y;
    int32_t   orig_vx = vx;
    int32_t   max_vx, max_vy;

    uint32_t *src;
    uint16_t *dst;
    int       src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (dst_image, dst_x, dst_y, uint16_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src_first_line, 1);

    max_vx = src_image->bits.width << 16;
    max_vy = src_image->bits.height << 16;

    while (orig_vx < 0) orig_vx += max_vx;
    while (vy < 0) vy += max_vy;
    while (orig_vx >= max_vx) orig_vx -= max_vx;
    while (vy >= max_vy) vy -= max_vy;

    while (--height >= 0)
    {
	dst = dst_line;
	dst_line += dst_stride;

	y = vy >> 16;
	vy += unit_y;
	while (vy >= max_vy) vy -= max_vy;

	src = src_first_line + src_stride * y;

	w = width;
	vx = orig_vx;
	while ((w -= 2) >= 0)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    x2 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s2 = src[x2];

	    *dst++ = CONVERT_8888_TO_0565 (s1);
	    *dst++ = CONVERT_8888_TO_0565 (s2);
	}
	if (w & 1)
	{
	    x1 = vx >> 16;
	    vx += unit_x;
	    while (vx >= max_vx) vx -= max_vx;
	    s1 = src[x1];

	    *dst++ = CONVERT_8888_TO_0565 (s1);
	}
    }
}

/*
 * Check if the source image boundary is crossed in horizontal direction
 */
static inline pixman_bool_t
have_horizontal_oversampling (pixman_image_t *pict,
				     int             width,
				     int32_t         vx,
				     int32_t         unit_x)
{
    while (--width >= 0)
    {
	int x = vx >> 16;
	if ((x < 0) || (x >= pict->bits.width)) return 1;
	vx += unit_x;
    }
    return 0;
}

/*
 * Check if the source image boundary is crossed in vertical direction
 */
static inline pixman_bool_t
have_vertical_oversampling (pixman_image_t *pict,
				   int             height,
				   int32_t         vy,
				   int32_t         unit_y)
{
    while (--height >= 0)
    {
	int y = vy >> 16;
	if ((y < 0) || (y >= pict->bits.height)) return 1;
	vy += unit_y;
    }
    return 0;
}

/*
 * Easy case of transform without rotation or complex clipping
 * Returns 1 in the case if it was able to handle this operation and 0 otherwise
 */
pixman_bool_t
_pixman_run_fast_path_scale (pixman_op_t      op,
			     pixman_image_t * src_image,
			     pixman_image_t * mask_image,
			     pixman_image_t * dst_image,
			     int32_t          src_x,
			     int32_t          src_y,
			     int32_t          mask_x,
			     int32_t          mask_y,
			     int32_t          dst_x,
			     int32_t          dst_y,
			     int32_t          width,
			     int32_t          height)
{
    pixman_vector_t v, unit;
    int             skipdst_x = 0, skipdst_y = 0;

    /* Handle destination clipping */
    int clip_x1, clip_x2, clip_y1, clip_y2;
    if (!dst_image->common.have_clip_region)
    {
	clip_x1 = 0;
	clip_y1 = 0;
	clip_x2 = dst_image->bits.width;
	clip_y2 = dst_image->bits.height;
    }
    else
    {
	clip_x1 = dst_image->common.clip_region.extents.x1;
	clip_y1 = dst_image->common.clip_region.extents.y1;
	clip_x2 = dst_image->common.clip_region.extents.x2;
	clip_y2 = dst_image->common.clip_region.extents.y2;
    }

    if (dst_x < clip_x1)
    {
	skipdst_x = clip_x1 - dst_x;
	if (skipdst_x >= (int)width)
	    return 1;
	dst_x = clip_x1;
	width -= skipdst_x;
    }

    if (dst_y < clip_y1)
    {
	skipdst_y = clip_y1 - dst_y;
	if (skipdst_y >= (int)height)
	    return 1;
	dst_y = clip_y1;
	height -= skipdst_y;
    }

    if (dst_x >= clip_x2 ||
	dst_y >= clip_y2)
    {
	return 1;
    }

    if (dst_x + width > clip_x2)
	width = clip_x2 - dst_x;
    if (dst_y + height > clip_y2)
	height = clip_y2 - dst_y;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (src_x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (src_y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (src_image->common.transform, &v))
	return 0;

    /* Round down to closest integer, ensuring that 0.5 rounds to 0, not 1 */
    v.vector[0] -= pixman_fixed_e;
    v.vector[1] -= pixman_fixed_e;

    unit.vector[0] = src_image->common.transform->matrix[0][0];
    unit.vector[1] = src_image->common.transform->matrix[1][1];

    v.vector[0] += unit.vector[0] * skipdst_x;
    v.vector[1] += unit.vector[1] * skipdst_y;

    /* Check for possible fixed point arithmetics problems/overflows */
    if (unit.vector[0] <= 0 || unit.vector[1] <= 0)
	return 0;
    if (width == 0 || height == 0)
	return 0;
    if ((uint32_t)width + (unit.vector[0] >> 16) >= 0x7FFF)
	return 0;
    if ((uint32_t)height + (unit.vector[1] >> 16) >= 0x7FFF)
	return 0;

    /* Horizontal source oversampling is only supported for NORMAL repeat */
    if (src_image->common.repeat != PIXMAN_REPEAT_NORMAL &&
	have_horizontal_oversampling (src_image, width, v.vector[0], unit.vector[0]))
    {
	return 0;
    }

    /* Vertical source oversampling is only supported for NONE and NORMAL repeat */
    if (src_image->common.repeat != PIXMAN_REPEAT_NONE &&
	src_image->common.repeat != PIXMAN_REPEAT_NORMAL &&
	have_vertical_oversampling (src_image, height, v.vector[1], unit.vector[1]))
    {
	return 0;
    }

    if (op == PIXMAN_OP_OVER &&
	src_image->bits.format == PIXMAN_a8r8g8b8 &&
	(dst_image->bits.format == PIXMAN_x8r8g8b8 ||
	 dst_image->bits.format == PIXMAN_a8r8g8b8))
    {
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat != PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_over_8888_8888 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat == PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_normal_repeat_over_8888_8888 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
    }

    if (op == PIXMAN_OP_SRC &&
	(src_image->bits.format == PIXMAN_x8r8g8b8 ||
	 src_image->bits.format == PIXMAN_a8r8g8b8) &&
	(dst_image->bits.format == PIXMAN_x8r8g8b8 ||
	 dst_image->bits.format == src_image->bits.format))
    {
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat != PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_src_8888_8888 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat == PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_normal_repeat_src_8888_8888 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
    }

    if (op == PIXMAN_OP_OVER &&
	src_image->bits.format == PIXMAN_a8r8g8b8 &&
	dst_image->bits.format == PIXMAN_r5g6b5)
    {
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat != PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_over_8888_0565 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat == PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_normal_repeat_over_8888_0565 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
    }

    if (op == PIXMAN_OP_SRC &&
	src_image->bits.format == PIXMAN_r5g6b5 &&
	dst_image->bits.format == PIXMAN_r5g6b5)
    {
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat != PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_src_0565_0565 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat == PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_normal_repeat_src_0565_0565 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
    }

    if (op == PIXMAN_OP_SRC &&
	(src_image->bits.format == PIXMAN_x8r8g8b8 ||
	 src_image->bits.format == PIXMAN_a8r8g8b8) &&
	dst_image->bits.format == PIXMAN_r5g6b5)
    {
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat != PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_src_8888_0565 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
	if (src_image->common.filter == PIXMAN_FILTER_NEAREST &&
	    src_image->common.repeat == PIXMAN_REPEAT_NORMAL)
	{
	    fast_composite_scale_nearest_normal_repeat_src_8888_0565 (
		src_image, dst_image, src_x, src_y, dst_x, dst_y, width, height,
		v.vector[0], v.vector[1], unit.vector[0], unit.vector[1]);
	    return 1;
	}
    }

    /* No fast path scaling implementation for this case */
    return 0;
}
