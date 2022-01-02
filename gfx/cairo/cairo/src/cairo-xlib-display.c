/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Chris Wilson
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
 * The Initial Developer of the Original Code is Chris Wilson.
 *
 * Contributor(s):
 *	Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 */

#include "cairoint.h"

#if !CAIRO_HAS_XLIB_XCB_FUNCTIONS

#include "cairo-xlib-private.h"
#include "cairo-xlib-xrender-private.h"
#include "cairo-freelist-private.h"
#include "cairo-error-private.h"
#include "cairo-list-inline.h"

#include <X11/Xlibint.h>	/* For XESetCloseDisplay */

typedef int (*cairo_xlib_error_func_t) (Display     *display,
					XErrorEvent *event);

static cairo_xlib_display_t *_cairo_xlib_display_list;

static int
_noop_error_handler (Display     *display,
		     XErrorEvent *event)
{
    return False;		/* return value is ignored */
}

static void
_cairo_xlib_display_finish (void *abstract_display)
{
    cairo_xlib_display_t *display = abstract_display;
    Display *dpy = display->display;

    _cairo_xlib_display_fini_shm (display);

    if (! cairo_device_acquire (&display->base)) {
	cairo_xlib_error_func_t old_handler;

	/* protect the notifies from triggering XErrors */
	XSync (dpy, False);
	old_handler = XSetErrorHandler (_noop_error_handler);

	while (! cairo_list_is_empty (&display->fonts)) {
	    _cairo_xlib_font_close (cairo_list_first_entry (&display->fonts,
							    cairo_xlib_font_t,
							    link));
	}

	while (! cairo_list_is_empty (&display->screens)) {
	    _cairo_xlib_screen_destroy (display,
					cairo_list_first_entry (&display->screens,
								cairo_xlib_screen_t,
								link));
	}

	XSync (dpy, False);
	XSetErrorHandler (old_handler);

	cairo_device_release (&display->base);
    }
}

static void
_cairo_xlib_display_destroy (void *abstract_display)
{
    cairo_xlib_display_t *display = abstract_display;

    free (display);
}

static int
_cairo_xlib_close_display (Display *dpy, XExtCodes *codes)
{
    cairo_xlib_display_t *display, **prev, *next;

    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);
    for (display = _cairo_xlib_display_list; display; display = display->next)
	if (display->display == dpy)
	    break;
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);
    if (display == NULL)
	return 0;

    cairo_device_finish (&display->base);

    /*
     * Unhook from the global list
     */
    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);
    prev = &_cairo_xlib_display_list;
    for (display = _cairo_xlib_display_list; display; display = next) {
	next = display->next;
	if (display->display == dpy) {
	    *prev = next;
	    break;
	} else
	    prev = &display->next;
    }
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);

    display->display = NULL; /* catch any later invalid access */
    cairo_device_destroy (&display->base);

    /* Return value in accordance with requirements of
     * XESetCloseDisplay */
    return 0;
}

static const cairo_device_backend_t _cairo_xlib_device_backend = {
    CAIRO_DEVICE_TYPE_XLIB,

    NULL,
    NULL,

    NULL, /* flush */
    _cairo_xlib_display_finish,
    _cairo_xlib_display_destroy,
};

static void _cairo_xlib_display_select_compositor (cairo_xlib_display_t *display)
{
#if 1
    if (display->render_major > 0 || display->render_minor >= 4)
	display->compositor = _cairo_xlib_traps_compositor_get ();
    else if (display->render_major > 0 || display->render_minor >= 0)
	display->compositor = _cairo_xlib_mask_compositor_get ();
    else
	display->compositor = _cairo_xlib_core_compositor_get ();
#else
    display->compositor = _cairo_xlib_fallback_compositor_get ();
#endif
}

/**
 * _cairo_xlib_device_create:
 * @dpy: the display to create the device for
 *
 * Gets the device belonging to @dpy, or creates it if it doesn't exist yet.
 *
 * Returns: the device belonging to @dpy
 **/
cairo_device_t *
_cairo_xlib_device_create (Display *dpy)
{
    cairo_xlib_display_t *display;
    cairo_xlib_display_t **prev;
    cairo_device_t *device;
    XExtCodes *codes;
    const char *env;

    CAIRO_MUTEX_INITIALIZE ();

    /* There is an apparent deadlock between this mutex and the
     * mutex for the display, but it's actually safe. For the
     * app to call XCloseDisplay() while any other thread is
     * inside this function would be an error in the logic
     * app, and the CloseDisplay hook is the only other place we
     * acquire this mutex.
     */
    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);

    for (prev = &_cairo_xlib_display_list; (display = *prev); prev = &(*prev)->next)
    {
	if (display->display == dpy) {
	    /*
	     * MRU the list
	     */
	    if (prev != &_cairo_xlib_display_list) {
		*prev = display->next;
		display->next = _cairo_xlib_display_list;
		_cairo_xlib_display_list = display;
	    }
            device = cairo_device_reference (&display->base);
	    goto UNLOCK;
	}
    }

    display = _cairo_malloc (sizeof (cairo_xlib_display_t));
    if (unlikely (display == NULL)) {
	device = _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);
	goto UNLOCK;
    }

    _cairo_device_init (&display->base, &_cairo_xlib_device_backend);

    display->display = dpy;
    cairo_list_init (&display->screens);
    cairo_list_init (&display->fonts);
    display->closed = FALSE;

    /* Xlib calls out to the extension close_display hooks in LIFO
     * order. So we have to ensure that all extensions that we depend
     * on in our close_display hook are properly initialized before we
     * add our hook. For now, that means Render, so we call into its
     * QueryVersion function to ensure it gets initialized.
     */
    display->render_major = display->render_minor = -1;
    XRenderQueryVersion (dpy, &display->render_major, &display->render_minor);
    env = getenv ("CAIRO_DEBUG");
    if (env != NULL && (env = strstr (env, "xrender-version=")) != NULL) {
	int max_render_major, max_render_minor;

	env += sizeof ("xrender-version=") - 1;
	if (sscanf (env, "%d.%d", &max_render_major, &max_render_minor) != 2)
	    max_render_major = max_render_minor = -1;

	if (max_render_major < display->render_major ||
	    (max_render_major == display->render_major &&
	     max_render_minor < display->render_minor))
	{
	    display->render_major = max_render_major;
	    display->render_minor = max_render_minor;
	}
    }

    _cairo_xlib_display_select_compositor (display);

    display->white = NULL;
    memset (display->alpha, 0, sizeof (display->alpha));
    memset (display->solid, 0, sizeof (display->solid));
    memset (display->solid_cache, 0, sizeof (display->solid_cache));
    memset (display->last_solid_cache, 0, sizeof (display->last_solid_cache));

    memset (display->cached_xrender_formats, 0,
	    sizeof (display->cached_xrender_formats));

    display->force_precision = -1;

    _cairo_xlib_display_init_shm (display);

    /* Prior to Render 0.10, there is no protocol support for gradients and
     * we call function stubs instead, which would silently consume the drawing.
     */
#if RENDER_MAJOR == 0 && RENDER_MINOR < 10
    display->buggy_gradients = TRUE;
#else
    display->buggy_gradients = FALSE;
#endif
    display->buggy_pad_reflect = FALSE;
    display->buggy_repeat = FALSE;

    /* This buggy_repeat condition is very complicated because there
     * are multiple X server code bases (with multiple versioning
     * schemes within a code base), and multiple bugs.
     *
     * The X servers:
     *
     *    1. The Vendor=="XFree86" code base with release numbers such
     *    as 4.7.0 (VendorRelease==40700000).
     *
     *    2. The Vendor=="X.Org" code base (a descendant of the
     *    XFree86 code base). It originally had things like
     *    VendorRelease==60700000 for release 6.7.0 but then changed
     *    its versioning scheme so that, for example,
     *    VendorRelease==10400000 for the 1.4.0 X server within the
     *    X.Org 7.3 release.
     *
     * The bugs:
     *
     *    1. The original bug that led to the buggy_repeat
     *    workaround. This was a bug that Owen Taylor investigated,
     *    understood well, and characterized against various X
     *    servers. Confirmed X servers with this bug include:
     *
     *		"XFree86" <= 40500000
     *		"X.Org" <= 60802000 (only with old numbering >= 60700000)
     *
     *    2. A separate bug resulting in a crash of the X server when
     *    using cairo's extend-reflect test case, (which, surprisingly
     *    enough was not passing RepeatReflect to the X server, but
     *    instead using RepeatNormal in a workaround). Nobody to date
     *    has understood the bug well, but it appears to be gone as of
     *    the X.Org 1.4.0 server. This bug is coincidentally avoided
     *    by using the same buggy_repeat workaround. Confirmed X
     *    servers with this bug include:
     *
     *		"X.org" == 60900000 (old versioning scheme)
     *		"X.org"  < 10400000 (new numbering scheme)
     *
     *    For the old-versioning-scheme X servers we don't know
     *    exactly when second the bug started, but since bug 1 is
     *    present through 6.8.2 and bug 2 is present in 6.9.0 it seems
     *    safest to just blacklist all old-versioning-scheme X servers,
     *    (just using VendorRelease < 70000000), as buggy_repeat=TRUE.
     */
    if (_cairo_xlib_vendor_is_xorg (dpy)) {
	if (VendorRelease (dpy) >= 60700000) {
	    if (VendorRelease (dpy) < 70000000)
		display->buggy_repeat = TRUE;

	    /* We know that gradients simply do not work in early Xorg servers */
	    if (VendorRelease (dpy) < 70200000)
		display->buggy_gradients = TRUE;

	    /* And the extended repeat modes were not fixed until much later */
	    display->buggy_pad_reflect = TRUE;
	} else {
	    if (VendorRelease (dpy) < 10400000)
		display->buggy_repeat = TRUE;

	    /* Too many bugs in the early drivers */
	    if (VendorRelease (dpy) < 10699000)
		display->buggy_pad_reflect = TRUE;
	}
    } else if (strstr (ServerVendor (dpy), "XFree86") != NULL) {
	if (VendorRelease (dpy) <= 40500000)
	    display->buggy_repeat = TRUE;

	display->buggy_gradients = TRUE;
	display->buggy_pad_reflect = TRUE;
    }

    codes = XAddExtension (dpy);
    if (unlikely (codes == NULL)) {
	device = _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);
	free (display);
	goto UNLOCK;
    }

    XESetCloseDisplay (dpy, codes->extension, _cairo_xlib_close_display);
    cairo_device_reference (&display->base); /* add one for the CloseDisplay */

    display->next = _cairo_xlib_display_list;
    _cairo_xlib_display_list = display;

    device = &display->base;

UNLOCK:
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);
    return device;
}

cairo_status_t
_cairo_xlib_display_acquire (cairo_device_t *device, cairo_xlib_display_t **display)
{
    cairo_status_t status;

    status = cairo_device_acquire (device);
    if (status)
        return status;

    *display = (cairo_xlib_display_t *) device;
    return CAIRO_STATUS_SUCCESS;
}

XRenderPictFormat *
_cairo_xlib_display_get_xrender_format_for_pixman(cairo_xlib_display_t *display,
						  pixman_format_code_t format)
{
    Display *dpy = display->display;
    XRenderPictFormat tmpl;
    int mask;

    /* No equivalent in X11 yet. */
    if (format == PIXMAN_rgba_float || format == PIXMAN_rgb_float)
	return NULL;

#define MASK(x) ((1<<(x))-1)

    tmpl.depth = PIXMAN_FORMAT_DEPTH(format);
    mask = PictFormatType | PictFormatDepth;

    switch (PIXMAN_FORMAT_TYPE(format)) {
    case PIXMAN_TYPE_ARGB:
	tmpl.type = PictTypeDirect;

	tmpl.direct.alphaMask = MASK(PIXMAN_FORMAT_A(format));
	if (PIXMAN_FORMAT_A(format))
	    tmpl.direct.alpha = (PIXMAN_FORMAT_R(format) +
				 PIXMAN_FORMAT_G(format) +
				 PIXMAN_FORMAT_B(format));

	tmpl.direct.redMask = MASK(PIXMAN_FORMAT_R(format));
	tmpl.direct.red = (PIXMAN_FORMAT_G(format) +
			   PIXMAN_FORMAT_B(format));

	tmpl.direct.greenMask = MASK(PIXMAN_FORMAT_G(format));
	tmpl.direct.green = PIXMAN_FORMAT_B(format);

	tmpl.direct.blueMask = MASK(PIXMAN_FORMAT_B(format));
	tmpl.direct.blue = 0;

	mask |= PictFormatRed | PictFormatRedMask;
	mask |= PictFormatGreen | PictFormatGreenMask;
	mask |= PictFormatBlue | PictFormatBlueMask;
	mask |= PictFormatAlpha | PictFormatAlphaMask;
	break;

    case PIXMAN_TYPE_ABGR:
	tmpl.type = PictTypeDirect;

	tmpl.direct.alphaMask = MASK(PIXMAN_FORMAT_A(format));
	if (tmpl.direct.alphaMask)
	    tmpl.direct.alpha = (PIXMAN_FORMAT_B(format) +
				 PIXMAN_FORMAT_G(format) +
				 PIXMAN_FORMAT_R(format));

	tmpl.direct.blueMask = MASK(PIXMAN_FORMAT_B(format));
	tmpl.direct.blue = (PIXMAN_FORMAT_G(format) +
			    PIXMAN_FORMAT_R(format));

	tmpl.direct.greenMask = MASK(PIXMAN_FORMAT_G(format));
	tmpl.direct.green = PIXMAN_FORMAT_R(format);

	tmpl.direct.redMask = MASK(PIXMAN_FORMAT_R(format));
	tmpl.direct.red = 0;

	mask |= PictFormatRed | PictFormatRedMask;
	mask |= PictFormatGreen | PictFormatGreenMask;
	mask |= PictFormatBlue | PictFormatBlueMask;
	mask |= PictFormatAlpha | PictFormatAlphaMask;
	break;

    case PIXMAN_TYPE_BGRA:
	tmpl.type = PictTypeDirect;

	tmpl.direct.blueMask = MASK(PIXMAN_FORMAT_B(format));
	tmpl.direct.blue = (PIXMAN_FORMAT_BPP(format) - PIXMAN_FORMAT_B(format));

	tmpl.direct.greenMask = MASK(PIXMAN_FORMAT_G(format));
	tmpl.direct.green = (PIXMAN_FORMAT_BPP(format) - PIXMAN_FORMAT_B(format) -
			     PIXMAN_FORMAT_G(format));

	tmpl.direct.redMask = MASK(PIXMAN_FORMAT_R(format));
	tmpl.direct.red = (PIXMAN_FORMAT_BPP(format) - PIXMAN_FORMAT_B(format) -
			   PIXMAN_FORMAT_G(format) - PIXMAN_FORMAT_R(format));

	tmpl.direct.alphaMask = MASK(PIXMAN_FORMAT_A(format));
	tmpl.direct.alpha = 0;

	mask |= PictFormatRed | PictFormatRedMask;
	mask |= PictFormatGreen | PictFormatGreenMask;
	mask |= PictFormatBlue | PictFormatBlueMask;
	mask |= PictFormatAlpha | PictFormatAlphaMask;
	break;

    case PIXMAN_TYPE_A:
	tmpl.type = PictTypeDirect;

	tmpl.direct.alpha = 0;
	tmpl.direct.alphaMask = MASK(PIXMAN_FORMAT_A(format));

	mask |= PictFormatAlpha | PictFormatAlphaMask;
	break;

    case PIXMAN_TYPE_COLOR:
    case PIXMAN_TYPE_GRAY:
	/* XXX Find matching visual/colormap */
	tmpl.type = PictTypeIndexed;
	//tmpl.colormap = screen->visuals[PIXMAN_FORMAT_VIS(format)].vid;
	//mask |= PictFormatColormap;
	return NULL;
    }
#undef MASK

    /* XXX caching? */
    return XRenderFindFormat(dpy, mask, &tmpl, 0);
}

XRenderPictFormat *
_cairo_xlib_display_get_xrender_format (cairo_xlib_display_t	*display,
	                                cairo_format_t		 format)
{
    XRenderPictFormat *xrender_format;

    xrender_format = display->cached_xrender_formats[format];
    if (xrender_format == NULL) {
	int pict_format = PictStandardNUM;

	switch (format) {
	case CAIRO_FORMAT_A1:
	    pict_format = PictStandardA1; break;
	case CAIRO_FORMAT_A8:
	    pict_format = PictStandardA8; break;
	case CAIRO_FORMAT_RGB24:
	    pict_format = PictStandardRGB24; break;
	case CAIRO_FORMAT_RGB16_565:
	    xrender_format = _cairo_xlib_display_get_xrender_format_for_pixman(display,
									       PIXMAN_r5g6b5);
	    break;
	case CAIRO_FORMAT_RGB30:
	    xrender_format = _cairo_xlib_display_get_xrender_format_for_pixman(display,
									       PIXMAN_x2r10g10b10);
	    break;
	case CAIRO_FORMAT_RGBA128F:
	    xrender_format = _cairo_xlib_display_get_xrender_format_for_pixman(display,
									       PIXMAN_rgba_float);
	    break;
	case CAIRO_FORMAT_RGB96F:
	    xrender_format = _cairo_xlib_display_get_xrender_format_for_pixman(display,
									       PIXMAN_rgb_float);
	    break;
	case CAIRO_FORMAT_INVALID:
	default:
	    ASSERT_NOT_REACHED;
	case CAIRO_FORMAT_ARGB32:
	    pict_format = PictStandardARGB32; break;
	}
	if (pict_format != PictStandardNUM)
	    xrender_format =
		XRenderFindStandardFormat (display->display, pict_format);
	display->cached_xrender_formats[format] = xrender_format;
    }

    return xrender_format;
}

cairo_xlib_screen_t *
_cairo_xlib_display_get_screen (cairo_xlib_display_t *display,
				Screen *screen)
{
    cairo_xlib_screen_t *info;

    cairo_list_foreach_entry (info, cairo_xlib_screen_t, &display->screens, link) {
	if (info->screen == screen) {
            if (display->screens.next != &info->link)
                cairo_list_move (&info->link, &display->screens);
            return info;
        }
    }

    return NULL;
}

cairo_bool_t
_cairo_xlib_display_has_repeat (cairo_device_t *device)
{
    return ! ((cairo_xlib_display_t *) device)->buggy_repeat;
}

cairo_bool_t
_cairo_xlib_display_has_reflect (cairo_device_t *device)
{
    return ! ((cairo_xlib_display_t *) device)->buggy_pad_reflect;
}

cairo_bool_t
_cairo_xlib_display_has_gradients (cairo_device_t *device)
{
    return ! ((cairo_xlib_display_t *) device)->buggy_gradients;
}

/**
 * cairo_xlib_device_debug_cap_xrender_version:
 * @device: a #cairo_device_t for the Xlib backend
 * @major_version: major version to restrict to
 * @minor_version: minor version to restrict to
 *
 * Restricts all future Xlib surfaces for this devices to the specified version
 * of the RENDER extension. This function exists solely for debugging purpose.
 * It lets you find out how cairo would behave with an older version of
 * the RENDER extension.
 *
 * Use the special values -1 and -1 for disabling the RENDER extension.
 *
 * Since: 1.12
 **/
void
cairo_xlib_device_debug_cap_xrender_version (cairo_device_t *device,
					     int major_version,
					     int minor_version)
{
    cairo_xlib_display_t *display = (cairo_xlib_display_t *) device;

    if (device == NULL || device->status)
	return;

    if (device->backend->type != CAIRO_DEVICE_TYPE_XLIB)
	return;

    if (major_version < display->render_major ||
	(major_version == display->render_major &&
	 minor_version < display->render_minor))
    {
	display->render_major = major_version;
	display->render_minor = minor_version;
    }

    _cairo_xlib_display_select_compositor (display);
}

/**
 * cairo_xlib_device_debug_set_precision:
 * @device: a #cairo_device_t for the Xlib backend
 * @precision: the precision to use
 *
 * Render supports two modes of precision when rendering trapezoids. Set
 * the precision to the desired mode.
 *
 * Since: 1.12
 **/
void
cairo_xlib_device_debug_set_precision (cairo_device_t *device,
				       int precision)
{
    if (device == NULL || device->status)
	return;
    if (device->backend->type != CAIRO_DEVICE_TYPE_XLIB) {
	cairo_status_t status;

	status = _cairo_device_set_error (device, CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	(void) status;
	return;
    }

    ((cairo_xlib_display_t *) device)->force_precision = precision;
}

/**
 * cairo_xlib_device_debug_get_precision:
 * @device: a #cairo_device_t for the Xlib backend
 *
 * Get the Xrender precision mode.
 *
 * Returns: the render precision mode
 *
 * Since: 1.12
 **/
int
cairo_xlib_device_debug_get_precision (cairo_device_t *device)
{
    if (device == NULL || device->status)
	return -1;
    if (device->backend->type != CAIRO_DEVICE_TYPE_XLIB) {
	cairo_status_t status;

	status = _cairo_device_set_error (device, CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	(void) status;
	return -1;
    }

    return ((cairo_xlib_display_t *) device)->force_precision;
}

#endif /* !CAIRO_HAS_XLIB_XCB_FUNCTIONS */
