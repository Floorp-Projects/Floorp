/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
 */

#ifndef CAIRO_XLIB_H
#define CAIRO_XLIB_H

#include <cairo.h>

#ifdef  CAIRO_HAS_XLIB_SURFACE

#include <X11/extensions/Xrender.h>

CAIRO_BEGIN_DECLS

/* XXX: This shold be renamed to cairo_set_target_xlib to match the
 * other backends */
void
cairo_set_target_drawable (cairo_t	*cr,
			   Display	*dpy,
			   Drawable	drawable);

/* XXX: This is a mess from the user's POV. Should the Visual or the
   cairo_format_t control what render format is used? Maybe I can have
   cairo_surface_create_for_window with a visual, and
   cairo_surface_create_for_pixmap with a cairo_format_t. Would that work?
*/
cairo_surface_t *
cairo_xlib_surface_create (Display		*dpy,
			   Drawable		drawable,
			   Visual		*visual,
			   cairo_format_t	format,
			   Colormap		colormap);
  
/* XXX: This has been proposed
cairo_status_t
cairo_xlib_surface_set_size (cairo_surface_t *surface, int width, int height);
*/

CAIRO_END_DECLS

#endif /* CAIRO_HAS_XLIB_SURFACE */
#endif /* CAIRO_XLIB_H */

