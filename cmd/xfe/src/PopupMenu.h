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

class XFE_PopupMenu : public XFE_Menu
{
public:
  XFE_PopupMenu(String name,XFE_Frame *frame, Widget parent, MenuSpec * menu_spec = NULL);
  virtual ~XFE_PopupMenu();

  void position(XEvent *event);
  void raise();
  
  virtual void show();

  void removeLeftRightTranslations();

  static Widget CreatePopupMenu(Widget pw,String name,ArgList av,Cardinal ac);

private:

  static MenuSpec title_spec[];
};

#endif /* _xfe_popupmenu_h */
