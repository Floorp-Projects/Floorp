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
/* Name:		<Xfe/DynamicManager.h>									*/
/* Description:	XfeDynamicManager widget public header file.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeDynamicManager_h_				/* start DynamicManager.h	*/
#define _XfeDynamicManager_h_

#include <Xfe/Manager.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager resource names										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNdynamicChildren					"dynamicChildren"
#define XmNlinkNode							"linkNode"
#define XmNmaxDynamicChildrenHeight			"maxDynamicChildrenHeight"
#define XmNmaxDynamicChildrenWidth			"maxDynamicChildrenWidth"
#define XmNminDynamicChildrenHeight			"minDynamicChildrenHeight"
#define XmNminDynamicChildrenWidth			"minDynamicChildrenWidth"
#define XmNnumDynamicChildren				"numDynamicChildren"
#define XmNnumManagedDynamicChildren		"numManagedDynamicChildren"
#define XmNtotalDynamicChildrenHeight		"totalDynamicChildrenHeight"
#define XmNtotalDynamicChildrenWidth		"totalDynamicChildrenWidth"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager class names										*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeDynamicManagerWidgetClass;

typedef struct _XfeDynamicManagerClassRec *		XfeDynamicManagerWidgetClass;
typedef struct _XfeDynamicManagerRec *			XfeDynamicManagerWidget;

#define XfeIsDynamicManager(w)	XtIsSubclass(w,xfeDynamicManagerWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager class names										*/
/*																		*/
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif										/* end DynamicManager.h		*/
