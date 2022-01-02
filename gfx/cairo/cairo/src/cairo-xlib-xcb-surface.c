/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2009 Intel Corporation
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#if CAIRO_HAS_XLIB_XCB_FUNCTIONS

#include "cairo-xlib.h"
#include "cairo-xcb.h"

#include "cairo-xcb-private.h"
#include "cairo-xlib-xrender-private.h"

#include "cairo-default-context-private.h"
#include "cairo-list-inline.h"
#include "cairo-image-surface-private.h"
#include "cairo-surface-backend-private.h"

#include <X11/Xlib-xcb.h>
#include <X11/Xlibint.h>	/* For XESetCloseDisplay */

struct cairo_xlib_xcb_display_t {
    cairo_device_t  base;

    Display        *dpy;
    cairo_device_t *xcb_device;
    XExtCodes      *codes;

    cairo_list_t    link;
};
typedef struct cairo_xlib_xcb_display_t cairo_xlib_xcb_display_t;

/* List of all #cairo_xlib_xcb_display_t alive,
 * protected by _cairo_xlib_display_mutex */
static cairo_list_t displays;

static cairo_surface_t *
_cairo_xlib_xcb_surface_create (void *dpy,
				void *scr,
				void *visual,
				void *format,
				cairo_surface_t *xcb);

static cairo_surface_t *
_cairo_xlib_xcb_surface_create_similar (void			*abstract_other,
					cairo_content_t		 content,
					int			 width,
					int			 height)
{
    cairo_xlib_xcb_surface_t *other = abstract_other;
    cairo_surface_t *xcb;

    xcb = other->xcb->base.backend->create_similar (other->xcb, content, width, height);
    if (unlikely (xcb == NULL || xcb->status))
	return xcb;

    return _cairo_xlib_xcb_surface_create (other->display, other->screen, NULL, NULL, xcb);
}

static cairo_status_t
_cairo_xlib_xcb_surface_finish (void *abstract_surface)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    cairo_status_t status;

    cairo_surface_finish (&surface->xcb->base);
    status = surface->xcb->base.status;
    cairo_surface_destroy (&surface->xcb->base);
    surface->xcb = NULL;

    return status;
}

static cairo_surface_t *
_cairo_xlib_xcb_surface_create_similar_image (void			*abstract_other,
					      cairo_format_t		 format,
					      int			 width,
					      int			 height)
{
    cairo_xlib_xcb_surface_t *surface = abstract_other;
    return cairo_surface_create_similar_image (&surface->xcb->base, format, width, height);
}

static cairo_image_surface_t *
_cairo_xlib_xcb_surface_map_to_image (void *abstract_surface,
				      const cairo_rectangle_int_t *extents)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_map_to_image (&surface->xcb->base, extents);
}

static cairo_int_status_t
_cairo_xlib_xcb_surface_unmap (void *abstract_surface,
			       cairo_image_surface_t *image)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_unmap_image (&surface->xcb->base, image);
}

static cairo_surface_t *
_cairo_xlib_xcb_surface_source (void *abstract_surface,
				cairo_rectangle_int_t *extents)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_get_source (&surface->xcb->base, extents);
}

static cairo_status_t
_cairo_xlib_xcb_surface_acquire_source_image (void *abstract_surface,
					      cairo_image_surface_t **image_out,
					      void **image_extra)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_acquire_source_image (&surface->xcb->base,
						image_out, image_extra);
}

static void
_cairo_xlib_xcb_surface_release_source_image (void *abstract_surface,
					      cairo_image_surface_t *image_out,
					      void *image_extra)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    _cairo_surface_release_source_image (&surface->xcb->base, image_out, image_extra);
}

static cairo_bool_t
_cairo_xlib_xcb_surface_get_extents (void *abstract_surface,
				     cairo_rectangle_int_t *extents)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_get_extents (&surface->xcb->base, extents);
}

static void
_cairo_xlib_xcb_surface_get_font_options (void *abstract_surface,
					  cairo_font_options_t *options)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    cairo_surface_get_font_options (&surface->xcb->base, options);
}

static cairo_int_status_t
_cairo_xlib_xcb_surface_paint (void			*abstract_surface,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       const cairo_clip_t	*clip)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_paint (&surface->xcb->base, op, source, clip);
}

static cairo_int_status_t
_cairo_xlib_xcb_surface_mask (void			*abstract_surface,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      const cairo_pattern_t	*mask,
			      const cairo_clip_t	*clip)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_mask (&surface->xcb->base, op, source, mask, clip);
}

static cairo_int_status_t
_cairo_xlib_xcb_surface_stroke (void				*abstract_surface,
				cairo_operator_t		 op,
				const cairo_pattern_t		*source,
				const cairo_path_fixed_t	*path,
				const cairo_stroke_style_t	*style,
				const cairo_matrix_t		*ctm,
				const cairo_matrix_t		*ctm_inverse,
				double				 tolerance,
				cairo_antialias_t		 antialias,
				const cairo_clip_t		*clip)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_stroke (&surface->xcb->base,
				  op, source, path, style, ctm, ctm_inverse,
				  tolerance, antialias, clip);
}

static cairo_int_status_t
_cairo_xlib_xcb_surface_fill (void			*abstract_surface,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      const cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		 antialias,
			      const cairo_clip_t	*clip)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_fill (&surface->xcb->base,
				op, source, path,
				fill_rule, tolerance,
				antialias, clip);
}

static cairo_int_status_t
_cairo_xlib_xcb_surface_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				const cairo_pattern_t	*source,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font,
				const cairo_clip_t	*clip)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    return _cairo_surface_show_text_glyphs (&surface->xcb->base, op, source,
					    NULL, 0,
					    glyphs, num_glyphs,
					    NULL, 0, 0,
					    scaled_font, clip);
}

static cairo_status_t
_cairo_xlib_xcb_surface_flush (void *abstract_surface, unsigned flags)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    /* We have to call cairo_surface_flush() to make sure snapshots are detached */
    return _cairo_surface_flush (&surface->xcb->base, flags);
}

static cairo_status_t
_cairo_xlib_xcb_surface_mark_dirty (void *abstract_surface,
				    int x, int y,
				    int width, int height)
{
    cairo_xlib_xcb_surface_t *surface = abstract_surface;
    cairo_surface_mark_dirty_rectangle (&surface->xcb->base, x, y, width, height);
    return cairo_surface_status (&surface->xcb->base);
}

static const cairo_surface_backend_t _cairo_xlib_xcb_surface_backend = {
    CAIRO_SURFACE_TYPE_XLIB,
    _cairo_xlib_xcb_surface_finish,

    _cairo_default_context_create, /* XXX */

    _cairo_xlib_xcb_surface_create_similar,
    _cairo_xlib_xcb_surface_create_similar_image,
    _cairo_xlib_xcb_surface_map_to_image,
    _cairo_xlib_xcb_surface_unmap,

    _cairo_xlib_xcb_surface_source,
    _cairo_xlib_xcb_surface_acquire_source_image,
    _cairo_xlib_xcb_surface_release_source_image,
    NULL, /* snapshot */

    NULL, /* copy_page */
    NULL, /* show_page */

    _cairo_xlib_xcb_surface_get_extents,
    _cairo_xlib_xcb_surface_get_font_options,

    _cairo_xlib_xcb_surface_flush,
    _cairo_xlib_xcb_surface_mark_dirty,

    _cairo_xlib_xcb_surface_paint,
    _cairo_xlib_xcb_surface_mask,
    _cairo_xlib_xcb_surface_stroke,
    _cairo_xlib_xcb_surface_fill,
    NULL, /* fill_stroke */
    _cairo_xlib_xcb_surface_glyphs,
};

static void
_cairo_xlib_xcb_display_finish (void *abstract_display)
{
    cairo_xlib_xcb_display_t *display = (cairo_xlib_xcb_display_t *) abstract_display;

    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);
    cairo_list_del (&display->link);
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);

    cairo_device_destroy (display->xcb_device);
    display->xcb_device = NULL;

    XESetCloseDisplay (display->dpy, display->codes->extension, NULL);
    /* Drop the reference from _cairo_xlib_xcb_device_create */
    cairo_device_destroy (&display->base);
}

static int
_cairo_xlib_xcb_close_display(Display *dpy, XExtCodes *codes)
{
    cairo_xlib_xcb_display_t *display;

    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);
    cairo_list_foreach_entry (display,
			      cairo_xlib_xcb_display_t,
			      &displays,
			      link)
    {
	if (display->dpy == dpy)
	{
	    /* _cairo_xlib_xcb_display_finish will lock the mutex again
	     * -> deadlock (This mutex isn't recursive) */
	    cairo_device_reference (&display->base);
	    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);

	    /* Make sure the xcb and xlib-xcb devices are finished */
	    cairo_device_finish (display->xcb_device);
	    cairo_device_finish (&display->base);

	    cairo_device_destroy (&display->base);
	    return 0;
	}
    }
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);

    return 0;
}

static const cairo_device_backend_t _cairo_xlib_xcb_device_backend = {
    CAIRO_DEVICE_TYPE_XLIB,

    NULL,
    NULL,

    NULL, /* flush */
    _cairo_xlib_xcb_display_finish,
    free, /* destroy */
};

static cairo_device_t *
_cairo_xlib_xcb_device_create (Display *dpy, cairo_device_t *xcb_device)
{
    cairo_xlib_xcb_display_t *display = NULL;
    cairo_device_t *device;

    if (xcb_device == NULL)
	return NULL;

    CAIRO_MUTEX_INITIALIZE ();

    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);
    if (displays.next == NULL) {
	cairo_list_init (&displays);
    }

    cairo_list_foreach_entry (display,
			      cairo_xlib_xcb_display_t,
			      &displays,
			      link)
    {
	if (display->dpy == dpy) {
	    /* Maintain MRU order. */
	    if (displays.next != &display->link)
		cairo_list_move (&display->link, &displays);

	    /* Grab a reference for our caller */
	    device = cairo_device_reference (&display->base);
	    assert (display->xcb_device == xcb_device);
	    goto unlock;
	}
    }

    display = _cairo_malloc (sizeof (cairo_xlib_xcb_display_t));
    if (unlikely (display == NULL)) {
	device = _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);
	goto unlock;
    }

    display->codes = XAddExtension (dpy);
    if (unlikely (display->codes == NULL)) {
	device = _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);
	free (display);
	goto unlock;
    }

    _cairo_device_init (&display->base, &_cairo_xlib_xcb_device_backend);

    XESetCloseDisplay (dpy, display->codes->extension, _cairo_xlib_xcb_close_display);
    /* Add a reference for _cairo_xlib_xcb_display_finish. This basically means
     * that the device's reference count never drops to zero
     * as long as our Display* is alive. We need this because there is no
     * "XDelExtension" to undo XAddExtension and having lots of registered
     * extensions slows down libX11. */
    cairo_device_reference (&display->base);

    display->dpy = dpy;
    display->xcb_device = cairo_device_reference(xcb_device);

    cairo_list_add (&display->link, &displays);
    device = &display->base;

unlock:
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);

    return device;
}

static cairo_surface_t *
_cairo_xlib_xcb_surface_create (void *dpy,
				void *scr,
				void *visual,
				void *format,
				cairo_surface_t *xcb)
{
    cairo_xlib_xcb_surface_t *surface;

    if (unlikely (xcb->status))
	return xcb;

    surface = _cairo_malloc (sizeof (*surface));
    if (unlikely (surface == NULL)) {
	cairo_surface_destroy (xcb);
	return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_surface_init (&surface->base,
			 &_cairo_xlib_xcb_surface_backend,
			 _cairo_xlib_xcb_device_create (dpy, xcb->device),
			 xcb->content,
			 FALSE); /* is_vector */

    /* _cairo_surface_init() got another reference to the device, drop ours */
    cairo_device_destroy (surface->base.device);

    surface->display = dpy;
    surface->screen = scr;
    surface->visual = visual;
    surface->format = format;
    surface->xcb = (cairo_xcb_surface_t *) xcb;

    return &surface->base;
}

static Screen *
_cairo_xlib_screen_from_visual (Display *dpy, Visual *visual)
{
    int s, d, v;

    for (s = 0; s < ScreenCount (dpy); s++) {
	Screen *screen;

	screen = ScreenOfDisplay (dpy, s);
	if (visual == DefaultVisualOfScreen (screen))
	    return screen;

	for (d = 0; d < screen->ndepths; d++) {
	    Depth  *depth;

	    depth = &screen->depths[d];
	    for (v = 0; v < depth->nvisuals; v++)
		if (visual == &depth->visuals[v])
		    return screen;
	}
    }

    return NULL;
}

cairo_surface_t *
cairo_xlib_surface_create (Display     *dpy,
			   Drawable	drawable,
			   Visual      *visual,
			   int		width,
			   int		height)
{
    Screen *scr;
    xcb_visualtype_t xcb_visual;

    scr = _cairo_xlib_screen_from_visual (dpy, visual);
    if (scr == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_VISUAL));

    xcb_visual.visual_id = visual->visualid;
#if defined(__cplusplus) || defined(c_plusplus)
    xcb_visual._class = visual->c_class;
#else
    xcb_visual._class = visual->class;
#endif
    xcb_visual.bits_per_rgb_value = visual->bits_per_rgb;
    xcb_visual.colormap_entries = visual->map_entries;
    xcb_visual.red_mask = visual->red_mask;
    xcb_visual.green_mask = visual->green_mask;
    xcb_visual.blue_mask = visual->blue_mask;

    return _cairo_xlib_xcb_surface_create (dpy, scr, visual, NULL,
					   cairo_xcb_surface_create (XGetXCBConnection (dpy),
								     drawable,
								     &xcb_visual,
								     width, height));
}

static xcb_screen_t *
_cairo_xcb_screen_from_root (xcb_connection_t *connection,
			     xcb_window_t id)
{
    xcb_screen_iterator_t s;

    s = xcb_setup_roots_iterator (xcb_get_setup (connection));
    for (; s.rem; xcb_screen_next (&s)) {
	if (s.data->root == id)
	    return s.data;
    }

    return NULL;
}

cairo_surface_t *
cairo_xlib_surface_create_for_bitmap (Display  *dpy,
				      Pixmap	bitmap,
				      Screen   *scr,
				      int	width,
				      int	height)
{
    xcb_connection_t *connection = XGetXCBConnection (dpy);
    xcb_screen_t *screen = _cairo_xcb_screen_from_root (connection, (xcb_window_t) scr->root);
    return _cairo_xlib_xcb_surface_create (dpy, scr, NULL, NULL,
					   cairo_xcb_surface_create_for_bitmap (connection,
										screen,
										bitmap,
										width, height));
}

#if CAIRO_HAS_XLIB_XRENDER_SURFACE
cairo_surface_t *
cairo_xlib_surface_create_with_xrender_format (Display		    *dpy,
					       Drawable		    drawable,
					       Screen		    *scr,
					       XRenderPictFormat    *format,
					       int		    width,
					       int		    height)
{
    xcb_render_pictforminfo_t xcb_format;
    xcb_connection_t *connection;
    xcb_screen_t *screen;

    xcb_format.id = format->id;
    xcb_format.type = format->type;
    xcb_format.depth = format->depth;
    xcb_format.direct.red_shift = format->direct.red;
    xcb_format.direct.red_mask = format->direct.redMask;
    xcb_format.direct.green_shift = format->direct.green;
    xcb_format.direct.green_mask = format->direct.greenMask;
    xcb_format.direct.blue_shift = format->direct.blue;
    xcb_format.direct.blue_mask = format->direct.blueMask;
    xcb_format.direct.alpha_shift = format->direct.alpha;
    xcb_format.direct.alpha_mask = format->direct.alphaMask;
    xcb_format.colormap = format->colormap;

    connection = XGetXCBConnection (dpy);
    screen = _cairo_xcb_screen_from_root (connection, (xcb_window_t) scr->root);

    return _cairo_xlib_xcb_surface_create (dpy, scr, NULL, format,
					   cairo_xcb_surface_create_with_xrender_format (connection, screen,
											 drawable,
											 &xcb_format,
											 width, height));
}

XRenderPictFormat *
cairo_xlib_surface_get_xrender_format (cairo_surface_t *surface)
{
    cairo_xlib_xcb_surface_t *xlib_surface = (cairo_xlib_xcb_surface_t *) surface;

    /* Throw an error for a non-xlib surface */
    if (surface->type != CAIRO_SURFACE_TYPE_XLIB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return xlib_surface->format;
}
#endif

void
cairo_xlib_surface_set_size (cairo_surface_t *abstract_surface,
			     int              width,
			     int              height)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *) abstract_surface;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return;
    if (unlikely (abstract_surface->finished)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
	return;
    }

    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    cairo_xcb_surface_set_size (&surface->xcb->base, width, height);
    if (unlikely (surface->xcb->base.status)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (surface->xcb->base.status));
    }
}

void
cairo_xlib_surface_set_drawable (cairo_surface_t   *abstract_surface,
				 Drawable	    drawable,
				 int		    width,
				 int		    height)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *)abstract_surface;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return;
    if (unlikely (abstract_surface->finished)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
	return;
    }

    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    cairo_xcb_surface_set_drawable (&surface->xcb->base, drawable, width, height);
    if (unlikely (surface->xcb->base.status)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (surface->xcb->base.status));
    }
}

Display *
cairo_xlib_surface_get_display (cairo_surface_t *abstract_surface)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *) abstract_surface;

    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return surface->display;
}

Drawable
cairo_xlib_surface_get_drawable (cairo_surface_t *abstract_surface)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *) abstract_surface;

    if (unlikely (abstract_surface->finished)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_FINISHED);
	return 0;
    }
    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }
    /* This can happen when e.g. create_similar falls back to an image surface
     * because we don't have the RENDER extension. */
    if (surface->xcb->base.type != CAIRO_SURFACE_TYPE_XCB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->xcb->drawable;
}

Screen *
cairo_xlib_surface_get_screen (cairo_surface_t *abstract_surface)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *) abstract_surface;

    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return surface->screen;
}

Visual *
cairo_xlib_surface_get_visual (cairo_surface_t *abstract_surface)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *) abstract_surface;

    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return surface->visual;
}

int
cairo_xlib_surface_get_depth (cairo_surface_t *abstract_surface)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *) abstract_surface;

    if (unlikely (abstract_surface->finished)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_FINISHED);
	return 0;
    }
    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }
    /* This can happen when e.g. create_similar falls back to an image surface
     * because we don't have the RENDER extension. */
    if (surface->xcb->base.type != CAIRO_SURFACE_TYPE_XCB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->xcb->depth;
}

int
cairo_xlib_surface_get_width (cairo_surface_t *abstract_surface)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *) abstract_surface;

    if (unlikely (abstract_surface->finished)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_FINISHED);
	return 0;
    }
    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }
    /* This can happen when e.g. create_similar falls back to an image surface
     * because we don't have the RENDER extension. */
    if (surface->xcb->base.type != CAIRO_SURFACE_TYPE_XCB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->xcb->width;
}

int
cairo_xlib_surface_get_height (cairo_surface_t *abstract_surface)
{
    cairo_xlib_xcb_surface_t *surface = (cairo_xlib_xcb_surface_t *) abstract_surface;

    if (unlikely (abstract_surface->finished)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_FINISHED);
	return 0;
    }
    if (surface->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }
    /* This can happen when e.g. create_similar falls back to an image surface
     * because we don't have the RENDER extension. */
    if (surface->xcb->base.type != CAIRO_SURFACE_TYPE_XCB) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->xcb->height;
}

void
cairo_xlib_device_debug_cap_xrender_version (cairo_device_t *device,
					     int major, int minor)
{
    cairo_xlib_xcb_display_t *display = (cairo_xlib_xcb_display_t *) device;

    if (device == NULL || device->status)
	return;

    if (device->backend->type != CAIRO_DEVICE_TYPE_XLIB)
	return;

    cairo_xcb_device_debug_cap_xrender_version (display->xcb_device,
						major, minor);
}

void
cairo_xlib_device_debug_set_precision (cairo_device_t *device,
				       int precision)
{
    cairo_xlib_xcb_display_t *display = (cairo_xlib_xcb_display_t *) device;

    if (device == NULL || device->status)
	return;
    if (device->backend->type != CAIRO_DEVICE_TYPE_XLIB) {
	cairo_status_t status;

	status = _cairo_device_set_error (device, CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	(void) status;
	return;
    }

    cairo_xcb_device_debug_set_precision (display->xcb_device, precision);
}

int
cairo_xlib_device_debug_get_precision (cairo_device_t *device)
{
    cairo_xlib_xcb_display_t *display = (cairo_xlib_xcb_display_t *) device;

    if (device == NULL || device->status)
	return -1;
    if (device->backend->type != CAIRO_DEVICE_TYPE_XLIB) {
	cairo_status_t status;

	status = _cairo_device_set_error (device, CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	(void) status;
	return -1;
    }

    return cairo_xcb_device_debug_get_precision (display->xcb_device);
}

#endif /* CAIRO_HAS_XLIB_XCB_FUNCTIONS */
