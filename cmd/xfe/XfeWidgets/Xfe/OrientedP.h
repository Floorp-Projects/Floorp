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
/* Name:		<Xfe/OrientedP.h>										*/
/* Description:	XfeOriented widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeOrientedP_h_						/* start OrientedP.h	*/
#define _XfeOrientedP_h_

#include <Xfe/Oriented.h>
#include <Xfe/ManagerP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOrientedClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XfeOrientedProc		enter;					/* Enter				*/
	XfeOrientedProc		leave;					/* Leave				*/
	XfeOrientedProc		motion;					/* Motion				*/

	XfeOrientedProc		drag_start;				/* Drag start			*/
	XfeOrientedProc		drag_end;				/* Drag end				*/
	XfeOrientedProc		drag_motion;			/* Drag motion			*/

	XfeOrientedProc		descendant_enter;		/* Descendant Enter		*/
	XfeOrientedProc		descendant_leave;		/* Descendant Leave		*/
	XfeOrientedProc		descendant_motion;		/* Descendant Motion	*/

	XfeOrientedProc		descendant_drag_start;	/* Drag start			*/
	XfeOrientedProc		descendant_drag_end;	/* Drag end				*/
	XfeOrientedProc		descendant_drag_motion;	/* Drag motion			*/

	XtPointer			extension;				/* Extension			*/ 
} XfeOrientedClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOrientedClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeOrientedClassRec
{
	CoreClassPart				core_class;
	CompositeClassPart			composite_class;
	ConstraintClassPart			constraint_class;
	XmManagerClassPart			manager_class;
	XfeManagerClassPart			xfe_manager_class;
	XfeOrientedClassPart		xfe_oriented_class;
} XfeOrientedClassRec;

externalref XfeOrientedClassRec xfeOrientedClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOrientedPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeOrientedPart
{
    /* Drag resources */
	Boolean				allow_drag;				/* Allow drag			*/
	Boolean				drag_in_progress;		/* Drag in progress		*/
	Boolean				cursor_on;				/* Cursor on			*/

    /* Cursor resources */
	Cursor				vertical_cursor;		/* Vertical cursor		*/
	Cursor				horizontal_cursor;		/* Horizontal cursor	*/

    /* Orientation resources */
    unsigned char		orientation;			/* Orientation			*/

	/* Spacing resources */
    Dimension			spacing;				/* Spacing				*/

    /* Private data -- Dont even look past this comment -- */
	int					drag_start_x;			/* Drag start x			*/
	int					drag_start_y;			/* Drag start x			*/
} XfeOrientedPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOrientedRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeOrientedRec
{
   CorePart				core;
   CompositePart		composite;
   ConstraintPart		constraint;
   XmManagerPart		manager;
   XfeManagerPart		xfe_manager;
   XfeOrientedPart		xfe_oriented;
} XfeOrientedRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOrientedConstraintPart											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeOrientedConstraintPart
{
	Boolean							allow_drag;
} XfeOrientedConstraintPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOrientedConstraintRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeOrientedConstraintRec
{
    XmManagerConstraintPart			manager;
    XfeManagerConstraintPart		xfe_manager;
    XfeOrientedConstraintPart		xfe_oriented;
} XfeOrientedConstraintRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOrientedPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeOrientedPart(w) &(((XfeOrientedWidget) w) -> xfe_oriented)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented member access macros										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeOrientedOrientation(w) \
(((XfeOrientedWidget) (w))-> xfe_oriented . orientation)
/*----------------------------------------------------------------------*/
#define _XfeOrientedSpacing(w) \
(((XfeOrientedWidget) (w))-> xfe_oriented . spacing)
/*----------------------------------------------------------------------*/
#define _XfeOrientedDragStartX(w) \
(((XfeOrientedWidget) (w))-> xfe_oriented . drag_start_x)
/*----------------------------------------------------------------------*/
#define _XfeOrientedDragStartY(w) \
(((XfeOrientedWidget) (w))-> xfe_oriented . drag_start_y)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOrientedPart child constraint part access macro					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeOrientedConstraintPart(w) \
(&(((XfeOrientedConstraintRec *) _XfeConstraints(w)) -> xfe_oriented))

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedEnter					(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedLeave					(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedMotion					(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDragStart				(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDragEnd					(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDragMotion				(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDescendantEnter			(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDescendantLeave			(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDescendantMotion		(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDescendantDragStart		(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDescendantDragEnd		(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedDescendantDragMotion	(Widget			w,
									 Widget			descendant,
									 int			x,
									 int			y);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented private Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeOrientedSetCursorState			(Widget			w,
									 Boolean		state);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end OrientedP.h		*/

