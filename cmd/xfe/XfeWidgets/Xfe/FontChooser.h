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
/* Name:		<Xfe/FontChooser.h>										*/
/* Description:	XfeFontChooser widget public header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeFontChooser_h_						/* start FontChooser.h	*/
#define _XfeFontChooser_h_

#include <Xfe/Cascade.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooser class names											*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeFontChooserWidgetClass;
    
typedef struct _XfeFontChooserClassRec *	XfeFontChooserWidgetClass;
typedef struct _XfeFontChooserRec *			XfeFontChooserWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooser subclass test macro									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsFontChooser(w)	XtIsSubclass(w,xfeFontChooserWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFontChooser public functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateFontChooser		(Widget		pw,
							 String		name,
							 Arg *		av,
							 Cardinal	ac);
/*----------------------------------------------------------------------*/
extern void
XfeFontChooserDestroyChildren	(Widget		w);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end FontChooser.h	*/
