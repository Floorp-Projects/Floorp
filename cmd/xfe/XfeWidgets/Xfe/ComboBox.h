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
/* Name:		<Xfe/ComboBox.h>										*/
/* Description:	XfeComboBox widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeComboBox_h_							/* start ComboBox.h		*/
#define _XfeComboBox_h_

#include <Xfe/Manager.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRComboBoxType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmCOMBO_BOX_EDITABLE,						/*						*/
	XmCOMBO_BOX_READ_ONLY						/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeComboBoxWidgetClass;

typedef struct _XfeComboBoxClassRec *			XfeComboBoxWidgetClass;
typedef struct _XfeComboBoxRec *				XfeComboBoxWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsComboBox(w)	XtIsSubclass(w,xfeComboBoxWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateComboBox				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/
extern void
XfeComboBoxAddItem				(Widget		w,
								 XmString	item,
								 int		position);
/*----------------------------------------------------------------------*/
extern void
XfeComboBoxAddItemUnique		(Widget		w,
								 XmString	item,
								 int		position);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ComboBox.h		*/
