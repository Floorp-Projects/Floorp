/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.og/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * Contributor(s):
 *      Robert Bragg <robert@linux.intel.com>
 */
#include "cairoint.h"

#include "cairo-cache-private.h"
#include "cairo-error-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-recording-surface-private.h"
#include "cairo-recording-surface-inline.h"
#include "cairo-fixed-private.h"
#include "cairo-device-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-image-surface-inline.h"
#include "cairo-cogl-private.h"
#include "cairo-cogl-gradient-private.h"
#include "cairo-arc-private.h"
#include "cairo-traps-private.h"
#include "cairo-surface-subsurface-inline.h"
#include "cairo-surface-fallback-private.h"
#include "cairo-surface-offset-private.h"

#include "cairo-cogl.h"

#include <cogl/cogl2-experimental.h>
#include <glib.h>

#define CAIRO_COGL_DEBUG 0
//#define DISABLE_BATCHING
#define MAX_JOURNAL_SIZE 100

#if CAIRO_COGL_DEBUG && __GNUC__
#define UNSUPPORTED(reason) ({ \
    g_warning ("cairo-cogl: hit unsupported operation: %s", reason); \
    CAIRO_INT_STATUS_UNSUPPORTED; \
})
#else
#define UNSUPPORTED(reason) CAIRO_INT_STATUS_UNSUPPORTED
#endif

#define CAIRO_COGL_PATH_META_CACHE_SIZE (1024 * 1024)

typedef struct _cairo_cogl_texture_attributes {
    /* nabbed from cairo_surface_attributes_t... */
    cairo_matrix_t	    matrix;
    cairo_extend_t	    extend;
    cairo_bool_t	    has_component_alpha;

    CoglPipelineFilter      filter;
    CoglPipelineWrapMode    s_wrap;
    CoglPipelineWrapMode    t_wrap;
} cairo_cogl_texture_attributes_t;

typedef struct _cairo_cogl_pipeline {
    int n_layers;
    cairo_operator_t op;

    /* These are not always the same as the operator's, as sometimes
     * this is a pre-render for a different operator */
    cairo_bool_t src_bounded;
    cairo_bool_t mask_bounded;

    cairo_bool_t has_src_tex_clip;
    cairo_path_fixed_t src_tex_clip;
    cairo_bool_t has_mask_tex_clip;
    cairo_path_fixed_t mask_tex_clip;
    cairo_rectangle_int_t unbounded_extents;

    CoglPipeline *pipeline;
} cairo_cogl_pipeline_t;

typedef enum _cairo_cogl_journal_entry_type {
    CAIRO_COGL_JOURNAL_ENTRY_TYPE_RECTANGLE,
    CAIRO_COGL_JOURNAL_ENTRY_TYPE_PRIMITIVE,
    CAIRO_COGL_JOURNAL_ENTRY_TYPE_CLIP
} cairo_cogl_journal_entry_type_t;

typedef struct _cairo_cogl_journal_entry {
    cairo_cogl_journal_entry_type_t type;
} cairo_cogl_journal_entry_t;

typedef struct _cairo_cogl_journal_clip_entry {
    cairo_cogl_journal_entry_t base;
    cairo_clip_t *clip;
} cairo_cogl_journal_clip_entry_t;

typedef struct _cairo_cogl_journal_rect_entry {
    cairo_cogl_journal_entry_t base;
    cairo_cogl_pipeline_t *pipeline;

    float x;
    float y;
    float width;
    float height;
    cairo_matrix_t ctm;
} cairo_cogl_journal_rect_entry_t;

typedef struct _cairo_cogl_journal_prim_entry {
    cairo_cogl_journal_entry_t base;
    cairo_cogl_pipeline_t *pipeline;

    CoglPrimitive *primitive;
    cairo_matrix_t transform;
} cairo_cogl_journal_prim_entry_t;

typedef struct _cairo_cogl_path_fill_meta {
    cairo_cache_entry_t	base;
    cairo_path_fixed_t path;
    cairo_fill_rule_t fill_rule;
    double tolerance;

    CoglPrimitive *prim;

    cairo_freelist_t *freelist;
} cairo_cogl_path_fill_meta_t;

typedef struct _cairo_cogl_path_stroke_meta {
    cairo_cache_entry_t	base;
    cairo_path_fixed_t path;
    cairo_stroke_style_t style;
    double tolerance;

    CoglPrimitive *prim;

    cairo_freelist_t *freelist;
} cairo_cogl_path_stroke_meta_t;

static cairo_surface_t *
_cairo_cogl_surface_create_full (cairo_cogl_device_t *dev,
				 cairo_content_t      content,
				 CoglFramebuffer     *framebuffer,
				 CoglTexture         *texture);

static void
_cairo_cogl_journal_flush (cairo_cogl_surface_t *surface);

cairo_private extern const cairo_surface_backend_t _cairo_cogl_surface_backend;

slim_hidden_proto (cairo_cogl_device_create);
slim_hidden_proto (cairo_cogl_surface_create_for_fb);
slim_hidden_proto (cairo_cogl_onscreen_surface_create);
slim_hidden_proto (cairo_cogl_offscreen_surface_create);
slim_hidden_proto (cairo_cogl_surface_get_framebuffer);
slim_hidden_proto (cairo_cogl_surface_get_texture);
slim_hidden_proto (cairo_cogl_surface_end_frame);
slim_hidden_proto (cairo_cogl_surface_synchronize);

static cairo_cogl_device_t *
to_device (cairo_device_t *device)
{
    return (cairo_cogl_device_t *)device;
}

/* moves trap points such that they become the actual corners of the trapezoid */
static void
_sanitize_trap (cairo_trapezoid_t *t)
{
    cairo_trapezoid_t s = *t;

#define FIX(lr, tb, p) \
    if (t->lr.p.y != t->tb) { \
        t->lr.p.x = s.lr.p2.x + _cairo_fixed_mul_div_floor (s.lr.p1.x - s.lr.p2.x, s.tb - s.lr.p2.y, s.lr.p1.y - s.lr.p2.y); \
        t->lr.p.y = s.tb; \
    }
    FIX (left,  top,    p1);
    FIX (left,  bottom, p2);
    FIX (right, top,    p1);
    FIX (right, bottom, p2);
}

static cairo_status_t
_cairo_cogl_surface_ensure_framebuffer (cairo_cogl_surface_t *surface)
{
    CoglError *error = NULL;

    if (surface->framebuffer)
	return CAIRO_STATUS_SUCCESS;

    surface->framebuffer =
        cogl_offscreen_new_with_texture (surface->texture);
    if (unlikely (!cogl_framebuffer_allocate (surface->framebuffer,
                                              &error)))
    {
        g_warning ("Could not create framebuffer for surface: %s",
                   error->message);
        cogl_error_free (error);
	cogl_object_unref (surface->framebuffer);
	surface->framebuffer = NULL;
	return CAIRO_STATUS_NO_MEMORY;
    }

    cogl_framebuffer_orthographic (surface->framebuffer, 0, 0,
                                   cogl_texture_get_width (surface->texture),
                                   cogl_texture_get_height (surface->texture),
                                   -1, 100);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_cogl_surface_get_extents (void                  *abstract_surface,
                                 cairo_rectangle_int_t *extents)
{
    cairo_cogl_surface_t *surface = abstract_surface;

    extents->x = 0;
    extents->y = 0;
    extents->width  = surface->width;
    extents->height = surface->height;

    return TRUE;
}

/* Taken from cairo-surface-clipper.c */
static cairo_status_t
_cairo_path_fixed_add_box (cairo_path_fixed_t *path,
			   const cairo_box_t *box)
{
    cairo_status_t status;

    status = _cairo_path_fixed_move_to (path, box->p1.x, box->p1.y);
    if (unlikely (status))
	return status;

    status = _cairo_path_fixed_line_to (path, box->p2.x, box->p1.y);
    if (unlikely (status))
	return status;

    status = _cairo_path_fixed_line_to (path, box->p2.x, box->p2.y);
    if (unlikely (status))
	return status;

    status = _cairo_path_fixed_line_to (path, box->p1.x, box->p2.y);
    if (unlikely (status))
	return status;

    return _cairo_path_fixed_close_path (path);
}

static void
_cairo_cogl_journal_discard (cairo_cogl_surface_t *surface)
{
    GList *l;

    if (!surface->journal) {
	assert (surface->last_clip == NULL);
	return;
    }

    for (l = surface->journal->head; l; l = l->next) {
	cairo_cogl_journal_entry_t *entry = l->data;
	gsize entry_size;

	switch (entry->type)
	{
	case CAIRO_COGL_JOURNAL_ENTRY_TYPE_CLIP: {
	    cairo_cogl_journal_clip_entry_t *clip_entry =
		(cairo_cogl_journal_clip_entry_t *)entry;
	    _cairo_clip_destroy (clip_entry->clip);
	    entry_size = sizeof (cairo_cogl_journal_clip_entry_t);
	    break;
	}
	case CAIRO_COGL_JOURNAL_ENTRY_TYPE_RECTANGLE: {
	    cairo_cogl_journal_rect_entry_t *rect_entry =
		(cairo_cogl_journal_rect_entry_t *)entry;
	    cogl_object_unref (rect_entry->pipeline->pipeline);
            if (rect_entry->pipeline->has_src_tex_clip)
                _cairo_path_fixed_fini (&rect_entry->pipeline->src_tex_clip);
            if (rect_entry->pipeline->has_mask_tex_clip)
                _cairo_path_fixed_fini (&rect_entry->pipeline->mask_tex_clip);
            g_free (rect_entry->pipeline);
	    entry_size = sizeof (cairo_cogl_journal_rect_entry_t);
	    break;
	}
	case CAIRO_COGL_JOURNAL_ENTRY_TYPE_PRIMITIVE: {
	    cairo_cogl_journal_prim_entry_t *prim_entry =
		(cairo_cogl_journal_prim_entry_t *)entry;
	    cogl_object_unref (prim_entry->pipeline->pipeline);
            if (prim_entry->primitive)
	        cogl_object_unref (prim_entry->primitive);
            if (prim_entry->pipeline->has_src_tex_clip)
                _cairo_path_fixed_fini (&prim_entry->pipeline->src_tex_clip);
            if (prim_entry->pipeline->has_mask_tex_clip)
                _cairo_path_fixed_fini (&prim_entry->pipeline->mask_tex_clip);
            g_free (prim_entry->pipeline);
	    entry_size = sizeof (cairo_cogl_journal_prim_entry_t);
	    break;
	}
	default:
	    assert (0); /* not reached! */
	    entry_size = 0; /* avoid compiler warning */
	}
	g_slice_free1 (entry_size, entry);
    }

    g_queue_clear (surface->journal);

    if (surface->last_clip) {
	_cairo_clip_destroy (surface->last_clip);
	surface->last_clip = NULL;
    }
}

static void
_cairo_cogl_journal_free (cairo_cogl_surface_t *surface)
{
    _cairo_cogl_journal_discard (surface);
    g_queue_free (surface->journal);
    surface->journal = NULL;
}

static void
_cairo_cogl_journal_log_primitive (cairo_cogl_surface_t  *surface,
				   cairo_cogl_pipeline_t *pipeline,
				   CoglPrimitive         *primitive,
				   cairo_matrix_t        *transform)
{
    cairo_cogl_journal_prim_entry_t *entry;

    if (unlikely (surface->journal == NULL))
	surface->journal = g_queue_new ();

    /* FIXME: Instead of a GList here we should stack allocate the journal
     * entries so it would be cheaper to allocate and they can all be freed in
     * one go after flushing! */
    entry = g_slice_new (cairo_cogl_journal_prim_entry_t);
    entry->base.type = CAIRO_COGL_JOURNAL_ENTRY_TYPE_PRIMITIVE;

    entry->pipeline = pipeline;

    entry->primitive = primitive;
    if (primitive)
        cogl_object_ref (primitive);

    entry->transform = *transform;

    g_queue_push_tail (surface->journal, entry);

#ifdef DISABLE_BATCHING
    _cairo_cogl_journal_flush (surface);
#else
    /* To avoid consuming too much memory, flush the journal if it gets
     * to a certain size */
    if (g_queue_get_length (surface->journal) > MAX_JOURNAL_SIZE)
        _cairo_cogl_journal_flush (surface);
#endif
}

static void
_cairo_cogl_journal_log_rectangle (cairo_cogl_surface_t  *surface,
				   cairo_cogl_pipeline_t *pipeline,
				   float                  x,
				   float                  y,
				   float                  width,
				   float                  height,
				   cairo_matrix_t        *ctm)
{
    cairo_cogl_journal_rect_entry_t *entry;

    if (unlikely (surface->journal == NULL))
	surface->journal = g_queue_new ();

    /* FIXME: Instead of a GList here we should stack allocate the journal
     * entries so it would be cheaper to allocate and they can all be freed in
     * one go after flushing! */
    entry = g_slice_new (cairo_cogl_journal_rect_entry_t);
    entry->base.type = CAIRO_COGL_JOURNAL_ENTRY_TYPE_RECTANGLE;

    entry->pipeline = pipeline;

    entry->x = x;
    entry->y = y;
    entry->width = width;
    entry->height = height;
    entry->ctm = *ctm;

    g_queue_push_tail (surface->journal, entry);

#ifdef DISABLE_BATCHING
    _cairo_cogl_journal_flush (surface);
#else
    /* To avoid consuming too much memory, flush the journal if it gets
     * to a certain size */
    if (g_queue_get_length (surface->journal) > MAX_JOURNAL_SIZE)
        _cairo_cogl_journal_flush (surface);
#endif
}

static void
_cairo_cogl_journal_log_clip (cairo_cogl_surface_t *surface,
			      const cairo_clip_t   *clip)
{
    cairo_cogl_journal_clip_entry_t *entry;

    if (unlikely (surface->journal == NULL))
	surface->journal = g_queue_new ();

    /* FIXME: Instead of a GList here we should stack allocate the journal
     * entries so it would be cheaper to allocate and they can all be freed in
     * one go after flushing! */
    entry = g_slice_new (cairo_cogl_journal_clip_entry_t);
    entry->base.type = CAIRO_COGL_JOURNAL_ENTRY_TYPE_CLIP;
    entry->clip = _cairo_clip_copy (clip);

    g_queue_push_tail (surface->journal, entry);
}

static CoglAttributeBuffer *
_cairo_cogl_device_allocate_buffer_space (cairo_cogl_device_t *dev,
                                          size_t               size,
                                          size_t              *offset,
                                          void               **pointer)
{
    /* XXX: In the Cogl journal we found it more efficient to have a pool of
     * buffers that we re-cycle but for now we simply throw away our stack
     * buffer each time we flush. */
    if (unlikely (dev->buffer_stack &&
		  (dev->buffer_stack_size - dev->buffer_stack_offset) < size)) {
	cogl_buffer_unmap (dev->buffer_stack);
	cogl_object_unref (dev->buffer_stack);
	dev->buffer_stack = NULL;
	dev->buffer_stack_size *= 2;
    }

    if (unlikely (dev->buffer_stack_size < size))
	dev->buffer_stack_size = size * 2;

    if (unlikely (dev->buffer_stack == NULL)) {
	dev->buffer_stack =
            cogl_attribute_buffer_new (dev->cogl_context,
                                       dev->buffer_stack_size,
                                       NULL);
	dev->buffer_stack_pointer =
	    cogl_buffer_map (dev->buffer_stack,
			     COGL_BUFFER_ACCESS_WRITE,
			     COGL_BUFFER_MAP_HINT_DISCARD);
	dev->buffer_stack_offset = 0;
    }

    *pointer = dev->buffer_stack_pointer + dev->buffer_stack_offset;
    *offset = dev->buffer_stack_offset;

    dev->buffer_stack_offset += size;
    return cogl_object_ref (dev->buffer_stack);
}


static CoglAttributeBuffer *
_cairo_cogl_traps_to_triangles_buffer (cairo_cogl_surface_t *surface,
				       cairo_traps_t        *traps,
				       size_t               *offset,
				       cairo_bool_t          one_shot)
{
    CoglAttributeBuffer *buffer;
    int n_traps = traps->num_traps;
    int i;
    CoglVertexP2 *triangles;
    cairo_cogl_device_t *dev = to_device(surface->base.device);

    if (one_shot) {
	buffer =
            _cairo_cogl_device_allocate_buffer_space (dev,
                                                      n_traps * sizeof (CoglVertexP2) * 6,
                                                      offset,
                                                      (void **)&triangles);
	if (!buffer)
	    return NULL;
    } else {
	buffer =
            cogl_attribute_buffer_new (dev->cogl_context,
                                       n_traps * sizeof (CoglVertexP2) * 6,
                                       NULL);
	if (!buffer)
	    return NULL;
	triangles = cogl_buffer_map (buffer,
				     COGL_BUFFER_ACCESS_WRITE,
				     COGL_BUFFER_MAP_HINT_DISCARD);
	if (!triangles)
	    return NULL;
	*offset = 0;
    }

    /* XXX: This is can be very expensive. I'm not sure a.t.m if it's
     * predominantly the bandwidth required or the cost of the fixed_to_float
     * conversions but either way we should try using an index buffer to
     * reduce the amount we upload by 1/3 (offset by allocating and uploading
     * indices though) sadly though my experience with the intel mesa drivers
     * is that slow paths can easily be hit when starting to use indices.
     */
    for (i = 0; i < n_traps; i++)
    {
	CoglVertexP2 *p = &triangles[i * 6];
	cairo_trapezoid_t *trap = &traps->traps[i];

	p[0].x = _cairo_fixed_to_double (trap->left.p1.x);
	p[0].y = _cairo_fixed_to_double (trap->left.p1.y);

	p[1].x = _cairo_fixed_to_double (trap->left.p2.x);
	p[1].y = _cairo_fixed_to_double (trap->left.p2.y);

	p[2].x = _cairo_fixed_to_double (trap->right.p2.x);
	p[2].y = _cairo_fixed_to_double (trap->right.p2.y);

	p[3].x = p[0].x;
	p[3].y = p[0].y;

	p[4].x = p[2].x;
	p[4].y = p[2].y;

	p[5].x = _cairo_fixed_to_double (trap->right.p1.x);
	p[5].y = _cairo_fixed_to_double (trap->right.p1.y);
    }

    if (!one_shot)
	cogl_buffer_unmap (buffer);

    return buffer;
}

static CoglPrimitive *
_cairo_cogl_traps_to_composite_prim (cairo_cogl_surface_t *surface,
				     cairo_traps_t        *traps,
				     cairo_bool_t          one_shot)
{
    int n_traps = traps->num_traps;
    size_t offset;
    CoglAttributeBuffer *buffer;
    CoglPrimitive *prim;
    CoglAttribute *attributes[3];
    const char *attrib_names[3] = {"cogl_position_in",
                                   "cogl_tex_coord0_in",
                                   "cogl_tex_coord1_in"};
    int i;

    /* XXX: Ideally we would skip tessellating to traps entirely since
     * given their representation, conversion to triangles is quite expensive.
     *
     * This simplifies the conversion to triangles by making the end points of
     * the two side lines actually just correspond to the corners of the
     * traps.
     */
    for (i = 0; i < n_traps; i++)
	_sanitize_trap (&traps->traps[i]);

    buffer = _cairo_cogl_traps_to_triangles_buffer (surface,
                                                    traps,
                                                    &offset,
                                                    one_shot);

    /* We create the largest used number of texture coordinate
     * attributes here to ensure that when cached, they can be reused
     * with any pipeline that we may associate with a given path. If
     * the corresponding layer for a texture coordinates attribute
     * doesn't exist, cogl simply ignores it. Since all of the
     * attributes are aliasing the same attribute buffer, the overhead
     * of specifying them is minimal. */
    for (i = 0; i < 3; i++) {
        attributes[i] = cogl_attribute_new (buffer,
                                            attrib_names[i],
                                            sizeof (CoglVertexP2),
                                            offset,
                                            2,
                                            COGL_ATTRIBUTE_TYPE_FLOAT);
    }

    /* The attributes will have taken references on the buffer */
    cogl_object_unref (buffer);

    prim =
        cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
			                    n_traps * 6, attributes, 3);

    /* The primitive will now keep the attributes alive... */
    for (i = 0; i < 3; i++)
        cogl_object_unref (attributes[i]);

    return prim;
}

/* In order to facilitate path caching, we transform the input path
 * into a form that will make all translations and rotations of a given
 * path identical, thereby allowing them to be identified with
 * conventional path hashing and equivalence functions. A
 * transformation matrix is also output so that the path can be
 * transformed back into its original form during rendering. */
static cairo_int_status_t
_cairo_cogl_get_untransformed_path (cairo_path_fixed_t       *copy,
                                    const cairo_path_fixed_t *orig,
                                    cairo_matrix_t           *transform_out)
{
    cairo_matrix_t transform;
    cairo_int_status_t status;

    if (orig->buf.base.num_points < 1)
        return CAIRO_INT_STATUS_NOTHING_TO_DO;

    status = _cairo_path_fixed_init_copy (copy, orig);
    if (unlikely (status))
        return status;

    /* First the path is translated so that its first point lies on the
     * origin. */
    cairo_matrix_init_translate (&transform,
                                 -_cairo_fixed_to_double(orig->buf.points[0].x),
                                 -_cairo_fixed_to_double(orig->buf.points[0].y));

    /* Then the path is rotated so that its second point lies on the
     * x axis. */
    if (orig->buf.base.num_points > 1) {
        double x = _cairo_fixed_to_double(orig->buf.points[1].x) -
                   _cairo_fixed_to_double(orig->buf.points[0].x);
        double y = _cairo_fixed_to_double(orig->buf.points[1].y) -
                   _cairo_fixed_to_double(orig->buf.points[0].y);
        double hyp = sqrt (x * x + y * y);

        transform.xx = x / hyp;
        transform.yy = x / hyp;
        transform.xy = -y / hyp;
        transform.yx = y / hyp;
    }

    _cairo_path_fixed_transform (copy, &transform);

    *transform_out = transform;
    status = cairo_matrix_invert (transform_out);
    if (unlikely (status)) {
        _cairo_path_fixed_fini (copy);
        return status;
    }

    return CAIRO_INT_STATUS_SUCCESS;
}

static void
_cairo_cogl_path_fill_meta_destroy (cairo_cogl_path_fill_meta_t *meta)
{
    _cairo_path_fixed_fini (&meta->path);
    cogl_object_unref (meta->prim);

    _cairo_freelist_free (meta->freelist, meta);
}

static cairo_bool_t
_cairo_cogl_path_fill_meta_equal (const void *key_a, const void *key_b)
{
    const cairo_cogl_path_fill_meta_t *meta0 = key_a;
    const cairo_cogl_path_fill_meta_t *meta1 = key_b;

    if (meta0->fill_rule != meta1->fill_rule)
        return FALSE;

    if (meta0->tolerance != meta1->tolerance)
        return FALSE;

    if (!_cairo_path_fixed_equal (&meta0->path, &meta1->path))
        return FALSE;

    return TRUE;
}

static cairo_int_status_t
_cairo_cogl_fill_to_primitive (cairo_cogl_surface_t	*surface,
			       const cairo_path_fixed_t	*path,
			       cairo_fill_rule_t	 fill_rule,
			       double			 tolerance,
			       cairo_bool_t		 one_shot,
			       CoglPrimitive	       **primitive,
                               cairo_matrix_t           *transform)
{
    cairo_traps_t traps;
    cairo_int_status_t status;
    cairo_cogl_path_fill_meta_t meta;
    cairo_cogl_path_fill_meta_t *acquired_meta;
    cairo_cogl_path_fill_meta_t *insert_meta = NULL;
    cairo_cogl_device_t *dev = to_device (surface->base.device);
    unsigned long hash;

    *primitive = NULL;

    status = _cairo_cogl_get_untransformed_path (&meta.path,
                                                 path,
                                                 transform);
    if (unlikely (status))
        return status;

    hash = _cairo_path_fixed_hash (&meta.path);
    hash = _cairo_hash_bytes (hash, &fill_rule, sizeof (fill_rule));
    hash = _cairo_hash_bytes (hash, &tolerance, sizeof (tolerance));
    meta.base.hash = hash;
    meta.tolerance = tolerance;
    meta.fill_rule = fill_rule;

    acquired_meta = _cairo_cache_lookup (&dev->path_fill_prim_cache,
                                         &meta.base);

    if (acquired_meta) {
        // g_print ("fill cache hit");
        *primitive = cogl_object_ref (acquired_meta->prim);
        _cairo_path_fixed_fini (&meta.path);
        return CAIRO_STATUS_SUCCESS;
    }

    _cairo_traps_init (&traps);
    status = _cairo_path_fixed_fill_to_traps (&meta.path,
                                              fill_rule,
                                              tolerance,
                                              &traps);
    if (unlikely (status))
	goto BAIL;

    if (traps.num_traps == 0) {
	status = CAIRO_INT_STATUS_NOTHING_TO_DO;
	goto BAIL;
    }

    *primitive = _cairo_cogl_traps_to_composite_prim (surface,
                                                      &traps,
                                                      one_shot);
    if (unlikely (!*primitive)) {
	status = CAIRO_INT_STATUS_NO_MEMORY;
	goto BAIL;
    }

    insert_meta =
        _cairo_freelist_alloc (&dev->path_fill_meta_freelist);
    if (unlikely (!insert_meta)) {
        status = CAIRO_INT_STATUS_NO_MEMORY;
        goto BAIL;
    }

    insert_meta->base.hash = meta.base.hash;
    insert_meta->base.size =
        traps.num_traps * sizeof (CoglVertexP2) * 6;
    insert_meta->tolerance = tolerance;
    insert_meta->fill_rule = fill_rule;
    insert_meta->prim = cogl_object_ref (*primitive);
    insert_meta->freelist = &dev->path_fill_meta_freelist;

    status = _cairo_path_fixed_init_copy (&insert_meta->path,
                                          &meta.path);
    if (unlikely (status))
        goto BAIL;

    if (unlikely (_cairo_cache_insert (&dev->path_fill_prim_cache,
                                       &insert_meta->base)))
    {
        g_warning ("Fill primitive cache insertion unsuccessful");
        goto BAIL;
    }

    _cairo_path_fixed_fini (&meta.path);
    _cairo_traps_fini (&traps);

    return status;

BAIL:
    if (*primitive) {
        cogl_object_unref (*primitive);
        *primitive = NULL;
    }
    if (insert_meta)
        _cairo_cogl_path_fill_meta_destroy (insert_meta);
    _cairo_path_fixed_fini (&meta.path);
    _cairo_traps_fini (&traps);

    return status;
}

static cairo_bool_t
_cairo_cogl_stroke_style_equal (const cairo_stroke_style_t *a,
			        const cairo_stroke_style_t *b)
{
    if (a->line_width == b->line_width &&
	a->line_cap == b->line_cap &&
	a->line_join == b->line_join &&
	a->miter_limit == b->miter_limit &&
	a->num_dashes == b->num_dashes &&
	a->dash_offset == b->dash_offset)
    {
	unsigned int i;
	for (i = 0; i < a->num_dashes; i++) {
	    if (a->dash[i] != b->dash[i])
		return FALSE;
	}
    }
    return TRUE;
}

static cairo_bool_t
_cairo_cogl_path_stroke_meta_equal (const void *key_a,
                                    const void *key_b)
{
    const cairo_cogl_path_stroke_meta_t *meta0 = key_a;
    const cairo_cogl_path_stroke_meta_t *meta1 = key_b;

    if (meta0->tolerance != meta1->tolerance)
        return FALSE;

    if (!_cairo_cogl_stroke_style_equal (&meta0->style, &meta1->style))
        return FALSE;

    if (!_cairo_path_fixed_equal (&meta0->path, &meta1->path))
        return FALSE;

    return TRUE;
}

static void
_cairo_cogl_path_stroke_meta_destroy (cairo_cogl_path_stroke_meta_t *meta)
{
    _cairo_stroke_style_fini (&meta->style);
    _cairo_path_fixed_fini (&meta->path);
    cogl_object_unref (meta->prim);

    _cairo_freelist_free (meta->freelist, meta);
}

static unsigned long
_cairo_cogl_stroke_style_hash (unsigned long               hash,
			       const cairo_stroke_style_t *style)
{
    unsigned int i;
    hash = _cairo_hash_bytes (hash, &style->line_width, sizeof (style->line_width));
    hash = _cairo_hash_bytes (hash, &style->line_cap, sizeof (style->line_cap));
    hash = _cairo_hash_bytes (hash, &style->line_join, sizeof (style->line_join));
    hash = _cairo_hash_bytes (hash, &style->miter_limit, sizeof (style->miter_limit));
    hash = _cairo_hash_bytes (hash, &style->num_dashes, sizeof (style->num_dashes));
    hash = _cairo_hash_bytes (hash, &style->dash_offset, sizeof (style->dash_offset));
    for (i = 0; i < style->num_dashes; i++)
	hash = _cairo_hash_bytes (hash, &style->dash[i], sizeof (double));
    return hash;
}

static cairo_int_status_t
_cairo_cogl_stroke_to_primitive (cairo_cogl_surface_t	    *surface,
				 const cairo_path_fixed_t   *path,
				 const cairo_stroke_style_t *style,
				 double			     tolerance,
				 cairo_bool_t		     one_shot,
				 CoglPrimitive		   **primitive,
                                 cairo_matrix_t             *transform)
{
    cairo_traps_t traps;
    cairo_int_status_t status;
    cairo_cogl_path_stroke_meta_t meta;
    cairo_cogl_path_stroke_meta_t *acquired_meta;
    cairo_cogl_path_stroke_meta_t *insert_meta = NULL;
    cairo_matrix_t identity;
    cairo_cogl_device_t *dev = to_device (surface->base.device);
    unsigned long hash;

    *primitive = NULL;

    status = _cairo_cogl_get_untransformed_path (&meta.path,
                                                 path,
                                                 transform);
    if (unlikely (status))
        return status;

    hash = _cairo_path_fixed_hash (&meta.path);
    hash = _cairo_cogl_stroke_style_hash (hash, style);
    hash = _cairo_hash_bytes (hash, &tolerance, sizeof (tolerance));
    meta.base.hash = hash;
    meta.tolerance = tolerance;

    status = _cairo_stroke_style_init_copy (&meta.style, style);
    if (unlikely (status)) {
        _cairo_path_fixed_fini (&meta.path);
        return status;
    }

    acquired_meta = _cairo_cache_lookup (&dev->path_stroke_prim_cache,
                                         &meta.base);

    if (acquired_meta) {
        // g_print ("stroke cache hit");
        *primitive = cogl_object_ref (acquired_meta->prim);
        _cairo_path_fixed_fini (&meta.path);
        return CAIRO_STATUS_SUCCESS;
    }

    _cairo_traps_init (&traps);

    cairo_matrix_init_identity (&identity);
    status = _cairo_path_fixed_stroke_polygon_to_traps (&meta.path,
                                                        style,
                                                        &identity,
                                                        &identity,
                                                        tolerance,
                                                        &traps);
    if (unlikely (status))
	goto BAIL;

    if (traps.num_traps == 0) {
	status = CAIRO_INT_STATUS_NOTHING_TO_DO;
	goto BAIL;
    }

    *primitive = _cairo_cogl_traps_to_composite_prim (surface,
                                                      &traps,
                                                      one_shot);
    if (unlikely (!*primitive)) {
	status = CAIRO_INT_STATUS_NO_MEMORY;
	goto BAIL;
    }

    insert_meta =
        _cairo_freelist_alloc (&dev->path_stroke_meta_freelist);
    if (unlikely (!insert_meta)) {
        status = CAIRO_INT_STATUS_NO_MEMORY;
        goto BAIL;
    }

    insert_meta->base.hash = meta.base.hash;
    insert_meta->base.size =
        traps.num_traps * sizeof (CoglVertexP2) * 6;
    insert_meta->tolerance = tolerance;
    insert_meta->prim = cogl_object_ref (*primitive);
    insert_meta->freelist = &dev->path_stroke_meta_freelist;

    status = _cairo_stroke_style_init_copy (&insert_meta->style,
                                            style);
    if (unlikely (status)) {
        _cairo_stroke_style_fini (&insert_meta->style);
        free (insert_meta);
        insert_meta = NULL;
        goto BAIL;
    }

    status = _cairo_path_fixed_init_copy (&insert_meta->path,
                                          &meta.path);
    if (unlikely (status))
        goto BAIL;

    if (unlikely (_cairo_cache_insert (&dev->path_stroke_prim_cache,
                                       &insert_meta->base)))
    {
        g_warning ("Stroke primitive cache insertion unsuccessful");
        goto BAIL;
    }

    _cairo_path_fixed_fini (&meta.path);
    _cairo_stroke_style_fini (&meta.style);
    _cairo_traps_fini (&traps);

    return status;

BAIL:
    if (*primitive) {
        cogl_object_unref (*primitive);
        *primitive = NULL;
    }
    if (insert_meta)
        _cairo_cogl_path_stroke_meta_destroy (insert_meta);
    _cairo_path_fixed_fini (&meta.path);
    _cairo_stroke_style_fini (&meta.style);
    _cairo_traps_fini (&traps);

    return status;
}

static void
_cairo_cogl_set_path_prim_clip (cairo_cogl_surface_t *surface,
                                cairo_path_fixed_t   *path,
                                int                  *clip_stack_depth,
                                cairo_fill_rule_t     fill_rule,
                                double                tolerance)
{
    cairo_rectangle_int_t extents;
    cairo_int_status_t status;
    CoglPrimitive *prim;
    cairo_matrix_t transform;
    CoglMatrix matrix;
    double x1, y1, x2, y2;

    status = _cairo_cogl_fill_to_primitive (surface,
                                            path,
                                            fill_rule,
                                            tolerance,
                                            FALSE,
                                            &prim,
                                            &transform);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO) {
        /* If the clip is of zero fill area, set all clipped */
        cogl_framebuffer_push_scissor_clip (surface->framebuffer,
                                            0, 0, 0, 0);
        (*clip_stack_depth)++;
        return;
    } else if (unlikely (status)) {
        g_warning ("Failed to get primitive for clip path while "
                   "flushing journal");
        goto BAIL;
    }

    float transformfv[16] = {
        transform.xx, transform.yx, 0, 0,
        transform.xy, transform.yy, 0, 0,
        0,            0,            1, 0,
        transform.x0, transform.y0, 0, 1
    };

    cogl_matrix_init_from_array (&matrix, transformfv);

    cogl_framebuffer_push_matrix (surface->framebuffer);
    cogl_framebuffer_transform (surface->framebuffer, &matrix);

    _cairo_path_fixed_approximate_clip_extents (path, &extents);

    /* The extents need to be transformed by the inverse of the
     * modelview matrix because they are in terms of un-transformed
     * device coordinates and the bounds coordinates will be
     * transformed by the modelview matrix */
    status = cairo_matrix_invert (&transform);
    if (unlikely (status)) {
        g_warning ("Could not apply clip due to invalid matrix from "
                   "path transformation");
        goto BAIL;
    }

    x1 = extents.x;
    y1 = extents.y;
    x2 = extents.x + extents.width;
    y2 = extents.y + extents.height;
    _cairo_matrix_transform_bounding_box (&transform,
                                          &x1, &y1, &x2, &y2,
                                          NULL);

    cogl_framebuffer_push_primitive_clip (surface->framebuffer, prim,
                                          x1, y1, x2, y2);
    (*clip_stack_depth)++;

    cogl_framebuffer_pop_matrix (surface->framebuffer);

BAIL:
    if (prim)
        cogl_object_unref (prim);
}

/* This is the way in which we handle CAIRO_EXTEND_NONE set on the
 * source or mask pattern surfaces, as well as unbounded operators.
 * First, we limit the rendering area to the region which will not be
 * sampled from beyond the source or mask textures with additional clip
 * paths, which were created when we obtained the original pipeline.
 * The region will also be limited by the drawing area due to the fact
 * we are drawing with the original primitive's vertices.
 *
 * In order to handle unbounded operators, we do a second rendering pass
 * for places outside of such region. We limit the rending to outside
 * this region by using a depth buffer to preserve all places where
 * rendering took place during the first pass. For this region, we also
 * have to remove the CAIRO_EXTEND_NONE clips if the operator is not
 * bound by their respective contents. Because OpenGL sets all vertex
 * z-values to 0.0 if none are supplied in the attributes data (we only
 * supply x and y values), it will update the region in the buffer to a
 * value over the default clearing value of 1.0. Given that the default
 * test function is GL_LESS, we don't have to set z attributes on the
 * vertices of the second rendering pass either, as 0.0 will never be
 * less than 0.0. If cogl ever adds a method to clip out a primitive
 * instead of just clipping it in, we may be able to use a more
 * efficient method using the stencil buffer. */
static void
_cairo_cogl_apply_tex_clips (cairo_cogl_surface_t  *surface,
                             int                   *clip_stack_depth,
                             cairo_cogl_pipeline_t *pipeline)
{
    CoglDepthState depth_state;
    CoglBool cogl_status;

    /* Enable the depth test if it will be needed */
    if ((!pipeline->mask_bounded && pipeline->has_mask_tex_clip) ||
        (!pipeline->src_bounded && pipeline->has_src_tex_clip))
    {
        cogl_depth_state_init (&depth_state);
        cogl_depth_state_set_test_enabled (&depth_state, TRUE);
        cogl_status = cogl_pipeline_set_depth_state (pipeline->pipeline,
                                                     &depth_state,
                                                     NULL);
        if (unlikely (cogl_status != TRUE))
            g_warning ("Error setting depth state for unbounded render");

        /* Clear the depth buffer to 1.0. The color values are unused
         * placeholders. */
        cogl_framebuffer_clear4f (surface->framebuffer,
                                  COGL_BUFFER_BIT_DEPTH,
                                  0.0, 0.0, 0.0, 0.0);
    }

    if (pipeline->mask_bounded && !pipeline->src_bounded) {
        /* Push mask clip first so later we can pop the source clip
         * and still be bound by the mask clip */
        if (pipeline->has_mask_tex_clip)
            _cairo_cogl_set_path_prim_clip (surface,
                                            &pipeline->mask_tex_clip,
                                            clip_stack_depth,
                                            CAIRO_FILL_RULE_WINDING,
                                            0.0);
        if (pipeline->has_src_tex_clip)
            _cairo_cogl_set_path_prim_clip (surface,
                                            &pipeline->src_tex_clip,
                                            clip_stack_depth,
                                            CAIRO_FILL_RULE_WINDING,
                                            0.0);
    } else {
        if (pipeline->has_src_tex_clip)
            _cairo_cogl_set_path_prim_clip (surface,
                                            &pipeline->src_tex_clip,
                                            clip_stack_depth,
                                            CAIRO_FILL_RULE_WINDING,
                                            0.0);
        if (pipeline->has_mask_tex_clip)
            _cairo_cogl_set_path_prim_clip (surface,
                                            &pipeline->mask_tex_clip,
                                            clip_stack_depth,
                                            CAIRO_FILL_RULE_WINDING,
                                            0.0);
    }
}

/* Get the pipeline for the second pass */
static CoglPipeline *
_cairo_cogl_setup_unbounded_area_pipeline (cairo_cogl_surface_t *surface,
                                           cairo_operator_t      op)
{
    CoglPipeline *unbounded_pipeline;
    CoglDepthState depth_state;
    CoglBool cogl_status;
    cairo_cogl_device_t *dev = to_device(surface->base.device);

    /* If a template pipeline exists for any given operator, the
     * corresponding solid template pipeline always exists */
    unbounded_pipeline =
        cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
    cogl_pipeline_set_color4f (unbounded_pipeline, 0.0, 0.0, 0.0, 0.0);

    /* Enable depth test on second-pass pipeline */
    cogl_depth_state_init (&depth_state);
    cogl_depth_state_set_test_enabled (&depth_state, TRUE);
    cogl_status = cogl_pipeline_set_depth_state (unbounded_pipeline,
                                                 &depth_state,
                                                 NULL);
    if (unlikely (cogl_status != TRUE))
        g_warning ("Error setting depth state for unbounded render");

    return unbounded_pipeline;
}

static void
_cairo_cogl_unbounded_render (cairo_cogl_surface_t  *surface,
                              int                   *clip_stack_depth,
                              cairo_cogl_pipeline_t *pipeline,
                              cairo_bool_t          *needs_vertex_render)
{
    /* We will need a second rendering of the original vertices if we
     * still need to be bounded by the mask but had a source tex clip */
    *needs_vertex_render = FALSE;

    /* Pop all unbounded tex clips. Do not pop clips the operator is
     * bounded by, so that we can still be bounded by them during the
     * second pass (vertex render or extents render). */
    if (pipeline->mask_bounded && pipeline->src_bounded) {
        /* We don't need a second pass if it will just be in the same
         * region as the first */
    } else if (pipeline->src_bounded) {
        if (pipeline->has_mask_tex_clip) {
            cogl_framebuffer_pop_clip (surface->framebuffer);
            (*clip_stack_depth)--;
        }
    } else if (pipeline->mask_bounded) {
        if (pipeline->has_src_tex_clip) {
            cogl_framebuffer_pop_clip (surface->framebuffer);
            (*clip_stack_depth)--;
            *needs_vertex_render = TRUE;
        }
    } else {
        if (pipeline->has_src_tex_clip) {
            cogl_framebuffer_pop_clip (surface->framebuffer);
            (*clip_stack_depth)--;
        }
        if (pipeline->has_mask_tex_clip) {
            cogl_framebuffer_pop_clip (surface->framebuffer);
            (*clip_stack_depth)--;
        }
    }

    /* If an operator is unbounded by the mask, we need to render the
     * second transparent pass within the full unbounded extents */
    if (!pipeline->mask_bounded) {
        CoglPipeline *unbounded_pipeline;

        /* Draw a transparent rectangle to cover the entire extents */
        unbounded_pipeline =
            _cairo_cogl_setup_unbounded_area_pipeline (surface,
                                                       pipeline->op);
        cogl_framebuffer_draw_rectangle (surface->framebuffer,
                                         unbounded_pipeline,
                                         pipeline->unbounded_extents.x,
                                         pipeline->unbounded_extents.y,
                                         pipeline->unbounded_extents.x + pipeline->unbounded_extents.width,
                                         pipeline->unbounded_extents.y + pipeline->unbounded_extents.height);
        cogl_object_unref (unbounded_pipeline);
    }
}

static void
_cairo_cogl_post_unbounded_render (cairo_cogl_surface_t  *surface,
                                   int                   *clip_stack_depth,
                                   cairo_cogl_pipeline_t *pipeline)
{
    CoglDepthState depth_state;
    CoglBool cogl_status;

    /* Disable the depth test */
    if ((!pipeline->mask_bounded && pipeline->has_mask_tex_clip) ||
        (!pipeline->src_bounded && pipeline->has_src_tex_clip))
    {
        cogl_depth_state_init (&depth_state);
        cogl_depth_state_set_test_enabled (&depth_state, FALSE);
        cogl_status = cogl_pipeline_set_depth_state (pipeline->pipeline,
                                                     &depth_state,
                                                     NULL);
        if (unlikely (cogl_status != TRUE))
            g_warning ("Error setting depth state after unbounded render");
    }

    /* Pop all bounded tex clips (those that were not popped before) */
    if (pipeline->src_bounded && pipeline->mask_bounded) {
        if (pipeline->has_src_tex_clip) {
            cogl_framebuffer_pop_clip (surface->framebuffer);
            (*clip_stack_depth)--;
        }
        if (pipeline->has_mask_tex_clip) {
            cogl_framebuffer_pop_clip (surface->framebuffer);
            (*clip_stack_depth)--;
        }
    } else if (pipeline->src_bounded) {
        if (pipeline->has_src_tex_clip) {
            cogl_framebuffer_pop_clip (surface->framebuffer);
            (*clip_stack_depth)--;
        }
    } else if (pipeline->mask_bounded) {
        if (pipeline->has_mask_tex_clip) {
            cogl_framebuffer_pop_clip (surface->framebuffer);
            (*clip_stack_depth)--;
        }
    } else {
        /* We have already popped all of the clips in the
         * unbounded_render function */
    }
}

static void
_cairo_cogl_journal_flush (cairo_cogl_surface_t *surface)
{
    GList *l;
    cairo_cogl_device_t *dev;
    int clip_stack_depth = 0;
    int i;

    if (!surface->journal)
	return;

    dev = to_device(surface->base.device);
    if (dev->buffer_stack && dev->buffer_stack_offset) {
	cogl_buffer_unmap (dev->buffer_stack);
	cogl_object_unref (dev->buffer_stack);
	dev->buffer_stack = NULL;
    }

    if (unlikely (_cairo_cogl_surface_ensure_framebuffer (surface))) {
        g_warning ("Could not get framebuffer for flushing journal");
        assert (0);
    }

    cogl_framebuffer_push_matrix (surface->framebuffer);

    for (l = surface->journal->head; l; l = l->next) {
	cairo_cogl_journal_entry_t *entry = l->data;

	switch (entry->type)
	{
	case CAIRO_COGL_JOURNAL_ENTRY_TYPE_CLIP: {
	    cairo_cogl_journal_clip_entry_t *clip_entry =
		(cairo_cogl_journal_clip_entry_t *)entry;
	    cairo_clip_path_t *path;

	    for (i = 0; i < clip_stack_depth; i++)
		cogl_framebuffer_pop_clip (surface->framebuffer);
	    clip_stack_depth = 0;

            if (clip_entry->clip == NULL)
                continue; // there is no clip

	    for (path = clip_entry->clip->path, i = 0;
                 path;
                 path = path->prev, i++)
            {
                _cairo_cogl_set_path_prim_clip (surface,
                                                &path->path,
                                                &clip_stack_depth,
                                                path->fill_rule,
                                                path->tolerance);
            }

	    if (clip_entry->clip->num_boxes > 0) {
                cairo_path_fixed_t boxes_path;

                _cairo_path_fixed_init (&boxes_path);
                for (int i = 0; i < clip_entry->clip->num_boxes; i++) {
                    if (unlikely (_cairo_path_fixed_add_box (&boxes_path,
                                                             &clip_entry->clip->boxes[i])))
                    {
                        g_warning ("Could not add all clip boxes while "
                                   "flushing journal");
                        break;
                    }
                }

                _cairo_cogl_set_path_prim_clip (surface,
                                                &boxes_path,
                                                &clip_stack_depth,
                                                CAIRO_FILL_RULE_WINDING,
                                                0.0);

                _cairo_path_fixed_fini (&boxes_path);
	    }

	    surface->n_clip_updates_per_frame++;
	    break;
	}
	case CAIRO_COGL_JOURNAL_ENTRY_TYPE_RECTANGLE: {
	    cairo_cogl_journal_rect_entry_t *rect_entry =
		(cairo_cogl_journal_rect_entry_t *)entry;
	    float tex_coords[8];
	    float x1 = rect_entry->x;
	    float y1 = rect_entry->y;
	    float x2 = rect_entry->x + rect_entry->width;
	    float y2 = rect_entry->y + rect_entry->height;
	    cairo_matrix_t *ctm = &rect_entry->ctm;
	    float ctmfv[16] = {
		ctm->xx, ctm->yx, 0, 0,
		ctm->xy, ctm->yy, 0, 0,
		0,	     0,	      1, 0,
		ctm->x0, ctm->y0, 0, 1
	    };
	    CoglMatrix transform;
            cairo_bool_t needs_vertex_render;
            CoglPipeline *unbounded_pipeline;

	    cogl_matrix_init_from_array (&transform, ctmfv);

            _cairo_cogl_apply_tex_clips (surface,
                                         &clip_stack_depth,
                                         rect_entry->pipeline);

	    if (rect_entry->pipeline->n_layers) {
		g_assert (rect_entry->pipeline->n_layers <= 2);
		tex_coords[0] = x1;
		tex_coords[1] = y1;
		tex_coords[2] = x2;
		tex_coords[3] = y2;
		if (rect_entry->pipeline->n_layers > 1)
		    memcpy (&tex_coords[4], tex_coords, sizeof (float) * 4);
	    }

	    cogl_framebuffer_push_matrix (surface->framebuffer);
	    cogl_framebuffer_transform (surface->framebuffer, &transform);
	    cogl_framebuffer_draw_multitextured_rectangle (surface->framebuffer,
                                                           rect_entry->pipeline->pipeline,
                                                           x1, y1,
                                                           x2, y2,
						           tex_coords,
                                                           4 * rect_entry->pipeline->n_layers);

            _cairo_cogl_unbounded_render (surface,
                                          &clip_stack_depth,
                                          rect_entry->pipeline,
                                          &needs_vertex_render);
            if (needs_vertex_render) {
                unbounded_pipeline =
                    _cairo_cogl_setup_unbounded_area_pipeline (surface,
                                                               rect_entry->pipeline->op);
                cogl_framebuffer_draw_multitextured_rectangle (surface->framebuffer,
                                                               unbounded_pipeline,
                                                               x1, y1,
                                                               x2, y2,
						               tex_coords,
                                                               4 * rect_entry->pipeline->n_layers);
                cogl_object_unref (unbounded_pipeline);
            }
            _cairo_cogl_post_unbounded_render (surface,
                                               &clip_stack_depth,
                                               rect_entry->pipeline);

	    cogl_framebuffer_pop_matrix (surface->framebuffer);
	    break;
	}
	case CAIRO_COGL_JOURNAL_ENTRY_TYPE_PRIMITIVE: {
	    cairo_cogl_journal_prim_entry_t *prim_entry =
		(cairo_cogl_journal_prim_entry_t *)entry;
	    CoglMatrix transform;
            cairo_bool_t needs_vertex_render;
            CoglPipeline *unbounded_pipeline;

            _cairo_cogl_apply_tex_clips (surface,
                                         &clip_stack_depth,
                                         prim_entry->pipeline);

	    cogl_framebuffer_push_matrix (surface->framebuffer);
            cairo_matrix_t *ctm = &prim_entry->transform;
            float ctmfv[16] = {
                ctm->xx, ctm->yx, 0, 0,
                ctm->xy, ctm->yy, 0, 0,
                0,       0,       1, 0,
                ctm->x0, ctm->y0, 0, 1
            };
            cogl_matrix_init_from_array (&transform, ctmfv);
            cogl_framebuffer_transform (surface->framebuffer, &transform);

            /* If the primitive is NULL, it means we just draw the
             * unbounded rectangle */
            if (prim_entry->primitive)
	        cogl_primitive_draw (prim_entry->primitive,
                                     surface->framebuffer,
                                     prim_entry->pipeline->pipeline);

            _cairo_cogl_unbounded_render (surface,
                                          &clip_stack_depth,
                                          prim_entry->pipeline,
                                          &needs_vertex_render);
            if (needs_vertex_render && prim_entry->primitive) {
                unbounded_pipeline =
                    _cairo_cogl_setup_unbounded_area_pipeline (surface,
                                                               prim_entry->pipeline->op);
                cogl_primitive_draw (prim_entry->primitive,
                                     surface->framebuffer,
                                     unbounded_pipeline);
                cogl_object_unref (unbounded_pipeline);
            }
            _cairo_cogl_post_unbounded_render (surface,
                                               &clip_stack_depth,
                                               prim_entry->pipeline);

	    cogl_framebuffer_pop_matrix (surface->framebuffer);
	    break;
	}
	default:
	    assert (0); /* not reached! */
	}
    }

    cogl_framebuffer_pop_matrix (surface->framebuffer);

    for (i = 0; i < clip_stack_depth; i++)
	cogl_framebuffer_pop_clip (surface->framebuffer);

    _cairo_cogl_journal_discard (surface);
}

static cairo_status_t
_cairo_cogl_surface_flush (void    *abstract_surface,
			   unsigned flags)
{
    cairo_cogl_surface_t *surface = (cairo_cogl_surface_t *)abstract_surface;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    _cairo_cogl_journal_flush (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_cogl_surface_finish (void *abstract_surface)
{
    cairo_cogl_surface_t *surface = abstract_surface;

    if (surface->texture)
	cogl_object_unref (surface->texture);

    if (surface->framebuffer)
	cogl_object_unref (surface->framebuffer);

    if (surface->journal)
	_cairo_cogl_journal_free (surface);

    /*XXX wtf */
    cairo_device_release (surface->base.device);

    return CAIRO_STATUS_SUCCESS;
}

static CoglTextureComponents
get_components_from_cairo_content (cairo_content_t content)
{
    /* We use RGBA for color-only surfaces due to the fact that cogl
     * does not provide a padded XRGB format, thereby making us use
     * RGBA formats to represent e.g. CAIRO_FORMAT_RGB24 and doing very
     * expensive format conversions on the cpu when images are read
     * back */
    return (content & CAIRO_CONTENT_COLOR) ?
           COGL_TEXTURE_COMPONENTS_RGBA :
           COGL_TEXTURE_COMPONENTS_A;
}

static CoglPixelFormat
get_default_cogl_format_from_components (CoglTextureComponents components)
{
    switch (components)
    {
    case COGL_TEXTURE_COMPONENTS_A:
        return COGL_PIXEL_FORMAT_A_8;
    case COGL_TEXTURE_COMPONENTS_RG:
        return COGL_PIXEL_FORMAT_RG_88;
    case COGL_TEXTURE_COMPONENTS_RGB:
    case COGL_TEXTURE_COMPONENTS_RGBA:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	return COGL_PIXEL_FORMAT_BGRA_8888_PRE;
#else
	return COGL_PIXEL_FORMAT_ARGB_8888_PRE;
#endif
    case COGL_TEXTURE_COMPONENTS_DEPTH:
        return COGL_PIXEL_FORMAT_DEPTH_32;
    default:
        return 0;
    }
}

static CoglTextureComponents
get_components_from_cairo_format (cairo_format_t format)
{
    switch (format)
    {
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_A8:
        return COGL_TEXTURE_COMPONENTS_A;

    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_RGB30:
    case CAIRO_FORMAT_RGB96F:
        return COGL_TEXTURE_COMPONENTS_RGB;

    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGBA128F:
        return COGL_TEXTURE_COMPONENTS_RGBA;

    case CAIRO_FORMAT_INVALID:
    default:
        g_warning("Cairo format unrepresentable by cogl");
        return 0;
    }
}

static CoglPixelFormat
get_cogl_format_from_cairo_format (cairo_format_t cairo_format);

/* XXX: We often use RGBA format for onscreen framebuffers so make sure
 * to handle CAIRO_FORMAT_INVALID sensibly */
static cairo_format_t
get_cairo_format_from_cogl_format (CoglPixelFormat format)
{
    switch ((int)format)
    {
    case COGL_PIXEL_FORMAT_A_8:
	return CAIRO_FORMAT_A8;
    case COGL_PIXEL_FORMAT_RGB_565:
	return CAIRO_FORMAT_RGB16_565;
    case COGL_PIXEL_FORMAT_RG_88:
        g_warning ("cairo cannot handle red-green textures");
        return CAIRO_FORMAT_INVALID;
    case COGL_PIXEL_FORMAT_DEPTH_32:
        g_warning ("cairo cannot handle depth textures");
        return CAIRO_FORMAT_INVALID;
    
    case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
    case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
    case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
	/* Note: this is ambiguous since CAIRO_FORMAT_RGB24
	 * would also map to the same CoglPixelFormat */
	return CAIRO_FORMAT_ARGB32;

    default:
	g_warning("bad format: %x a? %d, bgr? %d, pre %d, format: %d",
		  format,
		  format & COGL_A_BIT,
		  format & COGL_BGR_BIT,
		  format & COGL_PREMULT_BIT,
		  format & ~(COGL_A_BIT | COGL_BGR_BIT | COGL_PREMULT_BIT));
	return CAIRO_FORMAT_INVALID;
    }
}

static CoglPixelFormat
get_cogl_format_from_cairo_format (cairo_format_t cairo_format)
{
    switch (cairo_format)
    {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	return COGL_PIXEL_FORMAT_BGRA_8888_PRE;
#else
	return COGL_PIXEL_FORMAT_ARGB_8888_PRE;
#endif
    case CAIRO_FORMAT_A8:
	return COGL_PIXEL_FORMAT_A_8;
    case CAIRO_FORMAT_RGB16_565:
	return COGL_PIXEL_FORMAT_RGB_565;
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB30:
    case CAIRO_FORMAT_RGB96F:
    case CAIRO_FORMAT_RGBA128F:
	return 0;
    }

    g_warn_if_reached ();
    return 0;
}

static cairo_surface_t *
_cairo_cogl_surface_create_similar (void            *abstract_surface,
				    cairo_content_t  content,
				    int              width,
				    int              height)
{
    cairo_cogl_surface_t *reference_surface = abstract_surface;
    cairo_cogl_surface_t *surface;
    CoglTexture *texture;
    cairo_status_t status;
    cairo_cogl_device_t *dev =
        to_device(reference_surface->base.device);
    int tex_width = width;
    int tex_height = height;

    /* In the case of lack of NPOT texture support, we allocate texture
     * with dimensions of the next power of two */
    if (!dev->has_npots) {
        tex_width = pow (2, ceil (log2 (tex_width)));
        tex_height = pow (2, ceil (log2 (tex_height)));
    }

    texture = cogl_texture_2d_new_with_size (dev->cogl_context,
                                             tex_width, tex_height);
    if (!texture)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    cogl_texture_set_components (texture,
        get_components_from_cairo_content (content));

    surface = (cairo_cogl_surface_t *)
	_cairo_cogl_surface_create_full (dev, content, NULL, texture);
    if (unlikely (surface->base.status))
	return &surface->base;

    /* The surface will take a reference on the texture */
    cogl_object_unref (texture);

    /* If we passed a texture with larger dimensions, we need to set
     * the surface dimensions */
    surface->width = width;
    surface->height = height;

    status = _cairo_cogl_surface_ensure_framebuffer (surface);
    if (unlikely (status)) {
	cairo_surface_destroy (&surface->base);
	return _cairo_surface_create_in_error (status);
    }

    return &surface->base;
}

static cairo_status_t
_cairo_cogl_surface_read_rect_to_image_surface (cairo_cogl_surface_t   *surface,
						cairo_rectangle_int_t  *interest,
						cairo_image_surface_t **image_out)
{
    cairo_image_surface_t *image;
    cairo_status_t status;
    cairo_format_t cairo_format;
    CoglPixelFormat cogl_format;

    status = _cairo_cogl_surface_ensure_framebuffer (surface);
    if (unlikely (status))
	return status;

    if (surface->texture)
    {
        cogl_format =
            get_default_cogl_format_from_components (
                cogl_texture_get_components (surface->texture) );
        cairo_format = get_cairo_format_from_cogl_format (cogl_format);
    } else {
        cairo_format =
            _cairo_format_from_content (surface->base.content);
        cogl_format = get_cogl_format_from_cairo_format (cairo_format);
    }

    image = (cairo_image_surface_t *)
        cairo_image_surface_create (cairo_format,
                                    surface->width,
                                    surface->height);
    if (image->base.status)
        return image->base.status;

    cogl_framebuffer_read_pixels (surface->framebuffer, 0, 0,
                                  surface->width, surface->height,
                                  cogl_format, image->data);

    *image_out = image;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_cogl_surface_acquire_source_image (void		         *abstract_surface,
					  cairo_image_surface_t **image_out,
					  void		        **image_extra)
{
    cairo_cogl_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (unlikely (_cairo_surface_flush (abstract_surface, 0)))
        g_warning ("Error flushing journal while acquiring image");

    if (surface->texture) {
        CoglTextureComponents components =
            cogl_texture_get_components(surface->texture);
        CoglPixelFormat cogl_format =
            get_default_cogl_format_from_components (components);
        cairo_format_t cairo_format =
            get_cairo_format_from_cogl_format (cogl_format);
        if (cairo_format == CAIRO_FORMAT_INVALID) {
            cairo_format = CAIRO_FORMAT_ARGB32;
            cogl_format =
                get_cogl_format_from_cairo_format (cairo_format);
        }

        /* We use the actual texture dimensions here instead, because
         * if we have a larger texture than the surface dimensions for
         * devices not supporting NPOT textures, the surface dimensions
         * will not be able to fit the data */
        cairo_image_surface_t *image = (cairo_image_surface_t *)
	        cairo_image_surface_create (cairo_format,
	                                    cogl_texture_get_width (surface->texture),
	                                    cogl_texture_get_height (surface->texture));
        if (image->base.status)
            return image->base.status;

        cogl_texture_get_data (surface->texture,
                               cogl_format,
                               0,
                               image->data);

        /* If the texture dimensions were different than the surface
         * dimensions, this will set them to the correct values.
         * Because the stride stays the same, it will still function
         * correctly */
        image->width = surface->width;
        image->height = surface->height;

	image->base.is_clear = FALSE;
	*image_out = image;
    } else {
	cairo_rectangle_int_t extents = {
	    0, 0, surface->width, surface->height
	};
	status = _cairo_cogl_surface_read_rect_to_image_surface (surface, &extents,
								 image_out);
	if (unlikely (status))
	    return status;
    }

    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_cogl_surface_release_source_image (void			*abstract_surface,
					  cairo_image_surface_t *image,
					  void			*image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_cogl_surface_clear (cairo_cogl_surface_t *surface,
			   const cairo_color_t  *color)
{
    /* Anything batched in the journal up until now is redundant... */
    _cairo_cogl_journal_discard (surface);

    /* XXX: we currently implicitly clear the depth and stencil buffer here
     * but since we use the framebuffer_discard extension when available I
     * suppose this doesn't matter too much.
     *
     * The main concern is that we want to avoid re-loading an external z
     * buffer at the start of each frame, but also many gpu architectures have
     * optimizations for how they handle the depth/stencil buffers and can get
     * upset if they aren't cleared together at the start of the frame.
     *
     * FIXME: we need a way to assert that the clip stack currently isn't
     * using the stencil buffer before clearing it here!
     */
    cogl_framebuffer_clear4f (surface->framebuffer,
			      COGL_BUFFER_BIT_COLOR |
			      COGL_BUFFER_BIT_DEPTH |
			      COGL_BUFFER_BIT_STENCIL,
			      color->red * color->alpha,
			      color->green * color->alpha,
			      color->blue * color->alpha,
			      color->alpha);
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_cogl_path_fixed_rectangle (cairo_path_fixed_t *path,
				  cairo_fixed_t x,
				  cairo_fixed_t y,
				  cairo_fixed_t width,
				  cairo_fixed_t height)
{
    cairo_status_t status;

    status = _cairo_path_fixed_move_to (path, x, y);
    if (unlikely (status))
	return status;

    status = _cairo_path_fixed_rel_line_to (path, width, 0);
    if (unlikely (status))
	return status;

    status = _cairo_path_fixed_rel_line_to (path, 0, height);
    if (unlikely (status))
	return status;

    status = _cairo_path_fixed_rel_line_to (path, -width, 0);
    if (unlikely (status))
	return status;

    status = _cairo_path_fixed_close_path (path);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
}


static CoglPipelineWrapMode
get_cogl_wrap_mode_for_extend (cairo_extend_t       extend_mode,
                               cairo_cogl_device_t *dev)
{
    switch (extend_mode)
    {
    case CAIRO_EXTEND_NONE:
	return COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
    case CAIRO_EXTEND_PAD:
	return COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
    case CAIRO_EXTEND_REPEAT:
	return COGL_PIPELINE_WRAP_MODE_REPEAT;
    case CAIRO_EXTEND_REFLECT:
        if (!dev->has_mirrored_repeat)
            /* If the hardware cannot support mirrored repeating, we
             * emulate it elsewhere */
            return COGL_PIPELINE_WRAP_MODE_REPEAT;
        else
	    return COGL_PIPELINE_WRAP_MODE_MIRRORED_REPEAT;
    }
    assert (0); /* not reached */
    return COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
}

static CoglPipelineFilter
get_cogl_filter_for_filter (cairo_filter_t filter)
{
    switch (filter)
    {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
        return COGL_PIPELINE_FILTER_NEAREST;

    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
        return COGL_PIPELINE_FILTER_LINEAR;

    case CAIRO_FILTER_GAUSSIAN:
    default:
        g_warning("Invalid pattern filter");
        return COGL_PIPELINE_FILTER_NEAREST;
    }
}

static void
_cairo_cogl_matrix_all_scale (cairo_matrix_t *matrix,
                              double          xscale,
                              double          yscale)
{
    /* Since cairo_matrix_scale does not scale the x0 and y0 components,
     * which is required for scaling translations to normalized
     * coordinates, use a custom solution here. */
    matrix->xx *= xscale;
    matrix->yx *= yscale;
    matrix->xy *= xscale;
    matrix->yy *= yscale;
    matrix->x0 *= xscale;
    matrix->y0 *= yscale;
}

static CoglTexture *
_cairo_cogl_scale_texture (CoglContext           *context,
                           CoglTexture           *texture_in,
                           unsigned int           new_width,
                           unsigned int           new_height,
                           cairo_bool_t           do_mirror_texture,
                           cairo_bool_t           always_new_texture)
{
    CoglTexture *texture_out = NULL;
    CoglPipeline *copying_pipeline = NULL;
    CoglFramebuffer *fb = NULL;
    CoglError *error = NULL;
    unsigned int tex_width = new_width;
    unsigned int tex_height = new_height;

    /* If the texture is already in the desired dimensions and we are
     * not mirroring it, copying it, or reading from different extents,
     * return it unmodified */
    if (!do_mirror_texture && !always_new_texture &&
        new_width == cogl_texture_get_width (texture_in) &&
        new_height == cogl_texture_get_height (texture_in))
        return texture_in;

    if (do_mirror_texture) {
        tex_width *= 2;
        tex_height *= 2;
    }

    texture_out =
        cogl_texture_2d_new_with_size (context, tex_width, tex_height);
    if (unlikely (!texture_out)) {
        g_warning ("Failed to get texture for scaling");
        goto BAIL;
    }

    cogl_texture_set_components (texture_out,
        cogl_texture_get_components (texture_in));

    fb = cogl_offscreen_new_with_texture (texture_out);
    if (unlikely (!cogl_framebuffer_allocate (fb, &error))) {
        g_warning ("Could not get framebuffer for texture scaling: %s",
                   error->message);
        cogl_error_free (error);
        goto BAIL;
    }

    cogl_framebuffer_orthographic (fb, 0, 0,
                                       tex_width, tex_height,
                                       -1, 100);

    copying_pipeline = cogl_pipeline_new (context);
    cogl_pipeline_set_layer_texture (copying_pipeline, 0, texture_in);
    cogl_pipeline_set_layer_filters (copying_pipeline, 0,
                                     COGL_PIPELINE_FILTER_NEAREST,
                                     COGL_PIPELINE_FILTER_NEAREST);

    if (do_mirror_texture) {
        /* Draw four rectangles to the new texture with the appropriate
         * reflection on each one */

        const float rect_coordinates[32] = {
            /* Rectangle 1 */
            0, 0, 0.5 * tex_width, 0.5 * tex_height,
            0, 0, 1, 1,

            /* Rectangle 2 */
            tex_width, 0, 0.5 * tex_width, 0.5 * tex_height,
            0, 0, 1, 1,

            /* Rectangle 3 */
            0, tex_height, 0.5 * tex_width, 0.5 * tex_height,
            0, 0, 1, 1,

            /* Rectangle 4 */
            tex_width, tex_height, 0.5 * tex_width, 0.5 * tex_height,
            0, 0, 1, 1
        };

        cogl_framebuffer_draw_textured_rectangles (fb,
                                                   copying_pipeline,
                                                   rect_coordinates,
                                                   4);
    } else {
        cogl_framebuffer_draw_textured_rectangle (fb,
                                                  copying_pipeline,
                                                  0, 0,
                                                  tex_width,
                                                  tex_height,
                                                  0, 0, 1, 1);
    }

    cogl_object_unref (fb);
    cogl_object_unref (copying_pipeline);
    cogl_object_unref (texture_in);

    return texture_out;

BAIL:
    if (texture_out)
        cogl_object_unref (texture_out);
    if (fb)
        cogl_object_unref (fb);
    if (copying_pipeline)
        cogl_object_unref (copying_pipeline);

    return NULL;
}

/* NB: a reference for the texture is transferred to the caller which
 * should be unrefed */
static CoglTexture *
_cairo_cogl_acquire_cogl_surface_texture (cairo_cogl_surface_t        *reference_surface,
                                          cairo_surface_t             *surface,
                                          const cairo_rectangle_int_t *surface_extents,
                                          const cairo_matrix_t        *pattern_matrix,
                                          cairo_matrix_t              *out_matrix,
                                          const cairo_bool_t           need_mirrored_texture,
                                          cairo_bool_t                *is_mirrored_texture)
{
    CoglTexture *texture;
    cairo_surface_t *clone = NULL;
    cairo_cogl_surface_t *cogl_surface =
        (cairo_cogl_surface_t *)surface;
    cairo_bool_t do_mirror_texture;
    cairo_cogl_device_t *dev =
        to_device (reference_surface->base.device);
    double xscale, yscale;
    int new_width = surface_extents->width;
    int new_height = surface_extents->height;

    if (surface_extents->x < 0 || surface_extents->y < 0 ||
        (surface_extents->x + surface_extents->width) >
            cogl_surface->width ||
        (surface_extents->y + surface_extents->height) >
            cogl_surface->height)
        return NULL;

    *out_matrix = *pattern_matrix;
    *is_mirrored_texture = FALSE;

    if (unlikely (_cairo_surface_flush (surface, 0))) {
        g_warning ("Error flushing source surface while getting "
                   "pattern texture");
        goto BAIL;
    }

    *is_mirrored_texture =
        need_mirrored_texture || cogl_surface->is_mirrored_snapshot;
    do_mirror_texture =
        need_mirrored_texture && !cogl_surface->is_mirrored_snapshot;

    /* There seems to be a bug in which cogl isn't flushing its own
     * internal journal when reading from dependent sub-textures.
     * If this is ever fixed, the following block of code can be
     * removed. */
    {
        _cairo_cogl_surface_ensure_framebuffer (cogl_surface);
        cogl_framebuffer_finish (cogl_surface->framebuffer);
    }
    /* We copy the surface to a new texture, thereby making a
     * snapshot of it, as its contents may change between the time
     * we log the pipeline and when we flush the journal. The sub
     * texture itself cannot be used while drawing primitives, so we do
     * a copy to a 2d texture. */
    texture = cogl_sub_texture_new (dev->cogl_context,
                                    cogl_surface->texture,
                                    surface_extents->x,
                                    surface_extents->y,
                                    surface_extents->width,
                                    surface_extents->height);
    if (unlikely (!texture))
        goto BAIL;

    /* If we do not support NPOT dimensions, scale the new texture to
     * the next power of two while copying */
    if (!dev->has_npots) {
        new_width = (int)pow (2, ceil (log2 (new_width)));
        new_height = (int)pow (2, ceil (log2 (new_height)));
    }
    texture = _cairo_cogl_scale_texture (dev->cogl_context,
                                         texture,
                                         new_width,
                                         new_height,
                                         do_mirror_texture,
                                         TRUE);
    if (unlikely (!texture))
        goto BAIL;

    clone =
        _cairo_cogl_surface_create_full (dev,
                                         reference_surface->base.content,
                                         NULL,
                                         texture);
    if (unlikely (clone->status)) {
        g_warning ("Could not get clone surface for texture");
        goto BAIL;
    }
    _cairo_surface_attach_snapshot (surface, clone, NULL);

    /* Attaching the snapshot will take a reference on the clone surface... */
    cairo_surface_destroy (clone);
    clone = NULL;

    /* Convert from un-normalized source coordinates in backend
     * coordinates to normalized texture coordinates. */
    if (*is_mirrored_texture) {
        xscale = 0.5 / surface_extents->width;
        yscale = 0.5 / surface_extents->height;
    } else {
        xscale = 1.0 / surface_extents->width;
        yscale = 1.0 / surface_extents->height;
    }
    _cairo_cogl_matrix_all_scale (out_matrix, xscale, yscale);

    return texture;

BAIL:
    if (texture)
        cogl_object_unref (texture);
    if (clone)
        cairo_surface_destroy (clone);

    return NULL;
}

/* NB: a reference for the texture is transferred to the caller which
 * should be unrefed */
static CoglTexture *
_cairo_cogl_acquire_recording_surface_texture (cairo_cogl_surface_t        *reference_surface,
                                               cairo_surface_t             *surface,
                                               const cairo_rectangle_int_t *extents,
                                               const cairo_matrix_t        *pattern_matrix,
                                               cairo_matrix_t              *out_matrix,
                                               const cairo_bool_t           need_mirrored_texture,
                                               cairo_bool_t                *is_mirrored_texture)
{
    CoglTexture *texture = NULL;
    cairo_surface_t *clone = NULL;
    cairo_cogl_device_t *dev =
        to_device (reference_surface->base.device);
    cairo_matrix_t transform;
    int tex_height, tex_width;
    double xscale, yscale;

    *is_mirrored_texture = FALSE;

    /* We will pre-transform all of the drawing by the pattern matrix
     * and confine it to the required extents, so no later transform
     * will be required */
    cairo_matrix_init_translate (out_matrix, -extents->x, -extents->y);

    cairo_matrix_init_translate (&transform, extents->x, extents->y);
    cairo_matrix_multiply (&transform, &transform, pattern_matrix);

    if (!dev->has_npots) {
        /* Record to a texture sized to the next power of two */
        tex_width = (int)pow (2, ceil (log2 (extents->width)));
        tex_height = (int)pow (2, ceil (log2 (extents->height)));

        /* And scale accordingly */
        cairo_matrix_scale (&transform,
                            (double)extents->width / (double)tex_width,
                            (double)extents->height / (double)tex_height);
    } else {
        tex_width = extents->width;
        tex_height = extents->height;
    }

    texture = cogl_texture_2d_new_with_size (dev->cogl_context,
                                             tex_width,
                                             tex_height);
    if (unlikely (!texture)) {
        g_warning ("Failed to create texture for replaying recording "
                   "surface");
        goto BAIL;
    }

    cogl_texture_set_components (texture,
        get_components_from_cairo_content (surface->content));

    /* Do not attach this as a snapshot, as it only represents part of
     * the surface */
    clone =
        _cairo_cogl_surface_create_full (dev,
                                         reference_surface->base.content,
                                         NULL,
                                         texture);
    if (unlikely (_cairo_cogl_surface_ensure_framebuffer ((cairo_cogl_surface_t *)clone)))
    {
        g_warning ("Could not get framebuffer for replaying recording "
                   "surface");
        goto BAIL;
    }

    if (unlikely (_cairo_recording_surface_replay_with_clip (surface,
                                                             &transform,
                                                             clone,
                                                             NULL)))
    {
        g_warning ("Could not replay recording surface");
        goto BAIL;
    }
    _cairo_cogl_journal_flush ((cairo_cogl_surface_t *)clone);
    cairo_surface_destroy (clone);

    if (need_mirrored_texture) {
        /* Scale to the same image extents, but mirror the texture,
         * thereby making it larger */
        texture = _cairo_cogl_scale_texture (dev->cogl_context,
                                             texture,
                                             tex_width,
                                             tex_height,
                                             TRUE,
                                             FALSE);
        if (unlikely (!texture))
            goto BAIL;

        *is_mirrored_texture = TRUE;
    }

    /* Convert from un-normalized source coordinates in backend
     * coordinates to normalized texture coordinates. */
    if (*is_mirrored_texture) {
        xscale = 0.5 / extents->width;
        yscale = 0.5 / extents->height;
    } else {
        xscale = 1.0 / extents->width;
        yscale = 1.0 / extents->height;
    }
    _cairo_cogl_matrix_all_scale (out_matrix, xscale, yscale);

    return texture;

BAIL:
    if (clone)
        cairo_surface_destroy (clone);
    if (texture)
        cogl_object_unref (texture);

    return NULL;
}

/* NB: a reference for the texture is transferred to the caller which
 * should be unrefed */
static CoglTexture *
_cairo_cogl_acquire_generic_surface_texture (cairo_cogl_surface_t *reference_surface,
                                             cairo_surface_t      *surface,
                                             const cairo_matrix_t *pattern_matrix,
                                             cairo_matrix_t       *out_matrix,
                                             const cairo_bool_t    need_mirrored_texture,
                                             cairo_bool_t         *is_mirrored_texture)
{
    CoglTexture *texture = NULL;
    cairo_image_surface_t *image;
    cairo_image_surface_t *acquired_image = NULL;
    void *image_extra;
    cairo_image_surface_t *image_clone = NULL;
    CoglBitmap *bitmap;
    CoglError *error = NULL;
    cairo_surface_t *clone = NULL;
    CoglPixelFormat format;
    cairo_cogl_device_t *dev =
        to_device (reference_surface->base.device);
    ptrdiff_t stride;
    unsigned char *data;
    double xscale, yscale;

    *out_matrix = *pattern_matrix;
    *is_mirrored_texture = FALSE;

    if (_cairo_surface_is_image (surface)) {
        image = (cairo_image_surface_t *)surface;
    } else {
        cairo_status_t status =
            _cairo_surface_acquire_source_image (surface,
                                                 &acquired_image,
                                                 &image_extra);
        if (unlikely (status)) {
            g_warning ("acquire_source_image failed: %s [%d]",
                        cairo_status_to_string (status), status);
            return NULL;
        }
        image = acquired_image;
    }

    format = get_cogl_format_from_cairo_format (image->format);
    if (!format) {
        image_clone = _cairo_image_surface_coerce (image);
        if (unlikely (image_clone->base.status)) {
            g_warning ("image_surface_coerce failed");
            texture = NULL;
            goto BAIL;
        }

        format =
            get_cogl_format_from_cairo_format (image_clone->format);
        assert (format);

        image = image_clone;
    }

    if (image->stride < 0) {
        /* If the stride is negative, this modifies the data pointer so
         * that all of the pixels are read into the texture, but
         * upside-down. We then invert the matrix so the texture is
         * read from the bottom up instead of from the top down. */
        stride = -image->stride;
        data = image->data - stride * (image->height - 1);

        out_matrix->yx *= -1.0;
        out_matrix->yy *= -1.0;
        out_matrix->y0 += image->height;
    } else {
        stride = image->stride;
        data = image->data;
    }

    bitmap = cogl_bitmap_new_for_data (dev->cogl_context,
                                       image->width,
                                       image->height,
                                       format, /* incoming */
                                       stride,
                                       data);

    if (!dev->has_npots)
        texture =
            cogl_texture_2d_sliced_new_from_bitmap (bitmap,
                                                    COGL_TEXTURE_MAX_WASTE);
    else
        texture = cogl_texture_2d_new_from_bitmap (bitmap);

    /* The texture will have taken a reference on the bitmap */
    cogl_object_unref (bitmap);

    cogl_texture_set_components (texture,
        get_components_from_cairo_format (image->format));

    if (unlikely (!cogl_texture_allocate (texture, &error))) {
        g_warning ("Failed to allocate texture: %s", error->message);
        cogl_error_free (error);
        goto BAIL;
    }

    if (need_mirrored_texture) {
        int new_width = image->width;
        int new_height = image->height;

        /* If the device does not support npot textures, scale to the
         * next power of two as well */
        if (!dev->has_npots) {
            new_width = (int)pow (2, ceil (log2 (new_width)));
            new_height = (int)pow (2, ceil (log2 (new_height)));
        }

        texture = _cairo_cogl_scale_texture (dev->cogl_context,
                                             texture,
                                             new_width,
                                             new_height,
                                             TRUE,
                                             FALSE);
        if (unlikely (!texture))
            goto BAIL;

        *is_mirrored_texture = TRUE;
    } else if (!dev->has_npots) {
        /* We need to scale the texture up if the hardware does not
         * support npots */

        /* Get dimensions for the next power of two */
        int new_width = (int)pow (2, ceil (log2 (image->width)));
        int new_height = (int)pow (2, ceil (log2 (image->height)));

        texture = _cairo_cogl_scale_texture (dev->cogl_context,
                                             texture,
                                             new_width,
                                             new_height,
                                             FALSE,
                                             FALSE);
        if (unlikely (!texture))
            goto BAIL;
    }

    clone =
        _cairo_cogl_surface_create_full (dev,
                                         reference_surface->base.content,
                                         NULL,
                                         texture);
    if (unlikely (clone->status)) {
        g_warning ("Unable to create clone surface for texture");
        goto BAIL;
    }

    if (*is_mirrored_texture)
        ((cairo_cogl_surface_t *)clone)->is_mirrored_snapshot = TRUE;

    if (_cairo_surface_is_subsurface (surface))
        _cairo_surface_subsurface_set_snapshot (surface, clone);
    else
        _cairo_surface_attach_snapshot (surface, clone, NULL);

    /* Attaching the snapshot will take a reference on the clone surface... */
    cairo_surface_destroy (clone);
    clone = NULL;

    /* Convert from un-normalized source coordinates in backend
     * coordinates to normalized texture coordinates. */
    if (*is_mirrored_texture) {
        xscale = 0.5 / image->width;
        yscale = 0.5 / image->height;
    } else {
        xscale = 1.0 / image->width;
        yscale = 1.0 / image->height;
    }
    _cairo_cogl_matrix_all_scale (out_matrix, xscale, yscale);

    /* Release intermediate surface representations */
    if (image_clone) {
	cairo_surface_destroy (&image_clone->base);
        image_clone = NULL;
    }
    if (acquired_image) {
	_cairo_surface_release_source_image (surface,
                                             acquired_image,
                                             image_extra);
        acquired_image = NULL;
    }

    return texture;

BAIL:
    if (clone)
        cairo_surface_destroy (clone);
    if (image_clone)
	cairo_surface_destroy (&image_clone->base);
    if (acquired_image)
	_cairo_surface_release_source_image (surface,
                                             acquired_image,
                                             image_extra);
    if (texture)
        cogl_object_unref (texture);

    return NULL;
}

static cairo_status_t
_cairo_cogl_create_tex_clip (cairo_path_fixed_t *tex_clip,
                             cairo_matrix_t      inverse,
                             cairo_bool_t        is_mirrored_texture)
{
    cairo_status_t status;

    status = cairo_matrix_invert (&inverse);
    if (unlikely (status))
        return status;

    if (is_mirrored_texture)
        status =
            _cairo_cogl_path_fixed_rectangle (tex_clip, 0, 0,
                                              _cairo_fixed_from_double (0.5),
                                              _cairo_fixed_from_double (0.5));
    else
        status = _cairo_cogl_path_fixed_rectangle (tex_clip, 0, 0,
                                                   CAIRO_FIXED_ONE,
                                                   CAIRO_FIXED_ONE);
    if (unlikely (status))
	return status;

    _cairo_path_fixed_transform (tex_clip, &inverse);

    return CAIRO_STATUS_SUCCESS;
}

/* NB: a reference for the texture is transferred to the caller which should
 * be unrefed */
static CoglTexture *
_cairo_cogl_acquire_pattern_texture (const cairo_pattern_t           *pattern,
				     cairo_cogl_surface_t            *destination,
				     const cairo_rectangle_int_t     *extents,
				     cairo_cogl_texture_attributes_t *attributes,
                                     cairo_path_fixed_t              *tex_clip)
{
    CoglTexture *texture = NULL;
    cairo_cogl_device_t *dev = to_device (destination->base.device);
    cairo_bool_t is_mirrored_texture;
    cairo_bool_t need_mirrored_texture =
        (pattern->extend == CAIRO_EXTEND_REFLECT &&
         !dev->has_mirrored_repeat);

    switch ((int)pattern->type)
    {
    case CAIRO_PATTERN_TYPE_SURFACE: {
        cairo_cogl_surface_t *clone;
        cairo_surface_t *surface = ((cairo_surface_pattern_t *)pattern)->surface;

        clone = (cairo_cogl_surface_t *)
            _cairo_surface_has_snapshot (surface,
                                         &_cairo_cogl_surface_backend);
        if (clone && clone->texture)
            if ((!need_mirrored_texture) || clone->is_mirrored_snapshot)
            {
                texture = cogl_object_ref (clone->texture);
                attributes->matrix = pattern->matrix;
                is_mirrored_texture = clone->is_mirrored_snapshot;

                /* Convert from un-normalized source coordinates in
                 * backend coordinates to normalized texture
                 * coordinates. */
                _cairo_cogl_matrix_all_scale (&attributes->matrix,
                                              1.0 / clone->width,
                                              1.0 / clone->height);
            };

        if (!texture) {
            cairo_rectangle_int_t surface_extents;
            cairo_surface_t *unwrapped =
                _cairo_surface_get_source (surface, &surface_extents);

            if (_cairo_surface_is_recording (surface)) {
                texture =
                    _cairo_cogl_acquire_recording_surface_texture (destination,
                                                                   surface,
                                                                   extents,
                                                                   &pattern->matrix,
                                                                   &attributes->matrix,
                                                                   need_mirrored_texture,
                                                                   &is_mirrored_texture);
            } else if (surface->type == CAIRO_SURFACE_TYPE_COGL &&
                       ((cairo_cogl_surface_t *)unwrapped)->texture) {
                texture =
                    _cairo_cogl_acquire_cogl_surface_texture (destination,
                                                              unwrapped,
                                                              &surface_extents,
                                                              &pattern->matrix,
                                                              &attributes->matrix,
                                                              need_mirrored_texture,
                                                              &is_mirrored_texture);
            }
        }

        if (!texture)
            texture =
                _cairo_cogl_acquire_generic_surface_texture (destination,
                                                             surface,
                                                             &pattern->matrix,
                                                             &attributes->matrix,
                                                             need_mirrored_texture,
                                                             &is_mirrored_texture);

        if (unlikely (!texture))
            return NULL;

	attributes->extend = pattern->extend;
	attributes->filter =
            get_cogl_filter_for_filter (pattern->filter);
	attributes->has_component_alpha = pattern->has_component_alpha;

	attributes->s_wrap =
            get_cogl_wrap_mode_for_extend (pattern->extend, dev);
	attributes->t_wrap = attributes->s_wrap;

        /* In order to support CAIRO_EXTEND_NONE, we use the same wrap
         * mode as CAIRO_EXTEND_PAD, but pass a clip to the drawing
         * function to make sure that we never sample anything beyond
         * the texture boundaries. */
        if (pattern->extend == CAIRO_EXTEND_NONE && tex_clip)
            if (_cairo_cogl_create_tex_clip (tex_clip,
                                             attributes->matrix,
                                             is_mirrored_texture))
            {
                cogl_object_unref (texture);
                return NULL;
            }

	return texture;
    }
    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_MESH:
    case CAIRO_PATTERN_TYPE_RASTER_SOURCE: {
	cairo_surface_t *surface;
        cairo_matrix_t new_pattern_matrix;

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					      extents->width, extents->height);
	if (_cairo_surface_offset_paint (surface,
					 extents->x, extents->y,
					 CAIRO_OPERATOR_SOURCE,
					 pattern, NULL)) {
	    cairo_surface_destroy (surface);
	    return NULL;
	}

        cairo_matrix_init_translate (&new_pattern_matrix,
                                     -extents->x, -extents->y);

	texture =
            _cairo_cogl_acquire_generic_surface_texture (destination,
                                                         surface,
                                                         &new_pattern_matrix,
                                                         &attributes->matrix,
                                                         need_mirrored_texture,
                                                         &is_mirrored_texture);
	if (unlikely (!texture))
	    goto BAIL;

	attributes->extend = pattern->extend;
	attributes->filter = COGL_PIPELINE_FILTER_NEAREST;
	attributes->has_component_alpha = pattern->has_component_alpha;

	/* any pattern extend modes have already been dealt with... */
	attributes->s_wrap = COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
	attributes->t_wrap = attributes->s_wrap;

        /* In order to support CAIRO_EXTEND_NONE, we use the same wrap
         * mode as CAIRO_EXTEND_PAD, but pass a clip to the drawing
         * function to make sure that we never sample anything beyond
         * the texture boundaries. */
        if (pattern->extend == CAIRO_EXTEND_NONE && tex_clip)
            if (_cairo_cogl_create_tex_clip (tex_clip,
                                             attributes->matrix,
                                             is_mirrored_texture))
            {
                cogl_object_unref (texture);
                cairo_surface_destroy (surface);
                return NULL;
            }

BAIL:
	cairo_surface_destroy (surface);

	return texture;
    }
    case CAIRO_PATTERN_TYPE_LINEAR: {
	cairo_linear_pattern_t *linear_pattern = (cairo_linear_pattern_t *)pattern;
	cairo_cogl_linear_gradient_t *gradient;
	cairo_cogl_linear_texture_entry_t *linear_texture;
	cairo_int_status_t status;
        double dist, scale;

	status = _cairo_cogl_get_linear_gradient (to_device(destination->base.device),
						  pattern->extend,
						  linear_pattern->base.n_stops,
						  linear_pattern->base.stops,
                                                  need_mirrored_texture,
						  &gradient);
	if (unlikely (status))
	    return NULL;

	linear_texture = _cairo_cogl_linear_gradient_texture_for_extend (gradient, pattern->extend);

	attributes->extend = pattern->extend;
	attributes->filter =
            get_cogl_filter_for_filter (pattern->filter);
	attributes->has_component_alpha = pattern->has_component_alpha;
	attributes->s_wrap =
            get_cogl_wrap_mode_for_extend (pattern->extend, dev);
	attributes->t_wrap = attributes->s_wrap;

        attributes->matrix = pattern->matrix;

	double a = linear_pattern->pd2.x - linear_pattern->pd1.x;
	double b = linear_pattern->pd2.y - linear_pattern->pd1.y;
	double angle = - atan2f (b, a);

	cairo_matrix_rotate (&attributes->matrix, angle);

	cairo_matrix_translate (&attributes->matrix,
				-linear_pattern->pd1.x,
				-linear_pattern->pd1.y);

	/* Convert from un-normalized source coordinates in backend
	 * coordinates to normalized texture coordinates. */
	dist = sqrtf (a*a + b*b);
        if (need_mirrored_texture)
            scale = 0.5 / dist;
        else
	    scale = 1.0 / dist;
        _cairo_cogl_matrix_all_scale (&attributes->matrix,
                                      scale, scale);

	return cogl_object_ref (linear_texture->texture);
    }
    default:
	g_warning ("Unsupported source type");
	return NULL;
    }
}

static cairo_bool_t
set_blend (CoglPipeline *pipeline, const char *blend_string)
{
    CoglError *error = NULL;
    if (unlikely (!cogl_pipeline_set_blend (pipeline,
                                            blend_string,
                                            &error)))
    {
	g_warning ("Unsupported blend string with current gpu/driver: %s", blend_string);
	cogl_error_free (error);
	return FALSE;
    }
    return TRUE;
}

static cairo_bool_t
_cairo_cogl_setup_op_state (CoglPipeline    *pipeline,
                            cairo_operator_t op)
{
    cairo_bool_t status = FALSE;

    switch ((int)op)
    {
    case CAIRO_OPERATOR_OVER:
	status = set_blend (pipeline, "RGBA = ADD (SRC_COLOR, DST_COLOR * (1 - SRC_COLOR[A]))");
	break;
    case CAIRO_OPERATOR_IN:
	status = set_blend (pipeline, "RGBA = ADD (SRC_COLOR * DST_COLOR[A], 0)");
	break;
    case CAIRO_OPERATOR_OUT:
        status = set_blend (pipeline, "RGBA = ADD (SRC_COLOR * (1 - DST_COLOR[A]), 0)");
        break;
    case CAIRO_OPERATOR_ATOP:
        status = set_blend (pipeline, "RGBA = ADD (SRC_COLOR * DST_COLOR[A], DST_COLOR * (1 - SRC_COLOR[A]))");
        break;
    case CAIRO_OPERATOR_DEST:
        status = set_blend (pipeline, "RGBA = ADD (0, DST_COLOR)");
        break;
    case CAIRO_OPERATOR_DEST_OVER:
	status = set_blend (pipeline, "RGBA = ADD (SRC_COLOR * (1 - DST_COLOR[A]), DST_COLOR)");
	break;
    case CAIRO_OPERATOR_DEST_IN:
	status = set_blend (pipeline, "RGBA = ADD (0, DST_COLOR * SRC_COLOR[A])");
	break;
    case CAIRO_OPERATOR_DEST_OUT:
        status = set_blend (pipeline, "RGBA = ADD (0, DST_COLOR * (1 - SRC_COLOR[A]))");
        break;
    case CAIRO_OPERATOR_DEST_ATOP:
        status = set_blend (pipeline, "RGBA = ADD (SRC_COLOR * (1 - DST_COLOR[A]), DST_COLOR * SRC_COLOR[A])");
        break;
    case CAIRO_OPERATOR_XOR:
        status = set_blend (pipeline, "RGBA = ADD (SRC_COLOR * (1 - DST_COLOR[A]), DST_COLOR * (1 - SRC_COLOR[A]))");
        break;
    /* In order to handle SOURCE with a mask, we use two passes. The
     * first consists of a CAIRO_OPERATOR_DEST_OUT with the source alpha
     * replaced by the mask alpha in order to multiply all the
     * destination values by one minus the mask alpha. The second pass
     * (this one) then adds the source values, which have already been
     * premultiplied by the mask alpha. */
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_ADD:
	status = set_blend (pipeline, "RGBA = ADD (SRC_COLOR, DST_COLOR)");
	break;
    case CAIRO_OPERATOR_CLEAR:
        /* Runtime check */
        /* CAIRO_OPERATOR_CLEAR is not supposed to use its own pipeline
         * type. Use CAIRO_OPERATOR_DEST_OUT with the mask alpha as
         * source alpha instead. */
        assert (0);
    default:
        g_warning ("Unsupported blend operator");
        assert (0);
    }

    return status;
}

static void
create_template_for_op_type (cairo_cogl_device_t      *dev,
                              cairo_operator_t         op,
                              cairo_cogl_template_type type)
{
    CoglPipeline *pipeline;
    CoglColor color;

    if (dev->template_pipelines[op][type])
        return;

    cogl_color_init_from_4f (&color, 1.0f, 1.0f, 1.0f, 1.0f);

    if (!dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]) {
        CoglPipeline *base = cogl_pipeline_new (dev->cogl_context);

        if (!_cairo_cogl_setup_op_state (base, op)) {
            cogl_object_unref (base);
            return;
        }

        dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID] = base;
    }

    switch ((int)type)
    {
    case CAIRO_COGL_TEMPLATE_TYPE_SOLID:
        return;
    case CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_SOLID:
        pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
        cogl_pipeline_set_layer_combine_constant (pipeline, 0, &color);
        cogl_pipeline_set_layer_combine (pipeline, 0,
                                         "RGBA = MODULATE (PRIMARY, CONSTANT[A])",
                                         NULL);
        break;
    case CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_SOLID:
        pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
        cogl_pipeline_set_layer_null_texture (pipeline, 0,
                                              COGL_TEXTURE_TYPE_2D);
        cogl_pipeline_set_layer_combine (pipeline, 0,
                                         "RGBA = MODULATE (PRIMARY, TEXTURE[A])",
                                         NULL);
        break;
    case CAIRO_COGL_TEMPLATE_TYPE_TEXTURE:
        pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
        cogl_pipeline_set_layer_null_texture (pipeline, 0,
                                              COGL_TEXTURE_TYPE_2D);
        break;
    case CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_TEXTURE:
        pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
        cogl_pipeline_set_layer_null_texture (pipeline, 0,
                                              COGL_TEXTURE_TYPE_2D);
        cogl_pipeline_set_layer_combine_constant (pipeline, 1, &color);
        cogl_pipeline_set_layer_combine (pipeline, 1,
                                         "RGBA = MODULATE (PREVIOUS, CONSTANT[A])",
                                         NULL);
        break;
    case CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_TEXTURE:
        pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
        cogl_pipeline_set_layer_null_texture (pipeline, 0,
                                              COGL_TEXTURE_TYPE_2D);
        cogl_pipeline_set_layer_null_texture (pipeline, 1,
                                              COGL_TEXTURE_TYPE_2D);
        cogl_pipeline_set_layer_combine (pipeline, 1,
                                         "RGBA = MODULATE (PREVIOUS, TEXTURE[A])",
                                         NULL);
        break;
    case CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_IGNORE_ALPHA:
        pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
        cogl_pipeline_set_layer_null_texture (pipeline, 0,
                                              COGL_TEXTURE_TYPE_2D);
        /* We do not set the combine color when we use this template
         * pipeline, so the source texture alpha will be replaces by
         * ones */
        cogl_pipeline_set_layer_combine_constant (pipeline, 0, &color);
        cogl_pipeline_set_layer_combine (pipeline, 0,
                                         "RGB = REPLACE (TEXTURE)"
                                         "A = REPLACE (CONSTANT)",
                                         NULL);
        break;
    case CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_TEXTURE_IGNORE_ALPHA:
        pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
        cogl_pipeline_set_layer_null_texture (pipeline, 0,
                                              COGL_TEXTURE_TYPE_2D);
        /* We do not set the combine color when we use this template
         * pipeline, so the source texture alpha will be replaces by
         * ones */
        cogl_pipeline_set_layer_combine_constant (pipeline, 0, &color);
        cogl_pipeline_set_layer_combine (pipeline, 0,
                                         "RGB = REPLACE (TEXTURE)"
                                         "A = REPLACE (CONSTANT)",
                                         NULL);
        cogl_pipeline_set_layer_combine_constant (pipeline, 1, &color);
        cogl_pipeline_set_layer_combine (pipeline, 1,
                                         "RGBA = MODULATE (PREVIOUS, CONSTANT[A])",
                                         NULL);
        break;
    case CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_TEXTURE_IGNORE_ALPHA:
        pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][CAIRO_COGL_TEMPLATE_TYPE_SOLID]);
        cogl_pipeline_set_layer_null_texture (pipeline, 0,
                                              COGL_TEXTURE_TYPE_2D);
        /* We do not set the combine color when we use this template
         * pipeline, so the source texture alpha will be replaces by
         * ones */
        cogl_pipeline_set_layer_combine_constant (pipeline, 0, &color);
        cogl_pipeline_set_layer_combine (pipeline, 0,
                                         "RGB = REPLACE (TEXTURE)"
                                         "A = REPLACE (CONSTANT)",
                                         NULL);
        cogl_pipeline_set_layer_null_texture (pipeline, 1,
                                              COGL_TEXTURE_TYPE_2D);
        cogl_pipeline_set_layer_combine (pipeline, 1,
                                         "RGBA = MODULATE (PREVIOUS, TEXTURE[A])",
                                         NULL);
        break;
    default:
        g_warning ("Invalid cogl pipeline template type");
        return;
    }

    dev->template_pipelines[op][type] = pipeline;
}

static void
set_layer_texture_with_attributes (CoglPipeline                    *pipeline,
				   int                              layer_index,
				   CoglTexture                     *texture,
				   cairo_cogl_texture_attributes_t *attributes,
                                   cairo_matrix_t                  *path_transform)
{
    cairo_matrix_t m;

    cogl_pipeline_set_layer_texture (pipeline, layer_index, texture);

    cogl_pipeline_set_layer_filters (pipeline,
                                     layer_index,
                                     attributes->filter,
                                     attributes->filter);

    /* We multiply in the path transform here so that we read texture
     * values from coordinates that are consistent with the coordinates
     * of the path after it is transformed by the modelview matrix */
    if (path_transform)
        cairo_matrix_multiply (&m, path_transform, &attributes->matrix);
    else
        m = attributes->matrix;

    if (!_cairo_matrix_is_identity (&m)) {
	float texture_matrixfv[16] = {
	    m.xx, m.yx, 0, 0,
	    m.xy, m.yy, 0, 0,
	    0,    0,    1, 0,
	    m.x0, m.y0, 0, 1
	};
	CoglMatrix texture_matrix;
	cogl_matrix_init_from_array (&texture_matrix, texture_matrixfv);
	cogl_pipeline_set_layer_matrix (pipeline, layer_index, &texture_matrix);
    }

    if (attributes->s_wrap != attributes->t_wrap) {
	cogl_pipeline_set_layer_wrap_mode_s (pipeline, layer_index, attributes->s_wrap);
	cogl_pipeline_set_layer_wrap_mode_t (pipeline, layer_index, attributes->t_wrap);
    } else {
	cogl_pipeline_set_layer_wrap_mode (pipeline, layer_index, attributes->s_wrap);
    }
}

/* This takes an argument of a pointer to an array of two pointers to
 * #cairo_cogl_pipeline_t. On failure, both pointers will be set to
 * NULL */
static void
get_source_mask_operator_destination_pipelines (cairo_cogl_pipeline_t       **pipelines,
                                                const cairo_pattern_t        *mask,
					        const cairo_pattern_t        *source,
					        cairo_operator_t              op,
					        cairo_cogl_surface_t         *destination,
					        cairo_composite_rectangles_t *extents,
                                                cairo_matrix_t               *path_transform)
{
    cairo_cogl_template_type template_type;
    cairo_cogl_device_t *dev = to_device(destination->base.device);

    pipelines[0] = NULL;
    pipelines[1] = NULL;

    switch ((int)source->type)
    {
    case CAIRO_PATTERN_TYPE_SOLID:
        if (mask) {
            /* If the mask surface has no alpha content, we use a mask
             * of solid ones */
            if ((mask->type == CAIRO_PATTERN_TYPE_SOLID) ||
                (mask->type == CAIRO_PATTERN_TYPE_SURFACE &&
                 ((cairo_surface_pattern_t *)mask)->surface->content == CAIRO_CONTENT_COLOR))
                template_type =
                    CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_SOLID;
            else
                template_type =
                    CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_SOLID;
        } else {
            template_type = CAIRO_COGL_TEMPLATE_TYPE_SOLID;
        }
        break;
    case CAIRO_PATTERN_TYPE_SURFACE:
        /* If the source does not have alpha content, we have to use
         * a specialized set of texture combining functions in order to
         * ensure that if we have a CAIRO_FORMAT_RGB24 source, we are
         * ignoring the alpha and replacing it with ones. Otherwise, we
         * use the template types for any other type of non-solid
         * source. */
        if (((cairo_surface_pattern_t *)source)->surface->content ==
             CAIRO_CONTENT_COLOR)
        {
            if (mask) {
                /* If the mask surface has no alpha content, we use a
                 * mask of solid ones */
                if ((mask->type == CAIRO_PATTERN_TYPE_SOLID) ||
                    (mask->type == CAIRO_PATTERN_TYPE_SURFACE &&
                     ((cairo_surface_pattern_t *)mask)->surface->content == CAIRO_CONTENT_COLOR))
                    template_type =
                        CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_TEXTURE_IGNORE_ALPHA;
                else
                    template_type =
                        CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_TEXTURE_IGNORE_ALPHA;
            } else {
                template_type =
                    CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_IGNORE_ALPHA;
            }
            break;
        }
        // else fall through
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_MESH:
    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
        if (mask) {
            /* If the mask surface has no alpha content, we use a mask
             * of solid ones */
            if ((mask->type == CAIRO_PATTERN_TYPE_SOLID) ||
                (mask->type == CAIRO_PATTERN_TYPE_SURFACE &&
                 ((cairo_surface_pattern_t *)mask)->surface->content == CAIRO_CONTENT_COLOR))
                template_type =
                    CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_TEXTURE;
            else
                template_type =
                    CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_TEXTURE;
        } else {
            template_type = CAIRO_COGL_TEMPLATE_TYPE_TEXTURE;
        }
        break;
    default:
	g_warning ("Unsupported source type");
	return;
    }

    /* pipelines[0] is for pre-rendering the mask alpha in the case
     * that it cannot be represented through the source color alpha
     * value. For more details, go to the description in
     * _cairo_cogl_setup_op_state */
    if (op == CAIRO_OPERATOR_CLEAR || op == CAIRO_OPERATOR_SOURCE) {
        cairo_cogl_template_type prerender_type;

        pipelines[0] = g_new (cairo_cogl_pipeline_t, 1);

        if (mask && mask->type != CAIRO_PATTERN_TYPE_SOLID)
            prerender_type = CAIRO_COGL_TEMPLATE_TYPE_SOLID;
        else
            prerender_type = CAIRO_COGL_TEMPLATE_TYPE_TEXTURE;

        /* Lazily create pipeline templates */
        if (unlikely (dev->template_pipelines[CAIRO_OPERATOR_DEST_OUT][prerender_type] == NULL))
            create_template_for_op_type (dev,
                                         CAIRO_OPERATOR_DEST_OUT,
                                         prerender_type);

        pipelines[0]->pipeline =
            cogl_pipeline_copy (dev->template_pipelines[CAIRO_OPERATOR_DEST_OUT][prerender_type]);

        pipelines[0]->mask_bounded =
            _cairo_operator_bounded_by_mask (op);
        pipelines[0]->src_bounded =
            _cairo_operator_bounded_by_source (op);
        pipelines[0]->op = CAIRO_OPERATOR_DEST_OUT;
        pipelines[0]->n_layers = 0;
        pipelines[0]->has_src_tex_clip = FALSE;
        pipelines[0]->has_mask_tex_clip = FALSE;
        pipelines[0]->unbounded_extents = extents->unbounded;
    }

    /* pipelines[1] is for normal rendering, modulating the mask with
     * the source. Most operators will only need this pipeline. */
    if (op != CAIRO_OPERATOR_CLEAR) {
        pipelines[1] = g_new (cairo_cogl_pipeline_t, 1);

        /* Lazily create pipeline templates */
        if (unlikely (dev->template_pipelines[op][template_type] == NULL))
            create_template_for_op_type (dev, op, template_type);

        pipelines[1]->pipeline =
            cogl_pipeline_copy (dev->template_pipelines[op][template_type]);

        pipelines[1]->mask_bounded =
            _cairo_operator_bounded_by_mask (op);
        pipelines[1]->src_bounded =
            _cairo_operator_bounded_by_source (op);
        pipelines[1]->op = op;
        pipelines[1]->n_layers = 0;
        pipelines[1]->has_src_tex_clip = FALSE;
        pipelines[1]->has_mask_tex_clip = FALSE;
        pipelines[1]->unbounded_extents = extents->unbounded;
    }

    if (pipelines[1]) {
        if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
            cairo_solid_pattern_t *solid_pattern = (cairo_solid_pattern_t *)source;
            cogl_pipeline_set_color4f (pipelines[1]->pipeline,
                                       solid_pattern->color.red * solid_pattern->color.alpha,
                                       solid_pattern->color.green * solid_pattern->color.alpha,
                                       solid_pattern->color.blue * solid_pattern->color.alpha,
                                       solid_pattern->color.alpha);
        } else {
	    cairo_cogl_texture_attributes_t attributes;

            _cairo_path_fixed_init (&pipelines[1]->src_tex_clip);

	    CoglTexture *texture =
	        _cairo_cogl_acquire_pattern_texture (source, destination,
                                                     &extents->bounded,
                                                     &attributes,
                                                     &pipelines[1]->src_tex_clip);
            if (unlikely (!texture))
                goto BAIL;
            set_layer_texture_with_attributes (pipelines[1]->pipeline,
                                               pipelines[1]->n_layers++,
                                               texture,
                                               &attributes,
                                               path_transform);
            cogl_object_unref (texture);

            if (pipelines[1]->src_tex_clip.buf.base.num_ops > 0)
                pipelines[1]->has_src_tex_clip = TRUE;
            else
                _cairo_path_fixed_fini (&pipelines[1]->src_tex_clip);
        }
    }

    if (mask) {
	if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	    cairo_solid_pattern_t *solid_pattern = (cairo_solid_pattern_t *)mask;
	    CoglColor color;
	    cogl_color_init_from_4f (&color,
				     solid_pattern->color.red * solid_pattern->color.alpha,
				     solid_pattern->color.green * solid_pattern->color.alpha,
				     solid_pattern->color.blue * solid_pattern->color.alpha,
				     solid_pattern->color.alpha);
            if (pipelines[1])
	        cogl_pipeline_set_layer_combine_constant (pipelines[1]->pipeline,
                                                          pipelines[1]->n_layers++,
                                                          &color);
            if (pipelines[0])
                cogl_pipeline_set_color (pipelines[0]->pipeline,
                                         &color);
        /* If the only component present in our mask is a color
         * component, skip setting the layer texture, as we already
         * set a solid of uniform ones on it during the template
         * creation process */
	} else if (!(mask->type == CAIRO_PATTERN_TYPE_SURFACE &&
                    ((cairo_surface_pattern_t *)mask)->surface->content == CAIRO_CONTENT_COLOR)) {
	    cairo_cogl_texture_attributes_t attributes;
            cairo_path_fixed_t mask_tex_clip;

            _cairo_path_fixed_init (&mask_tex_clip);

	    CoglTexture *texture =
		_cairo_cogl_acquire_pattern_texture (mask, destination,
						     &extents->bounded,
						     &attributes,
                                                     &mask_tex_clip);
	    if (unlikely (!texture))
		goto BAIL;
            if (pipelines[1]) {
                if (mask_tex_clip.buf.base.num_ops > 0) {
                    pipelines[1]->has_mask_tex_clip = TRUE;
                    if (unlikely (_cairo_path_fixed_init_copy (&pipelines[1]->mask_tex_clip,
                                                               &mask_tex_clip)))
                        goto BAIL;
                }
	        set_layer_texture_with_attributes (pipelines[1]->pipeline,
                                                   pipelines[1]->n_layers++,
                                                   texture,
                                                   &attributes,
                                                   path_transform);
            }
            if (pipelines[0]) {
                if (mask_tex_clip.buf.base.num_ops > 0) {
                    pipelines[0]->has_mask_tex_clip = TRUE;
                    if (unlikely (_cairo_path_fixed_init_copy (&pipelines[0]->mask_tex_clip,
                                                               &mask_tex_clip)))
                        goto BAIL;
                }
                set_layer_texture_with_attributes (pipelines[0]->pipeline,
                                                   pipelines[0]->n_layers++,
                                                   texture,
                                                   &attributes,
                                                   path_transform);
            }

            _cairo_path_fixed_fini (&mask_tex_clip);
	    cogl_object_unref (texture);
	}
    }

    return;

BAIL:
    if (pipelines[0]) {
        cogl_object_unref (pipelines[0]->pipeline);
        if (pipelines[0]->has_src_tex_clip)
            _cairo_path_fixed_fini (&pipelines[0]->src_tex_clip);
        if (pipelines[0]->has_mask_tex_clip)
            _cairo_path_fixed_fini (&pipelines[0]->mask_tex_clip);
        g_free (pipelines[0]);
        pipelines[0] = NULL;
    }
    if (pipelines[1]) {
        cogl_object_unref (pipelines[1]->pipeline);
        if (pipelines[1]->has_src_tex_clip)
            _cairo_path_fixed_fini (&pipelines[1]->src_tex_clip);
        if (pipelines[1]->has_mask_tex_clip)
            _cairo_path_fixed_fini (&pipelines[1]->mask_tex_clip);
        g_free (pipelines[1]);
        pipelines[1] = NULL;
    }
}

#if 0
CoglPrimitive *
_cairo_cogl_rectangle_new_p2t2t2 (CoglContext *cogl_context,
                                  float        x,
                                  float        y,
                                  float        width,
                                  float        height)
{
    CoglVertexP2 vertices[] = {
	{x, y}, {x, y + height}, {x + width, y + height},
	{x, y}, {x + width, y + height}, {x + width, y}
    };
    CoglAttributeBuffer *buffer = cogl_attribute_buffer_new (cogl_context,
                                                             sizeof (vertices));
    CoglAttribute *pos = cogl_attribute_new (buffer,
					     "cogl_position_in",
					     sizeof (CoglVertexP2),
					     0,
					     2,
					     COGL_ATTRIBUTE_TYPE_FLOAT);
    CoglAttribute *tex_coords0 = cogl_attribute_new (buffer,
						     "cogl_tex_coord0_in",
						     sizeof (CoglVertexP2),
						     0,
						     2,
						     COGL_ATTRIBUTE_TYPE_FLOAT);
    CoglAttribute *tex_coords0 = cogl_attribute_new (buffer,
						     "cogl_tex_coord0_in",
						     sizeof (CoglVertexP2),
						     0,
						     2,
						     COGL_ATTRIBUTE_TYPE_FLOAT);
    CoglPrimitive *prim;

    cogl_buffer_set_data (buffer, 0, vertices, sizeof (vertices));

    /* The attributes will now keep the buffer alive... */
    cogl_object_unref (buffer);

    prim = cogl_primitive_new (COGL_VERTICES_MODE_TRIANGLES,
			       6, pos, tex_coords, NULL);

    /* The primitive will now keep the attribute alive... */
    cogl_object_unref (pos);

    return prim;
}
#endif

static void
_cairo_cogl_log_clip (cairo_cogl_surface_t *surface,
		      const cairo_clip_t   *clip)
{
    if (!_cairo_clip_equal (clip, surface->last_clip)) {
	_cairo_cogl_journal_log_clip (surface, clip);
	_cairo_clip_destroy (surface->last_clip);
	surface->last_clip = _cairo_clip_copy (clip);
    }
}

static void
_cairo_cogl_maybe_log_clip (cairo_cogl_surface_t         *surface,
			    cairo_composite_rectangles_t *composite)
{
    cairo_clip_t *clip = composite->clip;

    if (_cairo_composite_rectangles_can_reduce_clip (composite, clip))
	clip = NULL;

    if (clip == NULL) {
	if (_cairo_composite_rectangles_can_reduce_clip (composite,
							 surface->last_clip))
	    return;
    }

    _cairo_cogl_log_clip (surface, clip);
}

static cairo_bool_t
is_operator_supported (cairo_operator_t op)
{
    switch ((int)op) {
    case CAIRO_OPERATOR_CLEAR:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_ATOP:
    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_DEST_ATOP:
    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
	return TRUE;

    default:
        g_warning("cairo-cogl: Blend operator not supported");
	return FALSE;
    }
}

static cairo_int_status_t
_cairo_cogl_surface_paint (void                  *abstract_surface,
                           cairo_operator_t       op,
                           const cairo_pattern_t *source,
                           const cairo_clip_t    *clip)
{
    cairo_cogl_surface_t *surface;
    cairo_int_status_t status;
    cairo_matrix_t identity;
    cairo_cogl_pipeline_t *pipelines[2];
    cairo_composite_rectangles_t extents;

    if (clip == NULL) {
        status = _cairo_cogl_surface_ensure_framebuffer (abstract_surface);
        if (unlikely (status))
            return status;

	if (op == CAIRO_OPERATOR_CLEAR)
            return _cairo_cogl_surface_clear (abstract_surface, CAIRO_COLOR_TRANSPARENT);
	else if (source->type == CAIRO_PATTERN_TYPE_SOLID &&
                (op == CAIRO_OPERATOR_SOURCE ||
                 (op == CAIRO_OPERATOR_OVER && (((cairo_surface_t *)abstract_surface)->is_clear || _cairo_pattern_is_opaque_solid (source))))) {
            return _cairo_cogl_surface_clear (abstract_surface,
					      &((cairo_solid_pattern_t *) source)->color);
        }
    }

    /* fall back to handling the paint in terms of a rectangle... */

    surface = (cairo_cogl_surface_t *)abstract_surface;

    if (!is_operator_supported (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status =
        _cairo_composite_rectangles_init_for_paint (&extents,
                                                    &surface->base,
                                                    op,
                                                    source,
                                                    clip);
    if (unlikely (status))
	return status;

    get_source_mask_operator_destination_pipelines (pipelines,
                                                    NULL,
                                                    source,
                                                    op,
                                                    surface,
                                                    &extents,
                                                    NULL);
    if (unlikely (pipelines[0] == NULL && pipelines[1] == NULL)) {
        status = CAIRO_INT_STATUS_UNSUPPORTED;
        goto BAIL;
    }

    _cairo_cogl_maybe_log_clip (surface, &extents);

    cairo_matrix_init_identity (&identity);
    if (pipelines[0])
        _cairo_cogl_journal_log_rectangle (surface,
                                           pipelines[0],
                                           extents.bounded.x,
                                           extents.bounded.y,
                                           extents.bounded.width,
                                           extents.bounded.height,
                                           &identity);
    if (pipelines[1])
        _cairo_cogl_journal_log_rectangle (surface,
                                           pipelines[1],
                                           extents.bounded.x,
                                           extents.bounded.y,
                                           extents.bounded.width,
                                           extents.bounded.height,
                                           &identity);

BAIL:
    _cairo_composite_rectangles_fini (&extents);

    return status;
}

static cairo_int_status_t
_cairo_cogl_surface_mask (void                  *abstract_surface,
                          cairo_operator_t       op,
                          const cairo_pattern_t *source,
                          const cairo_pattern_t *mask,
                          const cairo_clip_t    *clip)
{
    cairo_cogl_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_int_status_t status;
    cairo_cogl_pipeline_t *pipelines[2];
    cairo_matrix_t identity;

    /* XXX: Use this to smoke test the acquire_source/dest_image fallback
     * paths... */
    //return CAIRO_INT_STATUS_UNSUPPORTED;

    if (!is_operator_supported (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_composite_rectangles_init_for_mask (&extents,
							&surface->base,
							op, source, mask, clip);
    if (unlikely (status))
	return status;

    get_source_mask_operator_destination_pipelines (pipelines,
                                                    mask,
                                                    source,
                                                    op,
                                                    surface,
                                                    &extents,
                                                    NULL);
    if (unlikely (pipelines[0] == NULL && pipelines[1] == NULL)) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
        goto BAIL;
    }

    _cairo_cogl_maybe_log_clip (surface, &extents);

    cairo_matrix_init_identity (&identity);
    if (pipelines[0])
        _cairo_cogl_journal_log_rectangle (surface,
                                           pipelines[0],
                                           extents.bounded.x,
                                           extents.bounded.y,
                                           extents.bounded.width,
                                           extents.bounded.height,
                                           &identity);
    if (pipelines[1])
        _cairo_cogl_journal_log_rectangle (surface,
                                           pipelines[1],
                                           extents.bounded.x,
                                           extents.bounded.y,
                                           extents.bounded.width,
                                           extents.bounded.height,
                                           &identity);

BAIL:
    _cairo_composite_rectangles_fini (&extents);

    return status;
}

static cairo_int_status_t
_cairo_cogl_surface_stroke (void                       *abstract_surface,
			    cairo_operator_t            op,
			    const cairo_pattern_t      *source,
			    const cairo_path_fixed_t   *path,
			    const cairo_stroke_style_t *style,
			    const cairo_matrix_t       *ctm,
			    const cairo_matrix_t       *ctm_inverse,
			    double                      tolerance,
			    cairo_antialias_t           antialias,
			    const cairo_clip_t         *clip)
{
    cairo_cogl_surface_t *surface = (cairo_cogl_surface_t *)abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_cogl_pipeline_t *pipelines[2];
    cairo_int_status_t status;
    cairo_matrix_t transform;
    CoglPrimitive *prim = NULL;

    if (! is_operator_supported (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_composite_rectangles_init_for_stroke (&extents,
							  &surface->base,
							  op, source, path,
							  style,
							  ctm,
							  clip);
    if (unlikely (status))
	return status;

    status = _cairo_cogl_stroke_to_primitive (surface, path, style,
                                              tolerance, TRUE, &prim,
                                              &transform);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO
        && _cairo_operator_bounded_by_mask (op) == FALSE) {
        /* Just render the unbounded rectangle */
        prim = NULL;
    } else if (unlikely (status)) {
        goto BAIL;
    }

    get_source_mask_operator_destination_pipelines (pipelines,
                                                    NULL,
                                                    source,
                                                    op,
                                                    surface,
                                                    &extents,
                                                    &transform);
    if (unlikely (pipelines[0] == NULL && pipelines[1] == NULL)) {
        status = CAIRO_INT_STATUS_UNSUPPORTED;
        goto BAIL;
    }

    _cairo_cogl_maybe_log_clip (surface, &extents);

    if (pipelines[0])
        _cairo_cogl_journal_log_primitive (surface,
                                           pipelines[0],
                                           prim,
                                           &transform);
    if (pipelines[1])
        _cairo_cogl_journal_log_primitive (surface,
                                           pipelines[1],
                                           prim,
                                           &transform);

BAIL:
    /* The journal will take a reference on the primitive... */
    if (prim)
	cogl_object_unref (prim);

    _cairo_composite_rectangles_fini (&extents);

    return status;
}

static cairo_int_status_t
_cairo_cogl_surface_fill (void			    *abstract_surface,
                          cairo_operator_t	     op,
                          const cairo_pattern_t	    *source,
                          const cairo_path_fixed_t  *path,
                          cairo_fill_rule_t	     fill_rule,
                          double		     tolerance,
                          cairo_antialias_t	     antialias,
                          const cairo_clip_t	    *clip)
{
    cairo_cogl_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_int_status_t status;
    cairo_matrix_t transform;
    CoglPrimitive *prim = NULL;
    cairo_cogl_pipeline_t *pipelines[2];

    if (! is_operator_supported (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_composite_rectangles_init_for_fill (&extents,
							&surface->base,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    status = _cairo_cogl_fill_to_primitive (surface, path, fill_rule,
                                            tolerance, TRUE, &prim,
                                            &transform);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO
        && _cairo_operator_bounded_by_mask (op) == FALSE) {
        /* Just render the unbounded rectangle */
        prim = NULL;
    } else if (unlikely (status)) {
        goto BAIL;
    }

    get_source_mask_operator_destination_pipelines (pipelines,
                                                    NULL,
                                                    source,
                                                    op,
                                                    surface,
                                                    &extents,
                                                    &transform);
    if (unlikely (pipelines[0] == NULL && pipelines[1] == NULL)) {
        status = CAIRO_INT_STATUS_UNSUPPORTED;
        goto BAIL;
    }

    _cairo_cogl_maybe_log_clip (surface, &extents);

    if (pipelines[0])
        _cairo_cogl_journal_log_primitive (surface,
                                           pipelines[0],
                                           prim,
                                           &transform);
    if (pipelines[1])
        _cairo_cogl_journal_log_primitive (surface,
                                           pipelines[1],
                                           prim,
                                           &transform);

BAIL:
    /* The journal will take a reference on the prim */
    if (prim)
	cogl_object_unref (prim);
    _cairo_composite_rectangles_fini (&extents);

    return status;
}

/* Mostly taken from #cairo_vg_surface.c */
/* TODO: implement actual font support, with either cogl-pango's glyph
 * cache or our own */
static cairo_int_status_t
_cairo_cogl_surface_show_glyphs (void                  *abstract_surface,
                                 cairo_operator_t       op,
                                 const cairo_pattern_t *source,
                                 cairo_glyph_t         *glyphs,
                                 int                    num_glyphs,
                                 cairo_scaled_font_t   *scaled_font,
                                 const cairo_clip_t    *clip)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_path_fixed_t path;
    int num_chunk_glyphs;
    int i;

    if (num_glyphs <= 0)
        return CAIRO_STATUS_SUCCESS;

#define GLYPH_CHUNK_SIZE 100

    /* Chunk glyphs in order to avoid large computation overheads
     * during tessellation of long strings */
    for (i = 0; i < num_glyphs; i += GLYPH_CHUNK_SIZE) {
        num_chunk_glyphs = (num_glyphs - i) < GLYPH_CHUNK_SIZE ?
                           (num_glyphs - i) : GLYPH_CHUNK_SIZE;

        _cairo_path_fixed_init (&path);
        status = _cairo_scaled_font_glyph_path (scaled_font,
                                                &glyphs[i],
                                                num_chunk_glyphs,
                                                &path);
        if (unlikely (status))
            goto BAIL;

        status = _cairo_cogl_surface_fill (abstract_surface,
                                           op, source, &path,
                                           CAIRO_FILL_RULE_WINDING,
                                           CAIRO_GSTATE_TOLERANCE_DEFAULT,
                                           CAIRO_ANTIALIAS_DEFAULT,
                                           clip);

        _cairo_path_fixed_fini (&path);
    }

#undef GLYPH_CHUNK_SIZE

    return CAIRO_STATUS_SUCCESS;

BAIL:
    _cairo_path_fixed_fini (&path);

    return status;
}

const cairo_surface_backend_t _cairo_cogl_surface_backend = {
    CAIRO_SURFACE_TYPE_COGL,
    _cairo_cogl_surface_finish,
    _cairo_default_context_create,

    _cairo_cogl_surface_create_similar,
    NULL, /* create similar image */
    NULL, /* map to image */
    NULL, /* unmap image */

    _cairo_surface_default_source,
    _cairo_cogl_surface_acquire_source_image,
    _cairo_cogl_surface_release_source_image,
    NULL, /* snapshot */

    NULL, /* copy_page */
    NULL, /* show_page */

    _cairo_cogl_surface_get_extents,
    NULL, /* get_font_options */

    _cairo_cogl_surface_flush, /* flush */
    NULL, /* mark_dirty_rectangle */

    _cairo_cogl_surface_paint,
    _cairo_cogl_surface_mask,
    _cairo_cogl_surface_stroke,
    _cairo_cogl_surface_fill,
    NULL, /* fill_stroke */
    _cairo_cogl_surface_show_glyphs,
};

static cairo_surface_t *
_cairo_cogl_surface_create_full (cairo_cogl_device_t *dev,
				 cairo_content_t      content,
				 CoglFramebuffer     *framebuffer,
				 CoglTexture         *texture)
{
    cairo_cogl_surface_t *surface;
    cairo_status_t status;

    status = cairo_device_acquire (&dev->base);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    surface = _cairo_malloc (sizeof (cairo_cogl_surface_t));
    if (unlikely (surface == NULL))
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    surface->is_mirrored_snapshot = FALSE;

    surface->framebuffer = framebuffer;
    if (framebuffer) {
        surface->width = cogl_framebuffer_get_width (framebuffer);
        surface->height = cogl_framebuffer_get_height (framebuffer);
	cogl_object_ref (framebuffer);
    }

    /* FIXME: If texture == NULL and we are given an offscreen framebuffer
     * then we want a way to poke inside the framebuffer to get a texture */
    surface->texture = texture;
    if (texture) {
	if (!framebuffer) {
            surface->width = cogl_texture_get_width (texture);
            surface->height = cogl_texture_get_height (texture);
        }
	cogl_object_ref (texture);
    }

    surface->journal = NULL;

    surface->last_clip = NULL;

    surface->n_clip_updates_per_frame = 0;

    surface->path_is_rectangle = FALSE;
    surface->user_path = NULL;

    _cairo_surface_init (&surface->base,
                         &_cairo_cogl_surface_backend,
                         &dev->base,
                         content,
			 FALSE); /* is_vector */

    return &surface->base;
}

cairo_surface_t *
cairo_cogl_surface_create_for_fb (cairo_device_t  *abstract_device,
                                  CoglFramebuffer *framebuffer,
                                  cairo_content_t  content)
{
    cairo_cogl_device_t *dev = (cairo_cogl_device_t *)abstract_device;

    if (abstract_device == NULL)
	return _cairo_surface_create_in_error (CAIRO_STATUS_DEVICE_ERROR);

    if (abstract_device->status)
	return _cairo_surface_create_in_error (abstract_device->status);

    if (abstract_device->backend->type != CAIRO_DEVICE_TYPE_COGL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    return _cairo_cogl_surface_create_full (dev,
                                            content,
                                            framebuffer,
                                            NULL);
}
slim_hidden_def (cairo_cogl_surface_create_for_fb);

cairo_surface_t *
cairo_cogl_onscreen_surface_create (cairo_device_t *abstract_device,
                                    cairo_content_t content,
                                    int width, int height)
{
    CoglFramebuffer *fb;
    CoglTextureComponents components;
    CoglError *error = NULL;
    cairo_surface_t *surface;
    cairo_cogl_device_t *dev = (cairo_cogl_device_t *)abstract_device;

    if (abstract_device->backend->type != CAIRO_DEVICE_TYPE_COGL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    /* We don't yet have a way to set the components of a framebuffer */
    components = get_components_from_cairo_content (content);

    fb = cogl_onscreen_new (dev->cogl_context, width, height);

    if (unlikely (!cogl_framebuffer_allocate (fb, &error))) {
        g_warning ("Could not allocate framebuffer for onscreen "
                   "surface: %s", error->message);
        cogl_error_free (error);
        return _cairo_surface_create_in_error (CAIRO_STATUS_DEVICE_ERROR);
    }
    cogl_framebuffer_orthographic (fb, 0, 0, width, height, -1, 100);

    surface = cairo_cogl_surface_create_for_fb (abstract_device,
                                                fb,
                                                content);

    /* The surface will take a reference on the framebuffer */
    cogl_object_unref (fb);

    return surface;
}
slim_hidden_def (cairo_cogl_onscreen_surface_create);

cairo_surface_t *
cairo_cogl_offscreen_surface_create (cairo_device_t *abstract_device,
                                     cairo_content_t content,
                                     int width, int height)
{
    CoglFramebuffer *fb;
    CoglTexture *tex;
    CoglError *error = NULL;
    cairo_surface_t *surface;
    cairo_cogl_device_t *dev = (cairo_cogl_device_t *)abstract_device;
    int tex_width = width;
    int tex_height = height;

    if (abstract_device->backend->type != CAIRO_DEVICE_TYPE_COGL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    /* If we cannot use an NPOT texture, allocate the texture in power
     * of two dimensions instead */
    if (!dev->has_npots) {
        tex_width = (int)pow (2, ceil (log2 (tex_width)));
        tex_height = (int)pow (2, ceil (log2 (tex_height)));
    }

    tex = cogl_texture_2d_new_with_size (dev->cogl_context,
                                         tex_width, tex_height);
    cogl_texture_set_components (tex,
        get_components_from_cairo_content (content));
    fb = cogl_offscreen_new_with_texture (tex);

    if (unlikely (!cogl_framebuffer_allocate (fb, &error))) {
        g_warning ("Could not allocate framebuffer for offscreen "
                   "surface: %s", error->message);
        cogl_error_free (error);
        return _cairo_surface_create_in_error (CAIRO_STATUS_DEVICE_ERROR);
    }
    cogl_framebuffer_orthographic (fb, 0, 0,
                                   tex_width, tex_height,
                                   -1, 100);

    /* The framebuffer will take a reference on the texture */
    cogl_object_unref (tex);

    surface = cairo_cogl_surface_create_for_fb (abstract_device,
                                                fb,
                                                content);

    /* The surface will take a reference on the framebuffer */
    cogl_object_unref (fb);

    ((cairo_cogl_surface_t *)surface)->width = width;
    ((cairo_cogl_surface_t *)surface)->height = height;

    return surface;
}
slim_hidden_def (cairo_cogl_offscreen_surface_create);

CoglFramebuffer *
cairo_cogl_surface_get_framebuffer (cairo_surface_t *abstract_surface)
{
    cairo_cogl_surface_t *surface;

    if (abstract_surface->backend != &_cairo_cogl_surface_backend) {
        _cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
        return NULL;
    }

    surface = (cairo_cogl_surface_t *) abstract_surface;

    return surface->framebuffer;
}
slim_hidden_def (cairo_cogl_surface_get_framebuffer);

CoglTexture *
cairo_cogl_surface_get_texture (cairo_surface_t *abstract_surface)
{
    cairo_cogl_surface_t *surface;

    if (abstract_surface->backend != &_cairo_cogl_surface_backend) {
        _cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
        return NULL;
    }

    surface = (cairo_cogl_surface_t *) abstract_surface;

    return surface->texture;
}
slim_hidden_def (cairo_cogl_surface_get_texture);

static cairo_status_t
_cairo_cogl_device_flush (void *device)
{
    cairo_status_t status;
    cairo_cogl_device_t *dev = device;

    status = cairo_device_acquire (device);
    if (unlikely (status))
	return status;

    /* XXX: we don't need to flush Cogl here, we just need to flush
     * any batching we do of compositing primitives. */

    if (dev->buffer_stack && dev->buffer_stack_offset) {
        cogl_buffer_unmap (dev->buffer_stack);
        cogl_object_unref (dev->buffer_stack);
        dev->buffer_stack = NULL;
    }

    cairo_device_release (device);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_cogl_device_finish (void *device)
{
    cairo_status_t status;
    cairo_cogl_device_t *dev = device;
    int i, j;

    status = cairo_device_acquire (device);
    if (unlikely (status))
	return;

    /* XXX: Drop references to external resources */

    _cairo_cache_fini (&dev->linear_cache);
    _cairo_cache_fini (&dev->path_fill_prim_cache);
    _cairo_cache_fini (&dev->path_stroke_prim_cache);

    _cairo_freelist_fini (&dev->path_fill_meta_freelist);
    _cairo_freelist_fini (&dev->path_stroke_meta_freelist);

    if (dev->buffer_stack && dev->buffer_stack_offset) {
        cogl_buffer_unmap (dev->buffer_stack);
        cogl_object_unref (dev->buffer_stack);
        dev->buffer_stack = NULL;
    }

    for (i = 0; i < CAIRO_OPERATOR_SATURATE; i++)
        for (j = 0; j < CAIRO_COGL_TEMPLATE_TYPE_COUNT; j++)
            if (dev->template_pipelines[i][j] != NULL) {
                cogl_object_unref (dev->template_pipelines[i][j]);
                dev->template_pipelines[i][j] = NULL;
            }

    cogl_object_unref (dev->cogl_context);

    cairo_device_release (device);
}

static void
_cairo_cogl_device_destroy (void *device)
{
    cairo_cogl_device_t *dev = device;

    g_free (dev);
}

static const cairo_device_backend_t _cairo_cogl_device_backend = {
    CAIRO_DEVICE_TYPE_COGL,

    NULL, /* lock */
    NULL, /* unlock */

    _cairo_cogl_device_flush,
    _cairo_cogl_device_finish,
    _cairo_cogl_device_destroy,
};

cairo_device_t *
cairo_cogl_device_create (CoglContext *cogl_context)
{
    cairo_cogl_device_t *dev = g_new (cairo_cogl_device_t, 1);
    cairo_status_t status;

    dev->cogl_context = cogl_object_ref (cogl_context);

    dev->has_npots =
        cogl_has_features (cogl_context,
                           COGL_FEATURE_ID_TEXTURE_NPOT_BASIC,
                           COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT,
                           0);

    dev->has_mirrored_repeat =
        cogl_has_feature (cogl_context,
                          COGL_FEATURE_ID_MIRRORED_REPEAT);

    dev->buffer_stack = NULL;
    dev->buffer_stack_size = 4096;

    /* Set all template pipelines to NULL */
    memset (dev->template_pipelines, 0, sizeof (dev->template_pipelines));

    status = _cairo_cache_init (&dev->linear_cache,
                                _cairo_cogl_linear_gradient_equal,
                                NULL,
                                (cairo_destroy_func_t) _cairo_cogl_linear_gradient_destroy,
                                CAIRO_COGL_LINEAR_GRADIENT_CACHE_SIZE);
    if (unlikely (status)) {
        g_free (dev);
        return _cairo_device_create_in_error (status);
    }

    status = _cairo_cache_init (&dev->path_fill_prim_cache,
                                _cairo_cogl_path_fill_meta_equal,
                                NULL,
                                (cairo_destroy_func_t) _cairo_cogl_path_fill_meta_destroy,
                                CAIRO_COGL_PATH_META_CACHE_SIZE);

    status = _cairo_cache_init (&dev->path_stroke_prim_cache,
                                _cairo_cogl_path_stroke_meta_equal,
                                NULL,
                                (cairo_destroy_func_t) _cairo_cogl_path_stroke_meta_destroy,
                                CAIRO_COGL_PATH_META_CACHE_SIZE);

    _cairo_freelist_init (&dev->path_fill_meta_freelist,
                          sizeof(cairo_cogl_path_fill_meta_t));
    _cairo_freelist_init (&dev->path_stroke_meta_freelist,
                          sizeof(cairo_cogl_path_stroke_meta_t));

    _cairo_device_init (&dev->base, &_cairo_cogl_device_backend);
    return &dev->base;
}
slim_hidden_def (cairo_cogl_device_create);

cairo_status_t
cairo_cogl_surface_end_frame (cairo_surface_t *abstract_surface)
{
    cairo_cogl_surface_t *surface = (cairo_cogl_surface_t *)abstract_surface;

    if (abstract_surface->backend != &_cairo_cogl_surface_backend)
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    cairo_surface_flush (abstract_surface);

    if (surface->framebuffer)
        if (cogl_is_onscreen (surface->framebuffer))
            cogl_onscreen_swap_buffers (surface->framebuffer);

    //g_print ("n_clip_updates_per_frame = %d\n", surface->n_clip_updates_per_frame);
    surface->n_clip_updates_per_frame = 0;

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def (cairo_cogl_surface_end_frame);

cairo_status_t
cairo_cogl_surface_synchronize (cairo_surface_t *abstract_surface)
{
    cairo_cogl_surface_t *surface = (cairo_cogl_surface_t *)abstract_surface;

    if (abstract_surface->backend != &_cairo_cogl_surface_backend)
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    if (surface->framebuffer)
        cogl_framebuffer_finish (surface->framebuffer);

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def (cairo_cogl_surface_synchronize);
