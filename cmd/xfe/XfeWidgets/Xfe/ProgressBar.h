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
/* Name:		<Xfe/ProgressBar.h>										*/
/* Description:	XfeProgressBar widget public header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeProgressBar_h_						/* start ProgressBar.h	*/
#define _XfeProgressBar_h_

#include <Xfe/Xfe.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeProgressBar resource names										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNbarColor						"barColor"
#define XmNcylonInterval				"cylonInterval"
#define XmNcylonOffset					"cylonOffset"
#define XmNcylonRunning					"cylonRunning"
#define XmNendPercent					"endPercent"
#define XmNstartPercent					"startPercent"
#define XmNcylonWidth					"cylonWidth"

#define XmCBarColor						"BarColor"
#define XmCCylonInterval				"CylonInterval"
#define XmCCylonOffset					"CylonOffset"
#define XmCCylonWidth					"CylonWidth"
#define XmCEndPercent					"EndPercent"
#define XmCStartPercent					"StartPercent"

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeProgressBar class names											*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeProgressBarWidgetClass;
    
typedef struct _XfeProgressBarClassRec *	XfeProgressBarWidgetClass;
typedef struct _XfeProgressBarRec *			XfeProgressBarWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeProgressBar subclass test macro									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsProgressBar(w)	XtIsSubclass(w,xfeProgressBarWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeProgressBar public functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget	
XfeCreateProgressBar			(Widget		w,
								 String		name,
								 ArgList	args,
								 Cardinal	nargs);
/*----------------------------------------------------------------------*/
extern void	
XfeProgressBarSetPercentages	(Widget		w,
								 int		start,
								 int		end);
/*----------------------------------------------------------------------*/
extern void	
XfeProgressBarSetComponents		(Widget		w,
								 XmString	xm_label,
								 int		start,
								 int		end);
/*----------------------------------------------------------------------*/
extern void
XfeProgressBarCylonStart		(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeProgressBarCylonStop			(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeProgressBarCylonReset		(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeProgressBarCylonTick			(Widget		w);
/*----------------------------------------------------------------------*/


XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ProgressBar.h	*/
