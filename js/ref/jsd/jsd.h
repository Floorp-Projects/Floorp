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
 * Header for JavaScript Debugger module internal stuff
 */

#ifndef jsd_h___
#define jsd_h___

/* Get prtypes.h included first. After that we can use PR macros for doing
*  this extern "C" stuff!
*/ 
#ifdef __cplusplus
extern "C"
{
#endif
#include "prtypes.h"
#ifdef __cplusplus
}
#endif

PR_BEGIN_EXTERN_C
#include "jsdebug.h"
#include "jsdbgapi.h"
#include "jsd_lock.h"   /* ifdef NETSCAPE_INTERNAL then defines JSD_THREADSAFE*/
#include "prprintf.h"
#include "prassert.h"   /* for PR_ASSERT */
#include "prhash.h"
#include "prclist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef LIVEWIRE
#include <base/pblock.h>
#include <base/session.h>
#include <frame/log.h>
#include <frame/req.h>
#endif /* LIVEWIRE */
PR_END_EXTERN_C

PR_BEGIN_EXTERN_C

#define JSD_MAJOR_VERSION 1
#define JSD_MINOR_VERSION 0

#define MY_XP_STRDUP        strdup
#define MY_XP_FREE          free
#define MY_XP_STRNCASECMP   strncasecomp
#define MY_XP_TO_LOWER      tolower

#ifdef XP_WIN16
#define MY_XP_HUGE __huge
#define MY_XP_HUGE_ALLOC(SIZE) halloc(SIZE,1)
#define MY_XP_HUGE_FREE(SIZE) hfree(SIZE)
#define MY_XP_HUGE_MEMCPY(DEST, SOURCE, LEN) hmemcpy(DEST, SOURCE, LEN)
#else
#define MY_XP_HUGE
#define MY_XP_HUGE_ALLOC(SIZE) malloc(SIZE)
#define MY_XP_HUGE_FREE(SIZE) free(SIZE)
#define MY_XP_HUGE_MEMCPY(DEST, SOURCE, LEN) memcpy(DEST, SOURCE, LEN)
#endif

#define MY_XP_HUGE_CHAR_PTR char MY_XP_HUGE *

/***************************************************************************/
/* Support for an unfortunate Navigator hack */
#if defined(NETSCAPE_INTERNAL) && ! defined(LIVEWIRE)
#define JSD_HAS_DANGEROUS_THREAD 1
#endif

/***************************************************************************/
/* Our structures */

/**
* XXX This is lame. What I'm calling a JSDContext is really more of a
* TaskState. I should use 'JSDContext' for the thing I'm now calling
* a JSDContextWrapper. arg!
*/

typedef struct JSDContext
{
    PRCList                 links;      /* we are part of a PRCList */
    JSBool                  inited;
    JSD_ScriptHookProc      scriptHook;
    void*                   scriptHookData;
    JSD_ExecutionHookProc   interruptHook;
    void*                   interruptHookData;
    JSTaskState*            jstaskstate;
    PRHashTable*            jscontexts;
    JSD_ErrorReporter       errorReporter;
    void*                   errorReporterData;
    PRCList                 threadsStates;
    JSD_ExecutionHookProc   debugBreakHook;
    void*                   debugBreakHookData;
    JSContext*              dumbContext;
    JSObject*               glob;
    JSD_UserCallbacks       userCallbacks;
    void*                   user; 
    PRCList                 scripts;
    PRCList                 sources;
    PRCList                 removedSources;
    uintN                   sourceAlterCount;

#ifdef JSD_THREADSAFE
    void*                   scriptsLock;
    void*                   sourceTextLock;
    void*                   threadStatesLock;
#endif /* JSD_THREADSAFE */

#ifdef JSD_HAS_DANGEROUS_THREAD
    void*                   dangerousThread;
#endif /* JSD_HAS_DANGEROUS_THREAD */

} JSDContext;

typedef struct JSDContextWrapper
{
    JSContext*          context;
    JSDContext*         jsdc;
    JSErrorReporter     originalErrorReporter;

} JSDContextWrapper;

typedef struct JSDScript
{
    PRCList     links;      /* we are part of a PRCList */
    JSDContext* jsdc;       /* JSDContext for this jsdscript */
    JSScript*   script;     /* script we are wrapping */
    JSFunction* function;   /* back pointer to owning function (can be NULL) */
    uintN       lineBase;   /* we cache this */
    uintN       lineExtent; /* we cache this */
    PRCList     hooks;      /* PRCList of JSDExecHooks for this script */
    char*       url;
#ifdef LIVEWIRE
    LWDBGApp*    app;    
    LWDBGScript* lwscript;
#endif
} JSDScript;

typedef struct JSDSourceText
{
    PRCList          links;      /* we are part of a PRCList */
    char*            url;
    MY_XP_HUGE_CHAR_PTR text;
    uintN            textLength;
    uintN            textSpace;
    JSBool           dirty;
    JSDSourceStatus  status;
    uintN            alterCount;
    JSBool           doingEval;
} JSDSourceText;

typedef struct JSDExecHook
{
    PRCList               links;        /* we are part of a PRCList */
    JSDScript*            jsdscript;
    pruword               pc;
    JSD_ExecutionHookProc hook;
    void*                 callerdata;
} JSDExecHook;

typedef struct JSDThreadState
{
    PRCList             links;        /* we are part of a PRCList */
    JSContext*          context;
    void*               thread;
    PRCList             stack;
    uintN               stackDepth;
} JSDThreadState;

typedef struct JSDStackFrameInfo
{
    PRCList             links;        /* we are part of a PRCList */
    JSDThreadState*     jsdthreadstate;
    JSDScript*          jsdscript;
    pruword             pc;
    JSObject*           object;
    JSObject*           thisp;
    JSStackFrame*       fp;
} JSDStackFrameInfo;

/***************************************************************************/
/* Code validation support */

#ifdef DEBUG
extern void JSD_ASSERT_VALID_CONTEXT(JSDContext* jsdc);
extern void JSD_ASSERT_VALID_SCRIPT(JSDScript* jsdscript);
extern void JSD_ASSERT_VALID_SOURCE_TEXT(JSDSourceText* jsdsrc);
extern void JSD_ASSERT_VALID_THREAD_STATE(JSDThreadState* jsdthreadstate);
extern void JSD_ASSERT_VALID_STACK_FRAME(JSDStackFrameInfo* jsdframe);
extern void JSD_ASSERT_VALID_EXEC_HOOK(JSDExecHook* jsdhook);
#else
#define JSD_ASSERT_VALID_CONTEXT(x)     ((void)0)
#define JSD_ASSERT_VALID_SCRIPT(x)      ((void)0)
#define JSD_ASSERT_VALID_SOURCE_TEXT(x) ((void)0)
#define JSD_ASSERT_VALID_THREAD_STATE(x)((void)0)
#define JSD_ASSERT_VALID_STACK_FRAME(x) ((void)0)
#define JSD_ASSERT_VALID_EXEC_HOOK(x)   ((void)0)
#endif

/***************************************************************************/
/* higher level functions */

extern JSDContext*
jsd_DebuggerOnForUser(JSTaskState*       jstaskstate, 
                      JSD_UserCallbacks* callbacks, 
                      void*              user);
extern JSDContext*
jsd_DebuggerOn(void);

extern void
jsd_DebuggerOff(JSDContext* jsdc);

extern void
jsd_SetUserCallbacks(JSTaskState* jstaskstate, JSD_UserCallbacks* callbacks, void* user);

extern void 
jsd_JSContextUsed(JSDContext* jsdc, JSContext* context);

extern JSDContext*
jsd_JSDContextForJSContext(JSContext* context);

extern JSBool
jsd_SetErrorReporter(JSDContext*       jsdc, 
                     JSD_ErrorReporter reporter, 
                     void*             callerdata);

extern JSBool
jsd_GetErrorReporter(JSDContext*        jsdc, 
                     JSD_ErrorReporter* reporter, 
                     void**             callerdata);

/***************************************************************************/
/* Script functions */

extern void 
jsd_DestroyAllJSDScripts(JSDContext* jsdc);

extern JSDScript*
jsd_FindJSDScript(JSDContext*  jsdc,
                  JSScript     *script);

extern JSDScript*
jsd_IterateScripts(JSDContext* jsdc, JSDScript **iterp);

extern JSBool
jsd_IsActiveScript(JSDContext* jsdc, JSDScript *jsdscript);

extern const char*
jsd_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript);

extern const char*
jsd_GetScriptFunctionName(JSDContext* jsdc, JSDScript *jsdscript);

extern uintN
jsd_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript);

extern uintN
jsd_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript);

extern JSBool
jsd_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata);

extern JSBool
jsd_GetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc* hook, void** callerdata);

extern pruword
jsd_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, uintN line);

extern uintN
jsd_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, pruword pc);

extern void PR_CALLBACK
jsd_NewScriptHookProc(
                JSContext   *cx,
                const char  *filename,      /* URL this script loads from */
                uintN       lineno,         /* line where this script starts */
                JSScript    *script,
                JSFunction  *fun,                
                void*       callerdata);
                
extern void PR_CALLBACK
jsd_DestroyScriptHookProc(
                JSContext   *cx,
                JSScript    *script,
                void*       callerdata);

/* Script execution hook functions */

extern JSBool
jsd_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     pruword               pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

extern JSBool
jsd_ClearExecutionHook(JSDContext*           jsdc, 
                       JSDScript*            jsdscript,
                       pruword               pc);

extern JSBool
jsd_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript);

extern JSBool
jsd_ClearAllExecutionHooks(JSDContext* jsdc);

extern void
jsd_ScriptCreated(JSDContext* jsdc,
                  JSContext   *cx,
                  const char  *filename,    /* URL this script loads from */
                  uintN       lineno,       /* line where this script starts */
                  JSScript    *script,
                  JSFunction  *fun);

extern void
jsd_ScriptDestroyed(JSDContext* jsdc,
                    JSContext   *cx,
                    JSScript    *script);

/***************************************************************************/
/* Source Text functions */

extern JSDSourceText*
jsd_IterateSources(JSDContext* jsdc, JSDSourceText **iterp);

extern JSDSourceText*
jsd_FindSourceForURL(JSDContext* jsdc, const char* url);

extern const char*
jsd_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JSBool
jsd_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen);

extern void
jsd_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JSDSourceStatus
jsd_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JSBool
jsd_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern void
jsd_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, JSBool dirty);

extern uintN
jsd_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern uintN
jsd_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JSDSourceText*
jsd_NewSourceText(JSDContext* jsdc, const char* url);

extern JSDSourceText*
jsd_AppendSourceText(JSDContext* jsdc, 
                     JSDSourceText* jsdsrc,
                     const char* text,       /* *not* zero terminated */
                     size_t length,
                     JSDSourceStatus status);

/* convienence function for adding complete source of url in one call */
extern JSBool
jsd_AddFullSourceText(JSDContext* jsdc, 
                      const char* text,       /* *not* zero terminated */
                      size_t      length,
                      const char* url);

extern void
jsd_DestroyAllSources(JSDContext* jsdc);

extern const char*
jsd_BuildNormalizedURL(const char* url_string);

extern void
jsd_StartingEvalUsingFilename(JSDContext* jsdc, const char* url);

extern void
jsd_FinishedEvalUsingFilename(JSDContext* jsdc, const char* url);
    
/***************************************************************************/
/* Interrupt Hook functions */

extern JSBool
jsd_SetInterruptHook(JSDContext*           jsdc, 
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

extern JSBool
jsd_ClearInterruptHook(JSDContext* jsdc); 

extern JSBool
jsd_SetDebugBreakHook(JSDContext*           jsdc, 
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata);

extern JSBool
jsd_ClearDebugBreakHook(JSDContext* jsdc); 

/***************************************************************************/
/* Stack Frame functions */

extern uintN
jsd_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JSDStackFrameInfo*
jsd_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JSDStackFrameInfo*
jsd_GetCallingStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

extern JSDScript*
jsd_GetScriptForStackFrame(JSDContext* jsdc, 
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe);

extern pruword
jsd_GetPCForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe);

extern JSDThreadState*
jsd_NewThreadState(JSDContext* jsdc, JSContext *cx);

extern void
jsd_DestroyThreadState(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JSBool
jsd_EvaluateScriptInStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, uintN length,
                               const char *filename, uintN lineno, jsval *rval);

extern JSString*
jsd_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val);

extern JSBool
jsd_IsValidThreadState(JSDContext*        jsdc, 
                       JSDThreadState*    jsdthreadstate);

extern JSBool
jsd_IsValidFrameInThreadState(JSDContext*        jsdc, 
                              JSDThreadState*    jsdthreadstate,
                              JSDStackFrameInfo* jsdframe);

/***************************************************************************/
/* Locking support */

/* protos are in js_lock.h for: 
 *      jsd_CreateLock
 *      jsd_Lock
 *      jsd_Unlock
 *      jsd_IsLocked
 *      jsd_CurrentThread
 */

#ifdef JSD_THREADSAFE

/* the system-wide lock */
extern void* _jsd_global_lock;
#define JSD_LOCK()                               \
    PR_BEGIN_MACRO                               \
        if(!_jsd_global_lock)                    \
            _jsd_global_lock = jsd_CreateLock(); \
        PR_ASSERT(_jsd_global_lock);             \
        jsd_Lock(_jsd_global_lock);              \
    PR_END_MACRO

#define JSD_UNLOCK()                             \
    PR_BEGIN_MACRO                               \
        PR_ASSERT(_jsd_global_lock);             \
        jsd_Unlock(_jsd_global_lock);            \
    PR_END_MACRO

/* locks for the subsystems of a given context */
#define JSD_INIT_LOCKS(jsdc)                                    \
    ( (NULL != (jsdc->scriptsLock = jsd_CreateLock())) &&       \
      (NULL != (jsdc->sourceTextLock = jsd_CreateLock())) &&    \
      (NULL != (jsdc->threadStatesLock = jsd_CreateLock())) )

#define JSD_LOCK_SCRIPTS(jsdc)        jsd_Lock(jsdc->scriptsLock)
#define JSD_UNLOCK_SCRIPTS(jsdc)      jsd_Unlock(jsdc->scriptsLock)

#define JSD_LOCK_SOURCE_TEXT(jsdc)    jsd_Lock(jsdc->sourceTextLock)
#define JSD_UNLOCK_SOURCE_TEXT(jsdc)  jsd_Unlock(jsdc->sourceTextLock)

#define JSD_LOCK_THREADSTATES(jsdc)   jsd_Lock(jsdc->threadStatesLock)
#define JSD_UNLOCK_THREADSTATES(jsdc) jsd_Unlock(jsdc->threadStatesLock)

#else  /* !JSD_THREADSAFE */

#define JSD_LOCK()                    ((void)0)
#define JSD_UNLOCK()                  ((void)0)

#define JSD_INIT_LOCKS(jsdc)          1

#define JSD_LOCK_SCRIPTS(jsdc)        ((void)0)
#define JSD_UNLOCK_SCRIPTS(jsdc)      ((void)0)

#define JSD_LOCK_SOURCE_TEXT(jsdc)    ((void)0)
#define JSD_UNLOCK_SOURCE_TEXT(jsdc)  ((void)0)

#define JSD_LOCK_THREADSTATES(jsdc)   ((void)0)
#define JSD_UNLOCK_THREADSTATES(jsdc) ((void)0)

#endif /* JSD_THREADSAFE */

/* NOTE: These are intended for ASSERTs. Thus we supply checks for both
 * LOCKED and UNLOCKED (rather that just LOCKED and !LOCKED) so that in 
 * the DEBUG non-Threadsafe case we can have an ASSERT that always succeeds
 * without having to special case things in the code.
 */
#if defined(JSD_THREADSAFE) && defined(DEBUG)
#define JSD_SCRIPTS_LOCKED(jsdc)        (jsd_IsLocked(jsdc->scriptsLock))
#define JSD_SOURCE_TEXT_LOCKED(jsdc)    (jsd_IsLocked(jsdc->sourceTextLock))
#define JSD_THREADSTATES_LOCKED(jsdc)   (jsd_IsLocked(jsdc->threadStatesLock))
#define JSD_SCRIPTS_UNLOCKED(jsdc)      (!jsd_IsLocked(jsdc->scriptsLock))
#define JSD_SOURCE_TEXT_UNLOCKED(jsdc)  (!jsd_IsLocked(jsdc->sourceTextLock))
#define JSD_THREADSTATES_UNLOCKED(jsdc) (!jsd_IsLocked(jsdc->threadStatesLock))
#else
#define JSD_SCRIPTS_LOCKED(jsdc)        1
#define JSD_SOURCE_TEXT_LOCKED(jsdc)    1
#define JSD_THREADSTATES_LOCKED(jsdc)   1
#define JSD_SCRIPTS_UNLOCKED(jsdc)      1
#define JSD_SOURCE_TEXT_UNLOCKED(jsdc)  1
#define JSD_THREADSTATES_UNLOCKED(jsdc) 1
#endif /* defined(JSD_THREADSAFE) && defined(DEBUG) */

/***************************************************************************/
/* Threading support */

#ifdef JSD_THREADSAFE

#define JSD_CURRENT_THREAD()        jsd_CurrentThread()

#else  /* !JSD_THREADSAFE */

#define JSD_CURRENT_THREAD()        ((void*)0)

#endif /* JSD_THREADSAFE */

/***************************************************************************/
/* Dangerous thread support */

#ifdef JSD_HAS_DANGEROUS_THREAD

#define JSD_IS_DANGEROUS_THREAD(jsdc) \
    (JSD_CURRENT_THREAD() == jsdc->dangerousThread)

#else  /* !JSD_HAS_DANGEROUS_THREAD */

#define JSD_IS_DANGEROUS_THREAD(jsdc)   0

#endif /* JSD_HAS_DANGEROUS_THREAD */

/***************************************************************************/
/* Livewire specific API */
#ifdef LIVEWIRE

extern LWDBGScript*
jsdlw_GetLWScript(JSDContext* jsdc, JSDScript* jsdscript);

extern char*
jsdlw_BuildAppRelativeFilename(LWDBGApp* app, const char* filename);

extern JSDSourceText*
jsdlw_PreLoadSource(JSDContext* jsdc, LWDBGApp* app, 
                     const char* filename, JSBool clear);

extern JSDSourceText*
jsdlw_ForceLoadSource(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JSBool
jsdlw_UserCodeAtPC(JSDContext* jsdc, JSDScript* jsdscript, pruword pc);

extern JSBool
jsdlw_RawToProcessedLineNumber(JSDContext* jsdc, JSDScript* jsdscript, 
                               uintN lineIn, uintN* lineOut);

extern JSBool
jsdlw_ProcessedToRawLineNumber(JSDContext* jsdc, JSDScript* jsdscript, 
                               uintN lineIn, uintN* lineOut);


#if 0
/* our hook proc for LiveWire app start/stop */
extern void PR_CALLBACK
jsdlw_AppHookProc(LWDBGApp* app,         
                  PRBool created,
                  void *callerdata);    
#endif


#endif
/***************************************************************************/

PR_END_EXTERN_C

#endif /* jsd_h___ */
