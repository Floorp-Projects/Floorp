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
/* Name:		<Xfe/Label.h>											*/
/* Description:	XfeLabel widget public header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeLabel_h_							/* start Label.h		*/
#define _XfeLabel_h_

#include <Xfe/Primitive.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel resource names												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNselectionChangedCallback		"selectionChangedCallback"

#define XmNeditModifiers				"editModifiers"
#define XmNlabelDirection				"labelDirection"
#define XmNselected						"selected"
#define XmNselectionModifiers			"selectionModifiers"
#define XmNselectionColor				"selectionColor"
#define XmNtruncateLabel				"truncateLabel"
#define XmNlabelAlignment				"labelAlignment"
#define XmNtruncateProc					"truncateProc"
#define XmNunderlineThickness			"underlineThickness"

#define XmCEditModifiers				"EditModifiers"
#define XmCFontItemLabels				"FontItemLabels"
#define XmCLabelAlignment				"LabelAlignment"
#define XmCLabelDirection				"LabelDirection"
#define XmCSelectionModifiers			"SelectionModifiers"
#define XmCSelected						"Selected"
#define XmCSelectionColor				"SelectionColor"
#define XmCTruncateLabel				"TruncateLabel"
#define XmCTruncateProc					"TruncateProc"

#define XmRModifiers					"Modifiers"

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

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Label.h			*/
