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
/* Name:		<Xfe/Primitive.h>										*/
/* Description:	XfePrimitive widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfePrimitive_h_						/* start Primitive.h	*/
#define _XfePrimitive_h_

#include <Xfe/Xfe.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRBufferType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmBUFFER_SHARED,							/*						*/
    XmBUFFER_NONE,								/*						*/
    XmBUFFER_PRIVATE							/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfePrimitiveWidgetClass;

typedef struct _XfePrimitiveClassRec *		XfePrimitiveWidgetClass;
typedef struct _XfePrimitiveRec *			XfePrimitiveWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsPrimitive(w)	XtIsSubclass(w,xfePrimitiveWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* Public pretend sensitive methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeSetPretendSensitive			(Widget			w,
								 Boolean		state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeIsPretendSensitive			(Widget			w);
/*----------------------------------------------------------------------*/
extern Boolean
XfeIsSensitive					(Widget			w);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Primitive.h		*/
