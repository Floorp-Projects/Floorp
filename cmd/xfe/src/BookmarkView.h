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
   BookmarkView.h -- class definition for BookmarkView
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_bookmarkview_h
#define _xfe_bookmarkview_h

#include "View.h"
#include "Outliner.h"
#include "Outlinable.h"
#include "PopupMenu.h"
#include "BookmarkPropDialog.h"
#include "BookmarkWhatsNewDialog.h"
#include "bkmks.h"

typedef struct BookmarkClipboard {
  void *block;
  int32 length;
} BookmarkClipboard;

class XFE_BookmarkView : public XFE_View, public XFE_Outlinable
{
public:
  XFE_BookmarkView(XFE_Component *toplevel, Widget parent, XFE_View *parent_view, MWContext *context);

  virtual ~XFE_BookmarkView();

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
  
  // Our equivalent to the BMFE_* calls
  void refreshCells(int first = 1, int last = BM_LAST_CELL,
                    XP_Bool now = TRUE);
  void gotoBookmark(const char *url);
  void scrollIntoView(BM_Entry *entry);
  void setClipContents(void *block, int32 length);
  void *getClipContents(int32 *length);
  void freeClipContents();
  void startWhatsChanged();
  void updateWhatsChanged(const char *url, int32 done, int32 total, const char *totaltime);
  void finishedWhatsChanged(int32 totalchecked, int32 numreached, int32 numchanged);
  void startBatch();
  void endBatch();
  void bookmarkMenuInvalid();

  // Wrapper around BM_SetName() that handles personal toolbar folder
  void setName(BM_Entry *entry, char *name);

  Boolean loadBookmarks(char *filename);
  Boolean copyBookmarksFile(char *dst, char *src);

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

  // columns for the Outliner
  static const int OUTLINER_COLUMN_NAME;
  static const int OUTLINER_COLUMN_LOCATION;
  static const int OUTLINER_COLUMN_LASTVISITED;
  static const int OUTLINER_COLUMN_CREATEDON;

  static const char *bookmarksChangedCallback; // the user has added/editted/deleted a bookmark entry.
  static const char *bookmarkDoubleClickedCallback;  // the user has double clicked a bookmark entry.
  static const char *bookmarkClickedCallback; // the user has single clicked a bookmark entry.

  // Open properties dialog
  void openBookmarksWindow();
  void closeBookmarksWindow();

  // Edit an item in the properties dialog if it's open
  void editItem(BM_Entry *entry);
  void entryGoingAway(BM_Entry *entry);

  void makeEditItemDialog();

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
  
  int ProcessTargets(int row, int col,
					 Atom *targets,
					 const char **data,
					 int numItems);
  static int processTargets(void *this_ptr,
							int row, int col,
							Atom *targets,
							const char **data,
							int numItems);

  /* no instance method needed for getDropTargets */
  static void getDropTargets(void *this_ptr,
							 Atom **targets, int *num_targets);
#endif /* USE_MOTIF_DND */

private:
  BM_Entry *m_entry;                    // needed by the outlinable interface methods.
  int m_entryDepth;                     // needed by the outlinable interface methods.
  OutlinerAncestorInfo *m_ancestorInfo; // needed by the outlinable interface methods.

  XFE_Outliner *m_outliner; // the outliner object used to display everything.
  XFE_PopupMenu *m_popup;

  XP_Bool m_sortDescending;
  CommandType m_lastSort;

  BookmarkClipboard clip;   // our clipboard.
  BM_Entry *m_editentry;    // the entry currently being editted
  
  XFE_BookmarkPropDialog *m_propertiesDialog;
  XFE_BookmarkWhatsNewDialog *m_whatsNewDialog;

  // Use these to avoid multiple FE updates
  XP_Bool m_batchDepth;
  XP_Bool m_menuIsInvalid;

  static MenuSpec open_popup_spec[];
  static MenuSpec new_popup_spec[];
  static MenuSpec set_popup_spec[];
  static MenuSpec saveas_popup_spec[];
  static MenuSpec cutcopy_popup_spec[];
  static MenuSpec copylink_popup_spec[];
  static MenuSpec paste_popup_spec[];
  static MenuSpec delete_popup_spec[];
  static MenuSpec makealias_popup_spec[];
  static MenuSpec properties_popup_spec[];

  // Remove our reference to the dialog when it goes away
  static void properties_destroy_cb(Widget widget,
                                    XtPointer closure, XtPointer call_data);
  static void whats_new_destroy_cb(Widget widget,
                                    XtPointer closure, XtPointer call_data);

  BM_CommandType commandToBMCmd(CommandType cmd);

#if !defined(USE_MOTIF_DND)

  void dropfunc(Widget dropw, fe_dnd_Event type, fe_dnd_Source *source, XEvent *event);
  static void drop_func(Widget dropw, void *closure, fe_dnd_Event type,
			fe_dnd_Source *source, XEvent* event);
#endif

  void doPopup(XEvent *event);

  // icons for use in the bookmark window.
  static fe_icon bookmark;
  static fe_icon mailBookmark;
  static fe_icon newsBookmark;
  static fe_icon changedBookmark;
  //  static fe_icon unknownBookmark;
  static fe_icon closedFolder;
  static fe_icon openedFolder;
  static fe_icon closedPersonalFolder;
  static fe_icon openedPersonalFolder;
  static fe_icon closedFolderDest;
  static fe_icon openedFolderDest;
  static fe_icon closedFolderMenu;
  static fe_icon openedFolderMenu;
  static fe_icon closedFolderMenuDest;
  static fe_icon openedFolderMenuDest;
};

#endif /* _xfe_bookmarkview_h */
