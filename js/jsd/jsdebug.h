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
** Header for JavaScript Debugger for Navigator interfaces
*/

#ifndef jsdebug_h___
#define jsdebug_h___

#include "ntypes.h"
#ifndef NSPR20
#include "prmacros.h"
#else
#include "prtypes.h"
#endif
#include "jsapi.h"

NSPR_BEGIN_EXTERN_C

/***************************************************************************/
/* Opaque typdefs for handles */

typedef struct JSDContext        JSDContext;
typedef struct JSDScript         JSDScript;
typedef struct JSDSourceText     JSDSourceText;
typedef struct JSDThreadState    JSDThreadState;
typedef struct JSDStackFrameInfo JSDStackFrameInfo;

/***************************************************************************/
/* High Level calls */

PR_PUBLIC_API(JSDContext*)
JSD_DebuggerOn(void);

PR_PUBLIC_API(void)
JSD_DebuggerOff(JSDContext* jsdc);

PR_PUBLIC_API(PRUintn)
JSD_GetMajorVersion(void);

PR_PUBLIC_API(PRUintn)
JSD_GetMinorVersion(void);

PR_PUBLIC_API(JSContext*)
JSD_GetDefaultJSContext(JSDContext* jsdc);

/* callback stuff for calls in EXE to be accessed by this code in a DLL */

typedef void 
(*JSD_SetContextProc)(JSDContext* jsdc, void* user);

typedef struct
{
    PRUintn            size;       /* size of this struct (init before use)*/
    JSD_SetContextProc setContext;
} JSD_UserCallbacks;

PR_PUBLIC_API(void)
JSD_SetUserCallbacks(JSTaskState* jstaskstate, JSD_UserCallbacks* callbacks, void* user);

/***************************************************************************/
/* Script functions */

PR_PUBLIC_API(void)
JSD_LockScriptSubsystem(JSDContext* jsdc);

PR_PUBLIC_API(void)
JSD_UnlockScriptSubsystem(JSDContext* jsdc);

PR_PUBLIC_API(JSDScript*)
JSD_IterateScripts(JSDContext* jsdc, JSDScript **iterp);

PR_PUBLIC_API(const char*)
JSD_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript);

PR_PUBLIC_API(const char*)
JSD_GetScriptFunctionName(JSDContext* jsdc, JSDScript *jsdscript);

PR_PUBLIC_API(PRUintn)
JSD_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript);

PR_PUBLIC_API(PRUintn)
JSD_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript);

typedef void 
(*JSD_ScriptHookProc)( JSDContext* jsdc, 
                       JSDScript*  jsdscript,
                       JSBool      creating,
                       void*       callerdata );

PR_PUBLIC_API(JSD_ScriptHookProc)
JSD_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata);

PR_PUBLIC_API(JSD_ScriptHookProc)
JSD_GetScriptHook(JSDContext* jsdc);

PR_PUBLIC_API(prword_t)
JSD_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, PRUintn line);

PR_PUBLIC_API(PRUintn)
JSD_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, prword_t pc);

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

PR_PUBLIC_API(void)
JSD_LockSourceTextSubsystem(JSDContext* jsdc);

PR_PUBLIC_API(void)
JSD_UnlockSourceTextSubsystem(JSDContext* jsdc);

PR_PUBLIC_API(JSDSourceText*)
JSD_IterateSources(JSDContext* jsdc, JSDSourceText **iterp);

PR_PUBLIC_API(JSDSourceText*)
JSD_FindSourceForURL(JSDContext* jsdc, const char* url);

PR_PUBLIC_API(const char*)
JSD_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc);

PR_PUBLIC_API(JSBool)
JSD_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen);

PR_PUBLIC_API(void)
JSD_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc);

PR_PUBLIC_API(JSDSourceStatus)
JSD_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc);

PR_PUBLIC_API(JSBool)
JSD_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc);

PR_PUBLIC_API(void)
JSD_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, JSBool dirty);

PR_PUBLIC_API(PRUintn)
JSD_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

PR_PUBLIC_API(PRUintn)
JSD_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

/* functions for adding source items */

PR_PUBLIC_API(JSDSourceText*)
JSD_NewSourceText(JSDContext* jsdc, const char* url);

PR_PUBLIC_API(JSDSourceText*)
JSD_AppendSourceText(JSDContext* jsdc, 
                     JSDSourceText* jsdsrc,
                     const char* text,       /* *not* zero terminated */
                     size_t length,
                     JSDSourceStatus status);

/***************************************************************************/
/* Execution/Interrupt Hook functions */

#define JSD_HOOK_INTERRUPTED            0
#define JSD_HOOK_BREAKPOINT             1
#define JSD_HOOK_DEBUG_REQUESTED        2

#define JSD_HOOK_RETURN_HOOK_ERROR      0
#define JSD_HOOK_RETURN_CONTINUE        1
#define JSD_HOOK_RETURN_ABORT           2
#define JSD_HOOK_RETURN_RET_WITH_VAL    3   /* not yet supported */

typedef PRUintn 
(*JSD_ExecutionHookProc)( JSDContext*     jsdc, 
                          JSDThreadState* jsdthreadstate,
                          PRUintn         type,
                          void*           callerdata );
PR_PUBLIC_API(JSBool)
JSD_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     prword_t              pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

PR_PUBLIC_API(JSBool)
JSD_ClearExecutionHook(JSDContext*           jsdc, 
                       JSDScript*            jsdscript,
                       prword_t              pc);

PR_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript);

PR_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooks(JSDContext* jsdc);

PR_PUBLIC_API(JSBool)
JSD_SetInterruptHook(JSDContext*           jsdc, 
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

PR_PUBLIC_API(JSBool)
JSD_ClearInterruptHook(JSDContext* jsdc); 

PR_PUBLIC_API(JSBool)
JSD_SetDebugBreakHook(JSDContext*           jsdc, 
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata);

PR_PUBLIC_API(JSBool)
JSD_ClearDebugBreakHook(JSDContext* jsdc); 


/***************************************************************************/
/* Stack Frame functions */

PR_PUBLIC_API(PRUintn)
JSD_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

PR_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

PR_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetCallingStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
						 JSDStackFrameInfo* jsdframe);

PR_PUBLIC_API(JSDScript*)
JSD_GetScriptForStackFrame(JSDContext* jsdc, 
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe);

PR_PUBLIC_API(prword_t)
JSD_GetPCForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe);

PR_PUBLIC_API(JSBool)
JSD_EvaluateScriptInStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, PRUintn length,
                               const char *filename, PRUintn lineno, jsval *rval);

PR_PUBLIC_API(JSString*)
JSD_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val);

/***************************************************************************/
/* Error Reporter functions */

#define JSD_ERROR_REPORTER_PASS_ALONG   0
#define JSD_ERROR_REPORTER_RETURN       1
#define JSD_ERROR_REPORTER_DEBUG        2

typedef PRUintn 
(*JSD_ErrorReporter)( JSDContext*     jsdc, 
                      JSContext*      cx,
                      const char*     message, 
                      JSErrorReport*  report,
                      void*           callerdata );

PR_PUBLIC_API(JSD_ErrorReporter)
JSD_SetErrorReporter( JSDContext* jsdc, JSD_ErrorReporter reporter, void* callerdata);

NSPR_END_EXTERN_C

#endif /* jsdebug_h___ */
