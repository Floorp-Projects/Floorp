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
/* Name:		<Xfe/BmButton.h>										*/
/* Description:	XfeBmButton widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeBmButton_h_							/* start BmButton.h		*/
#define _XfeBmButton_h_

#include <Xfe/XfeBm.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeBmButtonWidgetClass;
    
typedef struct _XfeBmButtonClassRec *		XfeBmButtonWidgetClass;
typedef struct _XfeBmButtonRec *			XfeBmButtonWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsBmButton(w)	XtIsSubclass(w,xfeBmButtonWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateBmButton			(Widget			parent,
							 char *			name,
							 Arg *			args,
							 Cardinal		count);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Rep type registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmButtonRegisterRepTypes		(void);

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end BmButton.h		*/
