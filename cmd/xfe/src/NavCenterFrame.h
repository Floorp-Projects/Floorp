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
   NavCenterFrame.cpp -- class definition for the NavCenter frame class
   Created: Stephen Lamm <slamm@netscape.com>, 05-Nov-97.
 */



#ifndef _xfe_navcenterframe_h
#define _xfe_navcenterframe_h

#include "Frame.h"

class XFE_NavCenterFrame : public XFE_Frame
{
public:
  XFE_NavCenterFrame(Widget toplevel, XFE_Frame *parent_frame,
                     Chrome *chromespec);
  virtual ~XFE_NavCenterFrame();

  //virtual void updateToolbar();

  virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

private:
  static MenuSpec menu_bar_spec[];
  static MenuSpec file_menu_spec[];
  static MenuSpec view_menu_spec[];
};

extern "C" MWContext *fe_showNavCenter(Widget toplevel, 
                                       XFE_Frame *parent_frame,
                                       Chrome *chromespec, URL_Struct *url);

#endif /* _xfe_navcenterframe_h */
