/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   outline.h --- includes for the outline widget hack.
   Created: Terry Weissman <terry@netscape.com>, 24-Jun-95.
 */


#ifndef __xfe_outline_h_
#define __xfe_outline_h_

#ifdef	__cplusplus
extern "C" {
#endif

#include "msgcom.h"
#include "icons.h"
#include "dragdrop.h"

typedef int fe_OutlineType;

#define FE_OUTLINE_String		0x01
#define FE_OUTLINE_Chopped		0x02
#define FE_OUTLINE_Icon		0x04
#define FE_OUTLINE_Box		0x08

/* these two tell the outline which columns (note you can have multiple)
   are supposed to be indented, and, optional are to show an 
   additional control to toggle between indented mode 
   and non indented mode. */

#define FE_OUTLINE_IndentedColumn	0x10
#define FE_OUTLINE_IndentToggle	0x20
#define FE_OUTLINE_IndentLines	0x40

#define FE_OUTLINE_ChoppedString	(FE_OUTLINE_String | FE_OUTLINE_Chopped)
#define FE_OUTLINE_IconString	(FE_OUTLINE_Icon | FE_OUTLINE_String)
#define FE_OUTLINE_IconChoppedString	 (FE_OUTLINE_Icon | FE_OUTLINE_ChoppedString)

typedef enum {
  FE_OUTLINE_Leaf,
  FE_OUTLINE_Folded,
  FE_OUTLINE_Expanded
} fe_OutlineFlippyType;

typedef enum {
  FE_OUTLINE_Default,
  FE_OUTLINE_Bold,
  FE_OUTLINE_Italic
} fe_OutlineTextStyle;

typedef struct fe_OutlineDesc {
  fe_OutlineFlippyType flippy; 
  int depth;
  fe_OutlineType *type;		/* What to draw in this column. */
  fe_icon **icons;
  const char **str;		/* Used only if the corresponding entry in
				   the type array is FE_OUTLINE_String. */
  fe_OutlineTextStyle style;	/* What tag to use to paint any strings.
				   The datafunc *must* fill this in.  Choices
				   are (right now) XmFONTLIST_DEFAULT_TAG,
				   "BOLD", and "ITALIC". */
  const char **column_headers;	/* headers for the various columns. */
  int numcolumns;
  XP_Bool selected;
} fe_OutlineDesc;

typedef struct fe_OutlineHeaderDesc {
  fe_OutlineType type[10];
  const char **header_strings;
  fe_icon *icons[10];
} fe_OutlineHeaderDesc;

typedef Boolean (*fe_OutlineGetDataFunc)(Widget, void* closure, int row,
					 fe_OutlineDesc* data, int tag);

typedef void (*fe_OutlineClickFunc)(Widget, void* closure, int row, int column,
				    const char* header, /* Header for this column */
				    int button, int clicks, Boolean shift,
				    Boolean ctrl, int tag);

typedef void (*fe_OutlineIconClickFunc)(Widget, void* closure, int row, int tag);


/*
 * Be sure to pass fe_OutlineCreate() an ArgList that has at least 5 empty
 * slots that you aren't using, as it will.  Yick.  ###
 */
extern Widget fe_GridCreate(MWContext* context, Widget parent, String name,
			    ArgList av, Cardinal ac,
			    int maxindentdepth, /* Make the indentation
						   icon column deep enough
						   to initially show this
						   many levels. If zero,
						   don't let the user resize
						   it, and it will be
						   resized by calls to
						   fe_OutlineSetMaxDepth().
						   */
			    /* always pass in one less number of actual 
			       number of columns */
			    int numcolumns, 
			    /* for outline, pass in one less number of actual
			       columns for widths (ignore first column), 
			       for grid, pass in widths for all columns */
			    int* columnwidths,
			    fe_OutlineGetDataFunc datafunc,
			    fe_OutlineClickFunc clickfunc,
			    fe_OutlineIconClickFunc iconfunc,
			    void* closure,
			    char** posinfo,
			    int tag /* for multipaned window */,
			    XP_Bool isOutline);


/* ############### see doc above ################### */
extern Widget fe_OutlineCreate(MWContext* context, Widget parent, String name,
			       ArgList av, Cardinal ac,
			       int maxindentdepth, 
			       int numcolumns, int* columnwidths,
			       fe_OutlineGetDataFunc datafunc,
			       fe_OutlineClickFunc clickfunc,
			       fe_OutlineIconClickFunc iconfunc,
			       void* closure,
			       char** posinfo,
			       int tag);


/* Set the header strings for the given widget.  The outline code will keep the
   pointers you give it, so you'd better never touch them.  It will pass these
   same pointers back to the clickfunc, so you can write code that compares
   strings instead of remembering magic numbers.  Note that any column
   reordering that may get done by the user is completely hidden from the
   caller; the reordering is a display artifact only. */

extern void fe_OutlineSetHeaders(Widget widget, fe_OutlineHeaderDesc *headers);

/* Tell the outline whether draw the given header in a highlighted way
   (currently, boldface). */

extern void fe_OutlineSetHeaderHighlight(Widget widget, const char* header,
					 XP_Bool value);


/* Change the text displayed for the given header.  Note the original header
   string is still used for calls that want to manipulate that header; this
   just changes what string is presented for the user.  If the label is
   NULL, then the header is reverted back to its original string. */

extern void fe_OutlineChangeHeaderLabel(Widget widget, const char* headername,
					const char* label);



/* Tells the outline widget that "length" rows starting at "first"
 have been changed.  The outline is now to consider itself to have
 "newnumrows" rows in it.*/

extern void fe_OutlineChange(Widget outline, int first, int length,
			     int newnumrows);

extern void fe_OutlineSelect(Widget outline, int row, Boolean exclusive);

extern void fe_OutlineUnselect(Widget outline, int row);

extern void fe_OutlineUnselectAll(Widget outline);

extern void fe_OutlineMakeVisible(Widget outline, int visible);


/* Returns how many items are currently selected.  If sellist is not NULL,
   it gets set with the indices of the selection (up to sizesellist
   entries.) */

extern int fe_OutlineGetSelection(Widget outline, MSG_ViewIndex* sellist,
				  int sizesellist);


/* Tell the outline widget to set the maximum indentation depth.  This should
 only be called if the outline widget was created with maxindentdepth set to
 zero.*/
extern void fe_OutlineSetMaxDepth(Widget outline, int maxdepth);


/* Given an (x,y) in the root window, return which row it corresponds to in the
   outline (-1 if none). If nearbottom is given, then it is set to TRUE if the
   (x,y) is very near the bottom of the returned row (used by drag'n'drop to
   determine if the cursor should be considered to be between rows.) */
extern int fe_OutlineRootCoordsToRow(Widget outline, int x, int y,
				     XP_Bool* nearbottom);


/* Enables dragging from this outline widget. */
extern void fe_OutlineEnableDrag(Widget outline, fe_icon *dragicon, fe_dnd_Type dragtype);

/* Disables dragging from this outline widget. */
extern void fe_OutlineDisableDrag(Widget outline);

/* Handles auto-scrolling during a drag operation.  If you call this, then
   you have to call this for every event on a particular drag.   This routine
   should be called if the given drag operation could possibly be destined
   for this widget. */
extern void fe_OutlineHandleDragEvent(Widget outline, XEvent* event,
				      fe_dnd_Event type,
				      fe_dnd_Source* source);


/* Helps provide feedback during drag operation.  Up to one row may have a drag
   highlight, which is either a box around it or a line under it.  Setting row
   to a negative value will turn it off entirely. */
extern void fe_OutlineSetDragFeedback(Widget outline, int row, XP_Bool usebox);

#ifdef	__cplusplus
}
#endif

#endif /* __xfe_outline_h_ */
