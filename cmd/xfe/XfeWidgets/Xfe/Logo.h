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
/* Name:		<Xfe/Logo.h>											*/
/* Description:	XfeLogo widget public header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeLogo_h_								/* start Logo.h			*/
#define _XfeLogo_h_

#include <Xfe/Button.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogo resource names												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNanimationCallback				"animationCallback"

#define XmNanimationInterval				"animationInterval"
#define XmNresetWhenIdle					"resetWhenIdle"
#define XmRPixmapTable						"PixmapTable"
#define XmNnumAnimationPixmaps			"numAnimationPixmaps"
#define XmNcurrentPixmapIndex			"currentPixmapIndex"
#define XmNanimationPixmaps				"animationPixmaps"
#define XmNanimationRunning				"animationRunning"

#define XmCAnimationInterval				"AnimationInterval"
#define XmCAnimationPixmaps					"AnimationPixmaps"
#define XmCCurrentPixmapIndex				"CurrentPixmapIndex"
#define XmCNumAnimationPixmaps				"NumAnimationPixmaps"
#define XmCResetWhenIdle					"ResetWhenIdle"

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

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Logo.h			*/
