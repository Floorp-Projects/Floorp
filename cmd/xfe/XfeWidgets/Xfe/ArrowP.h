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
/* Name:		<Xfe/ArrowP.h>											*/
/* Description:	XfeArrow widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeArrowP_h_							/* start ArrowP.h		*/
#define _XfeArrowP_h_

#include <Xfe/Arrow.h>
#include <Xfe/ButtonP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrowClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    XtPointer		extension;					/* Extension			*/
} XfeArrowClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrowClassRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeArrowClassRec
{
    CoreClassPart				core_class;
    XmPrimitiveClassPart		primitive_class;
    XfePrimitiveClassPart		xfe_primitive_class;
    XfeLabelClassPart			xfe_label_class;
    XfeButtonClassPart			xfe_button_class;
    XfeArrowClassPart			xfe_arrow_class;
} XfeArrowClassRec;

externalref XfeArrowClassRec xfeArrowClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrowPart															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeArrowPart
{
    /* Arrow resources */
	unsigned char		arrow_direction;		/* arrow_direction		*/
	Dimension			arrow_width;			/* arrow_width			*/
	Dimension			arrow_height;			/* arrow_height			*/

    /* Private data -- Dont even look past this comment -- */
	GC					arrow_insens_GC;		/* Arrow insens GC		*/

} XfeArrowPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrowRec															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeArrowRec
{
    CorePart				core;				/* Core Part			*/
    XmPrimitivePart			primitive;			/* XmPrimitive Part		*/
    XfePrimitivePart		xfe_primitive;		/* XfePrimitive Part	*/
    XfeLabelPart			xfe_label;			/* XfeLabel Part		*/
    XfeButtonPart			xfe_button;			/* XfeButton Part		*/
    XfeArrowPart			xfe_arrow;			/* XfeArrow Part		*/
} XfeArrowRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrowPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeArrowPart(w) &(((XfeArrowWidget) w)->xfe_arrow)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ArrowP.h			*/
