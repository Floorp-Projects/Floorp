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
   MozillaApp.h -- Class which represents the application
   Created: Chris Toshok <toshok@netscape.com>, 24-Sep-96.
   */



#ifndef _xfe_mozillaapp_h
#define _xfe_mozillaapp_h

#include "xpassert.h"
#include "xp_core.h"
#include "xp_list.h"

#include "Frame.h"
#include "NotificationCenter.h"

class XFE_Frame;

class XFE_MozillaApp : public XFE_NotificationCenter {
public:
  XFE_MozillaApp(int *argc, char **argv);
  XFE_MozillaApp(); // hacked constructor for now.
  
  void exit(int status = 0);
  
  void registerFrame(XFE_Frame *f);
  void unregisterFrame(XFE_Frame *f); // Frame's destructor was called.

  XP_List *getFrameList(EFrameType type);
  XP_List *getAllFrameList();

  int toplevelWindowCount();
  int mailNewsWindowCount();

  XP_Bool getBusy();

  static XFE_MozillaApp *theApp();

  static const char *appBusyCallback;
  static const char *appNotBusyCallback;

  static const char *changeInToplevelFrames;
  static const char *bookmarksHaveChanged;
  static const char *biffStateChanged;
  static const char *updateToolbarAppearance;
  static const char *linksAttributeChanged; 
  static const char *defaultColorsChanged; 
  static const char *defaultFontChanged; 
  static const char *refreshMsgWindow; 
  static const char *personalToolbarFolderChanged; 

  // called when a registered frame is being destroyed.
  XFE_CALLBACK_DECL(frameUnregistering)
  XFE_CALLBACK_DECL(updateBusyState)

private:
  XP_Bool m_isbusy;

  int fe_MNWindowCount;
  XFE_Frame *session_frame;

  XP_List *m_frameList;

  void closeFrames(XP_List *frame_list);
  XP_Bool isOkToExitFrameList(XP_List *frame_list);
  XP_Bool isOkToExitNonMailNewsFrames();
  XP_Bool isOkToExitMailNewsFrames();

  // this is wasteful, but it makes the deletion code _much_ nicer.
  XP_List *m_msgFrameList;
  XP_List *m_threadFrameList;
  XP_List *m_folderFrameList;
  XP_List *m_composeFrameList;
  XP_List *m_searchFrameList;
  XP_List *m_ldapFrameList;
  XP_List *m_editorFrameList;
  XP_List *m_browserFrameList;
  XP_List *m_addressbookFrameList;
  XP_List *m_bookmarkFrameList;
  XP_List *m_mailfilterFrameList;
  XP_List *m_maildownloadFrameList;
  XP_List *m_historyFrameList;
  XP_List *m_downloadFrameList;
  XP_List *m_htmldialogFrameList;
  XP_List *m_navcenterFrameList;

  XP_Bool m_exiting; /* used by unregisterFrame to determine whether the 
			session manager stuff should be updated */
  int     m_exitstatus; /* exit status passed as argument to exit() method. */

  XP_Bool m_actioninstalled; /* if the xfeDoCommand() action has been added. */

  int     m_exitwindowcount;  /* # XFE_Frames currently registered      */
  XP_Bool timeToDie();        /* determine if there are any windows left*/
  void    byebye(int status); /* final, final, final finalization code  */
};

extern "C" XFE_MozillaApp *theMozillaApp();

#endif /* _xfe_mozillaapp_h */
