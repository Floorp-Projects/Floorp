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
/* Name:		<Xfe/TempShell.h>										*/
/* Description:	XfeTempShell widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeFrameShell_h_						/* start FrameShell.h	*/
#define _XfeFrameShell_h_

#include <Xfe/Manager.h>
#include <Xfe/BypassShell.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShell resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNbeforeResizeCallback				"beforeResizeCallback"
#define XmNdeleteWindowCallback				"deleteWindowCallback"
#define XmNmoveCallback						"moveCallback"
#define XmNsaveYourselfCallback				"saveYourselfCallback"
#define XmNtitleChangedCallback				"titleChangedCallback"

#define XmNstartIconic						"startIconic"
#define XmNtrackPosition					"trackPosition"
#define XmNtrackSaveYourself				"trackSaveYourself"
#define XmNhasBeenMapped				"hasBeenMapped"
#define XmNbypassShell					"bypassShell"
#define XmNtrackSize						"trackSize"
#define XmNtrackDeleteWindow			"trackDeleteWindow"
#define XmNtrackEditres					"trackEditres"
#define XmNtrackMapping					"trackMapping"

#define XmCBypassShell						"BypassShell"
#define XmCStartIconic						"StartIconic"
#define XmCTrackEditres						"TrackEditres"
#define XmCTrackMapping						"TrackMapping"
#define XmCTrackPosition					"TrackPosition"
#define XmCTrackSaveYourself				"TrackSaveYourself"
#define XmCTrackSize						"TrackSize"


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeFrameShellWidgetClass;

typedef struct _XfeFrameShellClassRec *			XfeFrameShellWidgetClass;
typedef struct _XfeFrameShellRec *				XfeFrameShellWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsFrameShell(w)	XtIsSubclass(w,xfeFrameShellWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShell public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateFrameShell					(Widget		pw,
									 String		name,
									 ArgList	av,
									 Cardinal	ac);
/*----------------------------------------------------------------------*/
extern Widget
XfeFrameShellGetBypassShell			(Widget		w);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end FrameShell.h		*/
