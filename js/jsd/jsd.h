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
** Header for JavaScript Debugger for Navigator internal protos
*/

#ifndef jsd_h___
#define jsd_h___

#include "jsdebug.h"
#include "jsdbgapi.h"
#include "prmem.h"
#include "prprf.h"
#include "prmon.h"
#include "prtypes.h"
#include "prlog.h"	/* for PR_ASSERT */
#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif
#include "prclist.h"
#include "prthread.h"
/* #include "xp.h" */

NSPR_BEGIN_EXTERN_C

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
/* Our structures */

/**
* XXX This is lame. What I'm calling a JSDContext is really more of a
* TaskState. I should use 'JSDContext' for the thing I'm now calling
* a JSDContextWrapper. arg!
*/

struct JSDContext
{
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
};

typedef struct 
{
    JSContext*          context;
    JSDContext*         jsdc;
    JSErrorReporter     originalErrorReporter;

} JSDContextWrapper;

struct JSDScript
{
    PRCList     links;      /* we are part of a PRCList */
    JSDContext* jsdc;       /* JSDContext for this jsdscript */
    JSScript*   script;     /* script we are wrapping */
    JSFunction* function;   /* back pointer to owning function (can be NULL) */
    PRUintn     lineBase;   /* we cache this */
    PRUintn     lineExtent; /* we cache this */
    PRCList     hooks;      /* PRCList of JSDExecHooks for this script */
    char*       url;
};

struct JSDSourceText
{
    PRCList          links;      /* we are part of a PRCList */
    char*            url;
    MY_XP_HUGE_CHAR_PTR text;
    PRUintn          textLength;
    PRUintn          textSpace;
    JSBool           dirty;
    JSDSourceStatus  status;
    PRUintn          alterCount;
};

typedef struct JSDExecHook
{
    PRCList               links;        /* we are part of a PRCList */
    JSDScript*            jsdscript;
    prword_t              pc;
    JSD_ExecutionHookProc hook;
    void*                 callerdata;
} JSDExecHook;

struct JSDThreadState
{
    PRCList             links;        /* we are part of a PRCList */
    JSContext*          context;
    PRThread*           thread;
    PRCList             stack;
    PRUintn             stackDepth;
    PRUintn             wait;
};

struct JSDStackFrameInfo
{
    PRCList             links;        /* we are part of a PRCList */
    JSDThreadState*     jsdthreadstate;
    JSDScript*          jsdscript;
    prword_t            pc;
    JSObject*           object;
    JSObject*           thisp;
    JSStackFrame*       fp;
};

/***************************************************************************/
/* Code validation support */

#ifdef DEBUG
extern void JSD_ASSERT_VALID_CONTEXT( JSDContext* jsdc );
extern void JSD_ASSERT_VALID_SCRIPT( JSDScript* jsdscript );
extern void JSD_ASSERT_VALID_SOURCE_TEXT( JSDSourceText* jsdsrc );
extern void JSD_ASSERT_VALID_THREAD_STATE( JSDThreadState* jsdthreadstate );
extern void JSD_ASSERT_VALID_STACK_FRAME( JSDStackFrameInfo* jsdframe );
extern void JSD_ASSERT_VALID_EXEC_HOOK( JSDExecHook* jsdhook );
#else
#define JSD_ASSERT_VALID_CONTEXT(x)     ((void)x)
#define JSD_ASSERT_VALID_SCRIPT(x)      ((void)x)
#define JSD_ASSERT_VALID_SOURCE_TEXT(x) ((void)x)
#define JSD_ASSERT_VALID_THREAD_STATE(x)((void)x)
#define JSD_ASSERT_VALID_STACK_FRAME(x) ((void)x)
#define JSD_ASSERT_VALID_EXEC_HOOK(x)   ((void)x)
#endif

/***************************************************************************/
/* higher level functions */

extern JSDContext*
jsd_DebuggerOn(void);

extern void
jsd_DebuggerOff(JSDContext* jsdc);

extern void
jsd_SetUserCallbacks(JSTaskState* jstaskstate, JSD_UserCallbacks* callbacks, void* user);

extern void 
jsd_JSContextUsed( JSDContext* jsdc, JSContext* context );

extern JSDContext*
jsd_GetDefaultJSDContext(void);

extern JSD_ErrorReporter
jsd_SetErrorReporter( JSDContext* jsdc, JSD_ErrorReporter reporter, void* callerdata);

extern JSBool
jsd_IsCurrentThreadDangerous(void);

/***************************************************************************/
/* Script functions */

extern void
jsd_LockScriptSubsystem(JSDContext* jsdc);

extern void
jsd_UnlockScriptSubsystem(JSDContext* jsdc);

extern void 
jsd_DestroyAllJSDScripts( JSDContext* jsdc );

extern JSDScript*
jsd_FindJSDScript( JSDContext*  jsdc,
                   JSScript     *script );

extern JSDScript*
jsd_IterateScripts(JSDContext* jsdc, JSDScript **iterp);

extern const char*
jsd_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript);

extern const char*
jsd_GetScriptFunctionName(JSDContext* jsdc, JSDScript *jsdscript);

extern PRUintn
jsd_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript);

extern PRUintn
jsd_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript);

extern JSD_ScriptHookProc
jsd_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata);

extern JSD_ScriptHookProc
jsd_GetScriptHook(JSDContext* jsdc);

extern prword_t
jsd_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, PRUintn line);

extern PRUintn
jsd_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, prword_t pc);

extern void PR_CALLBACK
jsd_NewScriptHookProc( 
                JSContext   *cx,
                const char  *filename,      /* URL this script loads from */
                PRUintn     lineno,         /* line where this script starts */
                JSScript    *script,
                JSFunction  *fun,                
                void*       callerdata );
                
extern void PR_CALLBACK
jsd_DestroyScriptHookProc( 
                JSContext   *cx,
                JSScript    *script,
                void*       callerdata );

/* Script execution hook functions */

extern JSBool
jsd_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     prword_t              pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

extern JSBool
jsd_ClearExecutionHook(JSDContext*           jsdc, 
                       JSDScript*            jsdscript,
                       prword_t              pc);

extern JSBool
jsd_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript);

extern JSBool
jsd_ClearAllExecutionHooks(JSDContext* jsdc);

/***************************************************************************/
/* Source Text functions */

extern void
jsd_LockSourceTextSubsystem(JSDContext* jsdc);

extern void
jsd_UnlockSourceTextSubsystem(JSDContext* jsdc);

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

extern PRUintn
jsd_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern PRUintn
jsd_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JSDSourceText*
jsd_NewSourceText(JSDContext* jsdc, const char* url);

extern JSDSourceText*
jsd_AppendSourceText(JSDContext* jsdc, 
                     JSDSourceText* jsdsrc,
                     const char* text,       /* *not* zero terminated */
                     size_t length,
                     JSDSourceStatus status);

extern void
jsd_DestroyAllSources( JSDContext* jsdc );

extern const char*
jsd_BuildNormalizedURL( const char* url_string );
    
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

extern PRUintn
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

extern prword_t
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
                               const char *bytes, PRUintn length,
                               const char *filename, PRUintn lineno, jsval *rval);

extern JSString*
jsd_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val);

extern void
jsd_LockThreadsStates(JSDContext* jsdc);

extern void
jsd_UnlockThreadStates(JSDContext* jsdc);

extern JSBool
jsd_ThreadStatesIsLocked(JSDContext* jsdc);

extern JSBool
jsd_IsValidThreadState(JSDContext*        jsdc, 
                       JSDThreadState*    jsdthreadstate);

extern JSBool
jsd_IsValidFrameInThreadState(JSDContext*        jsdc, 
                              JSDThreadState*    jsdthreadstate,
                              JSDStackFrameInfo* jsdframe);

/***************************************************************************/


NSPR_END_EXTERN_C

#endif /* jsd_h___ */
