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
/* Name:		<Xfe/Logo.h>											*/
/* Description:	XfeLogo widget public header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeLogo_h_								/* start Logo.h			*/
#define _XfeLogo_h_

#include <Xfe/Xfe.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogo class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeLogoWidgetClass;
    
typedef struct _XfeLogoClassRec *	XfeLogoWidgetClass;
typedef struct _XfeLogoRec *		XfeLogoWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogo subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsLogo(w)	XtIsSubclass(w,xfeLogoWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogo public functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateLogo				(Widget		parent,
							 String		name,
							 Arg *		args,
							 Cardinal	num_args);
/*----------------------------------------------------------------------*/
extern void
XfeLogoAnimationStart		(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeLogoAnimationStop		(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeLogoAnimationReset		(Widget		w);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Logo.h			*/
