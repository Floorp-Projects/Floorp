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
/* Name:		<Xfe/DashBoard.h>										*/
/* Description:	XfeDashBoard widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeDashBoard_h_						/* start DashBoard.h	*/
#define _XfeDashBoard_h_

#include <Xfe/Xfe.h>
#include <Xfe/Button.h>
#include <Xfe/Label.h>
#include <Xfe/ProgressBar.h>
#include <Xfe/TaskBar.h>
#include <Xfe/ToolBar.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoard components												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef enum
{
	XfeDASH_BOARD_DOCKED_TASK_BAR,
	XfeDASH_BOARD_FLOATING_SHELL,
	XfeDASH_BOARD_FLOATING_TASK_BAR,
	XfeDASH_BOARD_PROGRESS_BAR,
	XfeDASH_BOARD_STATUS_BAR,
	XfeDASH_BOARD_TOOL_BAR
} XfeDashBoardComponent;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeDashBoardWidgetClass;

typedef struct _XfeDashBoardClassRec *		XfeDashBoardWidgetClass;
typedef struct _XfeDashBoardRec *			XfeDashBoardWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsDashBoard(w)	XtIsSubclass(w,xfeDashBoardWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDashBoard Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateDashBoard				(Widget		parent,
								 String		name,
								 Arg *		args,
								 Cardinal	num_args);
/*----------------------------------------------------------------------*/
extern Widget
XfeDashBoardGetComponent		(Widget					dashboard,
								 XfeDashBoardComponent	component);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end DashBoard.h		*/
