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
/* Name:		<Xfe/Cascade.h>											*/
/* Description:	XfeCascade widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeCascade_h_							/* start Cascade.h		*/
#define _XfeCascade_h_

#include <Xfe/Button.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNsubmenuTearCallback				"submenuTearCallback"

#define XmNallowTearOff					"allowTearOff"
#define XmNcascadeArrowDirection		"cascadeArrowDirection"
#define XmNcascadeArrowLocation			"cascadeArrowLocation"
#define XmNcascadeArrowHeight			"cascadeArrowHeight"
#define XmNcascadeArrowWidth			"cascadeArrowWidth"
#define XmNdrawCascadeArrow				"drawCascadeArrow"
#define XmNtorn							"torn"
#define XmNmatchSubMenuWidth			"matchSubMenuWidth"
#define XmNsubMenuAlignment				"subMenuAlignment"
#define XmNsubMenuLocation				"subMenuLocation"
#define XmNtornShellTitle				"tornShellTitle"

#define XmCCascadeArrowDirection		"CascadeArrowDirection"
#define XmCCascadeArrowHeight			"CascadeArrowHeight"
#define XmCCascadeArrowLocation			"CascadeArrowLocation"
#define XmCSubMenuAlignment				"SubMenuAlignment"
#define XmCCascadeArrowWidth			"CascadeArrowWidth"
#define XmCDrawCascadeArrow				"DrawCascadeArrow"
#define XmCMatchSubMenuWidth			"MatchSubMenuWidth"
#define XmCSubMenuLocation				"SubMenuLocation"
#define XmCTornShellTitle				"TornShellTitle"

#define XmRLocationType					"LocationType"

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRLocationType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmLOCATION_EAST,
	XmLOCATION_NORTH,
	XmLOCATION_NORTH_EAST,
	XmLOCATION_NORTH_WEST,
	XmLOCATION_SOUTH,
	XmLOCATION_SOUTH_EAST,
	XmLOCATION_SOUTH_WEST,
	XmLOCATION_WEST
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Cascade tear submenu callback structure								*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
    Boolean		torn;					/* Cascade torn ?				*/
} XfeSubmenuTearCallbackStruct;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeCascadeWidgetClass;
    
typedef struct _XfeCascadeClassRec *	XfeCascadeWidgetClass;
typedef struct _XfeCascadeRec *			XfeCascadeWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsCascade(w)	XtIsSubclass(w,xfeCascadeWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCascade public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateCascade			(Widget		parent,
							 String		name,
							 Arg *		args,
							 Cardinal	num_args);
/*----------------------------------------------------------------------*/
extern void
XfeCascadeDestroyChildren	(Widget		w);
/*----------------------------------------------------------------------*/
extern Boolean
XfeCascadeArmAndPost		(Widget		w,
							 XEvent *	event);
/*----------------------------------------------------------------------*/
extern Boolean
XfeCascadeDisarmAndUnpost	(Widget		w,
							 XEvent *	event);
/*----------------------------------------------------------------------*/
extern void
XfeCascadeGetChildren		(Widget			w,
							 WidgetList *	children,
							 Cardinal *		num_children);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Cascade.h		*/
