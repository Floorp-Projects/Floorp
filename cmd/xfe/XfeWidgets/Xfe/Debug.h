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
/* Name:		<Xfe/Debug.h>											*/
/* Description:	Xfe widgets functions for debugging.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeDebug_h_							/* start Debug.h		*/
#define _XfeDebug_h_

#include <Xm/Xm.h>								/* Motif public defs	*/
#include <stdio.h>								/* stdio				*/

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

#ifdef DEBUG									/* ifdef DEBUG			*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Debug functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern String
XfeDebugXmStringToStaticPSZ			(XmString		xmstring);
/*----------------------------------------------------------------------*/
extern void
XfeDebugPrintArgVector				(FILE *			fp,
									 String			prefix,
									 String			suffix,
									 ArgList		av,
									 Cardinal		ac);
/*----------------------------------------------------------------------*/
extern String
XfeDebugRepTypeValueToName			(String			rep_type,
									 unsigned char	value);
/*----------------------------------------------------------------------*/
extern unsigned char
XfeDebugRepTypeNameToValue			(String			rep_type,
									 char *			name);
/*----------------------------------------------------------------------*/
extern unsigned char
XfeDebugRepTypeIndexToValue			(String			rep_type,
									 Cardinal		i);
/*----------------------------------------------------------------------*/

#endif											/* endif DEBUG			*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Debug.h			*/
