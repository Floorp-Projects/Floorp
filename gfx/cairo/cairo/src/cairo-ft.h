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
 *	Carl D. Worth <cworth@isi.edu>
 */

#include <cairo.h>

#ifndef CAIRO_FT_H
#define CAIRO_FT_H
#ifdef  CAIRO_HAS_FT_FONT

/* Fontconfig/Freetype platform-specific font interface */

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

cairo_font_t *
cairo_ft_font_create (FT_Library ft_library, FcPattern *pattern);

cairo_font_t *
cairo_ft_font_create_for_ft_face (FT_Face face);

FT_Face
cairo_ft_font_face (cairo_font_t *ft_font);

FcPattern *
cairo_ft_font_pattern (cairo_font_t  *ft_font);

#endif /* CAIRO_HAS_FT_FONT */
#endif /* CAIRO_FT_H */
