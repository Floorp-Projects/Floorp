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
/* Name:		<Xfe/Pane.h>											*/
/* Description:	XfePane widget public header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfePane_h_								/* start Pane.h			*/
#define _XfePane_h_

#include <Xfe/Xfe.h>
#include <Xfe/Oriented.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRPaneChildType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmPANE_CHILD_NONE,
	XmPANE_CHILD_ATTACHMENT_ONE,
	XmPANE_CHILD_ATTACHMENT_TWO,
	XmPANE_CHILD_WORK_AREA_ONE,
	XmPANE_CHILD_WORK_AREA_TWO
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRPaneAttachmentType												*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmPANE_CHILD_ATTACH_NONE,
	XmPANE_CHILD_ATTACH_BOTTOM,
	XmPANE_CHILD_ATTACH_LEFT,
	XmPANE_CHILD_ATTACH_RIGHT,
	XmPANE_CHILD_ATTACH_TOP
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRPaneDragModeType													*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmPANE_DRAG_PRESERVE_ONE,
	XmPANE_DRAG_PRESERVE_TWO,
	XmPANE_DRAG_PRESERVE_RATIO
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRSashType															*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmPANE_SASH_DOUBLE_LINE,
	XmPANE_SASH_FILLED_RECTANGLE,
	XmPANE_SASH_LIVE,
	XmPANE_SASH_RECTANGLE,
	XmPANE_SASH_SINGLE_LINE
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfePaneWidgetClass;

typedef struct _XfePaneClassRec *			XfePaneWidgetClass;
typedef struct _XfePaneRec *				XfePaneWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsPane(w)	XtIsSubclass(w,xfePaneWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePane public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreatePane					(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Pane.h			*/
