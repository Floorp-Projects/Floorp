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
/* Name:		<Xfe/Chrome.h>											*/
/* Description:	XfeChrome widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeChrome_h_							/* start Chrome.h		*/
#define _XfeChrome_h_

#include <Xfe/ToolBox.h>
#include <Xfe/DashBoard.h>
#include <Xfe/TaskBar.h>
#include <Xm/RowColumn.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRChromeChildType													*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmCHROME_BOTTOM_VIEW,
	XmCHROME_CENTER_VIEW,
	XmCHROME_DASH_BOARD,
	XmCHROME_IGNORE,
	XmCHROME_LEFT_VIEW,
	XmCHROME_MENU_BAR,
	XmCHROME_RIGHT_VIEW,
	XmCHROME_TOOL_BOX,
	XmCHROME_TOP_VIEW
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeChromeWidgetClass;

typedef struct _XfeChromeClassRec *			XfeChromeWidgetClass;
typedef struct _XfeChromeRec *				XfeChromeWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsChrome(w)	XtIsSubclass(w,xfeChromeWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChrome public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateChrome					(Widget				pw,
								 String				name,
								 Arg *				av,
								 Cardinal			ac);
/*----------------------------------------------------------------------*/
extern Widget
XfeChromeGetComponent			(Widget				w,
								 unsigned char		component);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Chrome.h			*/
