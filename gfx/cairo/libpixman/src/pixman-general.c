/*
 * Copyright © 2009 Red Hat, Inc.
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
 *             2008 Aaron Plattner, NVIDIA Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pixman-private.h"

static void
general_src_iter_init (pixman_implementation_t *imp, pixman_iter_t *iter)
{
    pixman_image_t *image = iter->image;

    if (image->type == SOLID)
	_pixman_solid_fill_iter_init (image, iter);
    else if (image->type == LINEAR)
	_pixman_linear_gradient_iter_init (image, iter);
    else if (image->type == RADIAL)
	_pixman_radial_gradient_iter_init (image, iter);
    else if (image->type == CONICAL)
	_pixman_conical_gradient_iter_init (image, iter);
    else if (image->type == BITS)
	_pixman_bits_image_src_iter_init (image, iter);
    else
	_pixman_log_error (FUNC, "Pixman bug: unknown image type\n");
}

static void
general_dest_iter_init (pixman_implementation_t *imp, pixman_iter_t *iter)
{
    if (iter->image->type == BITS)
    {
	_pixman_bits_image_dest_iter_init (iter->image, iter);
    }
    else
    {
	_pixman_log_error (FUNC, "Trying to write to a non-writable image");
    }
}

typedef struct op_info_t op_info_t;
struct op_info_t
{
    uint8_t src, dst;
};

#define ITER_IGNORE_BOTH						\
    (ITER_IGNORE_ALPHA | ITER_IGNORE_RGB | ITER_LOCALIZED_ALPHA)

static const op_info_t op_flags[PIXMAN_N_OPERATORS] =
{
    /* Src                   Dst                   */
    { ITER_IGNORE_BOTH,      ITER_IGNORE_BOTH      }, /* CLEAR */
    { ITER_LOCALIZED_ALPHA,  ITER_IGNORE_BOTH      }, /* SRC */
    { ITER_IGNORE_BOTH,      ITER_LOCALIZED_ALPHA  }, /* DST */
    { 0,                     ITER_LOCALIZED_ALPHA  }, /* OVER */
    { ITER_LOCALIZED_ALPHA,  0                     }, /* OVER_REVERSE */
    { ITER_LOCALIZED_ALPHA,  ITER_IGNORE_RGB       }, /* IN */
    { ITER_IGNORE_RGB,       ITER_LOCALIZED_ALPHA  }, /* IN_REVERSE */
    { ITER_LOCALIZED_ALPHA,  ITER_IGNORE_RGB       }, /* OUT */
    { ITER_IGNORE_RGB,       ITER_LOCALIZED_ALPHA  }, /* OUT_REVERSE */
    { 0,                     0                     }, /* ATOP */
    { 0,                     0                     }, /* ATOP_REVERSE */
    { 0,                     0                     }, /* XOR */
    { ITER_LOCALIZED_ALPHA,  ITER_LOCALIZED_ALPHA  }, /* ADD */
    { 0,                     0                     }, /* SATURATE */
};

#define SCANLINE_BUFFER_LENGTH 8192

static void
general_composite_rect  (pixman_implementation_t *imp,
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
    uint64_t stack_scanline_buffer[(SCANLINE_BUFFER_LENGTH * 3 + 7) / 8];
    uint8_t *scanline_buffer = (uint8_t *) stack_scanline_buffer;
    uint8_t *src_buffer, *mask_buffer, *dest_buffer;
    pixman_iter_t src_iter, mask_iter, dest_iter;
    pixman_combine_32_func_t compose;
    pixman_bool_t component_alpha;
    iter_flags_t narrow, src_flags;
    int Bpp;
    int i;

    if ((src->common.flags & FAST_PATH_NARROW_FORMAT)		&&
	(!mask || mask->common.flags & FAST_PATH_NARROW_FORMAT)	&&
	(dest->common.flags & FAST_PATH_NARROW_FORMAT))
    {
	narrow = ITER_NARROW;
	Bpp = 4;
    }
    else
    {
	narrow = 0;
	Bpp = 8;
    }

    if (width * Bpp > SCANLINE_BUFFER_LENGTH)
    {
	scanline_buffer = pixman_malloc_abc (width, 3, Bpp);

	if (!scanline_buffer)
	    return;
    }

    src_buffer = scanline_buffer;
    mask_buffer = src_buffer + width * Bpp;
    dest_buffer = mask_buffer + width * Bpp;

    /* src iter */
    src_flags = narrow | op_flags[op].src;

    _pixman_implementation_src_iter_init (imp->toplevel, &src_iter, src,
					  src_x, src_y, width, height,
					  src_buffer, src_flags);

    /* mask iter */
    if ((src_flags & (ITER_IGNORE_ALPHA | ITER_IGNORE_RGB)) ==
	(ITER_IGNORE_ALPHA | ITER_IGNORE_RGB))
    {
	/* If it doesn't matter what the source is, then it doesn't matter
	 * what the mask is
	 */
	mask = NULL;
    }

    component_alpha =
        mask                            &&
        mask->common.type == BITS       &&
        mask->common.component_alpha    &&
        PIXMAN_FORMAT_RGB (mask->bits.format);

    _pixman_implementation_src_iter_init (
	imp->toplevel, &mask_iter, mask, mask_x, mask_y, width, height,
	mask_buffer, narrow | (component_alpha? 0 : ITER_IGNORE_RGB));

    /* dest iter */
    _pixman_implementation_dest_iter_init (imp->toplevel, &dest_iter, dest,
					   dest_x, dest_y, width, height,
					   dest_buffer,
					   narrow | op_flags[op].dst);

    if (narrow)
    {
	if (component_alpha)
	    compose = _pixman_implementation_combine_32_ca;
	else
	    compose = _pixman_implementation_combine_32;
    }
    else
    {
	if (component_alpha)
	    compose = (pixman_combine_32_func_t)_pixman_implementation_combine_64_ca;
	else
	    compose = (pixman_combine_32_func_t)_pixman_implementation_combine_64;
    }

    if (!compose)
	return;

    for (i = 0; i < height; ++i)
    {
	uint32_t *s, *m, *d;

	m = mask_iter.get_scanline (&mask_iter, NULL);
	s = src_iter.get_scanline (&src_iter, m);
	d = dest_iter.get_scanline (&dest_iter, NULL);

	compose (imp->toplevel, op, d, s, m, width);

	dest_iter.write_back (&dest_iter);
    }

    if (scanline_buffer != (uint8_t *) stack_scanline_buffer)
	free (scanline_buffer);
}

static const pixman_fast_path_t general_fast_path[] =
{
    { PIXMAN_OP_any, PIXMAN_any, 0, PIXMAN_any,	0, PIXMAN_any, 0, general_composite_rect },
    { PIXMAN_OP_NONE }
};

static pixman_bool_t
general_blt (pixman_implementation_t *imp,
             uint32_t *               src_bits,
             uint32_t *               dst_bits,
             int                      src_stride,
             int                      dst_stride,
             int                      src_bpp,
             int                      dst_bpp,
             int                      src_x,
             int                      src_y,
             int                      dst_x,
             int                      dst_y,
             int                      width,
             int                      height)
{
    /* We can't blit unless we have sse2 or mmx */

    return FALSE;
}

static pixman_bool_t
general_fill (pixman_implementation_t *imp,
              uint32_t *               bits,
              int                      stride,
              int                      bpp,
              int                      x,
              int                      y,
              int                      width,
              int                      height,
              uint32_t xor)
{
    return FALSE;
}

pixman_implementation_t *
_pixman_implementation_create_general (void)
{
    pixman_implementation_t *imp = _pixman_implementation_create (NULL, general_fast_path);

    _pixman_setup_combiner_functions_32 (imp);
    _pixman_setup_combiner_functions_64 (imp);

    imp->blt = general_blt;
    imp->fill = general_fill;
    imp->src_iter_init = general_src_iter_init;
    imp->dest_iter_init = general_dest_iter_init;

    return imp;
}

