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
/* Name:		<Xfe/Manager.h>											*/
/* Description:	XfeManager widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeManager_h_							/* start Manager.h		*/
#define _XfeManager_h_

#include <Xfe/Xfe.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeManagerWidgetClass;

typedef struct _XfeManagerClassRec *	XfeManagerWidgetClass;
typedef struct _XfeManagerRec *			XfeManagerWidget;

#define XfeIsManager(w)	XtIsSubclass(w,xfeManagerWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* Manager apply function type											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void		(*XfeManagerApplyProc)	(Widget		w,
											 Widget		child,
											 XtPointer	client_data);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeManagerLayout				(Widget					w);
/*----------------------------------------------------------------------*/
extern void
XfeManagerSetChildrenValues		(Widget					w,
								 ArgList				args,
								 Cardinal				n,
								 Boolean				only_managed);
/*----------------------------------------------------------------------*/
extern void
XfeManagerResizeChildren		(Widget					w,
								 Boolean				set_width,
								 Boolean				set_height,
								 Boolean				only_managed);
/*----------------------------------------------------------------------*/
extern void
XfeManagerApply					(Widget					w,
								 XfeManagerApplyProc	proc,
								 XtPointer				data,
								 Boolean				only_managed);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif						/* end Manager.h	*/
