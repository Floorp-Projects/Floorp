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
 */

#ifndef CAIRO_XLIB_SURFACE_PRIVATE_H
#define CAIRO_XLIB_SURFACE_PRIVATE_H

#include "cairo-xlib.h"
#include "cairo-xlib-private.h"
#include "cairo-xlib-xrender-private.h"

#include "cairo-surface-private.h"

typedef struct _cairo_xlib_surface cairo_xlib_surface_t;

struct _cairo_xlib_surface {
    cairo_surface_t base;

    cairo_xlib_screen_t *screen;
    cairo_xlib_hook_t close_display_hook;

    Drawable drawable;
    cairo_bool_t owns_pixmap;
    Visual *visual;

    int use_pixmap;

    int render_major;
    int render_minor;

    /* TRUE if the server has a bug with repeating pictures
     *
     *  https://bugs.freedesktop.org/show_bug.cgi?id=3566
     *
     * We can't test for this because it depends on whether the
     * picture is in video memory or not.
     *
     * We also use this variable as a guard against a second
     * independent bug with transformed repeating pictures:
     *
     * http://lists.freedesktop.org/archives/cairo/2004-September/001839.html
     *
     * Both are fixed in xorg >= 6.9 and hopefully in > 6.8.2, so
     * we can reuse the test for now.
     */
    unsigned int buggy_gradients : 1;
    unsigned int buggy_pad_reflect : 1;
    unsigned int buggy_repeat : 1;
#define CAIRO_XLIB_SURFACE_HAS_BUGGY_GRADIENTS 1
#define CAIRO_XLIB_SURFACE_HAS_BUGGY_PAD_REFLECT 1
#define CAIRO_XLIB_SURFACE_HAS_BUGGY_REPEAT 1

    int width;
    int height;
    int depth;

    Picture dst_picture, src_picture;

    unsigned int clip_dirty;
    XRectangle embedded_clip_rects[8];
    XRectangle *clip_rects;
    int num_clip_rects;
    cairo_region_t *clip_region;

    XRenderPictFormat *xrender_format;
    cairo_filter_t filter;
    cairo_extend_t extend;
    cairo_bool_t has_component_alpha;
    int precision;
    XTransform xtransform;

    uint32_t a_mask;
    uint32_t r_mask;
    uint32_t g_mask;
    uint32_t b_mask;
};

enum {
    CAIRO_XLIB_SURFACE_CLIP_DIRTY_GC      = 0x01,
    CAIRO_XLIB_SURFACE_CLIP_DIRTY_PICTURE = 0x02,
    CAIRO_XLIB_SURFACE_CLIP_DIRTY_ALL     = 0x03
};

#endif /* CAIRO_XLIB_SURFACE_PRIVATE_H */
