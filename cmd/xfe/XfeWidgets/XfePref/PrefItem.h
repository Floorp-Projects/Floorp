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
/* Name:		<Xfe/PrefItem.h>										*/
/* Description:	XfePrefItem widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfePrefItem_h_							/* start PrefItem.h		*/
#define _XfePrefItem_h_

#include <Xfe/Manager.h>
#include <Xfe/XfePref.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRPrefItemType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmPREF_ITEM_TOGGLE_GROUP,					/*						*/
    XmPREF_ITEM_RADIO_GROUP,					/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefItem class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfePrefItemWidgetClass;

typedef struct _XfePrefItemClassRec *		XfePrefItemWidgetClass;
typedef struct _XfePrefItemRec *			XfePrefItemWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefItem subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsPrefItem(w)	XtIsSubclass(w,xfePrefItemWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefItem public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreatePrefItem				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefItem.h		*/
