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
 * The Initial Developer of the Original Code is NVIDIA Corporation.
 *
 * Contributor(s):
 */

#ifndef CAIRO_DDRAW_PRIVATE_H
#define CAIRO_DDRAW_PRIVATE_H

#include "cairo-ddraw.h"
#include "cairoint.h"

#ifdef CAIRO_DDRAW_USE_GL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#define CAIRO_DDRAW_FILL_ACCELERATION

#undef CAIRO_DDRAW_CREATE_SIMILAR
#undef CAIRO_DDRAW_CLONE_SIMILAR

#ifdef CAIRO_DDRAW_USE_GL

/* these paths need GL */

#define CAIRO_DDRAW_FONT_ACCELERATION
#define CAIRO_DDRAW_COMPOSITE_ACCELERATION

#endif /* CAIRO_DDRAW_USE_GL */

#ifdef CAIRO_DDRAW_USE_GL
#define CAIRO_DDRAW_FILL_THRESHOLD    32
#else
#define CAIRO_DDRAW_FILL_THRESHOLD    1024
#endif

typedef struct _cairo_ddraw_surface cairo_ddraw_surface_t;

struct _cairo_ddraw_surface {

  /*
   * fields that apply to all surfaces (roots and aliases)
   */

  /* base surface object */
  cairo_surface_t base;

  /* cairo format */
  cairo_format_t format;

  /* pointer to root surface (in root points to itself) */
  cairo_ddraw_surface_t *root;

  /* origin of surface relative to root */
  cairo_point_int_t origin;

  /* image surface alias (may be NULL until created) */
  cairo_surface_t *image;

  /* data offset of image relative to root (0 in root) */
  uint32_t data_offset;

  /* valid image extents.  in aliases, may be clipped by root */
  cairo_rectangle_int_t extents;

  /* current clip region, translated by extents */
  cairo_region_t clip_region;

  /* direct-draw clipper object for surface */
  LPDIRECTDRAWCLIPPER lpddc;

  /*
   * fields that are copied to aliases (not addref'ed)
   */

  /* pointer to direct draw */
  LPDIRECTDRAW lpdd;

  /* pointer to root surface */
  LPDIRECTDRAWSURFACE lpdds;

  /*
   * fields that apply only to the root
   */

  /* currently-installed clipper object (not addref'ed) */
  LPDIRECTDRAWCLIPPER installed_clipper;

#ifdef CAIRO_DDRAW_USE_GL

  /* gl id for texture, renderbuffer and fbo */
  GLuint gl_id;

#endif /* CAIRO_DDRAW_USE_GL */

  /*
   * bitfield flags that apply only to the root
   */

  /* surface is a ddraw surface, and locked */
  cairo_bool_t locked : 1;

#ifdef CAIRO_DDRAW_USE_GL

  /* has been rendered to by GL, needs a glFinish() */
  cairo_bool_t dirty : 1;

#endif /* CAIRO_DDRAW_USE_GL */

  /*
   * bitfield flags that apply to all surfaces
   */
  
  /* we have a non-NULL clip region (in clip_region) */
  cairo_bool_t has_clip_region : 1;

  /* clip region has been set to image surface */
  cairo_bool_t has_image_clip : 1;

  /* image clip doesn't match region */
  cairo_bool_t image_clip_invalid : 1;

  /* clip list doesn't match region */
  cairo_bool_t clip_list_invalid : 1;
};

#endif /* CAIRO_DDRAW_PRIVATE_H */
