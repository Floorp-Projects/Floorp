/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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

#ifndef CAIRO_FEATURES_H
#define CAIRO_FEATURES_H

#include "cairo-platform.h"

#ifdef  __cplusplus
# define CAIRO_BEGIN_DECLS  extern "C" {
# define CAIRO_END_DECLS    }
#else
# define CAIRO_BEGIN_DECLS
# define CAIRO_END_DECLS
#endif

#ifndef cairo_public
# define cairo_public
#endif

#ifdef MOZ_PDF_PRINTING
#define CAIRO_HAS_PDF_SURFACE 1
#endif

#if defined(MOZ_X11) || defined(MOZ_WAYLAND)
#define CAIRO_HAS_PS_SURFACE 1
#endif
#ifdef MOZ_X11
#define CAIRO_HAS_XLIB_XRENDER_SURFACE 0
#define CAIRO_HAS_XLIB_SURFACE 1
#endif

#if defined(MOZ_WIDGET_COCOA) || defined(MOZ_WIDGET_UIKIT)
#define CAIRO_HAS_QUARTZ_SURFACE 1
#define CAIRO_HAS_QUARTZ_IMAGE_SURFACE 1
#define CAIRO_HAS_QUARTZ_FONT 1
#define CAIRO_HAS_QUARTZ_CORE_GRAPHICS 1
#endif

#if defined(MOZ_WIDGET_COCOA)
#define CAIRO_HAS_QUARTZ_ATSUFONTID 1
#define CAIRO_HAS_QUARTZ_APPLICATION_SERVICES 1
#endif

#ifdef XP_WIN
#define CAIRO_HAS_DWRITE_FONT 1
#define CAIRO_HAS_WIN32_FONT 1
#define CAIRO_HAS_WIN32_SURFACE 1
#endif

#if (defined(MOZ_TREE_FREETYPE) && !defined(XP_WIN)) || defined(MOZ_HAVE_FREETYPE2)
#define CAIRO_HAS_FT_FONT 1
#endif

#define CAIRO_HAS_TEE_SURFACE 1

#ifdef USE_FC_FREETYPE
#define CAIRO_HAS_FC_FONT 1
#endif

#endif
