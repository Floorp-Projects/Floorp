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
   ComposeView.h -- class definition for ComposeView
   Created: Dora Hsu<dora@netscape.com>, 23-Sept-96.
 */



#ifndef _xfe_composeaddressview_h
#define _xfe_composeaddressview_h


#include "MNView.h"

/* Compose Address View */


class XFE_ComposeAddressView : public XFE_MNView
{
public:
  XFE_ComposeAddressView(XFE_View *parent_view,
			XFE_Component *toplevel_component, MSG_Pane *p = NULL);

  virtual void createWidgets(Widget parent);

  virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

private:
};


#endif /* _xfe_composeview_h */
