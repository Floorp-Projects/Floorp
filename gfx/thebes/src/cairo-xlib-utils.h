/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@novell.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef CAIROXLIBUTILS_H_
#define CAIROXLIBUTILS_H_

#include "cairo.h"
#include <X11/Xlib.h>

CAIRO_BEGIN_DECLS

/**
 * This callback encapsulates Xlib-based rendering. We assume that the
 * execution of the callback is equivalent to compositing some RGBA image of
 * size (bounds_width, bounds_height) onto the drawable at offset (offset_x,
 * offset_y), clipped to the union of the clip_rects if num_rects is greater
 * than zero. This includes the assumption that the same RGBA image
 * is composited if you call the callback multiple times with the same closure,
 * display and visual during a single cairo_draw_with_xlib call.
 * 
 * @return True on success, False on non-recoverable error
 */
typedef cairo_bool_t (* cairo_xlib_drawing_callback)
    (void *closure,
     Display *dpy,
     Drawable drawable,
     Visual *visual,
     short offset_x, short offset_y,
     XRectangle* clip_rects, unsigned int num_rects);

/**
 * This structure captures the result of the native drawing, in case the
 * caller may wish to reapply the drawing efficiently later.
 */
typedef struct {
    cairo_surface_t *surface;
    cairo_bool_t    uniform_alpha;
    cairo_bool_t    uniform_color;
    double          alpha; /* valid only if uniform_alpha is TRUE */
    double          r, g, b; /* valid only if uniform_color is TRUE */
} cairo_xlib_drawing_result_t;

/**
 * This type specifies whether the native drawing callback draws all pixels
 * in its bounds opaquely, independent of the contents of the target drawable,
 * or whether it leaves pixels transparent/translucent or depends on the
 * existing contents of the target drawable in some way.
 */
typedef enum _cairo_xlib_drawing_opacity {
    CAIRO_XLIB_DRAWING_OPAQUE,
    CAIRO_XLIB_DRAWING_TRANSPARENT
} cairo_xlib_drawing_opacity_t;

/**
 * This type encodes the capabilities of the native drawing callback.
 * 
 * If CAIRO_XLIB_DRAWING_SUPPORTS_OFFSET is set, 'offset_x' and 'offset_y'
 * can be nonzero in the call to the callback; otherwise they will be zero.
 * 
 * If CAIRO_XLIB_DRAWING_SUPPORTS_CLIP_RECT is set, then 'num_rects' can be
 * zero or one in the call to the callback. If
 * CAIRO_XLIB_DRAWING_SUPPORTS_CLIP_LIST is set, then 'num_rects' can be
 * anything in the call to the callback. Otherwise 'num_rects' will be zero.
 * Do not set both of these values.
 * 
 * If CAIRO_XLIB_DRAWING_SUPPORTS_ALTERNATE_DISPLAY is set, then 'dpy' can be
 * any display, otherwise it will be the display passed into
 * cairo_draw_with_xlib.
 * 
 * If CAIRO_XLIB_DRAWING_SUPPORTS_NONDEFAULT_VISUAL is set, then 'visual' can be
 * any visual, otherwise it will be equal to
 * DefaultVisual (dpy, DefaultScreen (dpy)).
 */
typedef enum {
    CAIRO_XLIB_DRAWING_SUPPORTS_OFFSET = 0x01,
    CAIRO_XLIB_DRAWING_SUPPORTS_CLIP_RECT = 0x02,
    CAIRO_XLIB_DRAWING_SUPPORTS_CLIP_LIST = 0x04,
    CAIRO_XLIB_DRAWING_SUPPORTS_ALTERNATE_DISPLAY = 0x08,
    CAIRO_XLIB_DRAWING_SUPPORTS_NONDEFAULT_VISUAL = 0x10
} cairo_xlib_drawing_support_t;

/**
 * Draw Xlib output into any cairo context. All cairo transforms and effects
 * are honored, including the current operator. This is equivalent to a
 * cairo_set_source_surface and then cairo_paint.
 * @param cr the context to draw into
 * @param callback the code to perform Xlib rendering
 * @param closure associated data
 * @param dpy an X display to use in case the cairo context has no associated X display
 * @param width the width of the putative image drawn by the callback
 * @param height the height of the putative image drawn by the callback
 * @param is_opaque set to CAIRO_XLIB_DRAWING_IS_OPAQUE to indicate
 * that all alpha values of the putative image will be 1.0; the pixels drawn into
 * the Drawable must not depend on the prior contents of the Drawable
 * in any way
 * @param capabilities the capabilities of the callback as described above.
 * @param result if non-NULL, we *may* fill in the struct with information about
 * the rendered image. 'surface' may be filled in with a surface representing
 * the image, similar to the target of 'cr'. If 'uniform_alpha' is True then
 * every pixel of the image has the same alpha value 'alpha'. If
 * 'uniform_color' is True then every pixel of the image has the same RGB
 * color (r, g, b). If the image has uniform color and alpha (or alpha is zero,
 * in which case the color is always uniform) then we won't bother returning
 * a surface for it.
 */
void cairo_draw_with_xlib (cairo_t *cr,
                           cairo_xlib_drawing_callback callback,
                           void *closure,
                           Display *dpy,
                           unsigned int width, unsigned int height,
                           cairo_xlib_drawing_opacity_t is_opaque,
                           cairo_xlib_drawing_support_t capabilities,
                           cairo_xlib_drawing_result_t *result);

CAIRO_END_DECLS

#endif /*CAIROXLIBUTILS_H_*/
