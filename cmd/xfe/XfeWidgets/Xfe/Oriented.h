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
/* Name:		<Xfe/Oriented.h>										*/
/* Description:	XfeOriented widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeOriented_h_							/* start Oriented.h		*/
#define _XfeOriented_h_

#include <Xfe/Xfe.h>
#include <Xfe/Manager.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeOrientedWidgetClass;

typedef struct _XfeOrientedClassRec *		XfeOrientedWidgetClass;
typedef struct _XfeOrientedRec *			XfeOrientedWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsOriented(w)	XtIsSubclass(w,xfeOrientedWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeOrientedSetAllowDrag				(Widget			w,
									 Boolean		draggable);
/*----------------------------------------------------------------------*/
extern Boolean
XfeOrientedDescendantSetAllowDrag	(Widget			w,
									 Widget			descendant,
									 Boolean		draggable);
/*----------------------------------------------------------------------*/
extern void
XfeOrientedChildrenSetAllowDrag		(Widget			w,
									 Boolean		draggable);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Oriented.h		*/
