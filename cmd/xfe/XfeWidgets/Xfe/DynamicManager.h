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
