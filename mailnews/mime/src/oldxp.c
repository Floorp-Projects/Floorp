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
/* 
 * i18n.c - I18N depencencies
 */

#include "xp.h"
#include "prmem.h"
#include "plstr.h"

#ifdef XP_UNIX
#define PUBLIC
#endif

/* RICHIE - THIS HACK MUST GO!!! - INCLUDING FILE TO TRY TO KEEP DIRECTORY CLEAN */
#include "../../../lib/xp/xp_time.c"

PUBLIC void * 
FE_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs)
{
    return NULL;
}

char 
*XP_GetStringForHTML (int i, PRInt16 wincsid, char* english)
{
  return english; 
}

char *XP_AppendStr(char *in, const char *append)
{
    int alen, inlen;

    alen = PL_strlen(append);
    if (in) {
		inlen = PL_strlen(in);
		in = (char*) PR_Realloc(in,inlen+alen+1);
		if (in) {
			memcpy(in+inlen, append, alen+1);
		}
    } else {
		in = (char*) PR_Malloc(alen+1);
		if (in) {
			memcpy(in, append, alen+1);
		}
    }
    return in;
}

#define MOZ_FUNCTION_STUB

char *
WH_TempName(XP_FileType type, const char * prefix)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

/* If you want to trace netlib, set this to 1, or use CTRL-ALT-T
 * stroke (preferred method) to toggle it on and off */
int MKLib_trace_flag=0;


/* Used by NET_NTrace() */
PRIVATE void net_Trace(char *msg) {
#if defined(WIN32) && defined(DEBUG)
		OutputDebugString(msg);
		OutputDebugString("\n");
#else
        PR_LogPrint(msg);
#endif        
}

/* #define'd in mktrace.h to TRACEMSG */
void ns_MK_TraceMsg(char *fmt, ...) {
	va_list ap;
	char buf[512];

	va_start(ap, fmt);
	PR_vsnprintf(buf, sizeof(buf), fmt, ap);

    net_Trace(buf);
}
