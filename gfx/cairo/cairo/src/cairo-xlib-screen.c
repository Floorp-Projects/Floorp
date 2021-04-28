/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
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
 * Partially on code from xftdpy.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "cairoint.h"

#if !CAIRO_HAS_XLIB_XCB_FUNCTIONS

#include "cairo-xlib-private.h"
#include "cairo-xlib-xrender-private.h"

#include "cairo-xlib-surface-private.h"
#include "cairo-error-private.h"
#include "cairo-list-inline.h"

#include "cairo-fontconfig-private.h"

static int
parse_boolean (const char *v)
{
    char c0, c1;

    c0 = *v;
    if (c0 == 't' || c0 == 'T' || c0 == 'y' || c0 == 'Y' || c0 == '1')
	return 1;
    if (c0 == 'f' || c0 == 'F' || c0 == 'n' || c0 == 'N' || c0 == '0')
	return 0;
    if (c0 == 'o')
    {
	c1 = v[1];
	if (c1 == 'n' || c1 == 'N')
	    return 1;
	if (c1 == 'f' || c1 == 'F')
	    return 0;
    }

    return -1;
}

static cairo_bool_t
get_boolean_default (Display       *dpy,
		     const char    *option,
		     cairo_bool_t  *value)
{
    char *v;
    int i;

    v = XGetDefault (dpy, "Xft", option);
    if (v) {
	i = parse_boolean (v);
	if (i >= 0) {
	    *value = i;
	    return TRUE;
	}
    }

    return FALSE;
}

static cairo_bool_t
get_integer_default (Display    *dpy,
		     const char *option,
		     int        *value)
{
    char *v, *e;

    v = XGetDefault (dpy, "Xft", option);
    if (v) {
#if CAIRO_HAS_FC_FONT
	if (FcNameConstant ((FcChar8 *) v, value))
	    return TRUE;
#endif

	*value = strtol (v, &e, 0);
	if (e != v)
	    return TRUE;
    }

    return FALSE;
}

static void
_cairo_xlib_init_screen_font_options (Display *dpy,
				      cairo_xlib_screen_t *info)
{
    cairo_bool_t xft_hinting;
    cairo_bool_t xft_antialias;
    int xft_hintstyle;
    int xft_rgba;
    int xft_lcdfilter;
    cairo_antialias_t antialias;
    cairo_subpixel_order_t subpixel_order;
    cairo_lcd_filter_t lcd_filter;
    cairo_hint_style_t hint_style;

    if (!get_boolean_default (dpy, "antialias", &xft_antialias))
	xft_antialias = TRUE;

    if (!get_integer_default (dpy, "lcdfilter", &xft_lcdfilter)) {
	/* -1 is an non-existant Fontconfig constant used to differentiate
	 * the case when no lcdfilter property is available.
	 */
	xft_lcdfilter = -1;
    }

    if (!get_boolean_default (dpy, "hinting", &xft_hinting))
	xft_hinting = TRUE;

    if (!get_integer_default (dpy, "hintstyle", &xft_hintstyle))
	xft_hintstyle = FC_HINT_FULL;

    if (!get_integer_default (dpy, "rgba", &xft_rgba))
    {
        cairo_xlib_display_t *display = (cairo_xlib_display_t *) info->device;

	xft_rgba = FC_RGBA_UNKNOWN;

#if RENDER_MAJOR > 0 || RENDER_MINOR >= 6
        if (display->render_major > 0 || display->render_minor >= 6) {
	    int render_order = XRenderQuerySubpixelOrder (dpy,
							  XScreenNumberOfScreen (info->screen));

	    switch (render_order) {
	    default:
	    case SubPixelUnknown:
		xft_rgba = FC_RGBA_UNKNOWN;
		break;
	    case SubPixelHorizontalRGB:
		xft_rgba = FC_RGBA_RGB;
		break;
	    case SubPixelHorizontalBGR:
		xft_rgba = FC_RGBA_BGR;
		break;
	    case SubPixelVerticalRGB:
		xft_rgba = FC_RGBA_VRGB;
		break;
	    case SubPixelVerticalBGR:
		xft_rgba = FC_RGBA_VBGR;
		break;
	    case SubPixelNone:
		xft_rgba = FC_RGBA_NONE;
		break;
	    }
	}
#endif
    }

    if (xft_hinting) {
	switch (xft_hintstyle) {
	case FC_HINT_NONE:
	    hint_style = CAIRO_HINT_STYLE_NONE;
	    break;
	case FC_HINT_SLIGHT:
	    hint_style = CAIRO_HINT_STYLE_SLIGHT;
	    break;
	case FC_HINT_MEDIUM:
	    hint_style = CAIRO_HINT_STYLE_MEDIUM;
	    break;
	case FC_HINT_FULL:
	    hint_style = CAIRO_HINT_STYLE_FULL;
	    break;
	default:
	    hint_style = CAIRO_HINT_STYLE_DEFAULT;
	}
    } else {
	hint_style = CAIRO_HINT_STYLE_NONE;
    }

    switch (xft_rgba) {
    case FC_RGBA_RGB:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
	break;
    case FC_RGBA_BGR:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
	break;
    case FC_RGBA_VRGB:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
	break;
    case FC_RGBA_VBGR:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
	break;
    case FC_RGBA_UNKNOWN:
    case FC_RGBA_NONE:
    default:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
    }

    switch (xft_lcdfilter) {
    case FC_LCD_NONE:
	lcd_filter = CAIRO_LCD_FILTER_NONE;
	break;
    case FC_LCD_DEFAULT:
	lcd_filter = CAIRO_LCD_FILTER_FIR5;
	break;
    case FC_LCD_LIGHT:
	lcd_filter = CAIRO_LCD_FILTER_FIR3;
	break;
    case FC_LCD_LEGACY:
	lcd_filter = CAIRO_LCD_FILTER_INTRA_PIXEL;
	break;
    default:
	lcd_filter = CAIRO_LCD_FILTER_DEFAULT;
	break;
    }

    if (xft_antialias) {
	if (subpixel_order == CAIRO_SUBPIXEL_ORDER_DEFAULT)
	    antialias = CAIRO_ANTIALIAS_GRAY;
	else
	    antialias = CAIRO_ANTIALIAS_SUBPIXEL;
    } else {
	antialias = CAIRO_ANTIALIAS_NONE;
    }

    cairo_font_options_set_hint_style (&info->font_options, hint_style);
    cairo_font_options_set_antialias (&info->font_options, antialias);
    cairo_font_options_set_subpixel_order (&info->font_options, subpixel_order);
    cairo_font_options_set_lcd_filter (&info->font_options, lcd_filter);
    cairo_font_options_set_hint_metrics (&info->font_options, CAIRO_HINT_METRICS_ON);
}

void
_cairo_xlib_screen_destroy (cairo_xlib_display_t *display,
			    cairo_xlib_screen_t *info)
{
    Display *dpy;
    int i;

    dpy = display->display;

    while (! cairo_list_is_empty (&info->surfaces)) {
	cairo_xlib_surface_t *surface;

	surface = cairo_list_first_entry (&info->surfaces,
					  cairo_xlib_surface_t,
					  link);
	cairo_surface_finish (&surface->base);
    }

    for (i = 0; i < ARRAY_LENGTH (info->gc); i++) {
	if (info->gc_depths[i] != 0) {
	    XFreeGC (dpy, info->gc[i]);
	    info->gc_depths[i] = 0;
	}
    }

    while (! cairo_list_is_empty (&info->visuals)) {
        _cairo_xlib_visual_info_destroy (cairo_list_first_entry (&info->visuals,
                                                                 cairo_xlib_visual_info_t,
                                                                 link));
    }

    cairo_list_del (&info->link);

    free (info);
}

cairo_status_t
_cairo_xlib_screen_get (Display *dpy,
			Screen *screen,
			cairo_xlib_screen_t **out)
{
    cairo_xlib_display_t *display;
    cairo_device_t *device;
    cairo_xlib_screen_t *info;
    cairo_status_t status;

    device = _cairo_xlib_device_create (dpy);
    status = device->status;
    if (unlikely (status))
        goto CLEANUP_DEVICE;

    status =  _cairo_xlib_display_acquire (device, &display);
    if (unlikely (status))
        goto CLEANUP_DEVICE;

    info = _cairo_xlib_display_get_screen (display, screen);
    if (info != NULL) {
	*out = info;
	goto CLEANUP_DISPLAY;
    }

    info = _cairo_malloc (sizeof (cairo_xlib_screen_t));
    if (unlikely (info == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_DISPLAY;
    }

    info->device = device;
    info->screen = screen;
    info->has_font_options = FALSE;
    memset (info->gc_depths, 0, sizeof (info->gc_depths));
    memset (info->gc, 0, sizeof (info->gc));

    cairo_list_init (&info->surfaces);
    cairo_list_init (&info->visuals);
    cairo_list_add (&info->link, &display->screens);

    *out = info;

  CLEANUP_DISPLAY:
    cairo_device_release (&display->base);

  CLEANUP_DEVICE:
    cairo_device_destroy (device);
    return status;
}

GC
_cairo_xlib_screen_get_gc (cairo_xlib_display_t *display,
                           cairo_xlib_screen_t *info,
			   int depth,
			   Drawable drawable)
{
    GC gc = NULL;
    int i;

    for (i = 0; i < ARRAY_LENGTH (info->gc); i++) {
	if (info->gc_depths[i] == depth) {
	    info->gc_depths[i] = 0;
	    gc = info->gc[i];
	    break;
	}
    }

    if (gc == NULL) {
	XGCValues gcv;

	gcv.graphics_exposures = False;
	gcv.fill_style = FillTiled;
	gc = XCreateGC (display->display,
			drawable,
			GCGraphicsExposures | GCFillStyle, &gcv);
    }

    return gc;
}

void
_cairo_xlib_screen_put_gc (cairo_xlib_display_t *display,
                           cairo_xlib_screen_t *info,
			   int depth,
			   GC gc)
{
    int i;

    for (i = 0; i < ARRAY_LENGTH (info->gc); i++) {
	if (info->gc_depths[i] == 0)
	    break;
    }

    if (i == ARRAY_LENGTH (info->gc)) {
	/* perform random substitution to ensure fair caching over depths */
	i = rand () % ARRAY_LENGTH (info->gc);
	XFreeGC(display->display, info->gc[i]);
    }

    info->gc[i] = gc;
    info->gc_depths[i] = depth;
}

cairo_status_t
_cairo_xlib_screen_get_visual_info (cairo_xlib_display_t *display,
                                    cairo_xlib_screen_t *info,
				    Visual *v,
				    cairo_xlib_visual_info_t **out)
{
    cairo_xlib_visual_info_t *visual;
    cairo_status_t status;

    cairo_list_foreach_entry (visual,
                              cairo_xlib_visual_info_t,
                              &info->visuals,
                              link)
    {
	if (visual->visualid == v->visualid) {
            *out = visual;
            return CAIRO_STATUS_SUCCESS;
	}
    }

    status = _cairo_xlib_visual_info_create (display->display,
					     XScreenNumberOfScreen (info->screen),
					     v->visualid,
					     &visual);
    if (unlikely (status))
	return status;

    cairo_list_add (&visual->link, &info->visuals);
    *out = visual;
    return CAIRO_STATUS_SUCCESS;
}

cairo_font_options_t *
_cairo_xlib_screen_get_font_options (cairo_xlib_screen_t *info)
{
    if (! info->has_font_options) {
	_cairo_font_options_init_default (&info->font_options);
	_cairo_font_options_set_round_glyph_positions (&info->font_options, CAIRO_ROUND_GLYPH_POS_ON);

	if (info->screen != NULL) {
            cairo_xlib_display_t *display;

            if (! _cairo_xlib_display_acquire (info->device, &display)) {
                _cairo_xlib_init_screen_font_options (display->display,
                                                      info);
                cairo_device_release (&display->base);
            }
	}

	info->has_font_options = TRUE;
    }

    return &info->font_options;
}

#endif /* !CAIRO_HAS_XLIB_XCB_FUNCTIONS */
