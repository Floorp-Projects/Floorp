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
/* Name:		<Xfe/StringUtil.h>										*/
/* Description:	Xfe widgets XmString utilities.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeStringUtil_h_						/* start StringUtil.h	*/
#define _XfeStringUtil_h_

#include <Xm/Xm.h>								/* Motif public defs	*/

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* Fast access to a Widget XmNlabelString.  Result is read only			*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmString
XfeFastAccessLabelString	(Widget			w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmString utils														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern XmString
XfeXmStringCopy				(Widget			w,
							 XmString		xm_string,
							 String			fallback);
/*----------------------------------------------------------------------*/
extern String
XfeXmStringGetPSZ			(XmString		xm_string,
							 char *			tag);
/*----------------------------------------------------------------------*/
extern void
XfeSetXmStringPSZ			(Widget			w,
							 String			name,
							 char *			tag,
							 char *			value);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmStringTable utils													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern XmString *
XfeXmStringTableCopy			(XmString *		items,
								 Cardinal		num_items);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end StringUtil.h		*/
