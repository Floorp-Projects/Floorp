/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * Roland Mainz         <roland.mainz@informatik.med.uni-giessen.de> 
 */


#ifndef __XLIB_RGB_H__
#define __XLIB_RGB_H__

/* Force ANSI C prototypes from X11 headers */
#undef FUNCPROTO
#define FUNCPROTO 15

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>

_XFUNCPROTOBEGIN

/* Porting Note:
 * If you are going to use this code somewhere other than mozilla
 * you will need to set these defines.  It's pretty easy for Intel
 * but I'm not sure about other platforms.
 */
#ifdef USE_MOZILLA_TYPES
/* prtypes contains definitions for uint32/int32 and uint16/int16 */
#include "prtypes.h"
#include "prcpucfg.h"

#define NS_TO_XXLIB_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)
#else
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
#endif /* USE_MOZILLA_TYPES */

typedef struct _XlibRgbCmap XlibRgbCmap;
typedef struct _XlibRgbHandle XlibRgbHandle;

struct _XlibRgbCmap {
  unsigned int colors[256];
  unsigned char lut[256]; /* for 8-bit modes */
};


typedef enum
{
  XLIB_RGB_DITHER_NONE,
  XLIB_RGB_DITHER_NORMAL,
  XLIB_RGB_DITHER_MAX
} XlibRgbDither;

typedef struct
{
  const char *handle_name;
  int         pseudogray; /* emulate GrayScale via PseudoColor visuals */
  int         install_colormap;
  int         disallow_image_tiling;
  int         disallow_mit_shmem;
  int         verbose;
  XVisualInfo xtemplate;
  long        xtemplate_mask;
} XlibRgbArgs;

XlibRgbHandle *
xxlib_rgb_create_handle (Display *display, Screen *screen, 
                         XlibRgbArgs *args);
                                  
void
xxlib_rgb_destroy_handle (XlibRgbHandle *handle);

unsigned long
xxlib_rgb_xpixel_from_rgb (XlibRgbHandle *handle, uint32 rgb);

void
xxlib_rgb_gc_set_foreground (XlibRgbHandle *handle, GC gc, uint32 rgb);

void
xxlib_rgb_gc_set_background (XlibRgbHandle *handle, GC gc, uint32 rgb);

void
xxlib_draw_rgb_image (XlibRgbHandle *handle, Drawable drawable,
                      GC gc,
                      int x,
                      int y,
                      int width,
                      int height,
                      XlibRgbDither dith,
                      unsigned char *rgb_buf,
                      int rowstride);

void
xxlib_draw_rgb_image_dithalign (XlibRgbHandle *handle, Drawable drawable,
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
xxlib_draw_rgb_32_image (XlibRgbHandle *handle, Drawable drawable,
                         GC gc,
                         int x,
                         int y,
                         int width,
                         int height,
                         XlibRgbDither dith,
                         unsigned char *buf,
                         int rowstride);

void
xxlib_draw_gray_image (XlibRgbHandle *handle, Drawable drawable,
                       GC gc,
                       int x,
                       int y,
                       int width,
                       int height,
                       XlibRgbDither dith,
                       unsigned char *buf,
                       int rowstride);

XlibRgbCmap *
xxlib_rgb_cmap_new (XlibRgbHandle *handle, uint32 *colors, int n_colors);

void
xxlib_rgb_cmap_free (XlibRgbHandle *handle, XlibRgbCmap *cmap);

void
xxlib_draw_indexed_image (XlibRgbHandle *handle, Drawable drawable,
                          GC gc,
                          int x,
                          int y,
                          int width,
                          int height,
                          XlibRgbDither dith,
                          unsigned char *buf,
                          int rowstride,
                          XlibRgbCmap *cmap);

void
xxlib_draw_xprint_scaled_rgb_image( XlibRgbHandle *handle,
                                    Drawable drawable,
                                    long paper_resolution,
                                    long image_resolution,
                                    GC gc,
                                    int x,
                                    int y,
                                    int width,
                                    int height,
                                    XlibRgbDither dith,
                                    unsigned char *rgb_buf,
                                    int rowstride);

/* Below are some functions which are primarily useful for debugging
   and experimentation. */
Bool
xxlib_rgb_ditherable (XlibRgbHandle *handle);

void
xxlib_rgb_set_verbose (XlibRgbHandle *handle, Bool verbose);

void
xxlib_rgb_set_min_colors (XlibRgbHandle *handle, int min_colors);

Colormap
xxlib_rgb_get_cmap (XlibRgbHandle *handle);

Visual *
xxlib_rgb_get_visual (XlibRgbHandle *handle);

XVisualInfo *
xxlib_rgb_get_visual_info (XlibRgbHandle *handle);

int
xxlib_rgb_get_depth (XlibRgbHandle *handle);

/* hint: if you don't how to obtain a handle - use |xxlib_find_handle()| :-) */
Display *
xxlib_rgb_get_display (XlibRgbHandle *handle);

Screen *
xxlib_rgb_get_screen (XlibRgbHandle *handle);

unsigned long
xxlib_get_prec_from_mask(unsigned long);

unsigned long
xxlib_get_shift_from_mask(unsigned long);

/* default name - for cases where there is only one XlibRgbHandle required */
#define XXLIBRGB_DEFAULT_HANDLE ("xxlib-default")

Bool 
xxlib_register_handle(const char *name, XlibRgbHandle *handle);

Bool 
xxlib_deregister_handle(const char *name);

XlibRgbHandle *
xxlib_find_handle(const char *name);

_XFUNCPROTOEND

#endif /* !__XLIB_RGB_H__ */

