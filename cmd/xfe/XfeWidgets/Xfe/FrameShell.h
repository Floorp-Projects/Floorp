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
/* Name:		<Xfe/TempShell.h>										*/
/* Description:	XfeTempShell widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeFrameShell_h_						/* start FrameShell.h	*/
#define _XfeFrameShell_h_

#include <Xfe/Manager.h>
#include <Xfe/BypassShell.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

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

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end FrameShell.h		*/
