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
/* Name:		<Xfe/ToolBarP.h>										*/
/* Description:	XfeToolBar widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeToolBarP_h_							/* start ToolBarP.h		*/
#define _XfeToolBarP_h_

#include <Xfe/ToolBar.h>
#include <Xfe/OrientedP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBarClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer		extension;					/* Extension			*/ 
} XfeToolBarClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBarClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBarClassRec
{
    CoreClassPart			core_class;
    CompositeClassPart		composite_class;
    ConstraintClassPart		constraint_class;
    XmManagerClassPart		manager_class;
    XfeManagerClassPart		xfe_manager_class;
    XfeOrientedClassPart	xfe_oriented_class;
    XfeToolBarClassPart		xfe_tool_bar_class;
} XfeToolBarClassRec;

externalref XfeToolBarClassRec xfeToolBarClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBarPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBarPart
{
    /* Callback resources */
    XtCallbackList	button_3_down_callback;		/* Button 3 down cb		*/
    XtCallbackList	button_3_up_callback;		/* Button 3 up cb		*/
    XtCallbackList	selection_changed_callback;	/* Selection changed cb	*/
    XtCallbackList	value_changed_callback;		/* Value changed cb		*/

    /* Button resources */
    unsigned char		button_layout;			/* Button layout		*/

    /* Separator resources */
	int					separator_thickness;	/* Separator thickness	*/

	/* Raised resources */
	Dimension			raise_border_thickness;	/* Raise border thickness*/
    Boolean				raised;					/* Raised ?				*/

	/* Radio resources */
    Boolean				radio_behavior;			/* Radio behavior ?		*/
    Widget				active_button;			/* Active button ?		*/

	/* Selection resources */
    unsigned char		selection_policy;		/* Sel Radio behavior ?*/
    Widget				selected_button;		/* Selected button ?	*/
    Modifiers			selection_modifiers;	/* Selection modifiers	*/

	/* Indicator resources */
    int					indicator_position;		/* Indicator Position	*/

	/* Geometry resources */
	Boolean				child_use_pref_width;	/* Child use pref width	*/
	Boolean				child_use_pref_height;	/* Child use pref height*/

	Boolean				child_force_width;		/* Child force width ?	*/
	Boolean				child_force_height;		/* Child force height ?	*/

	Dimension			max_child_width;		/* Max Width			*/
	Dimension			max_child_height;		/* Max Height			*/

	/* Wrapping resources */
	Boolean				allow_wrap;				/* Allow wrap			*/
	Cardinal			max_num_columns;		/* Max num columns		*/
	Cardinal			max_num_rows;			/* Max num rows			*/
    
    /* Private data -- Dont even look past this comment -- */
	Dimension			total_children_width;	/* Total children width	*/
	Dimension			total_children_height;	/* Total children height*/
	Cardinal			num_managed;			/* Num managed widgets	*/
	Cardinal			num_components;			/* Num components		*/
	Widget				indicator;				/* Indicator			*/

} XfeToolBarPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBarRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBarRec
{
    CorePart		core;
    CompositePart	composite;
    ConstraintPart	constraint;
    XmManagerPart	manager;
    XfeManagerPart	xfe_manager;
    XfeOrientedPart	xfe_oriented;
    XfeToolBarPart	xfe_tool_bar;
} XfeToolBarRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBarConstraintPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBarConstraintPart
{
    int	dummy;
} XfeToolBarConstraintPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBarConstraintRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolBarConstraintRec
{
    XmManagerConstraintPart		manager;
    XfeManagerConstraintPart	xfe_manager;
    XfeOrientedConstraintPart	xfe_oriented;
    XfeToolBarConstraintPart	xfe_tool_bar;
} XfeToolBarConstraintRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBarPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeToolBarPart(w) &(((XfeToolBarWidget) w) -> xfe_tool_bar)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ToolBarP.h		*/

