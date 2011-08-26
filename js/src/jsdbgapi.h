/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nick Fitzgerald <nfitzgerald@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsdbgapi_h___
#define jsdbgapi_h___
/*
 * JS debugger API.
 */
#include "jsapi.h"
#include "jsopcode.h"
#include "jsprvtd.h"

JS_BEGIN_EXTERN_C

extern JS_PUBLIC_API(JSCrossCompartmentCall *)
JS_EnterCrossCompartmentCallScript(JSContext *cx, JSScript *target);

extern JS_PUBLIC_API(JSCrossCompartmentCall *)
JS_EnterCrossCompartmentCallStackFrame(JSContext *cx, JSStackFrame *target);

#ifdef __cplusplus
JS_END_EXTERN_C

namespace JS {

class JS_PUBLIC_API(AutoEnterScriptCompartment)
{
  protected:
    JSCrossCompartmentCall *call;

  public:
    AutoEnterScriptCompartment() : call(NULL) {}

    bool enter(JSContext *cx, JSScript *target);

    bool entered() const { return call != NULL; }

    ~AutoEnterScriptCompartment() {
        if (call && call != reinterpret_cast<JSCrossCompartmentCall*>(1))
            JS_LeaveCrossCompartmentCall(call);
    }
};

class JS_PUBLIC_API(AutoEnterFrameCompartment) : public AutoEnterScriptCompartment
{
  public:
    bool enter(JSContext *cx, JSStackFrame *target);
};

} /* namespace JS */

JS_BEGIN_EXTERN_C
#endif

extern JS_PUBLIC_API(JSScript *)
JS_GetScriptFromObject(JSObject *scriptObject);

extern JS_PUBLIC_API(JSString *)
JS_DecompileScript(JSContext *cx, JSScript *script, const char *name, uintN indent);

/*
 * Currently, we only support runtime-wide debugging. In the future, we should
 * be able to support compartment-wide debugging.
 */
extern JS_PUBLIC_API(void)
JS_SetRuntimeDebugMode(JSRuntime *rt, JSBool debug);

/*
 * Debug mode is a compartment-wide mode that enables a debugger to attach
 * to and interact with running methodjit-ed frames. In particular, it causes
 * every function to be compiled as if an eval was present (so eval-in-frame)
 * can work, and it ensures that functions can be re-JITed for other debug
 * features. In general, it is not safe to interact with frames that were live
 * before debug mode was enabled. For this reason, it is also not safe to
 * enable debug mode while frames are live.
 */

/* Get current state of debugging mode. */
extern JS_PUBLIC_API(JSBool)
JS_GetDebugMode(JSContext *cx);

/*
 * Turn on/off debugging mode for a single compartment. This should only be
 * used when no code from this compartment is running or on the stack in any
 * thread.
 */
JS_FRIEND_API(JSBool)
JS_SetDebugModeForCompartment(JSContext *cx, JSCompartment *comp, JSBool debug);

/*
 * Turn on/off debugging mode for a context's compartment.
 */
JS_FRIEND_API(JSBool)
JS_SetDebugMode(JSContext *cx, JSBool debug);

/* Turn on single step mode. */
extern JS_PUBLIC_API(JSBool)
JS_SetSingleStepMode(JSContext *cx, JSScript *script, JSBool singleStep);

/*
 * Unexported library-private helper used to unpatch all traps in a script.
 * Returns script->code if script has no traps, else a JS_malloc'ed copy of
 * script->code which the caller must JS_free, or null on JS_malloc OOM.
 */
extern jsbytecode *
js_UntrapScriptCode(JSContext *cx, JSScript *script);

/* The closure argument will be marked. */
extern JS_PUBLIC_API(JSBool)
JS_SetTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
           JSTrapHandler handler, jsval closure);

extern JS_PUBLIC_API(JSOp)
JS_GetTrapOpcode(JSContext *cx, JSScript *script, jsbytecode *pc);

extern JS_PUBLIC_API(void)
JS_ClearTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
             JSTrapHandler *handlerp, jsval *closurep);

extern JS_PUBLIC_API(void)
JS_ClearScriptTraps(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(void)
JS_ClearAllTrapsForCompartment(JSContext *cx);

extern JS_PUBLIC_API(JSBool)
JS_SetInterrupt(JSRuntime *rt, JSInterruptHook handler, void *closure);

extern JS_PUBLIC_API(JSBool)
JS_ClearInterrupt(JSRuntime *rt, JSInterruptHook *handlerp, void **closurep);

/************************************************************************/

extern JS_PUBLIC_API(JSBool)
JS_SetWatchPoint(JSContext *cx, JSObject *obj, jsid id,
                 JSWatchPointHandler handler, JSObject *closure);

extern JS_PUBLIC_API(JSBool)
JS_ClearWatchPoint(JSContext *cx, JSObject *obj, jsid id,
                   JSWatchPointHandler *handlerp, JSObject **closurep);

extern JS_PUBLIC_API(JSBool)
JS_ClearWatchPointsForObject(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(JSBool)
JS_ClearAllWatchPoints(JSContext *cx);

/************************************************************************/

extern JS_PUBLIC_API(uintN)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc);

extern JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, uintN lineno);

extern JS_PUBLIC_API(jsbytecode *)
JS_EndPC(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(JSBool)
JS_GetLinePCs(JSContext *cx, JSScript *script,
              uintN startLine, uintN maxLines,
              uintN* count, uintN** lines, jsbytecode*** pcs);

extern JS_PUBLIC_API(uintN)
JS_GetFunctionArgumentCount(JSContext *cx, JSFunction *fun);

extern JS_PUBLIC_API(JSBool)
JS_FunctionHasLocalNames(JSContext *cx, JSFunction *fun);

/*
 * N.B. The mark is in the context temp pool and thus the caller must take care
 * to call JS_ReleaseFunctionLocalNameArray in a LIFO manner (wrt to any other
 * call that may use the temp pool.
 */
extern JS_PUBLIC_API(jsuword *)
JS_GetFunctionLocalNameArray(JSContext *cx, JSFunction *fun, void **markp);

extern JS_PUBLIC_API(JSAtom *)
JS_LocalNameToAtom(jsuword w);

extern JS_PUBLIC_API(JSString *)
JS_AtomKey(JSAtom *atom);

extern JS_PUBLIC_API(void)
JS_ReleaseFunctionLocalNameArray(JSContext *cx, void *mark);

extern JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, JSFunction *fun);

extern JS_PUBLIC_API(JSNative)
JS_GetFunctionNative(JSContext *cx, JSFunction *fun);

extern JS_PUBLIC_API(JSPrincipals *)
JS_GetScriptPrincipals(JSContext *cx, JSScript *script);

/*
 * Stack Frame Iterator
 *
 * Used to iterate through the JS stack frames to extract
 * information from the frames.
 */

extern JS_PUBLIC_API(JSStackFrame *)
JS_FrameIterator(JSContext *cx, JSStackFrame **iteratorp);

extern JS_PUBLIC_API(JSScript *)
JS_GetFrameScript(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(jsbytecode *)
JS_GetFramePC(JSContext *cx, JSStackFrame *fp);

/*
 * Get the closest scripted frame below fp.  If fp is null, start from cx->fp.
 */
extern JS_PUBLIC_API(JSStackFrame *)
JS_GetScriptedCaller(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(void *)
JS_GetFrameAnnotation(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(void)
JS_SetFrameAnnotation(JSContext *cx, JSStackFrame *fp, void *annotation);

extern JS_PUBLIC_API(void *)
JS_GetFramePrincipalArray(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(JSBool)
JS_IsScriptFrame(JSContext *cx, JSStackFrame *fp);

/* this is deprecated, use JS_GetFrameScopeChain instead */
extern JS_PUBLIC_API(JSObject *)
JS_GetFrameObject(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(JSObject *)
JS_GetFrameScopeChain(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(JSObject *)
JS_GetFrameCallObject(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(JSBool)
JS_GetFrameThis(JSContext *cx, JSStackFrame *fp, jsval *thisv);

extern JS_PUBLIC_API(JSFunction *)
JS_GetFrameFunction(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(JSObject *)
JS_GetFrameFunctionObject(JSContext *cx, JSStackFrame *fp);

/* XXXrginda Initially published with typo */
#define JS_IsContructorFrame JS_IsConstructorFrame
extern JS_PUBLIC_API(JSBool)
JS_IsConstructorFrame(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(JSBool)
JS_IsDebuggerFrame(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(JSBool)
JS_IsGlobalFrame(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(jsval)
JS_GetFrameReturnValue(JSContext *cx, JSStackFrame *fp);

extern JS_PUBLIC_API(void)
JS_SetFrameReturnValue(JSContext *cx, JSStackFrame *fp, jsval rval);

/**
 * Return fp's callee function object (fp->callee) if it has one. Note that
 * this API cannot fail. A null return means "no callee": fp is a global or
 * eval-from-global frame, not a call frame.
 *
 * This API began life as an infallible getter, but now it can return either:
 *
 * 1. An optimized closure that was compiled assuming the function could not
 *    escape and be called from sites the compiler could not see.
 *
 * 2. A "joined function object", an optimization whereby SpiderMonkey avoids
 *    creating fresh function objects for every evaluation of a function
 *    expression that is used only once by a consumer that either promises to
 *    clone later when asked for the value or that cannot leak the value.
 *
 * Because Mozilla's Gecko embedding of SpiderMonkey (and no doubt other
 * embeddings) calls this API in potentially performance-sensitive ways (e.g.
 * in nsContentUtils::GetDocumentFromCaller), we are leaving this API alone. It
 * may now return an unwrapped non-escaping optimized closure, or a joined
 * function object. Such optimized objects may work well if called from the
 * correct context, never mutated or compared for identity, etc.
 *
 * However, if you really need to get the same callee object that JS code would
 * see, which means undoing the optimizations, where an undo attempt can fail,
 * then use JS_GetValidFrameCalleeObject.
 */
extern JS_PUBLIC_API(JSObject *)
JS_GetFrameCalleeObject(JSContext *cx, JSStackFrame *fp);

/**
 * Return fp's callee function object after running the deferred closure
 * cloning "method read barrier". This API can fail! If the frame has no
 * callee, this API returns true with JSVAL_IS_VOID(*vp).
 */
extern JS_PUBLIC_API(JSBool)
JS_GetValidFrameCalleeObject(JSContext *cx, JSStackFrame *fp, jsval *vp);

/************************************************************************/

extern JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(const jschar *)
JS_GetScriptSourceMap(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(uintN)
JS_GetScriptBaseLineNumber(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(uintN)
JS_GetScriptLineExtent(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(JSVersion)
JS_GetScriptVersion(JSContext *cx, JSScript *script);

/************************************************************************/

/*
 * Hook setters for script creation and destruction, see jsprvtd.h for the
 * typedefs.  These macros provide binary compatibility and newer, shorter
 * synonyms.
 */
#define JS_SetNewScriptHook     JS_SetNewScriptHookProc
#define JS_SetDestroyScriptHook JS_SetDestroyScriptHookProc

extern JS_PUBLIC_API(void)
JS_SetNewScriptHook(JSRuntime *rt, JSNewScriptHook hook, void *callerdata);

extern JS_PUBLIC_API(void)
JS_SetDestroyScriptHook(JSRuntime *rt, JSDestroyScriptHook hook,
                        void *callerdata);

/************************************************************************/

extern JS_PUBLIC_API(JSBool)
JS_EvaluateUCInStackFrame(JSContext *cx, JSStackFrame *fp,
                          const jschar *chars, uintN length,
                          const char *filename, uintN lineno,
                          jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_EvaluateInStackFrame(JSContext *cx, JSStackFrame *fp,
                        const char *bytes, uintN length,
                        const char *filename, uintN lineno,
                        jsval *rval);

/************************************************************************/

typedef struct JSPropertyDesc {
    jsval           id;         /* primary id, atomized string, or int */
    jsval           value;      /* property value */
    uint8           flags;      /* flags, see below */
    uint8           spare;      /* unused */
    uint16          slot;       /* argument/variable slot */
    jsval           alias;      /* alias id if JSPD_ALIAS flag */
} JSPropertyDesc;

#define JSPD_ENUMERATE  0x01    /* visible to for/in loop */
#define JSPD_READONLY   0x02    /* assignment is error */
#define JSPD_PERMANENT  0x04    /* property cannot be deleted */
#define JSPD_ALIAS      0x08    /* property has an alias id */
#define JSPD_ARGUMENT   0x10    /* argument to function */
#define JSPD_VARIABLE   0x20    /* local variable in function */
#define JSPD_EXCEPTION  0x40    /* exception occurred fetching the property, */
                                /* value is exception */
#define JSPD_ERROR      0x80    /* native getter returned JS_FALSE without */
                                /* throwing an exception */

typedef struct JSPropertyDescArray {
    uint32          length;     /* number of elements in array */
    JSPropertyDesc  *array;     /* alloc'd by Get, freed by Put */
} JSPropertyDescArray;

typedef struct JSScopeProperty JSScopeProperty;

extern JS_PUBLIC_API(JSScopeProperty *)
JS_PropertyIterator(JSObject *obj, JSScopeProperty **iteratorp);

extern JS_PUBLIC_API(JSBool)
JS_GetPropertyDesc(JSContext *cx, JSObject *obj, JSScopeProperty *shape,
                   JSPropertyDesc *pd);

extern JS_PUBLIC_API(JSBool)
JS_GetPropertyDescArray(JSContext *cx, JSObject *obj, JSPropertyDescArray *pda);

extern JS_PUBLIC_API(void)
JS_PutPropertyDescArray(JSContext *cx, JSPropertyDescArray *pda);

/************************************************************************/

extern JS_PUBLIC_API(JSBool)
JS_SetDebuggerHandler(JSRuntime *rt, JSDebuggerHandler hook, void *closure);

extern JS_PUBLIC_API(JSBool)
JS_SetSourceHandler(JSRuntime *rt, JSSourceHandler handler, void *closure);

extern JS_PUBLIC_API(JSBool)
JS_SetExecuteHook(JSRuntime *rt, JSInterpreterHook hook, void *closure);

extern JS_PUBLIC_API(JSBool)
JS_SetCallHook(JSRuntime *rt, JSInterpreterHook hook, void *closure);

extern JS_PUBLIC_API(JSBool)
JS_SetThrowHook(JSRuntime *rt, JSThrowHook hook, void *closure);

extern JS_PUBLIC_API(JSBool)
JS_SetDebugErrorHook(JSRuntime *rt, JSDebugErrorHook hook, void *closure);

/************************************************************************/

extern JS_PUBLIC_API(size_t)
JS_GetObjectTotalSize(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(size_t)
JS_GetFunctionTotalSize(JSContext *cx, JSFunction *fun);

extern JS_PUBLIC_API(size_t)
JS_GetScriptTotalSize(JSContext *cx, JSScript *script);

/*
 * Return true if obj is a "system" object, that is, one created by
 * JS_NewSystemObject with the system flag set and not JS_NewObject.
 *
 * What "system" means is up to the API client, but it can be used to implement
 * access control policies based on script filenames and their prefixes, using
 * JS_FlagScriptFilenamePrefix and JS_GetTopScriptFilenameFlags.
 */
extern JS_PUBLIC_API(JSBool)
JS_IsSystemObject(JSContext *cx, JSObject *obj);

/*
 * Mark an object as being a system object. This should be called immediately
 * after allocating the object. A system object is an object for which
 * JS_IsSystemObject returns true.
 */
extern JS_PUBLIC_API(JSBool)
JS_MakeSystemObject(JSContext *cx, JSObject *obj);

/************************************************************************/

extern JS_FRIEND_API(void)
js_RevertVersion(JSContext *cx);

extern JS_PUBLIC_API(const JSDebugHooks *)
JS_GetGlobalDebugHooks(JSRuntime *rt);

extern JS_PUBLIC_API(JSDebugHooks *)
JS_SetContextDebugHooks(JSContext *cx, const JSDebugHooks *hooks);

/* Disable debug hooks for this context. */
extern JS_PUBLIC_API(JSDebugHooks *)
JS_ClearContextDebugHooks(JSContext *cx);

/**
 * Start any profilers that are available and have been configured on for this
 * platform. This is NOT thread safe.
 *
 * The profileName is used by some profilers to describe the current profiling
 * run. It may be used for part of the filename of the output, but the
 * specifics depend on the profiler. Many profilers will ignore it. Passing in
 * NULL is legal; some profilers may use it to output to stdout or similar.
 *
 * Returns true if no profilers fail to start.
 */
extern JS_PUBLIC_API(JSBool)
JS_StartProfiling(const char *profileName);

/**
 * Stop any profilers that were previously started with JS_StartProfiling.
 * Returns true if no profilers fail to stop.
 */
extern JS_PUBLIC_API(JSBool)
JS_StopProfiling(const char *profileName);

/**
 * Write the current profile data to the given file, if applicable to whatever
 * profiler is being used.
 */
extern JS_PUBLIC_API(JSBool)
JS_DumpProfile(const char *outfile, const char *profileName);

/**
 * Pause currently active profilers (only supported by some profilers). Returns
 * whether any profilers failed to pause. (Profilers that do not support
 * pause/resume do not count.)
 */
extern JS_PUBLIC_API(JSBool)
JS_PauseProfilers(const char *profileName);

/**
 * Resume suspended profilers
 */
extern JS_PUBLIC_API(JSBool)
JS_ResumeProfilers(const char *profileName);

/**
 * Add various profiling-related functions as properties of the given object.
 */
extern JS_PUBLIC_API(JSBool)
JS_DefineProfilingFunctions(JSContext *cx, JSObject *obj);

/* Defined in vm/Debugger.cpp. */
extern JS_PUBLIC_API(JSBool)
JS_DefineDebuggerObject(JSContext *cx, JSObject *obj);

/**
 * The profiling API calls are not able to report errors, so they use a
 * thread-unsafe global memory buffer to hold the last error encountered. This
 * should only be called after something returns false.
 */
JS_PUBLIC_API(const char *)
JS_UnsafeGetLastProfilingError();

#ifdef MOZ_CALLGRIND

extern JS_FRIEND_API(JSBool)
js_StopCallgrind();

extern JS_FRIEND_API(JSBool)
js_StartCallgrind();

extern JS_FRIEND_API(JSBool)
js_DumpCallgrind(const char *outfile);

#endif /* MOZ_CALLGRIND */

#ifdef MOZ_VTUNE

extern JS_FRIEND_API(bool)
js_StartVtune(const char *profileName);

extern JS_FRIEND_API(bool)
js_StopVtune();

extern JS_FRIEND_API(bool)
js_PauseVtune();

extern JS_FRIEND_API(bool)
js_ResumeVtune();

#endif /* MOZ_VTUNE */

#ifdef MOZ_TRACEVIS
extern JS_FRIEND_API(JSBool)
js_InitEthogram(JSContext *cx, uintN argc, jsval *vp);
extern JS_FRIEND_API(JSBool)
js_ShutdownEthogram(JSContext *cx, uintN argc, jsval *vp);
#endif /* MOZ_TRACEVIS */

#ifdef MOZ_TRACE_JSCALLS
typedef void (*JSFunctionCallback)(const JSFunction *fun,
                                   const JSScript *scr,
                                   const JSContext *cx,
                                   int entering);

/*
 * The callback is expected to be quick and noninvasive. It should not
 * trigger interrupts, turn on debugging, or produce uncaught JS
 * exceptions. The state of the stack and registers in the context
 * cannot be relied upon, since this callback may be invoked directly
 * from either JIT. The 'entering' field means we are entering a
 * function if it is positive, leaving a function if it is zero or
 * negative.
 */
extern JS_PUBLIC_API(void)
JS_SetFunctionCallback(JSContext *cx, JSFunctionCallback fcb);

extern JS_PUBLIC_API(JSFunctionCallback)
JS_GetFunctionCallback(JSContext *cx);
#endif /* MOZ_TRACE_JSCALLS */

extern JS_PUBLIC_API(void)
JS_DumpBytecode(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(void)
JS_DumpCompartmentBytecode(JSContext *cx);

JS_END_EXTERN_C

#endif /* jsdbgapi_h___ */
