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
/* Name:		<Xfe/BmButtonP.h>										*/
/* Description:	XfeBmButton widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeBmButtonP_h_						/* start BmButtonP.h	*/
#define _XfeBmButtonP_h_

#include <Xfe/PrimitiveP.h>
#include <Xfe/BmButton.h>
#include <Xm/PushBP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButtonClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    XtPointer		extension;					/* Extension			*/
} XfeBmButtonClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButtonPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBmButtonClassRec
{
    CoreClassPart				core_class;			/* Core class		*/
    XmPrimitiveClassPart		primitive_class;	/* XmPrimitive		*/
    XmLabelClassPart			label_class;		/* XmLabel			*/
    XmPushButtonClassPart		pushbutton_class;	/* XmPushButton		*/
    XfeBmButtonClassPart		bm_button_class;	/* XfeBmButton		*/
} XfeBmButtonClassRec;

externalref XfeBmButtonClassRec xfeBmButtonClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButtonRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBmButtonPart
{
    /* Pixmap resources */
	Pixmap				arm_pixmap_mask;			/* Arm pixmap mask	*/
	Pixmap				label_pixmap_mask;			/* Label pixmap mask*/
	unsigned char		accent_type;				/* Accent type		*/

    /* Private Data Members */
    GC					pixmap_GC;					/* Pixmap gc		*/

	Dimension			pixmap_width;
	Dimension			pixmap_height;
} XfeBmButtonPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButtonPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBmButtonRec
{
    CorePart				core;				/* Core Part			*/
    XmPrimitivePart			primitive;			/* XmPrimitive Part		*/
    XmLabelPart				label;				/* XmLabel Part			*/
    XmPushButtonPart		pushbutton;			/* XmPushButton Part	*/
    XfeBmButtonPart			bm_button;			/* XfeBmButton part		*/
} XfeBmButtonRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButtonPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeBmButtonPart(w) &(((XfeBmButtonWidget) w) -> bm_button)

/*----------------------------------------------------------------------*/
/*																		*/
/* Private XfeBmButton/BmCascade functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeBmActionWithoutDrawing		(Widget				w,
								 XtActionProc		proc,
								 XEvent *			event,
								 char **			params,
								 Cardinal *			nparams);
/*----------------------------------------------------------------------*/
extern void
_XfeBmProcWithoutDrawing		(Widget				w,
								 XtWidgetProc		proc);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Means the cursor is outside the target accent button/cascade.		*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeACCENT_OUTSIDE -1

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end BmButtonP.h		*/
