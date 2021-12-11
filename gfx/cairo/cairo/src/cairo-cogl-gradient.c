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
//#include "cairoint.h"

#include "cairo-cogl-private.h"
#include "cairo-cogl-gradient-private.h"
#include "cairo-image-surface-private.h"

#include <cogl/cogl2-experimental.h>
#include <glib.h>

//#define DUMP_GRADIENTS_TO_PNG

static unsigned long
_cairo_cogl_linear_gradient_hash (unsigned int                  n_stops,
				  const cairo_gradient_stop_t  *stops)
{
    return _cairo_hash_bytes (n_stops, stops,
                              sizeof (cairo_gradient_stop_t) * n_stops);
}

static cairo_cogl_linear_gradient_t *
_cairo_cogl_linear_gradient_lookup (cairo_cogl_device_t          *ctx,
				    unsigned long                 hash,
				    unsigned int                  n_stops,
				    const cairo_gradient_stop_t  *stops)
{
    cairo_cogl_linear_gradient_t lookup;

    lookup.cache_entry.hash = hash,
    lookup.n_stops = n_stops;
    lookup.stops = stops;

    return _cairo_cache_lookup (&ctx->linear_cache, &lookup.cache_entry);
}

cairo_bool_t
_cairo_cogl_linear_gradient_equal (const void *key_a, const void *key_b)
{
    const cairo_cogl_linear_gradient_t *a = key_a;
    const cairo_cogl_linear_gradient_t *b = key_b;

    if (a->n_stops != b->n_stops)
        return FALSE;

    return memcmp (a->stops, b->stops, a->n_stops * sizeof (cairo_gradient_stop_t)) == 0;
}

cairo_cogl_linear_gradient_t *
_cairo_cogl_linear_gradient_reference (cairo_cogl_linear_gradient_t *gradient)
{
    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&gradient->ref_count));

    _cairo_reference_count_inc (&gradient->ref_count);

    return gradient;
}

void
_cairo_cogl_linear_gradient_destroy (cairo_cogl_linear_gradient_t *gradient)
{
    GList *l;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&gradient->ref_count));

    if (! _cairo_reference_count_dec_and_test (&gradient->ref_count))
	return;

    for (l = gradient->textures; l; l = l->next) {
	cairo_cogl_linear_texture_entry_t *entry = l->data;
	cogl_object_unref (entry->texture);
	free (entry);
    }
    g_list_free (gradient->textures);

    free (gradient);
}

static int
_cairo_cogl_util_next_p2 (int a)
{
  int rval = 1;

  while (rval < a)
    rval <<= 1;

  return rval;
}

static float
get_max_color_component_range (const cairo_color_stop_t *color0,
                               const cairo_color_stop_t *color1)
{
    float range;
    float max = 0;

    range = fabs (color0->red - color1->red);
    max = MAX (range, max);
    range = fabs (color0->green - color1->green);
    max = MAX (range, max);
    range = fabs (color0->blue - color1->blue);
    max = MAX (range, max);
    range = fabs (color0->alpha - color1->alpha);
    max = MAX (range, max);

    return max;
}

static int
_cairo_cogl_linear_gradient_width_for_stops (cairo_extend_t		  extend,
					     unsigned int                 n_stops,
					     const cairo_gradient_stop_t *stops)
{
    unsigned int n;
    float max_texels_per_unit_offset = 0;
    float total_offset_range;

    /* Find the stop pair demanding the most precision because we are
     * interpolating the largest color-component range.
     *
     * From that we can define the relative sizes of all the other
     * stop pairs within our texture and thus the overall size.
     *
     * To determine the maximum number of texels for a given gap we
     * look at the range of colors we are expected to interpolate (so
     * long as the stop offsets are not degenerate) and we simply
     * assume we want one texel for each unique color value possible
     * for a one byte-per-component representation.
     * XXX: maybe this is overkill and just allowing 128 levels
     * instead of 256 would be enough and then we'd rely on the
     * bilinear filtering to give the full range.
     *
     * XXX: potentially we could try and map offsets to pixels to come
     * up with a more precise mapping, but we are aiming to cache
     * the gradients so we can't make assumptions about how it will be
     * scaled in the future.
     */
    for (n = 1; n < n_stops; n++) {
	float color_range;
	float offset_range;
	float texels;
	float texels_per_unit_offset;

	/* note: degenerate stops don't need to be represented in the
	 * texture but we want to be sure that solid gaps get at least
	 * one texel and all other gaps get at least 2 texels.
	 */

	if (stops[n].offset == stops[n-1].offset)
	    continue;

	color_range = get_max_color_component_range (&stops[n].color, &stops[n-1].color);
	if (color_range == 0)
	    texels = 1;
	else
	    texels = MAX (2, 256.0f * color_range);

	/* So how many texels would we need to map over the full [0,1]
	 * gradient range so this gap would have enough texels? ... */
	offset_range = stops[n].offset - stops[n - 1].offset;
	texels_per_unit_offset = texels / offset_range;

	if (texels_per_unit_offset > max_texels_per_unit_offset)
	    max_texels_per_unit_offset = texels_per_unit_offset;
    }

    total_offset_range = fabs (stops[n_stops - 1].offset - stops[0].offset);
    return max_texels_per_unit_offset * total_offset_range;
}

/* Aim to create gradient textures without an alpha component so we can avoid
 * needing to use blending... */
static CoglTextureComponents
_cairo_cogl_linear_gradient_components_for_stops (cairo_extend_t               extend,
					          unsigned int                 n_stops,
					          const cairo_gradient_stop_t *stops)
{
    unsigned int n;

    /* We have to add extra transparent texels to the end of the gradient to
     * handle CAIRO_EXTEND_NONE... */
    if (extend == CAIRO_EXTEND_NONE)
	return COGL_TEXTURE_COMPONENTS_RGBA;

    for (n = 1; n < n_stops; n++) {
	if (stops[n].color.alpha != 1.0)
	    return COGL_TEXTURE_COMPONENTS_RGBA;
    }

    return COGL_TEXTURE_COMPONENTS_RGBA;
}

static cairo_cogl_gradient_compatibility_t
_cairo_cogl_compatibility_from_extend_mode (cairo_extend_t extend_mode)
{
    switch (extend_mode)
    {
    case CAIRO_EXTEND_NONE:
	return CAIRO_COGL_GRADIENT_CAN_EXTEND_NONE;
    case CAIRO_EXTEND_PAD:
	return CAIRO_COGL_GRADIENT_CAN_EXTEND_PAD;
    case CAIRO_EXTEND_REPEAT:
	return CAIRO_COGL_GRADIENT_CAN_EXTEND_REPEAT;
    case CAIRO_EXTEND_REFLECT:
	return CAIRO_COGL_GRADIENT_CAN_EXTEND_REFLECT;
    }

    assert (0); /* not reached */
    return CAIRO_EXTEND_NONE;
}

cairo_cogl_linear_texture_entry_t *
_cairo_cogl_linear_gradient_texture_for_extend (cairo_cogl_linear_gradient_t *gradient,
						cairo_extend_t                extend_mode)
{
    GList *l;
    cairo_cogl_gradient_compatibility_t compatibility =
	_cairo_cogl_compatibility_from_extend_mode (extend_mode);
    for (l = gradient->textures; l; l = l->next) {
	cairo_cogl_linear_texture_entry_t *entry = l->data;
	if (entry->compatibility & compatibility)
	    return entry;
    }
    return NULL;
}

static void
color_stop_lerp (const cairo_color_stop_t *c0,
		 const cairo_color_stop_t *c1,
		 float factor,
		 cairo_color_stop_t *dest)
{
    /* NB: we always ignore the short members in this file so we don't need to
     * worry about initializing them here. */
    dest->red = c0->red * (1.0f-factor) + c1->red * factor;
    dest->green = c0->green * (1.0f-factor) + c1->green * factor;
    dest->blue = c0->blue * (1.0f-factor) + c1->blue * factor;
    dest->alpha = c0->alpha * (1.0f-factor) + c1->alpha * factor;
}

static size_t
_cairo_cogl_linear_gradient_size (cairo_cogl_linear_gradient_t *gradient)
{
    GList *l;
    size_t size = 0;
    for (l = gradient->textures; l; l = l->next) {
	cairo_cogl_linear_texture_entry_t *entry = l->data;
	size += cogl_texture_get_width (entry->texture) * 4;
    }
    return size;
}

static void
emit_stop (CoglVertexP2C4 **position,
	   float left,
	   float right,
	   const cairo_color_stop_t *left_color,
	   const cairo_color_stop_t *right_color)
{
    CoglVertexP2C4 *p = *position;

    guint8 lr = left_color->red * 255;
    guint8 lg = left_color->green * 255;
    guint8 lb = left_color->blue * 255;
    guint8 la = left_color->alpha * 255;

    guint8 rr = right_color->red * 255;
    guint8 rg = right_color->green * 255;
    guint8 rb = right_color->blue * 255;
    guint8 ra = right_color->alpha * 255;

    p[0].x = left;
    p[0].y = 0;
    p[0].r = lr; p[0].g = lg; p[0].b = lb; p[0].a = la;
    p[1].x = left;
    p[1].y = 1;
    p[1].r = lr; p[1].g = lg; p[1].b = lb; p[1].a = la;
    p[2].x = right;
    p[2].y = 1;
    p[2].r = rr; p[2].g = rg; p[2].b = rb; p[2].a = ra;

    p[3].x = left;
    p[3].y = 0;
    p[3].r = lr; p[3].g = lg; p[3].b = lb; p[3].a = la;
    p[4].x = right;
    p[4].y = 1;
    p[4].r = rr; p[4].g = rg; p[4].b = rb; p[4].a = ra;
    p[5].x = right;
    p[5].y = 0;
    p[5].r = rr; p[5].g = rg; p[5].b = rb; p[5].a = ra;

    *position = &p[6];
}

#ifdef DUMP_GRADIENTS_TO_PNG
static void
dump_gradient_to_png (CoglTexture *texture)
{
    cairo_image_surface_t *image = (cairo_image_surface_t *)
	cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
				    cogl_texture_get_width (texture),
				    cogl_texture_get_height (texture));
    CoglPixelFormat format;
    static int gradient_id = 0;
    char *gradient_name;

    if (image->base.status)
	return;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
#else
    format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
#endif
    cogl_texture_get_data (texture,
			   format,
			   0,
			   image->data);
    gradient_name = g_strdup_printf ("./gradient%d.png", gradient_id++);
    g_print ("writing gradient: %s\n", gradient_name);
    cairo_surface_write_to_png ((cairo_surface_t *)image, gradient_name);
    g_free (gradient_name);
}
#endif

cairo_int_status_t
_cairo_cogl_get_linear_gradient (cairo_cogl_device_t           *device,
				 cairo_extend_t                 extend_mode,
				 int                            n_stops,
				 const cairo_gradient_stop_t   *stops,
				 const cairo_bool_t             need_mirrored_gradient,
				 cairo_cogl_linear_gradient_t **gradient_out)
{
    unsigned long hash;
    cairo_cogl_linear_gradient_t *gradient;
    cairo_cogl_linear_texture_entry_t *entry;
    cairo_gradient_stop_t *internal_stops;
    int stop_offset;
    int n_internal_stops;
    int n;
    cairo_cogl_gradient_compatibility_t compatibilities;
    int width;
    int tex_width;
    int left_padding = 0;
    cairo_color_stop_t left_padding_color;
    int right_padding = 0;
    cairo_color_stop_t right_padding_color;
    CoglTextureComponents components;
    CoglTexture2D *tex;
    int un_padded_width;
    CoglFramebuffer *offscreen = NULL;
    cairo_int_status_t status;
    int n_quads;
    int n_vertices;
    float prev;
    float right;
    CoglVertexP2C4 *vertices;
    CoglVertexP2C4 *p;
    CoglPrimitive *prim;
    CoglPipeline *pipeline;

    hash = _cairo_cogl_linear_gradient_hash (n_stops, stops);

    gradient = _cairo_cogl_linear_gradient_lookup (device, hash, n_stops, stops);
    if (gradient) {
	cairo_cogl_linear_texture_entry_t *entry =
	    _cairo_cogl_linear_gradient_texture_for_extend (gradient, extend_mode);
	if (entry) {
	    *gradient_out = _cairo_cogl_linear_gradient_reference (gradient);
	    return CAIRO_INT_STATUS_SUCCESS;
	}
    }

    if (!gradient) {
	gradient = _cairo_malloc (sizeof (cairo_cogl_linear_gradient_t) +
			   sizeof (cairo_gradient_stop_t) * (n_stops - 1));
	if (!gradient)
	    return CAIRO_INT_STATUS_NO_MEMORY;

	CAIRO_REFERENCE_COUNT_INIT (&gradient->ref_count, 1);
	/* NB: we update the cache_entry size at the end before
	 * [re]adding it to the cache. */
	gradient->cache_entry.hash = hash;
	gradient->textures = NULL;
	gradient->n_stops = n_stops;
	gradient->stops = gradient->stops_embedded;
	memcpy (gradient->stops_embedded, stops, sizeof (cairo_gradient_stop_t) * n_stops);
    } else {
	_cairo_cogl_linear_gradient_reference (gradient);
    }

    entry = _cairo_malloc (sizeof (cairo_cogl_linear_texture_entry_t));
    if (unlikely (!entry)) {
	status = CAIRO_INT_STATUS_NO_MEMORY;
	goto BAIL;
    }

    compatibilities = _cairo_cogl_compatibility_from_extend_mode (extend_mode);

    n_internal_stops = n_stops;
    stop_offset = 0;

    /* We really need stops covering the full [0,1] range for repeat/reflect
     * if we want to use sampler REPEAT/MIRROR wrap modes so we may need
     * to add some extra stops... */
    if (extend_mode == CAIRO_EXTEND_REPEAT || extend_mode == CAIRO_EXTEND_REFLECT)
    {
	/* If we don't need any extra stops then actually the texture
	 * will be shareable for repeat and reflect... */
	compatibilities = (CAIRO_COGL_GRADIENT_CAN_EXTEND_REPEAT |
			   CAIRO_COGL_GRADIENT_CAN_EXTEND_REFLECT);

	if (stops[0].offset != 0) {
	    n_internal_stops++;
	    stop_offset++;
	}

	if (stops[n_stops - 1].offset != 1)
	    n_internal_stops++;
    }

    internal_stops = alloca (n_internal_stops * sizeof (cairo_gradient_stop_t));
    memcpy (&internal_stops[stop_offset], stops, sizeof (cairo_gradient_stop_t) * n_stops);

    /* cairo_color_stop_t values are all unpremultiplied but we need to
     * interpolate premultiplied colors so we premultiply all the double
     * components now. (skipping any extra stops added for repeat/reflect)
     *
     * Another thing to note is that by premultiplying the colors
     * early we'll also reduce the range of colors to interpolate
     * which can result in smaller gradient textures.
     */
    for (n = stop_offset; n < n_stops; n++) {
	cairo_color_stop_t *color = &internal_stops[n].color;
	color->red *= color->alpha;
	color->green *= color->alpha;
	color->blue *= color->alpha;
    }

    if (n_internal_stops != n_stops)
    {
	if (extend_mode == CAIRO_EXTEND_REPEAT) {
	    compatibilities &= ~CAIRO_COGL_GRADIENT_CAN_EXTEND_REFLECT;
	    if (stops[0].offset != 0) {
		/* what's the wrap-around distance between the user's end-stops? */
		double dx = (1.0 - stops[n_stops - 1].offset) + stops[0].offset;
		internal_stops[0].offset = 0;
		color_stop_lerp (&stops[0].color,
				 &stops[n_stops - 1].color,
				 stops[0].offset / dx,
				 &internal_stops[0].color);
	    }
	    if (stops[n_stops - 1].offset != 1) {
		internal_stops[n_internal_stops - 1].offset = 1;
		internal_stops[n_internal_stops - 1].color = internal_stops[0].color;
	    }
	} else if (extend_mode == CAIRO_EXTEND_REFLECT) {
	    compatibilities &= ~CAIRO_COGL_GRADIENT_CAN_EXTEND_REPEAT;
	    if (stops[0].offset != 0) {
		internal_stops[0].offset = 0;
		internal_stops[0].color = stops[n_stops - 1].color;
	    }
	    if (stops[n_stops - 1].offset != 1) {
		internal_stops[n_internal_stops - 1].offset = 1;
		internal_stops[n_internal_stops - 1].color = stops[0].color;
	    }
	}
    }

    stops = internal_stops;
    n_stops = n_internal_stops;

    width = _cairo_cogl_linear_gradient_width_for_stops (extend_mode, n_stops, stops);

    if (extend_mode == CAIRO_EXTEND_PAD) {

	/* Here we need to guarantee that the edge texels of our
	 * texture correspond to the desired padding color so we
	 * can use CLAMP_TO_EDGE.
	 *
	 * For short stop-gaps and especially for degenerate stops
	 * it's possible that without special consideration the
	 * user's end stop colors would not be present in our final
	 * texture.
	 *
	 * To handle this we forcibly add two extra padding texels
	 * at the edges which extend beyond the [0,1] range of the
	 * gradient itself and we will later report a translate and
	 * scale transform to compensate for this.
	 */

	/* XXX: If we consider generating a mipmap for our 1d texture
	 * at some point then we also need to consider how much
	 * padding to add to be sure lower mipmap levels still have
	 * the desired edge color (as opposed to a linear blend with
	 * other colors of the gradient).
	 */

	left_padding = 1;
	left_padding_color = stops[0].color;
	right_padding = 1;
	right_padding_color = stops[n_stops - 1].color;
    } else if (extend_mode == CAIRO_EXTEND_NONE) {
	/* We handle EXTEND_NONE by adding two extra, transparent, texels at
	 * the ends of the texture and use CLAMP_TO_EDGE.
	 *
	 * We add a scale and translate transform so to account for our texels
	 * extending beyond the [0,1] range. */

	left_padding = 1;
	left_padding_color.red = 0;
	left_padding_color.green = 0;
	left_padding_color.blue = 0;
	left_padding_color.alpha = 0;
	right_padding = 1;
	right_padding_color = left_padding_color;
    }

    /* If we still have stops that don't cover the full [0,1] range
     * then we need to define a texture-coordinate scale + translate
     * transform to account for that... */
    if (stops[n_stops - 1].offset - stops[0].offset < 1) {
	float range = stops[n_stops - 1].offset - stops[0].offset;
	entry->scale_x = 1.0 / range;
	entry->translate_x = -(stops[0].offset * entry->scale_x);
    }

    width += left_padding + right_padding;

    width = _cairo_cogl_util_next_p2 (width);
    width = MIN (4096, width); /* lets not go too stupidly big! */

    if (!device->has_npots)
        width = pow (2, ceil (log2 (width)));

    if (need_mirrored_gradient)
        tex_width = width * 2;
    else
        tex_width = width;

    components = _cairo_cogl_linear_gradient_components_for_stops (extend_mode, n_stops, stops);

    do {
	tex = cogl_texture_2d_new_with_size (device->cogl_context,
	                                     tex_width, 1);
    } while (tex == NULL && width >> 1 && tex_width >> 1);

    if (unlikely (!tex)) {
	status = CAIRO_INT_STATUS_NO_MEMORY;
	goto BAIL;
    }

    cogl_texture_set_components (tex, components);

    entry->texture = tex;
    entry->compatibility = compatibilities;

    un_padded_width = width - left_padding - right_padding;

    /* XXX: only when we know the final texture width can we calculate the
     * scale and translate factors needed to account for padding... */
    if (un_padded_width != width)
	entry->scale_x *= (float)un_padded_width / (float)width;
    if (left_padding)
	entry->translate_x += (entry->scale_x / (float)un_padded_width) * (float)left_padding;

    offscreen = cogl_offscreen_new_with_texture (tex);
    cogl_framebuffer_orthographic (offscreen, 0, 0,
                                              tex_width, 1,
                                              -1, 100);
    cogl_framebuffer_clear4f (offscreen,
                              COGL_BUFFER_BIT_COLOR,
                              0, 0, 0, 0);

    n_quads = n_stops - 1 + !!left_padding + !!right_padding;
    n_vertices = 6 * n_quads;
    vertices = _cairo_malloc_ab (n_vertices, sizeof (CoglVertexP2C4));
    if (unlikely (!vertices)) {
        status = CAIRO_INT_STATUS_NO_MEMORY;
        goto BAIL;
    }

    p = vertices;
    if (left_padding)
	emit_stop (&p, 0, left_padding, &left_padding_color, &left_padding_color);
    prev = (float)left_padding;
    for (n = 1; n < n_stops; n++) {
	right = (float)left_padding + (float)un_padded_width * stops[n].offset;
	emit_stop (&p, prev, right, &stops[n-1].color, &stops[n].color);
	prev = right;
    }
    if (right_padding)
	emit_stop (&p, prev, width, &right_padding_color, &right_padding_color);

    prim = cogl_primitive_new_p2c4 (device->cogl_context,
                                    COGL_VERTICES_MODE_TRIANGLES,
                                    n_vertices,
                                    vertices);
    free (vertices);
    pipeline = cogl_pipeline_new (device->cogl_context);
    cogl_primitive_draw (prim, offscreen, pipeline);

    if (need_mirrored_gradient) {
        /* In order to use a reflected gradient on hardware that
         * doesn't have a mirrored repeating texture wrap mode, we
         * render two reflected images to a double-length linear
         * texture and reflect that */
        CoglMatrix transform;

        cogl_matrix_init_identity (&transform);
        cogl_matrix_translate (&transform, tex_width, 0.0f, 0.0f);
        cogl_matrix_scale (&transform, -1.0f, 1.0f, 1.0f);

        cogl_framebuffer_transform (offscreen, &transform);
        cogl_primitive_draw (prim, offscreen, pipeline);
    }

    cogl_object_unref (prim);
    cogl_object_unref (pipeline);

    cogl_object_unref (offscreen);
    offscreen = NULL;

    gradient->textures = g_list_prepend (gradient->textures, entry);
    gradient->cache_entry.size = _cairo_cogl_linear_gradient_size (gradient);

#ifdef DUMP_GRADIENTS_TO_PNG
    dump_gradient_to_png (tex);
#endif

#warning "FIXME:"
    /* XXX: it seems the documentation of _cairo_cache_insert isn't true - it
     * doesn't handle re-adding the same entry gracefully - the cache will
     * just keep on growing and then it will start randomly evicting things
     * pointlessly */
    /* we ignore errors here and just return an uncached gradient */
    if (likely (! _cairo_cache_insert (&device->linear_cache, &gradient->cache_entry)))
        _cairo_cogl_linear_gradient_reference (gradient);

    *gradient_out = gradient;
    return CAIRO_INT_STATUS_SUCCESS;

BAIL:
    free (entry);
    if (gradient)
	_cairo_cogl_linear_gradient_destroy (gradient);
    if (offscreen)
        cogl_object_unref (offscreen);
    return status;
}
