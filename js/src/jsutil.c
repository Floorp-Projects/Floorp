/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * PR assertion checker.
 */
#include "jsstddef.h"
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

#if defined(XP_OS2) && defined(DEBUG)
/* Added definitions for DebugBreak() for 2 different OS/2 compilers.  Doing
 * the int3 on purpose for Visual Age so that a developer can step over the
 * instruction if so desired.  Not always possible if trapping due to exception
 * handling IBM-AKR
 */
#if defined(XP_OS2_VACPP)
   #include <builtin.h>
   #define DebugBreak() { _interrupt(3); }
#elif defined(XP_OS2_EMX)
   /* Force a trap */
   #define DebugBreak() { int *pTrap=NULL; *pTrap = 1; }
#else
   #define DebugBreak()
#endif

#elif defined(XP_OS2)
   #define DebugBreak()
#endif /* XP_OS2 && DEBUG */

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

static void jsdebugstr(const char *debuggerMsg)
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

	jsdebugstr(buffer);
	JS_DELETE(buffer);
}
#endif   /* XP_MAC */

JS_PUBLIC_API(void) JS_Assert(const char *s, const char *file, JSIntn ln)
{
#if defined(XP_UNIX) || defined(XP_OS2)
    fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, ln);
#endif
#ifdef XP_MAC
    dprintf("Assertion failure: %s, at %s:%d\n", s, file, ln);
#endif
#if defined(WIN32) || defined(XP_OS2)
    DebugBreak();
#endif
#ifndef XP_MAC
    abort();
#endif
}
