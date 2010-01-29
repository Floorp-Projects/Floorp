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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "pixman-private.h"
#include "pixman-combine32.h"

pixman_bool_t
_pixman_init_gradient (gradient_t *                  gradient,
                       const pixman_gradient_stop_t *stops,
                       int                           n_stops)
{
    return_val_if_fail (n_stops > 0, FALSE);

    gradient->stops = pixman_malloc_ab (n_stops, sizeof (pixman_gradient_stop_t));
    if (!gradient->stops)
	return FALSE;

    memcpy (gradient->stops, stops, n_stops * sizeof (pixman_gradient_stop_t));

    gradient->n_stops = n_stops;

    gradient->stop_range = 0xffff;
    gradient->common.class = SOURCE_IMAGE_CLASS_UNKNOWN;

    return TRUE;
}

/*
 * By default, just evaluate the image at 32bpp and expand.  Individual image
 * types can plug in a better scanline getter if they want to. For example
 * we  could produce smoother gradients by evaluating them at higher color
 * depth, but that's a project for the future.
 */
void
_pixman_image_get_scanline_generic_64 (pixman_image_t * image,
                                       int              x,
                                       int              y,
                                       int              width,
                                       uint32_t *       buffer,
                                       const uint32_t * mask,
                                       uint32_t         mask_bits)
{
    uint32_t *mask8 = NULL;

    /* Contract the mask image, if one exists, so that the 32-bit fetch
     * function can use it.
     */
    if (mask)
    {
	mask8 = pixman_malloc_ab (width, sizeof(uint32_t));
	if (!mask8)
	    return;

	pixman_contract (mask8, (uint64_t *)mask, width);
    }

    /* Fetch the source image into the first half of buffer. */
    _pixman_image_get_scanline_32 (image, x, y, width, (uint32_t*)buffer, mask8,
                                   mask_bits);

    /* Expand from 32bpp to 64bpp in place. */
    pixman_expand ((uint64_t *)buffer, buffer, PIXMAN_a8r8g8b8, width);

    free (mask8);
}

pixman_image_t *
_pixman_image_allocate (void)
{
    pixman_image_t *image = malloc (sizeof (pixman_image_t));

    if (image)
    {
	image_common_t *common = &image->common;

	pixman_region32_init (&common->clip_region);

	common->have_clip_region = FALSE;
	common->clip_sources = FALSE;
	common->transform = NULL;
	common->repeat = PIXMAN_REPEAT_NONE;
	common->filter = PIXMAN_FILTER_NEAREST;
	common->filter_params = NULL;
	common->n_filter_params = 0;
	common->alpha_map = NULL;
	common->component_alpha = FALSE;
	common->ref_count = 1;
	common->classify = NULL;
	common->client_clip = FALSE;
	common->destroy_func = NULL;
	common->destroy_data = NULL;
	common->need_workaround = FALSE;
	common->dirty = TRUE;
    }

    return image;
}

source_image_class_t
_pixman_image_classify (pixman_image_t *image,
                        int             x,
                        int             y,
                        int             width,
                        int             height)
{
    if (image->common.classify)
	return image->common.classify (image, x, y, width, height);
    else
	return SOURCE_IMAGE_CLASS_UNKNOWN;
}

void
_pixman_image_get_scanline_32 (pixman_image_t *image,
                               int             x,
                               int             y,
                               int             width,
                               uint32_t *      buffer,
                               const uint32_t *mask,
                               uint32_t        mask_bits)
{
    image->common.get_scanline_32 (image, x, y, width, buffer, mask, mask_bits);
}

/* Even thought the type of buffer is uint32_t *, the function actually expects
 * a uint64_t *buffer.
 */
void
_pixman_image_get_scanline_64 (pixman_image_t *image,
                               int             x,
                               int             y,
                               int             width,
                               uint32_t *      buffer,
                               const uint32_t *unused,
                               uint32_t        unused2)
{
    image->common.get_scanline_64 (image, x, y, width, buffer, unused, unused2);
}

static void
image_property_changed (pixman_image_t *image)
{
    image->common.dirty = TRUE;
}

/* Ref Counting */
PIXMAN_EXPORT pixman_image_t *
pixman_image_ref (pixman_image_t *image)
{
    image->common.ref_count++;

    return image;
}

/* returns TRUE when the image is freed */
PIXMAN_EXPORT pixman_bool_t
pixman_image_unref (pixman_image_t *image)
{
    image_common_t *common = (image_common_t *)image;

    common->ref_count--;

    if (common->ref_count == 0)
    {
	if (image->common.destroy_func)
	    image->common.destroy_func (image, image->common.destroy_data);

	pixman_region32_fini (&common->clip_region);

	if (common->transform)
	    free (common->transform);

	if (common->filter_params)
	    free (common->filter_params);

	if (common->alpha_map)
	    pixman_image_unref ((pixman_image_t *)common->alpha_map);

	if (image->type == LINEAR ||
	    image->type == RADIAL ||
	    image->type == CONICAL)
	{
	    if (image->gradient.stops)
		free (image->gradient.stops);
	}

	if (image->type == BITS && image->bits.free_me)
	    free (image->bits.free_me);

	free (image);

	return TRUE;
    }

    return FALSE;
}

PIXMAN_EXPORT void
pixman_image_set_destroy_function (pixman_image_t *            image,
                                   pixman_image_destroy_func_t func,
                                   void *                      data)
{
    image->common.destroy_func = func;
    image->common.destroy_data = data;
}

void
_pixman_image_reset_clip_region (pixman_image_t *image)
{
    image->common.have_clip_region = FALSE;
}

void
_pixman_image_validate (pixman_image_t *image)
{
    if (image->common.dirty)
    {
	image->common.property_changed (image);
	image->common.dirty = FALSE;
    }
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_set_clip_region32 (pixman_image_t *   image,
                                pixman_region32_t *region)
{
    image_common_t *common = (image_common_t *)image;
    pixman_bool_t result;

    if (region)
    {
	if ((result = pixman_region32_copy (&common->clip_region, region)))
	    image->common.have_clip_region = TRUE;
    }
    else
    {
	_pixman_image_reset_clip_region (image);

	result = TRUE;
    }

    image_property_changed (image);

    return result;
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_set_clip_region (pixman_image_t *   image,
                              pixman_region16_t *region)
{
    image_common_t *common = (image_common_t *)image;
    pixman_bool_t result;

    if (region)
    {
	if ((result = pixman_region32_copy_from_region16 (&common->clip_region, region)))
	    image->common.have_clip_region = TRUE;
    }
    else
    {
	_pixman_image_reset_clip_region (image);

	result = TRUE;
    }

    image_property_changed (image);

    return result;
}

PIXMAN_EXPORT void
pixman_image_set_has_client_clip (pixman_image_t *image,
                                  pixman_bool_t   client_clip)
{
    image->common.client_clip = client_clip;
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_set_transform (pixman_image_t *          image,
                            const pixman_transform_t *transform)
{
    static const pixman_transform_t id =
    {
	{ { pixman_fixed_1, 0, 0 },
	  { 0, pixman_fixed_1, 0 },
	  { 0, 0, pixman_fixed_1 } }
    };

    image_common_t *common = (image_common_t *)image;
    pixman_bool_t result;

    if (common->transform == transform)
	return TRUE;

    if (memcmp (&id, transform, sizeof (pixman_transform_t)) == 0)
    {
	free (common->transform);
	common->transform = NULL;
	result = TRUE;

	goto out;
    }

    if (common->transform == NULL)
	common->transform = malloc (sizeof (pixman_transform_t));

    if (common->transform == NULL)
    {
	result = FALSE;

	goto out;
    }

    memcpy (common->transform, transform, sizeof(pixman_transform_t));

    result = TRUE;

out:
    image_property_changed (image);

    return result;
}

PIXMAN_EXPORT void
pixman_image_set_repeat (pixman_image_t *image,
                         pixman_repeat_t repeat)
{
    image->common.repeat = repeat;

    image_property_changed (image);
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_set_filter (pixman_image_t *      image,
                         pixman_filter_t       filter,
                         const pixman_fixed_t *params,
                         int                   n_params)
{
    image_common_t *common = (image_common_t *)image;
    pixman_fixed_t *new_params;

    if (params == common->filter_params && filter == common->filter)
	return TRUE;

    new_params = NULL;
    if (params)
    {
	new_params = pixman_malloc_ab (n_params, sizeof (pixman_fixed_t));
	if (!new_params)
	    return FALSE;

	memcpy (new_params,
	        params, n_params * sizeof (pixman_fixed_t));
    }

    common->filter = filter;

    if (common->filter_params)
	free (common->filter_params);

    common->filter_params = new_params;
    common->n_filter_params = n_params;

    image_property_changed (image);
    return TRUE;
}

PIXMAN_EXPORT void
pixman_image_set_source_clipping (pixman_image_t *image,
                                  pixman_bool_t   clip_sources)
{
    image->common.clip_sources = clip_sources;

    image_property_changed (image);
}

/* Unlike all the other property setters, this function does not
 * copy the content of indexed. Doing this copying is simply
 * way, way too expensive.
 */
PIXMAN_EXPORT void
pixman_image_set_indexed (pixman_image_t *        image,
                          const pixman_indexed_t *indexed)
{
    bits_image_t *bits = (bits_image_t *)image;

    bits->indexed = indexed;

    image_property_changed (image);
}

PIXMAN_EXPORT void
pixman_image_set_alpha_map (pixman_image_t *image,
                            pixman_image_t *alpha_map,
                            int16_t         x,
                            int16_t         y)
{
    image_common_t *common = (image_common_t *)image;

    return_if_fail (!alpha_map || alpha_map->type == BITS);

    if (common->alpha_map != (bits_image_t *)alpha_map)
    {
	if (common->alpha_map)
	    pixman_image_unref ((pixman_image_t *)common->alpha_map);

	if (alpha_map)
	    common->alpha_map = (bits_image_t *)pixman_image_ref (alpha_map);
	else
	    common->alpha_map = NULL;
    }

    common->alpha_origin_x = x;
    common->alpha_origin_y = y;

    image_property_changed (image);
}

PIXMAN_EXPORT void
pixman_image_set_component_alpha   (pixman_image_t *image,
                                    pixman_bool_t   component_alpha)
{
    image->common.component_alpha = component_alpha;

    image_property_changed (image);
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_get_component_alpha   (pixman_image_t *image)
{
    return image->common.component_alpha;
}

PIXMAN_EXPORT void
pixman_image_set_accessors (pixman_image_t *           image,
                            pixman_read_memory_func_t  read_func,
                            pixman_write_memory_func_t write_func)
{
    return_if_fail (image != NULL);

    if (image->type == BITS)
    {
	image->bits.read_func = read_func;
	image->bits.write_func = write_func;

	image_property_changed (image);
    }
}

PIXMAN_EXPORT uint32_t *
pixman_image_get_data (pixman_image_t *image)
{
    if (image->type == BITS)
	return image->bits.bits;

    return NULL;
}

PIXMAN_EXPORT int
pixman_image_get_width (pixman_image_t *image)
{
    if (image->type == BITS)
	return image->bits.width;

    return 0;
}

PIXMAN_EXPORT int
pixman_image_get_height (pixman_image_t *image)
{
    if (image->type == BITS)
	return image->bits.height;

    return 0;
}

PIXMAN_EXPORT int
pixman_image_get_stride (pixman_image_t *image)
{
    if (image->type == BITS)
	return image->bits.rowstride * (int) sizeof (uint32_t);

    return 0;
}

PIXMAN_EXPORT int
pixman_image_get_depth (pixman_image_t *image)
{
    if (image->type == BITS)
	return PIXMAN_FORMAT_DEPTH (image->bits.format);

    return 0;
}

pixman_bool_t
_pixman_image_is_solid (pixman_image_t *image)
{
    if (image->type == SOLID)
	return TRUE;

    if (image->type != BITS     ||
        image->bits.width != 1  ||
        image->bits.height != 1)
    {
	return FALSE;
    }

    if (image->common.repeat == PIXMAN_REPEAT_NONE)
	return FALSE;

    return TRUE;
}

uint32_t
_pixman_image_get_solid (pixman_image_t *     image,
                         pixman_format_code_t format)
{
    uint32_t result;

    _pixman_image_get_scanline_32 (image, 0, 0, 1, &result, NULL, 0);

    /* If necessary, convert RGB <--> BGR. */
    if (PIXMAN_FORMAT_TYPE (format) != PIXMAN_TYPE_ARGB)
    {
	result = (((result & 0xff000000) >>  0) |
	          ((result & 0x00ff0000) >> 16) |
	          ((result & 0x0000ff00) >>  0) |
	          ((result & 0x000000ff) << 16));
    }

    return result;
}

pixman_bool_t
_pixman_image_is_opaque (pixman_image_t *image)
{
    int i;

    if (image->common.alpha_map)
	return FALSE;

    switch (image->type)
    {
    case BITS:
	if (image->common.repeat == PIXMAN_REPEAT_NONE)
	    return FALSE;

	if (PIXMAN_FORMAT_A (image->bits.format))
	    return FALSE;
	break;

    case LINEAR:
    case RADIAL:
	if (image->common.repeat == PIXMAN_REPEAT_NONE)
	    return FALSE;

	for (i = 0; i < image->gradient.n_stops; ++i)
	{
	    if (image->gradient.stops[i].color.alpha != 0xffff)
		return FALSE;
	}
	break;

    case CONICAL:
	/* Conical gradients always have a transparent border */
	return FALSE;
	break;

    case SOLID:
	if (ALPHA_8 (image->solid.color) != 0xff)
	    return FALSE;
	break;

    default:
        return FALSE;
        break;
    }

    /* Convolution filters can introduce translucency if the sum of the
     * weights is lower than 1.
     */
    if (image->common.filter == PIXMAN_FILTER_CONVOLUTION)
	return FALSE;

    return TRUE;
}

