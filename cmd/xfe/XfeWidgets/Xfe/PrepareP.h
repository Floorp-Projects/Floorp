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
#define _XFE_PREPARE_LABEL_STRING					XfePrepare1

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton - superclass = XfeLabel									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_BUTTON_PIXMAP					XfePrepare2
#define _XFE_PREPARE_BUTTON_ARMED_PIXMAP			XfePrepare3
#define _XFE_PREPARE_BUTTON_RAISED_PIXMAP			XfePrepare4
#define _XFE_PREPARE_BUTTON_INSENSITIVE_PIXMAP		XfePrepare5

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLogo - superclass = XfeButton										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_LOGO_ANIMATION					XfePrepare6

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab - superclass = XfeButton										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_TAB_BOTTOM_PIXMAP				XfePrepare6
#define _XFE_PREPARE_TAB_HORIZONTAL_PIXMAP			XfePrepare7
#define _XFE_PREPARE_TAB_LEFT_PIXMAP				XfePrepare8
#define _XFE_PREPARE_TAB_RIGHT_PIXMAP				XfePrepare9
#define _XFE_PREPARE_TAB_TOP_PIXMAP					XfePrepare10
#define _XFE_PREPARE_TAB_VERTICAL_PIXMAP			XfePrepare11
#define _XFE_PREPARE_TAB_BOTTOM_RAISED_PIXMAP		XfePrepare12
#define _XFE_PREPARE_TAB_HORIZONTAL_RAISED_PIXMAP	XfePrepare13
#define _XFE_PREPARE_TAB_LEFT_RAISED_PIXMAP			XfePrepare14
#define _XFE_PREPARE_TAB_RIGHT_RAISED_PIXMAP		XfePrepare15
#define _XFE_PREPARE_TAB_TOP_RAISED_PIXMAP			XfePrepare16
#define _XFE_PREPARE_TAB_VERTICAL_RAISED_PIXMAP		XfePrepare17

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox - superclass = XfeManager								*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_ARROW							XfePrepare1

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar - superclass = XfeOriented								*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_MAX_CHILD_DIMENSIONS			XfePrepare1

#endif											/* end PrepareP.h		*/
