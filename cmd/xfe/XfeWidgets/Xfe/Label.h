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
/* Name:		<Xfe/Label.h>											*/
/* Description:	XfeLabel widget public header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeLabel_h_							/* start Label.h		*/
#define _XfeLabel_h_

#include <Xfe/Primitive.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeLabelWidgetClass;
    
typedef struct _XfeLabelClassRec *	XfeLabelWidgetClass;
typedef struct _XfeLabelRec *		XfeLabelWidget;


/*----------------------------------------------------------------------*/
/*																		*/
/* Label selection callback structure									*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
    Boolean		selected;				/* Label selected ?				*/
} XfeLabelSelectionChangedCallbackStruct;

/*----------------------------------------------------------------------*/
/*																		*/
/* Manager apply function type											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef XmString	(*XfeTruncateXmStringProc)	(char *		message,
												 char *		tag,
												 XmFontList	font_list,
												 int		max_width);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsLabel(w)	XtIsSubclass(w,xfeLabelWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateLabel					(Widget			pw,
								 String			name,
								 ArgList		av,
								 Cardinal		ac);
/*----------------------------------------------------------------------*/
extern void
XfeLabelSetString				(Widget			w,
								 XmString		xm_label);
/*----------------------------------------------------------------------*/
extern void
XfeLabelSetStringPSZ			(Widget			w,
								 String			psz_label);
/*----------------------------------------------------------------------*/
extern  void
XfeLabelSetSelected				(Widget			w,
								 Boolean		selected);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Label.h			*/
