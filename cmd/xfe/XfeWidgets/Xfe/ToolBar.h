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
/* Name:		<Xfe/ToolBar.h>											*/
/* Description:	XfeToolBar widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeToolBar_h_							/* start ToolBar.h		*/
#define _XfeToolBar_h_

#include <Xfe/Oriented.h>
#include <Xfe/Cascade.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRToolBarSelectionType												*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmTOOL_BAR_SELECT_NONE,
	XmTOOL_BAR_SELECT_SINGLE,
	XmTOOL_BAR_SELECT_MULTIPLE
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRToolBarIndicatorLocation											*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmINDICATOR_LOCATION_NONE,
	XmINDICATOR_LOCATION_BEGINNING,
	XmINDICATOR_LOCATION_END,
	XmINDICATOR_LOCATION_MIDDLE
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmINDICATOR_DONT_SHOW - for XmNindicatorPosition to hide indicator.	*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmINDICATOR_DONT_SHOW -2

/*----------------------------------------------------------------------*/
/*																		*/
/* ToolBar callback structure											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
	Widget		button;					/* Button that invoked callback	*/
    Boolean		armed;					/* Button armed ?				*/
    Boolean		selected;				/* Button selected ?			*/
} XfeToolBarCallbackStruct;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeToolBarWidgetClass;

typedef struct _XfeToolBarClassRec *		XfeToolBarWidgetClass;
typedef struct _XfeToolBarRec *				XfeToolBarWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsToolBar(w)	XtIsSubclass(w,xfeToolBarWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateToolBar				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolBarSetActiveButton		(Widget		w,
								 Widget		button);
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolBarSetSelectedButton		(Widget		w,
								 Widget		button);
/*----------------------------------------------------------------------*/
extern unsigned char
XfeToolBarXYToIndicatorLocation	(Widget		w,
								 Widget		item,
								 int		x,
								 int		y);
/*----------------------------------------------------------------------*/
extern Widget
XfeToolBarGetLastItem			(Widget		w);
/*----------------------------------------------------------------------*/
extern Widget
XfeToolBarGetIndicator			(Widget		w);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ToolBar.h		*/
