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
/* Name:		<Xfe/BmButton.h>										*/
/* Description:	XfeBmButton widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeBmButton_h_							/* start BmButton.h		*/
#define _XfeBmButton_h_

#include <Xfe/Xfe.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRAccentType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmACCENT_NONE,
    XmACCENT_BOTTOM,
	XmACCENT_TOP,
    XmACCENT_ALL
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Accent filing mode													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef enum
{
    XmACCENT_FILE_ANYWHERE,
    XmACCENT_FILE_SELF
} XfeAccentFileMode;
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeBmButtonWidgetClass;
    
typedef struct _XfeBmButtonClassRec *		XfeBmButtonWidgetClass;
typedef struct _XfeBmButtonRec *			XfeBmButtonWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton subclass test macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsBmButton(w)	XtIsSubclass(w,xfeBmButtonWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBmButton public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateBmButton			(Widget			parent,
							 char *			name,
							 Arg *			args,
							 Cardinal		count);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Accent enabled/disabled functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentDisable			(void);
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentEnable			(void);
/*----------------------------------------------------------------------*/
extern Boolean
XfeBmAccentIsEnabled		(void);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade accent offset values set/get functions		*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetOffsetLeft		(Dimension		offset_left);
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetOffsetRight		(Dimension		offset_right);
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetShadowThickness	(Dimension		shadow_thickness);
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetThickness			(Dimension		thickness);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmAccentGetOffsetLeft		(void);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmAccentGetOffsetRight		(void);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmAccentGetShadowThickness	(void);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmAccentGetThickness			(void);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade accent filing mode.						*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetFileMode			(XfeAccentFileMode	file_mode);
/*----------------------------------------------------------------------*/
extern XfeAccentFileMode
XfeBmAccentGetFileMode			(void);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade pixmap offset values set/get functions		*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmPixmapSetOffset			(Dimension		offset_lefft);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmPixmapGetOffset			(void);
/*----------------------------------------------------------------------*/


#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end BmPushB.h		*/
