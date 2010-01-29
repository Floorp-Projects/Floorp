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
#include "pixman-private.h"

/*
 * Operator optimizations based on source or destination opacity
 */
typedef struct
{
    pixman_op_t op;
    pixman_op_t op_src_dst_opaque;
    pixman_op_t op_src_opaque;
    pixman_op_t op_dst_opaque;
} optimized_operator_info_t;

static const optimized_operator_info_t optimized_operators[] =
{
    /* Input Operator           SRC&DST Opaque          SRC Opaque              DST Opaque      */
    { PIXMAN_OP_OVER,           PIXMAN_OP_SRC,          PIXMAN_OP_SRC,          PIXMAN_OP_OVER },
    { PIXMAN_OP_OVER_REVERSE,   PIXMAN_OP_DST,          PIXMAN_OP_OVER_REVERSE, PIXMAN_OP_DST },
    { PIXMAN_OP_IN,             PIXMAN_OP_SRC,          PIXMAN_OP_IN,           PIXMAN_OP_SRC },
    { PIXMAN_OP_IN_REVERSE,     PIXMAN_OP_DST,          PIXMAN_OP_DST,          PIXMAN_OP_IN_REVERSE },
    { PIXMAN_OP_OUT,            PIXMAN_OP_CLEAR,        PIXMAN_OP_OUT,          PIXMAN_OP_CLEAR },
    { PIXMAN_OP_OUT_REVERSE,    PIXMAN_OP_CLEAR,        PIXMAN_OP_CLEAR,        PIXMAN_OP_OUT_REVERSE },
    { PIXMAN_OP_ATOP,           PIXMAN_OP_SRC,          PIXMAN_OP_IN,           PIXMAN_OP_OVER },
    { PIXMAN_OP_ATOP_REVERSE,   PIXMAN_OP_DST,          PIXMAN_OP_OVER_REVERSE, PIXMAN_OP_IN_REVERSE },
    { PIXMAN_OP_XOR,            PIXMAN_OP_CLEAR,        PIXMAN_OP_OUT,          PIXMAN_OP_OUT_REVERSE },
    { PIXMAN_OP_SATURATE,       PIXMAN_OP_DST,          PIXMAN_OP_OVER_REVERSE, PIXMAN_OP_DST },
    { PIXMAN_OP_NONE }
};

static pixman_implementation_t *imp;

/*
 * Check if the current operator could be optimized
 */
static const optimized_operator_info_t*
pixman_operator_can_be_optimized (pixman_op_t op)
{
    const optimized_operator_info_t *info;

    for (info = optimized_operators; info->op != PIXMAN_OP_NONE; info++)
    {
	if (info->op == op)
	    return info;
    }
    return NULL;
}

/*
 * Optimize the current operator based on opacity of source or destination
 * The output operator should be mathematically equivalent to the source.
 */
static pixman_op_t
pixman_optimize_operator (pixman_op_t     op,
                          pixman_image_t *src_image,
                          pixman_image_t *mask_image,
                          pixman_image_t *dst_image)
{
    pixman_bool_t is_source_opaque;
    pixman_bool_t is_dest_opaque;
    const optimized_operator_info_t *info = pixman_operator_can_be_optimized (op);

    if (!info || mask_image)
	return op;

    is_source_opaque = _pixman_image_is_opaque (src_image);
    is_dest_opaque = _pixman_image_is_opaque (dst_image);

    if (is_source_opaque == FALSE && is_dest_opaque == FALSE)
	return op;

    if (is_source_opaque && is_dest_opaque)
	return info->op_src_dst_opaque;
    else if (is_source_opaque)
	return info->op_src_opaque;
    else if (is_dest_opaque)
	return info->op_dst_opaque;

    return op;

}

static void
apply_workaround (pixman_image_t *image,
		  int16_t *       x,
		  int16_t *       y,
		  uint32_t **     save_bits,
		  int *           save_dx,
		  int *           save_dy)
{
    /* Some X servers generate images that point to the
     * wrong place in memory, but then set the clip region
     * to point to the right place. Because of an old bug
     * in pixman, this would actually work.
     *
     * Here we try and undo the damage
     */
    int bpp = PIXMAN_FORMAT_BPP (image->bits.format) / 8;
    pixman_box32_t *extents;
    uint8_t *t;
    int dx, dy;

    extents = pixman_region32_extents (&(image->common.clip_region));
    dx = extents->x1;
    dy = extents->y1;

    *save_bits = image->bits.bits;

    *x -= dx;
    *y -= dy;
    pixman_region32_translate (&(image->common.clip_region), -dx, -dy);

    t = (uint8_t *)image->bits.bits;
    t += dy * image->bits.rowstride * 4 + dx * bpp;
    image->bits.bits = (uint32_t *)t;

    *save_dx = dx;
    *save_dy = dy;
}

static void
unapply_workaround (pixman_image_t *image, uint32_t *bits, int dx, int dy)
{
    image->bits.bits = bits;
    pixman_region32_translate (&image->common.clip_region, dx, dy);
}

PIXMAN_EXPORT void
pixman_image_composite (pixman_op_t      op,
                        pixman_image_t * src,
                        pixman_image_t * mask,
                        pixman_image_t * dest,
                        int16_t          src_x,
                        int16_t          src_y,
                        int16_t          mask_x,
                        int16_t          mask_y,
                        int16_t          dest_x,
                        int16_t          dest_y,
                        uint16_t         width,
                        uint16_t         height)
{
    uint32_t *src_bits;
    int src_dx, src_dy;
    uint32_t *mask_bits;
    int mask_dx, mask_dy;
    uint32_t *dest_bits;
    int dest_dx, dest_dy;

    _pixman_image_validate (src);
    if (mask)
	_pixman_image_validate (mask);
    _pixman_image_validate (dest);
    
    /*
     * Check if we can replace our operator by a simpler one
     * if the src or dest are opaque. The output operator should be
     * mathematically equivalent to the source.
     */
    op = pixman_optimize_operator(op, src, mask, dest);
    if (op == PIXMAN_OP_DST		||
	op == PIXMAN_OP_CONJOINT_DST	||
	op == PIXMAN_OP_DISJOINT_DST)
    {
        return;
    }

    if (!imp)
	imp = _pixman_choose_implementation ();

    if (src->common.need_workaround)
	apply_workaround (src, &src_x, &src_y, &src_bits, &src_dx, &src_dy);
    if (mask && mask->common.need_workaround)
	apply_workaround (mask, &mask_x, &mask_y, &mask_bits, &mask_dx, &mask_dy);
    if (dest->common.need_workaround)
	apply_workaround (dest, &dest_x, &dest_y, &dest_bits, &dest_dx, &dest_dy);

    _pixman_implementation_composite (imp, op,
                                      src, mask, dest,
                                      src_x, src_y,
                                      mask_x, mask_y,
                                      dest_x, dest_y,
                                      width, height);

    if (src->common.need_workaround)
	unapply_workaround (src, src_bits, src_dx, src_dy);
    if (mask && mask->common.need_workaround)
	unapply_workaround (mask, mask_bits, mask_dx, mask_dy);
    if (dest->common.need_workaround)
	unapply_workaround (dest, dest_bits, dest_dx, dest_dy);
}

PIXMAN_EXPORT pixman_bool_t
pixman_blt (uint32_t *src_bits,
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
    if (!imp)
	imp = _pixman_choose_implementation ();

    return _pixman_implementation_blt (imp, src_bits, dst_bits, src_stride, dst_stride,
                                       src_bpp, dst_bpp,
                                       src_x, src_y,
                                       dst_x, dst_y,
                                       width, height);
}

PIXMAN_EXPORT pixman_bool_t
pixman_fill (uint32_t *bits,
             int       stride,
             int       bpp,
             int       x,
             int       y,
             int       width,
             int       height,
             uint32_t xor)
{
    if (!imp)
	imp = _pixman_choose_implementation ();

    return _pixman_implementation_fill (imp, bits, stride, bpp, x, y, width, height, xor);
}

static uint32_t
color_to_uint32 (const pixman_color_t *color)
{
    return
        (color->alpha >> 8 << 24) |
        (color->red >> 8 << 16) |
        (color->green & 0xff00) |
        (color->blue >> 8);
}

static pixman_bool_t
color_to_pixel (pixman_color_t *     color,
                uint32_t *           pixel,
                pixman_format_code_t format)
{
    uint32_t c = color_to_uint32 (color);

    if (!(format == PIXMAN_a8r8g8b8     ||
          format == PIXMAN_x8r8g8b8     ||
          format == PIXMAN_a8b8g8r8     ||
          format == PIXMAN_x8b8g8r8     ||
          format == PIXMAN_b8g8r8a8     ||
          format == PIXMAN_b8g8r8x8     ||
          format == PIXMAN_r5g6b5       ||
          format == PIXMAN_b5g6r5       ||
          format == PIXMAN_a8))
    {
	return FALSE;
    }

    if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_ABGR)
    {
	c = ((c & 0xff000000) >>  0) |
	    ((c & 0x00ff0000) >> 16) |
	    ((c & 0x0000ff00) >>  0) |
	    ((c & 0x000000ff) << 16);
    }
    if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_BGRA)
    {
	c = ((c & 0xff000000) >> 24) |
	    ((c & 0x00ff0000) >>  8) |
	    ((c & 0x0000ff00) <<  8) |
	    ((c & 0x000000ff) << 24);
    }

    if (format == PIXMAN_a8)
	c = c >> 24;
    else if (format == PIXMAN_r5g6b5 ||
             format == PIXMAN_b5g6r5)
	c = CONVERT_8888_TO_0565 (c);

#if 0
    printf ("color: %x %x %x %x\n", color->alpha, color->red, color->green, color->blue);
    printf ("pixel: %x\n", c);
#endif

    *pixel = c;
    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_fill_rectangles (pixman_op_t                 op,
                              pixman_image_t *            dest,
                              pixman_color_t *            color,
                              int                         n_rects,
                              const pixman_rectangle16_t *rects)
{
    pixman_image_t *solid;
    pixman_color_t c;
    int i;

    _pixman_image_validate (dest);
    
    if (color->alpha == 0xffff)
    {
	if (op == PIXMAN_OP_OVER)
	    op = PIXMAN_OP_SRC;
    }

    if (op == PIXMAN_OP_CLEAR)
    {
	c.red = 0;
	c.green = 0;
	c.blue = 0;
	c.alpha = 0;

	color = &c;

	op = PIXMAN_OP_SRC;
    }

    if (op == PIXMAN_OP_SRC)
    {
	uint32_t pixel;

	if (color_to_pixel (color, &pixel, dest->bits.format))
	{
	    for (i = 0; i < n_rects; ++i)
	    {
		pixman_region32_t fill_region;
		int n_boxes, j;
		pixman_box32_t *boxes;

		pixman_region32_init_rect (&fill_region, rects[i].x, rects[i].y, rects[i].width, rects[i].height);

		if (dest->common.have_clip_region)
		{
		    if (!pixman_region32_intersect (&fill_region,
		                                    &fill_region,
		                                    &dest->common.clip_region))
			return FALSE;
		}

		boxes = pixman_region32_rectangles (&fill_region, &n_boxes);
		for (j = 0; j < n_boxes; ++j)
		{
		    const pixman_box32_t *box = &(boxes[j]);
		    pixman_fill (dest->bits.bits, dest->bits.rowstride, PIXMAN_FORMAT_BPP (dest->bits.format),
		                 box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1,
		                 pixel);
		}

		pixman_region32_fini (&fill_region);
	    }
	    return TRUE;
	}
    }

    solid = pixman_image_create_solid_fill (color);
    if (!solid)
	return FALSE;

    for (i = 0; i < n_rects; ++i)
    {
	const pixman_rectangle16_t *rect = &(rects[i]);

	pixman_image_composite (op, solid, NULL, dest,
	                        0, 0, 0, 0,
	                        rect->x, rect->y,
	                        rect->width, rect->height);
    }

    pixman_image_unref (solid);

    return TRUE;
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
PIXMAN_EXPORT int
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
PIXMAN_EXPORT const char*
pixman_version_string (void)
{
    return PIXMAN_VERSION_STRING;
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
PIXMAN_EXPORT pixman_bool_t
pixman_format_supported_source (pixman_format_code_t format)
{
    switch (format)
    {
    /* 32 bpp formats */
    case PIXMAN_a2b10g10r10:
    case PIXMAN_x2b10g10r10:
    case PIXMAN_a2r10g10b10:
    case PIXMAN_x2r10g10b10:
    case PIXMAN_a8r8g8b8:
    case PIXMAN_x8r8g8b8:
    case PIXMAN_a8b8g8r8:
    case PIXMAN_x8b8g8r8:
    case PIXMAN_b8g8r8a8:
    case PIXMAN_b8g8r8x8:
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
PIXMAN_EXPORT pixman_bool_t
pixman_format_supported_destination (pixman_format_code_t format)
{
    /* YUV formats cannot be written to at the moment */
    if (format == PIXMAN_yuy2 || format == PIXMAN_yv12)
	return FALSE;

    return pixman_format_supported_source (format);
}

