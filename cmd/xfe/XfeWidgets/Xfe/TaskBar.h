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
/* Name:		<Xfe/TaskBar.h>											*/
/* Description:	XfeTaskBar widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeTaskBar_h_							/* start TaskBar.h		*/
#define _XfeTaskBar_h_

#include <Xfe/Xfe.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeTaskBarWidgetClass;

typedef struct _XfeTaskBarClassRec *		XfeTaskBarWidgetClass;
typedef struct _XfeTaskBarRec *				XfeTaskBarWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsTaskBar(w)	XtIsSubclass(w,xfeTaskBarWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTaskBar Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateTaskBar				(Widget		parent,
								 String		name,
								 Arg *		args,
								 Cardinal	num_args);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end TaskBar.h		*/
