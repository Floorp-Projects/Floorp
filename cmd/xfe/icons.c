/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* */
/* icons.c --- icons and stuff
   Created: Jamie Zawinski <jwz@netscape.com>
 */


#include "mozilla.h"
#include "xfe.h"
#include "fonts.h"
#include "e_kit.h"

#include <Xm/PushBP.h>		/* For fast updating of the button pixmap... */

#ifndef NO_SECURITY
#include "ssl.h"
#endif

#ifndef NETSCAPE_PRIV
#include "../../lib/xp/flamer.h"
#else
#include "../../lib/xp/biglogo.h"
#include "../../lib/xp/photo.h"
#include "../../lib/xp/hype.h"
#ifndef NO_SECURITY
#include "../../lib/xp/rsalogo.h"
#endif
#ifdef JAVA
#include "../../lib/xp/javalogo.h"
#endif
#ifdef FORTEZZA
#include "../../lib/xp/litronic.h"
#endif
#include "../../lib/xp/coslogo.h"
#include "../../lib/xp/insologo.h"
#include "../../lib/xp/mclogo.h"
#include "../../lib/xp/ncclogo.h"
#include "../../lib/xp/odilogo.h"
#include "../../lib/xp/qt_logo.h"
#include "../../lib/xp/tdlogo.h"
#include "../../lib/xp/visilogo.h"
#endif /* !NETSCAPE_PRIV */
#ifdef EDITOR
#include "edt.h" /* for EDT_GetEmptyDocumentString() */
#endif
#ifndef NO_WEB_FONTS
#include "nf.h"
#include "Mnfrc.h"
#include "Mnfrf.h"
#include "Mnffbu.h"
#endif

#define ABS(x)     (((x) < 0) ? -(x) : (x))

#include "il_icons.h"           /* Image icon enumeration. */
#include "libimg.h"             /* Image Library public API. */

#include "icondata.h"
#include "icons.h"

#ifndef NSPR20
#include "prosdep.h"  /* for IS_LITTLE_ENDIAN / IS_BIG_ENDIAN */
#else
#include "prtypes.h"
#endif


#include <xpgetstr.h> /* for XP_GetString() */
extern int XFE_SECURITY_WITH;
extern int XFE_SECURITY_DISABLED;


extern char fe_LicenseData[];

#define DELAYED_ICON_BORDER	1
#define DELAYED_ICON_PAD	2

static struct fe_icon fe_icons[512 + MAX_ANIM_FRAMES * MAX_ANIMS] = { { 0, } };

Pixel *fe_icon_pixels = 0;

void
fe_InitIconColors (MWContext *context)
{
  int i;
  static Boolean done = False;
  Pixel pixel;

  /* Only pass through this function once per context. */
  if (CONTEXT_DATA (context)->icon_colors_initialized) return;

  CONTEXT_DATA (context)->icon_colors_initialized = True;

#ifdef DEBUG_username
  printf("Colormap using %d colors.\n", fe_n_icon_colors);
#endif

  if (!fe_icon_pixels)
    fe_icon_pixels = (Pixel *) malloc (sizeof (Pixel) * 256);
    /*fe_icon_pixels = (Pixel *) malloc (sizeof (Pixel) * fe_n_icon_colors);*/
  
  for (i = 0; i < fe_n_icon_colors; i++)
    {
      XColor color;
      color.red   = fe_icon_colors [i][0];
      color.green = fe_icon_colors [i][1],
      color.blue  = fe_icon_colors [i][2];

      pixel = fe_GetPermanentPixel (context,
                                    color.red, color.green, color.blue);

      fe_icon_pixels [i] = pixel;
    }
  done = True;
}


static char *
fe_get_app_dir(Display *dpy)
{
	char		clas[64];
	XrmDatabase	db;
	char		instance[64];
	char		*type;
	XrmValue	value;

	db = XtDatabase(dpy);
	PR_snprintf(instance, sizeof (instance), "%s.appDir", fe_progclass);
	PR_snprintf(clas, sizeof (clas), "%s.AppDir", fe_progclass);
	if (XrmGetResource(db, instance, clas, &type, &value))
	{
		return value.addr;
	}

#ifdef __sun
	return "/usr/openwin/lib/netscape";
#else
	return "/usr/lib/X11/netscape";
#endif
}


static char *
fe_get_xpm_string(FILE *f, int size)
{
	static int	alloc;
	static char	*buf = NULL;
	int		c;
	int		i;

	if (buf)
	{
		if (size > alloc)
		{
			alloc = size;
			buf = realloc(buf, alloc);
			if (!buf)
			{
				return NULL;
			}
		}
	}
	else
	{
		if (size > 128)
		{
			alloc = size;
		}
		else
		{
			alloc = 128;
		}
		buf = malloc(alloc);
		if (!buf)
		{
			return NULL;
		}
	}

	do
	{
		c = getc(f);
	}
	while ((c != '"') && (c != EOF));

	for (i = 0; i < alloc; i++)
	{
		c = getc(f);
		buf[i] = c;
		if ((c == EOF) || (buf[i] == '"'))
		{
			break;
		}
	}

	if (buf[i] != '"')
	{
		do
		{
			c = getc(f);
		}
		while ((c != '"') && (c != EOF));
	}

	buf[i] = 0;

	return buf;
}


typedef struct {
	char	type[16];
	char	spec[16];
} xpm_color_entry;


typedef struct {
	unsigned int	mask : 1;
	unsigned int	mono : 1;
	unsigned char	color;
} fe_color_entry;

static fe_color_entry fe_color_entries[128];

void
fe_process_color(int index, xpm_color_entry *entry)
{
	int	b;
	int	dist;
	int	g;
	int	i;
	int	min;
	int	min_i;
	int	r;

	switch (entry->type[0])
	{
	case 'm':
		if (entry->spec[0] == '#')
		{
			if (entry->spec[1] == '0')
			{
				fe_color_entries[index].mono = 1;
			}
		}
		break;
	case 'c':
		if (entry->spec[0] == '#')
		{
			sscanf(entry->spec + 1, "%2x%2x%2x", &r, &g, &b);
			r |= (r << 8);
			g |= (g << 8);
			b |= (b << 8);
			min = 0xffff * 3;
			min_i = 0;
			for (i = 0; i < fe_n_icon_colors; i++)
			{
				dist =
					ABS(r - (int) fe_icon_colors[i][0]) +
					ABS(g - (int) fe_icon_colors[i][1]) +
					ABS(b - (int) fe_icon_colors[i][2]);
				if (dist < min)
				{
					min = dist;
					min_i = i;
				}
			}
			fe_color_entries[index].color = min_i;
		}
		break;
	case 's':
		if (!strcmp(entry->spec, "mask"))
		{
			fe_color_entries[index].mask = 1;
		}
		break;
	}
}


void
fe_get_external_icon(Display *dpy, char **name, int *width, int *height,
	unsigned char **mono_data, unsigned char **color_data,
	unsigned char **mask_data)
{
	int		chars_per_pixel;
	unsigned char	*color;
	xpm_color_entry	entry[3];
	FILE		*f;
	char		file[512];
	int		h;
	int		i;
	int		j;
	int		k;
	unsigned char	*mask;
	int		mlen;
	unsigned char	*mono;
	int		ncolors;
	unsigned char	*p;
	char		*s;
	int		w;

	f = NULL;

	mono = NULL;
	color = NULL;
	mask = NULL;

        if ( **name != '/' ) PR_snprintf(file, sizeof (file),
		    "%s/%s.xpm", fe_get_app_dir(dpy), *name);
	f = fopen(file, "r");
	if (!f)
	{
		goto BAD_BAD_ICON;
	}

	s = fe_get_xpm_string(f, -1);
	if (!s)
	{
		goto BAD_BAD_ICON;
	}

	w = h = ncolors = chars_per_pixel = -1;
	sscanf(s, "%d %d %d %d", &w, &h, &ncolors, &chars_per_pixel);
	if ((w < 0) || (h < 0) || (ncolors < 0) || (chars_per_pixel < 0))
	{
		goto BAD_BAD_ICON;
	}

	mlen = ((w + 7) / 8) * h;

	mono = malloc(mlen);
	color = malloc(w * h);
	mask = malloc(mlen);
	if ((!mono) || (!color) || (!mask))
	{
		goto BAD_BAD_ICON;
	}

	for (i = 0; i < 128; i++)
	{
		fe_color_entries[i].mask = 0;
		fe_color_entries[i].mono = 0;
		fe_color_entries[i].color = 0;
	}

	for (i = 0; i < ncolors; i++)
	{
		s = fe_get_xpm_string(f, -1);
		if ((!s) || (!(*s)) || (s[1] != ' '))
		{
			goto BAD_BAD_ICON;
		}
		entry[0].type[0] = 0;
		entry[0].spec[0] = 0;
		entry[1].type[0] = 0;
		entry[1].spec[0] = 0;
		entry[2].type[0] = 0;
		entry[2].spec[0] = 0;
		sscanf(s + 2, "%s %s %s %s %s %s",
			entry[0].type, entry[0].spec,
			entry[1].type, entry[1].spec,
			entry[2].type, entry[2].spec);
		fe_process_color(s[0], &entry[0]);
		fe_process_color(s[0], &entry[1]);
		fe_process_color(s[0], &entry[2]);
	}

	for (i = 0; i < mlen; i++)
	{
		mask[i] = 0;
		mono[i] = 0;
	}

	j = 0;
	k = 0;
	for (i = 0; i < h; i++)
	{
		s = fe_get_xpm_string(f, w + 1);
		if (!s)
		{
			goto BAD_BAD_ICON;
		}
		p = (unsigned char *) s;
		while (*p)
		{
			color[j] = fe_color_entries[*p].color;
			if (!fe_color_entries[*p].mask)
			{
				mask[k / 8] |= (1 << (k % 8));
			}
			if (fe_color_entries[*p].mono)
			{
				mono[k / 8] |= (1 << (k % 8));
			}
			p++;
			j++;
			k++;
		}
		k = ((k + 7) / 8) * 8;
	}

	*width = w;
	*height = h;
	*mono_data = mono;
	*color_data = color;
	*mask_data = mask;

	fclose(f);

	return;

BAD_BAD_ICON:
	if (f)
	{
		fclose(f);
	}

	if (mono)
	{
		free(mono);
	}
	if (color)
	{
		free(color);
	}
	if (mask)
	{
		free(mask);
	}

	*name = NULL;
}

void
fe_NewMakeIcon(Widget toplevel_widget, 
	       Pixel foreground_color,
	       Pixel transparent_color, fe_icon *result,
	       char *name, int width, int height, 
	       unsigned char *mono_data,
	       unsigned char *color_data,
	       unsigned char *mask_data,
	       Boolean hack_mask_and_cmap_p)
{
  Display *dpy = XtDisplay (toplevel_widget);
  Screen *screen;
  Window window;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal visual_depth = 0;
  Cardinal pixmap_depth = 0;
  unsigned char *data;
  Boolean free_data = False;
  XImage *ximage;
  Pixmap pixmap = 0;
  Pixmap mask_pixmap = 0;
  XGCValues gcv;
  GC gc;
  int i;

  if (result->pixmap) return;	/* Already done. */

  if (name) fe_get_external_icon(dpy, &name, &width, &height, &mono_data,
	&color_data, &mask_data);

  XtVaGetValues (toplevel_widget, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNscreen, &screen, XtNdepth, &visual_depth, 0);

  if (hack_mask_and_cmap_p || !v)
    {
      v = DefaultVisualOfScreen (screen);
      cmap = DefaultColormapOfScreen (screen);
      visual_depth = fe_VisualDepth (dpy, v);
    }

  window = RootWindowOfScreen (screen);
  pixmap_depth = fe_VisualPixmapDepth (dpy, v);

  if (pixmap_depth == 1 || fe_globalData.force_mono_p)
    {
  MONO:
      data = mono_data;
    }
  else
    {
      /* Remap the numbers in the data to match the colors we allocated.
	 We need to copy it since the string might not be writable.
	 Also, the data is 8 deep - we might need to deepen it if we're
	 on a deeper visual.
       */
      unsigned char  *data8  = 0;
      unsigned short *data16 = 0;
      unsigned char  *data24 = 0;
      unsigned int   *data32 = 0;
      unsigned long  *data64 = 0;

      if (pixmap_depth == 8)
	{
	  data8 = (unsigned char *) malloc (width * height);
	  data  = (unsigned char *) data8;
	}
      else if (pixmap_depth == 16)
	{
	  data16 = (unsigned short *) malloc (width * height * 2);
	  data   = (unsigned char *) data16;
	}
      else if (pixmap_depth == 24)
       {
         data24 = (unsigned char *) malloc (width * height * 3);
         data   = (unsigned char *) data24;
       }
      else if (pixmap_depth == 32)
	{
	  data32 = (unsigned int *) malloc (width * height * 4);
	  data   = (unsigned char *) data32;
	}
      else if (pixmap_depth == 64)
	{
	  data64 = (unsigned long *) malloc (width * height * 8);
	  data   = (unsigned char *) data64;
	}
      else
	{
	  /* Oh great, a goofy depth. */
	  goto MONO;
	}

      free_data = True;

      if (!hack_mask_and_cmap_p)
	{
	  if (pixmap_depth == 8)
	    for (i = 0; i < (width * height); i++)
	      data8  [i] = fe_icon_pixels [color_data [i]];
	  else if (pixmap_depth == 16)
	    for (i = 0; i < (width * height); i++)
	      data16 [i] = fe_icon_pixels [color_data [i]];
         else if (pixmap_depth == 24)
           for (i = 0; i < (width * height); i++){
              unsigned int i3 = i +(i << 1);
              unsigned char *color24 = (unsigned char *)(fe_icon_pixels+color_data [i]);
             data24 [i3++] = *color24++;
             data24 [i3++] = *color24++;
             data24 [i3++] = *color24++;
            }
	  else if (pixmap_depth == 32)
	    for (i = 0; i < (width * height); i++)
	      data32 [i] = fe_icon_pixels [color_data [i]];
	  else if (pixmap_depth == 64)
	    for (i = 0; i < (width * height); i++)
	      data64 [i] = fe_icon_pixels [color_data [i]];
	  else
	    abort ();
	}
      else
	{
	  /* The hack_mask_and_cmap_p flag means that these colors need to come
	     out of the default colormap, not the window's colormap, since this
	     is an icon for the desktop.  So, go through the image, find the
	     colors that are in it, and duplicate them.
	   */
	  char color_duped [255];
	  Pixel new_pixels [255];
	  memset (color_duped, 0, sizeof (color_duped));
	  memset (new_pixels, ~0, sizeof (new_pixels));
	  for (i = 0; i < (width * height); i++)
	    {
	      if (!color_duped [color_data [i]])
		{
		  XColor color;
                  fe_colormap *colormap = fe_globalData.default_colormap;
		  color.red   = fe_icon_colors [color_data [i]][0];
		  color.green = fe_icon_colors [color_data [i]][1];
		  color.blue  = fe_icon_colors [color_data [i]][2];
                  if (!fe_AllocColor (colormap, &color))
                      fe_AllocClosestColor (colormap, &color);
		  new_pixels [color_data [i]] = color.pixel;
		  color_duped [color_data [i]] = 1;
		}
              switch(pixmap_depth){
              case 8:
               data8  [i] = new_pixels [color_data [i]];
                break;
              case 16:
               data16 [i] = new_pixels [color_data [i]];
                break;
             case 24:
                {
                 unsigned int i3 = i + (i << 1);
                 unsigned char *color24 = (unsigned char *)(new_pixels+color_data [i]);
                 data24 [i3++] = *color24++;
                 data24 [i3++] = *color24++;
                 data24 [i3++] = *color24++;
                }
                break;
              case 32:
               data32 [i] = new_pixels [color_data [i]];
                break;
              case 64:
               data64 [i] = new_pixels [color_data [i]];
                break;
              default:
               abort ();
              }
	    }
	}
    }

  ximage = XCreateImage (dpy, v,
			 (data == mono_data ? 1 : visual_depth),
			 (data == mono_data ? XYPixmap : ZPixmap),
			 0,				   /* offset */
			 (char *) data, width, height,
			 8,				   /* bitmap_pad */
			 0);
  if (data == mono_data)
    {
      /* This ordering is implicit in the data in icondata.h, which is
	 the same implicit ordering as in all XBM files.  I think. */
      ximage->bitmap_bit_order = LSBFirst;
      ximage->byte_order = LSBFirst;
    }
  else
    {
#if defined(IS_LITTLE_ENDIAN)
      ximage->byte_order = LSBFirst;
#elif defined (IS_BIG_ENDIAN)
      ximage->byte_order = MSBFirst;
#else
  ERROR! Endianness is unknown.
#endif
    }

  if (data == mono_data && visual_depth != 1 && !hack_mask_and_cmap_p)
    {
      /* If we're in mono-mode, and the screen is not of depth 1,
	 deepen the pixmap. */
      Pixmap shallow;
      shallow = XCreatePixmap (dpy, window, width, height, 1);
      pixmap  = XCreatePixmap (dpy, window, width, height, pixmap_depth);
      gcv.function = GXcopy;
      gcv.background = 0;
      gcv.foreground = 1;
      gc = XCreateGC (dpy, shallow, GCFunction|GCForeground|GCBackground,
		      &gcv);
      XPutImage (dpy, shallow, gc, ximage, 0, 0, 0, 0, width, height);
      XFreeGC (dpy, gc);

      gcv.function = GXcopy;
      gcv.background = transparent_color;
      gcv.foreground = foreground_color;
      gc = XCreateGC (dpy, pixmap, GCFunction|GCForeground|GCBackground,
		      &gcv);
      XCopyPlane (dpy, shallow, pixmap, gc, 0, 0, width, height, 0, 0, 1L);
      XFreePixmap (dpy, shallow);
      XFreeGC (dpy, gc);
      /* No need for a mask in this case - the coloring is done. */
      mask_data = 0;
    }
  else
    {
      /* Both the screen and pixmap are of the same depth.
       */
      pixmap = XCreatePixmap (dpy, window, width, height,
			      (data == mono_data ? 1 : visual_depth));

      if (visual_depth == 1 && WhitePixelOfScreen (screen) == 1)
	/* A server with backwards WhitePixel, like NCD... */
	gcv.function = GXcopyInverted;
      else
	gcv.function = GXcopy;

      gcv.background = transparent_color;
      gcv.foreground = foreground_color;
      gc = XCreateGC (dpy, pixmap, GCFunction|GCForeground|GCBackground, &gcv);
      XPutImage (dpy, pixmap, gc, ximage, 0, 0, 0, 0, width, height);
      XFreeGC (dpy, gc);
    }

  ximage->data = 0;
  XDestroyImage (ximage);
  if (free_data)
    free (data);

  /* Optimization: if the mask is all 1's, don't bother sending it. */
  if (mask_data)
    {
      int max = width * height / 8;
      for (i = 0; i < max; i++)
	if (mask_data [i] != 0xFF)
	  break;
      if (i == max)
	mask_data = 0;
    }

  /* Fill the "transparent" areas with the background color. */
  if (mask_data)
    {
      ximage = XCreateImage (dpy, v, 1, XYPixmap,
			     0,				   /* offset */
			     (char *) mask_data, width, height,
			     8,				   /* bitmap_pad */
			     0);
      /* This ordering is implicit in the data in icondata.h, which is
	 the same implicit ordering as in all XBM files.  I think. */
      ximage->byte_order = LSBFirst;
      ximage->bitmap_bit_order = LSBFirst;

      mask_pixmap = XCreatePixmap (dpy, window, width, height, 1);

      gcv.function = GXcopy;
      gc = XCreateGC (dpy, mask_pixmap, GCFunction, &gcv);
      XPutImage (dpy, mask_pixmap, gc, ximage, 0, 0, 0, 0, width, height);
      XFreeGC (dpy, gc);
      ximage->data = 0;
      XDestroyImage (ximage);

      if (! hack_mask_and_cmap_p)
	{
	  /* Create a pixmap of the mask, inverted. */
	  Pixmap inverted_mask_pixmap =
	    XCreatePixmap (dpy, window, width, height, 1);
	  gcv.function = GXcopyInverted;
	  gc = XCreateGC (dpy, inverted_mask_pixmap, GCFunction, &gcv);
	  XCopyArea (dpy, mask_pixmap, inverted_mask_pixmap, gc,
		     0, 0, width, height, 0, 0);
	  XFreeGC (dpy, gc);

	  /* Fill the background color through that inverted mask. */
	  gcv.function = GXcopy;
	  gcv.foreground = transparent_color;
	  gcv.clip_mask = inverted_mask_pixmap;
	  gc = XCreateGC (dpy, pixmap, GCFunction|GCForeground|GCClipMask,
			  &gcv);
	  XFillRectangle (dpy, pixmap, gc, 0, 0, width, height);
	  XFreeGC (dpy, gc);

	  XFreePixmap (dpy, inverted_mask_pixmap);
	}
    }

  result->pixmap = pixmap;
  result->mask = mask_pixmap;
  result->width = width;
  result->height = height;

  if (name)
    {
      free(mono_data);
      free(color_data);
      free(mask_data);
    }
}

void
fe_MakeIcon(MWContext *context, Pixel transparent_color, fe_icon* result,
	    char *name,
	    int width, int height,
	    unsigned char *mono_data,
	    unsigned char *color_data,
	    unsigned char *mask_data,
	    Boolean hack_mask_and_cmap_p)
{
  Widget widget = CONTEXT_WIDGET (context);
  Display *dpy = XtDisplay (widget);
  Screen *screen;
  Window window;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal visual_depth = 0;
  Cardinal pixmap_depth = 0;
  unsigned char *data;
  Boolean free_data = False;
  XImage *ximage;
  Pixmap pixmap = 0;
  Pixmap mask_pixmap = 0;
  XGCValues gcv;
  GC gc;
  int i;

  if (result->pixmap) return;	/* Already done. */

  if (name) fe_get_external_icon(dpy, &name, &width, &height, &mono_data,
	&color_data, &mask_data);

  XtVaGetValues (widget, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNscreen, &screen, XtNdepth, &visual_depth, 0);

  if (hack_mask_and_cmap_p)
    {
      v = DefaultVisualOfScreen (screen);
      cmap = DefaultColormapOfScreen (screen);
      visual_depth = fe_VisualDepth (dpy, v);
    }

#ifdef OSF1
    /***
       This is a major hack. We hide a 4.4b9 problem here
       For some reason, sometimes, XtVaGetValues returns a zero visual.
       If this happens, we core dump. This hides the problem by getting the
       visual straight from the X server. "DEC port team".
    ****/
    if( v == 0 ){
       v = DefaultVisualOfScreen( screen );
    }
#endif

  window = RootWindowOfScreen (screen);
  pixmap_depth = fe_VisualPixmapDepth (dpy, v);

  if (pixmap_depth == 1 || fe_globalData.force_mono_p)
    {
  MONO:
      data = mono_data;
    }
  else
    {
      /* Remap the numbers in the data to match the colors we allocated.
	 We need to copy it since the string might not be writable.
	 Also, the data is 8 deep - we might need to deepen it if we're
	 on a deeper visual.
       */
      unsigned char  *data8  = 0;
      unsigned short *data16 = 0;
      unsigned char  *data24 = 0;
      unsigned int   *data32 = 0;
      unsigned long  *data64 = 0;

      if (pixmap_depth == 8)
	{
	  data8 = (unsigned char *) malloc (width * height);
	  data  = (unsigned char *) data8;
	}
      else if (pixmap_depth == 16)
	{
	  data16 = (unsigned short *) malloc (width * height * 2);
	  data   = (unsigned char *) data16;
	}
      else if (pixmap_depth == 24)
       {
         data24 = (unsigned char *) malloc (width * height * 3);
         data   = (unsigned char *) data24;
       }
      else if (pixmap_depth == 32)
	{
	  data32 = (unsigned int *) malloc (width * height * 4);
	  data   = (unsigned char *) data32;
	}
      else if (pixmap_depth == 64)
	{
	  data64 = (unsigned long *) malloc (width * height * 8);
	  data   = (unsigned char *) data64;
	}
      else
	{
	  /* Oh great, a goofy depth. */
	  goto MONO;
	}

      free_data = True;

      if (!hack_mask_and_cmap_p)
	{
	  if (pixmap_depth == 8)
	    for (i = 0; i < (width * height); i++)
	      data8  [i] = fe_icon_pixels [color_data [i]];
	  else if (pixmap_depth == 16)
	    for (i = 0; i < (width * height); i++)
	      data16 [i] = fe_icon_pixels [color_data [i]];
         else if (pixmap_depth == 24)
           for (i = 0; i < (width * height); i++){
              unsigned int i3 = i + (i << 1);
              unsigned char *color24 = (unsigned char *)(fe_icon_pixels+color_data [i]);
             data24 [i3++] = *color24++;
             data24 [i3++] = *color24++;
             data24 [i3++] = *color24++;
            }
	  else if (pixmap_depth == 32)
	    for (i = 0; i < (width * height); i++)
	      data32 [i] = fe_icon_pixels [color_data [i]];
	  else if (pixmap_depth == 64)
	    for (i = 0; i < (width * height); i++)
	      data64 [i] = fe_icon_pixels [color_data [i]];
	  else
	    abort ();
	}
      else
	{
	  /* The hack_mask_and_cmap_p flag means that these colors need to come
	     out of the default colormap, not the window's colormap, since this
	     is an icon for the desktop.  So, go through the image, find the
	     colors that are in it, and duplicate them.
	   */
	  char color_duped [255];
	  Pixel new_pixels [255];
	  memset (color_duped, 0, sizeof (color_duped));
	  memset (new_pixels, ~0, sizeof (new_pixels));
	  for (i = 0; i < (width * height); i++)
	    {
	      if (!color_duped [color_data [i]])
		{
		  XColor color;
                  fe_colormap *colormap = fe_globalData.default_colormap;
		  color.red   = fe_icon_colors [color_data [i]][0];
		  color.green = fe_icon_colors [color_data [i]][1];
		  color.blue  = fe_icon_colors [color_data [i]][2];
                  if (!fe_AllocColor (colormap, &color))
                      fe_AllocClosestColor (colormap, &color);
		  new_pixels [color_data [i]] = color.pixel;
		  color_duped [color_data [i]] = 1;
		}
              switch(pixmap_depth){
              case 8:
               data8  [i] = new_pixels [color_data [i]];
                break;
              case 16:
               data16 [i] = new_pixels [color_data [i]];
                break;
              case 24:
                {
                unsigned int i3 = i + (i << 1);
                unsigned char *color24 = (unsigned char *)(new_pixels+color_data [i]);
               data24 [i3++] = *color24++;
               data24 [i3++] = *color24++;
               data24 [i3++] = *color24++;
                }
                break;
              case 32:
               data32 [i] = new_pixels [color_data [i]];
                break;
              case 64:
               data64 [i] = new_pixels [color_data [i]];
                break;
              default:
               abort ();
              }
	    }
	}
    }

  ximage = XCreateImage (dpy, v,
			 (data == mono_data ? 1 : visual_depth),
			 (data == mono_data ? XYPixmap : ZPixmap),
			 0,				   /* offset */
			 (char *) data, width, height,
			 8,				   /* bitmap_pad */
			 0);
  if (data == mono_data)
    {
      /* This ordering is implicit in the data in icondata.h, which is
	 the same implicit ordering as in all XBM files.  I think. */
      ximage->bitmap_bit_order = LSBFirst;
      ximage->byte_order = LSBFirst;
    }
  else
    {
#if defined(IS_LITTLE_ENDIAN)
      ximage->byte_order = LSBFirst;
#elif defined (IS_BIG_ENDIAN)
      ximage->byte_order = MSBFirst;
#else
  ERROR! Endianness is unknown.
#endif
    }

  if (data == mono_data && visual_depth != 1 && !hack_mask_and_cmap_p)
    {
      /* If we're in mono-mode, and the screen is not of depth 1,
	 deepen the pixmap. */
      Pixmap shallow;
      shallow = XCreatePixmap (dpy, window, width, height, 1);
      pixmap  = XCreatePixmap (dpy, window, width, height, pixmap_depth);
      gcv.function = GXcopy;
      gcv.background = 0;
      gcv.foreground = 1;
      gc = XCreateGC (dpy, shallow, GCFunction|GCForeground|GCBackground,
		      &gcv);
      XPutImage (dpy, shallow, gc, ximage, 0, 0, 0, 0, width, height);
      XFreeGC (dpy, gc);

      gcv.function = GXcopy;
      gcv.background = transparent_color;
      gcv.foreground = CONTEXT_DATA (context)->fg_pixel;
      gc = XCreateGC (dpy, pixmap, GCFunction|GCForeground|GCBackground,
		      &gcv);
      XCopyPlane (dpy, shallow, pixmap, gc, 0, 0, width, height, 0, 0, 1L);
      XFreePixmap (dpy, shallow);
      XFreeGC (dpy, gc);
      /* No need for a mask in this case - the coloring is done. */
      mask_data = 0;
    }
  else
    {
      /* Both the screen and pixmap are of the same depth.
       */
      pixmap = XCreatePixmap (dpy, window, width, height,
			      (data == mono_data ? 1 : visual_depth));

      if (visual_depth == 1 && WhitePixelOfScreen (screen) == 1)
	/* A server with backwards WhitePixel, like NCD... */
	gcv.function = GXcopyInverted;
      else
	gcv.function = GXcopy;

      gcv.background = transparent_color;
      gcv.foreground = CONTEXT_DATA (context)->fg_pixel;
      gc = XCreateGC (dpy, pixmap, GCFunction|GCForeground|GCBackground, &gcv);
      XPutImage (dpy, pixmap, gc, ximage, 0, 0, 0, 0, width, height);
      XFreeGC (dpy, gc);
    }

  ximage->data = 0;
  XDestroyImage (ximage);
  if (free_data)
    free (data);

  /* Optimization: if the mask is all 1's, don't bother sending it. */
  if (mask_data)
    {
      int max = width * height / 8;
      for (i = 0; i < max; i++)
	if (mask_data [i] != 0xFF)
	  break;
      if (i == max)
	mask_data = 0;
    }

  /* Fill the "transparent" areas with the background color. */
  if (mask_data)
    {
      ximage = XCreateImage (dpy, v, 1, XYPixmap,
			     0,				   /* offset */
			     (char *) mask_data, width, height,
			     8,				   /* bitmap_pad */
			     0);
      /* This ordering is implicit in the data in icondata.h, which is
	 the same implicit ordering as in all XBM files.  I think. */
      ximage->byte_order = LSBFirst;
      ximage->bitmap_bit_order = LSBFirst;

      mask_pixmap = XCreatePixmap (dpy, window, width, height, 1);

      gcv.function = GXcopy;
      gc = XCreateGC (dpy, mask_pixmap, GCFunction, &gcv);
      XPutImage (dpy, mask_pixmap, gc, ximage, 0, 0, 0, 0, width, height);
      XFreeGC (dpy, gc);
      ximage->data = 0;
      XDestroyImage (ximage);

      if (! hack_mask_and_cmap_p)
	{
	  /* Create a pixmap of the mask, inverted. */
	  Pixmap inverted_mask_pixmap =
	    XCreatePixmap (dpy, window, width, height, 1);
	  gcv.function = GXcopyInverted;
	  gc = XCreateGC (dpy, inverted_mask_pixmap, GCFunction, &gcv);
	  XCopyArea (dpy, mask_pixmap, inverted_mask_pixmap, gc,
		     0, 0, width, height, 0, 0);
	  XFreeGC (dpy, gc);

	  /* Fill the background color through that inverted mask. */
	  gcv.function = GXcopy;
	  gcv.foreground = transparent_color;
	  gcv.clip_mask = inverted_mask_pixmap;
	  gc = XCreateGC (dpy, pixmap, GCFunction|GCForeground|GCClipMask,
			  &gcv);
	  XFillRectangle (dpy, pixmap, gc, 0, 0, width, height);
	  XFreeGC (dpy, gc);

	  XFreePixmap (dpy, inverted_mask_pixmap);
	}
    }

  result->pixmap = pixmap;
  result->mask = mask_pixmap;
  result->width = width;
  result->height = height;

  if (name)
    {
      free(mono_data);
      free(color_data);
      free(mask_data);
    }
}


static void
fe_make_icon_1 (MWContext *context, Pixel transparent_color, int id,
		char *name,
		int width, int height,
		unsigned char *mono_data,
		unsigned char *color_data,
		unsigned char *mask_data,
		Boolean hack_mask_and_cmap_p)
{
  fe_MakeIcon(context, transparent_color, fe_icons + id, name, width, height,
	      mono_data, color_data, mask_data, hack_mask_and_cmap_p);
}


static void
fe_make_icon (MWContext *context, Pixel transparent_color, int id,
	      char *name,
	      int width, int height,
	      unsigned char *mono_data,
	      unsigned char *color_data,
	      unsigned char *mask_data)
{
  fe_make_icon_1 (context, transparent_color, id, name, width, height,
		  mono_data, color_data, mask_data, False);
}


static void
fe_new_init_security_icons (Widget widget)
{
#ifndef NO_SECURITY
  Pixel fg = 0;
  Pixel bg = 0;

  XtVaGetValues (widget, XmNbackground, &bg, 0);
  XtVaGetValues (widget, XmNforeground, &fg, 0);

  fe_NewMakeIcon (widget, fg, bg, &fe_icons[IL_IMAGE_INSECURE],
		  NULL,
		  SEC_Replace.width, SEC_Replace.height,
		  SEC_Replace.mono_bits, SEC_Replace.color_bits, SEC_Replace.mask_bits,
		  FALSE);

#endif
}

static void
fe_init_document_icons (MWContext *c)
{
  Pixel bg, bg2, white;
  static Bool done = False;
  Boolean save;

  if (done) return;
  done = True;

  bg = CONTEXT_DATA (c)->default_bg_pixel;
  white = WhitePixelOfScreen (XtScreen (CONTEXT_WIDGET (c)));

  fe_make_icon (c, bg, IL_IMAGE_DELAYED,
		NULL,
		IReplace.width, IReplace.height,
		IReplace.mono_bits, IReplace.color_bits, IReplace.mask_bits);
  fe_make_icon (c, bg, IL_IMAGE_NOT_FOUND,
		NULL,
		IconUnknown.width, IconUnknown.height,
		IconUnknown.mono_bits, IconUnknown.color_bits, IconUnknown.mask_bits);
  fe_make_icon (c, bg, IL_IMAGE_BAD_DATA,
		NULL,
		IBad.width, IBad.height,
		IBad.mono_bits, IBad.color_bits, IBad.mask_bits);
  fe_make_icon (c, bg, IL_IMAGE_EMBED,    /* #### Need an XPM for this one */
		NULL,
		IconUnknown.width, IconUnknown.height,
		IconUnknown.mono_bits, IconUnknown.color_bits, IconUnknown.mask_bits);
#ifndef NO_SECURITY
  fe_make_icon (c, bg, IL_IMAGE_INSECURE,
		NULL,
		SEC_Replace.width, SEC_Replace.height,
		SEC_Replace.mono_bits, SEC_Replace.color_bits, SEC_Replace.mask_bits);

  bg2 = bg;
  if (CONTEXT_DATA(c)->dashboard) {
   /* XXX need to find why we do this - dp */
    XtVaGetValues (CONTEXT_DATA (c)->dashboard, XmNbackground, &bg2, 0);
  }
#endif
  /* Load all the desktop icons */
  save = fe_globalData.force_mono_p; /* hack. hack, hack */
  if (XP_STRCASECMP(fe_globalData.wm_icon_policy, "mono") == 0)
      fe_globalData.force_mono_p = TRUE;

  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_NAVIGATOR,	/* Navigator */
		  NULL,
		  Desk_Navigator.width, Desk_Navigator.height,
		  Desk_Navigator.mono_bits, Desk_Navigator.color_bits,
		  Desk_Navigator.mask_bits,
		  True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_MSGCENTER,	/* Message Center */
		  NULL,
		  Desk_MsgCenter.width, Desk_MsgCenter.height,
		  Desk_MsgCenter.mono_bits, Desk_MsgCenter.color_bits,
		  Desk_MsgCenter.mask_bits,
		  True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_ABOOK,	/* AddressBook */
		  NULL,
		  Desk_Address.width, Desk_Address.height,
		  Desk_Address.mono_bits, Desk_Address.color_bits,
		  Desk_Address.mask_bits, True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_BOOKMARK,	/* Bookmark */
		  NULL,
		  Desk_Bookmark.width, Desk_Bookmark.height,
		  Desk_Bookmark.mono_bits, Desk_Bookmark.color_bits,
		  Desk_Bookmark.mask_bits, True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_NOMAIL,	/* No new mail */
		  NULL,
		  Desk_Messenger.width, Desk_Messenger.height,
		  Desk_Messenger.mono_bits, Desk_Messenger.color_bits,
		  Desk_Messenger.mask_bits, True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_YESMAIL,	/* New mail */
		  NULL,
		  Desk_NewMail.width, Desk_NewMail.height,
		  Desk_NewMail.mono_bits, Desk_NewMail.color_bits,
		  Desk_NewMail.mask_bits, True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_NEWS,	/* News */
		  NULL,
		  Desk_Collabra.width, Desk_Collabra.height,
		  Desk_Collabra.mono_bits, Desk_Collabra.color_bits, 
          Desk_Collabra.mask_bits, True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_MSGCOMPOSE,	/* Message Compose */
		  NULL,
		  Desk_MsgCompose.width, Desk_MsgCompose.height,
		  Desk_MsgCompose.mono_bits, Desk_MsgCompose.color_bits,
		  Desk_MsgCompose.mask_bits, True);
#ifdef EDITOR
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_EDITOR,	/* Editor */
		  NULL,
		  Desk_Composer.width, Desk_Composer.height,
		  Desk_Composer.mono_bits, Desk_Composer.color_bits,
		  Desk_Composer.mask_bits, True);
#endif /*EDITOR*/
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_COMMUNICATOR,	/* Communicator */
		  NULL,
		  Desk_Communicator.width, Desk_Communicator.height,
		  Desk_Communicator.mono_bits, Desk_Communicator.color_bits,
		  Desk_Communicator.mask_bits,
		  True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_HISTORY,	/* History */
		  NULL,
		  Desk_History.width, Desk_History.height,
		  Desk_History.mono_bits, Desk_History.color_bits,
		  Desk_History.mask_bits,
		  True);
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_SEARCH,	/* Search */
		  NULL,
		  Desk_Search.width, Desk_Search.height,
		  Desk_Search.mono_bits, Desk_Search.color_bits,
		  Desk_Search.mask_bits,
		  True);
#ifdef NETSCAPE_PRIV
#ifdef JAVA
  fe_make_icon_1 (c, white, IL_ICON_DESKTOP_JAVACONSOLE,	/* Java Console */
		  NULL,
		  Desk_JavaConsole.width, Desk_JavaConsole.height,
		  Desk_JavaConsole.mono_bits, Desk_JavaConsole.color_bits,
		  Desk_JavaConsole.mask_bits,
		  True);
#endif /* JAVA */
  fe_globalData.force_mono_p = save;
#endif /* NETSCAPE_PRIV */
}

static void
fe_init_gopher_icons (MWContext *c)
{
  Pixel bg;
  static Bool done = False;

  if (done) return;
  done = True;

  bg = CONTEXT_DATA (c)->default_bg_pixel;

  fe_make_icon (c, bg, IL_GOPHER_TEXT,
		NULL,
		GText.width, GText.height,
		GText.mono_bits, GText.color_bits, GText.mask_bits);
  fe_make_icon (c, bg, IL_GOPHER_IMAGE,
		NULL,
		GImage.width, GImage.height,
		GImage.mono_bits, GImage.color_bits, GImage.mask_bits);
  fe_make_icon (c, bg, IL_GOPHER_BINARY,
		NULL,
		GBinary.width, GBinary.height,
		GBinary.mono_bits, GBinary.color_bits, GBinary.mask_bits);
  fe_make_icon (c, bg, IL_GOPHER_SOUND,
		NULL,
		GAudio.width, GAudio.height,
		GAudio.mono_bits, GAudio.color_bits, GAudio.mask_bits);
  fe_make_icon (c, bg, IL_GOPHER_MOVIE,
		NULL,
		GMovie.width, GMovie.height,
		GMovie.mono_bits, GMovie.color_bits, GMovie.mask_bits);
  fe_make_icon (c, bg, IL_GOPHER_FOLDER,
		NULL,
		GFolder.width, GFolder.height,
		GFolder.mono_bits, GFolder.color_bits, GFolder.mask_bits);
  fe_make_icon (c, bg, IL_GOPHER_SEARCHABLE,
		NULL,
		GFind.width, GFind.height,
		GFind.mono_bits, GFind.color_bits, GFind.mask_bits);
  fe_make_icon (c, bg, IL_GOPHER_TELNET,
		NULL,
		GTelnet.width, GTelnet.height,
		GTelnet.mono_bits, GTelnet.color_bits, GTelnet.mask_bits);
  fe_make_icon (c, bg, IL_GOPHER_UNKNOWN,
		NULL,
		GUnknown.width, GUnknown.height,
		GUnknown.mono_bits, GUnknown.color_bits, GUnknown.mask_bits);
}

#ifdef EDITOR

#if 0
static void
fe_init_editor_icons_notext(MWContext* c)
{
  Pixel  bg2 = 0;
  static Bool done = False;

  if (done)
    return;

  XtVaGetValues (CONTEXT_DATA (c)->top_area, XmNbackground, &bg2, 0);

  fe_make_icon (c, bg2, IL_EDITOR_NEW,		/* 23FEB96RCJ */
		"ed_new",
		ed_new.width, ed_new.height,
		ed_new.mono_bits, ed_new.color_bits, ed_new.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_NEW_GREY,		/* 23FEB96RCJ */
		"ed_new.i",
		ed_new_i.width, ed_new_i.height,
		ed_new_i.mono_bits, ed_new_i.color_bits, ed_new_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_OPEN,
		"ed_open",
		ed_open.width, ed_open.height,
		ed_open.mono_bits, ed_open.color_bits, ed_open.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_OPEN_GREY,
		"ed_open.i",
		ed_open_i.width, ed_open_i.height,
		ed_open_i.mono_bits, ed_open_i.color_bits, ed_open_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_SAVE,	
		"ed_save",
		ed_save.width, ed_save.height,
		ed_save.mono_bits, ed_save.color_bits, ed_save.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_SAVE_GREY,
		"ed_save.i",
		ed_save_i.width, ed_save_i.height,
		ed_save_i.mono_bits, ed_save_i.color_bits, ed_save_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_BROWSE,	
		"ed_browse",
		ed_browse.width, ed_browse.height,
		ed_browse.mono_bits, ed_browse.color_bits, ed_browse.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_BROWSE_GREY,
		"ed_browse.i",
		ed_browse_i.width, ed_browse_i.height,
		ed_browse_i.mono_bits, ed_browse_i.color_bits, ed_browse_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_CUT,
		"ed_cut",
		ed_cut.width, ed_cut.height,
		ed_cut.mono_bits, ed_cut.color_bits, ed_cut.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_CUT_GREY,
		"ed_cut.i",
		ed_cut_i.width, ed_cut_i.height,
		ed_cut_i.mono_bits, ed_cut_i.color_bits, ed_cut_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_COPY,
		"ed_copy",
		ed_copy.width, ed_copy.height,
		ed_copy.mono_bits, ed_copy.color_bits, ed_copy.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_COPY_GREY,
		"ed_copy.i",
		ed_copy_i.width, ed_copy_i.height,
		ed_copy_i.mono_bits, ed_copy_i.color_bits, ed_copy_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PASTE,
		"ed_paste",
		ed_paste.width, ed_paste.height,
		ed_paste.mono_bits, ed_paste.color_bits, ed_paste.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PASTE_GREY,
		"ed_paste.i",
		ed_paste_i.width, ed_paste_i.height,
		ed_paste_i.mono_bits, ed_paste_i.color_bits, ed_paste_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PRINT,
		"ed_print",
		ed_print.width, ed_print.height,
		ed_print.mono_bits, ed_print.color_bits, ed_print_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PRINT_GREY,
		"ed_print.i",
		ed_print_i.width, ed_print_i.height,
		ed_print_i.mono_bits, ed_print_i.color_bits, ed_print_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_FIND,
		"ed_find",
		ed_find.width, ed_find.height,
		ed_find.mono_bits, ed_find.color_bits, ed_find.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_FIND_GREY,
		"ed_find.i",
		ed_find_i.width, ed_find_i.height,
		ed_find_i.mono_bits, ed_find_i.color_bits, ed_find_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PUBLISH,
		"ed_publish",
		ed_publish.width, ed_publish.height, 
		ed_publish.mono_bits, ed_publish.color_bits, ed_publish.mask_bits); 
  fe_make_icon (c, bg2, IL_EDITOR_PUBLISH_GREY,
		"ed_publish.i",
		ed_publish_i.width, ed_publish_i.height, 
		ed_publish_i.mono_bits, ed_publish_i.color_bits, ed_publish_i.mask_bits); 
  
  fe_make_icon (c, bg2, IL_ICON_BACK_GREY,
		NULL,
		TB_Back_i.width, TB_Back_i.height,
		TB_Back_i.mono_bits, TB_Back_i.color_bits,
                TB_Back_i.mask_bits);
  fe_make_icon (c, bg2, IL_ICON_LOAD_GREY,
		NULL,
		TB_LoadImages_i.width, TB_LoadImages_i.height,
		TB_LoadImages_i.mono_bits, TB_LoadImages_i.color_bits,
		TB_LoadImages_i.mask_bits);
  fe_make_icon (c, bg2, IL_ICON_FWD_GREY,
		NULL,
		TB_Forward_i.width, TB_Forward_i.height,
		TB_Forward_i.mono_bits, TB_Forward_i.color_bits,
                TB_Forward_i.mask_bits);
  fe_make_icon (c, bg2, IL_ICON_STOP_GREY,
		NULL,
		TB_Stop_i.width, TB_Stop_i.height,
		TB_Stop_i.mono_bits, TB_Stop_i.color_bits, TB_Stop_i.mask_bits);
  done = True;
}
#endif /*0*/

static void fe_init_align_icons(MWContext* c) /* added 14MAR96RCJ */
{
  Pixel  bg2 = 0;

  XtVaGetValues (CONTEXT_DATA (c)->top_area, XmNbackground, &bg2, 0);

  fe_make_icon (c, bg2, IL_ALIGN4_RAISED,
		"ImgB2B_r",
		ImgB2B_r.width, ImgB2B_r.height,
		ImgB2B_r.mono_bits, ImgB2B_r.color_bits, ImgB2B_r.mask_bits);

  fe_make_icon (c, bg2, IL_ALIGN5_RAISED,
		"ImgB2D_r",
		ImgB2D_r.width, ImgB2D_r.height,
		ImgB2D_r.mono_bits, ImgB2D_r.color_bits, ImgB2D_r.mask_bits);

  fe_make_icon (c, bg2, IL_ALIGN3_RAISED,
		"ImgC2B_r",
		ImgC2B_r.width, ImgC2B_r.height,
		ImgC2B_r.mono_bits, ImgC2B_r.color_bits, ImgC2B_r.mask_bits);

  fe_make_icon (c, bg2, IL_ALIGN2_RAISED,
		"ImgC2C_r",
		ImgC2C_r.width, ImgC2C_r.height,
		ImgC2C_r.mono_bits, ImgC2C_r.color_bits, ImgC2C_r.mask_bits);

  fe_make_icon (c, bg2, IL_ALIGN7_RAISED,
		"ImgWL_r",
		ImgWL_r.width, ImgWL_r.height,
		ImgWL_r.mono_bits, ImgWL_r.color_bits, ImgWL_r.mask_bits);

  fe_make_icon (c, bg2, IL_ALIGN6_RAISED,
		"ImgWR_r",
		ImgWR_r.width, ImgWR_r.height,
		ImgWR_r.mono_bits, ImgWR_r.color_bits, ImgWR_r.mask_bits);

  fe_make_icon (c, bg2, IL_ALIGN1_RAISED,
		"ImgT2T_r",
		ImgT2T_r.width, ImgT2T_r.height,
		ImgT2T_r.mono_bits, ImgT2T_r.color_bits, ImgT2T_r.mask_bits);

} /* end fe_init_align_icons 14MAR96RCJ */

static void
fe_init_editor_icons_withtext(MWContext* c)
{
  Pixel  bg2 = 0;
  static Bool done = False;

  if (done)
    return;

  XtVaGetValues (CONTEXT_DATA (c)->top_area, XmNbackground, &bg2, 0);
/*
  fe_make_icon (c, bg2, IL_EDITOR_NEW_PT,
		"ed_new.pt",
		ed_new_pt.width, ed_new_pt.height,
		ed_new_pt.mono_bits, ed_new_pt.color_bits,
                ed_new_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_OPEN_PT,
		"ed_open.pt",
		ed_open_pt.width, ed_open_pt.height,
		ed_open_pt.mono_bits, ed_open_pt.color_bits,
                ed_open_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_SAVE_PT,
		"ed_save.pt",
		ed_save_pt.width, ed_save_pt.height,
		ed_save_pt.mono_bits, ed_save_pt.color_bits,
                ed_save_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_BROWSE_PT,
		"ed_browse.pt",
		ed_browse_pt.width, ed_browse_pt.height,
		ed_browse_pt.mono_bits, ed_browse_pt.color_bits,
                ed_browse_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_CUT_PT,
		"ed_cut.pt",
		ed_cut_pt.width, ed_cut_pt.height,
		ed_cut_pt.mono_bits, ed_cut_pt.color_bits,
                ed_cut_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_COPY_PT,
		"ed_copy.pt",
		ed_copy_pt.width, ed_copy_pt.height,
		ed_copy_pt.mono_bits, ed_copy_pt.color_bits,
                ed_copy_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PASTE_PT,
		"ed_paste.pt",
		ed_paste_pt.width, ed_paste_pt.height,
		ed_paste_pt.mono_bits, ed_paste_pt.color_bits,
                ed_paste_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PRINT_PT,
		"ed_print.pt",
		ed_print_pt.width, ed_print_pt.height,
		ed_print_pt.mono_bits, ed_print_pt.color_bits,
                ed_print_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_FIND_PT,
		"ed_find.pt",
		ed_find_pt.width, ed_find_pt.height,
		ed_find_pt.mono_bits, ed_find_pt.color_bits,
                ed_find_pt.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PUBLISH_PT,
		"ed_publish.pt",
		ed_publish_pt.width, ed_publish_pt.height, 
		ed_publish_pt.mono_bits, ed_publish_pt.color_bits,
                ed_publish_pt.mask_bits); 

  fe_make_icon (c, bg2, IL_EDITOR_NEW_PT_GREY,
		"ed_new.pt",
		ed_new_pt_i.width, ed_new_pt_i.height,
		ed_new_pt_i.mono_bits, ed_new_pt_i.color_bits,
                ed_new_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_OPEN_PT_GREY,
		"ed_open.pt",
		ed_open_pt_i.width, ed_open_pt_i.height,
		ed_open_pt_i.mono_bits, ed_open_pt_i.color_bits,
                ed_open_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_SAVE_PT_GREY,
		"ed_save.pt",
		ed_save_pt_i.width, ed_save_pt_i.height,
		ed_save_pt_i.mono_bits, ed_save_pt_i.color_bits,
                ed_save_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_BROWSE_PT_GREY,
		"ed_browse.pt",
		ed_browse_pt_i.width, ed_browse_pt_i.height,
		ed_browse_pt_i.mono_bits, ed_browse_pt_i.color_bits,
                ed_browse_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_CUT_PT_GREY,
		"ed_cut.pt",
		ed_cut_pt_i.width, ed_cut_pt_i.height,
		ed_cut_pt_i.mono_bits, ed_cut_pt_i.color_bits,
                ed_cut_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_COPY_PT_GREY,
		"ed_copy.pt",
		ed_copy_pt_i.width, ed_copy_pt_i.height,
		ed_copy_pt_i.mono_bits, ed_copy_pt_i.color_bits,
                ed_copy_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PASTE_PT_GREY,
		"ed_paste.pt",
		ed_paste_pt_i.width, ed_paste_pt_i.height,
		ed_paste_pt_i.mono_bits, ed_paste_pt_i.color_bits,
                ed_paste_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PRINT_PT_GREY,
		"ed_print.pt",
		ed_print_pt_i.width, ed_print_pt_i.height,
		ed_print_pt_i.mono_bits, ed_print_pt_i.color_bits,
                ed_print_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_FIND_PT_GREY,
		"ed_find.pt",
		ed_find_pt_i.width, ed_find_pt_i.height,
		ed_find_pt_i.mono_bits, ed_find_pt_i.color_bits,
                ed_find_pt_i.mask_bits);
  fe_make_icon (c, bg2, IL_EDITOR_PUBLISH_PT_GREY,
		"ed_publish.pt",
		ed_publish_pt_i.width, ed_publish_pt_i.height, 
		ed_publish_pt_i.mono_bits, ed_publish_pt_i.color_bits,
                ed_publish_pt_i.mask_bits); 
  fe_make_icon (c, bg2, IL_ICON_BACK_GREY, 
		NULL, 
		TB_Back_i.width, TB_Back_i.height, 
		TB_Back_i.mono_bits, TB_Back_i.color_bits,
                TB_Back_i.mask_bits); 
  fe_make_icon (c, bg2, IL_ICON_LOAD_GREY, 
		NULL, 
		TB_LoadImages_i.width, TB_LoadImages_i.height,
		TB_LoadImages_i.mono_bits, TB_LoadImages_i.color_bits,
		TB_LoadImages_i.mask_bits);
  fe_make_icon (c, bg2, IL_ICON_FWD_GREY,
		NULL,
		TB_Forward_i.width, TB_Forward_i.height,
		TB_Forward_i.mono_bits, TB_Forward_i.color_bits,
                TB_Forward_i.mask_bits);
  fe_make_icon (c, bg2, IL_ICON_STOP_GREY,
		NULL,
		TB_Stop_i.width, TB_Stop_i.height,
		TB_Stop_i.mono_bits, TB_Stop_i.color_bits, TB_Stop_i.mask_bits);
*/
  done = True;
}

static void
fe_init_editor_icons_other(MWContext* c)
{
  Pixel  bg2 = 0;
  static Bool done = False;

  if (done)
    return;

  XtVaGetValues (CONTEXT_DATA (c)->top_area, XmNbackground, &bg2, 0);

  done = True;
/*  
  fe_make_icon(c, bg2, IL_EDITOR_BOLD,
	       "ed_bold",
	       ed_bold.width, ed_bold.height,
	       ed_bold.mono_bits, ed_bold.color_bits, ed_bold.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_BULLET,
	       "ed_bullet",
	       ed_bullet.width, ed_bullet.height,
	       ed_bullet.mono_bits, ed_bullet.color_bits, ed_bullet.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_CENTER,
	       "ed_center",
	       ed_center.width, ed_center.height,
	       ed_center.mono_bits, ed_center.color_bits, ed_center.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_CLEAR,
	       "ed_clear",
	       ed_clear.width, ed_clear.height,
	       ed_clear.mono_bits, ed_clear.color_bits, ed_clear.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_COLOR,
	       "ed_color",
	       ed_color.width, ed_color.height,
	       ed_color.mono_bits, ed_color.color_bits, ed_color.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_FIXED,
	       "ed_fixed",
	       ed_fixed.width, ed_fixed.height,
	       ed_fixed.mono_bits, ed_fixed.color_bits, ed_fixed.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_GROW,
	       "ed_grow",
	       ed_grow.width, ed_grow.height,
	       ed_grow.mono_bits, ed_grow.color_bits, ed_grow.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_GROW_GREY,
	       "ed_grow.i",
	       ed_grow_i.width, ed_grow_i.height,
	       ed_grow_i.mono_bits, ed_grow_i.color_bits, ed_grow_i.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_HRULE,
	       "ed_hrule",
	       ed_hrule.width, ed_hrule.height,
	       ed_hrule.mono_bits, ed_hrule.color_bits, ed_hrule.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_IMAGE,
	       "ed_image",
	       ed_image.width, ed_image.height,
	       ed_image.mono_bits, ed_image.color_bits, ed_image.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_INDENT,
	       "ed_indent",
	       ed_indent.width, ed_indent.height,
	       ed_indent.mono_bits, ed_indent.color_bits, ed_indent.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_ITALIC,
	       "ed_italic",
	       ed_italic.width, ed_italic.height,
	       ed_italic.mono_bits, ed_italic.color_bits, ed_italic.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_LEFT,
	       "ed_left",
	       ed_left.width, ed_left.height,
	       ed_left.mono_bits, ed_left.color_bits, ed_left.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_LINK,
	       "ed_link",
	       ed_link.width, ed_link.height,
	       ed_link.mono_bits, ed_link.color_bits, ed_link.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_NUMBER,
	       "ed_number",
	       ed_number.width, ed_number.height,
	       ed_number.mono_bits, ed_number.color_bits, ed_number.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_OUTDENT,
	       "ed_outdent",
	       ed_outdent.width, ed_outdent.height,
	       ed_outdent.mono_bits, ed_outdent.color_bits, ed_outdent.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_PROPS,
	       "ed_props",
	       ed_props.width, ed_props.height,
	       ed_props.mono_bits, ed_props.color_bits, ed_props.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_RIGHT,
	       "ed_right",
	       ed_right.width, ed_right.height,
	       ed_right.mono_bits, ed_right.color_bits, ed_right.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_SHRINK,
	       "ed_shrink",
	       ed_shrink.width, ed_shrink.height,
	       ed_shrink.mono_bits, ed_shrink.color_bits, ed_shrink.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_SHRINK_GREY,
	       "ed_shrink.i",
	       ed_shrink_i.width, ed_shrink_i.height,
	       ed_shrink_i.mono_bits, ed_shrink_i.color_bits, ed_shrink_i.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_TARGET,
	       "ed_target",
	       ed_target.width, ed_target.height,
	       ed_target.mono_bits, ed_target.color_bits, ed_target.mask_bits);
  fe_make_icon(c, bg2, IL_EDITOR_TABLE,
	       "ed_table",
	       ed_table.width, ed_table.height,
	       ed_table.mono_bits, ed_table.color_bits, ed_table.mask_bits);
*/
}

/*
 *    Icons used on the page.
 */
static void
fe_init_editor_icons(MWContext* c)
{
  Pixel  bg_pixel = 0;
  static Bool done = False;

  if (done)
    return;

  bg_pixel = CONTEXT_DATA (c)->default_bg_pixel;

  done = True;
  
  fe_make_icon(c, bg_pixel, IL_EDIT_UNSUPPORTED_TAG,
	       "ed_tag",
	       ed_tag.width, ed_tag.height,
	       ed_tag.mono_bits, ed_tag.color_bits, ed_tag.mask_bits);
  fe_make_icon(c, bg_pixel, IL_EDIT_UNSUPPORTED_END_TAG,
	       "ed_tage",
	       ed_tage.width, ed_tage.height,
	       ed_tage.mono_bits, ed_tage.color_bits, ed_tage.mask_bits);
  fe_make_icon(c, bg_pixel, IL_EDIT_FORM_ELEMENT,
	       "ed_form",
	       ed_form.width, ed_form.height,
	       ed_form.mono_bits, ed_form.color_bits, ed_form.mask_bits);
  fe_make_icon(c, bg_pixel, IL_EDIT_NAMED_ANCHOR,
	       "ed_target",
	       ed_target.width, ed_target.height,
	       ed_target.mono_bits, ed_target.color_bits, ed_target.mask_bits);
}

/*
 *    Map the toolbar location onto an ICON id.
 */
static int gold_browser_map[] = {
  IL_ICON_BACK, IL_ICON_BACK_GREY, IL_ICON_BACK_PT, IL_ICON_BACK_PT_GREY,
  IL_ICON_FWD,  IL_ICON_FWD_GREY,  IL_ICON_FWD_PT,  IL_ICON_FWD_PT_GREY,
  IL_ICON_HOME, IL_ICON_HOME_GREY, IL_ICON_HOME_PT, IL_ICON_HOME_PT_GREY,
  IL_EDITOR_EDIT,IL_EDITOR_EDIT_GREY,IL_EDITOR_EDIT_PT,IL_EDITOR_EDIT_PT_GREY,
  IL_ICON_RELOAD,IL_ICON_RELOAD_GREY,IL_ICON_RELOAD_PT,IL_ICON_RELOAD_PT_GREY,
  IL_ICON_LOAD, IL_ICON_LOAD_GREY, IL_ICON_LOAD_PT, IL_ICON_LOAD_PT_GREY,
  IL_ICON_OPEN, IL_ICON_OPEN_GREY, IL_ICON_OPEN_PT, IL_ICON_OPEN_PT_GREY,
  IL_ICON_PRINT,IL_ICON_PRINT_GREY,IL_ICON_PRINT_PT,IL_ICON_PRINT_PT_GREY,
  IL_ICON_FIND, IL_ICON_FIND_GREY, IL_ICON_FIND_PT, IL_ICON_FIND_PT_GREY,
  IL_ICON_STOP, IL_ICON_STOP_GREY, IL_ICON_STOP_PT, IL_ICON_STOP_PT_GREY,
  IL_ICON_NETSCAPE,IL_ICON_NETSCAPE,IL_ICON_NETSCAPE_PT,IL_ICON_NETSCAPE_PT
};

static int gold_editor_map[] = {
  /* toolbar */
  IL_EDITOR_NEW, IL_EDITOR_NEW_GREY, IL_EDITOR_NEW_PT, IL_EDITOR_NEW_PT_GREY,
  IL_EDITOR_OPEN,IL_EDITOR_OPEN_GREY,IL_EDITOR_OPEN_PT,IL_EDITOR_OPEN_PT_GREY,
  IL_EDITOR_SAVE,IL_EDITOR_SAVE_GREY,IL_EDITOR_SAVE_PT,IL_EDITOR_SAVE_PT_GREY,
  IL_EDITOR_BROWSE,IL_EDITOR_BROWSE_GREY,IL_EDITOR_BROWSE_PT,
    IL_EDITOR_BROWSE_PT_GREY,
  IL_EDITOR_CUT,IL_EDITOR_CUT_GREY, IL_EDITOR_CUT_PT,  IL_EDITOR_CUT_PT_GREY,
  IL_EDITOR_COPY, IL_EDITOR_COPY_GREY, IL_EDITOR_COPY_PT,
    IL_EDITOR_COPY_PT_GREY,
  IL_EDITOR_PASTE, IL_EDITOR_PASTE_GREY, IL_EDITOR_PASTE_PT,
    IL_EDITOR_PASTE_PT_GREY,
  IL_ICON_PRINT, IL_ICON_PRINT_GREY, IL_ICON_PRINT_PT, IL_ICON_PRINT_PT_GREY,
  IL_ICON_FIND,  IL_ICON_FIND_GREY,  IL_ICON_FIND_PT,  IL_ICON_FIND_PT_GREY,
  IL_EDITOR_PUBLISH, IL_EDITOR_PUBLISH_GREY, IL_EDITOR_PUBLISH_PT,
    IL_EDITOR_PUBLISH_PT_GREY
};

#endif /* EDITOR */

static void
fe_init_sa_icons (MWContext *c)
{
  Pixel bg = 0;
  static Bool done = False;
  if (done) return;
  done = True;

/*  XtVaGetValues (CONTEXT_DATA (c)->top_area, XmNbackground, &bg, 0); */

  fe_make_icon (c, bg, IL_SA_SIGNED,
		"A_Signed",
		A_Signed.width, A_Signed.height,
		A_Signed.mono_bits, A_Signed.color_bits, A_Signed.mask_bits);
  fe_make_icon (c, bg, IL_SA_ENCRYPTED,
		"A_Encrypt",
		A_Encrypt.width, A_Encrypt.height,
		A_Encrypt.mono_bits, A_Encrypt.color_bits,
		A_Encrypt.mask_bits);
  fe_make_icon (c, bg, IL_SA_NONENCRYPTED,
		"A_NoEncrypt",
		A_NoEncrypt.width, A_NoEncrypt.height,
		A_NoEncrypt.mono_bits, A_NoEncrypt.color_bits,
		A_NoEncrypt.mask_bits);
  fe_make_icon (c, bg, IL_SA_SIGNED_BAD,
		"A_SignBad",
		A_SignBad.width, A_SignBad.height,
		A_SignBad.mono_bits, A_SignBad.color_bits,
		A_SignBad.mask_bits);
  fe_make_icon (c, bg, IL_SA_ENCRYPTED_BAD,
		"A_EncrypBad",
		A_EncrypBad.width, A_EncrypBad.height,
		A_EncrypBad.mono_bits, A_EncrypBad.color_bits,
		A_EncrypBad.mask_bits);
  fe_make_icon (c, bg, IL_SMIME_ATTACHED,
		"M_Attach",
		M_Attach.width, M_Attach.height,
		M_Attach.mono_bits, M_Attach.color_bits, M_Attach.mask_bits);
  fe_make_icon (c, bg, IL_SMIME_SIGNED,
		"M_Signed",
		M_Signed.width, M_Signed.height,
		M_Signed.mono_bits, M_Signed.color_bits, M_Signed.mask_bits);
  fe_make_icon (c, bg, IL_SMIME_ENCRYPTED,
		"M_Encrypt",
		M_Encrypt.width, M_Encrypt.height,
		M_Encrypt.mono_bits, M_Encrypt.color_bits,
		M_Encrypt.mask_bits);
  fe_make_icon (c, bg, IL_SMIME_ENC_SIGNED,
		"M_SignEncyp",
		M_SignEncyp.width, M_SignEncyp.height,
		M_SignEncyp.mono_bits, M_SignEncyp.color_bits,
		M_SignEncyp.mask_bits);
  fe_make_icon (c, bg, IL_SMIME_SIGNED_BAD,
		"M_SignBad",
		M_SignBad.width, M_SignBad.height,
		M_SignBad.mono_bits, M_SignBad.color_bits,
		M_SignBad.mask_bits);
  fe_make_icon (c, bg, IL_SMIME_ENCRYPTED_BAD,
		"M_EncrypBad",
		M_EncrypBad.width, M_EncrypBad.height,
		M_EncrypBad.mono_bits, M_EncrypBad.color_bits,
		M_EncrypBad.mask_bits);
  fe_make_icon (c, bg, IL_SMIME_ENC_SIGNED_BAD,
		"M_SgnEncypBad",
		M_SgnEncypBad.width, M_SgnEncypBad.height,
		M_SgnEncypBad.mono_bits, M_SgnEncypBad.color_bits,
		M_SgnEncypBad.mask_bits);
}

static void
fe_init_msg_icons (MWContext *c)
{
  Pixel bg = 0;
  static Bool done = False;
  if (done) return;
  done = True;

/*  XtVaGetValues (CONTEXT_DATA (c)->top_area, XmNbackground, &bg, 0); */

  fe_make_icon (c, bg, IL_MSG_ATTACH,
		"M_ToggleAttach",
		M_ToggleAttach.width, M_ToggleAttach.height,
		M_ToggleAttach.mono_bits, M_ToggleAttach.color_bits, M_ToggleAttach.mask_bits);
}

void
fe_InitIcons (MWContext *c, MSG_BIFF_STATE biffstate)
{
  int icon_index;
  Widget shell = CONTEXT_WIDGET (c);

  fe_init_document_icons (c);
  switch (c->type) {
      case MWContextMailMsg: /* Fall through */
      case MWContextMail:
		  /* Both MailThread and MessageCenter come through here. */
		  if(shell && !strcmp(XtName(shell), "MailFolder")) {
			  icon_index = IL_ICON_DESKTOP_MSGCENTER;
		  } else {
			  icon_index = (biffstate == MSG_BIFF_NewMail) ?
				  IL_ICON_DESKTOP_YESMAIL : IL_ICON_DESKTOP_NOMAIL;
		  }
	break;
      case MWContextMessageComposition:
	icon_index = IL_ICON_DESKTOP_MSGCOMPOSE;
	break;

      case MWContextAddressBook:
	icon_index = IL_ICON_DESKTOP_ABOOK;
	break;
      case MWContextBookmarks:
	icon_index = IL_ICON_DESKTOP_BOOKMARK;
	break;
#ifdef EDITOR
      case MWContextEditor:
        icon_index = IL_ICON_DESKTOP_EDITOR;
	break;
#endif /*EDITOR*/
      case MWContextBrowser:
		icon_index = IL_ICON_DESKTOP_NAVIGATOR;
	break;
      case MWContextHistory:
		icon_index = IL_ICON_DESKTOP_HISTORY;
	break;
      case MWContextSearch: /* FALL THROUGH */
      case MWContextSearchLdap:
		icon_index = IL_ICON_DESKTOP_SEARCH;
	break;
      case MWContextDialog:	/* FALL THROUGH */
      default:
	icon_index = IL_ICON_DESKTOP_COMMUNICATOR;
	break;
  }

  if (fe_icons [icon_index].pixmap) {
      Arg av [5];
      int ac = 0;
      XtSetArg (av[ac], XtNiconPixmap, fe_icons[icon_index].pixmap); ac++;
      if (!fe_icons [icon_index].mask) {
	  /*
	   *    Must make a mask, because olwm is so stupid, it doesn't clip
	   *    mask the image to the image size, just blasts away at 60x60.
	   */
	  Pixmap mask_pixmap;
	  Dimension height = fe_icons[icon_index].height;
	  Dimension width = fe_icons[icon_index].width;
	  Display*  dpy = XtDisplay(CONTEXT_WIDGET(c));
	  Screen*   screen = XtScreen(CONTEXT_WIDGET(c));
	  XGCValues gcv;
	  GC        gc;
	  
	  mask_pixmap = XCreatePixmap(dpy, RootWindowOfScreen(screen),
				      width, height, 1);
	  
	  gcv.function = GXset;
	  /* gcv.foreground = BlackPixelOfScreen(screen); */
	  gc = XCreateGC(dpy, mask_pixmap, GCFunction, &gcv);
	  XFillRectangle(dpy, mask_pixmap, gc, 0, 0, width, height);
	  XFreeGC(dpy, gc);

	  fe_icons[icon_index].mask = mask_pixmap;
      }
      XtSetArg (av[ac], XtNiconMask, fe_icons[icon_index].mask); ac++;

      XtSetValues (CONTEXT_WIDGET (c), av, ac);
  }
}

#ifndef NO_SECURITY
Pixmap
fe_NewSecurityPixmap (Widget widget, Dimension *w, Dimension *h,
		      int type)
{
  int index = 0;
  switch (type)
    {
    case SSL_SECURITY_STATUS_ON_LOW:  index = IL_ICON_SECURITY_ON; break;
    case SSL_SECURITY_STATUS_ON_HIGH: index = IL_ICON_SECURITY_HIGH; break;
#ifdef FORTEZZA
    case SSL_SECURITY_STATUS_FORTEZZA: index = IL_ICON_SECURITY_FORTEZZA; break;
#endif
    case SSL_SECURITY_STATUS_OFF:	/* Fall Through */
    default:				index = IL_ICON_SECURITY_OFF;
    }
  fe_new_init_security_icons (widget);
  if (w) *w = fe_icons [index].width;
  if (h) *h = fe_icons [index].height;
  return fe_icons [index].pixmap;
}

Pixmap
fe_SecurityPixmap (MWContext *context, Dimension *w, Dimension *h,
		   int type)
{
  int index = 0;
  switch (type)
    {
    case SSL_SECURITY_STATUS_ON_LOW:  index = IL_ICON_SECURITY_ON; break;
    case SSL_SECURITY_STATUS_ON_HIGH: index = IL_ICON_SECURITY_HIGH; break;
#ifdef FORTEZZA
    case SSL_SECURITY_STATUS_FORTEZZA: index = IL_ICON_SECURITY_FORTEZZA; break;
#endif
    case SSL_SECURITY_STATUS_OFF:	/* Fall Through */
    default:				index = IL_ICON_SECURITY_OFF;
    }
  if (context)	/* for awt, we know we've already done this */
      fe_init_document_icons (context);
  if (w) *w = fe_icons [index].width;
  if (h) *h = fe_icons [index].height;
  return fe_icons [index].pixmap;
}
#endif /* ! NO_SECURITY */

Pixmap
fe_ToolbarPixmapByName(MWContext *context, char *pixmap_name,
		       Boolean disabled_p, Boolean urls_p)
{
  Boolean both_p = CONTEXT_DATA (context)->show_toolbar_text_p;

  /* this function should only be used for the Mail window. */
  XP_ASSERT(context->type == MWContextMail);

  if (!strcmp(pixmap_name, "getNewMail"))
    return fe_icons[disabled_p && both_p ? IL_MSG_GET_MAIL_PT_GREY:
		   disabled_p ? IL_MSG_GET_MAIL_GREY :
		   both_p ? IL_MSG_GET_MAIL_PT :
		   IL_MSG_GET_MAIL].pixmap;
  else if (!strcmp(pixmap_name, "deleteMessage"))
    return fe_icons[disabled_p && both_p ? IL_MSG_DELETE_PT_GREY :
		   disabled_p ? IL_MSG_DELETE_GREY :
		   both_p ? IL_MSG_DELETE_PT :
		   IL_MSG_DELETE].pixmap;
  else if (!strcmp(pixmap_name, "mailNew"))
    return fe_icons[disabled_p && both_p ? IL_MSG_NEW_MSG_PT_GREY :
		   disabled_p ? IL_MSG_NEW_MSG_GREY :
		   both_p ? IL_MSG_NEW_MSG_PT :
		   IL_MSG_NEW_MSG].pixmap;
  else if (!strcmp(pixmap_name, "replyToSender"))
    return fe_icons[disabled_p && both_p ?
		   IL_MSG_REPLY_TO_SENDER_PT_GREY :
		   disabled_p ? IL_MSG_REPLY_TO_SENDER_GREY :
		   both_p ? IL_MSG_REPLY_TO_SENDER_PT :
		   IL_MSG_REPLY_TO_SENDER].pixmap;
  else if (!strcmp(pixmap_name, "replyToAll"))
    return fe_icons[disabled_p && both_p ?
		   IL_MSG_REPLY_TO_ALL_PT_GREY :
		   disabled_p ? IL_MSG_REPLY_TO_ALL_GREY :
		   both_p ? IL_MSG_REPLY_TO_ALL_PT :
		   IL_MSG_REPLY_TO_ALL].pixmap;
  else if (!strcmp(pixmap_name, "forwardMessage"))
    return fe_icons[disabled_p && both_p ?
		   IL_MSG_FORWARD_MSG_PT_GREY :
		   disabled_p ? IL_MSG_FORWARD_MSG_GREY :
		   both_p ? IL_MSG_FORWARD_MSG_PT :
		   IL_MSG_FORWARD_MSG].pixmap;
  else if (!strcmp(pixmap_name, "abort"))
    return fe_icons[disabled_p && both_p ? IL_ICON_STOP_PT_GREY :
		   disabled_p ? IL_ICON_STOP_GREY :
		   both_p ? IL_ICON_STOP_PT :
		   IL_ICON_STOP].pixmap;
  else if (!strcmp(pixmap_name, "postNew"))
    return fe_icons[disabled_p && both_p ? IL_MSG_NEW_POST_PT_GREY:
		   disabled_p ? IL_MSG_NEW_POST_GREY :
		   both_p ? IL_MSG_NEW_POST_PT :
		   IL_MSG_NEW_POST].pixmap;
  else if (!strcmp(pixmap_name, "send"))
    return fe_icons[disabled_p && both_p ? IL_COMPOSE_SEND_PT_GREY :
		   disabled_p ? IL_COMPOSE_SEND_GREY :
		   both_p ? IL_COMPOSE_SEND_PT :
		   IL_COMPOSE_SEND].pixmap;
  else if (!strcmp(pixmap_name, "previousMessage"))
    return fe_icons[disabled_p && both_p ?
		   IL_MSG_PREV_UNREAD_PT_GREY :
		   disabled_p ? IL_MSG_PREV_UNREAD_GREY :
		   both_p ? IL_MSG_PREV_UNREAD_PT :
		   IL_MSG_PREV_UNREAD].pixmap;
  else if (!strcmp(pixmap_name, "nextMessage"))
    return fe_icons[disabled_p && both_p ?
		   IL_MSG_NEXT_UNREAD_PT_GREY :
		   disabled_p ? IL_MSG_NEXT_UNREAD_GREY :
		   both_p ? IL_MSG_NEXT_UNREAD_PT :
		   IL_MSG_NEXT_UNREAD].pixmap;
  else if (!strcmp(pixmap_name, "previousUnreadMessage"))
    return fe_icons[disabled_p && both_p ?
		   IL_MSG_PREV_UNREAD_PT_GREY :
		   disabled_p ? IL_MSG_PREV_UNREAD_GREY :
		   both_p ? IL_MSG_PREV_UNREAD_PT :
		   IL_MSG_PREV_UNREAD].pixmap;
  else if (!strcmp(pixmap_name, "nextUnreadMessage"))
    return fe_icons[disabled_p && both_p ?
		   IL_MSG_NEXT_UNREAD_PT_GREY :
		   disabled_p ? IL_MSG_NEXT_UNREAD_GREY :
		   both_p ? IL_MSG_NEXT_UNREAD_PT :
		   IL_MSG_NEXT_UNREAD].pixmap;
  else XP_ASSERT(0);
  return 0;	/* XXX ??? */
}

Pixmap
fe_ToolbarPixmap (MWContext *context, int i, Boolean disabled_p,
		  Boolean urls_p)
{
  Boolean both_p = CONTEXT_DATA (context)->show_toolbar_text_p;
  int offset;
  int grey_offset;
  int pt_offset;
#ifdef EDITOR
  static align_icons_done=0; 	/* added 14MAR96RCJ */
#endif

  if (urls_p)
    return (fe_icons [IL_ICON_TOUR + i].pixmap);

  offset = (both_p ? IL_ICON_BACK_PT : IL_ICON_BACK);
  grey_offset = (disabled_p ? 1 : 0);
  pt_offset = (both_p ? 1: 0);

  switch (context->type)
    {
#ifdef EDITOR
    case MWContextEditor:
#if 0
      if (both_p)
	fe_init_editor_icons_withtext(context);
      else
	fe_init_editor_icons_notext(context);
#endif /*0*/

      fe_init_editor_icons_other(context);

      if (i < 10)
	i = gold_editor_map[(4*i) + grey_offset + (2*pt_offset)];
      else if (i < 23) 
	i = IL_EDITOR_OTHER_GROUP + (2*(i - 10)) + grey_offset;
      else if (i>=IL_ALIGN1_RAISED && i <= IL_ALIGN7_DEPRESSED) {
           if (!align_icons_done) {
               fe_init_align_icons(context);
               align_icons_done=1;
           }
      }
      else 
	i = IL_EDITOR_BULLET + (2*(i - 23)) + grey_offset;

      return fe_icons[i].pixmap;
      /*NOTREACHED*/
      break;
    case MWContextBrowser:
      return fe_icons[gold_browser_map[(4*i) + grey_offset + (2*pt_offset)]].pixmap;
      /*NOTREACHED*/
      break;
#else
    case MWContextMailMsg:
      switch (i)
	{
	case 0: return fe_icons[disabled_p && both_p ? IL_MSG_NEW_MSG_PT_GREY :
			       disabled_p ? IL_MSG_NEW_MSG_GREY :
			       both_p ? IL_MSG_NEW_MSG_PT :
			       IL_MSG_NEW_MSG].pixmap;
	case 1: return fe_icons[disabled_p && both_p ?
			       IL_MSG_REPLY_TO_SENDER_PT_GREY :
			       disabled_p ? IL_MSG_REPLY_TO_SENDER_GREY :
			       both_p ? IL_MSG_REPLY_TO_SENDER_PT :
			       IL_MSG_REPLY_TO_SENDER].pixmap;
	case 2: return fe_icons[disabled_p && both_p ?
			       IL_MSG_REPLY_TO_ALL_PT_GREY :
			       disabled_p ? IL_MSG_REPLY_TO_ALL_GREY :
			       both_p ? IL_MSG_REPLY_TO_ALL_PT :
			       IL_MSG_REPLY_TO_ALL].pixmap;
	case 3: return fe_icons[disabled_p && both_p ?
			       IL_MSG_FORWARD_MSG_PT_GREY :
			       disabled_p ? IL_MSG_FORWARD_MSG_GREY :
			       both_p ? IL_MSG_FORWARD_MSG_PT :
			       IL_MSG_FORWARD_MSG].pixmap;
	case 4: return fe_icons[disabled_p && both_p ?
			       IL_MSG_NEXT_UNREAD_PT_GREY :
			       disabled_p ? IL_MSG_NEXT_UNREAD_GREY :
			       both_p ? IL_MSG_NEXT_UNREAD_PT :
			       IL_MSG_NEXT_UNREAD].pixmap;
	case 5: return fe_icons[disabled_p && both_p ?
			       IL_MSG_PREV_UNREAD_PT_GREY :
			       disabled_p ? IL_MSG_PREV_UNREAD_GREY :
			       both_p ? IL_MSG_PREV_UNREAD_PT :
			       IL_MSG_PREV_UNREAD].pixmap;
	case 6: return fe_icons[disabled_p && both_p ? IL_MSG_DELETE_PT_GREY :
			       disabled_p ? IL_MSG_DELETE_GREY :
			       both_p ? IL_MSG_DELETE_PT :
			       IL_MSG_DELETE].pixmap;
	case 7: return fe_icons[disabled_p && both_p ? IL_ICON_STOP_PT_GREY :
				disabled_p ? IL_ICON_STOP_GREY :
				both_p ? IL_ICON_STOP_PT :
				IL_ICON_STOP].pixmap;
	}
    case MWContextNewsMsg:	 /* ### wrong */

    case MWContextBrowser:
      return fe_icons [offset + (i*2) + (disabled_p ? 1 : 0)].pixmap;
      break;
#endif
    case MWContextMail:
      switch (i)
	{
	case 0: return fe_icons[disabled_p && both_p ? IL_MSG_GET_MAIL_PT_GREY:
			        disabled_p ? IL_MSG_GET_MAIL_GREY :
			        both_p ? IL_MSG_GET_MAIL_PT :
				IL_MSG_GET_MAIL].pixmap;
	case 1: return fe_icons[disabled_p && both_p ? IL_MSG_NEW_MSG_PT_GREY :
			       disabled_p ? IL_MSG_NEW_MSG_GREY :
			       both_p ? IL_MSG_NEW_MSG_PT :
			       IL_MSG_NEW_MSG].pixmap;
	case 2: return fe_icons[disabled_p && both_p ? IL_MSG_DELETE_PT_GREY :
				disabled_p ? IL_MSG_DELETE_GREY :
				both_p ? IL_MSG_DELETE_PT :
				IL_MSG_DELETE].pixmap;
				/* change this one to 4 when the security button
				   gets put in. */
	case 3: return fe_icons[disabled_p && both_p ? IL_ICON_STOP_PT_GREY :
				disabled_p ? IL_ICON_STOP_GREY :
				both_p ? IL_ICON_STOP_PT :
				IL_ICON_STOP].pixmap;
	default: fe_perror (context, "internal error: bogus mail icon accessed"); break;
	}
      break;
    case MWContextNews:
      switch (i)
	{
	case 0: return fe_icons[disabled_p && both_p ? IL_MSG_NEW_POST_PT_GREY:
				disabled_p ? IL_MSG_NEW_POST_GREY :
				both_p ? IL_MSG_NEW_POST_PT :
				IL_MSG_NEW_POST].pixmap;
	case 1: return fe_icons[disabled_p && both_p ? IL_MSG_NEW_MSG_PT_GREY :
				disabled_p ? IL_MSG_NEW_MSG_GREY :
				both_p ? IL_MSG_NEW_MSG_PT :
				IL_MSG_NEW_MSG].pixmap;
	case 2: return fe_icons[disabled_p && both_p ?
			         IL_MSG_REPLY_TO_SENDER_PT_GREY :
				disabled_p ? IL_MSG_REPLY_TO_SENDER_GREY :
				both_p ? IL_MSG_REPLY_TO_SENDER_PT :
				IL_MSG_REPLY_TO_SENDER].pixmap;
	case 3: return fe_icons[disabled_p && both_p ? IL_MSG_FOLLOWUP_PT_GREY:
				disabled_p ? IL_MSG_FOLLOWUP_GREY :
				both_p ? IL_MSG_FOLLOWUP_PT :
				IL_MSG_FOLLOWUP].pixmap;
	case 4: return fe_icons[disabled_p && both_p ?
			         IL_MSG_FOLLOWUP_AND_REPLY_PT_GREY :
				disabled_p ? IL_MSG_FOLLOWUP_AND_REPLY_GREY :
				both_p ? IL_MSG_FOLLOWUP_AND_REPLY_PT :
				IL_MSG_FOLLOWUP_AND_REPLY].pixmap;
	case 5: return fe_icons[disabled_p && both_p ?
			         IL_MSG_FORWARD_MSG_PT_GREY :
				disabled_p ? IL_MSG_FORWARD_MSG_GREY :
				both_p ? IL_MSG_FORWARD_MSG_PT :
				IL_MSG_FORWARD_MSG].pixmap;
	case 6: return fe_icons[disabled_p && both_p ?
				  IL_MSG_PREV_UNREAD_PT_GREY :
				disabled_p ? IL_MSG_PREV_UNREAD_GREY :
				both_p ? IL_MSG_PREV_UNREAD_PT :
				IL_MSG_PREV_UNREAD].pixmap;
	case 7: return fe_icons[disabled_p && both_p ?
			         IL_MSG_NEXT_UNREAD_PT_GREY :
				disabled_p ? IL_MSG_NEXT_UNREAD_GREY :
				both_p ? IL_MSG_NEXT_UNREAD_PT :
				IL_MSG_NEXT_UNREAD].pixmap;
	case 8: return fe_icons[disabled_p && both_p ?
			         IL_MSG_MARK_THREAD_READ_PT_GREY :
				disabled_p ? IL_MSG_MARK_THREAD_READ_GREY :
				both_p ? IL_MSG_MARK_THREAD_READ_PT :
				IL_MSG_MARK_THREAD_READ].pixmap;
	case 9: return fe_icons[disabled_p && both_p ?
			         IL_MSG_MARK_ALL_READ_PT_GREY :
				disabled_p ? IL_MSG_MARK_ALL_READ_GREY :
				both_p ? IL_MSG_MARK_ALL_READ_PT :
				IL_MSG_MARK_ALL_READ].pixmap;
	case 10:return fe_icons[disabled_p && both_p ? IL_ICON_PRINT_PT_GREY :
				disabled_p ? IL_ICON_PRINT_GREY :
				both_p ? IL_ICON_PRINT_PT :
				IL_ICON_PRINT].pixmap;
	case 11:return fe_icons[disabled_p && both_p ? IL_ICON_STOP_PT_GREY :
				disabled_p ? IL_ICON_STOP_GREY :
				both_p ? IL_ICON_STOP_PT :
				IL_ICON_STOP].pixmap;
/*	default: abort (); */
	default: fe_perror (context, "internal error: bogus news icon accessed"); break;
	}
      break;

    case MWContextMessageComposition:
      switch (i)
	{
	/* sendOrSendLater */
	case 0:
	    if (fe_globalPrefs.queue_for_later_p)
		/* SendLater */
		return fe_icons[disabled_p && both_p ? IL_COMPOSE_SENDLATER_PT_GREY :
				disabled_p ? IL_COMPOSE_SENDLATER_GREY :
				both_p ? IL_COMPOSE_SENDLATER_PT :
				IL_COMPOSE_SENDLATER].pixmap;
	    else
		/* Send */
		return fe_icons[disabled_p && both_p ? IL_COMPOSE_SEND_PT_GREY :
				disabled_p ? IL_COMPOSE_SEND_GREY :
				both_p ? IL_COMPOSE_SEND_PT :
				IL_COMPOSE_SEND].pixmap;
	/* Quote */
	case 1:return fe_icons[disabled_p && both_p ? IL_COMPOSE_QUOTE_PT_GREY :
				disabled_p ? IL_COMPOSE_QUOTE_GREY :
				both_p ? IL_COMPOSE_QUOTE_PT :
				IL_COMPOSE_QUOTE].pixmap;
	/* Attach */
	case 2:return fe_icons[disabled_p &&
				both_p ? IL_COMPOSE_ATTACH_PT_GREY :
				disabled_p ? IL_COMPOSE_ATTACH_GREY :
				both_p ? IL_COMPOSE_ATTACH_PT :
				IL_COMPOSE_ATTACH].pixmap;
	/* AddressBook */
	case 3:return fe_icons[disabled_p &&
				both_p ? IL_COMPOSE_ADDRESSBOOK_PT_GREY :
				disabled_p ? IL_COMPOSE_ADDRESSBOOK_GREY :
				both_p ? IL_COMPOSE_ADDRESSBOOK_PT :
				IL_COMPOSE_ADDRESSBOOK].pixmap;
	/* Stop */
	case 4:return fe_icons[disabled_p && both_p ? IL_ICON_STOP_PT_GREY :
				disabled_p ? IL_ICON_STOP_GREY :
				both_p ? IL_ICON_STOP_PT :
				IL_ICON_STOP].pixmap;
	default: fe_perror (context, "internal error: bogus compose icon accessed"); break;
	}
      break;



    default:
      abort ();
      break;
    }
  return 0;
}


void
fe_DrawIcon (MWContext *context, LO_ImageStruct *lo_image, int icon_number)
{
  Pixmap p = fe_icons [icon_number].pixmap;
  Pixmap m = fe_icons [icon_number].mask;
  Pixmap tmp_mask = 0;
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  Drawable drawable = fe_drawable->xdrawable;
  Display *dpy = XtDisplay(CONTEXT_DATA (context)->drawing_area);
  int x = lo_image->x + lo_image->x_offset
    + lo_image->border_width
    - CONTEXT_DATA (context)->document_x;
  int y = lo_image->y + lo_image->y_offset
    + lo_image->border_width
    - CONTEXT_DATA (context)->document_y;
  int w, h;
  unsigned long flags;

  if (!p)
    {
      icon_number = IL_IMAGE_NOT_FOUND;
      p = fe_icons [icon_number].pixmap;
    }

  x += fe_drawable->x_origin;
  y += fe_drawable->y_origin;
  w = fe_icons [icon_number].width;
  h = fe_icons [icon_number].height;

  /*
   * A delayed image with ALT text displays the icon, text, and a box instead
   * of just the icon.  I steal the code here from display text, there should
   * probably be some way to share code here.
   */
  if (icon_number == IL_IMAGE_DELAYED &&
      lo_image->alt &&
      lo_image->alt_len)
    {
      fe_Font font = fe_LoadFontFromFace (context, lo_image->text_attr,
					  &lo_image->text_attr->charset,
					  lo_image->text_attr->font_face,
					  lo_image->text_attr->size,
					  lo_image->text_attr->fontmask);
      /*char *str = (char *) lo_image->alt;*/
      int ascent, descent;
      int tx, ty;
      Boolean selected_p = False;
      XGCValues gcv, gcv3;
      GC gc, gc3;
      memset (&gcv, ~0, sizeof (gcv));
      memset (&gcv3, ~0, sizeof (gcv3));
      gcv.foreground = fe_GetPixel (context,
				    lo_image->text_attr->fg.red,
				    lo_image->text_attr->fg.green,
				    lo_image->text_attr->fg.blue);
      gcv3.foreground = fe_GetPixel (context,
				    lo_image->text_attr->bg.red,
				    lo_image->text_attr->bg.green,
				    lo_image->text_attr->bg.blue);
      gcv.line_width = 1;

      gc = fe_GetClipGC (CONTEXT_DATA (context)->widget,
                         GCForeground|GCLineWidth,
                         &gcv, fe_drawable->clip_region);

      /* beware: XDrawRectangle centers the line-thickness on the coords. */
      XDrawRectangle (dpy, drawable, gc,
                      x, y,
		      lo_image->width - DELAYED_ICON_BORDER,
		      lo_image->height - DELAYED_ICON_BORDER);
      x += (DELAYED_ICON_BORDER + DELAYED_ICON_PAD);
      y += (DELAYED_ICON_BORDER + DELAYED_ICON_PAD);

      if (!font)
      {
		return;
      }

      FE_FONT_EXTENTS(lo_image->text_attr->charset, font, &ascent, &descent);

      tx = x + w + DELAYED_ICON_PAD;
      ty = y + (h / 2) - ((ascent + descent) / 2) + ascent;
      gcv.background = fe_GetPixel (context,
				    lo_image->text_attr->bg.red,
				    lo_image->text_attr->bg.green,
				    lo_image->text_attr->bg.blue);

      flags = (GCForeground | GCBackground);
      FE_SET_GC_FONT(lo_image->text_attr->charset, &gcv, font, &flags);

      gc = fe_GetClipGC (CONTEXT_DATA (context)->widget, flags, &gcv,
                         fe_drawable->clip_region);

      gc3 = fe_GetClipGC (CONTEXT_DATA (context)->widget, flags, &gcv3,
                         fe_drawable->clip_region);

      if (CONTEXT_DATA (context)->backdrop_pixmap &&
	  /* This can only happen if something went wrong while loading
	     the background pixmap, I think. */
	  CONTEXT_DATA (context)->backdrop_pixmap != (Pixmap) ~0 &&
	  !selected_p)
        FE_DRAW_STRING (lo_image->text_attr->charset, dpy,
		drawable, font, gc, tx, ty, (char *) lo_image->alt,
		lo_image->alt_len);
      else
        FE_DRAW_IMAGE_STRING (lo_image->text_attr->charset, dpy,
		drawable, font, gc, gc3, tx, ty, (char *) lo_image->alt,
		lo_image->alt_len);
    }

  if (p) {
      XGCValues gcv;
      unsigned long flags;
      GC gc;
      memset (&gcv, ~0, sizeof (gcv));
      gcv.function = GXcopy;
      flags = GCFunction;

      if (m) /* #### no need for this if using default solid bg color */
	{
            /* We now have two masks: the icon's clip mask and the
               compositor's clip region, which both have different origins.
               When drawing the icon, we need to use a temporary clip mask
               which represents the logical AND of these two masks.  This 
               leaves the original icon clip mask unaltered for future
               use. */

              XGCValues gcv2;
              GC gc2;

              /* The clip origin for the icon clip mask. */
              gcv.clip_x_origin = x;
              gcv.clip_y_origin = y;

              /* Create a temporary mask and clear it. */
              tmp_mask = XCreatePixmap (dpy, drawable, w, h, 1);
              gcv2.function = GXclear;
              gc2 = XCreateGC (dpy, tmp_mask, GCFunction, &gcv2);
              XFillRectangle(dpy, tmp_mask, gc2, 0, 0, w, h);

              /* Use the compositors' clip region as a clip mask when copying
                 the icon mask to the temporary mask.  Note that XSetRegion
                 must be called before setting gc2's clip origin. */
              if (fe_drawable->clip_region)
                  XSetRegion(dpy, gc2, fe_drawable->clip_region);
              gcv2.function = GXcopy;
              gcv2.clip_x_origin = -gcv.clip_x_origin;
              gcv2.clip_y_origin = -gcv.clip_y_origin;
              XChangeGC(dpy, gc2, GCFunction | GCClipXOrigin | GCClipYOrigin,
                        &gcv2);
              XCopyArea (dpy, m, tmp_mask, gc2, 0, 0, w, h, 0, 0);

              /* Now use the temporary clip mask to draw the icon.  */
              gcv.clip_mask = tmp_mask;
              flags |= (GCClipMask | GCClipXOrigin | GCClipYOrigin);
              gc = fe_GetGCfromDW (dpy, drawable, flags, &gcv, NULL);
              XFreeGC(dpy, gc2);
	}
      else
          {
              /* We only have to deal with the compositor's clip region
                 when drawing the image. */
              gc = fe_GetGCfromDW (dpy, drawable, flags, &gcv,
                                   fe_drawable->clip_region);
          }

      XCopyArea (dpy, p, drawable, gc, 0, 0, w, h, x, y);

      if (tmp_mask)
          XFreePixmap(dpy, tmp_mask);
  } else { /* aaaaagh! */
      fe_DrawShadows(context, fe_drawable,
		     x, y, 50, 50, 2, XmSHADOW_OUT);
  }
}

/*
 * the fe_icons array is only local to this module.  We need
 * a way to get the size of an icon form another module.
 */
void
fe_IconSize (int icon_number, long *width, long *height)
{
  *width = (long)fe_icons [icon_number].width;
  *height = (long)fe_icons [icon_number].height;
}

#define XFE_ABOUT_FILE		0
#define XFE_SPLASH_FILE		1
#define XFE_LICENSE_FILE	2
#define XFE_MAILINTRO_FILE	3
#define XFE_PLUGIN_FILE		4

struct { char *name; char *str; } fe_localized_files[] = {
	{ "about",	NULL },
	{ "splash",	NULL },
	{ "license",	NULL },
	{ "mail.msg",	NULL },
	{ "plugins",	NULL }
};


static char *
fe_get_localized_file(int which)
{
	FILE		*f;
	char		file[512];
	char		*p;
	char		*ret;
	int		size;

	ret = fe_localized_files[which].str;
	if (ret)
	{
		return ret;
	}

	PR_snprintf(file, sizeof (file), "%s/%s", fe_get_app_dir(fe_display),
		fe_localized_files[which].name);
	f = fopen(file, "r");
	if (!f)
	{
		return NULL;
	}

	size = 20000;
	ret = malloc(size + 1);
	if (!ret)
	{
		fclose(f);
		return NULL;
	}
	size = fread(ret, 1, size, f);
	fclose(f);
	ret[size] = 0;

	p = strdup(ret);
	free(ret);
	if (!p)
	{
		return NULL;
	}
	ret = p;

	fe_localized_files[which].str = ret;

	return ret;
}


void *
FE_AboutData (const char *which,
	      char **data_ret, int32 *length_ret, char **content_type_ret)
{
  unsigned char *tmp;
  static XP_Bool ever_loaded_map = FALSE;
  char *rv = NULL;

  if (0) {;} 
#ifndef NETSCAPE_PRIV
  else if (!strcmp (which, "flamer"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) flamer_gif;
      *length_ret = sizeof (flamer_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#else
  else if (!strcmp (which, "logo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) biglogo_gif;
      *length_ret = sizeof (biglogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "photo"))
    {
      if (!ever_loaded_map)
	{
	  *data_ret = 0;
	  *length_ret = 0;
	  *content_type_ret = 0;
	  return 0;
	}
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) photo_jpg;
      *length_ret = sizeof (photo_jpg) - 1;
      *content_type_ret = IMAGE_JPG;
    }
  else if (!strcmp (which, "hype"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char*)hype_au;
      *length_ret = sizeof (hype_au) - 1;
      *content_type_ret = AUDIO_BASIC;
    }
#ifndef NO_SECURITY
  else if (!strcmp (which, "rsalogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) rsalogo_gif;
      *length_ret = sizeof (rsalogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif
#ifdef JAVA
  else if (!strcmp (which, "javalogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) javalogo_gif;
      *length_ret = sizeof (javalogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif
#ifdef HAVE_QUICKTIME
  else if (!strcmp (which, "qtlogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) qt_logo_gif;
      *length_ret = sizeof (qt_logo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif
#ifdef FORTEZZA
  else if (!strcmp (which, "litronic"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) litronic_gif;
      *length_ret = sizeof (litronic_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif
  else if (!strcmp (which, "coslogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) coslogo_jpg;
      *length_ret = sizeof (coslogo_jpg) - 1;
      *content_type_ret = IMAGE_JPG;
    }
  else if (!strcmp (which, "insologo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) insologo_gif;
      *length_ret = sizeof (insologo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "mclogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) mclogo_gif;
      *length_ret = sizeof (mclogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "ncclogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) ncclogo_gif;
      *length_ret = sizeof (ncclogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "odilogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) odilogo_gif;
      *length_ret = sizeof (odilogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "tdlogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) tdlogo_gif;
      *length_ret = sizeof (tdlogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "visilogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) visilogo_gif;
      *length_ret = sizeof (visilogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif /* !NETSCAPE_PRIV */
#ifdef EDITOR
  else if (!strcmp(which, "editfilenew"))
    {
      /* Magic about: for Editor new (blank) window */
      *data_ret = strdup(EDT_GetEmptyDocumentString());
      *length_ret = strlen (*data_ret);
      *content_type_ret = TEXT_MDL; 
    }
#endif
  else
    {
	  char *a = NULL;
      char *type = TEXT_HTML;
      Boolean do_PR_snprintf = False;
      Boolean do_rot = True;
      if (!strcmp (which, ""))
	{
	  do_PR_snprintf = True;
	  a = fe_get_localized_file(XFE_ABOUT_FILE);
	  if (a)
	    {
	      a = strdup(a);
	      do_rot = False;
	    }
	  else
	    {
	      a = strdup (
#ifdef JAVA
#ifndef MOZ_COMMUNICATOR_ABOUT
#		          include "../../l10n/us/xp/about-java-lite.h"
#else
#		          include "../../l10n/us/xp/about-java.h"
#endif
#else
#ifndef MOZ_COMMUNICATOR_ABOUT
#		          include "../../l10n/us/xp/about-lite.h"
#else
#		          include "../../l10n/us/xp/about.h"
#endif
#endif
		         );
	    }
	}
      else if (!strcmp (which, "splash"))
	{
	  do_PR_snprintf = True;
	  a = fe_get_localized_file(XFE_SPLASH_FILE);
	  if (a)
	    {
	      a = strdup(a);
	      do_rot = False;
	    }
	  else
	    {
	      a = strdup (
#ifdef JAVA
#ifndef MOZ_COMMUNICATOR_ABOUT
#		          include "../../l10n/us/xp/splash-java-lite.h"
#else
#		          include "../../l10n/us/xp/splash-java.h"
#endif
#else
#ifndef MOZ_COMMUNICATOR_ABOUT
#		          include "../../l10n/us/xp/splash-lite.h"
#else
#		          include "../../l10n/us/xp/splash.h"
#endif
#endif
		         );
	    }
	}
      else if (!strcmp (which, "1994"))
	{
	  ever_loaded_map = TRUE;
	  a = strdup (
#		      include "../../l10n/us/xp/authors2.h"
		     );
	}
      else if (!strcmp (which,"license"))
	{
	  type = TEXT_PLAIN;
	  a = fe_get_localized_file(XFE_LICENSE_FILE);
	  if (a)
	    {
	      a = strdup(a);
	      do_rot = False;
	    }
	  else
	    {
	      a = strdup (fe_LicenseData);
	    }
	}

      else if (!strcmp (which,"mozilla"))
	{
	  a = strdup (
#		      include "../../l10n/us/xp/mozilla.h"
		      );
	}
      else if (!strcmp (which,
			"mailintro"))
	{
	  type = MESSAGE_RFC822;
     a = fe_get_localized_file(XFE_MAILINTRO_FILE );
	  if (a)
	  {
        a = strdup( a );
        do_rot = False;
     }
     else
     {
		  a = strdup (
#		      include "../../l10n/us/xp/mail.h"
		      );
     }
	}

      else if (!strcmp (which, "blank"))
	a = strdup ("");
      else if (!strcmp (which, "custom"))
        {
          do_rot = False;
	  a = ekit_AboutData();
        }


      else if (!strcmp (which, "plugins"))
	{
	  a = fe_get_localized_file(XFE_PLUGIN_FILE);
	  if (a)
	    {
	      a = strdup(a);
	      do_rot = False;
	    }
	  else
	    {
	      a = strdup (
#		          include "../../l10n/us/xp/aboutplg.h"
		         );
	    }
	}



      else
	a = strdup ("\234\255\246\271\250\255\252\274\145\271\246"
		    "\261\260\256\263\154\145\154\247\264\272\271"
		    "\161\145\234\256\261\261\256\270\204");
      if (a)
	{
	  if (do_rot)
	    {
	      for (tmp = (unsigned char *) a; *tmp; tmp++) *tmp -= 69;
	    }

	  if (do_PR_snprintf)
	    {
	      char *a2;
	      int len;
#ifndef NO_SECURITY
              /* These strings are for display in the about: page */
	      char *s0 = XP_GetString(XFE_SECURITY_WITH);
	      char *s1 = SECNAV_SecurityVersion(PR_TRUE);
	      char *s2 = SECNAV_SSLCapabilities();
	      char *ss;
	      len = strlen(s0)+strlen(s1)+strlen(s2);
	      ss = (char*) malloc(len);
	      PR_snprintf(ss, len, s0, s1, s2);
#else
	      char *ss = XP_GetString(XFE_SECURITY_DISABLED);
#endif
	      len = strlen(a) + strlen(fe_version_and_locale) +
                        strlen(fe_version_and_locale) + strlen(ss);
	      a2 = (char *) malloc(len);
	      PR_snprintf (a2, len, a,
		       fe_version_and_locale,
		       fe_version_and_locale,
		       ss
		       );
#ifndef NO_SECURITY
	      free (s2);
	      free (ss);
#endif
	      free (a);
	      a = a2;
	    }

	  *data_ret = a;
	  rv = a;  /* Return means 'free this later' */
	  *length_ret = strlen (*data_ret);
	  *content_type_ret = type;
	}
      else
	{
	  *data_ret = 0;
	  *length_ret = 0;
	  *content_type_ret = 0;
	}
    }
  return rv;
}

void FE_FreeAboutData (void *fe_data, const char *which)
{
  if (fe_data) 
	free (fe_data);
}

/*****************************************************************************/
/*                       Image Library callbacks                             */
/*****************************************************************************/

/**************************** Icon dimensions ********************************/
JMC_PUBLIC_API(void)
_IMGCB_GetIconDimensions(IMGCB* img_cb, jint op, void* dpy_cx, int* width,
                         int* height, jint icon_number)
{
    MWContext *context = (MWContext *)dpy_cx; /* XXX This should be the FE's
                                                 display context. */

    /* Initialize the icon, if necessary. */
    if (icon_number >= IL_GOPHER_FIRST && icon_number <= IL_GOPHER_LAST)
        fe_init_gopher_icons (context);
    else if (icon_number >= IL_SA_FIRST && icon_number <= IL_SA_LAST)
        fe_init_sa_icons (context);
    else if (icon_number >= IL_MSG_FIRST && icon_number <= IL_MSG_LAST)
        fe_init_msg_icons (context);
#ifdef EDITOR
    else if (icon_number >= IL_EDIT_FIRST && icon_number <= IL_EDIT_LAST)
        fe_init_editor_icons(context);
#endif /*EDITOR*/

    /* Get the dimensions of the icon. */
    if (fe_icons [icon_number].pixmap) {
        *width  = fe_icons[icon_number].width;
        *height = fe_icons[icon_number].height;
    }
    else if (fe_icons [IL_IMAGE_NOT_FOUND].pixmap) {
        *width  = fe_icons[IL_IMAGE_NOT_FOUND].width;
        *height = fe_icons[IL_IMAGE_NOT_FOUND].height;
    }
    else /* aaaaagh! */ {
        *width = 50;
        *height = 50;
    }
}

/**************************** Icon display ***********************************/

JMC_PUBLIC_API(void)
_IMGCB_DisplayIcon(IMGCB* img_cb, jint op, void* dpy_cx, jint x, jint y,
                   jint icon_number)
{
    MWContext *context = (MWContext *)dpy_cx; /* XXX This should be the FE's
                                                 display context. */
    Pixmap icon_pixmap = fe_icons[icon_number].pixmap;
    Pixmap mask_pixmap = fe_icons[icon_number].mask;
    Pixmap tmp_mask = 0;
    fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
    Drawable drawable = fe_drawable->xdrawable;
    Display *dpy = XtDisplay(CONTEXT_DATA (context)->drawing_area);
    int32 icon_x_offset, icon_y_offset;
    uint32 width, height;
    /*unsigned long flags;*/

    /* Compute the offset into the drawable of the icon origin. */
    icon_x_offset = x - CONTEXT_DATA(context)->document_x +
        fe_drawable->x_origin;
    icon_y_offset = y - CONTEXT_DATA(context)->document_y +
        fe_drawable->y_origin;

    if (!icon_pixmap) {
        icon_number = IL_IMAGE_NOT_FOUND;
        icon_pixmap = fe_icons[icon_number].pixmap;
    }

    width = fe_icons [icon_number].width;
    height = fe_icons [icon_number].height;

    if (icon_pixmap) {
        XGCValues gcv;
        unsigned long flags;
        GC gc;
        memset (&gcv, ~0, sizeof (gcv));
        gcv.function = GXcopy;
        flags = GCFunction;

        if (mask_pixmap) { /* #### no need for this if using default solid bg
                              color */
            /* We now have two masks: the icon's clip mask and the
               compositor's clip region, which both have different origins.
               When drawing the icon, we need to use a temporary clip mask
               which represents the logical AND of these two masks.  This 
               leaves the original icon clip mask unaltered for future
               use. */

            XGCValues gcv2;
            GC gc2;

            /* The clip origin for the icon clip mask. */
            gcv.clip_x_origin = icon_x_offset;
            gcv.clip_y_origin = icon_y_offset;

            /* Create a temporary mask and clear it. */
            tmp_mask = XCreatePixmap (dpy, drawable, width, height, 1);
            gcv2.function = GXclear;
            gc2 = XCreateGC (dpy, tmp_mask, GCFunction, &gcv2);
            XFillRectangle(dpy, tmp_mask, gc2, 0, 0, width, height);

            /* Use the compositors' clip region as a clip mask when copying
               the icon mask to the temporary mask.  Note that XSetRegion
               must be called before setting gc2's clip origin. */
            if (fe_drawable->clip_region)
                XSetRegion(dpy, gc2, fe_drawable->clip_region);
            gcv2.function = GXcopy;
            gcv2.clip_x_origin = -gcv.clip_x_origin;
            gcv2.clip_y_origin = -gcv.clip_y_origin;
            XChangeGC(dpy, gc2, GCFunction | GCClipXOrigin | GCClipYOrigin,
                      &gcv2);
            XCopyArea (dpy, mask_pixmap, tmp_mask, gc2, 0, 0, width, height,
                       0, 0);

            /* Now use the temporary clip mask to draw the icon.  */
            gcv.clip_mask = tmp_mask;
            flags |= (GCClipMask | GCClipXOrigin | GCClipYOrigin);
            gc = fe_GetGCfromDW (dpy, drawable, flags, &gcv, NULL);
            XFreeGC(dpy, gc2);
	}
        else {
            /* We only have to deal with the compositor's clip region
               when drawing the icon. */
            gc = fe_GetGCfromDW (dpy, drawable, flags, &gcv,
                                 fe_drawable->clip_region);
        }
        XCopyArea (dpy, icon_pixmap, drawable, gc, 0, 0, width, height,
                   icon_x_offset, icon_y_offset);

        if (tmp_mask)
            XFreePixmap(dpy, tmp_mask);
    } else { /* aaaaagh! */
        fe_DrawShadows(context, fe_drawable,
                       icon_x_offset, icon_y_offset, 50, 50, 2, XmSHADOW_OUT);
    }
}

/*****************************************************************************/
/*                       End of Image Library callbacks                      */
/*****************************************************************************/

/*
 * Given an icon number, return a pointer to the corresponding static icon
 * data.  It would be a good idea to consider making this the central place
 * for associating icon numbers with icon data, since it would help simplify
 * all the XFE icon initialization code above.
 *
 * Currently, this function only deals with icons which appear on the page,
 * since its only use is to support the PostScript Front End.
 */
static struct fe_icon_data *
fe_get_icon_data(int icon_number) 
{
    switch (icon_number) {

        /* Image placeholder icons. */
    case IL_IMAGE_DELAYED:
        return &IReplace;
    case IL_IMAGE_NOT_FOUND:
        return &IconUnknown;
    case IL_IMAGE_BAD_DATA:
        return &IBad;
#ifndef NO_SECURITY
    case IL_IMAGE_INSECURE:
        return &SEC_Replace;
#endif /* ! NO_SECURITY */
    case IL_IMAGE_EMBED:
        return &IconUnknown;

        /* Gopher icons. */
    case IL_GOPHER_TEXT:
        return &GText;
    case IL_GOPHER_IMAGE:
        return &GImage;
    case IL_GOPHER_BINARY:
        return &GBinary;
    case IL_GOPHER_SOUND:
        return &GAudio;
    case IL_GOPHER_MOVIE:
        return &GMovie;
    case IL_GOPHER_FOLDER:
        return &GFolder;
    case IL_GOPHER_SEARCHABLE:
        return &GFind;
    case IL_GOPHER_TELNET:
        return &GTelnet;
    case IL_GOPHER_UNKNOWN:
        return &GUnknown;

#ifdef EDITOR    
        /* Editor icons. */
    case IL_EDIT_NAMED_ANCHOR:
        return &ed_target;
    case IL_EDIT_FORM_ELEMENT:
        return &ed_form;
    case IL_EDIT_UNSUPPORTED_TAG:
        return &ed_tag;
    case IL_EDIT_UNSUPPORTED_END_TAG:
        return &ed_tage;
#endif /* EDITOR */
        
        /* Security Advisor and S/MIME icons. */
    case IL_SA_SIGNED:
        return &A_Signed;
    case IL_SA_ENCRYPTED:
        return &A_Encrypt;
    case IL_SA_NONENCRYPTED:
        return &A_NoEncrypt;
    case IL_SA_SIGNED_BAD:
        return &A_SignBad;
    case IL_SA_ENCRYPTED_BAD:
        return &A_EncrypBad;
    case IL_SMIME_ATTACHED:
        return &M_Attach;
    case IL_SMIME_SIGNED:
        return &M_Signed;
    case IL_SMIME_ENCRYPTED:
        return &M_Encrypt;
    case IL_SMIME_ENC_SIGNED:
        return &M_SignEncyp;
    case IL_SMIME_SIGNED_BAD:
        return &M_SignBad;
    case IL_SMIME_ENCRYPTED_BAD:
        return &M_EncrypBad;
    case IL_SMIME_ENC_SIGNED_BAD:
        return &M_SgnEncypBad;

        /* Message attachment icon. */
    case IL_MSG_ATTACH:
        return &M_ToggleAttach;

        /* Return NULL if the icon number is not recognized. */
    default:
        return NULL;
    }
}

/* Get the dimensions of an icon in pixels for the PostScript front end. */
void
FE_GetPSIconDimensions(int icon_number, int *width, int *height)
{
    struct fe_icon_data *icon_data = fe_get_icon_data(icon_number);
    
    if (icon_data) {
        *width = icon_data->width;
        *height = icon_data->height;
    }
    else {
        *width = 0;
        *height = 0;
    }
}

/* Fill in the bits of an icon for the PostScript front end. */
XP_Bool
FE_GetPSIconData(int icon_number, IL_Pixmap *image, IL_Pixmap *mask) 
{
    uint8 bit_mask;
    int i, j, width, widthBytes, height, depth;
    int pixel_number, palette_index, mask_count;
    NI_PixmapHeader *img_header;
    struct fe_icon_data *icon_data;
    uint8 *color_bits, *mask_bits;

    icon_data = fe_get_icon_data(icon_number);
    if (!icon_data)
        return FALSE;

    img_header = &image->header;
    height = img_header->height;
    width = img_header->width;
    widthBytes = img_header->widthBytes;
    depth = img_header->color_space->pixmap_depth;
    color_bits = icon_data->color_bits;
    mask_bits = icon_data->mask_bits;
    
    XP_ASSERT(width == icon_data->width);
    XP_ASSERT(height == icon_data->height);
    XP_ASSERT(image->bits);
    
    switch (depth) {
    case 16:
        pixel_number = 0;
        bit_mask = 0x01;        /* Current bit within current mask byte. */
        mask_count = 0;         /* Current mask byte number. */
        for (i = 0; i < height; i++) {
            uint16 *bits = (uint16 *)((uint8 *)image->bits + widthBytes * i);

            for (j = 0; j < width; j++) {
                /* If we have a mask, check whether the mask bit for this
                   pixel is set. */
                if (mask_bits && !(mask_bits[mask_count] & bit_mask)) {
                    *bits++ = 0x0fff;
                }
                else {
                    palette_index = color_bits[pixel_number];
                    *bits++ = ((fe_icon_colors[palette_index][0]&0x00f0)<<4) +
                        (fe_icon_colors[palette_index][1]&0x00f0) +
                        ((fe_icon_colors[palette_index][2]&0x00f0)>>4);
                }
                pixel_number++;
                if (bit_mask == 0x80) {
                    /* Start a new mask byte. */
                    bit_mask = 0x01;
                    mask_count++;
                }
                else {
                    bit_mask <<= 1;
                }
            }

            /* Start next row of mask with a new byte. */
            if (bit_mask != 0x01) {
                bit_mask = 0x01;
                mask_count++;
            }
        }
        return TRUE;
        
    case 32:
        pixel_number = 0;
        bit_mask = 0x01;        /* Current bit within current mask byte. */
        mask_count = 0;         /* Current mask byte number. */
        for (i = 0; i < height; i++) {
            uint32 *bits = (uint32 *)((uint8 *)image->bits + widthBytes * i);

            for (j = 0; j < width; j++) {
                /* If we have a mask, check whether the mask bit for this
                   pixel is set. */
                if (mask_bits && !(mask_bits[mask_count] & bit_mask)) {
                    *bits++ = 0x00ffffff;
                }
                else {
                    palette_index = color_bits[pixel_number];
                    *bits++ = ((fe_icon_colors[palette_index][0]&0x00ff)<<16) +
                        ((fe_icon_colors[palette_index][1]&0x00ff)<<8) +
                        (fe_icon_colors[palette_index][2]&0x00ff);
                }
                pixel_number++;
                if (bit_mask == 0x80) {
                    /* Start a new mask byte. */
                    bit_mask = 0x01;
                    mask_count++;
                }
                else {
                    bit_mask <<= 1;
                }
            }

            /* Start next row of mask with a new byte. */
            if (bit_mask != 0x01) {
                bit_mask = 0x01;
                mask_count++;
            }
        }
        return TRUE;
    
    default:
        return FALSE;
    }
}
