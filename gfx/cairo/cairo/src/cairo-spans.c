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

#include "cairo-composite-rectangles-private.h"
#include "cairo-fixed-private.h"

static cairo_scan_converter_t *
_create_scan_converter (cairo_fill_rule_t			 fill_rule,
			cairo_antialias_t			 antialias,
			const cairo_composite_rectangles_t	*rects)
{
    if (antialias == CAIRO_ANTIALIAS_NONE) {
	ASSERT_NOT_REACHED;
	return NULL;
    }

    return _cairo_tor_scan_converter_create (rects->bounded.x,
					     rects->bounded.y,
					     rects->bounded.x + rects->bounded.width,
					     rects->bounded.y + rects->bounded.height,
					     fill_rule);
}

/* XXX Add me to the compositor interface. Ok, first create the compositor
 * interface, and then add this with associated fallback!
 */
cairo_status_t
_cairo_surface_composite_polygon (cairo_surface_t	*surface,
				  cairo_operator_t	 op,
				  const cairo_pattern_t	*pattern,
				  cairo_fill_rule_t	fill_rule,
				  cairo_antialias_t	antialias,
				  const cairo_composite_rectangles_t *rects,
				  cairo_polygon_t	*polygon,
				  cairo_region_t	*clip_region)
{
    cairo_span_renderer_t *renderer;
    cairo_scan_converter_t *converter;
    cairo_status_t status;

    converter = _create_scan_converter (fill_rule, antialias, rects);
    status = converter->add_polygon (converter, polygon);
    if (unlikely (status))
	goto CLEANUP_CONVERTER;

    renderer = _cairo_surface_create_span_renderer (op, pattern, surface,
						    antialias, rects,
						    clip_region);
    status = converter->generate (converter, renderer);
    if (unlikely (status))
	goto CLEANUP_RENDERER;

    status = renderer->finish (renderer);

 CLEANUP_RENDERER:
    renderer->destroy (renderer);
 CLEANUP_CONVERTER:
    converter->destroy (converter);
    return status;
}

static void
_cairo_nil_destroy (void *abstract)
{
    (void) abstract;
}

static cairo_status_t
_cairo_nil_scan_converter_add_polygon (void *abstract_converter,
				       const cairo_polygon_t *polygon)
{
    (void) abstract_converter;
    (void) polygon;
    return _cairo_scan_converter_status (abstract_converter);
}

static cairo_status_t
_cairo_nil_scan_converter_add_edge (void *abstract_converter,
				    const cairo_point_t *p1,
				    const cairo_point_t *p2,
				    int top, int bottom,
				    int dir)
{
    (void) abstract_converter;
    (void) p1;
    (void) p2;
    (void) top;
    (void) bottom;
    (void) dir;
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
	converter->add_polygon = _cairo_nil_scan_converter_add_polygon;
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
    case CAIRO_STATUS_DEVICE_TYPE_MISMATCH: RETURN_NIL;
    case CAIRO_STATUS_DEVICE_ERROR: RETURN_NIL;
    default:
	break;
    }
    status = CAIRO_STATUS_NO_MEMORY;
    RETURN_NIL;
#undef RETURN_NIL
}

static cairo_status_t
_cairo_nil_span_renderer_render_rows (
    void				*abstract_renderer,
    int					 y,
    int					 height,
    const cairo_half_open_span_t	*coverages,
    unsigned				 num_coverages)
{
    (void) y;
    (void) height;
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
	renderer->render_rows = _cairo_nil_span_renderer_render_rows;
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
    case CAIRO_STATUS_DEVICE_TYPE_MISMATCH: RETURN_NIL;
    case CAIRO_STATUS_DEVICE_ERROR: RETURN_NIL;
    default:
	break;
    }
    status = CAIRO_STATUS_NO_MEMORY;
    RETURN_NIL;
#undef RETURN_NIL
}
