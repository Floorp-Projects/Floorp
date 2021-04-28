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
 * Contributors(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 */

#ifndef CAIRO_XLIB_PRIVATE_H
#define CAIRO_XLIB_PRIVATE_H

#include "cairo-xlib.h"
#include "cairo-xlib-xrender-private.h"

#include "cairo-compiler-private.h"
#include "cairo-device-private.h"
#include "cairo-freelist-type-private.h"
#include "cairo-list-private.h"
#include "cairo-reference-count-private.h"
#include "cairo-types-private.h"

typedef struct _cairo_xlib_display cairo_xlib_display_t;
typedef struct _cairo_xlib_screen cairo_xlib_screen_t;

typedef struct _cairo_xlib_hook cairo_xlib_hook_t;
typedef struct _cairo_xlib_job cairo_xlib_job_t;
typedef void (*cairo_xlib_notify_func) (Display *, void *);
typedef void (*cairo_xlib_notify_resource_func) (Display *, XID);

struct _cairo_xlib_hook {
    cairo_xlib_hook_t *prev, *next; /* private */
    void (*func) (cairo_xlib_display_t *display, void *data);
};

/* size of color cube */
#define CUBE_SIZE 6
/* size of gray ramp */
#define RAMP_SIZE 16

struct _cairo_xlib_display {
    cairo_device_t base;

    cairo_xlib_display_t *next;

    Display *display;
    cairo_list_t screens;

    int render_major;
    int render_minor;
    XRenderPictFormat *cached_xrender_formats[CAIRO_FORMAT_RGB16_565 + 1];

    cairo_xlib_job_t *workqueue;
    cairo_freelist_t wq_freelist;

    cairo_xlib_hook_t *close_display_hooks;
    unsigned int buggy_gradients :1;
    unsigned int buggy_pad_reflect :1;
    unsigned int buggy_repeat :1;
    unsigned int closed :1;
};

typedef struct _cairo_xlib_visual_info {
    cairo_list_t link;
    VisualID visualid;
    struct { uint8_t a, r, g, b; } colors[256];
    uint8_t cube_to_pseudocolor[CUBE_SIZE][CUBE_SIZE][CUBE_SIZE];
    uint8_t field8_to_cube[256];
    int8_t  dither8_to_cube[256];
    uint8_t gray8_to_pseudocolor[256];
} cairo_xlib_visual_info_t;

struct _cairo_xlib_screen {
    cairo_list_t link;

    cairo_device_t *device;
    Screen *screen;

    cairo_bool_t has_font_options;
    cairo_font_options_t font_options;

    GC gc[4];
    cairo_atomic_int_t gc_depths; /* 4 x uint8_t */

    cairo_list_t visuals;
};

cairo_private cairo_device_t *
_cairo_xlib_device_create (Display *display);

cairo_private cairo_xlib_screen_t *
_cairo_xlib_display_get_screen (cairo_xlib_display_t *display,
				Screen *screen);

cairo_private void
_cairo_xlib_add_close_display_hook (cairo_xlib_display_t *display, cairo_xlib_hook_t *hook);

cairo_private void
_cairo_xlib_remove_close_display_hook (cairo_xlib_display_t *display, cairo_xlib_hook_t *hook);

cairo_private cairo_status_t
_cairo_xlib_display_queue_work (cairo_xlib_display_t *display,
	                        cairo_xlib_notify_func notify,
				void *data,
				void (*destroy)(void *));
cairo_private cairo_status_t
_cairo_xlib_display_queue_resource (cairo_xlib_display_t *display,
	                           cairo_xlib_notify_resource_func notify,
				   XID resource);
cairo_private cairo_status_t
_cairo_xlib_display_acquire (cairo_device_t *device,
                             cairo_xlib_display_t **display);

cairo_private void
_cairo_xlib_display_get_xrender_version (cairo_xlib_display_t *display,
					 int *major, int *minor);

cairo_private cairo_bool_t
_cairo_xlib_display_has_repeat (cairo_device_t *device);

cairo_private cairo_bool_t
_cairo_xlib_display_has_reflect (cairo_device_t *device);

cairo_private cairo_bool_t
_cairo_xlib_display_has_gradients (cairo_device_t *device);

cairo_private XRenderPictFormat *
_cairo_xlib_display_get_xrender_format (cairo_xlib_display_t	*display,
	                                cairo_format_t		 format);

cairo_private cairo_status_t
_cairo_xlib_screen_get (Display *dpy,
			Screen *screen,
			cairo_xlib_screen_t **out);

cairo_private void
_cairo_xlib_screen_destroy (cairo_xlib_screen_t *info);

cairo_private void
_cairo_xlib_screen_close_display (cairo_xlib_display_t *display,
                                  cairo_xlib_screen_t *info);

cairo_private GC
_cairo_xlib_screen_get_gc (cairo_xlib_display_t *display,
                           cairo_xlib_screen_t *info,
			   int depth,
			   Drawable drawable);

cairo_private void
_cairo_xlib_screen_put_gc (cairo_xlib_display_t *display,
                           cairo_xlib_screen_t *info,
			   int depth,
			   GC gc);

cairo_private cairo_font_options_t *
_cairo_xlib_screen_get_font_options (cairo_xlib_screen_t *info);

cairo_private cairo_status_t
_cairo_xlib_screen_get_visual_info (cairo_xlib_display_t *display,
                                    cairo_xlib_screen_t *info,
				    Visual *visual,
				    cairo_xlib_visual_info_t **out);

cairo_private cairo_status_t
_cairo_xlib_visual_info_create (Display *dpy,
	                        int screen,
				VisualID visualid,
				cairo_xlib_visual_info_t **out);

cairo_private void
_cairo_xlib_visual_info_destroy (cairo_xlib_visual_info_t *info);

#endif /* CAIRO_XLIB_PRIVATE_H */
