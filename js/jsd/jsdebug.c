/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JavaScript Debugging support - All public functions
 */

#include "jsd.h"

/***************************************************************************/
/* High Level calls */

JSD_PUBLIC_API(JSDContext*)
JSD_DebuggerOnForUser(JSRuntime*         jsrt,
                      JSD_UserCallbacks* callbacks,
                      void*              user)
{
    return jsd_DebuggerOnForUser(jsrt, callbacks, user, NULL);
}

JSD_PUBLIC_API(JSDContext*)
JSD_DebuggerOn(void)
{
    return jsd_DebuggerOn();
}

JSD_PUBLIC_API(void)
JSD_DebuggerOff(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_DebuggerOff(jsdc);
}

JSD_PUBLIC_API(void)
JSD_DebuggerPause(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_DebuggerPause(jsdc, JS_FALSE);
}

JSD_PUBLIC_API(void)
JSD_DebuggerUnpause(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_DebuggerUnpause(jsdc);
}

JSD_PUBLIC_API(unsigned)
JSD_GetMajorVersion(void)
{
    return JSD_MAJOR_VERSION;
}

JSD_PUBLIC_API(unsigned)
JSD_GetMinorVersion(void)
{
    return JSD_MINOR_VERSION;
}

JSD_PUBLIC_API(JSContext*)
JSD_GetDefaultJSContext(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsdc->dumbContext;
}

JSD_PUBLIC_API(JSRuntime*)
JSD_GetJSRuntime(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsdc->jsrt;
}

JSD_PUBLIC_API(void)
JSD_SetUserCallbacks(JSRuntime* jsrt, JSD_UserCallbacks* callbacks, void* user)
{
    jsd_SetUserCallbacks(jsrt, callbacks, user);
}

JSD_PUBLIC_API(void)
JSD_JSContextInUse(JSDContext* jsdc, JSContext* context)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    /* we no longer need this information - may need it again in the future */
}

JSD_PUBLIC_API(void *)
JSD_SetContextPrivate(JSDContext *jsdc, void *data)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetContextPrivate (jsdc, data);
}

JSD_PUBLIC_API(void *)
JSD_GetContextPrivate(JSDContext *jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetContextPrivate (jsdc);
}

JSD_PUBLIC_API(void)
JSD_ClearAllProfileData(JSDContext *jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_ClearAllProfileData(jsdc);    
}

JSD_PUBLIC_API(void)
JSD_SetContextFlags(JSDContext *jsdc, uint32_t flags)
{
    uint32_t oldFlags = jsdc->flags;
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsdc->flags = flags;
    if (flags & JSD_COLLECT_PROFILE_DATA) {
        /* Need to reenable our call hooks now */
        JS_SetExecuteHook(jsdc->jsrt, jsd_TopLevelCallHook, jsdc);
        JS_SetCallHook(jsdc->jsrt, jsd_FunctionCallHook, jsdc);
    }
}

JSD_PUBLIC_API(uint32_t)
JSD_GetContextFlags(JSDContext *jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsdc->flags;
}
    
JSD_PUBLIC_API(JSDContext*)
JSD_JSDContextForJSContext(JSContext* context)
{
    return jsd_JSDContextForJSContext(context);
}

/***************************************************************************/
/* Script functions */

JSD_PUBLIC_API(void)
JSD_LockScriptSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_LOCK_SCRIPTS(jsdc);
}

JSD_PUBLIC_API(void)
JSD_UnlockScriptSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_UNLOCK_SCRIPTS(jsdc);
}

JSD_PUBLIC_API(JSDScript*)
JSD_IterateScripts(JSDContext* jsdc, JSDScript **iterp)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IterateScripts(jsdc, iterp);
}

JSD_PUBLIC_API(uint32_t)
JSD_GetScriptFlags(JSDContext *jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptFlags(jsdc, script);
}
    
JSD_PUBLIC_API(void)
JSD_SetScriptFlags(JSDContext *jsdc, JSDScript *script, uint32_t flags)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_SetScriptFlags(jsdc, script, flags);
}

JSD_PUBLIC_API(unsigned)
JSD_GetScriptCallCount(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptCallCount(jsdc, script);
}

JSD_PUBLIC_API(unsigned)
JSD_GetScriptMaxRecurseDepth(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptMaxRecurseDepth(jsdc, script);
}
    

JSD_PUBLIC_API(double)
JSD_GetScriptMinExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptMinExecutionTime(jsdc, script);
}
    
JSD_PUBLIC_API(double)
JSD_GetScriptMaxExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptMaxExecutionTime(jsdc, script);
}

JSD_PUBLIC_API(double)
JSD_GetScriptTotalExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptTotalExecutionTime(jsdc, script);
}

JSD_PUBLIC_API(double)
JSD_GetScriptMinOwnExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptMinOwnExecutionTime(jsdc, script);
}
    
JSD_PUBLIC_API(double)
JSD_GetScriptMaxOwnExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptMaxOwnExecutionTime(jsdc, script);
}

JSD_PUBLIC_API(double)
JSD_GetScriptTotalOwnExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptTotalOwnExecutionTime(jsdc, script);
}

JSD_PUBLIC_API(void)
JSD_ClearScriptProfileData(JSDContext* jsdc, JSDScript *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_ClearScriptProfileData(jsdc, script);
}

JSD_PUBLIC_API(JSScript*)
JSD_GetJSScript(JSDContext* jsdc, JSDScript *script)
{
    return jsd_GetJSScript(jsdc, script);
}

JSD_PUBLIC_API(JSFunction*)
JSD_GetJSFunction(JSDContext* jsdc, JSDScript *script)
{
    return jsd_GetJSFunction (jsdc, script);
}

JSD_PUBLIC_API(void *)
JSD_SetScriptPrivate(JSDScript *jsdscript, void *data)
{
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_SetScriptPrivate (jsdscript, data);
}

JSD_PUBLIC_API(void *)
JSD_GetScriptPrivate(JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptPrivate (jsdscript);
}
    

JSD_PUBLIC_API(JSBool)
JSD_IsActiveScript(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IsActiveScript(jsdc, jsdscript);
}

JSD_PUBLIC_API(const char*)
JSD_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptFilename(jsdc, jsdscript);
}

JSD_PUBLIC_API(JSString *)
JSD_GetScriptFunctionId(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptFunctionId(jsdc, jsdscript);
}

JSD_PUBLIC_API(unsigned)
JSD_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptBaseLineNumber(jsdc, jsdscript);
}

JSD_PUBLIC_API(unsigned)
JSD_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetScriptLineExtent(jsdc, jsdscript);
}

JSD_PUBLIC_API(JSBool)
JSD_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetScriptHook(jsdc, hook, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_GetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc* hook, void** callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptHook(jsdc, hook, callerdata);
}

JSD_PUBLIC_API(uintptr_t)
JSD_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, unsigned line)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetClosestPC(jsdc, jsdscript, line);
}

JSD_PUBLIC_API(unsigned)
JSD_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, uintptr_t pc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetClosestLine(jsdc, jsdscript, pc);
}

JSD_PUBLIC_API(JSBool)
JSD_GetLinePCs(JSDContext* jsdc, JSDScript* jsdscript,
               unsigned startLine, unsigned maxLines,
               unsigned* count, unsigned** lines, uintptr_t** pcs)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_GetLinePCs(jsdc, jsdscript, startLine, maxLines, count, lines, pcs);
}

JSD_PUBLIC_API(void)
JSD_ScriptCreated(JSDContext* jsdc,
                  JSContext   *cx,
                  const char  *filename,    /* URL this script loads from */
                  unsigned       lineno,       /* line where this script starts */
                  JSScript    *script,
                  JSFunction  *fun)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_ScriptCreated(jsdc, cx, filename, lineno, script, fun);
}

JSD_PUBLIC_API(void)
JSD_ScriptDestroyed(JSDContext* jsdc,
                    JSFreeOp    *fop,
                    JSScript    *script)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_ScriptDestroyed(jsdc, fop, script);
}

/***************************************************************************/
/* Source Text functions */

JSD_PUBLIC_API(void)
JSD_LockSourceTextSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_LOCK_SOURCE_TEXT(jsdc);
}

JSD_PUBLIC_API(void)
JSD_UnlockSourceTextSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_UNLOCK_SOURCE_TEXT(jsdc);
}

JSD_PUBLIC_API(JSDSourceText*)
JSD_IterateSources(JSDContext* jsdc, JSDSourceText **iterp)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IterateSources(jsdc, iterp);
}

JSD_PUBLIC_API(JSDSourceText*)
JSD_FindSourceForURL(JSDContext* jsdc, const char* url)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(url);
    return jsd_FindSourceForURL(jsdc, url);
}

JSD_PUBLIC_API(const char*)
JSD_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceURL(jsdc,jsdsrc);
}

JSD_PUBLIC_API(JSBool)
JSD_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    JS_ASSERT(ppBuf);
    JS_ASSERT(pLen);
    return jsd_GetSourceText(jsdc, jsdsrc, ppBuf, pLen);
}

JSD_PUBLIC_API(void)
JSD_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    jsd_ClearSourceText(jsdc, jsdsrc);
}


JSD_PUBLIC_API(JSDSourceStatus)
JSD_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceStatus(jsdc, jsdsrc);
}

JSD_PUBLIC_API(JSBool)
JSD_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_IsSourceDirty(jsdc, jsdsrc);
}

JSD_PUBLIC_API(void)
JSD_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, JSBool dirty)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    jsd_SetSourceDirty(jsdc, jsdsrc, dirty);
}

JSD_PUBLIC_API(unsigned)
JSD_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_GetSourceAlterCount(jsdc, jsdsrc);
}

JSD_PUBLIC_API(unsigned)
JSD_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_IncrementSourceAlterCount(jsdc, jsdsrc);
}

JSD_PUBLIC_API(void)
JSD_DestroyAllSources( JSDContext* jsdc )
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    jsd_DestroyAllSources(jsdc);
}

JSD_PUBLIC_API(JSDSourceText*)
JSD_NewSourceText(JSDContext* jsdc, const char* url)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(url);
    return jsd_NewSourceText(jsdc, url);
}

JSD_PUBLIC_API(JSDSourceText*)
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

extern JSD_PUBLIC_API(JSDSourceText*)
JSD_AppendUCSourceText(JSDContext*     jsdc,
                       JSDSourceText*  jsdsrc,
                       const jschar*   text,       /* *not* zero terminated */
                       size_t          length,
                       JSDSourceStatus status)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsd_AppendUCSourceText(jsdc, jsdsrc, text, length, status);
}

JSD_PUBLIC_API(JSBool)
JSD_AddFullSourceText(JSDContext* jsdc,
                      const char* text,       /* *not* zero terminated */
                      size_t      length,
                      const char* url)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(url);
    return jsd_AddFullSourceText(jsdc, text, length, url);
}

/***************************************************************************/
/* Execution/Interrupt Hook functions */

JSD_PUBLIC_API(JSBool)
JSD_SetExecutionHook(JSDContext*           jsdc,
                     JSDScript*            jsdscript,
                     uintptr_t             pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_SetExecutionHook(jsdc, jsdscript, pc, hook, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearExecutionHook(JSDContext*           jsdc,
                       JSDScript*            jsdscript,
                       uintptr_t             pc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_ClearExecutionHook(jsdc, jsdscript, pc);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_ClearAllExecutionHooksForScript(jsdc, jsdscript);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearAllExecutionHooks(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearAllExecutionHooks(jsdc);
}

JSD_PUBLIC_API(JSBool)
JSD_SetInterruptHook(JSDContext*           jsdc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetInterruptHook(jsdc, hook, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_EnableSingleStepInterrupts(JSDContext* jsdc, JSDScript* jsdscript, JSBool enable)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsd_EnableSingleStepInterrupts(jsdc, jsdscript, enable);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearInterruptHook(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearInterruptHook(jsdc);
}

JSD_PUBLIC_API(JSBool)
JSD_SetDebugBreakHook(JSDContext*           jsdc,
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetDebugBreakHook(jsdc, hook, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearDebugBreakHook(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearDebugBreakHook(jsdc);
}

JSD_PUBLIC_API(JSBool)
JSD_SetDebuggerHook(JSDContext*           jsdc,
                    JSD_ExecutionHookProc hook,
                    void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetDebuggerHook(jsdc, hook, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearDebuggerHook(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearDebuggerHook(jsdc);
}

JSD_PUBLIC_API(JSBool)
JSD_SetThrowHook(JSDContext*           jsdc,
                 JSD_ExecutionHookProc hook,
                 void*                 callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetThrowHook(jsdc, hook, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearThrowHook(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearThrowHook(jsdc);
}

JSD_PUBLIC_API(JSBool)
JSD_SetTopLevelHook(JSDContext*      jsdc,
                    JSD_CallHookProc hook,
                    void*            callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetTopLevelHook(jsdc, hook, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearTopLevelHook(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearTopLevelHook(jsdc);
}

JSD_PUBLIC_API(JSBool)
JSD_SetFunctionHook(JSDContext*      jsdc,
                    JSD_CallHookProc hook,
                    void*            callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetFunctionHook(jsdc, hook, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_ClearFunctionHook(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ClearFunctionHook(jsdc);
}

/***************************************************************************/
/* Stack Frame functions */

JSD_PUBLIC_API(unsigned)
JSD_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetCountOfStackFrames(jsdc, jsdthreadstate);
}

JSD_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetStackFrame(jsdc, jsdthreadstate);
}

JSD_PUBLIC_API(JSContext*)
JSD_GetJSContext(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetJSContext(jsdc, jsdthreadstate);
}

JSD_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetCallingStackFrame(JSDContext* jsdc,
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetCallingStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(JSDScript*)
JSD_GetScriptForStackFrame(JSDContext* jsdc,
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(uintptr_t)
JSD_GetPCForStackFrame(JSDContext* jsdc,
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetPCForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetCallObjectForStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetCallObjectForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetScopeChainForStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScopeChainForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetThisForStackFrame(JSDContext* jsdc,
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetThisForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(JSString *)
JSD_GetIdForStackFrame(JSDContext* jsdc,
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetIdForStackFrame(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(JSBool)
JSD_IsStackFrameDebugger(JSDContext* jsdc,
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IsStackFrameDebugger(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(JSBool)
JSD_IsStackFrameConstructing(JSDContext* jsdc,
                             JSDThreadState* jsdthreadstate,
                             JSDStackFrameInfo* jsdframe)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IsStackFrameConstructing(jsdc, jsdthreadstate, jsdframe);
}

JSD_PUBLIC_API(JSBool)
JSD_EvaluateUCScriptInStackFrame(JSDContext* jsdc,
                                 JSDThreadState* jsdthreadstate,
                                 JSDStackFrameInfo* jsdframe,
                                 const jschar *bytes, unsigned length,
                                 const char *filename, unsigned lineno, jsval *rval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(bytes);
    JS_ASSERT(length);
    JS_ASSERT(filename);
    JS_ASSERT(rval);

    return jsd_EvaluateUCScriptInStackFrame(jsdc, jsdthreadstate,jsdframe,
                                            bytes, length, filename, lineno,
                                             JS_TRUE, rval);
}

JSD_PUBLIC_API(JSBool)
JSD_AttemptUCScriptInStackFrame(JSDContext* jsdc,
                                JSDThreadState* jsdthreadstate,
                                JSDStackFrameInfo* jsdframe,
                                const jschar *bytes, unsigned length,
                                const char *filename, unsigned lineno,
                                jsval *rval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(bytes);
    JS_ASSERT(length);
    JS_ASSERT(filename);
    JS_ASSERT(rval);

    return jsd_EvaluateUCScriptInStackFrame(jsdc, jsdthreadstate,jsdframe,
                                            bytes, length, filename, lineno,
                                            JS_FALSE, rval);
}

JSD_PUBLIC_API(JSBool)
JSD_EvaluateScriptInStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, unsigned length,
                               const char *filename, unsigned lineno, jsval *rval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(bytes);
    JS_ASSERT(length);
    JS_ASSERT(filename);
    JS_ASSERT(rval);

    return jsd_EvaluateScriptInStackFrame(jsdc, jsdthreadstate,jsdframe,
                                          bytes, length,
                                          filename, lineno, JS_TRUE, rval);
}

JSD_PUBLIC_API(JSBool)
JSD_AttemptScriptInStackFrame(JSDContext* jsdc,
                              JSDThreadState* jsdthreadstate,
                              JSDStackFrameInfo* jsdframe,
                              const char *bytes, unsigned length,
                              const char *filename, unsigned lineno, jsval *rval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(bytes);
    JS_ASSERT(length);
    JS_ASSERT(filename);
    JS_ASSERT(rval);

    return jsd_EvaluateScriptInStackFrame(jsdc, jsdthreadstate,jsdframe,
                                          bytes, length,
                                          filename, lineno, JS_FALSE, rval);
}

JSD_PUBLIC_API(JSString*)
JSD_ValToStringInStackFrame(JSDContext* jsdc,
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_ValToStringInStackFrame(jsdc, jsdthreadstate, jsdframe, val);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetException(jsdc, jsdthreadstate);
}

extern JSD_PUBLIC_API(JSBool)
JSD_SetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate,
                 JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetException(jsdc, jsdthreadstate, jsdval);
}

/***************************************************************************/

JSD_PUBLIC_API(JSBool)
JSD_SetErrorReporter(JSDContext*       jsdc,
                     JSD_ErrorReporter reporter,
                     void*             callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_SetErrorReporter(jsdc, reporter, callerdata);
}

JSD_PUBLIC_API(JSBool)
JSD_GetErrorReporter(JSDContext*        jsdc,
                     JSD_ErrorReporter* reporter,
                     void**             callerdata)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetErrorReporter(jsdc, reporter, callerdata);
}

/***************************************************************************/

JSD_PUBLIC_API(JSBool)
JSD_IsLockingAndThreadIdSupported()
{
#ifdef JSD_THREADSAFE
    return JS_TRUE;
#else
    return JS_FALSE;
#endif
}

JSD_PUBLIC_API(void*)
JSD_CreateLock()
{
#ifdef JSD_THREADSAFE
    return jsd_CreateLock();
#else
    return (void*)1;
#endif
}

JSD_PUBLIC_API(void)
JSD_Lock(void* lock)
{
#ifdef JSD_THREADSAFE
    jsd_Lock(lock);
#endif
}

JSD_PUBLIC_API(void)
JSD_Unlock(void* lock)
{
#ifdef JSD_THREADSAFE
    jsd_Unlock(lock);
#endif
}

JSD_PUBLIC_API(JSBool)
JSD_IsLocked(void* lock)
{
#if defined(JSD_THREADSAFE) && defined(DEBUG)
    return jsd_IsLocked(lock);
#else
    return JS_TRUE;
#endif
}

JSD_PUBLIC_API(JSBool)
JSD_IsUnlocked(void* lock)
{
#if defined(JSD_THREADSAFE) && defined(DEBUG)
    return ! jsd_IsLocked(lock);
#else
    return JS_TRUE;
#endif
}

JSD_PUBLIC_API(void*)
JSD_CurrentThread()
{
    return JSD_CURRENT_THREAD();
}

/***************************************************************************/
/* Value and Property Functions */

JSD_PUBLIC_API(JSDValue*)
JSD_NewValue(JSDContext* jsdc, jsval val)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_NewValue(jsdc, val);
}

JSD_PUBLIC_API(void)
JSD_DropValue(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    jsd_DropValue(jsdc, jsdval);
}

JSD_PUBLIC_API(jsval)
JSD_GetValueWrappedJSVal(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueWrappedJSVal(jsdc, jsdval);
}

JSD_PUBLIC_API(void)
JSD_RefreshValue(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    jsd_RefreshValue(jsdc, jsdval);
}

/**************************************************/

JSD_PUBLIC_API(JSBool)
JSD_IsValueObject(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueObject(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueNumber(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueNumber(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueInt(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueInt(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueDouble(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueDouble(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueString(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueString(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueBoolean(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueBoolean(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueNull(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueNull(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueVoid(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueVoid(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValuePrimitive(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValuePrimitive(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueFunction(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueFunction(jsdc, jsdval);
}

JSD_PUBLIC_API(JSBool)
JSD_IsValueNative(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_IsValueNative(jsdc, jsdval);
}

/**************************************************/

JSD_PUBLIC_API(JSBool)
JSD_GetValueBoolean(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueBoolean(jsdc, jsdval);
}

JSD_PUBLIC_API(int32_t)
JSD_GetValueInt(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueInt(jsdc, jsdval);
}

JSD_PUBLIC_API(double)
JSD_GetValueDouble(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueDouble(jsdc, jsdval);
}

JSD_PUBLIC_API(JSString*)
JSD_GetValueString(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueString(jsdc, jsdval);
}

JSD_PUBLIC_API(JSString *)
JSD_GetValueFunctionId(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueFunctionId(jsdc, jsdval);
}

JSD_PUBLIC_API(JSFunction*)
JSD_GetValueFunction(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueFunction(jsdc, jsdval);
}

/**************************************************/

JSD_PUBLIC_API(unsigned)
JSD_GetCountOfProperties(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetCountOfProperties(jsdc, jsdval);
}

JSD_PUBLIC_API(JSDProperty*)
JSD_IterateProperties(JSDContext* jsdc, JSDValue* jsdval, JSDProperty **iterp)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    JS_ASSERT(iterp);
    return jsd_IterateProperties(jsdc, jsdval, iterp);
}

JSD_PUBLIC_API(JSDProperty*)
JSD_GetValueProperty(JSDContext* jsdc, JSDValue* jsdval, JSString* name)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    JS_ASSERT(name);
    return jsd_GetValueProperty(jsdc, jsdval, name);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetValuePrototype(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValuePrototype(jsdc, jsdval);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetValueParent(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueParent(jsdc, jsdval);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetValueConstructor(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueConstructor(jsdc, jsdval);
}

JSD_PUBLIC_API(const char*)
JSD_GetValueClassName(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetValueClassName(jsdc, jsdval);
}

JSD_PUBLIC_API(JSDScript*)
JSD_GetScriptForValue(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_GetScriptForValue(jsdc, jsdval);
}
/**************************************************/

JSD_PUBLIC_API(void)
JSD_DropProperty(JSDContext* jsdc, JSDProperty* jsdprop)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_PROPERTY(jsdprop);
    jsd_DropProperty(jsdc, jsdprop);
}


JSD_PUBLIC_API(JSDValue*)
JSD_GetPropertyName(JSDContext* jsdc, JSDProperty* jsdprop)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_PROPERTY(jsdprop);
    return jsd_GetPropertyName(jsdc, jsdprop);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetPropertyValue(JSDContext* jsdc, JSDProperty* jsdprop)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_PROPERTY(jsdprop);
    return jsd_GetPropertyValue(jsdc, jsdprop);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetPropertyAlias(JSDContext* jsdc, JSDProperty* jsdprop)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_PROPERTY(jsdprop);
    return jsd_GetPropertyAlias(jsdc, jsdprop);
}

JSD_PUBLIC_API(unsigned)
JSD_GetPropertyFlags(JSDContext* jsdc, JSDProperty* jsdprop)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_PROPERTY(jsdprop);
    return jsd_GetPropertyFlags(jsdc, jsdprop);
}

JSD_PUBLIC_API(unsigned)
JSD_GetPropertyVarArgSlot(JSDContext* jsdc, JSDProperty* jsdprop)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_PROPERTY(jsdprop);
    return jsd_GetPropertyVarArgSlot(jsdc, jsdprop);
}

/**************************************************/
/* Object Functions */

JSD_PUBLIC_API(void)
JSD_LockObjectSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_LOCK_OBJECTS(jsdc);
}

JSD_PUBLIC_API(void)
JSD_UnlockObjectSubsystem(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_UNLOCK_OBJECTS(jsdc);
}

JSD_PUBLIC_API(JSDObject*)
JSD_IterateObjects(JSDContext* jsdc, JSDObject** iterp)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    return jsd_IterateObjects(jsdc, iterp);
}

JSD_PUBLIC_API(JSObject*)
JSD_GetWrappedObject(JSDContext* jsdc, JSDObject* jsdobj)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_OBJECT(jsdobj);
    return jsd_GetWrappedObject(jsdc, jsdobj);

}

JSD_PUBLIC_API(const char*)
JSD_GetObjectNewURL(JSDContext* jsdc, JSDObject* jsdobj)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_OBJECT(jsdobj);
    return jsd_GetObjectNewURL(jsdc, jsdobj);
}

JSD_PUBLIC_API(unsigned)
JSD_GetObjectNewLineNumber(JSDContext* jsdc, JSDObject* jsdobj)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_OBJECT(jsdobj);
    return jsd_GetObjectNewLineNumber(jsdc, jsdobj);
}

JSD_PUBLIC_API(const char*)
JSD_GetObjectConstructorURL(JSDContext* jsdc, JSDObject* jsdobj)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_OBJECT(jsdobj);
    return jsd_GetObjectConstructorURL(jsdc, jsdobj);
}

JSD_PUBLIC_API(unsigned)
JSD_GetObjectConstructorLineNumber(JSDContext* jsdc, JSDObject* jsdobj)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_OBJECT(jsdobj);
    return jsd_GetObjectConstructorLineNumber(jsdc, jsdobj);
}

JSD_PUBLIC_API(const char*)
JSD_GetObjectConstructorName(JSDContext* jsdc, JSDObject* jsdobj)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_OBJECT(jsdobj);
    return jsd_GetObjectConstructorName(jsdc, jsdobj);
}

JSD_PUBLIC_API(JSDObject*)
JSD_GetJSDObjectForJSObject(JSDContext* jsdc, JSObject* jsobj)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(jsobj);
    return jsd_GetJSDObjectForJSObject(jsdc, jsobj);
}

JSD_PUBLIC_API(JSDObject*)
JSD_GetObjectForValue(JSDContext* jsdc, JSDValue* jsdval)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_VALUE(jsdval);
    return jsd_GetObjectForValue(jsdc, jsdval);
}

JSD_PUBLIC_API(JSDValue*)
JSD_GetValueForObject(JSDContext* jsdc, JSDObject* jsdobj)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_OBJECT(jsdobj);
    return jsd_GetValueForObject(jsdc, jsdobj);
}

/***************************************************************************/
/* Livewire specific API */
#ifdef LIVEWIRE

JSD_PUBLIC_API(LWDBGScript*)
JSDLW_GetLWScript(JSDContext* jsdc, JSDScript* jsdscript)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsdlw_GetLWScript(jsdc, jsdscript);
}

JSD_PUBLIC_API(JSDSourceText*)
JSDLW_PreLoadSource( JSDContext* jsdc, LWDBGApp* app,
                     const char* filename, JSBool clear )
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JS_ASSERT(app);
    JS_ASSERT(filename);
    return jsdlw_PreLoadSource(jsdc, app, filename, clear);
}

JSD_PUBLIC_API(JSDSourceText*)
JSDLW_ForceLoadSource( JSDContext* jsdc, JSDSourceText* jsdsrc )
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SOURCE_TEXT(jsdsrc);
    return jsdlw_ForceLoadSource(jsdc, jsdsrc);
}

JSD_PUBLIC_API(JSBool)
JSDLW_RawToProcessedLineNumber(JSDContext* jsdc, JSDScript* jsdscript,
                               unsigned lineIn, unsigned* lineOut)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsdlw_RawToProcessedLineNumber(jsdc, jsdscript, lineIn, lineOut);
}

JSD_PUBLIC_API(JSBool)
JSDLW_ProcessedToRawLineNumber(JSDContext* jsdc, JSDScript* jsdscript,
                               unsigned lineIn, unsigned* lineOut)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    JSD_ASSERT_VALID_SCRIPT(jsdscript);
    return jsdlw_ProcessedToRawLineNumber(jsdc, jsdscript, lineIn, lineOut);
}

#endif
/***************************************************************************/
