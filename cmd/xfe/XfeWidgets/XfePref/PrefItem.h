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
