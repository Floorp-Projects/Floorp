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
   FolderView.h -- class definition for FolderView
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_folderview_h
#define _xfe_folderview_h

#include "MNListView.h"
#include "Outlinable.h"
#include "PopupMenu.h"
#include "Command.h"

class XFE_FolderView : public XFE_MNListView
{
public:
  XFE_FolderView(XFE_Component *toplevel_component, Widget parent, 
		 XFE_View *parent_view, MWContext *context,
		 MSG_Pane *p = NULL);

  virtual ~XFE_FolderView();

  virtual void paneChanged(XP_Bool asynchronous, MSG_PANE_CHANGED_NOTIFY_CODE code, int32 value);

  // this gets called by our toplevel to let us do some things
  // after it's been realized, but before we're on the screen.
  XFE_CALLBACK_DECL(beforeToplevelShow)

  virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual char *commandToString(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

  void selectFolder(MSG_FolderInfo *info);

#ifdef USE_3PANE
  void processClick();
#endif
  /* This is static so the thread banner can get at it. */
#if defined(USE_MOTIF_DND)
  static fe_icon_data* treeInfoToIconData(int depth, int flags, XP_Bool expanded);
#endif /* USE_MOTIF_DND */
  static fe_icon* treeInfoToIcon(int depth, int flags, XP_Bool expanded, XP_Bool secure = FALSE);
  static void initFolderIcons(Widget widget, Pixel bg_pixel, Pixel fg_pixel);
	
  /* Outlinable interface methods */
  virtual void *ConvFromIndex(int index);
  virtual int ConvToIndex(void *item);

  virtual char *getColumnName(int column);

  virtual char *getColumnHeaderText(int column);
  virtual fe_icon *getColumnHeaderIcon(int column);
  virtual EOutlinerTextStyle getColumnHeaderStyle(int column);
  virtual void *acquireLineData(int line);
  virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth,
			   OutlinerAncestorInfo **ancestor);
  virtual EOutlinerTextStyle getColumnStyle(int column);
  virtual char *getColumnText(int column);
  virtual fe_icon *getColumnIcon(int column);
  virtual void releaseLineData();
  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */);
  virtual char *getCellDocString(int /* row */, int /* column */);

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

  char *getColumnTextByFolderLine(MSG_FolderLine* folderLine, int column);

  virtual void Buttonfunc(const OutlineButtonFuncData *data);
  virtual void Flippyfunc(const OutlineFlippyFuncData *data);

  // notification when a user clicks *once* on a folder. MSG_FolderInfo *is sent in calldata.
  static const char *folderSelected;
  // notification when a user double clicks on a folder.
  static const char *folderDblClicked;
  // notification when a user uses the alternate gesture on a folder.
  static const char *folderAltDblClicked;


private:
  static const int OUTLINER_COLUMN_NAME;
  static const int OUTLINER_COLUMN_UNREAD;
  static const int OUTLINER_COLUMN_TOTAL;

#if !defined(USE_MOTIF_DND)
  void dropfunc(Widget dropw, fe_dnd_Event type, fe_dnd_Source *source, XEvent *event);
  static void drop_func(Widget dropw, void *closure, fe_dnd_Event type,
			fe_dnd_Source *source, XEvent* event);

  void sourcedropfunc(fe_dnd_Source *src, fe_dnd_Message msg, void *closure);
  static void source_drop_func(fe_dnd_Source *src, fe_dnd_Message msg, void *closure);
#endif /* USE_MOTIF_DND */

  void toggleExpansion(int row);

  // for the outlinable stuff
  OutlinerAncestorInfo *m_ancestorInfo;
  MSG_FolderLine m_folderLine;

  // context menu
  XFE_PopupMenu *m_popup;

#ifdef USE_3PANE
  XtIntervalId m_clickTimer;
  XtPointer m_clickData;
  int *m_cur_selected;
  int m_cur_count;

#endif

  static MenuSpec mailserver_popup_menu[];
  static MenuSpec newshost_popup_menu[];
  static MenuSpec mailfolder_popup_menu[];
  static MenuSpec newsgroup_popup_menu[];
};

#endif /* _xfe_folderview_h */
