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
   ProxyIcon.h -- a label that you can drag from.
   Created: Chris Toshok <toshok@netscape.com>, 1-Dec-96.
   */



#ifndef _xfe_proxyicon_h
#define _xfe_proxyicon_h

#include "mozilla.h"
#include "xfe.h"
#include "icons.h"
#include "Component.h"
#include "dragdrop.h"

class XFE_ProxyIcon : public XFE_Component
{
public:
  XFE_ProxyIcon(XFE_Component *toplevel_component, Widget parent, char *name, fe_icon *icon = NULL);

  void setIcon(fe_icon *icon);
  fe_icon *getIcon();

  void setDragType(fe_dnd_Type dragtype, XFE_Component *dragsource = NULL,
		   fe_dnd_SourceDropFunc func = NULL);
private:
  fe_icon *m_icon;


  EventMask m_activity;
  int m_lastmotionx;
  int m_lastmotiony;
  int m_hotSpot_x;
  int m_hotSpot_y;

  Boolean m_ignoreevents;

  fe_dnd_Type m_dragtype;
  XFE_Component *m_source;
  fe_dnd_SourceDropFunc m_sourcedropfunc;

  void makeDragWidget(int x, int y);
  void destroyDragWidget();

  void buttonEvent(XEvent *event, Boolean *c);
  static void buttonEventHandler(Widget, XtPointer, XEvent *, Boolean *);
};

#endif /* _xfe_proxyicon_h */
