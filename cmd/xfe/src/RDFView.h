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
   RDFView.h -- class definition for RDFView
   Created: Stephen Lamm <slamm@netscape.com>, 5-Nov-97.
 */



#ifndef _xfe_rdfview_h
#define _xfe_rdfview_h

#include "View.h"
#include "IconGroup.h"
#include "Outliner.h"
#include "Outlinable.h"
#include "htrdf.h"

//#include "PopupMenu.h"

class XFE_RDFView : public XFE_View, public XFE_Outlinable
{
public:
  XFE_RDFView(XFE_Component *toplevel, Widget parent,
              XFE_View *parent_view, MWContext *context, HT_View htview);

  ~XFE_RDFView();

  // Methods we override from XFE_View
  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual XP_Bool isCommandSelected(CommandType cmd, void *calldata,
                                    XFE_CommandInfo* info);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  
  //void setClipContents(void *block, int32 length);
  //void *getClipContents(int32 *length);
  //void freeClipContents();

  // The Outlinable interface.
  virtual void *ConvFromIndex(int index);
  virtual int ConvToIndex(void *item);

  virtual char *getColumnName(int column);

  virtual char *getColumnHeaderText(int column);
  virtual fe_icon *getColumnHeaderIcon(int column);
  virtual EOutlinerTextStyle getColumnHeaderStyle(int column);
  virtual void *acquireLineData(int line);
  virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, 
                           int *depth, OutlinerAncestorInfo **ancestor);
  virtual EOutlinerTextStyle getColumnStyle(int column);
  virtual char *getColumnText(int column);
  virtual fe_icon *getColumnIcon(int column);
  virtual void releaseLineData();

  virtual void Buttonfunc(const OutlineButtonFuncData *data);
  virtual void Flippyfunc(const OutlineFlippyFuncData *data);

  virtual XFE_Outliner *getOutliner();

  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */) {return NULL;}
  virtual char *getCellDocString(int /* row */, int /* column */) {return NULL;}

  // Open properties dialog
  //void openPropertiesWindow();
  //void closePropertiesWindow();

  // RDF Specific calls
  void setRDFView(HT_View view);

private:
  //HT_Pane m_Pane;		// The pane that owns this view
  HT_View m_rdfview;		// The view as registered in the hypertree
  HT_Resource m_node;      // needed by the outlinable interface methods.

  int m_nodeDepth;         // needed by the outlinable interface methods.
  OutlinerAncestorInfo *m_ancestorInfo; // needed by the outlinable interface methods.

  XFE_Outliner *m_outliner; // the outliner object used to display everything.

  //XFE_PopupMenu *m_popup;

  // icons for use in the bookmark window.
  static fe_icon bookmark;
  static fe_icon openedFolder;
  static fe_icon closedFolder;

#ifdef NOTYET
  void dropfunc(Widget dropw, fe_dnd_Event type, fe_dnd_Source *source, XEvent *event);
  static void drop_func(Widget dropw, void *closure, fe_dnd_Event type,
			fe_dnd_Source *source, XEvent* event);
  //void doPopup(XEvent *event);

  static fe_icon mailBookmark;
  static fe_icon newsBookmark;
  static fe_icon changedBookmark;
  //  static fe_icon unknownBookmark;
  static fe_icon closedPersonalFolder;
  static fe_icon openedPersonalFolder;
  static fe_icon closedFolderDest;
  static fe_icon openedFolderDest;
  static fe_icon closedFolderMenu;
  static fe_icon openedFolderMenu;
  static fe_icon closedFolderMenuDest;
  static fe_icon openedFolderMenuDest;
#endif /*NOTYET*/
};

#endif /* _xfe_rdfview_h */
