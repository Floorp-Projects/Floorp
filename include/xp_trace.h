/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
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
