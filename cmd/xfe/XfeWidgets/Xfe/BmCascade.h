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
/* Name:		<Xfe/BmCascade.h>										*/
/* Description:	XfeBmCascade widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeBmCascade_h_						/* start BmCascade.h	*/
#define _XfeBmCascade_h_

#include <Xfe/BmButton.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeBmCascadeWidgetClass;
    
typedef struct _XfeBmCascadeClassRec *		XfeBmCascadeWidgetClass;
typedef struct _XfeBmCascadeRec *			XfeBmCascadeWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsBmCascade(w)	XtIsSubclass(w,xfeBmCascadeWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmCascade public functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateBmCascade			(Widget			parent,
							 char *			name,
							 Arg *			args,
							 Cardinal		count);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end BmPushB.h		*/
