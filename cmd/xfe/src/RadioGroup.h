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
   RadioGroup.h -- Radio groups that don't have to share the same row column parent.
   Created: Chris Toshok <toshok@netscape.com>, 28-Sep-96.
 */



#ifndef _xfe_radiogroup_h
#define _xfe_radiogroup_h

#include "xp_core.h"
#include "xp_list.h"
#include <X11/Intrinsic.h>
#include "Frame.h"

class XFE_RadioGroup {
public:

  char *getName();
  XFE_Frame *getFrame();

  void addChild(Widget w);
  void removeChild(Widget w);

  //returns a named radio group, creating it if it doesn't already exist.
  static XFE_RadioGroup *getByName(char *name, XFE_Frame *forFrame);

private:
  char *m_name;
  XFE_Frame *m_frame;

  Boolean m_incallback;
  Widget m_callbackwidget;

  XP_List *m_children;

  static void registerGroup(XFE_RadioGroup *group);
  static void unregisterGroup(XFE_RadioGroup *group);
  static XP_List *groups;

  // keep these private.  Only use the getByName() method above to actually get at
  // a XFE_RadioGroup.
  XFE_RadioGroup(char *name, XFE_Frame *frame);
  ~XFE_RadioGroup();

  void toggle(Widget w);
  static void toggle_cb(Widget w, XtPointer clientData, XtPointer callData);

#ifdef DEBUG
  char *getLabel(Widget w);
#endif
};

#endif /* _xfe_radiogroup_h */
