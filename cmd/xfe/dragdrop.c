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

   dragdrop.c --- Very simplistic drag and drop support.
   Created: Terry Weissman <terry@netscape.com>, 27-Jun-95.
 */

#include "mozilla.h"
#include "xfe.h"
#include "dragdrop.h"

#include <Xfe/Xfe.h>			/* for xfe widgets and utilities */

typedef struct DropSite {
  Widget widget;
  fe_dnd_DropFunc func;
  void* closure;
  struct DropSite* next;
} DropSite;

static DropSite* FirstDropSite = NULL;

static void
fe_dnd_dropsite_destroyed(Widget widget, XtPointer closure,
			  XtPointer call_data)
{
  DropSite* site = (DropSite*) closure;
  DropSite** tmp;
  assert(widget == site->widget);
  for (tmp = &FirstDropSite ; *tmp ; tmp = &((*tmp)->next)) {
    if (*tmp == site) {
      *tmp = site->next;
      XP_FREE(site);
      return;
    }
  }
  abort();
}

void
fe_dnd_CreateDrop(Widget widget, fe_dnd_DropFunc func, void* closure)
{
  DropSite* tmp = XP_NEW_ZAP(DropSite);
  tmp->widget = widget;
  tmp->func = func;
  tmp->closure = closure;
  tmp->next = FirstDropSite;
  FirstDropSite = tmp;

  XtAddCallback(widget, XmNdestroyCallback, fe_dnd_dropsite_destroyed, tmp);
}



void
fe_dnd_DoDrag(fe_dnd_Source* source, XEvent* event, fe_dnd_Event type)
{
  Display* dpy = XtDisplay(source->widget);
  int x = event->xbutton.x_root - source->hotx;
  int y = event->xbutton.y_root - source->hoty;
  DropSite* site;
  Window dropwindow = None;
  XtRealizeWidget(source->widget);
  XMoveWindow(dpy, XtWindow(source->widget), x, y);
  if (type == FE_DND_START) {
    XtPopup(source->widget, XtGrabNone);
  }
  for (site = FirstDropSite ; site ; site = site->next) {
    XP_Bool callit = TRUE;
    if (type == FE_DND_DROP) {
      /* Only call drops if it is actually on this widget. */
      Position rootx;
      Position rooty;
      XtTranslateCoords(site->widget, 0, 0, &rootx, &rooty);
      if (event->xbutton.x_root < rootx || event->xbutton.y_root < rooty) {
	callit = FALSE;
      } else {
	rootx += XfeWidth(site->widget);
	rooty += XfeHeight(site->widget);
	if (event->xbutton.x_root >= rootx || event->xbutton.y_root >= rooty) {
	  callit = FALSE;
	} else {
	  /* Huh.  Well, it looks like we're over this widget, but we have more
	     tests to try.  First, make sure that this widget and all of its
	     ancesters are managed at the moment.  (For some reason, the
	     uppermost widget, the Shell widget, is generally not managed even
	     when visible, so we stop checking once we've hit a shell.)

	     We might also want to check for mapped, as well as managed, but
	     that seems a bit awkward to do right now, and is not strictly
	     necessary.  Such checks would be nice to do during dragging as
	     well as dropping; it would help prevent us from drawing into
	     iconified windows. */
	  Widget widget;
	  Window toplevelwindow = 0;
	  for (widget = site->widget;
	       widget && !XtIsShell(widget);
	       widget = XtParent(widget)) {
	    if (!XtIsManaged(widget)) {
	      callit = FALSE;
	      break;
	    }
	  }
	  if (callit) {
	    /* OK, the final acid test: is the mouse really on the window in
	       question?  I am trusting that we don't have any layout where the
	       widget is obscured by some other widget within the same toplevel
	       window; I'm more worried about the case where there is another
	       toplevel window above this widget's toplevel window and below
	       the mouse.  (There maybe ought to be a grab on during some of
	       this.) */
	    if (dropwindow == None) {
	      /* Figure out what toplevel window we are pointing at.  Done
		 only once; this does require a round trip to the X server. */
	      Window rootreturn;
	      int rootx, rooty, winx, winy;
	      unsigned int mask;
	      XUnmapWindow(dpy, XtWindow(source->widget));
	      if (!XQueryPointer(dpy, event->xbutton.root, &rootreturn,
				 &dropwindow, &rootx, &rooty, &winx, &winy,
				 &mask)) {
		/* This is not very likely -- the cursor has moved to another
		   screen since we got the drop event.  Whatever.*/
		dropwindow = None;
	      }
	      if (dropwindow == None) {
		/* Well, it appears that the drop didn't even happen on a
		   window at all; the mouse is pointing at the root window.
		   How bizarre, especially since the rectangle check showed
		   we were within a widget.  I guess it could happen if we
		   iconified a window, and then dropped where it was when
		   uniconified.

		   At any rate, we shouldn't deliver a drop event to anywhere.
		   Break out of the loop where we've been looking for a
		   destination. */
		break;
	      }
	    }
	    /* Find out what X window is the topmost container for this
	       site, so we can test if it's the same that XQueryWindow
	       returned.  Note that we can't easily cache this result; it
	       can change if the window manager decides to reparent the
	       shell to another window.  Which can happen, for example, if
	       we unmap this window and map it later.

	       Anyway, finding this toplevel window can take several round
	       trips to the server.  First we find the uppermost window we can
	       from the widget hierarchy, and then we work up our way up
	       the X window tree. */
	    for (widget = site->widget; widget; widget = XtParent(widget)) {
	      Window w = XtWindow(widget);
	      if (w) toplevelwindow = w;
	      if (XtIsShell(widget)) {
		/* Sometimes a shell has another shell as a parent (I guess
		   for trasient-for and stuff), so be sure we stop before
		   going to that other shell. */
		break;
	      }
	    }
	    if (toplevelwindow) {
	      Window parent;
	      Window root;
	      Window* children;
	      unsigned int numchildren;
	      while (toplevelwindow != dropwindow) {
		/* There oughta be a better call then XQueryTree... */
		if (!XQueryTree(dpy, toplevelwindow, &root, &parent,
				&children, &numchildren)) {
		  /* The call failed.  I dunno how it can do that.  Uh, uh,
		     well, we'll just not drop in there, shall we.  ### */
		  XP_ASSERT(0);
		  break;
		}
		if (children) XFree(children);
		if (parent == root) break;
		toplevelwindow = parent;
	      }
	    }
	    if (dropwindow != toplevelwindow) {
	      callit = FALSE;
	    }
	  }
	}
      }
    }
    if (callit) {
      (*site->func)(site->widget, site->closure, type, source, event);
      if (type == FE_DND_DROP) {
	/* Never drop on more than one dropsite. */
	break;
      }
    }
  }
}
