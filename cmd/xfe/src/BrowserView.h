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

#ifndef _xfe_browserview_h
#define _xfe_browserview_h

#include "NavCenterView.h"
#include "HTMLView.h"
#include "View.h"

class XFE_BrowserView : public XFE_View
{
public:
  XFE_BrowserView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context);

  virtual ~XFE_BrowserView();

  XFE_HTMLView * getHTMLView();
  XFE_NavCenterView * getNavCenterView();
  Boolean  isNavCenterShown();
  void     hideNavCenter();
  void     showNavCenter();


   /* These are the methods that views will want to overide to add
     their own functionality. */

#ifdef UNDEF
  /* this method is used by the toplevel to sensitize menu/toolbar items. */
  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* m = NULL);

  /* this method is used by the toplevel to dispatch a command. */
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* i = NULL);

  /* used by toplevel to see which view can handle a command.  Returns true
     if we can handle it. */
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL, 
								 XFE_CommandInfo* i = NULL);

  /* used by toplevel to change the labels specified in menu items.  Return NULL
     if no change. */
  virtual char* commandToString(CommandType cmd, void *calldata = NULL,
								XFE_CommandInfo* i = NULL);

  /* used by toplevel to change the selection state of specified toggle menu 
     items.  This method only applies to toggle button */
  virtual Boolean isCommandSelected(CommandType cmd, void *calldata = NULL,
									XFE_CommandInfo* i = NULL);


  virtual XFE_Command* getCommand(CommandType) { return NULL; };
  virtual XFE_View*    getCommandView(XFE_Command*);
#endif  /* UNDEF */

private:

	XFE_HTMLView       *  _htmlView;
	XFE_NavCenterView  *  _navCenterView;
};



#endif   /* _xfe_browserview_h_  */

