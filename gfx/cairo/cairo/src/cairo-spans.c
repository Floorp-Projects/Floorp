/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright (c) 2008  M Joonas Pihlaja
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include "cairoint.h"

typedef struct {
    cairo_scan_converter_t *converter;
    cairo_point_t current_point;
    cairo_point_t first_point;
    cairo_bool_t has_first_point;
} scan_converter_filler_t;

static void
scan_converter_filler_init (scan_converter_filler_t		*filler,
			    cairo_scan_converter_t		*converter)
{
    filler->converter = converter;
    filler->current_point.x = 0;
    filler->current_point.y = 0;
    filler->first_point = filler->current_point;
    filler->has_first_point = FALSE;
}

static cairo_status_t
scan_converter_filler_close_path (void *closure)
{
    scan_converter_filler_t *filler = closure;
    cairo_status_t status;

    filler->has_first_point = FALSE;

    if (filler->first_point.x == filler->current_point.x &&
	filler->first_point.y == filler->current_point.y)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    status = filler->converter->add_edge (
	filler->converter,
	filler->current_point.x, filler->current_point.y,
	filler->first_point.x, filler->first_point.y);

    filler->current_point = filler->first_point;
    return status;
}

static cairo_status_t
scan_converter_filler_move_to (void *closure,
			       const cairo_point_t *p)
{
    scan_converter_filler_t *filler = closure;
    if (filler->has_first_point) {
	cairo_status_t status = scan_converter_filler_close_path (closure);
	if (status)
	    return status;
    }
    filler->current_point.x = p->x;
    filler->current_point.y = p->y;
    filler->first_point = filler->current_point;
    filler->has_first_point = TRUE;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
scan_converter_filler_line_to (void *closure,
			       const cairo_point_t *p)
{
    scan_converter_filler_t *filler = closure;
    cairo_status_t status;
    cairo_point_t to;

    to.x = p->x;
    to.y = p->y;

    status = filler->converter->add_edge (
	filler->converter,
	filler->current_point.x, filler->current_point.y,
	to.x, to.y);

    filler->current_point = to;

    return status;
}

static cairo_status_t
_cairo_path_fixed_fill_to_scan_converter (
    cairo_path_fixed_t			*path,
    double				 tolerance,
    cairo_scan_converter_t		*converter)
{
    scan_converter_filler_t filler;
    cairo_status_t status;

    scan_converter_filler_init (&filler, converter);

    status = _cairo_path_fixed_interpret_flat (
	path, CAIRO_DIRECTION_FORWARD,
	scan_converter_filler_move_to,
	scan_converter_filler_line_to,
	scan_converter_filler_close_path,
	&filler, tolerance);
    if (status)
	return status;

    return scan_converter_filler_close_path (&filler);
}

static cairo_scan_converter_t *
_create_scan_converter (cairo_fill_rule_t			 fill_rule,
			cairo_antialias_t			 antialias,
			const cairo_composite_rectangles_t	*rects)
{
    if (antialias == CAIRO_ANTIALIAS_NONE) {
	ASSERT_NOT_REACHED;
	return _cairo_scan_converter_create_in_error (
	    CAIRO_INT_STATUS_UNSUPPORTED);
    }
    else {
	return _cairo_tor_scan_converter_create (
	    rects->mask.x,
	    rects->mask.y,
	    rects->mask.x + rects->width,
	    rects->mask.y + rects->height,
	    fill_rule);
    }
}

cairo_status_t
_cairo_path_fixed_fill_using_spans (
    cairo_operator_t		 op,
    const cairo_pattern_t	*pattern,
    cairo_path_fixed_t		*path,
    cairo_surface_t		*dst,
    cairo_fill_rule_t		 fill_rule,
    double			 tolerance,
    cairo_antialias_t		 antialias,
    const cairo_composite_rectangles_t *rects)
{
    cairo_status_t status;
    cairo_span_renderer_t *renderer = _cairo_surface_create_span_renderer (
	op, pattern, dst, antialias, rects);
    cairo_scan_converter_t *converter = _create_scan_converter (
	fill_rule, antialias, rects);

    status = _cairo_path_fixed_fill_to_scan_converter (
	path, tolerance, converter);
    if (status)
	goto BAIL;

    status = converter->generate (converter, renderer);
    if (status)
	goto BAIL;

    status = renderer->finish (renderer);
    if (status)
	goto BAIL;

 BAIL:
    renderer->destroy (renderer);
    converter->destroy (converter);
    return status;
}

static void
_cairo_nil_destroy (void *abstract)
{
    (void) abstract;
}

static cairo_status_t
_cairo_nil_scan_converter_add_edge (void *abstract_converter,
				    cairo_fixed_t x1,
				    cairo_fixed_t y1,
				    cairo_fixed_t x2,
				    cairo_fixed_t y2)
{
    (void) abstract_converter;
    (void) x1;
    (void) y1;
    (void) x2;
    (void) y2;
    return _cairo_scan_converter_status (abstract_converter);
}

static cairo_status_t
_cairo_nil_scan_converter_generate (void *abstract_converter,
				    cairo_span_renderer_t *renderer)
{
    (void) abstract_converter;
    (void) renderer;
    return _cairo_scan_converter_status (abstract_converter);
}

cairo_status_t
_cairo_scan_converter_status (void *abstract_converter)
{
    cairo_scan_converter_t *converter = abstract_converter;
    return converter->status;
}

cairo_status_t
_cairo_scan_converter_set_error (void *abstract_converter,
				 cairo_status_t error)
{
    cairo_scan_converter_t *converter = abstract_converter;
    if (error == CAIRO_STATUS_SUCCESS)
	ASSERT_NOT_REACHED;
    if (converter->status == CAIRO_STATUS_SUCCESS) {
	converter->add_edge = _cairo_nil_scan_converter_add_edge;
	converter->generate = _cairo_nil_scan_converter_generate;
	converter->status = error;
    }
    return converter->status;
}

static void
_cairo_nil_scan_converter_init (cairo_scan_converter_t *converter,
				cairo_status_t status)
{
    converter->destroy = _cairo_nil_destroy;
    converter->status = CAIRO_STATUS_SUCCESS;
    status = _cairo_scan_converter_set_error (converter, status);
}

cairo_scan_converter_t *
_cairo_scan_converter_create_in_error (cairo_status_t status)
{
#define RETURN_NIL {\
	    static cairo_scan_converter_t nil;\
	    _cairo_nil_scan_converter_init (&nil, status);\
	    return &nil;\
	}
    switch (status) {
    case CAIRO_STATUS_SUCCESS:
    case CAIRO_STATUS_LAST_STATUS:
	ASSERT_NOT_REACHED;
	break;
    case CAIRO_STATUS_INVALID_RESTORE: RETURN_NIL;
    case CAIRO_STATUS_INVALID_POP_GROUP: RETURN_NIL;
    case CAIRO_STATUS_NO_CURRENT_POINT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_MATRIX: RETURN_NIL;
    case CAIRO_STATUS_INVALID_STATUS: RETURN_NIL;
    case CAIRO_STATUS_NULL_POINTER: RETURN_NIL;
    case CAIRO_STATUS_INVALID_STRING: RETURN_NIL;
    case CAIRO_STATUS_INVALID_PATH_DATA: RETURN_NIL;
    case CAIRO_STATUS_READ_ERROR: RETURN_NIL;
    case CAIRO_STATUS_WRITE_ERROR: RETURN_NIL;
    case CAIRO_STATUS_SURFACE_FINISHED: RETURN_NIL;
    case CAIRO_STATUS_SURFACE_TYPE_MISMATCH: RETURN_NIL;
    case CAIRO_STATUS_PATTERN_TYPE_MISMATCH: RETURN_NIL;
    case CAIRO_STATUS_INVALID_CONTENT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_FORMAT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_VISUAL: RETURN_NIL;
    case CAIRO_STATUS_FILE_NOT_FOUND: RETURN_NIL;
    case CAIRO_STATUS_INVALID_DASH: RETURN_NIL;
    case CAIRO_STATUS_INVALID_DSC_COMMENT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_INDEX: RETURN_NIL;
    case CAIRO_STATUS_CLIP_NOT_REPRESENTABLE: RETURN_NIL;
    case CAIRO_STATUS_TEMP_FILE_ERROR: RETURN_NIL;
    case CAIRO_STATUS_INVALID_STRIDE: RETURN_NIL;
    case CAIRO_STATUS_FONT_TYPE_MISMATCH: RETURN_NIL;
    case CAIRO_STATUS_USER_FONT_IMMUTABLE: RETURN_NIL;
    case CAIRO_STATUS_USER_FONT_ERROR: RETURN_NIL;
    case CAIRO_STATUS_NEGATIVE_COUNT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_CLUSTERS: RETURN_NIL;
    case CAIRO_STATUS_INVALID_SLANT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_WEIGHT: RETURN_NIL;
    case CAIRO_STATUS_NO_MEMORY: RETURN_NIL;
    case CAIRO_STATUS_INVALID_SIZE: RETURN_NIL;
    case CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED: RETURN_NIL;
    default:
	break;
    }
    status = CAIRO_STATUS_NO_MEMORY;
    RETURN_NIL;
#undef RETURN_NIL
}

static cairo_status_t
_cairo_nil_span_renderer_render_row (
    void				*abstract_renderer,
    int					 y,
    const cairo_half_open_span_t	*coverages,
    unsigned				 num_coverages)
{
    (void) y;
    (void) coverages;
    (void) num_coverages;
    return _cairo_span_renderer_status (abstract_renderer);
}

static cairo_status_t
_cairo_nil_span_renderer_finish (void *abstract_renderer)
{
    return _cairo_span_renderer_status (abstract_renderer);
}

cairo_status_t
_cairo_span_renderer_status (void *abstract_renderer)
{
    cairo_span_renderer_t *renderer = abstract_renderer;
    return renderer->status;
}

cairo_status_t
_cairo_span_renderer_set_error (
    void *abstract_renderer,
    cairo_status_t error)
{
    cairo_span_renderer_t *renderer = abstract_renderer;
    if (error == CAIRO_STATUS_SUCCESS) {
	ASSERT_NOT_REACHED;
    }
    if (renderer->status == CAIRO_STATUS_SUCCESS) {
	renderer->render_row = _cairo_nil_span_renderer_render_row;
	renderer->finish = _cairo_nil_span_renderer_finish;
	renderer->status = error;
    }
    return renderer->status;
}

static void
_cairo_nil_span_renderer_init (cairo_span_renderer_t *renderer,
			       cairo_status_t status)
{
    renderer->destroy = _cairo_nil_destroy;
    renderer->status = CAIRO_STATUS_SUCCESS;
    status = _cairo_span_renderer_set_error (renderer, status);
}

cairo_span_renderer_t *
_cairo_span_renderer_create_in_error (cairo_status_t status)
{
#define RETURN_NIL {\
	    static cairo_span_renderer_t nil;\
	    _cairo_nil_span_renderer_init (&nil, status);\
	    return &nil;\
	}
    switch (status) {
    case CAIRO_STATUS_SUCCESS:
    case CAIRO_STATUS_LAST_STATUS:
	ASSERT_NOT_REACHED;
	break;
    case CAIRO_STATUS_INVALID_RESTORE: RETURN_NIL;
    case CAIRO_STATUS_INVALID_POP_GROUP: RETURN_NIL;
    case CAIRO_STATUS_NO_CURRENT_POINT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_MATRIX: RETURN_NIL;
    case CAIRO_STATUS_INVALID_STATUS: RETURN_NIL;
    case CAIRO_STATUS_NULL_POINTER: RETURN_NIL;
    case CAIRO_STATUS_INVALID_STRING: RETURN_NIL;
    case CAIRO_STATUS_INVALID_PATH_DATA: RETURN_NIL;
    case CAIRO_STATUS_READ_ERROR: RETURN_NIL;
    case CAIRO_STATUS_WRITE_ERROR: RETURN_NIL;
    case CAIRO_STATUS_SURFACE_FINISHED: RETURN_NIL;
    case CAIRO_STATUS_SURFACE_TYPE_MISMATCH: RETURN_NIL;
    case CAIRO_STATUS_PATTERN_TYPE_MISMATCH: RETURN_NIL;
    case CAIRO_STATUS_INVALID_CONTENT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_FORMAT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_VISUAL: RETURN_NIL;
    case CAIRO_STATUS_FILE_NOT_FOUND: RETURN_NIL;
    case CAIRO_STATUS_INVALID_DASH: RETURN_NIL;
    case CAIRO_STATUS_INVALID_DSC_COMMENT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_INDEX: RETURN_NIL;
    case CAIRO_STATUS_CLIP_NOT_REPRESENTABLE: RETURN_NIL;
    case CAIRO_STATUS_TEMP_FILE_ERROR: RETURN_NIL;
    case CAIRO_STATUS_INVALID_STRIDE: RETURN_NIL;
    case CAIRO_STATUS_FONT_TYPE_MISMATCH: RETURN_NIL;
    case CAIRO_STATUS_USER_FONT_IMMUTABLE: RETURN_NIL;
    case CAIRO_STATUS_USER_FONT_ERROR: RETURN_NIL;
    case CAIRO_STATUS_NEGATIVE_COUNT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_CLUSTERS: RETURN_NIL;
    case CAIRO_STATUS_INVALID_SLANT: RETURN_NIL;
    case CAIRO_STATUS_INVALID_WEIGHT: RETURN_NIL;
    case CAIRO_STATUS_NO_MEMORY: RETURN_NIL;
    case CAIRO_STATUS_INVALID_SIZE: RETURN_NIL;
    case CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED: RETURN_NIL;
    default:
	break;
    }
    status = CAIRO_STATUS_NO_MEMORY;
    RETURN_NIL;
#undef RETURN_NIL
}
