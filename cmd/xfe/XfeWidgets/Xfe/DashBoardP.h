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
/* Name:		<Xfe/DashBoardP.h>										*/
/* Description:	XfeDashBoard widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeDashBoardP_h_						/* start DashBoardP.h	*/
#define _XfeDashBoardP_h_

#include <Xfe/DashBoard.h>
#include <Xfe/ManagerP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoardClassPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer		extension;					/* Extension			*/ 
} XfeDashBoardClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoardClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDashBoardClassRec
{
	CoreClassPart				core_class;
	CompositeClassPart			composite_class;
	ConstraintClassPart			constraint_class;
	XmManagerClassPart			manager_class;
	XfeManagerClassPart			xfe_manager_class;
	XfeDashBoardClassPart		xfe_dash_board_class;
} XfeDashBoardClassRec;

externalref XfeDashBoardClassRec xfeDashBoardClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoardPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDashBoardPart
{
    /* Callbacks */
    XtCallbackList		dock_callback;			/* Dock callback		*/
    XtCallbackList		floating_map_callback;	/* Floating map cb		*/
    XtCallbackList		floating_unmap_callback;/* Floating unmap cb	*/
    XtCallbackList		undock_callback;		/* Undock callback		*/
    
    /* Component resources */
    Widget				tool_bar;				/* Button tool bar		*/
    Widget				status_bar;				/* Status bar			*/
    Widget				progress_bar;			/* Progress bar			*/
    Widget				docked_task_bar;		/* Docked Task bar		*/

	/* Boolean resources */
	Boolean				show_docked_task_bar;	/* Show docked task bar ?*/

	/* Floating components */
    Widget				floating_shell;			/* Floating shell		*/
    Widget				floating_target;		/* Floating target		*/
    Widget				floating_task_bar;		/* Floating Task bar	*/

	/* Pixmap resources */
    Pixmap				undock_pixmap;			/* Undock pixmap		*/

    /* Misc resources */
    Dimension			spacing;				/* Spacing				*/
    Boolean				docked;					/* Task Bar Docked		*/

	Dimension			max_component_height;	/* Max component height	*/

} XfeDashBoardPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoardRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDashBoardRec
{
   CorePart				core;
   CompositePart		composite;
   ConstraintPart		constraint;
   XmManagerPart		manager;
   XfeManagerPart		xfe_manager;
   XfeDashBoardPart		xfe_dash_board;
} XfeDashBoardRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoardPart Access Macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeDashBoardPart(w) &(((XfeDashBoardWidget) w) -> xfe_dash_board)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end DashBoardP.h		*/

