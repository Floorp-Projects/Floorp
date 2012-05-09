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
#include <limits.h>

#include "pixman-private.h"

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

pixman_bool_t
_pixman_lookup_composite_function (pixman_implementation_t     *toplevel,
				   pixman_op_t			op,
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

    for (imp = toplevel; imp != NULL; imp = imp->delegate)
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

pixman_bool_t
_pixman_multiply_overflows_size (size_t a, size_t b)
{
    return a >= SIZE_MAX / b;
}

pixman_bool_t
_pixman_multiply_overflows_int (unsigned int a, unsigned int b)
{
    return a >= INT32_MAX / b;
}

pixman_bool_t
_pixman_addition_overflows_int (unsigned int a, unsigned int b)
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
	uint8_t a, r, g, b;
	uint64_t a16, r16, g16, b16;

	if (a_size)
	{
	    a = (pixel >> a_shift) & a_mask;
	    a16 = unorm_to_unorm (a, a_size, 16);
	}
	else
	{
	    a16 = 0xffff;
	}

	if (r_size)
	{
	    r = (pixel >> r_shift) & r_mask;
	    g = (pixel >> g_shift) & g_mask;
	    b = (pixel >> b_shift) & b_mask;
	    r16 = unorm_to_unorm (r, r_size, 16);
	    g16 = unorm_to_unorm (g, g_size, 16);
	    b16 = unorm_to_unorm (b, b_size, 16);
	}
	else
	{
	    r16 = g16 = b16 = 0;
	}
	
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

uint32_t *
_pixman_iter_get_scanline_noop (pixman_iter_t *iter, const uint32_t *mask)
{
    return iter->buffer;
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

#ifdef DEBUG

void
_pixman_log_error (const char *function, const char *message)
{
    static int n_messages = 0;

    if (n_messages < 10)
    {
	fprintf (stderr,
		 "*** BUG ***\n"
		 "In %s: %s\n"
		 "Set a breakpoint on '_pixman_log_error' to debug\n\n",
                 function, message);

	n_messages++;
    }
}

#endif
