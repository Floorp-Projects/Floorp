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
   PopupMenu.h -- class definition for popup menus.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_popupmenu_h
#define _xfe_popupmenu_h

#include "Menu.h"

class XFE_PopupMenuBase
{
public:
  XFE_PopupMenuBase(String name, Widget parent);
  virtual ~XFE_PopupMenuBase();

  void position(XEvent *event);
  void raise();
  
  void removeLeftRightTranslations();

  static Widget CreatePopupMenu(Widget pw,String name,ArgList av,Cardinal ac);

protected:
  Widget m_popup_menu;
};

//////////////////////////////////////////////////////////////////////

class XFE_PopupMenu : public XFE_Menu, public XFE_PopupMenuBase
{
public:

  XFE_PopupMenu(String name, XFE_Frame *frame, Widget parent,
                MenuSpec * menu_spec = NULL);

private:

  static MenuSpec title_spec[];
};

//////////////////////////////////////////////////////////////////////

class XFE_SimplePopupMenu : public XFE_PopupMenuBase
{
public:
  XFE_SimplePopupMenu(String name, Widget parent);

  void show();

  void addSeparator();
  void addPushButton(String name, void *userData = NULL, Boolean isSensitive = True);

  static void pushb_activate_cb(Widget w, XtPointer clientData, XtPointer callData);

  virtual void PushButtonActivate(Widget w, XtPointer userData);
};
#endif /* _xfe_popupmenu_h */
