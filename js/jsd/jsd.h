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
 * Header for JavaScript Debugging support - Internal ONLY declarations
 */

#ifndef jsd_h___
#define jsd_h___

/*
* NOTE: This is a *private* header file and should only be included by
* the sources in js/jsd. Defining EXPORT_JSD_API in an outside module
* using jsd would be bad.
*/
#define EXPORT_JSD_API 1 /* if used, must be set before include of jsdebug.h */

/*
* These can be controled by the makefile, but this allows a place to set
* the values always used in the mozilla client, but perhaps done differently
* in other embeddings.
*/
#ifdef MOZILLA_CLIENT
#define JSD_THREADSAFE 1
/* define JSD_HAS_DANGEROUS_THREAD 1 */
#define JSD_USE_NSPR_LOCKS 1
#endif /* MOZILLA_CLIENT */


/* Get jstypes.h included first. After that we can use PR macros for doing
*  this extern "C" stuff!
*/
#ifdef __cplusplus
extern "C"
{
#endif
#include "jstypes.h"
#ifdef __cplusplus
}
#endif

JS_BEGIN_EXTERN_C
#include "jsprf.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsclist.h"
#include "jsdebug.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "jsd_lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef LIVEWIRE
#include <base/pblock.h>
#include <base/session.h>
#include <frame/log.h>
#include <frame/req.h>
#endif /* LIVEWIRE */
JS_END_EXTERN_C

JS_BEGIN_EXTERN_C

#define JSD_MAJOR_VERSION 1
#define JSD_MINOR_VERSION 1

/***************************************************************************/
/* handy macros */
#undef  CHECK_BIT_FLAG
#define CHECK_BIT_FLAG(f,b) ((f)&(b))
#undef  SET_BIT_FLAG
#define SET_BIT_FLAG(f,b)   ((f)|=(b))
#undef  CLEAR_BIT_FLAG
#define CLEAR_BIT_FLAG(f,b) ((f)&=(~(b)))

#define JSD_IS_DEBUG_ENABLED(jsdc,jsdscript)                                   \
        (!(((jsdc->flags & JSD_DEBUG_WHEN_SET) ? 1 : 0)  ^                     \
           ((jsdscript->flags & JSD_SCRIPT_DEBUG_BIT) ?  1 : 0)))
#define JSD_IS_PROFILE_ENABLED(jsdc,jsdscript)                                 \
        ((jsdc->flags & JSD_COLLECT_PROFILE_DATA) &&                           \
         (!(((jsdc->flags & JSD_PROFILE_WHEN_SET) ? 1 : 0) ^                   \
            ((jsdscript->flags & JSD_SCRIPT_PROFILE_BIT) ? 1 : 0))))


/***************************************************************************/
/* These are not exposed in jsdebug.h - typedef here for consistency */

typedef struct JSDExecHook          JSDExecHook;
typedef struct JSDAtom              JSDAtom;
typedef struct JSDProfileData       JSDProfileData;
/***************************************************************************/
/* Our structures */

/*
* XXX What I'm calling a JSDContext is really more of a JSDTaskState. 
*/

struct JSDContext
{
    JSCList                 links;      /* we are part of a JSCList */
    JSBool                  inited;
    void*                   data;
    uint32                  flags;
    JSD_ScriptHookProc      scriptHook;
    void*                   scriptHookData;
    JSD_ExecutionHookProc   interruptHook;
    void*                   interruptHookData;
    JSRuntime*              jsrt;
    JSD_ErrorReporter       errorReporter;
    void*                   errorReporterData;
    JSCList                 threadsStates;
    JSD_ExecutionHookProc   debugBreakHook;
    void*                   debugBreakHookData;
    JSD_ExecutionHookProc   debuggerHook;
    void*                   debuggerHookData;
    JSD_ExecutionHookProc   throwHook;
    void*                   throwHookData;
    JSD_CallHookProc        functionHook;
    void*                   functionHookData;
    JSD_CallHookProc        toplevelHook;
    void*                   toplevelHookData;
    JSContext*              dumbContext;
    JSObject*               glob;
    JSD_UserCallbacks       userCallbacks;
    void*                   user;
    JSCList                 scripts;
    JSHashTable*            scriptsTable;
    JSCList                 sources;
    JSCList                 removedSources;
    uintN                   sourceAlterCount;
    JSHashTable*            atoms;
    JSCList                 objectsList;
    JSHashTable*            objectsTable;
    JSDProfileData*         callingFunctionPData;
    int64                   lastReturnTime;
#ifdef JSD_THREADSAFE
    void*                   scriptsLock;
    void*                   sourceTextLock;
    void*                   objectsLock;
    void*                   atomsLock;
    void*                   threadStatesLock;
#endif /* JSD_THREADSAFE */
#ifdef JSD_HAS_DANGEROUS_THREAD
    void*                   dangerousThread;
#endif /* JSD_HAS_DANGEROUS_THREAD */

};

struct JSDScript
{
    JSCList     links;      /* we are part of a JSCList */
    JSDContext* jsdc;       /* JSDContext for this jsdscript */
    JSScript*   script;     /* script we are wrapping */
    JSFunction* function;   /* back pointer to owning function (can be NULL) */
    uintN       lineBase;   /* we cache this */
    uintN       lineExtent; /* we cache this */
    JSCList     hooks;      /* JSCList of JSDExecHooks for this script */
    char*       url;
    uint32      flags;
    void*       data;

    JSDProfileData  *profileData;

#ifdef LIVEWIRE
    LWDBGApp*    app;
    LWDBGScript* lwscript;
#endif
};

struct JSDProfileData
{
    JSDProfileData* caller;
    int64    lastCallStart;
    int64    runningTime;
    uintN    callCount;
    uintN    recurseDepth;
    uintN    maxRecurseDepth;
    jsdouble minExecutionTime;
    jsdouble maxExecutionTime;
    jsdouble totalExecutionTime;
    jsdouble minOwnExecutionTime;
    jsdouble maxOwnExecutionTime;
    jsdouble totalOwnExecutionTime;
};

struct JSDSourceText
{
    JSCList          links;      /* we are part of a JSCList */
    char*            url;
    char*            text;
    uintN            textLength;
    uintN            textSpace;
    JSBool           dirty;
    JSDSourceStatus  status;
    uintN            alterCount;
    JSBool           doingEval;
};

struct JSDExecHook
{
    JSCList               links;        /* we are part of a JSCList */
    JSDScript*            jsdscript;
    jsuword               pc;
    JSD_ExecutionHookProc hook;
    void*                 callerdata;
};

#define TS_HAS_DISABLED_FRAME 0x01

struct JSDThreadState
{
    JSCList             links;        /* we are part of a JSCList */
    JSContext*          context;
    void*               thread;
    JSCList             stack;
    uintN               stackDepth;
    uintN               flags;
};

struct JSDStackFrameInfo
{
    JSCList             links;        /* we are part of a JSCList */
    JSDThreadState*     jsdthreadstate;
    JSDScript*          jsdscript;
    jsuword             pc;
    JSStackFrame*       fp;
};

#define GOT_PROTO   ((short) (1 << 0))
#define GOT_PROPS   ((short) (1 << 1))
#define GOT_PARENT  ((short) (1 << 2))
#define GOT_CTOR    ((short) (1 << 3))

struct JSDValue
{
    jsval       val;
    intN        nref;
    JSCList     props;
    JSString*   string;
    JSString*   funName;
    const char* className;
    JSDValue*   proto;
    JSDValue*   parent;
    JSDValue*   ctor;
    uintN       flags;
};

struct JSDProperty
{
    JSCList     links;      /* we are part of a JSCList */
    intN        nref;
    JSDValue*   val;
    JSDValue*   name;
    JSDValue*   alias;
    uintN       slot;
    uintN       flags;
};

struct JSDAtom
{
    char* str;      /* must be first element in struct for compare */
    intN  refcount;
};

struct JSDObject
{
    JSCList     links;      /* we are part of a JSCList */
    JSObject*   obj;
    JSDAtom*    newURL;
    uintN       newLineno;
    JSDAtom*    ctorURL;
    uintN       ctorLineno;
    JSDAtom*    ctorName;
};

/***************************************************************************/
/* Code validation support */

#ifdef DEBUG
extern void JSD_ASSERT_VALID_CONTEXT(JSDContext* jsdc);
extern void JSD_ASSERT_VALID_SCRIPT(JSDScript* jsdscript);
extern void JSD_ASSERT_VALID_SOURCE_TEXT(JSDSourceText* jsdsrc);
extern void JSD_ASSERT_VALID_THREAD_STATE(JSDThreadState* jsdthreadstate);
extern void JSD_ASSERT_VALID_STACK_FRAME(JSDStackFrameInfo* jsdframe);
extern void JSD_ASSERT_VALID_EXEC_HOOK(JSDExecHook* jsdhook);
extern void JSD_ASSERT_VALID_VALUE(JSDValue* jsdval);
extern void JSD_ASSERT_VALID_PROPERTY(JSDProperty* jsdprop);
extern void JSD_ASSERT_VALID_OBJECT(JSDObject* jsdobj);
#else
#define JSD_ASSERT_VALID_CONTEXT(x)     ((void)0)
#define JSD_ASSERT_VALID_SCRIPT(x)      ((void)0)
#define JSD_ASSERT_VALID_SOURCE_TEXT(x) ((void)0)
#define JSD_ASSERT_VALID_THREAD_STATE(x)((void)0)
#define JSD_ASSERT_VALID_STACK_FRAME(x) ((void)0)
#define JSD_ASSERT_VALID_EXEC_HOOK(x)   ((void)0)
#define JSD_ASSERT_VALID_VALUE(x)       ((void)0)
#define JSD_ASSERT_VALID_PROPERTY(x)    ((void)0)
#define JSD_ASSERT_VALID_OBJECT(x)      ((void)0)
#endif

/***************************************************************************/
/* higher level functions */

extern JSDContext*
jsd_DebuggerOnForUser(JSRuntime*         jsrt,
                      JSD_UserCallbacks* callbacks,
                      void*              user,
                      JSObject*          scopeobj);

extern JSDContext*
jsd_DebuggerOn(void);

extern void
jsd_DebuggerOff(JSDContext* jsdc);

extern void
jsd_DebuggerPause(JSDContext* jsdc, JSBool forceAllHooksOff);

extern void
jsd_DebuggerUnpause(JSDContext* jsdc);

extern void
jsd_SetUserCallbacks(JSRuntime* jsrt, JSD_UserCallbacks* callbacks, void* user);

extern JSDContext*
jsd_JSDContextForJSContext(JSContext* context);

extern void*
jsd_SetContextPrivate(JSDContext* jsdc, void *data);

extern void*
jsd_GetContextPrivate(JSDContext* jsdc);

extern void
jsd_ClearAllProfileData(JSDContext* jsdc);

extern JSBool
jsd_SetErrorReporter(JSDContext*       jsdc,
                     JSD_ErrorReporter reporter,
                     void*             callerdata);

extern JSBool
jsd_GetErrorReporter(JSDContext*        jsdc,
                     JSD_ErrorReporter* reporter,
                     void**             callerdata);

static JSBool
jsd_DebugErrorHook(JSContext *cx, const char *message,
                   JSErrorReport *report, void *closure);

/***************************************************************************/
/* Script functions */

extern JSBool
jsd_InitScriptManager(JSDContext *jsdc);

extern void
jsd_DestroyScriptManager(JSDContext* jsdc);

extern JSDScript*
jsd_FindJSDScript(JSDContext*  jsdc,
                  JSScript     *script);

extern JSDScript*
jsd_FindOrCreateJSDScript(JSDContext    *jsdc,
                          JSContext     *cx,
                          JSScript      *script,
                          JSStackFrame  *fp);

extern JSDProfileData*
jsd_GetScriptProfileData(JSDContext* jsdc, JSDScript *script);

extern uint32
jsd_GetScriptFlags(JSDContext *jsdc, JSDScript *script);

extern void
jsd_SetScriptFlags(JSDContext *jsdc, JSDScript *script, uint32 flags);

extern uintN
jsd_GetScriptCallCount(JSDContext* jsdc, JSDScript *script);

extern  uintN
jsd_GetScriptMaxRecurseDepth(JSDContext* jsdc, JSDScript *script);

extern jsdouble
jsd_GetScriptMinExecutionTime(JSDContext* jsdc, JSDScript *script);

extern jsdouble
jsd_GetScriptMaxExecutionTime(JSDContext* jsdc, JSDScript *script);

extern jsdouble
jsd_GetScriptTotalExecutionTime(JSDContext* jsdc, JSDScript *script);

extern jsdouble
jsd_GetScriptMinOwnExecutionTime(JSDContext* jsdc, JSDScript *script);

extern jsdouble
jsd_GetScriptMaxOwnExecutionTime(JSDContext* jsdc, JSDScript *script);

extern jsdouble
jsd_GetScriptTotalOwnExecutionTime(JSDContext* jsdc, JSDScript *script);

extern void
jsd_ClearScriptProfileData(JSDContext* jsdc, JSDScript *script);

extern JSScript *
jsd_GetJSScript (JSDContext *jsdc, JSDScript *script);

extern JSFunction *
jsd_GetJSFunction (JSDContext *jsdc, JSDScript *script);

extern JSDScript*
jsd_IterateScripts(JSDContext* jsdc, JSDScript **iterp);

extern void *
jsd_SetScriptPrivate (JSDScript *jsdscript, void *data);

extern void *
jsd_GetScriptPrivate (JSDScript *jsdscript);

extern JSBool
jsd_IsActiveScript(JSDContext* jsdc, JSDScript *jsdscript);

extern const char*
jsd_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript);

extern JSString*
jsd_GetScriptFunctionId(JSDContext* jsdc, JSDScript *jsdscript);

extern uintN
jsd_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript);

extern uintN
jsd_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript);

extern JSBool
jsd_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata);

extern JSBool
jsd_GetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc* hook, void** callerdata);

extern jsuword
jsd_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, uintN line);

extern uintN
jsd_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, jsuword pc);

extern void
jsd_NewScriptHookProc(
                JSContext   *cx,
                const char  *filename,      /* URL this script loads from */
                uintN       lineno,         /* line where this script starts */
                JSScript    *script,
                JSFunction  *fun,
                void*       callerdata);

extern void
jsd_DestroyScriptHookProc(
                JSContext   *cx,
                JSScript    *script,
                void*       callerdata);

/* Script execution hook functions */

extern JSBool
jsd_SetExecutionHook(JSDContext*           jsdc,
                     JSDScript*            jsdscript,
                     jsuword               pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

extern JSBool
jsd_ClearExecutionHook(JSDContext*           jsdc,
                       JSDScript*            jsdscript,
                       jsuword               pc);

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
                  const char** ppBuf, intN* pLen);

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

extern JSDSourceText*
jsd_AppendUCSourceText(JSDContext* jsdc,
                       JSDSourceText* jsdsrc,
                       const jschar* text,       /* *not* zero terminated */
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
jsd_EnableSingleStepInterrupts(JSDContext* jsdc,
                               JSDScript*  jsdscript,
                               JSBool      enable);

extern JSBool
jsd_SetDebugBreakHook(JSDContext*           jsdc,
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata);

extern JSBool
jsd_ClearDebugBreakHook(JSDContext* jsdc);

extern JSBool
jsd_SetDebuggerHook(JSDContext*           jsdc,
                    JSD_ExecutionHookProc hook,
                    void*                 callerdata);

extern JSBool
jsd_ClearDebuggerHook(JSDContext* jsdc);

extern JSTrapStatus
jsd_CallExecutionHook(JSDContext*           jsdc,
                      JSContext*            cx,
                      uintN                 type,
                      JSD_ExecutionHookProc hook,
                      void*                 hookData,
                      jsval*                rval);

extern JSBool
jsd_CallCallHook (JSDContext*      jsdc,
                  JSContext*       cx,
                  uintN            type,
                  JSD_CallHookProc hook,
                  void*            hookData);

extern JSBool
jsd_SetThrowHook(JSDContext*           jsdc,
                 JSD_ExecutionHookProc hook,
                 void*                 callerdata);
extern JSBool
jsd_ClearThrowHook(JSDContext* jsdc);

extern JSTrapStatus
jsd_DebuggerHandler(JSContext *cx, JSScript *script, jsbytecode *pc,
                    jsval *rval, void *closure);

extern JSTrapStatus
jsd_ThrowHandler(JSContext *cx, JSScript *script, jsbytecode *pc,
                 jsval *rval, void *closure);

extern JSBool
jsd_SetFunctionHook(JSDContext*      jsdc,
                    JSD_CallHookProc hook,
                    void*            callerdata);

extern JSBool
jsd_ClearFunctionHook(JSDContext* jsdc);

extern JSBool
jsd_SetTopLevelHook(JSDContext*      jsdc,
                    JSD_CallHookProc hook,
                    void*            callerdata);

extern JSBool
jsd_ClearTopLevelHook(JSDContext* jsdc);

/***************************************************************************/
/* Stack Frame functions */

extern uintN
jsd_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JSDStackFrameInfo*
jsd_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JSContext*
jsd_GetJSContext(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JSDStackFrameInfo*
jsd_GetCallingStackFrame(JSDContext* jsdc,
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

extern JSDScript*
jsd_GetScriptForStackFrame(JSDContext* jsdc,
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe);

extern jsuword
jsd_GetPCForStackFrame(JSDContext* jsdc,
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe);

extern JSDValue*
jsd_GetCallObjectForStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe);

extern JSDValue*
jsd_GetScopeChainForStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe);

extern JSBool
jsd_IsStackFrameDebugger(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

extern JSBool
jsd_IsStackFrameConstructing(JSDContext* jsdc, 
                             JSDThreadState* jsdthreadstate,
                             JSDStackFrameInfo* jsdframe);

extern JSDValue*
jsd_GetThisForStackFrame(JSDContext* jsdc,
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

extern JSString*
jsd_GetIdForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe);

extern JSDThreadState*
jsd_NewThreadState(JSDContext* jsdc, JSContext *cx);

extern void
jsd_DestroyThreadState(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JSBool
jsd_EvaluateUCScriptInStackFrame(JSDContext* jsdc,
                                 JSDThreadState* jsdthreadstate,
                                 JSDStackFrameInfo* jsdframe,
                                 const jschar *bytes, uintN length,
                                 const char *filename, uintN lineno,
                                 JSBool eatExceptions, jsval *rval);

extern JSBool
jsd_EvaluateScriptInStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, uintN length,
                               const char *filename, uintN lineno,
                               JSBool eatExceptions, jsval *rval);

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

extern JSDValue*
jsd_GetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern JSBool
jsd_SetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate, 
                 JSDValue* jsdval);

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
    JS_BEGIN_MACRO                               \
        if(!_jsd_global_lock)                    \
            _jsd_global_lock = jsd_CreateLock(); \
        JS_ASSERT(_jsd_global_lock);             \
        jsd_Lock(_jsd_global_lock);              \
    JS_END_MACRO

#define JSD_UNLOCK()                             \
    JS_BEGIN_MACRO                               \
        JS_ASSERT(_jsd_global_lock);             \
        jsd_Unlock(_jsd_global_lock);            \
    JS_END_MACRO

/* locks for the subsystems of a given context */
#define JSD_INIT_LOCKS(jsdc)                                    \
    ( (NULL != (jsdc->scriptsLock      = jsd_CreateLock())) &&  \
      (NULL != (jsdc->sourceTextLock   = jsd_CreateLock())) &&  \
      (NULL != (jsdc->atomsLock        = jsd_CreateLock())) &&  \
      (NULL != (jsdc->objectsLock      = jsd_CreateLock())) &&  \
      (NULL != (jsdc->threadStatesLock = jsd_CreateLock())) )

#define JSD_LOCK_SCRIPTS(jsdc)        jsd_Lock(jsdc->scriptsLock)
#define JSD_UNLOCK_SCRIPTS(jsdc)      jsd_Unlock(jsdc->scriptsLock)

#define JSD_LOCK_SOURCE_TEXT(jsdc)    jsd_Lock(jsdc->sourceTextLock)
#define JSD_UNLOCK_SOURCE_TEXT(jsdc)  jsd_Unlock(jsdc->sourceTextLock)

#define JSD_LOCK_ATOMS(jsdc)          jsd_Lock(jsdc->atomsLock)
#define JSD_UNLOCK_ATOMS(jsdc)        jsd_Unlock(jsdc->atomsLock)

#define JSD_LOCK_OBJECTS(jsdc)        jsd_Lock(jsdc->objectsLock)
#define JSD_UNLOCK_OBJECTS(jsdc)      jsd_Unlock(jsdc->objectsLock)

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

#define JSD_LOCK_ATOMS(jsdc)          ((void)0)
#define JSD_UNLOCK_ATOMS(jsdc)        ((void)0)

#define JSD_LOCK_OBJECTS(jsdc)        ((void)0)
#define JSD_UNLOCK_OBJECTS(jsdc)      ((void)0)

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
#define JSD_ATOMS_LOCKED(jsdc)          (jsd_IsLocked(jsdc->atomsLock))
#define JSD_OBJECTS_LOCKED(jsdc)        (jsd_IsLocked(jsdc->objectsLock))
#define JSD_THREADSTATES_LOCKED(jsdc)   (jsd_IsLocked(jsdc->threadStatesLock))
#define JSD_SCRIPTS_UNLOCKED(jsdc)      (!jsd_IsLocked(jsdc->scriptsLock))
#define JSD_SOURCE_TEXT_UNLOCKED(jsdc)  (!jsd_IsLocked(jsdc->sourceTextLock))
#define JSD_ATOMS_UNLOCKED(jsdc)        (!jsd_IsLocked(jsdc->atomsLock))
#define JSD_OBJECTS_UNLOCKED(jsdc)      (!jsd_IsLocked(jsdc->objectsLock))
#define JSD_THREADSTATES_UNLOCKED(jsdc) (!jsd_IsLocked(jsdc->threadStatesLock))
#else
#define JSD_SCRIPTS_LOCKED(jsdc)        1
#define JSD_SOURCE_TEXT_LOCKED(jsdc)    1
#define JSD_ATOMS_LOCKED(jsdc)          1
#define JSD_OBJECTS_LOCKED(jsdc)        1
#define JSD_THREADSTATES_LOCKED(jsdc)   1
#define JSD_SCRIPTS_UNLOCKED(jsdc)      1
#define JSD_SOURCE_TEXT_UNLOCKED(jsdc)  1
#define JSD_ATOMS_UNLOCKED(jsdc)        1
#define JSD_OBJECTS_UNLOCKED(jsdc)      1
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
/* Value and Property Functions */

extern JSDValue*
jsd_NewValue(JSDContext* jsdc, jsval val);

extern void
jsd_DropValue(JSDContext* jsdc, JSDValue* jsdval);

extern jsval
jsd_GetValueWrappedJSVal(JSDContext* jsdc, JSDValue* jsdval);

extern void
jsd_RefreshValue(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

extern JSBool
jsd_IsValueObject(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueNumber(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueInt(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueDouble(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueString(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueBoolean(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueNull(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueVoid(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValuePrimitive(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueFunction(JSDContext* jsdc, JSDValue* jsdval);

extern JSBool
jsd_IsValueNative(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

extern JSBool
jsd_GetValueBoolean(JSDContext* jsdc, JSDValue* jsdval);

extern int32
jsd_GetValueInt(JSDContext* jsdc, JSDValue* jsdval);

extern jsdouble
jsd_GetValueDouble(JSDContext* jsdc, JSDValue* jsdval);

extern JSString*
jsd_GetValueString(JSDContext* jsdc, JSDValue* jsdval);

extern JSString*
jsd_GetValueFunctionId(JSDContext* jsdc, JSDValue* jsdval);

extern JSFunction*
jsd_GetValueFunction(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

extern uintN
jsd_GetCountOfProperties(JSDContext* jsdc, JSDValue* jsdval);

extern JSDProperty*
jsd_IterateProperties(JSDContext* jsdc, JSDValue* jsdval, JSDProperty **iterp);

extern JSDProperty*
jsd_GetValueProperty(JSDContext* jsdc, JSDValue* jsdval, JSString* name);

extern JSDValue*
jsd_GetValuePrototype(JSDContext* jsdc, JSDValue* jsdval);

extern JSDValue*
jsd_GetValueParent(JSDContext* jsdc, JSDValue* jsdval);

extern JSDValue*
jsd_GetValueConstructor(JSDContext* jsdc, JSDValue* jsdval);

extern const char*
jsd_GetValueClassName(JSDContext* jsdc, JSDValue* jsdval);

extern JSDScript*
jsd_GetScriptForValue(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

extern void
jsd_DropProperty(JSDContext* jsdc, JSDProperty* jsdprop);

extern JSDValue*
jsd_GetPropertyName(JSDContext* jsdc, JSDProperty* jsdprop);

extern JSDValue*
jsd_GetPropertyValue(JSDContext* jsdc, JSDProperty* jsdprop);

extern JSDValue*
jsd_GetPropertyAlias(JSDContext* jsdc, JSDProperty* jsdprop);

extern uintN
jsd_GetPropertyFlags(JSDContext* jsdc, JSDProperty* jsdprop);

extern uintN
jsd_GetPropertyVarArgSlot(JSDContext* jsdc, JSDProperty* jsdprop);

/**************************************************/
/* Stepping Functions */

extern void *
jsd_FunctionCallHook(JSContext *cx, JSStackFrame *fp, JSBool before,
                     JSBool *ok, void *closure);

extern void *
jsd_TopLevelCallHook(JSContext *cx, JSStackFrame *fp, JSBool before,
                     JSBool *ok, void *closure);

/**************************************************/
/* Object Functions */

extern JSBool
jsd_InitObjectManager(JSDContext* jsdc);

extern void
jsd_DestroyObjectManager(JSDContext* jsdc);

extern void
jsd_DestroyObjects(JSDContext* jsdc);

extern void
jsd_Constructing(JSDContext* jsdc, JSContext *cx, JSObject *obj,
                 JSStackFrame *fp);

extern JSDObject*
jsd_IterateObjects(JSDContext* jsdc, JSDObject** iterp);

extern JSObject*
jsd_GetWrappedObject(JSDContext* jsdc, JSDObject* jsdobj);

extern const char*
jsd_GetObjectNewURL(JSDContext* jsdc, JSDObject* jsdobj);

extern uintN
jsd_GetObjectNewLineNumber(JSDContext* jsdc, JSDObject* jsdobj);

extern const char*
jsd_GetObjectConstructorURL(JSDContext* jsdc, JSDObject* jsdobj);

extern uintN
jsd_GetObjectConstructorLineNumber(JSDContext* jsdc, JSDObject* jsdobj);

extern const char*
jsd_GetObjectConstructorName(JSDContext* jsdc, JSDObject* jsdobj);

extern JSDObject*
jsd_GetJSDObjectForJSObject(JSDContext* jsdc, JSObject* jsobj);

extern JSDObject*
jsd_GetObjectForValue(JSDContext* jsdc, JSDValue* jsdval);

/*
* returns new refcounted JSDValue
*/
extern JSDValue*
jsd_GetValueForObject(JSDContext* jsdc, JSDObject* jsdobj);

/**************************************************/
/* Atom Functions */

extern JSBool
jsd_CreateAtomTable(JSDContext* jsdc);

extern void
jsd_DestroyAtomTable(JSDContext* jsdc);

extern JSDAtom*
jsd_AddAtom(JSDContext* jsdc, const char* str);

extern JSDAtom*
jsd_CloneAtom(JSDContext* jsdc, JSDAtom* atom);

extern void
jsd_DropAtom(JSDContext* jsdc, JSDAtom* atom);

#define JSD_ATOM_TO_STRING(a) ((const char*)((a)->str))

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
jsdlw_UserCodeAtPC(JSDContext* jsdc, JSDScript* jsdscript, jsuword pc);

extern JSBool
jsdlw_RawToProcessedLineNumber(JSDContext* jsdc, JSDScript* jsdscript,
                               uintN lineIn, uintN* lineOut);

extern JSBool
jsdlw_ProcessedToRawLineNumber(JSDContext* jsdc, JSDScript* jsdscript,
                               uintN lineIn, uintN* lineOut);


#if 0
/* our hook proc for LiveWire app start/stop */
extern void
jsdlw_AppHookProc(LWDBGApp* app,
                  JSBool created,
                  void *callerdata);
#endif


#endif
/***************************************************************************/

JS_END_EXTERN_C

#endif /* jsd_h___ */
