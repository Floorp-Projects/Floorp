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
/* Name:		<Xfe/PrefGroup.h>										*/
/* Description:	XfePrefGroup widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfePrefGroup_h_						/* start PrefGroup.h	*/
#define _XfePrefGroup_h_

#include <Xfe/Manager.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroup resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNframeThickness			"frameThickness"
#define XmNframeType				"frameType"
#define XmNframeWidgetName			"frameWidgetName"
#define XmNtitleWidgetName			"titleWidgetName"
#define XmNtitleAlignment			"titleAlignment"
#define XmNtitleDirection			"titleDirection"
#define XmNtitleOffset				"titleOffset"
#define XmNtitleSpacing				"titleSpacing"

#define XmCFrameThickness			"FrameThickness"
#define XmCFrameType				"FrameType"
#define XmCTitleAlignment			"TitleAlignment"
#define XmCTitleDirection			"TitleDirection"
#define XmCTitleOffset				"TitleOffset"
#define XmCTitleSpacing				"TitleSpacing"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroup class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfePrefGroupWidgetClass;

typedef struct _XfePrefGroupClassRec *		XfePrefGroupWidgetClass;
typedef struct _XfePrefGroupRec *			XfePrefGroupWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroup subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsPrefGroup(w)	XtIsSubclass(w,xfePrefGroupWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroup public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreatePrefGroup				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefGroup.h		*/
