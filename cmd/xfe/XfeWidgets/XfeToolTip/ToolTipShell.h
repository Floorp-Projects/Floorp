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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/ToolTipShell.h>									*/
/* Description:	XfeToolTipShell widget public header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeToolTipShell_h_						/* start ToolTipShell.h	*/
#define _XfeToolTipShell_h_

#include <Xfe/BypassShell.h>
#include <Xfe/Button.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShell resource names										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNtoolTipCallback				"toolTipCallback"

#define XmNtoolTipLabel					"toolTipLabel"
#define XmNtoolTipPlacement				"toolTipPlacement"
#define XmNtoolTipTimeout				"toolTipTimeout"
#define XmNtoolTipType					"toolTipType"
#define XmNtoolTipHorizontalOffset		"toolTipHorizontalOffset"
#define XmNtoolTipVerticalOffset		"toolTipVerticalOffset"

#define XmCToolTipPlacement				"ToolTipPlacement"
#define XmCToolTipTimeout				"ToolTipTimeout"
#define XmCToolTipType					"ToolTipType"

#define XmRToolTipPlacement				"ToolTipPlacement"
#define XmRToolTipType					"ToolTipType"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTip reasonable defaults for some resources					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeDEFAULT_TOOL_TIP_BOTTOM_OFFSET				10
#define XfeDEFAULT_TOOL_TIP_LEFT_OFFSET					10
#define XfeDEFAULT_TOOL_TIP_RIGHT_OFFSET				10
#define XfeDEFAULT_TOOL_TIP_TOP_OFFSET					10
#define XfeDEFAULT_TOOL_TIP_TIMEOUT						200

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRToolTipType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmTOOL_TIP_EDITABLE,						/*						*/
	XmTOOL_TIP_READ_ONLY						/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRToolTipPlacement													*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmTOOL_TIP_PLACE_BOTTOM,					/*						*/
	XmTOOL_TIP_PLACE_LEFT,						/*						*/
	XmTOOL_TIP_PLACE_RIGHT,						/*						*/
	XmTOOL_TIP_PLACE_TOP						/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTip class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeToolTipShellWidgetClass;

typedef struct _XfeToolTipShellClassRec *		XfeToolTipShellWidgetClass;
typedef struct _XfeToolTipShellRec *			XfeToolTipShellWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTip subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsToolTipShell(w)	XtIsSubclass(w,xfeToolTipShellWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShell public methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateToolTipShell				(Widget		pw,
									 String		name,
									 Arg *		av,
									 Cardinal	ac);
/*----------------------------------------------------------------------*/
extern Widget
XfeToolTipShellGetLabel				(Widget		w);
/*----------------------------------------------------------------------*/

extern void
XfeToolTipShellAdd					(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipShellRemove				(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipShellAddCallback			(Widget		w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShellIsShowing() - Check whether the global tooltip is showing	*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipShellIsShowing				(void);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ToolTipShell.h	*/
