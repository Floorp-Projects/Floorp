/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Library General Public License (the "LGPL"), in
 * which case the provisions of the LGPL are applicable instead of
 * those above.  If you wish to allow use of your version of this file
 * only under the terms of the LGPL and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the LGPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the LGPL.
 */

/*
 * This code is derived from GdkRgb.
 * For more information on GdkRgb, see http://www.levien.com/gdkrgb/
 * Raph Levien <raph@acm.org>
 */

/* Ported by Christopher Blizzard to Xlib.  With permission from the
 * original authors of this file, the contents of this file are also
 * redistributable under the terms of the Mozilla Public license.  For
 * information about the Mozilla Public License, please see the
 * license information at http://www.mozilla.org/MPL/
 */

/* This code is copyright the following authors:
 * Raph Levien          <raph@acm.org>
 * Manish Singh         <manish@gtk.org>
 * Tim Janik            <timj@gtk.org>
 * Peter Mattis         <petm@xcf.berkeley.edu>
 * Spencer Kimball      <spencer@xcf.berkeley.edu>
 * Josh MacDonald       <jmacd@xcf.berkeley.edu>
 * Christopher Blizzard <blizzard@redhat.com>
 * Owen Taylor          <otaylor@redhat.com>
 * Shawn T. Amundson    <amundson@gtk.org>
*/


#ifndef __XLIB_RGB_H__
#define __XLIB_RGB_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>

/* Porting Note:
 * If you are going to use this code somewhere other than mozilla
 * you will need to set these defines.  It's pretty easy for Intel
 * but I'm not sure about other platforms.
 */
#ifdef USE_MOZILLA_TYPES
/* prtypes contains definitions for uint32/int32 and uint16/int16 */
#include "prtypes.h"
#include "prcpucfg.h"
#else
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
#endif

typedef struct _XlibRgbCmap XlibRgbCmap;

struct _XlibRgbCmap {
  unsigned int colors[256];
  unsigned char lut[256]; /* for 8-bit modes */
};

void
xlib_rgb_init (Display *display, Screen *screen);
void
xlib_rgb_init_with_depth (Display *display, Screen *screen, int prefDepth);

unsigned long
xlib_rgb_xpixel_from_rgb (uint32 rgb);

void
xlib_rgb_gc_set_foreground (GC gc, uint32 rgb);

void
xlib_rgb_gc_set_background (GC gc, uint32 rgb);

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
xlib_rgb_cmap_new (uint32 *colors, int n_colors);

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

XVisualInfo *
xlib_rgb_get_visual_info (void);

int
xlib_rgb_get_depth (void);

Display *
xlib_rgb_get_display (void);

Screen *
xlib_rgb_get_screen (void);

unsigned long
xlib_get_prec_from_mask(unsigned long);

unsigned long
xlib_get_shift_from_mask(unsigned long);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __XLIB_RGB_H__ */
