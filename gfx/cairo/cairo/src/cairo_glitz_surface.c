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
 * Author: David Reveman <c99drn@cs.umu.se>
 */

#include "cairoint.h"
#include "cairo-glitz.h"

#define GLITZ_FIXED_TO_FLOAT(f) \
  (((glitz_float_t) (f)) / 65536)

#define GLITZ_FIXED_LINE_X_TO_FLOAT(line, v) \
  (((glitz_float_t) \
      ((line).p1.x + (cairo_fixed_16_16_t) \
       (((cairo_fixed_32_32_t) ((v) - (line).p1.y) * \
        ((line).p2.x - (line).p1.x)) / \
	((line).p2.y - (line).p1.y)))) / 65536)

#define GLITZ_FIXED_LINE_X_CEIL_TO_FLOAT(line, v) \
  (((glitz_float_t) \
      ((line).p1.x + (cairo_fixed_16_16_t) \
       (((((line).p2.y - (line).p1.y) - 1) + \
         ((cairo_fixed_32_32_t) ((v) - (line).p1.y) * \
          ((line).p2.x - (line).p1.x))) / \
	((line).p2.y - (line).p1.y)))) / 65536)

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
    cairo_surface_t base;

    glitz_surface_t *surface;
    glitz_format_t *format;

    cairo_pattern_t pattern;
    cairo_box_t pattern_box;
} cairo_glitz_surface_t;

static void
_cairo_glitz_surface_destroy (void *abstract_surface)
{
    cairo_glitz_surface_t *surface = abstract_surface;

    glitz_surface_destroy (surface->surface);

    _cairo_pattern_fini (&surface->pattern);

    free (surface);
}

static double
_cairo_glitz_surface_pixels_per_inch (void *abstract_surface)
{
    return 96.0;
}

static cairo_image_surface_t *
_cairo_glitz_surface_get_image (void *abstract_surface)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    char *pixels;
    int width, height;
    cairo_format_masks_t format;
    glitz_buffer_t *buffer;
    glitz_pixel_format_t pf;

    if (surface->pattern.type != CAIRO_PATTERN_SURFACE) {
	cairo_box_t box;

	box.p1.x = box.p1.y = 0;
	box.p2.x = surface->pattern_box.p2.x;
	box.p2.y = surface->pattern_box.p2.y;
	
	return _cairo_pattern_get_image (&surface->pattern, &box);
    }


    width = glitz_surface_get_width (surface->surface);
    height = glitz_surface_get_height (surface->surface);

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
    pf.bytes_per_line = (((width * format.bpp) / 8) + 3) & -4;
    pf.scanline_order = GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN;

    pixels = malloc (height * pf.bytes_per_line);
    if (!pixels)
	return NULL;

    buffer = glitz_buffer_create_for_data (pixels);
    if (!buffer) {
	free (pixels);
	return NULL;
    }
    
    glitz_get_pixels (surface->surface,
		      0, 0,
		      width, height,
		      &pf,
		      buffer);

    glitz_buffer_destroy (buffer);
    
    image = (cairo_image_surface_t *)
        _cairo_image_surface_create_with_masks (pixels,
						&format,
						width, height,
						pf.bytes_per_line);
    
    _cairo_image_surface_assume_ownership_of_data (image);

    _cairo_image_surface_set_repeat (image, surface->base.repeat);
    _cairo_image_surface_set_matrix (image, &(surface->base.matrix));

    return image;
}

static cairo_status_t
_cairo_glitz_surface_set_image (void *abstract_surface,
				cairo_image_surface_t *image)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    glitz_buffer_t *buffer;
    glitz_pixel_format_t pf;
    pixman_format_t *format;
    int am, rm, gm, bm;
    
    format = pixman_image_get_format (image->pixman_image);
    if (format == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    pixman_format_get_masks (format, &pf.masks.bpp, &am, &rm, &gm, &bm);

    pf.masks.alpha_mask = am;
    pf.masks.red_mask = rm;
    pf.masks.green_mask = gm;
    pf.masks.blue_mask = bm;
    pf.xoffset = 0;
    pf.skip_lines = 0;
    pf.bytes_per_line = image->stride;
    pf.scanline_order = GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN;

    buffer = glitz_buffer_create_for_data (image->data);
    if (!buffer)
	return CAIRO_STATUS_NO_MEMORY;
    
    glitz_set_pixels (surface->surface,
		      0, 0,
		      image->width, image->height,
		      &pf,
		      buffer);
    
    glitz_buffer_destroy (buffer);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_glitz_surface_set_matrix (void *abstract_surface,
				 cairo_matrix_t *matrix)
{
    cairo_glitz_surface_t *surface = abstract_surface;
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

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_glitz_surface_set_filter (void *abstract_surface, cairo_filter_t filter)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    glitz_filter_t glitz_filter;

    switch (filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	glitz_filter = GLITZ_FILTER_NEAREST;
	break;
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
    default:
	glitz_filter = GLITZ_FILTER_BILINEAR;
	break;
    }

    glitz_surface_set_filter (surface->surface, glitz_filter, NULL, 0);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_glitz_surface_set_repeat (void *abstract_surface, int repeat)
{
    cairo_glitz_surface_t *surface = abstract_surface;

    glitz_surface_set_fill (surface->surface,
			    (repeat)? GLITZ_FILL_REPEAT:
			    GLITZ_FILL_TRANSPARENT);

    return CAIRO_STATUS_SUCCESS;
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

static glitz_surface_t *
_glitz_surface_create_solid (glitz_surface_t *other,
			     glitz_format_name_t format_name,
			     glitz_color_t *color)
{
    glitz_drawable_t *drawable;
    glitz_format_t *format;
    glitz_surface_t *surface;

    drawable = glitz_surface_get_drawable (other);
    
    format = glitz_find_standard_format (drawable, format_name);
    if (format == NULL)
	return NULL;
    
    surface = glitz_surface_create (drawable, format, 1, 1);
    if (surface == NULL)
	return NULL;

    glitz_set_rectangle (surface, color, 0, 0, 1, 1);
    
    glitz_surface_set_fill (surface, GLITZ_FILL_REPEAT);
    
    return surface;
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
	glitz_pbuffer_attributes_t attributes;
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

	attributes.width = glitz_surface_get_width (surface);
	attributes.height = glitz_surface_get_height (surface);
	mask = GLITZ_PBUFFER_WIDTH_MASK | GLITZ_PBUFFER_HEIGHT_MASK;

	pbuffer = glitz_create_pbuffer_drawable (drawable, dformat,
						 &attributes, mask);
	if (!pbuffer)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (glitz_drawable_get_width (pbuffer) < attributes.width ||
	    glitz_drawable_get_height (pbuffer) < attributes.height) {
	    glitz_drawable_destroy (pbuffer);
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	glitz_surface_attach (surface, pbuffer,
			      GLITZ_DRAWABLE_BUFFER_FRONT_COLOR,
			      0, 0);

	glitz_drawable_destroy (pbuffer);
    }

    return CAIRO_STATUS_SUCCESS;
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
_cairo_glitz_surface_create_similar (void *abstract_src,
				     cairo_format_t format,
				     int draw,
				     int width,
				     int height)
{
    cairo_glitz_surface_t *src = abstract_src;
    cairo_surface_t *crsurface;
    glitz_drawable_t *drawable;
    glitz_surface_t *surface;
    glitz_format_t *gformat;

    drawable = glitz_surface_get_drawable (src->surface);
    
    gformat = glitz_find_standard_format (drawable, _glitz_format (format));
    if (gformat == NULL)
	return NULL;
    
    surface = glitz_surface_create (drawable, gformat, width, height);
    if (surface == NULL)
	return NULL;

    crsurface = cairo_glitz_surface_create (surface);
    
    glitz_surface_destroy (surface);

    return crsurface;
}

static cairo_glitz_surface_t *
_cairo_glitz_surface_clone_similar (cairo_glitz_surface_t *templ,
				    cairo_surface_t *src,
				    cairo_format_t format)
{
    cairo_glitz_surface_t *clone;
    cairo_image_surface_t *src_image;

    src_image = _cairo_surface_get_image (src);

    clone = (cairo_glitz_surface_t *)
        _cairo_glitz_surface_create_similar (templ, format, 0,
					     src_image->width,
					     src_image->height);
    if (clone == NULL)
	return NULL;
    
    _cairo_glitz_surface_set_filter (clone, cairo_surface_get_filter (src));
    
    _cairo_glitz_surface_set_image (clone, src_image);
    
    _cairo_glitz_surface_set_matrix (clone, &(src_image->base.matrix));
    
    cairo_surface_destroy (&src_image->base);

    return clone;
}

static cairo_int_status_t
_glitz_composite (glitz_operator_t op,
		  glitz_surface_t *src,
		  glitz_surface_t *mask,
		  glitz_surface_t *dst,
		  int src_x,
		  int src_y,
		  int mask_x,
		  int mask_y,
		  int dst_x,
		  int dst_y,
		  int width,
		  int height,
		  glitz_buffer_t *geometry,
		  glitz_geometry_format_t *format)
{
    if (_glitz_ensure_target (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;
	
    if (glitz_surface_get_status (dst))
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    glitz_set_geometry (dst,
			0, 0,
			format, geometry);

    glitz_composite (op,
		     src,
		     mask,
		     dst,
		     src_x, src_y,
		     mask_x, mask_y,
		     dst_x, dst_y,
		     width, height);

    glitz_set_geometry (dst, 0, 0, NULL, NULL);

    if (glitz_surface_get_status (dst) == GLITZ_STATUS_NOT_SUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_glitz_surface_composite (cairo_operator_t op,
				cairo_surface_t *generic_src,
				cairo_surface_t *generic_mask,
				void *abstract_dst,
				int src_x,
				int src_y,
				int mask_x,
				int mask_y,
				int dst_x,
				int dst_y,
				unsigned int width,
				unsigned int height)
{
    cairo_glitz_surface_t *dst = abstract_dst;
    cairo_glitz_surface_t *src = (cairo_glitz_surface_t *) generic_src;
    cairo_glitz_surface_t *mask = (cairo_glitz_surface_t *) generic_mask;
    cairo_glitz_surface_t *src_clone = NULL;
    cairo_glitz_surface_t *mask_clone = NULL;
    cairo_int_status_t status;

    if (op == CAIRO_OPERATOR_SATURATE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (generic_src->backend != dst->base.backend) {
	src_clone = _cairo_glitz_surface_clone_similar (dst, generic_src,
							CAIRO_FORMAT_ARGB32);
	if (!src_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	
	src = src_clone;
    }
    
    if (generic_mask && (generic_mask->backend != dst->base.backend)) {
	mask_clone = _cairo_glitz_surface_clone_similar (dst, generic_mask,
							 CAIRO_FORMAT_A8);
	if (!mask_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	
	mask = mask_clone;
    }

    status = _glitz_composite (_glitz_operator (op),
			       src->surface,
			       (mask)? mask->surface: NULL,
			       dst->surface,
			       src_x, src_y,
			       mask_x, mask_y,
			       dst_x, dst_y,
			       width, height,
			       NULL, NULL);

    if (src_clone)
	cairo_surface_destroy (&src_clone->base);
    
    if (mask_clone)
	cairo_surface_destroy (&mask_clone->base);

    return status;
}

static cairo_int_status_t
_cairo_glitz_surface_fill_rectangles (void *abstract_dst,
				      cairo_operator_t op,
				      const cairo_color_t *color,
				      cairo_rectangle_t *rects,
				      int n_rects)
{
    cairo_glitz_surface_t *dst = abstract_dst;
    glitz_color_t glitz_color;

    if (op == CAIRO_OPERATOR_SATURATE)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    
    glitz_color.red = color->red_short;
    glitz_color.green = color->green_short;
    glitz_color.blue = color->blue_short;
    glitz_color.alpha = color->alpha_short;
	
    if (op != CAIRO_OPERATOR_SRC) {
	glitz_surface_t *solid;
	glitz_float_t *vertices;
	glitz_buffer_t *buffer;
	glitz_geometry_format_t gf;
	cairo_int_status_t status;
	int width, height;
	void *data;
	
	gf.mode = GLITZ_GEOMETRY_MODE_DIRECT;
	gf.edge_hint = GLITZ_GEOMETRY_EDGE_HINT_SHARP;
	gf.primitive = GLITZ_GEOMETRY_PRIMITIVE_QUADS;
	gf.type = GLITZ_DATA_TYPE_FLOAT;
	gf.first = 0;
	gf.count = n_rects * 4;

	data = malloc (n_rects * 8 * sizeof (glitz_float_t));
	if (!data)
	    return CAIRO_STATUS_NO_MEMORY;

	buffer = glitz_buffer_create_for_data (data);
	if (buffer == NULL) {
	    free (data);
	    return CAIRO_STATUS_NO_MEMORY;
	}

	width = height = 0;
	vertices = glitz_buffer_map (buffer, GLITZ_BUFFER_ACCESS_WRITE_ONLY);
	for (; n_rects; rects++, n_rects--) {
	    *vertices++ = (glitz_float_t) rects->x;
	    *vertices++ = (glitz_float_t) rects->y;
	    *vertices++ = (glitz_float_t) (rects->x + rects->width);
	    *vertices++ = (glitz_float_t) rects->y;
	    *vertices++ = (glitz_float_t) (rects->x + rects->width);
	    *vertices++ = (glitz_float_t) (rects->y + rects->height);
	    *vertices++ = (glitz_float_t) rects->x;
	    *vertices++ = (glitz_float_t) (rects->y + rects->height);

	    if ((rects->x + rects->width) > width)
		width = rects->x + rects->width;

	    if ((rects->y + rects->height) > height)
		height = rects->y + rects->height;
	}
	glitz_buffer_unmap (buffer);

	solid = _glitz_surface_create_solid (dst->surface,
					     GLITZ_STANDARD_ARGB32,
					     &glitz_color);
	if (solid == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	status = _glitz_composite (_glitz_operator (op),
				   solid,
				   NULL,
				   dst->surface,
				   0, 0,
				   0, 0,
				   0, 0,
				   width, height,
				   buffer, &gf);

	glitz_surface_destroy (solid);
	glitz_buffer_destroy (buffer);
	free (data);

	return status;
    } else {
	if (glitz_surface_get_width (dst->surface) != 1 ||
	    glitz_surface_get_height (dst->surface) != 1) {
	    if (_glitz_ensure_target (dst->surface))
		return CAIRO_INT_STATUS_UNSUPPORTED;
	}
	
	glitz_set_rectangles (dst->surface, &glitz_color,
			      (glitz_rectangle_t *) rects, n_rects);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_glitz_surface_composite_trapezoids (cairo_operator_t op,
					   cairo_surface_t *generic_src,
					   void *abstract_dst,
					   int x_src,
					   int y_src,
					   cairo_trapezoid_t *traps,
					   int n_traps)
{
    cairo_glitz_surface_t *dst = abstract_dst;
    cairo_glitz_surface_t *src = (cairo_glitz_surface_t *) generic_src;
    glitz_surface_t *mask = NULL;
    glitz_float_t *vertices;
    glitz_buffer_t *buffer;
    glitz_geometry_format_t gf;
    cairo_int_status_t status;
    int x_dst, y_dst, x_rel, y_rel, width, height;
    void *data;

    if (op == CAIRO_OPERATOR_SATURATE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (generic_src->backend != dst->base.backend)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    gf.mode = GLITZ_GEOMETRY_MODE_DIRECT;
    gf.edge_hint = GLITZ_GEOMETRY_EDGE_HINT_GOOD_SMOOTH;
    gf.primitive = GLITZ_GEOMETRY_PRIMITIVE_QUADS;
    gf.type = GLITZ_DATA_TYPE_FLOAT;
    gf.first = 0;
    gf.count = n_traps * 4;

    data = malloc (n_traps * 8 * sizeof (glitz_float_t));
    if (!data)
	return CAIRO_STATUS_NO_MEMORY;

    buffer = glitz_buffer_create_for_data (data);
    if (buffer == NULL) {
	free (data);
	return CAIRO_STATUS_NO_MEMORY;
    }
	
    x_dst = traps[0].left.p1.x >> 16;
    y_dst = traps[0].left.p1.y >> 16;
	
    vertices = glitz_buffer_map (buffer, GLITZ_BUFFER_ACCESS_WRITE_ONLY);
    for (; n_traps; traps++, n_traps--) {
	glitz_float_t top, bottom;

	top = GLITZ_FIXED_TO_FLOAT (traps->top);
	bottom = GLITZ_FIXED_TO_FLOAT (traps->bottom);
	
	*vertices++ = GLITZ_FIXED_LINE_X_TO_FLOAT (traps->left, traps->top);
	*vertices++ = top;
	*vertices++ =
	    GLITZ_FIXED_LINE_X_CEIL_TO_FLOAT (traps->right, traps->top);
	*vertices++ = top;
	*vertices++ =
	    GLITZ_FIXED_LINE_X_CEIL_TO_FLOAT (traps->right, traps->bottom);
	*vertices++ = bottom;
	*vertices++ = GLITZ_FIXED_LINE_X_TO_FLOAT (traps->left, traps->bottom);
	*vertices++ = bottom;
    }
    glitz_buffer_unmap (buffer);

    if ((src->pattern.type == CAIRO_PATTERN_SURFACE) &&
	(src->pattern.color.alpha != 1.0)) {
	glitz_color_t color;
	
	color.red = color.green = color.blue = 0;
	color.alpha = src->pattern.color.alpha_short;
    
	mask = _glitz_surface_create_solid (dst->surface,
					    GLITZ_STANDARD_A8,
					    &color);
    }

    x_rel = (src->pattern_box.p1.x >> 16) + x_src - x_dst;
    y_rel = (src->pattern_box.p1.y >> 16) + y_src - y_dst;

    x_dst = src->pattern_box.p1.x >> 16;
    y_dst = src->pattern_box.p1.y >> 16;
    
    width = ((src->pattern_box.p2.x + 65535) >> 16) -
	(src->pattern_box.p1.x >> 16);
    height = ((src->pattern_box.p2.y + 65535) >> 16) -
	(src->pattern_box.p1.y >> 16);
    
    status = _glitz_composite (_glitz_operator (op),
			       src->surface,
			       mask,
			       dst->surface,
			       x_rel, y_rel,
			       0, 0,
			       x_dst, y_dst,
			       width, height,
			       buffer, &gf);

    if (mask)
	glitz_surface_destroy (mask);

    glitz_buffer_destroy (buffer);
    free (data);
    
    return status;
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
_cairo_glitz_surface_create_pattern (void *abstract_dst,
				     cairo_pattern_t *pattern,
				     cairo_box_t *box)
{
    cairo_glitz_surface_t *dst = abstract_dst;
    cairo_surface_t *generic_src = NULL;
    cairo_image_surface_t *image = NULL;
    cairo_glitz_surface_t *src;
    
    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID:
	generic_src =
	    _cairo_surface_create_similar_solid (abstract_dst,
						 CAIRO_FORMAT_ARGB32,
						 1, 1,
						 &pattern->color);
	if (generic_src)
	    cairo_surface_set_repeat (generic_src, 1);
	break;
    case CAIRO_PATTERN_RADIAL:
	/* glitz doesn't support inner and outer circle with different
	   center points. */
	if (pattern->u.radial.center0.x != pattern->u.radial.center1.x ||
	    pattern->u.radial.center0.y != pattern->u.radial.center1.y)
	    break;
	/* fall-through */
    case CAIRO_PATTERN_LINEAR: {
	glitz_drawable_t *drawable;
	glitz_fixed16_16_t *params;
	int i, n_params;

	drawable = glitz_surface_get_drawable (dst->surface);
	if (!(glitz_drawable_get_features (drawable) &
	      GLITZ_FEATURE_FRAGMENT_PROGRAM_MASK))
	    break;

	if (pattern->filter != CAIRO_FILTER_BILINEAR)
	    break;

	n_params = pattern->n_stops * 3 + 4;

	params = malloc (sizeof (glitz_fixed16_16_t) * n_params);
	if (params == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	generic_src =
	    _cairo_glitz_surface_create_similar (abstract_dst,
						 CAIRO_FORMAT_ARGB32, 0,
						 pattern->n_stops, 1);
	if (generic_src == NULL) {
	    free (params);
	    return CAIRO_STATUS_NO_MEMORY;
	}

	src = (cairo_glitz_surface_t *) generic_src;
	
	for (i = 0; i < pattern->n_stops; i++) {
	    glitz_color_t color;

	    color.alpha = pattern->stops[i].color_char[3];
	    color.red = pattern->stops[i].color_char[0] * color.alpha;
	    color.green = pattern->stops[i].color_char[1] * color.alpha;
	    color.blue = pattern->stops[i].color_char[2] * color.alpha;
	    color.alpha *= 256;
	
	    glitz_set_rectangle (src->surface, &color, i, 0, 1, 1);

	    params[4 + 3 * i] = pattern->stops[i].offset;
	    params[5 + 3 * i] = i << 16;
	    params[6 + 3 * i] = 0;
	}

	if (pattern->type == CAIRO_PATTERN_LINEAR) {
	    params[0] = _cairo_fixed_from_double (pattern->u.linear.point0.x);
	    params[1] = _cairo_fixed_from_double (pattern->u.linear.point0.y);
	    params[2] = _cairo_fixed_from_double (pattern->u.linear.point1.x);
	    params[3] = _cairo_fixed_from_double (pattern->u.linear.point1.y);

	    glitz_surface_set_filter (src->surface,
				      GLITZ_FILTER_LINEAR_GRADIENT,
				      params, n_params);	    
	} else {
	    params[0] = _cairo_fixed_from_double (pattern->u.radial.center0.x);
	    params[1] = _cairo_fixed_from_double (pattern->u.radial.center0.y);
	    params[2] = _cairo_fixed_from_double (pattern->u.radial.radius0);
	    params[3] = _cairo_fixed_from_double (pattern->u.radial.radius1);
	    
	    glitz_surface_set_filter (src->surface,
				      GLITZ_FILTER_RADIAL_GRADIENT,
				      params, n_params);
	}

	switch (pattern->extend) {
	case CAIRO_EXTEND_REPEAT:
	    glitz_surface_set_fill (src->surface, GLITZ_FILL_REPEAT);
	    break;
	case CAIRO_EXTEND_REFLECT:
	    glitz_surface_set_fill (src->surface, GLITZ_FILL_REFLECT);
	    break;
	case CAIRO_EXTEND_NONE:
	default:
	    glitz_surface_set_fill (src->surface, GLITZ_FILL_NEAREST);
	    break;
	}

	cairo_surface_set_matrix (&src->base, &pattern->matrix);

	free (params);
    } break;
    case CAIRO_PATTERN_SURFACE:
	generic_src = pattern->u.surface.surface;
	cairo_surface_reference (generic_src);
	break;
    }

    if (generic_src == NULL) {
	image = _cairo_pattern_get_image (pattern, box);
	if (image == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	
	generic_src = &image->base;
    }
	
    if (generic_src->backend != dst->base.backend) {
	src = _cairo_glitz_surface_clone_similar (dst, generic_src,
						  CAIRO_FORMAT_ARGB32);
	if (src == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	
	cairo_surface_set_repeat (&src->base, generic_src->repeat);
    } else
	src = (cairo_glitz_surface_t *) generic_src;

    if (image)
	cairo_surface_destroy (&image->base);

    _cairo_pattern_init_copy (&src->pattern, pattern);
    src->pattern_box = *box;
    
    pattern->source = &src->base;
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_glitz_surface_set_clip_region (void *abstract_surface,
				      pixman_region16_t *region)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static const cairo_surface_backend_t cairo_glitz_surface_backend = {
    _cairo_glitz_surface_create_similar,
    _cairo_glitz_surface_destroy,
    _cairo_glitz_surface_pixels_per_inch,
    _cairo_glitz_surface_get_image,
    _cairo_glitz_surface_set_image,
    _cairo_glitz_surface_set_matrix,
    _cairo_glitz_surface_set_filter,
    _cairo_glitz_surface_set_repeat,
    _cairo_glitz_surface_composite,
    _cairo_glitz_surface_fill_rectangles,
    _cairo_glitz_surface_composite_trapezoids,
    _cairo_glitz_surface_copy_page,
    _cairo_glitz_surface_show_page,
    _cairo_glitz_surface_set_clip_region,
    _cairo_glitz_surface_create_pattern,
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
    crsurface->format = glitz_surface_get_format (surface);
    
    _cairo_pattern_init (&crsurface->pattern);
    crsurface->pattern.type = CAIRO_PATTERN_SURFACE;
    crsurface->pattern.u.surface.surface = NULL;
    
    return (cairo_surface_t *) crsurface;
}
