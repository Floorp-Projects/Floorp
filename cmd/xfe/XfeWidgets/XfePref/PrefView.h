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
/* Name:		<Xfe/PrefView.h>										*/
/* Description:	XfePrefView widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfePrefView_h_							/* start PrefView.h		*/
#define _XfePrefView_h_

#include <Xfe/Manager.h>
#include <Xfe/PrefPanel.h>
#include <Xfe/PrefPanelInfo.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNnumGroups				"numGroups"
#define XmNpanelInfoList			"panelInfoList"
#define XmNpanelSubTitle			"panelSubTitle"
#define XmNpanelTitle				"panelTitle"
#define XmNpanelTree				"panelTree"

#define XmCNumGroups				"NumGroups"
#define XmCPanelInfoList			"PanelInfoList"
#define XmCPanelSubTitle			"PanelSubTitle"
#define XmCPanelTitle				"PanelTitle"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfePrefViewWidgetClass;

typedef struct _XfePrefViewClassRec *		XfePrefViewWidgetClass;
typedef struct _XfePrefViewRec *			XfePrefViewWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsPrefView(w)	XtIsSubclass(w,xfePrefViewWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreatePrefView				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/
extern Widget
XfePrefViewAddPanel				(Widget		w,
								 String		panel_name,
								 XmString	xm_panel_name,
								 Boolean	has_sub_panels);
/*----------------------------------------------------------------------*/
extern XfePrefPanelInfo
XfePaneViewStringtoPanelInfo	(Widget		w,
								 String		panel_name);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefView.h		*/
