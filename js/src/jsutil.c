/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
 * PR assertion checker.
 */
#include <stdio.h>
#include <stdlib.h>
#include "jstypes.h"
#include "jsutil.h"

#ifdef WIN32
#    include <windows.h>
#endif

#ifdef XP_MAC
#	 include <Types.h>
#    include <stdarg.h>
#	 include "jsprf.h"
#endif

#ifdef XP_MAC
/*
 * PStrFromCStr converts the source C string to a destination
 * pascal string as it copies. The dest string will
 * be truncated to fit into an Str255 if necessary.
 * If the C String pointer is NULL, the pascal string's length is
 * set to zero.
 */
static void PStrFromCStr(const char* src, Str255 dst)
{
	short 	length  = 0;
	
	/* handle case of overlapping strings */
	if ( (void*)src == (void*)dst )
	{
		unsigned char*		curdst = &dst[1];
		unsigned char		thisChar;
				
		thisChar = *(const unsigned char*)src++;
		while ( thisChar != '\0' ) 
		{
			unsigned char	nextChar;
			
			/*
                         * Use nextChar so we don't overwrite what we
                         * are about to read
                         */
			nextChar = *(const unsigned char*)src++;
			*curdst++ = thisChar;
			thisChar = nextChar;
			
			if ( ++length >= 255 )
				break;
		}
	}
	else if ( src != NULL )
	{
		unsigned char*		curdst = &dst[1];
		/* count down so test it loop is faster */
		short 				overflow = 255;
		register char		temp;
	
		/*
                 * Can't do the K&R C thing of while (*s++ = *t++)
                 * because it will copy trailing zero which might
                 * overrun pascal buffer.  Instead we use a temp variable.
                 */
		while ( (temp = *src++) != 0 ) 
		{
			*(char*)curdst++ = temp;
				
			if ( --overflow <= 0 )
				break;
		}
		length = 255 - overflow;
	}
	dst[0] = length;
}

static void debugstr(const char *debuggerMsg)
{
	Str255		pStr;
	
	PStrFromCStr(debuggerMsg, pStr);
	DebugStr(pStr);
}

static void dprintf(const char *format, ...)
{
    va_list ap;
	char	*buffer;
	
	va_start(ap, format);
	buffer = (char *)JS_vsmprintf(format, ap);
	va_end(ap);
	
	debugstr(buffer);
	JS_DELETE(buffer);
}
#endif   /* XP_MAC */

JS_EXPORT_API(void) JS_Assert(const char *s, const char *file, JSIntn ln)
{
#if defined(XP_UNIX) || defined(XP_OS2)
    fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, ln);
#endif
#ifdef XP_MAC
    dprintf("Assertion failure: %s, at %s:%d\n", s, file, ln);
#endif
#ifdef WIN32
    DebugBreak();
#endif
#ifndef XP_MAC
    abort();
#endif
}
