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
/* Name:		<Xfe/LogoP.h>											*/
/* Description:	XfeLogo widget private header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeLogoP_h_							/* start LogoP.h		*/
#define _XfeLogoP_h_

#include <Xfe/Logo.h>
#include <Xfe/ButtonP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogoClassPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    XtPointer		extension;					/* Extension			*/
} XfeLogoClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogoClassRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeLogoClassRec
{
    CoreClassPart			core_class;
    XmPrimitiveClassPart	primitive_class;
    XfePrimitiveClassPart	xfe_primitive_class;
    XfeLabelClassPart		xfe_label_class;
    XfeButtonClassPart		xfe_button_class;
    XfeLogoClassPart		xfe_logo_class;
} XfeLogoClassRec;

externalref XfeLogoClassRec xfeLogoClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogoPart															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeLogoPart
{
    /* Callback resources */
    XtCallbackList		animation_callback;		/* Animation callback	*/

    /* Pixmap resources */
	Cardinal			current_pixmap_index;	/* Current pixmap index	*/
    XfePixmapTable		animation_pixmaps;		/* Logo pixmaps			*/
	Cardinal			num_animation_pixmaps;	/* Num logo pixmaps		*/
	int					animation_interval;		/* Animation interval	*/
	Boolean				animation_running;		/* Animation running	*/
	Boolean				reset_when_idle;		/* Reset when idle		*/

    /* Private Data Members */
    GC					copy_GC;				/* Copy gc				*/
	Dimension			animation_width;		/* Animation width		*/
	Dimension			animation_height;		/* Animation height		*/

	XtIntervalId		timer_id;				/* Timer id				*/
} XfeLogoPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogoRec															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeLogoRec
{
    CorePart			core;					/* Core Part			*/
    XmPrimitivePart		primitive;				/* XmPrimitive Part		*/
    XfePrimitivePart	xfe_primitive;			/* XfePrimitive Part	*/
    XfeLabelPart		xfe_label;				/* XfeLabel Part		*/
    XfeButtonPart		xfe_button;				/* XfeButton Part		*/
    XfeLogoPart			xfe_logo;				/* XfeLogo Part			*/
} XfeLogoRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogoPart Access Macro												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeLogoPart(w) &(((XfeLogoWidget) w) -> xfe_logo)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end LogoP.h			*/
