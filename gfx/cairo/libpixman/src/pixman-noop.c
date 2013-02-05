/* -*- Mode: c; c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t; -*- */
/*
 * Copyright © 2011 Red Hat, Inc.
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
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdlib.h>
#include "pixman-private.h"
#include "pixman-combine32.h"
#include "pixman-inlines.h"

static void
noop_composite (pixman_implementation_t *imp,
		pixman_composite_info_t *info)
{
    return;
}

static void
dest_write_back_direct (pixman_iter_t *iter)
{
    iter->buffer += iter->image->bits.rowstride;
}

static uint32_t *
noop_get_scanline (pixman_iter_t *iter, const uint32_t *mask)
{
    uint32_t *result = iter->buffer;

    iter->buffer += iter->image->bits.rowstride;

    return result;
}

static uint32_t *
get_scanline_null (pixman_iter_t *iter, const uint32_t *mask)
{
    return NULL;
}

static pixman_bool_t
noop_src_iter_init (pixman_implementation_t *imp, pixman_iter_t *iter)
{
    pixman_image_t *image = iter->image;

#define FLAGS						\
    (FAST_PATH_STANDARD_FLAGS | FAST_PATH_ID_TRANSFORM)

    if (!image)
    {
	iter->get_scanline = get_scanline_null;
    }
    else if ((iter->iter_flags & (ITER_IGNORE_ALPHA | ITER_IGNORE_RGB)) ==
	     (ITER_IGNORE_ALPHA | ITER_IGNORE_RGB))
    {
	iter->get_scanline = _pixman_iter_get_scanline_noop;
    }
    else if (image->common.extended_format_code == PIXMAN_solid		&&
	     ((iter->image_flags & (FAST_PATH_BITS_IMAGE | FAST_PATH_NO_ALPHA_MAP)) ==
	      (FAST_PATH_BITS_IMAGE | FAST_PATH_NO_ALPHA_MAP)))
    {
	bits_image_t *bits = &image->bits;

	if (iter->iter_flags & ITER_NARROW)
	{
	    uint32_t color = bits->fetch_pixel_32 (bits, 0, 0);
	    uint32_t *buffer = iter->buffer;
	    uint32_t *end = buffer + iter->width;

	    while (buffer < end)
		*(buffer++) = color;
	}
	else
	{
	    argb_t color = bits->fetch_pixel_float (bits, 0, 0);
	    argb_t *buffer = (argb_t *)iter->buffer;
	    argb_t *end = buffer + iter->width;

	    while (buffer < end)
		*(buffer++) = color;
	}

	iter->get_scanline = _pixman_iter_get_scanline_noop;
    }
    else if (image->common.extended_format_code == PIXMAN_a8r8g8b8	&&
	     (iter->iter_flags & ITER_NARROW)				&&
	     (iter->image_flags & FLAGS) == FLAGS			&&
	     iter->x >= 0 && iter->y >= 0				&&
	     iter->x + iter->width <= image->bits.width			&&
	     iter->y + iter->height <= image->bits.height)
    {
	iter->buffer =
	    image->bits.bits + iter->y * image->bits.rowstride + iter->x;

	iter->get_scanline = noop_get_scanline;
    }
    else
    {
	return FALSE;
    }

    return TRUE;
}

static pixman_bool_t
noop_dest_iter_init (pixman_implementation_t *imp, pixman_iter_t *iter)
{
    pixman_image_t *image = iter->image;
    uint32_t image_flags = iter->image_flags;
    uint32_t iter_flags = iter->iter_flags;
    
    if ((image_flags & FAST_PATH_STD_DEST_FLAGS) == FAST_PATH_STD_DEST_FLAGS	&&
	(iter_flags & ITER_NARROW) == ITER_NARROW				&&
	((image->common.extended_format_code == PIXMAN_a8r8g8b8)	||
	 (image->common.extended_format_code == PIXMAN_x8r8g8b8 &&
	  (iter_flags & (ITER_LOCALIZED_ALPHA)))))
    {
	iter->buffer = image->bits.bits + iter->y * image->bits.rowstride + iter->x;

	iter->get_scanline = _pixman_iter_get_scanline_noop;
	iter->write_back = dest_write_back_direct;

	return TRUE;
    }
    else
    {
	return FALSE;
    }
}

static const pixman_fast_path_t noop_fast_paths[] =
{
    { PIXMAN_OP_DST, PIXMAN_any, 0, PIXMAN_any, 0, PIXMAN_any, 0, noop_composite },
    { PIXMAN_OP_NONE },
};

pixman_implementation_t *
_pixman_implementation_create_noop (pixman_implementation_t *fallback)
{
    pixman_implementation_t *imp =
	_pixman_implementation_create (fallback, noop_fast_paths);
 
    imp->src_iter_init = noop_src_iter_init;
    imp->dest_iter_init = noop_dest_iter_init;

    return imp;
}
