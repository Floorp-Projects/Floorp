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
/* Name:		<Xfe/Arrow.h>											*/
/* Description:	XfeArrow widget public header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeArrow_h_							/* start Arrow.h		*/
#define _XfeArrow_h_

#include <Xfe/Xfe.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow resource names												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNarrowHeight					"arrowHeight"
#define XmNarrowWidth					"arrowWidth"

#define XmCArrowHeight					"ArrowHeight"
#define XmCArrowWidth					"ArrowWidth"

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRArrowType															*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmARROW_POINTER,
    XmARROW_POINTER_BASE,
    XmARROW_TRIANGLE,
    XmARROW_TRIANGLE_BASE
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeArrowWidgetClass;
    
typedef struct _XfeArrowClassRec *	XfeArrowWidgetClass;
typedef struct _XfeArrowRec *			XfeArrowWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsArrow(w)	XtIsSubclass(w,xfeArrowWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateArrow				(Widget		pw,
							 char *		name,
							 Arg *		av,
							 Cardinal	ac);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Arrow.h			*/
