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
#include "pixman-inlines.h"

static uint32_t *
_pixman_image_get_scanline_generic_float (pixman_iter_t * iter,
					  const uint32_t *mask)
{
    pixman_iter_get_scanline_t fetch_32 = iter->data;
    uint32_t *buffer = iter->buffer;

    fetch_32 (iter, NULL);

    pixman_expand_to_float ((argb_t *)buffer, buffer, PIXMAN_a8r8g8b8, iter->width);

    return iter->buffer;
}

/* Fetch functions */

static force_inline uint32_t
fetch_pixel_no_alpha (bits_image_t *image,
		      int x, int y, pixman_bool_t check_bounds)
{
    if (check_bounds &&
	(x < 0 || x >= image->width || y < 0 || y >= image->height))
    {
	return 0;
    }

    return image->fetch_pixel_32 (image, x, y);
}

typedef uint32_t (* get_pixel_t) (bits_image_t *image,
				  int x, int y, pixman_bool_t check_bounds);

static force_inline uint32_t
bits_image_fetch_pixel_nearest (bits_image_t   *image,
				pixman_fixed_t  x,
				pixman_fixed_t  y,
				get_pixel_t	get_pixel)
{
    int x0 = pixman_fixed_to_int (x - pixman_fixed_e);
    int y0 = pixman_fixed_to_int (y - pixman_fixed_e);

    if (image->common.repeat != PIXMAN_REPEAT_NONE)
    {
	repeat (image->common.repeat, &x0, image->width);
	repeat (image->common.repeat, &y0, image->height);

	return get_pixel (image, x0, y0, FALSE);
    }
    else
    {
	return get_pixel (image, x0, y0, TRUE);
    }
}

static force_inline uint32_t
bits_image_fetch_pixel_bilinear (bits_image_t   *image,
				 pixman_fixed_t  x,
				 pixman_fixed_t  y,
				 get_pixel_t	 get_pixel)
{
    pixman_repeat_t repeat_mode = image->common.repeat;
    int width = image->width;
    int height = image->height;
    int x1, y1, x2, y2;
    uint32_t tl, tr, bl, br;
    int32_t distx, disty;

    x1 = x - pixman_fixed_1 / 2;
    y1 = y - pixman_fixed_1 / 2;

    distx = pixman_fixed_to_bilinear_weight (x1);
    disty = pixman_fixed_to_bilinear_weight (y1);

    x1 = pixman_fixed_to_int (x1);
    y1 = pixman_fixed_to_int (y1);
    x2 = x1 + 1;
    y2 = y1 + 1;

    if (repeat_mode != PIXMAN_REPEAT_NONE)
    {
	repeat (repeat_mode, &x1, width);
	repeat (repeat_mode, &y1, height);
	repeat (repeat_mode, &x2, width);
	repeat (repeat_mode, &y2, height);

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

static uint32_t *
bits_image_fetch_bilinear_no_repeat_8888 (pixman_iter_t *iter,
					  const uint32_t *mask)
{

    pixman_image_t * ima = iter->image;
    int              offset = iter->x;
    int              line = iter->y++;
    int              width = iter->width;
    uint32_t *       buffer = iter->buffer;

    bits_image_t *bits = &ima->bits;
    pixman_fixed_t x_top, x_bottom, x;
    pixman_fixed_t ux_top, ux_bottom, ux;
    pixman_vector_t v;
    uint32_t top_mask, bottom_mask;
    uint32_t *top_row;
    uint32_t *bottom_row;
    uint32_t *end;
    uint32_t zero[2] = { 0, 0 };
    uint32_t one = 1;
    int y, y1, y2;
    int disty;
    int mask_inc;
    int w;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (bits->common.transform, &v))
	return iter->buffer;

    ux = ux_top = ux_bottom = bits->common.transform->matrix[0][0];
    x = x_top = x_bottom = v.vector[0] - pixman_fixed_1/2;

    y = v.vector[1] - pixman_fixed_1/2;
    disty = pixman_fixed_to_bilinear_weight (y);

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
        mask = &one;
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
	return iter->buffer;
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

	distx = pixman_fixed_to_bilinear_weight (x);

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

	    distx = pixman_fixed_to_bilinear_weight (x);

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

	    distx = pixman_fixed_to_bilinear_weight (x);

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

    return iter->buffer;
}

static force_inline uint32_t
bits_image_fetch_pixel_convolution (bits_image_t   *image,
				    pixman_fixed_t  x,
				    pixman_fixed_t  y,
				    get_pixel_t     get_pixel)
{
    pixman_fixed_t *params = image->common.filter_params;
    int x_off = (params[0] - pixman_fixed_1) >> 1;
    int y_off = (params[1] - pixman_fixed_1) >> 1;
    int32_t cwidth = pixman_fixed_to_int (params[0]);
    int32_t cheight = pixman_fixed_to_int (params[1]);
    int32_t i, j, x1, x2, y1, y2;
    pixman_repeat_t repeat_mode = image->common.repeat;
    int width = image->width;
    int height = image->height;
    int srtot, sgtot, sbtot, satot;

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
		    repeat (repeat_mode, &rx, width);
		    repeat (repeat_mode, &ry, height);

		    pixel = get_pixel (image, rx, ry, FALSE);
		}
		else
		{
		    pixel = get_pixel (image, rx, ry, TRUE);
		}

		srtot += (int)RED_8 (pixel) * f;
		sgtot += (int)GREEN_8 (pixel) * f;
		sbtot += (int)BLUE_8 (pixel) * f;
		satot += (int)ALPHA_8 (pixel) * f;
	    }

	    params++;
	}
    }

    satot = (satot + 0x8000) >> 16;
    srtot = (srtot + 0x8000) >> 16;
    sgtot = (sgtot + 0x8000) >> 16;
    sbtot = (sbtot + 0x8000) >> 16;

    satot = CLIP (satot, 0, 0xff);
    srtot = CLIP (srtot, 0, 0xff);
    sgtot = CLIP (sgtot, 0, 0xff);
    sbtot = CLIP (sbtot, 0, 0xff);

    return ((satot << 24) | (srtot << 16) | (sgtot <<  8) | (sbtot));
}

static uint32_t
bits_image_fetch_pixel_separable_convolution (bits_image_t *image,
                                              pixman_fixed_t x,
                                              pixman_fixed_t y,
                                              get_pixel_t    get_pixel)
{
    pixman_fixed_t *params = image->common.filter_params;
    pixman_repeat_t repeat_mode = image->common.repeat;
    int width = image->width;
    int height = image->height;
    int cwidth = pixman_fixed_to_int (params[0]);
    int cheight = pixman_fixed_to_int (params[1]);
    int x_phase_bits = pixman_fixed_to_int (params[2]);
    int y_phase_bits = pixman_fixed_to_int (params[3]);
    int x_phase_shift = 16 - x_phase_bits;
    int y_phase_shift = 16 - y_phase_bits;
    int x_off = ((cwidth << 16) - pixman_fixed_1) >> 1;
    int y_off = ((cheight << 16) - pixman_fixed_1) >> 1;
    pixman_fixed_t *y_params;
    int srtot, sgtot, sbtot, satot;
    int32_t x1, x2, y1, y2;
    int32_t px, py;
    int i, j;

    /* Round x and y to the middle of the closest phase before continuing. This
     * ensures that the convolution matrix is aligned right, since it was
     * positioned relative to a particular phase (and not relative to whatever
     * exact fraction we happen to get here).
     */
    x = ((x >> x_phase_shift) << x_phase_shift) + ((1 << x_phase_shift) >> 1);
    y = ((y >> y_phase_shift) << y_phase_shift) + ((1 << y_phase_shift) >> 1);

    px = (x & 0xffff) >> x_phase_shift;
    py = (y & 0xffff) >> y_phase_shift;

    y_params = params + 4 + (1 << x_phase_bits) * cwidth + py * cheight;

    x1 = pixman_fixed_to_int (x - pixman_fixed_e - x_off);
    y1 = pixman_fixed_to_int (y - pixman_fixed_e - y_off);
    x2 = x1 + cwidth;
    y2 = y1 + cheight;

    srtot = sgtot = sbtot = satot = 0;

    for (i = y1; i < y2; ++i)
    {
        pixman_fixed_48_16_t fy = *y_params++;
        pixman_fixed_t *x_params = params + 4 + px * cwidth;

        if (fy)
        {
            for (j = x1; j < x2; ++j)
            {
                pixman_fixed_t fx = *x_params++;
		int rx = j;
		int ry = i;

                if (fx)
                {
                    pixman_fixed_t f;
                    uint32_t pixel;

                    if (repeat_mode != PIXMAN_REPEAT_NONE)
                    {
                        repeat (repeat_mode, &rx, width);
                        repeat (repeat_mode, &ry, height);

                        pixel = get_pixel (image, rx, ry, FALSE);
                    }
                    else
                    {
                        pixel = get_pixel (image, rx, ry, TRUE);
		    }

                    f = (fy * fx + 0x8000) >> 16;

                    srtot += (int)RED_8 (pixel) * f;
                    sgtot += (int)GREEN_8 (pixel) * f;
                    sbtot += (int)BLUE_8 (pixel) * f;
                    satot += (int)ALPHA_8 (pixel) * f;
                }
            }
	}
    }

    satot = (satot + 0x8000) >> 16;
    srtot = (srtot + 0x8000) >> 16;
    sgtot = (sgtot + 0x8000) >> 16;
    sbtot = (sbtot + 0x8000) >> 16;

    satot = CLIP (satot, 0, 0xff);
    srtot = CLIP (srtot, 0, 0xff);
    sgtot = CLIP (sgtot, 0, 0xff);
    sbtot = CLIP (sbtot, 0, 0xff);

    return ((satot << 24) | (srtot << 16) | (sgtot <<  8) | (sbtot));
}

static force_inline uint32_t
bits_image_fetch_pixel_filtered (bits_image_t *image,
				 pixman_fixed_t x,
				 pixman_fixed_t y,
				 get_pixel_t    get_pixel)
{
    switch (image->common.filter)
    {
    case PIXMAN_FILTER_NEAREST:
    case PIXMAN_FILTER_FAST:
	return bits_image_fetch_pixel_nearest (image, x, y, get_pixel);
	break;

    case PIXMAN_FILTER_BILINEAR:
    case PIXMAN_FILTER_GOOD:
    case PIXMAN_FILTER_BEST:
	return bits_image_fetch_pixel_bilinear (image, x, y, get_pixel);
	break;

    case PIXMAN_FILTER_CONVOLUTION:
	return bits_image_fetch_pixel_convolution (image, x, y, get_pixel);
	break;

    case PIXMAN_FILTER_SEPARABLE_CONVOLUTION:
        return bits_image_fetch_pixel_separable_convolution (image, x, y, get_pixel);
        break;

    default:
        break;
    }

    return 0;
}

static uint32_t *
bits_image_fetch_affine_no_alpha (pixman_iter_t *  iter,
				  const uint32_t * mask)
{
    pixman_image_t *image  = iter->image;
    int             offset = iter->x;
    int             line   = iter->y++;
    int             width  = iter->width;
    uint32_t *      buffer = iter->buffer;

    pixman_fixed_t x, y;
    pixman_fixed_t ux, uy;
    pixman_vector_t v;
    int i;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (image->common.transform)
    {
	if (!pixman_transform_point_3d (image->common.transform, &v))
	    return iter->buffer;

	ux = image->common.transform->matrix[0][0];
	uy = image->common.transform->matrix[1][0];
    }
    else
    {
	ux = pixman_fixed_1;
	uy = 0;
    }

    x = v.vector[0];
    y = v.vector[1];

    for (i = 0; i < width; ++i)
    {
	if (!mask || mask[i])
	{
	    buffer[i] = bits_image_fetch_pixel_filtered (
		&image->bits, x, y, fetch_pixel_no_alpha);
	}

	x += ux;
	y += uy;
    }

    return buffer;
}

/* General fetcher */
static force_inline uint32_t
fetch_pixel_general (bits_image_t *image, int x, int y, pixman_bool_t check_bounds)
{
    uint32_t pixel;

    if (check_bounds &&
	(x < 0 || x >= image->width || y < 0 || y >= image->height))
    {
	return 0;
    }

    pixel = image->fetch_pixel_32 (image, x, y);

    if (image->common.alpha_map)
    {
	uint32_t pixel_a;

	x -= image->common.alpha_origin_x;
	y -= image->common.alpha_origin_y;

	if (x < 0 || x >= image->common.alpha_map->width ||
	    y < 0 || y >= image->common.alpha_map->height)
	{
	    pixel_a = 0;
	}
	else
	{
	    pixel_a = image->common.alpha_map->fetch_pixel_32 (
		image->common.alpha_map, x, y);

	    pixel_a = ALPHA_8 (pixel_a);
	}

	pixel &= 0x00ffffff;
	pixel |= (pixel_a << 24);
    }

    return pixel;
}

static uint32_t *
bits_image_fetch_general (pixman_iter_t  *iter,
			  const uint32_t *mask)
{
    pixman_image_t *image  = iter->image;
    int             offset = iter->x;
    int             line   = iter->y++;
    int             width  = iter->width;
    uint32_t *      buffer = iter->buffer;

    pixman_fixed_t x, y, w;
    pixman_fixed_t ux, uy, uw;
    pixman_vector_t v;
    int i;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (image->common.transform)
    {
	if (!pixman_transform_point_3d (image->common.transform, &v))
	    return buffer;

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

    for (i = 0; i < width; ++i)
    {
	pixman_fixed_t x0, y0;

	if (!mask || mask[i])
	{
	    if (w != 0)
	    {
		x0 = ((pixman_fixed_48_16_t)x << 16) / w;
		y0 = ((pixman_fixed_48_16_t)y << 16) / w;
	    }
	    else
	    {
		x0 = 0;
		y0 = 0;
	    }

	    buffer[i] = bits_image_fetch_pixel_filtered (
		&image->bits, x0, y0, fetch_pixel_general);
	}

	x += ux;
	y += uy;
	w += uw;
    }

    return buffer;
}

typedef uint32_t (* convert_pixel_t) (const uint8_t *row, int x);

static force_inline void
bits_image_fetch_separable_convolution_affine (pixman_image_t * image,
					       int              offset,
					       int              line,
					       int              width,
					       uint32_t *       buffer,
					       const uint32_t * mask,

					       convert_pixel_t	convert_pixel,
					       pixman_format_code_t	format,
					       pixman_repeat_t	repeat_mode)
{
    bits_image_t *bits = &image->bits;
    pixman_fixed_t *params = image->common.filter_params;
    int cwidth = pixman_fixed_to_int (params[0]);
    int cheight = pixman_fixed_to_int (params[1]);
    int x_off = ((cwidth << 16) - pixman_fixed_1) >> 1;
    int y_off = ((cheight << 16) - pixman_fixed_1) >> 1;
    int x_phase_bits = pixman_fixed_to_int (params[2]);
    int y_phase_bits = pixman_fixed_to_int (params[3]);
    int x_phase_shift = 16 - x_phase_bits;
    int y_phase_shift = 16 - y_phase_bits;
    pixman_fixed_t vx, vy;
    pixman_fixed_t ux, uy;
    pixman_vector_t v;
    int k;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (image->common.transform, &v))
	return;

    ux = image->common.transform->matrix[0][0];
    uy = image->common.transform->matrix[1][0];

    vx = v.vector[0];
    vy = v.vector[1];

    for (k = 0; k < width; ++k)
    {
	pixman_fixed_t *y_params;
	int satot, srtot, sgtot, sbtot;
	pixman_fixed_t x, y;
	int32_t x1, x2, y1, y2;
	int32_t px, py;
	int i, j;

	if (mask && !mask[k])
	    goto next;

	/* Round x and y to the middle of the closest phase before continuing. This
	 * ensures that the convolution matrix is aligned right, since it was
	 * positioned relative to a particular phase (and not relative to whatever
	 * exact fraction we happen to get here).
	 */
	x = ((vx >> x_phase_shift) << x_phase_shift) + ((1 << x_phase_shift) >> 1);
	y = ((vy >> y_phase_shift) << y_phase_shift) + ((1 << y_phase_shift) >> 1);

	px = (x & 0xffff) >> x_phase_shift;
	py = (y & 0xffff) >> y_phase_shift;

	x1 = pixman_fixed_to_int (x - pixman_fixed_e - x_off);
	y1 = pixman_fixed_to_int (y - pixman_fixed_e - y_off);
	x2 = x1 + cwidth;
	y2 = y1 + cheight;

	satot = srtot = sgtot = sbtot = 0;

	y_params = params + 4 + (1 << x_phase_bits) * cwidth + py * cheight;

	for (i = y1; i < y2; ++i)
	{
	    pixman_fixed_t fy = *y_params++;

	    if (fy)
	    {
		pixman_fixed_t *x_params = params + 4 + px * cwidth;

		for (j = x1; j < x2; ++j)
		{
		    pixman_fixed_t fx = *x_params++;
		    int rx = j;
		    int ry = i;
		    
		    if (fx)
		    {
			pixman_fixed_t f;
			uint32_t pixel, mask;
			uint8_t *row;

			mask = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;

			if (repeat_mode != PIXMAN_REPEAT_NONE)
			{
			    repeat (repeat_mode, &rx, bits->width);
			    repeat (repeat_mode, &ry, bits->height);

			    row = (uint8_t *)bits->bits + bits->rowstride * 4 * ry;
			    pixel = convert_pixel (row, rx) | mask;
			}
			else
			{
			    if (rx < 0 || ry < 0 || rx >= bits->width || ry >= bits->height)
			    {
				pixel = 0;
			    }
			    else
			    {
				row = (uint8_t *)bits->bits + bits->rowstride * 4 * ry;
				pixel = convert_pixel (row, rx) | mask;
			    }
			}

			f = ((pixman_fixed_32_32_t)fx * fy + 0x8000) >> 16;
			srtot += (int)RED_8 (pixel) * f;
			sgtot += (int)GREEN_8 (pixel) * f;
			sbtot += (int)BLUE_8 (pixel) * f;
			satot += (int)ALPHA_8 (pixel) * f;
		    }
		}
	    }
	}

	satot = (satot + 0x8000) >> 16;
	srtot = (srtot + 0x8000) >> 16;
	sgtot = (sgtot + 0x8000) >> 16;
	sbtot = (sbtot + 0x8000) >> 16;

	satot = CLIP (satot, 0, 0xff);
	srtot = CLIP (srtot, 0, 0xff);
	sgtot = CLIP (sgtot, 0, 0xff);
	sbtot = CLIP (sbtot, 0, 0xff);

	buffer[k] = (satot << 24) | (srtot << 16) | (sgtot << 8) | (sbtot << 0);

    next:
	vx += ux;
	vy += uy;
    }
}

static const uint8_t zero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static force_inline void
bits_image_fetch_bilinear_affine (pixman_image_t * image,
				  int              offset,
				  int              line,
				  int              width,
				  uint32_t *       buffer,
				  const uint32_t * mask,

				  convert_pixel_t	convert_pixel,
				  pixman_format_code_t	format,
				  pixman_repeat_t	repeat_mode)
{
    pixman_fixed_t x, y;
    pixman_fixed_t ux, uy;
    pixman_vector_t v;
    bits_image_t *bits = &image->bits;
    int i;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (image->common.transform, &v))
	return;

    ux = image->common.transform->matrix[0][0];
    uy = image->common.transform->matrix[1][0];

    x = v.vector[0];
    y = v.vector[1];

    for (i = 0; i < width; ++i)
    {
	int x1, y1, x2, y2;
	uint32_t tl, tr, bl, br;
	int32_t distx, disty;
	int width = image->bits.width;
	int height = image->bits.height;
	const uint8_t *row1;
	const uint8_t *row2;

	if (mask && !mask[i])
	    goto next;

	x1 = x - pixman_fixed_1 / 2;
	y1 = y - pixman_fixed_1 / 2;

	distx = pixman_fixed_to_bilinear_weight (x1);
	disty = pixman_fixed_to_bilinear_weight (y1);

	y1 = pixman_fixed_to_int (y1);
	y2 = y1 + 1;
	x1 = pixman_fixed_to_int (x1);
	x2 = x1 + 1;

	if (repeat_mode != PIXMAN_REPEAT_NONE)
	{
	    uint32_t mask;

	    mask = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;

	    repeat (repeat_mode, &x1, width);
	    repeat (repeat_mode, &y1, height);
	    repeat (repeat_mode, &x2, width);
	    repeat (repeat_mode, &y2, height);

	    row1 = (uint8_t *)bits->bits + bits->rowstride * 4 * y1;
	    row2 = (uint8_t *)bits->bits + bits->rowstride * 4 * y2;

	    tl = convert_pixel (row1, x1) | mask;
	    tr = convert_pixel (row1, x2) | mask;
	    bl = convert_pixel (row2, x1) | mask;
	    br = convert_pixel (row2, x2) | mask;
	}
	else
	{
	    uint32_t mask1, mask2;
	    int bpp;

	    /* Note: PIXMAN_FORMAT_BPP() returns an unsigned value,
	     * which means if you use it in expressions, those
	     * expressions become unsigned themselves. Since
	     * the variables below can be negative in some cases,
	     * that will lead to crashes on 64 bit architectures.
	     *
	     * So this line makes sure bpp is signed
	     */
	    bpp = PIXMAN_FORMAT_BPP (format);

	    if (x1 >= width || x2 < 0 || y1 >= height || y2 < 0)
	    {
		buffer[i] = 0;
		goto next;
	    }

	    if (y2 == 0)
	    {
		row1 = zero;
		mask1 = 0;
	    }
	    else
	    {
		row1 = (uint8_t *)bits->bits + bits->rowstride * 4 * y1;
		row1 += bpp / 8 * x1;

		mask1 = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;
	    }

	    if (y1 == height - 1)
	    {
		row2 = zero;
		mask2 = 0;
	    }
	    else
	    {
		row2 = (uint8_t *)bits->bits + bits->rowstride * 4 * y2;
		row2 += bpp / 8 * x1;

		mask2 = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;
	    }

	    if (x2 == 0)
	    {
		tl = 0;
		bl = 0;
	    }
	    else
	    {
		tl = convert_pixel (row1, 0) | mask1;
		bl = convert_pixel (row2, 0) | mask2;
	    }

	    if (x1 == width - 1)
	    {
		tr = 0;
		br = 0;
	    }
	    else
	    {
		tr = convert_pixel (row1, 1) | mask1;
		br = convert_pixel (row2, 1) | mask2;
	    }
	}

	buffer[i] = bilinear_interpolation (
	    tl, tr, bl, br, distx, disty);

    next:
	x += ux;
	y += uy;
    }
}

static force_inline void
bits_image_fetch_nearest_affine (pixman_image_t * image,
				 int              offset,
				 int              line,
				 int              width,
				 uint32_t *       buffer,
				 const uint32_t * mask,
				 
				 convert_pixel_t	convert_pixel,
				 pixman_format_code_t	format,
				 pixman_repeat_t	repeat_mode)
{
    pixman_fixed_t x, y;
    pixman_fixed_t ux, uy;
    pixman_vector_t v;
    bits_image_t *bits = &image->bits;
    int i;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (image->common.transform, &v))
	return;

    ux = image->common.transform->matrix[0][0];
    uy = image->common.transform->matrix[1][0];

    x = v.vector[0];
    y = v.vector[1];

    for (i = 0; i < width; ++i)
    {
	int width, height, x0, y0;
	const uint8_t *row;

	if (mask && !mask[i])
	    goto next;
	
	width = image->bits.width;
	height = image->bits.height;
	x0 = pixman_fixed_to_int (x - pixman_fixed_e);
	y0 = pixman_fixed_to_int (y - pixman_fixed_e);

	if (repeat_mode == PIXMAN_REPEAT_NONE &&
	    (y0 < 0 || y0 >= height || x0 < 0 || x0 >= width))
	{
	    buffer[i] = 0;
	}
	else
	{
	    uint32_t mask = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;

	    if (repeat_mode != PIXMAN_REPEAT_NONE)
	    {
		repeat (repeat_mode, &x0, width);
		repeat (repeat_mode, &y0, height);
	    }

	    row = (uint8_t *)bits->bits + bits->rowstride * 4 * y0;

	    buffer[i] = convert_pixel (row, x0) | mask;
	}

    next:
	x += ux;
	y += uy;
    }
}

static force_inline uint32_t
convert_a8r8g8b8 (const uint8_t *row, int x)
{
    return *(((uint32_t *)row) + x);
}

static force_inline uint32_t
convert_x8r8g8b8 (const uint8_t *row, int x)
{
    return *(((uint32_t *)row) + x);
}

static force_inline uint32_t
convert_a8 (const uint8_t *row, int x)
{
    return *(row + x) << 24;
}

static force_inline uint32_t
convert_r5g6b5 (const uint8_t *row, int x)
{
    return convert_0565_to_0888 (*((uint16_t *)row + x));
}

#define MAKE_SEPARABLE_CONVOLUTION_FETCHER(name, format, repeat_mode)  \
    static uint32_t *							\
    bits_image_fetch_separable_convolution_affine_ ## name (pixman_iter_t   *iter, \
							    const uint32_t * mask) \
    {									\
	bits_image_fetch_separable_convolution_affine (                 \
	    iter->image,                                                \
	    iter->x, iter->y++,                                         \
	    iter->width,                                                \
	    iter->buffer, mask,                                         \
	    convert_ ## format,                                         \
	    PIXMAN_ ## format,                                          \
	    repeat_mode);                                               \
									\
	return iter->buffer;                                            \
    }

#define MAKE_BILINEAR_FETCHER(name, format, repeat_mode)		\
    static uint32_t *							\
    bits_image_fetch_bilinear_affine_ ## name (pixman_iter_t   *iter,	\
					       const uint32_t * mask)	\
    {									\
	bits_image_fetch_bilinear_affine (iter->image,			\
					  iter->x, iter->y++,		\
					  iter->width,			\
					  iter->buffer, mask,		\
					  convert_ ## format,		\
					  PIXMAN_ ## format,		\
					  repeat_mode);			\
	return iter->buffer;						\
    }

#define MAKE_NEAREST_FETCHER(name, format, repeat_mode)			\
    static uint32_t *							\
    bits_image_fetch_nearest_affine_ ## name (pixman_iter_t   *iter,	\
					      const uint32_t * mask)	\
    {									\
	bits_image_fetch_nearest_affine (iter->image,			\
					 iter->x, iter->y++,		\
					 iter->width,			\
					 iter->buffer, mask,		\
					 convert_ ## format,		\
					 PIXMAN_ ## format,		\
					 repeat_mode);			\
	return iter->buffer;						\
    }

#define MAKE_FETCHERS(name, format, repeat_mode)			\
    MAKE_NEAREST_FETCHER (name, format, repeat_mode)			\
    MAKE_BILINEAR_FETCHER (name, format, repeat_mode)			\
    MAKE_SEPARABLE_CONVOLUTION_FETCHER (name, format, repeat_mode)

MAKE_FETCHERS (pad_a8r8g8b8,     a8r8g8b8, PIXMAN_REPEAT_PAD)
MAKE_FETCHERS (none_a8r8g8b8,    a8r8g8b8, PIXMAN_REPEAT_NONE)
MAKE_FETCHERS (reflect_a8r8g8b8, a8r8g8b8, PIXMAN_REPEAT_REFLECT)
MAKE_FETCHERS (normal_a8r8g8b8,  a8r8g8b8, PIXMAN_REPEAT_NORMAL)
MAKE_FETCHERS (pad_x8r8g8b8,     x8r8g8b8, PIXMAN_REPEAT_PAD)
MAKE_FETCHERS (none_x8r8g8b8,    x8r8g8b8, PIXMAN_REPEAT_NONE)
MAKE_FETCHERS (reflect_x8r8g8b8, x8r8g8b8, PIXMAN_REPEAT_REFLECT)
MAKE_FETCHERS (normal_x8r8g8b8,  x8r8g8b8, PIXMAN_REPEAT_NORMAL)
MAKE_FETCHERS (pad_a8,           a8,       PIXMAN_REPEAT_PAD)
MAKE_FETCHERS (none_a8,          a8,       PIXMAN_REPEAT_NONE)
MAKE_FETCHERS (reflect_a8,	 a8,       PIXMAN_REPEAT_REFLECT)
MAKE_FETCHERS (normal_a8,	 a8,       PIXMAN_REPEAT_NORMAL)
MAKE_FETCHERS (pad_r5g6b5,       r5g6b5,   PIXMAN_REPEAT_PAD)
MAKE_FETCHERS (none_r5g6b5,      r5g6b5,   PIXMAN_REPEAT_NONE)
MAKE_FETCHERS (reflect_r5g6b5,   r5g6b5,   PIXMAN_REPEAT_REFLECT)
MAKE_FETCHERS (normal_r5g6b5,    r5g6b5,   PIXMAN_REPEAT_NORMAL)

static void
replicate_pixel_32 (bits_image_t *   bits,
		    int              x,
		    int              y,
		    int              width,
		    uint32_t *       buffer)
{
    uint32_t color;
    uint32_t *end;

    color = bits->fetch_pixel_32 (bits, x, y);

    end = buffer + width;
    while (buffer < end)
	*(buffer++) = color;
}

static void
replicate_pixel_float (bits_image_t *   bits,
		       int              x,
		       int              y,
		       int              width,
		       uint32_t *       b)
{
    argb_t color;
    argb_t *buffer = (argb_t *)b;
    argb_t *end;

    color = bits->fetch_pixel_float (bits, x, y);

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
	memset (buffer, 0, width * (wide? sizeof (argb_t) : 4));
	return;
    }

    if (x < 0)
    {
	w = MIN (width, -x);

	memset (buffer, 0, w * (wide ? sizeof (argb_t) : 4));

	width -= w;
	buffer += w * (wide? 4 : 1);
	x += w;
    }

    if (x < image->width)
    {
	w = MIN (width, image->width - x);

	if (wide)
	    image->fetch_scanline_float ((pixman_image_t *)image, x, y, w, buffer, NULL);
	else
	    image->fetch_scanline_32 ((pixman_image_t *)image, x, y, w, buffer, NULL);

	width -= w;
	buffer += w * (wide? 4 : 1);
	x += w;
    }

    memset (buffer, 0, width * (wide ? sizeof (argb_t) : 4));
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

    if (image->width == 1)
    {
	if (wide)
	    replicate_pixel_float (image, 0, y, width, buffer);
	else
	    replicate_pixel_32 (image, 0, y, width, buffer);

	return;
    }

    while (width)
    {
	while (x < 0)
	    x += image->width;
	while (x >= image->width)
	    x -= image->width;

	w = MIN (width, image->width - x);

	if (wide)
	    image->fetch_scanline_float ((pixman_image_t *)image, x, y, w, buffer, NULL);
	else
	    image->fetch_scanline_32 ((pixman_image_t *)image, x, y, w, buffer, NULL);

	buffer += w * (wide? 4 : 1);
	x += w;
	width -= w;
    }
}

static uint32_t *
bits_image_fetch_untransformed_32 (pixman_iter_t * iter,
				   const uint32_t *mask)
{
    pixman_image_t *image  = iter->image;
    int             x      = iter->x;
    int             y      = iter->y;
    int             width  = iter->width;
    uint32_t *      buffer = iter->buffer;

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

    iter->y++;
    return buffer;
}

static uint32_t *
bits_image_fetch_untransformed_float (pixman_iter_t * iter,
				      const uint32_t *mask)
{
    pixman_image_t *image  = iter->image;
    int             x      = iter->x;
    int             y      = iter->y;
    int             width  = iter->width;
    uint32_t *      buffer = iter->buffer;

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

    iter->y++;
    return buffer;
}

typedef struct
{
    pixman_format_code_t	format;
    uint32_t			flags;
    pixman_iter_get_scanline_t	get_scanline_32;
    pixman_iter_get_scanline_t  get_scanline_float;
} fetcher_info_t;

static const fetcher_info_t fetcher_info[] =
{
    { PIXMAN_any,
      (FAST_PATH_NO_ALPHA_MAP			|
       FAST_PATH_ID_TRANSFORM			|
       FAST_PATH_NO_CONVOLUTION_FILTER		|
       FAST_PATH_NO_PAD_REPEAT			|
       FAST_PATH_NO_REFLECT_REPEAT),
      bits_image_fetch_untransformed_32,
      bits_image_fetch_untransformed_float
    },

#define FAST_BILINEAR_FLAGS						\
    (FAST_PATH_NO_ALPHA_MAP		|				\
     FAST_PATH_NO_ACCESSORS		|				\
     FAST_PATH_HAS_TRANSFORM		|				\
     FAST_PATH_AFFINE_TRANSFORM		|				\
     FAST_PATH_X_UNIT_POSITIVE		|				\
     FAST_PATH_Y_UNIT_ZERO		|				\
     FAST_PATH_NONE_REPEAT		|				\
     FAST_PATH_BILINEAR_FILTER)

    { PIXMAN_a8r8g8b8,
      FAST_BILINEAR_FLAGS,
      bits_image_fetch_bilinear_no_repeat_8888,
      _pixman_image_get_scanline_generic_float
    },

    { PIXMAN_x8r8g8b8,
      FAST_BILINEAR_FLAGS,
      bits_image_fetch_bilinear_no_repeat_8888,
      _pixman_image_get_scanline_generic_float
    },

#define GENERAL_BILINEAR_FLAGS						\
    (FAST_PATH_NO_ALPHA_MAP		|				\
     FAST_PATH_NO_ACCESSORS		|				\
     FAST_PATH_HAS_TRANSFORM		|				\
     FAST_PATH_AFFINE_TRANSFORM		|				\
     FAST_PATH_BILINEAR_FILTER)

#define GENERAL_NEAREST_FLAGS						\
    (FAST_PATH_NO_ALPHA_MAP		|				\
     FAST_PATH_NO_ACCESSORS		|				\
     FAST_PATH_HAS_TRANSFORM		|				\
     FAST_PATH_AFFINE_TRANSFORM		|				\
     FAST_PATH_NEAREST_FILTER)

#define GENERAL_SEPARABLE_CONVOLUTION_FLAGS				\
    (FAST_PATH_NO_ALPHA_MAP            |				\
     FAST_PATH_NO_ACCESSORS            |				\
     FAST_PATH_HAS_TRANSFORM           |				\
     FAST_PATH_AFFINE_TRANSFORM        |				\
     FAST_PATH_SEPARABLE_CONVOLUTION_FILTER)
    
#define SEPARABLE_CONVOLUTION_AFFINE_FAST_PATH(name, format, repeat)   \
    { PIXMAN_ ## format,                                               \
      GENERAL_SEPARABLE_CONVOLUTION_FLAGS | FAST_PATH_ ## repeat ## _REPEAT, \
      bits_image_fetch_separable_convolution_affine_ ## name,          \
      _pixman_image_get_scanline_generic_float			       \
    },

#define BILINEAR_AFFINE_FAST_PATH(name, format, repeat)			\
    { PIXMAN_ ## format,						\
      GENERAL_BILINEAR_FLAGS | FAST_PATH_ ## repeat ## _REPEAT,		\
      bits_image_fetch_bilinear_affine_ ## name,			\
      _pixman_image_get_scanline_generic_float				\
    },

#define NEAREST_AFFINE_FAST_PATH(name, format, repeat)			\
    { PIXMAN_ ## format,						\
      GENERAL_NEAREST_FLAGS | FAST_PATH_ ## repeat ## _REPEAT,		\
      bits_image_fetch_nearest_affine_ ## name,				\
      _pixman_image_get_scanline_generic_float				\
    },

#define AFFINE_FAST_PATHS(name, format, repeat)				\
    SEPARABLE_CONVOLUTION_AFFINE_FAST_PATH(name, format, repeat)	\
    BILINEAR_AFFINE_FAST_PATH(name, format, repeat)			\
    NEAREST_AFFINE_FAST_PATH(name, format, repeat)
    
    AFFINE_FAST_PATHS (pad_a8r8g8b8, a8r8g8b8, PAD)
    AFFINE_FAST_PATHS (none_a8r8g8b8, a8r8g8b8, NONE)
    AFFINE_FAST_PATHS (reflect_a8r8g8b8, a8r8g8b8, REFLECT)
    AFFINE_FAST_PATHS (normal_a8r8g8b8, a8r8g8b8, NORMAL)
    AFFINE_FAST_PATHS (pad_x8r8g8b8, x8r8g8b8, PAD)
    AFFINE_FAST_PATHS (none_x8r8g8b8, x8r8g8b8, NONE)
    AFFINE_FAST_PATHS (reflect_x8r8g8b8, x8r8g8b8, REFLECT)
    AFFINE_FAST_PATHS (normal_x8r8g8b8, x8r8g8b8, NORMAL)
    AFFINE_FAST_PATHS (pad_a8, a8, PAD)
    AFFINE_FAST_PATHS (none_a8, a8, NONE)
    AFFINE_FAST_PATHS (reflect_a8, a8, REFLECT)
    AFFINE_FAST_PATHS (normal_a8, a8, NORMAL)
    AFFINE_FAST_PATHS (pad_r5g6b5, r5g6b5, PAD)
    AFFINE_FAST_PATHS (none_r5g6b5, r5g6b5, NONE)
    AFFINE_FAST_PATHS (reflect_r5g6b5, r5g6b5, REFLECT)
    AFFINE_FAST_PATHS (normal_r5g6b5, r5g6b5, NORMAL)

    /* Affine, no alpha */
    { PIXMAN_any,
      (FAST_PATH_NO_ALPHA_MAP | FAST_PATH_HAS_TRANSFORM | FAST_PATH_AFFINE_TRANSFORM),
      bits_image_fetch_affine_no_alpha,
      _pixman_image_get_scanline_generic_float
    },

    /* General */
    { PIXMAN_any,
      0,
      bits_image_fetch_general,
      _pixman_image_get_scanline_generic_float
    },

    { PIXMAN_null },
};

static void
bits_image_property_changed (pixman_image_t *image)
{
    _pixman_bits_image_setup_accessors (&image->bits);
}

void
_pixman_bits_image_src_iter_init (pixman_image_t *image, pixman_iter_t *iter)
{
    pixman_format_code_t format = image->common.extended_format_code;
    uint32_t flags = image->common.flags;
    const fetcher_info_t *info;

    for (info = fetcher_info; info->format != PIXMAN_null; ++info)
    {
	if ((info->format == format || info->format == PIXMAN_any)	&&
	    (info->flags & flags) == info->flags)
	{
	    if (iter->iter_flags & ITER_NARROW)
	    {
		iter->get_scanline = info->get_scanline_32;
	    }
	    else
	    {
		iter->data = info->get_scanline_32;
		iter->get_scanline = info->get_scanline_float;
	    }
	    return;
	}
    }

    /* Just in case we somehow didn't find a scanline function */
    iter->get_scanline = _pixman_iter_get_scanline_noop;
}

static uint32_t *
dest_get_scanline_16 (pixman_iter_t *iter, const uint32_t *mask)
{
    pixman_image_t *image  = iter->image;
    int             x      = iter->x;
    int             y      = iter->y;
    int             width  = iter->width;
    uint32_t *	    buffer = iter->buffer;

    image->bits.fetch_scanline_16 (image, x, y, width, buffer, mask);

    return iter->buffer;
}

static uint32_t *
dest_get_scanline_narrow (pixman_iter_t *iter, const uint32_t *mask)
{
    pixman_image_t *image  = iter->image;
    int             x      = iter->x;
    int             y      = iter->y;
    int             width  = iter->width;
    uint32_t *	    buffer = iter->buffer;

    image->bits.fetch_scanline_32 (image, x, y, width, buffer, mask);
    if (image->common.alpha_map)
    {
	uint32_t *alpha;

	if ((alpha = malloc (width * sizeof (uint32_t))))
	{
	    int i;

	    x -= image->common.alpha_origin_x;
	    y -= image->common.alpha_origin_y;

	    image->common.alpha_map->fetch_scanline_32 (
		(pixman_image_t *)image->common.alpha_map,
		x, y, width, alpha, mask);

	    for (i = 0; i < width; ++i)
	    {
		buffer[i] &= ~0xff000000;
		buffer[i] |= (alpha[i] & 0xff000000);
	    }

	    free (alpha);
	}
    }

    return iter->buffer;
}

static uint32_t *
dest_get_scanline_wide (pixman_iter_t *iter, const uint32_t *mask)
{
    bits_image_t *  image  = &iter->image->bits;
    int             x      = iter->x;
    int             y      = iter->y;
    int             width  = iter->width;
    argb_t *	    buffer = (argb_t *)iter->buffer;

    image->fetch_scanline_float (
	(pixman_image_t *)image, x, y, width, (uint32_t *)buffer, mask);
    if (image->common.alpha_map)
    {
	argb_t *alpha;

	if ((alpha = malloc (width * sizeof (argb_t))))
	{
	    int i;

	    x -= image->common.alpha_origin_x;
	    y -= image->common.alpha_origin_y;

	    image->common.alpha_map->fetch_scanline_float (
		(pixman_image_t *)image->common.alpha_map,
		x, y, width, (uint32_t *)alpha, mask);

	    for (i = 0; i < width; ++i)
		buffer[i].a = alpha[i].a;

	    free (alpha);
	}
    }

    return iter->buffer;
}

static void
dest_write_back_16 (pixman_iter_t *iter)
{
    bits_image_t *  image  = &iter->image->bits;
    int             x      = iter->x;
    int             y      = iter->y;
    int             width  = iter->width;
    const uint32_t *buffer = iter->buffer;

    image->store_scanline_16 (image, x, y, width, buffer);

    iter->y++;
}

static void
dest_write_back_narrow (pixman_iter_t *iter)
{
    bits_image_t *  image  = &iter->image->bits;
    int             x      = iter->x;
    int             y      = iter->y;
    int             width  = iter->width;
    const uint32_t *buffer = iter->buffer;

    image->store_scanline_32 (image, x, y, width, buffer);

    if (image->common.alpha_map)
    {
	x -= image->common.alpha_origin_x;
	y -= image->common.alpha_origin_y;

	image->common.alpha_map->store_scanline_32 (
	    image->common.alpha_map, x, y, width, buffer);
    }

    iter->y++;
}

static void
dest_write_back_wide (pixman_iter_t *iter)
{
    bits_image_t *  image  = &iter->image->bits;
    int             x      = iter->x;
    int             y      = iter->y;
    int             width  = iter->width;
    const uint32_t *buffer = iter->buffer;

    image->store_scanline_float (image, x, y, width, buffer);

    if (image->common.alpha_map)
    {
	x -= image->common.alpha_origin_x;
	y -= image->common.alpha_origin_y;

	image->common.alpha_map->store_scanline_float (
	    image->common.alpha_map, x, y, width, buffer);
    }

    iter->y++;
}

void
_pixman_bits_image_dest_iter_init (pixman_image_t *image, pixman_iter_t *iter)
{
    if (iter->iter_flags & ITER_16)
    {
        if ((iter->iter_flags & (ITER_IGNORE_RGB | ITER_IGNORE_ALPHA)) ==
	    (ITER_IGNORE_RGB | ITER_IGNORE_ALPHA))
	{
            iter->get_scanline = _pixman_iter_get_scanline_noop;
        }
        else
        {
	    iter->get_scanline = dest_get_scanline_16;
        }
	iter->write_back = dest_write_back_16;
    }
    else if (iter->iter_flags & ITER_NARROW)
    {
	if ((iter->iter_flags & (ITER_IGNORE_RGB | ITER_IGNORE_ALPHA)) ==
	    (ITER_IGNORE_RGB | ITER_IGNORE_ALPHA))
	{
	    iter->get_scanline = _pixman_iter_get_scanline_noop;
	}
	else
	{
	    iter->get_scanline = dest_get_scanline_narrow;
	}

	iter->write_back = dest_write_back_narrow;
    }
    else
    {
	iter->get_scanline = dest_get_scanline_wide;
	iter->write_back = dest_write_back_wide;
    }
}

static uint32_t *
create_bits (pixman_format_code_t format,
             int                  width,
             int                  height,
             int *		  rowstride_bytes,
	     pixman_bool_t	  clear)
{
    int stride;
    size_t buf_size;
    int bpp;

    /* what follows is a long-winded way, avoiding any possibility of integer
     * overflows, of saying:
     * stride = ((width * bpp + 0x1f) >> 5) * sizeof (uint32_t);
     */

    bpp = PIXMAN_FORMAT_BPP (format);
    if (_pixman_multiply_overflows_int (width, bpp))
	return NULL;

    stride = width * bpp;
    if (_pixman_addition_overflows_int (stride, 0x1f))
	return NULL;

    stride += 0x1f;
    stride >>= 5;

    stride *= sizeof (uint32_t);

    if (_pixman_multiply_overflows_size (height, stride))
	return NULL;

    buf_size = height * stride;

    if (rowstride_bytes)
	*rowstride_bytes = stride;

    if (clear)
	return calloc (buf_size, 1);
    else
	return malloc (buf_size);
}

pixman_bool_t
_pixman_bits_image_init (pixman_image_t *     image,
                         pixman_format_code_t format,
                         int                  width,
                         int                  height,
                         uint32_t *           bits,
                         int                  rowstride,
			 pixman_bool_t	      clear)
{
    uint32_t *free_me = NULL;

    if (!bits && width && height)
    {
	int rowstride_bytes;

	free_me = bits = create_bits (format, width, height, &rowstride_bytes, clear);

	if (!bits)
	    return FALSE;

	rowstride = rowstride_bytes / (int) sizeof (uint32_t);
    }

    _pixman_image_init (image);

    image->type = BITS;
    image->bits.format = format;
    image->bits.width = width;
    image->bits.height = height;
    image->bits.bits = bits;
    image->bits.free_me = free_me;
    image->bits.read_func = NULL;
    image->bits.write_func = NULL;
    image->bits.rowstride = rowstride;
    image->bits.indexed = NULL;

    image->common.property_changed = bits_image_property_changed;

    _pixman_image_reset_clip_region (image);

    return TRUE;
}

static pixman_image_t *
create_bits_image_internal (pixman_format_code_t format,
			    int                  width,
			    int                  height,
			    uint32_t *           bits,
			    int                  rowstride_bytes,
			    pixman_bool_t	 clear)
{
    pixman_image_t *image;

    /* must be a whole number of uint32_t's
     */
    return_val_if_fail (
	bits == NULL || (rowstride_bytes % sizeof (uint32_t)) == 0, NULL);

    return_val_if_fail (PIXMAN_FORMAT_BPP (format) >= PIXMAN_FORMAT_DEPTH (format), NULL);

    image = _pixman_image_allocate ();

    if (!image)
	return NULL;

    if (!_pixman_bits_image_init (image, format, width, height, bits,
				  rowstride_bytes / (int) sizeof (uint32_t),
				  clear))
    {
	free (image);
	return NULL;
    }

    return image;
}

/* If bits is NULL, a buffer will be allocated and initialized to 0 */
PIXMAN_EXPORT pixman_image_t *
pixman_image_create_bits (pixman_format_code_t format,
                          int                  width,
                          int                  height,
                          uint32_t *           bits,
                          int                  rowstride_bytes)
{
    return create_bits_image_internal (
	format, width, height, bits, rowstride_bytes, TRUE);
}


/* If bits is NULL, a buffer will be allocated and _not_ initialized */
PIXMAN_EXPORT pixman_image_t *
pixman_image_create_bits_no_clear (pixman_format_code_t format,
				   int                  width,
				   int                  height,
				   uint32_t *           bits,
				   int                  rowstride_bytes)
{
    return create_bits_image_internal (
	format, width, height, bits, rowstride_bytes, FALSE);
}
