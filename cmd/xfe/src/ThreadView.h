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
   ThreadView.h -- class definition for ThreadView
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_threadview_h
#define _xfe_threadview_h

#include "MNListView.h"
#include "MNBanner.h"
#include "MsgView.h"
#include "Outlinable.h"
#include "msgcom.h"

// commands that are executed after the currently running
// command is through.  That is, when allConnectionsComplete is called.
enum PendingCommand {
	noPendingCommand,
	invalidateThreadAndSelection, /* Used to update the treeinfo and selection for the thread
									 containing the deleted message. */
	getNewMessages,   /* Used in the thread window to do a get new mail after
						 the summary file is created. */
	selectByIndex,
	selectByKey,
	selectFirstUnread,
	selectLastUnread,

	scrollToFirstNew      /* used to scroll the outliner to the first new message,
							  after a get new mail */
};

#define HANDLE_CMD_QUEUE 1

/* support 5.0 delete model
 */
#define DEL_5_0 1

class XFE_ThreadView : public XFE_MNListView
{
public:
  XFE_ThreadView(XFE_Component *toplevel_component, Widget parent, 
		 XFE_View *parent_view, MWContext *context,
		 MSG_Pane *p = NULL);

  virtual ~XFE_ThreadView();

  void selectTimer();

  virtual void paneChanged(XP_Bool asynchronous, 
						   MSG_PANE_CHANGED_NOTIFY_CODE code, int32 value);

#if defined(DEL_5_0)
  void listChangeStarting(XP_Bool asynchronous, MSG_NOTIFY_CODE notify, 
						  MSG_ViewIndex where, int32 num);
#endif /* DEL_5_0 */
  virtual void listChangeFinished(XP_Bool asynchronous, MSG_NOTIFY_CODE notify, 
								  MSG_ViewIndex where, int32 num);

  void loadFolder(MSG_FolderInfo *folderInfo);
  MSG_FolderInfo *getFolderInfo();

  void showMessage(int row);

  virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean isCommandSelected(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

  virtual char *commandToString(CommandType command, 
								void *calldata = NULL,
								XFE_CommandInfo* i = NULL);

  /* This is static so the message banner can get at it. */
  static fe_icon* flagToIcon(int folder_flags, int message_flags);
#if defined(USE_MOTIF_DND)
  static fe_icon_data* flagToIconData(int folder_flags, int message_flags);
#endif /* USE_MOTIF_DND */
  static void initMessageIcons(Widget widget, Pixel bg_pixel, Pixel fg_pixel);

  /* Outlinable interface methods */
  virtual void *ConvFromIndex(int index);
  virtual int ConvToIndex(void *item);

  virtual char *getColumnName(int column);

  virtual char *getColumnHeaderText(int column);
  virtual fe_icon *getColumnHeaderIcon(int column);
  virtual EOutlinerTextStyle getColumnHeaderStyle(int column);
  virtual void *acquireLineData(int line);
  virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth, OutlinerAncestorInfo **ancestor);
  virtual EOutlinerTextStyle getColumnStyle(int column);
  virtual char *getColumnText(int column);
  virtual fe_icon *getColumnIcon(int column);
  virtual void releaseLineData();
  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */);
  virtual char *getCellDocString(int /* row */, int /* column */);
  char *getColumnTextByMsgLine(MSG_MessageLine* msgLine, int column);

#if defined(USE_MOTIF_DND)
  /* motif drag and drop interface. */
  fe_icon_data *GetDragIconData(int row, int column);
  static fe_icon_data *getDragIconData(void *this_ptr,
									   int row, int column);

  void GetDragTargets(int row, int column, Atom **targets, int *num_targets);
  static void getDragTargets(void *this_ptr,
							 int row, int col,
							 Atom **targets, int *num_targets);

  char *DragConvert(Atom atom);
  static char* dragConvert(void *this_ptr,
						   Atom atom);
#endif /* USE_MOTIF_DND */

  void handlePendingCommand();
  XFE_CALLBACK_DECL(allConnectionsComplete)

  virtual void Buttonfunc(const OutlineButtonFuncData *data);
  virtual void Flippyfunc(const OutlineFlippyFuncData *data);

#if !defined(USE_MOTIF_DND)
  // needs to be public so the ProxyIcon can get at it.
  static void source_drop_func(fe_dnd_Source *src, fe_dnd_Message msg, void *closure);
#endif /* USE_MOTIF_DND */

  void openWithKey(MessageKey key);
  void setPendingCmdSelByKey(PendingCommand cmd, MessageKey key);

  // get new msg ?
  void setGetNewMsg(XP_Bool b);

#if HANDLE_CMD_QUEUE
  void processCmdQueue();
#endif /* HANDLE_CMD_QUEUE */

private:

  /* keep track of on loading folders
   */
  int m_nLoadingFolders;

#if HANDLE_CMD_QUEUE

#if !defined(HANDLE_LIST_CHANGED)
	/* will be promoted to MNListView.h */
  MSG_ViewIndex m_lineChanged;
#endif /* HANDLE_LIST_CHANGED */

  MSG_ViewIndex m_lastLoadedInd;  /* which line is being displayed */
#if defined(DEL_5_0)
  MessageKey    m_lastLoadedKey; /* which message is being displayed */
#endif /* DEL_5_0 */
  int     *m_selected;    /* from msgdeleted; */
  int      m_selectedCount;

  int     *m_deleted; /* from lsitchange  */
  int      m_deletedCount;

  /* This is really stupid..
   */
  int     *m_collapsed;
  int      m_collapsedCount;
  void     addCollapsed(int i);
  XP_Bool  isCollapsed(int i);

#endif /* HANDLE_CMD_QUEUE */

  /* Hack: use this flag to determine if we need to getNewMessage
   */
  XP_Bool m_getNewMsg;

  /* True: ThreadFrame is deleted; 
   * False (default): otherwise
   */
  XP_Bool m_frameDeleted;

  XP_Bool m_displayingDraft;

  PendingCommand m_commandPending;

  MSG_ViewIndex m_selectionAfterDeleting;
  MSG_ViewIndex m_pendingSelectionIndex;
  MessageKey m_pendingSelectionKey;

  Widget m_arrowb, m_arrowlabel, m_arrowform;

  XFE_MsgView *m_msgview;
  XP_Bool m_msgExpanded;

  MSG_FolderInfo *m_folderInfo;

  MSG_MessageLine m_messageLine;
  OutlinerAncestorInfo *m_ancestorInfo;
  
  static const int OUTLINER_COLUMN_SUBJECT;
  static const int OUTLINER_COLUMN_UNREADMSG;
  static const int OUTLINER_COLUMN_SENDERRECIPIENT;
  static const int OUTLINER_COLUMN_DATE;
  static const int OUTLINER_COLUMN_PRIORITY;
  static const int OUTLINER_COLUMN_SIZE;
  static const int OUTLINER_COLUMN_STATUS;
  static const int OUTLINER_COLUMN_FLAG;

  XFE_CALLBACK_DECL(spaceAtMsgEnd)
  XFE_CALLBACK_DECL(newMessageLoading)
  void updateExpandoFlippyText(int row);
  void toggleMsgExpansion();
  static void toggleMsgExpansionCallback(Widget, XtPointer, XtPointer);

#if !defined(USE_MOTIF_DND)
  void dropfunc(Widget dropw, fe_dnd_Event type, fe_dnd_Source *source, XEvent *event);
  static void drop_func(Widget dropw, void *closure, fe_dnd_Event type,
			fe_dnd_Source *source, XEvent* event);

  void sourcedropfunc(fe_dnd_Source *src, fe_dnd_Message msg, void *closure);
#endif /* !USE_MOTIF_DND */

  // icons for the outliner
  static fe_icon threadonIcon;
  static fe_icon threadoffIcon;

  XFE_PopupMenu *m_popup;

  static MenuSpec mail_popup_spec[];
  static MenuSpec news_popup_spec[];
  static MenuSpec priority_popup_submenu[];
  static MenuSpec addrbk_submenu_spec[];

  void selectThread();

  /* select timer
   */
  XtIntervalId  m_scrollTimer;
  MSG_ViewIndex m_targetIndex;
};

#endif /* _xfe_threadview_h */
