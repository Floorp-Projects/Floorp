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
** JavaScript Debugger Navigator API - All public functions
*/

#include "jsd.h"

/* use a global context for now (avoid direct references to it!) */
static JSDContext _static_context;

/***************************************************************************/
/* High Level calls */

PR_PUBLIC_API(JSDContext*)
JSD_DebuggerOn(void)
{
    return jsd_DebuggerOn();
}

PR_PUBLIC_API(void)
JSD_DebuggerOff(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_DebuggerOff(jsdc);
}


PR_PUBLIC_API(PRUintn)
JSD_GetMajorVersion(void)
{
    return JSD_MAJOR_VERSION;    
}

PR_PUBLIC_API(PRUintn)
JSD_GetMinorVersion(void)
{
    return JSD_MINOR_VERSION;    
}

PR_PUBLIC_API(JSContext*)
JSD_GetDefaultJSContext(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsdc->dumbContext;
}

PR_PUBLIC_API(void)
JSD_SetUserCallbacks(JSTaskState* jstaskstate, JSD_UserCallbacks* callbacks, void* user)
{
    jsd_SetUserCallbacks(jstaskstate, callbacks, user);
}

/***************************************************************************/
/* Script functions */

PR_PUBLIC_API(void)
JSD_LockScriptSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_LockScriptSubsystem(jsdc);
}

PR_PUBLIC_API(void)
JSD_UnlockScriptSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_UnlockScriptSubsystem(jsdc);
}

PR_PUBLIC_API(JSDScript*)
JSD_IterateScripts(JSDContext* jsdc, JSDScript **iterp)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IterateScripts(jsdc, iterp);
}

PR_PUBLIC_API(const char*)
JSD_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptFilename(jsdc, jsdscript);
}

PR_PUBLIC_API(const char*)
JSD_GetScriptFunctionName(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptFunctionName(jsdc, jsdscript);
}

PR_PUBLIC_API(PRUintn)
JSD_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptBaseLineNumber(jsdc, jsdscript);
}

PR_PUBLIC_API(PRUintn)
JSD_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptLineExtent(jsdc, jsdscript);
}

PR_PUBLIC_API(JSD_ScriptHookProc)
JSD_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetScriptHook(jsdc, hook, callerdata);
}

PR_PUBLIC_API(JSD_ScriptHookProc)
JSD_GetScriptHook(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptHook(jsdc);
}

PR_PUBLIC_API(prword_t)
JSD_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, PRUintn line)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetClosestPC(jsdc, jsdscript, line);
}

PR_PUBLIC_API(PRUintn)
JSD_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, prword_t pc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetClosestLine(jsdc, jsdscript, pc);
}

/***************************************************************************/
/* Source Text functions */

PR_PUBLIC_API(void)
JSD_LockSourceTextSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_LockSourceTextSubsystem(jsdc);
}

PR_PUBLIC_API(void)
JSD_UnlockSourceTextSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_UnlockSourceTextSubsystem(jsdc);
}

PR_PUBLIC_API(JSDSourceText*)
JSD_IterateSources(JSDContext* jsdc, JSDSourceText **iterp)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IterateSources(jsdc, iterp);
}

PR_PUBLIC_API(JSDSourceText*)
JSD_FindSourceForURL(JSDContext* jsdc, const char* url)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    PR_ASSERT(url);
    return jsd_FindSourceForURL(jsdc, url);
}

PR_PUBLIC_API(const char*)
JSD_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceURL(jsdc,jsdsrc);
}

PR_PUBLIC_API(JSBool)
JSD_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    PR_ASSERT(ppBuf);
    PR_ASSERT(pLen);
    return jsd_GetSourceText(jsdc, jsdsrc, ppBuf, pLen);
}

PR_PUBLIC_API(void)
JSD_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    jsd_ClearSourceText(jsdc, jsdsrc);
}


PR_PUBLIC_API(JSDSourceStatus)
JSD_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceStatus(jsdc, jsdsrc);
}

PR_PUBLIC_API(JSBool)
JSD_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_IsSourceDirty(jsdc, jsdsrc);
}

PR_PUBLIC_API(void)
JSD_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, JSBool dirty)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    jsd_SetSourceDirty(jsdc, jsdsrc, dirty);
}

PR_PUBLIC_API(PRUintn)
JSD_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceAlterCount(jsdc, jsdsrc);
}

PR_PUBLIC_API(PRUintn)
JSD_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_IncrementSourceAlterCount(jsdc, jsdsrc);
}

PR_PUBLIC_API(JSDSourceText*)
JSD_NewSourceText(JSDContext* jsdc, const char* url)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_NewSourceText(jsdc, url);
}

PR_PUBLIC_API(JSDSourceText*)
JSD_AppendSourceText(JSDContext* jsdc, 
                     JSDSourceText* jsdsrc,
                     const char* text,       /* *not* zero terminated */
                     size_t length,
                     JSDSourceStatus status)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_AppendSourceText(jsdc, jsdsrc, text, length, status);
}

/***************************************************************************/
/* Execution/Interrupt Hook functions */

PR_PUBLIC_API(JSBool)
JSD_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     prword_t              pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_SetExecutionHook(jsdc, jsdscript, pc, hook, callerdata);
}

PR_PUBLIC_API(JSBool)
JSD_ClearExecutionHook(JSDContext*           jsdc, 
                       JSDScript*            jsdscript,
                       prword_t              pc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_ClearExecutionHook(jsdc, jsdscript, pc);
}

PR_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_ClearAllExecutionHooksForScript(jsdc, jsdscript);
}

PR_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooks(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearAllExecutionHooks(jsdc);
}

PR_PUBLIC_API(JSBool)
JSD_SetInterruptHook(JSDContext*           jsdc, 
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetInterruptHook(jsdc, hook, callerdata);
}

PR_PUBLIC_API(JSBool)
JSD_ClearInterruptHook(JSDContext* jsdc) 
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearInterruptHook(jsdc);
}

PR_PUBLIC_API(JSBool)
JSD_SetDebugBreakHook(JSDContext*           jsdc, 
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetDebugBreakHook(jsdc, hook, callerdata);
}

PR_PUBLIC_API(JSBool)
JSD_ClearDebugBreakHook(JSDContext* jsdc) 
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearDebugBreakHook(jsdc);
}


/***************************************************************************/
/* Stack Frame functions */

PR_PUBLIC_API(PRUintn)
JSD_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    return jsd_GetCountOfStackFrames(jsdc, jsdthreadstate);
}

PR_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    
    return jsd_GetStackFrame(jsdc, jsdthreadstate);
}

PR_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetCallingStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    return jsd_GetCallingStackFrame(jsdc, jsdthreadstate, jsdframe);
}

PR_PUBLIC_API(JSDScript*)
JSD_GetScriptForStackFrame(JSDContext* jsdc, 
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    return jsd_GetScriptForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

PR_PUBLIC_API(prword_t)
JSD_GetPCForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    return jsd_GetPCForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

PR_PUBLIC_API(JSBool)
JSD_EvaluateScriptInStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, PRUintn length,
                               const char *filename, PRUintn lineno, jsval *rval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    PR_ASSERT(bytes);
    PR_ASSERT(length);
    PR_ASSERT(filename);
    PR_ASSERT(rval);

    return jsd_EvaluateScriptInStackFrame(jsdc, jsdthreadstate,jsdframe, 
                                          bytes, length,
                                          filename, lineno, rval);
}

PR_PUBLIC_API(JSString*)
JSD_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ValToStringInStackFrame(jsdc, jsdthreadstate, jsdframe, val);
}    

/***************************************************************************/

PR_PUBLIC_API(JSD_ErrorReporter)
JSD_SetErrorReporter( JSDContext* jsdc, JSD_ErrorReporter reporter, void* callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetErrorReporter(jsdc, reporter, callerdata);
}
