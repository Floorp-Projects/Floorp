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
   FolderFrame.h -- class definitions for mail/news folder frames.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */



#ifndef _xfe_folderframe_h
#define _xfe_folderframe_h

#include "xp_core.h"
#include "Frame.h"
#include "MNBanner.h"
#include <Xm/Xm.h>

class XFE_FolderFrame : public XFE_Frame
{
public:
  XFE_FolderFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
  virtual ~XFE_FolderFrame();

  virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

  // opens the folderframe and selects the folder specified by info.
  virtual void show(MSG_FolderInfo *info);
  virtual void show(MSG_ViewIndex index);
  virtual void show();
  virtual XP_Bool isOkToClose();

private:
  static MenuSpec menu_bar_spec[];
  static MenuSpec file_menu_spec[];
  static MenuSpec edit_menu_spec[];
  static MenuSpec view_menu_spec[];


  static ToolbarSpec toolbar_spec[];

  XFE_CALLBACK_DECL(MNChromeUpdate)

  XFE_MNBanner *m_banner;

  // Toolbox methods
  virtual void		toolboxItemSnap			(XFE_ToolboxItem * item);
  virtual void		toolboxItemClose		(XFE_ToolboxItem * item);
  virtual void		toolboxItemOpen		(XFE_ToolboxItem * item);
  virtual void		toolboxItemChangeShowing(XFE_ToolboxItem * item);

  virtual void		configureToolbox	();
};

extern "C" MWContext* fe_showFoldersWithSelected(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, MSG_FolderInfo *info);
extern "C" MWContext* fe_showFolders(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
extern "C" MWContext* fe_showNewsgroups(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);

#endif /* _xfe_folderframe_h */
