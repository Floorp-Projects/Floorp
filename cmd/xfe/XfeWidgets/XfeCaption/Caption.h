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
/* Name:		<Xfe/Caption.h>											*/
/* Description:	XfeCaption widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeCaption_h_							/* start Caption.h		*/
#define _XfeCaption_h_

#include <Xfe/Manager.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNcaptionLayout				"captionLayout"
#define XmNchild						"child"
#define XmNchildResize					"childResize"
#define XmNmaxChildHeight				"maxChildHeight"
#define XmNmaxChildWidth				"maxChildWidth"
#define XmNtitleDirection				"titleDirection"
#define XmNtitleHorizontalAlignment		"TitleHorizontalAlignment"
#define XmNtitleVerticalAlignment		"TitleVerticalAlignment"

#define XmCCaptionHorizontalAlignment	"CaptionHorizontalAlignment"
#define XmCCaptionLayout				"CaptionLayout"
#define XmCCaptionVerticalAlignment		"CaptionVerticalAlignment"
#define XmCChild						"Child"
#define XmCChildResize					"ChildResize"
#define XmCMaxChildHeight				"MaxChildHeight"
#define XmCMaxChildWidth				"MaxChildWidth"
#define XmCTitleDirection				"TitleDirection"

#define XmRCaptionHorizontalAlignment	"CaptionHorizontalAlignment"
#define XmRCaptionLayout				"CaptionLayout"
#define XmRCaptionVerticalAlignment		"CaptionVerticalAlignment"

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRCaptionLayout														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmCAPTION_CHILD_ON_BOTTOM,
    XmCAPTION_CHILD_ON_LEFT,
    XmCAPTION_CHILD_ON_RIGHT,
    XmCAPTION_CHILD_ON_TOP
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRHorizontalAlignment												*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmCAPTION_HORIZONTAL_ALIGNMENT_LEFT,
    XmCAPTION_HORIZONTAL_ALIGNMENT_CENTER,
    XmCAPTION_HORIZONTAL_ALIGNMENT_RIGHT
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRVerticalAlignment													*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmCAPTION_VERTICAL_ALIGNMENT_BOTTOM,
    XmCAPTION_VERTICAL_ALIGNMENT_CENTER,
    XmCAPTION_VERTICAL_ALIGNMENT_TOP
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Caption max dimensions callback structure							*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
	Dimension	max_child_width;		/* Max child width				*/
	Dimension	max_child_height;		/* Max child height				*/
} XfeCaptionMaxDimensionsCallbackStruct;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeCaptionWidgetClass;

typedef struct _XfeCaptionClassRec *		XfeCaptionWidgetClass;
typedef struct _XfeCaptionRec *				XfeCaptionWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsCaption(w)	XtIsSubclass(w,xfeCaptionWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateCaption				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/
extern Dimension
XfeCaptionMaxChildWidth			(Widget		w);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Caption.h		*/
