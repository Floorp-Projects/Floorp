/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Owen Taylor <otaylor@redhat.com>
 */

#ifndef CAIRO_DDRAW_PRIVATE_H
#define CAIRO_DDRAW_PRIVATE_H

#include "cairo-ddraw.h"
#include "cairoint.h"

#define FILL_THRESHOLD (1024u)

typedef struct _cairo_ddraw_surface {
  cairo_surface_t base;
  cairo_format_t format;
  LPDIRECTDRAWSURFACE lpdds;
  cairo_surface_t *image;
  cairo_surface_t *alias;
  cairo_point_int_t origin;
  cairo_rectangle_int_t acquirable_rect;
  uint32_t data_offset;
  LPDIRECTDRAWCLIPPER lpddc;
  LPDIRECTDRAWCLIPPER installed_clipper;
  cairo_rectangle_int_t extents;
  cairo_region_t clip_region;
  cairo_bool_t locked : 1;
  cairo_bool_t has_clip_region : 1;
  cairo_bool_t has_image_clip : 1;
  cairo_bool_t has_clip_list : 1;
  cairo_bool_t image_clip_invalid : 1;
  cairo_bool_t clip_list_invalid : 1;
} cairo_ddraw_surface_t;

#endif /* CAIRO_DDRAW_PRIVATE_H */
