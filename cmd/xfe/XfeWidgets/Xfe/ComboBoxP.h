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
/* Name:		<Xfe/ComboBoxP.h>										*/
/* Description:	XfeComboBox widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeComboBoxP_h_						/* start ComboBoxP.h	*/
#define _XfeComboBoxP_h_

#include <Xfe/ComboBox.h>
#include <Xfe/ManagerP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBoxClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    XtWidgetProc			layout_title;		/* layout_title			*/
    XtWidgetProc			layout_arrow;		/* layout_arrow			*/
    XfeExposeProc			draw_highlight;		/* draw_highlight		*/
    XfeExposeProc			draw_title_shadow;	/* draw_title_shadow	*/
	XtPointer				extension;			/* extension			*/
} XfeComboBoxClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBoxClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeComboBoxClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfeComboBoxClassPart		xfe_combo_box_class;
} XfeComboBoxClassRec;

externalref XfeComboBoxClassRec xfeComboBoxClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBoxPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeComboBoxPart
{
    /* Title resources */
    unsigned char		combo_box_type;			/* Combo box type		*/
	Widget				title;					/* Title				*/
    Dimension			spacing;				/* Spacing				*/
	XmFontList			title_font_list;		/* Title font list		*/
    Dimension			title_shadow_thickness;	/* Title shadow thickness*/
    unsigned char		title_shadow_type;		/* Title shadow type	*/

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
    Boolean				remain_popped_up;		/* Remain popped up ?	*/
	XtIntervalId		delay_timer_id;			/* Delay timer id		*/

} XfeComboBoxPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBoxRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeComboBoxRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfeComboBoxPart		xfe_combo_box;
} XfeComboBoxRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBoxPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeComboBoxPart(w) &(((XfeComboBoxWidget) w) -> xfe_combo_box)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeComboBoxLayoutTitle			(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeComboBoxLayoutArrow			(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeComboBoxDrawHighlight		(Widget			w,
								 XEvent *		event,
								 Region			region,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfeComboBoxDrawTitleShadow		(Widget			w,
								 XEvent *		event,
								 Region			region,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ComboBoxP.h		*/

