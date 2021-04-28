/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006, 2008 Red Hat, Inc
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
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *      Kristian Høgsberg <krh@redhat.com>
 *      Behdad Esfahbod <behdad@behdad.org>
 */

#include "cairoint.h"
#include "cairo-user-font-private.h"
#include "cairo-recording-surface-private.h"
#include "cairo-analysis-surface-private.h"
#include "cairo-error-private.h"

/**
 * SECTION:cairo-user-fonts
 * @Title:User Fonts
 * @Short_Description: Font support with font data provided by the user
 * 
 * The user-font feature allows the cairo user to provide drawings for glyphs
 * in a font.  This is most useful in implementing fonts in non-standard
 * formats, like SVG fonts and Flash fonts, but can also be used by games and
 * other application to draw "funky" fonts.
 **/

/**
 * CAIRO_HAS_USER_FONT:
 *
 * Defined if the user font backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 * The user font backend is always built in versions of cairo that support
 * this feature (1.8 and later).
 *
 * Since: 1.8
 **/

typedef struct _cairo_user_scaled_font_methods {
    cairo_user_scaled_font_init_func_t			init;
    cairo_user_scaled_font_render_glyph_func_t		render_glyph;
    cairo_user_scaled_font_unicode_to_glyph_func_t	unicode_to_glyph;
    cairo_user_scaled_font_text_to_glyphs_func_t	text_to_glyphs;
} cairo_user_scaled_font_methods_t;

typedef struct _cairo_user_font_face {
    cairo_font_face_t	             base;

    /* Set to true after first scaled font is created.  At that point,
     * the scaled_font_methods cannot change anymore. */
    cairo_bool_t		     immutable;

    cairo_user_scaled_font_methods_t scaled_font_methods;
} cairo_user_font_face_t;

typedef struct _cairo_user_scaled_font {
    cairo_scaled_font_t  base;

    cairo_text_extents_t default_glyph_extents;

    /* space to compute extents in, and factors to convert back to user space */
    cairo_matrix_t extent_scale;
    double extent_x_scale;
    double extent_y_scale;

    /* multiplier for metrics hinting */
    double snap_x_scale;
    double snap_y_scale;

} cairo_user_scaled_font_t;

/* #cairo_user_scaled_font_t */

static cairo_surface_t *
_cairo_user_scaled_font_create_recording_surface (const cairo_user_scaled_font_t *scaled_font)
{
    cairo_content_t content;

    content = scaled_font->base.options.antialias == CAIRO_ANTIALIAS_SUBPIXEL ?
						     CAIRO_CONTENT_COLOR_ALPHA :
						     CAIRO_CONTENT_ALPHA;

    return cairo_recording_surface_create (content, NULL);
}


static cairo_t *
_cairo_user_scaled_font_create_recording_context (const cairo_user_scaled_font_t *scaled_font,
						  cairo_surface_t                *recording_surface)
{
    cairo_t *cr;

    cr = cairo_create (recording_surface);

    if (!_cairo_matrix_is_scale_0 (&scaled_font->base.scale)) {
        cairo_matrix_t scale;
	scale = scaled_font->base.scale;
	scale.x0 = scale.y0 = 0.;
	cairo_set_matrix (cr, &scale);
    }

    cairo_set_font_size (cr, 1.0);
    cairo_set_font_options (cr, &scaled_font->base.options);
    cairo_set_source_rgb (cr, 1., 1., 1.);

    return cr;
}

static cairo_int_status_t
_cairo_user_scaled_glyph_init (void			 *abstract_font,
			       cairo_scaled_glyph_t	 *scaled_glyph,
			       cairo_scaled_glyph_info_t  info)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_user_scaled_font_t *scaled_font = abstract_font;
    cairo_surface_t *recording_surface = scaled_glyph->recording_surface;

    if (!scaled_glyph->recording_surface) {
	cairo_user_font_face_t *face =
	    (cairo_user_font_face_t *) scaled_font->base.font_face;
	cairo_text_extents_t extents = scaled_font->default_glyph_extents;
	cairo_t *cr;

	if (!face->scaled_font_methods.render_glyph)
	    return CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED;

	recording_surface = _cairo_user_scaled_font_create_recording_surface (scaled_font);

	/* special case for 0 rank matrix (as in _cairo_scaled_font_init): empty surface */
        if (!_cairo_matrix_is_scale_0 (&scaled_font->base.scale)) {
	    cr = _cairo_user_scaled_font_create_recording_context (scaled_font, recording_surface);
	    status = face->scaled_font_methods.render_glyph ((cairo_scaled_font_t *)scaled_font,
							     _cairo_scaled_glyph_index(scaled_glyph),
							     cr, &extents);
	    if (status == CAIRO_INT_STATUS_SUCCESS)
	        status = cairo_status (cr);

	    cairo_destroy (cr);

	    if (unlikely (status)) {
	        cairo_surface_destroy (recording_surface);
	        return status;
	    }
	}

	_cairo_scaled_glyph_set_recording_surface (scaled_glyph,
						   &scaled_font->base,
						   recording_surface);


	/* set metrics */

	if (extents.width == 0.) {
	    cairo_box_t bbox;
	    double x1, y1, x2, y2;
	    double x_scale, y_scale;

	    /* Compute extents.x/y/width/height from recording_surface,
	     * in font space.
	     */
	    status = _cairo_recording_surface_get_bbox ((cairo_recording_surface_t *) recording_surface,
							&bbox,
							&scaled_font->extent_scale);
	    if (unlikely (status))
		return status;

	    _cairo_box_to_doubles (&bbox, &x1, &y1, &x2, &y2);

	    x_scale = scaled_font->extent_x_scale;
	    y_scale = scaled_font->extent_y_scale;
	    extents.x_bearing = x1 * x_scale;
	    extents.y_bearing = y1 * y_scale;
	    extents.width     = (x2 - x1) * x_scale;
	    extents.height    = (y2 - y1) * y_scale;
	}

	if (scaled_font->base.options.hint_metrics != CAIRO_HINT_METRICS_OFF) {
	    extents.x_advance = _cairo_lround (extents.x_advance / scaled_font->snap_x_scale) * scaled_font->snap_x_scale;
	    extents.y_advance = _cairo_lround (extents.y_advance / scaled_font->snap_y_scale) * scaled_font->snap_y_scale;
	}

	_cairo_scaled_glyph_set_metrics (scaled_glyph,
					 &scaled_font->base,
					 &extents);
    }

    if (info & CAIRO_SCALED_GLYPH_INFO_SURFACE) {
	cairo_surface_t	*surface;
	cairo_format_t format;
	int width, height;

	/* TODO
	 * extend the glyph cache to support argb glyphs.
	 * need to figure out the semantics and interaction with subpixel
	 * rendering first.
	 */

	width = _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.x) -
	  _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.x);
	height = _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.y) -
	  _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.y);

	switch (scaled_font->base.options.antialias) {
	default:
	case CAIRO_ANTIALIAS_DEFAULT:
	case CAIRO_ANTIALIAS_FAST:
	case CAIRO_ANTIALIAS_GOOD:
	case CAIRO_ANTIALIAS_GRAY:	format = CAIRO_FORMAT_A8;	break;
	case CAIRO_ANTIALIAS_NONE:	format = CAIRO_FORMAT_A1;	break;
	case CAIRO_ANTIALIAS_BEST:
	case CAIRO_ANTIALIAS_SUBPIXEL:	format = CAIRO_FORMAT_ARGB32;	break;
	}
	surface = cairo_image_surface_create (format, width, height);

	cairo_surface_set_device_offset (surface,
	                                 - _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.x),
	                                 - _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.y));
	status = _cairo_recording_surface_replay (recording_surface, surface);

	if (unlikely (status)) {
	    cairo_surface_destroy(surface);
	    return status;
	}

	_cairo_scaled_glyph_set_surface (scaled_glyph,
					 &scaled_font->base,
					 (cairo_image_surface_t *) surface);
    }

    if (info & CAIRO_SCALED_GLYPH_INFO_PATH) {
	cairo_path_fixed_t *path = _cairo_path_fixed_create ();
	if (!path)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_recording_surface_get_path (recording_surface, path);
	if (unlikely (status)) {
	    _cairo_path_fixed_destroy (path);
	    return status;
	}

	_cairo_scaled_glyph_set_path (scaled_glyph,
				      &scaled_font->base,
				      path);
    }

    return status;
}

static unsigned long
_cairo_user_ucs4_to_index (void	    *abstract_font,
			   uint32_t  ucs4)
{
    cairo_user_scaled_font_t *scaled_font = abstract_font;
    cairo_user_font_face_t *face =
	(cairo_user_font_face_t *) scaled_font->base.font_face;
    unsigned long glyph = 0;

    if (face->scaled_font_methods.unicode_to_glyph) {
	cairo_status_t status;

	status = face->scaled_font_methods.unicode_to_glyph (&scaled_font->base,
							     ucs4, &glyph);

	if (status == CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED)
	    goto not_implemented;

	if (status != CAIRO_STATUS_SUCCESS) {
	    status = _cairo_scaled_font_set_error (&scaled_font->base, status);
	    glyph = 0;
	}

    } else {
not_implemented:
	glyph = ucs4;
    }

    return glyph;
}

static cairo_int_status_t
_cairo_user_text_to_glyphs (void		      *abstract_font,
			    double		       x,
			    double		       y,
			    const char		      *utf8,
			    int			       utf8_len,
			    cairo_glyph_t	     **glyphs,
			    int			       *num_glyphs,
			    cairo_text_cluster_t      **clusters,
			    int			       *num_clusters,
			    cairo_text_cluster_flags_t *cluster_flags)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

    cairo_user_scaled_font_t *scaled_font = abstract_font;
    cairo_user_font_face_t *face =
	(cairo_user_font_face_t *) scaled_font->base.font_face;

    if (face->scaled_font_methods.text_to_glyphs) {
	int i;
	cairo_glyph_t *orig_glyphs = *glyphs;
	int orig_num_glyphs = *num_glyphs;

	status = face->scaled_font_methods.text_to_glyphs (&scaled_font->base,
							   utf8, utf8_len,
							   glyphs, num_glyphs,
							   clusters, num_clusters, cluster_flags);

	if (status != CAIRO_INT_STATUS_SUCCESS &&
	    status != CAIRO_INT_STATUS_USER_FONT_NOT_IMPLEMENTED)
	    return status;

	if (status == CAIRO_INT_STATUS_USER_FONT_NOT_IMPLEMENTED ||
	    *num_glyphs < 0) {
	    if (orig_glyphs != *glyphs) {
		cairo_glyph_free (*glyphs);
		*glyphs = orig_glyphs;
	    }
	    *num_glyphs = orig_num_glyphs;
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	/* Convert from font space to user space and add x,y */
	for (i = 0; i < *num_glyphs; i++) {
	    double gx = (*glyphs)[i].x;
	    double gy = (*glyphs)[i].y;

	    cairo_matrix_transform_point (&scaled_font->base.font_matrix,
					  &gx, &gy);

	    (*glyphs)[i].x = gx + x;
	    (*glyphs)[i].y = gy + y;
	}
    }

    return status;
}

static cairo_status_t
_cairo_user_font_face_scaled_font_create (void                        *abstract_face,
					  const cairo_matrix_t        *font_matrix,
					  const cairo_matrix_t        *ctm,
					  const cairo_font_options_t  *options,
					  cairo_scaled_font_t        **scaled_font);

static cairo_status_t
_cairo_user_font_face_create_for_toy (cairo_toy_font_face_t   *toy_face,
				      cairo_font_face_t      **font_face)
{
    return _cairo_font_face_twin_create_for_toy (toy_face, font_face);
}

static const cairo_scaled_font_backend_t _cairo_user_scaled_font_backend = {
    CAIRO_FONT_TYPE_USER,
    NULL,	/* scaled_font_fini */
    _cairo_user_scaled_glyph_init,
    _cairo_user_text_to_glyphs,
    _cairo_user_ucs4_to_index,
    NULL,	/* show_glyphs */
    NULL,	/* load_truetype_table */
    NULL	/* index_to_ucs4 */
};

/* #cairo_user_font_face_t */

static cairo_status_t
_cairo_user_font_face_scaled_font_create (void                        *abstract_face,
					  const cairo_matrix_t        *font_matrix,
					  const cairo_matrix_t        *ctm,
					  const cairo_font_options_t  *options,
					  cairo_scaled_font_t        **scaled_font)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_user_font_face_t *font_face = abstract_face;
    cairo_user_scaled_font_t *user_scaled_font = NULL;
    cairo_font_extents_t font_extents = {1., 0., 1., 1., 0.};

    font_face->immutable = TRUE;

    user_scaled_font = _cairo_malloc (sizeof (cairo_user_scaled_font_t));
    if (unlikely (user_scaled_font == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_scaled_font_init (&user_scaled_font->base,
				      &font_face->base,
				      font_matrix, ctm, options,
				      &_cairo_user_scaled_font_backend);

    if (unlikely (status)) {
	free (user_scaled_font);
	return status;
    }

    /* XXX metrics hinting? */

    /* compute a normalized version of font scale matrix to compute
     * extents in.  This is to minimize error caused by the cairo_fixed_t
     * representation. */
    {
	double fixed_scale, x_scale, y_scale;

	user_scaled_font->extent_scale = user_scaled_font->base.scale_inverse;
	status = _cairo_matrix_compute_basis_scale_factors (&user_scaled_font->extent_scale,
						      &x_scale, &y_scale,
						      1);
	if (status == CAIRO_STATUS_SUCCESS) {

	    if (x_scale == 0) x_scale = 1.;
	    if (y_scale == 0) y_scale = 1.;

	    user_scaled_font->snap_x_scale = x_scale;
	    user_scaled_font->snap_y_scale = y_scale;

	    /* since glyphs are pretty much 1.0x1.0, we can reduce error by
	     * scaling to a larger square.  say, 1024.x1024. */
	    fixed_scale = 1024.;
	    x_scale /= fixed_scale;
	    y_scale /= fixed_scale;

	    cairo_matrix_scale (&user_scaled_font->extent_scale, 1. / x_scale, 1. / y_scale);

	    user_scaled_font->extent_x_scale = x_scale;
	    user_scaled_font->extent_y_scale = y_scale;
	}
    }

    if (status == CAIRO_STATUS_SUCCESS &&
	font_face->scaled_font_methods.init != NULL)
    {
	/* Lock the scaled_font mutex such that user doesn't accidentally try
         * to use it just yet. */
	CAIRO_MUTEX_LOCK (user_scaled_font->base.mutex);

	/* Give away fontmap lock such that user-font can use other fonts */
	status = _cairo_scaled_font_register_placeholder_and_unlock_font_map (&user_scaled_font->base);
	if (status == CAIRO_STATUS_SUCCESS) {
	    cairo_surface_t *recording_surface;
	    cairo_t *cr;

	    recording_surface = _cairo_user_scaled_font_create_recording_surface (user_scaled_font);
	    cr = _cairo_user_scaled_font_create_recording_context (user_scaled_font, recording_surface);
	    cairo_surface_destroy (recording_surface);

	    status = font_face->scaled_font_methods.init (&user_scaled_font->base,
							  cr,
							  &font_extents);

	    if (status == CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED)
		status = CAIRO_STATUS_SUCCESS;

	    if (status == CAIRO_STATUS_SUCCESS)
		status = cairo_status (cr);

	    cairo_destroy (cr);

	    _cairo_scaled_font_unregister_placeholder_and_lock_font_map (&user_scaled_font->base);
	}

	CAIRO_MUTEX_UNLOCK (user_scaled_font->base.mutex);
    }

    if (status == CAIRO_STATUS_SUCCESS)
	status = _cairo_scaled_font_set_metrics (&user_scaled_font->base, &font_extents);

    if (status != CAIRO_STATUS_SUCCESS) {
        _cairo_scaled_font_fini (&user_scaled_font->base);
	free (user_scaled_font);
    } else {
        user_scaled_font->default_glyph_extents.x_bearing = 0.;
        user_scaled_font->default_glyph_extents.y_bearing = -font_extents.ascent;
        user_scaled_font->default_glyph_extents.width = 0.;
        user_scaled_font->default_glyph_extents.height = font_extents.ascent + font_extents.descent;
        user_scaled_font->default_glyph_extents.x_advance = font_extents.max_x_advance;
        user_scaled_font->default_glyph_extents.y_advance = 0.;

	*scaled_font = &user_scaled_font->base;
    }

    return status;
}

const cairo_font_face_backend_t _cairo_user_font_face_backend = {
    CAIRO_FONT_TYPE_USER,
    _cairo_user_font_face_create_for_toy,
    _cairo_font_face_destroy,
    _cairo_user_font_face_scaled_font_create
};


cairo_bool_t
_cairo_font_face_is_user (cairo_font_face_t *font_face)
{
    return font_face->backend == &_cairo_user_font_face_backend;
}

/* Implement the public interface */

/**
 * cairo_user_font_face_create:
 *
 * Creates a new user font-face.
 *
 * Use the setter functions to associate callbacks with the returned
 * user font.  The only mandatory callback is render_glyph.
 *
 * After the font-face is created, the user can attach arbitrary data
 * (the actual font data) to it using cairo_font_face_set_user_data()
 * and access it from the user-font callbacks by using
 * cairo_scaled_font_get_font_face() followed by
 * cairo_font_face_get_user_data().
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 *
 * Since: 1.8
 **/
cairo_font_face_t *
cairo_user_font_face_create (void)
{
    cairo_user_font_face_t *font_face;

    font_face = _cairo_malloc (sizeof (cairo_user_font_face_t));
    if (!font_face) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_font_face_t *)&_cairo_font_face_nil;
    }

    _cairo_font_face_init (&font_face->base, &_cairo_user_font_face_backend);

    font_face->immutable = FALSE;
    memset (&font_face->scaled_font_methods, 0, sizeof (font_face->scaled_font_methods));

    return &font_face->base;
}
slim_hidden_def(cairo_user_font_face_create);

/* User-font method setters */


/**
 * cairo_user_font_face_set_init_func:
 * @font_face: A user font face
 * @init_func: The init callback, or %NULL
 *
 * Sets the scaled-font initialization function of a user-font.
 * See #cairo_user_scaled_font_init_func_t for details of how the callback
 * works.
 *
 * The font-face should not be immutable or a %CAIRO_STATUS_USER_FONT_IMMUTABLE
 * error will occur.  A user font-face is immutable as soon as a scaled-font
 * is created from it.
 *
 * Since: 1.8
 **/
void
cairo_user_font_face_set_init_func (cairo_font_face_t                  *font_face,
				    cairo_user_scaled_font_init_func_t  init_func)
{
    cairo_user_font_face_t *user_font_face;

    if (font_face->status)
	return;

    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return;
    }

    user_font_face = (cairo_user_font_face_t *) font_face;
    if (user_font_face->immutable) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_USER_FONT_IMMUTABLE))
	    return;
    }
    user_font_face->scaled_font_methods.init = init_func;
}
slim_hidden_def(cairo_user_font_face_set_init_func);

/**
 * cairo_user_font_face_set_render_glyph_func:
 * @font_face: A user font face
 * @render_glyph_func: The render_glyph callback, or %NULL
 *
 * Sets the glyph rendering function of a user-font.
 * See #cairo_user_scaled_font_render_glyph_func_t for details of how the callback
 * works.
 *
 * The font-face should not be immutable or a %CAIRO_STATUS_USER_FONT_IMMUTABLE
 * error will occur.  A user font-face is immutable as soon as a scaled-font
 * is created from it.
 *
 * The render_glyph callback is the only mandatory callback of a user-font.
 * If the callback is %NULL and a glyph is tried to be rendered using
 * @font_face, a %CAIRO_STATUS_USER_FONT_ERROR will occur.
 *
 * Since: 1.8
 **/
void
cairo_user_font_face_set_render_glyph_func (cairo_font_face_t                          *font_face,
					    cairo_user_scaled_font_render_glyph_func_t  render_glyph_func)
{
    cairo_user_font_face_t *user_font_face;

    if (font_face->status)
	return;

    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return;
    }

    user_font_face = (cairo_user_font_face_t *) font_face;
    if (user_font_face->immutable) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_USER_FONT_IMMUTABLE))
	    return;
    }
    user_font_face->scaled_font_methods.render_glyph = render_glyph_func;
}
slim_hidden_def(cairo_user_font_face_set_render_glyph_func);

/**
 * cairo_user_font_face_set_text_to_glyphs_func:
 * @font_face: A user font face
 * @text_to_glyphs_func: The text_to_glyphs callback, or %NULL
 *
 * Sets th text-to-glyphs conversion function of a user-font.
 * See #cairo_user_scaled_font_text_to_glyphs_func_t for details of how the callback
 * works.
 *
 * The font-face should not be immutable or a %CAIRO_STATUS_USER_FONT_IMMUTABLE
 * error will occur.  A user font-face is immutable as soon as a scaled-font
 * is created from it.
 *
 * Since: 1.8
 **/
void
cairo_user_font_face_set_text_to_glyphs_func (cairo_font_face_t                            *font_face,
					      cairo_user_scaled_font_text_to_glyphs_func_t  text_to_glyphs_func)
{
    cairo_user_font_face_t *user_font_face;

    if (font_face->status)
	return;

    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return;
    }

    user_font_face = (cairo_user_font_face_t *) font_face;
    if (user_font_face->immutable) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_USER_FONT_IMMUTABLE))
	    return;
    }
    user_font_face->scaled_font_methods.text_to_glyphs = text_to_glyphs_func;
}

/**
 * cairo_user_font_face_set_unicode_to_glyph_func:
 * @font_face: A user font face
 * @unicode_to_glyph_func: The unicode_to_glyph callback, or %NULL
 *
 * Sets the unicode-to-glyph conversion function of a user-font.
 * See #cairo_user_scaled_font_unicode_to_glyph_func_t for details of how the callback
 * works.
 *
 * The font-face should not be immutable or a %CAIRO_STATUS_USER_FONT_IMMUTABLE
 * error will occur.  A user font-face is immutable as soon as a scaled-font
 * is created from it.
 *
 * Since: 1.8
 **/
void
cairo_user_font_face_set_unicode_to_glyph_func (cairo_font_face_t                              *font_face,
						cairo_user_scaled_font_unicode_to_glyph_func_t  unicode_to_glyph_func)
{
    cairo_user_font_face_t *user_font_face;
    if (font_face->status)
	return;

    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return;
    }

    user_font_face = (cairo_user_font_face_t *) font_face;
    if (user_font_face->immutable) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_USER_FONT_IMMUTABLE))
	    return;
    }
    user_font_face->scaled_font_methods.unicode_to_glyph = unicode_to_glyph_func;
}
slim_hidden_def(cairo_user_font_face_set_unicode_to_glyph_func);

/* User-font method getters */

/**
 * cairo_user_font_face_get_init_func:
 * @font_face: A user font face
 *
 * Gets the scaled-font initialization function of a user-font.
 *
 * Return value: The init callback of @font_face
 * or %NULL if none set or an error has occurred.
 *
 * Since: 1.8
 **/
cairo_user_scaled_font_init_func_t
cairo_user_font_face_get_init_func (cairo_font_face_t *font_face)
{
    cairo_user_font_face_t *user_font_face;

    if (font_face->status)
	return NULL;

    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return NULL;
    }

    user_font_face = (cairo_user_font_face_t *) font_face;
    return user_font_face->scaled_font_methods.init;
}

/**
 * cairo_user_font_face_get_render_glyph_func:
 * @font_face: A user font face
 *
 * Gets the glyph rendering function of a user-font.
 *
 * Return value: The render_glyph callback of @font_face
 * or %NULL if none set or an error has occurred.
 *
 * Since: 1.8
 **/
cairo_user_scaled_font_render_glyph_func_t
cairo_user_font_face_get_render_glyph_func (cairo_font_face_t *font_face)
{
    cairo_user_font_face_t *user_font_face;

    if (font_face->status)
	return NULL;

    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return NULL;
    }

    user_font_face = (cairo_user_font_face_t *) font_face;
    return user_font_face->scaled_font_methods.render_glyph;
}

/**
 * cairo_user_font_face_get_text_to_glyphs_func:
 * @font_face: A user font face
 *
 * Gets the text-to-glyphs conversion function of a user-font.
 *
 * Return value: The text_to_glyphs callback of @font_face
 * or %NULL if none set or an error occurred.
 *
 * Since: 1.8
 **/
cairo_user_scaled_font_text_to_glyphs_func_t
cairo_user_font_face_get_text_to_glyphs_func (cairo_font_face_t *font_face)
{
    cairo_user_font_face_t *user_font_face;

    if (font_face->status)
	return NULL;

    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return NULL;
    }

    user_font_face = (cairo_user_font_face_t *) font_face;
    return user_font_face->scaled_font_methods.text_to_glyphs;
}

/**
 * cairo_user_font_face_get_unicode_to_glyph_func:
 * @font_face: A user font face
 *
 * Gets the unicode-to-glyph conversion function of a user-font.
 *
 * Return value: The unicode_to_glyph callback of @font_face
 * or %NULL if none set or an error occurred.
 *
 * Since: 1.8
 **/
cairo_user_scaled_font_unicode_to_glyph_func_t
cairo_user_font_face_get_unicode_to_glyph_func (cairo_font_face_t *font_face)
{
    cairo_user_font_face_t *user_font_face;

    if (font_face->status)
	return NULL;

    if (! _cairo_font_face_is_user (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return NULL;
    }

    user_font_face = (cairo_user_font_face_t *) font_face;
    return user_font_face->scaled_font_methods.unicode_to_glyph;
}
