/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   MsgFrame.h -- class definitions for mail/news message frames.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */



#ifndef _xfe_msgframe_h
#define _xfe_msgframe_h

#include "Frame.h"
#include "ThreadFrame.h"
#include "MNBanner.h"

#include "xp_core.h"
#include "msgcom.h"

#include <Xm/Xm.h>

class XFE_MsgFrame : public XFE_Frame
{
public:
  XFE_MsgFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);

  virtual ~XFE_MsgFrame();

  virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

  virtual XP_Bool isOkToClose();

  virtual void allConnectionsComplete(MWContext  *context = NULL);
  void loadMessage(MSG_FolderInfo *folder_info, MessageKey msg_key);

  MSG_FolderInfo *getFolderInfo();
  MessageKey getMessageKey();

  static XFE_MsgFrame *frameForMessage(MSG_FolderInfo *folder_info, MessageKey msg_key);

  MSG_Pane *getPane();
  
  // For security icon state...
  virtual int getSecurityStatus();

  void setButtonsByContext(MWContextType cxType);

private:
  XFE_MNBanner *m_banner;

  XFE_CALLBACK_DECL(spaceAtMsgEnd)
  XFE_CALLBACK_DECL(msgLoaded)
  XFE_CALLBACK_DECL(msgDeleted)
  XFE_CALLBACK_DECL(folderDeleted)

  void updateReadAndTotalCounts();

  XFE_CALLBACK_DECL(MNChromeUpdate)
  XFE_CALLBACK_DECL(FolderChromeUpdate)

  static MenuSpec menu_bar_spec[];
  static MenuSpec file_menu_spec[];
  static MenuSpec edit_menu_spec[];
  static MenuSpec view_menu_spec[];
  static MenuSpec go_menu_spec[];
  static MenuSpec message_menu_spec[];

  static MenuSpec encoding_menu_spec[];

  static MenuSpec offline_submenu_spec[];

  static ToolbarSpec toolbar_spec[];

  // Toolbox methods
  virtual void		toolboxItemSnap			(XFE_ToolboxItem * item);
  virtual void		toolboxItemClose		(XFE_ToolboxItem * item);
  virtual void		toolboxItemOpen		(XFE_ToolboxItem * item);
  virtual void		toolboxItemChangeShowing(XFE_ToolboxItem * item);

  virtual void		configureToolbox	();
};

extern "C" MWContext* fe_showMsg(Widget toplevel, XFE_Frame *parent_frame, 
								 Chrome *chromespec, 
								 MSG_FolderInfo *folder_info, 
								 MessageKey msg_key, 
								 XP_Bool with_reuse);

#endif /* _xfe_msgframe_h */
