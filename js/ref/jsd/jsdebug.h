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
 * Header for JavaScript Debugger API All Public Functions
 */

#ifndef jsdebug_h___
#define jsdebug_h___

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
#include "jsapi.h"
#ifdef LIVEWIRE
#include "lwdbgapi.h"
#endif
PR_END_EXTERN_C


PR_BEGIN_EXTERN_C

/***************************************************************************/
/* Opaque typedefs for handles */

typedef struct JSDContext        JSDContext;
typedef struct JSDScript         JSDScript;
typedef struct JSDSourceText     JSDSourceText;
typedef struct JSDThreadState    JSDThreadState;
typedef struct JSDStackFrameInfo JSDStackFrameInfo;

/***************************************************************************/
/* High Level calls */

/*
* callback stuff for calls in EXE to be accessed by this code 
* when it lives in an explicitly loaded DLL
*/

typedef void 
(*JSD_SetContextProc)(JSDContext* jsdc, void* user);

/* This struct could have more fields in future versions */
typedef struct
{
    uintN              size;       /* size of this struct (init before use)*/
    JSD_SetContextProc setContext;
} JSD_UserCallbacks;

extern JS_PUBLIC_API(void)
JSD_SetUserCallbacks(JSTaskState*       jstaskstate, 
                     JSD_UserCallbacks* callbacks, 
                     void*              user);

/*
* This version of the init function requires that JSD_SetUserCallbacks() 
* has been previously called (with a non-NULL callback struct pointer)
*/

extern JS_PUBLIC_API(JSDContext*)
JSD_DebuggerOn(void);

extern JS_PUBLIC_API(JSDContext*)
JSD_DebuggerOnForUser(JSTaskState*       jstaskstate, 
                      JSD_UserCallbacks* callbacks, 
                      void*              user);

extern JS_PUBLIC_API(void)
JSD_DebuggerOff(JSDContext* jsdc);

extern JS_PUBLIC_API(uintN)
JSD_GetMajorVersion(void);

extern JS_PUBLIC_API(uintN)
JSD_GetMinorVersion(void);

extern JS_PUBLIC_API(JSContext*)
JSD_GetDefaultJSContext(JSDContext* jsdc);

extern JS_PUBLIC_API(void)
JSD_JSContextInUse(JSDContext* jsdc, JSContext* context);

extern JS_PUBLIC_API(JSDContext*)
JSD_JSDContextForJSContext(JSContext* context);

/***************************************************************************/
/* Script functions */

extern JS_PUBLIC_API(void)
JSD_LockScriptSubsystem(JSDContext* jsdc);

extern JS_PUBLIC_API(void)
JSD_UnlockScriptSubsystem(JSDContext* jsdc);

extern JS_PUBLIC_API(JSDScript*)
JSD_IterateScripts(JSDContext* jsdc, JSDScript **iterp);

extern JS_PUBLIC_API(JSBool)
JSD_IsActiveScript(JSDContext* jsdc, JSDScript *jsdscript);

extern JS_PUBLIC_API(const char*)
JSD_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript);

extern JS_PUBLIC_API(const char*)
JSD_GetScriptFunctionName(JSDContext* jsdc, JSDScript *jsdscript);

extern JS_PUBLIC_API(uintN)
JSD_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript);

extern JS_PUBLIC_API(uintN)
JSD_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript);

typedef void 
(*JSD_ScriptHookProc)( JSDContext* jsdc, 
                       JSDScript*  jsdscript,
                       JSBool      creating,
                       void*       callerdata );

extern JS_PUBLIC_API(JSBool)
JSD_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata);

extern JS_PUBLIC_API(JSBool)
JSD_GetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc* hook, void** callerdata);

extern JS_PUBLIC_API(pruword)
JSD_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, uintN line);

extern JS_PUBLIC_API(uintN)
JSD_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, pruword pc);

/* these are only used in cases where scripts are created outside of JS*/
extern JS_PUBLIC_API(void)
JSD_ScriptCreated(JSDContext* jsdc,
                  JSContext   *cx,
                  const char  *filename,    /* URL this script loads from */
                  uintN       lineno,       /* line where this script starts */
                  JSScript    *script,
                  JSFunction  *fun);

extern JS_PUBLIC_API(void)
JSD_ScriptDestroyed(JSDContext* jsdc,
                    JSContext   *cx,
                    JSScript    *script);

/***************************************************************************/
/* Source Text functions */

/* these coorespond to netscape.jsdebug.SourceTextItem.java values - 
*  change in both places if anywhere 
*/

typedef enum
{
    JSD_SOURCE_INITED,
    JSD_SOURCE_PARTIAL,
    JSD_SOURCE_COMPLETED,
    JSD_SOURCE_ABORTED,
    JSD_SOURCE_FAILED,
    JSD_SOURCE_CLEARED
} JSDSourceStatus;

extern JS_PUBLIC_API(void)
JSD_LockSourceTextSubsystem(JSDContext* jsdc);

extern JS_PUBLIC_API(void)
JSD_UnlockSourceTextSubsystem(JSDContext* jsdc);

extern JS_PUBLIC_API(JSDSourceText*)
JSD_IterateSources(JSDContext* jsdc, JSDSourceText **iterp);

extern JS_PUBLIC_API(JSDSourceText*)
JSD_FindSourceForURL(JSDContext* jsdc, const char* url);

extern JS_PUBLIC_API(const char*)
JSD_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JS_PUBLIC_API(JSBool)
JSD_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen);

extern JS_PUBLIC_API(void)
JSD_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JS_PUBLIC_API(JSDSourceStatus)
JSD_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JS_PUBLIC_API(JSBool)
JSD_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JS_PUBLIC_API(void)
JSD_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, JSBool dirty);

extern JS_PUBLIC_API(uintN)
JSD_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JS_PUBLIC_API(uintN)
JSD_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

/* new for server-side USE WITH CARE */
extern JS_PUBLIC_API(void)
JSD_DestroyAllSources( JSDContext* jsdc );

/* functions for adding source items */

extern JS_PUBLIC_API(JSDSourceText*)
JSD_NewSourceText(JSDContext* jsdc, const char* url);

extern JS_PUBLIC_API(JSDSourceText*)
JSD_AppendSourceText(JSDContext*     jsdc, 
                     JSDSourceText*  jsdsrc,
                     const char*     text,       /* *not* zero terminated */
                     size_t          length,
                     JSDSourceStatus status);

/* convienence function for adding complete source of url in one call */
extern JS_PUBLIC_API(JSBool)
JSD_AddFullSourceText(JSDContext* jsdc, 
                      const char* text,       /* *not* zero terminated */
                      size_t      length,
                      const char* url);

/***************************************************************************/
/* Execution/Interrupt Hook functions */

#define JSD_HOOK_INTERRUPTED            0
#define JSD_HOOK_BREAKPOINT             1
#define JSD_HOOK_DEBUG_REQUESTED        2

#define JSD_HOOK_RETURN_HOOK_ERROR      0
#define JSD_HOOK_RETURN_CONTINUE        1
#define JSD_HOOK_RETURN_ABORT           2
#define JSD_HOOK_RETURN_RET_WITH_VAL    3   /* not yet supported */

typedef uintN 
(*JSD_ExecutionHookProc)( JSDContext*     jsdc, 
                          JSDThreadState* jsdthreadstate,
                          uintN           type,
                          void*           callerdata );
extern JS_PUBLIC_API(JSBool)
JSD_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     pruword               pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

extern JS_PUBLIC_API(JSBool)
JSD_ClearExecutionHook(JSDContext*          jsdc, 
                       JSDScript*           jsdscript,
                       pruword              pc);

extern JS_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript);

extern JS_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooks(JSDContext* jsdc);

extern JS_PUBLIC_API(JSBool)
JSD_SetInterruptHook(JSDContext*           jsdc, 
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

extern JS_PUBLIC_API(JSBool)
JSD_ClearInterruptHook(JSDContext* jsdc); 

extern JS_PUBLIC_API(JSBool)
JSD_SetDebugBreakHook(JSDContext*           jsdc, 
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata);

extern JS_PUBLIC_API(JSBool)
JSD_ClearDebugBreakHook(JSDContext* jsdc); 


/***************************************************************************/
/* Stack Frame functions */

extern JS_PUBLIC_API(uintN)
JSD_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JS_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JS_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetCallingStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

extern JS_PUBLIC_API(JSDScript*)
JSD_GetScriptForStackFrame(JSDContext* jsdc, 
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe);

extern JS_PUBLIC_API(pruword)
JSD_GetPCForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe);

extern JS_PUBLIC_API(JSBool)
JSD_EvaluateScriptInStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, uintN length,
                               const char *filename, uintN lineno, jsval *rval);

extern JS_PUBLIC_API(JSString*)
JSD_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val);

/***************************************************************************/
/* Error Reporter functions */

#define JSD_ERROR_REPORTER_PASS_ALONG   0
#define JSD_ERROR_REPORTER_RETURN       1
#define JSD_ERROR_REPORTER_DEBUG        2

typedef uintN 
(*JSD_ErrorReporter)( JSDContext*     jsdc, 
                      JSContext*      cx,
                      const char*     message, 
                      JSErrorReport*  report,
                      void*           callerdata );

extern JS_PUBLIC_API(JSBool)
JSD_SetErrorReporter(JSDContext*       jsdc, 
                     JSD_ErrorReporter reporter, 
                     void*             callerdata);

extern JS_PUBLIC_API(JSBool)
JSD_GetErrorReporter(JSDContext*        jsdc, 
                     JSD_ErrorReporter* reporter, 
                     void**             callerdata);

/***************************************************************************/
/* Livewire specific API */
#ifdef LIVEWIRE

extern JS_PUBLIC_API(LWDBGScript*)
JSDLW_GetLWScript(JSDContext* jsdc, JSDScript* jsdscript);

extern JS_PUBLIC_API(JSDSourceText*)
JSDLW_PreLoadSource(JSDContext* jsdc, LWDBGApp* app, 
                    const char* filename, JSBool clear);

extern JS_PUBLIC_API(JSDSourceText*)
JSDLW_ForceLoadSource(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JS_PUBLIC_API(JSBool)
JSDLW_RawToProcessedLineNumber(JSDContext* jsdc, JSDScript* jsdscript, 
                               uintN lineIn, uintN* lineOut);

extern JS_PUBLIC_API(JSBool)
JSDLW_ProcessedToRawLineNumber(JSDContext* jsdc, JSDScript* jsdscript, 
                               uintN lineIn, uintN* lineOut);

#endif
/***************************************************************************/

PR_END_EXTERN_C

#endif /* jsdebug_h___ */
