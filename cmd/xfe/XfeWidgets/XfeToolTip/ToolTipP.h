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
/* Name:		<Xfe/ToolTipP.h>										*/
/* Description:	XfeToolTip - Tool tip and doc string support.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeToolTipP_h_						/* start ToolTipP.h	*/
#define _XfeToolTipP_h_

#include <Xfe/ToolTip.h>
#include <Xfe/XfeP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTip Private Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
_XfeToolTipGetShell				(Widget			w);
/*----------------------------------------------------------------------*/
extern Widget
_XfeToolTipGetLabel				(Widget			w);
/*----------------------------------------------------------------------*/
extern void						
_XfeToolTipLock					(void);
/*----------------------------------------------------------------------*/
extern void
_XfeToolTipUnlock				(void);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeToolTipIsLocked				(void);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ToolTipP.h		*/

