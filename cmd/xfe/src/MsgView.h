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
   MsgView.h -- class definition for MsgView.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-1996
 */



#ifndef _xfe_msgview_h
#define _xfe_msgview_h

#include "MNView.h"
#include "HTMLView.h"
#include "PopupMenu.h"
#include "Command.h"
#include "msgcom.h"
#include "ReadAttachPanel.h"

class XFE_MsgView : public XFE_MNView
{
public:
  XFE_MsgView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context, MSG_Pane *p = NULL);
  virtual ~XFE_MsgView();

  void loadMessage(MSG_FolderInfo *info, MessageKey key);

  MSG_FolderInfo *getFolderInfo();
  MessageKey getMessageKey();

  virtual void paneChanged(XP_Bool asynchronous, MSG_PANE_CHANGED_NOTIFY_CODE code, int32 value);

  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean isCommandSelected(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual char* commandToString(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

  /* posted when the message being displayed in this view has changed. */
  static const char *messageHasChanged;
  
  /* posted when we get a notice that the last msg in a folder was deleted. */
  static const char *lastMsgDeleted;
  
  /* posted when the user hits space at the end of the message */
  static const char *spacebarAtMsgBottom;
private:
  /* True: MsgFrame is deleted; 
   * False (default): otherwise
   */
  XP_Bool              m_frameDeleted; 

  /* True: notify thread to update itself!
   * False (default): otherwise
   */
  XP_Bool              m_updateThread;

  XFE_HTMLView        *m_htmlView;
  XFE_ReadAttachPanel *m_attachPanel;
    
	XFE_PopupMenu *m_popup;

  /* This gets called when allConnectionsComplete happens
   */
  XFE_CALLBACK_DECL(allConnectionsComplete)

  /* This gets called when the user hits space at the end of the message. */
  XFE_CALLBACK_DECL(spaceAtMsgEnd)

  /* This gets called when the user modifies preferences that require
	 the message views to refresh. */
  XFE_CALLBACK_DECL(refresh)

  /* This gets called on right mouse button events in our HTMLView */
  XFE_CALLBACK_DECL(showPopup)

  // called as attachments are processed
  static void AttachmentCountCb(MSG_Pane*,void*,int32,XP_Bool);
  void attachmentCountCb(int32,XP_Bool);
  // called when user clicks attachment icon in message header
  static void ToggleAttachmentPanelCb(MSG_Pane*,void*);
  void toggleAttachmentPanelCb();

  // remember application-set or user-set height of attachment panel
  int m_attachApplHeight;
  int m_attachUserHeight;

  void setAttachPrefHeight();
    
  MSG_FolderInfo *m_folderInfo;
  MessageKey m_messageKey;

	static MenuSpec separator_spec[];
	static MenuSpec openLinkNew_spec[];
	static MenuSpec openLinkEdit_spec[];

	static MenuSpec repl_spec[];

	static MenuSpec showImage_spec[];
	static MenuSpec stopLoading_spec[];
	static MenuSpec openImage_spec[];
	static MenuSpec addLinkBookmark_spec[];
	static MenuSpec addBookmark_spec[];
	static MenuSpec saveLink_spec[];
	static MenuSpec saveImage_spec[];
	static MenuSpec copy_spec[];
	static MenuSpec copyLink_spec[];
	static MenuSpec copyImage_spec[];
	static MenuSpec addr_spec[];
	static MenuSpec filemsg_spec[];

	static MenuSpec addToAddrbk_submenu_spec[];
};

#endif
