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
/* Name:		<Xfe/BmCascadeP.h>										*/
/* Description:	XfeBmCascade widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeBmCascadeP_h_						/* start BmCascadeP.h	*/
#define _XfeBmCascadeP_h_

#include <Xfe/PrimitiveP.h>
#include <Xfe/BmCascade.h>
#include <Xm/CascadeBP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascadeClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    XtPointer		extension;					/* Extension			*/
} XfeBmCascadeClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascadePart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBmCascadeClassRec
{
    CoreClassPart				core_class;			/* Core class		*/
    XmPrimitiveClassPart		primitive_class;	/* XmPrimitive		*/
    XmLabelClassPart			label_class;		/* XmLabel			*/
    XmCascadeButtonClassPart	cascade_button_class;/* XmCascadeButton	*/
    XfeBmCascadeClassPart		bm_cascade_class;	/* XfeBmCascade		*/
} XfeBmCascadeClassRec;

externalref XfeBmCascadeClassRec xfeBmCascadeClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascadeRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBmCascadePart
{
    /* Pixmap resources */
	Pixmap				arm_pixmap;					/* Arm pixmap		*/
	Pixmap				arm_pixmap_mask;			/* Arm pixmap mask	*/
	Pixmap				label_pixmap_mask;			/* Label pixmap mask*/
	unsigned char		accent_type;				/* Accent type		*/

    /* Private Data Members */
    GC					pixmap_GC;					/* Pixmap gc		*/
	Dimension			pixmap_width;
	Dimension			pixmap_height;
} XfeBmCascadePart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascadePart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBmCascadeRec
{
    CorePart				core;				/* Core Part			*/
    XmPrimitivePart			primitive;			/* XmPrimitive Part		*/
    XmLabelPart				label;				/* XmLabel Part			*/
    XmCascadeButtonPart		cascade_button;		/* XmCascadeButton Part	*/
    XfeBmCascadePart		bm_cascade;			/* XfeBmCascade part	*/
} XfeBmCascadeRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascadePart Access Macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeBmCascadePart(w) &(((XfeBmCascadeWidget) w) -> bm_cascade)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end BmCascadeP.h		*/
