/* This code is derived from the code in GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.
 */

/* Ported by Christopher Blizzard to Xlib.  With permission from the
 * original authors of this file, the contents of this file are also
 * redistributable under the terms of the Mozilla Public license.  For
 * information about the Mozilla Public License, please see the
 * license information at http://www.mozilla.org/MPL/ */

#ifndef __XLIB_RGB_H__
#define __XLIB_RGB_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>

typedef struct _XlibRgbCmap XlibRgbCmap;

struct _XlibRgbCmap {
  unsigned int colors[256];
  unsigned char lut[256]; /* for 8-bit modes */
};

void
xlib_rgb_init (Display *display, Screen *screen);

unsigned long
xlib_rgb_xpixel_from_rgb (unsigned int rgb);

void
xlib_rgb_gc_set_foreground (GC gc, unsigned int rgb);

void
xlib_rgb_gc_set_background (GC gc, unsigned int rgb);

typedef enum
{
  XLIB_RGB_DITHER_NONE,
  XLIB_RGB_DITHER_NORMAL,
  XLIB_RGB_DITHER_MAX
} XlibRgbDither;

void
xlib_draw_rgb_image (Drawable drawable,
		     GC gc,
		     int x,
		     int y,
		     int width,
		     int height,
		     XlibRgbDither dith,
		     unsigned char *rgb_buf,
		     int rowstride);

void
xlib_draw_rgb_image_dithalign (Drawable drawable,
			       GC gc,
			       int x,
			       int y,
			       int width,
			       int height,
			       XlibRgbDither dith,
			       unsigned char *rgb_buf,
			       int rowstride,
			       int xdith,
			       int ydith);

void
xlib_draw_rgb_32_image (Drawable drawable,
			GC gc,
			int x,
			int y,
			int width,
			int height,
			XlibRgbDither dith,
			unsigned char *buf,
			int rowstride);

void
xlib_draw_gray_image (Drawable drawable,
		      GC gc,
		      int x,
		      int y,
		      int width,
		      int height,
		      XlibRgbDither dith,
		      unsigned char *buf,
		      int rowstride);

XlibRgbCmap *
xlib_rgb_cmap_new (unsigned int *colors, int n_colors);

void
xlib_rgb_cmap_free (XlibRgbCmap *cmap);

void
xlib_draw_indexed_image (Drawable drawable,
			GC gc,
			int x,
			int y,
			int width,
			int height,
			XlibRgbDither dith,
			unsigned char *buf,
			int rowstride,
			XlibRgbCmap *cmap);


/* Below are some functions which are primarily useful for debugging
   and experimentation. */
Bool
xlib_rgb_ditherable (void);

void
xlib_rgb_set_verbose (Bool verbose);

/* experimental colormap stuff */
void
xlib_rgb_set_install (Bool install);

void
xlib_rgb_set_min_colors (int min_colors);

Colormap
xlib_rgb_get_cmap (void);

Visual *
xlib_rgb_get_visual (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __XLIB_RGB_H__ */
