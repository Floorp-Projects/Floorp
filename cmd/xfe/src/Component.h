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
   Component.h -- class definition for objects that have a base widget.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_component_h
#define _xfe_component_h

#include "NotificationCenter.h"

#include <Xm/Xm.h>
#include "Command.h"

struct fe_colormap;  /* don't include xfe.h here, since it conflicts with our stuff. */

class XFE_Component : public XFE_NotificationCenter
{
public:
  XFE_Component(Widget w, XFE_Component *toplevel_component = NULL);
  XFE_Component(XFE_Component *toplevel_component = NULL);

  virtual ~XFE_Component();

  Widget getBaseWidget();
  virtual void setBaseWidget(Widget w);
  
  XFE_Component *getToplevel();

  void setSensitive(Boolean sensitive);
  Boolean isSensitive();

  virtual void show();
  virtual void hide();
  virtual void setShowingState(XP_Bool showing);
  virtual void toggleShowingState();

  virtual XP_Bool isShown();
  virtual XP_Bool isAlive();

  virtual fe_colormap *getColormap();

  virtual Pixel getFGPixel();
  virtual Pixel getBGPixel();
  virtual Pixel getTopShadowPixel();
  virtual Pixel getBottomShadowPixel();

  // These two methods should be belonging to a class that is above
  // the Frame and Dialog Class, but below Component class
  // However, since we don't have that class now, we move these two
  // virtual methods from Frame.cpp to Component.cpp so that dashboard
  // can query for the string regardless the dashboard is in a 
  // Frame or Dialog
  // tooltips and doc string
  virtual char *getDocString(CommandType cmd);
  virtual char *getTipString(CommandType cmd);


  void translateFromRootCoords(int x_root, int y_root, int *x, int *y);
  Boolean	isWidgetInside(Widget w);

  static const char *afterRealizeCallback; // called after we are realized, if we need to notify anyone.

  // use this method to get 'cmd.labelString' resources
  // for the specified widget (default is the base widget)
  char *getLabelString(char* cmd, Widget for_w=NULL);
  // use this method to get 'cmd.[show|hide]LabelString' resources
  // for the specified widget (default is the base widget)
  char *getShowHideLabelString(char* cmd, Boolean show, Widget for_w=NULL);

protected:
  XFE_Component *m_toplevel;
  Widget m_widget;

  char *stringFromResource(char *command_string); // use this to get context sensitive strings
  void installDestroyHandler();

private:
  static void destroy_cb(Widget, XtPointer, XtPointer);
};

#endif /* _xfe_component_h */
