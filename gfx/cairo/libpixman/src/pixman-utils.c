/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 1999 Keith Packard
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
#include <stdio.h>
#include <stdlib.h>

#include "pixman-private.h"

/*
 * Computing composite region
 */
static inline pixman_bool_t
clip_general_image (pixman_region32_t * region,
                    pixman_region32_t * clip,
                    int                 dx,
                    int                 dy)
{
    if (pixman_region32_n_rects (region) == 1 &&
        pixman_region32_n_rects (clip) == 1)
    {
	pixman_box32_t *  rbox = pixman_region32_rectangles (region, NULL);
	pixman_box32_t *  cbox = pixman_region32_rectangles (clip, NULL);
	int v;

	if (rbox->x1 < (v = cbox->x1 + dx))
	    rbox->x1 = v;
	if (rbox->x2 > (v = cbox->x2 + dx))
	    rbox->x2 = v;
	if (rbox->y1 < (v = cbox->y1 + dy))
	    rbox->y1 = v;
	if (rbox->y2 > (v = cbox->y2 + dy))
	    rbox->y2 = v;
	if (rbox->x1 >= rbox->x2 || rbox->y1 >= rbox->y2)
	{
	    pixman_region32_init (region);
	    return FALSE;
	}
    }
    else if (!pixman_region32_not_empty (clip))
    {
	return FALSE;
    }
    else
    {
	if (dx || dy)
	    pixman_region32_translate (region, -dx, -dy);

	if (!pixman_region32_intersect (region, region, clip))
	    return FALSE;

	if (dx || dy)
	    pixman_region32_translate (region, dx, dy);
    }

    return pixman_region32_not_empty (region);
}

static inline pixman_bool_t
clip_source_image (pixman_region32_t * region,
                   pixman_image_t *    image,
                   int                 dx,
                   int                 dy)
{
    /* Source clips are ignored, unless they are explicitly turned on
     * and the clip in question was set by an X client. (Because if
     * the clip was not set by a client, then it is a hierarchy
     * clip and those should always be ignored for sources).
     */
    if (!image->common.clip_sources || !image->common.client_clip)
	return TRUE;

    return clip_general_image (region,
                               &image->common.clip_region,
                               dx, dy);
}

/*
 * returns FALSE if the final region is empty.  Indistinguishable from
 * an allocation failure, but rendering ignores those anyways.
 */
static pixman_bool_t
pixman_compute_composite_region32 (pixman_region32_t * region,
                                   pixman_image_t *    src_image,
                                   pixman_image_t *    mask_image,
                                   pixman_image_t *    dst_image,
                                   int32_t             src_x,
                                   int32_t             src_y,
                                   int32_t             mask_x,
                                   int32_t             mask_y,
                                   int32_t             dest_x,
                                   int32_t             dest_y,
                                   int32_t             width,
                                   int32_t             height)
{
    region->extents.x1 = dest_x;
    region->extents.x2 = dest_x + width;
    region->extents.y1 = dest_y;
    region->extents.y2 = dest_y + height;

    region->extents.x1 = MAX (region->extents.x1, 0);
    region->extents.y1 = MAX (region->extents.y1, 0);
    region->extents.x2 = MIN (region->extents.x2, dst_image->bits.width);
    region->extents.y2 = MIN (region->extents.y2, dst_image->bits.height);

    region->data = 0;

    /* Check for empty operation */
    if (region->extents.x1 >= region->extents.x2 ||
        region->extents.y1 >= region->extents.y2)
    {
	pixman_region32_init (region);
	return FALSE;
    }

    if (dst_image->common.have_clip_region)
    {
	if (!clip_general_image (region, &dst_image->common.clip_region, 0, 0))
	{
	    pixman_region32_fini (region);
	    return FALSE;
	}
    }

    if (dst_image->common.alpha_map && dst_image->common.alpha_map->common.have_clip_region)
    {
	if (!clip_general_image (region, &dst_image->common.alpha_map->common.clip_region,
	                         -dst_image->common.alpha_origin_x,
	                         -dst_image->common.alpha_origin_y))
	{
	    pixman_region32_fini (region);
	    return FALSE;
	}
    }

    /* clip against src */
    if (src_image->common.have_clip_region)
    {
	if (!clip_source_image (region, src_image, dest_x - src_x, dest_y - src_y))
	{
	    pixman_region32_fini (region);
	    return FALSE;
	}
    }
    if (src_image->common.alpha_map && src_image->common.alpha_map->common.have_clip_region)
    {
	if (!clip_source_image (region, (pixman_image_t *)src_image->common.alpha_map,
	                        dest_x - (src_x - src_image->common.alpha_origin_x),
	                        dest_y - (src_y - src_image->common.alpha_origin_y)))
	{
	    pixman_region32_fini (region);
	    return FALSE;
	}
    }
    /* clip against mask */
    if (mask_image && mask_image->common.have_clip_region)
    {
	if (!clip_source_image (region, mask_image, dest_x - mask_x, dest_y - mask_y))
	{
	    pixman_region32_fini (region);
	    return FALSE;
	}
	if (mask_image->common.alpha_map && mask_image->common.alpha_map->common.have_clip_region)
	{
	    if (!clip_source_image (region, (pixman_image_t *)mask_image->common.alpha_map,
	                            dest_x - (mask_x - mask_image->common.alpha_origin_x),
	                            dest_y - (mask_y - mask_image->common.alpha_origin_y)))
	    {
		pixman_region32_fini (region);
		return FALSE;
	    }
	}
    }

    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_compute_composite_region (pixman_region16_t * region,
                                 pixman_image_t *    src_image,
                                 pixman_image_t *    mask_image,
                                 pixman_image_t *    dst_image,
                                 int16_t             src_x,
                                 int16_t             src_y,
                                 int16_t             mask_x,
                                 int16_t             mask_y,
                                 int16_t             dest_x,
                                 int16_t             dest_y,
                                 uint16_t            width,
                                 uint16_t            height)
{
    pixman_region32_t r32;
    pixman_bool_t retval;

    pixman_region32_init (&r32);

    retval = pixman_compute_composite_region32 (
	&r32, src_image, mask_image, dst_image,
	src_x, src_y, mask_x, mask_y, dest_x, dest_y,
	width, height);

    if (retval)
    {
	if (!pixman_region16_copy_from_region32 (region, &r32))
	    retval = FALSE;
    }

    pixman_region32_fini (&r32);
    return retval;
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
pixman_malloc_ab (unsigned int a,
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

/*
 * Helper routine to expand a color component from 0 < n <= 8 bits to 16
 * bits by replication.
 */
static inline uint64_t
expand16 (const uint8_t val, int nbits)
{
    /* Start out with the high bit of val in the high bit of result. */
    uint16_t result = (uint16_t)val << (16 - nbits);

    if (nbits == 0)
	return 0;

    /* Copy the bits in result, doubling the number of bits each time, until
     * we fill all 16 bits.
     */
    while (nbits < 16)
    {
	result |= result >> nbits;
	nbits *= 2;
    }

    return result;
}

/*
 * This function expands images from ARGB8 format to ARGB16.  To preserve
 * precision, it needs to know the original source format.  For example, if the
 * source was PIXMAN_x1r5g5b5 and the red component contained bits 12345, then
 * the expanded value is 12345123.  To correctly expand this to 16 bits, it
 * should be 1234512345123451 and not 1234512312345123.
 */
void
pixman_expand (uint64_t *           dst,
               const uint32_t *     src,
               pixman_format_code_t format,
               int                  width)
{
    /*
     * Determine the sizes of each component and the masks and shifts
     * required to extract them from the source pixel.
     */
    const int a_size = PIXMAN_FORMAT_A (format),
              r_size = PIXMAN_FORMAT_R (format),
              g_size = PIXMAN_FORMAT_G (format),
              b_size = PIXMAN_FORMAT_B (format);
    const int a_shift = 32 - a_size,
              r_shift = 24 - r_size,
              g_shift = 16 - g_size,
              b_shift =  8 - b_size;
    const uint8_t a_mask = ~(~0 << a_size),
                  r_mask = ~(~0 << r_size),
                  g_mask = ~(~0 << g_size),
                  b_mask = ~(~0 << b_size);
    int i;

    /* Start at the end so that we can do the expansion in place
     * when src == dst
     */
    for (i = width - 1; i >= 0; i--)
    {
	const uint32_t pixel = src[i];
	const uint8_t a = (pixel >> a_shift) & a_mask,
	              r = (pixel >> r_shift) & r_mask,
	              g = (pixel >> g_shift) & g_mask,
	              b = (pixel >> b_shift) & b_mask;
	const uint64_t a16 = a_size ? expand16 (a, a_size) : 0xffff,
	               r16 = expand16 (r, r_size),
	               g16 = expand16 (g, g_size),
	               b16 = expand16 (b, b_size);

	dst[i] = a16 << 48 | r16 << 32 | g16 << 16 | b16;
    }
}

/*
 * Contracting is easier than expanding.  We just need to truncate the
 * components.
 */
void
pixman_contract (uint32_t *      dst,
                 const uint64_t *src,
                 int             width)
{
    int i;

    /* Start at the beginning so that we can do the contraction in
     * place when src == dst
     */
    for (i = 0; i < width; i++)
    {
	const uint8_t a = src[i] >> 56,
	              r = src[i] >> 40,
	              g = src[i] >> 24,
	              b = src[i] >> 8;

	dst[i] = a << 24 | r << 16 | g << 8 | b;
    }
}

static void
walk_region_internal (pixman_implementation_t *imp,
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
                      int32_t                  height,
                      pixman_bool_t            src_repeat,
                      pixman_bool_t            mask_repeat,
                      pixman_region32_t *      region,
                      pixman_composite_func_t  composite_rect)
{
    int n;
    const pixman_box32_t *pbox;
    int w, h, w_this, h_this;
    int x_msk, y_msk, x_src, y_src, x_dst, y_dst;

    pbox = pixman_region32_rectangles (region, &n);
    while (n--)
    {
	h = pbox->y2 - pbox->y1;
	y_src = pbox->y1 - dest_y + src_y;
	y_msk = pbox->y1 - dest_y + mask_y;
	y_dst = pbox->y1;

	while (h)
	{
	    h_this = h;
	    w = pbox->x2 - pbox->x1;
	    x_src = pbox->x1 - dest_x + src_x;
	    x_msk = pbox->x1 - dest_x + mask_x;
	    x_dst = pbox->x1;

	    if (mask_repeat)
	    {
		y_msk = MOD (y_msk, mask_image->bits.height);
		if (h_this > mask_image->bits.height - y_msk)
		    h_this = mask_image->bits.height - y_msk;
	    }

	    if (src_repeat)
	    {
		y_src = MOD (y_src, src_image->bits.height);
		if (h_this > src_image->bits.height - y_src)
		    h_this = src_image->bits.height - y_src;
	    }

	    while (w)
	    {
		w_this = w;

		if (mask_repeat)
		{
		    x_msk = MOD (x_msk, mask_image->bits.width);
		    if (w_this > mask_image->bits.width - x_msk)
			w_this = mask_image->bits.width - x_msk;
		}

		if (src_repeat)
		{
		    x_src = MOD (x_src, src_image->bits.width);
		    if (w_this > src_image->bits.width - x_src)
			w_this = src_image->bits.width - x_src;
		}

		(*composite_rect) (imp, op,
				   src_image, mask_image, dst_image,
				   x_src, y_src, x_msk, y_msk, x_dst, y_dst,
				   w_this, h_this);
		w -= w_this;

		x_src += w_this;
		x_msk += w_this;
		x_dst += w_this;
	    }

	    h -= h_this;
	    y_src += h_this;
	    y_msk += h_this;
	    y_dst += h_this;
	}

	pbox++;
    }
}

void
_pixman_walk_composite_region (pixman_implementation_t *imp,
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
                               int32_t                  height,
                               pixman_composite_func_t  composite_rect)
{
    pixman_region32_t region;

    pixman_region32_init (&region);

    if (pixman_compute_composite_region32 (
            &region, src_image, mask_image, dst_image,
            src_x, src_y, mask_x, mask_y, dest_x, dest_y,
            width, height))
    {
	walk_region_internal (imp, op,
	                      src_image, mask_image, dst_image,
	                      src_x, src_y, mask_x, mask_y, dest_x, dest_y,
	                      width, height, FALSE, FALSE,
	                      &region,
	                      composite_rect);

	pixman_region32_fini (&region);
    }
}

static const pixman_fast_path_t *
get_fast_path (const pixman_fast_path_t *fast_paths,
               pixman_op_t               op,
               pixman_image_t *          src_image,
               pixman_image_t *          mask_image,
               pixman_image_t *          dst_image,
	       int			 src_x,
	       int			 src_y,
	       int			 mask_x,
	       int			 mask_y)
{
    pixman_format_code_t src_format, mask_format, dest_format;
    const pixman_fast_path_t *info;

    /* Check for pixbufs */
    if (mask_image && mask_image->type == BITS								&&
	(mask_image->bits.format == PIXMAN_a8r8g8b8 || mask_image->bits.format == PIXMAN_a8b8g8r8)	&&
	(src_image->type == BITS && src_image->bits.bits == mask_image->bits.bits)			&&
	(src_image->common.repeat == mask_image->common.repeat)						&&
	(src_x == mask_x && src_y == mask_y))
    {
	if (src_image->bits.format == PIXMAN_x8b8g8r8)
	    src_format = mask_format = PIXMAN_pixbuf;
	else if (src_image->bits.format == PIXMAN_x8r8g8b8)
	    src_format = mask_format = PIXMAN_rpixbuf;
	else
	    return NULL;
    }
    else
    {
	/* Source */
	if (_pixman_image_is_solid (src_image))
	{
	    src_format = PIXMAN_solid;
	}
	else if (src_image->type == BITS)
	{
	    src_format = src_image->bits.format;
	}
	else
	{
	    return NULL;
	}

	/* Mask */
	if (!mask_image)
	{
	    mask_format = PIXMAN_null;
	}
	else if (mask_image->common.component_alpha)
	{
	    if (mask_image->type == BITS)
	    {
		/* These are the *only* component_alpha formats
		 * we support for fast paths
		 */
		if (mask_image->bits.format == PIXMAN_a8r8g8b8)
		    mask_format = PIXMAN_a8r8g8b8_ca;
		else if (mask_image->bits.format == PIXMAN_a8b8g8r8)
		    mask_format = PIXMAN_a8b8g8r8_ca;
		else
		    return NULL;
	    }
	    else
	    {
		return NULL;
	    }
	}
	else if (_pixman_image_is_solid (mask_image))
	{
	    mask_format = PIXMAN_solid;
	}
	else if (mask_image->common.type == BITS)
	{
	    mask_format = mask_image->bits.format;
	}
	else
	{
	    return NULL;
	}
    }

    dest_format = dst_image->bits.format;
    
    for (info = fast_paths; info->op != PIXMAN_OP_NONE; ++info)
    {
	if (info->op == op			&&
	    info->src_format == src_format	&&
	    info->mask_format == mask_format	&&
	    info->dest_format == dest_format)
	{
	    return info;
	}
    }

    return NULL;
}

static force_inline pixman_bool_t
image_covers (pixman_image_t *image,
              pixman_box32_t *extents,
              int             x,
              int             y)
{
    if (image->common.type == BITS &&
	image->common.repeat == PIXMAN_REPEAT_NONE)
    {
	if (x > extents->x1 || y > extents->y1 ||
	    x + image->bits.width < extents->x2 ||
	    y + image->bits.height < extents->y2)
	{
	    return FALSE;
	}
    }

    return TRUE;
}

static force_inline pixman_bool_t
sources_cover (pixman_image_t *src,
	       pixman_image_t *mask,
	       pixman_box32_t *extents,
	       int             src_x,
	       int             src_y,
	       int             mask_x,
	       int             mask_y,
	       int             dest_x,
	       int             dest_y)
{
    if (!image_covers (src, extents, dest_x - src_x, dest_y - src_y))
	return FALSE;

    if (!mask)
	return TRUE;

    if (!image_covers (mask, extents, dest_x - mask_x, dest_y - mask_y))
	return FALSE;

    return TRUE;
}

pixman_bool_t
_pixman_run_fast_path (const pixman_fast_path_t *paths,
                       pixman_implementation_t * imp,
                       pixman_op_t               op,
                       pixman_image_t *          src,
                       pixman_image_t *          mask,
                       pixman_image_t *          dest,
                       int32_t                   src_x,
                       int32_t                   src_y,
                       int32_t                   mask_x,
                       int32_t                   mask_y,
                       int32_t                   dest_x,
                       int32_t                   dest_y,
                       int32_t                   width,
                       int32_t                   height)
{
    pixman_composite_func_t func = NULL;
    pixman_bool_t src_repeat =
	src->common.repeat == PIXMAN_REPEAT_NORMAL;
    pixman_bool_t mask_repeat =
	mask && mask->common.repeat == PIXMAN_REPEAT_NORMAL;
    pixman_bool_t result;
    pixman_bool_t has_fast_path;

    has_fast_path = !dest->common.alpha_map &&
		    !dest->bits.read_func &&
		    !dest->bits.write_func;

    if (has_fast_path)
    {
	has_fast_path = !src->common.transform &&
	                !src->common.alpha_map &&
			src->common.filter != PIXMAN_FILTER_CONVOLUTION &&
			src->common.repeat != PIXMAN_REPEAT_PAD &&
			src->common.repeat != PIXMAN_REPEAT_REFLECT;
	if (has_fast_path && src->type == BITS)
	{
	    has_fast_path = !src->bits.read_func &&
	                    !src->bits.write_func &&
		            !PIXMAN_FORMAT_IS_WIDE (src->bits.format);
	}
    }

    if (mask && has_fast_path)
    {
	has_fast_path =
	    !mask->common.transform &&
	    !mask->common.alpha_map &&
	    !mask->bits.read_func &&
	    !mask->bits.write_func &&
	    mask->common.filter != PIXMAN_FILTER_CONVOLUTION &&
	    mask->common.repeat != PIXMAN_REPEAT_PAD &&
	    mask->common.repeat != PIXMAN_REPEAT_REFLECT &&
	    !PIXMAN_FORMAT_IS_WIDE (mask->bits.format);
    }

    if (has_fast_path)
    {
	const pixman_fast_path_t *info;

	if ((info = get_fast_path (paths, op, src, mask, dest, src_x, src_y, mask_x, mask_y)))
	{
	    func = info->func;

	    if (info->src_format == PIXMAN_solid)
		src_repeat = FALSE;

	    if (info->mask_format == PIXMAN_solid)
		mask_repeat = FALSE;

	    if ((src_repeat                     &&
		 src->bits.width == 1           &&
		 src->bits.height == 1)		||
		(mask_repeat			&&
		 mask->bits.width == 1		&&
		 mask->bits.height == 1))
	    {
		/* If src or mask are repeating 1x1 images and src_repeat or
		 * mask_repeat are still TRUE, it means the fast path we
		 * selected does not actually handle repeating images.
		 *
		 * So rather than calling the "fast path" with a zillion
		 * 1x1 requests, we just fall back to the general code (which
		 * does do something sensible with 1x1 repeating images).
		 */
		func = NULL;
	    }
	}
    }

    result = FALSE;

    if (func)
    {
	pixman_region32_t region;
	pixman_region32_init (&region);

	if (pixman_compute_composite_region32 (
	        &region, src, mask, dest,
	        src_x, src_y, mask_x, mask_y, dest_x, dest_y, width, height))
	{
	    pixman_box32_t *extents = pixman_region32_extents (&region);

	    if (sources_cover (
		    src, mask, extents,
		    src_x, src_y, mask_x, mask_y, dest_x, dest_y))
	    {
		walk_region_internal (imp, op,
		                      src, mask, dest,
		                      src_x, src_y, mask_x, mask_y,
		                      dest_x, dest_y,
		                      width, height,
		                      src_repeat, mask_repeat,
		                      &region,
		                      func);

		result = TRUE;
	    }

	    pixman_region32_fini (&region);
	}
    }

    return result;
}

#define N_TMP_BOXES (16)

pixman_bool_t
pixman_region16_copy_from_region32 (pixman_region16_t *dst,
                                    pixman_region32_t *src)
{
    int n_boxes, i;
    pixman_box32_t *boxes32;
    pixman_box16_t *boxes16;
    pixman_bool_t retval;

    boxes32 = pixman_region32_rectangles (src, &n_boxes);

    boxes16 = pixman_malloc_ab (n_boxes, sizeof (pixman_box16_t));

    if (!boxes16)
	return FALSE;

    for (i = 0; i < n_boxes; ++i)
    {
	boxes16[i].x1 = boxes32[i].x1;
	boxes16[i].y1 = boxes32[i].y1;
	boxes16[i].x2 = boxes32[i].x2;
	boxes16[i].y2 = boxes32[i].y2;
    }

    pixman_region_fini (dst);
    retval = pixman_region_init_rects (dst, boxes16, n_boxes);
    free (boxes16);
    return retval;
}

pixman_bool_t
pixman_region32_copy_from_region16 (pixman_region32_t *dst,
                                    pixman_region16_t *src)
{
    int n_boxes, i;
    pixman_box16_t *boxes16;
    pixman_box32_t *boxes32;
    pixman_box32_t tmp_boxes[N_TMP_BOXES];
    pixman_bool_t retval;

    boxes16 = pixman_region_rectangles (src, &n_boxes);

    if (n_boxes > N_TMP_BOXES)
	boxes32 = pixman_malloc_ab (n_boxes, sizeof (pixman_box32_t));
    else
	boxes32 = tmp_boxes;

    if (!boxes32)
	return FALSE;

    for (i = 0; i < n_boxes; ++i)
    {
	boxes32[i].x1 = boxes16[i].x1;
	boxes32[i].y1 = boxes16[i].y1;
	boxes32[i].x2 = boxes16[i].x2;
	boxes32[i].y2 = boxes16[i].y2;
    }

    pixman_region32_fini (dst);
    retval = pixman_region32_init_rects (dst, boxes32, n_boxes);

    if (boxes32 != tmp_boxes)
	free (boxes32);

    return retval;
}
