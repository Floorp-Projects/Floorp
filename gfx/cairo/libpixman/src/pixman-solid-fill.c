/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007, 2009 Red Hat, Inc.
 * Copyright © 2009 Soren Sandmann
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "pixman-private.h"

static void
solid_fill_get_scanline_32 (pixman_image_t *image,
                            int             x,
                            int             y,
                            int             width,
                            uint32_t *      buffer,
                            const uint32_t *mask,
                            uint32_t        mask_bits)
{
    uint32_t *end = buffer + width;
    uint32_t color = image->solid.color_32;

    while (buffer < end)
	*(buffer++) = color;

    return;
}

static void
solid_fill_get_scanline_64 (pixman_image_t *image,
			    int             x,
			    int             y,
			    int             width,
			    uint32_t *      buffer,
			    const uint32_t *mask,
			    uint32_t        mask_bits)
{
    uint64_t *b = (uint64_t *)buffer;
    uint64_t *e = b + width;
    uint64_t color = image->solid.color_64;

    while (b < e)
	*(b++) = color;
}

static source_image_class_t
solid_fill_classify (pixman_image_t *image,
                     int             x,
                     int             y,
                     int             width,
                     int             height)
{
    return (image->source.class = SOURCE_IMAGE_CLASS_HORIZONTAL);
}

static void
solid_fill_property_changed (pixman_image_t *image)
{
    image->common.get_scanline_32 = solid_fill_get_scanline_32;
    image->common.get_scanline_64 = solid_fill_get_scanline_64;
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

static uint64_t
color_to_uint64 (const pixman_color_t *color)
{
    return
        ((uint64_t)color->alpha << 48) |
        ((uint64_t)color->red << 32) |
        ((uint64_t)color->green << 16) |
        ((uint64_t)color->blue);
}

PIXMAN_EXPORT pixman_image_t *
pixman_image_create_solid_fill (pixman_color_t *color)
{
    pixman_image_t *img = _pixman_image_allocate ();

    if (!img)
	return NULL;

    img->type = SOLID;
    img->solid.color = *color;
    img->solid.color_32 = color_to_uint32 (color);
    img->solid.color_64 = color_to_uint64 (color);

    img->source.class = SOURCE_IMAGE_CLASS_UNKNOWN;
    img->common.classify = solid_fill_classify;
    img->common.property_changed = solid_fill_property_changed;

    return img;
}

