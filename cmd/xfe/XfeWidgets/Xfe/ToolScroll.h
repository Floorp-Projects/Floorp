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
/* Name:		<Xfe/ToolScroll.h>										*/
/* Description:	XfeToolScroll widget public header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeToolScroll_h_						/* start ToolScroll.h	*/
#define _XfeToolScroll_h_

#include <Xfe/Oriented.h>
#include <Xfe/Logo.h>
#include <Xfe/ToolBar.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRarrowPlacement													*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmTOOL_SCROLL_ARROW_PLACEMENT_BOTH,
	XmTOOL_SCROLL_ARROW_PLACEMENT_END,
	XmTOOL_SCROLL_ARROW_PLACEMENT_START
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeToolScrollWidgetClass;

typedef struct _XfeToolScrollClassRec *		XfeToolScrollWidgetClass;
typedef struct _XfeToolScrollRec *			XfeToolScrollWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsToolScroll(w)	XtIsSubclass(w,xfeToolScrollWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolScroll Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateToolScroll				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ToolScroll.h		*/
