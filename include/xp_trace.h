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

#ifndef _XP_Trace_
#define _XP_Trace_

#include <stdarg.h>

#ifdef __cplusplus

#if defined(_WINDOWS) && !defined(DEBUG)
inline void FE_Trace( const char *msg ) {}					/* implemented by the platform */
inline void XP_Trace( const char *format, ... ) {}
inline void XP_Trace1( const char *format, ... ) {}			/* XP_Trace without the newline */
inline void XP_TraceV( const char *msg, va_list args ) {}	/* varargs XP_Trace without the newline */
#else
extern "C" void FE_Trace( const char * );					/* implemented by the platform */
extern "C" void XP_Trace( const char *, ... );
extern "C" void XP_Trace1( const char *, ... );				/* XP_Trace without the newline */
extern "C" void XP_TraceV( const char *msg, va_list args );	/* varargs XP_Trace without the newline */
#if defined(XP_MAC)
extern "C" void XP_TraceInit(void);
#endif	/* XP_MAC */
#endif /* _WINDOWS && !DEBUG */

#else

void FE_Trace( const char * );								/* implemented by the platform */
void XP_Trace( const char *, ... );
void XP_Trace1( const char *, ... );						/* XP_Trace without the newline */
void XP_TraceV( const char *msg, va_list args );			/* varargs XP_Trace without the newline */

#if defined(XP_MAC)
extern void XP_TraceInit(void);
#endif

#endif /* __cplusplus */


#ifdef DEBUG

#define XP_TRACE(MESSAGE)		XP_Trace MESSAGE
#define XP_TRACE1(MESSAGE)		XP_Trace1 MESSAGE
#define XP_LTRACE(FLAG,LEVEL,MESSAGE) \
    do { if (FLAG >= (LEVEL)) XP_Trace MESSAGE; } while (0)

#else

#define XP_TRACE(MESSAGE)		((void) (MESSAGE))
#define XP_TRACE1(MESSAGE)		((void) (MESSAGE))
#define XP_LTRACE(FLAG,LEVEL,MESSAGE)	((void) (MESSAGE))

#endif

#endif /* _XP_Trace_ */
