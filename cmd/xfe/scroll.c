/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   scroll.c --- managing the scrolled area
   Created: Jamie Zawinski <jwz@netscape.com>, 23-Jul-94.
 */


#include "mozilla.h"
#include "xfe.h"
#include "scroller.h"
#include "scrollerP.h"
#include "new_manage.h"
#include "new_manageP.h"

/* for XP_GetString() */
#include <xpgetstr.h>

extern int XFE_SCROLL_WINDOW_GRAVITY_WARNING;

/* C entry point to XFE_HTMLDrag class in  xfe/src/HTMLDrag.cpp */
extern void XFE_HTMLDragCreate(Widget,MWContext*);

#ifdef EDITOR
#include "xeditor.h"
extern void fe_EditorReload(MWContext* context, Boolean super_reload);
#endif /*EDITOR*/

/* Layering support - LO_RefreshArea is called through compositor */
#include "layers.h"

void fe_find_scrollbar_sizes(MWContext *context); /* in xfe.h */

static void fe_expose_eh (Widget, XtPointer, XEvent *);
/* static void fe_expose_cb (Widget, XtPointer, XtPointer); */
static void fe_scroll_cb (Widget, XtPointer, XtPointer);

#ifdef RESIZE_CALLBACK_WORKS
static void fe_resize_cb (Widget, XtPointer, XtPointer);
#else
static void fe_scroller_resize (Widget, XtPointer);
#endif

static void guffaw (MWContext *context);
static void unguffaw (MWContext *context);

void
fe_InitScrolling (MWContext *context)
{
  if (CONTEXT_DATA (context)->drawing_area == NULL) return;
  /*
   * Guffaws has 3 states now.  If 0 it is just off.  If 1 it uses
   * static gravity, if 2 it uses a combination of N, S, E, W gravities.
   * Any method that uses gravity needs the configure handler.  Only
   * static uses the guffaw function.
   */
  if (fe_globalData.fe_guffaw_scroll > 0)
    {
      XtAddEventHandler (CONTEXT_DATA (context)->drawing_area,
			StructureNotifyMask, FALSE,
			 (XtEventHandler)fe_config_eh, context);
    }
  if (fe_globalData.fe_guffaw_scroll == 1)
    {
      guffaw (context);
    }
  fe_NukeBackingStore (CONTEXT_DATA (context)->drawing_area);
}


/*
 * Reverse the actions of fe_InitScrolling().  For when a normal
 * document context is turned into a FRAME parent context.
 */
void
fe_DisableScrolling (MWContext *context)
{
  if (CONTEXT_DATA (context)->drawing_area == NULL) return;
  /*
   * Turn off guffaws for a context's drawing area.
   */
  if (fe_globalData.fe_guffaw_scroll > 0)
    {
      XtRemoveEventHandler (CONTEXT_DATA (context)->drawing_area,
			StructureNotifyMask, FALSE,
			 (XtEventHandler)fe_config_eh, context);
    }
  if (fe_globalData.fe_guffaw_scroll == 1)
    {
      unguffaw (context);
    }
}


Widget
fe_MakeScrolledWindow (MWContext *context, Widget parent, const char *name)
{
  Widget scroller, drawing_area, hscroll, vscroll;
  Arg av [20];
  int ac = 0;

  ac = 0;
  XtSetArg (av[ac], XmNscrollingPolicy, XmAPPLICATION_DEFINED); ac++;
  /* The background of this widget is the color that shows up in the square
     at the bottom right where the horizontal and vertical scrollbars come
     together, and of the "pad" border between the scrollbars and the document
     area (which we'd like to be of size 0, see below.)  This should be in the
     background color of the widgets, not the background color of the document.
   */
  XtSetArg (av[ac], XmNbackground, parent->core.background_pixel); ac++;
/*  scroller = XmCreateScrolledWindow (parent, name, av, ac);*/
  scroller = XtCreateWidget((char *) name, scrollerClass, parent, av, ac);
#ifndef RESIZE_CALLBACK_WORKS
  ((Scroller) scroller)->scroller.resize_arg = (void *) context;
  ((Scroller) scroller)->scroller.resize_hook = fe_scroller_resize;
#endif

  /* This is a kludge - there ought to be a resource to control this, but
     I can't find it.  This controls the extra space between the scrollbars
     and the area being scrolled.  If this isn't 0, then there's a 4-pixel
     border between the document background and the scrollbar, which ends
     up in the user's background instead, which looks stupid.
   */
  ((Scroller) scroller)->swindow.pad = 0;

  ac = 0;
  XtSetArg (av[ac], XmNminimum, 0); ac++;
  XtSetArg (av[ac], XmNmaximum, 1); ac++;
  XtSetArg (av[ac], XmNorientation, XmHORIZONTAL); ac++;
  hscroll = XmCreateScrollBar (scroller, "hscroll", av, ac);
  ac = 0;
  XtSetArg (av[ac], XmNminimum, 0); ac++;
  XtSetArg (av[ac], XmNmaximum, 1); ac++;
  XtSetArg (av[ac], XmNorientation, XmVERTICAL); ac++;
  vscroll = XmCreateScrollBar (scroller, "vscroll", av, ac);
  ac = 0;

  /* The drawing area we're about to create is the actual document window,
     so its background should be the document background pixel. */
  XtSetArg (av[ac], XmNbackground,
	    CONTEXT_DATA (context)->default_bg_pixel); ac++;
/*
  XtSetArg (av[ac], XmNautoUnmanage, FALSE); ac++;
  XtSetArg (av[ac], XmNdefaultPosition, FALSE); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_WORK_AREA); ac++;
  XtSetArg (av[ac], XmNmarginHeight, 0); ac++;
  XtSetArg (av[ac], XmNmarginWidth, 0); ac++;
*/
#ifdef EDITOR
		if (EDT_IS_EDITOR(context)) {
			/* Let's try using the managed widget */
			drawing_area = XtCreateWidget("editorDrawingArea", newManageClass,
										  scroller, av, ac);
		}
  else
#endif /* EDITOR */
    drawing_area = XtCreateWidget("drawingArea", newManageClass, scroller, av, ac);

/*
  drawing_area = XtCreateWidget("drawingArea", xmManagerWidgetClass, scroller, av, ac);
  drawing_area = XtCreateWidget("drawingArea", xmBulletinBoardWidgetClass, scroller, av, ac);
*/
/*  XtAddCallback (drawing_area, XmNexposeCallback, fe_expose_cb, context); */
  XtAddEventHandler(drawing_area, ExposureMask, FALSE,
	(XtEventHandler)fe_expose_eh, context);
/* This is now added later in xfe.c, because we don't know fe_guffaw_scroll
 * yet.
  if (fe_globalData.fe_guffaw_scroll)
  {
    XtAddEventHandler(drawing_area, StructureNotifyMask, FALSE,
	(XtEventHandler)fe_config_eh, context);
  }
*/
#ifdef RESIZE_CALLBACK_WORKS
  XtAddCallback (drawing_area, XmNresizeCallback, fe_resize_cb, context);
#endif

  XtManageChild (drawing_area);
  SCROLLER_SET_AREAS (scroller, hscroll, vscroll, drawing_area);

  XtAddCallback (hscroll, XmNvalueChangedCallback, fe_scroll_cb, context);
  XtAddCallback (vscroll, XmNvalueChangedCallback, fe_scroll_cb, context);
  XtAddCallback (hscroll, XmNdragCallback, fe_scroll_cb, context);
  XtAddCallback (vscroll, XmNdragCallback, fe_scroll_cb, context);

  CONTEXT_DATA (context)->scrolled = scroller;
  CONTEXT_DATA (context)->drawing_area = drawing_area;
  CONTEXT_DATA (context)->hscroll = hscroll;
  CONTEXT_DATA (context)->vscroll = vscroll;

  XtManageChild (hscroll);
  XtManageChild (vscroll);

  /* add drag source via C API */
  XFE_HTMLDragCreate(drawing_area,context);
  
  return scroller;
}

static void
guffaw (MWContext *context)
{
  Widget widget = CONTEXT_DATA (context)->drawing_area;
  XSetWindowAttributes attr;
  unsigned long valuemask;
  valuemask = CWBitGravity | CWWinGravity;
  attr.win_gravity = StaticGravity;
  attr.bit_gravity = StaticGravity;
  if (! XtWindow (widget)) abort ();
  XChangeWindowAttributes (XtDisplay (widget), XtWindow (widget),
			   valuemask, &attr);
}

static void
unguffaw (MWContext *context)
{
  Widget widget = CONTEXT_DATA (context)->drawing_area;
  XSetWindowAttributes attr;
  unsigned long valuemask;
  valuemask = CWBitGravity | CWWinGravity;
  attr.win_gravity = NorthWestGravity;
  attr.bit_gravity = NorthWestGravity;
  if (! XtWindow (widget)) abort ();
  XChangeWindowAttributes (XtDisplay (widget), XtWindow (widget),
			   valuemask, &attr);
}

/*
 * Toggle on and off the bit and window gravity effects
 * of guffaws scrolling.
 */
void
fe_SetGuffaw(MWContext *context, Boolean on)
{
  if (fe_globalData.fe_guffaw_scroll == 1)
    {
      if (on)
        guffaw (context);
      else
        unguffaw (context);
    }
}

static void 
fe_scroll_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmScrollBarCallbackStruct *cb = (XmScrollBarCallbackStruct *) call_data;
  Widget da = CONTEXT_DATA (context)->drawing_area;
  int scroll_slop_hack =
    ((Scroller) CONTEXT_DATA (context)->scrolled)->swindow.pad;
  unsigned long lh = CONTEXT_DATA (context)->line_height;
  unsigned long old_x = CONTEXT_DATA (context)->document_x;
  unsigned long old_y = CONTEXT_DATA (context)->document_y;
  unsigned long new_x = old_x;
  unsigned long new_y = old_y;
  Dimension w = 0, h = 0;
  Boolean horiz_p;
  XGCValues gcv;
  GC gc;

  fe_UserActivity (context);
  if (widget == CONTEXT_DATA (context)->hscroll)
    new_x = cb->value;
  else if (widget == CONTEXT_DATA (context)->vscroll)
    new_y = cb->value;
  else
    abort ();
  horiz_p = (new_x != old_x);

  fe_SetDocPosition (context, new_x, new_y);

/*  XtVaGetValues (da, XmNwidth, &w, XmNheight, &h, 0); */
  w = da->core.width;
  h = da->core.height;

  memset (&gcv, ~0, sizeof (gcv));
  gcv.function = GXcopy;
  gc = fe_GetGC (da, GCFunction, &gcv);

  if (horiz_p)
    {
      if (new_x >= old_x)
	{
	  int shift = (new_x - old_x);
	  int sb_w = CONTEXT_DATA (context)->sb_w;
	  if (fe_globalData.fe_guffaw_scroll == 1)
	    {
	      int x, y;
	      unsigned int w, h, d1, d2;
	      Window dummy;

	      XGetGeometry(XtDisplay(da), XtWindow(da), &dummy,
			   &x, &y, &w, &h, &d1, &d2);
	      XResizeWindow(XtDisplay(da), XtWindow(da),
			    (w + shift + scroll_slop_hack), h);
	      XMoveWindow(XtDisplay(da), XtWindow(da), (x - shift), y);
	      XMoveResizeWindow(XtDisplay(da), XtWindow(da), x, y, w, h);
	      fe_GravityCorrectForms (context, -shift, 0);
	    }
	  else if (fe_globalData.fe_guffaw_scroll == 2)
	    {
	      int x, y;
	      unsigned int w, h, d1, d2;
	      Window dummy;

	      XGetGeometry(XtDisplay(da), XtWindow(da), &dummy,
			   &x, &y, &w, &h, &d1, &d2);
	      fe_SetFormsGravity(context, WestGravity);
	      XResizeWindow(XtDisplay(da), XtWindow(da),
			    (w + shift), h);
	      XMoveWindow(XtDisplay(da), XtWindow(da), (x - shift), y);
	      fe_SetFormsGravity(context, EastGravity);
	      XMoveResizeWindow(XtDisplay(da), XtWindow(da), x, y, w, h);
	      fe_SetFormsGravity(context, NorthWestGravity);
	      fe_GravityCorrectForms (context, -shift, 0);
	    }
	  else
	    {
	      XCopyArea (XtDisplay (da), XtWindow (da), XtWindow (da), gc,
			 shift, 0,
			 w - shift, h,
			 0, 0);
	      fe_ScrollForms (context, old_x - new_x, old_y - new_y);
	      fe_RefreshArea (context,
                              new_x + w - (shift+lh+lh),
                              new_y, shift+lh, h);
	    }
	  if (shift > w)
	    {
	      shift = w;
	    }

#ifdef DONT_rhess
	  /*
	   *  NOTE:  we always need to refresh the delta of the scrolled area...
	   *         [ bug:  improper redisplay during scrolling ]
	   */

	  if (fe_globalData.fe_guffaw_scroll == 0)
#endif
	    fe_RefreshArea (context,
                            new_x + w - sb_w - shift,
                            new_y, shift + sb_w, h);
	}
      else
	{
	  int shift = (old_x - new_x);
	  if (fe_globalData.fe_guffaw_scroll == 1)
	    {
	      int x, y;
	      unsigned int w, h, d1, d2;
	      Window dummy;

	      XGetGeometry(XtDisplay(da), XtWindow(da), &dummy,
			   &x, &y, &w, &h, &d1, &d2);
	      XMoveResizeWindow(XtDisplay(da), XtWindow(da),
				(x - shift), y, (w + shift), h);
	      XResizeWindow(XtDisplay(da), XtWindow(da), w, h);
	      XMoveWindow(XtDisplay(da), XtWindow(da), x, y);
	      fe_GravityCorrectForms (context, shift, 0);
	    }
	  else if (fe_globalData.fe_guffaw_scroll == 2)
	    {
	      int x, y;
	      unsigned int w, h, d1, d2;
	      Window dummy;

	      XGetGeometry(XtDisplay(da), XtWindow(da), &dummy,
			   &x, &y, &w, &h, &d1, &d2);
	      fe_SetFormsGravity(context, EastGravity);
	      XMoveResizeWindow(XtDisplay(da), XtWindow(da),
				(x - shift), y, (w + shift), h);
	      fe_SetFormsGravity(context, WestGravity);
	      XResizeWindow(XtDisplay(da), XtWindow(da), w, h);
	      XMoveWindow(XtDisplay(da), XtWindow(da), x, y);
	      fe_SetFormsGravity(context, NorthWestGravity);
	      fe_GravityCorrectForms (context, shift, 0);
	    }
	  else
	    {
	      XCopyArea (XtDisplay (da), XtWindow (da), XtWindow (da), gc,
			 0, 0,
			 w - (old_x - new_x), h,
			 (old_x - new_x), 0);
	      fe_ScrollForms (context, old_x - new_x, old_y - new_y);
	    }
	  if (shift > w)
	    {
	      shift = w;
	    }

#ifdef DONT_rhess
	  if (fe_globalData.fe_guffaw_scroll == 0)
#endif
	    fe_RefreshArea (context, new_x, new_y, shift, h);
	}
    }
  else
    {
      if (new_y >= old_y)
	{
	  int shift = (new_y - old_y);
	  int sb_h = CONTEXT_DATA (context)->sb_h;
	  if (fe_globalData.fe_guffaw_scroll == 1)
	    {
	      int x, y;
	      unsigned int w, h, d1, d2;
	      Window dummy;

	      XGetGeometry(XtDisplay(da), XtWindow(da), &dummy,
			   &x, &y, &w, &h, &d1, &d2);
	      XResizeWindow(XtDisplay(da), XtWindow(da), w,
			    (h + shift + scroll_slop_hack));
	      XMoveWindow(XtDisplay(da), XtWindow(da), x, (y - shift));
	      XMoveResizeWindow(XtDisplay(da), XtWindow(da), x, y, w, h);
	      fe_GravityCorrectForms (context, 0, -shift);
	    }
	  else if (fe_globalData.fe_guffaw_scroll == 2)
	    {
	      int x, y;
	      unsigned int w, h, d1, d2;
	      Window dummy;

	      XGetGeometry(XtDisplay(da), XtWindow(da), &dummy,
			   &x, &y, &w, &h, &d1, &d2);
	      fe_SetFormsGravity(context, NorthGravity);
	      XResizeWindow(XtDisplay(da), XtWindow(da), w,
			    (h + shift));
	      XMoveWindow(XtDisplay(da), XtWindow(da), x, (y - shift));
	      fe_SetFormsGravity(context, SouthGravity);
	      XMoveResizeWindow(XtDisplay(da), XtWindow(da), x, y, w, h);
	      fe_SetFormsGravity(context, NorthWestGravity);
	      fe_GravityCorrectForms (context, 0, -shift);
	    }
	  else
	    {
	      XCopyArea (XtDisplay (da), XtWindow (da), XtWindow (da), gc,
			 0, shift,
			 w, h - shift,
			 0, 0);
	      fe_ScrollForms (context, old_x - new_x, old_y - new_y);
	    }
	  if (shift > h)
	    {
	      shift = h;
	    }

#ifdef DONT_rhess
	  if (fe_globalData.fe_guffaw_scroll == 0)
#endif
	    fe_RefreshArea (context, new_x, new_y + h - sb_h - shift,
                            w, shift + sb_h);
	}
      else
	{
	  int shift = (old_y - new_y);
	  if (fe_globalData.fe_guffaw_scroll == 1)
	    {
	      int x, y;
	      unsigned int w, h, d1, d2;
	      Window dummy;

	      XGetGeometry(XtDisplay(da), XtWindow(da), &dummy,
			   &x, &y, &w, &h, &d1, &d2);
	      XMoveResizeWindow(XtDisplay(da), XtWindow(da),
				x, (y - shift), w, (h + shift));
	      XResizeWindow(XtDisplay(da), XtWindow(da), w, h);
	      XMoveWindow(XtDisplay(da), XtWindow(da), x, y);
	      fe_GravityCorrectForms (context, 0, shift);
	    }
	  else if (fe_globalData.fe_guffaw_scroll == 2)
	    {
	      int x, y;
	      unsigned int w, h, d1, d2;
	      Window dummy;

	      XGetGeometry(XtDisplay(da), XtWindow(da), &dummy,
			   &x, &y, &w, &h, &d1, &d2);
	      fe_SetFormsGravity(context, SouthGravity);
	      XMoveResizeWindow(XtDisplay(da), XtWindow(da),
				x, (y - shift), w, (h + shift));
	      fe_SetFormsGravity(context, NorthGravity);
	      XResizeWindow(XtDisplay(da), XtWindow(da), w, h);
	      XMoveWindow(XtDisplay(da), XtWindow(da), x, y);
	      fe_SetFormsGravity(context, NorthWestGravity);
	      fe_GravityCorrectForms (context, 0, shift);
	    }
	  else
	    {
	      XCopyArea (XtDisplay (da), XtWindow (da), XtWindow (da), gc,
			 0, 0,
			 w, h - (old_y - new_y),
			 0, (old_y - new_y));
	      fe_ScrollForms (context, old_x - new_x, old_y - new_y);
	    }
	  if (shift > h)
	    {
	      shift = h;
	    }

#ifdef DONT_rhess
	  if (fe_globalData.fe_guffaw_scroll == 0)
#endif
	    fe_RefreshArea (context, new_x, new_y, w, shift);
	}
    }

#ifdef PERFECT_SCROLL
  /*
   * In any X scrolling, there is the possibility of getting multiple
   * scrolling events before any of the exposure events they
   * generate appear.  The results in improperly placed
   * exposures because the values fo document_x and document_y in the
   * context are different at the time the exposure is processed than 
   * they were at the time the exposure was created.
   * The followind code elminates this.  It first flushes all exposures
   * generated by this scroll event, and then removes all motion events
   * That might potentially cause another scroll to get processed before
   * these exposures.  Throwing out these extra motions in theory makes
   * scrolling a little more jerky.  In practice I notice no difference.
   */
  XSync(XtDisplay(widget), FALSE);
  if (cb->event)
    {
      XEvent new_event;
      while (XCheckTypedWindowEvent(XtDisplay(widget), XtWindow(widget),
				    MotionNotify, &new_event) == TRUE);
    }
#endif /* PERFECT_SCROLL */
}

void
fe_ScrollTo (MWContext *context, unsigned long x, unsigned long y)
{
  if (x != CONTEXT_DATA (context)->document_x)
    {
      XmScrollBarCallbackStruct cb;
      {
	int nx = x;
	int max = 0;
	int size = 0;
	XtVaGetValues (CONTEXT_DATA (context)->hscroll,
		       XmNmaximum, &max, XmNsliderSize, &size, 0);
	if (nx > max - size) nx = max - size;
	if (nx < 0) nx = 0;
	XtVaSetValues (CONTEXT_DATA (context)->hscroll, XmNvalue, nx, 0);
	x = nx;
      }
      memset (&cb, 0, sizeof (cb));
      cb.value = x;
      fe_scroll_cb (CONTEXT_DATA (context)->hscroll, 
                    (XtPointer)context, (XtPointer)&cb);
    }
  if (y != CONTEXT_DATA (context)->document_y)
    {
      XmScrollBarCallbackStruct cb;
      {
	int ny = y;
	int max = 0;
	int size = 0;
	XtVaGetValues (CONTEXT_DATA (context)->vscroll,
		       XmNmaximum, &max, XmNsliderSize, &size, 0);
	if (ny > max - size) ny = max - size;
	if (ny < 0) ny = 0;
	XtVaSetValues (CONTEXT_DATA (context)->vscroll, XmNvalue, ny, 0);
	y = ny;
      }
      memset (&cb, 0, sizeof (cb));
      cb.value = y;
      fe_scroll_cb (CONTEXT_DATA (context)->vscroll,
                    (XtPointer)context, (XtPointer)&cb);
    }
}


/*
 * When scrolling by moving the window, some expose events are generated
 * when the window is NOT at 0,0 which need to be translated to 0,0
 * when those exposes are processed.  We can locate these exposes because the
 * will have the same serial number as the configure that created them.
 * Thus we need to save that serial number and offset here.
 */
void
fe_config_eh (Widget widget, XtPointer closure, XEvent *event)
{
  MWContext *context = (MWContext *) closure;
  XConfigureEvent *ce = (XConfigureEvent *) event;

  if (ce->type != ConfigureNotify)
  {
    return;
  }

  CONTEXT_DATA (context)->expose_x_offset = ce->x;
  CONTEXT_DATA (context)->expose_y_offset = ce->y;
  CONTEXT_DATA (context)->expose_serial = ce->serial;
}


static void
fe_expose_eh (Widget widget, XtPointer closure, XEvent *event)
{
  MWContext *context = (MWContext *) closure;
  XExposeEvent *ee = (XExposeEvent *) event;
  int scroll_slop_hack =
    ((Scroller) CONTEXT_DATA (context)->scrolled)->swindow.pad;

  if (fe_globalData.fe_guffaw_scroll)
    {
      int new_x, new_y;
      unsigned int new_width, new_height;

      new_x = ee->x;
      new_y = ee->y;
      new_width = ee->width;
      new_height = ee->height;

      /*
       * If this expose event has the same serial number as a configure
       * event that moved the window away from 0,0 we need to adjust the x,y of
       * this expose event to be in the proper location with respect to
       * its parent.
       */
      if ((CONTEXT_DATA (context)->expose_x_offset != 0)||
	  (CONTEXT_DATA (context)->expose_y_offset != 0))
	{
	  if (ee->serial == CONTEXT_DATA (context)->expose_serial)
	    {
	      new_x += CONTEXT_DATA (context)->expose_x_offset;
	      new_y += CONTEXT_DATA (context)->expose_y_offset;

	      /*
	       * Correct for the presence of a horizontal or
	       * vertical scrollbar.
	       */
              if (CONTEXT_DATA (context)->expose_y_offset < 0)
                {
                        new_y -= scroll_slop_hack;
                        new_height += scroll_slop_hack;
                }
              if (CONTEXT_DATA (context)->expose_x_offset < 0)
                {
                        new_x -= scroll_slop_hack;
                        new_width += scroll_slop_hack;
                }
	    }
	}

      /*
       * Because we are scrolling a window that may have many small child
       * widgets, we want to compress the many small exposes that happen around
       * those widgets.  We don't want to compress discontinuous regions
       * that happened to be exposed at the same time.
       * We want to compress:
       *
       *         -----                  ---------
       *         |   |------    and     |       |
       *         |   ||    |            ---------
       *         -----|    |                ---------
       *              ------                |       |
       *                                    ---------
       *
       *                    NOT
       *
       *           -----
       *           |   |
       *           -----
       *
       *                   -----
       *                   |   |
       *                   -----
       */

      /*
       * Non-zero counts are parts of multi-expose blocks, and might be
       * held for later compression.
       */
      if (ee->count != 0)
	{
	  /*
	   * If nothng held so far, just hold this one.
	   */
	  if (CONTEXT_DATA (context)->held_expose == FALSE)
	    {
	      CONTEXT_DATA (context)->held_expose = TRUE;
	      CONTEXT_DATA (context)->expose_x1 = new_x;
	      CONTEXT_DATA (context)->expose_y1 = new_y;
	      CONTEXT_DATA (context)->expose_x2 = new_x + new_width;
	      CONTEXT_DATA (context)->expose_y2 = new_y + new_height;
	    }
	  else
	    {
	      /*
	       * If we are adjacent to the held block, compress them together
	       */
	      if ((((new_y - 1) >= CONTEXT_DATA (context)->expose_y1)&&
		   ((new_y - 1) <= CONTEXT_DATA (context)->expose_y2))||
		  (((new_y + new_height + 1) >= CONTEXT_DATA (context)->expose_y1)&&
		   ((new_y + new_height + 1) <= CONTEXT_DATA (context)->expose_y2))||
		  ((CONTEXT_DATA (context)->expose_y1 >= (new_y - 1))&&
		   (CONTEXT_DATA (context)->expose_y1 <= (new_y + new_height + 1))))
		{
		  if (new_x < CONTEXT_DATA (context)->expose_x1)
		    CONTEXT_DATA (context)->expose_x1 = new_x;
		  if (new_y < CONTEXT_DATA (context)->expose_y1)
		    CONTEXT_DATA (context)->expose_y1 = new_y;
		  if ((new_x + (int)new_width) > CONTEXT_DATA (context)->expose_x2)
		    CONTEXT_DATA (context)->expose_x2 = new_x + (int)new_width;
		  if ((new_y + (int)new_height) > CONTEXT_DATA (context)->expose_y2)
		    CONTEXT_DATA (context)->expose_y2 = new_y + (int)new_height;
		}
	      /*
	       * We weren't adjacent.  Display the held block, and hold
	       * the new one.
	       */
	      else
		{
		  unsigned int hold_width, hold_height;

		  if (CONTEXT_DATA (context)->expose_x2
		      > CONTEXT_DATA (context)->scrolled_width)
		    CONTEXT_DATA (context)->expose_x2
		      = CONTEXT_DATA (context)->scrolled_width;
		  if (CONTEXT_DATA (context)->expose_y2
		      > CONTEXT_DATA (context)->scrolled_height)
		    CONTEXT_DATA (context)->expose_y2
		      = CONTEXT_DATA (context)->scrolled_height;
		  hold_width = (CONTEXT_DATA (context)->expose_x2 -
				CONTEXT_DATA (context)->expose_x1);
		  hold_height = (CONTEXT_DATA (context)->expose_y2 -
				 CONTEXT_DATA (context)->expose_y1);
		  if (hold_width < 1)
		    hold_width = 1;
		  if (hold_height < 1)
		    hold_height = 1;


		  fe_RefreshArea (context,
                                  (CONTEXT_DATA (context)->expose_x1 +
                                   CONTEXT_DATA (context)->document_x),
                                  (CONTEXT_DATA (context)->expose_y1 +
                                   CONTEXT_DATA (context)->document_y),
                                  hold_width, hold_height);
		  CONTEXT_DATA (context)->expose_x1 = new_x;
		  CONTEXT_DATA (context)->expose_y1 = new_y;
		  CONTEXT_DATA (context)->expose_x2 = new_x + new_width;
		  CONTEXT_DATA (context)->expose_y2 = new_y + new_height;
		}
	    }
	}
      /*
       * count = 0 is either a standalone expose, or part of a multipart
       * expose.
       */
      else
	{
	  /*
	   * If nothing held this is just a standalone expose, display it.
	   */
	  if (CONTEXT_DATA (context)->held_expose == FALSE)
	    {
	      if ((new_x + new_width) > CONTEXT_DATA (context)->scrolled_width)
		new_width = CONTEXT_DATA (context)->scrolled_width - new_x;
	      if ((new_y + new_height) > CONTEXT_DATA (context)->scrolled_height)
		new_height = CONTEXT_DATA (context)->scrolled_height - new_y;
	      if (new_width < 1)
		new_width = 1;
	      if (new_height < 1)
		new_height = 1;

	      fe_RefreshArea (context,
                              new_x + CONTEXT_DATA (context)->document_x,
                              new_y + CONTEXT_DATA (context)->document_y,
                              new_width, new_height);
	    }
	  else
	    {
	      CONTEXT_DATA (context)->held_expose = FALSE;
	      /*
	       * If we are adjacent to the held block, compress them together
	       * and display the combined chunk.
	       */
	      if ((((new_y - 1) >= CONTEXT_DATA (context)->expose_y1)&&
		   ((new_y - 1) <= CONTEXT_DATA (context)->expose_y2))||
		  (((new_y + new_height + 1) >= CONTEXT_DATA (context)->expose_y1)&&
		   ((new_y + new_height + 1) <= CONTEXT_DATA (context)->expose_y2))||
		  ((CONTEXT_DATA (context)->expose_y1 >= (new_y - 1))&&
		   (CONTEXT_DATA (context)->expose_y1 <= (new_y + new_height + 1))))
		{
		  if (new_x < CONTEXT_DATA (context)->expose_x1)
		    CONTEXT_DATA (context)->expose_x1 = new_x;
		  if (new_y < CONTEXT_DATA (context)->expose_y1)
		    CONTEXT_DATA (context)->expose_y1 = new_y;
		  if ((new_x + new_width) > CONTEXT_DATA (context)->expose_x2)
		    CONTEXT_DATA (context)->expose_x2 = new_x + new_width;
		  if ((new_y + new_height) > CONTEXT_DATA (context)->expose_y2)
		    CONTEXT_DATA (context)->expose_y2 = new_y + new_height;

		  if (CONTEXT_DATA (context)->expose_x2
		      > CONTEXT_DATA (context)->scrolled_width)
		    CONTEXT_DATA (context)->expose_x2
		      = CONTEXT_DATA (context)->scrolled_width;
		  if (CONTEXT_DATA (context)->expose_y2
		      > CONTEXT_DATA (context)->scrolled_height)
		    CONTEXT_DATA (context)->expose_y2
		      = CONTEXT_DATA (context)->scrolled_height;
		  new_width = (CONTEXT_DATA (context)->expose_x2 -
			       CONTEXT_DATA (context)->expose_x1);
		  new_height = (CONTEXT_DATA (context)->expose_y2 -
				CONTEXT_DATA (context)->expose_y1);
		  if (new_width < 1)
		    new_width = 1;
		  if (new_height < 1)
		    new_height = 1;

		  fe_RefreshArea (context,
                                  (CONTEXT_DATA (context)->expose_x1 +
                                   CONTEXT_DATA (context)->document_x),
                                  (CONTEXT_DATA (context)->expose_y1 +
                                   CONTEXT_DATA (context)->document_y),
                                  new_width, new_height);
		}
	      /*
	       * Else display the held block and this expose separately.
	       */
	      else
		{
		  unsigned int hold_width, hold_height;

		  if (CONTEXT_DATA (context)->expose_x2
		      > CONTEXT_DATA (context)->scrolled_width)
		    CONTEXT_DATA (context)->expose_x2
		      = CONTEXT_DATA (context)->scrolled_width;
		  if (CONTEXT_DATA (context)->expose_y2
		      > CONTEXT_DATA (context)->scrolled_height)
		    CONTEXT_DATA (context)->expose_y2
		      = CONTEXT_DATA (context)->scrolled_height;
		  hold_width = (CONTEXT_DATA (context)->expose_x2 -
				CONTEXT_DATA (context)->expose_x1);
		  hold_height = (CONTEXT_DATA (context)->expose_y2 -
				 CONTEXT_DATA (context)->expose_y1);
		  if (hold_width < 1)
		    hold_width = 1;
		  if (hold_height < 1)
		    hold_height = 1;

		  fe_RefreshArea (context,
                                  (CONTEXT_DATA (context)->expose_x1 +
                                   CONTEXT_DATA (context)->document_x),
                                  (CONTEXT_DATA (context)->expose_y1 +
                                   CONTEXT_DATA (context)->document_y),
                                  hold_width, hold_height);

		  if ((new_x + new_width) > CONTEXT_DATA (context)->scrolled_width)
		    new_width = CONTEXT_DATA (context)->scrolled_width - new_x;
		  if ((new_y + new_height) > CONTEXT_DATA (context)->scrolled_height)
		    new_height = CONTEXT_DATA (context)->scrolled_height - new_y;
		  if (new_width < 1)
		    new_width = 1;
		  if (new_height < 1)
		    new_height = 1;

		  fe_RefreshArea (context,
                                  new_x + CONTEXT_DATA (context)->document_x,
                                  new_y + CONTEXT_DATA (context)->document_y,
                                  new_width, new_height);
		}
	    }
	}
    }
  else
    {
      fe_RefreshArea (context,
                      ee->x + CONTEXT_DATA (context)->document_x,
                      ee->y + CONTEXT_DATA (context)->document_y,
                      ee->width, ee->height);
    }
}

void
fe_SyncExposures(MWContext* context)
{
    XEvent   event;
    Widget   drawing_area = CONTEXT_DATA(context)->drawing_area;
    Display* display = XtDisplay(drawing_area);
    Window   window = XtWindow(drawing_area);
    
    XSync(display, FALSE);
    
    while (XCheckTypedWindowEvent(display, window, Expose, &event) == TRUE) {
        fe_expose_eh(drawing_area, (XtPointer)context, &event);
    }

    /*
     *     Force compositor to sync.
     */
    if (context->compositor)
	CL_CompositeNow(context->compositor);
}

static void
fe_hack_scrollbar (Widget sb, int max, int inc, int page_inc,
		   int slider_size, int value)
{
  if (value + slider_size > max)
    value = max - slider_size;
  if (value < 0)
    value = 0;
  if (inc < 1)
    inc = 1;
  if (page_inc < inc)
    page_inc = inc;
  XtVaSetValues (sb,
		 XmNmaximum, max,
		 XmNincrement, inc,
		 XmNpageIncrement, page_inc,
		 XmNsliderSize, slider_size,
		 XmNvalue, value,
		 0);
}


void
fe_SetDocPosition (MWContext *context, unsigned long x, unsigned long y)
{
  unsigned long w = CONTEXT_DATA (context)->document_width;
  unsigned long h = CONTEXT_DATA (context)->document_height;
  unsigned long lh = CONTEXT_DATA (context)->line_height;
  Dimension ww = 0, wh = 0;
  XP_Bool are_scrollbars_active;
  XP_Bool frame_scrolling_yes = False;

  if (x >= w + 100) x = 0;
  if (y >= h + lh)  y = ((h > lh) ? h - lh : 0);

  if (context->is_grid_cell &&
      CONTEXT_DATA (context)->grid_scrolling == LO_SCROLL_NO) {
    /* We're done */
    CONTEXT_DATA (context)->document_x = x;
    CONTEXT_DATA (context)->document_y = y;

    return;
  }

  XP_ASSERT (CONTEXT_DATA (context)->drawing_area);
  if (!CONTEXT_DATA (context)->drawing_area) return;

  XtVaGetValues (CONTEXT_DATA (context)->drawing_area,
		 XmNwidth, &ww, XmNheight, &wh, 0);

  /*
   * Fix for 4.0 bug 55350 (reported by ebina)
   *
   * <frame scrolling=yes> doesn't always have scrollbars.
   *
   * If grid_scrolling == LO_SCROLL_YES, then we force 
   * both scrollbars to be on regardless of whether they
   * are needed or not.
   *
   * -ramiro
   */
  if (context->is_grid_cell &&
      CONTEXT_DATA (context)->grid_scrolling == LO_SCROLL_YES)
  {
      frame_scrolling_yes = True;
  }

  are_scrollbars_active = CONTEXT_DATA(context)->are_scrollbars_active;

  if ( (h <= wh) || (are_scrollbars_active == FALSE) )
    {
      if (!frame_scrolling_yes)
      {
	y = 0;
	XtUnmanageChild (CONTEXT_DATA (context)->vscroll);
	fe_hack_scrollbar (CONTEXT_DATA (context)->vscroll,
			   1, 1, 1, 1, 0);
      }
    }
  else
    {
      fe_hack_scrollbar (CONTEXT_DATA (context)->vscroll,
			 h,		/* max */
			 lh,		/* inc */
			 wh - lh,	/* page_inc */
			 wh,		/* slider_size */
			 y		/* value */
			 );
      XtManageChild (CONTEXT_DATA (context)->vscroll);
    }

  if ( (w <= ww) || (are_scrollbars_active == FALSE) )
    {
      if (!frame_scrolling_yes)
      {
	x = 0;
	XtUnmanageChild (CONTEXT_DATA (context)->hscroll);
	fe_hack_scrollbar (CONTEXT_DATA (context)->hscroll,
			   1, 1, 1, 1, 0);
      }
    }
  else
    {
      fe_hack_scrollbar (CONTEXT_DATA (context)->hscroll,
			 w,		/* max */
			 lh,		/* inc */
			 ww - lh,	/* page_inc */
			 ww,		/* slider_size */
			 x		/* value */
			 );
      XtManageChild (CONTEXT_DATA (context)->hscroll);
    }

  if (frame_scrolling_yes)
  {
      XtManageChild (CONTEXT_DATA (context)->vscroll);
      XtManageChild (CONTEXT_DATA (context)->hscroll);
  }

  fe_find_scrollbar_sizes(context);

    CONTEXT_DATA (context)->document_x = x;
    CONTEXT_DATA (context)->document_y = y;

    if (context->compositor)
        CL_ScrollCompositorWindow(context->compositor, x, y);
}

void
FE_ScrollDocBy (MWContext *context, int iLocation, int32 deltax, int32 deltay)
{
  int32 x;
  int32 y;
  XFE_GetDocPosition (context, iLocation, &x, &y);
  x += deltax;
  y += deltay;
  fe_ScrollTo (context, x, y);
}

void
FE_ScrollDocTo (MWContext *context, int iLocation, int32 x, int32 y)
{
	fe_ScrollTo (context, x, y);
}

void
XFE_GetDocPosition (MWContext *context, int iLocation,
		   int32 *iX, int32 *iY)
{
  *iX = (int32)CONTEXT_DATA (context)->document_x;
  *iY = (int32)CONTEXT_DATA (context)->document_y;
}

void
XFE_SetDocPosition (MWContext *context, int iLocation, int32 x, int32 y)
{
#if 0
#ifdef EDITOR
	if (EDT_IS_EDITOR(context)) {
		fe_ScrollTo (context, x, y);
	}
	else
#endif
	{
		fe_SetDocPosition (context, x, y);
	}
#else
	/*
	 *    In WinFE (the standard), XFE_SetDocPosition() and FE_ScrollDocTo()
	 *    do the same thing (one calls the other). We seem to have a number
	 *    of bugs that are fixed by emulating WinFE's semantics, so our
	 *    feeling is that the back-end is now weighted in that way.
	 *    SO, rather than fight a losing battle, we think we'll just make
	 *    this change and hope that nothing new breaks. For Bug #77767.
	 *    - djw, vidur, mcafee, brendan (shared blame) July/20/1997.
	 */
	fe_ScrollTo (context, x, y);
#endif
}

void
XFE_SetDocDimension (MWContext *context,
		    int iLocation, int32 iWidth, int32 iLength)
{
  Widget widget = CONTEXT_DATA (context)->drawing_area;
#ifdef EDITOR
  unsigned long win_h;
  unsigned long old_h;
#endif
  unsigned long w, h;
  time_t now = time ((time_t) 0);

  if (! widget) abort ();
  w = iWidth;
  h = iLength;
  if (w < 1) w = 1;
  if (h < 1) h = 1;

#ifdef EDITOR
  win_h = CONTEXT_DATA (context)->scrolled_height;
  old_h = CONTEXT_DATA (context)->document_height;
#endif

  CONTEXT_DATA (context)->document_width = w;
  CONTEXT_DATA (context)->document_height = h;

  /* Only actually resize the window once a second, to avoid thrashing. */
  if (now <= CONTEXT_DATA (context)->doc_size_last_update_time)
    return;

  /* We just need to update the scrollbars; nothing visible has changed. */
  fe_SetDocPosition (context,
		     CONTEXT_DATA (context)->document_x,
		     CONTEXT_DATA (context)->document_y);

  CONTEXT_DATA (context)->doc_size_last_update_time = now;

#ifdef EDITOR
  /*
   * NOTE:  fix for redisplay bugs... [ 73704, 73705, 73688 ]
   *
   */
  if (EDT_IS_EDITOR(context)) {
	  if (h < old_h) {
		  /*
		   * NOTE:  need to force a refresh if the we lost the scrollbars
		   *        in the process of shrinking the document size...
		   *
		   */
		  if (h < win_h && old_h > win_h) {
#ifdef DEBUG_rhess
			  fprintf(stderr, "ForceRefresh::[ %d, %d ][ %d ][ %d ]\n",
					  w, h, old_h, win_h);
#endif
			  fe_RefreshArea(context, 0, 0, w, h);
		  }
	  }
  }
#endif
}


#ifdef RESIZE_CALLBACK_WORKS

static void
fe_resize_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  XmDrawingAreaCallbackStruct *cb = (XmDrawingAreaCallbackStruct *) call_data;

#else

static void
fe_scroller_resize (Widget widget, XtPointer closure)
{
#endif

  MWContext *context = (MWContext *) closure;
  fe_ContextData *fep = CONTEXT_DATA(context);
  MWContextType type = context->type;

  /* I question the wisdom of having to do a reload every time the window
     size changes... */
  Dimension w = 0, h = 0;
  Boolean relayout_p = False;

  XtVaGetValues (widget, XmNwidth, &w, XmNheight, &h, 0);

  relayout_p = ((Dimension) fep->scrolled_width) != w;

/*
 * Inside FRAMES (and eventually elsewhere?) we DO want to reload, even if
 * just the height has changed.
 */
  if ((!relayout_p)&&(context->is_grid_cell ||
		    (context->grid_children &&
		    XP_ListTopObject (context->grid_children))))
    {
      relayout_p = ((Dimension) fep->scrolled_height) != h;
    }

  /* Well this is kinda bogus; don't do a relayout if we're displaying one
     of the magic internal-external images, since we know no wrapping can
     occur. */
  if (relayout_p)
    {
      History_entry *h = SHIST_GetCurrent (&context->hist);
      if (h && h->is_binary)
	relayout_p = False;
    }

  /* If we have just an image and we resize such that either the width
   * or the height of the window is bigger than the image, and we had
   * scrolled off from the left,top edge, then we will need to refresh
   * the entire image. If this doesn't happen, then in the exposed parts
   * the image that is repainted is off the document_x, document_y.
   */
  if (
      /* If there is a real resize going on */
      (fep->scrolled_height != h || fep->scrolled_width != w) &&
      /* and if either of width or height spans the full document */
      (h >= fep->document_height || w >= fep->document_width) &&
      /* and if the document is not at its (0,0) origin */
      (fep->document_x != 0 || fep->document_y != 0)) {
    relayout_p = True;
  }

  /* Fullpage plugins need be told about size changes. */
  if (CONTEXT_DATA(context)->is_fullpage_plugin)
    {
        fe_RefreshArea (context,
                        CONTEXT_DATA (context)->document_x,
                        CONTEXT_DATA (context)->document_y,
                        1, 1);
	relayout_p = False;
    }

  if (! XtIsRealized (widget))
    /* I'm not sure this is right, but otherwise layout happens while a
       new window is still getting created... */
    relayout_p = False;

  if (relayout_p)
    {
      /* If a document is currently being laid out, just update the
	 scrollbars and flag it as needing to be re-figured at the end.
	 Otherwise, re-lay out the document with the new width.
	 Note: don't use CONTEXT_DATA (context)->active_url_count
	 here, that only counts foreground transfers, not images.
       */
      if (XP_IsContextBusy (context)) {
	CONTEXT_DATA (context)->relayout_required = True;
      } else {

	  /* This is unset in XFE_LayoutNewDocument. */
	  CONTEXT_DATA (context)->is_resizing = TRUE;

          /*
	   *    Need to update these before calling relayout, because the BE
	   *    is going to ask us this stuff in FE_GetDocAndWindowPosition().
	   *    Seems obvious I guess. Bug #27455.
	   *
	   *	I moved this out of the next EDT_IS_EDITOR if, since
	   *	for the new relayout stuff, grid edges don't get moved
	   *	if their x or y is greater than the window width and
	   *	height.  I honestly can't imagine why you wouldn't
	   *	want the size change reflected as early as possible
	   *	anyway -- toshok
	   */
	  CONTEXT_DATA(context)->scrolled_width  = (unsigned long)w;
	  CONTEXT_DATA(context)->scrolled_height = (unsigned long)h;
      }
    }
  /* We just need to update the scrollbars and scrolled_height and width;
     the other expose events which are coming will take care of
     repainting the text.
     Moved this up here because fe_EditorRefresh needs the width
     *inside* the scrollbars: bug 94115. ...Akkana
   */
  fe_SetDocPosition (context,
		     CONTEXT_DATA (context)->document_x,
		     CONTEXT_DATA (context)->document_y);

#ifdef EDITOR
  /*
   *    This edit_view_source_hack is so completely bogus,
   *    but it seems to only be edit for the editor view
   *    source window. Do this for bug #23375. djw 96/06/15.
   */
  if (EDT_IS_EDITOR(context) || context->edit_view_source_hack) {
      fe_EditorRefresh(context);
  }
  else
#endif /*EDITOR*/

#ifdef ENABLE_MARINER
	  {
	    int32 margin_w, margin_h;
	    
            LO_GetDocumentMargins(context, &margin_w, &margin_h);
	    
	    LO_RelayoutOnResize(context, w, h, margin_w, margin_h);
	  }
#else
	/*  As vidur suggested in Bug 59214: because JS generated content
	was not put into wysiwyg, hence this source was not be shown on a resize.
  	The fix was to make the reload policy for Mail/News contexts NET_NORMAL_RELOAD */
        if ( (type == MWContextNews) || (type == MWContextMail) 
	     || (type == MWContextNewsMsg) || (type == MWContextMailMsg) )
	  fe_ReLayout (context, NET_NORMAL_RELOAD);
	else
	  fe_ReLayout (context, NET_RESIZE_RELOAD);
#endif

  /* 
  ** this is wretched and gross.  the assignments below are exactly the same as the ones
  ** about 20 lines above.  Why do I duplicate them here, you may ask.  Well, for some
  ** reason someplace in between the assignments above and the ones here (probably in
  ** fe_SetDocPosition), something happens to set the scrolled_width and scrolled_height
  ** to smaller values.
  ** this results in a band along the bottom and right hand sides of the document, where the
  ** scrollbars would normally be.
  **
  ** I am thoroughly disgusted.
  **
  ** - toshok
  */
  /* It's being set to the size of the drawing area (not counting
   * the scrollbars) by fe_find_scrollbar_sizes, inside the 
   * fe_SetDocPosition call.   ...Akkana
   */
  CONTEXT_DATA(context)->scrolled_width  = (unsigned long)w;
  CONTEXT_DATA(context)->scrolled_height = (unsigned long)h;

  /* We only need the composited area without the scrollbars.  However,
     since the scrollbars can become managed and then unmanaged during
     layout, it is not worth the effort of keeping track of when they
     are visible.  Instead we make the compositor the size of the drawing
     area and just let the scrollbars clip the background layer. */
  if (context->compositor)
      CL_ResizeCompositorWindow(context->compositor, w, h);
}


Boolean
fe_TestGravity (Widget widget)
{
  Widget area, element;
  Arg arg[10];
  Cardinal argcnt;
  int shift;
  int orig_x, orig_y;
  int x, y;
  unsigned int w, h, d1, d2;
  Window dummy;
  XSetWindowAttributes set_attr;
  unsigned long valuemask;
  Boolean works;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;

  /*
   * OW 3.4 has some horrid bug so that it tests OK for
   * guffaws scrolling, but then when the user resizes the
   * window, it crashes the X server.  Sigh.
   */
  if ((!strcmp(XServerVendor(XtDisplay(widget)), "Sun Microsystems, Inc."))&&
      (VendorRelease(XtDisplay(widget)) == 3400))
  {
    return(False);
  }

  XtVaGetValues (widget, XtNvisual, &v, XtNcolormap, &cmap,
    XtNdepth, &depth, 0);

  shift = 10;

  argcnt = 0;
  XtSetArg(arg[argcnt], XmNvisual, v); argcnt++;
  XtSetArg(arg[argcnt], XmNcolormap, cmap); argcnt++;
  XtSetArg(arg[argcnt], XmNdepth, depth); argcnt++;
  XtSetArg(arg[argcnt], XtNwidth, 200); argcnt++;
  XtSetArg(arg[argcnt], XtNheight, 200); argcnt++;
  XtSetArg(arg[argcnt], XmNmappedWhenManaged, False); argcnt++;
  area = XmCreateDrawingArea(widget, "Area", arg, argcnt);

  argcnt = 0;
  XtSetArg(arg[argcnt], XmNvisual, v); argcnt++;
  XtSetArg(arg[argcnt], XmNcolormap, cmap); argcnt++;
  XtSetArg(arg[argcnt], XmNdepth, depth); argcnt++;
  XtSetArg(arg[argcnt], XtNx, 50); argcnt++;
  XtSetArg(arg[argcnt], XtNy, 80); argcnt++;
  XtSetArg(arg[argcnt], XmNmappedWhenManaged, False); argcnt++;
  element = fe_CreateTextField(area, "Element", arg, argcnt);

  XtManageChild(area);
  XtManageChild(element);

  valuemask = CWBitGravity | CWWinGravity;
  set_attr.win_gravity = StaticGravity;
  set_attr.bit_gravity = StaticGravity;
  XChangeWindowAttributes (XtDisplay(area), XtWindow(area),
    valuemask, &set_attr);
  XChangeWindowAttributes (XtDisplay(element), XtWindow(element),
    valuemask, &set_attr);

  XGetGeometry(XtDisplay(element), XtWindow(element), &dummy,
    &x, &y, &w, &h, &d1, &d2);
  orig_x = x;
  orig_y = y;

  XGetGeometry(XtDisplay(area), XtWindow(area), &dummy,
    &x, &y, &w, &h, &d1, &d2);
  XResizeWindow(XtDisplay(area), XtWindow(area), w, (h + shift));
  XMoveWindow(XtDisplay(area), XtWindow(area), x, (y - shift));
  XMoveResizeWindow(XtDisplay(area), XtWindow(area), x, y, w, h);

  XGetGeometry(XtDisplay(area), XtWindow(area), &dummy,
    &x, &y, &w, &h, &d1, &d2);
  XResizeWindow(XtDisplay(area), XtWindow(area), w, (h + shift));
  XMoveWindow(XtDisplay(area), XtWindow(area), x, (y - shift));
  XMoveResizeWindow(XtDisplay(area), XtWindow(area), x, y, w, h);

  XGetGeometry(XtDisplay(element), XtWindow(element), &dummy,
    &x, &y, &w, &h, &d1, &d2);
  if (y != (orig_y - (2 * shift)))
  {
    works = False;
  }
  else
  {
    works = True;
  }

  XtDestroyWidget(area);
  return(works);
}


int
fe_WindowGravityWorks (Widget toplevel, Widget widget)
{
  String str = 0;
  static XtResource res = { "windowGravityWorks", XtCString, XtRString,
			    sizeof (String), 0, XtRImmediate, "guess" };
  XtGetApplicationResources (toplevel, &str, &res, 1, 0, 0);

  if (str && (!XP_STRCASECMP(str, "yes") || !XP_STRCASECMP(str, "true")))
    {
      return 1;
    }
  else if (str && (!XP_STRCASECMP(str, "partial") || !XP_STRCASECMP(str, "kindof")))
    {
      return 2;
    }
  else if (str && (!XP_STRCASECMP(str, "no") || !XP_STRCASECMP(str, "false")))
    {
      return 0;
    }
  else
    {
      char *vendor = XServerVendor (XtDisplay (toplevel));

      if (str && XP_STRCASECMP(str, "guess"))
	{
	  fprintf (stderr,
		   XP_GetString(XFE_SCROLL_WINDOW_GRAVITY_WARNING),
		   fe_progname, str);
	}

      /*
       * Short circuit all this other crap, and do the right thing.
       */
      if (fe_TestGravity(widget))
      {
	return 1;
      }
      else
      {
	return 2;
      }

      /* WindowGravity is known to be broken in the OpenWound 3.0 server
	 and is known to work in the OpenWound 3.3 server.
       */
      if (!strcmp (vendor, "X11/NeWS - Sun Microsystems Inc."))
#if 0
	return (VendorRelease (XtDisplay (toplevel)) >= 3300);
#else
      /* Ok, all of Sun's code is just buggy as hell.  There are lurking X
	 server crashes that happen when we mess with window gravity on OW 2.4,
	 so forget it. */
      return 0;
#endif
      else if (!strncmp (vendor, "MacX", 4) || !strncmp (vendor, "eXodus", 6))
	/* The Mac X servers get this wrong (actually I'm not sure that both
	   do, but at least one of them does.) */
	return 0;

      else if (!strcmp (vendor, "Sony Corporation"))
	/* Sony NEWS R4 is busted; later versions not *known* to work. */
	return (VendorRelease (XtDisplay (toplevel)) > 4);

      else if (!strcmp (vendor,
	   "DECWINDOWS (Compatibility String) Network Computing Devices Inc."))
	/* NCD 2004 is busted; later versions not *known* to work. */
	return (VendorRelease (XtDisplay (toplevel)) > 2004);

      /* #### It has been suggested that all R4-based servers lose? */

      else
	return 1;
    }
}

void
FE_ShowScrollBars(MWContext *context, XP_Bool show)
{
    /* XXX implement me */
}

