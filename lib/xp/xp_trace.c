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


#include <stddef.h>
#include <stdio.h>

#include "xp_trace.h"
#include "xp_mcom.h"

#ifndef NSPR20
#if defined(__sun)
#include "sunos4.h"
#endif
#endif /* NSPR20 */

void XP_TraceV (const char* message, va_list args)
{
#ifdef DEBUG
    char buf[2000]; 
	PR_vsnprintf(buf, sizeof(buf), message, args);
	FE_Trace(buf);
#endif /* DEBUG */
}

/* Trace with trailing newline */
void XP_Trace (const char* message, ...)
{
#ifdef DEBUG
    va_list args;
    va_start(args, message);
	XP_TraceV(message, args);
    va_end(args);
	FE_Trace("\n");
#endif /* DEBUG */
}

/* Trace without trailing newline */
void XP_Trace1 (const char* message, ...)
{
#ifdef DEBUG
    va_list args;
    va_start(args, message);
	XP_TraceV(message, args);
    va_end(args);
#endif /* DEBUG */
}

#if defined(XP_UNIX)
FILE *real_stderr = stderr;
#endif /* XP_UNIX */

#if defined(XP_UNIX) && defined(DEBUG)
void FE_Trace (const char* buffer)
{
#if defined(DEBUG_warren)
	int len = XP_STRLEN(buffer); /* vsprintf does not return length */
    fwrite(buffer, 1, len, real_stderr);
#endif
}
#endif /* defined(XP_UNIX) && defined(DEBUG) */

/*-----------------------------------------------------------------------------
	Macintosh FE_Trace
	Always exists, doesn't do anything unless in DEBUG
	
	Implemented in MemAllocatorPPCDebug project because of linkage conflicts.
-----------------------------------------------------------------------------*/

