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
/* Name:		<Xfe/Divider.h>											*/
/* Description:	XfeDivider widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeDivider_h_							/* start Divider.h		*/
#define _XfeDivider_h_

#include <Xfe/Xfe.h>
#include <Xfe/Oriented.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDivider resource strings											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNdividerType					"dividerType"
#define XmNdividerPercentage			"dividerPercentage"
#define XmNdividerOffset				"dividerOffset"
#define XmNdividerTarget				"dividerTarget"

#define XmCDividerType					"DividerType"
#define XmCDividerPercentage			"DividerPercentage"
#define XmCDividerOffset				"DividerOffset"
#define XmCDividerTarget				"DividerTarget"

#define XmRDividerType					"DividerType"

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRDividerChildType													*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmDIVIDER_OFFSET,
	XmDIVIDER_PERCENTAGE
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeDividerWidgetClass;

typedef struct _XfeDividerClassRec *			XfeDividerWidgetClass;
typedef struct _XfeDividerRec *					XfeDividerWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsDivider(w)	XtIsSubclass(w,xfeDividerWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDivider public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateDivider				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Divider.h		*/
