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
/* Name:		<Xfe/FontChooserP.h>									*/
/* Description:	XfeFontChooser widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeFontChooserP_h_						/* start FontChooserP.h	*/
#define _XfeFontChooserP_h_

#include <Xfe/FontChooser.h>
#include <Xfe/CascadeP.h>

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
} XfeFontChooserClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooserClassRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFontChooserClassRec
{
    CoreClassPart				core_class;
    XmPrimitiveClassPart		primitive_class;
    XfePrimitiveClassPart		xfe_primitive_class;
    XfeLabelClassPart			xfe_label_class;
    XfeButtonClassPart			xfe_button_class;
    XfeCascadeClassPart			xfe_cascade_class;
    XfeFontChooserClassPart		xfe_font_chooser;
} XfeFontChooserClassRec;

externalref XfeFontChooserClassRec xfeFontChooserClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooserPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFontChooserPart
{
    /* Callback resources */
    XtCallbackList	selection_changed_callback;	/* Selection cb			*/
	
    /* Font item resources */
	Cardinal			num_font_items;			/* Num font items		*/
	XmString *			font_item_labels;		/* Font item labels		*/
	XmFontList *		font_item_fonts;		/* Font item fonts		*/

    /* Private Data Members */
} XfeFontChooserPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooserRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFontChooserRec
{
    CorePart				core;				/* Core Part			*/
    XmPrimitivePart			primitive;			/* XmPrimitive Part		*/
    XfePrimitivePart		xfe_primitive;		/* XfePrimitive Part	*/
    XfeLabelPart			xfe_label;			/* XfeLabel Part		*/
    XfeButtonPart			xfe_button;			/* XfeButton Part		*/
    XfeCascadePart			xfe_cascade;		/* XfeCascade Part		*/
    XfeFontChooserPart		xfe_font_chooser;	/* XfeFontChooser Part	*/
} XfeFontChooserRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooserPart Access Macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeFontChooserPart(w) &(((XfeFontChooserWidget) w)->xfe_font_chooser)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end FontChooserP.h	*/
