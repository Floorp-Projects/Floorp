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
/* Name:		<Xfe/FancyBox.h>										*/
/* Description:	XfeFancyBox widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeFancyBox_h_							/* start FancyBox.h		*/
#define _XfeFancyBox_h_

#include <Xfe/ComboBox.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBox resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNicon							"icon"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBox class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeFancyBoxWidgetClass;

typedef struct _XfeFancyBoxClassRec *			XfeFancyBoxWidgetClass;
typedef struct _XfeFancyBoxRec *				XfeFancyBoxWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBox subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsFancyBox(w)	XtIsSubclass(w,xfeFancyBoxWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBox public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateFancyBox				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end FancyBox.h		*/
