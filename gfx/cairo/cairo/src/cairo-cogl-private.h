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

#ifndef CAIRO_COGL_PRIVATE_H
#define CAIRO_COGL_PRIVATE_H

#include "cairo-device-private.h"
#include "cairo-cache-private.h"
#include "cairo-backend-private.h"
#include "cairo-default-context-private.h"
#include "cairo-surface-private.h"
#include "cairo-freelist-private.h"

#include <cogl/cogl2-experimental.h>

typedef enum _cairo_cogl_template_type {
	/* solid source */
    CAIRO_COGL_TEMPLATE_TYPE_SOLID,
    /* solid source with solid mask */
    CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_SOLID,
    /* solid source with texture mask */
    CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_SOLID,
    /* texture source */
    CAIRO_COGL_TEMPLATE_TYPE_TEXTURE,
    /* texture source with solid mask */
    CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_TEXTURE,
    /* texture source with texture mask */
    CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_TEXTURE,
    /* texture source with source alpha ignored */
    CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_IGNORE_ALPHA,
    /* texture source with solid mask with source alpha ignored */
    CAIRO_COGL_TEMPLATE_TYPE_SOLID_MASK_TEXTURE_IGNORE_ALPHA,
    /* texture source with texture mask with source alpha ignored */
    CAIRO_COGL_TEMPLATE_TYPE_TEXTURE_MASK_TEXTURE_IGNORE_ALPHA,
    CAIRO_COGL_TEMPLATE_TYPE_COUNT
} cairo_cogl_template_type;

typedef struct _cairo_cogl_device {
    cairo_device_t base;

    CoglContext *cogl_context;

    cairo_bool_t has_npots;
    cairo_bool_t has_mirrored_repeat;

    CoglAttributeBuffer *buffer_stack;
    size_t buffer_stack_size;
    size_t buffer_stack_offset;
    guint8 *buffer_stack_pointer;

    /* This is a limited set of templates because we don't support the
     * full range of operators that cairo has. The CAIRO_OPERATOR_ADD
     * is the operator enum with the highest value that we support so
     * we cap the size of the array by that.
     *
     * For each operator, we have a template for when we have a solid
     * source and a non-solid source. For each of those, we also have
     * additional templates for when we have a solid mask or a
     * non-solid mask, for a total of six templates per operator.
     *
     * The templates are set to null at device creation time and only
     * actually created on their first use.
     */
    CoglPipeline *template_pipelines[CAIRO_OPERATOR_ADD + 1][CAIRO_COGL_TEMPLATE_TYPE_COUNT];

    /* Caches 1d linear gradient textures */
    cairo_cache_t linear_cache;

    cairo_cache_t path_fill_prim_cache;
    cairo_cache_t path_stroke_prim_cache;

    cairo_freelist_t path_fill_meta_freelist;
    cairo_freelist_t path_stroke_meta_freelist;
} cairo_cogl_device_t;

typedef struct _cairo_cogl_clip_primitives {
    cairo_t *clip;
    CoglPrimitive **primitives;
} cairo_cogl_clip_primitives_t;

typedef struct _cairo_cogl_surface {
    cairo_surface_t base;

    /* We currently have 3 basic kinds of Cogl surfaces:
     * 1) A light surface simply wrapping a CoglTexture
     * 2) A CoglOffscreen framebuffer that implicitly also wraps a CoglTexture
     * 3) A CoglOnscreen framebuffer which could potentially be mapped to
     *    a CoglTexture (e.g. via tfp on X11) but we don't currently do
     *    that.
     */

    CoglTexture *texture;
    CoglFramebuffer *framebuffer;

    int width;
    int height;

    /* Is this a snapshot used for mirrored repeating on hardware which
     * doesn't have it, consisting of four reflected images? */
    cairo_bool_t is_mirrored_snapshot;

    GQueue *journal;

    cairo_clip_t *last_clip;

    /* A small fifo of recently used cairo_clip_ts paired with CoglPrimitives
     * that can be used to mask the stencil buffer. */
    GList *clips_fifo;

    int n_clip_updates_per_frame;

    /* Since the surface backend drawing operator functions don't get
     * passed the current cairo_t context we don't have a good way
     * to get our user-coordinates path into our surface_fill function.
     *
     * For now we use our _cairo_cogl_context_fill() wrapper to set this
     * side band data on the surface...
     */
    cairo_path_fixed_t *user_path;
    cairo_matrix_t ctm;
    cairo_matrix_t ctm_inverse;
    cairo_bool_t path_is_rectangle;
    double path_rectangle_x;
    double path_rectangle_y;
    double path_rectangle_width;
    double path_rectangle_height;
} cairo_cogl_surface_t;

cairo_status_t
_cairo_cogl_path_fixed_rectangle (cairo_path_fixed_t *path,
				  cairo_fixed_t x,
				  cairo_fixed_t y,
				  cairo_fixed_t width,
				  cairo_fixed_t height);

#endif /* CAIRO_COGL_PRIVATE_H */
