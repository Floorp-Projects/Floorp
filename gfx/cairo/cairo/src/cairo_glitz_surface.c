/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 David Reveman
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of David
 * Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. David Reveman makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include "cairoint.h"
#include "cairo-glitz.h"

void
cairo_set_target_glitz (cairo_t *cr, glitz_surface_t *surface)
{
    cairo_surface_t *crsurface;

    if (cr->status && cr->status != CAIRO_STATUS_NO_TARGET_SURFACE)
	return;

    crsurface = cairo_glitz_surface_create (surface);
    if (crsurface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, crsurface);

    cairo_surface_destroy (crsurface);
}

typedef struct _cairo_glitz_surface {
    cairo_surface_t   base;

    glitz_surface_t   *surface;
    glitz_format_t    *format;
    pixman_region16_t *clip;
} cairo_glitz_surface_t;

static void
_cairo_glitz_surface_destroy (void *abstract_surface)
{
    cairo_glitz_surface_t *surface = abstract_surface;

    if (surface->clip)
    {
	glitz_surface_set_clip_region (surface->surface, 0, 0, NULL, 0);
	pixman_region_destroy (surface->clip);
    }
    
    glitz_surface_destroy (surface->surface);
    free (surface);
}

static glitz_format_name_t
_glitz_format (cairo_format_t format)
{
    switch (format) {
    default:
    case CAIRO_FORMAT_ARGB32:
	return GLITZ_STANDARD_ARGB32;
    case CAIRO_FORMAT_RGB24:
	return GLITZ_STANDARD_RGB24;
    case CAIRO_FORMAT_A8:
	return GLITZ_STANDARD_A8;
    case CAIRO_FORMAT_A1:
	return GLITZ_STANDARD_A1;
    }
}

static cairo_surface_t *
_cairo_glitz_surface_create_similar (void	    *abstract_src,
				     cairo_format_t format,
				     int	    draw,
				     int	    width,
				     int	    height)
{
    cairo_glitz_surface_t *src = abstract_src;
    cairo_surface_t	  *crsurface;
    glitz_drawable_t	  *drawable;
    glitz_surface_t	  *surface;
    glitz_format_t	  *gformat;

    drawable = glitz_surface_get_drawable (src->surface);
    
    gformat = glitz_find_standard_format (drawable, _glitz_format (format));
    if (!gformat)
	return NULL;
    
    surface = glitz_surface_create (drawable, gformat, width, height, 0, NULL);
    if (!surface)
	return NULL;

    crsurface = cairo_glitz_surface_create (surface);
    
    glitz_surface_destroy (surface);

    return crsurface;
}

static double
_cairo_glitz_surface_pixels_per_inch (void *abstract_surface)
{
    return 96.0;
}

static cairo_status_t
_cairo_glitz_surface_get_image (cairo_glitz_surface_t *surface,
				cairo_rectangle_t     *interest,
				cairo_image_surface_t **image_out,
				cairo_rectangle_t     *rect_out)
{
    cairo_image_surface_t *image;
    int			  x1, y1, x2, y2;
    int			  width, height;
    char		  *pixels;
    cairo_format_masks_t  format;
    glitz_buffer_t	  *buffer;
    glitz_pixel_format_t  pf;

    x1 = 0;
    y1 = 0;
    x2 = glitz_surface_get_width (surface->surface);
    y2 = glitz_surface_get_height (surface->surface);

    if (interest)
    {
	if (interest->x > x1)
	    x1 = interest->x;
	if (interest->y > y1)
	    y1 = interest->y;
	if (interest->x + interest->width < x2)
	    x2 = interest->x + interest->width;
	if (interest->y + interest->height < y2)
	    y2 = interest->y + interest->height;

	if (x1 >= x2 || y1 >= y2)
	{
	    *image_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    width  = x2 - x1;
    height = y2 - y1;

    if (rect_out)
    {
	rect_out->x = x1;
	rect_out->y = y1;
	rect_out->width = width;
	rect_out->height = height;
    }
    
    if (surface->format->type == GLITZ_FORMAT_TYPE_COLOR) {
	if (surface->format->color.red_size > 0) {
	    format.bpp = 32;
	    
	    if (surface->format->color.alpha_size > 0)
		format.alpha_mask = 0xff000000;
	    else
		format.alpha_mask = 0x0;
	    
	    format.red_mask = 0xff0000;
	    format.green_mask = 0xff00;
	    format.blue_mask = 0xff;
	} else {
	    format.bpp = 8;
	    format.blue_mask = format.green_mask = format.red_mask = 0x0;
	    format.alpha_mask = 0xff;
	}
    } else {
	format.bpp = 32;
	format.alpha_mask = 0xff000000;
	format.red_mask = 0xff0000;
	format.green_mask = 0xff00;
	format.blue_mask = 0xff;
    }

    pf.masks.bpp = format.bpp;
    pf.masks.alpha_mask = format.alpha_mask;
    pf.masks.red_mask = format.red_mask;
    pf.masks.green_mask = format.green_mask;
    pf.masks.blue_mask = format.blue_mask;
    pf.xoffset = 0;
    pf.skip_lines = 0;

    /* XXX: we should eventually return images with negative stride,
       need to verify that libpixman have no problem with this first. */
    pf.bytes_per_line = (((width * format.bpp) / 8) + 3) & -4;
    pf.scanline_order = GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN;

    pixels = malloc (height * pf.bytes_per_line);
    if (!pixels)
	return CAIRO_STATUS_NO_MEMORY;

    buffer = glitz_buffer_create_for_data (pixels);
    if (!buffer) {
	free (pixels);
	return CAIRO_STATUS_NO_MEMORY;
    }
    
    glitz_get_pixels (surface->surface,
		      x1, y1,
		      width, height,
		      &pf,
		      buffer);

    glitz_buffer_destroy (buffer);
    
    image = (cairo_image_surface_t *)
        _cairo_image_surface_create_with_masks (pixels,
						&format,
						width, height,
						pf.bytes_per_line);

    if (!image)
    {
	free (pixels);
	return CAIRO_STATUS_NO_MEMORY;
    }

    _cairo_image_surface_assume_ownership_of_data (image);

    _cairo_image_surface_set_repeat (image, surface->base.repeat);
    _cairo_image_surface_set_matrix (image, &(surface->base.matrix));

    *image_out = image;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_glitz_surface_set_image (void		      *abstract_surface,
				cairo_image_surface_t *image,
				int		      x_dst,
				int		      y_dst)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    glitz_buffer_t	  *buffer;
    glitz_pixel_format_t  pf;
    pixman_format_t	  *format;
    int			  am, rm, gm, bm;
    char		  *data;
    
    format = pixman_image_get_format (image->pixman_image);
    if (!format)
	return CAIRO_STATUS_NO_MEMORY;

    pixman_format_get_masks (format, &pf.masks.bpp, &am, &rm, &gm, &bm);

    pf.masks.alpha_mask = am;
    pf.masks.red_mask = rm;
    pf.masks.green_mask = gm;
    pf.masks.blue_mask = bm;
    pf.xoffset = 0;
    pf.skip_lines = 0;

    /* check for negative stride */
    if (image->stride < 0)
    {
	pf.bytes_per_line = -image->stride;
	pf.scanline_order = GLITZ_PIXEL_SCANLINE_ORDER_BOTTOM_UP;
	data = (char *) image->data + image->stride * (image->height - 1);
    }
    else
    {
	pf.bytes_per_line = image->stride;
	pf.scanline_order = GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN;
	data = (char *) image->data;
    }

    buffer = glitz_buffer_create_for_data (data);
    if (!buffer)
	return CAIRO_STATUS_NO_MEMORY;
    
    glitz_set_pixels (surface->surface,
		      x_dst, y_dst,
		      image->width, image->height,
		      &pf,
		      buffer);
    
    glitz_buffer_destroy (buffer);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_glitz_surface_acquire_source_image (void              *abstract_surface,
					   cairo_image_surface_t **image_out,
					   void                  **image_extra)
{
    cairo_glitz_surface_t *surface = abstract_surface;

    *image_extra = NULL;
    
    return _cairo_glitz_surface_get_image (surface, NULL, image_out, NULL);
}

static void
_cairo_glitz_surface_release_source_image (void              *abstract_surface,
					   cairo_image_surface_t *image,
					   void                  *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_glitz_surface_acquire_dest_image (void                *abstract_surface,
					 cairo_rectangle_t     *interest_rect,
					 cairo_image_surface_t **image_out,
					 cairo_rectangle_t     *image_rect_out,
					 void                  **image_extra)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t	  status;

    status = _cairo_glitz_surface_get_image (surface, interest_rect, &image,
					     image_rect_out);
    if (status)
	return status;

    *image_out   = image;
    *image_extra = NULL;

    return status;
}

static void
_cairo_glitz_surface_release_dest_image (void                *abstract_surface,
					 cairo_rectangle_t     *interest_rect,
					 cairo_image_surface_t *image,
					 cairo_rectangle_t     *image_rect,
					 void                  *image_extra)
{
    cairo_glitz_surface_t *surface = abstract_surface;

    _cairo_glitz_surface_set_image (surface, image,
				    image_rect->x, image_rect->y);

    cairo_surface_destroy (&image->base);
}


static cairo_status_t
_cairo_glitz_surface_clone_similar (void	    *abstract_surface,
				    cairo_surface_t *src,
				    cairo_surface_t **clone_out)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    cairo_glitz_surface_t *clone;

    if (src->backend == surface->base.backend)
    {
	*clone_out = src;
	cairo_surface_reference (src);
	
	return CAIRO_STATUS_SUCCESS;
    }
    else if (_cairo_surface_is_image (src))
    {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *) src;
    
	clone = (cairo_glitz_surface_t *)
	    _cairo_glitz_surface_create_similar (surface, image_src->format, 0,
						 image_src->width,
						 image_src->height);
	if (!clone)
	    return CAIRO_STATUS_NO_MEMORY;

	_cairo_glitz_surface_set_image (clone, image_src, 0, 0);
	
	*clone_out = &clone->base;

	return CAIRO_STATUS_SUCCESS;
    }
    
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void
_cairo_glitz_surface_set_matrix (cairo_glitz_surface_t *surface,
				 cairo_matrix_t	       *matrix)
{
    glitz_transform_t transform;

    transform.matrix[0][0] = _cairo_fixed_from_double (matrix->m[0][0]);
    transform.matrix[0][1] = _cairo_fixed_from_double (matrix->m[1][0]);
    transform.matrix[0][2] = _cairo_fixed_from_double (matrix->m[2][0]);

    transform.matrix[1][0] = _cairo_fixed_from_double (matrix->m[0][1]);
    transform.matrix[1][1] = _cairo_fixed_from_double (matrix->m[1][1]);
    transform.matrix[1][2] = _cairo_fixed_from_double (matrix->m[2][1]);

    transform.matrix[2][0] = 0;
    transform.matrix[2][1] = 0;
    transform.matrix[2][2] = 1 << 16;

    glitz_surface_set_transform (surface->surface, &transform);
}

static glitz_operator_t
_glitz_operator (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return GLITZ_OPERATOR_CLEAR;
    case CAIRO_OPERATOR_SRC:
	return GLITZ_OPERATOR_SRC;
    case CAIRO_OPERATOR_DST:
	return GLITZ_OPERATOR_DST;
    case CAIRO_OPERATOR_OVER_REVERSE:
	return GLITZ_OPERATOR_OVER_REVERSE;
    case CAIRO_OPERATOR_IN:
	return GLITZ_OPERATOR_IN;
    case CAIRO_OPERATOR_IN_REVERSE:
	return GLITZ_OPERATOR_IN_REVERSE;
    case CAIRO_OPERATOR_OUT:
	return GLITZ_OPERATOR_OUT;
    case CAIRO_OPERATOR_OUT_REVERSE:
	return GLITZ_OPERATOR_OUT_REVERSE;
    case CAIRO_OPERATOR_ATOP:
	return GLITZ_OPERATOR_ATOP;
    case CAIRO_OPERATOR_ATOP_REVERSE:
	return GLITZ_OPERATOR_ATOP_REVERSE;
    case CAIRO_OPERATOR_XOR:
	return GLITZ_OPERATOR_XOR;
    case CAIRO_OPERATOR_ADD:
	return GLITZ_OPERATOR_ADD;
    case CAIRO_OPERATOR_OVER:
    default:
	return GLITZ_OPERATOR_OVER;
    }
}

static glitz_status_t
_glitz_ensure_target (glitz_surface_t *surface)
{
    glitz_drawable_t *drawable;

    drawable = glitz_surface_get_attached_drawable (surface);
    if (!drawable) {
	glitz_drawable_format_t *dformat;
	glitz_drawable_format_t templ;
	glitz_format_t *format;
	glitz_drawable_t *pbuffer;
	unsigned long mask;
	int i;
	
	format = glitz_surface_get_format (surface);
	if (format->type != GLITZ_FORMAT_TYPE_COLOR)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	drawable = glitz_surface_get_drawable (surface);
	dformat = glitz_drawable_get_format (drawable);

	templ.types.pbuffer = 1;
	mask = GLITZ_FORMAT_PBUFFER_MASK;

	templ.samples = dformat->samples;
	mask |= GLITZ_FORMAT_SAMPLES_MASK;

	i = 0;
	do {
	    dformat = glitz_find_similar_drawable_format (drawable,
							  mask, &templ, i++);

	    if (dformat) {
		int sufficient = 1;
		
		if (format->color.red_size) {
		    if (dformat->color.red_size < format->color.red_size)
			sufficient = 0;
		}
		if (format->color.alpha_size) {
		    if (dformat->color.alpha_size < format->color.alpha_size)
			sufficient = 0;
		}

		if (sufficient)
		    break;
	    }
	} while (dformat);
	
	if (!dformat)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	pbuffer =
	    glitz_create_pbuffer_drawable (drawable, dformat,
					   glitz_surface_get_width (surface),
					   glitz_surface_get_height (surface));
	if (!pbuffer)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	glitz_surface_attach (surface, pbuffer,
			      GLITZ_DRAWABLE_BUFFER_FRONT_COLOR,
			      0, 0);

	glitz_drawable_destroy (pbuffer);
    }

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _cairo_glitz_surface_attributes {
    cairo_surface_attributes_t	base;
    
    glitz_fill_t		fill;
    glitz_filter_t		filter;
    glitz_fixed16_16_t		*params;
    int				n_params;
    cairo_bool_t		acquired;
} cairo_glitz_surface_attributes_t;

static cairo_int_status_t
_cairo_glitz_pattern_acquire_surface (cairo_pattern_t	              *pattern,
				      cairo_glitz_surface_t	       *dst,
				      int			       x,
				      int			       y,
				      unsigned int		       width,
				      unsigned int		       height,
				      cairo_glitz_surface_t	 **surface_out,
				      cairo_glitz_surface_attributes_t *attr)
{
    cairo_glitz_surface_t *src = NULL;

    attr->acquired = FALSE;

    switch (pattern->type) {
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL: {
	cairo_gradient_pattern_t *gradient =
	    (cairo_gradient_pattern_t *) pattern;
	glitz_drawable_t	 *drawable;
	glitz_fixed16_16_t	 *params;
	int			 n_params;
	int			 i;
	unsigned short		 alpha;
	
	/* XXX: the current color gradient acceleration provided by glitz is
	 * experimental, it's been proven inappropriate in a number of ways,
	 * most importantly, it's currently implemented as filters and
	 * gradients are not filters. eventually, it will be replaced with
	 * something more appropriate.
	 */

	if (gradient->n_stops < 2)
	    break;

	/* glitz doesn't support inner and outer circle with different
	   center points. */
	if (pattern->type == CAIRO_PATTERN_RADIAL)
	{
	    cairo_radial_pattern_t *grad = (cairo_radial_pattern_t *) pattern;
	    
	    if (grad->center0.x != grad->center1.x ||
		grad->center0.y != grad->center1.y)
		break;
	}

	drawable = glitz_surface_get_drawable (dst->surface);
	if (!(glitz_drawable_get_features (drawable) &
	      GLITZ_FEATURE_FRAGMENT_PROGRAM_MASK))
	    break;

	if (pattern->filter != CAIRO_FILTER_BILINEAR &&
	    pattern->filter != CAIRO_FILTER_GOOD &&
	    pattern->filter != CAIRO_FILTER_BEST)
	    break;

	alpha = (gradient->stops[0].color.alpha * pattern->alpha) * 0xffff;
	for (i = 1; i < gradient->n_stops; i++)
	{
	    unsigned short a;
	    
	    a = (gradient->stops[i].color.alpha * pattern->alpha) * 0xffff;
	    if (a != alpha)
		break;
	}

	/* we can't have color stops with different alpha as gradient color
	   interpolation should be done to unpremultiplied colors. */
	if (i < gradient->n_stops)
	    break;

	n_params = gradient->n_stops * 3 + 4;

	params = malloc (sizeof (glitz_fixed16_16_t) * n_params);
	if (!params)
	    return CAIRO_STATUS_NO_MEMORY;

	src = (cairo_glitz_surface_t *)
	    _cairo_surface_create_similar_scratch (&dst->base,
						   CAIRO_FORMAT_ARGB32, 0,
						   gradient->n_stops, 1);
	if (!src)
	{
	    free (params);
	    return CAIRO_STATUS_NO_MEMORY;
	}

	for (i = 0; i < gradient->n_stops; i++) {
	    glitz_color_t color;

	    color.red   = gradient->stops[i].color.red   * alpha;
	    color.green = gradient->stops[i].color.green * alpha;
	    color.blue  = gradient->stops[i].color.blue  * alpha;
	    color.alpha = alpha;

	    glitz_set_rectangle (src->surface, &color, i, 0, 1, 1);

	    params[4 + 3 * i] = gradient->stops[i].offset;
	    params[5 + 3 * i] = i << 16;
	    params[6 + 3 * i] = 0;
	}

	if (pattern->type == CAIRO_PATTERN_LINEAR)
	{
	    cairo_linear_pattern_t *grad = (cairo_linear_pattern_t *) pattern;
	    
	    params[0] = _cairo_fixed_from_double (grad->point0.x);
	    params[1] = _cairo_fixed_from_double (grad->point0.y);
	    params[2] = _cairo_fixed_from_double (grad->point1.x);
	    params[3] = _cairo_fixed_from_double (grad->point1.y);
	    attr->filter = GLITZ_FILTER_LINEAR_GRADIENT;
	}
	else
	{
	    cairo_radial_pattern_t *grad = (cairo_radial_pattern_t *) pattern;
	    
	    params[0] = _cairo_fixed_from_double (grad->center0.x);
	    params[1] = _cairo_fixed_from_double (grad->center0.y);
	    params[2] = _cairo_fixed_from_double (grad->radius0);
	    params[3] = _cairo_fixed_from_double (grad->radius1);
	    attr->filter = GLITZ_FILTER_RADIAL_GRADIENT;
	}

	switch (pattern->extend) {
	case CAIRO_EXTEND_NONE:
	    attr->fill = GLITZ_FILL_NEAREST;
	    break;
	case CAIRO_EXTEND_REPEAT:
	    attr->fill = GLITZ_FILL_REPEAT;
	    break;
	case CAIRO_EXTEND_REFLECT:
	    attr->fill = GLITZ_FILL_REFLECT;
	    break;
	}

	attr->params	    = params;
	attr->n_params	    = n_params;
	attr->base.matrix   = pattern->matrix;
	attr->base.x_offset = 0;
	attr->base.y_offset = 0;
    } break;
    default:
	break;
    }

    if (!src)
    {
	cairo_int_status_t status;

	status = _cairo_pattern_acquire_surface (pattern, &dst->base,
						 x, y, width, height,
						 (cairo_surface_t **) &src,
						 &attr->base);
	if (status)
	    return status;
	
	if (src)
	{
	    switch (attr->base.extend) {
	    case CAIRO_EXTEND_NONE:
		attr->fill = GLITZ_FILL_TRANSPARENT;
		break;
	    case CAIRO_EXTEND_REPEAT:
		attr->fill = GLITZ_FILL_REPEAT;
		break;
	    case CAIRO_EXTEND_REFLECT:
		attr->fill = GLITZ_FILL_REFLECT;
		break;
	    }

	    switch (attr->base.filter) {
	    case CAIRO_FILTER_FAST:
	    case CAIRO_FILTER_NEAREST:
		attr->filter = GLITZ_FILTER_NEAREST;
		break;
	    case CAIRO_FILTER_GOOD:
	    case CAIRO_FILTER_BEST:
	    case CAIRO_FILTER_BILINEAR:
	    default:
		attr->filter = GLITZ_FILTER_BILINEAR;
		break;
	    }
	    
	    attr->params   = NULL;
	    attr->n_params = 0;
	    attr->acquired = TRUE;
	}
    }

    *surface_out = src;
    
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_glitz_pattern_release_surface (cairo_glitz_surface_t	       *dst,
				      cairo_glitz_surface_t	      *surface,
				      cairo_glitz_surface_attributes_t *attr)
{
    if (attr->acquired)
	_cairo_pattern_release_surface (&dst->base, &surface->base,
					&attr->base);
    else
	_cairo_glitz_surface_destroy (surface);
}

static cairo_int_status_t
_cairo_glitz_pattern_acquire_surfaces (cairo_pattern_t	                *src,
				       cairo_pattern_t	                *mask,
				       cairo_glitz_surface_t	        *dst,
				       int			        src_x,
				       int			        src_y,
				       int			        mask_x,
				       int			        mask_y,
				       unsigned int		        width,
				       unsigned int		        height,
				       cairo_glitz_surface_t	    **src_out,
				       cairo_glitz_surface_t	    **mask_out,
				       cairo_glitz_surface_attributes_t *sattr,
				       cairo_glitz_surface_attributes_t *mattr)
{
    cairo_int_status_t	  status;
    cairo_pattern_union_t tmp;
    cairo_bool_t          src_opaque, mask_opaque;
    double		  src_alpha, mask_alpha;

    src_opaque = _cairo_pattern_is_opaque (src);
    mask_opaque = !mask || _cairo_pattern_is_opaque (mask);
    
    /* For surface patterns, we move any translucency from src->alpha
     * to mask->alpha so we can use the source unchanged. Otherwise we
     * move the translucency from mask->alpha to src->alpha so that
     * we can drop the mask if possible.
     */
    if (src->type == CAIRO_PATTERN_SURFACE)
    {
	if (mask) {
	    mask_opaque = mask_opaque && src_opaque;
	    mask_alpha = mask->alpha * src->alpha;
	} else {
	    mask_opaque = src_opaque;
	    mask_alpha = src->alpha;
	}
	
	src_alpha = 1.0;
	src_opaque = TRUE;
    }
    else
    {
	if (mask)
	{
	    src_opaque = mask_opaque && src_opaque;
	    src_alpha = mask->alpha * src->alpha;
	    /* FIXME: This needs changing when we support RENDER
	     * style 4-channel masks.
	     */
	    if (mask->type == CAIRO_PATTERN_SOLID)
		mask = NULL;
	} else
	    src_alpha = src->alpha;

	mask_alpha = 1.0;
	mask_opaque = TRUE;
    }

    _cairo_pattern_init_copy (&tmp.base, src);
    _cairo_pattern_set_alpha (&tmp.base, src_alpha);
	
    status = _cairo_glitz_pattern_acquire_surface (&tmp.base, dst,
						   src_x, src_y,
						   width, height,
						   src_out, sattr);
    
    _cairo_pattern_fini (&tmp.base);

    if (status)
	return status;

    if (mask || !mask_opaque)
    {
	if (mask)
	    _cairo_pattern_init_copy (&tmp.base, mask);
	else
	    _cairo_pattern_init_solid (&tmp.solid, 0.0, 0.0, 0.0);
	
	_cairo_pattern_set_alpha (&tmp.base, mask_alpha);
	
	status = _cairo_glitz_pattern_acquire_surface (&tmp.base, dst,
						       mask_x, mask_y,
						       width, height,
						       mask_out, mattr);
    
	_cairo_pattern_fini (&tmp.base);

	if (status)
	{
	    _cairo_glitz_pattern_release_surface (dst, *src_out, sattr);
	    return status;
	}
    }
    else
    {
	*mask_out = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_glitz_surface_set_attributes (cairo_glitz_surface_t	      *surface,
				     cairo_glitz_surface_attributes_t *a)
{
    _cairo_glitz_surface_set_matrix (surface, &a->base.matrix);
    glitz_surface_set_fill (surface->surface, a->fill);
    glitz_surface_set_filter (surface->surface, a->filter,
			      a->params, a->n_params);
}

static cairo_int_status_t
_cairo_glitz_surface_composite (cairo_operator_t op,
				cairo_pattern_t  *src_pattern,
				cairo_pattern_t  *mask_pattern,
				void		 *abstract_dst,
				int		 src_x,
				int		 src_y,
				int		 mask_x,
				int		 mask_y,
				int		 dst_x,
				int		 dst_y,
				unsigned int	 width,
				unsigned int	 height)
{
    cairo_glitz_surface_attributes_t	src_attr, mask_attr;
    cairo_glitz_surface_t		*dst = abstract_dst;
    cairo_glitz_surface_t		*src;
    cairo_glitz_surface_t		*mask;
    cairo_int_status_t			status;

    if (op == CAIRO_OPERATOR_SATURATE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (_glitz_ensure_target (dst->surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_glitz_pattern_acquire_surfaces (src_pattern, mask_pattern,
						    dst,
						    src_x, src_y,
						    mask_x, mask_y,
						    width, height,
						    &src, &mask,
						    &src_attr, &mask_attr);
    if (status)
	return status;

    _cairo_glitz_surface_set_attributes (src, &src_attr);
    if (mask)
    {
	_cairo_glitz_surface_set_attributes (mask, &mask_attr);
	glitz_composite (_glitz_operator (op),
			 src->surface,
			 mask->surface,
			 dst->surface,
			 src_x + src_attr.base.x_offset,
			 src_y + src_attr.base.y_offset,
			 mask_x + mask_attr.base.x_offset,
			 mask_y + mask_attr.base.y_offset,
			 dst_x, dst_y,
			 width, height);
	
	if (mask_attr.n_params)
	    free (mask_attr.params);
	
	_cairo_glitz_pattern_release_surface (dst, mask, &mask_attr);
    }
    else
    {    
	glitz_composite (_glitz_operator (op),
			 src->surface,
			 NULL,
			 dst->surface,
			 src_x + src_attr.base.x_offset,
			 src_y + src_attr.base.y_offset,
			 0, 0,
			 dst_x, dst_y,
			 width, height);
    }

    if (src_attr.n_params)
	free (src_attr.params);

    _cairo_glitz_pattern_release_surface (dst, src, &src_attr);

    if (glitz_surface_get_status (dst->surface) == GLITZ_STATUS_NOT_SUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_glitz_surface_fill_rectangles (void		  *abstract_dst,
				      cairo_operator_t	  op,
				      const cairo_color_t *color,
				      cairo_rectangle_t	  *rects,
				      int		  n_rects)
{
    cairo_glitz_surface_t *dst = abstract_dst;

    if (op == CAIRO_OPERATOR_SRC)
    {
	glitz_color_t glitz_color;

	glitz_color.red = color->red_short;
	glitz_color.green = color->green_short;
	glitz_color.blue = color->blue_short;
	glitz_color.alpha = color->alpha_short;

	if (glitz_surface_get_width (dst->surface) != 1 ||
	    glitz_surface_get_height (dst->surface) != 1)
	    _glitz_ensure_target (dst->surface);
	
	glitz_set_rectangles (dst->surface, &glitz_color,
			      (glitz_rectangle_t *) rects, n_rects);
    }
    else
    {
	cairo_glitz_surface_t *src;
	
	if (op == CAIRO_OPERATOR_SATURATE)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (_glitz_ensure_target (dst->surface))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	src = (cairo_glitz_surface_t *)
	    _cairo_surface_create_similar_solid (&dst->base,
						 CAIRO_FORMAT_ARGB32, 1, 1,
						 (cairo_color_t *) color);
	if (!src)
	    return CAIRO_STATUS_NO_MEMORY;
	
	while (n_rects--)
	{
	    glitz_composite (_glitz_operator (op),
			     src->surface,
			     NULL,
			     dst->surface,
			     0, 0,
			     0, 0,
			     rects->x, rects->y,
			     rects->width, rects->height);
	    rects++;
	}
	
	cairo_surface_destroy (&src->base);
    }

    if (glitz_surface_get_status (dst->surface) == GLITZ_STATUS_NOT_SUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_glitz_surface_composite_trapezoids (cairo_operator_t  op,
					   cairo_pattern_t   *pattern,
					   void		     *abstract_dst,
					   int		     src_x,
					   int		     src_y,
					   int		     dst_x,
					   int		     dst_y,
					   unsigned int	     width,
					   unsigned int	     height,
					   cairo_trapezoid_t *traps,
					   int		     n_traps)
{
    cairo_glitz_surface_attributes_t attributes;
    cairo_glitz_surface_t	     *dst = abstract_dst;
    cairo_glitz_surface_t	     *src;
    cairo_glitz_surface_t	     *mask = NULL;
    glitz_buffer_t		     *buffer = NULL;
    void			     *data = NULL;
    cairo_int_status_t		     status;
    unsigned short		     alpha;

    if (op == CAIRO_OPERATOR_SATURATE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (_glitz_ensure_target (dst->surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (pattern->type == CAIRO_PATTERN_SURFACE)
    {
	cairo_pattern_union_t tmp;

	_cairo_pattern_init_copy (&tmp.base, pattern);
	_cairo_pattern_set_alpha (&tmp.base, 1.0);
	
	status = _cairo_glitz_pattern_acquire_surface (&tmp.base, dst,
						       src_x, src_y,
						       width, height,
						       &src, &attributes);

	_cairo_pattern_fini (&tmp.base);
	
	alpha = pattern->alpha * 0xffff;
    }
    else
    {
	status = _cairo_glitz_pattern_acquire_surface (pattern, dst,
						       src_x, src_y,
						       width, height,
						       &src, &attributes);
	alpha = 0xffff;
    }

    if (status)
	return status;

    if (op == CAIRO_OPERATOR_ADD || n_traps <= 1)
    {
	static glitz_color_t	clear_black = { 0, 0, 0, 0 };
	glitz_color_t		color;
	glitz_geometry_format_t	format;
	int			n_trap_added;
	int			offset = 0;
	int			data_size = 0;
	int			size = 30 * n_traps; /* just a guess */

	format.vertex.primitive = GLITZ_PRIMITIVE_QUADS;
	format.vertex.type = GLITZ_DATA_TYPE_FLOAT;
	format.vertex.bytes_per_vertex = 3 * sizeof (glitz_float_t);
	format.vertex.attributes = GLITZ_VERTEX_ATTRIBUTE_MASK_COORD_MASK;
	format.vertex.mask.type = GLITZ_DATA_TYPE_FLOAT;
	format.vertex.mask.size = GLITZ_COORDINATE_SIZE_X;
	format.vertex.mask.offset = 2 * sizeof (glitz_float_t);

	mask = (cairo_glitz_surface_t *)
	    _cairo_glitz_surface_create_similar (&dst->base,
						 CAIRO_FORMAT_A8, 0,
						 2, 1);
	if (!mask)
	{
	    _cairo_glitz_pattern_release_surface (dst, src, &attributes);
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	color.red = color.green = color.blue = color.alpha = alpha;

	glitz_set_rectangle (mask->surface, &clear_black, 0, 0, 1, 1);
	glitz_set_rectangle (mask->surface, &color, 1, 0, 1, 1);

	glitz_surface_set_fill (mask->surface, GLITZ_FILL_NEAREST);
	glitz_surface_set_filter (mask->surface,
				  GLITZ_FILTER_BILINEAR,
				  NULL, 0);
	
	size *= format.vertex.bytes_per_vertex;
	
	while (n_traps)
	{
	    if (data_size < size)
	    {
		data_size = size;
		data = realloc (data, data_size);
		if (!data)
		{
		    _cairo_glitz_pattern_release_surface (dst, src,
							  &attributes);
		    return CAIRO_STATUS_NO_MEMORY;
		}

		if (buffer)
		    glitz_buffer_destroy (buffer);
		
		buffer = glitz_buffer_create_for_data (data);
		if (!buffer) {
		    free (data);
		    _cairo_glitz_pattern_release_surface (dst, src,
							  &attributes);
		    return CAIRO_STATUS_NO_MEMORY;
		}
	    }
    
	    offset +=
		glitz_add_trapezoids (buffer,
				      offset, size - offset,
				      format.vertex.type, mask->surface,
				      (glitz_trapezoid_t *) traps, n_traps,
				      &n_trap_added);
		
	    n_traps -= n_trap_added;
	    traps   += n_trap_added;
	    size    *= 2;
	}
    
	glitz_set_geometry (dst->surface,
			    GLITZ_GEOMETRY_TYPE_VERTEX,
			    &format, buffer);
	glitz_set_array (dst->surface, 0, 3,
			 offset / format.vertex.bytes_per_vertex,
			 0, 0);
    }
    else
    {
	cairo_image_surface_t *image;
	char		      *ptr;
	int		      stride;

	stride = (width + 3) & -4;
	data = malloc (stride * height);
	if (!data)
	{
	    _cairo_glitz_pattern_release_surface (dst, src, &attributes);
	    return CAIRO_STATUS_NO_MEMORY;
	}

	memset (data, 0, stride * height);

	/* using negative stride */
	ptr = (char *) data + stride * (height - 1);
	
	image = (cairo_image_surface_t *)
	    cairo_image_surface_create_for_data (ptr,
						 CAIRO_FORMAT_A8,
						 width, height,
						 -stride);
	if (!image)
	{
	    cairo_surface_destroy (&src->base);
	    free (data);
	    return CAIRO_STATUS_NO_MEMORY;
	}

	pixman_add_trapezoids (image->pixman_image, -dst_x, -dst_y,
			       (pixman_trapezoid_t *) traps, n_traps);

 	if (alpha != 0xffff)
 	{
 	    pixman_color_t color;
 	    
 	    color.red = color.green = color.blue = color.alpha = alpha;
 
 	    pixman_fill_rectangle (PIXMAN_OPERATOR_IN,
 				   image->pixman_image,
 				   &color,
 				   0, 0, width, height);
 	}
	
	mask = (cairo_glitz_surface_t *)
	    _cairo_surface_create_similar_scratch (&dst->base,
						   CAIRO_FORMAT_A8, 0,
						   width, height);
	if (!mask)
	{
	    _cairo_glitz_pattern_release_surface (dst, src, &attributes);
	    free (data);
	    cairo_surface_destroy (&image->base);
	    return CAIRO_STATUS_NO_MEMORY;
	}

	_cairo_glitz_surface_set_image (mask, image, 0, 0);
    }

    _cairo_glitz_surface_set_attributes (src, &attributes);
    
    glitz_composite (_glitz_operator (op),
		     src->surface,
		     mask->surface,
		     dst->surface,
		     src_x + attributes.base.x_offset,
		     src_y + attributes.base.y_offset,
		     0, 0,
		     dst_x, dst_y,
		     width, height);

    if (attributes.n_params)
	free (attributes.params);

    glitz_set_geometry (dst->surface,
			GLITZ_GEOMETRY_TYPE_NONE,
			NULL, NULL);

    if (buffer)
	glitz_buffer_destroy (buffer);
    
    free (data);

    _cairo_glitz_pattern_release_surface (dst, src, &attributes);
    if (mask)
	cairo_surface_destroy (&mask->base);

    if (glitz_surface_get_status (dst->surface) == GLITZ_STATUS_NOT_SUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_glitz_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_glitz_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_glitz_surface_set_clip_region (void		*abstract_surface,
				      pixman_region16_t *region)
{
    cairo_glitz_surface_t *surface = abstract_surface;

    if (region)
    {
	glitz_box_t *box;
	int	    n;
	
	if (!surface->clip)
	{
	    surface->clip = pixman_region_create ();
	    if (!surface->clip)
		return CAIRO_STATUS_NO_MEMORY;
	}
	pixman_region_copy (surface->clip, region);

	box = (glitz_box_t *) pixman_region_rects (surface->clip);
	n = pixman_region_num_rects (surface->clip);
	glitz_surface_set_clip_region (surface->surface, 0, 0, box, n);
    }
    else
    {
	glitz_surface_set_clip_region (surface->surface, 0, 0, NULL, 0);
	
	if (surface->clip)
	    pixman_region_destroy (surface->clip);

	surface->clip = NULL;
    }
    
    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_glitz_surface_backend = {
    _cairo_glitz_surface_create_similar,
    _cairo_glitz_surface_destroy,
    _cairo_glitz_surface_pixels_per_inch,
    _cairo_glitz_surface_acquire_source_image,
    _cairo_glitz_surface_release_source_image,
    _cairo_glitz_surface_acquire_dest_image,
    _cairo_glitz_surface_release_dest_image,
    _cairo_glitz_surface_clone_similar,
    _cairo_glitz_surface_composite,
    _cairo_glitz_surface_fill_rectangles,
    _cairo_glitz_surface_composite_trapezoids,
    _cairo_glitz_surface_copy_page,
    _cairo_glitz_surface_show_page,
    _cairo_glitz_surface_set_clip_region,
    NULL /* show_glyphs */
};

cairo_surface_t *
cairo_glitz_surface_create (glitz_surface_t *surface)
{
    cairo_glitz_surface_t *crsurface;

    if (!surface)
	return NULL;

    crsurface = malloc (sizeof (cairo_glitz_surface_t));
    if (crsurface == NULL)
	return NULL;

    _cairo_surface_init (&crsurface->base, &cairo_glitz_surface_backend);

    glitz_surface_reference (surface);
    
    crsurface->surface = surface;
    crsurface->format  = glitz_surface_get_format (surface);
    crsurface->clip    = NULL;
    
    return (cairo_surface_t *) crsurface;
}
