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
/* Name:		<Xfe/Find.h>											*/
/* Description:	Xfe widgets utilities to find children.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeFind_h_								/* start Find.h			*/
#define _XfeFind_h_

#include <Xm/Xm.h>								/* Motif public defs	*/
#include <Xfe/StringDefs.h>						/* Xfe public str defs	*/
#include <Xfe/Defaults.h>						/* Xfe default res vals	*/

#include <assert.h>								/* Assert				*/

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget find test func												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef Boolean	(*XfeWidgetTestFunc)	(Widget		w,
										 XtPointer	data);

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget find bits														*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeFIND_ANY			(0)
#define XfeFIND_ALIVE		(1 << 0)
#define XfeFIND_MANAGED		(1 << 1)
#define XfeFIND_REALIZED	(1 << 2)
#define XfeFIND_ALL			(XfeFIND_ALIVE | 		\
							 XfeFIND_MANAGED |		\
							 XfeFIND_REALIZED)

/*----------------------------------------------------------------------*/
/*																		*/
/* Find descendant functions.											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeDescendantFindByFunction		(Widget				w,
								 XfeWidgetTestFunc	func,
								 int				mask,
								 Boolean			popups,
								 XtPointer			data);
/*----------------------------------------------------------------------*/
extern Widget
XfeDescendantFindByName			(Widget				w,
								 String				name,
								 int				mask,
								 Boolean			popups);
/*----------------------------------------------------------------------*/
extern Widget
XfeDescendantFindByClass		(Widget				w,
								 WidgetClass		wc,
								 int				mask,
								 Boolean			popups);
/*----------------------------------------------------------------------*/
extern Widget
XfeDescendantFindByWindow		(Widget				w,
								 Window				window,
								 int				mask,
								 Boolean			popups);
/*----------------------------------------------------------------------*/
extern Widget
XfeDescendantFindByCoordinates	(Widget				w,
								 Position			x,
								 Position			y);
/*----------------------------------------------------------------------*/
extern Widget
XfeChildFindByIndex				(Widget				w,
								 Cardinal			i);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Find ancestor functions.												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeAncestorFindByFunction		(Widget				w,
								 XfeWidgetTestFunc	func,
								 int				mask,
								 XtPointer			data);
/*----------------------------------------------------------------------*/
extern Widget
XfeAncestorFindByName			(Widget				w,
								 String				name,
								 int				mask);
/*----------------------------------------------------------------------*/
extern Widget
XfeAncestorFindByClass			(Widget				w,
								 WidgetClass		wc,
								 int				mask);
/*----------------------------------------------------------------------*/
extern Widget
XfeAncestorFindByWindow			(Widget				w,
								 Window				window,
								 int				mask);
/*----------------------------------------------------------------------*/
extern Widget
XfeAncestorFindTopLevelShell	(Widget				w);
/*----------------------------------------------------------------------*/
extern Widget
XfeAncestorFindApplicationShell	(Widget				w);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Find.h			*/
