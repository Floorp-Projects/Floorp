/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#include "jsapi.h"
#include "jshash.h"
#include "jsclist.h"
#include "jsdebug.h"
#include "js/OldDebugAPI.h"
#include "jsd_lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    bool                    inited;
    void*                   data;
    uint32_t                flags;
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
    JSObject*               glob;
    JSD_UserCallbacks       userCallbacks;
    void*                   user;
    JSCList                 scripts;
    JSHashTable*            scriptsTable;
    JSCList                 sources;
    JSCList                 removedSources;
    unsigned                   sourceAlterCount;
    JSHashTable*            atoms;
    JSCList                 objectsList;
    JSHashTable*            objectsTable;
    JSDProfileData*         callingFunctionPData;
    int64_t                 lastReturnTime;
#ifdef JSD_THREADSAFE
    JSDStaticLock*          scriptsLock;
    JSDStaticLock*          sourceTextLock;
    JSDStaticLock*          objectsLock;
    JSDStaticLock*          atomsLock;
    JSDStaticLock*          threadStatesLock;
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
    unsigned       lineBase;   /* we cache this */
    unsigned       lineExtent; /* we cache this */
    JSCList     hooks;      /* JSCList of JSDExecHooks for this script */
    char*       url;
    uint32_t    flags;
    void*       data;

    JSDProfileData  *profileData;
};

struct JSDProfileData
{
    JSDProfileData* caller;
    int64_t  lastCallStart;
    int64_t  runningTime;
    unsigned    callCount;
    unsigned    recurseDepth;
    unsigned    maxRecurseDepth;
    double minExecutionTime;
    double maxExecutionTime;
    double totalExecutionTime;
    double minOwnExecutionTime;
    double maxOwnExecutionTime;
    double totalOwnExecutionTime;
};

struct JSDSourceText
{
    JSCList          links;      /* we are part of a JSCList */
    char*            url;
    char*            text;
    unsigned         textLength;
    unsigned         textSpace;
    bool             dirty;
    JSDSourceStatus  status;
    unsigned         alterCount;
    bool             doingEval;
};

struct JSDExecHook
{
    JSCList               links;        /* we are part of a JSCList */
    JSDScript*            jsdscript;
    uintptr_t             pc;
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
    unsigned               stackDepth;
    unsigned               flags;
};

struct JSDStackFrameInfo
{
    JSCList             links;        /* we are part of a JSCList */
    JSDThreadState*     jsdthreadstate;
    JSDScript*          jsdscript;
    uintptr_t           pc;
    bool                isConstructing;
    JSAbstractFramePtr  frame;
};

#define GOT_PROTO   ((short) (1 << 0))
#define GOT_PROPS   ((short) (1 << 1))
#define GOT_PARENT  ((short) (1 << 2))
#define GOT_CTOR    ((short) (1 << 3))

struct JSDValue
{
    jsval       val;
    int        nref;
    JSCList     props;
    JSString*   string;
    JSString*   funName;
    const char* className;
    JSDValue*   proto;
    JSDValue*   parent;
    JSDValue*   ctor;
    unsigned       flags;
};

struct JSDProperty
{
    JSCList     links;      /* we are part of a JSCList */
    int        nref;
    JSDValue*   val;
    JSDValue*   name;
    JSDValue*   alias;
    unsigned       flags;
};

struct JSDAtom
{
    char* str;      /* must be first element in struct for compare */
    int  refcount;
};

struct JSDObject
{
    JSCList     links;      /* we are part of a JSCList */
    JSObject*   obj;
    JSDAtom*    newURL;
    unsigned       newLineno;
    JSDAtom*    ctorURL;
    unsigned       ctorLineno;
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
jsd_DebuggerPause(JSDContext* jsdc, bool forceAllHooksOff);

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

extern bool
jsd_SetErrorReporter(JSDContext*       jsdc,
                     JSD_ErrorReporter reporter,
                     void*             callerdata);

extern bool
jsd_GetErrorReporter(JSDContext*        jsdc,
                     JSD_ErrorReporter* reporter,
                     void**             callerdata);

/***************************************************************************/
/* Script functions */

extern bool
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
                          JSAbstractFramePtr frame);

extern JSDProfileData*
jsd_GetScriptProfileData(JSDContext* jsdc, JSDScript *script);

extern uint32_t
jsd_GetScriptFlags(JSDContext *jsdc, JSDScript *script);

extern void
jsd_SetScriptFlags(JSDContext *jsdc, JSDScript *script, uint32_t flags);

extern unsigned
jsd_GetScriptCallCount(JSDContext* jsdc, JSDScript *script);

extern  unsigned
jsd_GetScriptMaxRecurseDepth(JSDContext* jsdc, JSDScript *script);

extern double
jsd_GetScriptMinExecutionTime(JSDContext* jsdc, JSDScript *script);

extern double
jsd_GetScriptMaxExecutionTime(JSDContext* jsdc, JSDScript *script);

extern double
jsd_GetScriptTotalExecutionTime(JSDContext* jsdc, JSDScript *script);

extern double
jsd_GetScriptMinOwnExecutionTime(JSDContext* jsdc, JSDScript *script);

extern double
jsd_GetScriptMaxOwnExecutionTime(JSDContext* jsdc, JSDScript *script);

extern double
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

extern bool
jsd_IsActiveScript(JSDContext* jsdc, JSDScript *jsdscript);

extern const char*
jsd_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript);

extern JSString*
jsd_GetScriptFunctionId(JSDContext* jsdc, JSDScript *jsdscript);

extern unsigned
jsd_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript);

extern unsigned
jsd_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript);

extern bool
jsd_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata);

extern bool
jsd_GetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc* hook, void** callerdata);

extern uintptr_t
jsd_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, unsigned line);

extern unsigned
jsd_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, uintptr_t pc);

extern bool
jsd_GetLinePCs(JSDContext* jsdc, JSDScript* jsdscript,
               unsigned startLine, unsigned maxLines,
               unsigned* count, unsigned** lines, uintptr_t** pcs);

extern void
jsd_NewScriptHookProc(
                JSContext   *cx,
                const char  *filename,      /* URL this script loads from */
                unsigned       lineno,         /* line where this script starts */
                JSScript    *script,
                JSFunction  *fun,
                void*       callerdata);

extern void
jsd_DestroyScriptHookProc(
                JSFreeOp    *fop,
                JSScript    *script,
                void*       callerdata);

/* Script execution hook functions */

extern bool
jsd_SetExecutionHook(JSDContext*           jsdc,
                     JSDScript*            jsdscript,
                     uintptr_t             pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

extern bool
jsd_ClearExecutionHook(JSDContext*           jsdc,
                       JSDScript*            jsdscript,
                       uintptr_t             pc);

extern bool
jsd_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript);

extern bool
jsd_ClearAllExecutionHooks(JSDContext* jsdc);

extern void
jsd_ScriptCreated(JSDContext* jsdc,
                  JSContext   *cx,
                  const char  *filename,    /* URL this script loads from */
                  unsigned       lineno,       /* line where this script starts */
                  JSScript    *script,
                  JSFunction  *fun);

extern void
jsd_ScriptDestroyed(JSDContext* jsdc,
                    JSFreeOp    *fop,
                    JSScript    *script);

/***************************************************************************/
/* Source Text functions */

extern JSDSourceText*
jsd_IterateSources(JSDContext* jsdc, JSDSourceText **iterp);

extern JSDSourceText*
jsd_FindSourceForURL(JSDContext* jsdc, const char* url);

extern const char*
jsd_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern bool
jsd_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen);

extern void
jsd_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern JSDSourceStatus
jsd_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern bool
jsd_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern void
jsd_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, bool dirty);

extern unsigned
jsd_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

extern unsigned
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
extern bool
jsd_AddFullSourceText(JSDContext* jsdc,
                      const char* text,       /* *not* zero terminated */
                      size_t      length,
                      const char* url);

extern void
jsd_DestroyAllSources(JSDContext* jsdc);

extern char*
jsd_BuildNormalizedURL(const char* url_string);

extern void
jsd_StartingEvalUsingFilename(JSDContext* jsdc, const char* url);

extern void
jsd_FinishedEvalUsingFilename(JSDContext* jsdc, const char* url);

/***************************************************************************/
/* Interrupt Hook functions */

extern bool
jsd_SetInterruptHook(JSDContext*           jsdc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

extern bool
jsd_ClearInterruptHook(JSDContext* jsdc);

extern bool
jsd_EnableSingleStepInterrupts(JSDContext* jsdc,
                               JSDScript*  jsdscript,
                               bool        enable);

extern bool
jsd_SetDebugBreakHook(JSDContext*           jsdc,
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata);

extern bool
jsd_ClearDebugBreakHook(JSDContext* jsdc);

extern bool
jsd_SetDebuggerHook(JSDContext*           jsdc,
                    JSD_ExecutionHookProc hook,
                    void*                 callerdata);

extern bool
jsd_ClearDebuggerHook(JSDContext* jsdc);

extern JSTrapStatus
jsd_CallExecutionHook(JSDContext*           jsdc,
                      JSContext*            cx,
                      unsigned                 type,
                      JSD_ExecutionHookProc hook,
                      void*                 hookData,
                      jsval*                rval);

extern bool
jsd_CallCallHook (JSDContext*      jsdc,
                  JSContext*       cx,
                  unsigned            type,
                  JSD_CallHookProc hook,
                  void*            hookData);

extern bool
jsd_SetThrowHook(JSDContext*           jsdc,
                 JSD_ExecutionHookProc hook,
                 void*                 callerdata);
extern bool
jsd_ClearThrowHook(JSDContext* jsdc);

extern JSTrapStatus
jsd_DebuggerHandler(JSContext *cx, JSScript *script, jsbytecode *pc,
                    jsval *rval, void *closure);

extern JSTrapStatus
jsd_ThrowHandler(JSContext *cx, JSScript *script, jsbytecode *pc,
                 jsval *rval, void *closure);

extern bool
jsd_SetFunctionHook(JSDContext*      jsdc,
                    JSD_CallHookProc hook,
                    void*            callerdata);

extern bool
jsd_ClearFunctionHook(JSDContext* jsdc);

extern bool
jsd_SetTopLevelHook(JSDContext*      jsdc,
                    JSD_CallHookProc hook,
                    void*            callerdata);

extern bool
jsd_ClearTopLevelHook(JSDContext* jsdc);

/***************************************************************************/
/* Stack Frame functions */

extern unsigned
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

extern uintptr_t
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

extern bool
jsd_IsStackFrameDebugger(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

extern bool
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

extern bool
jsd_EvaluateUCScriptInStackFrame(JSDContext* jsdc,
                                 JSDThreadState* jsdthreadstate,
                                 JSDStackFrameInfo* jsdframe,
                                 const jschar *bytes, unsigned length,
                                 const char *filename, unsigned lineno,
                                 bool eatExceptions, JS::MutableHandleValue rval);

extern bool
jsd_EvaluateScriptInStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, unsigned length,
                               const char *filename, unsigned lineno,
                               bool eatExceptions, JS::MutableHandleValue rval);

extern JSString*
jsd_ValToStringInStackFrame(JSDContext* jsdc,
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val);

extern bool
jsd_IsValidThreadState(JSDContext*        jsdc,
                       JSDThreadState*    jsdthreadstate);

extern bool
jsd_IsValidFrameInThreadState(JSDContext*        jsdc,
                              JSDThreadState*    jsdthreadstate,
                              JSDStackFrameInfo* jsdframe);

extern JSDValue*
jsd_GetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

extern bool
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
extern JSDStaticLock* _jsd_global_lock;
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
#define JSD_INIT_LOCKS(jsdc)                                       \
    ( (nullptr != (jsdc->scriptsLock      = jsd_CreateLock())) &&  \
      (nullptr != (jsdc->sourceTextLock   = jsd_CreateLock())) &&  \
      (nullptr != (jsdc->atomsLock        = jsd_CreateLock())) &&  \
      (nullptr != (jsdc->objectsLock      = jsd_CreateLock())) &&  \
      (nullptr != (jsdc->threadStatesLock = jsd_CreateLock())) )

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

extern bool
jsd_IsValueObject(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueNumber(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueInt(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueDouble(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueString(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueBoolean(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueNull(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueVoid(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValuePrimitive(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueFunction(JSDContext* jsdc, JSDValue* jsdval);

extern bool
jsd_IsValueNative(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

extern bool
jsd_GetValueBoolean(JSDContext* jsdc, JSDValue* jsdval);

extern int32_t
jsd_GetValueInt(JSDContext* jsdc, JSDValue* jsdval);

extern double
jsd_GetValueDouble(JSDContext* jsdc, JSDValue* jsdval);

extern JSString*
jsd_GetValueString(JSDContext* jsdc, JSDValue* jsdval);

extern JSString*
jsd_GetValueFunctionId(JSDContext* jsdc, JSDValue* jsdval);

extern JSFunction*
jsd_GetValueFunction(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

extern unsigned
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

extern unsigned
jsd_GetPropertyFlags(JSDContext* jsdc, JSDProperty* jsdprop);

/**************************************************/
/* Stepping Functions */

extern void *
jsd_FunctionCallHook(JSContext *cx, JSAbstractFramePtr frame, bool isConstructing,
                     bool before, bool *ok, void *closure);

extern void *
jsd_TopLevelCallHook(JSContext *cx, JSAbstractFramePtr frame, bool isConstructing,
                     bool before, bool *ok, void *closure);

/**************************************************/
/* Object Functions */

extern bool
jsd_InitObjectManager(JSDContext* jsdc);

extern void
jsd_DestroyObjectManager(JSDContext* jsdc);

extern void
jsd_DestroyObjects(JSDContext* jsdc);

extern void
jsd_Constructing(JSDContext* jsdc, JSContext *cx, JSObject *obj,
                 JSAbstractFramePtr frame);

extern JSDObject*
jsd_IterateObjects(JSDContext* jsdc, JSDObject** iterp);

extern JSObject*
jsd_GetWrappedObject(JSDContext* jsdc, JSDObject* jsdobj);

extern const char*
jsd_GetObjectNewURL(JSDContext* jsdc, JSDObject* jsdobj);

extern unsigned
jsd_GetObjectNewLineNumber(JSDContext* jsdc, JSDObject* jsdobj);

extern const char*
jsd_GetObjectConstructorURL(JSDContext* jsdc, JSDObject* jsdobj);

extern unsigned
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

extern bool
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

struct AutoSaveExceptionState {
    AutoSaveExceptionState(JSContext *cx) : mCx(cx) {
        mState = JS_SaveExceptionState(cx);
    }
    ~AutoSaveExceptionState() {
        JS_RestoreExceptionState(mCx, mState);
    }
    JSContext *mCx;
    JSExceptionState *mState;
};

#endif /* jsd_h___ */
