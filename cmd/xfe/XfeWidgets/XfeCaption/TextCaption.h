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
/* Name:		<Xfe/TextCaption.h>										*/
/* Description:	XfeTextCaption widget public header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeTextCaption_h_						/* start TextCaption.h	*/
#define _XfeTextCaption_h_

#include <Xfe/Caption.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTextCaption class names											*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeTextCaptionWidgetClass;

typedef struct _XfeTextCaptionClassRec *		XfeTextCaptionWidgetClass;
typedef struct _XfeTextCaptionRec *				XfeTextCaptionWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTextCaption subclass test macro									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsTextCaption(w)	XtIsSubclass(w,xfeTextCaptionWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTextCaption public methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateTextCaption			(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Caption.h		*/
