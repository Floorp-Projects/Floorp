/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
   visual.c --- hackery involving X visuals
   Created: Jamie Zawinski <jwz@netscape.com>, 7-Jul-94.

   This file is based on code I wrote for xscreensaver, which I previously
   released under the standard X copyright (that is, not CopyLeft.)
 */


#include "mozilla.h"
#include "xfe.h"

static Visual *fe_pick_best_visual (Screen *screen);
static Visual *fe_pick_best_visual_of_class (Screen *screen, int visual_class);
static Visual *fe_id_to_visual (Screen *screen, int id);
int fe_VisualDepth (Display *dpy, Visual *visual);

#define DEFAULT_VISUAL	-1
#define BEST_VISUAL	-2
#define SPECIFIC_VISUAL	-3


/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_UNRECOGNISED_VISUAL;
extern int XFE_NO_VISUAL_WITH_ID;
extern int XFE_NO_VISUAL_OF_CLASS;


Visual *
fe_ParseVisual (Screen *screen, const char *v)
{
  int vclass;
  unsigned long id;
  char c;

  if (!v)					  vclass = BEST_VISUAL;
  else if (!XP_STRCASECMP(v, "default"))	  vclass = DEFAULT_VISUAL;
  else if (!XP_STRCASECMP(v, "best")) 		  vclass = BEST_VISUAL;
  else if (!XP_STRCASECMP(v, "staticgray")) 	  vclass = StaticGray;
  else if (!XP_STRCASECMP(v, "staticcolor"))	  vclass = StaticColor;
  else if (!XP_STRCASECMP(v, "truecolor"))	  vclass = TrueColor;
  else if (!XP_STRCASECMP(v, "grayscale"))	  vclass = GrayScale;
  else if (!XP_STRCASECMP(v, "pseudocolor"))	  vclass = PseudoColor;
  else if (!XP_STRCASECMP(v, "directcolor"))	  vclass = DirectColor;
  else if (1 == sscanf(v, " %ld %c", &id, &c))	  vclass = SPECIFIC_VISUAL;
  else if (1 == sscanf(v, " 0x%lx %c", &id, &c))  vclass = SPECIFIC_VISUAL;
  else
    {
      fprintf (stderr, XP_GetString( XFE_UNRECOGNISED_VISUAL ), fe_progname, v);
      vclass = DEFAULT_VISUAL;
    }

  if (vclass == DEFAULT_VISUAL)
    return DefaultVisualOfScreen (screen);
  else if (vclass == BEST_VISUAL)
    return fe_pick_best_visual (screen);
  else if (vclass == SPECIFIC_VISUAL)
    {
      Visual *visual = fe_id_to_visual (screen, id);
      if (visual) return visual;
      fprintf (stderr, XP_GetString( XFE_NO_VISUAL_WITH_ID ), fe_progname,
	       (unsigned int) id);
      return DefaultVisualOfScreen (screen);
    }
  else
    {
      Visual *visual = fe_pick_best_visual_of_class (screen, vclass);
      if (visual) return visual;
      fprintf (stderr, XP_GetString( XFE_NO_VISUAL_OF_CLASS ), fe_progname, v);
      return DefaultVisualOfScreen (screen);
    }
}

static Visual *
fe_pick_best_visual (Screen *screen)
{
  /* The "best" visual is the one on which we can allocate the largest
     range and number of colors.

     Therefore, a TrueColor visual which is at least 16 bits deep is best.
     (The assumption here being that a TrueColor of less than 16 bits is
     really just a PseudoColor visual with a pre-allocated color cube.)

     The next best thing is a PseudoColor visual of any type.  After that
     come the non-colormappable visuals, and non-color visuals.
   */
  Display *dpy = DisplayOfScreen (screen);
  Visual *visual;
  if ((visual = fe_pick_best_visual_of_class (screen, TrueColor)) &&
      fe_VisualDepth (dpy, visual) >= 16)
    return visual;
  if ((visual = fe_pick_best_visual_of_class (screen, PseudoColor)))
    return visual;
  if ((visual = fe_pick_best_visual_of_class (screen, TrueColor)))
    return visual;
#ifdef DIRECTCOLOR_WORKS
  if ((visual = fe_pick_best_visual_of_class (screen, DirectColor)))
    return visual;
#endif

  /* If this screen has only StaticGray or GrayScale visuals then just
     choose the default visual if it is deep enough.  This will keep
     us from unnecessarily installing a private colormap and causing
     colormap flashing.  Unfortunately, it also means that if we can't
     install a gray ramp in the GrayScale visual's default colormap,
     we will look bad.  Realistically, I don't expect this to ever
     happen and, if it does, the user can force the use of a
     private colormap with the -install command-line option or a
     resource setting. */

  visual = DefaultVisualOfScreen (screen);
  if (fe_VisualDepth (dpy, visual) >= 8)
      return visual;
  
  if ((visual = fe_pick_best_visual_of_class (screen, StaticGray)))
    return visual;
  if ((visual = fe_pick_best_visual_of_class (screen, GrayScale)))
    return visual;
  return DefaultVisualOfScreen (screen);
}

#define XXX_IMAGE_LIBRARY_IS_SOMEWHAT_BROKEN

static Visual *
fe_pick_best_visual_of_class (Screen *screen, int visual_class)
{
  /* The best visual of a class is the one which on which we can allocate
     the largest range and number of colors, which means the one with the
     greatest depth and number of cells.
   */
  Display *dpy = DisplayOfScreen (screen);
  XVisualInfo vi_in, *vi_out;
  int out_count;

  vi_in.class = visual_class;
  vi_in.screen = fe_ScreenNumber (screen);
  vi_out = XGetVisualInfo (dpy, (VisualClassMask | VisualScreenMask),
			   &vi_in, &out_count);
  if (vi_out)
    {
      /* choose the 'best' one, if multiple */
      int i, best;
      Visual *visual;
      for (i = 0, best = 0; i < out_count; i++)
	/* It's better if it's deeper, or if it's the same depth with
	   more cells (does that ever happen?  Well, it could...)
	   NOTE: don't allow pseudo color to get larger than 8! */
	if (((vi_out [i].depth > vi_out [best].depth) ||
	     ((vi_out [i].depth == vi_out [best].depth) &&
	      (vi_out [i].colormap_size > vi_out [best].colormap_size)))
#ifdef XXX_IMAGE_LIBRARY_IS_SOMEWHAT_BROKEN
	    /* For now, the image library doesn't like PseudoColor visuals
	       of depths other than 1 or 8.  Depths greater than 8 only occur
	       on machines which have TrueColor anyway, so probably we'll end
	       up using that (it is the one that `Best' would pick) but if a
	       PseudoColor visual is explicitly specified, pick the 8 bit one.
	     */
	    && (visual_class != PseudoColor ||
		vi_out [i].depth == 1 ||
		vi_out [i].depth == 8)
#endif
            
            /* SGI has 30-bit deep visuals.  Ignore them. 
               (We only have 24-bit data anyway.)
             */
            && (vi_out [i].depth <= 24)
	    )
	  best = i;
      visual = vi_out [best].visual;
      XFree ((char *) vi_out);
      return visual;
    }
  else
    return 0;
}

static Visual *
fe_id_to_visual (Screen *screen, int id)
{
  Display *dpy = DisplayOfScreen (screen);
  XVisualInfo vi_in, *vi_out;
  int out_count;
  vi_in.screen = fe_ScreenNumber (screen);
  vi_in.visualid = id;
  vi_out = XGetVisualInfo (dpy, (VisualScreenMask | VisualIDMask),
			   &vi_in, &out_count);
  if (vi_out)
    {
      Visual *v = vi_out[0].visual;
      XFree ((char *) vi_out);
      return v;
    }
  return 0;
}

char *
fe_VisualDescription (Screen *screen, Visual *visual)
{
  Display *dpy = DisplayOfScreen (screen);
  XVisualInfo vi_in, *vi_out;
  int out_count;
  char buf [255];
  vi_in.screen = fe_ScreenNumber (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, (VisualScreenMask | VisualIDMask),
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  PR_snprintf (buf, sizeof (buf),
	   "0x%02x (%s depth: %2d, cmap: %3d)",
	   (unsigned int) vi_out->visualid,
	   (vi_out->class == StaticGray  ? "StaticGray, " :
	    vi_out->class == StaticColor ? "StaticColor," :
	    vi_out->class == TrueColor   ? "TrueColor,  " :
	    vi_out->class == GrayScale   ? "GrayScale,  " :
	    vi_out->class == PseudoColor ? "PseudoColor," :
	    vi_out->class == DirectColor ? "DirectColor," :
					   "UNKNOWN:    "),
	   vi_out->depth, vi_out->colormap_size /*, vi_out->bits_per_rgb*/);
  XFree ((char *) vi_out);
  return strdup (buf);
}

int
fe_ScreenNumber (Screen *screen)
{
  Display *dpy = DisplayOfScreen (screen);
  int i;
  for (i = 0; i < ScreenCount (dpy); i++)
    if (ScreenOfDisplay (dpy, i) == screen)
      return i;
  abort ();
}


/* Changing visuals
 */

#if 0 /* This doesn't work real well - Motif bugs apparently. */
void
fe_ChangeVisualCallback (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  Visual *v = fe_ReadVisual (context);
  if (v && v != fe_globalData.default_visual)
    {
      Display *dpy = XtDisplay (CONTEXT_WIDGET (context));
      Screen *screen = XtScreen (CONTEXT_WIDGET (context));
      if (v == DefaultVisualOfScreen (screen))
	fe_globalData.default_cmap = DefaultColormapOfScreen (screen);
      else
	fe_globalData.default_cmap =
	  XCreateColormap (dpy, XtWindow (CONTEXT_WIDGET (context)),
			   v, AllocNone);
    }
}
#endif

int
fe_VisualDepth (Display *dpy, Visual *visual)
{
  XVisualInfo vi_in, *vi_out;
  int out_count, d;
/*  vi_in.screen = DefaultScreen (dpy);*/
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, /*VisualScreenMask|*/VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  d = vi_out [0].depth;
  XFree ((char *) vi_out);
  return d;
}

int
fe_VisualPixmapDepth (Display *dpy, Visual *visual)
{
  int visual_depth = fe_VisualDepth (dpy, visual);
  int pixmap_depth = visual_depth;
  int i, pfvc = 0;
  XPixmapFormatValues *pfv = XListPixmapFormats (dpy, &pfvc);
  /* Return the first matching depth in the pixmap formats.
     If there are no matching pixmap formats (which shouldn't
     be able to happen) return the visual depth instead. */
  for (i = 0; i < pfvc; i++)
    if (pfv[i].depth == visual_depth)
      {
	pixmap_depth = pfv[i].bits_per_pixel;
	break;
      }
  if (pfv)
    XFree (pfv);
  return pixmap_depth;
}


int
fe_VisualCells (Display *dpy, Visual *visual)
{
  XVisualInfo vi_in, *vi_out;
  int out_count, c;
/*  vi_in.screen = DefaultScreen (dpy);*/
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, /*VisualScreenMask|*/VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  c = vi_out [0].colormap_size;
  XFree ((char *) vi_out);
  return c;
}
