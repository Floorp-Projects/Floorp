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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		RDFTreeView.h       									*/
/* Description:	XFE_RDFTreeView component header file - A class to     	*/
/*              encapsulate and rdf tree widget.					    */
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _xfe_rdf_tree_view_h_
#define _xfe_rdf_tree_view_h_

#include "View.h"
#include "IconGroup.h"
#include "htrdf.h"
#include "NavCenterView.h"
#include "utf8xfe.h"

//////////////////////////////////////////////////////////////////////////
class XFE_ColumnData
{
public:
  XFE_ColumnData(void) : token(NULL), token_type(0) {}
  XFE_ColumnData(void *t, uint32 tt) : token(t), token_type(tt) {}

  void* token;
  uint32 token_type;
};
//////////////////////////////////////////////////////////////////////////
class XFE_RDFPopupMenu;

class XFE_RDFTreeView : public XFE_View,
                        public XFE_RDFBase
{
public:
  XFE_RDFTreeView(XFE_Component *toplevel, Widget parent,
              XFE_View *parent_view, MWContext *context);

  ~XFE_RDFTreeView();

  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /*row*/, int /*column*/) {return NULL;}
  virtual char *getCellDocString(int /*row*/, int /*column*/) {return NULL;}

  // Refresh the outliner
  void refresh(HT_Resource node);
  void updateRoot();

  // Open properties dialog
  //void openPropertiesWindow();
  //void closePropertiesWindow();

  // Override RDFBase method
  void notify(HT_Resource n, HT_Event whatHappened);

	
	void doPopup(XEvent *event);

	// Stand alone set/get methods
	void setStandAlone(XP_Bool);
	XP_Bool isStandAlone() { return _isStandAlone; }

    // Set HT Properties
    void setHTTreeViewProperties(HT_View); 

    void            setColumnData   (int column, void *token, uint32 tkntype);
    XFE_ColumnData *getColumnData   (int column);

protected:
    // The XmL tree widget
    Widget                  _tree;
    UTF8ToXmStringConverter *_tree_utf8_converter;
    FontListNotifier        *_tree_fontlist_notifier;

	virtual Widget	getTreeParent	();
	virtual void	doAttachments	();
	virtual void	createTree		();

private:

	// The popup menu
	XFE_RDFPopupMenu *		_popup;

	// Is this a stand alone view ?
	XP_Bool					_isStandAlone;

  // icons for use in the bookmark window.
  static fe_icon bookmark;
  static fe_icon openedFolder;
  static fe_icon closedFolder;


  void init_pixmaps();

  // Set editing behavior and label.
  void initCell(HT_Resource node, int row, int column);

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
  void sort_column(int column);
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

#endif // _xfe_rdf_tree_view_h_
