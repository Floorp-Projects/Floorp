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
   ThreePaneView.h -- class definition for XFE_ThreePaneView
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_threepaneview_h
#define _xfe_threepaneview_h

#include "FolderView.h"
#include "ThreadView.h"
#include "View.h"
#include "Command.h"

class XFE_ThreePaneView : public XFE_View
{
public:
  XFE_ThreePaneView(XFE_Component *toplevel_component, Widget parent, 
		     XFE_View *parent_view, MWContext *context);

  virtual ~XFE_ThreePaneView();

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

  static const char *ShowFolder;
  void selectFolder(MSG_FolderInfo *info);
  XFE_View* getThreadView();
  XFE_View* getFolderView();

  XFE_CALLBACK_DECL(userClickedFolder)
  XFE_CALLBACK_DECL(twoPaneView)
  XFE_CALLBACK_DECL(changeFocus)

private:

  XFE_MNListView *m_focusview;
  XFE_FolderView *m_folderview;
  XFE_ThreadView *m_threadview;
};

#endif /* _xfe_folderview_h */
