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
 * JavaScript Debugger API - All public functions
 */

#include "jsd.h"

/***************************************************************************/
/* High Level calls */

JS_PUBLIC_API(JSDContext*)
JSD_DebuggerOnForUser(JSTaskState*       jstaskstate, 
                      JSD_UserCallbacks* callbacks, 
                      void*              user)
{
    return jsd_DebuggerOnForUser(jstaskstate, callbacks, user);
}

JS_PUBLIC_API(JSDContext*)
JSD_DebuggerOn(void)
{
    return jsd_DebuggerOn();
}

JS_PUBLIC_API(void)
JSD_DebuggerOff(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_DebuggerOff(jsdc);
}


JS_PUBLIC_API(uintN)
JSD_GetMajorVersion(void)
{
    return JSD_MAJOR_VERSION;    
}

JS_PUBLIC_API(uintN)
JSD_GetMinorVersion(void)
{
    return JSD_MINOR_VERSION;    
}

JS_PUBLIC_API(JSContext*)
JSD_GetDefaultJSContext(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsdc->dumbContext;
}

JS_PUBLIC_API(void)
JSD_SetUserCallbacks(JSTaskState* jstaskstate, JSD_UserCallbacks* callbacks, void* user)
{
    jsd_SetUserCallbacks(jstaskstate, callbacks, user);
}

JS_PUBLIC_API(void)
JSD_JSContextInUse(JSDContext* jsdc, JSContext* context)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_JSContextUsed(jsdc, context);
}    

JS_PUBLIC_API(JSDContext*)
JSD_JSDContextForJSContext(JSContext* context)
{
    return jsd_JSDContextForJSContext(context);
}    

/***************************************************************************/
/* Script functions */

JS_PUBLIC_API(void)
JSD_LockScriptSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_LOCK_SCRIPTS(jsdc);
}

JS_PUBLIC_API(void)
JSD_UnlockScriptSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_UNLOCK_SCRIPTS(jsdc);
}

JS_PUBLIC_API(JSDScript*)
JSD_IterateScripts(JSDContext* jsdc, JSDScript **iterp)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IterateScripts(jsdc, iterp);
}

JS_PUBLIC_API(JSBool)
JSD_IsActiveScript(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IsActiveScript(jsdc, jsdscript);
}

JS_PUBLIC_API(const char*)
JSD_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptFilename(jsdc, jsdscript);
}

JS_PUBLIC_API(const char*)
JSD_GetScriptFunctionName(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptFunctionName(jsdc, jsdscript);
}

JS_PUBLIC_API(uintN)
JSD_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptBaseLineNumber(jsdc, jsdscript);
}

JS_PUBLIC_API(uintN)
JSD_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptLineExtent(jsdc, jsdscript);
}

JS_PUBLIC_API(JSBool)
JSD_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetScriptHook(jsdc, hook, callerdata);
}

JS_PUBLIC_API(JSBool)
JSD_GetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc* hook, void** callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptHook(jsdc, hook, callerdata);
}

JS_PUBLIC_API(pruword)
JSD_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, uintN line)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetClosestPC(jsdc, jsdscript, line);
}

JS_PUBLIC_API(uintN)
JSD_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, pruword pc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetClosestLine(jsdc, jsdscript, pc);
}

JS_PUBLIC_API(void)
JSD_ScriptCreated(JSDContext* jsdc,
                  JSContext   *cx,
                  const char  *filename,    /* URL this script loads from */
                  uintN       lineno,       /* line where this script starts */
                  JSScript    *script,
                  JSFunction  *fun)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_ScriptCreated(jsdc, cx, filename, lineno, script, fun);
}    

JS_PUBLIC_API(void)
JSD_ScriptDestroyed(JSDContext* jsdc,
                    JSContext   *cx,
                    JSScript    *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_ScriptDestroyed(jsdc, cx, script);
}    

/***************************************************************************/
/* Source Text functions */

JS_PUBLIC_API(void)
JSD_LockSourceTextSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_LOCK_SOURCE_TEXT(jsdc);
}

JS_PUBLIC_API(void)
JSD_UnlockSourceTextSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_UNLOCK_SOURCE_TEXT(jsdc);
}

JS_PUBLIC_API(JSDSourceText*)
JSD_IterateSources(JSDContext* jsdc, JSDSourceText **iterp)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IterateSources(jsdc, iterp);
}

JS_PUBLIC_API(JSDSourceText*)
JSD_FindSourceForURL(JSDContext* jsdc, const char* url)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    PR_ASSERT(url);
    return jsd_FindSourceForURL(jsdc, url);
}

JS_PUBLIC_API(const char*)
JSD_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceURL(jsdc,jsdsrc);
}

JS_PUBLIC_API(JSBool)
JSD_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    PR_ASSERT(ppBuf);
    PR_ASSERT(pLen);
    return jsd_GetSourceText(jsdc, jsdsrc, ppBuf, pLen);
}

JS_PUBLIC_API(void)
JSD_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    jsd_ClearSourceText(jsdc, jsdsrc);
}


JS_PUBLIC_API(JSDSourceStatus)
JSD_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceStatus(jsdc, jsdsrc);
}

JS_PUBLIC_API(JSBool)
JSD_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_IsSourceDirty(jsdc, jsdsrc);
}

JS_PUBLIC_API(void)
JSD_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, JSBool dirty)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    jsd_SetSourceDirty(jsdc, jsdsrc, dirty);
}

JS_PUBLIC_API(uintN)
JSD_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceAlterCount(jsdc, jsdsrc);
}

JS_PUBLIC_API(uintN)
JSD_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_IncrementSourceAlterCount(jsdc, jsdsrc);
}

JS_PUBLIC_API(void)
JSD_DestroyAllSources( JSDContext* jsdc )
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_DestroyAllSources(jsdc);
}    

JS_PUBLIC_API(JSDSourceText*)
JSD_NewSourceText(JSDContext* jsdc, const char* url)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    PR_ASSERT(url);
    return jsd_NewSourceText(jsdc, url);
}

JS_PUBLIC_API(JSDSourceText*)
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

JS_PUBLIC_API(JSBool)
JSD_AddFullSourceText(JSDContext* jsdc, 
                      const char* text,       /* *not* zero terminated */
                      size_t      length,
                      const char* url)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    PR_ASSERT(url);
    return jsd_AddFullSourceText(jsdc, text, length, url);
}

/***************************************************************************/
/* Execution/Interrupt Hook functions */

JS_PUBLIC_API(JSBool)
JSD_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     pruword              pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_SetExecutionHook(jsdc, jsdscript, pc, hook, callerdata);
}

JS_PUBLIC_API(JSBool)
JSD_ClearExecutionHook(JSDContext*           jsdc, 
                       JSDScript*            jsdscript,
                       pruword              pc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_ClearExecutionHook(jsdc, jsdscript, pc);
}

JS_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_ClearAllExecutionHooksForScript(jsdc, jsdscript);
}

JS_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooks(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearAllExecutionHooks(jsdc);
}

JS_PUBLIC_API(JSBool)
JSD_SetInterruptHook(JSDContext*           jsdc, 
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetInterruptHook(jsdc, hook, callerdata);
}

JS_PUBLIC_API(JSBool)
JSD_ClearInterruptHook(JSDContext* jsdc) 
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearInterruptHook(jsdc);
}

JS_PUBLIC_API(JSBool)
JSD_SetDebugBreakHook(JSDContext*           jsdc, 
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetDebugBreakHook(jsdc, hook, callerdata);
}

JS_PUBLIC_API(JSBool)
JSD_ClearDebugBreakHook(JSDContext* jsdc) 
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearDebugBreakHook(jsdc);
}


/***************************************************************************/
/* Stack Frame functions */

JS_PUBLIC_API(uintN)
JSD_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    return jsd_GetCountOfStackFrames(jsdc, jsdthreadstate);
}

JS_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    
    return jsd_GetStackFrame(jsdc, jsdthreadstate);
}

JS_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetCallingStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    return jsd_GetCallingStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JS_PUBLIC_API(JSDScript*)
JSD_GetScriptForStackFrame(JSDContext* jsdc, 
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    return jsd_GetScriptForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JS_PUBLIC_API(pruword)
JSD_GetPCForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    return jsd_GetPCForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JS_PUBLIC_API(JSBool)
JSD_EvaluateScriptInStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, uintN length,
                               const char *filename, uintN lineno, jsval *rval)
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

JS_PUBLIC_API(JSString*)
JSD_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ValToStringInStackFrame(jsdc, jsdthreadstate, jsdframe, val);
}    

/***************************************************************************/

JS_PUBLIC_API(JSBool)
JSD_SetErrorReporter(JSDContext*       jsdc, 
                     JSD_ErrorReporter reporter, 
                     void*             callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetErrorReporter(jsdc, reporter, callerdata);
}

JS_PUBLIC_API(JSBool)
JSD_GetErrorReporter(JSDContext*        jsdc, 
                     JSD_ErrorReporter* reporter, 
                     void**             callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetErrorReporter(jsdc, reporter, callerdata);
}

/***************************************************************************/
/* Livewire specific API */
#ifdef LIVEWIRE

JS_PUBLIC_API(LWDBGScript*)
JSDLW_GetLWScript(JSDContext* jsdc, JSDScript* jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsdlw_GetLWScript(jsdc, jsdscript);
}    

JS_PUBLIC_API(JSDSourceText*)
JSDLW_PreLoadSource( JSDContext* jsdc, LWDBGApp* app, 
                     const char* filename, JSBool clear )
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    PR_ASSERT(app);
    PR_ASSERT(filename);
    return jsdlw_PreLoadSource(jsdc, app, filename, clear);
}    

JS_PUBLIC_API(JSDSourceText*)
JSDLW_ForceLoadSource( JSDContext* jsdc, JSDSourceText* jsdsrc )
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsdlw_ForceLoadSource(jsdc, jsdsrc);
}    

JS_PUBLIC_API(JSBool)
JSDLW_RawToProcessedLineNumber(JSDContext* jsdc, JSDScript* jsdscript, 
                               uintN lineIn, uintN* lineOut)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsdlw_RawToProcessedLineNumber(jsdc, jsdscript, lineIn, lineOut);
}    

JS_PUBLIC_API(JSBool)
JSDLW_ProcessedToRawLineNumber(JSDContext* jsdc, JSDScript* jsdscript, 
                               uintN lineIn, uintN* lineOut)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsdlw_ProcessedToRawLineNumber(jsdc, jsdscript, lineIn, lineOut);
}    

#endif
/***************************************************************************/
