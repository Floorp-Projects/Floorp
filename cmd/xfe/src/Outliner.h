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
   Outliner.h -- class definition for the Outliner object
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_outliner_h
#define _xfe_outliner_h

#include "Component.h"

#include "mozilla.h"
#if !defined(USE_MOTIF_DND)
#include "dragdrop.h"
#endif /* USE_MOTIF_DND */
#include "msgcom.h"
#include "icons.h"
#if defined(USE_MOTIF_DND)
#include "icons/icondata.h"
#endif /* USE_MOTIF_DND */
#include "xp_list.h"

#include "XmL/Grid.h"

typedef enum {
  OUTLINER_Leaf,
  OUTLINER_Folded,
  OUTLINER_Expanded
} EOutlinerFlippyType;

typedef enum {
  OUTLINER_Default,
  OUTLINER_Bold,
  OUTLINER_Italic
} EOutlinerTextStyle;

typedef enum 
{
  PIPE_OPENTOPPARENT,
  PIPE_OPENMIDDLEPARENT,
  PIPE_OPENBOTTOMPARENT,
  PIPE_OPENSINGLEPARENT,

  PIPE_CLOSEDTOPPARENT,
  PIPE_CLOSEDMIDDLEPARENT,
  PIPE_CLOSEDBOTTOMPARENT,
  PIPE_CLOSEDSINGLEPARENT,

  PIPE_TOPITEM,
  PIPE_MIDDLEITEM,
  PIPE_BOTTOMITEM,
  PIPE_EMPTYITEM
} EOutlinerPipeType;

typedef enum
{
	OUTLINER_SortAscending,
	OUTLINER_SortDescending
} EOutlinerSortDirection;

class XFE_Outlinable;
#if defined(USE_MOTIF_DND)
class XFE_OutlinerDrop;
class XFE_OutlinerDrag;
#endif /* USE_MOTIF_DND */

struct OutlineButtonFuncData;
struct OutlineFlippyFuncData;
struct OutlinerAncestorInfo;

#if defined(USE_MOTIF_DND)
typedef void (*FEGetDropTargetFunc)(void *,
									Atom **targets, int *numtargets);
typedef void (*FEGetDragTargetFunc)(void *,
									int row, int col,
									Atom **targets, int *numtargets);
typedef fe_icon_data* (*FEGetDragIconDataFunc)(void *,
											   int row, int col);
typedef char* (*FEConvertDragDataFunc)(void *,
									   Atom atom);
typedef int (*FEProcessTargetsFunc)(void *,
									int row, int col,
									Atom *targets,
									const char **data,
									int numItems);
#endif /* USE_MOTIF_DND */

class XFE_Outliner : public XFE_Component
{
public:
  XFE_Outliner(const char *name,
	       XFE_Outlinable *o, 
	       XFE_Component *toplevel_component,
	       Widget widget_parent,
	       XP_Bool constantSize,
	       XP_Bool hasHeadings,
	       int num_columns,
	       int num_visible,
	       int *column_widths,
		   char *geom_prefname);

  XFE_Outliner();

  virtual ~XFE_Outliner();

#if !defined(USE_MOTIF_DND)
  void setOutlinable(XFE_Outlinable *o);
#endif /* USE_MOTIF_DND */

  XFE_Outlinable *getOutlinable();

  void setSortColumn(int column, EOutlinerSortDirection direction);
  int getSortColumn();
  void toggleSortDirection();
  EOutlinerSortDirection getSortDirection();

  XP_Bool isColTextFit(char *str, int row, int col);
  int textStart(int col, int maxX);

  void setPipeColumn(int column);
  int getPipeColumn();

  void setColumnResizable(int column, XP_Bool resizable);
  XP_Bool getColumnResizable(int column);

  void setColumnWidth(int column, int width);
  int getColumnWidth(int column);

  int getNumContextAndHeaderRows();

  void showAllRowsWithRange(int minRows, int maxRows);
  
  void setDescendantSelectAllowed(XP_Bool flag);
  XP_Bool getDescendantSelectAllowed();

  void setMultiSelectAllowed(XP_Bool flag);
  XP_Bool getMultiSelectAllowed();

  void setHideColumnsAllowed(XP_Bool flag);
  XP_Bool getHideColumnsAllowed();

#if !defined(USE_MOTIF_DND)
  void setDragType(fe_dnd_Type dragtype, fe_icon *drag_icon = NULL, 
		   XFE_Component *dragsource = NULL, fe_dnd_SourceDropFunc func = NULL);

  fe_dnd_Type getDragType();
  fe_icon *getDragIcon();
  fe_dnd_SourceDropFunc getSourceDropFunc();

#else
  void enableDragDrop(void *this_ptr = NULL,
					  FEGetDropTargetFunc drop_target_func = NULL,
					  FEGetDragTargetFunc drag_target_func = NULL,
					  FEGetDragIconDataFunc drag_icon_func = NULL,
					  FEConvertDragDataFunc drag_conv_func = NULL,
					  FEProcessTargetsFunc process_targets_func = NULL);

  fe_icon_data* getDragIconData(int row, int col);
  void getDragTargets(int row, int col, Atom **targets, int *numTargets);
  void getDropTargets(Atom **targets, int *numTargets);
  char *dragConvert(Atom atom);
  int processTargets(int row, int col,
					 Atom *targets, const char **data, int numItems);

#endif /* USE_MOTIF_DND */
  /* set selection blocked or not
   */
  void setBlockSel(XP_Bool b);
  void scroll2Item(int index);

  void selectAllItems();
  void selectItem(int index);
  void selectRangeByIndices(int start_index, int end_index);
  void selectItemExclusive(int index);
  void deselectItem(int index);
  void deselectAllItems();
  /* set Focus selection
   */
  void setInFocus(XP_Bool infocus);
  XP_Bool isFocus();

  /* timOrExpandSelection - pretty funky.  look at Outliner.cpp */
  void trimOrExpandSelection(int new_index); 
  void deselectRangeByIndices(int selection_begin, int selection_end);
  void toggleSelected(int index);	
  XP_Bool isSelected(int line);

  int getSelection(const int **indices, int *count);

  int getTotalLines();
  int XYToRow(int x, int y, XP_Bool *nearbottom = NULL);

  void makeVisible(int index);

  void change(int index, int num, int total_lines);

  /* use these when we need to force the redraw of a line or a group of
     lines. */
  void invalidate();
  void invalidateLine(int line);
  void invalidateLines(int start, int count);
  void invalidateHeaders();

  /* used to give dnd feedback. */
  void outlineLine(int line);
  void underlineLine(int line);

  /* hide/show the right-most column */
  void hideColumn();
  void showColumn();

  /* apply a delta across all columns.  Returns the actual delta applied. */
  int applyDelta(int delta, int starting_at = 0);

  /* move a particular column to another place. */
  void moveColumn(int column_to_move, int destination);

#if !defined(USE_MOTIF_DND)
  void handleDragEvent(XEvent *event, fe_dnd_Event type, fe_dnd_Source *source);
#endif /* USE_MOTIF_DND */
  /* these are needed by the libmsg stuff, and probably should
     be used by the bookmark/history/address book/etc stuff
     so that sorting is consistent. */

  /* Tao_16dec96
   * Make them virtual to perform some customization
   */
  int getListChangeDepth();

  virtual void listChangeStarting(XP_Bool asynchronous, 
				  MSG_NOTIFY_CODE notify,
				  MSG_ViewIndex where, 
				  int32 num, 
				  int32 total_lines);
  virtual void listChangeFinished(XP_Bool asynchronous,
				  MSG_NOTIFY_CODE notify,
				  MSG_ViewIndex where, 
				  int32 num, 
				  int32 total_lines);

  Widget getScroller();

private:
  XFE_Outlinable *m_outlinable;

#if defined(USE_MOTIF_DND)
  XFE_OutlinerDrop *m_outlinerdrop;
  XFE_OutlinerDrag *m_outlinerdrag;
#endif /* USE_MOTIF_DND */

  XP_Bool setDefaultGeometry();

  /* used to maintain cached selection information */
  void addSelection(int selected_line);
  void removeSelection(int selected_line);
  XP_Bool insertLines(int start, int count);
  XP_Bool deleteLines(int start, int count);
  void deselectAll();

  /* used to save off and restore the selection when our list
	 is going to get screwed up (as in mail/news sorting) */
  void saveSelection();
  void restoreSelection();

  /* used to restore a column on the right hand side. */
  void hideColumn(int column_number);
  void showColumn(int column_numder);

  void showHeaderDraw(XmLGridCallbackStruct *call);
  void hideHeaderDraw(XmLGridCallbackStruct *call);
  void showHeaderClick();
  void hideHeaderClick();

  int numResizableVisibleColumns(int starting_at = 0);

  /* used to maintain the selection information */
  int *m_selectedIndices;
  int m_selectedItemCount;
  int m_selectedSize;
  int m_selectedCount;
  int m_firstSelectedIndex;
  int m_lastSelectedIndex;
  int m_selectionDirection;  /* -1 for bottom-to-top, and 1 for top-to-bottom.
								see the command in selectRangeByIndices */
  /* this one is only used by listChange{Starting,Finished} */
  void **m_selectedItems; 
  XP_Bool m_selBlocked; /* False (default): can't select; True, otherwise */

  int m_totalLines;

	XP_Bool m_descendantSelectAllowed;
  XP_Bool m_hideColumnsAllowed;

  // pointers used in calls to the flippyfunc and buttonfunc portions
  // of the Outlinable interface.
  OutlineButtonFuncData *m_buttonfunc_data;
  OutlineFlippyFuncData *m_flippyfunc_data;

  int m_numcolumns;
  int m_numvisible;
  char *m_prefname;
  char *m_geominfo;
  void *m_visibleTimer;
  int m_visibleLine;
  int m_DataRow;
  Time m_lastuptime;
  Time m_lastdowntime;
  int m_lastx;
  int m_lasty;
  int *m_columnwidths;
  XP_Bool *m_columnResizable;
  int *m_columnIndex;

  /* drag specific stuff. */
#if !defined(USE_MOTIF_DND)
  int m_lastmotionx;
  int m_lastmotiony;
  int m_hotSpot_x;
  int m_hotSpot_y;
#endif /* USE_MOTIF_DND */
  XP_Bool m_ignoreevents;
#if !defined(USE_MOTIF_DND)
  fe_dnd_Type m_dragtype;
  fe_icon *m_dragicon;
  XFE_Component *m_source;
  fe_dnd_SourceDropFunc m_sourcedropfunc;
  void makeDragWidget(int x, int y, fe_dnd_Type type);
  void destroyDragWidget();
#endif /* USE_MOTIF_DND */

#if defined(USE_MOTIF_DND)
  /* the pointer to the outlinable. */
  void *m_dragsource;
  FEGetDragTargetFunc m_dragTargetFunc;
  FEGetDropTargetFunc m_dropTargetFunc;
  FEGetDragIconDataFunc m_dragIconDataFunc;
  FEConvertDragDataFunc m_dragConvFunc;
  FEProcessTargetsFunc m_processTargetsFunc;
#endif /* USE_MOTIF_DND */

  // XXX
  XP_Bool m_dragrowbox;
  void* m_dragscrolltimer;
  int m_dragscrolldir;
  int m_dragrow;
#if !defined(USE_MOTIF_DND)
  int m_dragcolumn;
#endif /* USE_MOTIF_DND */
  GC m_draggc;

  EventMask m_activity;

  /* the column that displays the sort indicator. */
  int m_sortColumn; 
  /* the direction of the sort. */
  EOutlinerSortDirection m_sortDirection;

  /* the column that displays the pipes. */
  int m_pipeColumn; 

  /* the column that resizes when the outliner resizes wider */
  int m_resizeColumn; 

  /* used by the listChange{Starting,Finished} machinery to restore
	 the selection at the proper time. */
  int m_listChangeDepth; 

  const char *styleToTag(EOutlinerTextStyle style);

  static fe_icon closedParentIcon;
  static fe_icon openParentIcon;
  static fe_icon showColumnIcon;
  static fe_icon showColumnInsensIcon;
  static fe_icon hideColumnIcon;
  static fe_icon hideColumnInsensIcon;

  void setColumnResizableByIndex(int column_index, XP_Bool resizable);
  XP_Bool getColumnResizableByIndex(int column_index);

  void setColumnWidthByIndex(int column_index, int width);
  int getColumnWidthByIndex(int column_index);

  void rememberGeometry();
  int getNumVisible(int default_visible);

  void drawDottedLine(GC gc, XRectangle *clipRect, int x1, int y1, int x2, int y2);

  void PixmapDraw(Pixmap pixmap, Pixmap mask,
		  int pixmapWidth, int pixmapHeight, unsigned char alignment, GC gc,
		  XRectangle *rect, XRectangle *clipRect, Pixel bg_color);

  EOutlinerPipeType getPipeType(XP_Bool expandable,
				XP_Bool expanded,
				int depth,
				OutlinerAncestorInfo *ancestorInfo);

  void pipeDraw(XmLGridCallbackStruct *call);
  void headerCellDraw(XmLGridCallbackStruct *call);
  void contentCellDraw(XmLGridCallbackStruct *call);
  void celldraw(XtPointer callData);
  void resize(XtPointer callData);
  void sendClick(XEvent *event, Boolean only_if_not_selected);
  void buttonEvent(XEvent *event, Boolean *c);

  static void celldrawCallback(Widget, XtPointer, XtPointer);
  static void resizeCallback(Widget, XtPointer, XtPointer);
  static void buttonEventHandler(Widget, XtPointer, XEvent *, Boolean *);

	/* tooltips
	 */
  int     m_lastRow;
  int     m_lastCol;
  XP_Bool m_inGrid;
  XP_Bool m_isFocus;
	
  char    *m_tip_msg_buf;
  char    *m_doc_msg_buf;

  static  void tip_cb(Widget, XtPointer, XtPointer cb_data);
  virtual void tipCB(Widget, XtPointer cb_data);

};

#endif /* _xfe_outliner_h */
