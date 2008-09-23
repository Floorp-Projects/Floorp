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

#include "pixman-private.h"

#define Alpha(x) ((x) >> 24)

static void
init_source_image (source_image_t *image)
{
    image->class = SOURCE_IMAGE_CLASS_UNKNOWN;
}

static pixman_bool_t
init_gradient (gradient_t     *gradient,
	       const pixman_gradient_stop_t *stops,
	       int	       n_stops)
{
    return_val_if_fail (n_stops > 0, FALSE);

    init_source_image (&gradient->common);

    gradient->stops = pixman_malloc_ab (n_stops, sizeof (pixman_gradient_stop_t));
    if (!gradient->stops)
	return FALSE;

    memcpy (gradient->stops, stops, n_stops * sizeof (pixman_gradient_stop_t));

    gradient->n_stops = n_stops;

    gradient->stop_range = 0xffff;
    gradient->color_table = NULL;
    gradient->color_table_size = 0;

    return TRUE;
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

static pixman_image_t *
allocate_image (void)
{
    pixman_image_t *image = malloc (sizeof (pixman_image_t));

    if (image)
    {
	image_common_t *common = &image->common;

	pixman_region32_init (&common->full_region);
	pixman_region32_init (&common->clip_region);
	common->src_clip = &common->full_region;
	common->has_client_clip = FALSE;
	common->transform = NULL;
	common->repeat = PIXMAN_REPEAT_NONE;
	common->filter = PIXMAN_FILTER_NEAREST;
	common->filter_params = NULL;
	common->n_filter_params = 0;
	common->alpha_map = NULL;
	common->component_alpha = FALSE;
	common->ref_count = 1;
	common->read_func = NULL;
	common->write_func = NULL;
    }

    return image;
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
	pixman_region32_fini (&common->clip_region);
	pixman_region32_fini (&common->full_region);

	if (common->transform)
	    free (common->transform);

	if (common->filter_params)
	    free (common->filter_params);

	if (common->alpha_map)
	    pixman_image_unref ((pixman_image_t *)common->alpha_map);

#if 0
	if (image->type == BITS && image->bits.indexed)
	    free (image->bits.indexed);
#endif

#if 0
	memset (image, 0xaa, sizeof (pixman_image_t));
#endif
	if (image->type == LINEAR || image->type == RADIAL || image->type == CONICAL)
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

/* Constructors */
PIXMAN_EXPORT pixman_image_t *
pixman_image_create_solid_fill (pixman_color_t *color)
{
    pixman_image_t *img = allocate_image();
    if (!img)
	return NULL;

    init_source_image (&img->solid.common);

    img->type = SOLID;
    img->solid.color = color_to_uint32 (color);

    return img;
}

PIXMAN_EXPORT pixman_image_t *
pixman_image_create_linear_gradient (pixman_point_fixed_t         *p1,
				     pixman_point_fixed_t         *p2,
				     const pixman_gradient_stop_t *stops,
				     int                           n_stops)
{
    pixman_image_t *image;
    linear_gradient_t *linear;

    return_val_if_fail (n_stops >= 2, NULL);

    image = allocate_image();

    if (!image)
	return NULL;

    linear = &image->linear;

    if (!init_gradient (&linear->common, stops, n_stops))
    {
	free (image);
	return NULL;
    }

    linear->p1 = *p1;
    linear->p2 = *p2;

    image->type = LINEAR;

    return image;
}


PIXMAN_EXPORT pixman_image_t *
pixman_image_create_radial_gradient (pixman_point_fixed_t         *inner,
				     pixman_point_fixed_t         *outer,
				     pixman_fixed_t                inner_radius,
				     pixman_fixed_t                outer_radius,
				     const pixman_gradient_stop_t *stops,
				     int                           n_stops)
{
    pixman_image_t *image;
    radial_gradient_t *radial;

    return_val_if_fail (n_stops >= 2, NULL);

    image = allocate_image();

    if (!image)
	return NULL;

    radial = &image->radial;

    if (!init_gradient (&radial->common, stops, n_stops))
    {
	free (image);
	return NULL;
    }

    image->type = RADIAL;

    radial->c1.x = inner->x;
    radial->c1.y = inner->y;
    radial->c1.radius = inner_radius;
    radial->c2.x = outer->x;
    radial->c2.y = outer->y;
    radial->c2.radius = outer_radius;
    radial->cdx = pixman_fixed_to_double (radial->c2.x - radial->c1.x);
    radial->cdy = pixman_fixed_to_double (radial->c2.y - radial->c1.y);
    radial->dr = pixman_fixed_to_double (radial->c2.radius - radial->c1.radius);
    radial->A = (radial->cdx * radial->cdx
		 + radial->cdy * radial->cdy
		 - radial->dr  * radial->dr);

    return image;
}

PIXMAN_EXPORT pixman_image_t *
pixman_image_create_conical_gradient (pixman_point_fixed_t *center,
				      pixman_fixed_t angle,
				      const pixman_gradient_stop_t *stops,
				      int n_stops)
{
    pixman_image_t *image = allocate_image();
    conical_gradient_t *conical;

    if (!image)
	return NULL;

    conical = &image->conical;

    if (!init_gradient (&conical->common, stops, n_stops))
    {
	free (image);
	return NULL;
    }

    image->type = CONICAL;
    conical->center = *center;
    conical->angle = angle;

    return image;
}

static uint32_t *
create_bits (pixman_format_code_t format,
	     int		  width,
	     int		  height,
	     int		 *rowstride_bytes)
{
    int stride;
    int buf_size;
    int bpp;

    /* what follows is a long-winded way, avoiding any possibility of integer
     * overflows, of saying:
     * stride = ((width * bpp + FB_MASK) >> FB_SHIFT) * sizeof (uint32_t);
     */

    bpp = PIXMAN_FORMAT_BPP (format);
    if (pixman_multiply_overflows_int (width, bpp))
	return NULL;

    stride = width * bpp;
    if (pixman_addition_overflows_int (stride, FB_MASK))
	return NULL;

    stride += FB_MASK;
    stride >>= FB_SHIFT;

#if FB_SHIFT < 2
    if (pixman_multiply_overflows_int (stride, sizeof (uint32_t)))
	return NULL;
#endif
    stride *= sizeof (uint32_t);

    if (pixman_multiply_overflows_int (height, stride))
	return NULL;

    buf_size = height * stride;

    if (rowstride_bytes)
	*rowstride_bytes = stride;

    return calloc (buf_size, 1);
}

static void
reset_clip_region (pixman_image_t *image)
{
    pixman_region32_fini (&image->common.clip_region);

    if (image->type == BITS)
    {
	pixman_region32_init_rect (&image->common.clip_region, 0, 0,
				   image->bits.width, image->bits.height);
    }
    else
    {
	pixman_region32_init (&image->common.clip_region);
    }
}

PIXMAN_EXPORT pixman_image_t *
pixman_image_create_bits (pixman_format_code_t  format,
			  int                   width,
			  int                   height,
			  uint32_t	       *bits,
			  int			rowstride_bytes)
{
    pixman_image_t *image;
    uint32_t *free_me = NULL;

    /* must be a whole number of uint32_t's
     */
    return_val_if_fail (bits == NULL ||
			(rowstride_bytes % sizeof (uint32_t)) == 0, NULL);

    if (!bits && width && height)
    {
	free_me = bits = create_bits (format, width, height, &rowstride_bytes);
	if (!bits)
	    return NULL;
    }

    image = allocate_image();

    if (!image) {
	if (free_me)
	    free (free_me);
	return NULL;
    }
    
    image->type = BITS;
    image->bits.format = format;
    image->bits.width = width;
    image->bits.height = height;
    image->bits.bits = bits;
    image->bits.free_me = free_me;

    image->bits.rowstride = rowstride_bytes / (int) sizeof (uint32_t); /* we store it in number
								  * of uint32_t's
								  */
    image->bits.indexed = NULL;

    pixman_region32_fini (&image->common.full_region);
    pixman_region32_init_rect (&image->common.full_region, 0, 0,
			       image->bits.width, image->bits.height);

    reset_clip_region (image);
    return image;
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_set_clip_region32 (pixman_image_t *image,
				pixman_region32_t *region)
{
    image_common_t *common = (image_common_t *)image;

    if (region)
    {
	return pixman_region32_copy (&common->clip_region, region);
    }
    else
    {
	reset_clip_region (image);

	return TRUE;
    }
}


PIXMAN_EXPORT pixman_bool_t
pixman_image_set_clip_region (pixman_image_t    *image,
			      pixman_region16_t *region)
{
    image_common_t *common = (image_common_t *)image;

    if (region)
    {
	return pixman_region32_copy_from_region16 (&common->clip_region, region);
    }
    else
    {
	reset_clip_region (image);

	return TRUE;
    }
}

/* Sets whether the clip region includes a clip region set by the client
 */
PIXMAN_EXPORT void
pixman_image_set_has_client_clip (pixman_image_t *image,
				  pixman_bool_t	  client_clip)
{
    image->common.has_client_clip = client_clip;
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_set_transform (pixman_image_t           *image,
			    const pixman_transform_t *transform)
{
    static const pixman_transform_t id =
    {
	{ { pixman_fixed_1, 0, 0 },
	  { 0, pixman_fixed_1, 0 },
	  { 0, 0, pixman_fixed_1 }
	}
    };

    image_common_t *common = (image_common_t *)image;

    if (common->transform == transform)
	return TRUE;

    if (memcmp (&id, transform, sizeof (pixman_transform_t)) == 0)
    {
	free(common->transform);
	common->transform = NULL;
	return TRUE;
    }

    if (common->transform == NULL)
	common->transform = malloc (sizeof (pixman_transform_t));
    if (common->transform == NULL)
	return FALSE;

    memcpy(common->transform, transform, sizeof(pixman_transform_t));

    return TRUE;
}

PIXMAN_EXPORT void
pixman_image_set_repeat (pixman_image_t  *image,
			 pixman_repeat_t  repeat)
{
    image->common.repeat = repeat;
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_set_filter (pixman_image_t       *image,
			 pixman_filter_t       filter,
			 const pixman_fixed_t *params,
			 int		       n_params)
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
    return TRUE;
}

PIXMAN_EXPORT void
pixman_image_set_source_clipping (pixman_image_t  *image,
				  pixman_bool_t    source_clipping)
{
    image_common_t *common = &image->common;

    if (source_clipping)
	common->src_clip = &common->clip_region;
    else
	common->src_clip = &common->full_region;
}

/* Unlike all the other property setters, this function does not
 * copy the content of indexed. Doing this copying is simply
 * way, way too expensive.
 */
PIXMAN_EXPORT void
pixman_image_set_indexed (pixman_image_t	 *image,
			  const pixman_indexed_t *indexed)
{
    bits_image_t *bits = (bits_image_t *)image;

    bits->indexed = indexed;
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

    common->alpha_origin.x = x;
    common->alpha_origin.y = y;
}

PIXMAN_EXPORT void
pixman_image_set_component_alpha   (pixman_image_t       *image,
				    pixman_bool_t         component_alpha)
{
    image->common.component_alpha = component_alpha;
}


PIXMAN_EXPORT void
pixman_image_set_accessors (pixman_image_t             *image,
			    pixman_read_memory_func_t	read_func,
			    pixman_write_memory_func_t	write_func)
{
    return_if_fail (image != NULL);

    image->common.read_func = read_func;
    image->common.write_func = write_func;
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

static pixman_bool_t
color_to_pixel (pixman_color_t *color,
		uint32_t       *pixel,
		pixman_format_code_t format)
{
    uint32_t c = color_to_uint32 (color);

    if (!(format == PIXMAN_a8r8g8b8	||
	  format == PIXMAN_x8r8g8b8	||
	  format == PIXMAN_a8b8g8r8	||
	  format == PIXMAN_x8b8g8r8	||
	  format == PIXMAN_r5g6b5	||
	  format == PIXMAN_b5g6r5	||
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

    if (format == PIXMAN_a8)
	c = c >> 24;
    else if (format == PIXMAN_r5g6b5 ||
	     format == PIXMAN_b5g6r5)
	c = cvt8888to0565 (c);

#if 0
    printf ("color: %x %x %x %x\n", color->alpha, color->red, color->green, color->blue);
    printf ("pixel: %x\n", c);
#endif

    *pixel = c;
    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_image_fill_rectangles (pixman_op_t		    op,
			      pixman_image_t		   *dest,
			      pixman_color_t		   *color,
			      int			    n_rects,
			      const pixman_rectangle16_t   *rects)
{
    pixman_image_t *solid;
    pixman_color_t c;
    int i;

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
		pixman_region32_intersect (&fill_region, &fill_region, &dest->common.clip_region);

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

pixman_bool_t
pixman_image_can_get_solid (pixman_image_t *image)
{
    if (image->type == SOLID)
	return TRUE;

    if (image->type != BITS	||
	image->bits.width != 1	||
	image->bits.height != 1)
    {
	return FALSE;
    }

    if (image->common.repeat != PIXMAN_REPEAT_NORMAL)
	return FALSE;

    switch (image->bits.format)
    {
    case PIXMAN_a8r8g8b8:
    case PIXMAN_x8r8g8b8:
    case PIXMAN_a8b8g8r8:
    case PIXMAN_x8b8g8r8:
    case PIXMAN_r8g8b8:
    case PIXMAN_b8g8r8:
    case PIXMAN_r5g6b5:
    case PIXMAN_b5g6r5:
	return TRUE;
    default:
	return FALSE;
    }
}

pixman_bool_t
pixman_image_is_opaque(pixman_image_t *image)
{
    int i = 0;
    int gradientNumberOfColors = 0;

    if(image->common.alpha_map)
        return FALSE;

    switch(image->type)
    {
    case BITS:
        if(PIXMAN_FORMAT_A(image->bits.format))
            return FALSE;
        break;

    case LINEAR:
    case CONICAL:
    case RADIAL:
        gradientNumberOfColors = image->gradient.n_stops;
        i=0;
        while(i<gradientNumberOfColors)
        {
            if(image->gradient.stops[i].color.alpha != 0xffff)
                return FALSE;
            i++;
        }
        break;

    case SOLID:
         if(Alpha(image->solid.color) != 0xff)
            return FALSE;
        break;
    }

    /* Convolution filters can introduce translucency if the sum of the weights
       is lower than 1. */
    if (image->common.filter == PIXMAN_FILTER_CONVOLUTION)
         return FALSE;

    if (image->common.repeat == PIXMAN_REPEAT_NONE)
    {
        if (image->common.filter != PIXMAN_FILTER_NEAREST)
            return FALSE;

        if (image->common.transform)
            return FALSE;

	/* Gradients do not necessarily cover the entire compositing area */
	if (image->type == LINEAR || image->type == CONICAL || image->type == RADIAL)
	    return FALSE;
    }

     return TRUE;
}
