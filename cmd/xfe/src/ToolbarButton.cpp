/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* -*- Mode: C++; tab-width: 4 -*-
   ToolbarButton.cpp -- implementation file for buttons that can appear in 
                        the toolbar
   Created: Chris Toshok <toshok@netscape.com>, 13-Aug-96.
 */

#include "ToolbarButton.h"
#include <Xm/PushB.h>
#include "xp_mcom.h"

ToolbarButton::ToolbarButton(Toolbar *toolbar,
			     XFEView *respView,
			     const char *button_name,
			     XFECommandType button_command,
			     fe_icon *button_icon) : XFEComponent()
{
  Widget button;

  parentToolbar = toolbar;
  name = XP_STRDUP(name);
  command = button_command;
  icon = button_icon;
  v = respView;

  // this isn't really right...
  button = XtVaCreateManagedWidget(button_name,
				   xmPushButtonWidgetClass,
				   toolbar->baseWidget(),
				   NULL);

  setBaseWidget(button);

  XtAddCallback(button, XmNactivateCallback, activateCallback, this);
}

void
ToolbarButton::activateCallback(Widget, XtPointer clientData, XtPointer)
{
  ToolbarButton *obj = (ToolbarButton*)clientData;

  obj->activate();
}

void
ToolbarButton::activate()
{
  v->doCommand(command);
}
