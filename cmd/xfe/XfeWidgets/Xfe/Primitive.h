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
/* Name:		<Xfe/Primitive.h>										*/
/* Description:	XfePrimitive widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfePrimitive_h_						/* start Primitive.h	*/
#define _XfePrimitive_h_

#include <Xfe/Xfe.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNenterCallback					"enterCallback"
#define XmNleaveCallback					"leaveCallback"

#define XmNaccentBorderThickness			"accentBorderThickness"
#define XmNbufferType						"bufferType"
#define XmNcursor							"cursor"
#define XmNcursorOn							"cursorOn"
#define XmNinstancePointer					"instancePointer"
#define XmNnumPopupChildren					"numPopupChildren"
#define XmNpointerInside					"pointerInside"
#define XmNpopupChildren					"popupChildren"
#define XmNpreferredHeight					"preferredHeight"
#define XmNpreferredWidth					"preferredWidth"
#define XmNpretendSensitive					"pretendSensitive"
#define XmNusePreferredHeight				"usePreferredHeight"
#define XmNusePreferredWidth				"usePreferredWidth"

#define XmCAccentBorderThickness			"AccentBorderThickness"
#define XmCBufferType						"BufferType"
#define XmCInstancePointer					"InstancePointer"
#define XmCPretendSensitive					"PretendSensitive"
#define XmCUsePreferredHeight				"UsePreferredHeight"
#define XmCUsePreferredWidth				"UsePreferredWidth"

#define XmRBufferType						"BufferType"

/*----------------------------------------------------------------------*/
/*																		*/
/* Resources shared by more than one sub class.							*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNdragThreshold					"dragThreshold"
#define XmNpoppedUp							"poppedUp"
#define XmNtitleFontList					"titleFontList"

#define XmCDragThreshold					"DragThreshold"
#define XmCTitleFontList					"TitleFontList"
#define XmCTitleFontList					"TitleFontList"

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRBufferType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmBUFFER_SHARED,							/*						*/
    XmBUFFER_NONE,								/*						*/
    XmBUFFER_PRIVATE							/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfePrimitiveWidgetClass;

typedef struct _XfePrimitiveClassRec *		XfePrimitiveWidgetClass;
typedef struct _XfePrimitiveRec *			XfePrimitiveWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsPrimitive(w)	XtIsSubclass(w,xfePrimitiveWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* Public pretend sensitive methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeSetPretendSensitive			(Widget			w,
								 Boolean		state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeIsPretendSensitive			(Widget			w);
/*----------------------------------------------------------------------*/
extern Boolean
XfeIsSensitive					(Widget			w);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Primitive.h		*/
