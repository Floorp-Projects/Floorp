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
/* Name:		<Xfe/ChromeP.h>											*/
/* Description:	XfeChrome widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeChromeP_h_							/* start ChromeP.h		*/
#define _XfeChromeP_h_

#include <Xfe/Chrome.h>
#include <Xfe/ManagerP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChromeClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfeChromeClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChromeClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeChromeClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfeChromeClassPart			xfe_chrome_class;
} XfeChromeClassRec;

externalref XfeChromeClassRec xfeChromeClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChromePart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeChromePart
{
    /* Components */
	Widget				menu_bar;				/* Menu bar				*/
	Widget				tool_box;				/* Tool box				*/
	Widget				dash_board;				/* Dash board			*/

    /* View components */
	Widget				center_view;			/* Center view			*/
	Widget				top_view;				/* Top view				*/
	Widget				bottom_view;			/* Bottom view			*/
	Widget				left_view;				/* Left view			*/
	Widget				right_view;				/* Right view			*/

    /* Spacing */
    Dimension			spacing;				/* Spacing				*/

    /* Private data -- Dont even look past this comment -- */

} XfeChromePart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChromeRec															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeChromeRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfeChromePart		xfe_chrome;
} XfeChromeRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChromeConstraintPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeChromeConstraintPart
{
	unsigned char			chrome_child_type;
/* 	Dimension			min_width; */
/*     Dimension			min_height; */
/*     Dimension			max_width; */
/*     Dimension			max_height; */
} XfeChromeConstraintPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChromeConstraintRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeChromeConstraintRec
{
    XmManagerConstraintPart			manager;
    XfeManagerConstraintPart		xfe_manager;
    XfeChromeConstraintPart			xfe_chrome;
} XfeChromeConstraintRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChromePart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeChromePart(w) &(((XfeChromeWidget) w) -> xfe_chrome)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChromePart child constraint part access macro						*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeChromeConstraintPart(w) \
(&(((XfeChromeConstraintRec *) _XfeConstraints(w)) -> xfe_chrome))

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ChromeP.h		*/

