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
 * original authors and the copyright holders of this file, the
 * contents of this file are also redistributable under the terms of
 * the Mozilla Public license.  For information about the Mozilla
 * Public License, please see the license information at
 * http://www.mozilla.org/MPL/ */

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

#include <math.h>

#if HAVE_CONFIG_H
#  include <config.h>
#  if STDC_HEADERS
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#  endif
#else
#  include <stdio.h>
#  include <stdlib.h>
#endif

#define ENABLE_GRAYSCALE

#define G_LITTLE_ENDIAN 1
#define G_BIG_ENDIAN 2

/* include this before so that we can get endian definitions if
   they are there... */

#include "xlibrgb.h"

/* check our endianness */
#ifdef USE_MOZILLA_TYPES

/* check to make sure that we don't have both types defined. */
#ifdef IS_LITTLE_ENDIAN
#ifdef IS_BIG_ENDIAN
#error what the hell?  both endian types are defined!
#endif
#endif

#ifdef IS_LITTLE_ENDIAN
#define G_BYTE_ORDER G_LITTLE_ENDIAN
#else
#define G_BYTE_ORDER G_BIG_ENDIAN
#endif /* IS_LITTLE_ENDIAN */

#else

/* XXX fix this for an autodetect for endian-ness */
#define G_BYTE_ORDER G_LITTLE_ENDIAN

#endif /* USE_MOZILLA_TYPES */

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

typedef enum {
  LSB_FIRST,
  MSB_FIRST
} ByteOrder;


typedef struct _XlibRgbInfo   XlibRgbInfo;

typedef void (*XlibRgbConvFunc) (XImage *image,
				 int ax, int ay,
				 int width, int height,
				 unsigned char *buf, int rowstride,
				 int x_align, int y_align,
				 XlibRgbCmap *cmap);

/* Some of these fields should go, as they're not being used at all.
   Globals should generally migrate into here - it's very likely that
   we'll want to run more than one GdkRgbInfo context at the same time
   (i.e. some but not all windows have privately installed
   colormaps). */

struct _XlibRgbInfo
{
  Display          *display;
  Screen           *screen;
  int               screen_num;
  XVisualInfo      *x_visual_info;
  Colormap          cmap;
  XColor           *cmap_colors;
  Visual           *default_visualid;
  Colormap          default_colormap;

  unsigned long    *color_pixels;
  unsigned long    *gray_pixels;
  unsigned long    *reserved_pixels;

  unsigned long     red_shift;
  unsigned long     red_prec;
  unsigned long     blue_shift;
  unsigned long     blue_prec;
  unsigned long     green_shift;
  unsigned long     green_prec;

  unsigned int      nred_shades;
  unsigned int      ngreen_shades;
  unsigned int      nblue_shades;
  unsigned int      ngray_shades;
  unsigned int      nreserved;

  unsigned int      bpp;
  unsigned int      cmap_alloced;
  double            gamma_val;

  /* Generally, the stage buffer is used to convert 32bit RGB, gray,
     and indexed images into 24 bit packed RGB. */
  unsigned char *stage_buf;

  XlibRgbCmap *gray_cmap;

  Bool dith_default;

  Bool bitmap; /* set true if in 1 bit per pixel mode */
  GC own_gc;

  /* Convert functions */
  XlibRgbConvFunc conv;
  XlibRgbConvFunc conv_d;

  XlibRgbConvFunc conv_32;
  XlibRgbConvFunc conv_32_d;

  XlibRgbConvFunc conv_gray;
  XlibRgbConvFunc conv_gray_d;

  XlibRgbConvFunc conv_indexed;
  XlibRgbConvFunc conv_indexed_d;
};

static Bool xlib_rgb_install_cmap = FALSE;
static int xlib_rgb_min_colors = 5 * 5 * 5;
static Bool xlib_rgb_verbose = FALSE;

#define IMAGE_WIDTH 256
#define STAGE_ROWSTRIDE (IMAGE_WIDTH * 3)
#define IMAGE_HEIGHT 64
#define N_IMAGES 6

static XlibRgbInfo *image_info = NULL;
static XImage *static_image[N_IMAGES];
static int static_image_idx;

static unsigned char *colorcube;
static unsigned char *colorcube_d;

unsigned long
xlib_get_prec_from_mask(unsigned long val)
{
  unsigned long retval = 0;
  unsigned int cur_bit = 0;
  /* walk through the number, incrementing the value if
     the bit in question is set. */
  while (cur_bit < (sizeof(unsigned long) * 8)) {
    if ((val >> cur_bit) & 0x1) {
      retval++;
    }
    cur_bit++;
  }
  return retval;
}

unsigned long
xlib_get_shift_from_mask(unsigned long val)
{
  unsigned long cur_bit = 0;
  /* walk through the number, looking for the first 1 */
  while (cur_bit < (sizeof(unsigned long) * 8)) {
    if ((val >> cur_bit) & 0x1) {
      return cur_bit;
    }
    cur_bit++;
  }
  return cur_bit;
}


static int
xlib_rgb_cmap_fail (const char *msg, Colormap cmap, unsigned long *pixels)
{
  unsigned long free_pixels[256];
  int n_free;
  int i;

#ifdef VERBOSE
  printf ("%s", msg);
#endif
  n_free = 0;
  for (i = 0; i < 256; i++)
    if (pixels[i] < 256)
      free_pixels[n_free++] = pixels[i];
  
  if (n_free)
    XFreeColors(image_info->display,
		cmap,
		free_pixels,
		n_free,
		0);
  return 0;
}

static void
xlib_rgb_make_colorcube (unsigned long *pixels, int nr, int ng, int nb)
{
  unsigned char rt[16], gt[16], bt[16];
  int i;

  colorcube = malloc(sizeof(unsigned char) * 4096);
  memset(colorcube, 0, (sizeof(unsigned char) * 4096));
  for (i = 0; i < 16; i++)
    {
      rt[i] = ng * nb * ((i * 17 * (nr - 1) + 128) >> 8);
      gt[i] = nb * ((i * 17 * (ng - 1) + 128) >> 8);
      bt[i] = ((i * 17 * (nb - 1) + 128) >> 8);
    }

  for (i = 0; i < 4096; i++)
    {
      colorcube[i] = pixels[rt[i >> 8] + gt[(i >> 4) & 0x0f] + bt[i & 0x0f]];
#ifdef VERBOSE
      printf ("%03x %02x %x %x %x\n", i, colorcube[i], rt[i >> 8], gt[(i >> 4) & 0x0f], bt[i & 0x0f]);
#endif
    }
}

/* this is the colorcube suitable for dithering */
static void
xlib_rgb_make_colorcube_d (unsigned long *pixels, int nr, int ng, int nb)
{
  int r, g, b;
  int i;

  colorcube_d = malloc(sizeof(unsigned char) * 512);
  memset(colorcube_d, 0, (sizeof(unsigned char) * 512));
  for (i = 0; i < 512; i++)
    {
      r = MIN (nr - 1, i >> 6);
      g = MIN (ng - 1, (i >> 3) & 7);
      b = MIN (nb - 1, i & 7);
      colorcube_d[i] = pixels[(r * ng + g) * nb + b];
    }
}

/* Try installing a color cube of the specified size.
   Make the colorcube and return TRUE on success */
static int
xlib_rgb_try_colormap (int nr, int ng, int nb)
{
  int r, g, b;
  int ri, gi, bi;
  int r0, g0, b0;
  Colormap     cmap;
  XVisualInfo *visual;
  XColor      *colors = NULL;
  XColor       color;
  unsigned long pixels[256];
  unsigned long junk[256];
  int i;
  int d2;
  unsigned int colors_needed;
  int idx;
  int best[256];

  if (nr * ng * nb < xlib_rgb_min_colors)
    return FALSE;

  if (image_info->cmap_alloced) {
    cmap = image_info->cmap;
    visual = image_info->x_visual_info;
  }
  else {
    cmap = image_info->default_colormap;
    visual = image_info->x_visual_info;
  }
  colors_needed = nr * ng * nb;
  for (i = 0; i < 256; i++)
    {
      best[i] = 192;
      pixels[i] = 256;
    }

#ifndef GAMMA
  if (!xlib_rgb_install_cmap) {
    /* go out and get the colors for this colormap. */
    colors = malloc(sizeof(XColor) * visual->colormap_size);
    for (i=0; i < visual->colormap_size; i++){
      colors[i].pixel = i;
    }
    XQueryColors (image_info->display,
		  cmap,
		  colors, visual->colormap_size);
    /* find color cube colors that are already present */
    for (i = 0; i < MIN (256, visual->colormap_size); i++)
      {
	r = colors[i].red >> 8;
	g = colors[i].green >> 8;
	b = colors[i].blue >> 8;
	ri = (r * (nr - 1) + 128) >> 8;
	gi = (g * (ng - 1) + 128) >> 8;
	bi = (b * (nb - 1) + 128) >> 8;
	r0 = ri * 255 / (nr - 1);
	g0 = gi * 255 / (ng - 1);
	b0 = bi * 255 / (nb - 1);
	idx = ((ri * nr) + gi) * nb + bi;
	d2 = (r - r0) * (r - r0) + (g - g0) * (g - g0) + (b - b0) * (b - b0);
	if (d2 < best[idx]) {
	  if (pixels[idx] < 256)
	    XFreeColors(image_info->display,
			cmap,
			pixels + idx,
			1, 0);
	  else
	    colors_needed--;
	  color.pixel = colors[i].pixel;
	  color.red = colors[i].red;
	  color.green = colors[i].green;
	  color.blue = colors[i].blue;
	  color.flags = 0;
	  if (!XAllocColor(image_info->display, cmap, &color))
	    return xlib_rgb_cmap_fail ("error allocating system color\n",
				      cmap, pixels);
	  pixels[idx] = color.pixel; /* which is almost certainly i */
	  best[idx] = d2;
	}
      }
  }

#endif

  if (colors_needed)
    {
      if (!XAllocColorCells(image_info->display, cmap, 0, NULL, 0, junk, colors_needed))
	{
	  char tmp_str[80];
	  
	  sprintf (tmp_str,
		   "%d %d %d colormap failed (in XAllocColorCells)\n",
		   nr, ng, nb);
	  return xlib_rgb_cmap_fail (tmp_str, cmap, pixels);
	}
      XFreeColors(image_info->display, cmap, junk, (int)colors_needed, 0);
    }

  for (r = 0, i = 0; r < nr; r++)
    for (g = 0; g < ng; g++)
      for (b = 0; b < nb; b++, i++)
	{
	  if (pixels[i] == 256)
	    {
	      color.red = r * 65535 / (nr - 1);
	      color.green = g * 65535 / (ng - 1);
	      color.blue = b * 65535 / (nb - 1);

#ifdef GAMMA
	      color.red = 65535 * pow (color.red / 65535.0, 0.5);
	      color.green = 65535 * pow (color.green / 65535.0, 0.5);
	      color.blue = 65535 * pow (color.blue / 65535.0, 0.5);
#endif

	      /* This should be a raw XAllocColor call */
	      if (!XAllocColor(image_info->display, cmap, &color))
		{
		  char tmp_str[80];

		  sprintf (tmp_str, "%d %d %d colormap failed\n",
			   nr, ng, nb);
		  return xlib_rgb_cmap_fail (tmp_str,
					    cmap, pixels);
		}
	      pixels[i] = color.pixel;
	    }
#ifdef VERBOSE
	  printf ("%d: %lx\n", i, pixels[i]);
#endif
	}

  image_info->nred_shades = nr;
  image_info->ngreen_shades = ng;
  image_info->nblue_shades = nb;
  xlib_rgb_make_colorcube (pixels, nr, ng, nb);
  xlib_rgb_make_colorcube_d (pixels, nr, ng, nb);
  if (colors)
    free(colors);
  return TRUE;
}

/* Return TRUE on success. */
static Bool
xlib_rgb_do_colormaps (void)
{
  static const int sizes[][3] = {
    /*    { 6, 7, 6 }, */
    { 6, 6, 6 }, 
    { 6, 6, 5 }, 
    { 6, 6, 4 }, 
    { 5, 5, 5 }, 
    { 5, 5, 4 }, 
    { 4, 4, 4 }, 
    { 4, 4, 3 }, 
    { 3, 3, 3 }, 
    { 2, 2, 2 }
  };
  static const int n_sizes = sizeof(sizes) / (3 * sizeof(int));
  int i;
  
  for (i = 0; i < n_sizes; i++)
    if (xlib_rgb_try_colormap (sizes[i][0], sizes[i][1], sizes[i][2]))
      return TRUE;
  return FALSE;
}

/* Make a 2 x 2 x 2 colorcube */
static void
xlib_rgb_colorcube_222 (void)
{
  int i;
  XColor color;
  Colormap cmap;

  if (image_info->cmap_alloced)
    cmap = image_info->cmap;
  else
    cmap = image_info->default_colormap;

  colorcube_d = malloc(sizeof(unsigned char) * 512);

  for (i = 0; i < 8; i++)
    {
      color.red = ((i & 4) >> 2) * 65535;
      color.green = ((i & 2) >> 1) * 65535;
      color.blue = (i & 1) * 65535;
      XAllocColor (image_info->display, cmap, &color);
      colorcube_d[((i & 4) << 4) | ((i & 2) << 2) | (i & 1)] = color.pixel;
    }
}

void
xlib_rgb_set_verbose (Bool verbose)
{
  xlib_rgb_verbose = verbose;
}

void
xlib_rgb_set_install (Bool install)
{
  xlib_rgb_install_cmap = install;
}

void
xlib_rgb_set_min_colors (int min_colors)
{
  xlib_rgb_min_colors = min_colors;
}

/* Return a "score" based on the following criteria (in hex):

   x000 is the quality - 1 is 1bpp, 2 is 4bpp,
                         4 is 8bpp,
			 7 is 15bpp truecolor, 8 is 16bpp truecolor,
			 9 is 24bpp truecolor.
   0x00 is the speed - 1 is the normal case,
                       2 means faster than normal
   00x0 gets a point for being the system visual
   000x gets a point for being pseudocolor

   A caveat: in the 8bpp modes, being the system visual seems to be
   quite important. Thus, all of the 8bpp modes should be ranked at
   the same speed.
*/

static uint32
xlib_rgb_score_visual (XVisualInfo *visual)
{
  uint32 quality, speed, pseudo, sys;
  static const char* visual_names[] =
  {
    "static gray",
    "grayscale",
    "static color",
    "pseudo color",
    "true color",
    "direct color",
  };
  
  
  quality = 0;
  speed = 1;
  sys = 0;
  if (visual->class == TrueColor ||
      visual->class == DirectColor)
    {
      if (visual->depth == 24)
	{
	  quality = 9;
	  /* Should test for MSB visual here, and set speed if so. */
	}
      else if (visual->depth == 16)
	quality = 8;
      else if (visual->depth == 15)
	quality = 7;
      else if (visual->depth == 8)
	quality = 4;
    }
  else if (visual->class == PseudoColor ||
	   visual->class == StaticColor)
    {
      if (visual->depth == 8)
	quality = 4;
      else if (visual->depth == 4)
	quality = 2;
      else if (visual->depth == 1)
	quality = 1;
    }
  else if (visual->class == StaticGray
#ifdef ENABLE_GRAYSCALE
	   || visual->class == GrayScale
#endif
	   )
    {
      if (visual->depth == 8)
	quality = 4;
      else if (visual->depth == 4)
	quality = 2;
      else if (visual->depth == 1)
	quality = 1;
    }

  if (quality == 0)
    return 0;

  sys = (visual->visualid == image_info->default_visualid->visualid);
  
  pseudo = (visual->class == PseudoColor || visual->class == TrueColor);

  if (xlib_rgb_verbose)
    printf ("Visual 0x%x, type = %s, depth = %d, %ld:%ld:%ld%s; score=%x\n",
	    (int)visual->visualid,
	    visual_names[visual->class],
	    visual->depth,
	    visual->red_mask,
	    visual->green_mask,
	    visual->blue_mask,
	    sys ? " (system)" : "",
	    (quality << 12) | (speed << 8) | (sys << 4) | pseudo);
  
  return (quality << 12) | (speed << 8) | (sys << 4) | pseudo;
}

static void
xlib_rgb_choose_visual (void)
{
  XVisualInfo *visuals;
  XVisualInfo *visual;
  XVisualInfo *best_visual;
  XVisualInfo *final_visual;
  XVisualInfo template;
  int num_visuals;
  uint32 score, best_score;
  int cur_visual = 1;
  int i;
  
  template.screen = image_info->screen_num;
  visuals = XGetVisualInfo(image_info->display, VisualScreenMask,
			   &template, &num_visuals);
  
  best_visual = visuals;
  best_score = xlib_rgb_score_visual (best_visual);

  for (i = cur_visual; i < num_visuals; i++)
    {
      visual = &visuals[i];
      score = xlib_rgb_score_visual  (visual);
      if (score > best_score)
	{
	  best_score = score;
	  best_visual = visual;
	}
    }
  /* make a copy of the visual so that we can free
     the allocated visual list above. */
  final_visual = malloc(sizeof(XVisualInfo));
  memcpy(final_visual, best_visual, sizeof(XVisualInfo));
  image_info->x_visual_info = final_visual;
  XFree(visuals);
  /* set up the shift and the precision for the red, green and blue.
     this only applies to cool visuals like true color and direct color. */
  if (image_info->x_visual_info->class == TrueColor ||
      image_info->x_visual_info->class == DirectColor) {
    image_info->red_shift = xlib_get_shift_from_mask(image_info->x_visual_info->red_mask);
    image_info->red_prec = xlib_get_prec_from_mask(image_info->x_visual_info->red_mask);
    image_info->green_shift = xlib_get_shift_from_mask(image_info->x_visual_info->green_mask);
    image_info->green_prec = xlib_get_prec_from_mask(image_info->x_visual_info->green_mask);
    image_info->blue_shift = xlib_get_shift_from_mask(image_info->x_visual_info->blue_mask);
    image_info->blue_prec = xlib_get_prec_from_mask(image_info->x_visual_info->blue_mask);
  }
}

static void
xlib_rgb_choose_visual_for_xprint (int aDepth)
{
  XVisualInfo *visuals;
  XVisualInfo *visual;
  XVisualInfo *best_visual;
  XVisualInfo *final_visual;
  XVisualInfo template;
  int num_visuals;
  uint32 score;
  int cur_visual = 1;
  int i;

  XWindowAttributes win_att;
  Status ret_stat;
  Visual      *root_visual;

  ret_stat = XGetWindowAttributes(image_info->display, 
			RootWindow(image_info->display, image_info->screen_num),
			&win_att);
  root_visual = win_att.visual;
  template.screen = image_info->screen_num;
  visuals = XGetVisualInfo(image_info->display, VisualScreenMask,
			   &template, &num_visuals);
 
  best_visual = visuals;
  if (best_visual->visual != root_visual) {
     for (i = cur_visual; i < num_visuals; i++) {
        visual = &visuals[i];
        if (visual->visual == root_visual) {
           best_visual = visual;
           break;
        }
      }
   }
  /* make a copy of the visual so that we can free
     the allocated visual list above. */
  final_visual = malloc(sizeof(XVisualInfo));
  memcpy(final_visual, best_visual, sizeof(XVisualInfo));
  image_info->x_visual_info = final_visual;
  XFree(visuals);
  /* set up the shift and the precision for the red, green and blue.
     this only applies to cool visuals like true color and direct color. */
  if (image_info->x_visual_info->class == TrueColor ||
      image_info->x_visual_info->class == DirectColor) {
    image_info->red_shift = xlib_get_shift_from_mask(image_info->x_visual_info->red_mask);
    image_info->red_prec = xlib_get_prec_from_mask(image_info->x_visual_info->red_mask);
    image_info->green_shift = xlib_get_shift_from_mask(image_info->x_visual_info->green_mask);
    image_info->green_prec = xlib_get_prec_from_mask(image_info->x_visual_info->green_mask);
    image_info->blue_shift = xlib_get_shift_from_mask(image_info->x_visual_info->blue_mask);
    image_info->blue_prec = xlib_get_prec_from_mask(image_info->x_visual_info->blue_mask);
  }
}

static void xlib_rgb_select_conv (XImage *image, ByteOrder byte_order);

static void
xlib_rgb_set_gray_cmap (Colormap cmap)
{
  int i;
  XColor color;
  int status;
  unsigned long pixels[256];
  int r, g, b, gray;

  for (i = 0; i < 256; i++)
    {
      color.pixel = i;
      color.red = i * 257;
      color.green = i * 257;
      color.blue = i * 257;
      status = XAllocColor(image_info->display, cmap, &color);
      pixels[i] = color.pixel;
#ifdef VERBOSE
      printf ("allocating pixel %d, %x %x %x, result %d\n",
	       color.pixel, color.red, color.green, color.blue, status);
#endif
    }

  /* Now, we make fake colorcubes - we ultimately just use the pseudocolor
     methods. */

  colorcube = malloc(sizeof(unsigned char) * 4096);

  for (i = 0; i < 4096; i++)
    {
      r = (i >> 4) & 0xf0;
      r = r | r >> 4;
      g = i & 0xf0;
      g = g | g >> 4;
      b = (i << 4 & 0xf0);
      b = b | b >> 4;
      gray = (g + ((r + b) >> 1)) >> 1;
      colorcube[i] = pixels[gray];
    }
}

void
xlib_rgb_init (Display *display, Screen *screen)
{
  int prefDepth = -1;            // let the function do the visual scoring
  xlib_rgb_init_with_depth(display, screen, prefDepth);
}

void
xlib_rgb_init_with_depth (Display *display, Screen *screen, int prefDepth)
{
  int i;
  static const int byte_order[1] = { 1 };

  static int initialized = 0;

  if (initialized)
  {
    return;
  }

  initialized = 1;

  /* check endian sanity */
#if G_BYTE_ORDER == G_BIG_ENDIAN
  if (((char *)byte_order)[0] == 1) {
    printf ("xlib_rgb_init: compiled for big endian, but this is a little endian machine.\n\n");
    exit(1);
  }
#else
  if (((char *)byte_order)[0] != 1) {
    printf ("xlib_rgb_init: compiled for little endian, but this is a big endian machine.\n\n");
    exit(1);
  }
#endif

  if (image_info == NULL)
    {
      image_info = malloc(sizeof(XlibRgbInfo));
      memset(image_info, 0, sizeof(XlibRgbInfo));

      image_info->display = display;
      image_info->screen = screen;
      image_info->screen_num = XScreenNumberOfScreen(screen);
      image_info->x_visual_info = NULL;
      image_info->cmap = 0;
      image_info->default_visualid = DefaultVisual(display, image_info->screen_num);
      image_info->default_colormap = DefaultColormap(display, image_info->screen_num);

      image_info->color_pixels = NULL;
      image_info->gray_pixels = NULL;
      image_info->reserved_pixels = NULL;

      image_info->nred_shades = 6;
      image_info->ngreen_shades = 6;
      image_info->nblue_shades = 4;
      image_info->ngray_shades = 24;
      image_info->nreserved = 0;

      image_info->bpp = 0;
      image_info->cmap_alloced = FALSE;
      image_info->gamma_val = 1.0;

      image_info->stage_buf = NULL;

      image_info->own_gc = 0;
      
      image_info->red_shift = 0;
      image_info->red_prec = 0;
      image_info->green_shift = 0;
      image_info->green_prec = 0;
      image_info->blue_shift = 0;
      image_info->blue_prec = 0;

      if (prefDepth != -1)
        xlib_rgb_choose_visual_for_xprint (prefDepth);
      else
        xlib_rgb_choose_visual ();

      if ((image_info->x_visual_info->class == PseudoColor ||
	   image_info->x_visual_info->class == StaticColor) &&
	  image_info->x_visual_info->depth < 8 &&
	  image_info->x_visual_info->depth >= 3)
	{
	  image_info->cmap = image_info->default_colormap;
	  xlib_rgb_colorcube_222 ();
	}
      else if (image_info->x_visual_info->class == PseudoColor)
	{
	  if (xlib_rgb_install_cmap ||
	      image_info->x_visual_info->visualid != image_info->default_visualid->visualid)
	    {
	      image_info->cmap = XCreateColormap(image_info->display,
						 RootWindow(image_info->display, image_info->screen_num),
						 image_info->x_visual_info->visual,
						 AllocNone);
	      image_info->cmap_alloced = TRUE;
	    }
	  if (!xlib_rgb_do_colormaps ())
	    {
	      image_info->cmap = XCreateColormap(image_info->display,
						 RootWindow(image_info->display, image_info->screen_num),
						 image_info->x_visual_info->visual,
						 AllocNone);
	      image_info->cmap_alloced = TRUE;
	      xlib_rgb_do_colormaps ();
	    }
	  if (xlib_rgb_verbose)
	    printf ("color cube: %d x %d x %d\n",
		    image_info->nred_shades,
		    image_info->ngreen_shades,
		    image_info->nblue_shades);

	  if (!image_info->cmap_alloced)
	      image_info->cmap = image_info->default_colormap;
	}
#ifdef ENABLE_GRAYSCALE
      else if (image_info->x_visual_info->class == GrayScale)
	{
	  image_info->cmap = XCreateColormap(image_info->display,
					     RootWindow(image_info->display, image_info->screen_num),
					     image_info->x_visual_info->visual,
					     AllocNone);
	  xlib_rgb_set_gray_cmap (image_info->cmap);
	  image_info->cmap_alloced = TRUE;
     	}
#endif
      else
	{
	  /* Always install colormap in direct color. */
	  if (image_info->x_visual_info->class != DirectColor && 
	      image_info->x_visual_info->visualid == image_info->default_visualid->visualid)
	    image_info->cmap = image_info->default_colormap;
	  else
	    {
	      image_info->cmap = XCreateColormap(image_info->display,
						 RootWindow(image_info->display, image_info->screen_num),
						 image_info->x_visual_info->visual,
						 AllocNone);
	      image_info->cmap_alloced = TRUE;
	    }
	}

      image_info->bitmap = (image_info->x_visual_info->depth == 1);

      for (i = 0; i < N_IMAGES; i++) {
	if (image_info->bitmap) {
	  /* Use malloc() instead of g_malloc since X will free() this mem */
	  static_image[i] = XCreateImage(image_info->display,
					 image_info->x_visual_info->visual,
					 1,
					 XYBitmap,
					 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT,
					 8,
					 0);
	  static_image[i]->data = malloc(IMAGE_WIDTH * IMAGE_HEIGHT >> 3);
	  static_image[i]->bitmap_bit_order = MSBFirst;
	  static_image[i]->byte_order = MSBFirst;
	}
	else {
	  static_image[i] = XCreateImage(image_info->display,
					 image_info->x_visual_info->visual,
					 (unsigned int)image_info->x_visual_info->depth,
					 ZPixmap,
					 0, 0,
					 IMAGE_WIDTH,
					 IMAGE_HEIGHT,
					 32, 0);
	  /* remove this when we are using shared memory.. */
	  static_image[i]->data = malloc((size_t)IMAGE_WIDTH * IMAGE_HEIGHT * image_info->x_visual_info->depth);
	  static_image[i]->bitmap_bit_order = MSBFirst;
	  static_image[i]->byte_order = MSBFirst;
	}
      }
      /* ok, so apparently, image_info->bpp is actually
	 BYTES per pixel.  What fun! */
      switch (static_image[0]->bits_per_pixel) {
      case 1:
      case 8:
	image_info->bpp = 1;
	break;
      case 16:
	image_info->bpp = 2;
	break;
      case 24:
	image_info->bpp = 3;
	break;
      case 32:
	image_info->bpp = 4;
	break;
      }
      xlib_rgb_select_conv (static_image[0], MSB_FIRST);
    }
}

/* convert an rgb value into an X pixel code */
unsigned long
xlib_rgb_xpixel_from_rgb (uint32 rgb)
{
  unsigned long pixel = 0;

  if (image_info->bitmap)
    {
      return ((rgb & 0xff0000) >> 16) +
	((rgb & 0xff00) >> 7) +
	(rgb & 0xff) > 510;
    }
  else if (image_info->x_visual_info->class == PseudoColor)
    pixel = colorcube[((rgb & 0xf00000) >> 12) |
		     ((rgb & 0xf000) >> 8) |
		     ((rgb & 0xf0) >> 4)];
  else if (image_info->x_visual_info->depth < 8 &&
	   image_info->x_visual_info->class == StaticColor)
    {
      pixel = colorcube_d[((rgb & 0x800000) >> 17) |
			 ((rgb & 0x8000) >> 12) |
			 ((rgb & 0x80) >> 7)];
    }
  else if (image_info->x_visual_info->class == TrueColor ||
	   image_info->x_visual_info->class == DirectColor)
    {
#ifdef VERBOSE
      printf ("shift, prec: r %d %d g %d %d b %d %d\n",
	      image_info->red_shift,
	      image_info->red_prec,
	      image_info->green_shift,
	      image_info->green_prec,
	      image_info->blue_shift,
	      image_info->blue_prec);
#endif

      pixel = (((((rgb & 0xff0000) >> 16) >>
		 (8 - image_info->red_prec)) <<
		image_info->red_shift) +
	       ((((rgb & 0xff00) >> 8)  >>
		 (8 - image_info->green_prec)) <<
		image_info->green_shift) +
	       (((rgb & 0xff) >>
		 (8 - image_info->blue_prec)) <<
		image_info->blue_shift));
    }
  else if (image_info->x_visual_info->class == StaticGray ||
	   image_info->x_visual_info->class == GrayScale)
    {
      int gray = ((rgb & 0xff0000) >> 16) +
	((rgb & 0xff00) >> 7) +
	(rgb & 0xff);

      return gray >> (10 - image_info->x_visual_info->depth);
    }

  return pixel;
}

void
xlib_rgb_gc_set_foreground (GC gc, uint32 rgb)
{
  unsigned long color;

  color = xlib_rgb_xpixel_from_rgb (rgb);
  XSetForeground(image_info->display, gc, color);
}

void
xlib_rgb_gc_set_background (GC gc, uint32 rgb)
{
  unsigned long color;

  color = xlib_rgb_xpixel_from_rgb (rgb);
  XSetBackground(image_info->display, gc, color);
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define HAIRY_CONVERT_8
#endif

#ifdef HAIRY_CONVERT_8
static void
xlib_rgb_convert_8 (XImage *image,
		   int ax, int ay, int width, int height,
		   unsigned char *buf, int rowstride,
		   int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = 0; x < width; x++)
	    {
	      r = *bp2++;
	      g = *bp2++;
	      b = *bp2++;
	      obptr[0] = colorcube[((r & 0xf0) << 4) |
				  (g & 0xf0) |
				  (b >> 4)];
	      obptr++;
	    }
	}
      else
	{
	  for (x = 0; x < width - 3; x += 4)
	    {
	      uint32 r1b0g0r0;
	      uint32 g2r2b1g1;
	      uint32 b3g3r3b2;

	      r1b0g0r0 = ((uint32 *)bp2)[0];
	      g2r2b1g1 = ((uint32 *)bp2)[1];
	      b3g3r3b2 = ((uint32 *)bp2)[2];
	      ((uint32 *)obptr)[0] =
		colorcube[((r1b0g0r0 & 0xf0) << 4) | 
			 ((r1b0g0r0 & 0xf000) >> 8) |
			 ((r1b0g0r0 & 0xf00000) >> 20)] |
		(colorcube[((r1b0g0r0 & 0xf0000000) >> 20) |
			  (g2r2b1g1 & 0xf0) |
			  ((g2r2b1g1 & 0xf000) >> 12)] << 8) |
		(colorcube[((g2r2b1g1 & 0xf00000) >> 12) |
			  ((g2r2b1g1 & 0xf0000000) >> 24) |
			  ((b3g3r3b2 & 0xf0) >> 4)] << 16) |
		(colorcube[((b3g3r3b2 & 0xf000) >> 4) |
			  ((b3g3r3b2 & 0xf00000) >> 16) |
			  (b3g3r3b2 >> 28)] << 24);
	      bp2 += 12;
	      obptr += 4;
	    }
	  for (; x < width; x++)
	    {
	      r = *bp2++;
	      g = *bp2++;
	      b = *bp2++;
	      obptr[0] = colorcube[((r & 0xf0) << 4) |
				  (g & 0xf0) |
				  (b >> 4)];
	      obptr++;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
static void
xlib_rgb_convert_8 (XImage *image,
		   int ax, int ay, int width, int height,
		   unsigned char *buf, int rowstride,
		   int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  obptr[0] = colorcube[((r & 0xf0) << 4) |
			      (g & 0xf0) |
			      (b >> 4)];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

#if 1

/* This dither table was generated by Raph Levien using patented
   technology (US Patent 5,276,535). The dither table itself is in the
   public domain. */

#define DM_WIDTH 128
#define DM_WIDTH_SHIFT 7
#define DM_HEIGHT 128
static const unsigned char DM[128][128] =
{
  { 0, 41, 23, 5, 17, 39, 7, 15, 62, 23, 40, 51, 31, 47, 9, 32, 52, 27, 57, 25, 6, 61, 27, 52, 37, 7, 40, 63, 18, 36, 10, 42, 25, 62, 45, 34, 20, 42, 37, 14, 35, 29, 50, 10, 61, 2, 40, 8, 37, 12, 58, 22, 5, 41, 10, 39, 0, 60, 11, 46, 2, 55, 38, 17, 36, 59, 13, 54, 37, 56, 8, 29, 16, 13, 63, 22, 41, 55, 7, 20, 49, 14, 23, 55, 37, 23, 19, 36, 15, 49, 23, 63, 30, 14, 38, 27, 53, 13, 22, 41, 19, 31, 7, 19, 50, 30, 49, 16, 3, 32, 56, 40, 29, 34, 8, 48, 19, 45, 4, 51, 12, 46, 35, 49, 16, 42, 12, 62 },
  { 30, 57, 36, 54, 47, 34, 52, 27, 43, 4, 28, 7, 17, 36, 62, 13, 44, 7, 18, 48, 33, 21, 44, 14, 30, 47, 12, 33, 5, 55, 31, 58, 13, 30, 4, 17, 52, 10, 60, 26, 46, 0, 39, 27, 42, 22, 47, 25, 60, 32, 9, 38, 48, 17, 59, 30, 49, 18, 34, 25, 51, 19, 5, 48, 21, 8, 28, 46, 1, 32, 41, 19, 54, 47, 37, 18, 28, 11, 44, 30, 39, 56, 2, 33, 8, 42, 61, 28, 58, 8, 46, 9, 41, 4, 58, 7, 21, 48, 59, 10, 52, 14, 42, 57, 12, 25, 7, 53, 42, 24, 11, 50, 17, 59, 42, 2, 36, 60, 32, 17, 63, 29, 21, 7, 59, 32, 24, 39 },
  { 22, 8, 16, 32, 3, 25, 13, 57, 18, 45, 58, 39, 55, 20, 5, 42, 23, 34, 63, 1, 51, 10, 58, 4, 60, 23, 53, 27, 44, 21, 3, 48, 8, 50, 43, 54, 27, 32, 5, 55, 21, 58, 12, 53, 6, 36, 14, 50, 17, 29, 53, 15, 24, 52, 7, 36, 13, 42, 4, 53, 9, 35, 61, 26, 56, 32, 49, 15, 62, 23, 6, 60, 2, 31, 4, 48, 58, 38, 15, 61, 5, 25, 47, 28, 50, 15, 7, 40, 3, 32, 33, 52, 25, 50, 35, 42, 61, 3, 28, 36, 23, 63, 4, 33, 46, 62, 36, 23, 60, 6, 54, 28, 4, 37, 23, 55, 25, 8, 42, 54, 14, 6, 56, 38, 19, 52, 4, 46 },
  { 48, 53, 43, 12, 45, 63, 30, 37, 9, 34, 21, 1, 25, 47, 29, 58, 3, 54, 15, 39, 29, 17, 38, 35, 20, 43, 1, 49, 15, 59, 29, 39, 22, 35, 16, 23, 1, 47, 39, 18, 8, 44, 25, 31, 57, 19, 63, 4, 45, 3, 42, 61, 1, 31, 45, 20, 57, 29, 62, 21, 32, 41, 14, 44, 3, 39, 5, 34, 10, 43, 51, 35, 23, 52, 40, 10, 21, 1, 53, 18, 51, 43, 12, 62, 18, 54, 26, 51, 20, 57, 14, 1, 62, 16, 11, 18, 32, 39, 17, 44, 1, 48, 26, 37, 18, 2, 51, 14, 28, 45, 35, 18, 57, 13, 47, 11, 51, 20, 2, 39, 31, 47, 25, 1, 50, 11, 60, 7 },
  { 18, 28, 1, 56, 21, 10, 51, 2, 46, 54, 14, 61, 11, 50, 13, 38, 19, 31, 45, 9, 55, 24, 47, 5, 54, 9, 62, 11, 35, 8, 51, 14, 57, 6, 63, 40, 58, 14, 51, 28, 62, 34, 15, 48, 1, 41, 30, 35, 55, 21, 34, 11, 49, 37, 8, 52, 4, 23, 15, 43, 1, 58, 11, 23, 53, 16, 55, 26, 58, 18, 27, 12, 45, 14, 25, 63, 42, 33, 27, 35, 9, 31, 21, 38, 1, 44, 34, 12, 48, 38, 21, 44, 29, 47, 26, 53, 1, 46, 54, 8, 59, 29, 11, 55, 22, 41, 33, 20, 39, 1, 48, 9, 44, 32, 5, 62, 29, 44, 57, 23, 10, 58, 34, 43, 15, 37, 26, 33 },
  { 51, 38, 59, 24, 35, 42, 19, 60, 5, 32, 41, 26, 43, 33, 7, 53, 48, 11, 59, 23, 42, 2, 61, 30, 16, 40, 32, 24, 56, 41, 19, 33, 37, 26, 47, 9, 31, 22, 2, 45, 9, 54, 4, 37, 21, 52, 11, 23, 7, 57, 16, 25, 55, 18, 63, 27, 46, 39, 56, 10, 50, 37, 29, 47, 19, 63, 24, 9, 46, 2, 39, 60, 9, 57, 30, 7, 49, 11, 59, 3, 45, 57, 5, 60, 29, 22, 5, 60, 30, 9, 59, 18, 40, 6, 57, 36, 30, 12, 24, 34, 15, 40, 52, 6, 49, 9, 58, 4, 63, 12, 26, 61, 22, 53, 38, 16, 35, 14, 28, 50, 42, 17, 5, 28, 62, 20, 54, 12 },
  { 26, 6, 31, 15, 49, 6, 38, 27, 22, 49, 16, 56, 2, 62, 30, 21, 0, 36, 28, 6, 49, 32, 13, 52, 26, 50, 19, 46, 3, 26, 62, 0, 53, 12, 29, 3, 53, 41, 60, 24, 38, 13, 58, 16, 43, 9, 59, 39, 46, 28, 44, 40, 2, 33, 13, 41, 16, 6, 47, 31, 26, 17, 57, 6, 38, 0, 42, 36, 29, 52, 20, 31, 48, 0, 34, 56, 20, 36, 23, 54, 14, 41, 24, 37, 10, 55, 46, 25, 16, 45, 36, 4, 55, 23, 15, 8, 50, 62, 5, 56, 44, 20, 13, 28, 59, 31, 24, 47, 31, 52, 37, 17, 40, 0, 26, 49, 3, 60, 7, 33, 0, 61, 53, 40, 8, 45, 2, 41 },
  { 16, 63, 43, 4, 61, 24, 56, 13, 53, 8, 36, 12, 24, 41, 16, 46, 60, 26, 52, 39, 14, 57, 21, 37, 0, 45, 7, 59, 38, 17, 43, 10, 45, 20, 61, 43, 19, 11, 33, 17, 50, 32, 23, 61, 28, 49, 26, 0, 18, 51, 5, 60, 22, 58, 29, 0, 59, 34, 19, 62, 3, 52, 7, 44, 30, 59, 13, 50, 15, 62, 7, 17, 38, 22, 44, 15, 40, 4, 47, 28, 33, 17, 49, 16, 51, 40, 10, 56, 0, 53, 13, 49, 28, 38, 60, 21, 43, 19, 37, 27, 3, 51, 34, 39, 0, 45, 15, 43, 10, 21, 3, 55, 8, 33, 59, 10, 41, 18, 52, 24, 46, 20, 30, 13, 58, 22, 36, 57 },
  { 50, 34, 11, 47, 29, 17, 44, 0, 33, 63, 28, 46, 52, 5, 57, 10, 42, 18, 4, 63, 20, 8, 44, 10, 56, 34, 14, 29, 5, 54, 23, 59, 32, 49, 7, 34, 49, 27, 56, 0, 42, 7, 46, 3, 40, 6, 54, 32, 62, 13, 36, 10, 47, 8, 35, 49, 24, 51, 12, 40, 22, 35, 60, 12, 22, 51, 33, 4, 40, 25, 43, 55, 5, 54, 12, 61, 26, 51, 8, 62, 0, 53, 7, 63, 2, 32, 19, 34, 42, 24, 31, 63, 2, 10, 45, 33, 0, 48, 9, 61, 22, 47, 8, 62, 18, 56, 7, 54, 27, 57, 46, 30, 50, 19, 45, 30, 56, 36, 22, 47, 11, 38, 3, 51, 32, 48, 18, 9 },
  { 0, 21, 40, 19, 52, 9, 37, 48, 20, 40, 3, 18, 27, 38, 35, 22, 31, 56, 13, 35, 46, 28, 60, 40, 27, 18, 61, 50, 41, 30, 7, 36, 2, 25, 16, 57, 5, 15, 47, 29, 55, 19, 30, 52, 15, 34, 20, 12, 43, 30, 20, 54, 25, 44, 53, 12, 38, 5, 55, 27, 48, 15, 33, 27, 45, 8, 19, 28, 56, 11, 33, 49, 18, 36, 29, 2, 45, 16, 39, 19, 31, 43, 27, 35, 20, 52, 26, 6, 61, 11, 41, 17, 29, 51, 20, 56, 25, 32, 41, 17, 53, 31, 25, 14, 42, 23, 35, 16, 38, 6, 34, 12, 15, 62, 6, 21, 13, 1, 63, 9, 55, 27, 43, 25, 14, 4, 31, 55 },
  { 44, 29, 61, 2, 35, 58, 26, 15, 60, 10, 51, 59, 14, 55, 8, 50, 2, 44, 25, 51, 1, 33, 16, 4, 48, 36, 2, 21, 12, 57, 48, 13, 51, 55, 40, 28, 37, 62, 8, 39, 12, 63, 36, 10, 59, 24, 56, 47, 9, 50, 41, 1, 32, 17, 6, 21, 61, 30, 9, 43, 1, 54, 41, 2, 54, 37, 48, 61, 1, 46, 21, 3, 58, 24, 50, 32, 60, 10, 57, 25, 46, 12, 59, 4, 45, 13, 57, 47, 27, 39, 5, 58, 47, 14, 35, 4, 52, 13, 60, 6, 36, 10, 45, 55, 4, 50, 29, 2, 61, 50, 25, 58, 44, 24, 36, 42, 54, 28, 40, 32, 16, 56, 6, 62, 46, 39, 60, 23 },
  { 7, 48, 14, 54, 23, 40, 4, 45, 30, 22, 42, 32, 1, 44, 20, 29, 58, 8, 37, 19, 41, 54, 24, 58, 9, 53, 25, 46, 34, 16, 23, 38, 27, 11, 18, 1, 52, 21, 35, 22, 48, 5, 25, 45, 18, 38, 2, 27, 35, 4, 57, 15, 62, 39, 57, 28, 42, 16, 36, 60, 24, 18, 10, 63, 20, 5, 16, 23, 37, 14, 59, 27, 41, 8, 13, 42, 21, 35, 6, 50, 3, 38, 15, 48, 30, 39, 17, 3, 49, 14, 53, 33, 24, 7, 61, 44, 11, 39, 23, 49, 19, 58, 1, 32, 36, 12, 60, 41, 20, 13, 41, 4, 39, 1, 48, 8, 18, 51, 14, 44, 5, 37, 21, 34, 1, 26, 10, 37 },
  { 53, 36, 27, 9, 50, 12, 32, 55, 2, 57, 7, 17, 48, 34, 63, 15, 40, 26, 62, 11, 49, 6, 31, 39, 22, 42, 6, 63, 1, 39, 60, 4, 42, 61, 32, 45, 24, 44, 2, 60, 16, 41, 53, 1, 33, 61, 49, 17, 63, 23, 45, 26, 33, 3, 23, 46, 2, 50, 20, 4, 45, 34, 49, 30, 39, 58, 44, 31, 53, 34, 6, 52, 30, 47, 63, 1, 53, 22, 42, 31, 58, 23, 54, 22, 61, 8, 36, 59, 22, 35, 21, 1, 55, 40, 27, 16, 30, 54, 2, 29, 43, 16, 39, 63, 21, 46, 26, 10, 48, 32, 19, 53, 30, 56, 26, 60, 33, 4, 61, 23, 49, 59, 15, 53, 19, 58, 42, 16 },
  { 20, 5, 59, 46, 25, 62, 7, 19, 43, 25, 37, 61, 11, 24, 4, 54, 12, 52, 3, 32, 17, 61, 12, 47, 15, 55, 18, 31, 53, 28, 9, 50, 21, 6, 55, 9, 58, 14, 54, 26, 33, 7, 31, 58, 13, 21, 8, 42, 29, 6, 37, 11, 48, 52, 14, 60, 11, 39, 56, 32, 14, 58, 7, 26, 17, 4, 42, 8, 11, 47, 19, 38, 10, 17, 26, 37, 9, 55, 28, 13, 18, 40, 6, 33, 1, 43, 25, 11, 51, 7, 62, 43, 18, 37, 3, 57, 45, 9, 38, 58, 5, 52, 27, 7, 17, 53, 5, 57, 37, 2, 63, 9, 22, 15, 11, 38, 25, 45, 35, 0, 28, 10, 41, 30, 50, 8, 31, 57 },
  { 49, 33, 16, 38, 1, 42, 51, 34, 53, 14, 28, 49, 30, 56, 36, 23, 43, 20, 38, 56, 22, 45, 28, 0, 62, 35, 26, 44, 11, 19, 52, 35, 44, 15, 30, 38, 10, 31, 40, 4, 46, 50, 20, 40, 27, 44, 51, 14, 56, 53, 19, 59, 7, 29, 41, 19, 35, 25, 8, 52, 22, 44, 13, 53, 50, 32, 61, 24, 56, 25, 63, 0, 45, 57, 33, 59, 16, 46, 4, 62, 50, 11, 60, 37, 52, 19, 55, 29, 37, 46, 13, 26, 48, 10, 50, 34, 21, 63, 26, 13, 42, 33, 22, 55, 35, 28, 43, 15, 24, 51, 27, 34, 46, 49, 58, 3, 52, 9, 57, 19, 48, 55, 3, 35, 12, 45, 24, 3 },
  { 41, 11, 56, 28, 18, 31, 22, 10, 37, 6, 47, 13, 3, 41, 9, 46, 0, 48, 29, 6, 34, 10, 55, 37, 20, 8, 49, 3, 41, 59, 14, 25, 0, 63, 19, 47, 27, 51, 17, 57, 23, 10, 61, 6, 54, 3, 38, 31, 0, 22, 34, 43, 20, 55, 31, 0, 49, 63, 29, 38, 3, 62, 28, 40, 0, 22, 14, 35, 2, 48, 15, 43, 23, 14, 3, 29, 49, 20, 39, 34, 0, 44, 29, 9, 15, 47, 5, 42, 0, 31, 58, 5, 31, 61, 23, 15, 0, 47, 19, 50, 24, 3, 59, 11, 44, 0, 31, 59, 6, 42, 17, 60, 0, 39, 20, 31, 43, 17, 29, 40, 12, 25, 60, 22, 52, 15, 63, 29 },
  { 20, 52, 8, 44, 62, 4, 59, 49, 17, 63, 21, 39, 60, 18, 52, 27, 33, 59, 14, 51, 59, 43, 24, 5, 51, 30, 57, 17, 32, 5, 37, 56, 48, 34, 42, 3, 60, 5, 36, 13, 43, 37, 18, 34, 25, 12, 59, 24, 47, 36, 11, 50, 3, 38, 9, 58, 16, 5, 43, 18, 47, 10, 37, 18, 59, 46, 29, 52, 40, 12, 34, 28, 56, 36, 53, 7, 43, 8, 24, 52, 26, 17, 56, 43, 24, 32, 63, 20, 57, 16, 22, 52, 36, 8, 41, 56, 29, 32, 54, 7, 35, 57, 14, 48, 20, 62, 13, 39, 53, 29, 8, 45, 13, 29, 7, 61, 14, 54, 6, 63, 38, 32, 18, 43, 2, 39, 6, 47 },
  { 0, 58, 23, 35, 13, 46, 12, 39, 0, 31, 55, 24, 5, 35, 15, 61, 17, 5, 39, 25, 18, 2, 50, 33, 41, 13, 39, 23, 62, 46, 29, 12, 22, 8, 56, 25, 20, 49, 32, 62, 0, 56, 11, 46, 63, 42, 9, 16, 55, 5, 60, 15, 62, 26, 45, 21, 36, 51, 13, 57, 31, 24, 55, 6, 35, 9, 57, 5, 20, 60, 7, 51, 5, 19, 40, 25, 61, 32, 56, 12, 36, 48, 21, 2, 58, 12, 39, 28, 9, 50, 40, 12, 44, 18, 25, 49, 6, 38, 11, 62, 18, 46, 30, 9, 40, 25, 49, 19, 10, 36, 55, 22, 33, 52, 41, 18, 37, 27, 49, 21, 2, 46, 7, 53, 33, 61, 27, 35 },
  { 41, 31, 5, 39, 51, 26, 33, 57, 27, 41, 9, 44, 54, 29, 48, 7, 44, 36, 57, 10, 31, 63, 16, 45, 11, 60, 1, 47, 7, 20, 43, 3, 58, 36, 13, 52, 39, 7, 15, 28, 22, 48, 30, 21, 1, 29, 49, 44, 27, 17, 40, 30, 24, 42, 12, 53, 33, 7, 47, 20, 1, 42, 11, 49, 25, 43, 17, 32, 45, 27, 41, 21, 31, 62, 11, 49, 2, 15, 42, 5, 63, 7, 41, 27, 49, 6, 54, 23, 46, 34, 2, 28, 54, 3, 59, 12, 46, 17, 42, 28, 40, 1, 37, 51, 5, 55, 2, 34, 47, 16, 3, 62, 47, 5, 23, 56, 1, 44, 12, 34, 51, 16, 57, 11, 25, 17, 54, 13 },
  { 60, 26, 55, 18, 3, 60, 20, 6, 52, 15, 50, 19, 32, 11, 23, 53, 26, 21, 1, 47, 42, 27, 8, 58, 21, 27, 53, 36, 26, 54, 31, 50, 17, 30, 45, 1, 29, 59, 44, 53, 41, 4, 35, 58, 51, 19, 32, 4, 52, 34, 48, 8, 51, 5, 56, 2, 25, 61, 27, 38, 54, 27, 62, 21, 51, 1, 39, 62, 10, 50, 1, 58, 13, 47, 38, 18, 35, 54, 22, 51, 30, 19, 59, 34, 14, 32, 44, 4, 60, 15, 52, 62, 20, 43, 30, 35, 21, 60, 4, 52, 12, 24, 61, 18, 30, 42, 23, 61, 25, 50, 27, 38, 11, 59, 12, 35, 50, 30, 59, 24, 8, 42, 28, 37, 48, 9, 44, 21 },
  { 10, 47, 15, 50, 30, 43, 8, 45, 29, 2, 36, 59, 1, 58, 41, 3, 63, 31, 54, 20, 13, 55, 35, 38, 4, 44, 15, 9, 61, 2, 14, 38, 61, 10, 23, 54, 18, 12, 24, 2, 14, 55, 16, 8, 38, 14, 41, 60, 10, 23, 1, 58, 32, 17, 28, 37, 41, 15, 3, 60, 15, 33, 4, 36, 16, 59, 28, 14, 23, 55, 37, 18, 44, 28, 2, 57, 30, 10, 27, 46, 14, 38, 3, 53, 21, 61, 17, 35, 10, 41, 26, 7, 33, 9, 57, 1, 53, 37, 26, 20, 56, 48, 9, 33, 58, 16, 37, 7, 45, 1, 57, 15, 32, 26, 42, 23, 7, 20, 4, 54, 31, 62, 22, 1, 59, 30, 4, 51 },
  { 36, 2, 38, 11, 24, 36, 54, 22, 62, 47, 25, 8, 28, 45, 16, 38, 12, 43, 9, 37, 49, 3, 23, 52, 18, 30, 50, 33, 19, 42, 49, 26, 6, 40, 47, 35, 63, 38, 50, 33, 60, 26, 36, 47, 24, 57, 6, 26, 39, 63, 19, 44, 14, 46, 61, 9, 50, 30, 45, 23, 10, 50, 44, 8, 31, 54, 6, 46, 36, 4, 30, 54, 8, 52, 22, 41, 4, 60, 40, 0, 58, 24, 45, 10, 37, 1, 48, 30, 56, 17, 38, 48, 24, 47, 19, 39, 14, 8, 45, 32, 2, 34, 27, 44, 4, 52, 11, 56, 31, 21, 40, 19, 44, 51, 2, 63, 46, 58, 36, 43, 14, 5, 50, 38, 14, 56, 40, 23 },
  { 61, 46, 32, 63, 54, 1, 14, 34, 12, 40, 18, 49, 37, 10, 61, 30, 51, 24, 60, 7, 29, 40, 62, 11, 46, 58, 6, 56, 24, 10, 34, 52, 21, 59, 16, 3, 27, 5, 20, 46, 9, 40, 7, 62, 2, 30, 53, 15, 48, 10, 28, 35, 54, 6, 21, 34, 18, 55, 7, 40, 57, 19, 26, 60, 41, 13, 24, 51, 19, 61, 9, 25, 34, 15, 63, 11, 45, 17, 20, 47, 33, 8, 31, 62, 43, 26, 53, 7, 24, 59, 0, 13, 55, 4, 62, 27, 51, 31, 63, 15, 58, 7, 54, 14, 46, 22, 28, 43, 12, 63, 8, 54, 5, 17, 39, 33, 15, 10, 27, 17, 47, 34, 19, 45, 27, 12, 33, 17 },
  { 5, 28, 21, 7, 17, 48, 42, 58, 23, 4, 63, 14, 55, 21, 34, 5, 19, 0, 45, 17, 52, 15, 25, 32, 0, 22, 40, 13, 45, 62, 18, 0, 43, 11, 33, 55, 30, 42, 57, 19, 51, 31, 22, 43, 18, 45, 34, 0, 43, 31, 56, 3, 23, 40, 59, 0, 44, 13, 48, 35, 2, 32, 46, 0, 21, 48, 35, 3, 40, 32, 43, 59, 0, 48, 33, 26, 53, 36, 55, 12, 51, 16, 55, 5, 18, 29, 11, 39, 51, 19, 45, 31, 42, 21, 35, 6, 22, 47, 10, 38, 23, 50, 20, 36, 0, 60, 38, 4, 50, 35, 48, 34, 24, 57, 9, 53, 28, 48, 61, 0, 56, 24, 53, 3, 63, 6, 42, 57 },
  { 13, 53, 45, 40, 58, 27, 6, 16, 38, 51, 33, 30, 43, 2, 47, 56, 40, 50, 33, 57, 27, 5, 47, 42, 60, 36, 16, 54, 28, 4, 37, 57, 28, 51, 22, 8, 45, 14, 6, 39, 0, 54, 11, 59, 28, 12, 50, 21, 61, 13, 19, 38, 49, 11, 25, 37, 58, 29, 22, 63, 14, 56, 12, 53, 30, 63, 9, 57, 26, 12, 47, 16, 23, 39, 50, 6, 31, 2, 25, 6, 28, 41, 36, 22, 50, 57, 42, 3, 34, 8, 28, 61, 11, 50, 16, 54, 41, 0, 55, 43, 5, 29, 41, 63, 25, 16, 53, 18, 26, 10, 21, 0, 61, 30, 41, 22, 3, 38, 20, 39, 29, 8, 41, 16, 36, 52, 22, 19 },
  { 55, 34, 0, 25, 10, 32, 56, 44, 28, 0, 57, 7, 26, 53, 23, 8, 13, 35, 22, 12, 36, 60, 20, 8, 14, 29, 48, 2, 41, 49, 23, 13, 39, 7, 48, 58, 25, 53, 34, 62, 28, 16, 48, 4, 37, 56, 27, 5, 36, 52, 46, 7, 62, 33, 52, 11, 17, 53, 5, 28, 41, 24, 38, 17, 5, 39, 20, 45, 15, 56, 5, 38, 60, 8, 14, 57, 21, 48, 62, 39, 59, 13, 1, 60, 9, 32, 16, 63, 44, 25, 52, 15, 36, 2, 60, 29, 12, 33, 25, 17, 59, 45, 13, 8, 49, 32, 6, 40, 59, 29, 45, 37, 13, 47, 6, 55, 30, 45, 9, 52, 13, 59, 25, 47, 32, 1, 49, 30 },
  { 9, 39, 14, 61, 49, 37, 3, 20, 50, 13, 41, 19, 46, 17, 38, 59, 28, 62, 4, 44, 54, 1, 34, 51, 55, 7, 63, 32, 21, 8, 56, 31, 62, 19, 36, 1, 41, 17, 24, 12, 42, 35, 25, 52, 20, 8, 44, 59, 25, 2, 22, 42, 16, 29, 4, 46, 20, 36, 43, 9, 51, 8, 49, 26, 58, 33, 54, 1, 37, 29, 52, 20, 27, 45, 19, 35, 42, 16, 10, 32, 20, 49, 46, 27, 40, 4, 47, 22, 13, 55, 4, 47, 26, 44, 23, 40, 58, 19, 48, 13, 31, 2, 57, 34, 42, 19, 61, 32, 14, 55, 5, 51, 26, 19, 58, 16, 49, 14, 62, 5, 33, 44, 21, 7, 60, 26, 11, 41 },
  { 62, 24, 47, 29, 8, 19, 53, 11, 60, 24, 32, 61, 4, 55, 31, 2, 49, 16, 39, 9, 31, 24, 43, 17, 26, 38, 11, 25, 58, 43, 12, 35, 3, 46, 15, 32, 63, 4, 49, 56, 2, 60, 10, 32, 63, 17, 39, 12, 55, 30, 57, 9, 48, 55, 39, 24, 60, 2, 58, 31, 19, 61, 34, 3, 42, 11, 22, 46, 7, 61, 10, 42, 3, 55, 32, 1, 58, 28, 44, 54, 4, 34, 23, 15, 56, 20, 37, 58, 6, 30, 38, 18, 63, 9, 32, 5, 51, 3, 62, 37, 52, 18, 39, 23, 3, 51, 9, 47, 1, 23, 43, 15, 60, 35, 11, 40, 1, 36, 31, 26, 57, 2, 37, 54, 18, 44, 58, 16 },
  { 5, 51, 3, 33, 43, 62, 21, 42, 35, 9, 48, 15, 36, 10, 22, 42, 20, 46, 26, 56, 50, 12, 59, 3, 48, 19, 45, 53, 1, 27, 47, 17, 52, 24, 56, 11, 51, 21, 37, 30, 20, 46, 14, 41, 1, 47, 33, 7, 41, 17, 35, 27, 20, 1, 14, 54, 26, 33, 18, 47, 1, 44, 14, 59, 16, 52, 28, 18, 49, 31, 25, 34, 63, 13, 51, 24, 9, 50, 3, 23, 38, 63, 7, 52, 29, 46, 11, 33, 50, 22, 57, 36, 1, 57, 49, 17, 39, 28, 9, 35, 6, 27, 53, 15, 55, 30, 24, 58, 36, 41, 11, 52, 32, 3, 44, 25, 62, 23, 51, 15, 42, 22, 50, 10, 39, 4, 31, 35 },
  { 46, 22, 57, 17, 12, 39, 26, 5, 31, 59, 1, 45, 27, 62, 52, 7, 58, 33, 6, 18, 39, 22, 33, 41, 57, 5, 35, 18, 40, 16, 60, 5, 29, 42, 7, 39, 27, 44, 9, 47, 8, 26, 54, 22, 51, 29, 24, 49, 15, 61, 4, 51, 31, 63, 43, 6, 50, 8, 39, 12, 53, 37, 23, 30, 40, 6, 62, 43, 14, 53, 2, 49, 7, 36, 17, 41, 61, 37, 18, 56, 11, 18, 44, 35, 2, 19, 61, 0, 41, 14, 8, 30, 43, 12, 24, 46, 14, 54, 42, 21, 44, 61, 10, 46, 37, 11, 44, 7, 18, 63, 20, 29, 7, 49, 28, 54, 8, 43, 4, 48, 18, 63, 12, 29, 48, 24, 59, 20 },
  { 13, 36, 28, 54, 35, 2, 56, 46, 16, 49, 22, 40, 11, 34, 14, 43, 29, 12, 63, 48, 2, 61, 7, 15, 28, 30, 50, 9, 61, 33, 38, 23, 54, 13, 61, 33, 3, 59, 16, 35, 58, 40, 5, 38, 13, 57, 3, 58, 37, 21, 45, 12, 39, 7, 35, 30, 13, 56, 22, 62, 27, 6, 55, 10, 48, 21, 33, 2, 38, 23, 40, 20, 44, 29, 59, 4, 26, 12, 33, 47, 28, 53, 31, 13, 59, 41, 27, 49, 26, 54, 45, 16, 53, 21, 35, 7, 59, 26, 11, 56, 1, 24, 33, 4, 28, 62, 21, 49, 31, 2, 56, 39, 24, 58, 13, 17, 37, 21, 56, 10, 38, 0, 34, 55, 15, 43, 1, 52 },
  { 42, 9, 50, 6, 25, 60, 14, 38, 10, 29, 53, 18, 57, 3, 25, 51, 0, 53, 25, 17, 29, 37, 52, 46, 0, 62, 14, 37, 4, 50, 10, 44, 0, 46, 20, 25, 50, 19, 55, 0, 23, 31, 62, 34, 11, 45, 19, 32, 0, 53, 10, 59, 23, 47, 18, 60, 42, 28, 37, 3, 50, 15, 35, 44, 0, 51, 27, 60, 9, 57, 16, 58, 11, 22, 46, 15, 53, 48, 7, 42, 0, 60, 5, 49, 24, 54, 9, 17, 39, 5, 34, 62, 3, 40, 60, 31, 0, 47, 29, 16, 49, 39, 59, 17, 50, 0, 40, 13, 53, 38, 16, 46, 0, 42, 34, 60, 2, 53, 29, 31, 58, 46, 27, 6, 61, 8, 37, 28 },
  { 0, 63, 21, 40, 45, 18, 51, 23, 63, 34, 6, 43, 28, 38, 55, 19, 40, 35, 8, 41, 54, 10, 21, 32, 39, 23, 53, 26, 55, 28, 22, 63, 30, 34, 9, 48, 6, 38, 29, 43, 49, 6, 18, 52, 27, 61, 9, 43, 28, 42, 33, 26, 56, 3, 51, 23, 0, 48, 16, 45, 32, 25, 63, 20, 57, 17, 42, 12, 35, 47, 5, 31, 39, 56, 6, 30, 34, 21, 61, 25, 14, 40, 22, 38, 15, 6, 36, 56, 20, 60, 25, 12, 51, 27, 10, 56, 42, 20, 36, 63, 32, 6, 21, 41, 12, 34, 60, 26, 5, 48, 27, 10, 62, 19, 6, 47, 39, 14, 45, 7, 24, 17, 41, 32, 23, 51, 19, 56 },
  { 45, 31, 15, 59, 4, 33, 7, 47, 0, 41, 13, 61, 4, 47, 9, 23, 60, 14, 57, 31, 4, 45, 59, 6, 58, 10, 44, 20, 8, 42, 15, 6, 55, 17, 58, 31, 53, 12, 61, 10, 15, 57, 43, 2, 23, 35, 48, 14, 54, 6, 18, 49, 15, 38, 11, 34, 62, 9, 21, 58, 11, 41, 4, 31, 38, 8, 29, 55, 19, 36, 27, 52, 0, 25, 50, 43, 1, 39, 8, 55, 35, 51, 10, 30, 45, 62, 29, 2, 46, 10, 32, 48, 18, 38, 5, 22, 33, 8, 51, 3, 14, 44, 54, 25, 57, 30, 18, 52, 33, 22, 59, 28, 36, 52, 32, 21, 26, 50, 5, 55, 35, 60, 14, 54, 4, 40, 16, 33 },
  { 27, 3, 49, 10, 30, 40, 55, 27, 57, 24, 52, 21, 32, 17, 60, 30, 5, 44, 27, 49, 19, 34, 13, 24, 43, 36, 3, 49, 31, 59, 37, 48, 26, 41, 2, 41, 14, 36, 21, 32, 40, 26, 13, 49, 55, 5, 16, 40, 25, 60, 36, 1, 63, 29, 17, 44, 25, 40, 52, 5, 29, 47, 54, 13, 46, 24, 60, 4, 51, 22, 63, 14, 45, 18, 12, 62, 17, 57, 19, 42, 3, 26, 58, 48, 1, 21, 40, 52, 23, 37, 44, 1, 29, 58, 43, 50, 15, 61, 19, 45, 58, 28, 7, 48, 2, 46, 8, 42, 3, 55, 8, 50, 12, 4, 55, 10, 63, 33, 20, 40, 11, 3, 46, 20, 48, 26, 61, 11 },
  { 44, 56, 24, 36, 53, 19, 12, 37, 16, 44, 7, 36, 49, 54, 11, 37, 48, 21, 15, 1, 62, 25, 47, 56, 16, 18, 51, 12, 40, 1, 24, 11, 52, 16, 23, 59, 28, 1, 45, 53, 4, 60, 37, 21, 39, 30, 63, 20, 52, 10, 30, 45, 8, 41, 54, 4, 57, 7, 34, 55, 36, 18, 23, 59, 2, 48, 11, 32, 44, 1, 41, 8, 33, 54, 38, 23, 30, 46, 6, 29, 62, 18, 32, 16, 55, 34, 14, 11, 61, 7, 55, 16, 53, 13, 23, 2, 55, 37, 26, 10, 33, 23, 36, 16, 38, 22, 56, 15, 24, 43, 35, 17, 44, 40, 25, 46, 16, 1, 57, 25, 49, 36, 28, 62, 9, 35, 7, 53 },
  { 17, 38, 8, 61, 1, 50, 26, 62, 3, 31, 56, 15, 1, 26, 40, 2, 34, 51, 56, 36, 42, 9, 38, 2, 29, 60, 32, 57, 19, 62, 34, 47, 4, 57, 39, 7, 44, 63, 24, 18, 46, 28, 8, 54, 1, 34, 7, 46, 3, 37, 50, 23, 57, 21, 13, 46, 31, 20, 43, 15, 1, 61, 8, 33, 37, 17, 56, 26, 15, 49, 24, 59, 28, 3, 56, 9, 52, 32, 13, 49, 10, 43, 5, 45, 8, 25, 59, 42, 28, 33, 19, 40, 8, 63, 35, 47, 25, 4, 40, 52, 1, 60, 12, 53, 63, 9, 29, 60, 37, 19, 1, 62, 31, 20, 58, 12, 41, 30, 43, 9, 18, 52, 22, 1, 39, 30, 58, 21 },
  { 13, 47, 29, 18, 43, 34, 5, 48, 20, 42, 10, 45, 30, 58, 20, 63, 24, 11, 6, 28, 54, 14, 22, 52, 41, 7, 26, 5, 45, 15, 53, 13, 35, 27, 18, 50, 12, 33, 5, 56, 10, 17, 45, 24, 59, 15, 50, 26, 56, 13, 19, 5, 32, 52, 27, 36, 2, 61, 12, 26, 49, 40, 27, 52, 13, 50, 6, 39, 61, 34, 10, 37, 48, 20, 41, 27, 2, 36, 59, 24, 54, 33, 63, 20, 38, 50, 3, 17, 52, 4, 58, 27, 45, 21, 32, 11, 48, 17, 57, 20, 46, 38, 25, 43, 4, 34, 51, 6, 13, 45, 57, 26, 6, 48, 2, 35, 53, 23, 61, 34, 59, 6, 42, 56, 13, 51, 2, 41 },
  { 32, 5, 55, 23, 58, 14, 22, 52, 29, 15, 61, 25, 51, 8, 43, 13, 53, 41, 46, 20, 3, 33, 63, 11, 48, 21, 54, 38, 28, 3, 30, 43, 21, 62, 9, 31, 55, 22, 51, 29, 37, 62, 32, 12, 42, 29, 41, 9, 33, 44, 62, 28, 43, 1, 59, 19, 48, 30, 51, 39, 24, 4, 58, 19, 42, 29, 22, 43, 3, 18, 53, 5, 13, 50, 16, 60, 45, 21, 7, 40, 15, 0, 26, 53, 13, 31, 43, 24, 47, 31, 15, 49, 2, 41, 6, 59, 29, 42, 9, 30, 14, 7, 49, 18, 31, 47, 20, 39, 49, 32, 11, 41, 54, 15, 61, 18, 7, 38, 4, 13, 44, 28, 15, 32, 45, 19, 27, 49 },
  { 63, 34, 11, 39, 2, 45, 37, 8, 59, 39, 33, 4, 36, 17, 48, 5, 29, 18, 32, 61, 39, 50, 5, 27, 35, 0, 46, 12, 22, 49, 60, 6, 54, 0, 38, 49, 2, 42, 15, 40, 0, 47, 20, 51, 3, 57, 18, 61, 22, 0, 39, 16, 55, 12, 35, 8, 41, 22, 6, 59, 16, 45, 10, 36, 0, 62, 9, 54, 30, 58, 21, 43, 63, 31, 7, 35, 12, 48, 58, 28, 47, 37, 41, 9, 57, 20, 61, 0, 36, 11, 57, 35, 23, 52, 37, 18, 0, 62, 22, 55, 35, 62, 27, 54, 0, 15, 61, 28, 2, 59, 22, 9, 37, 27, 33, 51, 29, 48, 19, 50, 25, 37, 10, 57, 5, 37, 60, 8 },
  { 20, 25, 46, 52, 31, 60, 12, 55, 0, 19, 11, 46, 62, 35, 23, 38, 57, 0, 55, 10, 16, 30, 58, 44, 17, 59, 29, 63, 42, 8, 36, 20, 33, 46, 16, 61, 25, 35, 8, 54, 26, 7, 58, 22, 34, 6, 47, 14, 53, 31, 48, 9, 37, 25, 49, 63, 16, 55, 45, 14, 34, 63, 21, 53, 25, 33, 46, 16, 35, 7, 46, 29, 0, 39, 25, 55, 22, 34, 18, 4, 56, 11, 23, 51, 28, 6, 39, 14, 62, 44, 19, 8, 60, 12, 56, 28, 50, 34, 39, 5, 51, 3, 41, 12, 57, 35, 10, 53, 25, 17, 52, 30, 47, 0, 43, 14, 5, 57, 31, 55, 0, 63, 47, 23, 54, 24, 14, 43 },
  { 0, 57, 16, 6, 26, 19, 35, 28, 49, 42, 54, 26, 21, 1, 59, 27, 9, 47, 26, 44, 50, 22, 13, 40, 8, 37, 10, 34, 17, 56, 25, 58, 13, 27, 44, 9, 20, 58, 31, 17, 60, 36, 10, 41, 53, 25, 36, 39, 4, 24, 58, 17, 60, 4, 22, 38, 10, 32, 0, 50, 31, 7, 28, 47, 12, 57, 5, 26, 52, 23, 14, 40, 57, 17, 47, 5, 53, 1, 44, 31, 19, 60, 46, 2, 35, 48, 30, 54, 22, 5, 51, 39, 25, 31, 4, 43, 14, 9, 45, 16, 24, 44, 19, 29, 40, 23, 44, 7, 38, 42, 4, 63, 12, 54, 23, 59, 22, 42, 8, 15, 40, 21, 8, 34, 3, 41, 30, 50 },
  { 39, 10, 48, 33, 41, 54, 5, 47, 23, 13, 32, 7, 52, 44, 14, 39, 58, 18, 35, 6, 37, 2, 60, 24, 55, 19, 53, 2, 51, 32, 1, 41, 51, 4, 40, 29, 47, 3, 52, 44, 13, 49, 28, 16, 1, 62, 11, 27, 52, 35, 5, 42, 29, 47, 14, 56, 28, 53, 26, 38, 9, 56, 40, 3, 38, 15, 41, 60, 1, 37, 50, 25, 11, 28, 61, 19, 42, 62, 10, 52, 39, 6, 32, 14, 58, 17, 7, 26, 42, 34, 27, 10, 54, 40, 20, 63, 26, 53, 21, 61, 32, 7, 59, 48, 3, 56, 18, 31, 58, 14, 49, 21, 36, 16, 45, 9, 36, 24, 62, 45, 27, 31, 53, 17, 49, 12, 62, 18 },
  { 28, 59, 21, 58, 2, 16, 38, 9, 62, 3, 56, 41, 10, 31, 50, 4, 32, 52, 12, 63, 23, 46, 33, 31, 4, 48, 25, 43, 14, 23, 47, 11, 22, 55, 14, 60, 23, 37, 11, 39, 23, 2, 45, 56, 31, 43, 19, 55, 16, 46, 21, 51, 11, 33, 44, 2, 41, 18, 5, 52, 23, 44, 17, 60, 27, 49, 11, 32, 44, 10, 54, 2, 56, 33, 8, 38, 13, 29, 36, 16, 24, 63, 27, 51, 21, 43, 56, 12, 49, 3, 59, 48, 1, 15, 46, 7, 36, 2, 47, 11, 50, 27, 37, 13, 33, 8, 51, 46, 1, 34, 28, 40, 3, 33, 60, 29, 47, 1, 35, 11, 59, 42, 2, 60, 26, 46, 6, 35 },
  { 4, 43, 9, 29, 36, 63, 24, 44, 20, 50, 30, 17, 60, 22, 16, 43, 25, 3, 42, 19, 51, 15, 8, 54, 42, 15, 61, 5, 39, 57, 18, 61, 31, 48, 34, 2, 50, 19, 57, 5, 63, 33, 19, 38, 13, 27, 48, 7, 32, 61, 2, 26, 58, 6, 24, 50, 13, 61, 42, 20, 62, 2, 35, 20, 51, 4, 62, 18, 23, 58, 20, 31, 43, 15, 51, 45, 26, 50, 4, 55, 45, 3, 35, 9, 38, 1, 32, 61, 20, 45, 17, 33, 24, 57, 29, 51, 22, 58, 38, 30, 15, 1, 54, 21, 63, 43, 26, 12, 24, 56, 8, 60, 50, 19, 5, 52, 13, 54, 17, 50, 4, 16, 36, 12, 32, 56, 22, 54 },
  { 51, 25, 40, 53, 12, 49, 15, 57, 34, 7, 38, 47, 2, 36, 55, 8, 61, 30, 56, 7, 28, 59, 48, 11, 27, 35, 21, 45, 28, 36, 9, 38, 6, 16, 24, 63, 10, 32, 28, 43, 21, 53, 5, 60, 8, 57, 3, 45, 11, 37, 15, 54, 40, 20, 62, 36, 27, 34, 11, 48, 30, 15, 54, 8, 30, 42, 22, 34, 48, 13, 35, 63, 4, 37, 22, 2, 59, 9, 41, 23, 13, 41, 49, 18, 59, 24, 40, 5, 37, 30, 9, 61, 44, 6, 37, 11, 33, 17, 5, 55, 41, 60, 23, 39, 17, 5, 30, 62, 41, 16, 46, 25, 11, 56, 39, 26, 20, 38, 29, 39, 22, 52, 44, 20, 48, 1, 38, 14 },
  { 15, 33, 2, 18, 44, 6, 27, 0, 32, 61, 25, 12, 58, 28, 40, 20, 47, 13, 34, 43, 38, 1, 23, 62, 40, 0, 51, 10, 63, 3, 52, 26, 44, 30, 45, 6, 41, 54, 0, 51, 12, 30, 46, 24, 49, 22, 40, 33, 63, 23, 43, 30, 9, 47, 0, 17, 54, 7, 57, 3, 37, 47, 24, 46, 13, 55, 7, 52, 2, 42, 6, 26, 49, 18, 60, 34, 16, 57, 33, 20, 61, 30, 8, 54, 14, 46, 12, 53, 16, 55, 38, 13, 22, 53, 18, 59, 46, 27, 43, 19, 32, 10, 45, 6, 49, 36, 52, 2, 20, 55, 6, 39, 32, 15, 44, 3, 58, 10, 63, 6, 56, 30, 7, 58, 9, 40, 19, 63 },
  { 10, 47, 61, 23, 55, 31, 52, 42, 17, 45, 4, 51, 27, 6, 15, 53, 0, 49, 26, 10, 56, 18, 36, 6, 20, 58, 32, 30, 13, 49, 19, 56, 0, 59, 12, 53, 27, 17, 38, 25, 48, 9, 15, 36, 14, 30, 59, 17, 0, 50, 8, 58, 18, 56, 31, 45, 21, 41, 29, 19, 60, 6, 32, 59, 0, 36, 29, 39, 19, 59, 46, 12, 55, 30, 10, 47, 24, 3, 28, 48, 0, 55, 44, 27, 33, 4, 63, 29, 49, 0, 26, 50, 34, 2, 42, 14, 0, 62, 9, 56, 3, 52, 28, 34, 58, 9, 20, 48, 37, 32, 22, 53, 0, 62, 27, 49, 34, 46, 21, 33, 41, 14, 25, 37, 53, 29, 31, 45 },
  { 56, 28, 7, 37, 11, 36, 20, 9, 54, 14, 39, 19, 34, 63, 45, 37, 24, 17, 60, 31, 21, 45, 53, 29, 47, 15, 7, 55, 40, 23, 34, 14, 42, 20, 37, 35, 15, 59, 7, 62, 34, 40, 59, 1, 51, 42, 10, 28, 54, 21, 35, 5, 38, 13, 36, 4, 59, 12, 39, 53, 15, 43, 9, 21, 39, 62, 16, 56, 25, 9, 32, 38, 0, 41, 14, 51, 40, 53, 43, 11, 37, 17, 5, 22, 57, 39, 19, 7, 42, 21, 60, 10, 31, 63, 25, 52, 30, 49, 36, 25, 48, 17, 61, 14, 22, 42, 29, 13, 60, 11, 47, 18, 35, 41, 7, 23, 4, 16, 51, 11, 0, 48, 61, 3, 17, 50, 5, 24 },
  { 0, 42, 21, 49, 60, 3, 57, 40, 29, 48, 23, 56, 42, 11, 22, 5, 59, 39, 4, 50, 3, 41, 12, 57, 25, 50, 44, 18, 4, 46, 7, 62, 33, 50, 4, 56, 21, 32, 43, 18, 3, 23, 55, 34, 20, 4, 53, 38, 12, 46, 29, 52, 25, 61, 23, 51, 26, 46, 1, 34, 25, 57, 28, 51, 26, 11, 50, 3, 44, 28, 53, 21, 57, 27, 62, 6, 31, 19, 8, 63, 26, 59, 36, 47, 15, 29, 50, 25, 35, 47, 18, 41, 4, 48, 8, 40, 12, 23, 6, 44, 13, 40, 1, 31, 55, 0, 61, 43, 4, 50, 26, 58, 9, 53, 24, 61, 42, 55, 31, 43, 57, 20, 34, 27, 43, 8, 59, 39 },
  { 18, 51, 30, 13, 26, 16, 46, 22, 2, 59, 8, 30, 1, 48, 33, 51, 29, 9, 46, 16, 62, 14, 33, 2, 38, 9, 27, 60, 37, 26, 53, 17, 28, 10, 24, 46, 2, 49, 8, 57, 29, 45, 6, 26, 62, 44, 18, 25, 61, 3, 42, 14, 49, 10, 43, 6, 17, 32, 63, 10, 49, 4, 40, 14, 45, 33, 22, 37, 12, 61, 5, 17, 43, 7, 23, 37, 15, 58, 49, 13, 39, 21, 10, 52, 1, 62, 9, 56, 12, 2, 58, 28, 36, 16, 56, 28, 56, 35, 20, 63, 24, 37, 51, 8, 45, 25, 16, 33, 27, 38, 2, 44, 13, 30, 17, 36, 12, 26, 5, 18, 28, 47, 13, 60, 23, 45, 13, 33 },
  { 55, 4, 62, 34, 52, 38, 7, 63, 32, 37, 13, 53, 25, 62, 18, 12, 55, 41, 27, 35, 24, 49, 31, 52, 17, 63, 34, 1, 56, 12, 41, 2, 48, 58, 39, 16, 61, 27, 41, 52, 13, 19, 50, 39, 11, 31, 57, 6, 32, 40, 20, 55, 1, 28, 33, 57, 48, 8, 37, 22, 44, 18, 53, 1, 61, 5, 54, 16, 47, 36, 50, 24, 55, 34, 48, 45, 1, 30, 33, 46, 2, 50, 32, 42, 25, 34, 43, 21, 38, 52, 23, 45, 14, 54, 21, 4, 44, 16, 53, 29, 10, 47, 19, 57, 12, 54, 39, 10, 51, 15, 63, 21, 57, 40, 51, 1, 48, 57, 37, 62, 2, 38, 9, 52, 1, 35, 58, 22 },
  { 36, 46, 10, 42, 1, 27, 43, 15, 50, 21, 45, 16, 41, 3, 35, 44, 20, 1, 57, 11, 55, 7, 43, 8, 22, 42, 13, 46, 21, 39, 31, 60, 22, 5, 29, 44, 11, 35, 20, 4, 36, 58, 32, 15, 47, 2, 36, 48, 16, 60, 8, 35, 44, 63, 16, 2, 40, 26, 55, 14, 58, 35, 24, 31, 19, 42, 31, 58, 1, 29, 10, 40, 2, 19, 12, 54, 22, 61, 7, 24, 56, 5, 28, 16, 54, 3, 15, 58, 6, 30, 8, 62, 1, 43, 31, 47, 7, 59, 1, 38, 58, 4, 34, 27, 38, 5, 31, 59, 7, 46, 30, 3, 34, 6, 28, 59, 20, 8, 32, 15, 53, 24, 55, 31, 19, 49, 11, 26 },
  { 2, 24, 16, 58, 19, 55, 5, 35, 10, 61, 4, 28, 57, 24, 58, 7, 31, 47, 22, 38, 19, 28, 61, 36, 54, 5, 59, 29, 6, 52, 15, 11, 43, 36, 8, 54, 52, 1, 62, 25, 47, 9, 1, 60, 28, 53, 24, 14, 46, 27, 51, 22, 12, 24, 38, 53, 20, 11, 51, 3, 29, 7, 48, 63, 8, 49, 9, 21, 52, 14, 63, 32, 46, 60, 35, 4, 41, 16, 52, 35, 18, 42, 59, 7, 36, 61, 45, 27, 33, 51, 19, 39, 34, 11, 61, 18, 33, 41, 28, 15, 54, 22, 42, 3, 49, 21, 47, 18, 36, 23, 55, 19, 48, 24, 45, 10, 33, 44, 50, 40, 7, 35, 15, 41, 63, 6, 40, 54 },
  { 62, 41, 32, 8, 47, 28, 60, 24, 44, 30, 38, 49, 9, 33, 14, 40, 50, 14, 60, 2, 54, 40, 0, 20, 25, 39, 16, 49, 24, 35, 57, 47, 19, 61, 33, 18, 23, 37, 13, 55, 31, 43, 22, 41, 17, 8, 42, 58, 0, 37, 5, 56, 31, 54, 7, 30, 60, 33, 42, 17, 59, 39, 12, 27, 38, 17, 35, 41, 27, 45, 20, 7, 25, 15, 29, 58, 27, 47, 11, 40, 14, 54, 23, 46, 19, 31, 11, 40, 13, 49, 5, 58, 24, 51, 26, 6, 50, 20, 49, 9, 32, 46, 17, 60, 14, 63, 24, 1, 57, 41, 9, 43, 14, 62, 16, 52, 3, 27, 14, 22, 61, 45, 4, 28, 9, 47, 29, 17 },
  { 5, 50, 12, 53, 38, 18, 11, 51, 0, 55, 17, 6, 47, 54, 19, 63, 5, 26, 34, 45, 13, 30, 47, 58, 10, 48, 32, 3, 62, 9, 26, 0, 25, 14, 50, 3, 47, 30, 42, 16, 6, 63, 12, 49, 33, 55, 21, 10, 34, 63, 18, 41, 3, 47, 19, 43, 0, 49, 8, 28, 46, 20, 52, 0, 56, 24, 60, 3, 59, 5, 39, 57, 48, 52, 9, 38, 3, 21, 26, 60, 0, 32, 12, 38, 4, 48, 53, 0, 60, 15, 29, 44, 18, 10, 38, 57, 13, 60, 2, 26, 62, 7, 50, 29, 35, 8, 40, 53, 28, 12, 60, 33, 38, 5, 37, 29, 60, 39, 56, 0, 30, 18, 50, 34, 59, 25, 14, 44 },
  { 20, 31, 60, 22, 3, 49, 33, 25, 40, 13, 34, 59, 22, 36, 0, 28, 37, 56, 8, 18, 51, 16, 4, 45, 27, 12, 53, 42, 18, 44, 51, 31, 55, 40, 28, 58, 7, 60, 10, 51, 27, 37, 24, 56, 5, 26, 44, 29, 50, 23, 45, 11, 34, 15, 59, 27, 13, 23, 62, 37, 4, 57, 15, 32, 42, 6, 47, 11, 30, 43, 23, 13, 0, 36, 18, 44, 63, 51, 37, 29, 49, 20, 57, 27, 62, 9, 24, 35, 23, 53, 37, 3, 42, 55, 0, 36, 23, 39, 31, 43, 17, 37, 24, 11, 52, 43, 19, 32, 5, 50, 26, 0, 56, 21, 54, 11, 19, 6, 47, 25, 59, 42, 12, 54, 21, 3, 38, 57 },
  { 48, 0, 35, 27, 44, 14, 59, 7, 57, 46, 26, 2, 42, 12, 52, 43, 10, 27, 53, 42, 32, 62, 37, 21, 34, 61, 7, 23, 36, 4, 38, 12, 41, 5, 17, 45, 22, 27, 39, 21, 59, 0, 45, 18, 39, 62, 3, 38, 14, 7, 54, 26, 61, 39, 9, 52, 45, 36, 18, 50, 10, 34, 44, 22, 50, 14, 36, 55, 17, 34, 53, 62, 33, 26, 56, 6, 31, 12, 6, 53, 9, 44, 2, 50, 20, 40, 55, 17, 47, 7, 26, 63, 22, 32, 48, 16, 46, 8, 52, 12, 57, 41, 0, 56, 25, 3, 61, 14, 45, 35, 18, 44, 12, 46, 23, 42, 32, 51, 35, 10, 17, 36, 23, 1, 45, 52, 32, 10 },
  { 37, 15, 43, 8, 63, 39, 21, 31, 16, 37, 19, 62, 30, 46, 17, 60, 21, 48, 1, 23, 6, 25, 11, 56, 1, 40, 30, 58, 15, 54, 21, 59, 9, 63, 35, 56, 11, 51, 2, 46, 34, 14, 53, 7, 30, 11, 51, 19, 60, 40, 30, 1, 24, 50, 20, 32, 3, 56, 5, 25, 31, 13, 61, 2, 29, 60, 25, 20, 51, 2, 27, 8, 18, 42, 10, 45, 21, 34, 43, 17, 62, 29, 41, 14, 34, 6, 30, 43, 2, 57, 33, 13, 45, 12, 27, 62, 4, 55, 21, 35, 5, 27, 45, 33, 16, 47, 30, 54, 22, 10, 51, 27, 63, 7, 49, 1, 58, 22, 15, 43, 53, 7, 57, 39, 27, 12, 61, 24 },
  { 56, 51, 26, 56, 19, 2, 41, 54, 5, 52, 9, 48, 6, 23, 39, 4, 32, 15, 63, 35, 59, 49, 43, 15, 52, 19, 50, 9, 46, 33, 1, 29, 48, 20, 32, 1, 38, 33, 19, 54, 9, 32, 24, 48, 58, 35, 16, 48, 4, 52, 13, 57, 33, 5, 45, 59, 15, 29, 41, 55, 47, 39, 23, 53, 9, 40, 4, 57, 10, 44, 48, 40, 50, 14, 61, 24, 55, 1, 59, 22, 33, 8, 51, 25, 58, 46, 11, 59, 20, 41, 17, 51, 6, 56, 35, 25, 42, 30, 15, 58, 48, 18, 61, 9, 58, 39, 13, 2, 37, 59, 40, 2, 31, 16, 34, 41, 8, 30, 62, 3, 29, 48, 33, 5, 63, 16, 41, 7 },
  { 22, 4, 46, 11, 33, 51, 29, 10, 62, 24, 43, 27, 15, 58, 50, 25, 54, 44, 9, 38, 18, 3, 29, 57, 32, 5, 26, 43, 17, 61, 24, 52, 8, 42, 23, 53, 15, 61, 7, 28, 57, 43, 4, 40, 20, 2, 43, 25, 32, 35, 21, 43, 17, 48, 10, 22, 38, 54, 11, 21, 1, 58, 16, 30, 48, 18, 46, 32, 38, 13, 22, 4, 59, 35, 2, 51, 30, 39, 15, 47, 4, 56, 13, 37, 1, 28, 16, 52, 32, 9, 61, 29, 38, 19, 3, 52, 10, 48, 1, 32, 11, 40, 20, 36, 6, 22, 49, 29, 55, 6, 20, 56, 36, 52, 19, 60, 26, 46, 18, 54, 40, 13, 20, 46, 35, 19, 49, 29 },
  { 61, 17, 34, 53, 23, 6, 48, 35, 20, 40, 1, 56, 36, 29, 11, 34, 7, 41, 14, 30, 55, 20, 46, 8, 24, 38, 63, 2, 37, 10, 45, 14, 34, 49, 6, 13, 44, 25, 49, 41, 21, 12, 61, 15, 54, 29, 63, 12, 56, 8, 49, 2, 62, 36, 28, 61, 0, 25, 41, 63, 35, 8, 44, 6, 37, 62, 7, 21, 63, 28, 55, 31, 16, 24, 41, 19, 9, 57, 27, 36, 18, 42, 31, 62, 22, 55, 38, 4, 27, 47, 1, 40, 14, 54, 43, 20, 60, 23, 38, 63, 25, 51, 2, 53, 26, 63, 10, 42, 17, 34, 47, 25, 13, 5, 44, 11, 55, 2, 38, 27, 6, 60, 52, 25, 9, 55, 1, 40 },
  { 8, 30, 58, 3, 42, 61, 17, 38, 13, 59, 32, 10, 54, 3, 51, 20, 61, 26, 57, 2, 46, 33, 12, 60, 41, 13, 48, 29, 55, 20, 39, 27, 57, 18, 62, 29, 55, 2, 31, 16, 37, 50, 26, 36, 6, 46, 9, 41, 27, 57, 23, 39, 26, 6, 51, 12, 31, 46, 7, 16, 27, 52, 19, 56, 26, 12, 33, 53, 1, 41, 8, 57, 46, 7, 54, 32, 47, 5, 49, 11, 60, 23, 5, 48, 10, 43, 19, 63, 35, 24, 49, 21, 59, 5, 31, 37, 14, 44, 7, 42, 6, 30, 46, 13, 44, 32, 19, 50, 4, 58, 8, 30, 62, 38, 28, 53, 21, 36, 13, 50, 21, 33, 15, 2, 44, 31, 14, 47 },
  { 37, 13, 39, 16, 28, 9, 57, 0, 25, 49, 21, 45, 18, 47, 12, 42, 0, 49, 22, 39, 16, 53, 25, 36, 0, 52, 22, 16, 6, 60, 4, 51, 0, 26, 37, 47, 10, 36, 63, 5, 57, 0, 18, 59, 23, 33, 51, 19, 0, 44, 15, 11, 54, 17, 42, 35, 53, 18, 58, 33, 49, 4, 34, 42, 0, 50, 43, 25, 16, 49, 34, 20, 37, 28, 12, 63, 16, 38, 25, 44, 0, 40, 52, 17, 35, 3, 50, 14, 8, 53, 11, 36, 25, 45, 9, 62, 0, 54, 28, 17, 50, 55, 15, 24, 57, 0, 53, 34, 23, 41, 15, 45, 0, 49, 16, 4, 48, 9, 63, 45, 0, 42, 58, 37, 61, 22, 54, 26 },
  { 0, 50, 21, 47, 54, 36, 27, 45, 52, 4, 34, 15, 63, 29, 37, 59, 17, 31, 6, 61, 28, 5, 48, 18, 59, 27, 34, 56, 44, 31, 35, 12, 41, 59, 16, 3, 40, 20, 50, 22, 30, 40, 52, 10, 45, 3, 59, 22, 37, 61, 29, 46, 31, 58, 2, 22, 9, 43, 3, 39, 14, 61, 24, 54, 15, 29, 11, 60, 39, 17, 5, 61, 0, 44, 50, 3, 31, 14, 58, 21, 54, 28, 15, 45, 60, 26, 33, 58, 44, 22, 60, 2, 57, 34, 49, 27, 18, 34, 21, 59, 29, 4, 36, 41, 8, 39, 28, 11, 62, 26, 53, 20, 35, 24, 59, 32, 29, 39, 24, 31, 57, 23, 11, 28, 5, 36, 11, 59 },
  { 44, 32, 63, 5, 20, 12, 41, 7, 30, 61, 42, 8, 39, 5, 33, 8, 24, 53, 45, 11, 37, 58, 7, 44, 10, 50, 3, 40, 8, 22, 53, 19, 46, 9, 33, 52, 24, 58, 8, 44, 13, 47, 8, 34, 38, 30, 14, 47, 7, 34, 4, 55, 9, 19, 40, 49, 56, 26, 60, 21, 30, 45, 10, 19, 40, 58, 23, 36, 3, 52, 45, 23, 54, 13, 22, 42, 53, 45, 7, 33, 10, 36, 57, 6, 29, 12, 41, 0, 30, 15, 41, 30, 17, 7, 16, 53, 40, 56, 2, 39, 12, 61, 10, 52, 31, 60, 16, 45, 1, 37, 7, 61, 40, 10, 43, 17, 58, 7, 54, 14, 4, 51, 39, 49, 18, 56, 42, 20 },
  { 14, 6, 24, 36, 56, 49, 22, 60, 18, 14, 23, 51, 26, 57, 21, 52, 41, 14, 35, 50, 19, 31, 40, 23, 33, 14, 63, 17, 32, 47, 7, 62, 23, 30, 56, 11, 42, 27, 14, 60, 35, 19, 28, 61, 17, 55, 25, 39, 53, 17, 42, 21, 38, 63, 25, 5, 14, 36, 12, 50, 1, 37, 59, 32, 2, 51, 6, 56, 27, 32, 11, 30, 38, 26, 60, 8, 26, 19, 62, 39, 50, 2, 21, 39, 53, 23, 56, 19, 49, 39, 5, 46, 55, 23, 42, 4, 31, 11, 47, 26, 45, 22, 48, 18, 21, 5, 48, 25, 57, 14, 47, 30, 3, 56, 12, 50, 1, 42, 19, 47, 35, 17, 8, 30, 45, 25, 4, 51 },
  { 28, 58, 43, 1, 31, 8, 33, 2, 44, 55, 32, 1, 60, 12, 46, 27, 4, 62, 23, 1, 56, 13, 62, 2, 54, 36, 25, 51, 1, 57, 26, 42, 3, 49, 17, 38, 1, 48, 31, 4, 54, 3, 50, 24, 1, 49, 5, 63, 13, 27, 52, 1, 48, 13, 45, 33, 52, 30, 46, 20, 55, 28, 6, 48, 24, 38, 20, 47, 14, 62, 48, 9, 58, 4, 36, 30, 56, 1, 34, 12, 18, 63, 25, 48, 4, 16, 37, 7, 62, 10, 52, 28, 13, 50, 36, 63, 24, 51, 15, 58, 8, 33, 1, 38, 56, 35, 42, 9, 33, 51, 22, 18, 48, 32, 27, 37, 23, 61, 33, 11, 59, 29, 62, 1, 53, 10, 60, 33 },
  { 12, 39, 17, 52, 26, 46, 53, 38, 25, 11, 48, 36, 16, 43, 2, 35, 55, 17, 39, 29, 43, 9, 28, 45, 20, 5, 46, 12, 42, 28, 13, 52, 36, 6, 60, 22, 54, 17, 62, 39, 25, 42, 15, 55, 44, 20, 31, 10, 35, 57, 24, 32, 29, 6, 59, 18, 7, 62, 3, 41, 10, 44, 16, 54, 13, 62, 31, 9, 41, 1, 21, 43, 18, 47, 15, 40, 11, 49, 28, 55, 46, 30, 8, 43, 32, 61, 28, 47, 25, 34, 21, 61, 32, 1, 20, 9, 46, 6, 35, 19, 41, 54, 27, 63, 14, 3, 51, 20, 62, 2, 38, 55, 8, 21, 63, 6, 46, 9, 26, 51, 3, 24, 43, 34, 16, 41, 18, 48 },
  { 62, 23, 55, 9, 15, 62, 19, 13, 58, 40, 6, 30, 54, 19, 50, 31, 10, 44, 6, 59, 21, 47, 51, 15, 60, 39, 30, 54, 21, 61, 19, 33, 14, 29, 43, 11, 34, 45, 7, 21, 10, 56, 36, 6, 38, 11, 58, 42, 2, 47, 11, 60, 50, 16, 41, 28, 38, 23, 47, 17, 35, 63, 22, 33, 42, 5, 45, 17, 53, 35, 25, 56, 33, 6, 51, 19, 60, 23, 43, 15, 5, 40, 58, 13, 51, 1, 45, 11, 54, 3, 43, 8, 37, 48, 59, 29, 39, 21, 61, 43, 3, 31, 10, 44, 24, 29, 60, 12, 28, 40, 11, 25, 43, 52, 14, 41, 16, 57, 44, 20, 40, 55, 12, 21, 57, 27, 35, 2 },
  { 37, 6, 31, 42, 40, 4, 29, 50, 0, 20, 63, 28, 9, 58, 14, 24, 63, 26, 48, 16, 34, 4, 32, 38, 23, 11, 58, 4, 37, 9, 45, 5, 63, 48, 26, 57, 2, 28, 32, 51, 46, 29, 13, 62, 27, 46, 28, 18, 50, 15, 40, 4, 19, 34, 54, 0, 53, 9, 26, 58, 28, 5, 49, 0, 57, 27, 19, 60, 29, 8, 59, 12, 37, 63, 24, 46, 3, 37, 6, 52, 26, 32, 20, 36, 9, 22, 59, 18, 35, 51, 14, 57, 17, 24, 12, 44, 56, 0, 30, 13, 59, 20, 49, 17, 54, 43, 6, 34, 46, 17, 58, 36, 0, 34, 29, 54, 25, 2, 36, 15, 60, 6, 37, 46, 4, 50, 9, 45 },
  { 19, 59, 48, 3, 24, 60, 44, 22, 34, 51, 15, 45, 41, 5, 33, 47, 0, 37, 12, 55, 25, 54, 8, 57, 0, 47, 18, 34, 49, 15, 55, 24, 40, 20, 8, 35, 53, 13, 41, 18, 0, 59, 22, 33, 4, 52, 8, 60, 24, 36, 31, 56, 45, 26, 10, 43, 15, 56, 36, 4, 51, 14, 39, 30, 12, 55, 36, 2, 39, 49, 4, 44, 17, 0, 32, 13, 53, 35, 59, 17, 62, 0, 55, 24, 52, 38, 31, 6, 42, 19, 29, 40, 4, 54, 33, 5, 16, 27, 52, 37, 23, 55, 7, 37, 0, 39, 23, 49, 4, 53, 31, 15, 59, 10, 50, 4, 60, 34, 48, 7, 31, 49, 27, 14, 62, 22, 53, 29 },
  { 46, 21, 14, 51, 36, 17, 7, 57, 10, 32, 3, 37, 22, 60, 39, 18, 56, 20, 42, 3, 36, 10, 44, 26, 41, 29, 53, 27, 2, 39, 30, 52, 0, 59, 15, 48, 23, 61, 6, 58, 37, 12, 40, 49, 16, 39, 20, 44, 0, 62, 8, 21, 3, 59, 23, 32, 49, 31, 12, 44, 22, 59, 18, 50, 24, 7, 43, 52, 15, 23, 41, 26, 51, 28, 55, 39, 21, 27, 10, 42, 12, 45, 27, 47, 3, 15, 63, 26, 55, 0, 60, 26, 45, 18, 62, 38, 58, 49, 8, 47, 4, 33, 46, 29, 57, 13, 56, 16, 59, 21, 5, 47, 23, 39, 18, 44, 13, 22, 28, 53, 19, 0, 58, 32, 41, 7, 26, 13 },
  { 0, 56, 34, 28, 11, 55, 31, 47, 26, 41, 56, 13, 53, 28, 11, 49, 7, 52, 32, 61, 50, 22, 63, 17, 13, 56, 7, 19, 43, 62, 10, 21, 37, 32, 43, 4, 38, 19, 44, 25, 31, 54, 5, 23, 61, 30, 53, 12, 35, 22, 43, 53, 37, 48, 7, 62, 20, 2, 61, 41, 8, 34, 47, 9, 63, 34, 28, 10, 55, 33, 14, 57, 7, 47, 9, 61, 4, 49, 31, 50, 21, 38, 8, 16, 57, 44, 33, 5, 49, 36, 12, 50, 7, 34, 10, 25, 2, 22, 36, 15, 26, 61, 18, 9, 22, 46, 32, 8, 27, 37, 44, 30, 55, 3, 62, 24, 38, 56, 5, 45, 38, 24, 43, 10, 19, 54, 39, 61 },
  { 41, 30, 8, 63, 43, 23, 38, 3, 62, 19, 8, 49, 25, 1, 58, 30, 23, 40, 9, 28, 18, 40, 6, 38, 49, 22, 35, 59, 8, 27, 50, 5, 56, 17, 11, 50, 30, 9, 55, 2, 51, 19, 34, 47, 9, 41, 6, 26, 48, 57, 14, 28, 17, 12, 39, 13, 37, 46, 25, 19, 54, 27, 1, 37, 16, 45, 20, 60, 1, 48, 20, 38, 31, 22, 42, 15, 19, 44, 1, 61, 6, 34, 56, 40, 29, 10, 20, 46, 13, 22, 41, 23, 59, 42, 30, 51, 45, 13, 63, 53, 42, 12, 51, 38, 62, 2, 26, 41, 50, 1, 61, 10, 19, 42, 31, 8, 49, 32, 12, 63, 9, 52, 16, 56, 36, 2, 31, 16 },
  { 52, 5, 47, 20, 1, 53, 12, 50, 16, 35, 43, 21, 33, 43, 16, 44, 3, 59, 14, 46, 1, 30, 60, 33, 2, 45, 12, 42, 31, 47, 14, 33, 46, 25, 55, 27, 60, 36, 16, 42, 14, 46, 26, 1, 55, 15, 63, 32, 2, 38, 5, 47, 33, 61, 30, 52, 4, 57, 6, 38, 11, 43, 61, 24, 52, 3, 31, 22, 42, 10, 62, 3, 59, 11, 35, 57, 33, 54, 24, 14, 29, 48, 18, 2, 60, 41, 53, 24, 32, 62, 3, 53, 15, 1, 55, 17, 32, 40, 6, 31, 1, 40, 28, 5, 35, 52, 19, 63, 13, 33, 17, 41, 52, 26, 15, 57, 1, 20, 42, 17, 35, 27, 48, 5, 25, 50, 44, 11 },
  { 35, 25, 38, 57, 33, 17, 40, 6, 59, 27, 54, 5, 61, 10, 52, 26, 36, 19, 51, 35, 57, 48, 11, 20, 54, 25, 61, 16, 1, 58, 24, 61, 3, 39, 7, 47, 1, 22, 49, 28, 63, 10, 58, 32, 17, 36, 45, 19, 51, 29, 59, 10, 50, 1, 23, 42, 18, 29, 51, 21, 56, 32, 14, 5, 40, 58, 47, 13, 54, 35, 29, 45, 18, 52, 26, 2, 38, 8, 46, 36, 58, 11, 52, 35, 17, 28, 1, 58, 9, 39, 17, 28, 37, 48, 20, 9, 57, 24, 50, 19, 58, 16, 48, 25, 43, 11, 35, 6, 45, 24, 56, 4, 36, 7, 47, 35, 52, 28, 59, 30, 2, 61, 21, 33, 63, 12, 18, 59 },
  { 3, 49, 15, 10, 27, 61, 25, 45, 30, 0, 14, 47, 31, 38, 17, 62, 7, 55, 27, 4, 15, 24, 42, 52, 10, 34, 5, 51, 36, 18, 41, 11, 35, 21, 62, 13, 33, 57, 8, 35, 5, 40, 21, 43, 52, 3, 24, 56, 11, 16, 33, 25, 41, 20, 55, 8, 60, 35, 15, 48, 2, 57, 30, 49, 18, 25, 6, 39, 17, 57, 7, 25, 43, 5, 49, 16, 62, 22, 55, 4, 25, 43, 23, 7, 50, 11, 37, 48, 14, 51, 33, 57, 7, 27, 39, 46, 4, 29, 11, 43, 34, 56, 7, 60, 20, 54, 30, 57, 22, 49, 9, 33, 54, 14, 63, 23, 6, 43, 10, 40, 50, 13, 44, 8, 38, 33, 46, 23 },
  { 55, 39, 22, 50, 44, 4, 36, 9, 52, 23, 37, 59, 21, 2, 46, 13, 31, 41, 11, 45, 62, 29, 6, 37, 19, 48, 30, 23, 44, 7, 53, 28, 54, 16, 41, 29, 44, 18, 52, 24, 60, 15, 48, 7, 27, 59, 9, 34, 42, 54, 7, 63, 4, 46, 31, 27, 45, 0, 40, 26, 34, 17, 37, 10, 53, 29, 36, 50, 2, 27, 51, 11, 61, 37, 23, 41, 30, 7, 18, 50, 39, 14, 63, 32, 45, 61, 19, 30, 25, 44, 2, 47, 23, 63, 11, 34, 59, 37, 60, 3, 22, 14, 44, 30, 15, 0, 47, 15, 3, 38, 61, 20, 27, 45, 11, 39, 51, 16, 55, 3, 22, 54, 29, 58, 1, 57, 6, 29 },
  { 9, 17, 60, 2, 34, 56, 20, 62, 39, 12, 49, 6, 29, 56, 34, 48, 0, 58, 22, 38, 18, 43, 56, 0, 63, 14, 55, 3, 59, 31, 15, 45, 0, 49, 6, 58, 3, 38, 12, 45, 0, 37, 29, 57, 13, 39, 30, 49, 0, 23, 44, 36, 16, 57, 13, 54, 11, 24, 63, 9, 53, 7, 62, 42, 0, 59, 15, 23, 63, 34, 40, 16, 32, 0, 53, 12, 48, 28, 59, 33, 0, 53, 9, 27, 3, 22, 54, 5, 56, 9, 61, 13, 42, 14, 52, 19, 0, 21, 47, 27, 53, 36, 3, 50, 39, 58, 25, 40, 53, 28, 12, 50, 0, 59, 32, 2, 21, 34, 26, 46, 37, 7, 18, 47, 24, 14, 53, 42 },
  { 61, 32, 13, 54, 29, 7, 46, 13, 28, 57, 18, 41, 53, 15, 9, 39, 24, 49, 33, 3, 53, 9, 26, 32, 40, 28, 46, 39, 25, 9, 56, 21, 63, 37, 26, 22, 51, 27, 17, 56, 31, 53, 4, 43, 22, 46, 12, 18, 60, 40, 20, 26, 50, 21, 39, 5, 49, 33, 16, 44, 22, 46, 20, 32, 24, 45, 8, 43, 12, 46, 4, 48, 56, 20, 29, 58, 3, 40, 10, 42, 31, 21, 47, 41, 56, 38, 15, 42, 36, 27, 20, 33, 55, 3, 26, 44, 31, 54, 12, 35, 9, 63, 28, 10, 21, 32, 9, 60, 17, 8, 43, 29, 40, 16, 36, 48, 60, 7, 57, 14, 62, 31, 42, 15, 36, 40, 20, 26 },
  { 0, 37, 47, 23, 41, 18, 32, 48, 1, 35, 8, 25, 4, 26, 63, 20, 54, 8, 16, 61, 35, 23, 51, 15, 58, 7, 12, 20, 50, 34, 42, 4, 38, 10, 32, 47, 8, 60, 41, 20, 9, 25, 50, 19, 62, 1, 37, 56, 28, 8, 53, 11, 3, 58, 34, 43, 19, 60, 38, 4, 58, 31, 3, 51, 11, 55, 38, 30, 21, 58, 19, 26, 9, 44, 36, 13, 46, 20, 62, 24, 13, 60, 5, 28, 12, 34, 7, 59, 0, 53, 45, 6, 38, 30, 50, 7, 62, 16, 41, 5, 46, 18, 55, 42, 51, 5, 45, 23, 34, 48, 19, 58, 5, 25, 54, 19, 13, 41, 28, 21, 0, 49, 10, 60, 4, 51, 9, 45 },
  { 19, 28, 6, 58, 10, 51, 4, 22, 55, 42, 60, 45, 34, 51, 42, 5, 30, 45, 27, 40, 13, 47, 4, 49, 21, 38, 60, 29, 2, 57, 17, 27, 52, 19, 61, 14, 30, 34, 2, 44, 63, 33, 11, 35, 16, 51, 25, 6, 14, 47, 31, 61, 37, 29, 18, 8, 52, 2, 28, 54, 13, 41, 15, 62, 35, 18, 2, 60, 6, 33, 41, 61, 31, 6, 56, 17, 34, 50, 6, 52, 44, 35, 16, 51, 59, 24, 48, 18, 31, 40, 16, 49, 21, 60, 17, 39, 10, 49, 32, 57, 24, 39, 1, 25, 18, 62, 37, 12, 56, 1, 37, 11, 52, 44, 9, 30, 47, 4, 51, 40, 55, 25, 34, 27, 56, 30, 32, 54 },
  { 63, 40, 49, 15, 43, 26, 63, 38, 16, 20, 30, 12, 57, 14, 19, 60, 36, 12, 59, 2, 57, 17, 42, 31, 1, 44, 16, 35, 47, 11, 32, 48, 13, 43, 1, 39, 51, 12, 57, 23, 6, 40, 53, 3, 55, 31, 39, 60, 35, 44, 5, 15, 45, 1, 62, 41, 26, 14, 47, 22, 36, 27, 50, 9, 26, 47, 52, 28, 54, 16, 1, 13, 51, 39, 23, 63, 1, 30, 15, 26, 2, 57, 19, 37, 1, 44, 21, 50, 13, 63, 8, 24, 56, 1, 35, 25, 58, 20, 2, 28, 14, 51, 33, 59, 13, 30, 4, 49, 31, 24, 63, 26, 33, 3, 58, 38, 62, 24, 32, 8, 17, 45, 5, 48, 18, 3, 43, 11 },
  { 21, 4, 24, 34, 59, 1, 37, 11, 53, 5, 47, 2, 22, 40, 32, 1, 24, 50, 21, 29, 38, 25, 63, 8, 55, 24, 53, 6, 62, 23, 59, 3, 54, 20, 58, 24, 5, 46, 15, 38, 48, 14, 27, 42, 23, 7, 46, 10, 17, 58, 25, 52, 23, 32, 49, 12, 55, 30, 40, 7, 59, 1, 56, 21, 39, 4, 23, 15, 37, 46, 55, 42, 21, 4, 48, 8, 45, 54, 37, 55, 32, 8, 46, 10, 30, 54, 4, 41, 25, 29, 36, 48, 11, 43, 14, 47, 5, 43, 53, 36, 61, 10, 45, 6, 41, 54, 27, 43, 16, 55, 6, 46, 18, 42, 23, 15, 1, 45, 12, 60, 37, 22, 62, 12, 39, 59, 16, 52 },
  { 47, 35, 56, 7, 19, 46, 31, 50, 33, 24, 61, 35, 50, 7, 53, 44, 55, 6, 46, 10, 52, 5, 21, 43, 36, 10, 18, 41, 26, 37, 8, 29, 40, 36, 9, 49, 34, 26, 61, 21, 7, 59, 18, 62, 29, 54, 20, 32, 51, 0, 40, 10, 55, 6, 20, 36, 9, 61, 5, 51, 44, 19, 33, 43, 13, 57, 40, 63, 8, 24, 29, 10, 60, 34, 27, 40, 25, 18, 10, 42, 21, 49, 26, 62, 38, 12, 33, 61, 5, 57, 2, 19, 54, 28, 62, 22, 38, 31, 16, 7, 22, 47, 29, 17, 35, 8, 20, 51, 2, 40, 22, 50, 13, 61, 28, 53, 35, 20, 56, 30, 2, 53, 14, 41, 23, 34, 8, 31 },
  { 12, 2, 42, 29, 52, 13, 21, 8, 55, 14, 41, 17, 28, 58, 23, 11, 17, 36, 31, 62, 17, 34, 50, 14, 28, 61, 33, 52, 2, 51, 17, 45, 7, 25, 62, 30, 18, 55, 0, 42, 30, 35, 45, 1, 12, 48, 3, 63, 21, 36, 30, 48, 19, 59, 43, 27, 46, 17, 34, 25, 12, 29, 53, 6, 48, 31, 11, 34, 49, 3, 36, 50, 19, 47, 14, 61, 11, 36, 58, 4, 60, 14, 39, 22, 6, 52, 15, 35, 17, 46, 31, 42, 9, 34, 3, 52, 12, 60, 26, 56, 40, 2, 53, 23, 57, 38, 62, 14, 36, 59, 10, 31, 39, 6, 49, 9, 41, 26, 5, 48, 43, 27, 33, 58, 1, 50, 25, 57 },
  { 61, 37, 15, 61, 3, 39, 58, 43, 26, 0, 44, 10, 47, 3, 37, 63, 28, 43, 13, 39, 3, 57, 30, 59, 0, 48, 5, 43, 13, 22, 60, 33, 55, 15, 42, 4, 52, 10, 45, 13, 54, 4, 24, 49, 37, 26, 41, 14, 42, 9, 61, 13, 38, 23, 3, 53, 0, 58, 21, 42, 63, 10, 17, 61, 25, 0, 58, 28, 17, 44, 57, 12, 27, 0, 55, 5, 52, 28, 23, 47, 29, 0, 43, 17, 58, 28, 47, 23, 55, 10, 58, 23, 51, 40, 18, 33, 45, 0, 49, 8, 32, 61, 19, 48, 0, 26, 7, 47, 29, 18, 44, 0, 56, 34, 20, 59, 15, 51, 37, 18, 10, 52, 7, 20, 46, 9, 38, 17 },
  { 6, 27, 48, 23, 45, 29, 5, 18, 38, 62, 27, 56, 20, 32, 15, 9, 48, 0, 54, 22, 45, 20, 7, 41, 23, 39, 19, 27, 58, 31, 44, 0, 12, 50, 23, 56, 20, 39, 32, 59, 16, 52, 33, 9, 57, 22, 6, 58, 28, 50, 24, 2, 56, 35, 16, 45, 32, 38, 15, 54, 2, 38, 46, 22, 35, 45, 20, 5, 52, 25, 7, 35, 59, 32, 22, 43, 38, 3, 51, 16, 34, 53, 32, 50, 3, 40, 8, 43, 0, 39, 27, 4, 14, 61, 8, 55, 15, 41, 20, 44, 27, 13, 39, 11, 46, 42, 54, 33, 4, 52, 23, 61, 14, 25, 43, 2, 33, 11, 63, 29, 61, 17, 40, 55, 22, 62, 28, 44 },
  { 20, 54, 8, 56, 35, 10, 63, 31, 52, 12, 48, 6, 59, 41, 52, 33, 19, 58, 25, 49, 11, 37, 47, 12, 54, 15, 56, 35, 7, 47, 16, 53, 28, 34, 5, 37, 28, 8, 48, 3, 28, 38, 18, 61, 16, 43, 53, 32, 4, 17, 47, 27, 44, 8, 63, 10, 25, 49, 6, 37, 24, 52, 32, 3, 50, 12, 41, 56, 38, 14, 62, 20, 40, 16, 53, 31, 18, 63, 41, 9, 59, 7, 13, 25, 57, 20, 63, 26, 53, 18, 48, 62, 30, 46, 21, 25, 58, 29, 36, 4, 55, 34, 6, 60, 31, 16, 21, 12, 58, 38, 9, 29, 47, 7, 52, 30, 57, 44, 22, 0, 35, 45, 3, 31, 14, 36, 0, 51 },
  { 42, 14, 33, 24, 16, 49, 40, 2, 22, 33, 16, 36, 25, 1, 21, 61, 38, 8, 33, 4, 62, 26, 29, 60, 6, 46, 30, 11, 63, 4, 36, 40, 19, 57, 46, 11, 41, 63, 22, 25, 58, 10, 46, 2, 34, 27, 11, 38, 56, 34, 12, 53, 18, 33, 41, 51, 13, 28, 60, 20, 47, 14, 29, 59, 16, 62, 8, 22, 32, 47, 9, 49, 2, 44, 7, 12, 45, 6, 20, 27, 45, 24, 62, 42, 36, 11, 33, 15, 37, 7, 32, 10, 37, 1, 35, 50, 6, 11, 63, 24, 52, 15, 50, 24, 3, 37, 56, 27, 34, 22, 49, 16, 36, 62, 17, 39, 4, 15, 54, 24, 50, 8, 58, 26, 49, 54, 11, 30 },
  { 4, 59, 41, 1, 53, 12, 25, 45, 59, 7, 51, 39, 54, 14, 46, 4, 27, 53, 16, 44, 18, 51, 1, 32, 25, 2, 50, 40, 20, 54, 24, 9, 62, 2, 27, 60, 1, 17, 36, 50, 6, 40, 30, 55, 41, 19, 49, 1, 21, 60, 40, 5, 62, 1, 22, 30, 57, 4, 43, 31, 1, 55, 40, 7, 27, 37, 30, 54, 1, 19, 42, 30, 56, 26, 62, 49, 24, 57, 37, 56, 2, 39, 16, 5, 30, 55, 3, 49, 60, 23, 56, 44, 17, 52, 13, 42, 28, 48, 18, 45, 9, 37, 21, 41, 58, 10, 48, 1, 63, 5, 41, 57, 2, 24, 12, 48, 27, 42, 32, 46, 13, 38, 19, 34, 5, 41, 25, 60 },
  { 39, 28, 21, 46, 32, 57, 36, 9, 19, 42, 4, 29, 11, 43, 30, 49, 13, 42, 35, 56, 9, 39, 15, 52, 36, 61, 18, 26, 45, 14, 31, 48, 21, 43, 14, 33, 49, 54, 14, 44, 21, 62, 13, 23, 8, 62, 15, 51, 44, 7, 30, 37, 20, 42, 56, 7, 39, 18, 50, 11, 61, 9, 19, 43, 57, 2, 48, 11, 39, 60, 28, 4, 37, 17, 35, 1, 33, 11, 31, 14, 48, 19, 35, 51, 46, 21, 44, 29, 12, 41, 2, 22, 58, 26, 54, 4, 59, 38, 2, 33, 57, 1, 63, 13, 28, 51, 15, 40, 18, 45, 8, 30, 43, 37, 54, 19, 8, 59, 21, 6, 60, 29, 55, 10, 63, 15, 47, 17 },
  { 3, 50, 10, 62, 18, 5, 27, 49, 60, 23, 55, 18, 62, 24, 56, 10, 59, 28, 2, 23, 34, 59, 43, 20, 10, 42, 8, 49, 1, 37, 57, 6, 51, 29, 53, 7, 23, 31, 5, 32, 51, 0, 35, 54, 45, 31, 5, 26, 36, 24, 55, 15, 48, 29, 14, 48, 26, 60, 21, 41, 36, 26, 50, 33, 14, 44, 17, 24, 52, 15, 46, 23, 54, 6, 47, 21, 60, 50, 4, 53, 29, 61, 8, 23, 1, 60, 19, 6, 53, 16, 47, 34, 6, 39, 16, 31, 12, 20, 53, 22, 30, 43, 25, 46, 35, 6, 44, 32, 53, 26, 55, 19, 11, 59, 5, 33, 51, 1, 35, 53, 25, 3, 42, 23, 44, 32, 7, 53 },
  { 22, 44, 37, 6, 26, 51, 38, 0, 34, 13, 31, 46, 3, 37, 6, 19, 40, 21, 47, 63, 12, 5, 29, 55, 22, 58, 34, 28, 60, 22, 11, 41, 17, 38, 9, 44, 59, 39, 56, 19, 11, 47, 25, 15, 3, 39, 57, 17, 61, 11, 46, 3, 58, 9, 54, 35, 2, 34, 8, 45, 15, 56, 5, 23, 53, 33, 63, 35, 4, 59, 10, 51, 13, 61, 29, 41, 15, 25, 43, 19, 40, 10, 54, 33, 41, 12, 38, 51, 31, 26, 61, 9, 30, 45, 24, 62, 49, 40, 10, 61, 14, 49, 5, 17, 54, 20, 60, 23, 3, 13, 35, 50, 32, 23, 46, 27, 38, 63, 16, 12, 39, 48, 18, 51, 1, 27, 56, 35 },
  { 63, 15, 30, 55, 43, 14, 57, 17, 53, 44, 7, 48, 26, 50, 32, 60, 0, 53, 14, 31, 50, 24, 46, 0, 38, 13, 4, 52, 16, 45, 30, 59, 0, 25, 55, 35, 16, 10, 26, 42, 58, 29, 60, 38, 50, 22, 28, 47, 0, 50, 28, 19, 33, 39, 11, 44, 16, 52, 24, 59, 3, 38, 27, 51, 0, 21, 7, 42, 26, 34, 21, 40, 33, 18, 39, 3, 54, 38, 8, 59, 0, 44, 27, 15, 58, 28, 57, 9, 43, 0, 36, 50, 20, 59, 8, 34, 0, 27, 47, 7, 36, 19, 56, 32, 0, 38, 11, 29, 62, 47, 6, 61, 0, 41, 14, 56, 10, 23, 45, 31, 57, 8, 36, 13, 58, 38, 11, 19 },
  { 0, 34, 12, 47, 21, 2, 40, 30, 11, 25, 61, 20, 40, 15, 35, 22, 45, 36, 7, 41, 17, 57, 9, 48, 32, 62, 44, 24, 35, 3, 54, 13, 33, 63, 19, 4, 48, 22, 62, 2, 37, 8, 33, 6, 20, 52, 9, 32, 43, 13, 39, 63, 25, 4, 49, 23, 62, 32, 9, 30, 48, 18, 63, 12, 46, 29, 58, 13, 48, 8, 57, 31, 0, 51, 9, 58, 12, 22, 47, 29, 35, 22, 49, 5, 46, 4, 34, 20, 63, 24, 56, 11, 41, 3, 51, 19, 56, 35, 17, 58, 28, 42, 9, 45, 59, 26, 51, 42, 17, 36, 25, 15, 53, 21, 44, 3, 30, 55, 5, 50, 21, 28, 61, 32, 6, 49, 28, 46 },
  { 58, 42, 60, 4, 31, 59, 22, 63, 35, 38, 9, 54, 1, 57, 8, 51, 16, 58, 27, 53, 3, 38, 30, 15, 27, 6, 19, 56, 10, 50, 21, 36, 47, 5, 43, 28, 51, 32, 13, 46, 18, 54, 16, 43, 63, 12, 36, 59, 22, 34, 5, 52, 17, 59, 27, 41, 0, 19, 55, 37, 13, 43, 6, 34, 41, 10, 36, 55, 19, 44, 3, 16, 58, 27, 49, 25, 32, 62, 17, 55, 13, 63, 18, 52, 25, 37, 17, 48, 13, 32, 5, 46, 28, 37, 14, 43, 25, 5, 51, 39, 3, 52, 33, 22, 8, 40, 12, 4, 57, 9, 46, 39, 28, 58, 13, 62, 17, 42, 19, 36, 0, 47, 16, 43, 24, 21, 54, 13 },
  { 25, 9, 23, 50, 36, 8, 45, 14, 3, 51, 16, 28, 44, 12, 42, 29, 4, 26, 10, 47, 22, 61, 18, 54, 51, 39, 46, 13, 41, 26, 58, 7, 18, 39, 12, 57, 15, 1, 52, 27, 41, 23, 48, 1, 27, 45, 18, 2, 57, 26, 55, 8, 43, 31, 6, 58, 14, 51, 40, 5, 61, 31, 24, 54, 17, 60, 22, 1, 39, 30, 53, 45, 36, 13, 43, 5, 45, 2, 37, 6, 34, 42, 2, 39, 10, 62, 7, 54, 40, 18, 60, 15, 52, 21, 63, 8, 55, 46, 15, 30, 23, 13, 62, 16, 50, 24, 58, 31, 48, 21, 34, 2, 49, 7, 31, 37, 26, 48, 9, 61, 40, 11, 52, 2, 60, 40, 4, 37 },
  { 52, 28, 39, 16, 54, 19, 29, 55, 42, 20, 58, 33, 24, 63, 18, 55, 39, 62, 43, 34, 12, 40, 6, 35, 2, 25, 8, 62, 34, 1, 31, 42, 61, 27, 53, 24, 40, 61, 34, 8, 59, 4, 30, 56, 40, 6, 53, 42, 10, 48, 16, 37, 12, 46, 21, 36, 47, 11, 28, 45, 22, 10, 57, 2, 49, 31, 14, 44, 61, 11, 25, 6, 23, 63, 18, 36, 28, 56, 20, 51, 11, 48, 27, 56, 32, 22, 45, 30, 2, 42, 27, 39, 1, 44, 23, 31, 38, 22, 11, 61, 43, 54, 4, 47, 35, 2, 44, 16, 28, 54, 12, 62, 18, 43, 10, 52, 1, 58, 33, 15, 29, 56, 20, 34, 9, 30, 48, 17 },
  { 46, 2, 56, 11, 41, 1, 49, 6, 27, 47, 2, 48, 5, 32, 37, 3, 13, 19, 32, 1, 55, 28, 60, 17, 43, 59, 32, 20, 49, 16, 55, 23, 14, 46, 2, 36, 6, 30, 20, 49, 12, 47, 35, 14, 21, 60, 29, 14, 35, 24, 46, 1, 56, 29, 53, 8, 33, 23, 56, 1, 35, 46, 20, 39, 26, 4, 53, 28, 17, 38, 60, 34, 48, 9, 55, 15, 46, 7, 41, 31, 60, 24, 16, 36, 1, 59, 19, 52, 35, 6, 55, 11, 59, 33, 7, 57, 4, 29, 48, 1, 19, 26, 37, 30, 18, 63, 37, 6, 59, 1, 40, 24, 56, 33, 46, 22, 35, 7, 24, 53, 39, 5, 26, 45, 55, 18, 62, 7 },
  { 20, 60, 29, 34, 20, 62, 33, 52, 10, 36, 13, 60, 41, 21, 50, 27, 56, 49, 8, 51, 21, 45, 11, 48, 8, 23, 53, 3, 29, 44, 5, 52, 9, 32, 50, 17, 43, 56, 3, 38, 24, 10, 62, 25, 51, 9, 33, 49, 61, 7, 30, 62, 22, 19, 2, 42, 63, 5, 49, 18, 60, 15, 52, 7, 43, 56, 23, 50, 5, 50, 2, 20, 41, 30, 1, 52, 22, 61, 14, 26, 3, 43, 53, 7, 47, 28, 11, 14, 23, 58, 33, 25, 47, 13, 50, 17, 40, 54, 34, 60, 41, 6, 59, 14, 50, 7, 25, 55, 20, 42, 51, 8, 27, 4, 16, 60, 28, 50, 44, 3, 22, 49, 63, 12, 33, 1, 43, 31 },
  { 36, 5, 46, 8, 44, 24, 13, 39, 25, 57, 31, 18, 8, 52, 10, 45, 6, 30, 36, 24, 63, 4, 33, 26, 57, 40, 15, 56, 37, 12, 40, 25, 37, 58, 11, 63, 21, 45, 16, 60, 31, 53, 18, 33, 3, 45, 23, 0, 20, 54, 40, 15, 50, 38, 60, 16, 25, 42, 29, 38, 7, 41, 25, 62, 18, 33, 8, 35, 42, 16, 32, 56, 12, 39, 59, 19, 34, 9, 49, 38, 57, 12, 21, 50, 14, 40, 61, 44, 50, 9, 49, 19, 3, 29, 35, 62, 12, 24, 7, 18, 52, 32, 10, 46, 21, 41, 32, 11, 36, 29, 14, 34, 60, 38, 54, 11, 41, 14, 19, 57, 32, 16, 7, 41, 51, 25, 14, 57 },
  { 53, 18, 26, 50, 15, 58, 4, 63, 17, 43, 7, 40, 61, 35, 15, 41, 23, 60, 16, 38, 14, 42, 19, 50, 0, 31, 10, 46, 27, 63, 18, 60, 0, 20, 29, 39, 8, 26, 37, 5, 42, 0, 44, 39, 57, 17, 58, 41, 28, 37, 4, 32, 9, 44, 12, 31, 54, 10, 59, 14, 27, 53, 12, 36, 0, 47, 13, 63, 21, 58, 10, 24, 50, 27, 4, 26, 44, 53, 31, 0, 18, 42, 29, 33, 57, 4, 32, 26, 0, 38, 16, 61, 41, 53, 20, 0, 42, 44, 49, 27, 10, 56, 39, 0, 57, 15, 53, 49, 3, 61, 22, 47, 17, 5, 49, 26, 2, 63, 39, 10, 47, 27, 37, 23, 4, 59, 38, 10 },
  { 23, 39, 61, 3, 37, 28, 48, 31, 0, 34, 51, 23, 2, 26, 58, 0, 53, 11, 46, 1, 57, 29, 52, 14, 37, 61, 21, 35, 2, 49, 7, 34, 47, 55, 4, 33, 54, 13, 58, 52, 19, 50, 22, 7, 13, 29, 36, 11, 51, 17, 60, 25, 55, 4, 34, 51, 0, 35, 20, 48, 32, 3, 51, 30, 59, 28, 40, 3, 46, 29, 54, 43, 7, 62, 47, 11, 39, 4, 23, 46, 55, 8, 63, 5, 25, 37, 18, 46, 21, 56, 31, 5, 36, 8, 45, 58, 26, 15, 2, 36, 47, 21, 29, 44, 25, 34, 3, 27, 43, 10, 52, 0, 45, 30, 24, 36, 43, 18, 34, 59, 0, 52, 61, 15, 44, 19, 30, 49 },
  { 0, 27, 12, 43, 54, 9, 22, 53, 21, 46, 15, 55, 29, 47, 20, 33, 39, 28, 59, 35, 9, 44, 5, 24, 47, 7, 52, 17, 56, 22, 30, 42, 14, 26, 45, 18, 49, 1, 24, 34, 11, 27, 55, 32, 61, 47, 2, 56, 6, 44, 13, 47, 36, 27, 58, 22, 16, 47, 40, 4, 57, 38, 21, 45, 16, 9, 56, 26, 11, 38, 0, 22, 36, 17, 33, 57, 16, 30, 62, 15, 35, 40, 20, 45, 59, 10, 54, 8, 63, 13, 52, 27, 22, 57, 28, 12, 32, 51, 55, 22, 63, 4, 16, 54, 12, 62, 45, 19, 58, 13, 32, 40, 20, 56, 7, 57, 9, 54, 6, 29, 42, 21, 8, 55, 35, 47, 6, 41 },
  { 56, 33, 58, 32, 19, 35, 42, 6, 59, 11, 38, 5, 49, 12, 62, 7, 52, 17, 5, 25, 54, 20, 61, 31, 54, 27, 41, 11, 44, 5, 59, 12, 36, 51, 10, 61, 28, 41, 48, 9, 43, 63, 5, 40, 20, 8, 49, 26, 34, 21, 58, 1, 18, 45, 7, 39, 61, 26, 8, 50, 23, 10, 63, 5, 55, 37, 19, 49, 52, 15, 59, 47, 13, 54, 1, 25, 42, 58, 10, 48, 3, 27, 50, 1, 17, 48, 34, 41, 16, 40, 2, 45, 10, 39, 17, 61, 5, 38, 19, 9, 41, 31, 60, 38, 5, 23, 36, 8, 30, 55, 24, 63, 12, 48, 14, 51, 31, 20, 45, 25, 12, 50, 32, 2, 28, 11, 62, 14 },
  { 44, 16, 7, 48, 1, 62, 16, 50, 27, 33, 61, 25, 17, 44, 31, 14, 22, 43, 32, 48, 18, 40, 8, 36, 3, 16, 33, 62, 23, 38, 25, 53, 2, 21, 41, 6, 22, 15, 59, 29, 16, 37, 26, 15, 52, 42, 23, 15, 54, 39, 10, 30, 53, 11, 49, 24, 2, 43, 55, 17, 34, 44, 15, 31, 24, 44, 2, 32, 7, 35, 25, 5, 40, 45, 29, 51, 6, 21, 37, 52, 24, 60, 13, 31, 53, 23, 2, 28, 49, 24, 31, 60, 20, 51, 1, 34, 48, 14, 59, 33, 50, 1, 18, 33, 48, 60, 17, 51, 39, 6, 38, 2, 35, 29, 40, 23, 1, 62, 15, 53, 37, 17, 46, 57, 40, 51, 24, 22 },
  { 5, 37, 52, 24, 45, 13, 40, 3, 45, 9, 19, 42, 56, 4, 37, 46, 56, 2, 63, 11, 51, 1, 49, 13, 59, 45, 39, 1, 48, 15, 58, 9, 46, 31, 54, 35, 57, 38, 3, 46, 56, 4, 47, 57, 1, 30, 38, 63, 3, 46, 28, 63, 41, 14, 33, 62, 19, 32, 13, 28, 61, 1, 53, 42, 11, 60, 22, 62, 27, 42, 61, 31, 19, 8, 61, 12, 32, 55, 2, 18, 33, 12, 43, 36, 9, 62, 30, 55, 6, 58, 35, 7, 43, 29, 54, 23, 43, 30, 3, 25, 11, 45, 52, 28, 7, 14, 42, 1, 22, 50, 16, 53, 19, 59, 4, 46, 33, 41, 4, 35, 58, 5, 26, 13, 20, 2, 34, 54 },
  { 30, 63, 21, 10, 26, 55, 29, 59, 23, 39, 53, 1, 36, 24, 59, 27, 10, 34, 23, 38, 30, 60, 22, 42, 28, 19, 9, 57, 30, 19, 43, 33, 13, 63, 3, 19, 11, 50, 31, 20, 14, 34, 10, 35, 17, 59, 7, 31, 19, 25, 50, 5, 20, 57, 29, 6, 52, 41, 4, 46, 20, 37, 26, 17, 49, 6, 39, 18, 53, 14, 3, 49, 57, 23, 34, 48, 14, 41, 28, 38, 56, 6, 58, 25, 39, 19, 43, 15, 37, 11, 47, 18, 53, 4, 37, 9, 62, 21, 53, 40, 57, 24, 13, 40, 56, 26, 47, 31, 59, 25, 45, 27, 10, 43, 21, 61, 13, 27, 48, 9, 23, 43, 31, 62, 38, 59, 9, 47 },
  { 25, 4, 40, 60, 34, 6, 18, 36, 8, 57, 12, 30, 49, 14, 6, 54, 41, 16, 50, 6, 43, 15, 34, 4, 53, 24, 50, 35, 4, 51, 7, 55, 28, 24, 39, 44, 60, 7, 25, 62, 42, 53, 24, 61, 28, 45, 52, 12, 48, 37, 9, 35, 43, 3, 37, 48, 12, 58, 30, 52, 9, 59, 6, 57, 33, 29, 48, 4, 37, 45, 20, 34, 10, 39, 0, 60, 22, 45, 8, 63, 21, 42, 14, 49, 3, 56, 11, 46, 21, 61, 0, 42, 25, 13, 63, 17, 36, 8, 46, 16, 6, 35, 63, 0, 21, 37, 4, 57, 9, 34, 5, 61, 48, 32, 8, 37, 54, 17, 56, 30, 60, 0, 50, 16, 7, 29, 42, 17 },
  { 32, 50, 15, 48, 2, 43, 52, 25, 47, 16, 32, 63, 21, 52, 40, 19, 0, 61, 29, 58, 20, 56, 26, 46, 12, 55, 6, 22, 62, 32, 17, 40, 0, 49, 34, 8, 27, 32, 48, 0, 21, 39, 5, 44, 12, 6, 22, 40, 0, 57, 16, 60, 23, 17, 54, 22, 36, 15, 24, 39, 19, 34, 47, 23, 0, 54, 13, 51, 24, 9, 55, 16, 52, 27, 44, 20, 4, 54, 26, 49, 0, 30, 46, 16, 29, 51, 34, 4, 52, 28, 33, 15, 57, 39, 26, 49, 0, 56, 27, 31, 48, 20, 43, 29, 53, 11, 46, 19, 41, 13, 55, 18, 0, 57, 26, 51, 2, 44, 6, 38, 14, 40, 22, 45, 36, 53, 3, 57 },
  { 44, 12, 37, 28, 22, 57, 11, 38, 0, 51, 9, 41, 4, 29, 11, 47, 33, 45, 12, 26, 3, 36, 9, 63, 31, 16, 38, 44, 14, 47, 25, 61, 20, 58, 15, 47, 17, 57, 13, 36, 9, 51, 18, 29, 50, 36, 54, 20, 61, 27, 32, 13, 53, 44, 9, 27, 0, 63, 45, 2, 56, 10, 14, 43, 41, 28, 58, 11, 35, 60, 30, 41, 6, 63, 11, 51, 37, 32, 15, 10, 35, 53, 5, 61, 22, 7, 26, 59, 23, 9, 44, 48, 21, 3, 51, 32, 24, 41, 12, 61, 2, 55, 9, 15, 35, 58, 28, 15, 62, 30, 37, 23, 42, 29, 11, 17, 35, 24, 63, 20, 52, 28, 8, 55, 11, 23, 47, 19 },
  { 0, 56, 8, 53, 14, 31, 61, 20, 55, 28, 62, 18, 35, 60, 25, 57, 7, 23, 39, 54, 47, 17, 43, 0, 40, 59, 29, 2, 56, 10, 37, 5, 43, 11, 29, 52, 1, 23, 54, 41, 59, 30, 55, 1, 62, 15, 33, 4, 43, 10, 47, 39, 1, 31, 40, 60, 49, 33, 7, 55, 26, 50, 31, 61, 8, 18, 21, 32, 44, 1, 25, 47, 18, 36, 30, 23, 59, 7, 40, 59, 27, 19, 38, 32, 44, 54, 40, 17, 38, 60, 27, 6, 35, 55, 10, 14, 44, 5, 50, 17, 38, 26, 42, 50, 18, 3, 44, 52, 2, 49, 7, 52, 15, 46, 62, 39, 55, 10, 31, 48, 3, 58, 33, 18, 61, 34, 13, 59 },
  { 39, 27, 63, 20, 35, 41, 4, 45, 26, 5, 38, 13, 44, 2, 50, 17, 37, 52, 2, 13, 28, 58, 24, 51, 21, 8, 34, 48, 27, 42, 18, 51, 31, 56, 5, 36, 38, 44, 4, 17, 26, 11, 38, 23, 42, 8, 56, 39, 24, 51, 5, 56, 21, 59, 14, 6, 18, 42, 22, 35, 16, 37, 3, 25, 39, 46, 63, 5, 50, 17, 58, 8, 55, 3, 50, 12, 43, 17, 47, 2, 51, 9, 62, 12, 1, 35, 13, 50, 1, 37, 12, 51, 19, 29, 46, 59, 22, 58, 33, 45, 22, 60, 10, 32, 61, 39, 8, 33, 25, 36, 20, 60, 38, 4, 21, 5, 28, 45, 12, 18, 42, 11, 49, 1, 27, 40, 6, 30 },
  { 24, 16, 42, 1, 50, 10, 48, 17, 33, 43, 24, 48, 21, 55, 31, 42, 10, 21, 63, 35, 49, 6, 33, 13, 41, 53, 10, 20, 60, 6, 53, 26, 12, 41, 22, 60, 14, 28, 63, 33, 49, 3, 45, 16, 48, 26, 14, 46, 18, 30, 35, 26, 8, 50, 29, 51, 25, 57, 12, 47, 53, 9, 62, 20, 54, 2, 36, 15, 40, 28, 33, 13, 38, 24, 46, 1, 29, 56, 33, 20, 44, 24, 41, 26, 57, 20, 63, 8, 30, 55, 5, 41, 62, 8, 34, 2, 37, 10, 19, 6, 37, 1, 53, 23, 5, 27, 58, 22, 43, 12, 50, 26, 9, 34, 54, 32, 49, 1, 59, 37, 22, 46, 25, 36, 51, 15, 54, 46 },
  { 52, 7, 45, 33, 26, 58, 14, 60, 7, 54, 3, 58, 8, 34, 14, 5, 59, 30, 18, 44, 8, 22, 48, 62, 3, 26, 55, 38, 23, 16, 39, 1, 62, 24, 49, 9, 53, 19, 46, 7, 19, 60, 31, 58, 2, 34, 53, 7, 59, 2, 62, 42, 46, 19, 36, 11, 44, 4, 38, 28, 1, 43, 32, 51, 12, 29, 56, 22, 52, 2, 62, 49, 22, 60, 14, 35, 63, 5, 25, 57, 14, 53, 4, 46, 18, 31, 42, 22, 47, 20, 58, 31, 16, 43, 23, 54, 30, 42, 52, 57, 29, 49, 30, 13, 45, 48, 16, 55, 6, 63, 1, 44, 14, 58, 19, 47, 15, 24, 51, 34, 6, 55, 5, 63, 20, 41, 21, 9 },
  { 30, 62, 18, 55, 5, 23, 39, 29, 49, 30, 15, 36, 28, 46, 60, 25, 39, 46, 4, 32, 61, 40, 15, 30, 36, 45, 14, 2, 49, 33, 57, 45, 18, 32, 3, 45, 30, 2, 35, 52, 40, 27, 13, 21, 38, 63, 20, 28, 37, 23, 16, 10, 13, 55, 2, 62, 21, 32, 60, 17, 58, 23, 5, 40, 16, 48, 7, 45, 10, 26, 43, 19, 6, 31, 52, 21, 39, 16, 48, 9, 37, 28, 36, 55, 7, 48, 3, 59, 15, 45, 25, 1, 53, 13, 47, 7, 62, 15, 4, 25, 12, 41, 18, 60, 38, 11, 34, 19, 39, 31, 29, 56, 23, 42, 3, 27, 60, 41, 8, 16, 61, 29, 43, 9, 32, 2, 60, 34 },
  { 3, 38, 13, 37, 52, 44, 2, 19, 12, 42, 63, 19, 40, 1, 20, 50, 12, 55, 15, 56, 27, 1, 54, 11, 57, 18, 32, 63, 44, 4, 29, 13, 37, 61, 35, 16, 42, 57, 12, 22, 6, 55, 43, 10, 50, 5, 44, 11, 48, 52, 34, 58, 28, 41, 38, 30, 7, 52, 11, 49, 30, 14, 45, 27, 59, 34, 21, 38, 32, 58, 11, 36, 56, 42, 9, 41, 3, 54, 31, 42, 0, 60, 16, 11, 39, 24, 52, 33, 6, 36, 10, 40, 32, 60, 26, 20, 39, 28, 47, 34, 63, 8, 54, 3, 24, 56, 0, 51, 13, 47, 16, 40, 7, 35, 52, 11, 36, 4, 57, 30, 39, 13, 18, 50, 58, 28, 12, 48 },
  { 57, 24, 49, 21, 10, 31, 61, 36, 56, 0, 22, 53, 11, 56, 32, 7, 36, 27, 41, 9, 46, 19, 34, 42, 25, 7, 50, 9, 28, 21, 54, 8, 50, 7, 27, 59, 10, 25, 48, 62, 37, 0, 33, 58, 25, 18, 32, 61, 0, 15, 45, 5, 50, 3, 23, 55, 47, 17, 40, 6, 60, 34, 53, 8, 41, 0, 61, 13, 54, 4, 46, 28, 0, 17, 48, 27, 58, 13, 23, 61, 33, 21, 50, 30, 62, 8, 14, 29, 56, 27, 61, 49, 17, 2, 44, 11, 51, 0, 59, 17, 40, 20, 32, 47, 36, 21, 42, 28, 60, 4, 54, 10, 59, 17, 30, 62, 21, 43, 26, 48, 0, 56, 36, 25, 8, 44, 39, 17 },
  { 10, 42, 4, 59, 27, 47, 8, 23, 51, 32, 45, 6, 37, 26, 48, 43, 62, 0, 21, 53, 38, 12, 51, 5, 60, 47, 24, 37, 59, 15, 35, 47, 22, 55, 0, 50, 21, 40, 6, 29, 15, 52, 24, 8, 41, 55, 13, 29, 40, 56, 24, 31, 19, 33, 61, 15, 0, 35, 24, 42, 21, 2, 19, 57, 24, 15, 30, 50, 20, 25, 40, 16, 57, 34, 61, 8, 29, 45, 6, 49, 11, 47, 2, 44, 19, 57, 38, 50, 12, 42, 21, 4, 35, 52, 28, 56, 23, 36, 13, 45, 4, 52, 27, 14, 6, 62, 9, 45, 21, 37, 25, 46, 33, 49, 0, 44, 7, 53, 13, 19, 53, 31, 3, 47, 15, 56, 22, 51 },
  { 35, 28, 53, 32, 1, 16, 54, 40, 9, 17, 25, 58, 14, 59, 3, 22, 16, 51, 31, 5, 23, 58, 28, 17, 35, 20, 0, 42, 11, 52, 3, 31, 41, 17, 43, 13, 32, 54, 18, 60, 32, 45, 17, 49, 2, 36, 51, 22, 7, 36, 9, 63, 48, 12, 46, 26, 43, 28, 63, 13, 48, 37, 51, 33, 5, 47, 55, 9, 42, 63, 7, 51, 24, 12, 37, 19, 55, 34, 18, 38, 15, 28, 54, 34, 5, 43, 22, 0, 48, 14, 54, 24, 58, 9, 38, 5, 32, 55, 21, 30, 49, 9, 59, 43, 30, 51, 35, 26, 7, 53, 2, 22, 14, 27, 57, 18, 38, 24, 33, 45, 10, 41, 20, 60, 37, 5, 32, 0 },
  { 63, 19, 15, 40, 62, 35, 14, 28, 46, 61, 4, 49, 35, 10, 29, 54, 33, 8, 45, 62, 37, 1, 43, 55, 10, 52, 61, 30, 19, 40, 25, 62, 11, 38, 27, 58, 36, 3, 46, 8, 39, 4, 62, 28, 47, 20, 4, 54, 47, 27, 43, 1, 21, 38, 8, 58, 10, 54, 4, 56, 9, 26, 12, 39, 60, 27, 18, 37, 1, 31, 35, 5, 45, 50, 2, 43, 26, 1, 59, 23, 56, 40, 7, 26, 58, 17, 32, 63, 25, 39, 7, 31, 45, 19, 63, 15, 48, 8, 37, 61, 16, 34, 1, 56, 18, 3, 15, 58, 49, 32, 63, 41, 55, 5, 40, 22, 50, 6, 59, 2, 63, 23, 52, 11, 26, 61, 44, 23 },
  { 11, 56, 46, 6, 22, 43, 58, 3, 34, 21, 38, 30, 18, 44, 52, 13, 41, 57, 17, 28, 14, 49, 25, 7, 33, 39, 26, 6, 56, 48, 1, 20, 56, 5, 46, 9, 19, 51, 30, 25, 56, 21, 35, 14, 57, 42, 16, 33, 10, 57, 17, 59, 41, 25, 53, 37, 20, 40, 30, 18, 31, 62, 44, 22, 3, 44, 11, 48, 23, 53, 18, 60, 29, 22, 62, 15, 53, 47, 10, 41, 3, 19, 52, 36, 13, 46, 10, 35, 3, 61, 41, 16, 1, 50, 26, 42, 18, 46, 2, 25, 54, 20, 39, 23, 47, 31, 41, 12, 38, 17, 8, 19, 31, 48, 12, 61, 9, 54, 29, 35, 15, 38, 6, 43, 34, 14, 7, 47 },
  { 39, 2, 33, 26, 53, 8, 18, 50, 41, 12, 53, 1, 63, 24, 19, 39, 2, 24, 47, 10, 60, 38, 19, 63, 48, 4, 15, 45, 32, 14, 60, 36, 29, 53, 23, 63, 34, 12, 61, 1, 43, 11, 53, 30, 1, 26, 60, 45, 23, 39, 3, 29, 12, 50, 4, 16, 51, 3, 45, 36, 50, 1, 16, 54, 35, 14, 57, 30, 58, 9, 46, 14, 41, 10, 32, 38, 4, 30, 21, 51, 32, 63, 25, 1, 60, 27, 53, 18, 51, 22, 28, 55, 34, 12, 40, 3, 60, 29, 57, 41, 6, 44, 11, 53, 8, 61, 24, 57, 1, 28, 44, 59, 36, 3, 34, 25, 41, 31, 16, 44, 22, 47, 28, 58, 1, 49, 54, 29 },
  { 58, 25, 50, 13, 38, 30, 60, 24, 6, 57, 27, 42, 9, 45, 6, 61, 30, 50, 4, 34, 29, 3, 46, 13, 22, 42, 58, 28, 9, 39, 23, 44, 7, 15, 44, 2, 40, 15, 47, 41, 23, 37, 7, 59, 38, 11, 34, 6, 62, 14, 52, 35, 55, 19, 32, 61, 33, 24, 57, 6, 22, 59, 29, 7, 49, 25, 40, 3, 17, 39, 27, 52, 0, 55, 16, 57, 24, 61, 36, 6, 29, 12, 48, 39, 20, 44, 6, 40, 33, 5, 48, 10, 57, 36, 22, 51, 33, 9, 24, 12, 62, 29, 50, 35, 14, 43, 5, 33, 47, 52, 13, 23, 10, 51, 56, 16, 46, 1, 49, 4, 61, 9, 52, 18, 31, 21, 36, 17 },
  { 19, 42, 9, 48, 2, 44, 11, 37, 48, 20, 33, 16, 55, 35, 49, 15, 37, 20, 59, 16, 53, 22, 56, 31, 50, 11, 34, 54, 16, 51, 4, 49, 33, 53, 21, 28, 56, 24, 31, 9, 52, 16, 48, 24, 44, 13, 51, 20, 31, 49, 18, 6, 34, 2, 44, 14, 47, 8, 15, 43, 13, 41, 33, 52, 20, 61, 7, 51, 34, 62, 4, 20, 36, 33, 43, 8, 46, 13, 53, 17, 45, 42, 9, 31, 52, 11, 30, 56, 13, 59, 17, 44, 27, 6, 62, 11, 43, 17, 49, 38, 26, 2, 16, 27, 58, 21, 54, 18, 26, 5, 35, 61, 43, 27, 7, 39, 14, 58, 37, 55, 20, 33, 13, 40, 62, 10, 55, 5 },
  { 51, 14, 61, 29, 59, 20, 55, 31, 0, 49, 11, 60, 3, 26, 22, 56, 0, 40, 12, 43, 41, 8, 36, 0, 17, 57, 24, 2, 46, 26, 61, 18, 0, 38, 12, 59, 6, 49, 3, 57, 19, 63, 5, 33, 18, 54, 28, 56, 0, 43, 26, 46, 63, 27, 56, 22, 27, 54, 38, 28, 63, 24, 10, 45, 0, 31, 42, 21, 12, 25, 44, 49, 59, 6, 26, 50, 3, 34, 27, 59, 0, 35, 62, 16, 4, 58, 47, 0, 43, 24, 37, 2, 54, 20, 46, 31, 0, 56, 34, 5, 55, 45, 60, 37, 0, 40, 10, 38, 63, 46, 15, 20, 0, 53, 21, 62, 30, 11, 24, 27, 40, 0, 57, 26, 3, 45, 27, 35 },
};

#else
#define DM_WIDTH 8
#define DM_WIDTH_SHIFT 3
#define DM_HEIGHT 8
static const unsigned char DM[8][8] =
{
  { 0,  32, 8,  40, 2,  34, 10, 42 },
  { 48, 16, 56, 24, 50, 18, 58, 26 },
  { 12, 44, 4,  36, 14, 46, 6,  38 },
  { 60, 28, 52, 20, 62, 30, 54, 22 },
  { 3,  35, 11, 43, 1,  33, 9,  41 },
  { 51, 19, 59, 27, 49, 17, 57, 25 },
  { 15, 47, 7,  39, 13, 45, 5,  37 },
  { 63, 31, 55, 23, 61, 29, 53, 21 }
};
#endif

static uint32 *DM_565 = NULL;

static void
xlib_rgb_preprocess_dm_565 (void)
{
  int i;
  uint32 dith;

  if (DM_565 == NULL)
    {
      DM_565 = malloc(sizeof(uint32) * DM_WIDTH * DM_HEIGHT);
      for (i = 0; i < DM_WIDTH * DM_HEIGHT; i++)
	{
	  dith = DM[0][i] >> 3;
	  DM_565[i] = (dith << 20) | dith | (((7 - dith) >> 1) << 10);
#ifdef VERBOSE
	  printf ("%i %x %x\n", i, dith, DM_565[i]);
#endif
	}
    }
}

static void
xlib_rgb_convert_8_d666 (XImage *image,
			int ax, int ay, int width, int height,
			unsigned char *buf, int rowstride,
			int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;
  const unsigned char *dmp;
  int dith;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  dith = (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) | 7;
	  r = ((r * 5) + dith) >> 8;
	  g = ((g * 5) + (262 - dith)) >> 8;
	  b = ((b * 5) + dith) >> 8;
	  obptr[0] = colorcube_d[(r << 6) | (g << 3) | b];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_8_d (XImage *image,
		     int ax, int ay, int width, int height,
		     unsigned char *buf, int rowstride,
		     int x_align, int y_align,
		     XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;
  const unsigned char *dmp;
  int dith;
  int rs, gs, bs;

  bptr = buf;
  bpl = image->bytes_per_line;
  rs = image_info->nred_shades - 1;
  gs = image_info->ngreen_shades - 1;
  bs = image_info->nblue_shades - 1;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  dith = (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) | 7;
	  r = ((r * rs) + dith) >> 8;
	  g = ((g * gs) + (262 - dith)) >> 8;
	  b = ((b * bs) + dith) >> 8;
	  obptr[0] = colorcube_d[(r << 6) | (g << 3) | b];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_8_indexed (XImage *image,
			   int ax, int ay, int width, int height,
			   unsigned char *buf, int rowstride,
			   int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  unsigned char c;
  unsigned char *lut;

  lut = cmap->lut;
  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  c = *bp2++;
	  obptr[0] = lut[c];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_gray8 (XImage *image,
		       int ax, int ay, int width, int height,
		       unsigned char *buf, int rowstride,
		       int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  obptr[0] = (g + ((b + r) >> 1)) >> 1;
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_gray8_gray (XImage *image,
			    int ax, int ay, int width, int height,
			    unsigned char *buf, int rowstride,
			    int x_align, int y_align, XlibRgbCmap *cmap)
{
  int y;
  int bpl;
  unsigned char *obuf;
  unsigned char *bptr;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  for (y = 0; y < height; y++)
    {
      memcpy (obuf, bptr, (unsigned int)width);
      bptr += rowstride;
      obuf += bpl;
    }
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define HAIRY_CONVERT_565
#endif

#ifdef HAIRY_CONVERT_565
/* Render a 24-bit RGB image in buf into the GdkImage, without dithering.
   This assumes native byte ordering - what should really be done is to
   check whether static_image->byte_order is consistent with the _ENDIAN
   config flag, and if not, use a different function.

   This one is even faster than the one below - its inner loop loads 3
   words (i.e. 4 24-bit pixels), does a lot of shifting and masking,
   then writes 2 words. */
static void
xlib_rgb_convert_565 (XImage *image,
		     int ax, int ay, int width, int height,
		     unsigned char *buf, int rowstride,
		     int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf, *obptr;
  int bpl;
  unsigned char *bptr, *bp2;
  unsigned char r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = 0; x < width; x++)
	    {
	      r = *bp2++;
	      g = *bp2++;
	      b = *bp2++;
	      ((uint16 *)obptr)[0] = ((r & 0xf8) << 8) |
		((g & 0xfc) << 3) |
		(b >> 3);
	      obptr += 2;
	    }
	}
      else
	{
	  for (x = 0; x < width - 3; x += 4)
	    {
	      uint32 r1b0g0r0;
	      uint32 g2r2b1g1;
	      uint32 b3g3r3b2;

	      r1b0g0r0 = ((uint32 *)bp2)[0];
	      g2r2b1g1 = ((uint32 *)bp2)[1];
	      b3g3r3b2 = ((uint32 *)bp2)[2];
	      ((uint32 *)obptr)[0] =
		((r1b0g0r0 & 0xf8) << 8) |
		((r1b0g0r0 & 0xfc00) >> 5) |
		((r1b0g0r0 & 0xf80000) >> 19) |
		(r1b0g0r0 & 0xf8000000) |
		((g2r2b1g1 & 0xfc) << 19) |
		((g2r2b1g1 & 0xf800) << 5);
	      ((uint32 *)obptr)[1] =
		((g2r2b1g1 & 0xf80000) >> 8) |
		((g2r2b1g1 & 0xfc000000) >> 21) |
		((b3g3r3b2 & 0xf8) >> 3) |
		((b3g3r3b2 & 0xf800) << 16) |
		((b3g3r3b2 & 0xfc0000) << 3) |
		((b3g3r3b2 & 0xf8000000) >> 11);
	      bp2 += 12;
	      obptr += 8;
	    }
	  for (; x < width; x++)
	    {
	      r = *bp2++;
	      g = *bp2++;
	      b = *bp2++;
	      ((uint16 *)obptr)[0] = ((r & 0xf8) << 8) |
		((g & 0xfc) << 3) |
		(b >> 3);
	      obptr += 2;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
/* Render a 24-bit RGB image in buf into the GdkImage, without dithering.
   This assumes native byte ordering - what should really be done is to
   check whether static_image->byte_order is consistent with the _ENDIAN
   config flag, and if not, use a different function.

   This routine is faster than the one included with Gtk 1.0 for a number
   of reasons:

   1. Shifting instead of lookup tables (less memory traffic).

   2. Much less register pressure, especially because shifts are
   in the code.

   3. A memcpy is avoided (i.e. the transfer function).

   4. On big-endian architectures, byte swapping is avoided.

   That said, it wouldn't be hard to make it even faster - just make an
   inner loop that reads 3 words (i.e. 4 24-bit pixels), does a lot of
   shifting and masking, then writes 2 words.
*/
static void
xlib_rgb_convert_565 (XImage *image,
		     int ax, int ay, int width, int height,
		     unsigned char *buf, int rowstride,
		     int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  unsigned char r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  ((unsigned short *)obuf)[x] = ((r & 0xf8) << 8) |
	    ((g & 0xfc) << 3) |
	    (b >> 3);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

#ifdef HAIRY_CONVERT_565
static void
xlib_rgb_convert_565_gray (XImage *image,
			  int ax, int ay, int width, int height,
			  unsigned char *buf, int rowstride,
			  int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf, *obptr;
  int bpl;
  unsigned char *bptr, *bp2;
  unsigned char g;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = 0; x < width; x++)
	    {
	      g = *bp2++;
	      ((uint16 *)obptr)[0] = ((g & 0xf8) << 8) |
		((g & 0xfc) << 3) |
		(g >> 3);
	      obptr += 2;
	    }
	}
      else
	{
	  for (x = 0; x < width - 3; x += 4)
	    {
	      uint32 g3g2g1g0;

	      g3g2g1g0 = ((uint32 *)bp2)[0];
	      ((uint32 *)obptr)[0] =
		((g3g2g1g0 & 0xf8) << 8) |
		((g3g2g1g0 & 0xfc) << 3) |
		((g3g2g1g0 & 0xf8) >> 3) |
		(g3g2g1g0 & 0xf800) << 16 |
		((g3g2g1g0 & 0xfc00) << 11) |
		((g3g2g1g0 & 0xf800) << 5);
	      ((uint32 *)obptr)[1] =
		((g3g2g1g0 & 0xf80000) >> 8) |
		((g3g2g1g0 & 0xfc0000) >> 13) |
		((g3g2g1g0 & 0xf80000) >> 19) |
		(g3g2g1g0 & 0xf8000000) |
		((g3g2g1g0 & 0xfc000000) >> 5) |
		((g3g2g1g0 & 0xf8000000) >> 11);
	      bp2 += 4;
	      obptr += 8;
	    }
	  for (; x < width; x++)
	    {
	      g = *bp2++;
	      ((uint16 *)obptr)[0] = ((g & 0xf8) << 8) |
		((g & 0xfc) << 3) |
		(g >> 3);
	      obptr += 2;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
static void
xlib_rgb_convert_565_gray (XImage *image,
			  int ax, int ay, int width, int height,
			  unsigned char *buf, int rowstride,
			  int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  unsigned char g;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  g = *bp2++;
	  ((uint16 *)obuf)[x] = ((g & 0xf8) << 8) |
	    ((g & 0xfc) << 3) |
	    (g >> 3);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

static void
xlib_rgb_convert_565_br (XImage *image,
			 int ax, int ay, int width, int height,
			 unsigned char *buf, int rowstride,
			 int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  unsigned char r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  /* final word is:
	     g4 g3 g2 b7 b6 b5 b4 b3  r7 r6 r5 r4 r3 g7 g6 g5
	   */
	  ((unsigned short *)obuf)[x] = (r & 0xf8) |
	    ((g & 0xe0) >> 5) |
	    ((g & 0x1c) << 11) |
	    ((b & 0xf8) << 5);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* Thanks to Ray Lehtiniemi for a patch that resulted in a ~25% speedup
   in this mode. */
#ifdef HAIRY_CONVERT_565
static void
xlib_rgb_convert_565_d (XImage *image,
		     int ax, int ay, int width, int height,
		     unsigned char *buf, int rowstride,
		     int x_align, int y_align, XlibRgbCmap *cmap)
{
  /* Now this is what I'd call some highly tuned code! */
  int x, y;
  unsigned char *obuf, *obptr;
  int bpl;
  unsigned char *bptr, *bp2;

  width += x_align;
  height += y_align;
  
  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 2;
  for (y = y_align; y < height; y++)
    {
      uint32 *dmp = DM_565 + ((y & (DM_HEIGHT - 1)) << DM_WIDTH_SHIFT);
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = x_align; x < width; x++)
	    {
	      int32 rgb = *bp2++ << 20;
	      rgb += *bp2++ << 10;
	      rgb += *bp2++;
	      rgb += dmp[x & (DM_WIDTH - 1)];
	      rgb += 0x10040100
		- ((rgb & 0x1e0001e0) >> 5)
		- ((rgb & 0x00070000) >> 6);

	      ((unsigned short *)obptr)[0] =
		((rgb & 0x0f800000) >> 12) |
		((rgb & 0x0003f000) >> 7) |
		((rgb & 0x000000f8) >> 3);
	      obptr += 2;
	    }
	}
      else
	{
	  for (x = x_align; x < width - 3; x += 4)
	    {
	      uint32 r1b0g0r0;
	      uint32 g2r2b1g1;
	      uint32 b3g3r3b2;
	      uint32 rgb02, rgb13;

	      r1b0g0r0 = ((uint32 *)bp2)[0];
	      g2r2b1g1 = ((uint32 *)bp2)[1];
	      b3g3r3b2 = ((uint32 *)bp2)[2];
	      rgb02 =
		((r1b0g0r0 & 0xff) << 20) +
		((r1b0g0r0 & 0xff00) << 2) +
		((r1b0g0r0 & 0xff0000) >> 16) +
		dmp[x & (DM_WIDTH - 1)];
	      rgb02 += 0x10040100
		- ((rgb02 & 0x1e0001e0) >> 5)
		- ((rgb02 & 0x00070000) >> 6);
	      rgb13 =
		((r1b0g0r0 & 0xff000000) >> 4) +
		((g2r2b1g1 & 0xff) << 10) +
		((g2r2b1g1 & 0xff00) >> 8) +
		dmp[(x + 1) & (DM_WIDTH - 1)];
	      rgb13 += 0x10040100
		- ((rgb13 & 0x1e0001e0) >> 5)
		- ((rgb13 & 0x00070000) >> 6);
	      ((uint32 *)obptr)[0] =
		((rgb02 & 0x0f800000) >> 12) |
		((rgb02 & 0x0003f000) >> 7) |
		((rgb02 & 0x000000f8) >> 3) |
		((rgb13 & 0x0f800000) << 4) |
		((rgb13 & 0x0003f000) << 9) |
		((rgb13 & 0x000000f8) << 13);
	      rgb02 =
		((g2r2b1g1 & 0xff0000) << 4) +
		((g2r2b1g1 & 0xff000000) >> 14) +
		(b3g3r3b2 & 0xff) +
		dmp[(x + 2) & (DM_WIDTH - 1)];
	      rgb02 += 0x10040100
		- ((rgb02 & 0x1e0001e0) >> 5)
		- ((rgb02 & 0x00070000) >> 6);
	      rgb13 =
		((b3g3r3b2 & 0xff00) << 12) +
		((b3g3r3b2 & 0xff0000) >> 6) +
		((b3g3r3b2 & 0xff000000) >> 24) +
		dmp[(x + 3) & (DM_WIDTH - 1)];
	      rgb13 += 0x10040100
		- ((rgb13 & 0x1e0001e0) >> 5)
		- ((rgb13 & 0x00070000) >> 6);
	      ((uint32 *)obptr)[1] =
		((rgb02 & 0x0f800000) >> 12) |
		((rgb02 & 0x0003f000) >> 7) |
		((rgb02 & 0x000000f8) >> 3) |
		((rgb13 & 0x0f800000) << 4) |
		((rgb13 & 0x0003f000) << 9) |
		((rgb13 & 0x000000f8) << 13);
	      bp2 += 12;
	      obptr += 8;
	    }
	  for (; x < width; x++)
	    {
	      int32 rgb = *bp2++ << 20;
	      rgb += *bp2++ << 10;
	      rgb += *bp2++;
	      rgb += dmp[x & (DM_WIDTH - 1)];
	      rgb += 0x10040100
		- ((rgb & 0x1e0001e0) >> 5)
		- ((rgb & 0x00070000) >> 6);

	      ((unsigned short *)obptr)[0] =
		((rgb & 0x0f800000) >> 12) |
		((rgb & 0x0003f000) >> 7) |
		((rgb & 0x000000f8) >> 3);
	      obptr += 2;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
static void
xlib_rgb_convert_565_d (XImage *image,
                       int ax, int ay, int width, int height,
                       unsigned char *buf, int rowstride,
                       int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr;

  width += x_align;
  height += y_align;
  
  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + (ax - x_align) * 2;

  for (y = y_align; y < height; y++)
    {
      uint32 *dmp = DM_565 + ((y & (DM_HEIGHT - 1)) << DM_WIDTH_SHIFT);
      unsigned char *bp2 = bptr;

      for (x = x_align; x < width; x++)
        {
          int32 rgb = *bp2++ << 20;
          rgb += *bp2++ << 10;
          rgb += *bp2++;
	  rgb += dmp[x & (DM_WIDTH - 1)];
          rgb += 0x10040100
            - ((rgb & 0x1e0001e0) >> 5)
            - ((rgb & 0x00070000) >> 6);

          ((unsigned short *)obuf)[x] =
            ((rgb & 0x0f800000) >> 12) |
            ((rgb & 0x0003f000) >> 7) |
            ((rgb & 0x000000f8) >> 3);
        }

      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

static void
xlib_rgb_convert_555 (XImage *image,
		     int ax, int ay, int width, int height,
		     unsigned char *buf, int rowstride,
		     int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  unsigned char r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  ((unsigned short *)obuf)[x] = ((r & 0xf8) << 7) |
	    ((g & 0xf8) << 2) |
	    (b >> 3);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_555_br (XImage *image,
			int ax, int ay, int width, int height,
			unsigned char *buf, int rowstride,
			int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  unsigned char r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  /* final word is:
	     g5 g4 g3 b7 b6 b5 b4 b3  0 r7 r6 r5 r4 r3 g7 g6
	   */
	  ((unsigned short *)obuf)[x] = ((r & 0xf8) >> 1) |
	    ((g & 0xc0) >> 6) |
	    ((g & 0x18) << 10) |
	    ((b & 0xf8) << 5);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_888_msb (XImage *image,
			 int ax, int ay, int width, int height,
			 unsigned char *buf, int rowstride,
			 int x_align, int y_align, XlibRgbCmap *cmap)
{
  int y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 3;
  for (y = 0; y < height; y++)
    {
      memcpy (obuf, bptr, (unsigned int)(width + width + width));
      bptr += rowstride;
      obuf += bpl;
    }
}

/* todo: optimize this */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define HAIRY_CONVERT_888
#endif

#ifdef HAIRY_CONVERT_888
static void
xlib_rgb_convert_888_lsb (XImage *image,
			  int ax, int ay, int width, int height,
			  unsigned char *buf, int rowstride,
			  int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf, *obptr;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 3;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = 0; x < width; x++)
	    {
	      r = bp2[0];
	      g = bp2[1];
	      b = bp2[2];
	      *obptr++ = b;
	      *obptr++ = g;
	      *obptr++ = r;
	      bp2 += 3;
	    }
	}
      else
	{
	  for (x = 0; x < width - 3; x += 4)
	    {
	      uint32 r1b0g0r0;
	      uint32 g2r2b1g1;
	      uint32 b3g3r3b2;

	      r1b0g0r0 = ((uint32 *)bp2)[0];
	      g2r2b1g1 = ((uint32 *)bp2)[1];
	      b3g3r3b2 = ((uint32 *)bp2)[2];
	      ((uint32 *)obptr)[0] =
		(r1b0g0r0 & 0xff00) |
		((r1b0g0r0 & 0xff0000) >> 16) |
		(((g2r2b1g1 & 0xff00) | (r1b0g0r0 & 0xff)) << 16);
	      ((uint32 *)obptr)[1] =
		(g2r2b1g1 & 0xff0000ff) |
		((r1b0g0r0 & 0xff000000) >> 16) |
		((b3g3r3b2 & 0xff) << 16);
	      ((uint32 *)obptr)[2] =
		(((g2r2b1g1 & 0xff0000) | (b3g3r3b2 & 0xff000000)) >> 16) |
		((b3g3r3b2 & 0xff00) << 16) |
		((b3g3r3b2 & 0xff0000));
	      bp2 += 12;
	      obptr += 12;
	    }
	  for (; x < width; x++)
	    {
	      r = bp2[0];
	      g = bp2[1];
	      b = bp2[2];
	      *obptr++ = b;
	      *obptr++ = g;
	      *obptr++ = r;
	      bp2 += 3;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
static void
xlib_rgb_convert_888_lsb (XImage *image,
			 int ax, int ay, int width, int height,
			 unsigned char *buf, int rowstride,
			 int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 3;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  obuf[x * 3] = b;
	  obuf[x * 3 + 1] = g;
	  obuf[x * 3 + 2] = r;
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

/* convert 24-bit packed to 32-bit unpacked */
/* todo: optimize this */
static void
xlib_rgb_convert_0888 (XImage *image,
		      int ax, int ay, int width, int height,
		      unsigned char *buf, int rowstride,
		      int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 4;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  ((uint32 *)obuf)[x] = (r << 16) | (g << 8) | b;
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_0888_br (XImage *image,
			 int ax, int ay, int width, int height,
			 unsigned char *buf, int rowstride,
			 int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 4;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  ((uint32 *)obuf)[x] = (b << 24) | (g << 16) | (r << 8);
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_8880_br (XImage *image,
			 int ax, int ay, int width, int height,
			 unsigned char *buf, int rowstride,
			 int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * 4;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  ((uint32 *)obuf)[x] = (b << 16) | (g << 8) | r;
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* Generic truecolor/directcolor conversion function. Slow, but these
   are oddball modes. */
static void
xlib_rgb_convert_truecolor_lsb (XImage *image,
			       int ax, int ay, int width, int height,
			       unsigned char *buf, int rowstride,
			       int x_align, int y_align,
			       XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf, *obptr;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;
  int r_right, r_left;
  int g_right, g_left;
  int b_right, b_left;
  int bpp;
  uint32 pixel;
  int i;

  r_right = 8 - image_info->red_prec;
  r_left = image_info->red_shift;
  g_right = 8 - image_info->green_prec;
  g_left = image_info->green_shift;
  b_right = 8 - image_info->blue_prec;
  b_left = image_info->blue_shift;
  bpp = image_info->bpp;
  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * bpp;
  for (y = 0; y < height; y++)
    {
      obptr = obuf;
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  pixel = ((r >> r_right) << r_left) |
	    ((g >> g_right) << g_left) |
	    ((b >> b_right) << b_left);
	  for (i = 0; i < bpp; i++)
	    {
	      *obptr++ = pixel & 0xff;
	      pixel >>= 8;
	    }
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_truecolor_lsb_d (XImage *image,
				 int ax, int ay, int width, int height,
				 unsigned char *buf, int rowstride,
				 int x_align, int y_align,
				 XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf, *obptr;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;
  int r_right, r_left, r_prec;
  int g_right, g_left, g_prec;
  int b_right, b_left, b_prec;
  int bpp;
  uint32 pixel;
  int i;
  int dith;
  int r1, g1, b1;
  const unsigned char *dmp;

  r_right = 8 - image_info->red_prec;
  r_left = image_info->red_shift;
  r_prec = image_info->red_prec;
  g_right = 8 - image_info->green_prec;
  g_left = image_info->green_shift;
  g_prec = image_info->green_prec;
  b_right = 8 - image_info->blue_prec;
  b_left = image_info->blue_shift;
  b_prec = image_info->blue_prec;
  bpp = image_info->bpp;
  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * bpp;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      obptr = obuf;
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  dith = dmp[(x_align + x) & (DM_WIDTH - 1)] << 2;
	  r1 = r + (dith >> r_prec);
	  g1 = g + ((252 - dith) >> g_prec);
	  b1 = b + (dith >> b_prec);
	  pixel = (((r1 - (r1 >> r_prec)) >> r_right) << r_left) |
	    (((g1 - (g1 >> g_prec)) >> g_right) << g_left) |
	    (((b1 - (b1 >> b_prec)) >> b_right) << b_left);
	  for (i = 0; i < bpp; i++)
	    {
	      *obptr++ = pixel & 0xff;
	      pixel >>= 8;
	    }
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_truecolor_msb (XImage *image,
			       int ax, int ay, int width, int height,
			       unsigned char *buf, int rowstride,
			       int x_align, int y_align,
			       XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf, *obptr;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;
  int r_right, r_left;
  int g_right, g_left;
  int b_right, b_left;
  int bpp;
  uint32 pixel;
  int shift, shift_init;

  r_right = 8 - image_info->red_prec;
  r_left = image_info->red_shift;
  g_right = 8 - image_info->green_prec;
  g_left = image_info->green_shift;
  b_right = 8 - image_info->blue_prec;
  b_left = image_info->blue_shift;
  bpp = image_info->bpp;
  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * bpp;
  shift_init = (bpp - 1) << 3;
  for (y = 0; y < height; y++)
    {
      obptr = obuf;
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  pixel = ((r >> r_right) << r_left) |
	    ((g >> g_right) << g_left) |
	    ((b >> b_right) << b_left);
	  for (shift = shift_init; shift >= 0; shift -= 8)
	    {
	      *obptr++ = (pixel >> shift) & 0xff;
	    }
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_truecolor_msb_d (XImage *image,
				 int ax, int ay, int width, int height,
				 unsigned char *buf, int rowstride,
				 int x_align, int y_align,
				 XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *obuf, *obptr;
  int bpl;
  unsigned char *bptr, *bp2;
  int r, g, b;
  int r_right, r_left, r_prec;
  int g_right, g_left, g_prec;
  int b_right, b_left, b_prec;
  int bpp;
  uint32 pixel;
  int shift, shift_init;
  int dith;
  int r1, g1, b1;
  const unsigned char *dmp;

  r_right = 8 - image_info->red_prec;
  r_left = image_info->red_shift;
  r_prec = image_info->red_prec;
  g_right = 8 - image_info->green_prec;
  g_left = image_info->green_shift;
  g_prec = image_info->green_prec;
  b_right = 8 - image_info->blue_prec;
  b_left = image_info->blue_shift;
  b_prec = image_info->blue_prec;
  bpp = image_info->bpp;
  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax * bpp;
  shift_init = (bpp - 1) << 3;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      obptr = obuf;
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  dith = dmp[(x_align + x) & (DM_WIDTH - 1)] << 2;
	  r1 = r + (dith >> r_prec);
	  g1 = g + ((252 - dith) >> g_prec);
	  b1 = b + (dith >> b_prec);
	  pixel = (((r1 - (r1 >> r_prec)) >> r_right) << r_left) |
	    (((g1 - (g1 >> g_prec)) >> g_right) << g_left) |
	    (((b1 - (b1 >> b_prec)) >> b_right) << b_left);
	  for (shift = shift_init; shift >= 0; shift -= 8)
	    {
	      *obptr++ = (pixel >> shift) & 0xff;
	    }
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* This actually works for depths from 3 to 7 */
static void
xlib_rgb_convert_4 (XImage *image,
		   int ax, int ay, int width, int height,
		   unsigned char *buf, int rowstride,
		   int x_align, int y_align,
		   XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;
  const unsigned char *dmp;
  int dith;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x += 1)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  dith = (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) | 3;
	  obptr[0] = colorcube_d[(((r + dith) & 0x100) >> 2) |
				(((g + 258 - dith) & 0x100) >> 5) |
				(((b + dith) & 0x100) >> 8)];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* This actually works for depths from 3 to 7 */
static void
xlib_rgb_convert_gray4 (XImage *image,
		       int ax, int ay, int width, int height,
		       unsigned char *buf, int rowstride,
		       int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;
  int shift;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  shift = 9 - image_info->x_visual_info->depth;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  obptr[0] = (g + ((b + r) >> 1)) >> shift;
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_gray4_pack (XImage *image,
			    int ax, int ay, int width, int height,
			    unsigned char *buf, int rowstride,
			    int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;
  int shift;
  unsigned char pix0, pix1;
  /* todo: this is hardcoded to big-endian. Make endian-agile. */

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + (ax >> 1);
  shift = 9 - image_info->x_visual_info->depth;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x += 2)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  pix0 = (g + ((b + r) >> 1)) >> shift;
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  pix1 = (g + ((b + r) >> 1)) >> shift;
	  obptr[0] = (pix0 << 4) | pix1;
	  obptr++;
	}
      if (width & 1)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  pix0 = (g + ((b + r) >> 1)) >> shift;
	  obptr[0] = (pix0 << 4);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* This actually works for depths from 3 to 7 */
static void
xlib_rgb_convert_gray4_d (XImage *image,
		       int ax, int ay, int width, int height,
		       unsigned char *buf, int rowstride,
		       int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;
  const unsigned char *dmp;
  int prec, right;
  int gray;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + ax;
  prec = image_info->x_visual_info->depth;
  right = 8 - prec;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  gray = (g + ((b + r) >> 1)) >> 1;
	  gray += (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) >> prec;
	  obptr[0] = (gray - (gray >> prec)) >> right;
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_gray4_d_pack (XImage *image,
			      int ax, int ay, int width, int height,
			      unsigned char *buf, int rowstride,
			      int x_align, int y_align, XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;
  const unsigned char *dmp;
  int prec, right;
  int gray;
  unsigned char pix0, pix1;
  /* todo: this is hardcoded to big-endian. Make endian-agile. */

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + (ax >> 1);
  prec = image_info->x_visual_info->depth;
  right = 8 - prec;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      for (x = 0; x < width; x += 2)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  gray = (g + ((b + r) >> 1)) >> 1;
	  gray += (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) >> prec;
	  pix0 = (gray - (gray >> prec)) >> right;
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  gray = (g + ((b + r) >> 1)) >> 1;
	  gray += (dmp[(x_align + x + 1) & (DM_WIDTH - 1)] << 2) >> prec;
	  pix1 = (gray - (gray >> prec)) >> right;
	  obptr[0] = (pix0 << 4) | pix1;
	  obptr++;
	}
      if (width & 1)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  gray = (g + ((b + r) >> 1)) >> 1;
	  gray += (dmp[(x_align + x + 1) & (DM_WIDTH - 1)] << 2) >> prec;
	  pix0 = (gray - (gray >> prec)) >> right;
	  obptr[0] = (pix0 << 4);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
xlib_rgb_convert_1 (XImage *image,
		   int ax, int ay, int width, int height,
		   unsigned char *buf, int rowstride,
		   int x_align, int y_align,
		   XlibRgbCmap *cmap)
{
  int x, y;
  int bpl;
  unsigned char *obuf, *obptr;
  unsigned char *bptr, *bp2;
  int r, g, b;
  const unsigned char *dmp;
  int dith;
  unsigned char byte;

  bptr = buf;
  bpl = image->bytes_per_line;
  obuf = ((unsigned char *)image->data) + ay * bpl + (ax >> 3);
  byte = 0; /* unnecessary, but it keeps gcc from complaining */
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  dith = (dmp[(x_align + x) & (DM_WIDTH - 1)] << 4) | 4;
	  byte += byte + (r + g + g + b + dith > 1020);
	  if ((x & 7) == 7)
	    {
	      obptr[0] = byte;
	      obptr++;
	    }
	}
      if (x & 7)
	obptr[0] = byte << (8 - (x & 7));
      bptr += rowstride;
      obuf += bpl;
    }
}

/* Returns a pointer to the stage buffer. */
static unsigned char *
xlib_rgb_ensure_stage (void)
{
  if (image_info->stage_buf == NULL)
    image_info->stage_buf = malloc (IMAGE_HEIGHT * STAGE_ROWSTRIDE);
  return image_info->stage_buf;
}

/* This is slow. Speed me up, please. */
static void
xlib_rgb_32_to_stage (unsigned char *buf, int rowstride, int width, int height)
{
  int x, y;
  unsigned char *pi_start, *po_start;
  unsigned char *pi, *po;

  pi_start = buf;
  po_start = xlib_rgb_ensure_stage ();
  for (y = 0; y < height; y++)
    {
      pi = pi_start;
      po = po_start;
      for (x = 0; x < width; x++)
	{
	  *po++ = *pi++;
	  *po++ = *pi++;
	  *po++ = *pi++;
	  pi++;
	}
      pi_start += rowstride;
      po_start += STAGE_ROWSTRIDE;
    }
}

/* Generic 32bit RGB conversion function - convert to 24bit packed, then
   go from there. */
static void
xlib_rgb_convert_32_generic (XImage *image,
			    int ax, int ay, int width, int height,
			    unsigned char *buf, int rowstride,
			    int x_align, int y_align, XlibRgbCmap *cmap)
{
  xlib_rgb_32_to_stage (buf, rowstride, width, height);

  (*image_info->conv) (image, ax, ay, width, height,
		       image_info->stage_buf, STAGE_ROWSTRIDE,
		       x_align, y_align, cmap);
}

/* Generic 32bit RGB conversion function - convert to 24bit packed, then
   go from there. */
static void
xlib_rgb_convert_32_generic_d (XImage *image,
			      int ax, int ay, int width, int height,
			      unsigned char *buf, int rowstride,
			      int x_align, int y_align, XlibRgbCmap *cmap)
{
  xlib_rgb_32_to_stage (buf, rowstride, width, height);

  (*image_info->conv_d) (image, ax, ay, width, height,
			 image_info->stage_buf, STAGE_ROWSTRIDE,
			 x_align, y_align, cmap);
}

/* This is slow. Speed me up, please. */
static void
xlib_rgb_gray_to_stage (unsigned char *buf, int rowstride, int width, int height)
{
  int x, y;
  unsigned char *pi_start, *po_start;
  unsigned char *pi, *po;
  unsigned char gray;

  pi_start = buf;
  po_start = xlib_rgb_ensure_stage ();
  for (y = 0; y < height; y++)
    {
      pi = pi_start;
      po = po_start;
      for (x = 0; x < width; x++)
	{
	  gray = *pi++;
	  *po++ = gray;
	  *po++ = gray;
	  *po++ = gray;
	}
      pi_start += rowstride;
      po_start += STAGE_ROWSTRIDE;
    }
}

/* Generic gray conversion function - convert to 24bit packed, then go
   from there. */
static void
xlib_rgb_convert_gray_generic (XImage *image,
			      int ax, int ay, int width, int height,
			      unsigned char *buf, int rowstride,
			      int x_align, int y_align, XlibRgbCmap *cmap)
{
  xlib_rgb_gray_to_stage (buf, rowstride, width, height);

  (*image_info->conv) (image, ax, ay, width, height,
		       image_info->stage_buf, STAGE_ROWSTRIDE,
		       x_align, y_align, cmap);
}

static void
xlib_rgb_convert_gray_generic_d (XImage *image,
				int ax, int ay, int width, int height,
				unsigned char *buf, int rowstride,
				int x_align, int y_align, XlibRgbCmap *cmap)
{
  xlib_rgb_gray_to_stage (buf, rowstride, width, height);

  (*image_info->conv_d) (image, ax, ay, width, height,
			 image_info->stage_buf, STAGE_ROWSTRIDE,
			 x_align, y_align, cmap);
}

/* Render grayscale using indexed method. */
static void
xlib_rgb_convert_gray_cmap (XImage *image,
			   int ax, int ay, int width, int height,
			   unsigned char *buf, int rowstride,
			   int x_align, int y_align, XlibRgbCmap *cmap)
{
  (*image_info->conv_indexed) (image, ax, ay, width, height,
			       buf, rowstride,
			       x_align, y_align, image_info->gray_cmap);
}

#if 0
static void
xlib_rgb_convert_gray_cmap_d (XImage *image,
				int ax, int ay, int width, int height,
				unsigned char *buf, int rowstride,
				int x_align, int y_align, XlibRgbCmap *cmap)
{
  (*image_info->conv_indexed_d) (image, ax, ay, width, height,
				 buf, rowstride,
				 x_align, y_align, image_info->gray_cmap);
}
#endif

/* This is slow. Speed me up, please. */
static void
xlib_rgb_indexed_to_stage (unsigned char *buf, int rowstride, int width, int height,
			  XlibRgbCmap *cmap)
{
  int x, y;
  unsigned char *pi_start, *po_start;
  unsigned char *pi, *po;
  int rgb;

  pi_start = buf;
  po_start = xlib_rgb_ensure_stage ();
  for (y = 0; y < height; y++)
    {
      pi = pi_start;
      po = po_start;
      for (x = 0; x < width; x++)
	{
	  rgb = cmap->colors[*pi++];
	  *po++ = rgb >> 16;
	  *po++ = (rgb >> 8) & 0xff;
	  *po++ = rgb & 0xff;
	}
      pi_start += rowstride;
      po_start += STAGE_ROWSTRIDE;
    }
}

/* Generic gray conversion function - convert to 24bit packed, then go
   from there. */
static void
xlib_rgb_convert_indexed_generic (XImage *image,
				 int ax, int ay, int width, int height,
				 unsigned char *buf, int rowstride,
				 int x_align, int y_align, XlibRgbCmap *cmap)
{
  xlib_rgb_indexed_to_stage (buf, rowstride, width, height, cmap);

  (*image_info->conv) (image, ax, ay, width, height,
		       image_info->stage_buf, STAGE_ROWSTRIDE,
		       x_align, y_align, cmap);
}

static void
xlib_rgb_convert_indexed_generic_d (XImage *image,
				   int ax, int ay, int width, int height,
				   unsigned char *buf, int rowstride,
				   int x_align, int y_align,
				   XlibRgbCmap *cmap)
{
  xlib_rgb_indexed_to_stage (buf, rowstride, width, height, cmap);

  (*image_info->conv_d) (image, ax, ay, width, height,
			 image_info->stage_buf, STAGE_ROWSTRIDE,
			 x_align, y_align, cmap);
}

/* Select a conversion function based on the visual and a
   representative image. */
static void
xlib_rgb_select_conv (XImage *image, ByteOrder byte_order)
{
  int depth, byterev;
  int vtype; /* visual type */
  int bpp; /* bits per pixel - from the visual */
  uint32 red_mask, green_mask, blue_mask;
  XlibRgbConvFunc conv, conv_d;
  XlibRgbConvFunc conv_32, conv_32_d;
  XlibRgbConvFunc conv_gray, conv_gray_d;
  XlibRgbConvFunc conv_indexed, conv_indexed_d;
  Bool mask_rgb, mask_bgr;

  depth = image_info->x_visual_info->depth;
  bpp = image->bits_per_pixel;
  if (xlib_rgb_verbose)
    printf ("Chose visual 0x%x, image bpp=%d, %s first\n",
	    (int)image_info->x_visual_info->visual->visualid,
	    bpp, byte_order == LSB_FIRST ? "lsb" : "msb");

#if G_BYTE_ORDER == G_BIG_ENDIAN
  byterev = (byte_order == LSB_FIRST);
#else
  byterev = (byte_order == MSB_FIRST);
#endif

  vtype = image_info->x_visual_info->class;
  if (vtype == DirectColor)
    vtype = TrueColor;

  red_mask = image_info->x_visual_info->red_mask;
  green_mask = image_info->x_visual_info->green_mask;
  blue_mask = image_info->x_visual_info->blue_mask;

  mask_rgb = red_mask == 0xff0000 && green_mask == 0xff00 && blue_mask == 0xff;
  mask_bgr = red_mask == 0xff && green_mask == 0xff00 && blue_mask == 0xff0000;

  conv = NULL;
  conv_d = NULL;

  conv_32 = xlib_rgb_convert_32_generic;
  conv_32_d = xlib_rgb_convert_32_generic_d;

  conv_gray = xlib_rgb_convert_gray_generic;
  conv_gray_d = xlib_rgb_convert_gray_generic_d;

  conv_indexed = xlib_rgb_convert_indexed_generic;
  conv_indexed_d = xlib_rgb_convert_indexed_generic_d;

  image_info->dith_default = FALSE;

  if (image_info->bitmap)
    conv = xlib_rgb_convert_1;
  else if (bpp == 16 && depth == 16 && !byterev &&
      red_mask == 0xf800 && green_mask == 0x7e0 && blue_mask == 0x1f)
    {
      conv = xlib_rgb_convert_565;
      conv_d = xlib_rgb_convert_565_d;
      conv_gray = xlib_rgb_convert_565_gray;
      xlib_rgb_preprocess_dm_565 ();
    }
  else if (bpp == 16 && depth == 16 &&
	   vtype == TrueColor&& byterev &&
      red_mask == 0xf800 && green_mask == 0x7e0 && blue_mask == 0x1f)
    conv = xlib_rgb_convert_565_br;

  else if (bpp == 16 && depth == 15 &&
	   vtype == TrueColor && !byterev &&
      red_mask == 0x7c00 && green_mask == 0x3e0 && blue_mask == 0x1f)
    conv = xlib_rgb_convert_555;

  else if (bpp == 16 && depth == 15 &&
	   vtype == TrueColor && byterev &&
      red_mask == 0x7c00 && green_mask == 0x3e0 && blue_mask == 0x1f)
    conv = xlib_rgb_convert_555_br;

  /* I'm not 100% sure about the 24bpp tests - but testing will show*/
  else if (bpp == 24 && depth == 24 && vtype == TrueColor &&
	   ((mask_rgb && byte_order == LSB_FIRST) ||
	    (mask_bgr && byte_order == MSB_FIRST)))
    conv = xlib_rgb_convert_888_lsb;
  else if (bpp == 24 && depth == 24 && vtype == TrueColor &&
	   ((mask_rgb && byte_order == MSB_FIRST) ||
	    (mask_bgr && byte_order == LSB_FIRST)))
    conv = xlib_rgb_convert_888_msb;
#if G_BYTE_ORDER == G_BIG_ENDIAN
  else if (bpp == 32 && depth == 24 && vtype == TrueColor &&
	   (mask_rgb && byte_order == LSB_FIRST))
    conv = xlib_rgb_convert_0888_br;
  else if (bpp == 32 && depth == 24 && vtype == TrueColor &&
	   (mask_rgb && byte_order == MSB_FIRST))
    conv = xlib_rgb_convert_0888;
  else if (bpp == 32 && depth == 24 && vtype == TrueColor &&
	   (mask_bgr && byte_order == MSB_FIRST))
    conv = xlib_rgb_convert_8880_br;
#else
  else if (bpp == 32 && depth == 24 && vtype == TrueColor &&
	   (mask_rgb && byte_order == MSB_FIRST))
    conv = xlib_rgb_convert_0888_br;
  else if (bpp == 32 && (depth == 32 || depth == 24) && vtype == TrueColor &&
	   (mask_rgb && byte_order == LSB_FIRST))
    conv = xlib_rgb_convert_0888;
  else if (bpp == 32 && depth == 24 && vtype == TrueColor &&
	   (mask_bgr && byte_order == LSB_FIRST))
    conv = xlib_rgb_convert_8880_br;
#endif

  else if (vtype == TrueColor && byte_order == LSB_FIRST)
    {
      conv = xlib_rgb_convert_truecolor_lsb;
      conv_d = xlib_rgb_convert_truecolor_lsb_d;
    }
  else if (vtype == TrueColor && byte_order == MSB_FIRST)
    {
      conv = xlib_rgb_convert_truecolor_msb;
      conv_d = xlib_rgb_convert_truecolor_msb_d;
    }
  else if (bpp == 8 && depth == 8 && (vtype == PseudoColor
#ifdef ENABLE_GRAYSCALE
				      || vtype == GrayScale
#endif
				      ))
    {
      image_info->dith_default = TRUE;
      conv = xlib_rgb_convert_8;
      if (vtype != GrayScale)
	{
	  if (image_info->nred_shades == 6 &&
	      image_info->ngreen_shades == 6 &&
	      image_info->nblue_shades == 6)
	    conv_d = xlib_rgb_convert_8_d666;
	  else
	    conv_d = xlib_rgb_convert_8_d;
	}
      conv_indexed = xlib_rgb_convert_8_indexed;
      conv_gray = xlib_rgb_convert_gray_cmap;
    }
  else if (bpp == 8 && depth == 8 && (vtype == StaticGray
#ifdef not_ENABLE_GRAYSCALE
				      || vtype == GrayScale
#endif
				      ))
    {
      conv = xlib_rgb_convert_gray8;
      conv_gray = xlib_rgb_convert_gray8_gray;
    }
  else if (bpp == 8 && depth < 8 && depth >= 2 &&
	   (vtype == StaticGray
	    || vtype == GrayScale))
    {
      conv = xlib_rgb_convert_gray4;
      conv_d = xlib_rgb_convert_gray4_d;
    }
  else if (bpp == 8 && depth < 8 && depth >= 3)
    {
      conv = xlib_rgb_convert_4;
    }
  else if (bpp == 4 && depth <= 4 && depth >= 2 &&
	   (vtype == StaticGray
	    || vtype == GrayScale))
    {
      conv = xlib_rgb_convert_gray4_pack;
      conv_d = xlib_rgb_convert_gray4_d_pack;
    }

  if (conv_d == NULL)
    conv_d = conv;

  image_info->conv = conv;
  image_info->conv_d = conv_d;

  image_info->conv_32 = conv_32;
  image_info->conv_32_d = conv_32_d;

  image_info->conv_gray = conv_gray;
  image_info->conv_gray_d = conv_gray_d;

  image_info->conv_indexed = conv_indexed;
  image_info->conv_indexed_d = conv_indexed_d;
}

static int horiz_idx;
static int horiz_y = IMAGE_HEIGHT;
static int vert_idx;
static int vert_x = IMAGE_WIDTH;
static int tile_idx;
static int tile_x = IMAGE_WIDTH;
static int tile_y1 = IMAGE_HEIGHT;
static int tile_y2 = IMAGE_HEIGHT;

#ifdef VERBOSE
static int sincelast;
#endif

/* Defining NO_FLUSH can cause inconsistent screen updates, but is useful
   for performance evaluation. */

#undef NO_FLUSH

static int
xlib_rgb_alloc_scratch_image (void)
{
  if (static_image_idx == N_IMAGES)
    {
#ifndef NO_FLUSH
      XFlush(image_info->display);
#endif
#ifdef VERBOSE
      printf ("flush, %d puts since last flush\n", sincelast);
      sincelast = 0;
#endif
      static_image_idx = 0;
      horiz_y = IMAGE_HEIGHT;
      vert_x = IMAGE_WIDTH;
      tile_x = IMAGE_WIDTH;
      tile_y1 = tile_y2 = IMAGE_HEIGHT;
    }
  return static_image_idx++;
}

static XImage *
xlib_rgb_alloc_scratch (int width, int height, int *ax, int *ay)
{
  XImage *image;
  int idx;

  if (width >= (IMAGE_WIDTH >> 1))
    {
      if (height >= (IMAGE_HEIGHT >> 1))
	{
	  idx = xlib_rgb_alloc_scratch_image ();
	  *ax = 0;
	  *ay = 0;
	}
      else
	{
	  if (height + horiz_y > IMAGE_HEIGHT)
	    {
	      horiz_idx = xlib_rgb_alloc_scratch_image ();
	      horiz_y = 0;
	    }
	  idx = horiz_idx;
	  *ax = 0;
	  *ay = horiz_y;
	  horiz_y += height;
	}
    }
  else
    {
      if (height >= (IMAGE_HEIGHT >> 1))
	{
	  if (width + vert_x > IMAGE_WIDTH)
	    {
	      vert_idx = xlib_rgb_alloc_scratch_image ();
	      vert_x = 0;
	    }
	  idx = vert_idx;
	  *ax = vert_x;
	  *ay = 0;
	  /* using 3 and -4 would be slightly more efficient on 32-bit machines
	     with > 1bpp displays */
	  vert_x += (width + 7) & -8;
	}
      else
	{
	  if (width + tile_x > IMAGE_WIDTH)
	    {
	      tile_y1 = tile_y2;
	      tile_x = 0;
	    }
	  if (height + tile_y1 > IMAGE_HEIGHT)
	    {
	      tile_idx = xlib_rgb_alloc_scratch_image ();
	      tile_x = 0;
	      tile_y1 = 0;
	      tile_y2 = 0;
	    }
	  if (height + tile_y1 > tile_y2)
	    tile_y2 = height + tile_y1;
	  idx = tile_idx;
	  *ax = tile_x;
	  *ay = tile_y1;
	  tile_x += (width + 7) & -8;
	}
    }
  image = static_image[idx];
#ifdef VERBOSE
  printf ("index %d, x %d, y %d (%d x %d)\n", idx, *ax, *ay, width, height);
  sincelast++;
#endif
  return image;
}

static void
xlib_draw_rgb_image_core (Drawable drawable,
			  GC gc,
			  int x,
			  int y,
			  int width,
			  int height,
			  unsigned char *buf,
			  int pixstride,
			  int rowstride,
			  XlibRgbConvFunc conv,
			  XlibRgbCmap *cmap,
			  int xdith,
			  int ydith)
{
  int ay, ax;
  int xs0, ys0;
  XImage *image;
  int width1, height1;
  unsigned char *buf_ptr;

  if (image_info->bitmap)
    {
      if (image_info->own_gc == 0)
	{
	  XColor color;

	  image_info->own_gc = XCreateGC(image_info->display,
					 drawable,
					 0, NULL);
	  color.pixel = WhitePixel(image_info->display,
				   image_info->screen_num);
	  XSetForeground(image_info->display, image_info->own_gc, color.pixel);
	  color.pixel = BlackPixel(image_info->display,
				   image_info->screen_num);
	  XSetBackground(image_info->display, image_info->own_gc, color.pixel);
	}
      gc = image_info->own_gc;
    }
  for (ay = 0; ay < height; ay += IMAGE_HEIGHT)
    {
      height1 = MIN (height - ay, IMAGE_HEIGHT);
      for (ax = 0; ax < width; ax += IMAGE_WIDTH)
	{
	  width1 = MIN (width - ax, IMAGE_WIDTH);
	  buf_ptr = buf + ay * rowstride + ax * pixstride;

	  image = xlib_rgb_alloc_scratch (width1, height1, &xs0, &ys0);

	  conv (image, xs0, ys0, width1, height1, buf_ptr, rowstride,
		x + ax + xdith, y + ay + ydith, cmap);

#ifndef DONT_ACTUALLY_DRAW
	  XPutImage(image_info->display, drawable, gc, image,
		    xs0, ys0, x + ax, y + ay, (unsigned int)width1, (unsigned int)height1);
#endif
	}
    }
}


void
xlib_draw_rgb_image (Drawable drawable,
		     GC gc,
		     int x,
		     int y,
		     int width,
		     int height,
		     XlibRgbDither dith,
		     unsigned char *rgb_buf,
		     int rowstride)
{
  if (dith == XLIB_RGB_DITHER_NONE || (dith == XLIB_RGB_DITHER_NORMAL &&
				      !image_info->dith_default))
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      rgb_buf, 3, rowstride, image_info->conv, NULL,
			      0, 0);
  else
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      rgb_buf, 3, rowstride, image_info->conv_d, NULL,
			      0, 0);
}

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
			      int ydith)
{
  if (dith == XLIB_RGB_DITHER_NONE || (dith == XLIB_RGB_DITHER_NORMAL &&
				       !image_info->dith_default))
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      rgb_buf, 3, rowstride, image_info->conv, NULL,
			      xdith, ydith);
  else
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      rgb_buf, 3, rowstride, image_info->conv_d, NULL,
			      xdith, ydith);
}

void
xlib_draw_rgb_32_image (Drawable drawable,
			GC gc,
			int x,
			int y,
			int width,
			int height,
			XlibRgbDither dith,
			unsigned char *buf,
			int rowstride)
{
  if (dith == XLIB_RGB_DITHER_NONE || (dith == XLIB_RGB_DITHER_NORMAL &&
				       !image_info->dith_default))
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      buf, 4, rowstride,
			      image_info->conv_32, NULL, 0, 0);
  else
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      buf, 4, rowstride,
			      image_info->conv_32_d, NULL, 0, 0);
}

static void
xlib_rgb_make_gray_cmap (XlibRgbInfo *info)
{
  uint32 rgb[256];
  int i;

  for (i = 0; i < 256; i++)
    rgb[i] = (i << 16)  | (i << 8) | i;
  info->gray_cmap = xlib_rgb_cmap_new (rgb, 256);
}

void
xlib_draw_gray_image (Drawable drawable,
		      GC gc,
		      int x,
		      int y,
		      int width,
		      int height,
		      XlibRgbDither dith,
		      unsigned char *buf,
		      int rowstride)
{
  if (image_info->bpp == 1 &&
      image_info->gray_cmap == NULL &&
      (image_info->x_visual_info->class == PseudoColor ||
       image_info->x_visual_info->class == GrayScale))
    xlib_rgb_make_gray_cmap (image_info);
  
  if (dith == XLIB_RGB_DITHER_NONE || (dith == XLIB_RGB_DITHER_NORMAL &&
				      !image_info->dith_default))
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      buf, 1, rowstride,
			      image_info->conv_gray, NULL, 0, 0);
  else
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      buf, 1, rowstride,
			      image_info->conv_gray_d, NULL, 0, 0);
}

XlibRgbCmap *
xlib_rgb_cmap_new (uint32 *colors, int n_colors)
{
  XlibRgbCmap *cmap;
  int i, j;
  uint32 rgb;

  if (n_colors < 0)
    return NULL;
  if (n_colors > 256)
    return NULL;
  cmap = malloc(sizeof(XlibRgbCmap));
  memcpy (cmap->colors, colors, n_colors * sizeof(uint32));
  if (image_info->bpp == 1 &&
      (image_info->x_visual_info->class == PseudoColor ||
       image_info->x_visual_info->class == GrayScale))
    for (i = 0; i < n_colors; i++)
      {
	rgb = colors[i];
	j = ((rgb & 0xf00000) >> 12) |
		   ((rgb & 0xf000) >> 8) |
		   ((rgb & 0xf0) >> 4);
#ifdef VERBOSE
	printf ("%d %x %x %d\n", i, j, colorcube[j]);
#endif
	cmap->lut[i] = colorcube[j];
      }
  return cmap;
}

void
xlib_rgb_cmap_free (XlibRgbCmap *cmap)
{
  free (cmap);
}

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
			XlibRgbCmap *cmap)
{
  if (dith == XLIB_RGB_DITHER_NONE || (dith == XLIB_RGB_DITHER_NORMAL &&
				       !image_info->dith_default))
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      buf, 1, rowstride,
			      image_info->conv_indexed, cmap, 0, 0);
  else
    xlib_draw_rgb_image_core (drawable, gc, x, y, width, height,
			      buf, 1, rowstride,
			      image_info->conv_indexed_d, cmap, 0, 0);
}

Bool
xlib_rgb_ditherable (void)
{
  return (image_info->conv != image_info->conv_d);
}

Colormap
xlib_rgb_get_cmap (void)
{
  /* xlib_rgb_init (); */
  if (image_info)
    return image_info->cmap;
  else
    return 0;
}

Visual *
xlib_rgb_get_visual (void)
{
  /* xlib_rgb_init (); */
  if (image_info)
    return image_info->x_visual_info->visual;
  else
    return 0;
}

XVisualInfo *
xlib_rgb_get_visual_info (void)
{
  /* xlib_rgb_init (); */
  if (image_info)
    return image_info->x_visual_info;
  else
    return 0;
}

int
xlib_rgb_get_depth (void)
{
  XVisualInfo * v = xlib_rgb_get_visual_info();

  if (v)
  {
    return v->depth;
  }

  return 0;
}

Display *
xlib_rgb_get_display (void)
{
  if (image_info)
    return image_info->display;
  
  return NULL;
}

Screen *
xlib_rgb_get_screen (void)
{
  if (image_info)
    return image_info->screen;
  
  return NULL;
}
