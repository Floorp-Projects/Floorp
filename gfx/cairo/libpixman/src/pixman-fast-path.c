/* -*- Mode: c; c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t; -*- */
/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include "pixman-private.h"
#include "pixman-combine32.h"

static force_inline uint32_t
fetch_24 (uint8_t *a)
{
    if (((unsigned long)a) & 1)
    {
#ifdef WORDS_BIGENDIAN
	return (*a << 16) | (*(uint16_t *)(a + 1));
#else
	return *a | (*(uint16_t *)(a + 1) << 8);
#endif
    }
    else
    {
#ifdef WORDS_BIGENDIAN
	return (*(uint16_t *)a << 8) | *(a + 2);
#else
	return *(uint16_t *)a | (*(a + 2) << 16);
#endif
    }
}

static force_inline void
store_24 (uint8_t *a,
          uint32_t v)
{
    if (((unsigned long)a) & 1)
    {
#ifdef WORDS_BIGENDIAN
	*a = (uint8_t) (v >> 16);
	*(uint16_t *)(a + 1) = (uint16_t) (v);
#else
	*a = (uint8_t) (v);
	*(uint16_t *)(a + 1) = (uint16_t) (v >> 8);
#endif
    }
    else
    {
#ifdef WORDS_BIGENDIAN
	*(uint16_t *)a = (uint16_t)(v >> 8);
	*(a + 2) = (uint8_t)v;
#else
	*(uint16_t *)a = (uint16_t)v;
	*(a + 2) = (uint8_t)(v >> 16);
#endif
    }
}

static force_inline uint32_t
over (uint32_t src,
      uint32_t dest)
{
    uint32_t a = ~src >> 24;

    UN8x4_MUL_UN8_ADD_UN8x4 (dest, a, src);

    return dest;
}

static uint32_t
in (uint32_t x,
    uint8_t  y)
{
    uint16_t a = y;

    UN8x4_MUL_UN8 (x, a);

    return x;
}

/*
 * Naming convention:
 *
 *  op_src_mask_dest
 */
static void
fast_composite_over_x888_8_8888 (pixman_implementation_t *imp,
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
    uint32_t    *src, *src_line;
    uint32_t    *dst, *dst_line;
    uint8_t     *mask, *mask_line;
    int src_stride, mask_stride, dst_stride;
    uint8_t m;
    uint32_t s, d;
    uint16_t w;

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
	    m = *mask++;
	    if (m)
	    {
		s = *src | 0xff000000;

		if (m == 0xff)
		{
		    *dst = s;
		}
		else
		{
		    d = in (s, m);
		    *dst = over (d, *dst);
		}
	    }
	    src++;
	    dst++;
	}
    }
}

static void
fast_composite_in_n_8_8 (pixman_implementation_t *imp,
                         pixman_op_t              op,
                         pixman_image_t *         src_image,
                         pixman_image_t *         mask_image,
                         pixman_image_t *         dest_image,
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
    uint8_t     *dst_line, *dst;
    uint8_t     *mask_line, *mask, m;
    int dst_stride, mask_stride;
    uint16_t w;
    uint16_t t;

    src = _pixman_image_get_solid (src_image, dest_image->bits.format);

    srca = src >> 24;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    if (srca == 0xff)
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    while (w--)
	    {
		m = *mask++;

		if (m == 0)
		    *dst = 0;
		else if (m != 0xff)
		    *dst = MUL_UN8 (m, *dst, t);

		dst++;
	    }
	}
    }
    else
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    while (w--)
	    {
		m = *mask++;
		m = MUL_UN8 (m, srca, t);

		if (m == 0)
		    *dst = 0;
		else if (m != 0xff)
		    *dst = MUL_UN8 (m, *dst, t);

		dst++;
	    }
	}
    }
}

static void
fast_composite_in_8_8 (pixman_implementation_t *imp,
                       pixman_op_t              op,
                       pixman_image_t *         src_image,
                       pixman_image_t *         mask_image,
                       pixman_image_t *         dest_image,
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
    int dst_stride, src_stride;
    uint16_t w;
    uint8_t s;
    uint16_t t;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);

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

	    if (s == 0)
		*dst = 0;
	    else if (s != 0xff)
		*dst = MUL_UN8 (s, *dst, t);

	    dst++;
	}
    }
}

static void
fast_composite_over_n_8_8888 (pixman_implementation_t *imp,
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
    uint32_t    *dst_line, *dst, d;
    uint8_t     *mask_line, *mask, m;
    int dst_stride, mask_stride;
    uint16_t w;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    *dst = src;
		else
		    *dst = over (src, *dst);
	    }
	    else if (m)
	    {
		d = in (src, m);
		*dst = over (d, *dst);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_add_n_8888_8888_ca (pixman_implementation_t *imp,
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
    uint32_t src, srca, s;
    uint32_t    *dst_line, *dst, d;
    uint32_t    *mask_line, *mask, ma;
    int dst_stride, mask_stride;
    uint16_t w;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    ma = *mask++;

	    if (ma)
	    {
		d = *dst;
		s = src;

		UN8x4_MUL_UN8x4_ADD_UN8x4 (s, ma, d);

		*dst = s;
	    }

	    dst++;
	}
    }
}

static void
fast_composite_over_n_8888_8888_ca (pixman_implementation_t *imp,
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
    uint32_t src, srca, s;
    uint32_t    *dst_line, *dst, d;
    uint32_t    *mask_line, *mask, ma;
    int dst_stride, mask_stride;
    uint16_t w;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		    *dst = src;
		else
		    *dst = over (src, *dst);
	    }
	    else if (ma)
	    {
		d = *dst;
		s = src;

		UN8x4_MUL_UN8x4 (s, ma);
		UN8x4_MUL_UN8 (ma, srca);
		ma = ~ma;
		UN8x4_MUL_UN8x4_ADD_UN8x4 (d, ma, s);

		*dst = d;
	    }

	    dst++;
	}
    }
}

static void
fast_composite_over_n_8_0888 (pixman_implementation_t *imp,
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
    uint8_t     *dst_line, *dst;
    uint32_t d;
    uint8_t     *mask_line, *mask, m;
    int dst_stride, mask_stride;
    uint16_t w;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 3);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		{
		    d = src;
		}
		else
		{
		    d = fetch_24 (dst);
		    d = over (src, d);
		}
		store_24 (dst, d);
	    }
	    else if (m)
	    {
		d = over (in (src, m), fetch_24 (dst));
		store_24 (dst, d);
	    }
	    dst += 3;
	}
    }
}

static void
fast_composite_over_n_8_0565 (pixman_implementation_t *imp,
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
    uint16_t    *dst_line, *dst;
    uint32_t d;
    uint8_t     *mask_line, *mask, m;
    int dst_stride, mask_stride;
    uint16_t w;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		{
		    d = src;
		}
		else
		{
		    d = *dst;
		    d = over (src, CONVERT_0565_TO_0888 (d));
		}
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    else if (m)
	    {
		d = *dst;
		d = over (in (src, m), CONVERT_0565_TO_0888 (d));
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_over_n_8888_0565_ca (pixman_implementation_t *imp,
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
    uint32_t  src, srca, s;
    uint16_t  src16;
    uint16_t *dst_line, *dst;
    uint32_t  d;
    uint32_t *mask_line, *mask, ma;
    int dst_stride, mask_stride;
    uint16_t w;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    src16 = CONVERT_8888_TO_0565 (src);

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		{
		    *dst = src16;
		}
		else
		{
		    d = *dst;
		    d = over (src, CONVERT_0565_TO_0888 (d));
		    *dst = CONVERT_8888_TO_0565 (d);
		}
	    }
	    else if (ma)
	    {
		d = *dst;
		d = CONVERT_0565_TO_0888 (d);

		s = src;

		UN8x4_MUL_UN8x4 (s, ma);
		UN8x4_MUL_UN8 (ma, srca);
		ma = ~ma;
		UN8x4_MUL_UN8x4_ADD_UN8x4 (d, ma, s);

		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_over_8888_8888 (pixman_implementation_t *imp,
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
    uint32_t    *src_line, *src, s;
    int dst_stride, src_stride;
    uint8_t a;
    uint16_t w;

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
		*dst = s;
	    else if (s)
		*dst = over (s, *dst);
	    dst++;
	}
    }
}

#if 0
static void
fast_composite_over_8888_0888 (pixman_implementation_t *imp,
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
    uint32_t d;
    uint32_t    *src_line, *src, s;
    uint8_t a;
    int dst_stride, src_stride;
    uint16_t w;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 3);
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
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		    d = over (s, fetch_24 (dst));

		store_24 (dst, d);
	    }
	    dst += 3;
	}
    }
}
#endif

static void
fast_composite_over_8888_0565 (pixman_implementation_t *imp,
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
    uint32_t d;
    uint32_t    *src_line, *src, s;
    uint8_t a;
    int dst_stride, src_stride;
    uint16_t w;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);

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
	    if (s)
	    {
		if (a == 0xff)
		{
		    d = s;
		}
		else
		{
		    d = *dst;
		    d = over (s, CONVERT_0565_TO_0888 (d));
		}
		*dst = CONVERT_8888_TO_0565 (d);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_src_x888_0565 (pixman_implementation_t *imp,
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
    uint32_t    *src_line, *src, s;
    int dst_stride, src_stride;
    uint16_t w;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t, dst_stride, dst_line, 1);

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
	    *dst = CONVERT_8888_TO_0565 (s);
	    dst++;
	}
    }
}

static void
fast_composite_add_8000_8000 (pixman_implementation_t *imp,
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
    int dst_stride, src_stride;
    uint16_t w;
    uint8_t s, d;
    uint16_t t;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);

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
	    if (s)
	    {
		if (s != 0xff)
		{
		    d = *dst;
		    t = d + s;
		    s = t | (0 - (t >> 8));
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

static void
fast_composite_add_8888_8888 (pixman_implementation_t *imp,
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
    uint16_t w;
    uint32_t s, d;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);

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
	    if (s)
	    {
		if (s != 0xffffffff)
		{
		    d = *dst;
		    if (d)
			UN8x4_ADD_UN8x4 (s, d);
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

static void
fast_composite_add_n_8_8 (pixman_implementation_t *imp,
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
    uint16_t w;
    uint32_t src;
    uint8_t sa;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);
    src = _pixman_image_get_solid (src_image, dst_image->bits.format);
    sa = (src >> 24);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

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
}

#ifdef WORDS_BIGENDIAN
#define CREATE_BITMASK(n) (0x80000000 >> (n))
#define UPDATE_BITMASK(n) ((n) >> 1)
#else
#define CREATE_BITMASK(n) (1 << (n))
#define UPDATE_BITMASK(n) ((n) << 1)
#endif

#define TEST_BIT(p, n) \
	(*((p) + ((n) >> 5)) & CREATE_BITMASK ((n) & 31))
#define SET_BIT(p, n) \
	do { *((p) + ((n) >> 5)) |= CREATE_BITMASK ((n) & 31); } while (0);

static void
fast_composite_add_1000_1000 (pixman_implementation_t *imp,
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
    uint32_t     *dst_line, *dst;
    uint32_t     *src_line, *src;
    int           dst_stride, src_stride;
    int32_t       w;

    PIXMAN_IMAGE_GET_LINE (src_image, 0, src_y, uint32_t,
                           src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, 0, dest_y, uint32_t,
                           dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    /*
	     * TODO: improve performance by processing uint32_t data instead
	     *       of individual bits
	     */
	    if (TEST_BIT (src, src_x + w))
		SET_BIT (dst, dest_x + w);
	}
    }
}

static void
fast_composite_over_n_1_8888 (pixman_implementation_t *imp,
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
    uint32_t     src, srca;
    uint32_t    *dst, *dst_line;
    uint32_t    *mask, *mask_line;
    int          mask_stride, dst_stride;
    uint32_t     bitcache, bitmask;
    int32_t      w;

    if (width <= 0)
	return;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);
    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t,
                           dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, 0, mask_y, uint32_t,
                           mask_stride, mask_line, 1);
    mask_line += mask_x >> 5;

    if (srca == 0xff)
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    bitcache = *mask++;
	    bitmask = CREATE_BITMASK (mask_x & 31);

	    while (w--)
	    {
		if (bitmask == 0)
		{
		    bitcache = *mask++;
		    bitmask = CREATE_BITMASK (0);
		}
		if (bitcache & bitmask)
		    *dst = src;
		bitmask = UPDATE_BITMASK (bitmask);
		dst++;
	    }
	}
    }
    else
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    bitcache = *mask++;
	    bitmask = CREATE_BITMASK (mask_x & 31);

	    while (w--)
	    {
		if (bitmask == 0)
		{
		    bitcache = *mask++;
		    bitmask = CREATE_BITMASK (0);
		}
		if (bitcache & bitmask)
		    *dst = over (src, *dst);
		bitmask = UPDATE_BITMASK (bitmask);
		dst++;
	    }
	}
    }
}

static void
fast_composite_over_n_1_0565 (pixman_implementation_t *imp,
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
    uint32_t     src, srca;
    uint16_t    *dst, *dst_line;
    uint32_t    *mask, *mask_line;
    int          mask_stride, dst_stride;
    uint32_t     bitcache, bitmask;
    int32_t      w;
    uint32_t     d;
    uint16_t     src565;

    if (width <= 0)
	return;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);
    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint16_t,
                           dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, 0, mask_y, uint32_t,
                           mask_stride, mask_line, 1);
    mask_line += mask_x >> 5;

    if (srca == 0xff)
    {
	src565 = CONVERT_8888_TO_0565 (src);
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    bitcache = *mask++;
	    bitmask = CREATE_BITMASK (mask_x & 31);

	    while (w--)
	    {
		if (bitmask == 0)
		{
		    bitcache = *mask++;
		    bitmask = CREATE_BITMASK (0);
		}
		if (bitcache & bitmask)
		    *dst = src565;
		bitmask = UPDATE_BITMASK (bitmask);
		dst++;
	    }
	}
    }
    else
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    bitcache = *mask++;
	    bitmask = CREATE_BITMASK (mask_x & 31);

	    while (w--)
	    {
		if (bitmask == 0)
		{
		    bitcache = *mask++;
		    bitmask = CREATE_BITMASK (0);
		}
		if (bitcache & bitmask)
		{
		    d = over (src, CONVERT_0565_TO_0888 (*dst));
		    *dst = CONVERT_8888_TO_0565 (d);
		}
		bitmask = UPDATE_BITMASK (bitmask);
		dst++;
	    }
	}
    }
}

/*
 * Simple bitblt
 */

static void
fast_composite_solid_fill (pixman_implementation_t *imp,
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

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);

    if (dst_image->bits.format == PIXMAN_a8)
    {
	src = src >> 24;
    }
    else if (dst_image->bits.format == PIXMAN_r5g6b5 ||
             dst_image->bits.format == PIXMAN_b5g6r5)
    {
	src = CONVERT_8888_TO_0565 (src);
    }

    pixman_fill (dst_image->bits.bits, dst_image->bits.rowstride,
                 PIXMAN_FORMAT_BPP (dst_image->bits.format),
                 dest_x, dest_y,
                 width, height,
                 src);
}

static void
fast_composite_src_8888_x888 (pixman_implementation_t *imp,
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
    uint32_t    *dst;
    uint32_t    *src;
    int dst_stride, src_stride;
    uint32_t n_bytes = width * sizeof (uint32_t);

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst, 1);

    while (height--)
    {
	memcpy (dst, src, n_bytes);

	dst += dst_stride;
	src += src_stride;
    }
}

static const pixman_fast_path_t c_fast_paths[] =
{
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_r5g6b5,   fast_composite_over_n_8_0565 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_b5g6r5,   fast_composite_over_n_8_0565 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_r8g8b8,   fast_composite_over_n_8_0888 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_b8g8r8,   fast_composite_over_n_8_0888 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8r8g8b8, fast_composite_over_n_8_8888 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8r8g8b8, fast_composite_over_n_8_8888 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8b8g8r8, fast_composite_over_n_8_8888 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8b8g8r8, fast_composite_over_n_8_8888 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,       PIXMAN_a8r8g8b8, fast_composite_over_n_1_8888, },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,       PIXMAN_x8r8g8b8, fast_composite_over_n_1_8888, },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,       PIXMAN_a8b8g8r8, fast_composite_over_n_1_8888, },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,       PIXMAN_x8b8g8r8, fast_composite_over_n_1_8888, },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,       PIXMAN_r5g6b5,   fast_composite_over_n_1_0565 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,       PIXMAN_b5g6r5,   fast_composite_over_n_1_0565 },
    { PIXMAN_OP_OVER, PIXMAN_solid, PIXMAN_a8r8g8b8_ca, PIXMAN_a8r8g8b8, fast_composite_over_n_8888_8888_ca },
    { PIXMAN_OP_OVER, PIXMAN_solid, PIXMAN_a8r8g8b8_ca, PIXMAN_x8r8g8b8, fast_composite_over_n_8888_8888_ca },
    { PIXMAN_OP_OVER, PIXMAN_solid, PIXMAN_a8r8g8b8_ca, PIXMAN_r5g6b5,   fast_composite_over_n_8888_0565_ca },
    { PIXMAN_OP_OVER, PIXMAN_solid, PIXMAN_a8b8g8r8_ca, PIXMAN_a8b8g8r8, fast_composite_over_n_8888_8888_ca },
    { PIXMAN_OP_OVER, PIXMAN_solid, PIXMAN_a8b8g8r8_ca, PIXMAN_x8b8g8r8, fast_composite_over_n_8888_8888_ca },
    { PIXMAN_OP_OVER, PIXMAN_solid, PIXMAN_a8b8g8r8_ca, PIXMAN_b5g6r5,   fast_composite_over_n_8888_0565_ca },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,       PIXMAN_x8r8g8b8, fast_composite_over_x888_8_8888,      },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,       PIXMAN_a8r8g8b8, fast_composite_over_x888_8_8888,      },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8,       PIXMAN_x8b8g8r8, fast_composite_over_x888_8_8888,      },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8,       PIXMAN_a8b8g8r8, fast_composite_over_x888_8_8888,      },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_a8r8g8b8, fast_composite_over_8888_8888,   },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_x8r8g8b8, fast_composite_over_8888_8888,   },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_r5g6b5,   fast_composite_over_8888_0565,   },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_a8b8g8r8, fast_composite_over_8888_8888,   },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_x8b8g8r8, fast_composite_over_8888_8888,   },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_b5g6r5,   fast_composite_over_8888_0565,   },
    { PIXMAN_OP_ADD, PIXMAN_a8r8g8b8,  PIXMAN_null,     PIXMAN_a8r8g8b8, fast_composite_add_8888_8888,  },
    { PIXMAN_OP_ADD, PIXMAN_a8b8g8r8,  PIXMAN_null,     PIXMAN_a8b8g8r8, fast_composite_add_8888_8888,  },
    { PIXMAN_OP_ADD, PIXMAN_a8,        PIXMAN_null,     PIXMAN_a8,       fast_composite_add_8000_8000,  },
    { PIXMAN_OP_ADD, PIXMAN_a1,        PIXMAN_null,     PIXMAN_a1,       fast_composite_add_1000_1000,   },
    { PIXMAN_OP_ADD, PIXMAN_solid,   PIXMAN_a8r8g8b8_ca, PIXMAN_a8r8g8b8, fast_composite_add_n_8888_8888_ca },
    { PIXMAN_OP_ADD, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8,       fast_composite_add_n_8_8,   },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_a8r8g8b8, fast_composite_solid_fill },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_x8r8g8b8, fast_composite_solid_fill },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_a8b8g8r8, fast_composite_solid_fill },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_x8b8g8r8, fast_composite_solid_fill },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_a8,       fast_composite_solid_fill },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_r5g6b5,   fast_composite_solid_fill },
    { PIXMAN_OP_SRC, PIXMAN_a8r8g8b8,  PIXMAN_null,     PIXMAN_x8r8g8b8, fast_composite_src_8888_x888 },
    { PIXMAN_OP_SRC, PIXMAN_x8r8g8b8,  PIXMAN_null,     PIXMAN_x8r8g8b8, fast_composite_src_8888_x888 },
    { PIXMAN_OP_SRC, PIXMAN_a8b8g8r8,  PIXMAN_null,     PIXMAN_x8b8g8r8, fast_composite_src_8888_x888 },
    { PIXMAN_OP_SRC, PIXMAN_x8b8g8r8,  PIXMAN_null,     PIXMAN_x8b8g8r8, fast_composite_src_8888_x888 },
    { PIXMAN_OP_SRC, PIXMAN_a8r8g8b8,  PIXMAN_null,     PIXMAN_r5g6b5,   fast_composite_src_x888_0565 },
    { PIXMAN_OP_SRC, PIXMAN_x8r8g8b8,  PIXMAN_null,     PIXMAN_r5g6b5,   fast_composite_src_x888_0565 },
    { PIXMAN_OP_SRC, PIXMAN_a8b8g8r8,  PIXMAN_null,     PIXMAN_b5g6r5,   fast_composite_src_x888_0565 },
    { PIXMAN_OP_SRC, PIXMAN_x8b8g8r8,  PIXMAN_null,     PIXMAN_b5g6r5,   fast_composite_src_x888_0565 },
    { PIXMAN_OP_IN,  PIXMAN_a8,        PIXMAN_null,     PIXMAN_a8,       fast_composite_in_8_8,  },
    { PIXMAN_OP_IN,  PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8,       fast_composite_in_n_8_8 },
    { PIXMAN_OP_NONE },
};

static void
fast_composite_src_scale_nearest (pixman_implementation_t *imp,
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
    uint32_t       *dst;
    uint32_t       *src;
    int dst_stride, src_stride;
    int i, j;
    pixman_vector_t v;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t, src_stride, src, 1);

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (src_x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (src_y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (src_image->common.transform, &v))
	return;

    /* Round down to closest integer, ensuring that 0.5 rounds to 0, not 1 */
    v.vector[0] -= pixman_fixed_e;
    v.vector[1] -= pixman_fixed_e;

    for (j = 0; j < height; j++)
    {
	pixman_fixed_t vx = v.vector[0];
	pixman_fixed_t vy = v.vector[1];

	for (i = 0; i < width; ++i)
	{
	    pixman_bool_t inside_bounds;
	    uint32_t result;
	    int x, y;
	    x = vx >> 16;
	    y = vy >> 16;

	    /* apply the repeat function */
	    switch (src_image->common.repeat)
	    {
	    case PIXMAN_REPEAT_NORMAL:
		x = MOD (x, src_image->bits.width);
		y = MOD (y, src_image->bits.height);
		inside_bounds = TRUE;
		break;

	    case PIXMAN_REPEAT_PAD:
		x = CLIP (x, 0, src_image->bits.width - 1);
		y = CLIP (y, 0, src_image->bits.height - 1);
		inside_bounds = TRUE;
		break;

	    case PIXMAN_REPEAT_REFLECT:
		x = MOD (x, src_image->bits.width * 2);
		if (x >= src_image->bits.width)
		    x = src_image->bits.width * 2 - x - 1;
		y = MOD (y, src_image->bits.height * 2);
		if (y >= src_image->bits.height)
		    y = src_image->bits.height * 2 - y - 1;
		inside_bounds = TRUE;
		break;

	    case PIXMAN_REPEAT_NONE:
	    default:
		inside_bounds =
		    (x >= 0				&&
		     x < src_image->bits.width		&&
		     y >= 0				&&
		     y < src_image->bits.height);
		break;
	    }

	    if (inside_bounds)
	    {
		/* XXX: we should move this multiplication out of the loop */
		result = *(src + y * src_stride + x);
	    }
	    else
	    {
		result = 0;
	    }
	    *(dst + i) = result;

	    /* adjust the x location by a unit vector in the x direction:
	     * this is equivalent to transforming x+1 of the destination
	     * point to source space
	     */
	    vx += src_image->common.transform->matrix[0][0];
	}
	/* adjust the y location by a unit vector in the y direction
	 * this is equivalent to transforming y+1 of the destination point
	 * to source space
	 */
	v.vector[1] += src_image->common.transform->matrix[1][1];
	dst += dst_stride;
    }
}

static void
fast_path_composite (pixman_implementation_t *imp,
                     pixman_op_t              op,
                     pixman_image_t *         src,
                     pixman_image_t *         mask,
                     pixman_image_t *         dest,
                     int32_t                  src_x,
                     int32_t                  src_y,
                     int32_t                  mask_x,
                     int32_t                  mask_y,
                     int32_t                  dest_x,
                     int32_t                  dest_y,
                     int32_t                  width,
                     int32_t                  height)
{
    if (src->type == BITS
        && src->common.transform
        && !mask
        && op == PIXMAN_OP_SRC
        && !src->common.alpha_map && !dest->common.alpha_map
        && (src->common.filter == PIXMAN_FILTER_NEAREST)
        && PIXMAN_FORMAT_BPP (dest->bits.format) == 32
        && src->bits.format == dest->bits.format
        && !src->bits.read_func && !src->bits.write_func
        && !dest->bits.read_func && !dest->bits.write_func)
    {
	/* ensure that the transform matrix only has a scale */
	if (src->common.transform->matrix[0][1] == 0 &&
	    src->common.transform->matrix[1][0] == 0 &&
	    src->common.transform->matrix[2][0] == 0 &&
	    src->common.transform->matrix[2][1] == 0 &&
	    src->common.transform->matrix[2][2] == pixman_fixed_1)
	{
	    _pixman_walk_composite_region (imp, op,
	                                   src, mask, dest,
	                                   src_x, src_y,
	                                   mask_x, mask_y,
	                                   dest_x, dest_y,
	                                   width, height,
	                                   fast_composite_src_scale_nearest);
	    return;
	}
    }

    if (_pixman_run_fast_path (c_fast_paths, imp,
                               op, src, mask, dest,
                               src_x, src_y,
                               mask_x, mask_y,
                               dest_x, dest_y,
                               width, height))
    {
	return;
    }

    _pixman_implementation_composite (imp->delegate, op,
                                      src, mask, dest,
                                      src_x, src_y,
                                      mask_x, mask_y,
                                      dest_x, dest_y,
                                      width, height);
}

static void
pixman_fill8 (uint32_t *bits,
              int       stride,
              int       x,
              int       y,
              int       width,
              int       height,
              uint32_t xor)
{
    int byte_stride = stride * (int) sizeof (uint32_t);
    uint8_t *dst = (uint8_t *) bits;
    uint8_t v = xor & 0xff;
    int i;

    dst = dst + y * byte_stride + x;

    while (height--)
    {
	for (i = 0; i < width; ++i)
	    dst[i] = v;

	dst += byte_stride;
    }
}

static void
pixman_fill16 (uint32_t *bits,
               int       stride,
               int       x,
               int       y,
               int       width,
               int       height,
               uint32_t xor)
{
    int short_stride =
	(stride * (int)sizeof (uint32_t)) / (int)sizeof (uint16_t);
    uint16_t *dst = (uint16_t *)bits;
    uint16_t v = xor & 0xffff;
    int i;

    dst = dst + y * short_stride + x;

    while (height--)
    {
	for (i = 0; i < width; ++i)
	    dst[i] = v;

	dst += short_stride;
    }
}

static void
pixman_fill32 (uint32_t *bits,
               int       stride,
               int       x,
               int       y,
               int       width,
               int       height,
               uint32_t  xor)
{
    int i;

    bits = bits + y * stride + x;

    while (height--)
    {
	for (i = 0; i < width; ++i)
	    bits[i] = xor;

	bits += stride;
    }
}

static pixman_bool_t
fast_path_fill (pixman_implementation_t *imp,
                uint32_t *               bits,
                int                      stride,
                int                      bpp,
                int                      x,
                int                      y,
                int                      width,
                int                      height,
                uint32_t		 xor)
{
    switch (bpp)
    {
    case 8:
	pixman_fill8 (bits, stride, x, y, width, height, xor);
	break;

    case 16:
	pixman_fill16 (bits, stride, x, y, width, height, xor);
	break;

    case 32:
	pixman_fill32 (bits, stride, x, y, width, height, xor);
	break;

    default:
	return _pixman_implementation_fill (
	    imp->delegate, bits, stride, bpp, x, y, width, height, xor);
	break;
    }

    return TRUE;
}

pixman_implementation_t *
_pixman_implementation_create_fast_path (void)
{
    pixman_implementation_t *general = _pixman_implementation_create_general ();
    pixman_implementation_t *imp = _pixman_implementation_create (general);

    imp->composite = fast_path_composite;
    imp->fill = fast_path_fill;

    return imp;
}

