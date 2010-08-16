/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Turn on debugging mode, ignoring the presence of live frames. */
extern JS_FRIEND_API(JSBool)
js_SetDebugMode(JSContext *cx, JSBool debug);

/* Turn on debugging mode. */
extern JS_PUBLIC_API(JSBool)
JS_SetDebugMode(JSContext *cx, JSBool debug);

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
JS_ClearAllTraps(JSContext *cx);

extern JS_PUBLIC_API(JSTrapStatus)
JS_HandleTrap(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval);

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

#ifdef JS_HAS_OBJ_WATCHPOINT
/*
 * Hide these non-API function prototypes by testing whether the internal
 * header file "jsversion.h" has been included.
 */
extern void
js_TraceWatchPoints(JSTracer *trc, JSObject *obj);

extern void
js_SweepWatchPoints(JSContext *cx);

#ifdef __cplusplus

extern const js::Shape *
js_FindWatchPoint(JSRuntime *rt, JSObject *obj, jsid id);

extern JSBool
js_watch_set(JSContext *cx, JSObject *obj, jsid id, js::Value *vp);

extern JSBool
js_watch_set_wrapper(JSContext *cx, uintN argc, js::Value *vp);

extern js::PropertyOp
js_WrapWatchedSetter(JSContext *cx, jsid id, uintN attrs, js::PropertyOp setter);

#endif

#endif /* JS_HAS_OBJ_WATCHPOINT */

/************************************************************************/

extern JS_PUBLIC_API(uintN)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc);

extern JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, uintN lineno);

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

/*
 * Return a weak reference to fp's principals.  A null return does not denote
 * an error, it means there are no principals.
 */
extern JSPrincipals *
js_StackFramePrincipals(JSContext *cx, JSStackFrame *fp);

JSPrincipals *
js_EvalFramePrincipals(JSContext *cx, JSObject *callee, JSStackFrame *caller);

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

extern JS_PUBLIC_API(JSObject *)
JS_GetFrameThis(JSContext *cx, JSStackFrame *fp);

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

extern JS_FRIEND_API(JSBool)
js_GetPropertyByIdWithFakeFrame(JSContext *cx, JSObject *obj, JSObject *scopeobj, jsid id,
                                jsval *vp);

extern JS_FRIEND_API(JSBool)
js_SetPropertyByIdWithFakeFrame(JSContext *cx, JSObject *obj, JSObject *scopeobj, jsid id,
                                jsval *vp);

extern JS_FRIEND_API(JSBool)
js_CallFunctionValueWithFakeFrame(JSContext *cx, JSObject *obj, JSObject *scopeobj, jsval funval,
                                  uintN argc, jsval *argv, jsval *rval);

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
 * Get the top-most running script on cx starting from fp, or from the top of
 * cx's frame stack if fp is null, and return its script filename flags.  If
 * the script has a null filename member, return JSFILENAME_NULL.
 */
extern JS_PUBLIC_API(uint32)
JS_GetTopScriptFilenameFlags(JSContext *cx, JSStackFrame *fp);

/*
 * Get the script filename flags for the script.  If the script doesn't have a
 * filename, return JSFILENAME_NULL.
 */
extern JS_PUBLIC_API(uint32)
JS_GetScriptFilenameFlags(JSScript *script);

/*
 * Associate flags with a script filename prefix in rt, so that any subsequent
 * script compilation will inherit those flags if the script's filename is the
 * same as prefix, or if prefix is a substring of the script's filename.
 *
 * The API defines only one flag bit, JSFILENAME_SYSTEM, leaving the remaining
 * 31 bits up to the API client to define.  The union of all 32 bits must not
 * be a legal combination, however, in order to preserve JSFILENAME_NULL as a
 * unique value.  API clients may depend on JSFILENAME_SYSTEM being a set bit
 * in JSFILENAME_NULL -- a script with a null filename member is presumed to
 * be a "system" script.
 */
extern JS_PUBLIC_API(JSBool)
JS_FlagScriptFilenamePrefix(JSRuntime *rt, const char *prefix, uint32 flags);

#define JSFILENAME_NULL         0xffffffff      /* null script filename */
#define JSFILENAME_SYSTEM       0x00000001      /* "system" script, see below */
#define JSFILENAME_PROTECTED    0x00000002      /* scripts need protection */

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

extern JS_PUBLIC_API(const JSDebugHooks *)
JS_GetGlobalDebugHooks(JSRuntime *rt);

extern JS_PUBLIC_API(JSDebugHooks *)
JS_SetContextDebugHooks(JSContext *cx, const JSDebugHooks *hooks);

/* Disable debug hooks for this context. */
extern JS_PUBLIC_API(JSDebugHooks *)
JS_ClearContextDebugHooks(JSContext *cx);

#ifdef MOZ_SHARK

extern JS_PUBLIC_API(JSBool)
JS_StartChudRemote();

extern JS_PUBLIC_API(JSBool)
JS_StopChudRemote();

extern JS_PUBLIC_API(JSBool)
JS_ConnectShark();

extern JS_PUBLIC_API(JSBool)
JS_DisconnectShark();

extern JS_FRIEND_API(JSBool)
js_StopShark(JSContext *cx, uintN argc, jsval *vp);

extern JS_FRIEND_API(JSBool)
js_StartShark(JSContext *cx, uintN argc, jsval *vp);

extern JS_FRIEND_API(JSBool)
js_ConnectShark(JSContext *cx, uintN argc, jsval *vp);

extern JS_FRIEND_API(JSBool)
js_DisconnectShark(JSContext *cx, uintN argc, jsval *vp);

#endif /* MOZ_SHARK */

#ifdef MOZ_CALLGRIND

extern JS_FRIEND_API(JSBool)
js_StopCallgrind(JSContext *cx, uintN argc, jsval *vp);

extern JS_FRIEND_API(JSBool)
js_StartCallgrind(JSContext *cx, uintN argc, jsval *vp);

extern JS_FRIEND_API(JSBool)
js_DumpCallgrind(JSContext *cx, uintN argc, jsval *vp);

#endif /* MOZ_CALLGRIND */

#ifdef MOZ_VTUNE

extern JS_FRIEND_API(JSBool)
js_StartVtune(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval);

extern JS_FRIEND_API(JSBool)
js_StopVtune(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);

extern JS_FRIEND_API(JSBool)
js_PauseVtune(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval);

extern JS_FRIEND_API(JSBool)
js_ResumeVtune(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval);

#endif /* MOZ_VTUNE */

#ifdef MOZ_TRACEVIS
extern JS_FRIEND_API(JSBool)
js_InitEthogram(JSContext *cx, JSObject *obj,
                uintN argc, jsval *argv, jsval *rval);
extern JS_FRIEND_API(JSBool)
js_ShutdownEthogram(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *rval);
#endif /* MOZ_TRACEVIS */

JS_END_EXTERN_C

#endif /* jsdbgapi_h___ */
