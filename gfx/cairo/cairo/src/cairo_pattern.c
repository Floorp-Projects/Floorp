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

typedef void (*cairo_shader_function_t) (unsigned char *color0,
					 unsigned char *color1,
					 cairo_fixed_t factor,
					 uint32_t      *pixel);

typedef struct _cairo_shader_color_stop {
    cairo_fixed_t	offset;
    cairo_fixed_48_16_t scale;
    int			id;
    unsigned char	color_char[4];
} cairo_shader_color_stop_t;

typedef struct _cairo_shader_op {
    cairo_shader_color_stop_t *stops;
    int			      n_stops;
    cairo_extend_t	      extend;
    cairo_shader_function_t   shader_function;
} cairo_shader_op_t;

#define MULTIPLY_COLORCOMP(c1, c2) \
    ((unsigned char) \
     ((((unsigned char) (c1)) * (int) ((unsigned char) (c2))) / 0xff))

static void
_cairo_pattern_init (cairo_pattern_t *pattern, cairo_pattern_type_t type)
{
    pattern->type      = type; 
    pattern->ref_count = 1;
    pattern->extend    = CAIRO_EXTEND_DEFAULT;
    pattern->filter    = CAIRO_FILTER_DEFAULT;
    pattern->alpha     = 1.0;

    _cairo_matrix_init (&pattern->matrix);
}

static cairo_status_t
_cairo_gradient_pattern_init_copy (cairo_gradient_pattern_t *pattern,
				   cairo_gradient_pattern_t *other)
{
    if (other->base.type == CAIRO_PATTERN_LINEAR)
    {
	cairo_linear_pattern_t *dst = (cairo_linear_pattern_t *) pattern;
	cairo_linear_pattern_t *src = (cairo_linear_pattern_t *) other;
	
	*dst = *src;
    }
    else
    {
	cairo_radial_pattern_t *dst = (cairo_radial_pattern_t *) pattern;
	cairo_radial_pattern_t *src = (cairo_radial_pattern_t *) other;
	
	*dst = *src;
    }

    if (other->n_stops)
    {
	pattern->stops = malloc (other->n_stops * sizeof (cairo_color_stop_t));
	if (!pattern->stops)
	    return CAIRO_STATUS_NO_MEMORY;
	
	memcpy (pattern->stops, other->stops,
		other->n_stops * sizeof (cairo_color_stop_t));
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_pattern_init_copy (cairo_pattern_t *pattern, cairo_pattern_t *other)
{
    switch (other->type) {
    case CAIRO_PATTERN_SOLID: {
	cairo_solid_pattern_t *dst = (cairo_solid_pattern_t *) pattern;
	cairo_solid_pattern_t *src = (cairo_solid_pattern_t *) other;

	*dst = *src;
    } break;
    case CAIRO_PATTERN_SURFACE: {
	cairo_surface_pattern_t *dst = (cairo_surface_pattern_t *) pattern;
	cairo_surface_pattern_t *src = (cairo_surface_pattern_t *) other;
	
	*dst = *src;
	cairo_surface_reference (dst->surface);
    } break;
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL: {
	cairo_gradient_pattern_t *dst = (cairo_gradient_pattern_t *) pattern;
	cairo_gradient_pattern_t *src = (cairo_gradient_pattern_t *) other;
	cairo_status_t		 status;
	
	status = _cairo_gradient_pattern_init_copy (dst, src);
	if (status)
	    return status;
    } break;
    }
    
    pattern->ref_count = 1;
    
    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_fini (cairo_pattern_t *pattern)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID:
	break;
    case CAIRO_PATTERN_SURFACE: {
	cairo_surface_pattern_t *fini = (cairo_surface_pattern_t *) pattern;
	
	cairo_surface_destroy (fini->surface);
    } break;
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL: {
	cairo_gradient_pattern_t *fini = (cairo_gradient_pattern_t *) pattern;
	
	if (fini->n_stops)
	    free (fini->stops);
    } break;
    }
}

void
_cairo_pattern_init_solid (cairo_solid_pattern_t *pattern,
			   double		 red,
			   double		 green,
			   double		 blue)
{
    _cairo_pattern_init (&pattern->base, CAIRO_PATTERN_SOLID);
    
    pattern->red   = red;
    pattern->green = green;
    pattern->blue  = blue;
}

void
_cairo_pattern_init_for_surface (cairo_surface_pattern_t *pattern,
				 cairo_surface_t	 *surface)
{
    _cairo_pattern_init (&pattern->base, CAIRO_PATTERN_SURFACE);
    
    pattern->surface = surface;
    cairo_surface_reference (surface);
}

static void
_cairo_pattern_init_gradient (cairo_gradient_pattern_t *pattern,
			      cairo_pattern_type_t     type)
{
    _cairo_pattern_init (&pattern->base, type);

    pattern->stops   = 0;
    pattern->n_stops = 0;
}

void
_cairo_pattern_init_linear (cairo_linear_pattern_t *pattern,
			    double x0, double y0, double x1, double y1)
{
    _cairo_pattern_init_gradient (&pattern->base, CAIRO_PATTERN_LINEAR);
    
    pattern->point0.x = x0;
    pattern->point0.y = y0;
    pattern->point1.x = x1;
    pattern->point1.y = y1;
}

void
_cairo_pattern_init_radial (cairo_radial_pattern_t *pattern,
			    double cx0, double cy0, double radius0,
			    double cx1, double cy1, double radius1)
{
    _cairo_pattern_init_gradient (&pattern->base, CAIRO_PATTERN_RADIAL);

    pattern->center0.x = cx0;
    pattern->center0.y = cy0;
    pattern->radius0   = fabs (radius0);
    pattern->center1.x = cx1;
    pattern->center1.y = cy1;
    pattern->radius1   = fabs (radius1);
}

cairo_pattern_t *
_cairo_pattern_create_solid (double red, double green, double blue)
{
    cairo_solid_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_solid_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init_solid (pattern, red, green, blue);

    return &pattern->base;
}

cairo_pattern_t *
cairo_pattern_create_for_surface (cairo_surface_t *surface)
{
    cairo_surface_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_surface_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init_for_surface (pattern, surface);

    /* this will go away when we completely remove the surface attributes */
    if (surface->repeat)
	pattern->base.extend = CAIRO_EXTEND_REPEAT;
    else
	pattern->base.extend = CAIRO_EXTEND_DEFAULT;

    return &pattern->base;
}

cairo_pattern_t *
cairo_pattern_create_linear (double x0, double y0, double x1, double y1)
{
    cairo_linear_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_linear_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init_linear (pattern, x0, y0, x1, y1);

    return &pattern->base.base;
}

cairo_pattern_t *
cairo_pattern_create_radial (double cx0, double cy0, double radius0,
			     double cx1, double cy1, double radius1)
{
    cairo_radial_pattern_t *pattern;
    
    pattern = malloc (sizeof (cairo_radial_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init_radial (pattern, cx0, cy0, radius0, cx1, cy1, radius1);

    return &pattern->base.base;
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

static cairo_status_t
_cairo_pattern_add_color_stop (cairo_gradient_pattern_t *pattern,
			       double			offset,
			       double			red,
			       double			green,
			       double			blue,
			       double			alpha)
{
    cairo_color_stop_t *stop;

    pattern->n_stops++;
    pattern->stops = realloc (pattern->stops,
			      pattern->n_stops * sizeof (cairo_color_stop_t));
    if (pattern->stops == NULL) {
	pattern->n_stops = 0;
    
	return CAIRO_STATUS_NO_MEMORY;
    }

    stop = &pattern->stops[pattern->n_stops - 1];

    stop->offset = _cairo_fixed_from_double (offset);

    _cairo_color_init (&stop->color);
    _cairo_color_set_rgb (&stop->color, red, green, blue);
    _cairo_color_set_alpha (&stop->color, alpha);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_pattern_add_color_stop (cairo_pattern_t *pattern,
			      double	      offset,
			      double	      red,
			      double	      green,
			      double	      blue,
			      double	      alpha)
{
    if (pattern->type != CAIRO_PATTERN_LINEAR &&
	pattern->type != CAIRO_PATTERN_RADIAL)
    {
	/* XXX: CAIRO_STATUS_INVALID_PATTERN? */
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_restrict_value (&offset, 0.0, 1.0);
    _cairo_restrict_value (&red,    0.0, 1.0);
    _cairo_restrict_value (&green,  0.0, 1.0);
    _cairo_restrict_value (&blue,   0.0, 1.0);
    _cairo_restrict_value (&alpha,  0.0, 1.0);

    return _cairo_pattern_add_color_stop ((cairo_gradient_pattern_t *) pattern,
					  offset,
					  red, green, blue,
					  alpha);
}

cairo_status_t
cairo_pattern_set_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix)
{
    return cairo_matrix_copy (&pattern->matrix, matrix);
}

cairo_status_t
cairo_pattern_get_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix)
{
    return cairo_matrix_copy (matrix, &pattern->matrix);
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
			double		*red,
			double		*green,
			double		*blue)
{

    if (pattern->type == CAIRO_PATTERN_SOLID)
    {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) pattern;
	
	*red   = solid->red;
	*green = solid->green;
	*blue  = solid->blue;
    } else
	*red = *green = *blue = 1.0;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_set_alpha (cairo_pattern_t *pattern, double alpha)
{
    pattern->alpha = alpha;
}

void
_cairo_pattern_transform (cairo_pattern_t *pattern,
			  cairo_matrix_t  *ctm_inverse)
{
    cairo_matrix_multiply (&pattern->matrix, ctm_inverse, &pattern->matrix);
}

#define INTERPOLATE_COLOR_NEAREST(c1, c2, factor) \
  ((factor < 32768)? c1: c2)

static void
_cairo_pattern_shader_nearest (unsigned char *color0,
			       unsigned char *color1,
			       cairo_fixed_t factor,
			       uint32_t	     *pixel)
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
			      uint32_t	    *pixel)
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
				uint32_t      *pixel)
{
    double f = ((double) factor) / 65536.0;
    
    factor = (cairo_fixed_t) (((exp (f * f) - 1.0) / E_MINUS_ONE) * 65536);
    
    *pixel = ((INTERPOLATE_COLOR_LINEAR (color0[3], color1[3], factor) << 24) |
	      (INTERPOLATE_COLOR_LINEAR (color0[0], color1[0], factor) << 16) |
	      (INTERPOLATE_COLOR_LINEAR (color0[1], color1[1], factor) << 8) |
	      (INTERPOLATE_COLOR_LINEAR (color0[2], color1[2], factor) << 0));
}

#undef INTERPOLATE_COLOR_LINEAR

static int
_cairo_shader_color_stop_compare (const void *elem1, const void *elem2)
{
    cairo_shader_color_stop_t *s1 = (cairo_shader_color_stop_t *) elem1;
    cairo_shader_color_stop_t *s2 = (cairo_shader_color_stop_t *) elem2;
	
    return
        (s1->offset == s2->offset) ?
        /* equal offsets, sort on id */
        ((s1->id < s2->id) ? -1 : 1) :
        /* sort on offset */
        ((s1->offset < s2->offset) ? -1 : 1);
}

static cairo_status_t
_cairo_pattern_shader_init (cairo_gradient_pattern_t *pattern,
			    cairo_shader_op_t	     *op)
{
    int i;

    op->stops = malloc (pattern->n_stops * sizeof (cairo_shader_color_stop_t));
    if (!op->stops)
	return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < pattern->n_stops; i++)
    {
	op->stops[i].color_char[0] = pattern->stops[i].color.red * 0xff;
	op->stops[i].color_char[1] = pattern->stops[i].color.green * 0xff;
	op->stops[i].color_char[2] = pattern->stops[i].color.blue * 0xff;
	op->stops[i].color_char[3] = pattern->stops[i].color.alpha *
	    pattern->base.alpha * 0xff;
	op->stops[i].offset = pattern->stops[i].offset;
	op->stops[i].id = i;
    }

    /* sort stops in ascending order */
    qsort (op->stops, pattern->n_stops, sizeof (cairo_shader_color_stop_t),
	   _cairo_shader_color_stop_compare);

    for (i = 0; i < pattern->n_stops - 1; i++)
    {
	op->stops[i + 1].scale = op->stops[i + 1].offset - op->stops[i].offset;
	if (op->stops[i + 1].scale == 65536)
	    op->stops[i + 1].scale = 0;
    }

    op->n_stops = pattern->n_stops;
    op->extend = pattern->base.extend;

    /* XXX: this is wrong, the filter should not be used for selecting
       color stop interpolation function. function should always be 'linear'
       and filter should be used for computing pixels. */
    switch (pattern->base.filter) {
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

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_pattern_shader_fini (cairo_shader_op_t *op)
{
    if (op->stops)
	free (op->stops);
}

/* Find two color stops bounding the given offset. If the given offset
 * is before the first or after the last stop offset, the nearest
 * offset is returned twice.
 */
static void
_cairo_shader_op_find_color_stops (cairo_shader_op_t	     *op,
				   cairo_fixed_t	     offset,
				   cairo_shader_color_stop_t *stops[2])
{
    int i;

    /* Before first stop. */
    if (offset <= op->stops[0].offset) {
	stops[0] = &op->stops[0];
	stops[1] = &op->stops[0];
	return;
    }

    /* Between two stops. */
    for (i = 0; i < op->n_stops - 1; i++) {
	if (offset <= op->stops[i + 1].offset) {
	    stops[0] = &op->stops[i];
	    stops[1] = &op->stops[i + 1];
	    return;
	}
    }

    /* After last stop. */
    stops[0] = &op->stops[op->n_stops - 1];
    stops[1] = &op->stops[op->n_stops - 1];
}

static void
_cairo_pattern_calc_color_at_pixel (cairo_shader_op_t *op,
				    cairo_fixed_t     factor,
				    uint32_t	      *pixel)
{
    cairo_shader_color_stop_t *stops[2];

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

    _cairo_shader_op_find_color_stops (op, factor, stops);

    /* take offset as new 0 of coordinate system */
    factor -= stops[0]->offset;
	    
    /* difference between two offsets == 0, abrubt change */
    if (stops[1]->scale)
	factor = ((cairo_fixed_48_16_t) factor << 16) /
	    stops[1]->scale;

    op->shader_function (stops[0]->color_char,
			 stops[1]->color_char,
			 factor, pixel);
	    
    /* multiply alpha */
    if (((unsigned char) (*pixel >> 24)) != 0xff) {
	*pixel = (*pixel & 0xff000000) |
	    (MULTIPLY_COLORCOMP (*pixel >> 16, *pixel >> 24) << 16) |
	    (MULTIPLY_COLORCOMP (*pixel >> 8, *pixel >> 24) << 8) |
	    (MULTIPLY_COLORCOMP (*pixel >> 0, *pixel >> 24) << 0);
    }
}

static cairo_status_t
_cairo_image_data_set_linear (cairo_linear_pattern_t *pattern,
			      double		     offset_x,
			      double		     offset_y,
			      uint32_t		     *pixels,
			      int		     width,
			      int		     height)
{
    int x, y;
    cairo_point_double_t point0, point1;
    double a, b, c, d, tx, ty;
    double scale, start, dx, dy, factor;
    cairo_shader_op_t op;
    cairo_status_t status;

    status = _cairo_pattern_shader_init (&pattern->base, &op);
    if (status)
	return status;

    /* We compute the position in the linear gradient for
     * a point q as:
     *
     *  [q . (p1 - p0) - p0 . (p1 - p0)] / (p1 - p0) ^ 2
     *
     * The computation is done in pattern space. The 
     * calculation could be heavily optimized by using the
     * fact that 'factor' increases linearly in both
     * directions.
     */
    point0.x = pattern->point0.x;
    point0.y = pattern->point0.y;
    point1.x = pattern->point1.x;
    point1.y = pattern->point1.y;

    cairo_matrix_get_affine (&pattern->base.base.matrix,
			     &a, &b, &c, &d, &tx, &ty);

    dx = point1.x - point0.x;
    dy = point1.y - point0.y;
    scale = dx * dx + dy * dy;
    scale = (scale) ? 1.0 / scale : 1.0;

    start = dx * point0.x + dy * point0.y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    double qx_device = x + offset_x;
	    double qy_device = y + offset_y;
		
	    /* transform fragment into pattern space */
	    double qx = a * qx_device + c * qy_device + tx;
	    double qy = b * qx_device + d * qy_device + ty;
	    
	    factor = ((dx * qx + dy * qy) - start) * scale;

	    _cairo_pattern_calc_color_at_pixel (&op, factor * 65536, pixels++);
	}
    }

    _cairo_pattern_shader_fini (&op);
    
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_linear_pattern_classify (cairo_linear_pattern_t *pattern,
				double		       offset_x,
				double		       offset_y,
				int		       width,
				int		       height,
				cairo_bool_t           *is_horizontal,
				cairo_bool_t           *is_vertical)
{
    cairo_point_double_t point0, point1;
    double a, b, c, d, tx, ty;
    double scale, start, dx, dy;
    cairo_fixed_t factors[3];
    int i;

    /* To classidy a pattern as horizontal or vertical, we first
     * compute the (fixed point) factors at the corners of the
     * pattern. We actually only need 3/4 corners, so we skip the
     * fourth.
     */
    point0.x = pattern->point0.x;
    point0.y = pattern->point0.y;
    point1.x = pattern->point1.x;
    point1.y = pattern->point1.y;

    cairo_matrix_get_affine (&pattern->base.base.matrix,
			     &a, &b, &c, &d, &tx, &ty);

    dx = point1.x - point0.x;
    dy = point1.y - point0.y;
    scale = dx * dx + dy * dy;
    scale = (scale) ? 1.0 / scale : 1.0;

    start = dx * point0.x + dy * point0.y;

    for (i = 0; i < 3; i++) {
	double qx_device = (i % 2) * (width - 1) + offset_x;
	double qy_device = (i / 2) * (height - 1) + offset_y;

	/* transform fragment into pattern space */
	double qx = a * qx_device + c * qy_device + tx;
	double qy = b * qx_device + d * qy_device + ty;
	
	factors[i] = _cairo_fixed_from_double (((dx * qx + dy * qy) - start) * scale);
    }

    /* We consider a pattern to be vertical if the fixed point factor
     * at the two upper corners is the same. We could accept a small
     * change, but determining what change is acceptable would require
     * sorting the stops in the pattern and looking at the differences.
     *
     * Horizontal works the same way with the two left corners.
     */

    *is_vertical = factors[1] == factors[0];
    *is_horizontal = factors[2] == factors[0];
}

static cairo_status_t
_cairo_image_data_set_radial (cairo_radial_pattern_t *pattern,
			      double		     offset_x,
			      double		     offset_y,
			      uint32_t		     *pixels,
			      int		     width,
			      int		     height)
{
    int x, y, aligned_circles;
    cairo_point_double_t c0, c1;
    double px, py, ex, ey;
    double a, b, c, d, tx, ty;
    double r0, r1, c0_e_x, c0_e_y, c0_e, c1_e_x, c1_e_y, c1_e,
	c0_c1_x, c0_c1_y, c0_c1, angle_c0, c1_y, y_x, c0_y, c0_x, r1_2,
	denumerator, fraction, factor;
    cairo_shader_op_t op;
    cairo_status_t status;

    status = _cairo_pattern_shader_init (&pattern->base, &op);
    if (status)
	return status;

    c0.x = pattern->center0.x;
    c0.y = pattern->center0.y;
    r0 = pattern->radius0;
    c1.x = pattern->center1.x;
    c1.y = pattern->center1.y;
    r1 =  pattern->radius1;

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

    cairo_matrix_get_affine (&pattern->base.base.matrix,
			     &a, &b, &c, &d, &tx, &ty);

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

    _cairo_pattern_shader_fini (&op);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pattern_acquire_surface_for_gradient (cairo_gradient_pattern_t *pattern,
					     cairo_surface_t	        *dst,
					     int			x,
					     int			y,
					     unsigned int		width,
					     unsigned int	        height,
					     cairo_surface_t	        **out,
					     cairo_surface_attributes_t *attr)
{
    cairo_image_surface_t *image;
    cairo_status_t	  status;
    uint32_t		  *data;
    cairo_bool_t          repeat = FALSE;

    if (pattern->base.type == CAIRO_PATTERN_LINEAR) {
	cairo_bool_t is_horizontal;
	cairo_bool_t is_vertical;

	_cairo_linear_pattern_classify ((cairo_linear_pattern_t *)pattern,
					x, y, width, height,
					&is_horizontal, &is_vertical);
	if (is_horizontal) {
	    height = 1;
	    repeat = TRUE;
	}
	if (is_vertical) {
	    width = 1;
	    repeat = TRUE;
	}
    }
    
    data = malloc (width * height * 4);
    if (!data)
	return CAIRO_STATUS_NO_MEMORY;
    
    if (pattern->base.type == CAIRO_PATTERN_LINEAR)
    {
	cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) pattern;
	
	status = _cairo_image_data_set_linear (linear, x, y, data,
					       width, height);
    }
    else
    {
	cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) pattern;
	
	status = _cairo_image_data_set_radial (radial, x, y, data,
					       width, height);
    }

    if (status) {
	free (data);
	return status;
    }

    image = (cairo_image_surface_t *)
	cairo_image_surface_create_for_data ((char *) data,
					     CAIRO_FORMAT_ARGB32,
					     width, height,
					     width * 4);
    
    if (image == NULL) {
	free (data);
	return CAIRO_STATUS_NO_MEMORY;
    }

    _cairo_image_surface_assume_ownership_of_data (image);
    
    status = _cairo_surface_clone_similar (dst, &image->base, out);
	
    cairo_surface_destroy (&image->base);

    attr->x_offset = -x;
    attr->y_offset = -y;
    cairo_matrix_set_identity (&attr->matrix);
    attr->extend = repeat ? CAIRO_EXTEND_REPEAT : CAIRO_EXTEND_NONE;
    attr->filter = CAIRO_FILTER_NEAREST;
    attr->acquired = FALSE;
    
    return status;
}

static cairo_int_status_t
_cairo_pattern_acquire_surface_for_solid (cairo_solid_pattern_t	     *pattern,
					  cairo_surface_t	     *dst,
					  int			     x,
					  int			     y,
					  unsigned int		     width,
					  unsigned int		     height,
					  cairo_surface_t	     **out,
					  cairo_surface_attributes_t *attribs)
{
    cairo_color_t color;

    _cairo_color_init (&color);
    _cairo_color_set_rgb (&color, pattern->red, pattern->green, pattern->blue);
    _cairo_color_set_alpha (&color, pattern->base.alpha);
    
    *out = _cairo_surface_create_similar_solid (dst,
						CAIRO_FORMAT_ARGB32,
						1, 1,
						&color);
    
    if (*out == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    attribs->x_offset = attribs->y_offset = 0;
    cairo_matrix_set_identity (&attribs->matrix);
    attribs->extend = CAIRO_EXTEND_REPEAT;
    attribs->filter = CAIRO_FILTER_NEAREST;
    attribs->acquired = FALSE;
    
    return CAIRO_STATUS_SUCCESS;
}


/**
 * _cairo_pattern_is_opaque
 *
 * Convenience function to determine whether a pattern has an opaque
 * alpha value. This is done by testing whether the pattern's alpha
 * value when converted to a byte is 255, so if a backend actually
 * supported deep alpha channels this function might not do the right
 * thing.
 *
 * Note that for a gradient or surface pattern, the overall resulting
 * alpha for the pattern can be non-opaque even this function returns
 * %TRUE, since the resulting alpha is the multiplication of the
 * alpha of the gradient or surface with the pattern's alpha. In
 * the future, alpha will be moved from the base pattern to the
 * solid pattern subtype, at which point this function should
 * probably be renamed to _cairo_pattern_is_opaque_solid()
 *
 * Return value: %TRUE if the pattern is opaque
 **/
cairo_bool_t 
_cairo_pattern_is_opaque (cairo_pattern_t *pattern)
{
  return (pattern->alpha >= ((double)0xff00 / (double)0xffff));
}

static cairo_int_status_t
_cairo_pattern_acquire_surface_for_surface (cairo_surface_pattern_t   *pattern,
					    cairo_surface_t	       *dst,
					    int			       x,
					    int			       y,
					    unsigned int	       width,
					    unsigned int	       height,
					    cairo_surface_t	       **out,
					    cairo_surface_attributes_t *attr)
{
    cairo_int_status_t status;

    attr->acquired = FALSE;
    
    /* handle pattern opacity */
    if (!_cairo_pattern_is_opaque (&pattern->base))
    {
	cairo_surface_pattern_t tmp;
	cairo_color_t		color;

	_cairo_color_init (&color);
	_cairo_color_set_alpha (&color, pattern->base.alpha);

	*out = _cairo_surface_create_similar_solid (dst,
						    CAIRO_FORMAT_ARGB32,
						    width, height,
						    &color);
	if (*out == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	status = _cairo_pattern_init_copy (&tmp.base, &pattern->base);
	if (CAIRO_OK (status))
	{
	    tmp.base.alpha = 1.0;
	    status = _cairo_surface_composite (CAIRO_OPERATOR_IN,
					       &tmp.base,
					       NULL,
					       *out,
					       x, y, 0, 0, 0, 0,
					       width, height);

	    _cairo_pattern_fini (&tmp.base);
	}

	if (status) {
	    cairo_surface_destroy (*out);
	    return status;
	}
	
	attr->x_offset = -x;
	attr->y_offset = -y;
	attr->extend   = CAIRO_EXTEND_NONE;
	attr->filter   = CAIRO_FILTER_NEAREST;
	
	cairo_matrix_set_identity (&attr->matrix);
    }
    else
    {
	int tx, ty;
	    
	if (_cairo_surface_is_image (dst))
	{
	    cairo_image_surface_t *image;
	    
	    status = _cairo_surface_acquire_source_image (pattern->surface,
							  &image,
							  &attr->extra);
	    if (CAIRO_OK (status))
		*out = &image->base;

	    attr->acquired = TRUE;
	}
	else
	    status = _cairo_surface_clone_similar (dst, pattern->surface, out);
	
	attr->extend = pattern->base.extend;
	attr->filter = pattern->base.filter;
	if (_cairo_matrix_is_integer_translation (&pattern->base.matrix,
						  &tx, &ty))
	{
	    cairo_matrix_set_identity (&attr->matrix);
	    attr->x_offset = tx;
	    attr->y_offset = ty;
	}
	else
	{
	    attr->matrix = pattern->base.matrix;
	    attr->x_offset = attr->y_offset = 0;
	}
    }
    
    return status;
}

/**
 * _cairo_pattern_acquire_surface:
 * @pattern: a #cairo_pattern_t
 * @dst: destination surface
 * @x: X coordinate in source corresponding to left side of destination area
 * @y: Y coordinate in source corresponding to top side of destination area
 * @width: width of destination area
 * @height: height of destination area
 * @surface_out: location to store a pointer to a surface
 * @attributes: surface attributes that destination backend should apply to
 * the returned surface
 * 
 * A convenience function to obtain a surface to use as the source for
 * drawing on @dst.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if a surface was stored in @surface_out.
 **/
cairo_int_status_t
_cairo_pattern_acquire_surface (cairo_pattern_t		   *pattern,
				cairo_surface_t		   *dst,
				int			   x,
				int			   y,
				unsigned int		   width,
				unsigned int		   height,
				cairo_surface_t		   **surface_out,
				cairo_surface_attributes_t *attributes)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID: {
	cairo_solid_pattern_t *src = (cairo_solid_pattern_t *) pattern;
	
	return _cairo_pattern_acquire_surface_for_solid (src, dst,
							 x, y, width, height,
							 surface_out,
							 attributes);
	} break;
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL: {
	cairo_gradient_pattern_t *src = (cairo_gradient_pattern_t *) pattern;

	/* fast path for gradients with less than 2 color stops */
	if (src->n_stops < 2)
	{
	    cairo_solid_pattern_t solid;

	    if (src->n_stops)
	    {
		_cairo_pattern_init_solid (&solid,
					   src->stops->color.red,
					   src->stops->color.green,
					   src->stops->color.blue);
		_cairo_pattern_set_alpha (&solid.base,
					  src->stops->color.alpha);
	    }
	    else
	    {
		_cairo_pattern_init_solid (&solid, 0.0, 0.0, 0.0);
		_cairo_pattern_set_alpha (&solid.base, 0.0);
	    }

	    return _cairo_pattern_acquire_surface_for_solid (&solid, dst,
							     x, y,
							     width, height,
							     surface_out,
							     attributes);
	}
	else
	    return _cairo_pattern_acquire_surface_for_gradient (src, dst,
								x, y,
								width, height,
								surface_out,
								attributes);
    } break;
    case CAIRO_PATTERN_SURFACE: {
	cairo_surface_pattern_t *src = (cairo_surface_pattern_t *) pattern;
	
	return _cairo_pattern_acquire_surface_for_surface (src, dst,
							   x, y, width, height,
							   surface_out,
							   attributes);
    } break;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

/**
 * _cairo_pattern_release_surface:
 * @pattern: a #cairo_pattern_t
 * @info: pointer to #cairo_surface_attributes_t filled in by
 *        _cairo_pattern_acquire_surface
 * 
 * Releases resources obtained by _cairo_pattern_acquire_surface.
 **/
void
_cairo_pattern_release_surface (cairo_surface_t		   *dst,
				cairo_surface_t		   *surface,
				cairo_surface_attributes_t *attributes)
{
    if (attributes->acquired)
	_cairo_surface_release_source_image (dst,
					     (cairo_image_surface_t *) surface,
					     attributes->extra);
    else
	cairo_surface_destroy (surface);
}

cairo_int_status_t
_cairo_pattern_acquire_surfaces (cairo_pattern_t	    *src,
				 cairo_pattern_t	    *mask,
				 cairo_surface_t	    *dst,
				 int			    src_x,
				 int			    src_y,
				 int			    mask_x,
				 int			    mask_y,
				 unsigned int		    width,
				 unsigned int		    height,
				 cairo_surface_t	    **src_out,
				 cairo_surface_t	    **mask_out,
				 cairo_surface_attributes_t *src_attributes,
				 cairo_surface_attributes_t *mask_attributes)
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
	
    status = _cairo_pattern_acquire_surface (&tmp.base, dst,
					     src_x, src_y,
					     width, height,
					     src_out, src_attributes);
    
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
	
	status = _cairo_pattern_acquire_surface (&tmp.base, dst,
						 mask_x, mask_y,
						 width, height,
						 mask_out, mask_attributes);
    
	_cairo_pattern_fini (&tmp.base);

	if (status)
	{
	    _cairo_pattern_release_surface (dst, *src_out, src_attributes);
	    return status;
	}
    }
    else
    {
	*mask_out = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}
