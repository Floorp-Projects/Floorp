/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   ProxyIcon.cpp -- a label that you can drag from.
   Created: Chris Toshok <toshok@netscape.com>, 1-Dec-96.
   */



#include "ProxyIcon.h"
#include <Xm/Label.h>

#define NONMOTIF_DND

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#if defined(USE_MOTIF_DND)
fe_dnd_Source _xfe_dragsource = { 0 };
#else
extern fe_dnd_Source _xfe_dragsource;
#endif /* USE_MOTIF_DND */

XFE_ProxyIcon::XFE_ProxyIcon(XFE_Component *toplevel_component,
			     Widget parent, char *name, fe_icon *icon)
  : XFE_Component(toplevel_component)
{
  Widget label;
  m_icon = icon;

  m_toplevel = toplevel_component;

  label = XmCreateLabel(parent, name, NULL, 0);

  setBaseWidget(label);

  XtInsertEventHandler(label, ButtonPressMask | ButtonReleaseMask
		       | PointerMotionMask, False,
		       buttonEventHandler, this, XtListHead);

  m_activity = 0;
  m_lastmotionx = 
	  m_lastmotiony = 0;
  m_dragtype = FE_DND_NONE;

  setIcon(icon);
}

void
XFE_ProxyIcon::setIcon(fe_icon *icon)
{
  m_icon = icon;

  if (m_icon && m_icon->pixmap)
    {
      XtVaSetValues(m_widget,
		    XmNlabelType, XmPIXMAP,
		    XmNlabelPixmap, icon->pixmap,
		    NULL);
    }
}

fe_icon *
XFE_ProxyIcon::getIcon()
{
  return m_icon;
}

void
XFE_ProxyIcon::setDragType(fe_dnd_Type dragtype, XFE_Component *drag_source,
			   fe_dnd_SourceDropFunc sourcedropfunc)
{
  m_dragtype = dragtype;
  m_source = drag_source;
  m_sourcedropfunc = sourcedropfunc;
}

void
XFE_ProxyIcon::makeDragWidget(int x, int y)
{
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  Widget label;
  //  fe_icon *icon;
  Widget shell;
  Pixmap dragPixmap;

  D(printf("hot (x,y) = (%d,%d)\n", x, y);)

  if (_xfe_dragsource.widget) return;  

  shell = m_toplevel->getBaseWidget();

  XtVaGetValues (shell, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);

  _xfe_dragsource.type = m_dragtype; // XXX column drags happen when the row was a header.
  _xfe_dragsource.hotx = x;
  _xfe_dragsource.hoty = y;
  _xfe_dragsource.closure = m_dragtype == FE_DND_COLUMN ? (void*)this : (void*)m_source;
  _xfe_dragsource.func = m_sourcedropfunc;

  XP_ASSERT(m_icon != NULL);
  if (m_icon == NULL) return;

  dragPixmap = m_icon->pixmap;

  _xfe_dragsource.widget = XtVaCreateWidget("drag_win",
				       overrideShellWidgetClass,
				       m_widget,
				       XmNwidth, m_icon->width,
				       XmNheight, m_icon->height,
				       XmNvisual, v,
				       XmNcolormap, cmap,
				       XmNdepth, depth,
				       XmNborderWidth, 0,
				       NULL);
  
  label = XtVaCreateManagedWidget ("label",
				   xmLabelWidgetClass,
				   _xfe_dragsource.widget,
				   XmNlabelType, XmPIXMAP,
				   XmNlabelPixmap, dragPixmap,
				   NULL);
}

void
XFE_ProxyIcon::destroyDragWidget()
{
  if (!_xfe_dragsource.widget) return;
  XtDestroyWidget (_xfe_dragsource.widget);
  _xfe_dragsource.widget = NULL;
}

void
XFE_ProxyIcon::buttonEvent(XEvent *event, Boolean *c)
{
  int x = event->xbutton.x;
  int y = event->xbutton.y;

  m_ignoreevents = False;

  switch (event->type)
    {
    case ButtonPress:
      D(printf ("ButtonPress\n");)
      /* Always ignore btn3. Btn3 is for popups. - dp */
      if (event->xbutton.button == 3) break;

      m_activity |= ButtonPressMask;
      m_ignoreevents = True;

      m_lastmotionx = x;
      m_lastmotiony = y;

	  // Save this position off so we can draw the widget
	  // with the appropriate hot spot.  Ie, you should drag
	  // the icon from the point where you clicked on it, not
	  // some predetermined hot spot..
	  m_hotSpot_x = x;
	  m_hotSpot_y = y;
      break;
    case ButtonRelease:
      if (m_activity & ButtonPressMask)
	{
	  if (m_activity & PointerMotionMask)
	    {
	      /* handle the drop */
	      fe_dnd_DoDrag(&_xfe_dragsource, event, FE_DND_DROP);
	      fe_dnd_DoDrag(&_xfe_dragsource, event, FE_DND_END);

	      destroyDragWidget();
	    }
	}

      m_activity = 0;
      
      break;
#ifdef NONMOTIF_DND
    case MotionNotify:
      if (m_dragtype == FE_DND_NONE)
	break;

      if (!(m_activity & PointerMotionMask) &&
	  (abs(x - m_lastmotionx) < 5 && abs(y - m_lastmotiony) < 5)) 
	{
	  /* We aren't yet dragging, and the mouse hasn't moved enough for
	     this to be considered a drag. */
	  break;
	}

      if (m_activity & ButtonPressMask) 
	{
	  /* ok, the pointer moved while a button was held.
	   * we're gonna drag some stuff.
	   */
	  m_ignoreevents = True;

	  if (!(m_activity & PointerMotionMask)) 
	    {
	      // Create a drag source.
		  // We want the mouse-down location, not the last one. 
		  makeDragWidget(m_hotSpot_x, m_hotSpot_y);
	      fe_dnd_DoDrag(&_xfe_dragsource, event, FE_DND_START);

	      m_activity |= PointerMotionMask;
	    }
	  
	  fe_dnd_DoDrag(&_xfe_dragsource, event, FE_DND_DRAG);
	  
	  /* Now, force all the additional mouse motion events that are
	     lingering around on the server to come to us, so that Xt can
	     compress them away.  Yes, XSync really does improve performance
	     in this case, not hurt it. */
	  XSync(XtDisplay(m_widget), False);
	}
      
      m_lastmotionx = x;
      m_lastmotiony = y;
      break;
#endif
    }
  
  if (m_ignoreevents) 
    *c = False;
}

void 
XFE_ProxyIcon::buttonEventHandler(Widget, XtPointer clientData, XEvent *event, Boolean *cont)
{
  XFE_ProxyIcon *obj = (XFE_ProxyIcon*)clientData;

  obj->buttonEvent(event, cont);
}
