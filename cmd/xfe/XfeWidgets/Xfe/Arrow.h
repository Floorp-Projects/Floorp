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
/* Name:		<Xfe/Arrow.h>											*/
/* Description:	XfeArrow widget public header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeArrow_h_							/* start Arrow.h		*/
#define _XfeArrow_h_

#include <Xfe/Xfe.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRArrowType															*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmARROW_POINTER,
    XmARROW_POINTER_BASE,
    XmARROW_TRIANGLE,
    XmARROW_TRIANGLE_BASE
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeArrowWidgetClass;
    
typedef struct _XfeArrowClassRec *	XfeArrowWidgetClass;
typedef struct _XfeArrowRec *			XfeArrowWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsArrow(w)	XtIsSubclass(w,xfeArrowWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateArrow				(Widget		pw,
							 char *		name,
							 Arg *		av,
							 Cardinal	ac);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Arrow.h			*/
