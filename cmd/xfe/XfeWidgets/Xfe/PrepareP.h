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
/* Name:		<Xfe/PrepareP.h>										*/
/* Description:	Widget component preparation codes.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfePrepareP_h_							/* start PrepareP.h		*/
#define _XfePrepareP_h_

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel - superclass = XfePrimitive									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_LABEL_STRING					(1 << 0)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton - superclass = XfeLabel									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_BUTTON_PIXMAP					(1 << 1)
#define _XFE_PREPARE_BUTTON_ARMED_PIXMAP			(1 << 2)
#define _XFE_PREPARE_BUTTON_RAISED_PIXMAP			(1 << 3)
#define _XFE_PREPARE_BUTTON_INSENSITIVE_PIXMAP		(1 << 4)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogo - superclass = XfeButton										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_LOGO_ANIMATION					(1 << 5)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab - superclass = XfeButton										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_TAB_BOTTOM_PIXMAP				(1 << 5)
#define _XFE_PREPARE_TAB_HORIZONTAL_PIXMAP			(1 << 6)
#define _XFE_PREPARE_TAB_LEFT_PIXMAP				(1 << 7)
#define _XFE_PREPARE_TAB_RIGHT_PIXMAP				(1 << 8)
#define _XFE_PREPARE_TAB_TOP_PIXMAP					(1 << 9)
#define _XFE_PREPARE_TAB_VERTICAL_PIXMAP			(1 << 10)
#define _XFE_PREPARE_TAB_BOTTOM_RAISED_PIXMAP		(1 << 11)
#define _XFE_PREPARE_TAB_HORIZONTAL_RAISED_PIXMAP	(1 << 12)
#define _XFE_PREPARE_TAB_LEFT_RAISED_PIXMAP			(1 << 13)
#define _XFE_PREPARE_TAB_RIGHT_RAISED_PIXMAP		(1 << 14)
#define _XFE_PREPARE_TAB_TOP_RAISED_PIXMAP			(1 << 15)
#define _XFE_PREPARE_TAB_VERTICAL_RAISED_PIXMAP		(1 << 16)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox - superclass = XfeManager								*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_ARROW							(1 << 0)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar - superclass = XfeOriented								*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_MAX_CHILD_DIMENSIONS			(1 << 0)

#endif											/* end PrepareP.h		*/
