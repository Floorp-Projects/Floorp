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
   MNListView.h - Mail/News panes that are displayed using an outliner.
   Created: Chris Toshok <toshok@netscape.com>, 25-Aug-96.
 */



#ifndef _xfe_mnlistview_h
#define _xfe_mnlistview_h

#include "MNView.h"
#include "Command.h"
#include "msgcom.h"
#include "Outlinable.h"

/* This class implements one of the methods of XFE_Outlinable,
   getOutliner().  It leaves all the others abstract. */
class XFE_MNListView : public XFE_MNView, public XFE_Outlinable
{
public:
  XFE_MNListView(XFE_Component *toplevel_component, XFE_View *parent_view, MWContext *context, MSG_Pane *p);
  virtual ~XFE_MNListView();

  static const char *changeFocus;

  // We're nice to our subclasses :)
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean isCommandSelected(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  
  // The Outlinable interface.
  virtual void *ConvFromIndex(int index) = 0;
  virtual int ConvToIndex(void *item) = 0;
  virtual char *getColumnName(int column) = 0;
  virtual char *getColumnHeaderText(int column) = 0;
  virtual fe_icon *getColumnHeaderIcon(int column) = 0;
  virtual EOutlinerTextStyle getColumnHeaderStyle(int column) = 0;
  virtual void *acquireLineData(int line) = 0;
  virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth, 
			   OutlinerAncestorInfo **ancestor) = 0;
  virtual EOutlinerTextStyle getColumnStyle(int column) = 0;
  virtual char *getColumnText(int column) = 0;
  virtual fe_icon *getColumnIcon(int column) = 0;

  virtual void Buttonfunc(const OutlineButtonFuncData *data) = 0;
  virtual void Flippyfunc(const OutlineFlippyFuncData *data) = 0;

  virtual void releaseLineData() = 0;

  // we implement this one for all subclasses.
  virtual XFE_Outliner *getOutliner();
  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */) {return NULL;}
  virtual char *getCellDocString(int /* row */, int /* column */) {return NULL;}

  void setInFocus(XP_Bool infocus);
  XP_Bool isFocus();

protected:

  XFE_Outliner *m_outliner;
};


#endif /* _xfe_mnview_h */

