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
   ToolbarButton.h -- class definition for the toolbar buttons
   Created: Chris Toshok <toshok@netscape.com>, 13-Aug-96.
 */

#ifndef _xfe_toolbarbutton_h
#define _xfe_toolbarbutton_h

#include "xp_core.h" 

#include "XFEComponent.h"
#include "XFEView.h"
#include "Command.h"
#include "Toolbar.h"

#include "mozilla.h" /* for MWContext!!! */
#include "icons.h"

class ToolbarButton : public XFEComponent
{
private:
  Toolbar *parentToolbar;
  XFECommandType command;
  const char *name;
  fe_icon *icon;
  XFEView *v;

  void activate();

  static void activateCallback(Widget, XtPointer, XtPointer);

public:
  ToolbarButton(Toolbar *toolbar, 
		XFEView *respView,
		const char *button_name, 
		XFECommandType button_command, 
		fe_icon *button_icon);

  Toolbar *getToolbar() { return parentToolbar; }
};

#endif /* _xfe_toolbar_h */
