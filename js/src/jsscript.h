/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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

#ifndef jsscript_h___
#define jsscript_h___
/*
 * JS script descriptor.
 */
#include "jsatom.h"
#include "jsprvtd.h"

JS_BEGIN_EXTERN_C

/*
 * Type of try note associated with each catch or finally block or with for-in
 * loop.
 */
typedef enum JSTryNoteKind {
    JSTN_CATCH,
    JSTN_FINALLY,
    JSTN_ITER
} JSTryNoteKind;

/*
 * Exception handling record.
 */
struct JSTryNote {
    uint8           kind;       /* one of JSTryNoteKind */
    uint8           padding;    /* explicit padding on uint16 boundary */
    uint16          stackDepth; /* stack depth upon exception handler entry */
    uint32          start;      /* start of the try statement or for-in loop
                                   relative to script->main */
    uint32          length;     /* length of the try statement or for-in loop */
};

typedef struct JSTryNoteArray {
    JSTryNote       *vector;    /* array of indexed try notes */
    uint32          length;     /* count of indexded try notes */
} JSTryNoteArray;

typedef struct JSObjectArray {
    JSObject        **vector;   /* array of indexed objects */
    uint32          length;     /* count of indexded objects */
} JSObjectArray;

#define JS_OBJECT_ARRAY_SIZE(length)                                          \
    (offsetof(JSObjectArray, vector) + sizeof(JSObject *) * (length))

#if defined DEBUG && defined JS_THREADSAFE
# define CHECK_SCRIPT_OWNER 1
#endif

struct JSScript {
    jsbytecode      *code;      /* bytecodes and their immediate operands */
    uint32          length;     /* length of code vector */
    uint16          version;    /* JS version under which script was compiled */
    uint16          ngvars;     /* declared global var/const/function count */
    uint8           objectsOffset;  /* offset to the array of nested function,
                                       block, scope, xml and one-time regexps
                                       objects or 0 if none */
    uint8           regexpsOffset;  /* offset to the array of to-be-cloned
                                       regexps or 0 if none. */
    uint8           trynotesOffset; /* offset to the array of try notes or
                                       0 if none */
    jsbytecode      *main;      /* main entry point, after predef'ing prolog */
    JSAtomMap       atomMap;    /* maps immediate index to literal struct */
    const char      *filename;  /* source filename or null */
    uintN           lineno;     /* base line number of script */
    uintN           depth;      /* maximum stack depth in slots */
    JSPrincipals    *principals;/* principals for this script */
    JSObject        *object;    /* optional Script-class object wrapper */
#ifdef CHECK_SCRIPT_OWNER
    JSThread        *owner;     /* for thread-safe life-cycle assertions */
#endif
};

/* No need to store script->notes now that it is allocated right after code. */
#define SCRIPT_NOTES(script)    ((jssrcnote*)((script)->code+(script)->length))

#define JS_SCRIPT_OBJECTS(script)                                             \
    (JS_ASSERT((script)->objectsOffset != 0),                                 \
     (JSObjectArray *)((uint8 *)(script) + (script)->objectsOffset))

#define JS_SCRIPT_REGEXPS(script)                                             \
    (JS_ASSERT((script)->regexpsOffset != 0),                                 \
     (JSObjectArray *)((uint8 *)(script) + (script)->regexpsOffset))

#define JS_SCRIPT_TRYNOTES(script)                                            \
    (JS_ASSERT((script)->trynotesOffset != 0),                                \
     (JSTryNoteArray *)((uint8 *)(script) + (script)->trynotesOffset))

#define JS_GET_SCRIPT_ATOM(script, index, atom)                               \
    JS_BEGIN_MACRO                                                            \
        JSAtomMap *atoms_ = &(script)->atomMap;                               \
        JS_ASSERT((uint32)(index) < atoms_->length);                          \
        (atom) = atoms_->vector[(index)];                                     \
    JS_END_MACRO

#define JS_GET_SCRIPT_OBJECT(script, index, obj)                              \
    JS_BEGIN_MACRO                                                            \
        JSObjectArray *objects_ = JS_SCRIPT_OBJECTS(script);                  \
        JS_ASSERT((uint32)(index) < objects_->length);                        \
        (obj) = objects_->vector[(index)];                                    \
    JS_END_MACRO

#define JS_GET_SCRIPT_FUNCTION(script, index, fun)                            \
    JS_BEGIN_MACRO                                                            \
        JSObject *funobj_;                                                    \
                                                                              \
        JS_GET_SCRIPT_OBJECT(script, index, funobj_);                         \
        JS_ASSERT(HAS_FUNCTION_CLASS(funobj_));                               \
        JS_ASSERT(funobj_ == (JSObject *) STOBJ_GET_PRIVATE(funobj_));        \
        (fun) = (JSFunction *) funobj_;                                       \
        JS_ASSERT(FUN_INTERPRETED(fun));                                      \
    JS_END_MACRO

#define JS_GET_SCRIPT_REGEXP(script, index, obj)                              \
    JS_BEGIN_MACRO                                                            \
        JSObjectArray *regexps_ = JS_SCRIPT_REGEXPS(script);                  \
        JS_ASSERT((uint32)(index) < regexps_->length);                        \
        (obj) = regexps_->vector[(index)];                                    \
        JS_ASSERT(STOBJ_GET_CLASS(obj) == &js_RegExpClass);                   \
    JS_END_MACRO

/*
 * Check if pc is inside a try block that has finally code. GC calls this to
 * check if it is necessary to schedule generator.close() invocation for an
 * unreachable generator.
 */
JSBool
js_IsInsideTryWithFinally(JSScript *script, jsbytecode *pc);

extern JS_FRIEND_DATA(JSClass) js_ScriptClass;

extern JSObject *
js_InitScriptClass(JSContext *cx, JSObject *obj);

/*
 * On first new context in rt, initialize script runtime state, specifically
 * the script filename table and its lock.
 */
extern JSBool
js_InitRuntimeScriptState(JSRuntime *rt);

/*
 * On last context destroy for rt, if script filenames are all GC'd, free the
 * script filename table and its lock.
 */
extern void
js_FinishRuntimeScriptState(JSRuntime *rt);

/*
 * On JS_DestroyRuntime(rt), forcibly free script filename prefixes and any
 * script filename table entries that have not been GC'd, the latter using
 * js_FinishRuntimeScriptState.
 *
 * This allows script filename prefixes to outlive any context in rt.
 */
extern void
js_FreeRuntimeScriptState(JSRuntime *rt);

extern const char *
js_SaveScriptFilename(JSContext *cx, const char *filename);

extern const char *
js_SaveScriptFilenameRT(JSRuntime *rt, const char *filename, uint32 flags);

extern uint32
js_GetScriptFilenameFlags(const char *filename);

extern void
js_MarkScriptFilename(const char *filename);

extern void
js_MarkScriptFilenames(JSRuntime *rt, JSBool keepAtoms);

extern void
js_SweepScriptFilenames(JSRuntime *rt);

/*
 * Two successively less primitive ways to make a new JSScript.  The first
 * does *not* call a non-null cx->runtime->newScriptHook -- only the second,
 * js_NewScriptFromCG, calls this optional debugger hook.
 *
 * The js_NewScript function can't know whether the script it creates belongs
 * to a function, or is top-level or eval code, but the debugger wants access
 * to the newly made script's function, if any -- so callers of js_NewScript
 * are responsible for notifying the debugger after successfully creating any
 * kind (function or other) of new JSScript.
 */
extern JSScript *
js_NewScript(JSContext *cx, uint32 length, uint32 nsrcnotes, uint32 natoms,
             uint32 nobjects, uint32 nregexps, uint32 ntrynotes);

extern JSScript *
js_NewScriptFromCG(JSContext *cx, JSCodeGenerator *cg);

/*
 * New-script-hook calling is factored from js_NewScriptFromCG so that it
 * and callers of js_XDRScript can share this code.  In the case of callers
 * of js_XDRScript, the hook should be invoked only after successful decode
 * of any owning function (the fun parameter) or script object (null fun).
 */
extern JS_FRIEND_API(void)
js_CallNewScriptHook(JSContext *cx, JSScript *script, JSFunction *fun);

extern JS_FRIEND_API(void)
js_CallDestroyScriptHook(JSContext *cx, JSScript *script);

extern void
js_DestroyScript(JSContext *cx, JSScript *script);

extern void
js_TraceScript(JSTracer *trc, JSScript *script);

/*
 * To perturb as little code as possible, we introduce a js_GetSrcNote lookup
 * cache without adding an explicit cx parameter.  Thus js_GetSrcNote becomes
 * a macro that uses cx from its calls' lexical environments.
 */
#define js_GetSrcNote(script,pc) js_GetSrcNoteCached(cx, script, pc)

extern jssrcnote *
js_GetSrcNoteCached(JSContext *cx, JSScript *script, jsbytecode *pc);

/* XXX need cx to lock function objects declared by prolog bytecodes. */
extern uintN
js_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc);

extern jsbytecode *
js_LineNumberToPC(JSScript *script, uintN lineno);

extern JS_FRIEND_API(uintN)
js_GetScriptLineExtent(JSScript *script);

/*
 * If magic is non-null, js_XDRScript succeeds on magic number mismatch but
 * returns false in *magic; it reflects a match via a true *magic out param.
 * If magic is null, js_XDRScript returns false on bad magic number errors,
 * which it reports.
 *
 * NB: callers must call js_CallNewScriptHook after successful JSXDR_DECODE
 * and subsequent set-up of owning function or script object, if any.
 */
extern JSBool
js_XDRScript(JSXDRState *xdr, JSScript **scriptp, JSBool *magic);

JS_END_EXTERN_C

#endif /* jsscript_h___ */
