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
/* Name:		<Xfe/CascadeP.h>										*/
/* Description:	XfeCascade widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeCascadeP_h_							/* start CascadeP.h		*/
#define _XfeCascadeP_h_

#include <Xfe/Cascade.h>
#include <Xfe/ButtonP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascadeClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    XtPointer		extension;					/* Extension			*/
} XfeCascadeClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascadeClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeCascadeClassRec
{
    CoreClassPart				core_class;
    XmPrimitiveClassPart		primitive_class;
    XfePrimitiveClassPart		xfe_primitive_class;
    XfeLabelClassPart			xfe_label_class;
    XfeButtonClassPart			xfe_button_class;
    XfeCascadeClassPart			xfe_cascade_class;
} XfeCascadeClassRec;

externalref XfeCascadeClassRec xfeCascadeClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascadePart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeCascadePart
{
    /* Callback resources */
    XtCallbackList		popdown_callback;		/* Popdown cb			*/
    XtCallbackList		popup_callback;			/* Popup cb				*/
    XtCallbackList		cascading_callback;		/* Cascading cb			*/
    XtCallbackList		submenu_tear_callback;	/* Tear submenu cb		*/

    /* Sub menu resources */
	Widget				sub_menu_id;			/* Sub menu id			*/
	int					mapping_delay;			/* Mapping delay		*/
    unsigned char		sub_menu_alignment;		/* Sub menu alignment	*/
    unsigned char		sub_menu_location;		/* Sub menu location	*/
	Boolean				popped_up;				/* Popped up ?			*/
	Boolean				match_sub_menu_width;	/* Match sub menu width?*/

	/* Cascade arrow resources */
	unsigned char		cascade_arrow_direction;/* Arrow direction		*/
	unsigned char		cascade_arrow_location;	/* Arrow location		*/
	Boolean				draw_cascade_arrow;		/* Draw cascade arrow ? */

	/* Tear resources */
	Boolean				torn;					/* Torn ?				*/
	Boolean				allow_tear_off;			/* Allow tear off		*/
	String				torn_shell_title;		/* Torn shell title		*/

    /* Private Data Members */
	XtIntervalId		delay_timer_id;			/* Delay timer id		*/
	Cursor				default_menu_cursor;	/* Default menu cursor	*/
	XRectangle			cascade_arrow_rect;		/* Cascade arrow rect	*/
} XfeCascadePart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascadeRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeCascadeRec
{
    CorePart				core;				/* Core Part			*/
    XmPrimitivePart			primitive;			/* XmPrimitive Part		*/
    XfePrimitivePart		xfe_primitive;		/* XfePrimitive Part	*/
    XfeLabelPart			xfe_label;			/* XfeLabel Part		*/
    XfeButtonPart			xfe_button;			/* XfeButton Part		*/
    XfeCascadePart			xfe_cascade;		/* XfeCascade Part		*/
} XfeCascadeRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascadePart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeCascadePart(w) &(((XfeCascadeWidget) w)->xfe_cascade)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end CascadeP.h		*/
