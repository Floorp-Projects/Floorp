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
/* Name:		<Xfe/ToolBox.h>											*/
/* Description:	XfeToolBox widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeToolBox_h_							/* start ToolBox.h		*/
#define _XfeToolBox_h_

#include <Xfe/Manager.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Tool box callback structures											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
    Widget		item;					/* target Item					*/
    Widget		closed_tab;				/* Corresponding closed tab		*/
    Widget		opened_tab;				/* Corresponding opened tab		*/
	int			index;					/* Index of item				*/
} XfeToolBoxCallbackStruct;

typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
    Widget		descendant;				/* Descendant requesting drag	*/
    Widget		item;					/* Item to be dragged			*/
    Widget		tab;					/* Corresponding tab			*/
	int			index;					/* Index of item				*/
    Boolean		allow;					/* Allow the drag ?				*/
} XfeToolBoxDragAllowCallbackStruct;

typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
    Widget		swapped;				/* Item swapped into position	*/
    Widget		displaced;				/* Displaced item				*/
	int			from_index;				/* Index of displaced item		*/
	int			to_index;				/* New index of swapped item	*/
} XfeToolBoxSwapCallbackStruct;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeToolBoxWidgetClass;

typedef struct _XfeToolBoxClassRec *		XfeToolBoxWidgetClass;
typedef struct _XfeToolBoxRec *				XfeToolBoxWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsToolBox(w)	XtIsSubclass(w,xfeToolBoxWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateToolBox				(Widget			parent,
								 String			name,
								 Arg *			args,
								 Cardinal		num_args);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox drag descendant											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeToolBoxAddDragDescendant		(Widget			w,
								 Widget			descendant);
/*----------------------------------------------------------------------*/
extern void
XfeToolBoxRemoveDragDescendant	(Widget			w,
								 Widget			descendant);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox index methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern int
XfeToolBoxItemGetIndex			(Widget			w,
								 Widget			item);
/*----------------------------------------------------------------------*/
#if 0
extern int
XfeToolBoxTabGetIndex			(Widget			w,
								 Widget			tab);
/*----------------------------------------------------------------------*/
#endif
extern Widget
XfeToolBoxItemGetByIndex		(Widget			w,
								 Cardinal		index);
/*----------------------------------------------------------------------*/
extern Widget
XfeToolBoxItemGetTab			(Widget			w,
								 Widget			item,
								 Boolean		opened);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox position methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern  void
XfeToolBoxItemSetPosition		(Widget			w,
								 Widget			item,
								 int			position);
/*----------------------------------------------------------------------*/
extern int
XfeToolBoxItemGetPosition		(Widget			w,
								 Widget			item);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox open methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern  void
XfeToolBoxItemSetOpen			(Widget			w,
								 Widget			item,
								 Boolean		open);
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolBoxItemGetOpen			(Widget			w,
								 Widget			item);
/*----------------------------------------------------------------------*/
extern  void
XfeToolBoxItemToggleOpen		(Widget			w,
								 Widget			item);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxIsNeeded()													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean 
XfeToolBoxIsNeeded				(Widget			w);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ToolBox.h		*/
