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

#define MULTIPLY_COLORCOMP(c1, c2) \
    ((unsigned char) \
     ((((unsigned char) (c1)) * (int) ((unsigned char) (c2))) / 0xff))

void
_cairo_pattern_init (cairo_pattern_t *pattern)
{
    pattern->ref_count = 1;

    pattern->extend = CAIRO_EXTEND_DEFAULT;
    pattern->filter = CAIRO_FILTER_DEFAULT;

    _cairo_color_init (&pattern->color);

    _cairo_matrix_init (&pattern->matrix);

    pattern->stops = NULL;
    pattern->n_stops = 0;

    pattern->type = CAIRO_PATTERN_SOLID;

    pattern->source = NULL;
    pattern->source_offset.x = 0.0;
    pattern->source_offset.y = 0.0;
}

cairo_status_t
_cairo_pattern_init_copy (cairo_pattern_t *pattern, cairo_pattern_t *other)
{
    *pattern = *other;

    pattern->ref_count = 1;

    if (pattern->n_stops) {
	pattern->stops =
	    malloc (sizeof (cairo_color_stop_t) * pattern->n_stops);
	if (pattern->stops == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	memcpy (pattern->stops, other->stops,
		sizeof (cairo_color_stop_t) * other->n_stops);
    }

    if (pattern->source)
	cairo_surface_reference (other->source);

    if (pattern->type == CAIRO_PATTERN_SURFACE)
	cairo_surface_reference (other->u.surface.surface);
    
    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_fini (cairo_pattern_t *pattern)
{
    if (pattern->n_stops)
	free (pattern->stops);
    
    if (pattern->type == CAIRO_PATTERN_SURFACE) {
	/* show_surface require us to restore surface matrix, repeat
	   attribute, filter type */
	if (pattern->source) {
	    cairo_surface_set_matrix (pattern->source,
				      &pattern->u.surface.save_matrix);
	    cairo_surface_set_repeat (pattern->source,
				      pattern->u.surface.save_repeat);
	    cairo_surface_set_filter (pattern->source,
				      pattern->u.surface.save_filter);
	}
	cairo_surface_destroy (pattern->u.surface.surface);
    }
    
    if (pattern->source)
	cairo_surface_destroy (pattern->source);
}

void
_cairo_pattern_init_solid (cairo_pattern_t *pattern,
			   double red, double green, double blue)
{
    _cairo_pattern_init (pattern);

    pattern->type = CAIRO_PATTERN_SOLID;
    _cairo_color_set_rgb (&pattern->color, red, green, blue);
}

cairo_pattern_t *
_cairo_pattern_create_solid (double red, double green, double blue)
{
    cairo_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init_solid (pattern, red, green, blue);

    return pattern;
}

cairo_pattern_t *
cairo_pattern_create_for_surface (cairo_surface_t *surface)
{
    cairo_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init (pattern);
    
    pattern->type = CAIRO_PATTERN_SURFACE;
    pattern->u.surface.surface = surface;
    cairo_surface_reference (surface);

    return pattern;
}

cairo_pattern_t *
cairo_pattern_create_linear (double x0, double y0, double x1, double y1)
{
    cairo_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init (pattern);

    pattern->type = CAIRO_PATTERN_LINEAR;
    pattern->u.linear.point0.x = x0;
    pattern->u.linear.point0.y = y0;
    pattern->u.linear.point1.x = x1;
    pattern->u.linear.point1.y = y1;

    return pattern;
}

cairo_pattern_t *
cairo_pattern_create_radial (double cx0, double cy0, double radius0,
			     double cx1, double cy1, double radius1)
{
    cairo_pattern_t *pattern;
    
    pattern = malloc (sizeof (cairo_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init (pattern);
    
    pattern->type = CAIRO_PATTERN_RADIAL;
    pattern->u.radial.center0.x = cx0;
    pattern->u.radial.center0.y = cy0;
    pattern->u.radial.radius0 = fabs (radius0);
    pattern->u.radial.center1.x = cx1;
    pattern->u.radial.center1.y = cy1;
    pattern->u.radial.radius1 = fabs (radius1);

    return pattern;
}

void
cairo_pattern_reference (cairo_pattern_t *pattern)
{
    if (pattern == NULL)
	return;

    pattern->ref_count++;
}

void
cairo_pattern_destroy (cairo_pattern_t *pattern)
{
    if (pattern == NULL)
	return;

    pattern->ref_count--;
    if (pattern->ref_count)
	return;

    _cairo_pattern_fini (pattern);
    free (pattern);
}

static int
_cairo_pattern_stop_compare (const void *elem1, const void *elem2)
{
    return
        (((cairo_color_stop_t *) elem1)->offset ==
	 ((cairo_color_stop_t *) elem2)->offset) ?
        /* equal offsets, sort on id */
        ((((cairo_color_stop_t *) elem1)->id <
	  ((cairo_color_stop_t *) elem2)->id) ? -1 : 1) :
        /* sort on offset */
        ((((cairo_color_stop_t *) elem1)->offset <
	  ((cairo_color_stop_t *) elem2)->offset) ? -1 : 1);
}

cairo_status_t
cairo_pattern_add_color_stop (cairo_pattern_t *pattern,
			      double offset,
			      double red, double green, double blue,
			      double alpha)
{
    cairo_color_stop_t *stop;
    int i;

    _cairo_restrict_value (&offset, 0.0, 1.0);
    _cairo_restrict_value (&red, 0.0, 1.0);
    _cairo_restrict_value (&green, 0.0, 1.0);
    _cairo_restrict_value (&blue, 0.0, 1.0);

    pattern->n_stops++;
    pattern->stops = realloc (pattern->stops,
			      sizeof (cairo_color_stop_t) * pattern->n_stops);
    if (pattern->stops == NULL) {
	pattern->n_stops = 0;
    
	return CAIRO_STATUS_NO_MEMORY;
    }

    stop = &pattern->stops[pattern->n_stops - 1];

    stop->offset = _cairo_fixed_from_double (offset);
    stop->id = pattern->n_stops;

    stop->color_char[0] = red * 0xff;
    stop->color_char[1] = green * 0xff;
    stop->color_char[2] = blue * 0xff;
    stop->color_char[3] = alpha * 0xff;

    /* sort stops in ascending order */
    qsort (pattern->stops, pattern->n_stops, sizeof (cairo_color_stop_t),
	   _cairo_pattern_stop_compare);

    for (i = 0; i < pattern->n_stops - 1; i++) {
	pattern->stops[i + 1].scale =
	    pattern->stops[i + 1].offset - pattern->stops[i].offset;
	if (pattern->stops[i + 1].scale == 65536)
	    pattern->stops[i + 1].scale = 0;
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_pattern_set_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix)
{
    cairo_matrix_copy (&pattern->matrix, matrix);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_pattern_get_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix)
{
    cairo_matrix_copy (matrix, &pattern->matrix);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_pattern_set_filter (cairo_pattern_t *pattern, cairo_filter_t filter)
{
    pattern->filter = filter;

    return CAIRO_STATUS_SUCCESS;
}

cairo_filter_t
cairo_pattern_get_filter (cairo_pattern_t *pattern)
{
    return pattern->filter;
}

cairo_status_t
cairo_pattern_set_extend (cairo_pattern_t *pattern, cairo_extend_t extend)
{
    pattern->extend = extend;

    return CAIRO_STATUS_SUCCESS;
}

cairo_extend_t
cairo_pattern_get_extend (cairo_pattern_t *pattern)
{
    return pattern->extend;
}

cairo_status_t
_cairo_pattern_get_rgb (cairo_pattern_t *pattern,
			double *red, double *green, double *blue)
{
    _cairo_color_get_rgb (&pattern->color, red, green, blue);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_set_alpha (cairo_pattern_t *pattern, double alpha)
{
    int i;

    _cairo_color_set_alpha (&pattern->color, alpha);

    for (i = 0; i < pattern->n_stops; i++)
	pattern->stops[i].color_char[3] =
	    MULTIPLY_COLORCOMP (pattern->stops[i].color_char[3], alpha * 0xff);
}

void
_cairo_pattern_set_source_offset (cairo_pattern_t *pattern,
				  double x, double y)
{
    pattern->source_offset.x = x;
    pattern->source_offset.y = y;
}

void
_cairo_pattern_transform (cairo_pattern_t *pattern,
			  cairo_matrix_t *ctm_inverse)
{
    cairo_matrix_multiply (&pattern->matrix, ctm_inverse, &pattern->matrix);
}

void
_cairo_pattern_prepare_surface (cairo_pattern_t *pattern)
{
    cairo_matrix_t device_to_source;
    cairo_matrix_t user_to_source;
    
    /* should the surface matrix interface be remove from the API?
       for now we multiple the surface matrix with the pattern matrix */
    cairo_surface_get_matrix (pattern->u.surface.surface, &user_to_source);
    cairo_matrix_multiply (&device_to_source, &pattern->matrix,
			   &user_to_source);
    cairo_surface_set_matrix (pattern->source, &device_to_source);

    /* storing original surface matrix in pattern */
    pattern->u.surface.save_matrix = user_to_source;

    /* storing original surface repeat mode in pattern */
    pattern->u.surface.save_repeat = pattern->source->repeat;

    /* what do we do with extend types pad and reflect? */
    if (pattern->extend == CAIRO_EXTEND_REPEAT
	|| pattern->source->repeat == 1)
	cairo_surface_set_repeat (pattern->source, 1);
    else
	cairo_surface_set_repeat (pattern->source, 0);
    
    /* storing original surface filter in pattern */
    pattern->u.surface.save_filter =
        cairo_surface_get_filter (pattern->source);
    
    cairo_surface_set_filter (pattern->source, pattern->filter);
}

#define INTERPOLATE_COLOR_NEAREST(c1, c2, factor) \
  ((factor < 32768)? c1: c2)

static void
_cairo_pattern_shader_nearest (unsigned char *color0,
			       unsigned char *color1,
			       cairo_fixed_t factor,
			       int *pixel)
{
    *pixel =
	((INTERPOLATE_COLOR_NEAREST (color0[3], color1[3], factor) << 24) |
	 (INTERPOLATE_COLOR_NEAREST (color0[0], color1[0], factor) << 16) |
	 (INTERPOLATE_COLOR_NEAREST (color0[1], color1[1], factor) << 8) |
	 (INTERPOLATE_COLOR_NEAREST (color0[2], color1[2], factor) << 0));
}

#undef INTERPOLATE_COLOR_NEAREST

#define INTERPOLATE_COLOR_LINEAR(c1, c2, factor) \
  (((c2 * factor) + (c1 * (65536 - factor))) / 65536)

static void
_cairo_pattern_shader_linear (unsigned char *color0,
			      unsigned char *color1,
			      cairo_fixed_t factor,
			      int *pixel)
{
    *pixel = ((INTERPOLATE_COLOR_LINEAR (color0[3], color1[3], factor) << 24) |
	      (INTERPOLATE_COLOR_LINEAR (color0[0], color1[0], factor) << 16) |
	      (INTERPOLATE_COLOR_LINEAR (color0[1], color1[1], factor) << 8) |
	      (INTERPOLATE_COLOR_LINEAR (color0[2], color1[2], factor) << 0));
}

#define E_MINUS_ONE 1.7182818284590452354

static void
_cairo_pattern_shader_gaussian (unsigned char *color0,
				unsigned char *color1,
				cairo_fixed_t factor,
				int *pixel)
{
    double f = ((double) factor) / 65536.0;
    
    factor = (cairo_fixed_t) (((exp (f * f) - 1.0) / E_MINUS_ONE) * 65536);
    
    *pixel = ((INTERPOLATE_COLOR_LINEAR (color0[3], color1[3], factor) << 24) |
	      (INTERPOLATE_COLOR_LINEAR (color0[0], color1[0], factor) << 16) |
	      (INTERPOLATE_COLOR_LINEAR (color0[1], color1[1], factor) << 8) |
	      (INTERPOLATE_COLOR_LINEAR (color0[2], color1[2], factor) << 0));
}

#undef INTERPOLATE_COLOR_LINEAR

void
_cairo_pattern_shader_init (cairo_pattern_t *pattern,
			    cairo_shader_op_t *op)
{
    op->stops = pattern->stops;
    op->n_stops = pattern->n_stops - 1;
    op->min_offset = pattern->stops[0].offset;
    op->max_offset = pattern->stops[op->n_stops].offset;
    op->extend = pattern->extend;
    
    switch (pattern->filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	op->shader_function = _cairo_pattern_shader_nearest;
	break;
    case CAIRO_FILTER_GAUSSIAN:
	op->shader_function = _cairo_pattern_shader_gaussian;
	break;
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
	op->shader_function = _cairo_pattern_shader_linear;
	break;
    }
}

void
_cairo_pattern_calc_color_at_pixel (cairo_shader_op_t *op,
				    cairo_fixed_t factor,
				    int *pixel)
{
    int i;
    
    switch (op->extend) {
    case CAIRO_EXTEND_REPEAT:
	factor -= factor & 0xffff0000;
	break;
    case CAIRO_EXTEND_REFLECT:
	if (factor < 0 || factor > 65536) {
	    if ((factor >> 16) % 2)
		factor = 65536 - (factor - (factor & 0xffff0000));
	    else
		factor -= factor & 0xffff0000;
	}
	break;
    case CAIRO_EXTEND_NONE:
	break;
    }

    if (factor < op->min_offset)
	factor = op->min_offset;
    else if (factor > op->max_offset)
	factor = op->max_offset;
    
    for (i = 0; i < op->n_stops; i++) {
	if (factor <= op->stops[i + 1].offset) {
	    
	    /* take offset as new 0 of coordinate system */
	    factor -= op->stops[i].offset;
	    
	    /* difference between two offsets == 0, abrubt change */
	    if (op->stops[i + 1].scale)
		factor = ((cairo_fixed_48_16_t) factor << 16) /
		    op->stops[i + 1].scale;
	    
	    op->shader_function (op->stops[i].color_char,
				 op->stops[i + 1].color_char,
				 factor, pixel);
	    
	    /* multiply alpha */
	    if (((unsigned char) (*pixel >> 24)) != 0xff) {
		*pixel = (*pixel & 0xff000000) |
		    (MULTIPLY_COLORCOMP (*pixel >> 16, *pixel >> 24) << 16) |
		    (MULTIPLY_COLORCOMP (*pixel >> 8, *pixel >> 24) << 8) |
		    (MULTIPLY_COLORCOMP (*pixel >> 0, *pixel >> 24) << 0);
	    }
	    break;
	}
    }
}

static void
_cairo_image_data_set_linear (cairo_pattern_t *pattern,
			      double offset_x,
			      double offset_y,
			      int *pixels,
			      int width,
			      int height)
{
    int x, y;
    cairo_point_double_t point0, point1;
    double px, py, ex, ey;
    double a, b, c, d, tx, ty;
    double length, start, angle, fx, fy, factor;
    cairo_shader_op_t op;

    _cairo_pattern_shader_init (pattern, &op);

    point0.x = pattern->u.linear.point0.x;
    point0.y = pattern->u.linear.point0.y;
    point1.x = pattern->u.linear.point1.x;
    point1.y = pattern->u.linear.point1.y;

    cairo_matrix_get_affine (&pattern->matrix, &a, &b, &c, &d, &tx, &ty);
    
    length = sqrt ((point1.x - point0.x) * (point1.x - point0.x) +
		   (point1.y - point0.y) * (point1.y - point0.y));
    length = (length) ? 1.0 / length : CAIRO_MAXSHORT;

    angle = -atan2 (point1.y - point0.y, point1.x - point0.x);
    fx = cos (angle);
    fy = -sin (angle);
    
    start = fx * point0.x;
    start += fy * point0.y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    px = x + offset_x;
	    py = y + offset_y;
		
	    /* transform fragment */
	    ex = a * px + c * py + tx;
	    ey = b * px + d * py + ty;
	    
	    factor = ((fx * ex + fy * ey) - start) * length;

	    _cairo_pattern_calc_color_at_pixel (&op, factor * 65536, pixels++);
	}
    }
}

static void
_cairo_image_data_set_radial (cairo_pattern_t *pattern,
			      double offset_x,
			      double offset_y,
			      int *pixels,
			      int width,
			      int height)
{
    int x, y, aligned_circles;
    cairo_point_double_t c0, c1;
    double px, py, ex, ey;
    double a, b, c, d, tx, ty;
    double r0, r1, c0_e_x, c0_e_y, c0_e, c1_e_x, c1_e_y, c1_e,
	c0_c1_x, c0_c1_y, c0_c1, angle_c0, c1_y, y_x, c0_y, c0_x, r1_2,
	denumerator, fraction, factor;
    cairo_shader_op_t op;

    _cairo_pattern_shader_init (pattern, &op);

    c0.x = pattern->u.radial.center0.x;
    c0.y = pattern->u.radial.center0.y;
    r0 = pattern->u.radial.radius0;
    c1.x = pattern->u.radial.center1.x;
    c1.y = pattern->u.radial.center1.y;
    r1 =  pattern->u.radial.radius1;

    if (c0.x != c1.x || c0.y != c1.y) {
	aligned_circles = 0;
	c0_c1_x = c1.x - c0.x;
	c0_c1_y = c1.y - c0.y;
	c0_c1 = sqrt (c0_c1_x * c0_c1_x + c0_c1_y * c0_c1_y);
	r1_2 = r1 * r1;
    } else {
	aligned_circles = 1;
	r1 = 1.0 / (r1 - r0);
	r1_2 = c0_c1 = 0.0; /* shut up compiler */
    }

    cairo_matrix_get_affine (&pattern->matrix, &a, &b, &c, &d, &tx, &ty);

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    px = x + offset_x;
	    py = y + offset_y;
		
	    /* transform fragment */
	    ex = a * px + c * py + tx;
	    ey = b * px + d * py + ty;

	    if (aligned_circles) {
		ex = ex - c1.x;
		ey = ey - c1.y;

		factor = (sqrt (ex * ex + ey * ey) - r0) * r1;
	    } else {
	    /*
	                y         (ex, ey)
               c0 -------------------+---------- x
                  \     |                  __--
                   \    |              __--
                    \   |          __--
                     \  |      __-- r1
                      \ |  __--
                      c1 --

	       We need to calulate distance c0->x; the distance from
	       the inner circle center c0, through fragment position
	       (ex, ey) to point x where it crosses the outer circle.

	       From points c0, c1 and (ex, ey) we get angle C0. With
	       angle C0 we calculate distance c1->y and c0->y and by
	       knowing c1->y and r1, we also know y->x. Adding y->x to
	       c0->y gives us c0->x. The gradient offset can then be
	       calculated as:
	       
	       offset = (c0->e - r0) / (c0->x - r0)
	       
	       */

		c0_e_x = ex - c0.x;
		c0_e_y = ey - c0.y;
		c0_e = sqrt (c0_e_x * c0_e_x + c0_e_y * c0_e_y);

		c1_e_x = ex - c1.x;
		c1_e_y = ey - c1.y;
		c1_e = sqrt (c1_e_x * c1_e_x + c1_e_y * c1_e_y);

		denumerator = -2.0 * c0_e * c0_c1;
		
		if (denumerator != 0.0) {
		    fraction = (c1_e * c1_e - c0_e * c0_e - c0_c1 * c0_c1) /
			denumerator;

		    if (fraction > 1.0)
			fraction = 1.0;
		    else if (fraction < -1.0)
			fraction = -1.0;
		    
		    angle_c0 = acos (fraction);
		    
		    c0_y = cos (angle_c0) * c0_c1;
		    c1_y = sin (angle_c0) * c0_c1;
		    
		    y_x = sqrt (r1_2 - c1_y * c1_y);
		    c0_x = y_x + c0_y;
		    
		    factor = (c0_e - r0) / (c0_x - r0);
		} else
		    factor = -r0;
	    }

	    _cairo_pattern_calc_color_at_pixel (&op, factor * 65536, pixels++);
	}
    }
}

cairo_image_surface_t *
_cairo_pattern_get_image (cairo_pattern_t *pattern, cairo_box_t *box)
{
    cairo_surface_t *surface;

    switch (pattern->type) {
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL: {
	char *data;
	double x = box->p1.x >> 16;
	double y = box->p1.y >> 16;
	int width = ((box->p2.x + 65535) >> 16) - (box->p1.x >> 16);
	int height = ((box->p2.y + 65535) >> 16) - (box->p1.y >> 16);
	
	data = malloc (width * height * 4);
	if (!data)
	    return NULL;
	
	if (pattern->type == CAIRO_PATTERN_RADIAL)
	    _cairo_image_data_set_radial (pattern, x, y, (int *) data,
					  width, height);
	else
	    _cairo_image_data_set_linear (pattern, x, y, (int *) data,
					  width, height);

	_cairo_pattern_set_source_offset (pattern, x, y);

	surface = cairo_image_surface_create_for_data (data,
						       CAIRO_FORMAT_ARGB32,
						       width, height,
						       width * 4);
	
	if (surface)
	    _cairo_image_surface_assume_ownership_of_data (
		(cairo_image_surface_t *) surface);
    }
	break;
    case CAIRO_PATTERN_SOLID:
	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
	if (surface) {
	    _cairo_surface_fill_rectangle (surface,
					   CAIRO_OPERATOR_SRC,
					   &pattern->color, 0, 0, 1, 1);
	    cairo_surface_set_repeat (surface, 1);
	}
	break;
    case CAIRO_PATTERN_SURFACE: {
	cairo_image_surface_t *image;

	image = _cairo_surface_get_image (pattern->u.surface.surface);
	if (image)
	    surface = &image->base;
	else
	    surface = NULL;
    
    }
	break;
    default:
	surface = NULL;
	break;
    }
    
    return (cairo_image_surface_t *) surface;
}
 
