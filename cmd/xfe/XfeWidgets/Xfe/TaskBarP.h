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
/* Name:		<Xfe/TaskBarP.h>										*/
/* Description:	XfeTaskBar widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeTaskBarP_h_							/* start TaskBarP.h		*/
#define _XfeTaskBarP_h_

#include <Xfe/ToolBarP.h>
#include <Xfe/TaskBar.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTaskBarClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
   XtPointer		extension;		/* Extension */ 
} XfeTaskBarClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTaskBarClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTaskBarClassRec
{
    CoreClassPart			core_class;
    CompositeClassPart		composite_class;
    ConstraintClassPart		constraint_class;
    XmManagerClassPart		manager_class;
    XfeManagerClassPart		xfe_manager_class;
    XfeOrientedClassPart	xfe_oriented_class;
    XfeToolBarClassPart		xfe_tool_bar_class;
    XfeTaskBarClassPart		xfe_task_bar_class;
} XfeTaskBarClassRec;

externalref XfeTaskBarClassRec xfeTaskBarClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTaskBarPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTaskBarPart
{
    /* Callbacks */
	XtCallbackList		action_callback;			/* Action callbacks	*/
    
    /* Resource Data Members */
    Widget				action_button;				/* Dock Button		*/
    Cursor				action_cursor;				/* Action cursor	*/
    Pixmap				action_pixmap;				/* Action pixmap	*/
	Boolean				show_action_button;			/* Show action btn ?*/

    /* Private data -- Dont even look past this comment --	*/
} XfeTaskBarPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTaskBarRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTaskBarRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfeToolBarPart		xfe_tool_bar;
    XfeOrientedPart		xfe_oriented;
    XfeTaskBarPart		xfe_task_bar;
} XfeTaskBarRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTaskBarPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeTaskBarPart(w) &(((XfeTaskBarWidget) w) -> xfe_task_bar)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end TaskBarP.h		*/

