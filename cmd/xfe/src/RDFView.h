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
#include "htrdf.h"

#include "PopupMenu.h"

class XFE_RDFPopupMenu : public XFE_SimplePopupMenu
{
public:
  XFE_RDFPopupMenu(String name, Widget parent,
                   HT_View view, 
                   Boolean isWorkspace, Boolean isBackground);

  void PushButtonActivate(Widget w, XtPointer userData);

protected:
  HT_Pane m_pane;
};

class XFE_RDFView : public XFE_View
{
public:
  XFE_RDFView(XFE_Component *toplevel, Widget parent,
              XFE_View *parent_view, MWContext *context, XP_Bool isStandalone);

  ~XFE_RDFView();

  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */) {return NULL;}
  virtual char *getCellDocString(int /* row */, int /* column */) {return NULL;}

  // Refresh the outliner
  void refresh(HT_Resource node);

  // Open properties dialog
  //void openPropertiesWindow();
  //void closePropertiesWindow();

  // RDF Specific calls
  void notify(HT_Notification ns, HT_Resource n, HT_Event whatHappened);

  void setRDFView(HT_View view);

  void doPopup(XEvent *event);

private:
  //HT_Pane m_Pane;		// The pane that owns this view
  HT_View m_rdfview;		// The view as registered in the hypertree

  XFE_RDFPopupMenu *m_popup;

  // icons for use in the bookmark window.
  static fe_icon bookmark;
  static fe_icon openedFolder;
  static fe_icon closedFolder;

  void init_pixmaps();

  void fill_tree();
  void destroy_tree();

  void add_row(HT_Resource node);
  void add_row(int node);
  void delete_row(int row);
  void add_column(int index, char *name, uint32 width,
                  void *token, uint32 token_type);

  void expand_row(int row);
  void collapse_row(int row);
  void delete_column(HT_Resource cursor);
  void activate_row(int row);
  void resize(XtPointer);
  void edit_cell(XtPointer);
  void select_row(int row);
  void deselect_row(int row);

  static void expand_row_cb(Widget, XtPointer, XtPointer);
  static void collapse_row_cb(Widget, XtPointer, XtPointer);
  static void delete_cb(Widget, XtPointer, XtPointer);
  static void activate_cb(Widget, XtPointer, XtPointer);
  static void resize_cb(Widget, XtPointer, XtPointer);
  static void edit_cell_cb(Widget, XtPointer, XtPointer);
  static void deselect_cb(Widget, XtPointer, XtPointer);
  static void select_cb(Widget, XtPointer, XtPointer);
  static void popup_cb(Widget, XtPointer, XtPointer);

#ifdef NOTYET
  void dropfunc(Widget dropw, fe_dnd_Event type, fe_dnd_Source *source, XEvent *event);
  static void drop_func(Widget dropw, void *closure, fe_dnd_Event type,
			fe_dnd_Source *source, XEvent* event);

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
