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
/* Name:		<Xfe/ListUtilP.h>										*/
/* Description:	List misc utilities private header.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeListUtilP_h_						/* start ListUtilP.h	*/
#define _XfeListUtilP_h_

#include <Xm/Xm.h>								/* Motif public defs	*/
#include <Xm/ListP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Access a XmList member												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeXmListAccess(_w,_m) (((XmListWidget) _w ) -> list . _m)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmList widget part access											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeXmListPart(w) &(((XmListWidget) w) -> list)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ListUtilP.h		*/
