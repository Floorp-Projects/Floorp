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
   layout.c --- UI routines called by the layout module.
   Created: Jamie Zawinski <jwz@netscape.com>, 23-Jun-94.
 */


#include "mozilla.h"
#include "xfe.h"
#include "selection.h"
#include "fonts.h"
#include "felocale.h"
#include "fe_proto.h"
#include "msgcom.h"
#include "mozjava.h"
#include "np.h"
#include "nppriv.h"
#include "layout.h"
#include "Xfe/Xfe.h"

#include <X11/keysym.h>

extern Widget applet_storage;

extern PRLogModuleInfo* NSJAVA;
#define warn	PR_LOG_WARN

#include <Xm/SashP.h> /* for grid edges */
#include <Xm/DrawP.h> /* #### for _XmDrawShadows() */

#include "il_icons.h"           /* Image icon enumeration. */

#include "layers.h"

#include <plevent.h>
#include <prtypes.h>
#include "libevent.h"

#include <libi18n.h>
#include "intl_csi.h"
/* for XP_GetString() */
#include <xpgetstr.h>

#ifndef NO_WEB_FONTS
#include "nf.h"
#include "Mnfrc.h"
#include "Mnfrf.h"
#include "Mnffbu.h"
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#if defined(DEBUG_tao)
#define XDBG(x) x
#else
#define XDBG(x) 
#endif

extern int XFE_UNTITLED;
extern int XFE_COMPOSE;
extern int XFE_NO_SUBJECT;
extern int XFE_MAIL_TITLE_FMT, XFE_NEWS_TITLE_FMT, XFE_TITLE_FMT;
extern int XFE_EDITOR_TITLE_FMT;
extern int XFE_LAY_UNKNOWN_PARAMETER_TO_ACTIVATE_LINK_ACTION;
extern int XFE_LAY_TOO_MANY_ARGS_TO_ACTIVATE_LINK_ACTION;
extern int XFE_LAY_LOCAL_FILE_URL_UNTITLED;

extern MWContext * XFE_showBrowser(Widget toplevel, URL_Struct *url);
extern void fe_HTMLViewTooltipsEH(MWContext *context, CL_Layer *layer,
								  CL_Event *layer_event, int state);

static void fe_get_url_x_selection_cb(Widget w,XtPointer client_data,
									Atom * sel,Atom * type,XtPointer value, 
									unsigned long * len,int * format);

void XFE_ClearView (MWContext *context, int which);

void fe_ClearArea (MWContext *context, int x, int y, unsigned int w,
                   unsigned int h);
void fe_ClearAreaWithColor (MWContext *context, int x, int y, unsigned int w,
                            unsigned int h, Pixel color);

/* State for the highlighted item (this should eventually be done by
   layout I think).  This is kind of a kludge, but it only assumes that: 
   there is only one mouse; and that the events and translations are 
   being dispatched correctly... */
static LO_Element *last_armed_xref = 0;
static struct {
  MWContext* context;
#ifdef LAYERS_FULL_FE_EVENT
  XEvent xevent;
#else
  fe_EventStruct fe_event;
#endif
} last_armed_xref_closure_for_disarm;
static Boolean last_armed_xref_highlighted_p = False;
MWContext *last_documented_xref_context = 0;
LO_Element *last_documented_xref = 0;
LO_AnchorData *last_documented_anchor_data = 0;

int xfeKeycodeToWhich(KeyCode keycode,
		      Modifiers modifiers)
{
  Modifiers modout;
  KeySym res;
  XtTranslateKeycode(fe_display,
		     keycode, modifiers,
		     &modout,&res);
  return res;
}

/* takes the state field from a {Button,Key}{Press,Release} event and
 *  maps it into JS event flags.
 */
int xfeToLayerModifiers(int state)
{
  return (  ((state & ShiftMask) ? EVENT_SHIFT_MASK : 0)
	  | ((state & ControlMask) ? EVENT_CONTROL_MASK : 0)
	  | ((state & Mod1Mask) ? EVENT_ALT_MASK : 0)
	  | ((state & (  Mod2Mask
		       | Mod3Mask
		       | Mod4Mask
		       | Mod5Mask
		       )) ? EVENT_META_MASK : 0)
	  );
}

/* This is in the set of function pointers, but there is no
   definition for it. */
MWContext*
XFE_CreateNewDocWindow(MWContext * calling_context,URL_Struct * URL)
{
	if (calling_context) 
	{
		Widget widget = CONTEXT_WIDGET (calling_context);
		
		if (widget)
		{
			Widget app_shell = XfeAncestorFindApplicationShell(widget);

			if (XfeIsAlive(app_shell))
			{
				return XFE_showBrowser(app_shell,URL);
			}
		}
	}/* if */

	return NULL;
}

/* Translate the string from ISO-8859/1 to something that the window
   system can use (for X, this is nearly a no-op.)
 */
char *
XFE_TranslateISOText (MWContext *context, int charset, char *ISO_Text)
{
  unsigned char *s;

  /* charsets such as Shift-JIS contain 0240's that are valid */
  if (INTL_CharSetType(charset) != SINGLEBYTE)
    return ISO_Text;

  /* When &nbsp; is encountered, display a normal space character instead.
     This is necessary because the MIT fonts are messed up, and have a
     zero-width character for nobreakspace, so we need to print it as a
     normal space instead. */
  if (ISO_Text)
    for (s = (unsigned char *) ISO_Text; *s; s++)
      if (*s == 0240) *s = ' ';

  return ISO_Text;
}

struct fe_gc_data
{
  unsigned long flags;
  XGCValues gcv;
  Region clip_region;
  GC gc;
};

/* The GC cache is shared among all windows, since it doesn't hog
   any scarce resources (like colormap entries.) */
static struct fe_gc_data fe_gc_cache [30] = { { 0, }, };
static int fe_gc_cache_fp;
static int fe_gc_cache_wrapped_p = 0;

/* Dispose of entries matching the given flags, compressing the GC cache */
void
fe_FlushGCCache (Widget widget, unsigned long flags)
{
  int i, new_fp;

  Display *dpy = XtDisplay (widget);
  int maxi = (fe_gc_cache_wrapped_p ? countof (fe_gc_cache) : fe_gc_cache_fp);
  new_fp = 0;
  for (i = 0; i < maxi; i++)
    {
      if (fe_gc_cache [i].flags & flags)
        {
          XFreeGC (dpy, fe_gc_cache [i].gc);
          if (fe_gc_cache [i].clip_region)
              FE_DestroyRegion(fe_gc_cache [i].clip_region);
          memset (&fe_gc_cache [i], 0,  sizeof (fe_gc_cache [i]));
        }
      else
        fe_gc_cache[new_fp++] = fe_gc_cache[i];
    }
  if (new_fp == countof (fe_gc_cache))
    {
      fe_gc_cache_wrapped_p = 1;
      fe_gc_cache_fp = 0;
    }
  else
    {
      fe_gc_cache_wrapped_p = 0;
      fe_gc_cache_fp = new_fp;
    }
}

GC
fe_GetGCfromDW(Display* dpy, Window win, unsigned long flags, XGCValues *gcv,
               Region clip_region)
{
  int i;
  for (i = 0;
       i < (fe_gc_cache_wrapped_p ? countof (fe_gc_cache) : fe_gc_cache_fp);
       i++)
      {
          if (flags == fe_gc_cache [i].flags &&
              !memcmp (gcv, &fe_gc_cache [i].gcv, sizeof (*gcv)))
              if (clip_region)
                  {
                      if (fe_gc_cache[i].clip_region &&
                          XEqualRegion(clip_region,
                                       fe_gc_cache[i].clip_region))
                          return fe_gc_cache [i].gc;
                  }
              else
                  {
                      if(!fe_gc_cache[i].clip_region)
                          return fe_gc_cache [i].gc;
                  }
      }

  {
    GC gc;
    int this_slot = fe_gc_cache_fp;
    int clear_p = fe_gc_cache_wrapped_p;

    fe_gc_cache_fp++;
    if (fe_gc_cache_fp >= countof (fe_gc_cache))
      {
	fe_gc_cache_fp = 0;
	fe_gc_cache_wrapped_p = 1;
      }

    if (clear_p)
      {
		  XFreeGC (dpy, fe_gc_cache [this_slot].gc);
		  if (fe_gc_cache [this_slot].clip_region)
			  FE_DestroyRegion(fe_gc_cache [this_slot].clip_region);
		  fe_gc_cache [this_slot].gc = NULL;
		  fe_gc_cache [this_slot].clip_region = NULL;
      }

    gc = XCreateGC (dpy, win, flags, gcv);

    fe_gc_cache [this_slot].flags = flags;
    fe_gc_cache [this_slot].gcv = *gcv;
    fe_gc_cache [this_slot].clip_region = NULL;
    if (clip_region) {
		fe_gc_cache [this_slot].clip_region = FE_CopyRegion(clip_region, NULL);

		if (fe_gc_cache [this_slot].clip_region) {
			XSetRegion(dpy, gc, fe_gc_cache [this_slot].clip_region);
		}
	}

    fe_gc_cache [this_slot].gc = gc;

    return gc;
  }
}

GC
fe_GetClipGC(Widget widget, unsigned long flags, XGCValues *gcv,
             Region clip_region)
{
    Display *dpy = XtDisplay (widget);
    Window win = XtWindow (widget);

    return fe_GetGCfromDW(dpy, win, flags, gcv, clip_region);
}

GC
fe_GetGC(Widget widget, unsigned long flags, XGCValues *gcv)
{
    Display *dpy = XtDisplay (widget);
    Window win = XtWindow (widget);

    return fe_GetGCfromDW(dpy, win, flags, gcv, NULL);
}

static GC
fe_get_text_gc (MWContext *context, LO_TextAttr *text, fe_Font *font_ret,
		Boolean *selected_p, Boolean blunk)
{
  unsigned long flags;
  XGCValues gcv;
  Widget widget = CONTEXT_WIDGET (context);
  fe_Font font = fe_LoadFontFromFace (context, text, &text->charset, text->font_face,
			      text->size, text->fontmask);
  Display *dpy = XtDisplay (widget);
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  Drawable drawable = fe_drawable->xdrawable;

  Pixel fg, bg;
  bg = fe_GetPixel(context,
                   text->bg.red, text->bg.green, text->bg.blue);
  fg = fe_GetPixel(context,
                   text->fg.red, text->fg.green, text->fg.blue);

/*  if (text->attrmask & LO_ATTR_ANCHOR)
    fg = CONTEXT_DATA (context)->xref_pixel;*/

  if (selected_p && *selected_p)
    {
      Pixel nfg = CONTEXT_DATA (context)->select_fg_pixel;
      Pixel nbg = CONTEXT_DATA (context)->select_bg_pixel;

      fg = nfg;
      bg = nbg;
    }

  if (blunk)
    fg = bg;

  if (! font) return NULL;

  memset (&gcv, ~0, sizeof (gcv));

  flags = 0;
  FE_SET_GC_FONT(text->charset, &gcv, font, &flags);
  gcv.foreground = fg;
  gcv.background = bg;
  flags |= (GCForeground | GCBackground);

  if (font_ret) *font_ret = font;

  return fe_GetGCfromDW (dpy, drawable,
                         flags, &gcv, fe_drawable->clip_region);
}

/* Given text and attributes, returns the size of those characters.
 */
int
XFE_GetTextInfo (MWContext *context,
		LO_TextStruct *text,
		LO_TextInfo *text_info)
{
  fe_Font font;
  char *str = (char *) text->text;
  int length = text->text_len;
  int remaining = length;

  font = fe_LoadFontFromFace (context, text->text_attr,
			      &text->text_attr->charset,
			      text->text_attr->font_face,
			      text->text_attr->size,
			      text->text_attr->fontmask);
  /* X is such a winner, it uses 16 bit quantities to represent all pixel
     widths.  This is really swell, because it means that if you've got
     a large font, you can't correctly compute the size of strings which
     are only a few thousand characters long.  So, when the string is more
     than N characters long, we divide up our calls to XTextExtents to
     keep the size down so that the library doesn't run out of fingers
     and toes.
   */
#define SUCKY_X_MAX_LENGTH 600

  text_info->ascent = 14;
  text_info->descent = 3;
  text_info->max_width = 0;
  text_info->lbearing = 0;
  text_info->rbearing = 0;
  if (! font) return 0;

  do
    {
      int L = (remaining > SUCKY_X_MAX_LENGTH ? SUCKY_X_MAX_LENGTH :
	       remaining);
      int ascent, descent;
      XCharStruct overall;
      FE_TEXT_EXTENTS (text->text_attr->charset, font, str, L,
		    &ascent, &descent, &overall);
      /* ascent and descent are per the font, not per this text. */
      text_info->ascent = ascent;
      text_info->descent = descent;

      text_info->max_width += overall.width;

#define FOO(x,y) if (y > x) x = y
      FOO (text_info->lbearing,   overall.lbearing);
      FOO (text_info->rbearing,   overall.rbearing);
      /*
       * If font metrics were set right, overall.descent should never exceed
       * descent, but since there are broken fonts in the world.
       */
      FOO (text_info->descent,   overall.descent);
#undef FOO

      str += L;
      remaining -= L;
    }
  while (remaining > 0);

  /* What is the return value expected to be?
     layout/layout.c doesn't seem to use it. */
  return 0;
}


/* Draws a rectangle with dropshadows on the drawing_area window.
   shadow_width is the border thickness (they go inside the rect).
   shadow_style is XmSHADOW_IN or XmSHADOW_OUT.
 */
void
fe_DrawSelectedShadows(MWContext *context, fe_Drawable *fe_drawable,
                       int x, int y, int width, int height,
                       int shadow_width, int shadow_style, Boolean selected)
{
  Widget widget = CONTEXT_WIDGET (context);
  Display *dpy = XtDisplay (widget);
  XGCValues gcv1, gcv2;
  GC gc1, gc2;
  unsigned long flags;
  Drawable drawable = fe_drawable->xdrawable;

  memset (&gcv1, ~0, sizeof (gcv1));
  memset (&gcv2, ~0, sizeof (gcv2));

#ifdef EDITOR
  if (selected)
  {
      flags = GCForeground;
      gcv1.foreground = CONTEXT_DATA(context)->select_bg_pixel;
      gcv2.foreground = CONTEXT_DATA(context)->select_bg_pixel;
      if (shadow_width < 1)
          shadow_width = 1;
  }
  else if (EDT_IS_EDITOR(context) && shadow_width < 1)
  {
      flags = GCForeground;
      gcv1.foreground = CONTEXT_DATA(context)->bg_pixel;
      gcv2.foreground = CONTEXT_DATA(context)->bg_pixel;
      shadow_width = 1;
  }
  else
#endif /* EDITOR */

  if (CONTEXT_DATA (context)->backdrop_pixmap ||
      CONTEXT_DATA (context)->bg_pixel !=
      CONTEXT_DATA (context)->default_bg_pixel)
    {
      static Pixmap gray50 = 0;
      if (! gray50)
	{
#	  define gray50_width  8
#	  define gray50_height 2
	  static char gray50_bits[] = { 0x55, 0xAA };
	  gray50 =
	    XCreateBitmapFromData (XtDisplay (widget),
				   RootWindowOfScreen (XtScreen (widget)),
				   gray50_bits, gray50_width, gray50_height);
	}
      flags = (GCForeground | GCStipple | GCFillStyle |
	       GCTileStipXOrigin | GCTileStipYOrigin);

#ifdef EDITOR
      if (EDT_IS_EDITOR(context))
      {
          flags |= GCBackground;
          gcv1.background = CONTEXT_DATA(context)->bg_pixel;
          gcv1.fill_style = FillOpaqueStippled;
      }
      else
#endif /* EDITOR */
      gcv1.fill_style = FillStippled;
      gcv1.stipple = gray50;
      gcv1.ts_x_origin = -CONTEXT_DATA (context)->document_x;
      gcv1.ts_y_origin = -CONTEXT_DATA (context)->document_y;
      gcv1.foreground = fe_GetPixel (context, 0xFF, 0xFF, 0xFF);
      gcv2 = gcv1;
      gcv2.foreground = fe_GetPixel (context, 0x00, 0x00, 0x00);
    }
  else
    {
      flags = GCForeground;
      gcv1.foreground = CONTEXT_DATA (context)->top_shadow_pixel;
      gcv2.foreground = CONTEXT_DATA (context)->bottom_shadow_pixel;
    }

#ifdef EDITOR_OLD
  /* We don't want this for tables any more; need to test on
   * other elements which use this code to make sure nothing else
   * depends on this behavior.
   */
  if (selected) {
      gcv1.background = CONTEXT_DATA(context)->select_bg_pixel;
      gcv1.line_style = LineDoubleDash;

      gcv2.background = CONTEXT_DATA(context)->select_bg_pixel;
      gcv2.line_style = LineDoubleDash;

      flags |= GCLineStyle|GCBackground;
  }
#endif /*EDITOR*/

  gc1 = fe_GetGCfromDW (fe_display, drawable, flags, &gcv1, fe_drawable->clip_region);
  gc2 = fe_GetGCfromDW (fe_display, drawable, flags, &gcv2, fe_drawable->clip_region);

  _XmDrawShadows (dpy, drawable, gc1, gc2, x, y, width, height,
		  shadow_width, shadow_style);
}

void
fe_DrawShadows (MWContext *context, fe_Drawable *fe_drawable,
		int x, int y, int width, int height,
		int shadow_width, int shadow_style)
{
    fe_DrawSelectedShadows(context, fe_drawable,
                           x, y, width, height,
                           shadow_width, shadow_style, FALSE);

}


/* Put some text on the screen at the given location with the given
   attributes.  (What is iLocation?  It is ignored.)
 */
static void
fe_display_text (MWContext *context, int iLocation, LO_TextStruct *text,
		 int32 start, int32 end, Boolean blunk)
{
  Widget shell = CONTEXT_WIDGET (context);
  Widget drawing_area = CONTEXT_DATA (context)->drawing_area;
  Display *dpy = XtDisplay (shell);
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  Drawable drawable = fe_drawable->xdrawable;
  fe_Font font;
  int ascent, descent;
  GC gc;
  long x, y;
  long x_offset = 0;
  Boolean selected_p = False;


  if ((text->ele_attrmask & LO_ELE_SELECTED) &&
      (start >= text->sel_start) && (end-1 <= text->sel_end))
    selected_p = True;
  
  gc = fe_get_text_gc (context, text->text_attr, &font, &selected_p, blunk);
  if (!gc)
    {
      return;
    }
  
  FE_FONT_EXTENTS(text->text_attr->charset, font, &ascent, &descent);
  
  x = (text->x + text->x_offset
       - CONTEXT_DATA (context)->document_x);
  y = (text->y + text->y_offset + ascent
       - CONTEXT_DATA (context)->document_y);
  x += fe_drawable->x_origin;
  y += fe_drawable->y_origin;
  
  if (text->text_len == 0)
    return;
  
  if (! XtIsRealized (drawing_area))
    return;
  
  if ((x > 0 && x > CONTEXT_DATA (context)->scrolled_width) ||
      (y > 0 && y > (CONTEXT_DATA (context)->scrolled_height +
                     text->y_offset + ascent)) ||
      (x + text->width < 0) ||
      (y + text->line_height < 0))
    return;
  
  if (start < 0)
	start = 0;
  if (end > text->text_len)
	end = text->text_len;
  
  if (end - start > SUCKY_X_MAX_LENGTH)
    /* that's a fine way to make X blow up *real* good! */
    end = start + SUCKY_X_MAX_LENGTH;
  
  /* #### Oh, this doesn't even work, because Bina is
     passing us massively negative (> 16 bit) starting pixel positions.
  */
  
  if (start > 0)
    {
      XCharStruct overall;
      int ascent, descent;
      if (! font) abort ();
      FE_TEXT_EXTENTS (text->text_attr->charset, font, (char *) text->text,
                       start, &ascent, &descent, &overall);
      x_offset = overall.width;
      x += x_offset;
    }
  
  
  if (blunk)
    ;	/* No text to draw. */
  else if (!selected_p && text->text_attr->no_background)
    FE_DRAW_STRING (text->text_attr->charset, dpy, drawable, font, gc, x, y,
                    ((char *) text->text) + start, end - start);
  else
    {
      GC gc2;
      gc2 = fe_get_text_gc (context, text->text_attr, &font, &selected_p, TRUE);
      if (!gc2)
		gc2 = gc;
      
      FE_DRAW_IMAGE_STRING (text->text_attr->charset, dpy, drawable, font, gc, gc2,
                            x, y, ((char *) text->text)+start, end - start);
    }
  
  /* Anchor text is no longer underlined by the front end.
   * We deliberately do not test for the LO_ATTR_ANCHOR bit in the attr mask.
   */
  if (text->text_attr->attrmask & 
      (LO_ATTR_UNDERLINE | LO_ATTR_SPELL | LO_ATTR_STRIKEOUT)
#if 0
      || (text->height < text->line_height)
#endif
      )
    {
      int upos;
      unsigned int uthick;
      int ul_width;
      
      if (start == 0 && end == text->text_len)
        {
          ul_width = text->width;
        }
      else
        {
          XCharStruct overall;
          int ascent, descent;
          if (! font) abort ();
          FE_TEXT_EXTENTS (text->text_attr->charset, font,
                           (char *) text->text+start, end-start, &ascent, &descent,
                           &overall);
          ul_width = overall.width;
        }
      
#if 0
      if (text->height < text->line_height)
        {
          /* If the text is shorter than the line, then XDrawImageString()
             won't fill in the whole background - so we need to do that by
             hand. */
          GC gc;
          XGCValues gcv;
          memset (&gcv, ~0, sizeof (gcv));
          gcv.foreground = (text->selected
                            ? CONTEXT_DATA (context)->highlight_bg_pixel
                            : fe_GetPixel (context,
                                           text->text_attr->bg.red,
                                           text->text_attr->bg.green,
                                           text->text_attr->bg.blue));
        }
#endif
      
      if (text->text_attr->attrmask & LO_ATTR_UNDERLINE)
        {
          int lineDescent;
          upos = fe_GetUnderlinePosition(text->text_attr->charset);
          lineDescent = text->line_height - text->y_offset - ascent - 1;

          if (upos > lineDescent && lineDescent > 0)
            upos = lineDescent;

          XDrawLine (dpy, drawable, gc, x, y + upos, x + ul_width, y + upos);
        }
      
	  if (text->text_attr->attrmask & LO_ATTR_SPELL)
        {
          int lineDescent;
          GC gc2;
          XGCValues gcv2;
          
          memset (&gcv2, ~0, sizeof (gcv2));
          gcv2.foreground = fe_GetPixel(context, 0xFF, 0x00, 0x00);
          
          gc2 = fe_GetGC (CONTEXT_WIDGET (context), GCForeground, &gcv2);
          
          upos = fe_GetUnderlinePosition(text->text_attr->charset);
          lineDescent = text->line_height - text->y_offset - ascent - 1;
          if (upos > lineDescent)
			upos = lineDescent;
          XDrawLine (dpy, drawable, gc2, x, y + upos, x + ul_width, y + upos);
        }

      if (text->text_attr->attrmask & LO_ATTR_STRIKEOUT)
        {
          upos = fe_GetStrikePosition(text->text_attr->charset, font);
          uthick = (ascent / 8);
          if (uthick <= 1)
            XDrawLine (dpy, drawable, gc, x, y + upos, x + ul_width, y + upos);
          else
            XFillRectangle (dpy, drawable, gc, x, y + upos, ul_width, uthick);
        }
    }
}

void
XFE_DisplayText (MWContext *context, int iLocation, LO_TextStruct *text,
	XP_Bool need_bg)
{
  fe_display_text (context, iLocation, text, 0, text->text_len, False);
}

void
XFE_DisplaySubtext (MWContext *context, int iLocation,
		   LO_TextStruct *text, int32 start_pos, int32 end_pos,
		   XP_Bool need_bg)
{
  fe_display_text (context, iLocation, text, start_pos, end_pos + 1, False);
}

#ifdef EDITOR

/*
 *    End of line indicator.
 */
typedef struct fe_bitmap_info
{
    unsigned char* bits;
    Dimension      width;
    Dimension      height;
    Pixmap         pixmap;
} fe_bitmap_info;

#define line_feed_width 7
#define line_feed_height 10
static unsigned char line_feed_bits[] = { /* lifted from Lucid Emacs */
    0x00, 0xbc, 0xfc, 0xe0, 0xe0, 0x72, 0x3e, 0x1e, 0x1e, 0x3e};
#define page_mark_width 8
#define page_mark_height 15
static char page_mark_bits[] = { /* from RobinS */
 0xfe,0x4f,0x4f,0x4f,0x4f,0x4e,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48};

static fe_bitmap_info fe_line_feed_bitmap =
{ line_feed_bits, line_feed_width, line_feed_height };
static fe_bitmap_info fe_page_mark_bitmap =
{ page_mark_bits, page_mark_width, page_mark_height };

static Display* fe_line_feed_display;

static Pixmap
fe_make_line_feed_pixmap(Display* display, int type,
			 Dimension* r_width, Dimension* r_height)
{
    fe_bitmap_info* info;

    if (type == LO_LINEFEED_BREAK_PARAGRAPH)
	info = &fe_page_mark_bitmap;
    else
	info = &fe_line_feed_bitmap;

    if (fe_line_feed_display != display && fe_line_feed_display != 0) {
	if (info->pixmap != 0)
	    XFreePixmap(fe_line_feed_display, info->pixmap);
    }

    if (info->pixmap == 0) {

	info->pixmap = XCreateBitmapFromData(display,
					     DefaultRootWindow(display),
					     info->bits,
					     info->width,
					     info->height);
 	
	fe_line_feed_display = display;
    }

    *r_width = info->width;
    *r_height = info->height;
    
    return info->pixmap;
}

#endif /*EDITOR*/

/* Display a glyph representing a linefeed at the given location with the
   given attributes.  This looks just like a " " character.  (What is
   iLocation?  It is ignored.)
 */
void
XFE_DisplayLineFeed (MWContext *context,
		    int iLocation, LO_LinefeedStruct *line_feed, XP_Bool need_bg)
{
  GC gc;
  XGCValues gcv;
  unsigned long flags;
  LO_TextAttr *text = line_feed->text_attr;

#ifdef EDITOR
  if (EDT_IS_EDITOR(context)
      &&
      EDT_DISPLAY_PARAGRAPH_MARKS(context)
      &&
      (line_feed->break_type != LO_LINEFEED_BREAK_SOFT)
      &&
      (!line_feed->prev || line_feed->prev->lo_any.edit_offset >= 0)) {

      LO_Color* fg_color;
      Display*  display = XtDisplay(CONTEXT_WIDGET(context));
      Window    window = XtWindow(CONTEXT_DATA(context)->drawing_area);
      Position  target_x;
      Position  target_y;
      Dimension width;
      Dimension height;
      Pixmap    bitmap;

      bitmap = fe_make_line_feed_pixmap(display,
					line_feed->break_type,
					&width,
					&height);
      
      target_x = line_feed->x + line_feed->x_offset
	         - CONTEXT_DATA(context)->document_x;

      target_y = line_feed->y + line_feed->y_offset
		 - CONTEXT_DATA(context)->document_y;

      memset (&gcv, ~0, sizeof (gcv));

      if ((line_feed->ele_attrmask & LO_ELE_SELECTED) != 0)
	  fg_color = &text->bg; /* layout delivers inverted colors */
      else
	  fg_color = &text->fg;

      gcv.foreground = fe_GetPixel(context,
				   fg_color->red,
				   fg_color->green,
				   fg_color->blue);
      gcv.graphics_exposures = False;
      gcv.clip_mask = bitmap;
      gcv.clip_x_origin = target_x;
      gcv.clip_y_origin = target_y;

      flags = GCClipMask|GCForeground|GCClipXOrigin| \
	      GCClipYOrigin|GCGraphicsExposures;

      gc = fe_GetGC(CONTEXT_DATA(context)->drawing_area, flags, &gcv);

      if (height > line_feed->height)
	  height = line_feed->height;

      XCopyPlane(display, bitmap, window,
		 gc,
		 0, 0, width, height,
		 target_x, target_y,
		 1L);
  }
#endif /*EDITOR*/
}

/* Display a horizontal line at the given location.
 */
void
XFE_DisplayHR (MWContext *context, int iLocation, LO_HorizRuleStruct *hr)
{
  int shadow_width = 1;			/* #### customizable? */
  int shadow_style = XmSHADOW_IN;	/* #### customizable? */
  int thickness = hr->thickness;
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  Drawable drawable = fe_drawable->xdrawable;
  long x = hr->x + hr->x_offset - CONTEXT_DATA (context)->document_x +
      fe_drawable->x_origin;
  long y = hr->y + hr->y_offset - CONTEXT_DATA (context)->document_y +
      fe_drawable->y_origin;
  int w = hr->width;

  if ((x > 0 && x > CONTEXT_DATA (context)->scrolled_width) ||
      (y > 0 && y > CONTEXT_DATA (context)->scrolled_height) ||
      (x + hr->width < 0) ||
      (y + hr->line_height < 0))
    return;

  thickness -= (shadow_width * 2);
  if (thickness < 0) thickness = 0;

#ifdef EDITOR
  /*
   *    Don't draw the editor's end-of-document hrule unless we're
   *    displaying paragraph marks.
   */
  if (hr->edit_offset < 0 && !EDT_DISPLAY_PARAGRAPH_MARKS(context)) {
      return;
  }
#endif /* EDITOR */

  if (hr->ele_attrmask & LO_ELE_SHADED)
    {
#ifdef EDITOR
	fe_DrawSelectedShadows(context, fe_drawable,
			       x, y, w, thickness + (shadow_width * 2),
			       shadow_width, shadow_style,
			       ((hr->ele_attrmask & LO_ELE_SELECTED) != 0));
#else
	fe_DrawShadows (context, fe_drawable, x, y, w,
                        thickness + (shadow_width * 2),
                        shadow_width, shadow_style);
#endif /* EDITOR */
    }
  else
    {
      Display *dpy = XtDisplay (CONTEXT_DATA (context)->drawing_area);
      GC gc;
      XGCValues gcv;

      memset (&gcv, ~0, sizeof(gcv));
      gcv.foreground = CONTEXT_DATA(context)->fg_pixel;
      gc = fe_GetClipGC(CONTEXT_WIDGET (context), GCForeground, &gcv,
                        fe_drawable->clip_region);
      XFillRectangle(dpy, drawable, gc, x, y, w, hr->height);
#ifdef EDITOR
      if ((hr->ele_attrmask & LO_ELE_SELECTED) != 0) {
	  gcv.background = CONTEXT_DATA(context)->select_bg_pixel;
	  gcv.line_width = 1;
	  gcv.line_style = LineDoubleDash;
	  gc = fe_GetGC(CONTEXT_WIDGET(context),
			GCForeground|GCBackground|GCLineWidth|GCLineStyle,
			&gcv);
	  XDrawRectangle(dpy, drawable, gc, x, y, w-1, hr->height-1);
      }
#endif /* EDITOR */
    }
}

void
XFE_DisplayBullet (MWContext *context, int iLocation, LO_BulletStruct *bullet)
{
  int w,h;
  Boolean hollow_p;
  GC gc;
  Drawable drawable;

  Widget widget = CONTEXT_WIDGET (context);
  Display *dpy = XtDisplay (widget);

  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  long x = bullet->x + bullet->x_offset - CONTEXT_DATA (context)->document_x +
      fe_drawable->x_origin;
  long y = bullet->y + bullet->y_offset - CONTEXT_DATA (context)->document_y +
      fe_drawable->y_origin;
  drawable = fe_drawable->xdrawable;
  w = bullet->width;
  h = bullet->height;
  hollow_p = (bullet->bullet_type != BULLET_BASIC);

  if ((x > 0 && x > CONTEXT_DATA (context)->scrolled_width) ||
      (y > 0 && y > CONTEXT_DATA (context)->scrolled_height) ||
      (x + w < 0) ||
      (y + h < 0))
    return;

  gc = fe_get_text_gc (context, bullet->text_attr, 0, 0, False);
  if (!gc)
  {
	return;
  }
  switch (bullet->bullet_type)
    {
    case BULLET_BASIC:
    case BULLET_ROUND:
      /* Subtract 1 to compensate for the behavior of XDrawArc(). */
      w -= 1;
      h -= 1;
      /* Now round up to an even number so that the circles look nice. */
      if (! (w & 1)) w++;
      if (! (h & 1)) h++;
      if (hollow_p)
	XDrawArc (dpy, drawable, gc, x, y, w, h, 0, 360*64);
      else
	XFillArc (dpy, drawable, gc, x, y, w, h, 0, 360*64);
      break;
    case BULLET_SQUARE:
      if (hollow_p)
	XDrawRectangle (dpy, drawable, gc, x, y, w, h);
      else
	XFillRectangle (dpy, drawable, gc, x, y, w, h);
      break;
    case BULLET_MQUOTE:
		/*
		 * WARNING... [ try drawing a 2 pixel wide filled rectangle ]
		 *
		 *
		 */
#ifdef DOH_WHICH_ONE
        XDrawLine (dpy, drawable, gc, x, y, x, (y + h));
#else
		w = 2;
		XFillRectangle (dpy, drawable, gc, x, y, w, h);
#endif
		break;

	case BULLET_NONE:
		/* Do nothing. */
		break;

    default:
		XP_ASSERT(0);
    }
}

void
XFE_DisplaySubDoc (MWContext *context, int loc, LO_SubDocStruct *sd)
{
  int shadow_style = XmSHADOW_IN;	/* #### customizable? */
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  /*Drawable drawable = fe_drawable->xdrawable;*/
  long x = sd->x + sd->x_offset - CONTEXT_DATA (context)->document_x +
      fe_drawable->x_origin;
  long y = sd->y + sd->y_offset - CONTEXT_DATA (context)->document_y +
      fe_drawable->y_origin;

  if ((x > 0 && x > CONTEXT_DATA (context)->scrolled_width) ||
      (y > 0 && y > CONTEXT_DATA (context)->scrolled_height) ||
      (x + sd->width < 0) ||
      (y + sd->line_height< 0))
    return;

  fe_DrawShadows (context, fe_drawable, x, y, sd->width, sd->height,
		  sd->border_width, shadow_style);
}

void
XFE_DisplayCell (MWContext *context, int loc, LO_CellStruct *cell)
{
  int shadow_style = XmSHADOW_IN;	/* #### customizable? */
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  long x = cell->x + cell->x_offset - CONTEXT_DATA (context)->document_x +
      fe_drawable->x_origin;
  long y = cell->y + cell->y_offset - CONTEXT_DATA (context)->document_y +
      fe_drawable->y_origin;
  int border_width = cell->border_width;

  if ((x > 0 && x > CONTEXT_DATA (context)->scrolled_width) ||
      (y > 0 && y > CONTEXT_DATA (context)->scrolled_height) ||
      (x + cell->width < 0) ||
      (y + cell->line_height< 0))
    return;

#ifdef EDITOR
  if (EDT_IS_EDITOR(context))
  {
      Boolean selected = ((cell->ele_attrmask & LO_ELE_SELECTED) != 0);
      fe_DrawSelectedShadows (context, fe_drawable,
                              x, y, cell->width, cell->height,
                              border_width, shadow_style, selected);
  }
  else
#endif /* EDITOR */
  fe_DrawShadows (context, fe_drawable, x, y, cell->width, cell->height,
                  cell->border_width, shadow_style);
}

typedef struct
{
  Dimension left;
  Dimension top;
  Dimension right;
  Dimension bottom;
} FE_BorderWidths;

static void
fe_DisplaySolidBorder(MWContext *context, LO_TableStruct *ts,
					  XRectangle *rect, FE_BorderWidths *widths)
{
  XGCValues gcv;
  unsigned long gc_flags;
  GC gc;
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;

  /* if they're all the same width, just vary the line thickness and use XDrawRectangle */
  if (widths->left == widths->top && widths->left == widths->right && widths->left == widths->bottom)
	{
	  gc_flags = GCForeground | GCLineWidth;

	  gcv.foreground = fe_GetPixel(context, 
								   ts->border_color.red,
								   ts->border_color.green,
								   ts->border_color.blue);

	  gcv.line_width = widths->left; /* doesn't really matter which one we choose. */

          if (ts->border_style == BORDER_DASHED
              || ts->border_style == BORDER_DOTTED)
          {
              gc_flags |= GCLineStyle;
              gcv.line_style = LineOnOffDash;
          }

	  gc = fe_GetGCfromDW(fe_display, fe_drawable->xdrawable, gc_flags, &gcv, fe_drawable->clip_region);

	  rect->x += widths->left / 2;
	  rect->y += widths->left / 2;
	  rect->width -= widths->left;
	  rect->height -= widths->left;

	  XDrawRectangle(fe_display,
					 XtWindow(CONTEXT_DATA(context)->drawing_area),
					 gc,
					 rect->x, rect->y, rect->width, rect->height);
          /* set the line attributes back to solid in case we changed them */
          if (ts->border_style == BORDER_DASHED
              || ts->border_style == BORDER_DOTTED)
          {
              gc_flags = GCLineStyle;
              gcv.line_style = LineSolid;     /* should get previous value? */
              XChangeGC(fe_display, gc, gc_flags, &gcv);
          }
	}
  else
	{
	  /* since they are (possibly) all different, we do each border will XFillRectangle */
	  gc_flags = GCForeground;

	  gcv.foreground = fe_GetPixel(context,
								   ts->border_color.red,
								   ts->border_color.green,
								   ts->border_color.blue);


	  gc = fe_GetGCfromDW(fe_display, fe_drawable->xdrawable, gc_flags, &gcv, fe_drawable->clip_region);

	  if (widths->left > 0)
		XFillRectangle(fe_display,
					   XtWindow(CONTEXT_DATA(context)->drawing_area),
					   gc,
					   rect->x, rect->y, widths->left, rect->height);

	  if (widths->top > 0)
		XFillRectangle(fe_display,
					   XtWindow(CONTEXT_DATA(context)->drawing_area),
					   gc,
					   rect->x, rect->y, rect->width, widths->top);

	  if (widths->right > 0)
		XFillRectangle(fe_display,
					   XtWindow(CONTEXT_DATA(context)->drawing_area),
					   gc,
					   rect->x + rect->width - widths->right, rect->y, widths->right, rect->height);

	  if (widths->bottom > 0)
		XFillRectangle(fe_display,
					   XtWindow(CONTEXT_DATA(context)->drawing_area),
					   gc,
					   rect->x, rect->y + rect->height - widths->bottom, rect->width, widths->bottom);
	}
}

static void
fe_DisplayDoubleBorder(MWContext *context, LO_TableStruct *ts,
					   XRectangle *rect, FE_BorderWidths *widths)
{
  FE_BorderWidths stroke_widths;

  stroke_widths.left = (widths->left + 1) / 3;
  stroke_widths.top = (widths->top + 1) / 3;
  stroke_widths.right = (widths->right + 1) / 3;
  stroke_widths.bottom = (widths->bottom + 1) / 3;

  /* draw the outer lines */
  fe_DisplaySolidBorder(context, ts, rect, &stroke_widths);

  /* adjust the rectangle to be the inner one */
  rect->x += (widths->left - stroke_widths.left);
  rect->y += (widths->top - stroke_widths.top);
  rect->width -= (widths->right - stroke_widths.right) + (widths->left - stroke_widths.left);
  rect->height -= (widths->bottom - stroke_widths.bottom) + (widths->top - stroke_widths.top);

  /* draw the inner lines */
  fe_DisplaySolidBorder(context, ts, rect, &stroke_widths);
}

/* warning.  much of this gc related code is duplicated from the fe_DrawSelectedShadows function above */
static void
fe_Display3DBorder(MWContext *context, LO_TableStruct *ts,
				   XRectangle *rect, FE_BorderWidths *widths,
				   Pixel top_shadow, Pixel bottom_shadow)
{
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  XGCValues gcv1, gcv2;
  unsigned long gc_flags = GCForeground;
  GC gc1, gc2;

  memset (&gcv1, ~0, sizeof (gcv1));
  memset (&gcv2, ~0, sizeof (gcv2));

  if (CONTEXT_DATA (context)->backdrop_pixmap ||
      CONTEXT_DATA (context)->bg_pixel !=
      CONTEXT_DATA (context)->default_bg_pixel)
    {
      static Pixmap gray50 = 0;
      if (! gray50)
		{
#	  define gray50_width  8
#	  define gray50_height 2
		  static char gray50_bits[] = { 0x55, 0xAA };
		  gray50 =
			XCreateBitmapFromData (fe_display,
								   RootWindowOfScreen (XtScreen (CONTEXT_WIDGET(context))),
								   gray50_bits, gray50_width, gray50_height);
		}
      gc_flags = (GCForeground | GCStipple | GCFillStyle |
				  GCTileStipXOrigin | GCTileStipYOrigin);
	  
      gcv1.fill_style = FillStippled;
      gcv1.stipple = gray50;
      gcv1.ts_x_origin = -CONTEXT_DATA (context)->document_x;
      gcv1.ts_y_origin = -CONTEXT_DATA (context)->document_y;
      gcv1.foreground = fe_GetPixel (context, 0xFF, 0xFF, 0xFF);
      gcv2 = gcv1;
      gcv2.foreground = fe_GetPixel (context, 0x00, 0x00, 0x00);
    }
  else
    {
      gc_flags = GCForeground;
	  gcv1.foreground = top_shadow;
	  gcv2.foreground = bottom_shadow;
    }
  
  gc1 = fe_GetGCfromDW (fe_display, fe_drawable->xdrawable, gc_flags, &gcv1, fe_drawable->clip_region);
  gc2 = fe_GetGCfromDW (fe_display, fe_drawable->xdrawable, gc_flags, &gcv2, fe_drawable->clip_region);
  
  if (widths->left == widths->top && widths->left == widths->right && widths->left == widths->bottom)
	{
	  _XmDrawShadows(fe_display,
					 fe_drawable->xdrawable,
					 gc1, gc2, rect->x, rect->y, rect->width, rect->height,
					 widths->left, XmSHADOW_OUT);
	}
  else
	{
	  XPoint points[6];

	  points[0].x = rect->x + widths->left;
	  points[0].y = rect->y + widths->top;
	  points[1].x = rect->x + widths->left;
	  points[1].y = rect->y + rect->height - widths->bottom;
	  points[2].x = rect->x;
	  points[2].y = rect->y + rect->height;
	  points[3].x = rect->x;
	  points[3].y = rect->y;
	  points[4].x = rect->x + rect->width;
	  points[4].y = rect->y;
	  points[5].x = rect->x + rect->width - widths->right;
	  points[5].y = rect->y + widths->top;
	  
	  XFillPolygon(fe_display,
				   fe_drawable->xdrawable,
				   gc1,
				   points, 6, 
				   Nonconvex,
				   CoordModeOrigin);

	  points[0].x = rect->x + rect->width - widths->right;
	  points[0].y = rect->y + rect->height - widths->bottom;
	  points[3].x = rect->x + rect->width;
	  points[3].y = rect->y + rect->height;

	  XFillPolygon(fe_display,
				   fe_drawable->xdrawable,
				   gc2,
				   points, 6, 
				   Nonconvex,
				   CoordModeOrigin);
	}
}

static void
fe_DisplayTableBorder(MWContext *context, LO_TableStruct *ts,
					  XRectangle *rect, FE_BorderWidths *widths)
{
  Pixel table_color, top_color, bottom_color;
  XRectangle inset_rect;
  
  if (ts->border_style == BORDER_GROOVE
	  || ts->border_style == BORDER_RIDGE
	  || ts->border_style == BORDER_INSET
	  || ts->border_style == BORDER_OUTSET)
	{
	  table_color = fe_GetPixel(context,
								ts->border_color.red,
								ts->border_color.green,
								ts->border_color.blue);
	  
	  XmGetColors(XtScreen(CONTEXT_WIDGET(context)),
				  fe_cmap(context),
				  table_color,
				  NULL,
				  &top_color, &bottom_color,
				  NULL);
	}
	  
  switch (ts->border_style)
	{
	case BORDER_NONE:
	  break;

	case BORDER_DOTTED:
	case BORDER_DASHED:
	case BORDER_SOLID:
	  fe_DisplaySolidBorder(context, ts, rect, widths);
	  break;

	case BORDER_DOUBLE:
	  fe_DisplayDoubleBorder(context, ts, rect, widths);
	  break;

	case BORDER_GROOVE:
	  widths->left /= 2;
	  widths->top /= 2;
	  widths->right /= 2;
	  widths->bottom /= 2;
	  fe_Display3DBorder(context, ts, rect, widths, bottom_color, top_color);
	  inset_rect.x = rect->x + widths->left;
	  inset_rect.y = rect->y + widths->top;
	  inset_rect.width = rect->width - widths->left - widths->right;
	  inset_rect.height = rect->height - widths->top - widths->bottom;
	  fe_Display3DBorder(context, ts, &inset_rect, widths, top_color, bottom_color);
	  break;

	case BORDER_RIDGE:
	  widths->left /= 2;
	  widths->top /= 2;
	  widths->right /= 2;
	  widths->bottom /= 2;
	  fe_Display3DBorder(context, ts, rect, widths, top_color, bottom_color);
	  inset_rect.x = rect->x + widths->left;
	  inset_rect.y = rect->y + widths->top;
	  inset_rect.width = rect->width - widths->left - widths->right;
	  inset_rect.height = rect->height - widths->top - widths->bottom;
	  fe_Display3DBorder(context, ts, &inset_rect, widths, bottom_color, top_color);
	  break;

	case BORDER_INSET:
	  fe_Display3DBorder(context, ts, rect, widths, bottom_color, top_color);
	  break;

	case BORDER_OUTSET:
	  fe_Display3DBorder(context, ts, rect, widths, top_color, bottom_color);
	  break;

	default:
	  XP_ASSERT(0);
	  break;
	}
}

#define ED_SELECTION_BORDER 3

void
XFE_DisplayTable (MWContext *context, int loc, LO_TableStruct *ts)
{
  XP_Bool hasBorder;
  int32 savedBorderStyle;
  LO_Color savedBorderColor;
  XRectangle table_rect;
  FE_BorderWidths widths;
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  long x = ts->x + ts->x_offset - CONTEXT_DATA (context)->document_x +
      fe_drawable->x_origin;
  long y = ts->y + ts->y_offset - CONTEXT_DATA (context)->document_y +
      fe_drawable->y_origin;

  if ((x > 0 && x > CONTEXT_DATA (context)->scrolled_width) ||
      (y > 0 && y > CONTEXT_DATA (context)->scrolled_height) ||
      (x + ts->width < 0) ||
      (y + ts->line_height< 0))
    return;

  hasBorder = (ts->border_top_width > 0 || ts->border_right_width > 0
                || ts->border_bottom_width > 0 || ts->border_left_width > 0);

  /* Set the border rect if we have a border or if the table is selected */
  table_rect.x = x;
  table_rect.y = y;
  table_rect.width = ts->width;
  table_rect.height = ts->height;

#ifdef EDITOR
  /* Figure out the boundaries for the selection highlight.
   * This needs to be done whether or not we're selected,
   * because we may have to clear the selection (but only in the editor).
   * Some of this code is stolen from the macfe.
   */
  if (EDT_IS_EDITOR(context))
  {
      int iSelectionBorderThickness;
      XGCValues gcv;
      unsigned long gc_flags;
      GC gc;
      fe_Drawable *fe_drawable = CONTEXT_DATA(context)->drawable;

      /* This is what the macfe does, but on X it's slow.
       * lo_DisplayLine ends up calling lo_DisplayTable for every
       * table element!  This has something to do with the entropy
       * calculated for the layer in liblayer/cl_util.h, which causes
       * the table to be laid out cell by cell even if the whole page
       * is being redisplayed; and the table border gets entirely
       * redrawn for each of these redraws.  Yuck!
       */

      /* set the border thickness to be the minimum of all border widths */
      if (!hasBorder && (0 == ts->inter_cell_space))
          iSelectionBorderThickness = 1;
      else
      {
          iSelectionBorderThickness = ts->border_left_width;
          if ( ts->border_right_width < iSelectionBorderThickness )
              iSelectionBorderThickness = ts->border_right_width;
          if ( ts->border_top_width < iSelectionBorderThickness )
              iSelectionBorderThickness = ts->border_top_width;
          if ( ts->border_bottom_width < iSelectionBorderThickness )
              iSelectionBorderThickness = ts->border_bottom_width;

          /* allow for a larger selection if the border is large */
          if ( iSelectionBorderThickness > 2 * ED_SELECTION_BORDER )
              iSelectionBorderThickness = 2 * ED_SELECTION_BORDER;

      /* else if the area is too small, use the spacing between cells */
          else if ( iSelectionBorderThickness < ED_SELECTION_BORDER )
          {
              iSelectionBorderThickness += ts->inter_cell_space;

              /* but don't use it all; stick to the minimal amount */
              if ( iSelectionBorderThickness > ED_SELECTION_BORDER )
                  iSelectionBorderThickness = ED_SELECTION_BORDER;
          }
      }

      if (ts->ele_attrmask & LO_ELE_SELECTED)
      {
          gc_flags = GCForeground | GCLineWidth | GCLineStyle;
          gcv.foreground = CONTEXT_DATA(context)->select_bg_pixel;
          gcv.line_style = LineOnOffDash;
      }
      else
      {
          gc_flags = GCForeground | GCLineWidth;
          gcv.foreground =
              CONTEXT_DATA(context)->drawing_area->core.background_pixel;
      }

      gcv.line_width = iSelectionBorderThickness;
      gc = fe_GetGCfromDW(fe_display, fe_drawable->xdrawable,
                          gc_flags, &gcv, fe_drawable->clip_region);
      XDrawRectangle(fe_display,
                     XtWindow(CONTEXT_DATA(context)->drawing_area), gc,
                     table_rect.x, table_rect.y,
                     table_rect.width-1, table_rect.height-1);
  } /* end showing selection highlight if editor */
#endif /* EDITOR */

  if (hasBorder)
  {
      widths.top = ts->border_top_width;
      widths.right = ts->border_right_width;
      widths.bottom = ts->border_bottom_width;
      widths.left = ts->border_left_width;
      fe_DisplayTableBorder(context, ts, &table_rect, &widths);
  }
}

typedef struct _SashInfo {
  Widget sash;
  Widget separator;
  LO_EdgeStruct *edge;
  MWContext *context;
  time_t last;
} SashInfo;

static void
fe_sash_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  SashInfo *sashinfo = (SashInfo *) closure;
  MWContext *context;
  LO_EdgeStruct *edge;
  SashCallData sash_data = (SashCallData) call_data;

  static EventMask activity = 0;
  static GC trackgc = 0;
  static int lastx = 0;
  static int lasty = 0;
  static int lastw = 0;
  static int lasth = 0;

  TRACEMSG (("fe_sash_cb\n"));

  if (!sashinfo) return;
  context = sashinfo->context;
  edge = sashinfo->edge;

  switch (sash_data->event->xany.type) {
  case ButtonPress: {
      XGCValues values;
      unsigned long valuemask;

      if (activity) return;
      activity = ButtonPressMask;

      if (!trackgc) {
	valuemask = GCForeground | GCSubwindowMode | GCFunction;
	values.foreground = CONTEXT_DATA (context)->default_bg_pixel;
	values.subwindow_mode = IncludeInferiors;
	values.function = GXinvert;
	trackgc = XCreateGC (XtDisplay (widget),
			     XtWindow (CONTEXT_WIDGET (context)),
			     valuemask, &values);
      }
    }
    break;
  case ButtonRelease:
    if (activity & PointerMotionMask) {
      static time_t last = 0;
      time_t now;

      /* Clean up the last line drawn */
      XDrawLine (XtDisplay (widget),
		   XtWindow (CONTEXT_DATA (context)->drawing_area),
		   trackgc, lastx, lasty, lastw, lasth);

      activity = 0; /* make sure we clear this for next time */

      if (trackgc) XFreeGC (XtDisplay (widget), trackgc);
      trackgc = 0;

      /* What's the scrolling policy for this context? */
      if (!edge->movable)
	return;

      /* Don't thrash */
      now = time ((time_t) 0);
      if (now > last)
        LO_MoveGridEdge (context, edge, lastx, lasty);
      last = now;

      lastx = lasty = lastw = lasth = 0;
    }
    break;
  case MotionNotify: {
      Display *dpy = XtDisplay (widget);
      Window kid;
      int da_x, da_y;

      if (!(activity & ButtonPressMask)) return;

      /* What's the scrolling policy for this context? */
      if (!edge->movable)
	return;

      /* Now that we know we're going to do something */
      activity |= PointerMotionMask;

      XTranslateCoordinates(dpy,
                            XtWindow (CONTEXT_DATA (context)->drawing_area),
                            DefaultRootWindow (dpy),
                            0, 0, &da_x, &da_y, &kid);

      if (lastw && lasth)
        XDrawLine (XtDisplay (widget),
		   XtWindow (CONTEXT_DATA (context)->drawing_area),
		   trackgc, lastx, lasty, lastw, lasth);

      if (edge->is_vertical)
        {
          int cx = sash_data->event->xmotion.x_root;

	  lastx = cx - da_x;
	  lasty = edge->y + edge->y_offset;

	  if (lastx < edge->left_top_bound + 20)
	    lastx = edge->left_top_bound + 20;

	  if (lastx > edge->right_bottom_bound - 20)
	    lastx = edge->right_bottom_bound - 20;

	  lastw = lastx;
	  lasth = lasty + edge->height - 1;
        }
      else
        {
          int cy = sash_data->event->xmotion.y_root;

	  lastx = edge->x + edge->x_offset;
	  lasty = cy - da_y;

	  if (lasty < edge->left_top_bound + 20)
	    lasty = edge->left_top_bound + 20;

	  if (lasty > edge->right_bottom_bound - 20)
	    lasty = edge->right_bottom_bound - 20;

	  lastw = lastx + edge->width - 1;
	  lasth = lasty;
        }

      XDrawLine (XtDisplay (widget),
		 XtWindow (CONTEXT_DATA (context)->drawing_area),
		 trackgc, lastx, lasty, lastw, lasth);
    }
    break;
  default:
    break;
  }
}

static void
fe_sash_destroy_cb (Widget w, XtPointer closure, XtPointer cb)
{
  SashInfo *sashinfo = (SashInfo *) closure;
  sashinfo->sash = NULL;
}

void
XFE_DisplayEdge (MWContext *context, int loc, LO_EdgeStruct *edge)
{
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  long x = edge->x + edge->x_offset - CONTEXT_DATA (context)->document_x +
      fe_drawable->x_origin;
  long y = edge->y + edge->y_offset - CONTEXT_DATA (context)->document_y +
      fe_drawable->y_origin;
  Widget drawing_area = CONTEXT_DATA (context)->drawing_area;
  Widget sash;
  static XtCallbackRec sashCallback[] = { {fe_sash_cb, 0}, {0, 0} };
  SashInfo *sashinfo;
  Arg av [50];
  int ac;

  if ((x > 0 && x > CONTEXT_DATA (context)->scrolled_width) ||
      (y > 0 && y > CONTEXT_DATA (context)->scrolled_height) ||
      (x + edge->width < 0) ||
      (y + edge->height< 0))
    return;

  /* Set up the args for the sash.
   * Careful! This is the only place we initialize av.
   */
  ac = 0;
  XtSetArg (av[ac], XmNx, x); ac++;
  XtSetArg (av[ac], XmNy, y); ac++;
  XtSetArg (av[ac], XmNwidth, edge->width); ac++;
  XtSetArg (av[ac], XmNheight, edge->height); ac++;
  if (edge->bg_color) {
    Pixel color = fe_GetPixel (context,
			       edge->bg_color->red,
			       edge->bg_color->green,
			       edge->bg_color->blue);
    XtSetArg (av[ac], XmNbackground, color); ac++;
  }

  if (edge->FE_Data) {
    time_t now = time((time_t) 0);

    sashinfo = (SashInfo *) edge->FE_Data;
    if (now <= sashinfo->last) return;
    sashinfo->last = now;

    if (sashinfo->sash) {
      XtSetValues (sashinfo->sash, av, ac);
      return;
    }
      
    edge->FE_Data = NULL;
    XP_FREE (sashinfo);
  }

  /* Otherwise, create and display a new one */

  sashinfo = (SashInfo *) XP_ALLOC (sizeof (SashInfo));
  if (!sashinfo) return;

  sashCallback[0].closure = (XtPointer) sashinfo;

  /* av and ac were initialized above */
  XtSetArg (av[ac], XmNcallback, (XtArgVal) sashCallback); ac++;
  sash = XtCreateWidget("sash", xmSashWidgetClass, drawing_area, av, ac);
  if (!edge->movable)
    XtVaSetValues (sash, XmNsensitive, False, 0);

  XtAddCallback (sash, XtNdestroyCallback, fe_sash_destroy_cb,
		 (XtPointer) sashinfo);
  XtManageChild (sash);

  sashinfo->sash = sash;
  sashinfo->edge = edge;
  sashinfo->context = context;
  sashinfo->last = time((time_t) 0);

  edge->FE_Data = (void *) sashinfo;
}


void
XFE_SetBackgroundColor (MWContext *context, uint8 red, uint8 green,
			uint8 blue)
{
  Pixel bg = fe_GetPixel (context, red, green, blue);
  CONTEXT_DATA (context)->bg_red   = red;
  CONTEXT_DATA (context)->bg_green = green;
  CONTEXT_DATA (context)->bg_blue  = blue;
  CONTEXT_DATA (context)->bg_pixel = bg;

/* Set the transparent pixel color.  The transparent pixel is passed into
   calls to IL_GetImage for image requests that do not use a mask. */
  fe_SetTransparentPixel(context, red, green, blue, bg);

  XSetWindowBackground (XtDisplay(CONTEXT_DATA (context)->drawing_area),
			XtWindow(CONTEXT_DATA (context)->drawing_area), bg);
  CONTEXT_DATA (context)->drawing_area->core.background_pixel = bg;
}


/* XXXM12N FE_EraseBackground needs to be completely restructured.  The way
   it is planned, layout would only call FE_EraseBackground to clear an
   area with a solid color.  Backdrop pixmaps would be handled by calling
   IL_DisplaySubImage, which can perform tiling if necessary. */
/* Erase the background at the given rect. x, y, width and height are */
/* in document coordinates. */
void
XFE_EraseBackground(MWContext *context, int iLocation, int32 x, int32 y,
                    uint32 width, uint32 height, LO_Color *bg)
{
    if (width > 0 && height > 0) {
        if (bg) {
            Pixel color = fe_GetPixel(context, bg->red, bg->green, bg->blue);
            fe_ClearAreaWithColor(context,
                                  -CONTEXT_DATA(context)->document_x + x,
                                  -CONTEXT_DATA(context)->document_y + y,
                                  width, height, color);
        }
        else {
            fe_ClearArea(context,
                         -CONTEXT_DATA(context)->document_x + x,
                         -CONTEXT_DATA(context)->document_y + y,
                         width, height);
        }
    }
}

void
FE_GetEdgeMinSize(MWContext *context, int32 *size_p)
{
	*size_p = 5;
}


void
FE_GetFullWindowSize(MWContext *context, int32 *width, int32 *height)
{
  Dimension     w = 0;
  Dimension     h = 0;
  Dimension  hpad = CONTEXT_DATA (context)->sb_w;
  Dimension  vpad = CONTEXT_DATA (context)->sb_h;
  
  if (context->is_grid_cell) {
    Position    sx, sy;
    
    XtVaGetValues (CONTEXT_DATA (context)->scrolled, 
		   XmNwidth, &w, XmNheight, &h, 0);
    
    *width = w;
    *height = h;
    
    XtVaGetValues (CONTEXT_DATA (context)->vscroll, XmNx, &sx, 0);
    XtVaGetValues (CONTEXT_DATA (context)->hscroll, XmNy, &sy, 0);
    
    if ((long) sx < (long) w)
      *width = (int32)w;
    else
      *width = (int32)(w + vpad);
    
    if ((long) sy < (long) h) 
      *height = (int32)h;
    else
      *height = (int32)(h + hpad);
    
    XtVaSetValues (CONTEXT_DATA (context)->scrolled, 
		   XmNwidth, *width, XmNheight, *height, 0);
	}
  else {
    Widget dap = XtParent( CONTEXT_DATA (context)->drawing_area );
    
    XtVaGetValues (dap, XmNwidth, &w, XmNheight, &h, 0);
    *width = (int32) w;
    *height = (int32) h;

    XtVaSetValues (CONTEXT_DATA (context)->drawing_area, 
		   XmNwidth, *width, XmNheight, *height, 0);
  }
}

void
fe_GetMargin(MWContext *context, int32 *marginw_ptr, int32 *marginh_ptr)
{
  int32 w, h;
  if (context->is_grid_cell) {
    w = FEUNITS_X(7, context);
    h = FEUNITS_X(4, context);
  } else if (context->type == MWContextMail ||
	     context->type == MWContextNews) {
    w = FEUNITS_X(8, context);
    h = 0; /* No top margin for mail and news windows */
  } else {
    w = FEUNITS_X(8, context);
    h = FEUNITS_X(8, context);
  }

  if (marginw_ptr) *marginw_ptr = w;
  if (marginh_ptr) *marginh_ptr = h;
}


void
XFE_LayoutNewDocument (MWContext *context, URL_Struct *url,
		      int32 *iWidth, int32 *iHeight,
		       int32 *mWidth, int32 *mHeight)
{
  Dimension w = 0, h = 0;
  XColor color;
  int32 fe_mWidth, fe_mHeight;
  Boolean grid_cell_p = context->is_grid_cell;

  /* Fix for bug #29631 */
  if (context == last_documented_xref_context)
        {
                last_documented_xref_context = 0;
                last_documented_xref = 0;
                last_documented_anchor_data = 0;
        }

  fe_FreeTransientColors(context);
  
  CONTEXT_DATA (context)->delayed_images_p = False;

  color.pixel = CONTEXT_DATA (context)->default_bg_pixel;
  fe_QueryColor (context, &color);

  /* The pixmap itself is freed when its IL_Image is destroyed. */
  CONTEXT_DATA (context)->backdrop_pixmap = 0;

  /* Set background after making the backdrop_pixmap 0 as SetBackground
   * will ignore a background setting request if backdrop_pixmap is
   * available.
   */
  XFE_SetBackgroundColor (context,
			  color.red >> 8,
			  color.green >> 8,
			  color.blue >> 8);

  if (grid_cell_p) {
    if (!CONTEXT_DATA (context)->drawing_area) return;

    XtVaGetValues (CONTEXT_DATA (context)->drawing_area,
               XmNwidth, &w, XmNheight, &h, 0);
  } else {
    if (!CONTEXT_DATA (context)->scrolled) return;

    XtVaGetValues (CONTEXT_DATA (context)->scrolled,
		 XmNwidth, &w, XmNheight, &h, 0);
  }
  if (!w || !h) abort ();

  /* Clear the background since, in the case of grid cells without borders, 
     the grid boundaries don't get cleared */
  XClearArea (XtDisplay (CONTEXT_WIDGET (context)),
	      XtWindow (CONTEXT_DATA (context)->drawing_area), 
	      0, 0,
	      CONTEXT_DATA (context)->scrolled_width,
	      CONTEXT_DATA (context)->scrolled_height, 
	      False);

  /* Only make room for scrollbar if is non-grid cell,
   * or is grid cell but scrolling is turned on.  -slamm 
   */
  if (!context->is_grid_cell || CONTEXT_DATA(context)->grid_scrolling) {
  /* Subtract out the size of the scrollbars - they might not end up being
     present, but tell layout that all it has to work with is the area that
     would be available if there *were* scrollbars. */
	w -= CONTEXT_DATA (context)->sb_w;
	h -= CONTEXT_DATA (context)->sb_h;
  }

  w -= 2;	/* No, this isn't a hack.  What makes you think that? */

  *iWidth = w;
  *iHeight = h;

  fe_GetMargin(context, &fe_mWidth, &fe_mHeight);

  /*
   * If layout already knows margin width, let it pass unless it
   * is just too big.
   */
  if (*mWidth != 0)
    {
      if (*mWidth > ((w / 2) - 1))
        *mWidth = ((w / 2) - 1);
    }
  else
    {
      *mWidth = fe_mWidth;
    }

  /*
   * If layout already knows margin height, let it pass unless it
   * is just too big.
   */
  if (*mHeight != 0)
    {
      if (*mHeight > ((h / 2) - 1))
        *mHeight = ((h / 2) - 1);
    }
  else
    {
      *mHeight = fe_mHeight;
    }

  /* Get rid of the old title; don't install "Unknown" until we've gotten
     to the end of the document without XFE_SetDocTitle() having been called.
   */
  if (context->title)
    free (context->title);
  context->title = 0;

  if (!grid_cell_p && !CONTEXT_DATA (context)->is_resizing)
    fe_SetURLString (context, url);

  /* This is set in fe_resize_cb */
  CONTEXT_DATA (context)->is_resizing = FALSE;

  if (url->address && !(sploosh && !strcmp (url->address, sploosh)))
    {
      if (sploosh)
	{
	  free (sploosh);
	  sploosh = 0;
	}
      
      SHIST_AddDocument (context, SHIST_CreateHistoryEntry (url, ""));
    }

  /* Make sure we clear the string from the previous document */
  if (context->defaultStatus) {
    XP_FREE (context->defaultStatus);
    context->defaultStatus = 0;
  }

  /* #### temporary, until we support printing GIFs that don't have HTML
     wrapped around them. */
#if 0
  if (CONTEXT_DATA (context)->print_menuitem)
    XtVaSetValues (CONTEXT_DATA (context)->print_menuitem,
		   XmNsensitive, !url->is_binary, 0);
  if (CONTEXT_DATA (context)->print_button)
    XtVaSetValues (CONTEXT_DATA (context)->print_button,
		   XmNsensitive, !url->is_binary, 0);
#endif

#ifdef LEDGES
  XFE_ClearView (context, FE_TLEDGE);
  XFE_ClearView (context, FE_BLEDGE);
#endif
  fe_SetDocPosition (context, 0, 0);

  fe_FindReset (context);
  if (!grid_cell_p)
    fe_UpdateDocInfoDialog (context);
}


void
fe_FormatDocTitle (const char *title, const char *url, char *output, int size)
{
  if (size < 0) return;

  if (title && !XP_STRCASECMP(title, XP_GetString(XFE_UNTITLED)))
    title = 0;  /* Losers!!! */

  if (title)
	{
	  XP_SAFE_SPRINTF (output, size, "%.200s", title);
	}
  else if (!url || 
		   !*url || 
		   strcmp(url,XP_GetString(XFE_LAY_LOCAL_FILE_URL_UNTITLED)) == 0)
    {
      XP_STRNCPY_SAFE(output, XP_GetString(XFE_UNTITLED), size);
    }
  else
    {
      const char *s = (const char *) strrchr (url, '/');
      if (s)
	s++;
      else
	s = url;
      PR_snprintf (output, size, "%.200s", s);
    }
}


void
XFE_SetDocTitle (MWContext *context, char *title)
{
  char buf [1024];
  char buf2 [1024];
  Widget shell = CONTEXT_WIDGET (context);
  XTextProperty text_prop;
  char *fmt;
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

  if (context->type == MWContextSaveToDisk || !shell)
    return;

  /* For some context types like MWContextDialog, the shell is the parent
   * of the CONTEXT_WIDGET. In general, traverse back and get the shell.
   */
  while(!XtIsWMShell(shell) && (XtParent(shell)!=0))
    shell = XtParent(shell);

  /* We don't need to set the title for grid cells;
   * the backend sets the toplevel's title for us.
   */
  if (context->is_grid_cell)
    return;

  if (context->type == MWContextMessageComposition)
    {
      /* I18N watch */
      if (context->title) free (context->title);
      context->title = (title ? strdup (title) : 0);
      if (!title)
	title = XP_GetString( XFE_NO_SUBJECT );
      PR_snprintf (buf, sizeof(buf), "%.200s", title);
    }
  else
    {
      History_entry *he = SHIST_GetCurrent (&context->hist);
      char *url = (he && he->address ? he->address : 0);

      SHIST_SetTitleOfCurrentDoc (&context->hist, title);

      fe_UpdateDocInfoDialog (context);

      if (context->title) free (context->title);
      context->title = (title ? strdup (title) : 0);

#ifdef EDITOR
		/*
		 *    For the editor we would rather at least have the filename,
		 *    not so for Browser where it may be intentional.
		 */
	    if (context->type == MWContextEditor) {
			if (title == NULL || (title != NULL && title[0] == '\0'))
				title = url;
		}
#endif /*EDITOR*/

		fe_FormatDocTitle (title, url, buf, sizeof(buf));
    }

  switch (context->type) {
    case MWContextAddressBook:
    case MWContextBookmarks:
    case MWContextHistory:
	  fmt = "%s";
	  break;
    case MWContextMail:
        /* Don't reset the title on the folders window */
        if (shell && !strcmp(XtName(shell), "MailFolder"))
	  return;
	fmt = XP_GetString(XFE_MAIL_TITLE_FMT);
	break;
    case MWContextNews:
	fmt = XP_GetString(XFE_NEWS_TITLE_FMT);
	break;
    case MWContextMessageComposition:
        fmt = XP_GetString(XFE_COMPOSE);
	break;
    case MWContextEditor:
	fmt = XP_GetString(XFE_EDITOR_TITLE_FMT);
	break;
    case MWContextBrowser: 	/* FALL THROUGH */
    default:
	fmt = XP_GetString(XFE_TITLE_FMT);
  }
  XP_SAFE_SPRINTF(buf2, sizeof (buf2), fmt, buf);

  /* For some context types like MWContextDialog, the shell is the parent
   * of the CONTEXT_WIDGET. In general, traverse back and get the shell.
   */
  while(!XtIsWMShell(shell) && (XtParent(shell)!=0))
    shell = XtParent(shell);

  if (INTL_GetCSIWinCSID(c) == CS_LATIN1)
    {
      text_prop.value = (unsigned char *) buf2;
      text_prop.encoding = XA_STRING;
      text_prop.format = 8;
      text_prop.nitems = strlen(buf2);
    }
  else
    {
      char *loc;
      int status;

      loc = (char *) fe_ConvertToLocaleEncoding(INTL_GetCSIWinCSID(c),
                                                (unsigned char *) buf2);
      status = XmbTextListToTextProperty(XtDisplay(shell), &loc, 1,
                                         XStdICCTextStyle, &text_prop);
      if (loc != buf2)
        {
          XP_FREE(loc);
        }
      if (status != Success)
        {
          text_prop.value = (unsigned char *) buf2;
          text_prop.encoding = XA_STRING;
          text_prop.format = 8;
          text_prop.nitems = strlen(buf2);
        }
    }

   /* We should not call XSetWMName here because the shell might not
      be realized yet. You are going to get NULL window id when shell
      is not realized. Then, we will get X Error (Bad Window) with
      ChangeProperty error id.

      To fix this, we should use XtSetValues on both
      titleEncoding and title. And, when shell is realized, it will
      turn around, and change the window property (ie Calling
      XSetWMName) at the right time.

     I really don't know why they say that SetValues
     does not work for high-bit characters earlier. 

     Here, I set the titleEncoding too. I think that the hight-bit
     character problem should not exist now once we set the encoding.

     If I was proven to be wrong, please let me know - dh */
   XtVaSetValues (shell, XtNtitleEncoding, text_prop.encoding, 0);
   XtVaSetValues (shell, XtNtitle, text_prop.value, 0);

    /* Only set the icon title on browser windows - not mail, news or
       download. */
  if (context->type == MWContextBrowser)
  {
   /* Same comments at the block for XtNtitle */
   XtVaSetValues (shell, XtNiconNameEncoding, text_prop.encoding, 0);
   XtVaSetValues (shell, XtNiconName, text_prop.value, 0);
  }
}


void
fe_DestroyLayoutData (MWContext *context)
{
  LO_DiscardDocument (context);
  free (CONTEXT_DATA (context)->color_data);
}

void
XFE_FinishedLayout (MWContext *context)
{
  /* Since our processing of XFE_SetDocDimension() may have been lazy,
     do it for real this time. */
  CONTEXT_DATA (context)->doc_size_last_update_time = 0;
  /* Update scrollbars using final dimensions. */
  fe_SetDocPosition(context, CONTEXT_DATA (context)->document_x,
                    CONTEXT_DATA (context)->document_y);
}


void
fe_ClearArea (MWContext *context, int x, int y, unsigned int w, unsigned int h)
{
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  Drawable drawable = fe_drawable->xdrawable;
  Display *dpy = fe_display;
  GC gc;
  XGCValues gcv;

  memset (&gcv, ~0, sizeof (gcv));
  gcv.foreground = CONTEXT_DATA (context)->bg_pixel;
  gc = fe_GetGCfromDW (dpy, drawable, (GCForeground), &gcv,
                       fe_drawable->clip_region);

  XFillRectangle (dpy, drawable, gc, x, y, w, h);
}

void
fe_ClearAreaWithColor (MWContext *context, int x, int y, unsigned int w,
                       unsigned int h, Pixel color)
{
  fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;
  Drawable drawable = fe_drawable->xdrawable;
  Display *dpy = fe_display;
  GC gc;
  XGCValues gcv;

  memset (&gcv, ~0, sizeof (gcv));
  gcv.foreground = color;
  gc = fe_GetGCfromDW (dpy, drawable, (GCForeground), &gcv,
                       fe_drawable->clip_region);
  XFillRectangle (dpy, drawable, gc, x, y, w, h);
}

void
XFE_ClearView (MWContext *context, int which)
{
  if (!XtIsManaged (CONTEXT_WIDGET (context)))
    return;

  /* Clear out the data for the mouse-highlighted item.
     #### What if one ledge is being cleared but not all, and the
     highlighted item is in the other?
   */
  last_armed_xref = 0;
  last_armed_xref_highlighted_p = False;
  last_documented_xref_context = 0;
  last_documented_xref = 0;
  last_documented_anchor_data = 0;
  fe_SetCursor (context, False);

  switch (which)
    {
#ifdef LEDGES
    case FE_TLEDGE:
      XClearWindow (XtDisplay (CONTEXT_WIDGET (context)),
		    XtWindow (CONTEXT_DATA (context)->top_ledge));
      break;
    case FE_BLEDGE:
      XClearWindow (XtDisplay (CONTEXT_WIDGET (context)),
		    XtWindow (CONTEXT_DATA (context)->bottom_ledge));
      break;
#endif
    case FE_VIEW:
      fe_ClearArea (context, 0, 0,
		    /* Some random big number (but if it's too big,
		       like most-possible-short, it will make some
		       MIT R4 servers mallog excessively, sigh.) */
		    CONTEXT_WIDGET (context)->core.width * 2,
		    CONTEXT_WIDGET (context)->core.height * 2);
      break;
    default:
      abort ();
    }
}

void
XFE_BeginPreSection (MWContext *context)
{
}

void
XFE_EndPreSection (MWContext *context)
{
}

void
XFE_FreeEdgeElement (MWContext *context, LO_EdgeStruct *edge)
{
  SashInfo *sashinfo = edge->FE_Data;

  if (!sashinfo) return;

  if (sashinfo->sash) {
    XtRemoveCallback (sashinfo->sash, XtNdestroyCallback,
		      fe_sash_destroy_cb, (XtPointer) sashinfo);
    XtDestroyWidget (sashinfo->sash);
    sashinfo->sash = NULL;
  }

  edge->FE_Data = NULL;
  XP_FREE (sashinfo);
}

#ifdef SHACK
extern Widget fe_showRDFView (Widget w, int width, int height);

void
XFE_DisplayBuiltin (MWContext *context, int iLocation,
                                   LO_BuiltinStruct *builtin_struct)
{
	fe_Drawable *fe_drawable = CONTEXT_DATA(context)->drawable;
	Drawable drawable = fe_drawable->xdrawable;
	Display *dpy = XtDisplay (CONTEXT_WIDGET (context));
	Widget w = XtWindowToWidget (dpy, drawable);
	Widget view = NULL;
	int xs, ys;

#ifdef DEBUG_spence
    printf ("XFE_DisplayBuiltin\n");
#endif

	if (!builtin_struct) return;
	
	if (builtin_struct->FE_Data) return; /* been here XXX */

	view = fe_showRDFView (CONTEXT_DATA (context)->drawing_area,
						   builtin_struct->width, builtin_struct->height);
    builtin_struct->FE_Data = (void *) view;

	/* update the window's position */
	xs = builtin_struct->x + builtin_struct->x_offset -
		CONTEXT_DATA (context)->document_x;
	ys = builtin_struct->y + builtin_struct->y_offset -
		CONTEXT_DATA (context)->document_y;

	XtVaSetValues (view, XmNx, (Position) xs,
				   XmNy, (Position) ys, 0);
}

void
XFE_FreeBuiltinElement (MWContext *context, LO_BuiltinStruct *builtin_struct)
{
	Widget view;

#ifdef DEBUG_spence
	printf ("XFE_FreeBuiltinElement\n");
#endif

	if (!builtin_struct || !builtin_struct->FE_Data) return;

	view = (Widget) builtin_struct->FE_Data;
	XtDestroyWidget (view);
	builtin_struct->FE_Data = NULL;
}
#endif /* SHACK */

/*
 * This is called by the plugin code to create a new embedded window
 * for the plugin in the specified context.
 */
void
XFE_CreateEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
    Widget parent = CONTEXT_DATA (context)->drawing_area;
    LO_EmbedStruct* lo_struct;
    Widget embed;
    Window win;
    int xp, yp;
    int32 xs, ys;

    /* now we have a live embed and its the first time so we need to
       prepare a window for it */
    if (XP_FAIL_ASSERT(app->np_data != NULL))
        return;

    lo_struct = ((np_data *) app->np_data)->lo_struct;
    if (XP_FAIL_ASSERT(lo_struct != NULL))
        return;

    xp = lo_struct->x;
    yp = lo_struct->y;
    xs = lo_struct->width;
    ys = lo_struct->height;
	
    if (CONTEXT_DATA(context)->is_fullpage_plugin) {
        /* This is a full page plugin */
        int32 mWidth, mHeight;

        FE_GetFullWindowSize(context, &xs, &ys);
        fe_GetMargin(context, &mWidth, &mHeight);
        xs -= mWidth;
        ys -= mHeight;
	    
        xp = yp = 0;
    }

    {
        Pixel bg;
        Arg av[20];
        int ac = 0;

        XtVaGetValues(parent, XmNbackground, &bg, 0);

        /* XtSetArg(av[ac], XmNborderWidth, 1);         ac++ */
        XtSetArg(av[ac], XmNx, (Position)xp);           ac++;
        XtSetArg(av[ac], XmNy, (Position)yp);           ac++;
        XtSetArg(av[ac], XmNwidth, (Dimension)xs);      ac++;
        XtSetArg(av[ac], XmNheight, (Dimension)ys);     ac++;
#ifdef X_PLUGINS
        XtSetArg(av[ac], XmNmarginWidth, 0);            ac++;
        XtSetArg(av[ac], XmNmarginHeight, 0);           ac++;
#endif
        XtSetArg(av[ac], XmNbackground, bg);            ac++;
        embed = XmCreateDrawingArea(parent, "netscapeEmbed", av, ac);
    }

    XtRealizeWidget (embed); /* create window, but don't map */
    win = XtWindow(embed);

    if (fe_globalData.fe_guffaw_scroll == 1) {
        XSetWindowAttributes attr;
        unsigned long valuemask;

        valuemask = CWBitGravity | CWWinGravity;
        attr.win_gravity = StaticGravity;
        attr.bit_gravity = StaticGravity;
        XChangeWindowAttributes(XtDisplay (embed), XtWindow (embed),
                                valuemask, &attr);
    }
    /* XtManageChild (embed); */

    /* make a plugin wininfo */
    {
        NPWindow *nWin = (NPWindow *)malloc(sizeof(NPWindow));
        if(nWin) {
#ifdef X_PLUGINS
            NPSetWindowCallbackStruct *fe_data;
#endif /* X_PLUGINS */

            nWin->window = (void *)win;
            nWin->x = xp;
            nWin->y = yp;
            nWin->width = xs;
            nWin->height = ys;
            nWin->type = NPWindowTypeWindow;
		
#ifdef X_PLUGINS
            fe_data = (NPSetWindowCallbackStruct *)
                malloc(sizeof(NPSetWindowCallbackStruct));

            if (fe_data) {
                Visual *v = 0;
                Colormap cmap = 0;
                Cardinal depth = 0;

                XtVaGetValues(CONTEXT_WIDGET(context), XtNvisual, &v,
                              XtNcolormap, &cmap, XtNdepth, &depth, 0);

                fe_data->type = NP_SETWINDOW;
                fe_data->display = (void *) XtDisplay(embed);
                fe_data->visual = v;
                fe_data->colormap = cmap;
                fe_data->depth = depth;
                nWin->ws_info = (void *) fe_data;
            }
#endif /* X_PLUGINS */
        }
        app->wdata = nWin;
        app->fe_data = (void *)embed;
    }
}

/*
 * This is called by the plugin code to save an embed window in a
 * "safe place" (i.e., where it won't get destroyed when the context
 * goes away. It is called in response to NPL_DeleteEmbed().
 */
void
XFE_SaveEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
    Widget embedWidget;
    MWContext *safeContext;
    Widget safeWidget;
    Widget parentWidget;

    if (XP_FAIL_ASSERT(app->fe_data != NULL))
        return;

    embedWidget = (Widget) app->fe_data;
    if (XP_FAIL_ASSERT(XtIsWidget(embedWidget)))
        return;

    XUnmapWindow(XtDisplay(embedWidget), XtWindow(embedWidget));

    safeContext = XP_GetNonGridContext(context);
    if (XP_FAIL_ASSERT(safeContext != NULL))
        return;

    safeWidget = CONTEXT_DATA(safeContext)->drawing_area;
    if (XP_FAIL_ASSERT(safeWidget != NULL &&
                       XtIsWidget(safeWidget) &&
                       XtIsComposite(safeWidget)))
        return;
            
    parentWidget = XtParent(embedWidget);

    if (safeWidget != parentWidget) {
        /* If the safe widget and the parent widget are not one in the
           same, then we have a situation where the embedded object is
           on a grid context. Reparent the embedded object's widget to
           the safe context. First, we'll remove it from the current
           context... */
        if (XP_OK_ASSERT(parentWidget != NULL && XtIsComposite(parentWidget))) {
            CompositeWidgetClass c;
            c = (CompositeWidgetClass) XtClass(parentWidget);
            (c->composite_class.delete_child)(embedWidget);
        }

        /* Then, reparent it to the safe widget; both at the X and Xt
           levels. */
        XtParent(embedWidget) = safeWidget;

        {
            CompositeWidgetClass c;
            c = (CompositeWidgetClass) XtClass(safeWidget);
            (c->composite_class.insert_child)(embedWidget);
        }

        XReparentWindow(XtDisplay(embedWidget), XtWindow(embedWidget),
                        XtWindow(safeWidget), 0, 0);
    }
}

/*
 * This is called by the plugin code to restore a previously "saved"
 * embedded window to the new context.
 */
void
XFE_RestoreEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
    Widget embedWidget;
    Widget safeWidget;

    if (XP_FAIL_ASSERT(app != NULL && app->fe_data != NULL))
        return;

    embedWidget = (Widget) app->fe_data;
    if (XP_FAIL_ASSERT(XtIsWidget(embedWidget)))
        return;

    if (XtParent(embedWidget) != CONTEXT_DATA(context)->drawing_area) {
        /* Reparent the embedded object's widget from the safe context
           to the current context. */
        Widget safeWidget = XtParent(embedWidget);
        Widget parentWidget = CONTEXT_DATA(context)->drawing_area;

        /* Start by reparenting it at the X level */
        {
            int xp = 0;
            int yp = 0;

            if (app->np_data) {
                LO_EmbedStruct* embed_struct
                    = ((np_data*) app->np_data)->lo_struct;

                if (embed_struct) {
                    xp = embed_struct->x + embed_struct->x_offset -
                        CONTEXT_DATA(context)->document_x;
                    yp = embed_struct->y + embed_struct->y_offset -
                        CONTEXT_DATA(context)->document_y;
                }
            }

            XReparentWindow(XtDisplay(embedWidget), XtWindow(embedWidget),
                            XtWindow(parentWidget), xp, yp);
        }

        /* Now remove it from the safe context. Check to make sure we
           can really do composite ops on this thing... */
        if (XP_OK_ASSERT(safeWidget != NULL && XtIsComposite(safeWidget))) {
            CompositeWidgetClass c;
            c = (CompositeWidgetClass) XtClass(safeWidget);
            (c->composite_class.delete_child)(embedWidget);
        }

        /* Now reparent it to the current context */
        XtParent(embedWidget) = parentWidget;

        /* Again, a sanity check... */
        if (XP_OK_ASSERT(parentWidget != NULL && XtIsComposite(parentWidget))) {
            CompositeWidgetClass c;
            c = (CompositeWidgetClass) XtClass(parentWidget);
            (c->composite_class.insert_child)(embedWidget);
        }
    }

    XtMapWidget(embedWidget);
}

/*
 * This is called by the plugin code to destroy the embedded window.
 * It is either called _immediately_ in response to NPL_DeleteEmbed(),
 * or via layout when a saved embed window is released from the
 * history.
 */
void
XFE_DestroyEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
    if (app) {
        NPWindow* nWin = app->wdata;
        Widget embed_widget = (Widget)app->fe_data;

        if (embed_widget)
            XtDestroyWidget(embed_widget);

        if (nWin) {
            if (nWin->ws_info)
                free(nWin->ws_info);

            free(nWin);
        }
    }

    /* Reset fullpage plugin */
    CONTEXT_DATA(context)->is_fullpage_plugin = 0;
}

void
XFE_FreeEmbedElement (MWContext *context, LO_EmbedStruct *embed_struct)
{
    NPL_EmbedDelete(context, embed_struct);
}

void
XFE_DisplayEmbed (MWContext *context,
		  int iLocation, LO_EmbedStruct *embed_struct)
{
    NPEmbeddedApp *eApp;
    int32 xs, ys;

    if (!embed_struct) return;
    eApp = (NPEmbeddedApp *)embed_struct->FE_Data;
    if (!eApp) return;

    /* Shouldn't be here if HIDDEN */
    if (embed_struct->ele_attrmask & LO_ELE_HIDDEN) 
      return;
    

    /* Layout might have changed the location of the embed since we
     * created the embed in XFE_GetEmbedSize()
     */
    xs = embed_struct->x + embed_struct->x_offset -
      CONTEXT_DATA (context)->document_x;
    ys = embed_struct->y + embed_struct->y_offset -
      CONTEXT_DATA (context)->document_y;

    /* If this is a full page plugin, then plugin needs to be notified of
       the new size as relayout never happens for this when we resize.
       Our resize handler marks this context as a fullpage plugin. */
	
    if (CONTEXT_DATA(context)->is_fullpage_plugin) {
	NPWindow *nWin = (NPWindow *)eApp->wdata;

	FE_GetFullWindowSize(context, &xs, &ys);
	
#if 0
	int32 mWidth, mHeight;
	/* Normally the right thing to do is to subtract the margin width.
	 * But we wont do this and give the plugin the full html area.
	 * Remember, layout still thinks the we offset the fullpage plugin
	 * by the margin offset.
	 */
	fe_GetMargin(context, &mWidth, &mHeight);
	xs -= mWidth;
	ys -= mHeight;
#else /* 0 */
	/* In following suit with our hack of no margins for fullpage plugins
	 * we force the plugin to (0,0) position.
	 */
	XtVaSetValues((Widget)eApp->fe_data, XmNx, (Position)0,
		      XmNy, (Position)0, 0);
#endif /* 0 */

	if (nWin->width != xs || nWin->height != ys) {
	    nWin->width = xs;
	    nWin->height = ys;
	    (void)NPL_EmbedSize(eApp);
	}
    }
    else {
	/* The layer containing the plugin may be hidden or clipped by
	   an enclosing layer, in which case we should unmap the
	   plugin's window */
	XtSetMappedWhenManaged((Widget)eApp->fe_data,
			       !(embed_struct->ele_attrmask &LO_ELE_INVISIBLE));

	/* The location of the plugin may have changed since it was
	 * created, either at the behest of Layout or due to movement
	 * of an enclosing layer.  So, change the position of the
	 * plugin. Do this only if we are not a fullpage plugin as
	 * fullpage plugins are always at (0,0).  */

	/* but first, update the size and call a set window... */
	(void)NPL_EmbedSize(eApp);

	XtVaSetValues((Widget)eApp->fe_data, XmNx, (Position)xs,
		      XmNy, (Position)ys, 0);
    }

    /* Manage the embed window. XFE_GetEmbedSize() only creates the it. */
    if (!XtIsManaged((Widget)eApp->fe_data))
	XtManageChild((Widget)eApp->fe_data);
}

void
XFE_GetEmbedSize (MWContext *context, LO_EmbedStruct *embed_struct,
		  NET_ReloadMethod force_reload)
{
    NPEmbeddedApp *eApp = (NPEmbeddedApp *)embed_struct->FE_Data;
    int32 doc_id;
    lo_TopState *top_state;

    /* here we need only decrement the number of embeds expected to load */
    doc_id = XP_DOCID(context);
    top_state = lo_FetchTopState(doc_id);

    if(!eApp)
    {
	/* Determine if this is a fullpage plugin. Do this _now_ so
           that it'll be available when NPL_EmbedCreate() calls back
           to XFE_CreateEmbedWindow() */
	if((embed_struct->width == 1) &&
           (embed_struct->height == 1) &&
           (embed_struct->attribute_cnt > 0) &&
           (!strcmp(embed_struct->attribute_list[0], "src")) &&
           (!strcmp(embed_struct->value_list[0], "internal-external-plugin"))) {
            CONTEXT_DATA(context)->is_fullpage_plugin = 1;
        }

	/* attempt to make a plugin */
#ifdef UNIX_EMBED
	if(!(eApp = NPL_EmbedCreate(context, embed_struct)))
#else
	if(1)  /* disable unix plugin's */
#endif
	{
	    /* hmm, that failed which is unusual */
	    embed_struct->width = embed_struct->height=1;
	    return;
	}
	eApp->type = NP_Plugin;

	if (embed_struct->ele_attrmask & LO_ELE_HIDDEN) {
	    /* Hidden plugin. Dont create window for it. */
	    eApp->fe_data = 0;
	    eApp->wdata = 0;
	    embed_struct->width = embed_struct->height=0;
	    /* --- begin fix for bug# 35087 --- */
	    embed_struct->FE_Data = (void *)eApp;

            if (NPL_EmbedStart(context, embed_struct, eApp) != NPERR_NO_ERROR) {
	        /* Spoil sport! */
                /* XXX This used to be a call to fe_destroyEmbed,
                   which has now been massaged into a front-end
                   callback. However, it doesn't (and didn't!) _do_
                   anything unless eApp->fe_data or eApp->wdata
                   contain something, and we've just hard-coded them
                   to zero!

                XFE_DestroyEmbedWindow(context, eApp) */
                embed_struct->FE_Data = NULL;
	        return;
	    }
	    /* --- end fix for bug# 35087 --- */

            /* XXX NPL_EmbedSize does nothing if eApp->wdata == NULL;
               makes sense because this thing is _hidden_.

            (void)NPL_EmbedSize(eApp); */
	    return;
	}

	if (NPL_EmbedStart(context, embed_struct, eApp) != NPERR_NO_ERROR) {
	    /* Spoil sport! */
	    XFE_DestroyEmbedWindow(context, eApp);
            embed_struct->FE_Data = NULL;
	    return;
	}
    }

    /* always inform plugins of size changes */
    (void)NPL_EmbedSize(eApp);
}

/*************************************************************************
 * Java Stuff
 ************************************************************************/

#ifdef JAVA
static void* PR_CALLBACK 
FE_GetAwtWindow(MWContext *context, LJAppletData* ad)
{
    return ad->fe_data;
}
#endif

#ifdef JAVA
static void PR_CALLBACK
FE_SaveJavaWindow(MWContext *context, LJAppletData* ad, void* window)
{
    Widget * kids;
    Cardinal nkids = 0;
    Widget contextWidget = (Widget)window;
    XtUnmapWidget(contextWidget);  
	 
    /*
    ** We are about to destroy contextWidget, but we want to hang on
    ** to its child, so that we can remap it when the applet's page
    ** is browsed again. We reparent the child to mozilla's main 
    ** drawing area since we know that it will be there as long as
    ** mozilla is there. We do the reparenting at both X and Xt level.
    */
    
    XtVaGetValues(contextWidget, XmNchildren, &kids,
		  XmNnumChildren, &nkids, NULL);
    /* XP_ASSERT(nkids == 1);  */

    if (nkids >= 1) {
	Widget kid;

	kid = kids[0];
	(((CompositeWidgetClass) contextWidget->core.widget_class)->
	 composite_class.delete_child)(kid);
	kid->core.parent = CONTEXT_DATA(ad->context)->drawing_area; 

	(((CompositeWidgetClass) XtParent(kid)->core.widget_class)->
	 composite_class.insert_child)(kid);

	XUnmapWindow(XtDisplay(kid), XtWindow(kid));
		XReparentWindow(XtDisplay(kid), XtWindow(kid),
			XtWindow(CONTEXT_DATA(ad->context)->drawing_area), 0, 0); 
    }

    /*
    ** Destroy the window and set the pointer to null because it will
    ** need to get recreated.
    */
    XtDestroyWidget(contextWidget); 
    ad->window = NULL; 
}
#endif

void
XFE_HideJavaAppElement(MWContext *context, struct LJAppletData* session_data)
{
#ifdef JAVA
    LJ_HideJavaAppElement(context, session_data, FE_SaveJavaWindow);
#endif /* JAVA */
}

static void PR_CALLBACK
FE_FreeJavaWindow(MWContext *context, struct LJAppletData *appletData,
		  void* window)
{
    Widget contextWidget = (Widget)window;
    XtDestroyWidget(contextWidget);
}

void
XFE_FreeJavaAppElement(MWContext *context, struct LJAppletData *appletData)
{
#ifdef JAVA
    LJ_FreeJavaAppElement(context, appletData, 
			  FE_SaveJavaWindow,
			  FE_FreeJavaWindow);
#endif /* JAVA */
}


static void PR_CALLBACK 
FE_DisplayNoJavaIcon(MWContext *pContext, LO_JavaAppStruct *java_struct)
{
    /* write me */
}

#ifdef JAVA

static void* PR_CALLBACK 
FE_CreateJavaWindow(MWContext *context, LO_JavaAppStruct *java_struct,
		    int32 xp, int32 yp, int32 xs, int32 ys)
{
    LJAppletData* ad = (LJAppletData*)java_struct->session_data;
    Widget parent;
    Arg av[20];
    int ac = 0;
    Pixel bg;
    Widget contextWidget;

    parent = CONTEXT_DATA(context)->drawing_area;

    /* Adjust xp and yp for their offsets within the window */
    xp -= CONTEXT_DATA(context)->document_x;
    yp -= CONTEXT_DATA(context)->document_y;
    
    /*
    ** First time in for this applet; create motif widget for it
    */
    XtVaGetValues(parent, XmNbackground, &bg, 0);
    ac = 0;
    XtSetArg(av[ac], XmNborderWidth, 0); ac++;
    XtSetArg(av[ac], XmNx, (Position)xp); ac++;
    XtSetArg(av[ac], XmNy, (Position)yp); ac++;
    XtSetArg(av[ac], XmNwidth, (Dimension)xs); ac++;
    XtSetArg(av[ac], XmNheight, (Dimension)ys); ac++;
    XtSetArg(av[ac], XmNmarginWidth, 0); ac++;
    XtSetArg(av[ac], XmNmarginHeight, 0); ac++;
    XtSetArg(av[ac], XmNresizePolicy, XmRESIZE_NONE); ac++;
    XtSetArg(av[ac], XmNbackground, bg); ac++;
#ifdef DEBUG
    XtSetArg(av[ac], XmNtitle, ad->documentURL); ac++;
#endif /* DEBUG */
    contextWidget = XmCreateDrawingArea(parent,
					(char *)java_struct->attr_name,/* XXX */
					av, ac);
    XtSetMappedWhenManaged(contextWidget, FALSE);
    XtRealizeWidget(contextWidget); /* create window, but don't map */

    if (fe_globalData.fe_guffaw_scroll == 1)
    {
	XSetWindowAttributes attr;
	unsigned long valuemask;
	valuemask = CWBitGravity | CWWinGravity;
	attr.win_gravity = StaticGravity;
	attr.bit_gravity = StaticGravity;
	XChangeWindowAttributes(XtDisplay(contextWidget),
				XtWindow(contextWidget),
				valuemask, &attr);
    }
    XtManageChild(contextWidget);
    /* XSync(XtDisplay(contextWidget), 0); */

    return contextWidget;
}

static void PR_CALLBACK 
FE_RestoreJavaWindow(MWContext *context, LJAppletData* ad,
		     int32 xp, int32 yp, int32 xs, int32 ys)
{
    /*
    ** If the user goes  to another page and comes back to the applet's 
    ** page, the applet needs to be shown, So reparent the applet to the
    ** embedParent, when we have to show it. We don't need the old 
    ** squirrelling away code anymore.
    */

    Widget kid            = ad->fe_data;
    Widget embedParent    = ad->window;

    if (kid == NULL) return;
    
    /* Adjust xp and yp for their offsets within the window */
    xp -= CONTEXT_DATA(context)->document_x;
    yp -= CONTEXT_DATA(context)->document_y;
    
    XReparentWindow(XtDisplay(kid), XtWindow(kid), XtWindow(embedParent), 0, 0);
    if (XtParent(kid) != embedParent) {
	/* Motif hackery */
	(((CompositeWidgetClass) XtParent(kid)->core.widget_class)-> composite_class.delete_child) (kid);
	kid->core.parent = embedParent;
	(((CompositeWidgetClass) embedParent->core.widget_class)->composite_class.insert_child) (kid);
	(((CompositeWidgetClass) embedParent->core.widget_class)-> composite_class.change_managed) (embedParent);

    }
    XtMapWidget(kid); 
	    
}

static void PR_CALLBACK 
FE_SetJavaWindowPos(MWContext *context, void* window,
		    int32 xp, int32 yp, int32 xs, int32 ys)
{
    /* Adjust xp and yp for their offsets within the window */
    xp -= CONTEXT_DATA(context)->document_x;
    yp -= CONTEXT_DATA(context)->document_y;
    
    XtVaSetValues((Widget)window,
		  XmNx, (Position)xp,
		  XmNy, (Position)yp, 0);
}

static void PR_CALLBACK 
FE_SetJavaWindowVisibility(MWContext *context, void* window, PRBool visible)
{
    /* The layer containing the applet may be hidden or clipped by
       an enclosing layer, in which case we should unmap the
       applet's window */
    XtSetMappedWhenManaged((Widget)window, visible);
}

#endif /* JAVA */

void
XFE_DisplayJavaApp(MWContext *context,
		   int iLocation, LO_JavaAppStruct *java_struct)
{
#ifdef JAVA
    LJ_DisplayJavaApp(context, java_struct,
		      FE_DisplayNoJavaIcon,
		      FE_GetFullWindowSize,
		      FE_CreateJavaWindow,
                      FE_GetAwtWindow,
		      FE_RestoreJavaWindow,
		      FE_SetJavaWindowPos,
		      FE_SetJavaWindowVisibility);
#endif /* JAVA */
}

void
XFE_DrawJavaApp(MWContext *context,
		   int iLocation, LO_JavaAppStruct *java_struct)
{
}

void
XFE_GetJavaAppSize (MWContext *context, LO_JavaAppStruct *java_struct,
		    NET_ReloadMethod reloadMethod)
{
#ifdef JAVA
    LJ_GetJavaAppSize(context, java_struct, reloadMethod);
#else
    FE_DisplayNoJavaIcon(context, java_struct);
    java_struct->width = 1;
    java_struct->height = 1;
#endif
}

/*************************************************************************
 * End of Java Stuff
 ************************************************************************/


void 
XFE_HandleClippingView(MWContext *context, struct LJAppletData *appletD,
		       int x, int y, int width, int height)
{
}

void
fe_ReLayout (MWContext *context, NET_ReloadMethod force_reload)
{
  LO_Element *e = LO_XYToNearestElement (context,
					 CONTEXT_DATA (context)->document_x,
					 CONTEXT_DATA (context)->document_y,
                                         NULL);
  History_entry *he = SHIST_GetCurrent (&context->hist);
  URL_Struct *url;
  /* We must store the position into the History_entry before making
     a URL_Struct from it. */
  if (e && he)
    SHIST_SetPositionOfCurrentDoc (&context->hist, e->lo_any.ele_id);

  if (he)
    url = (force_reload == NET_RESIZE_RELOAD)
	? SHIST_CreateWysiwygURLStruct (context, he)
	: SHIST_CreateURLStructFromHistoryEntry (context, he);
  else if (sploosh)
    url = NET_CreateURLStruct (sploosh, FALSE);
  else
    url = 0;

  if (url)
    {
      if (force_reload != NET_DONT_RELOAD)
	url->force_reload = force_reload;

      /* warn plugins that the page relayout is not disasterous so that
	 it can fake caching their instances */
      /* XXX Only need to do this if you're eventually going to call
         NPL_EmbedDelete(), which doesn't appear to be the case?
      if (force_reload == NET_RESIZE_RELOAD || force_reload == NET_DONT_RELOAD)
	NPL_SamePage (context);
      */

      fe_GetURL (context, url, FALSE);
    }
}


/* Following links */

/* Returns the URL string of the LO_Element, if it has one.
   Returns "" for LO_ATTR_ISFORM, which are a total kludge...
 */
static char *
fe_url_of_xref (MWContext *context, LO_Element *xref, long x, long y)
{
  switch (xref->type)
    {
    case LO_TEXT:
      if (xref->lo_text.anchor_href)
	{
	  return (char *) xref->lo_text.anchor_href->anchor;
	}
      else
	{
          return (char *) NULL;
	}

    case LO_IMAGE:
      if (xref->lo_image.is_icon &&
          xref->lo_image.icon_number == IL_IMAGE_DELAYED)
        {
          long width, height;

          fe_IconSize(IL_IMAGE_DELAYED, &width, &height);
          if (xref->lo_image.alt &&
              xref->lo_image.alt_len &&
              (x > xref->lo_image.x + xref->lo_image.x_offset + 1 + 4 +
                  width))
            {
              if (xref->lo_image.anchor_href)
                {
                  return (char *) xref->lo_image.anchor_href->anchor;
                }
              else
                {
                  return (char *) NULL;
                }
            }
          else
            {
              return (char *) xref->lo_image.image_url;
            }
        }
      else if (xref->lo_image.image_attr->attrmask & LO_ATTR_ISFORM)
        {
          return "";
        }
      /*
       * This would be a client-side usemap image.
       */
      else if (xref->lo_image.image_attr->usemap_name != NULL)
        {
          LO_AnchorData *anchor_href;

          long ix = xref->lo_image.x + xref->lo_image.x_offset;
          long iy = xref->lo_image.y + xref->lo_image.y_offset;
          long mx = x - ix - xref->lo_image.border_width;
          long my = y - iy - xref->lo_image.border_width;

          anchor_href = LO_MapXYToAreaAnchor(context, (LO_ImageStruct *)xref,
							mx, my);
          if (anchor_href)
            {
              if (anchor_href->alt)
                {
                  return (char *) anchor_href->alt;
                }
              else
                {
                  return (char *) anchor_href->anchor;
                }
            }
          else
            {
              return (char *) NULL;
            }
        }
      else
        {
          if (xref->lo_image.anchor_href)
            {
              return (char *) xref->lo_image.anchor_href->anchor;
            }
          else
            {
              return (char *) NULL;
            }
        }

    default:
      return 0;
    }
}

void
fe_EventLOCoords (MWContext *context, XEvent *event,
		  unsigned long *x, unsigned long *y)
{
  *x = 0;
  *y = 0;

  switch (event->xany.type)
    {
    case ButtonPress:
    case ButtonRelease:
      *x = event->xbutton.x;
      *y = event->xbutton.y;
      break;

    case MotionNotify:
      *x = event->xmotion.x;
      *y = event->xmotion.y;
      break;

    case KeyPress:
    case KeyRelease:
      *x=event->xkey.x;
      *x=event->xkey.y;
      break;

    default:
      fprintf(stderr,
	      "fe_EventLOCoords(): unknown XEvent type %d\n",
	      event->xany.type);
      abort ();
      break;
    }

  *x += CONTEXT_DATA (context)->document_x;
  *y += CONTEXT_DATA (context)->document_y;
}

/* Returns the LO_Element under the mouse, if it is an anchor. */
static LO_Element *
fe_anchor_of_action (MWContext *context, CL_Event *layer_event,
                     CL_Layer *layer)
{
  LO_Element *le;
  unsigned long x, y;
  x = layer_event->x;
  y = layer_event->y;
  le = LO_XYToElement (context, x, y, layer);
  if (le && !fe_url_of_xref (context, le, x, y) && (le->type != LO_EDGE))
    le = 0;
  return le;
}

static void fe_WidgetLOCoords(MWContext *context, Widget widget,
			      unsigned long *x, unsigned long *y)
{
  Position wx, wy;

  XtVaGetValues(widget,XmNx,&wx,XmNy,&wy,NULL);

  (*x)=wx;
  (*y)=wy;

  *x += CONTEXT_DATA (context)->document_x;
  *y += CONTEXT_DATA (context)->document_y;
}
     

/* Returns the LO_Element of the widget, if it is a form element. */
static LO_Element *
fe_text_of_widget (MWContext *context, Widget widget,
                     CL_Layer *layer)
{
  LO_Element *le;
  unsigned long x, y;

  fe_WidgetLOCoords(context, widget, &x, &y);

  le = LO_XYToElement (context, x, y, layer);
  if (le && (le->type != LO_FORM_ELE))
    return NULL;

  return le;
}


void
fe_SetCursor (MWContext *context, Boolean over_link_p)
{
  Cursor c;

  if (CONTEXT_DATA (context)->save_next_mode_p)
    {
      if (over_link_p)
	c = CONTEXT_DATA (context)->save_next_link_cursor;
      else
	c = CONTEXT_DATA (context)->save_next_nonlink_cursor;
    }
  else if (CONTEXT_DATA (context)->clicking_blocked ||
	   CONTEXT_DATA (context)->synchronous_url_dialog)
    {
      c = CONTEXT_DATA (context)->busy_cursor;
    }
  else
    {
      if (over_link_p)
	c = CONTEXT_DATA (context)->link_cursor;
      else
	c = None;
    }
  if (CONTEXT_DATA (context)->drawing_area) {
    XDefineCursor (XtDisplay (CONTEXT_DATA (context)->drawing_area),
		   XtWindow (CONTEXT_DATA (context)->drawing_area),
		   c);
  }
}

static int click_x = -1, click_y = -1;	/* gag */
static Boolean moving = False;
static XtIntervalId auto_scroll_timer = 0;
static int fe_auto_scroll_x = 0;
static int fe_auto_scroll_y = 0;

static void
fe_auto_scroll_timer (XtPointer closure, XtIntervalId *id)
{
  MWContext *context = closure;
  int scale = 50; /* #### */
  int msecs = 10; /* #### */
  long new_x = (CONTEXT_DATA (context)->document_x +
		(scale * fe_auto_scroll_x));
  long new_y = (CONTEXT_DATA (context)->document_y +
		(scale * fe_auto_scroll_y));

  LO_ExtendSelection (context, new_x, new_y);
  fe_ScrollTo (context, (new_x > 0 ? new_x : 0), (new_y > 0 ? new_y : 0));

  auto_scroll_timer =
    XtAppAddTimeOut (fe_XtAppContext, msecs, fe_auto_scroll_timer, closure);
}

/* Invoked via a translation on <Btn1Down> and <Btn2Down>.
 */
extern void fe_HTMLDragSetLayer(CL_Layer *layer);

static void
fe_arm_link_action (Widget widget, XEvent *event, String *av, Cardinal *ac)
{
  MWContext *context = fe_MotionWidgetToMWContext (widget);
  CL_Event layer_event;
  fe_EventStruct fe_event;

  /* Clear global HTMLView drag layer.
   * If event gets dispatched to a layer, then a new value will be set
   * by fe_arm_link_action_for_layer()
   */
  fe_HTMLDragSetLayer(NULL);

  XP_ASSERT (context);
  if (!context) return;

  fe_UserActivity (context);

  fe_NeutralizeFocus (context);

  if (CONTEXT_DATA (context)->clicking_blocked ||
      CONTEXT_DATA (context)->synchronous_url_dialog)
    {
      XBell (XtDisplay (widget), 0);
      return;
    }

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  fe_event.av = av;
  fe_event.ac = ac;
  fe_event.mouse_action = FE_ARM_LINK;
#else
  fe_event_stuff(context,&fe_event,event,av,ac,FE_ARM_LINK);
  layer_event.fe_event_size = sizeof(fe_event);
#endif

  layer_event.fe_event = (void *)&fe_event;


  layer_event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
  layer_event.which = event->xbutton.button;
  layer_event.modifiers = xfeToLayerModifiers(event->xbutton.state);

  if (context->compositor)
      {
          unsigned long x, y;

          fe_EventLOCoords (context, event, &x, &y);
          layer_event.x = x;
          layer_event.y = y;

          CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
          fe_arm_link_action_for_layer(context, NULL, &layer_event);
      }
}


/* Layer specific actions.  fe_arm_link_action() */
void
fe_arm_link_action_for_layer(MWContext *context, CL_Layer *layer,
                             CL_Event *layer_event)
{
  LO_Element *xref;
  unsigned long x, y;
  Time time;

  /* Note that the av and ac parameters that were passed to 
	 fe_arm_link_action() can be obtained from the fe_event structure. */
  fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
#ifdef LAYERS_FULL_FE_EVENT
  XEvent *event = fe_event->event;
#else
  XEvent *event = fe_event_extract(fe_event,NULL,NULL,NULL);
#endif

  if (context->compositor)
      CL_GrabMouseEvents(context->compositor, layer);

  xref = fe_anchor_of_action (context, layer_event, layer);

  time = (event && (event->type == KeyPress ||
		    event->type == KeyRelease)
	       ? event->xkey.time :
	       event && (event->type == ButtonPress ||
			 event->type == ButtonRelease)
	       ? event->xbutton.time :
	       XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context))));

  x = layer_event->x;
  y = layer_event->y;
  fe_DisownSelection (context, time, False);
  LO_StartSelection (context, x, y, layer);

  click_x = x;
  click_y = y;
  moving = False;

#ifdef DEBUG
  if (last_armed_xref)
    fprintf (stderr,
	     "%s: ArmLink() invoked twice without intervening DisarmLink()?\n",
	     fe_progname);
#endif

  last_armed_xref = xref;
  if (xref)
    {
      LO_HighlightAnchor (context, last_armed_xref, True);
      last_armed_xref_highlighted_p = True;
    }
  else
    {
      last_armed_xref_highlighted_p = False;
    }

  if (CONTEXT_DATA (context)->save_next_mode_p)
    {
      if (! xref)
	{
	  XBell (XtDisplay (CONTEXT_WIDGET(context)), 0);
	  CONTEXT_DATA (context)->save_next_mode_p = False;
	  fe_SetCursor (context, False);
	  XFE_Progress (context,
			fe_globalData.click_to_save_cancelled_message);
	}
    }
}

static void
fe_disarm_link_action_by_context(MWContext* context, XEvent *event,
				 String *av, Cardinal *ac);

/* Invoked via a translation on <Btn1Up>
 */
static void
fe_disarm_link_action (Widget widget, XEvent *event, String *av, Cardinal *ac)
{
  MWContext *context = fe_MotionWidgetToMWContext (widget);
  fe_disarm_link_action_by_context(context, event, av, ac);
}

static void
fe_disarm_link_action_by_context(MWContext* context, XEvent *event,
				 String *av, Cardinal *ac)
{
#ifndef LAYERS_SEPARATE_DISARM
  Time time;
#endif /* LAYERS_SEPARATE_DISARM */

  XP_ASSERT (context);

  if (!context) return;

  if (auto_scroll_timer)
    {
      XtRemoveTimeOut (auto_scroll_timer);
      auto_scroll_timer = 0;
    }

  fe_UserActivity (context);

#ifdef LAYERS_SEPARATE_DISARM

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  fe_event.av = av;
  fe_event.ac = ac;
  fe_event.mouse_action = FE_DISARM_LINK;
#else
  fe_event_stuff(context,&fe_event,event,av,ac,FE_DISARM_LINK);
  layer_event.fe_event_size = sizeof(fe_event);
#endif
  layer_event.fe_event = (void *)&fe_event;

  layer_event.type = CL_EVENT_MOUSE_BUTTON_UP;
  layer_event.which = event->xbutton.button;
  layer_event.modifiers = xfeToLayerModifiers(event->xbutton.state);

  if (context->compositor)
      {
          unsigned long x, y;

          fe_EventLOCoords (context, event, &x, &y);
          layer_event.x = x;
          layer_event.y = y;

          CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
          fe_disarm_link_action_for_layer(context, NULL, &layer_event);
      }
}


/* Layer specific actions.  fe_disarm_link_action() */
void
fe_disarm_link_action_for_layer(MWContext *context, CL_Layer *layer,
                                CL_Event *layer_event)
{
  Time time;
  /* Note that the av and ac parameters that were passed to 
	 fe_disarm_link_action() can be obtained from the fe_event structure. */
  fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
#ifdef LAYERS_FULL_FE_EVENT
  XEvent *event = fe_event->event;
#else
  XEvent *event = fe_event_extract(fe_event,NULL,NULL,NULL);
#endif

  if (context->compositor)
      CL_GrabMouseEvents(context->compositor, NULL);
#endif /* LAYERS_SEPARATE_DISARM */

  time = (event && (event->type == KeyPress ||
		    event->type == KeyRelease)
	       ? event->xkey.time :
	       event && (event->type == ButtonPress ||
			 event->type == ButtonRelease)
	       ? event->xbutton.time :
	       XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context))));
  LO_EndSelection (context);
  fe_OwnSelection (context, time, False);

  if (last_armed_xref)
    {
      LO_HighlightAnchor (context, last_armed_xref, False);
    }

  last_armed_xref = 0;
  last_armed_xref_highlighted_p = False;
}

/* Invoked via a translation on <Btn1Motion>
 */
static void
fe_disarm_link_if_moved_action (Widget widget, XEvent *event,
				String *av, Cardinal *ac)
{
  MWContext *context = fe_MotionWidgetToMWContext (widget);
  CL_Event layer_event;
  fe_EventStruct fe_event;

  XP_ASSERT (context);
  if (!context) return;

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  fe_event.av = av;
  fe_event.ac = ac;
  fe_event.mouse_action = FE_DISARM_LINK_IF_MOVED;
#else
  fe_event_stuff(context,&fe_event,event,av,ac,FE_DISARM_LINK_IF_MOVED);
  layer_event.fe_event_size = sizeof(fe_event);
#endif
  layer_event.fe_event = (void *)&fe_event;

  layer_event.type = CL_EVENT_MOUSE_MOVE;
  layer_event.which = 0;
  layer_event.modifiers=0;
  
  if (context->compositor)
      {
          unsigned long x, y;

          fe_EventLOCoords (context, event, &x, &y);
          layer_event.x = x;
          layer_event.y = y;
          CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
          fe_disarm_link_if_moved_action_for_layer(context, NULL,
                                                   &layer_event);
      }
}


/* Layer specific actions.  fe_disarm_link_if_moved action() */
void
fe_disarm_link_if_moved_action_for_layer(MWContext *context, CL_Layer *layer,
                                         CL_Event *layer_event)
{
  LO_Element *xref;
  Boolean same_xref;
  unsigned long x, y;
  /* Note that the av and ac parameters that were passed to 
	 fe_disarm_link_if_moved_action() can be obtained from the 
	 fe_event structure. */
  fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
#ifdef LAYERS_FULL_FE_EVENT
  XEvent *event = fe_event->event;
#else
  XEvent *event = fe_event_extract(fe_event,NULL,NULL,NULL);
#endif

  xref = fe_anchor_of_action (context, layer_event, layer);

  x = layer_event->x;
  y = layer_event->y;

  same_xref = (last_armed_xref && xref &&
	       fe_url_of_xref (context, last_armed_xref, x, y) ==
	       fe_url_of_xref (context, xref, x, y));

  if (!moving &&
      (x > click_x + CONTEXT_DATA (context)->hysteresis ||
       x < click_x - CONTEXT_DATA (context)->hysteresis ||
       y > click_y + CONTEXT_DATA (context)->hysteresis ||
       y < click_y - CONTEXT_DATA (context)->hysteresis))
    moving = True;

  if (moving &&
      !CONTEXT_DATA (context)->clicking_blocked &&
      !CONTEXT_DATA (context)->synchronous_url_dialog)
    {
      int x_region, y_region;

      if (event->xmotion.x < 0)
	x_region = -1;
      else if (event->xmotion.x > CONTEXT_DATA (context)->scrolled_width)
	x_region = 1;
      else
	x_region = 0;

      if (event->xmotion.y < 0)
	y_region = -1;
      else if (event->xmotion.y > CONTEXT_DATA (context)->scrolled_height)
	y_region = 1;
      else
	y_region = 0;

      if (last_armed_xref && last_armed_xref_highlighted_p)
	{
	  LO_HighlightAnchor (context, last_armed_xref, False);
	  last_armed_xref = 0;
	  last_armed_xref_highlighted_p = False;
	  fe_SetCursor (context, False);
	}
      LO_ExtendSelection (context, x, y);

      fe_auto_scroll_x = x_region;
      fe_auto_scroll_y = y_region;

      if ((x_region != 0 || y_region != 0) && !auto_scroll_timer)
	{
	  /* turn on the timer */
	  fe_auto_scroll_timer (context, 0);
	}
      else if ((x_region == 0 && y_region == 0) && auto_scroll_timer)
	{
	  /* cancel the timer */
	  XtRemoveTimeOut (auto_scroll_timer);
	  auto_scroll_timer = 0;
	}
    }

  if (!last_armed_xref)
    return;

  if (!same_xref && last_armed_xref_highlighted_p)
    {
      LO_HighlightAnchor (context, last_armed_xref, False);
      last_armed_xref_highlighted_p = False;
    }
  else if (same_xref && !last_armed_xref_highlighted_p)
    {
      LO_HighlightAnchor (context, last_armed_xref, True);
      last_armed_xref_highlighted_p = True;
    }
}

typedef struct fe_mocha_closure {
  long x;
  long y;
  LO_FormSubmitData *data;
  LO_AnchorData *anchor_data;
  URL_Struct *url;
  XEvent *event;
  CL_Event *layer_event;
  Boolean save_p;
  Boolean other_p;
  Boolean image_delayed_p;
  Boolean free_element_p;
  String *av;
  Cardinal *ac;
} fe_mocha_closure;

static Boolean fe_FinishHREF (MWContext *context, 
			      LO_Element *element, 
			      fe_mocha_closure *closure);

static void fe_ParseHREF (MWContext *context,
			  LO_Element *element,
			  fe_mocha_closure *closure);

static void
fe_mocha_handle_submit (MWContext *context, LO_Element *element, int32 event,
			void *closure, ETEventStatus status)
{
  fe_mocha_closure *mocha_closure = (fe_mocha_closure *) closure;
  LO_FormSubmitData *data = NULL;
  char *action = NULL;

  if (status != EVENT_OK)  {
    XP_FREE (mocha_closure);
    return;
  }

  data = LO_SubmitImageForm (context, &element->lo_image,
			     mocha_closure->x, 
			     mocha_closure->y);
  if (data == NULL) {
    XP_FREE (mocha_closure);
    return;	/* XXX ignored anyway? what is right? */
  }
    
  action = (char *) data->action;
  mocha_closure->data = data;
  mocha_closure->url = NET_CreateURLStruct (action, FALSE);
  NET_AddLOSubmitDataToURLStruct (data, mocha_closure->url);
  fe_FinishHREF (context, element, mocha_closure);

  if (mocha_closure->event) XP_FREE (mocha_closure->event);
  XP_FREE (mocha_closure);
}

void fe_disarm_last_xref(void)
{
  XEvent* xevent;

#ifdef LAYERS_FULL_FE_EVENT
  xevent=&(last_armed_xref_closure_for_disarm.xevent);
#else
  xevent=fe_event_extract(&(last_armed_xref_closure_for_disarm.fe_event),
			  NULL,NULL,NULL);
#endif

  fe_disarm_link_action_by_context(last_armed_xref_closure_for_disarm.context,
			    xevent,
			    NULL,NULL);
}

static void
fe_mocha_handle_click (MWContext *context, LO_Element *element, int32 event,
		       void *closure, ETEventStatus status)
{
  fe_mocha_closure *mocha_closure = (fe_mocha_closure *) closure;

  if (status != EVENT_OK)
    {
      if (status==EVENT_PANIC)
	{
	  last_armed_xref = 0;
	  last_armed_xref_highlighted_p = False;
	}
      else
	fe_disarm_last_xref();
      if (mocha_closure)
	XP_FREE (mocha_closure);
      return;
    }

  fe_disarm_last_xref();

  /* mocha may have swapped our url - call the parsing code now. */
  fe_ParseHREF (context, element, mocha_closure);
  fe_FinishHREF (context, element, mocha_closure);

  if (mocha_closure->free_element_p) XP_DELETE (element);
  if (mocha_closure->event) XP_FREE (mocha_closure->event);
  XP_FREE (mocha_closure);
}


/* Ok. Now we have to delay the parsing of the anchor info until
 * after mocha has had a chance to change anything it wants.
 */

static void fe_ParseHREF (MWContext *context,
			  LO_Element *xref,
			  fe_mocha_closure *mocha_closure)
{
  if (xref->type == LO_IMAGE)
    {
      if (xref->lo_image.is_icon &&
	  xref->lo_image.icon_number == IL_IMAGE_DELAYED)
	{
	  long width, height;

	  fe_IconSize(IL_IMAGE_DELAYED, &width, &height);
	  if (xref->lo_image.alt &&
	      xref->lo_image.alt_len &&
	      (mocha_closure->layer_event->x > xref->lo_image.x +
	       xref->lo_image.x_offset + 1 + 4 + width))
		{
		  char *anchor = NULL;
	  
		  if (xref->lo_image.anchor_href)
		    {
		      anchor = (char *) xref->lo_image.anchor_href->anchor;
		      mocha_closure->anchor_data = xref->lo_image.anchor_href;
		    }
		  mocha_closure->url = NET_CreateURLStruct (anchor, FALSE);
		}
	      else
		{
		  mocha_closure->image_delayed_p = True;
		  mocha_closure->url = NET_CreateURLStruct (
					  (char *) xref->lo_image.image_url,
					  FALSE);
		}
	}
      else if (xref->lo_image.image_attr->usemap_name != NULL)
	/* If this is a usemap image, map the x,y to a url */
	{
	  char *anchor = NULL;
	  LO_AnchorData *anchor_href;
	  
	  anchor_href = LO_MapXYToAreaAnchor(context,
					     (LO_ImageStruct *)xref, 
					     mocha_closure->x, 
					     mocha_closure->y);
	  if (anchor_href)
	    {
	      mocha_closure->anchor_data = anchor_href;
	      anchor = (char *) anchor_href->anchor;
	    }

	  /* The user clicked; tell libmocha */
	  mocha_closure->url = NET_CreateURLStruct (anchor, FALSE);
	}
      else if (xref->lo_image.image_attr->attrmask & LO_ATTR_ISMAP)
	/* If this is an image map, append ?x?y to the URL. */
	{
	  char *anchor = NULL;
	  int x = mocha_closure->x;
	  int y = mocha_closure->y;
      
	  if (xref->lo_image.anchor_href)
	    {
	      anchor = (char *) xref->lo_image.anchor_href->anchor;
	      mocha_closure->anchor_data = xref->lo_image.anchor_href;
	    }
	  mocha_closure->url = NET_CreateURLStruct (anchor, FALSE);
	  NET_AddCoordinatesToURLStruct (mocha_closure->url, 
					 ((x < 0) ? 0 : x),
					 ((y < 0) ? 0 : y));
	}
      else
	{
	  char *anchor = NULL;
	  
	  if (xref->lo_image.anchor_href)
	    {
	      anchor = (char *) xref->lo_image.anchor_href->anchor;
	      mocha_closure->anchor_data = xref->lo_image.anchor_href;
	    }
	  mocha_closure->url = NET_CreateURLStruct (anchor, FALSE);
	}
    }
  else if (xref->type == LO_TEXT)
    {
      char *anchor = NULL;
      
      if (xref->lo_text.anchor_href)
	{
	  anchor = (char *) xref->lo_text.anchor_href->anchor;
	  mocha_closure->anchor_data = xref->lo_text.anchor_href;
	}
      mocha_closure->url = NET_CreateURLStruct (anchor, FALSE);
    }
  else if (xref->type == LO_EDGE)
    {
      /* Nothing to do here - should we ever get here? ### */
      ;
    }
  else
    {
      XP_ASSERT (False);
    }
}


Boolean fe_HandleHREF (MWContext *context,
		       LO_Element *xref,
                       Boolean save_p,
		       Boolean other_p,
		       CL_Event *layer_event,
		       CL_Layer *layer) /* in: may be NULL */
{
  fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
#ifdef LAYERS_FULL_FE_EVENT
  XEvent *event = fe_event->event;
  String *av = fe_event->av;
  Cardinal *ac = fe_event->ac;
#else
  String *av;
  Cardinal *ac;
  XEvent *event = fe_event_extract(fe_event,&av,&ac,NULL);
#endif
  /*MWContext *top = NULL;*/
  /*LO_AnchorData *anchor_data = NULL;*/
  /*LO_FormSubmitData *data = NULL;*/
  fe_mocha_closure *mocha_closure = XP_NEW_ZAP(fe_mocha_closure);

  /* setup the mocha callback data */
  mocha_closure->save_p = save_p;
  mocha_closure->other_p = other_p;
  mocha_closure->event = XP_NEW_ZAP (XEvent);
  XP_MEMCPY (mocha_closure->event, event, sizeof (XEvent));
  mocha_closure->layer_event = XP_NEW_ZAP (CL_Event);
  XP_MEMCPY (mocha_closure->layer_event, layer_event, sizeof (CL_Event));
  /* mocha_closure->av = av; */
  /* mocha_closure->ac = ac; */

  if (xref->type == LO_IMAGE)
    {
      long cx = layer_event->x;
      long cy = layer_event->y;
      long ix = xref->lo_image.x + xref->lo_image.x_offset;
      long iy = xref->lo_image.y + xref->lo_image.y_offset;
      long x = cx - ix - xref->lo_image.border_width;
      long y = cy - iy - xref->lo_image.border_width;

      /* store these away */
      mocha_closure->x = x;
      mocha_closure->y = y;

      if (xref->lo_image.image_attr->attrmask & LO_ATTR_ISFORM)
	/* If this is a form image, submit it... */
	{
	  {
	    JSEvent *event = XP_NEW_ZAP(JSEvent);
		
	    event->type = EVENT_SUBMIT;
		
	    ET_SendEvent (context, (LO_Element *) &xref->lo_image, 
			  event, fe_mocha_handle_submit,
			  mocha_closure);
	    return True;
	  }
	}
      else if (xref->lo_image.image_attr->usemap_name != NULL)
	{
	  LO_AnchorData *anchor_data;

	  anchor_data = LO_MapXYToAreaAnchor(context, (LO_ImageStruct *)xref, x, y);
	  if (anchor_data)
	    {
	      /* Imagemap area pretend to be links for JavaScript. */
	      mocha_closure->free_element_p = True;
	      xref = (LO_Element *) XP_NEW_ZAP(LO_Element);
	      xref->lo_text.type = LO_TEXT;
	      xref->lo_text.anchor_href = anchor_data;

	      /* We use the text of the element to determine if it is still
	         valid later so give the dummy text struct's text a value. */
	      if (anchor_data->anchor)
		xref->lo_text.text = anchor_data->anchor;
	    }
	}
    }
  {
    JSEvent *jsevent = XP_NEW_ZAP(JSEvent);

    jsevent->type = EVENT_CLICK;

    jsevent->x = layer_event->x;
    jsevent->y = layer_event->y;
    if (layer) {
      jsevent->docx = layer_event->x + CL_GetLayerXOrigin(layer);
      jsevent->docy = layer_event->y + CL_GetLayerYOrigin(layer);
    }
    else {
      jsevent->docx = layer_event->x;
      jsevent->docy = layer_event->y;
    }
    jsevent->which = layer_event->which;
    jsevent->modifiers = layer_event->modifiers;
    jsevent->screenx = event->xbutton.x_root;
    jsevent->screeny = event->xbutton.y_root;
    ET_SendEvent (context, (LO_Element *) xref,
		  jsevent, fe_mocha_handle_click,
		  mocha_closure);
    return True;
  }

  return False;
}

static Boolean
fe_FinishHREF (MWContext *context, 
	       LO_Element *element, 
	       fe_mocha_closure *mocha_closure)
{
  MWContext *top = NULL;
  URL_Struct *url		= mocha_closure->url;
  LO_FormSubmitData *data	= mocha_closure->data;
  LO_AnchorData *anchor_data	= mocha_closure->anchor_data;
  XEvent *event			= mocha_closure->event;
  Boolean image_delayed_p	= mocha_closure->image_delayed_p;
  Boolean other_p		= mocha_closure->other_p;
  Boolean save_p		= mocha_closure->save_p;
  String *av			= mocha_closure->av;
  Cardinal *ac			= mocha_closure->ac;
  Boolean link_selected_p = False;

    {

		/* Add the referer to the URL. */
		History_entry *he = SHIST_GetCurrent (&context->hist);
		if (url->referer) {
			free (url->referer);
			url->referer = 0;
		}
		
		url->referer = fe_GetURLForReferral(he);

#ifdef MOZ_MAIL_NEWS
      if (MSG_NewWindowProhibited (context, url->address))
	{
	  XP_ASSERT (!MSG_NewWindowRequired (context, url->address));
	  other_p = False;
	}
      else if (MSG_NewWindowRequired (context, url->address))
	{
	  MWContext *new_context = 0;
	  XP_ASSERT (!MSG_NewWindowProhibited (context, url->address));

	  /* If the user has clicked left (the "open in this window" gesture)
	     on a link in a window which is not able to display that kind of
	     URL (like, clicking on an HTTP link in a mail message) then we
	     find an existing context of an appropriate type (in this case,
	     a browser window) to display it in.  If there is no window of
	     the appropriate type, of if they had used the `new window'
	     gesture, then we create a new context of the apropriate type.
	   */
	  if (other_p)
	    new_context = 0;
	  else if (MSG_RequiresMailWindow (url->address))
	    new_context = XP_FindContextOfType (context, MWContextMail);
	  else if (MSG_RequiresNewsWindow (url->address))
	    new_context = XP_FindContextOfType (context, MWContextNews);
	  else if (MSG_RequiresBrowserWindow (url->address))
	    {
		  /* Be sure to skip nethelps when looking for context */
		  new_context = fe_FindNonCustomBrowserContext(context);
	    }

	  if (!new_context)
	    other_p = True;
	  else
	    {
	      if (context != new_context)
		/* If we have picked an existing context that isn't this
		   one in which to display this document, make sure that
		   context is uniconified and raised first. */
		XMapRaised(XtDisplay(CONTEXT_WIDGET(new_context)),
			   XtWindow(CONTEXT_WIDGET(new_context)));
	      context = new_context;
	    }
	}
#endif  /* MOZ_MAIL_NEWS */

      /* Regardless of how we got here, we need to make sure and
       * and use the toplevel context if our current one is a grid
       * cell. Grid cell's don't have chrome, and our new window
       * should.
       */
      top = XP_GetNonGridContext(context);

      if (save_p)
	{
	  fe_SaveURL (context, url);
	}
      /*
       * definitely get here from middle-click, are there other ways?
       */
      else if (other_p)
	{
	  /* Need to clear it right away, or it doesn't get cleared because
	     we blast last_armed_xref from fe_ClearArea...  Sigh. */
	  fe_disarm_link_action (CONTEXT_DATA (context)->drawing_area, event, av, ac);

	  /*
	   * When we middle-click for a new window we need
	   * to ignore all window targets.  It is easy to ignore
	   * the target on the anchor here, but we also need to
	   * ignore other targets that might be set later.  We do
	   * this by setting window_target in the URL struct, but
	   * not setting a window name in the context.
	   */
	  url->window_target = strdup ("");

	  /*
	   * We no longer want to follow anchor targets from middle-clicks.
	   */
	  fe_MakeWindow (XtParent (CONTEXT_WIDGET (top)), top,
			     url, NULL, MWContextBrowser, FALSE);
	}
      else if (image_delayed_p)
        {
          fe_LoadDelayedImage (context, url->address);
          NET_FreeURLStruct (url);
        }
      /*
       * Else a normal click on a link.
       * Follow that link in this window.
       */
      else
	{
	  /*
	   * If this link was targetted to a name window we need to either
	   * open it in that window (if it exists) or create a new window
	   * to open this link in (and assign the name to).
	   *
	   * Ignore targets for ComposeWindow urls.
	   */
	  if ( ((anchor_data)&&(anchor_data->target))
#ifdef MOZ_MAIL_NEWS
		&& !MSG_RequiresComposeWindow(url->address)
#endif
	  )
	    {
		MWContext *target_context = XP_FindNamedContextInList(context,
						(char *)anchor_data->target);
		/*
		 * If we copy the real target it, it will get processed
		 * again at parse time.  This is bad, because magic names
		 * like _parent return different values each time.
		 * So if we put the magic empty string here, it prevents
		 * us being overridden later, while not causing reprocessing.
		 */
		url->window_target = strdup ("");
		/*
		 * We found the named window, open this link there.
		 */
		if (target_context)
		  {
		    fe_GetURL (target_context, url, FALSE);
		  }
		/*
		 * No such named window, create one and open the link there.
		 */
		else
		  {
		    fe_MakeWindow (XtParent (CONTEXT_WIDGET (top)), top,
				   url, (char *)anchor_data->target,
				   MWContextBrowser, FALSE);
		  }
	    }
	  /*
	   * Else no target, just follow the link in this window.
	   */
	  else
	    {
	      fe_GetURL (context, url, FALSE);
	    }
	}

      if (data)
	LO_FreeSubmitData (data);

      link_selected_p = True;
    }
    return link_selected_p;
}

/* Invoked via a translation on <Btn1Up>
 */
static void
fe_activate_link_action (Widget widget, XEvent *event,
			 String *av, Cardinal *ac)
{
  MWContext *context = fe_MotionWidgetToMWContext (widget);
  CL_Event layer_event;
  fe_EventStruct fe_event;

  XP_ASSERT (context);
  if (!context) return;

  fe_NeutralizeFocus (context);

  if (auto_scroll_timer)
    {
      XtRemoveTimeOut (auto_scroll_timer);
      auto_scroll_timer = 0;
    }

  fe_UserActivity (context);

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  fe_event.av = av;
  fe_event.ac = ac;
  fe_event.mouse_action = FE_ACTIVATE_LINK;
#else
  fe_event_stuff(context,&fe_event,event,av,ac,FE_ACTIVATE_LINK);
  layer_event.fe_event_size = sizeof(fe_event);
#endif
  layer_event.fe_event = (void *)&fe_event;

  layer_event.type = CL_EVENT_MOUSE_BUTTON_UP;
  layer_event.which = event->xbutton.button;
  layer_event.modifiers = xfeToLayerModifiers(event->xbutton.state);

  if (context->compositor)
      {
          unsigned long x, y;

          fe_EventLOCoords (context, event, &x, &y);
          layer_event.x = x;
          layer_event.y = y;

          CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
          fe_activate_link_action_for_layer(context, NULL, &layer_event);
      }
}


/* Layer specific actions.  fe_activate_link_action() */
void
fe_activate_link_action_for_layer(MWContext *context, CL_Layer *layer,
                                  CL_Event *layer_event)
{
  LO_Element *xref;
  Boolean other_p = False;
  Boolean save_p = False;
  Boolean link_selected_p = False;
  fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
#ifdef LAYERS_FULL_FE_EVENT
  String *av = fe_event->av;
  Cardinal *ac = fe_event->ac;
#else
  String *av;
  Cardinal *ac;
  XEvent* event=fe_event_extract(fe_event,&av,&ac,NULL);
#endif

  if (context->compositor)
      CL_GrabMouseEvents(context->compositor, NULL);

  xref = fe_anchor_of_action (context, layer_event, layer);

  if (*ac > 2)
    fprintf (stderr,
			 XP_GetString(XFE_LAY_TOO_MANY_ARGS_TO_ACTIVATE_LINK_ACTION),
			 fe_progname,*ac);
  else if (*ac == 1 && !strcmp ("new-window", av[0]))
    other_p = True;
  else if (*ac == 1 && !strcmp ("save-only", av[0]))
    save_p = True;
  else if (*ac > 0)
    fprintf (stderr,
			 XP_GetString(XFE_LAY_UNKNOWN_PARAMETER_TO_ACTIVATE_LINK_ACTION),
	     fe_progname, av[0]);

  if (CONTEXT_DATA (context)->save_next_mode_p)
    {
      save_p = True;
      CONTEXT_DATA (context)->save_next_mode_p = False;
    }

  /* Turn off the selection cursor.  It'll be updated again at next motion. */
  fe_SetCursor (context, False);

  if (   /* If a selection was made, don't follow the link. */
         (LO_HaveSelection (context))
      || CONTEXT_DATA (context)->clicking_blocked
      || CONTEXT_DATA (context)->synchronous_url_dialog
      || (!xref)
      || ((last_armed_xref) && (xref != last_armed_xref))
      )
    {
      fe_disarm_link_action_by_context(context,event,NULL,NULL);

	  /*  If (1) there was no link and
       *     (2) there was no selection and
	   *	 (3) mouse button 2 was pressed and
	   *	 (4) mouse button was released and
	   *	 (5) this is a browser context
	   *
	   *  The user clicked button 2 on nothing.
	   *
	   *  Try to do the primary selection magic.
	   */
	   	
	  if (!xref && 
		  !LO_HaveSelection (context) &&
		  layer_event &&
		  (layer_event->which == 2) &&
		  (layer_event->type == CL_EVENT_MOUSE_BUTTON_UP) &&
		  (context->type == MWContextBrowser) &&
		  CONTEXT_WIDGET(context))
	  {
		  fe_PrimarySelectionFetchURL(context);
	  }
    }
  else
    {
#ifdef LAYERS_FULL_FE_EVENT
      memcpy(&(last_armed_xref_closure_for_disarm.xevent),
	     event,
	     sizeof(XEvent));
#else
      last_armed_xref_closure_for_disarm.fe_event=(*fe_event);
#endif
      last_armed_xref_closure_for_disarm.context=context;

      link_selected_p = fe_HandleHREF (context, xref, save_p, other_p,
				       layer_event, layer);
    }

/* DONT ACCESS context AFTER A GetURL. fe_HandleHREF could do fe_GetURL. */

}

/* Invoked via a translation on <Motion>
 */
void
fe_describe_link_action (Widget widget, XEvent *event,
			 String *av, Cardinal *ac)
{
  MWContext *context = fe_MotionWidgetToMWContext (widget);
  CL_Event layer_event;
  fe_EventStruct fe_event;

/*   XP_ASSERT (context); */
  if (!context) return;

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  fe_event.av = av;
  fe_event.ac = ac;
  fe_event.mouse_action = FE_DESCRIBE_LINK;
#else
  fe_event_stuff(context,&fe_event,event,av,ac,FE_DESCRIBE_LINK);
  layer_event.fe_event_size = sizeof(fe_event);
#endif
  layer_event.fe_event = (void *)&fe_event;

  layer_event.type = CL_EVENT_MOUSE_MOVE;
  layer_event.which = 0;
  layer_event.modifiers=0;

  if (context->compositor)
      {
          unsigned long x, y;

          fe_EventLOCoords (context, event, &x, &y);
          layer_event.x = x;
          layer_event.y = y;
          CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
          fe_describe_link_action_for_layer(context, NULL, &layer_event);
      }
}

static void
fe_mouse_over_callback(MWContext * context, LO_Element * lo_element, int32 event,
		      void * pObj, ETEventStatus status)
{

	switch(status) {
	case EVENT_OK:
#ifdef DEBUG_spence
		printf ("fe_mouse_over_cb: event ok\n");
#endif
		break;
	case EVENT_PANIC:
		/* backend says don't do anything */
#ifdef DEBUG_spence
		printf ("fe_mouse_over_cb: event panic!\n");
#endif
		break;
    default:
	  {
/*  		char *url = NULL; */
#ifdef DEBUG_spence
		printf ("fe_mouse_over_cb: event !ok; we'll set the status bar\n");
#endif
#if 0
		/* backend didn't set the status bar, so we'll do it */
		if (event == EVENT_MOUSEOVER) {
			if (lo_element) {
				url = (char *) lo_element->lo_text.anchor_href->alt;
				if (url == NULL)
					url = (char *) lo_element->lo_text.anchor_href->anchor;
			}
			if (url)
				fe_MidTruncatedProgress (context, url);
		}
#endif /* 0 */
		break;
	  }
	} /* end switch */

	/* Free the temporary dummy layout element. */
	XP_FREE(lo_element);
}

/* Layer specific actions.  fe_describe_link_action() */
void
fe_describe_link_action_for_layer(MWContext *context, CL_Layer *layer,
                                  CL_Event *layer_event)
{
  static XP_Bool m_isImage = False;
  MWContext *top = XP_GetNonGridContext (context);
  LO_Element *xref;
  LO_AnchorData *anchor_data = NULL;
  unsigned long x, y;
  long ix, iy, mx, my;

  /* Note that the av and ac parameters that were passed to 
	 fe_describe_link_action() can be obtained from the fe_event structure. */

  xref = fe_anchor_of_action (context, layer_event, layer);

  x = layer_event->x;
  y = layer_event->y;

  {
	  static LO_Element *m_lastLE = NULL;

	  LO_Element *le = LO_XYToElement (context, x, y, layer);

	  if (le && le->type == LO_IMAGE) {
		  /* In image 
		   */
		  if (!m_isImage) {
			  /* Enter image
			   */
			  fe_HTMLViewTooltipsEH(context, layer,	layer_event, 1);
		  }/* if */


		  m_isImage = True;
	  }/* if */
	  else {
		  if (m_isImage) {
			  /* Leave image
			   */
			  fe_HTMLViewTooltipsEH(context, layer,	layer_event, 4);
		  }/* */
		  m_isImage = False;
	  }/* else */
	  m_lastLE = le;
	  /*
	  XDBG(printf("\n fe_describe_link_action_for_layer, le->type=%d %s\n", 
				  le?le->type:-10,
				  (le && le->type==LO_IMAGE)?"-->>LO_IMAGE":""));
				  */
  }
  if (xref == NULL || xref != last_documented_xref  ||
      (last_documented_xref && (last_documented_xref->type == LO_IMAGE)&&
       (last_documented_xref->lo_image.image_attr->usemap_name != NULL)))
    {
      char *url = (xref ? fe_url_of_xref (context, xref, x, y) : 0);
      anchor_data = NULL;
      if (xref) {
		  if (last_documented_xref != xref && xref->type == LO_TEXT)
			  anchor_data = xref->lo_text.anchor_href;
		  else if (xref->type == LO_IMAGE)
			  if (xref->lo_image.image_attr->usemap_name != NULL) {
				  /* Image map */
				  ix = xref->lo_image.x + xref->lo_image.x_offset;
				  iy = xref->lo_image.y + xref->lo_image.y_offset;
				  mx = x - ix - xref->lo_image.border_width;
				  my = y - iy - xref->lo_image.border_width;
				  anchor_data =
					  LO_MapXYToAreaAnchor(context, (LO_ImageStruct *)xref, mx, my);
			  }
			  else if (last_documented_xref != xref)
				  anchor_data = xref->lo_image.anchor_href;
      }
      
      /* send mouse out mocha event only if we have left a link.
       * conditions are :
       *  i) left a link to go to a non-link
       * ii) left a link to go to another link
       * iii) Moving around inside an image
       * Note: Mouse Out must happen before mouse over.
       */
      if (last_documented_anchor_data && last_documented_xref
		  && last_documented_xref_context)
		  if (last_documented_anchor_data != anchor_data) {
			  JSEvent *event;
			  LO_Element *dummy_xref = (LO_Element *) XP_NEW_ZAP (LO_Element);

			  TRACEMSG (("sending MouseOut\n"));

			  dummy_xref->lo_text.type = LO_TEXT;

			  /* this is problematic -- what if the anchor has been destroyed? */
			  dummy_xref->lo_text.anchor_href = last_documented_anchor_data;
			  dummy_xref->lo_text.text = dummy_xref->lo_text.anchor_href->anchor;

			  event = XP_NEW_ZAP(JSEvent);
			  event->type = EVENT_MOUSEOUT;
			  event->x = layer_event->x;
			  event->y = layer_event->y;
			  {
			    fe_EventStruct* e=(fe_EventStruct*)layer_event->fe_event;
			    event->screenx=e->compressedEvent.pos.root.x;
			    event->screeny=e->compressedEvent.pos.root.y;
			  }
			  if (layer) {
			    event->docx = layer_event->x + CL_GetLayerXOrigin(layer);
			    event->docy = layer_event->y + CL_GetLayerYOrigin(layer);
			  }
			  else {
			    event->docx = layer_event->x;
			    event->docy = layer_event->y;
			  }

			  if (m_isImage) {
				  fe_HTMLViewTooltipsEH(context, layer,	layer_event, 2);
			  }/* if */
			  
#ifdef DEBUG_spence
			  printf ("Sending MouseOut\n");
#endif
			  ET_SendEvent (last_documented_xref_context, dummy_xref,
							event, fe_mouse_over_callback, NULL);
		  }

      if (CONTEXT_DATA (context)->active_url_count == 0) {
		  /* If there are transfers in progress, don't document the URL under
			 the mouse, since that message would interfere with the transfer
			 messages.  Do change the cursor, however. */
		  XP_Bool used = False;
		  if (anchor_data) {
			  if (anchor_data != last_documented_anchor_data) {
				  JSEvent *event;
				  LO_Element *dummy_xref = (LO_Element *) XP_NEW_ZAP (LO_Element);

				  XP_MEMSET (dummy_xref, 0, sizeof (LO_Element));

				  dummy_xref->lo_text.type = LO_TEXT;
				  dummy_xref->lo_text.anchor_href = anchor_data;

				  /* we use the text of the element to determine if it is still
					 valid later so give the dummy text struct's text a value.
					 */
				  dummy_xref->lo_text.text = anchor_data->anchor;
				  
				  /* just tell mocha - nothing else to do? */
				  event = XP_NEW_ZAP(JSEvent);
				  event->type = EVENT_MOUSEOVER;

				  /* get a valid layer id */
				  event->layer_id = LO_GetIdFromLayer (context, layer);

				  event->x = layer_event->x;
				  event->y = layer_event->y;
				  {
				    fe_EventStruct* e=(fe_EventStruct*)layer_event->fe_event;
				    event->screenx=e->compressedEvent.pos.root.x;
				    event->screeny=e->compressedEvent.pos.root.y;
				  }

				  if (layer) {
				    event->docx = layer_event->x + CL_GetLayerXOrigin(layer);
				    event->docy = layer_event->y + CL_GetLayerYOrigin(layer);
				  }
				  else {
				    event->docx = layer_event->x;
				    event->docy = layer_event->y;
				  }

				  if (m_isImage) {
					  fe_HTMLViewTooltipsEH(context, layer,	layer_event, 3);
				  }/* if */
#ifdef DEBUG_spence
				  printf ("Sending MouseOver\n");
#endif
				  ET_SendEvent (context, dummy_xref, event,
								fe_mouse_over_callback, NULL);
			  }
			  else
				  /* Dont update url too as we haven't moved to a new AREA */
				  used = True;
		  } 
#if 0
		  else {
			  printf ("anchor_data == NULL\n");
		  }
#endif

	if (!used)
		fe_MidTruncatedProgress (context, (xref ? url : ""));
      }
	  
      last_documented_xref_context = context;
      last_documented_xref = xref;
      last_documented_anchor_data = anchor_data;

      fe_SetCursor (top, !!xref);
    }
}

/* Invoked via a translation on <Btn3Down>
 */
void
fe_extend_selection_action (Widget widget, XEvent *event,
			    String *av, Cardinal *ac)
{
  MWContext *context = fe_MotionWidgetToMWContext (widget);
  CL_Event layer_event;
  fe_EventStruct fe_event;

  XP_ASSERT (context);
  if (!context) return;

  if (auto_scroll_timer)
    {
      XtRemoveTimeOut (auto_scroll_timer);
      auto_scroll_timer = 0;
    }

  fe_UserActivity (context);

  fe_NeutralizeFocus (context);

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  fe_event.av = av;
  fe_event.ac = ac;
  fe_event.mouse_action = FE_EXTEND_SELECTION;
#else
  fe_event_stuff(context,&fe_event,event,av,ac,FE_EXTEND_SELECTION);
  layer_event.fe_event_size = sizeof(fe_event);
#endif
  layer_event.fe_event = (void *)&fe_event;

  layer_event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
  layer_event.which = event->xbutton.button;
  layer_event.modifiers = xfeToLayerModifiers(event->xbutton.state);

  if (context->compositor)
      {
          unsigned long x, y;

          fe_EventLOCoords (context, event, &x, &y);
          layer_event.x = x;
          layer_event.y = y;
          CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
          fe_extend_selection_action_for_layer(context, NULL, &layer_event);
      }
}

/* Layer specific actions.  fe_extend_selection_action() */
void
fe_extend_selection_action_for_layer(MWContext *context, CL_Layer *layer,
                                     CL_Event *layer_event)
{
  Time time;
  unsigned long x, y;

  /* Note that the av and ac parameters that were passed to 
	 fe_extend_selection_action() can be obtained from the 
	 fe_event structure. */
  fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
#ifdef LAYERS_FULL_FE_EVENT
  XEvent *event = fe_event->event;
#else
  XEvent *event = fe_event_extract(fe_event,NULL,NULL,NULL);
#endif

  time = (event && (event->type == KeyPress ||
		    event->type == KeyRelease)
	  ? event->xkey.time :
	  event && (event->type == ButtonPress ||
		    event->type == ButtonRelease)
	  ? event->xbutton.time :
	  XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context))));

  x = layer_event->x;
  y = layer_event->y;

  LO_ExtendSelection (context, x, y);
  fe_OwnSelection (context, time, False);

  /* Making a selection turns off "Save Next" mode. */
  if (CONTEXT_DATA (context)->save_next_mode_p)
    {
      XBell (XtDisplay (CONTEXT_WIDGET(context)), 0);
      CONTEXT_DATA (context)->save_next_mode_p = False;
      fe_SetCursor (context, False);
      XFE_Progress (context, fe_globalData.click_to_save_cancelled_message);
    }
}


#ifdef DEBUG_francis
static void printKeyEvent(XEvent* event)
{
  if (!(   ((event->xany.type)==KeyPress)
	|| ((event->xany.type)==KeyRelease)
	)
      )
    {
      printf("{non-key event}\n");
      return;
    }

  printf("{key event:\n"
	 "\tserial==%u\n"
	 "\tsend_event==%s\n"
	 "\tdisplay==0x%x\n"
	 "\twindow==0x%x\n"
	 "\troot==0x%x\n"
	 "\tsubwindow==0x%x\n"
	 "\ttime==0x%x\n"
	 "\t(x,y)==(%d,%d)\n"
	 "\t(x_root,y_root)==(%d,%d)\n"
	 "\tstate==%d\n"
	 "\tkeycode==%d\n"
	 "\tsame_screen==%s}\n",
	 event->xkey.serial,
	 (event->xkey.send_event ? "true" : "false"),
	 event->xkey.display,
	 event->xkey.window,
	 event->xkey.root,
	 event->xkey.subwindow,
	 event->xkey.time,
	 event->xkey.x,event->xkey.y,
	 event->xkey.x_root,event->xkey.y_root,
	 event->xkey.state,event->xkey.keycode,
	 (event->xkey.same_screen ? "true" : "false")
	 );
}
#endif

static XP_Bool keyStates[65536];
static XP_Bool keyStatesInited=False;

static void keyStatesInit(void)
{
  if (keyStatesInited)
    return;
  memset(keyStates,0,sizeof(keyStates));
  keyStatesInited=True;
}

static XP_Bool keyStatesDown(int keycode)
{
  if ((keycode<0) || (keycode>=65536))
    return 0;

  keyStatesInit();

  {
    XP_Bool res=keyStates[keycode];
    keyStates[keycode]=1;
    return res;
  }
}

static XP_Bool keyStatesUp(int keycode)
{
  if ((keycode<0) || (keycode>=65536))
    return 0;

  keyStatesInit();

  {
    XP_Bool res=keyStates[keycode];
    keyStates[keycode]=0;
    return res;
  }
}

static void fe_key_up_in_text_action(Widget widget,
				     XEvent *event,
				     String *av, Cardinal *ac)
{
  MWContext *context = fe_WidgetToMWContext (widget);
  CL_Event layer_event;
  fe_EventStruct fe_event;

  XP_ASSERT (context);
  if (!context) return;

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  fe_event.av = av;
  fe_event.ac = ac;
  fe_event.mouse_action = FE_KEY_UP;
#else
  fe_event_stuff(context,&fe_event,event,av,ac,FE_KEY_UP);
  layer_event.fe_event_size = sizeof(fe_event);

  fe_event.data=widget;
#endif

  layer_event.fe_event = (void *)&fe_event;

  layer_event.type = CL_EVENT_KEY_UP;
  layer_event.which = xfeKeycodeToWhich(event->xkey.keycode,
										event->xkey.state);
  layer_event.modifiers = xfeToLayerModifiers(event->xkey.state);

  if (context->compositor)
      {
          unsigned long x, y;

          fe_EventLOCoords (context, event, &x, &y);
          layer_event.x = x;
          layer_event.y = y;
          CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
          fe_key_up_in_text_action_for_layer(context, NULL, &layer_event);
      }
}

/* Layer specific actions.  fe_extend_selection_action() */
void
fe_key_up_in_text_action_for_layer(MWContext *context, CL_Layer *layer,
				   CL_Event *layer_event)
{
  Time time;
  unsigned long x, y;
  /* Note that the av and ac parameters that were passed to 
	 fe_extend_selection_action() can be obtained from the 
	 fe_event structure. */
  fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
#ifndef LAYERS_FULL_FE_EVENT
  XEvent *event = fe_event_extract(fe_event,NULL,NULL,NULL);
  Widget widget=(Widget)fe_event->data;
#endif

  time = (event && (event->type == KeyPress ||
		    event->type == KeyRelease)
	  ? event->xkey.time :
	  event && (event->type == ButtonPress ||
		    event->type == ButtonRelease)
	  ? event->xbutton.time :
	  XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context))));

  x = layer_event->x;
  y = layer_event->y;

  {
    LO_Element* text=fe_text_of_widget(context, widget, layer);

    keyStatesUp(layer_event->which);
    
    {
      JSEvent *jsevent = (JSEvent*)XP_NEW_ZAP(JSEvent);

      jsevent->type = EVENT_KEYUP;

      jsevent->x = layer_event->x;
      jsevent->y = layer_event->y;
      if (layer) {
	jsevent->docx = layer_event->x + CL_GetLayerXOrigin(layer);
	jsevent->docy = layer_event->y + CL_GetLayerYOrigin(layer);
      }
      else {
	jsevent->docx = layer_event->x;
	jsevent->docy = layer_event->y;
      }
      jsevent->which = layer_event->which;
      jsevent->modifiers = layer_event->modifiers;
      jsevent->screenx = event->xbutton.x_root;
      jsevent->screeny = event->xbutton.y_root;
      
      ET_SendEvent (context, text,
		    jsevent,
		    NULL, NULL);
    }
  }
}

typedef struct {
  Widget widget;
  fe_EventStruct evt;
  int newInsertionPoint;
} KeydownClosure;

static int fe_textModifyVerifyCallbackInhibited=0;

int fe_isTextModifyVerifyCallbackInhibited(void)
{
  return fe_textModifyVerifyCallbackInhibited;
}

static int fe_textModifyVerifyCallbackNewInsertionPoint=-1;

static void finish_keydown(void* _closure)
{
  KeydownClosure* closure=(KeydownClosure*)_closure;
  String* av;
  Cardinal* ac;
  XEvent *event = fe_event_extract(&(closure->evt),&av,&ac,NULL);
  KeySym keysym=xfeKeycodeToWhich(event->xkey.keycode,
				  event->xkey.state);
  
  fe_textModifyVerifyCallbackInhibited++;
  if ((closure->newInsertionPoint)<0)
    XtCallActionProc(closure->widget,
		     (  (keysym==XK_Return)
		      ? "process-return"
		      : "self-insert"
		      ),
		     event,
		     av,*ac);
  else
    XmTextSetInsertionPosition(closure->widget,
			       closure->newInsertionPoint);
  fe_textModifyVerifyCallbackInhibited--;
  XP_FREE(closure);
}

static void fe_mocha_handle_keydown(MWContext* context,
				    LO_Element* element,
				    int32 _event,
				    void* closure,
				    ETEventStatus status)
{
  if (status == EVENT_OK)
    finish_keydown(closure);
  else
    XP_FREE(closure);
}

static void fe_key_down_in_text_action(Widget widget,
				       XEvent *event,
				       String *av, Cardinal *ac)
{
  MWContext *context = fe_WidgetToMWContext (widget);
  CL_Event layer_event;
  fe_EventStruct fe_event;

  XP_ASSERT (context);
  if (!context) return;

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  fe_event.av = av;
  fe_event.ac = ac;
  fe_event.mouse_action = FE_KEY_DOWN;
  fe_event.data=widget;
#else
  fe_event_stuff(context,&fe_event,event,av,ac,FE_KEY_DOWN);
  fe_event.data=widget;
  layer_event.fe_event_size = sizeof(fe_event);

#endif
  layer_event.fe_event = (void *)&fe_event;

  layer_event.type = CL_EVENT_KEY_DOWN;
  layer_event.which = xfeKeycodeToWhich(event->xkey.keycode,
										event->xkey.state);
  layer_event.modifiers = xfeToLayerModifiers(event->xkey.state);

  if (context->compositor)
    {
      unsigned long x, y;

      fe_EventLOCoords (context, event, &x, &y);
      layer_event.x = x;
      layer_event.y = y;
      CL_DispatchEvent(context->compositor, &layer_event);
    }
  else
    {
      fe_key_down_in_text_action_for_layer(context, NULL, &layer_event);
    }
}

/* Layer specific actions.  fe_extend_selection_action() */
void
fe_key_down_in_text_action_for_layer(MWContext *context, CL_Layer *layer,
				     CL_Event *layer_event)
{
  Time time;
  unsigned long x, y;
  /* Note that the av and ac parameters that were passed to 
	 fe_extend_selection_action() can be obtained from the 
	 fe_event structure. */
  fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
#ifndef LAYERS_FULL_FE_EVENT
  String *av;
  Cardinal *ac;
  XEvent *event = fe_event_extract(fe_event,&av,&ac,NULL);
  Widget widget=(Widget)fe_event->data;
#endif

  time = (event && (event->type == KeyPress ||
		    event->type == KeyRelease)
	  ? event->xkey.time :
	  event && (event->type == ButtonPress ||
		    event->type == ButtonRelease)
	  ? event->xbutton.time :
	  XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context))));

  x = layer_event->x;
  y = layer_event->y;

  {
    LO_Element* text=fe_text_of_widget(context, widget, layer);
    KeydownClosure* closure=XP_NEW_ZAP(KeydownClosure);

    closure->widget=widget;
    closure->evt=(*fe_event);

#if 0
    if ((*ac)==2)
      sscanf(av[1],"%d",&(closure->newInsertionPoint));
    else
      closure->newInsertionPoint=-1;
#else
    closure->newInsertionPoint=fe_textModifyVerifyCallbackNewInsertionPoint;
#endif

    {
      JSEvent *jsevent = XP_NEW_ZAP(JSEvent);

      jsevent->type = (  keyStatesDown(layer_event->which)
		       ? EVENT_KEYPRESS
		       : EVENT_KEYDOWN
		       );

      jsevent->x = layer_event->x;
      jsevent->y = layer_event->y;
      if (layer) {
	jsevent->docx = layer_event->x + CL_GetLayerXOrigin(layer);
	jsevent->docy = layer_event->y + CL_GetLayerYOrigin(layer);
      }
      else {
	jsevent->docx = layer_event->x;
	jsevent->docy = layer_event->y;
      }
      jsevent->which = layer_event->which;
      jsevent->modifiers = layer_event->modifiers;
      jsevent->screenx = event->xbutton.x_root;
      jsevent->screeny = event->xbutton.y_root;
      
      ET_SendEvent (context, text,
		    jsevent, fe_mocha_handle_keydown,
		    closure);
    }
  }

}

void fe_textModifyVerifyCallback(Widget w,
								 XtPointer closure,
								 XtPointer call_data)
{
  if (!fe_textModifyVerifyCallbackInhibited)
    {
      XmTextVerifyCallbackStruct* cbs=(XmTextVerifyCallbackStruct*)call_data;
      XEvent* event=cbs->event;
      if (   event
		  && (event->type==KeyPress)
		  && (cbs->text)
		  && ((cbs->text->length)==1)
		  )
		{
		  cbs->doit=False;
		  fe_textModifyVerifyCallbackNewInsertionPoint=-1;
		  fe_key_down_in_text_action(w,event,NULL,NULL);
		}
	  else
		{
		  fe_textModifyVerifyCallbackInhibited++;
		  XtCallCallbacks(w,XmNmodifyVerifyCallback,call_data);
		  fe_textModifyVerifyCallbackInhibited--;
		}
    }
}

void fe_textMotionVerifyCallback(Widget w,
				 XtPointer closure,
				 XtPointer call_data)
{
  if (!fe_textModifyVerifyCallbackInhibited)
    {
      XmTextVerifyCallbackStruct* cbs=(XmTextVerifyCallbackStruct*)call_data;
      XEvent* event=cbs->event;
      if (   event
	  && (event->type==KeyPress)
	  )
	{
#if 0
	  char* buff=malloc(20);
	  String argv[3]={"motion",buff,0};
	  Cardinal argc=2;
	  sprintf(buff,"%d",cbs->newInsert);
#endif
	  cbs->doit=False;
	  fe_textModifyVerifyCallbackNewInsertionPoint=cbs->newInsert;
	  fe_key_down_in_text_action(w,event,
#if 0
				     argv,&argc
#else
				     NULL,NULL
#endif
				     );
	}
    }
}



static XtActionsRec fe_mouse_actions [] =
{
  { "ArmLink",		 fe_arm_link_action },
  { "DisarmLink",	 fe_disarm_link_action },
  { "ActivateLink",	 fe_activate_link_action },
  { "DisarmLinkIfMoved", fe_disarm_link_if_moved_action },
  { "ExtendSelection",	 fe_extend_selection_action },
  { "DescribeLink",	 fe_describe_link_action }
};

void
fe_InitMouseActions ()
{
  XtAppAddActions (fe_XtAppContext, fe_mouse_actions,
		   countof (fe_mouse_actions));
}

static XtActionsRec fe_key_actions [] =
{
  { "KeyUpInText",	 fe_key_up_in_text_action }
/*,
  { "KeyDownInText",	 fe_key_down_in_text_action }*/
};

void
fe_InitKeyActions ()
{
  XtAppAddActions (fe_XtAppContext, fe_key_actions,
		   countof (fe_key_actions));
}

static ContextFuncs _xfe_funcs = {
#define FE_DEFINE(func, returns, args) XFE##_##func,
#include "mk_cx_fn.h"
};

ContextFuncs *
fe_BuildDisplayFunctionTable(void)
{
	return &_xfe_funcs;
}

void
FE_LoadGridCellFromHistory(MWContext *context, void *hist,
			   NET_ReloadMethod force_reload)
{
  History_entry *he = (History_entry *)hist;
  URL_Struct *url;

  if (! he) return;
  url = SHIST_CreateURLStructFromHistoryEntry (context, he);
  url->force_reload = force_reload;
  fe_GetURL (context, url, FALSE);
}

void *
FE_FreeGridWindow(MWContext *context, XP_Bool save_history)
{
  LO_Element *e;
  History_entry *he;
  XP_List *hist_list;

  hist_list = NULL;
  he = NULL;
  if ((context)&&(context->is_grid_cell))
    {
      /* remove focus from this grid */
      CONTEXT_DATA (context)->focus_grid = False;

      /*
       * If we are going to save the history of this grid cell
       * we need to stuff the last scroll position into the
       * history structure, and then remove that structure
       * from its linked list so it won't be freed when the
       * context is destroyed.
       */
      if (save_history)
        {
          e = LO_XYToNearestElement (context,
                                     CONTEXT_DATA (context)->document_x,
                                     CONTEXT_DATA (context)->document_y,
                                     NULL);
          he = SHIST_GetCurrent (&context->hist);
          if (e)
            SHIST_SetPositionOfCurrentDoc (&context->hist, e->lo_any.ele_id);

	  hist_list = context->hist.list_ptr;
	  context->hist.list_ptr = NULL;
        }

      fe_DestroyContext(context);
    }
  return(hist_list);
}

void XFE_GetTextFrame(MWContext *context, LO_TextStruct *text, int32 start,
                      int32 end, XP_Rect *frame)
{
    LO_TextAttr * attr = text->text_attr;
    fe_Font font;
    int L, remaining, width, height, ascent, descent;
    char *str;
    XCharStruct overall;

    font = fe_LoadFontFromFace (context, attr, &attr->charset,
                                attr->font_face, attr->size, attr->fontmask);
    frame->left = text->x + text->x_offset;
    frame->top = text->y + text->y_offset;

  /* X is such a winner, it uses 16 bit quantities to represent all pixel
     widths.  This is really swell, because it means that if you've got
     a large font, you can't correctly compute the size of strings which
     are only a few thousand characters long.  So, when the string is more
     than N characters long, we divide up our calls to XTextExtents to
     keep the size down so that the library doesn't run out of fingers
     and toes.
   */
#define SUCKY_X_MAX_LENGTH 600

    XP_ASSERT(font);              /* Should really return FALSE on failure. */

    str = (char *) text->text;
    remaining = start;
    width = 0;
    height = 0;
    do
        {
            L = (remaining > SUCKY_X_MAX_LENGTH ? SUCKY_X_MAX_LENGTH :
                 remaining);
            FE_TEXT_EXTENTS (attr->charset, font, str, L,
                             &ascent, &descent, &overall);
            width += overall.width;
            height = MAX(height, ascent + descent);
            str += L;
            remaining -= L;
        }
    while (remaining > 0);
    frame->left += width;

    str = (char *) text->text + start;
    remaining = end - start + 1;
    width = 0;
    do
        {
            L = (remaining > SUCKY_X_MAX_LENGTH ? SUCKY_X_MAX_LENGTH :
                 remaining);
            FE_TEXT_EXTENTS (attr->charset, font, str, L,
                             &ascent, &descent, &overall);
            width += overall.width;
            height = MAX(height, ascent + descent);
            str += L;
            remaining -= L;
        }
    while (remaining > 0);
    frame->right = frame->left + width;
    frame->bottom = frame->top + height;
}

/* Display a border.  x, y, width and height specify the outer perimeter of the
   border. */
void
XFE_DisplayBorder(MWContext *context, int iLocation, int x, int y, int width,
                  int height, int bw, LO_Color *color, LO_LineStyle style) 
{
    fe_Drawable *fe_drawable = CONTEXT_DATA(context)->drawable;
    Drawable drawable = fe_drawable->xdrawable;
    Widget widget = CONTEXT_WIDGET(context);
    Display *dpy = XtDisplay(widget);

    if (bw > 0) {
        GC gc;
        XGCValues gcv;
        unsigned long flags;

        memset (&gcv, ~0, sizeof (gcv));
        gcv.function = GXcopy;
        gcv.foreground = fe_GetPixel (context, color->red, color->green,
                                      color->blue);
        gcv.line_width = bw;
        flags = GCFunction | GCForeground | GCLineWidth;
        gc = fe_GetGCfromDW (dpy, drawable, flags, &gcv,
                             fe_drawable->clip_region);

        /* Add in the layer origin. */
        x += fe_drawable->x_origin - CONTEXT_DATA(context)->document_x;
        y += fe_drawable->y_origin - CONTEXT_DATA(context)->document_y;

        switch (style) {
        case LO_SOLID:
            /* Beware: XDrawRectangle centers the line-thickness on the
               coords. */
            XDrawRectangle (dpy, drawable, gc, x + (bw / 2), y + (bw / 2),
                            width - bw, height - bw);
            break;
            
        case LO_BEVEL:
            fe_DrawShadows (context, fe_drawable, x, y, width, height, bw,
                            XmSHADOW_IN);
            break;

        default:
            break;
        }
    }
}

static void
xfe_display_image_feedback(MWContext* context, LO_ImageStruct* lo_image)
{
	Display*     display;
    fe_Drawable* fe_drawable;
	Drawable     drawable;
	XGCValues    gcv;
	GC           gc;
	int          x;
	int          y;
	unsigned     w;
	unsigned     h;
	unsigned     bw;

#ifdef EDITOR
    /*
     *    Draw selection effects.
     */
	if (EDT_IS_EDITOR(context) && /* still only for editor in 4.0? */
		(lo_image->ele_attrmask & LO_ELE_SELECTED) != 0) {

		display = XtDisplay(CONTEXT_DATA(context)->drawing_area);
		fe_drawable = CONTEXT_DATA(context)->drawable;
		drawable = fe_drawable->xdrawable;

		memset(&gcv, ~0, sizeof (gcv));
		gcv.foreground = CONTEXT_DATA(context)->fg_pixel;
		gcv.background = CONTEXT_DATA(context)->select_bg_pixel;
		gcv.line_width = 1;
		gcv.line_style = LineDoubleDash;
		gc = fe_GetGCfromDW(display, drawable,
							GCForeground|GCBackground|GCLineStyle|GCLineWidth,
							&gcv,
							fe_drawable->clip_region);

		bw = lo_image->border_width;
		x = fe_drawable->x_origin - CONTEXT_DATA(context)->document_x +
			lo_image->x + lo_image->x_offset;

		y = fe_drawable->y_origin - CONTEXT_DATA(context)->document_y +
			lo_image->y + lo_image->y_offset;
		w = lo_image->width  + bw + bw;
		h = lo_image->height + bw + bw;

		/* beware: XDrawRectangle centers the line-thickness on the coords. */
		XDrawRectangle(display, drawable,
					   gc, x, y, (w - gcv.line_width), (h - gcv.line_width));
	}
#endif /*EDITOR*/

	/*
	 *    Tab navigation.
	 */
}

/* Display feedback about a layout element e.g. editor selection, tab navigation
   highlighting, etc. */
void
XFE_DisplayFeedback(MWContext *context, int iLocation, LO_Element *element)
{
	if (element->lo_any.type == LO_IMAGE ) {
		xfe_display_image_feedback(context, (LO_ImageStruct*)element);
	}

    /* XXX Implement me. */
}

#ifndef LAYERS_FULL_FE_EVENT

static fe_EventActivateKind calcActivateKind(const String* av,
					     const Cardinal* ac,
					     fe_MouseActionEnum mouse_action)
{
  if (mouse_action!=FE_ACTIVATE_LINK)
    return fe_EventActivateKindNone;

  if (!(av && ac))
    return fe_EventActivateKindNone;

  {
    int N=*ac;

    if (N>2)
      {
	fprintf (stderr, 
			 XP_GetString(XFE_LAY_TOO_MANY_ARGS_TO_ACTIVATE_LINK_ACTION),
		 fe_progname, *ac);
	return fe_EventActivateKindNone;
      }
      
    if (N==0)
      return fe_EventActivateKindNormal;

    if (N==1)
      {
	if (!strcmp ("new-window", av[0]))
	  return fe_EventActivateKindNewWindow;

	if (!strcmp ("save-only", av[0]))
	  return fe_EventActivateKindSaveOnly;
      }
  }

  fprintf (stderr,
		   XP_GetString(XFE_LAY_UNKNOWN_PARAMETER_TO_ACTIVATE_LINK_ACTION),
	   fe_progname, av[0]);
  return fe_EventActivateKindNone;
}

static String* invertActivateKind(fe_EventActivateKind kind)
{
  static String res;

  switch (kind)
    {
    case fe_EventActivateKindSaveOnly:
      res="save-only";
      return &res;

    case fe_EventActivateKindNewWindow:
      res="new-window";
      return &res;

    case fe_EventActivateKindNone:
    case fe_EventActivateKindNormal:
    default:
      return NULL;
    }
}

void fe_event_stuff(MWContext* context,
		    fe_EventStruct* fe_event,
		    const XEvent* event,
		    const String* av,
		    const Cardinal* ac,
		    fe_MouseActionEnum mouse_action)
{
  if (!fe_event)
    return;

  fe_event->mouse_action=mouse_action;

  if (event)
    {
      switch (event->type)
	{
	case KeyPress:
	case KeyRelease:
	  if (   (mouse_action==FE_KEY_UP)
	      || (mouse_action==FE_KEY_DOWN)
	      )
	    {
	      fe_event->compressedEvent.pos.win.x=event->xkey.x;
	      fe_event->compressedEvent.pos.win.y=event->xkey.y;
	      fe_event->compressedEvent.pos.root.x=event->xkey.x_root;
	      fe_event->compressedEvent.pos.root.y=event->xkey.y_root;
	      fe_event->compressedEvent.arg.key.state=event->xkey.state;
	      fe_event->compressedEvent.arg.key.keycode=event->xkey.keycode;
	    }
	  else
	    {
	      fe_event->compressedEvent.pos.win.x=
		fe_event->compressedEvent.pos.win.y=
		  fe_event->compressedEvent.pos.root.x=
		    fe_event->compressedEvent.pos.root.y=
		      0;
	    }
	  fe_event->compressedEvent.time=event->xkey.time;
	  break;

	case ButtonPress:
	case ButtonRelease:
	  fe_event->compressedEvent.pos.win.x=event->xbutton.x;
	  fe_event->compressedEvent.pos.win.y=event->xbutton.y;
	  fe_event->compressedEvent.pos.root.x=event->xbutton.x_root;
	  fe_event->compressedEvent.pos.root.y=event->xbutton.y_root;
	  fe_event->compressedEvent.arg.button.root=event->xbutton.root;
	  fe_event->compressedEvent.time=event->xbutton.time;
	  break;

	case MotionNotify:
	  fe_event->compressedEvent.pos.win.x=event->xmotion.x;
	  fe_event->compressedEvent.pos.win.y=event->xmotion.y;
	  fe_event->compressedEvent.pos.root.x=event->xmotion.x_root;
	  fe_event->compressedEvent.pos.root.y=event->xmotion.y_root;
	  fe_event->compressedEvent.time=0;
	  break;

	default:
#ifdef DEBUG
	  fprintf(stderr,
		  "fe_event_stuff(): unsupported XEvent type %d\n",
		  event->type);
#endif
	  fe_event->compressedEvent.pos.win.x=
	    fe_event->compressedEvent.pos.win.y=
	      fe_event->compressedEvent.pos.root.x=
		fe_event->compressedEvent.pos.root.y=
		  0;
	  fe_event->compressedEvent.time=0;
	  break;
	}
      fe_event->compressedEvent.type=event->type;
    }
  else
    {
      fe_event->compressedEvent.pos.win.x=
	fe_event->compressedEvent.pos.win.y=
	  fe_event->compressedEvent.pos.root.x=
	    fe_event->compressedEvent.pos.root.y=
	      0;
      fe_event->compressedEvent.time=0;
      fe_event->compressedEvent.type=0;
    }

  fe_event->activateKind=calcActivateKind(av,ac,mouse_action);
  fe_event->data=0;

  if (context)
    fe_CacheWindowOffset(context,
			 ( fe_event->compressedEvent.pos.root.x
			  -fe_event->compressedEvent.pos.win.x
			  ),
			 ( fe_event->compressedEvent.pos.root.y
			  -fe_event->compressedEvent.pos.win.y
			  )
			 );
}

XEvent* fe_event_extract(const fe_EventStruct* fe_event,
			 String** av,
			 Cardinal** ac,
			 fe_MouseActionEnum* mouse_action)
{
  static XEvent event;
  static Cardinal zero=0;

  event.xany.display=fe_display;

  if (!fe_event)
    {
      if (av)
	(*av)=0;
      if (ac)
	(*ac)=&zero;

      if (mouse_action)
	(*mouse_action)=FE_INVALID_MOUSE_ACTION;
      return NULL;
    }

  event.type=fe_event->compressedEvent.type;

  switch (event.type)
    {
    case KeyPress:
    case KeyRelease:
      if (   (fe_event->mouse_action==FE_KEY_UP)
	  || (fe_event->mouse_action==FE_KEY_DOWN)
	  )
	{
	  event.xkey.x=fe_event->compressedEvent.pos.win.x;
	  event.xkey.y=fe_event->compressedEvent.pos.win.y;
	  event.xkey.x_root=fe_event->compressedEvent.pos.root.x;
	  event.xkey.y_root=fe_event->compressedEvent.pos.root.y;
	  event.xkey.state=fe_event->compressedEvent.arg.key.state;
	  event.xkey.keycode=fe_event->compressedEvent.arg.key.keycode;
	}
      event.xkey.time=fe_event->compressedEvent.time;
      break;
      
    case ButtonPress:
    case ButtonRelease:
      event.xbutton.x=fe_event->compressedEvent.pos.win.x;
      event.xbutton.y=fe_event->compressedEvent.pos.win.y;
      event.xbutton.x_root=fe_event->compressedEvent.pos.root.x;
      event.xbutton.y_root=fe_event->compressedEvent.pos.root.y;
      event.xbutton.root=fe_event->compressedEvent.arg.button.root;
      event.xbutton.time=fe_event->compressedEvent.time;
      break;
      
    case MotionNotify:
      event.xmotion.x=fe_event->compressedEvent.pos.win.x;
      event.xmotion.y=fe_event->compressedEvent.pos.win.y;
      event.xmotion.x_root=fe_event->compressedEvent.pos.root.x;
      event.xmotion.y_root=fe_event->compressedEvent.pos.root.y;
      break;
      
    default:
#ifdef DEBUG
      fprintf(stderr,
	      "fe_event_extract(): unsupported XEvent type %d\n",
	      event.type);
      event.type=0;
#endif
      break;
    }

  if (av)
    (*av)=invertActivateKind(fe_event->activateKind);

  if (ac)
    {
      if (av && (*av))
	{
	  static Cardinal one=1;
	  (*ac)=&one;
	}
      else
	(*ac)=&zero;
    }

  if (mouse_action)
    (*mouse_action)=fe_event->mouse_action;

  return &event;
}
#endif

extern void plonk_cancel(void);

void fe_PrimarySelectionFetchURL(MWContext * context)
{
	XP_ASSERT( context != NULL );

	if (!context ||
		LO_HaveSelection(context) ||
		(context->type != MWContextBrowser) ||
		!CONTEXT_WIDGET(context))
	{
		return;
	}

	XtGetSelectionValue(CONTEXT_WIDGET(context),
						XA_PRIMARY, 
						XA_STRING,
						fe_get_url_x_selection_cb,
						(XtPointer) context,
						CurrentTime);
}

static void
fe_get_url_x_selection_cb(Widget			w,
						  XtPointer			client_data,
						  Atom *			sel, 
						  Atom *			type, 
						  XtPointer			value, 
						  unsigned long *	len, 
						  int *				format)
{
	MWContext * context = (MWContext *) client_data;
	MWContext * top_context;

	if (!context)
	{
		return;
	}

	/* Load URL on the top most frame */
	top_context = XP_GetNonGridContext(context);

	if (!top_context)
	{
		return;
	}

	if (len && *len && value)
	{
		if (*type == XA_STRING)
		{
			String			str = (String) value;
			URL_Struct *	url = NET_CreateURLStruct(str,NET_DONT_RELOAD);

			/* hack to cancel initial sploosh-screen loader timeout */
			plonk_cancel();

			fe_GetURL(top_context,url,(url == NULL));
		}
	}
}

