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
/* Name:		<Xfe/TabP.h>											*/
/* Description:	XfeTab widget private header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeTabP_h_								/* start TabP.h			*/
#define _XfeTabP_h_

#include <Xfe/Tab.h>
#include <Xfe/ButtonP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTabClassPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    XtPointer		extension;					/* Extension			*/
} XfeTabClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTabClassRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTabClassRec
{
    CoreClassPart				core_class;
    XmPrimitiveClassPart		primitive_class;
    XfePrimitiveClassPart		xfe_primitive_class;
    XfeLabelClassPart			xfe_label_class;
    XfeButtonClassPart			xfe_button_class;
    XfeTabClassPart				xfe_tab_class;
} XfeTabClassRec;

externalref XfeTabClassRec xfeTabClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTabPart															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTabPart
{
    /* Resources */
	Pixmap				bottom_pixmap;			/* Bottom pixmap		*/
	Pixmap				horizontal_pixmap;		/* Horizontal pixmap	*/
	Pixmap				left_pixmap;			/* Left pixmap			*/
	Pixmap				right_pixmap;			/* Right pixmap			*/
	Pixmap				top_pixmap;				/* Top pixmap			*/
	Pixmap				vertical_pixmap;		/* Vertical pixmap		*/
	Pixmap				bottom_raised_pixmap;	/* Bottom Raised pixmap	*/
	Pixmap				horizontal_raised_pixmap;/* Hor Raised pixmap	*/
	Pixmap				left_raised_pixmap;		/* Left Raised pixmap	*/
	Pixmap				right_raised_pixmap;	/* Right Raised pixmap	*/
	Pixmap				top_raised_pixmap;		/* Top Raised pixmap	*/
	Pixmap				vertical_raised_pixmap;	/* Ver Raised pixmap	*/

	unsigned char		orientation;			/* Orientation			*/

    /* Private Data Members */
	Dimension			bottom_width;			/* Bottom width			*/
	Dimension			bottom_height;			/* Bottom height		*/
	Dimension			horizontal_width;		/* Horizontal width		*/
	Dimension			horizontal_height;		/* Horizontal height	*/
	Dimension			left_width;				/* Left width			*/
	Dimension			left_height;			/* Left height			*/
	Dimension			right_width;			/* Right width			*/
	Dimension			right_height;			/* Right height			*/
	Dimension			top_width;				/* Top width			*/
	Dimension			top_height;				/* Top height			*/
	Dimension			vertical_width;			/* Vertical width		*/
	Dimension			vertical_height;		/* Vertical height		*/

} XfeTabPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTabRec															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTabRec
{
    CorePart				core;				/* Core Part			*/
    XmPrimitivePart			primitive;			/* XmPrimitive Part		*/
    XfePrimitivePart		xfe_primitive;		/* XfePrimitive Part	*/
    XfeLabelPart			xfe_label;			/* XfeLabel Part		*/
    XfeButtonPart			xfe_button;			/* XfeButton Part		*/
    XfeTabPart				xfe_tab;			/* XfeTab Part			*/
} XfeTabRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTabPart Access Macro												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeTabPart(w) &(((XfeTabWidget) w)->xfe_tab)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end TabP.h		*/
