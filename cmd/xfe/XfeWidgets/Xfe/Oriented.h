/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/Oriented.h>										*/
/* Description:	XfeOriented widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeOriented_h_							/* start Oriented.h		*/
#define _XfeOriented_h_

#include <Xfe/Xfe.h>
#include <Xfe/DynamicManager.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNallowDrag					"allowDrag"
#define XmNdragInProgress				"dragInProgress"
#define XmNhorizontalCursor				"horizontalCursor"
#define XmNverticalCursor				"verticalCursor"

#define XmCDragInProgress				"DragInProgress"
#define XmCHorizontalCursor				"HorizontalCursor"
#define XmCVerticalCursor				"VerticalCursor"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeOrientedWidgetClass;

typedef struct _XfeOrientedClassRec *		XfeOrientedWidgetClass;
typedef struct _XfeOrientedRec *			XfeOrientedWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsOriented(w)	XtIsSubclass(w,xfeOrientedWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeOrientedSetAllowDrag				(Widget			w,
									 Boolean		draggable);
/*----------------------------------------------------------------------*/
extern Boolean
XfeOrientedDescendantSetAllowDrag	(Widget			w,
									 Widget			descendant,
									 Boolean		draggable);
/*----------------------------------------------------------------------*/
extern void
XfeOrientedChildrenSetAllowDrag		(Widget			w,
									 Boolean		draggable);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Oriented.h		*/
