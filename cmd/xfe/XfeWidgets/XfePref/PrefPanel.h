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
/* Name:		<Xfe/PrefPanel.h>										*/
/* Description:	XfePrefPanel widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfePrefPanel_h_						/* start PrefPanel.h	*/
#define _XfePrefPanel_h_

#include <Xfe/Manager.h>
#include <Xfe/PrefGroup.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroup resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNsubTitle					"subTitle"
#define XmNsubTitleAlignment		"subTitleAlignment"
#define XmNsubTitleFontList			"subTitleFontList"
#define XmNsubTitleString			"subTitleString"
#define XmNtitleBackground			"titleBackground"
#define XmNtitleForeground			"titleForeground"
#define XmNsubTitleWidgetName		"subTitleWidgetName"

#define XmCSubTitleAlignment		"SubTitleAlignment"
#define XmCSubTitleFontList			"SubTitleFontList"
#define XmCSubTitleString			"SubTitleString"
#define XmCTitleBackground			"TitleBackground"
#define XmCTitleForeground			"TitleForeground"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanel class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfePrefPanelWidgetClass;

typedef struct _XfePrefPanelClassRec *		XfePrefPanelWidgetClass;
typedef struct _XfePrefPanelRec *			XfePrefPanelWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanel subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsPrefPanel(w)	XtIsSubclass(w,xfePrefPanelWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanel public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreatePrefPanel				(Widget		pw,
								 String		name,
								 Arg *		av,
								 Cardinal	ac);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefPanel.h		*/
