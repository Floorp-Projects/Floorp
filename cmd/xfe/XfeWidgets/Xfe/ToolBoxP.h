/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/ToolBoxP.h>										*/
/* Description:	XfeToolBox widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeToolBoxP_h_							/* start ToolBoxP.h		*/
#define _XfeToolBoxP_h_

#include <Xfe/ToolBox.h>
#include <Xfe/ManagerP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer		extension;					/* Extension			*/ 
} XfeToolBoxClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBoxClassRec
{
	CoreClassPart				core_class;
	CompositeClassPart			composite_class;
	ConstraintClassPart			constraint_class;
	XmManagerClassPart			manager_class;
	XfeManagerClassPart			xfe_manager_class;
	XfeToolBoxClassPart			xfe_tool_box_class;
} XfeToolBoxClassRec;

externalref XfeToolBoxClassRec xfeToolBoxClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBoxPart
{
    /* Callbacks */
	XtCallbackList		new_item_callback;		/* New item callback	*/
	XtCallbackList		swap_callback;			/* Swap callback		*/
    XtCallbackList		close_callback;			/* Close callback		*/
    XtCallbackList		drag_allow_callback;	/* Drag allow callback	*/
    XtCallbackList		drag_end_callback;		/* Drag end callback	*/
    XtCallbackList		drag_motion_callback;	/* Drag motion callback	*/
    XtCallbackList		drag_start_callback;	/* Drag start callback	*/
    XtCallbackList		open_callback;			/* Open callback		*/
    XtCallbackList		snap_callback;			/* Snap callback		*/

    /* Item resources */
	WidgetList			items;					/* Items				*/
	Cardinal			item_count;				/* Item count			*/

    /* Tab resources */
	WidgetList			closed_tabs;			/* Closed tabs			*/
	WidgetList			opened_tabs;			/* Opened tabs			*/
	Dimension			tab_offset;				/* Tab offset			*/

    /* Spacing resources */
	Dimension			vertical_spacing;		/* Vertical spacing		*/
	Dimension			horizontal_spacing;		/* Horizontal spacing	*/

    /* Drag resources */
	Dimension			drag_threshold;			/* Drag threshold		*/ 
	Cursor				drag_cursor;			/* Drag cursor			*/
	int					drag_button;			/* Drag button			*/

    /* Swap resources */
	Dimension			swap_threshold;			/* Swap threshold		*/ 
    
    /* Pixmap resources */
	Pixmap				bottom_pixmap;			/* Bottom pixmap		*/
	Pixmap				horizontal_pixmap;		/* Horizontal pixmap	*/
	Pixmap				left_pixmap;			/* Left pixmap			*/
	Pixmap				right_pixmap;			/* Right pixmap			*/
	Pixmap				top_pixmap;				/* Top pixmap			*/
	Pixmap				vertical_pixmap;		/* Vertical pixmap		*/
	Pixmap				bottom_raised_pixmap;	/* Bottom Raised pixmap	*/
	Pixmap				horizontal_raised_pixmap;/* Hor Raised pixmap	*/
	Pixmap				left_raised_pixmap;		/* Left Raised pixmap	*/
	Pixmap				right_raised_pixmap;	/* Right Raised pixmap	*/
	Pixmap				top_raised_pixmap;		/* Top Raised pixmap	*/
	Pixmap				vertical_raised_pixmap;	/* Ver Raised pixmap	*/

	/* Private data -- Dont even look past this comment -- */
	int					last_y;					/* Last y				*/
	int					original_y;				/* Origian y			*/
	int					last_drag_y;			/* Last drag y			*/
	int					start_drag_y;			/* Start drag y			*/

	Widget				last_moved_item;		/* Last moved item		*/

	int drag_direction;

	Boolean				dragging;				/* Dragging ?			*/
	Boolean				dragging_tab;			/* Dragging Tab ?		*/
	Boolean				clicking_tab;			/* Clicking Tab ?		*/

} XfeToolBoxPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBoxRec
{
   CorePart				core;
   CompositePart		composite;
   ConstraintPart		constraint;
   XmManagerPart		manager;
   XfeManagerPart		xfe_manager;
   XfeToolBoxPart		xfe_tool_box;
} XfeToolBoxRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxConstraintPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBoxConstraintPart
{
	Boolean 		open;				/* open			*/
} XfeToolBoxConstraintPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxConstraintRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBoxConstraintRec
{
    XmManagerConstraintPart		manager;
    XfeManagerConstraintPart	xfe_manager;
    XfeToolBoxConstraintPart	xfe_tool_box;
} XfeToolBoxConstraintRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeToolBoxPart(w) &(((XfeToolBoxWidget) w) -> xfe_tool_box)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxPart child constraint part access macro					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeToolBoxConstraintPart(w) \
(&(((XfeToolBoxConstraintRec *) _XfeConstraints(w)) -> xfe_tool_box))

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxPart child constraint open access macro					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeToolBoxChildOpen(child) \
((_XfeToolBoxConstraintPart(child)) -> open)
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ToolBoxP.h	*/

