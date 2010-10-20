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

#include <stdlib.h>

static force_inline pixman_implementation_t *
get_implementation (void)
{
    static pixman_implementation_t *global_implementation;

    if (!global_implementation)
	global_implementation = _pixman_choose_implementation ();

    return global_implementation;
}

typedef struct operator_info_t operator_info_t;

struct operator_info_t
{
    uint8_t	opaque_info[4];
};

#define PACK(neither, src, dest, both)			\
    {{	    (uint8_t)PIXMAN_OP_ ## neither,		\
	    (uint8_t)PIXMAN_OP_ ## src,			\
	    (uint8_t)PIXMAN_OP_ ## dest,		\
	    (uint8_t)PIXMAN_OP_ ## both		}}

static const operator_info_t operator_table[] =
{
    /*    Neither Opaque         Src Opaque             Dst Opaque             Both Opaque */
    PACK (CLEAR,                 CLEAR,                 CLEAR,                 CLEAR),
    PACK (SRC,                   SRC,                   SRC,                   SRC),
    PACK (DST,                   DST,                   DST,                   DST),
    PACK (OVER,                  SRC,                   OVER,                  SRC),
    PACK (OVER_REVERSE,          OVER_REVERSE,          DST,                   DST),
    PACK (IN,                    IN,                    SRC,                   SRC),
    PACK (IN_REVERSE,            DST,                   IN_REVERSE,            DST),
    PACK (OUT,                   OUT,                   CLEAR,                 CLEAR),
    PACK (OUT_REVERSE,           CLEAR,                 OUT_REVERSE,           CLEAR),
    PACK (ATOP,                  IN,                    OVER,                  SRC),
    PACK (ATOP_REVERSE,          OVER_REVERSE,          IN_REVERSE,            DST),
    PACK (XOR,                   OUT,                   OUT_REVERSE,           CLEAR),
    PACK (ADD,                   ADD,                   ADD,                   ADD),
    PACK (SATURATE,              OVER_REVERSE,          DST,                   DST),

    {{ 0 /* 0x0e */ }},
    {{ 0 /* 0x0f */ }},

    PACK (CLEAR,                 CLEAR,                 CLEAR,                 CLEAR),
    PACK (SRC,                   SRC,                   SRC,                   SRC),
    PACK (DST,                   DST,                   DST,                   DST),
    PACK (DISJOINT_OVER,         DISJOINT_OVER,         DISJOINT_OVER,         DISJOINT_OVER),
    PACK (DISJOINT_OVER_REVERSE, DISJOINT_OVER_REVERSE, DISJOINT_OVER_REVERSE, DISJOINT_OVER_REVERSE),
    PACK (DISJOINT_IN,           DISJOINT_IN,           DISJOINT_IN,           DISJOINT_IN),
    PACK (DISJOINT_IN_REVERSE,   DISJOINT_IN_REVERSE,   DISJOINT_IN_REVERSE,   DISJOINT_IN_REVERSE),
    PACK (DISJOINT_OUT,          DISJOINT_OUT,          DISJOINT_OUT,          DISJOINT_OUT),
    PACK (DISJOINT_OUT_REVERSE,  DISJOINT_OUT_REVERSE,  DISJOINT_OUT_REVERSE,  DISJOINT_OUT_REVERSE),
    PACK (DISJOINT_ATOP,         DISJOINT_ATOP,         DISJOINT_ATOP,         DISJOINT_ATOP),
    PACK (DISJOINT_ATOP_REVERSE, DISJOINT_ATOP_REVERSE, DISJOINT_ATOP_REVERSE, DISJOINT_ATOP_REVERSE),
    PACK (DISJOINT_XOR,          DISJOINT_XOR,          DISJOINT_XOR,          DISJOINT_XOR),

    {{ 0 /* 0x1c */ }},
    {{ 0 /* 0x1d */ }},
    {{ 0 /* 0x1e */ }},
    {{ 0 /* 0x1f */ }},

    PACK (CLEAR,                 CLEAR,                 CLEAR,                 CLEAR),
    PACK (SRC,                   SRC,                   SRC,                   SRC),
    PACK (DST,                   DST,                   DST,                   DST),
    PACK (CONJOINT_OVER,         CONJOINT_OVER,         CONJOINT_OVER,         CONJOINT_OVER),
    PACK (CONJOINT_OVER_REVERSE, CONJOINT_OVER_REVERSE, CONJOINT_OVER_REVERSE, CONJOINT_OVER_REVERSE),
    PACK (CONJOINT_IN,           CONJOINT_IN,           CONJOINT_IN,           CONJOINT_IN),
    PACK (CONJOINT_IN_REVERSE,   CONJOINT_IN_REVERSE,   CONJOINT_IN_REVERSE,   CONJOINT_IN_REVERSE),
    PACK (CONJOINT_OUT,          CONJOINT_OUT,          CONJOINT_OUT,          CONJOINT_OUT),
    PACK (CONJOINT_OUT_REVERSE,  CONJOINT_OUT_REVERSE,  CONJOINT_OUT_REVERSE,  CONJOINT_OUT_REVERSE),
    PACK (CONJOINT_ATOP,         CONJOINT_ATOP,         CONJOINT_ATOP,         CONJOINT_ATOP),
    PACK (CONJOINT_ATOP_REVERSE, CONJOINT_ATOP_REVERSE, CONJOINT_ATOP_REVERSE, CONJOINT_ATOP_REVERSE),
    PACK (CONJOINT_XOR,          CONJOINT_XOR,          CONJOINT_XOR,          CONJOINT_XOR),

    {{ 0 /* 0x2c */ }},
    {{ 0 /* 0x2d */ }},
    {{ 0 /* 0x2e */ }},
    {{ 0 /* 0x2f */ }},

    PACK (MULTIPLY,              MULTIPLY,              MULTIPLY,              MULTIPLY),
    PACK (SCREEN,                SCREEN,                SCREEN,                SCREEN),
    PACK (OVERLAY,               OVERLAY,               OVERLAY,               OVERLAY),
    PACK (DARKEN,                DARKEN,                DARKEN,                DARKEN),
    PACK (LIGHTEN,               LIGHTEN,               LIGHTEN,               LIGHTEN),
    PACK (COLOR_DODGE,           COLOR_DODGE,           COLOR_DODGE,           COLOR_DODGE),
    PACK (COLOR_BURN,            COLOR_BURN,            COLOR_BURN,            COLOR_BURN),
    PACK (HARD_LIGHT,            HARD_LIGHT,            HARD_LIGHT,            HARD_LIGHT),
    PACK (SOFT_LIGHT,            SOFT_LIGHT,            SOFT_LIGHT,            SOFT_LIGHT),
    PACK (DIFFERENCE,            DIFFERENCE,            DIFFERENCE,            DIFFERENCE),
    PACK (EXCLUSION,             EXCLUSION,             EXCLUSION,             EXCLUSION),
    PACK (HSL_HUE,               HSL_HUE,               HSL_HUE,               HSL_HUE),
    PACK (HSL_SATURATION,        HSL_SATURATION,        HSL_SATURATION,        HSL_SATURATION),
    PACK (HSL_COLOR,             HSL_COLOR,             HSL_COLOR,             HSL_COLOR),
    PACK (HSL_LUMINOSITY,        HSL_LUMINOSITY,        HSL_LUMINOSITY,        HSL_LUMINOSITY),
};

/*
 * Optimize the current operator based on opacity of source or destination
 * The output operator should be mathematically equivalent to the source.
 */
static pixman_op_t
optimize_operator (pixman_op_t     op,
		   uint32_t        src_flags,
		   uint32_t        mask_flags,
		   uint32_t        dst_flags)
{
    pixman_bool_t is_source_opaque, is_dest_opaque;

#define OPAQUE_SHIFT 13
    
    COMPILE_TIME_ASSERT (FAST_PATH_IS_OPAQUE == (1 << OPAQUE_SHIFT));
    
    is_dest_opaque = (dst_flags & FAST_PATH_IS_OPAQUE);
    is_source_opaque = ((src_flags & mask_flags) & FAST_PATH_IS_OPAQUE);

    is_dest_opaque >>= OPAQUE_SHIFT - 1;
    is_source_opaque >>= OPAQUE_SHIFT;

    return operator_table[op].opaque_info[is_dest_opaque | is_source_opaque];
}

static void
apply_workaround (pixman_image_t *image,
		  int32_t *       x,
		  int32_t *       y,
		  uint32_t **     save_bits,
		  int *           save_dx,
		  int *           save_dy)
{
    if (image && (image->common.flags & FAST_PATH_NEEDS_WORKAROUND))
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
}

static void
unapply_workaround (pixman_image_t *image, uint32_t *bits, int dx, int dy)
{
    if (image && (image->common.flags & FAST_PATH_NEEDS_WORKAROUND))
    {
	image->bits.bits = bits;
	pixman_region32_translate (&image->common.clip_region, dx, dy);
    }
}

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
	region->extents.x1 = 0;
	region->extents.x2 = 0;
	region->extents.y1 = 0;
	region->extents.y2 = 0;
	return FALSE;
    }

    if (dst_image->common.have_clip_region)
    {
	if (!clip_general_image (region, &dst_image->common.clip_region, 0, 0))
	    return FALSE;
    }

    if (dst_image->common.alpha_map)
    {
	if (!pixman_region32_intersect_rect (region, region,
					     dst_image->common.alpha_origin_x,
					     dst_image->common.alpha_origin_y,
					     dst_image->common.alpha_map->width,
					     dst_image->common.alpha_map->height))
	{
	    return FALSE;
	}
	if (!pixman_region32_not_empty (region))
	    return FALSE;
	if (dst_image->common.alpha_map->common.have_clip_region)
	{
	    if (!clip_general_image (region, &dst_image->common.alpha_map->common.clip_region,
				     -dst_image->common.alpha_origin_x,
				     -dst_image->common.alpha_origin_y))
	    {
		return FALSE;
	    }
	}
    }

    /* clip against src */
    if (src_image->common.have_clip_region)
    {
	if (!clip_source_image (region, src_image, dest_x - src_x, dest_y - src_y))
	    return FALSE;
    }
    if (src_image->common.alpha_map && src_image->common.alpha_map->common.have_clip_region)
    {
	if (!clip_source_image (region, (pixman_image_t *)src_image->common.alpha_map,
	                        dest_x - (src_x - src_image->common.alpha_origin_x),
	                        dest_y - (src_y - src_image->common.alpha_origin_y)))
	{
	    return FALSE;
	}
    }
    /* clip against mask */
    if (mask_image && mask_image->common.have_clip_region)
    {
	if (!clip_source_image (region, mask_image, dest_x - mask_x, dest_y - mask_y))
	    return FALSE;

	if (mask_image->common.alpha_map && mask_image->common.alpha_map->common.have_clip_region)
	{
	    if (!clip_source_image (region, (pixman_image_t *)mask_image->common.alpha_map,
	                            dest_x - (mask_x - mask_image->common.alpha_origin_x),
	                            dest_y - (mask_y - mask_image->common.alpha_origin_y)))
	    {
		return FALSE;
	    }
	}
    }

    return TRUE;
}

#define N_CACHED_FAST_PATHS 8

typedef struct
{
    struct
    {
	pixman_implementation_t *	imp;
	pixman_fast_path_t		fast_path;
    } cache [N_CACHED_FAST_PATHS];
} cache_t;

PIXMAN_DEFINE_THREAD_LOCAL (cache_t, fast_path_cache);

static force_inline pixman_bool_t
lookup_composite_function (pixman_op_t			op,
			   pixman_format_code_t		src_format,
			   uint32_t			src_flags,
			   pixman_format_code_t		mask_format,
			   uint32_t			mask_flags,
			   pixman_format_code_t		dest_format,
			   uint32_t			dest_flags,
			   pixman_implementation_t    **out_imp,
			   pixman_composite_func_t     *out_func)
{
    pixman_implementation_t *imp;
    cache_t *cache;
    int i;

    /* Check cache for fast paths */
    cache = PIXMAN_GET_THREAD_LOCAL (fast_path_cache);

    for (i = 0; i < N_CACHED_FAST_PATHS; ++i)
    {
	const pixman_fast_path_t *info = &(cache->cache[i].fast_path);

	/* Note that we check for equality here, not whether
	 * the cached fast path matches. This is to prevent
	 * us from selecting an overly general fast path
	 * when a more specific one would work.
	 */
	if (info->op == op			&&
	    info->src_format == src_format	&&
	    info->mask_format == mask_format	&&
	    info->dest_format == dest_format	&&
	    info->src_flags == src_flags	&&
	    info->mask_flags == mask_flags	&&
	    info->dest_flags == dest_flags	&&
	    info->func)
	{
	    *out_imp = cache->cache[i].imp;
	    *out_func = cache->cache[i].fast_path.func;

	    goto update_cache;
	}
    }

    for (imp = get_implementation (); imp != NULL; imp = imp->delegate)
    {
	const pixman_fast_path_t *info = imp->fast_paths;

	while (info->op != PIXMAN_OP_NONE)
	{
	    if ((info->op == op || info->op == PIXMAN_OP_any)		&&
		/* Formats */
		((info->src_format == src_format) ||
		 (info->src_format == PIXMAN_any))			&&
		((info->mask_format == mask_format) ||
		 (info->mask_format == PIXMAN_any))			&&
		((info->dest_format == dest_format) ||
		 (info->dest_format == PIXMAN_any))			&&
		/* Flags */
		(info->src_flags & src_flags) == info->src_flags	&&
		(info->mask_flags & mask_flags) == info->mask_flags	&&
		(info->dest_flags & dest_flags) == info->dest_flags)
	    {
		*out_imp = imp;
		*out_func = info->func;

		/* Set i to the last spot in the cache so that the
		 * move-to-front code below will work
		 */
		i = N_CACHED_FAST_PATHS - 1;

		goto update_cache;
	    }

	    ++info;
	}
    }
    return FALSE;

update_cache:
    if (i)
    {
	while (i--)
	    cache->cache[i + 1] = cache->cache[i];

	cache->cache[0].imp = *out_imp;
	cache->cache[0].fast_path.op = op;
	cache->cache[0].fast_path.src_format = src_format;
	cache->cache[0].fast_path.src_flags = src_flags;
	cache->cache[0].fast_path.mask_format = mask_format;
	cache->cache[0].fast_path.mask_flags = mask_flags;
	cache->cache[0].fast_path.dest_format = dest_format;
	cache->cache[0].fast_path.dest_flags = dest_flags;
	cache->cache[0].fast_path.func = *out_func;
    }

    return TRUE;
}

static pixman_bool_t
compute_sample_extents (pixman_transform_t *transform,
			pixman_box32_t *extents, int x, int y, 
			pixman_fixed_t x_off, pixman_fixed_t y_off,
			pixman_fixed_t width, pixman_fixed_t height)
{
    pixman_fixed_t x1, y1, x2, y2;
    pixman_fixed_48_16_t tx1, ty1, tx2, ty2;

    /* We have checked earlier that (extents->x1 - x) etc. fit in a pixman_fixed_t */
    x1 = (pixman_fixed_48_16_t)pixman_int_to_fixed (extents->x1 - x) + pixman_fixed_1 / 2;
    y1 = (pixman_fixed_48_16_t)pixman_int_to_fixed (extents->y1 - y) + pixman_fixed_1 / 2;
    x2 = (pixman_fixed_48_16_t)pixman_int_to_fixed (extents->x2 - x) - pixman_fixed_1 / 2;
    y2 = (pixman_fixed_48_16_t)pixman_int_to_fixed (extents->y2 - y) - pixman_fixed_1 / 2;

    if (!transform)
    {
	tx1 = (pixman_fixed_48_16_t)x1;
	ty1 = (pixman_fixed_48_16_t)y1;
	tx2 = (pixman_fixed_48_16_t)x2;
	ty2 = (pixman_fixed_48_16_t)y2;
    }
    else
    {
	int i;

	/* Silence GCC */
	tx1 = ty1 = tx2 = ty2 = 0;
    
	for (i = 0; i < 4; ++i)
	{
	    pixman_fixed_48_16_t tx, ty;
	    pixman_vector_t v;

	    v.vector[0] = (i & 0x01)? x1 : x2;
	    v.vector[1] = (i & 0x02)? y1 : y2;
	    v.vector[2] = pixman_fixed_1;

	    if (!pixman_transform_point (transform, &v))
		return FALSE;

	    tx = (pixman_fixed_48_16_t)v.vector[0];
	    ty = (pixman_fixed_48_16_t)v.vector[1];

	    if (i == 0)
	    {
		tx1 = tx;
		ty1 = ty;
		tx2 = tx;
		ty2 = ty;
	    }
	    else
	    {
		if (tx < tx1)
		    tx1 = tx;
		if (ty < ty1)
		    ty1 = ty;
		if (tx > tx2)
		    tx2 = tx;
		if (ty > ty2)
		    ty2 = ty;
	    }
	}
    }

    /* Expand the source area by a tiny bit so account of different rounding that
     * may happen during sampling. Note that (8 * pixman_fixed_e) is very far from
     * 0.5 so this won't cause the area computed to be overly pessimistic.
     */
    tx1 += x_off - 8 * pixman_fixed_e;
    ty1 += y_off - 8 * pixman_fixed_e;
    tx2 += x_off + width + 8 * pixman_fixed_e;
    ty2 += y_off + height + 8 * pixman_fixed_e;

    if (tx1 < pixman_min_fixed_48_16 || tx1 > pixman_max_fixed_48_16 ||
	ty1 < pixman_min_fixed_48_16 || ty1 > pixman_max_fixed_48_16 ||
	tx2 < pixman_min_fixed_48_16 || tx2 > pixman_max_fixed_48_16 ||
	ty2 < pixman_min_fixed_48_16 || ty2 > pixman_max_fixed_48_16)
    {
	return FALSE;
    }
    else
    {
	extents->x1 = pixman_fixed_to_int (tx1);
	extents->y1 = pixman_fixed_to_int (ty1);
	extents->x2 = pixman_fixed_to_int (tx2) + 1;
	extents->y2 = pixman_fixed_to_int (ty2) + 1;

	return TRUE;
    }
}

#define IS_16BIT(x) (((x) >= INT16_MIN) && ((x) <= INT16_MAX))

static pixman_bool_t
analyze_extent (pixman_image_t *image, int x, int y,
		const pixman_box32_t *extents, uint32_t *flags)
{
    pixman_transform_t *transform;
    pixman_fixed_t *params;
    pixman_fixed_t x_off, y_off;
    pixman_fixed_t width, height;
    pixman_box32_t ex;

    if (!image)
	return TRUE;

    /* Some compositing functions walk one step
     * outside the destination rectangle, so we
     * check here that the expanded-by-one source
     * extents in destination space fits in 16 bits
     */
    if (!IS_16BIT (extents->x1 - x - 1)		||
	!IS_16BIT (extents->y1 - y - 1)		||
	!IS_16BIT (extents->x2 - x + 1)		||
	!IS_16BIT (extents->y2 - y + 1))
    {
	return FALSE;
    }

    transform = image->common.transform;
    if (image->common.type == BITS)
    {
	/* During repeat mode calculations we might convert the
	 * width/height of an image to fixed 16.16, so we need
	 * them to be smaller than 16 bits.
	 */
	if (image->bits.width >= 0x7fff	|| image->bits.height >= 0x7fff)
	    return FALSE;

#define ID_AND_NEAREST (FAST_PATH_ID_TRANSFORM | FAST_PATH_NEAREST_FILTER)
	
	if ((image->common.flags & ID_AND_NEAREST) == ID_AND_NEAREST &&
	    extents->x1 - x >= 0 &&
	    extents->y1 - y >= 0 &&
	    extents->x2 - x <= image->bits.width &&
	    extents->y2 - y <= image->bits.height)
	{
	    *flags |= FAST_PATH_SAMPLES_COVER_CLIP;
	    return TRUE;
	}
    
	switch (image->common.filter)
	{
	case PIXMAN_FILTER_CONVOLUTION:
	    params = image->common.filter_params;
	    x_off = - pixman_fixed_e - ((params[0] - pixman_fixed_1) >> 1);
	    y_off = - pixman_fixed_e - ((params[1] - pixman_fixed_1) >> 1);
	    width = params[0];
	    height = params[1];
	    break;

	case PIXMAN_FILTER_GOOD:
	case PIXMAN_FILTER_BEST:
	case PIXMAN_FILTER_BILINEAR:
	    x_off = - pixman_fixed_1 / 2;
	    y_off = - pixman_fixed_1 / 2;
	    width = pixman_fixed_1;
	    height = pixman_fixed_1;
	    break;

	case PIXMAN_FILTER_FAST:
	case PIXMAN_FILTER_NEAREST:
	    x_off = - pixman_fixed_e;
	    y_off = - pixman_fixed_e;
	    width = 0;
	    height = 0;
	    break;

	default:
	    return FALSE;
	}

	/* Check whether the non-expanded, transformed extent is entirely within
	 * the source image, and set the FAST_PATH_SAMPLES_COVER_CLIP if it is.
	 */
	ex = *extents;
	if (compute_sample_extents (transform, &ex, x, y, x_off, y_off, width, height) &&
	    ex.x1 >= 0 && ex.y1 >= 0 &&
	    ex.x2 <= image->bits.width && ex.y2 <= image->bits.height)
	{
	    *flags |= FAST_PATH_SAMPLES_COVER_CLIP;
	}
    }
    else
    {
	x_off = 0;
	y_off = 0;
	width = 0;
	height = 0;
    }

    /* Check that the extents expanded by one don't overflow. This ensures that
     * compositing functions can simply walk the source space using 16.16
     * variables without worrying about overflow.
     */
    ex.x1 = extents->x1 - 1;
    ex.y1 = extents->y1 - 1;
    ex.x2 = extents->x2 + 1;
    ex.y2 = extents->y2 + 1;

    if (!compute_sample_extents (transform, &ex, x, y, x_off, y_off, width, height))
	return FALSE;

    return TRUE;
}

/*
 * Work around GCC bug causing crashes in Mozilla with SSE2
 *
 * When using -msse, gcc generates movdqa instructions assuming that
 * the stack is 16 byte aligned. Unfortunately some applications, such
 * as Mozilla and Mono, end up aligning the stack to 4 bytes, which
 * causes the movdqa instructions to fail.
 *
 * The __force_align_arg_pointer__ makes gcc generate a prologue that
 * realigns the stack pointer to 16 bytes.
 *
 * On x86-64 this is not necessary because the standard ABI already
 * calls for a 16 byte aligned stack.
 *
 * See https://bugs.freedesktop.org/show_bug.cgi?id=15693
 */
#if defined (USE_SSE2) && defined(__GNUC__) && !defined(__x86_64__) && !defined(__amd64__)
__attribute__((__force_align_arg_pointer__))
#endif
PIXMAN_EXPORT void
pixman_image_composite32 (pixman_op_t      op,
                          pixman_image_t * src,
                          pixman_image_t * mask,
                          pixman_image_t * dest,
                          int32_t          src_x,
                          int32_t          src_y,
                          int32_t          mask_x,
                          int32_t          mask_y,
                          int32_t          dest_x,
                          int32_t          dest_y,
                          int32_t          width,
                          int32_t          height)
{
    pixman_format_code_t src_format, mask_format, dest_format;
    uint32_t src_flags, mask_flags, dest_flags;
    pixman_region32_t region;
    pixman_box32_t *extents;
    uint32_t *src_bits;
    int src_dx, src_dy;
    uint32_t *mask_bits;
    int mask_dx, mask_dy;
    uint32_t *dest_bits;
    int dest_dx, dest_dy;
    pixman_bool_t need_workaround;
    pixman_implementation_t *imp;
    pixman_composite_func_t func;

    _pixman_image_validate (src);
    if (mask)
	_pixman_image_validate (mask);
    _pixman_image_validate (dest);

    src_format = src->common.extended_format_code;
    src_flags = src->common.flags;

    if (mask)
    {
	mask_format = mask->common.extended_format_code;
	mask_flags = mask->common.flags;
    }
    else
    {
	mask_format = PIXMAN_null;
	mask_flags = FAST_PATH_IS_OPAQUE;
    }

    dest_format = dest->common.extended_format_code;
    dest_flags = dest->common.flags;

    /* Check for pixbufs */
    if ((mask_format == PIXMAN_a8r8g8b8 || mask_format == PIXMAN_a8b8g8r8) &&
	(src->type == BITS && src->bits.bits == mask->bits.bits)	   &&
	(src->common.repeat == mask->common.repeat)			   &&
	(src_x == mask_x && src_y == mask_y))
    {
	if (src_format == PIXMAN_x8b8g8r8)
	    src_format = mask_format = PIXMAN_pixbuf;
	else if (src_format == PIXMAN_x8r8g8b8)
	    src_format = mask_format = PIXMAN_rpixbuf;
    }

    /* Check for workaround */
    need_workaround = (src_flags | mask_flags | dest_flags) & FAST_PATH_NEEDS_WORKAROUND;

    if (need_workaround)
    {
	apply_workaround (src, &src_x, &src_y, &src_bits, &src_dx, &src_dy);
	apply_workaround (mask, &mask_x, &mask_y, &mask_bits, &mask_dx, &mask_dy);
	apply_workaround (dest, &dest_x, &dest_y, &dest_bits, &dest_dx, &dest_dy);
    }

    pixman_region32_init (&region);

    if (!pixman_compute_composite_region32 (
	    &region, src, mask, dest,
	    src_x, src_y, mask_x, mask_y, dest_x, dest_y, width, height))
    {
	goto out;
    }

    extents = pixman_region32_extents (&region);

    if (!analyze_extent (src, dest_x - src_x, dest_y - src_y, extents, &src_flags))
	goto out;

    if (!analyze_extent (mask, dest_x - mask_x, dest_y - mask_y, extents, &mask_flags))
	goto out;

    /* If the clip is within the source samples, and the samples are opaque,
     * then the source is effectively opaque.
     */
#define BOTH (FAST_PATH_SAMPLES_OPAQUE | FAST_PATH_SAMPLES_COVER_CLIP)

    if ((src_flags & BOTH) == BOTH)
	src_flags |= FAST_PATH_IS_OPAQUE;
    
    if ((mask_flags & BOTH) == BOTH)
	mask_flags |= FAST_PATH_IS_OPAQUE;
    
    /*
     * Check if we can replace our operator by a simpler one
     * if the src or dest are opaque. The output operator should be
     * mathematically equivalent to the source.
     */
    op = optimize_operator (op, src_flags, mask_flags, dest_flags);
    if (op == PIXMAN_OP_DST)
	goto out;

    if (lookup_composite_function (op,
				   src_format, src_flags,
				   mask_format, mask_flags,
				   dest_format, dest_flags,
				   &imp, &func))
    {
	const pixman_box32_t *pbox;
	int n;

	pbox = pixman_region32_rectangles (&region, &n);
	
	while (n--)
	{
	    func (imp, op,
		  src, mask, dest,
		  pbox->x1 + src_x - dest_x,
		  pbox->y1 + src_y - dest_y,
		  pbox->x1 + mask_x - dest_x,
		  pbox->y1 + mask_y - dest_y,
		  pbox->x1,
		  pbox->y1,
		  pbox->x2 - pbox->x1,
		  pbox->y2 - pbox->y1);
	    
	    pbox++;
	}
    }

out:
    if (need_workaround)
    {
	unapply_workaround (src, src_bits, src_dx, src_dy);
	unapply_workaround (mask, mask_bits, mask_dx, mask_dy);
	unapply_workaround (dest, dest_bits, dest_dx, dest_dy);
    }

    pixman_region32_fini (&region);
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
    pixman_image_composite32 (op, src, mask, dest, src_x, src_y, 
                              mask_x, mask_y, dest_x, dest_y, width, height);
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
    return _pixman_implementation_blt (get_implementation(),
				       src_bits, dst_bits, src_stride, dst_stride,
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
    return _pixman_implementation_fill (
	get_implementation(), bits, stride, bpp, x, y, width, height, xor);
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
    pixman_box32_t stack_boxes[6];
    pixman_box32_t *boxes;
    pixman_bool_t result;
    int i;

    if (n_rects > 6)
    {
        boxes = pixman_malloc_ab (sizeof (pixman_box32_t), n_rects);
        if (boxes == NULL)
            return FALSE;
    }
    else
    {
        boxes = stack_boxes;
    }

    for (i = 0; i < n_rects; ++i)
    {
        boxes[i].x1 = rects[i].x;
        boxes[i].y1 = rects[i].y;
        boxes[i].x2 = boxes[i].x1 + rects[i].width;
        boxes[i].y2 = boxes[i].y1 + rects[i].height;
    }

    result = pixman_image_fill_boxes (op, dest, color, n_rects, boxes);

    if (boxes != stack_boxes)
        free (boxes);
    
    return result;
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_fill_boxes (pixman_op_t           op,
                         pixman_image_t *      dest,
                         pixman_color_t *      color,
                         int                   n_boxes,
                         const pixman_box32_t *boxes)
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
            pixman_region32_t fill_region;
            int n_rects, j;
            pixman_box32_t *rects;

            if (!pixman_region32_init_rects (&fill_region, boxes, n_boxes))
                return FALSE;

            if (dest->common.have_clip_region)
            {
                if (!pixman_region32_intersect (&fill_region,
                                                &fill_region,
                                                &dest->common.clip_region))
                    return FALSE;
            }

            rects = pixman_region32_rectangles (&fill_region, &n_rects);
            for (j = 0; j < n_rects; ++j)
            {
                const pixman_box32_t *rect = &(rects[j]);
                pixman_fill (dest->bits.bits, dest->bits.rowstride, PIXMAN_FORMAT_BPP (dest->bits.format),
                             rect->x1, rect->y1, rect->x2 - rect->x1, rect->y2 - rect->y1,
                             pixel);
            }

            pixman_region32_fini (&fill_region);
            return TRUE;
        }
    }

    solid = pixman_image_create_solid_fill (color);
    if (!solid)
        return FALSE;

    for (i = 0; i < n_boxes; ++i)
    {
        const pixman_box32_t *box = &(boxes[i]);

        pixman_image_composite32 (op, solid, NULL, dest,
                                  0, 0, 0, 0,
                                  box->x1, box->y1,
                                  box->x2 - box->x1, box->y2 - box->y1);
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
    case PIXMAN_x14r6g6b6:
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
