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
 * JSDebug StreamConverter hooks for source text in Navigator
 */

/* Please leave outside of ifdef for windows precompiled headers */
#include "lm.h"

#include "net.h"
#include "prlink.h"


#ifdef JSDEBUGGER
#include "jsdebug.h"

/***************************************/
/* a static global, oh well... */

static JSDContext* g_jsdc = NULL;

typedef struct 
{
    NET_StreamClass* next_stream;
    JSDSourceText*   jsdsrc;
    
} LocalData;

/***************************************/
/* support for dynamic library load and calling */

/* these must match declarations in jsdebug.h */

typedef void 
(*SetUserCallbacksProc)(JSRuntime* jsruntime, JSD_UserCallbacks* callbacks, void* user);

typedef JSDSourceText* 
(*NewSourceTextProc)(JSDContext* jsdc, const char* url);

typedef JSDSourceText* 
(*AppendSourceTextProc)(JSDContext* jsdc, 
                     JSDSourceText* jsdsrc,
                     const char* text,       /* *not* zero terminated */
                     size_t length,
                     JSDSourceStatus status);

static SetUserCallbacksProc g_SetUserCallbacks = NULL;
static NewSourceTextProc    g_NewSourceText    = NULL;
static AppendSourceTextProc g_AppendSourceText = NULL;
static PRLibrary*           g_JSDModule        = NULL;

#ifdef XP_PC
#if defined(_WIN32) || defined(XP_OS2)
    static char moduleName[] = "jsd3240.dll";
#else
    static char moduleName[] = "jsd1640.dll";
#endif /* _WIN32 */
#endif /* XP_PC */

#ifdef XP_MAC
	static char moduleName[] = "JavaScript Debug Support";
#endif

#ifdef XP_UNIX
    static char moduleName[] = "libjsd";
#endif /*XP_UNIX */

#ifdef XP_UNIX
static PRLibrary*
LoadUnixLibrary(const char *name)
{
    PRLibrary* lib = NULL;
    char* libPath;
    char* fullname;
    const char* rawLibPath;
    char* cur;
    char* next;

    rawLibPath = PR_GetLibraryPath();
    if( ! rawLibPath )
        return NULL;

    cur = libPath = strdup(rawLibPath);
    if( ! libPath )
        return NULL;

    do
    {
        next = strchr(cur, ':');
        if(next)
            *next++ = 0;

        if( cur[strlen(cur)-1] == '/' )
            fullname = PR_smprintf("%s%s.%s", cur, name, DLL_SUFFIX);
        else
            fullname = PR_smprintf("%s/%s.%s", cur, name, DLL_SUFFIX);

        lib = PR_LoadLibrary(fullname);
        free(fullname);

        if( lib )
            break;
    
        cur = next;    
    } while( cur && *cur );

    free(libPath);
    return lib;
} 
#endif /*XP_UNIX */


static void
UnLoadJSDModule(void)
{
    if( g_JSDModule )
        PR_UnloadLibrary(g_JSDModule);

    g_JSDModule         = NULL;
    g_SetUserCallbacks  = NULL;
    g_NewSourceText     = NULL;
    g_AppendSourceText  = NULL;
}

static int
LoadJSDModuleAndCallbacks(void)
{
#ifdef XP_MAC
	const char *libPath = PR_GetLibraryPath();
	PR_SetLibraryPath( "/usr/local/netscape/" );
    
    g_JSDModule = PR_LoadLibrary(moduleName);
	
	/* set path back to original path (don't have ourselves in list) */
	PR_SetLibraryPath( libPath );
#elif defined(XP_UNIX)
   g_JSDModule = LoadUnixLibrary(moduleName);
#else
   g_JSDModule = PR_LoadLibrary(moduleName);
#endif
    
    if( ! g_JSDModule )
        return 0;

#ifndef NSPR20
    g_SetUserCallbacks = (SetUserCallbacksProc) PR_FindSymbol( "JSD_SetUserCallbacks" , g_JSDModule);
    g_NewSourceText    = (NewSourceTextProc   ) PR_FindSymbol( "JSD_NewSourceText"    , g_JSDModule);
    g_AppendSourceText = (AppendSourceTextProc) PR_FindSymbol( "JSD_AppendSourceText" , g_JSDModule);
#else
    g_SetUserCallbacks = (SetUserCallbacksProc) PR_FindSymbol( g_JSDModule, "JSD_SetUserCallbacks");
    g_NewSourceText    = (NewSourceTextProc   ) PR_FindSymbol( g_JSDModule, "JSD_NewSourceText");
    g_AppendSourceText = (AppendSourceTextProc) PR_FindSymbol( g_JSDModule, "JSD_AppendSourceText");
#endif

    if( ! g_SetUserCallbacks    ||
        ! g_NewSourceText       ||
        ! g_AppendSourceText )
    {
        UnLoadJSDModule();
        return 0;
    }
    return 1;
}

/***************************************/
/* helpers */

PR_STATIC_CALLBACK(void)
lmjsd_SetContext(JSDContext* jsdc, void* user)
{
    g_jsdc = jsdc;
}

static void cleanup_ld( LocalData* ld )
{
    if( ld )
    {
    	XP_FREEIF(ld->next_stream);
        XP_FREE(ld);
    }
}

/***************************************/
/* converter callbacks */

PR_STATIC_CALLBACK(int)
lmjsd_CvtPutBlock (NET_StreamClass *stream, const char *str, int32 len)
{
    LocalData* ld = (LocalData*) stream->data_object;

    if( g_jsdc && ld->jsdsrc )
        ld->jsdsrc = g_AppendSourceText(g_jsdc, ld->jsdsrc,
                                        str, len, JSD_SOURCE_PARTIAL );

	return ((*ld->next_stream->put_block)(ld->next_stream,str,len));
}

PR_STATIC_CALLBACK(void)
lmjsd_CvtComplete (NET_StreamClass *stream)
{
    LocalData* ld = (LocalData*) stream->data_object;
    
    if( g_jsdc && ld->jsdsrc )
        g_AppendSourceText(g_jsdc, ld->jsdsrc,
                           NULL, 0, JSD_SOURCE_COMPLETED );

	(*ld->next_stream->complete)(ld->next_stream);
    
    cleanup_ld(ld);    
}

PR_STATIC_CALLBACK(void)
lmjsd_CvtAbort (NET_StreamClass *stream, int status)
{
    LocalData* ld = (LocalData*) stream->data_object;
    
    if( g_jsdc && ld->jsdsrc )
        g_AppendSourceText(g_jsdc, ld->jsdsrc,
                           NULL, 0, JSD_SOURCE_ABORTED );
    
	(*ld->next_stream->abort)(ld->next_stream, status);
    
    cleanup_ld(ld);    
}

PR_STATIC_CALLBACK(unsigned int)
lmjsd_CvtWriteReady (NET_StreamClass *stream)
{
    LocalData* ld = (LocalData*) stream->data_object;
    return (*ld->next_stream->is_write_ready)(ld->next_stream);
}

/***************************************/
/* exported functions */

void
lm_InitJSDebug(JSRuntime *jsruntime)
{
    JSD_UserCallbacks cbs;

    if( LoadJSDModuleAndCallbacks() )
    {
        memset(&cbs, 0, sizeof(JSD_UserCallbacks) );
        cbs.size        = sizeof(JSD_UserCallbacks);
        cbs.setContext  = lmjsd_SetContext;
        g_SetUserCallbacks( jsruntime, &cbs, &g_jsdc ); /* &g_jsdc is just a unique address */
    }

}
 
void
lm_ExitJSDebug(JSRuntime *jsruntime)
{
    if( g_SetUserCallbacks )
        g_SetUserCallbacks( jsruntime, NULL, &g_jsdc ); /* &g_jsdc is just a unique address */
    g_jsdc = NULL;
    UnLoadJSDModule();
}

JSBool
LM_GetJSDebugActive(void)
{
    return (JSBool)(NULL != g_jsdc);
}

void
LM_JamSourceIntoJSDebug( const char *filename,
                         const char *str, 
                         int32      len,
                         MWContext  *mwcontext )
{
    JSDSourceText* jsdsrc;

    if( ! g_jsdc )
        return;

    jsdsrc = g_NewSourceText(g_jsdc, filename);
    if( jsdsrc )
        jsdsrc = g_AppendSourceText(g_jsdc, jsdsrc,
                                    str, len, JSD_SOURCE_PARTIAL );
    if( jsdsrc )
        g_AppendSourceText(g_jsdc, jsdsrc,
                           NULL, 0, JSD_SOURCE_COMPLETED );
}

extern NET_StreamClass*
LM_StreamBuilder( int         format_out,
                  void        *data_obj,
                  URL_Struct  *URL_s,
                  MWContext   *mwcontext )
{
    LocalData*       ld;
    NET_StreamClass* stream;

    /* If JSDebug is not active, then don't link into chain */
    if( ! g_jsdc )
        return NET_StreamBuilder(format_out, URL_s, mwcontext);

    if( ! (ld = XP_NEW_ZAP(LocalData)) ||
        ! (ld->jsdsrc = g_NewSourceText(g_jsdc, URL_s->address)) )
    {
        cleanup_ld(ld);
        return NET_StreamBuilder(format_out, URL_s, mwcontext);
    }

    if( ! (stream = NET_NewStream("JSDebuggerConverter", lmjsd_CvtPutBlock,
                                  lmjsd_CvtComplete, lmjsd_CvtAbort, 
                                  lmjsd_CvtWriteReady, ld, mwcontext)) )
    {
        g_AppendSourceText(g_jsdc, ld->jsdsrc, NULL, 0, JSD_SOURCE_FAILED);
        cleanup_ld(ld);
        return NET_StreamBuilder(format_out, URL_s, mwcontext);
    }

    if( ! (ld->next_stream = NET_StreamBuilder(format_out, URL_s, mwcontext)) )
    {
        g_AppendSourceText(g_jsdc, ld->jsdsrc, NULL, 0, JSD_SOURCE_FAILED);
        cleanup_ld(ld);
        XP_FREE(stream);
        return NULL;
    }

    return stream;
}

#endif /* JSDEBUGGER */
