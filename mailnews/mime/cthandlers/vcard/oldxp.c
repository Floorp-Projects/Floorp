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

PUBLIC void * 
FE_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs)
{
    return NULL;
}

/* RICHIE - THIS HACK MUST GO!!! - INCLUDING FILE TO TRY TO KEEP DIRECTORY CLEAN */
#ifdef XP_UNIX
#define PUBLIC
#else
#include "..\..\..\lib\xp\xp_time.c"
#endif


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

/*
// The caller is responsible for PR_Free()ing the return string
*/
PUBLIC char *
WH_FileName (const char *NetName, XP_FileType type)
{
    MOZ_FUNCTION_STUB;

    if (type == xpHTTPCookie) {
#ifdef XP_PC
        return PL_strdup("cookies.txt");
#else
        return PL_strdup("cookies");
#endif
    } else if (type == xpCacheFAT) {
;//		sprintf(newName, "%s\\fat.db", (const char *)theApp.m_pCacheDir);
        
    } else if ((type == xpURL) || (type == xpFileToPost)) {
        /*
         * This is the body of XP_NetToDosFileName(...) which is implemented 
         * for Windows only in fegui.cpp
         */
        PRBool bChopSlash = PR_FALSE;
        char *p, *newName;

        if(!NetName)
            return NULL;
        
        //  If the name is only '/' or begins '//' keep the
        //    whole name else strip the leading '/'

        if(NetName[0] == '/')
            bChopSlash = PR_TRUE;

        // save just / as a path
        if(NetName[0] == '/' && NetName[1] == '\0')
            bChopSlash = PR_FALSE;

        // spanky Win9X path name
        if(NetName[0] == '/' && NetName[1] == '/')
            bChopSlash = PR_FALSE;

        if(bChopSlash)
            newName = PL_strdup(&(NetName[1]));
        else
            newName = PL_strdup(NetName);

        if(!newName)
            return NULL;

        if( newName[1] == '|' )
            newName[1] = ':';

        for(p = newName; *p; p++){
            if( *p == '/' )
                *p = '\\';
        }
        return(newName);
    }

    return NULL;
}

PUBLIC XP_File 
XP_FileOpen(const char * name, XP_FileType type, const XP_FilePerm perm)
{
    MOZ_FUNCTION_STUB;
    switch (type) {
        case xpURL:
        case xpFileToPost:
        case xpHTTPCookie:
        {
            XP_File fp;
            char* newName = WH_FileName(name, type);

            if (!newName) return NULL;

        	fp = fopen(newName, (char *) perm);
            PR_Free(newName);
            return fp;

        }
        default:
            break;
    }

    return NULL;
}

PUBLIC XP_Dir 
XP_OpenDir(const char * name, XP_FileType type)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

#ifndef XP_UNIX
PUBLIC void 
XP_CloseDir(XP_Dir dir)
{
    MOZ_FUNCTION_STUB;
}

PUBLIC XP_DirEntryStruct * 
XP_ReadDir(XP_Dir dir)
{                                         
    MOZ_FUNCTION_STUB;
    return NULL;
}
#endif

PUBLIC int 
XP_FileRemove(const char * name, XP_FileType type)
{
    if (PR_Delete(name) == PR_SUCCESS)
        return 0;
    return -1;
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
void _MK_TraceMsg(char *fmt, ...) {
	va_list ap;
	char buf[512];

	va_start(ap, fmt);
	PR_vsnprintf(buf, sizeof(buf), fmt, ap);

    net_Trace(buf);
}

PUBLIC int 
XP_Stat(const char * name, XP_StatStruct * info, XP_FileType type)
{
    int result = -1;
    MOZ_FUNCTION_STUB;

    switch (type) {
        case xpURL:
        case xpFileToPost: {
            char *newName = WH_FileName(name, type);
    	
            if (!newName) return -1;
            result = stat( newName, info );
            PR_Free(newName);
            break;
        }
        default:
            break;
    }
    return result;
}

char *XP_STRNCPY_SAFE (char *dest, const char *src, size_t destLength)
{
	char *result = strncpy (dest, src, --destLength);
	dest[destLength] = '\0';
	return result;
}

int
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
  size_t i;
  char *ptr = (char *) dest;

  for (i=0; i<len; i++)
  {
    *(ptr+i) = 1;
  }

  return 0;
}

