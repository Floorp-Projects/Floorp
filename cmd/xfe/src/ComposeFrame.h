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
   ComposeFrame.h -- class definitions for mail compose frames.
   Created: Dora Hsu <dora@netscape.com>, 23-Sept-96.
 */



#ifndef _xfe_composeframe_h
#define _xfe_composeframe_h

#include "Frame.h"
#include <Xm/Xm.h>
#include "msgcom.h"
#include "PopupMenu.h"


class XFE_ComposeFrame : public XFE_Frame
{
public:
  XFE_ComposeFrame(Widget toplevel, Chrome *chromespec, 
                MWContext *old_context = NULL,
		MSG_CompositionFields *fields = NULL,
		const char *pInitialText = NULL,
		XP_Bool useHtml = False);
  virtual ~XFE_ComposeFrame();

  MSG_Pane   *getPane();

  virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual void    doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  XFE_Command* getCommand(CommandType cmd);

  virtual int initEditor();

  virtual void allConnectionsComplete(MWContext  *context = NULL);

  void destroyWhenAllConnectionsComplete();

  // For security icon state...
  virtual int getSecurityStatus();
  virtual XP_Bool isOkToClose();

protected:
  
private:
  static MenuSpec menu_bar_spec[];
  static MenuSpec file_menu_spec[];
  static MenuSpec edit_menu_spec[];
  static MenuSpec view_menu_spec[];
  static MenuSpec show_chrome_spec[];
  static MenuSpec saveas_spec[];
  static MenuSpec message_attach_menu_spec[];
  static ToolbarSpec toolbar_spec[];
  static MenuSpec html_edit_menu_spec[];
  static MenuSpec html_view_menu_spec[];
  static MenuSpec html_show_chrome_spec[];
  static MenuSpec html_menu_bar_spec[];
  XP_Bool m_destroyWhenConnectionsComplete;

  // Toolbox methods
  virtual void		toolboxItemClose		(XFE_ToolboxItem * item);
  virtual void		toolboxItemOpen		(XFE_ToolboxItem * item);
  virtual void		toolboxItemChangeShowing(XFE_ToolboxItem * item);

  virtual void		configureToolbox	();
};

#endif /* _xfe_composeframe_h */
