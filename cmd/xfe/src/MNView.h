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
   MNView.h - anything that has a MSG_Pane in it. (mail/news views.)
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_mnview_h
#define _xfe_mnview_h

#include "View.h"
#include "Command.h"
#include "msgcom.h"
#include "xfe.h"
#include "icons.h"

class XFE_MNView : public XFE_View
{
public:
  XFE_MNView(XFE_Component *toplevel_component, XFE_View *parent_view, MWContext *context, MSG_Pane *p);
  virtual ~XFE_MNView();

  MSG_Pane *getPane();
  void setPane(MSG_Pane *new_pane);

  void destroyPane();

	/* the one command method we have. */
	char *commandToString(CommandType cmd, void *calldata, XFE_CommandInfo *info);

  virtual void paneChanged(XP_Bool asynchronous, MSG_PANE_CHANGED_NOTIFY_CODE notify_code, int32 value);

	virtual void updateCompToolbar();
  /* used by toplevel to see which view can handle a command.  Returns true
     if we can handle it. */
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL,
                                 XFE_CommandInfo* i = NULL);

  /* this method is used by the toplevel to dispatch a command. */
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
                                                 XFE_CommandInfo* i = NULL);


  XP_Bool isDisplayingNews();

  static MSG_BIFF_STATE getBiffState();
  static void setBiffState(MSG_BIFF_STATE state);

  static MSG_Master *getMaster();
  static void destroyMasterAndShutdown(); // only do this if we're shutting down the application

  static const char *bannerNeedsUpdating;  // notify the parent frame that the MNBanner needs updating.
  static const char *foldersHaveChanged;
  static const char *newsgroupsHaveChanged;
  static const char *msgWasDeleted;    // in case we need to close a frame 
  static const char *folderDeleted;    // in case we need to close a frame 

  // these next two are useful in updating more than one window's chrome.
  static const char *folderChromeNeedsUpdating;
  static const char *MNChromeNeedsUpdating;

  // icons used in the mail/news outliners and proxy icons

  // special folder icons
  static fe_icon inboxIcon;
  static fe_icon inboxOpenIcon;
  static fe_icon draftsIcon;
  static fe_icon draftsOpenIcon;
  static fe_icon filedMailIcon;
  static fe_icon filedMailOpenIcon;
  static fe_icon outboxIcon;
  static fe_icon outboxOpenIcon;
  static fe_icon trashIcon;
  static fe_icon folderIcon;
  static fe_icon folderOpenIcon;
  static fe_icon folderServerIcon;
  static fe_icon folderServerOpenIcon;
  static fe_icon newsgroupIcon;

  // server icons
  static fe_icon mailLocalIcon;
  static fe_icon mailServerIcon;
  static fe_icon newsServerSecureIcon;
  static fe_icon newsServerIcon;

  // message icons
  static fe_icon mailMessageReadIcon;
  static fe_icon mailMessageUnreadIcon;
  static fe_icon newsPostIcon;
  static fe_icon draftIcon;
  static fe_icon newsNewIcon;
  static fe_icon msgReadIcon;
  static fe_icon msgUnreadIcon;
  static fe_icon deletedIcon;
  static fe_icon msgFlagIcon;

	// thread selection icons;
	static fe_icon openSpoolIgnoredIcon;
	static fe_icon closedSpoolIgnoredIcon;
	static fe_icon openSpoolWatchedIcon;
	static fe_icon closedSpoolWatchedIcon;
	static fe_icon openSpoolNewIcon;
	static fe_icon closedSpoolNewIcon;
	static fe_icon openSpoolIcon;
	static fe_icon closedSpoolIcon;

  static fe_icon collectionsIcon;
protected:
  MSG_Pane *m_pane;

	/* useful in both the threadview and msgview. */
	XP_Bool m_displayingNewsgroup;

  static MSG_BIFF_STATE m_biffstate;

  static XP_Bool m_messageDownloadInProgress;
  static int 	m_numFolderBeingLoaded; // This is controlled by Thread View only
					// Indicating how many folder being
					// loaded in the meantime

  static MSG_Master *m_master;
  static MSG_Prefs *m_prefs;

  virtual MSG_MotionType commandToMsgNav(CommandType cmd);
  virtual MSG_CommandType commandToMsgCmd(CommandType cmd);
  virtual MSG_PRIORITY commandToPriority(CommandType cmd);

  virtual char *priorityToString(MSG_PRIORITY priority);

  virtual void getNewNews();
  virtual void getNewMail();
  virtual void markReadByDate();

  // update the biff desktop icon
  XFE_CALLBACK_DECL(updateBiffState)
};

extern "C" MSG_Master *fe_getMNMaster();

#endif /* _xfe_mnview_h */
