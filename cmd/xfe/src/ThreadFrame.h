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
   ThreadFrame.h -- class definitions for mail/news thread frames.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */



#ifndef _xfe_threadframe_h
#define _xfe_threadframe_h

#include "xp_core.h"
#include "Frame.h"
#include "MNBanner.h"
#include "FolderDropdown.h"
#include "msgcom.h"
#include <Xm/Xm.h>

class XFE_ThreadFrame : public XFE_Frame
{
public:
  XFE_ThreadFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
  virtual ~XFE_ThreadFrame();

  virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

  void loadFolder(MSG_FolderInfo *folderInfo);
  MSG_FolderInfo *getFolderInfo();

  static XFE_ThreadFrame *frameForInfo(MSG_FolderInfo *info);

    // For security icon state...
  virtual int getSecurityStatus();

  // Shared with MsgFrame and MsgCenter
  static MenuSpec new_submenu_spec[];
  static MenuSpec save_submenu_spec[];
  static MenuSpec select_submenu_spec[];
  static MenuSpec show_submenu_spec[];
  static MenuSpec next_submenu_spec[];
  static MenuSpec prev_submenu_spec[];
  static MenuSpec go_menu_spec[];
  static MenuSpec mark_submenu_spec[];
private:
  static MenuSpec menu_bar_spec[];
  static MenuSpec file_menu_spec[];
  static MenuSpec edit_menu_spec[];
  static MenuSpec view_menu_spec[];
  static MenuSpec message_menu_spec[];

  static MenuSpec offline_submenu_spec[];
  static MenuSpec ignore_submenu_spec[];

  static ToolbarSpec toolbar_spec[];

  void updateReadAndTotalCounts();

  XFE_CALLBACK_DECL(mommy)
  XFE_CALLBACK_DECL(dropdownFolderLoad)
  XFE_CALLBACK_DECL(MNChromeUpdate)
  XFE_CALLBACK_DECL(FolderChromeUpdate)
  XFE_CALLBACK_DECL(updateBanner)
  XFE_CALLBACK_DECL(folderDeleted)
  XFE_CALLBACK_DECL(showFolder)

  XFE_FolderDropdown *m_dropdown;
  XFE_MNBanner *m_banner;

  // Toolbox methods
  virtual void		toolboxItemSnap			(XFE_ToolboxItem * item);
  virtual void		toolboxItemClose		(XFE_ToolboxItem * item);
  virtual void		toolboxItemOpen		(XFE_ToolboxItem * item);
  virtual void		toolboxItemChangeShowing(XFE_ToolboxItem * item);

  virtual void		configureToolbox	();
  virtual XP_Bool 	isOkToClose();
};

extern "C" MWContext* fe_showInbox(Widget toplevel, XFE_Frame *parent_frame,
								   Chrome *chromespec,
								   XP_Bool with_reuse,
								   XP_Bool getNewMail);
extern "C" MWContext* fe_showMessages(Widget toplevel, XFE_Frame *parent_frame,
									  Chrome *chromespec,
									  MSG_FolderInfo *info,
									  XP_Bool with_reuse,
									  XP_Bool getNewMail,
									  MessageKey key = MSG_MESSAGEKEYNONE);

#endif /* _xfe_threadframe_h */
