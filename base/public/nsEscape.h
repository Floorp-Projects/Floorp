/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*	First checked in on 98/12/03 by John R. McMullen, derived from net.h/mkparse.c. */

#ifndef _ESCAPE_H_
#define _ESCAPE_H_

#include "prtypes.h"
#include "nscore.h"

/* valid mask values for NET_Escape() and NET_EscapedSize(). */
typedef enum {
	url_XAlphas		= (1<<0)
,	url_XPAlphas	= (1<<1)
,	url_Path		= (1<<2)
} nsEscapeMask;

#ifdef __cplusplus
extern "C" {
#endif
NS_BASE char * nsEscape(const char * str, nsEscapeMask mask);
	/* Caller must use delete [] on the result */

NS_BASE char * nsUnescape(char * str);
	/* decode % escaped hex codes into character values,
	 * modifies the parameter, returns the same buffer
	 */

NS_BASE char * nsEscapeCount(const char * str, PRInt32 len, nsEscapeMask mask, PRInt32* out_len);
	/* Like nsEscape, but if out_len is non-null, return result string length
	 * in *out_len, and uses len instead of NUL termination.
	 * Caller must use delete [] on the result.
	 */
NS_BASE PRInt32 nsUnescapeCount (char * str);
	/* decode % escaped hex codes into character values,
	 * modifies the parameter buffer, returns the length of the result
	 * (result may contain \0's).
	 */

NS_BASE char *
nsEscapeHTML(const char * string);
 /*
  * Escape problem char's for HTML display 
  */

#ifdef __cplusplus
}
#endif



#endif //  _ESCAPE_H_
