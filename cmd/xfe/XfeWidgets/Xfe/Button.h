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
/* Name:		<Xfe/Button.h>											*/
/* Description:	XfeButton widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeButton_h_							/* start Button.h		*/
#define _XfeButton_h_

#include <Xfe/Label.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRButtonType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmBUTTON_NONE,								/*						*/
    XmBUTTON_PUSH,								/*						*/
    XmBUTTON_TOGGLE								/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRButtonLayout														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmBUTTON_LABEL_ONLY,
    XmBUTTON_LABEL_ON_BOTTOM,
    XmBUTTON_LABEL_ON_LEFT,
    XmBUTTON_LABEL_ON_RIGHT,
    XmBUTTON_LABEL_ON_TOP,
    XmBUTTON_PIXMAP_ONLY
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRButtonTrigger														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmBUTTON_TRIGGER_ANYWHERE,
    XmBUTTON_TRIGGER_LABEL,
    XmBUTTON_TRIGGER_PIXMAP,
    XmBUTTON_TRIGGER_EITHER,
    XmBUTTON_TRIGGER_NEITHER
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Button callback structure											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
    Boolean		armed;					/* Button armed ?				*/
    Boolean		raised;					/* Button raised ?				*/
} XfeButtonCallbackStruct;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeButtonWidgetClass;
    
typedef struct _XfeButtonClassRec *	XfeButtonWidgetClass;
typedef struct _XfeButtonRec *		XfeButtonWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsButton(w)	XtIsSubclass(w,xfeButtonWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateButton				(Widget		parent,
							 String		name,
							 Arg *		args,
							 Cardinal	num_args);
/*----------------------------------------------------------------------*/
extern void
XfeButtonPreferredGeometry	(Widget				w,
							 unsigned char		layout,
							 Dimension *		width_out,
							 Dimension *		height_out);
/*----------------------------------------------------------------------*/
extern Boolean
XfeButtonAcceptXY			(Widget				w,
							 int				x,
							 int				y);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Button.h			*/
