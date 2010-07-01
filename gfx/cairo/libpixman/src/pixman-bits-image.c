/*
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
 *             2008 Aaron Plattner, NVIDIA Corporation
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007, 2009 Red Hat, Inc.
 * Copyright © 2008 André Tupinambá <andrelrt@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pixman-private.h"
#include "pixman-combine32.h"

/* Store functions */

static void
bits_image_store_scanline_32 (bits_image_t *  image,
                              int             x,
                              int             y,
                              int             width,
                              const uint32_t *buffer)
{
    image->store_scanline_raw_32 (image, x, y, width, buffer);

    if (image->common.alpha_map)
    {
	x -= image->common.alpha_origin_x;
	y -= image->common.alpha_origin_y;

	bits_image_store_scanline_32 (image->common.alpha_map, x, y, width, buffer);
    }
}

static void
bits_image_store_scanline_64 (bits_image_t *  image,
                              int             x,
                              int             y,
                              int             width,
                              const uint32_t *buffer)
{
    image->store_scanline_raw_64 (image, x, y, width, buffer);

    if (image->common.alpha_map)
    {
	x -= image->common.alpha_origin_x;
	y -= image->common.alpha_origin_y;

	bits_image_store_scanline_64 (image->common.alpha_map, x, y, width, buffer);
    }
}

void
_pixman_image_store_scanline_32 (bits_image_t *  image,
                                 int             x,
                                 int             y,
                                 int             width,
                                 const uint32_t *buffer)
{
    image->store_scanline_32 (image, x, y, width, buffer);
}

void
_pixman_image_store_scanline_64 (bits_image_t *  image,
                                 int             x,
                                 int             y,
                                 int             width,
                                 const uint32_t *buffer)
{
    image->store_scanline_64 (image, x, y, width, buffer);
}

/* Fetch functions */

static uint32_t
bits_image_fetch_pixel_alpha (bits_image_t *image, int x, int y)
{
    uint32_t pixel;
    uint32_t pixel_a;

    pixel = image->fetch_pixel_raw_32 (image, x, y);

    assert (image->common.alpha_map);

    x -= image->common.alpha_origin_x;
    y -= image->common.alpha_origin_y;

    if (x < 0 || x >= image->common.alpha_map->width ||
	y < 0 || y >= image->common.alpha_map->height)
    {
	pixel_a = 0;
    }
    else
    {
	pixel_a = image->fetch_pixel_raw_32 (
	    image->common.alpha_map, x, y);
	pixel_a = ALPHA_8 (pixel_a);
    }

    UN8x4_MUL_UN8 (pixel, pixel_a);

    return pixel;
}

static force_inline uint32_t
get_pixel (bits_image_t *image, int x, int y, pixman_bool_t check_bounds)
{
    if (check_bounds &&
	(x < 0 || x >= image->width || y < 0 || y >= image->height))
    {
	return 0;
    }

    return image->fetch_pixel_32 (image, x, y);
}

static force_inline void
repeat (pixman_repeat_t repeat, int size, int *coord)
{
    switch (repeat)
    {
    case PIXMAN_REPEAT_NORMAL:
	*coord = MOD (*coord, size);
	break;

    case PIXMAN_REPEAT_PAD:
	*coord = CLIP (*coord, 0, size - 1);
	break;

    case PIXMAN_REPEAT_REFLECT:
	*coord = MOD (*coord, size * 2);

	if (*coord >= size)
	    *coord = size * 2 - *coord - 1;
	break;

    case PIXMAN_REPEAT_NONE:
	break;

    default:
        break;
    }
}

static force_inline uint32_t
bits_image_fetch_pixel_nearest (bits_image_t   *image,
				pixman_fixed_t  x,
				pixman_fixed_t  y)
{
    int x0 = pixman_fixed_to_int (x - pixman_fixed_e);
    int y0 = pixman_fixed_to_int (y - pixman_fixed_e);

    if (image->common.repeat != PIXMAN_REPEAT_NONE)
    {
	repeat (image->common.repeat, image->width, &x0);
	repeat (image->common.repeat, image->height, &y0);

	return get_pixel (image, x0, y0, FALSE);
    }
    else
    {
	return get_pixel (image, x0, y0, TRUE);
    }
}

#if SIZEOF_LONG > 4

static force_inline uint32_t
bilinear_interpolation (uint32_t tl, uint32_t tr,
			uint32_t bl, uint32_t br,
			int distx, int disty)
{
    uint64_t distxy, distxiy, distixy, distixiy;
    uint64_t tl64, tr64, bl64, br64;
    uint64_t f, r;

    distxy = distx * disty;
    distxiy = distx * (256 - disty);
    distixy = (256 - distx) * disty;
    distixiy = (256 - distx) * (256 - disty);

    /* Alpha and Blue */
    tl64 = tl & 0xff0000ff;
    tr64 = tr & 0xff0000ff;
    bl64 = bl & 0xff0000ff;
    br64 = br & 0xff0000ff;

    f = tl64 * distixiy + tr64 * distxiy + bl64 * distixy + br64 * distxy;
    r = f & 0x0000ff0000ff0000ull;

    /* Red and Green */
    tl64 = tl;
    tl64 = ((tl64 << 16) & 0x000000ff00000000ull) | (tl64 & 0x0000ff00ull);

    tr64 = tr;
    tr64 = ((tr64 << 16) & 0x000000ff00000000ull) | (tr64 & 0x0000ff00ull);

    bl64 = bl;
    bl64 = ((bl64 << 16) & 0x000000ff00000000ull) | (bl64 & 0x0000ff00ull);

    br64 = br;
    br64 = ((br64 << 16) & 0x000000ff00000000ull) | (br64 & 0x0000ff00ull);

    f = tl64 * distixiy + tr64 * distxiy + bl64 * distixy + br64 * distxy;
    r |= ((f >> 16) & 0x000000ff00000000ull) | (f & 0xff000000ull);

    return (uint32_t)(r >> 16);
}

#else

static force_inline uint32_t
bilinear_interpolation (uint32_t tl, uint32_t tr,
			uint32_t bl, uint32_t br,
			int distx, int disty)
{
    int distxy, distxiy, distixy, distixiy;
    uint32_t f, r;

    distxy = distx * disty;
    distxiy = (distx << 8) - distxy;	/* distx * (256 - disty) */
    distixy = (disty << 8) - distxy;	/* disty * (256 - distx) */
    distixiy =
	256 * 256 - (disty << 8) -
	(distx << 8) + distxy;		/* (256 - distx) * (256 - disty) */

    /* Blue */
    r = (tl & 0x000000ff) * distixiy + (tr & 0x000000ff) * distxiy
      + (bl & 0x000000ff) * distixy  + (br & 0x000000ff) * distxy;

    /* Green */
    f = (tl & 0x0000ff00) * distixiy + (tr & 0x0000ff00) * distxiy
      + (bl & 0x0000ff00) * distixy  + (br & 0x0000ff00) * distxy;
    r |= f & 0xff000000;

    tl >>= 16;
    tr >>= 16;
    bl >>= 16;
    br >>= 16;
    r >>= 16;

    /* Red */
    f = (tl & 0x000000ff) * distixiy + (tr & 0x000000ff) * distxiy
      + (bl & 0x000000ff) * distixy  + (br & 0x000000ff) * distxy;
    r |= f & 0x00ff0000;

    /* Alpha */
    f = (tl & 0x0000ff00) * distixiy + (tr & 0x0000ff00) * distxiy
      + (bl & 0x0000ff00) * distixy  + (br & 0x0000ff00) * distxy;
    r |= f & 0xff000000;

    return r;
}

#endif

static force_inline uint32_t
bits_image_fetch_pixel_bilinear (bits_image_t   *image,
				 pixman_fixed_t  x,
				 pixman_fixed_t  y)
{
    pixman_repeat_t repeat_mode = image->common.repeat;
    int width = image->width;
    int height = image->height;
    int x1, y1, x2, y2;
    uint32_t tl, tr, bl, br;
    int32_t distx, disty;

    x1 = x - pixman_fixed_1 / 2;
    y1 = y - pixman_fixed_1 / 2;

    distx = (x1 >> 8) & 0xff;
    disty = (y1 >> 8) & 0xff;

    x1 = pixman_fixed_to_int (x1);
    y1 = pixman_fixed_to_int (y1);
    x2 = x1 + 1;
    y2 = y1 + 1;

    if (repeat_mode != PIXMAN_REPEAT_NONE)
    {
	repeat (repeat_mode, width, &x1);
	repeat (repeat_mode, height, &y1);
	repeat (repeat_mode, width, &x2);
	repeat (repeat_mode, height, &y2);

	tl = get_pixel (image, x1, y1, FALSE);
	bl = get_pixel (image, x1, y2, FALSE);
	tr = get_pixel (image, x2, y1, FALSE);
	br = get_pixel (image, x2, y2, FALSE);
    }
    else
    {
	tl = get_pixel (image, x1, y1, TRUE);
	tr = get_pixel (image, x2, y1, TRUE);
	bl = get_pixel (image, x1, y2, TRUE);
	br = get_pixel (image, x2, y2, TRUE);
    }

    return bilinear_interpolation (tl, tr, bl, br, distx, disty);
}

static void
bits_image_fetch_bilinear_no_repeat_8888 (pixman_image_t * ima,
					  int              offset,
					  int              line,
					  int              width,
					  uint32_t *       buffer,
					  const uint32_t * mask,
					  uint32_t         mask_bits)
{
    bits_image_t *bits = &ima->bits;
    pixman_fixed_t x_top, x_bottom, x;
    pixman_fixed_t ux_top, ux_bottom, ux;
    pixman_vector_t v;
    uint32_t top_mask, bottom_mask;
    uint32_t *top_row;
    uint32_t *bottom_row;
    uint32_t *end;
    uint32_t zero[2] = { 0, 0 };
    int y, y1, y2;
    int disty;
    int mask_inc;
    int w;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (bits->common.transform, &v))
	return;

    ux = ux_top = ux_bottom = bits->common.transform->matrix[0][0];
    x = x_top = x_bottom = v.vector[0] - pixman_fixed_1/2;

    y = v.vector[1] - pixman_fixed_1/2;
    disty = (y >> 8) & 0xff;

    /* Load the pointers to the first and second lines from the source
     * image that bilinear code must read.
     *
     * The main trick in this code is about the check if any line are
     * outside of the image;
     *
     * When I realize that a line (any one) is outside, I change
     * the pointer to a dummy area with zeros. Once I change this, I
     * must be sure the pointer will not change, so I set the
     * variables to each pointer increments inside the loop.
     */
    y1 = pixman_fixed_to_int (y);
    y2 = y1 + 1;

    if (y1 < 0 || y1 >= bits->height)
    {
	top_row = zero;
	x_top = 0;
	ux_top = 0;
    }
    else
    {
	top_row = bits->bits + y1 * bits->rowstride;
	x_top = x;
	ux_top = ux;
    }

    if (y2 < 0 || y2 >= bits->height)
    {
	bottom_row = zero;
	x_bottom = 0;
	ux_bottom = 0;
    }
    else
    {
	bottom_row = bits->bits + y2 * bits->rowstride;
	x_bottom = x;
	ux_bottom = ux;
    }

    /* Instead of checking whether the operation uses the mast in
     * each loop iteration, verify this only once and prepare the
     * variables to make the code smaller inside the loop.
     */
    if (!mask)
    {
        mask_inc = 0;
        mask_bits = 1;
        mask = &mask_bits;
    }
    else
    {
        /* If have a mask, prepare the variables to check it */
        mask_inc = 1;
    }

    /* If both are zero, then the whole thing is zero */
    if (top_row == zero && bottom_row == zero)
    {
	memset (buffer, 0, width * sizeof (uint32_t));
	return;
    }
    else if (bits->format == PIXMAN_x8r8g8b8)
    {
	if (top_row == zero)
	{
	    top_mask = 0;
	    bottom_mask = 0xff000000;
	}
	else if (bottom_row == zero)
	{
	    top_mask = 0xff000000;
	    bottom_mask = 0;
	}
	else
	{
	    top_mask = 0xff000000;
	    bottom_mask = 0xff000000;
	}
    }
    else
    {
	top_mask = 0;
	bottom_mask = 0;
    }

    end = buffer + width;

    /* Zero fill to the left of the image */
    while (buffer < end && x < pixman_fixed_minus_1)
    {
	*buffer++ = 0;
	x += ux;
	x_top += ux_top;
	x_bottom += ux_bottom;
	mask += mask_inc;
    }

    /* Left edge
     */
    while (buffer < end && x < 0)
    {
	uint32_t tr, br;
	int32_t distx;

	tr = top_row[pixman_fixed_to_int (x_top) + 1] | top_mask;
	br = bottom_row[pixman_fixed_to_int (x_bottom) + 1] | bottom_mask;

	distx = (x >> 8) & 0xff;

	*buffer++ = bilinear_interpolation (0, tr, 0, br, distx, disty);

	x += ux;
	x_top += ux_top;
	x_bottom += ux_bottom;
	mask += mask_inc;
    }

    /* Main part */
    w = pixman_int_to_fixed (bits->width - 1);

    while (buffer < end  &&  x < w)
    {
	if (*mask)
	{
	    uint32_t tl, tr, bl, br;
	    int32_t distx;

	    tl = top_row [pixman_fixed_to_int (x_top)] | top_mask;
	    tr = top_row [pixman_fixed_to_int (x_top) + 1] | top_mask;
	    bl = bottom_row [pixman_fixed_to_int (x_bottom)] | bottom_mask;
	    br = bottom_row [pixman_fixed_to_int (x_bottom) + 1] | bottom_mask;

	    distx = (x >> 8) & 0xff;

	    *buffer = bilinear_interpolation (tl, tr, bl, br, distx, disty);
	}

	buffer++;
	x += ux;
	x_top += ux_top;
	x_bottom += ux_bottom;
	mask += mask_inc;
    }

    /* Right Edge */
    w = pixman_int_to_fixed (bits->width);
    while (buffer < end  &&  x < w)
    {
	if (*mask)
	{
	    uint32_t tl, bl;
	    int32_t distx;

	    tl = top_row [pixman_fixed_to_int (x_top)] | top_mask;
	    bl = bottom_row [pixman_fixed_to_int (x_bottom)] | bottom_mask;

	    distx = (x >> 8) & 0xff;

	    *buffer = bilinear_interpolation (tl, 0, bl, 0, distx, disty);
	}

	buffer++;
	x += ux;
	x_top += ux_top;
	x_bottom += ux_bottom;
	mask += mask_inc;
    }

    /* Zero fill to the left of the image */
    while (buffer < end)
	*buffer++ = 0;
}

static force_inline uint32_t
bits_image_fetch_pixel_convolution (bits_image_t   *image,
				    pixman_fixed_t  x,
				    pixman_fixed_t  y)
{
    pixman_fixed_t *params = image->common.filter_params;
    int x_off = (params[0] - pixman_fixed_1) >> 1;
    int y_off = (params[1] - pixman_fixed_1) >> 1;
    int32_t cwidth = pixman_fixed_to_int (params[0]);
    int32_t cheight = pixman_fixed_to_int (params[1]);
    int32_t srtot, sgtot, sbtot, satot;
    int32_t i, j, x1, x2, y1, y2;
    pixman_repeat_t repeat_mode = image->common.repeat;
    int width = image->width;
    int height = image->height;

    params += 2;

    x1 = pixman_fixed_to_int (x - pixman_fixed_e - x_off);
    y1 = pixman_fixed_to_int (y - pixman_fixed_e - y_off);
    x2 = x1 + cwidth;
    y2 = y1 + cheight;

    srtot = sgtot = sbtot = satot = 0;

    for (i = y1; i < y2; ++i)
    {
	for (j = x1; j < x2; ++j)
	{
	    int rx = j;
	    int ry = i;

	    pixman_fixed_t f = *params;

	    if (f)
	    {
		uint32_t pixel;

		if (repeat_mode != PIXMAN_REPEAT_NONE)
		{
		    repeat (repeat_mode, width, &rx);
		    repeat (repeat_mode, height, &ry);

		    pixel = get_pixel (image, rx, ry, FALSE);
		}
		else
		{
		    pixel = get_pixel (image, rx, ry, TRUE);
		}

		srtot += RED_8 (pixel) * f;
		sgtot += GREEN_8 (pixel) * f;
		sbtot += BLUE_8 (pixel) * f;
		satot += ALPHA_8 (pixel) * f;
	    }

	    params++;
	}
    }

    satot >>= 16;
    srtot >>= 16;
    sgtot >>= 16;
    sbtot >>= 16;

    satot = CLIP (satot, 0, 0xff);
    srtot = CLIP (srtot, 0, 0xff);
    sgtot = CLIP (sgtot, 0, 0xff);
    sbtot = CLIP (sbtot, 0, 0xff);

    return ((satot << 24) | (srtot << 16) | (sgtot <<  8) | (sbtot));
}

static force_inline uint32_t
bits_image_fetch_pixel_filtered (bits_image_t *image,
				 pixman_fixed_t x,
				 pixman_fixed_t y)
{
    switch (image->common.filter)
    {
    case PIXMAN_FILTER_NEAREST:
    case PIXMAN_FILTER_FAST:
	return bits_image_fetch_pixel_nearest (image, x, y);
	break;

    case PIXMAN_FILTER_BILINEAR:
    case PIXMAN_FILTER_GOOD:
    case PIXMAN_FILTER_BEST:
	return bits_image_fetch_pixel_bilinear (image, x, y);
	break;

    case PIXMAN_FILTER_CONVOLUTION:
	return bits_image_fetch_pixel_convolution (image, x, y);
	break;

    default:
        break;
    }

    return 0;
}

static void
bits_image_fetch_transformed (pixman_image_t * image,
                              int              offset,
                              int              line,
                              int              width,
                              uint32_t *       buffer,
                              const uint32_t * mask,
                              uint32_t         mask_bits)
{
    pixman_fixed_t x, y, w;
    pixman_fixed_t ux, uy, uw;
    pixman_vector_t v;
    int i;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    /* when using convolution filters or PIXMAN_REPEAT_PAD one
     * might get here without a transform */
    if (image->common.transform)
    {
	if (!pixman_transform_point_3d (image->common.transform, &v))
	    return;

	ux = image->common.transform->matrix[0][0];
	uy = image->common.transform->matrix[1][0];
	uw = image->common.transform->matrix[2][0];
    }
    else
    {
	ux = pixman_fixed_1;
	uy = 0;
	uw = 0;
    }

    x = v.vector[0];
    y = v.vector[1];
    w = v.vector[2];

    if (w == pixman_fixed_1 && uw == 0) /* Affine */
    {
	for (i = 0; i < width; ++i)
	{
	    if (!mask || (mask[i] & mask_bits))
	    {
		buffer[i] =
		    bits_image_fetch_pixel_filtered (&image->bits, x, y);
	    }

	    x += ux;
	    y += uy;
	}
    }
    else
    {
	for (i = 0; i < width; ++i)
	{
	    pixman_fixed_t x0, y0;

	    if (!mask || (mask[i] & mask_bits))
	    {
		x0 = ((pixman_fixed_48_16_t)x << 16) / w;
		y0 = ((pixman_fixed_48_16_t)y << 16) / w;

		buffer[i] =
		    bits_image_fetch_pixel_filtered (&image->bits, x0, y0);
	    }

	    x += ux;
	    y += uy;
	    w += uw;
	}
    }
}

static void
bits_image_fetch_solid_32 (pixman_image_t * image,
                           int              x,
                           int              y,
                           int              width,
                           uint32_t *       buffer,
                           const uint32_t * mask,
                           uint32_t         mask_bits)
{
    uint32_t color;
    uint32_t *end;

    color = image->bits.fetch_pixel_raw_32 (&image->bits, 0, 0);

    end = buffer + width;
    while (buffer < end)
	*(buffer++) = color;
}

static void
bits_image_fetch_solid_64 (pixman_image_t * image,
                           int              x,
                           int              y,
                           int              width,
                           uint32_t *       b,
                           const uint32_t * unused,
                           uint32_t         unused2)
{
    uint64_t color;
    uint64_t *buffer = (uint64_t *)b;
    uint64_t *end;

    color = image->bits.fetch_pixel_raw_64 (&image->bits, 0, 0);

    end = buffer + width;
    while (buffer < end)
	*(buffer++) = color;
}

static void
bits_image_fetch_untransformed_repeat_none (bits_image_t *image,
                                            pixman_bool_t wide,
                                            int           x,
                                            int           y,
                                            int           width,
                                            uint32_t *    buffer)
{
    uint32_t w;

    if (y < 0 || y >= image->height)
    {
	memset (buffer, 0, width * (wide? 8 : 4));
	return;
    }

    if (x < 0)
    {
	w = MIN (width, -x);

	memset (buffer, 0, w * (wide ? 8 : 4));

	width -= w;
	buffer += w * (wide? 2 : 1);
	x += w;
    }

    if (x < image->width)
    {
	w = MIN (width, image->width - x);

	if (wide)
	    image->fetch_scanline_raw_64 ((pixman_image_t *)image, x, y, w, buffer, NULL, 0);
	else
	    image->fetch_scanline_raw_32 ((pixman_image_t *)image, x, y, w, buffer, NULL, 0);

	width -= w;
	buffer += w * (wide? 2 : 1);
	x += w;
    }

    memset (buffer, 0, width * (wide ? 8 : 4));
}

static void
bits_image_fetch_untransformed_repeat_normal (bits_image_t *image,
                                              pixman_bool_t wide,
                                              int           x,
                                              int           y,
                                              int           width,
                                              uint32_t *    buffer)
{
    uint32_t w;

    while (y < 0)
	y += image->height;

    while (y >= image->height)
	y -= image->height;

    while (width)
    {
	while (x < 0)
	    x += image->width;
	while (x >= image->width)
	    x -= image->width;

	w = MIN (width, image->width - x);

	if (wide)
	    image->fetch_scanline_raw_64 ((pixman_image_t *)image, x, y, w, buffer, NULL, 0);
	else
	    image->fetch_scanline_raw_32 ((pixman_image_t *)image, x, y, w, buffer, NULL, 0);

	buffer += w * (wide? 2 : 1);
	x += w;
	width -= w;
    }
}

static void
bits_image_fetch_untransformed_32 (pixman_image_t * image,
                                   int              x,
                                   int              y,
                                   int              width,
                                   uint32_t *       buffer,
                                   const uint32_t * mask,
                                   uint32_t         mask_bits)
{
    if (image->common.repeat == PIXMAN_REPEAT_NONE)
    {
	bits_image_fetch_untransformed_repeat_none (
	    &image->bits, FALSE, x, y, width, buffer);
    }
    else
    {
	bits_image_fetch_untransformed_repeat_normal (
	    &image->bits, FALSE, x, y, width, buffer);
    }
}

static void
bits_image_fetch_untransformed_64 (pixman_image_t * image,
                                   int              x,
                                   int              y,
                                   int              width,
                                   uint32_t *       buffer,
                                   const uint32_t * unused,
                                   uint32_t         unused2)
{
    if (image->common.repeat == PIXMAN_REPEAT_NONE)
    {
	bits_image_fetch_untransformed_repeat_none (
	    &image->bits, TRUE, x, y, width, buffer);
    }
    else
    {
	bits_image_fetch_untransformed_repeat_normal (
	    &image->bits, TRUE, x, y, width, buffer);
    }
}

static pixman_bool_t out_of_bounds_workaround = TRUE;

/* Old X servers rely on out-of-bounds accesses when they are asked
 * to composite with a window as the source. They create a pixman image
 * pointing to some bogus position in memory, but then they set a clip
 * region to the position where the actual bits are.
 *
 * Due to a bug in old versions of pixman, where it would not clip
 * against the image bounds when a clip region was set, this would
 * actually work. So by default we allow certain out-of-bound access
 * to happen unless explicitly disabled.
 *
 * Fixed X servers should call this function to disable the workaround.
 */
PIXMAN_EXPORT void
pixman_disable_out_of_bounds_workaround (void)
{
    out_of_bounds_workaround = FALSE;
}

static pixman_bool_t
source_image_needs_out_of_bounds_workaround (bits_image_t *image)
{
    if (image->common.clip_sources                      &&
        image->common.repeat == PIXMAN_REPEAT_NONE      &&
	image->common.have_clip_region			&&
        out_of_bounds_workaround)
    {
	if (!image->common.client_clip)
	{
	    /* There is no client clip, so if the clip region extends beyond the
	     * drawable geometry, it must be because the X server generated the
	     * bogus clip region.
	     */
	    const pixman_box32_t *extents = pixman_region32_extents (&image->common.clip_region);

	    if (extents->x1 >= 0 && extents->x2 <= image->width &&
		extents->y1 >= 0 && extents->y2 <= image->height)
	    {
		return FALSE;
	    }
	}

	return TRUE;
    }

    return FALSE;
}

static void
bits_image_property_changed (pixman_image_t *image)
{
    bits_image_t *bits = (bits_image_t *)image;

    _pixman_bits_image_setup_raw_accessors (bits);

    image->bits.fetch_pixel_32 = image->bits.fetch_pixel_raw_32;

    if (bits->common.alpha_map)
    {
	image->common.get_scanline_64 =
	    _pixman_image_get_scanline_generic_64;
	image->common.get_scanline_32 =
	    bits_image_fetch_transformed;

	image->bits.fetch_pixel_32 = bits_image_fetch_pixel_alpha;
    }
    else if ((bits->common.repeat != PIXMAN_REPEAT_NONE) &&
             bits->width == 1 &&
             bits->height == 1)
    {
	image->common.get_scanline_64 = bits_image_fetch_solid_64;
	image->common.get_scanline_32 = bits_image_fetch_solid_32;
    }
    else if (!bits->common.transform &&
             bits->common.filter != PIXMAN_FILTER_CONVOLUTION &&
             (bits->common.repeat == PIXMAN_REPEAT_NONE ||
              bits->common.repeat == PIXMAN_REPEAT_NORMAL))
    {
	image->common.get_scanline_64 = bits_image_fetch_untransformed_64;
	image->common.get_scanline_32 = bits_image_fetch_untransformed_32;
    }
    else if (bits->common.transform					&&
	     bits->common.transform->matrix[2][0] == 0			&&
	     bits->common.transform->matrix[2][1] == 0			&&
	     bits->common.transform->matrix[2][2] == pixman_fixed_1	&&
	     bits->common.transform->matrix[0][0] > 0			&&
	     bits->common.transform->matrix[1][0] == 0			&&
	     (bits->common.filter == PIXMAN_FILTER_BILINEAR ||
	      bits->common.filter == PIXMAN_FILTER_GOOD	    ||
	      bits->common.filter == PIXMAN_FILTER_BEST)		&&
	     bits->common.repeat == PIXMAN_REPEAT_NONE			&&
	     (bits->format == PIXMAN_a8r8g8b8	||
	      bits->format == PIXMAN_x8r8g8b8))
    {
	image->common.get_scanline_64 =
	    _pixman_image_get_scanline_generic_64;
	image->common.get_scanline_32 =
	    bits_image_fetch_bilinear_no_repeat_8888;
    }
    else
    {
	image->common.get_scanline_64 =
	    _pixman_image_get_scanline_generic_64;
	image->common.get_scanline_32 =
	    bits_image_fetch_transformed;
    }

    bits->store_scanline_64 = bits_image_store_scanline_64;
    bits->store_scanline_32 = bits_image_store_scanline_32;

    bits->common.need_workaround =
        source_image_needs_out_of_bounds_workaround (bits);
}

static uint32_t *
create_bits (pixman_format_code_t format,
             int                  width,
             int                  height,
             int *                rowstride_bytes)
{
    int stride;
    int buf_size;
    int bpp;

    /* what follows is a long-winded way, avoiding any possibility of integer
     * overflows, of saying:
     * stride = ((width * bpp + 0x1f) >> 5) * sizeof (uint32_t);
     */

    bpp = PIXMAN_FORMAT_BPP (format);
    if (pixman_multiply_overflows_int (width, bpp))
	return NULL;

    stride = width * bpp;
    if (pixman_addition_overflows_int (stride, 0x1f))
	return NULL;

    stride += 0x1f;
    stride >>= 5;

    stride *= sizeof (uint32_t);

    if (pixman_multiply_overflows_int (height, stride))
	return NULL;

    buf_size = height * stride;

    if (rowstride_bytes)
	*rowstride_bytes = stride;

    return calloc (buf_size, 1);
}

PIXMAN_EXPORT pixman_image_t *
pixman_image_create_bits (pixman_format_code_t format,
                          int                  width,
                          int                  height,
                          uint32_t *           bits,
                          int                  rowstride_bytes)
{
    pixman_image_t *image;
    uint32_t *free_me = NULL;

    /* must be a whole number of uint32_t's
     */
    return_val_if_fail (bits == NULL ||
                        (rowstride_bytes % sizeof (uint32_t)) == 0, NULL);

    if (!bits && width && height)
    {
	free_me = bits = create_bits (format, width, height, &rowstride_bytes);
	if (!bits)
	    return NULL;
    }

    image = _pixman_image_allocate ();

    if (!image)
    {
	if (free_me)
	    free (free_me);

	return NULL;
    }

    image->type = BITS;
    image->bits.format = format;
    image->bits.width = width;
    image->bits.height = height;
    image->bits.bits = bits;
    image->bits.free_me = free_me;
    image->bits.read_func = NULL;
    image->bits.write_func = NULL;

    /* The rowstride is stored in number of uint32_t */
    image->bits.rowstride = rowstride_bytes / (int) sizeof (uint32_t);

    image->bits.indexed = NULL;

    image->common.property_changed = bits_image_property_changed;

    _pixman_image_reset_clip_region (image);

    return image;
}
