/*
 * Copyright © 2009 Red Hat, Inc.
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
#include "pixman-private.h"

static pixman_bool_t
delegate_blt (pixman_implementation_t * imp,
              uint32_t *                src_bits,
              uint32_t *                dst_bits,
              int                       src_stride,
              int                       dst_stride,
              int                       src_bpp,
              int                       dst_bpp,
              int                       src_x,
              int                       src_y,
              int                       dest_x,
              int                       dest_y,
              int                       width,
              int                       height)
{
    return _pixman_implementation_blt (
	imp->delegate, src_bits, dst_bits, src_stride, dst_stride,
	src_bpp, dst_bpp, src_x, src_y, dest_x, dest_y,
	width, height);
}

static pixman_bool_t
delegate_fill (pixman_implementation_t *imp,
               uint32_t *               bits,
               int                      stride,
               int                      bpp,
               int                      x,
               int                      y,
               int                      width,
               int                      height,
               uint32_t                 xor)
{
    return _pixman_implementation_fill (
	imp->delegate, bits, stride, bpp, x, y, width, height, xor);
}

static void
delegate_src_iter_init (pixman_implementation_t *imp,
			pixman_iter_t *	         iter)
{
    imp->delegate->src_iter_init (imp->delegate, iter);
}

static void
delegate_dest_iter_init (pixman_implementation_t *imp,
			 pixman_iter_t *	  iter)
{
    imp->delegate->dest_iter_init (imp->delegate, iter);
}

pixman_implementation_t *
_pixman_implementation_create (pixman_implementation_t *delegate,
			       const pixman_fast_path_t *fast_paths)
{
    pixman_implementation_t *imp = malloc (sizeof (pixman_implementation_t));
    pixman_implementation_t *d;
    int i;

    if (!imp)
	return NULL;

    assert (fast_paths);

    /* Make sure the whole delegate chain has the right toplevel */
    imp->delegate = delegate;
    for (d = imp; d != NULL; d = d->delegate)
	d->toplevel = imp;

    /* Fill out function pointers with ones that just delegate
     */
    imp->blt = delegate_blt;
    imp->fill = delegate_fill;
    imp->src_iter_init = delegate_src_iter_init;
    imp->dest_iter_init = delegate_dest_iter_init;

    imp->fast_paths = fast_paths;

    for (i = 0; i < PIXMAN_N_OPERATORS; ++i)
    {
	imp->combine_32[i] = NULL;
	imp->combine_64[i] = NULL;
	imp->combine_32_ca[i] = NULL;
	imp->combine_64_ca[i] = NULL;
    }

    return imp;
}

pixman_combine_32_func_t
_pixman_implementation_lookup_combiner (pixman_implementation_t *imp,
					pixman_op_t		 op,
					pixman_bool_t		 component_alpha,
					pixman_bool_t		 narrow)
{
    pixman_combine_32_func_t f;

    do
    {
	pixman_combine_32_func_t (*combiners[]) =
	{
	    (pixman_combine_32_func_t *)imp->combine_64,
	    (pixman_combine_32_func_t *)imp->combine_64_ca,
	    imp->combine_32,
	    imp->combine_32_ca,
	};

	f = combiners[component_alpha | (narrow << 1)][op];

	imp = imp->delegate;
    }
    while (!f);

    return f;
}

pixman_bool_t
_pixman_implementation_blt (pixman_implementation_t * imp,
                            uint32_t *                src_bits,
                            uint32_t *                dst_bits,
                            int                       src_stride,
                            int                       dst_stride,
                            int                       src_bpp,
                            int                       dst_bpp,
                            int                       src_x,
                            int                       src_y,
                            int                       dest_x,
                            int                       dest_y,
                            int                       width,
                            int                       height)
{
    return (*imp->blt) (imp, src_bits, dst_bits, src_stride, dst_stride,
			src_bpp, dst_bpp, src_x, src_y, dest_x, dest_y,
			width, height);
}

pixman_bool_t
_pixman_implementation_fill (pixman_implementation_t *imp,
                             uint32_t *               bits,
                             int                      stride,
                             int                      bpp,
                             int                      x,
                             int                      y,
                             int                      width,
                             int                      height,
                             uint32_t                 xor)
{
    return (*imp->fill) (imp, bits, stride, bpp, x, y, width, height, xor);
}

void
_pixman_implementation_src_iter_init (pixman_implementation_t	*imp,
				      pixman_iter_t             *iter,
				      pixman_image_t		*image,
				      int			 x,
				      int			 y,
				      int			 width,
				      int			 height,
				      uint8_t			*buffer,
				      iter_flags_t		 flags)
{
    iter->image = image;
    iter->buffer = (uint32_t *)buffer;
    iter->x = x;
    iter->y = y;
    iter->width = width;
    iter->height = height;
    iter->flags = flags;

    (*imp->src_iter_init) (imp, iter);
}

void
_pixman_implementation_dest_iter_init (pixman_implementation_t	*imp,
				       pixman_iter_t            *iter,
				       pixman_image_t		*image,
				       int			 x,
				       int			 y,
				       int			 width,
				       int			 height,
				       uint8_t			*buffer,
				       iter_flags_t		 flags)
{
    iter->image = image;
    iter->buffer = (uint32_t *)buffer;
    iter->x = x;
    iter->y = y;
    iter->width = width;
    iter->height = height;
    iter->flags = flags;

    (*imp->dest_iter_init) (imp, iter);
}
