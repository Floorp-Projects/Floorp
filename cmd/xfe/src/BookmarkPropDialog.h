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
   BookmarkPropDialog.h -- class definitions for edit properties dialog
   Created: Stephen Lamm <slamm@netscape.com>, 10-Mar-97.
 */



#ifndef _xfe_bookmark_prop_dialog_h
#define _xfe_bookmark_prop_dialog_h

#include "Component.h"
#include "Dialog.h"
#include "mozilla.h"  /* for MWContext ! */

class XFE_BookmarkPropDialog : public XFE_Dialog
{
public:

	// Constructors, Destructors

  XFE_BookmarkPropDialog(MWContext *context, Widget parent);

  virtual ~XFE_BookmarkPropDialog();

  void editItem(BM_Entry *entry);
  void entryGoingAway(BM_Entry *entry);
  void selectAlias();
  void close();
  void ok();

  XP_Bool commitChanges();

  //virtual void show();

  //virtual void hide();

  static void selectalias_cb(Widget widget,
                             XtPointer closure, XtPointer call_data);
  static void destroy_cb(Widget widget,
                         XtPointer closure, XtPointer call_data);
  static void close_cb(Widget widget,
                       XtPointer closure, XtPointer call_data);
  static void ok_cb(Widget widget,
                    XtPointer closure, XtPointer call_data);

protected:

  char *getAndCleanText(Widget widget, Boolean new_lines_too_p);

  /* Parent class members
	Widget m_wParent;        // parent widget
	Widget m_chrome;         // dialog chrome - selection box
	Widget m_okButton;
	Widget m_cancelButton;
	Widget m_helpButton;
	Widget m_applyButton;
    */

  MWContext *m_context;

  BM_Entry *m_entry;

  Widget m_title;
  Widget m_nickname;
  Widget m_name;
  Widget m_location;		
  Widget m_locationLabel;
  Widget m_description;
  Widget m_lastVisited;
  Widget m_lastVisitedLabel;
  Widget m_addedOn;
  Widget m_aliasLabel;
  Widget m_aliasButton;
};

#endif /* _xfe_dialog_h */
