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
/* Name:		<Xfe/FancyBoxP.h>										*/
/* Description:	XfeFancyBox widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeFancyBoxP_h_						/* start FancyBoxP.h	*/
#define _XfeFancyBoxP_h_

#include <Xfe/FancyBox.h>
#include <Xfe/ComboBoxP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBoxClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfeFancyBoxClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBoxClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFancyBoxClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfeComboBoxClassPart		xfe_combo_box_class;
    XfeFancyBoxClassPart		xfe_fancy_box_class;
} XfeFancyBoxClassRec;

externalref XfeFancyBoxClassRec xfeFancyBoxClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBoxPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFancyBoxPart
{
    /* Icon resources */
	Widget				icon;					/* Icon					*/

#if 0
	/* List resources */
	Widget				list;					/* List					*/
    XmStringTable		items;					/* Items				*/
    int					item_count;				/* Item count			*/
	XmFontList			list_font_list;			/* List font list		*/
    Dimension			list_margin_height;		/* List margin height	*/
    Dimension			list_margin_width;		/* List margin width	*/
    Dimension			list_spacing;			/* List spacing			*/
	int					top_item_position;		/* Top item position	*/
	int					visible_item_count;		/* Visible item count	*/

	/* Arrow resources */
	Widget				arrow;					/* Arrow				*/

	/* Shell resources */
	Boolean				share_shell;			/* Share shell			*/
	Widget				shell;					/* Shell				*/
	Boolean				popped_up;				/* Popped up ?			*/

	/* Selected resources */
	XmString			selected_item;			/* Selected				*/
	int					selected_position;		/* Selected position	*/

	/* Traversal resources */
    Dimension			highlight_thickness;	/* Highlight thickness	*/
    Boolean				traversal_on;			/* Traversal on ?		*/

    /* Private data -- Dont even look past this comment -- */
    Boolean				highlighted;			/* Highlighted ?		*/
#endif

} XfeFancyBoxPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBoxRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFancyBoxRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfeComboBoxPart		xfe_combo_box;
    XfeFancyBoxPart		xfe_fancy_box;
} XfeFancyBoxRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBoxPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeFancyBoxPart(w) &(((XfeFancyBoxWidget) w) -> xfe_fancy_box)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end FancyBoxP.h		*/

