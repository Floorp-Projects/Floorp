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
/* Name:		<Xfe/TempTwoP.h>										*/
/* Description:	XfeTempTwo widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeTempTwoP_h_							/* start TempTwoP.h		*/
#define _XfeTempTwoP_h_

#include <Xfe/TempTwo.h>
#include <Xfe/ManagerP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwoClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtWidgetProc		layout_string;				/* layout_label		*/
    XfeExposeProc		draw_string;				/* draw_string		*/
	XtPointer			extension;					/* extension		*/ 
} XfeTempTwoClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwoClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTempTwoClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfeTempTwoClassPart			xfe_temp_two_class;
} XfeTempTwoClassRec;

externalref XfeTempTwoClassRec xfeTempTwoClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwoPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTempTwoPart
{
    /* Cursor resources */
	Cursor				vertical_cursor;		/* Vertical cursor		*/
	Cursor				horizontal_cursor;		/* Horizontal cursor	*/

	/* Color resources */
	Pixel				sash_color;				/* Sash color			*/

    /* Resource Data Members */

    Dimension			spacing;				/* Spacing				*/

    Dimension			separator_height;		/* Separator height		*/
    int					separator_thickness;	/* Separator thickness	*/
    unsigned char		separator_type;			/* Separator type		*/
    Dimension			separator_width;		/* Separator width		*/

    unsigned char		orientation;			/* Orientation			*/
    unsigned char		button_layout;			/* Button layout		*/

	Dimension			raise_border_thickness;	/* Raise border thickness*/
    Boolean				raised;					/* Raised ?				*/

	Boolean				child_use_pref_width;	/* child use pref width	*/
	Boolean				child_use_pref_height;	/* child use pref height*/

    /* Private data -- Dont even look past this comment -- */
	GC					temp_GC;				/* Temp GC				*/
	XRectangle			temp_rect;				/* Temp rect			*/

} XfeTempTwoPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwoRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTempTwoRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfeTempTwoPart		xfe_temp_two;
} XfeTempTwoRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwoConstraintPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTempTwoConstraintPart
{
	Dimension			min_width;
    Dimension			min_height;
    Dimension			max_width;
    Dimension			max_height;
} XfeTempTwoConstraintPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwoConstraintRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTempTwoConstraintRec
{
    XmManagerConstraintPart			manager;
    XfeManagerConstraintPart		xfe_manager;
    XfeTempTwoConstraintPart		xfe_temp_two;
} XfeTempTwoConstraintRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwoPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeTempTwoPart(w) &(((XfeTempTwoWidget) w) -> xfe_temp_two)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwoPart child constraint part access macro					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeTempTwoConstraintPart(w) \
(&(((XfeTempTwoConstraintRec *) _XfeConstraints(w)) -> xfe_temp_two))

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end TempTwoP.h		*/

