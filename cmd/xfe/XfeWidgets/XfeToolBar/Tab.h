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
/* Name:		<Xfe/Tab.h>												*/
/* Description:	XfeTab widget public header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeTab_h_								/* start Tab.h			*/
#define _XfeTab_h_

#include <Xfe/Xfe.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab resource names												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNbottomPixmap					"bottomPixmap"
#define XmNbottomRaisedPixmap			"bottomRaisedPixmap"
#define XmNhorizontalPixmap				"horizontalPixmap"
#define XmNhorizontalRaisedPixmap		"horizontalRaisedPixmap"
#define XmNleftPixmap					"leftPixmap"
#define XmNleftRaisedPixmap				"leftRaisedPixmap"
#define XmNrightPixmap					"rightPixmap"
#define XmNrightRaisedPixmap			"rightRaisedPixmap"
#define XmNtopPixmap					"topPixmap"
#define XmNtopRaisedPixmap				"topRaisedPixmap"
#define XmNverticalPixmap				"verticalPixmap"
#define XmNverticalRaisedPixmap			"verticalRaisedPixmap"

#define XmCBottomPixmap					"BottomPixmap"
#define XmCBottomRaisedPixmap			"BottomRaisedPixmap"
#define XmCHorizontalPixmap				"HorizontalPixmap"
#define XmCHorizontalRaisedPixmap		"HorizontalRaisedPixmap"
#define XmCLeftPixmap					"LeftPixmap"
#define XmCLeftRaisedPixmap				"LeftRaisedPixmap"
#define XmCRightPixmap					"RightPixmap"
#define XmCRightRaisedPixmap			"RightRaisedPixmap"
#define XmCTopPixmap					"TopPixmap"
#define XmCTopRaisedPixmap				"TopRaisedPixmap"
#define XmCVerticalPixmap				"VerticalPixmap"
#define XmCVerticalRaisedPixmap			"VerticalRaisedPixmap"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeTabWidgetClass;
    
typedef struct _XfeTabClassRec *	XfeTabWidgetClass;
typedef struct _XfeTabRec *			XfeTabWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsTab(w)	XtIsSubclass(w,xfeTabWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab public functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateTab				(Widget		pw,
							 String		name,
							 Arg *		av,
							 Cardinal	ac);
/*----------------------------------------------------------------------*/
extern void
XfeTabDrawRaised			(Widget		w,
							 Boolean	raised);
/*----------------------------------------------------------------------*/
extern unsigned char
XfeTabGetOrientation		(Widget		w);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Tab.h		*/
