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
   BookmarkWhatsNewDialog.h -- dialog for checking "What's New".
   Created: Stephen Lamm <slamm@netscape.com>, 28-May-97.
 */



#ifndef _xfe_bookmarkwhatsnewdialog_h
#define _xfe_bookmarkwhatsnewdialog_h

#include "Dialog.h"
#include "bkmks.h"

class XFE_BookmarkWhatsNewDialog : public XFE_Dialog
{
public:
  XFE_BookmarkWhatsNewDialog(MWContext *context, Widget parent);

  virtual ~XFE_BookmarkWhatsNewDialog();

  void updateWhatsChanged(const char *url, int32 done, int32 total, const char *totaltime);
  void finishedWhatsChanged(int32 totalchecked, int32 numreached, int32 numchanged);

private:
  void close();
  void apply();
  void ok();
  
  static void destroy_cb(Widget widget,
                         XtPointer closure, XtPointer call_data);
  static void close_cb(Widget, XtPointer, XtPointer);
  static void apply_cb(Widget, XtPointer, XtPointer);
  static void ok_cb(Widget, XtPointer, XtPointer);

  MWContext *m_context;

  Widget m_text;

  Widget m_radioBox;
  Widget m_doAll;
  Widget m_doSelected;
};


#endif /* xfe_bookmarkwhatsnewdialog_h */
