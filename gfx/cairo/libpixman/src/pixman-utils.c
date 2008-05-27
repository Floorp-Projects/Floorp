/*
 * Copyright © 2000 SuSE, Inc.
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

#include <stdlib.h>

#include "pixman-private.h"
#include "pixman-mmx.h"

pixman_bool_t
pixman_transform_point_3d (pixman_transform_t *transform,
			   pixman_vector_t *vector)
{
    pixman_vector_t		result;
    int				i, j;
    pixman_fixed_32_32_t	partial;
    pixman_fixed_48_16_t	v;

    for (j = 0; j < 3; j++)
    {
	v = 0;
	for (i = 0; i < 3; i++)
	{
	    partial = ((pixman_fixed_48_16_t) transform->matrix[j][i] *
		       (pixman_fixed_48_16_t) vector->vector[i]);
	    v += partial >> 16;
	}

	if (v > pixman_max_fixed_48_16 || v < pixman_min_fixed_48_16)
	    return FALSE;

	result.vector[j] = (pixman_fixed_48_16_t) v;
    }

    if (!result.vector[2])
	return FALSE;

    *vector = result;
    return TRUE;
}

pixman_bool_t
pixman_blt (uint32_t *src_bits,
	    uint32_t *dst_bits,
	    int src_stride,
	    int dst_stride,
	    int src_bpp,
	    int dst_bpp,
	    int src_x, int src_y,
	    int dst_x, int dst_y,
	    int width, int height)
{
#ifdef USE_MMX
    if (pixman_have_mmx())
    {
	return pixman_blt_mmx (src_bits, dst_bits, src_stride, dst_stride, src_bpp, dst_bpp,
			       src_x, src_y, dst_x, dst_y, width, height);
    }
    else
#endif
	return FALSE;
}

static void
pixman_fill8 (uint32_t  *bits,
	      int	stride,
	      int	x,
	      int	y,
	      int	width,
	      int	height,
	      uint32_t  xor)
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
	       uint32_t  xor)
{
    int short_stride = (stride * (int) sizeof (uint32_t)) / (int) sizeof (uint16_t);
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

pixman_bool_t
pixman_fill (uint32_t *bits,
	     int stride,
	     int bpp,
	     int x,
	     int y,
	     int width,
	     int height,
	     uint32_t xor)
{
#if 0
    printf ("filling: %d %d %d %d (stride: %d, bpp: %d)   pixel: %x\n",
	    x, y, width, height, stride, bpp, xor);
#endif

#ifdef USE_MMX
    if (!pixman_have_mmx() || !pixman_fill_mmx (bits, stride, bpp, x, y, width, height, xor))
#endif
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
	    return FALSE;
	    break;
	}
    }

    return TRUE;
}


/*
 * Compute the smallest value no less than y which is on a
 * grid row
 */

pixman_fixed_t
pixman_sample_ceil_y (pixman_fixed_t y, int n)
{
    pixman_fixed_t   f = pixman_fixed_frac(y);
    pixman_fixed_t   i = pixman_fixed_floor(y);

    f = ((f + Y_FRAC_FIRST(n)) / STEP_Y_SMALL(n)) * STEP_Y_SMALL(n) + Y_FRAC_FIRST(n);
    if (f > Y_FRAC_LAST(n))
    {
	f = Y_FRAC_FIRST(n);
	i += pixman_fixed_1;
    }
    return (i | f);
}

#define _div(a,b)    ((a) >= 0 ? (a) / (b) : -((-(a) + (b) - 1) / (b)))

/*
 * Compute the largest value no greater than y which is on a
 * grid row
 */
pixman_fixed_t
pixman_sample_floor_y (pixman_fixed_t y, int n)
{
    pixman_fixed_t   f = pixman_fixed_frac(y);
    pixman_fixed_t   i = pixman_fixed_floor (y);

    f = _div(f - Y_FRAC_FIRST(n), STEP_Y_SMALL(n)) * STEP_Y_SMALL(n) + Y_FRAC_FIRST(n);
    if (f < Y_FRAC_FIRST(n))
    {
	f = Y_FRAC_LAST(n);
	i -= pixman_fixed_1;
    }
    return (i | f);
}

/*
 * Step an edge by any amount (including negative values)
 */
void
pixman_edge_step (pixman_edge_t *e, int n)
{
    pixman_fixed_48_16_t	ne;

    e->x += n * e->stepx;

    ne = e->e + n * (pixman_fixed_48_16_t) e->dx;

    if (n >= 0)
    {
	if (ne > 0)
	{
	    int nx = (ne + e->dy - 1) / e->dy;
	    e->e = ne - nx * (pixman_fixed_48_16_t) e->dy;
	    e->x += nx * e->signdx;
	}
    }
    else
    {
	if (ne <= -e->dy)
	{
	    int nx = (-ne) / e->dy;
	    e->e = ne + nx * (pixman_fixed_48_16_t) e->dy;
	    e->x -= nx * e->signdx;
	}
    }
}

/*
 * A private routine to initialize the multi-step
 * elements of an edge structure
 */
static void
_pixman_edge_tMultiInit (pixman_edge_t *e, int n, pixman_fixed_t *stepx_p, pixman_fixed_t *dx_p)
{
    pixman_fixed_t	stepx;
    pixman_fixed_48_16_t	ne;

    ne = n * (pixman_fixed_48_16_t) e->dx;
    stepx = n * e->stepx;
    if (ne > 0)
    {
	int nx = ne / e->dy;
	ne -= nx * e->dy;
	stepx += nx * e->signdx;
    }
    *dx_p = ne;
    *stepx_p = stepx;
}

/*
 * Initialize one edge structure given the line endpoints and a
 * starting y value
 */
void
pixman_edge_init (pixman_edge_t	*e,
		  int		n,
		  pixman_fixed_t		y_start,
		  pixman_fixed_t		x_top,
		  pixman_fixed_t		y_top,
		  pixman_fixed_t		x_bot,
		  pixman_fixed_t		y_bot)
{
    pixman_fixed_t	dx, dy;

    e->x = x_top;
    e->e = 0;
    dx = x_bot - x_top;
    dy = y_bot - y_top;
    e->dy = dy;
    e->dx = 0;
    if (dy)
    {
	if (dx >= 0)
	{
	    e->signdx = 1;
	    e->stepx = dx / dy;
	    e->dx = dx % dy;
	    e->e = -dy;
	}
	else
	{
	    e->signdx = -1;
	    e->stepx = -(-dx / dy);
	    e->dx = -dx % dy;
	    e->e = 0;
	}

	_pixman_edge_tMultiInit (e, STEP_Y_SMALL(n), &e->stepx_small, &e->dx_small);
	_pixman_edge_tMultiInit (e, STEP_Y_BIG(n), &e->stepx_big, &e->dx_big);
    }
    pixman_edge_step (e, y_start - y_top);
}

/*
 * Initialize one edge structure given a line, starting y value
 * and a pixel offset for the line
 */
void
pixman_line_fixed_edge_init (pixman_edge_t *e,
			     int	    n,
			     pixman_fixed_t	    y,
			     const pixman_line_fixed_t *line,
			     int	    x_off,
			     int	    y_off)
{
    pixman_fixed_t	x_off_fixed = pixman_int_to_fixed(x_off);
    pixman_fixed_t	y_off_fixed = pixman_int_to_fixed(y_off);
    const pixman_point_fixed_t *top, *bot;

    if (line->p1.y <= line->p2.y)
    {
	top = &line->p1;
	bot = &line->p2;
    }
    else
    {
	top = &line->p2;
	bot = &line->p1;
    }
    pixman_edge_init (e, n, y,
		    top->x + x_off_fixed,
		    top->y + y_off_fixed,
		    bot->x + x_off_fixed,
		    bot->y + y_off_fixed);
}

pixman_bool_t
pixman_multiply_overflows_int (unsigned int a,
		               unsigned int b)
{
    return a >= INT32_MAX / b;
}

pixman_bool_t
pixman_addition_overflows_int (unsigned int a,
		               unsigned int b)
{
    return a > INT32_MAX - b;
}

void *
pixman_malloc_ab(unsigned int a,
		 unsigned int b)
{
    if (a >= INT32_MAX / b)
	return NULL;

    return malloc (a * b);
}

void *
pixman_malloc_abc (unsigned int a,
		   unsigned int b,
		   unsigned int c)
{
    if (a >= INT32_MAX / b)
	return NULL;
    else if (a * b >= INT32_MAX / c)
	return NULL;
    else
	return malloc (a * b * c);
}


/**
 * pixman_version:
 *
 * Returns the version of the pixman library encoded in a single
 * integer as per %PIXMAN_VERSION_ENCODE. The encoding ensures that
 * later versions compare greater than earlier versions.
 *
 * A run-time comparison to check that pixman's version is greater than
 * or equal to version X.Y.Z could be performed as follows:
 *
 * <informalexample><programlisting>
 * if (pixman_version() >= PIXMAN_VERSION_ENCODE(X,Y,Z)) {...}
 * </programlisting></informalexample>
 *
 * See also pixman_version_string() as well as the compile-time
 * equivalents %PIXMAN_VERSION and %PIXMAN_VERSION_STRING.
 *
 * Return value: the encoded version.
 **/
int
pixman_version (void)
{
    return PIXMAN_VERSION;
}

/**
 * pixman_version_string:
 *
 * Returns the version of the pixman library as a human-readable string
 * of the form "X.Y.Z".
 *
 * See also pixman_version() as well as the compile-time equivalents
 * %PIXMAN_VERSION_STRING and %PIXMAN_VERSION.
 *
 * Return value: a string containing the version.
 **/
const char*
pixman_version_string (void)
{
    return PIXMAN_VERSION_STRING;
}

/**
 * pixman_format_supported_destination:
 * @format: A pixman_format_code_t format
 * 
 * Return value: whether the provided format code is a supported
 * format for a pixman surface used as a destination in
 * rendering.
 *
 * Currently, all pixman_format_code_t values are supported
 * except for the YUV formats.
 **/
pixman_bool_t
pixman_format_supported_destination (pixman_format_code_t format)
{
    switch (format) {
    /* 32 bpp formats */
    case PIXMAN_a8r8g8b8:
    case PIXMAN_x8r8g8b8:
    case PIXMAN_a8b8g8r8:
    case PIXMAN_x8b8g8r8:
    case PIXMAN_r8g8b8:
    case PIXMAN_b8g8r8:
    case PIXMAN_r5g6b5:
    case PIXMAN_b5g6r5:
    /* 16 bpp formats */
    case PIXMAN_a1r5g5b5:
    case PIXMAN_x1r5g5b5:
    case PIXMAN_a1b5g5r5:
    case PIXMAN_x1b5g5r5:
    case PIXMAN_a4r4g4b4:
    case PIXMAN_x4r4g4b4:
    case PIXMAN_a4b4g4r4:
    case PIXMAN_x4b4g4r4:
    /* 8bpp formats */
    case PIXMAN_a8:
    case PIXMAN_r3g3b2:
    case PIXMAN_b2g3r3:
    case PIXMAN_a2r2g2b2:
    case PIXMAN_a2b2g2r2:
    case PIXMAN_c8:
    case PIXMAN_g8:
    case PIXMAN_x4a4:
    /* Collides with PIXMAN_c8
    case PIXMAN_x4c4:
    */
    /* Collides with PIXMAN_g8
    case PIXMAN_x4g4:
    */
    /* 4bpp formats */
    case PIXMAN_a4:
    case PIXMAN_r1g2b1:
    case PIXMAN_b1g2r1:
    case PIXMAN_a1r1g1b1:
    case PIXMAN_a1b1g1r1:
    case PIXMAN_c4:
    case PIXMAN_g4:
    /* 1bpp formats */
    case PIXMAN_a1:
    case PIXMAN_g1:
	return TRUE;
	
    /* YUV formats */
    case PIXMAN_yuy2:
    case PIXMAN_yv12:
    default:
	return FALSE;
    }
}

/**
 * pixman_format_supported_source:
 * @format: A pixman_format_code_t format
 * 
 * Return value: whether the provided format code is a supported
 * format for a pixman surface used as a source in
 * rendering.
 *
 * Currently, all pixman_format_code_t values are supported.
 **/
pixman_bool_t
pixman_format_supported_source (pixman_format_code_t format)
{
    switch (format) {
    /* 32 bpp formats */
    case PIXMAN_a8r8g8b8:
    case PIXMAN_x8r8g8b8:
    case PIXMAN_a8b8g8r8:
    case PIXMAN_x8b8g8r8:
    case PIXMAN_r8g8b8:
    case PIXMAN_b8g8r8:
    case PIXMAN_r5g6b5:
    case PIXMAN_b5g6r5:
    /* 16 bpp formats */
    case PIXMAN_a1r5g5b5:
    case PIXMAN_x1r5g5b5:
    case PIXMAN_a1b5g5r5:
    case PIXMAN_x1b5g5r5:
    case PIXMAN_a4r4g4b4:
    case PIXMAN_x4r4g4b4:
    case PIXMAN_a4b4g4r4:
    case PIXMAN_x4b4g4r4:
    /* 8bpp formats */
    case PIXMAN_a8:
    case PIXMAN_r3g3b2:
    case PIXMAN_b2g3r3:
    case PIXMAN_a2r2g2b2:
    case PIXMAN_a2b2g2r2:
    case PIXMAN_c8:
    case PIXMAN_g8:
    case PIXMAN_x4a4:
    /* Collides with PIXMAN_c8
    case PIXMAN_x4c4:
    */
    /* Collides with PIXMAN_g8
    case PIXMAN_x4g4:
    */
    /* 4bpp formats */
    case PIXMAN_a4:
    case PIXMAN_r1g2b1:
    case PIXMAN_b1g2r1:
    case PIXMAN_a1r1g1b1:
    case PIXMAN_a1b1g1r1:
    case PIXMAN_c4:
    case PIXMAN_g4:
    /* 1bpp formats */
    case PIXMAN_a1:
    case PIXMAN_g1:
    /* YUV formats */
    case PIXMAN_yuy2:
    case PIXMAN_yv12:
	return TRUE;

    default:
	return FALSE;
    }
}
