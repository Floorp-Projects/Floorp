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
/* Name:		<Xfe/ListUtilP.h>										*/
/* Description:	List misc utilities private header.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeListUtilP_h_						/* start ListUtilP.h	*/
#define _XfeListUtilP_h_

#include <Xm/Xm.h>								/* Motif public defs	*/
#include <Xm/ListP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

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

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ListUtilP.h		*/
