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
   HistoryView.h -- class definition for HistoryView
   Created: Stephen Lamm <slamm@netscape.com>, 24-Feb-96.
 */



#ifndef _xfe_historyview_h
#define _xfe_historyview_h

#include "View.h"
#include "Outliner.h"
#include "Outlinable.h"

#include "glhist.h"
#include "xp.h"

enum enHistSortCol
{
   eHSC_INVALID = -1,
   eHSC_TITLE,
   eHSC_LOCATION,
   eHSC_FIRSTVISIT,
   eHSC_LASTVISIT,
   eHSC_EXPIRES,
   eHSC_VISITCOUNT
};

typedef struct HistoryClipboard {
  void *block;
  int32 length;
} HistoryClipboard;

class XFE_HistoryView : public XFE_View, public XFE_Outlinable
{
public:
  XFE_HistoryView(XFE_Component *toplevel, Widget parent, XFE_View *parent_view, MWContext *context);

  ~XFE_HistoryView();

  // Methods we override from XFE_View
  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual XP_Bool isCommandSelected(CommandType cmd,
                                    void *calldata = NULL,
                                    XFE_CommandInfo* info = NULL);
  
  // Our equivalent to the backend calls
  void refreshCells(int first, int last, XP_Bool now);
  void scrollIntoView(gh_HistEntry *entry);
  void setClipContents(void *block, int32 length);
  void *getClipContents(int32 *length);
  void freeClipContents();
  //  void openHistoryWindow();

  // public access to history list
  virtual gh_HistEntry *getEntry(int);

  //Boolean loadHistory(char *filename);

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
  static const int OUTLINER_COLUMN_TITLE;
  static const int OUTLINER_COLUMN_LOCATION;
  static const int OUTLINER_COLUMN_FIRSTVISITED;
  static const int OUTLINER_COLUMN_LASTVISITED;
  static const int OUTLINER_COLUMN_EXPIRES;
  static const int OUTLINER_COLUMN_VISITCOUNT;

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
#endif /* USE_MOTIF_DND */

  static const char *historysChangedCallback; // the user has added/editted/deleted a history entry.
  static const char *historyDoubleClickedCallback;  // the user has double clicked a history entry.
  static const char *historyClickedCallback; // the user has single clicked a history entry.

private:
  GHHANDLE m_histCursor;
  int m_totalLines;

  XP_Bool m_dirty;

  enHistSortCol m_sortBy;
  XP_Bool       m_sortDescending;
  gh_Filter*    m_filter;

  // needed by the outlinable interface methods.
  OutlinerAncestorInfo *m_ancestorInfo;
  gh_HistEntry *m_entry;

#if defined(USE_MOTIF_DND)
  /* the entry that we're currently dragging. */
  gh_HistEntry *m_dragentry;
#endif /* USE_MOTIF_DND */

  XFE_Outliner *m_outliner;

  XP_List *m_selectList;

  // icons for use in the history window.
  static fe_icon historyIcon;

  void dropfunc(Widget dropw, fe_dnd_Event type,
                fe_dnd_Source *source, XEvent *event);
  static void drop_func(Widget dropw, void *closure, fe_dnd_Event type,
                        fe_dnd_Source *source, XEvent* event);

  gh_HistEntry *getSelection();

  void openSelected();
  void reflectSelected();
  void saveSelected();
  void restoreSelected();

  int notify(gh_NotifyMsg *msg);
  static int notify_cb(gh_NotifyMsg *msg);

  void resort(enHistSortCol sortBy, XP_Bool sortDescending);

  void refresh();
  static void refresh_cb(XtPointer closure, XtIntervalId *id);
};

#endif /* _xfe_historyview_h */
