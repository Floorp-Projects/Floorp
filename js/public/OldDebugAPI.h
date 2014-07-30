/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_OldDebugAPI_h
#define js_OldDebugAPI_h

/*
 * JS debugger API.
 */

#include "mozilla/NullPtr.h"

#include "jsapi.h"
#include "jsbytecode.h"

#include "js/CallArgs.h"
#include "js/TypeDecls.h"

class JSAtom;
struct JSFreeOp;

namespace js {
class InterpreterFrame;
class FrameIter;
class ScriptSource;
}


namespace JS {

extern JS_PUBLIC_API(char *)
FormatStackDump(JSContext *cx, char *buf, bool showArgs, bool showLocals, bool showThisProps);

} // namespace JS

# ifdef JS_DEBUG
JS_FRIEND_API(void) js_DumpValue(const JS::Value &val);
JS_FRIEND_API(void) js_DumpId(jsid id);
JS_FRIEND_API(void) js_DumpInterpreterFrame(JSContext *cx, js::InterpreterFrame *start = nullptr);
# endif

JS_FRIEND_API(void)
js_DumpBacktrace(JSContext *cx);

typedef enum JSTrapStatus {
    JSTRAP_ERROR,
    JSTRAP_CONTINUE,
    JSTRAP_RETURN,
    JSTRAP_THROW,
    JSTRAP_LIMIT
} JSTrapStatus;

extern JS_PUBLIC_API(JSString *)
JS_DecompileScript(JSContext *cx, JS::HandleScript script, const char *name, unsigned indent);

/*
 * Currently, we only support runtime-wide debugging. In the future, we should
 * be able to support compartment-wide debugging.
 */
extern JS_PUBLIC_API(void)
JS_SetRuntimeDebugMode(JSRuntime *rt, bool debug);

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
extern JS_PUBLIC_API(bool)
JS_GetDebugMode(JSContext *cx);

/*
 * Turn on/off debugging mode for all compartments. This returns false if any code
 * from any of the runtime's compartments is running or on the stack.
 */
JS_FRIEND_API(bool)
JS_SetDebugModeForAllCompartments(JSContext *cx, bool debug);

/*
 * Turn on/off debugging mode for a single compartment. This should only be
 * used when no code from this compartment is running or on the stack in any
 * thread.
 */
JS_FRIEND_API(bool)
JS_SetDebugModeForCompartment(JSContext *cx, JSCompartment *comp, bool debug);

/*
 * Turn on/off debugging mode for a context's compartment.
 */
JS_FRIEND_API(bool)
JS_SetDebugMode(JSContext *cx, bool debug);

/* Turn on single step mode. */
extern JS_PUBLIC_API(bool)
JS_SetSingleStepMode(JSContext *cx, JS::HandleScript script, bool singleStep);


/************************************************************************/

// Raw JSScript* because this needs to be callable from a signal handler.
extern JS_PUBLIC_API(unsigned)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc);

extern JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, unsigned lineno);

extern JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, JS::HandleFunction fun);


/************************************************************************/

extern JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSScript *script);

extern JS_PUBLIC_API(const jschar *)
JS_GetScriptSourceMap(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(unsigned)
JS_GetScriptBaseLineNumber(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(unsigned)
JS_GetScriptLineExtent(JSContext *cx, JSScript *script);


/************************************************************************/

/**
 * Add various profiling-related functions as properties of the given object.
 */
extern JS_PUBLIC_API(bool)
JS_DefineProfilingFunctions(JSContext *cx, JSObject *obj);

/* Defined in vm/Debugger.cpp. */
extern JS_PUBLIC_API(bool)
JS_DefineDebuggerObject(JSContext *cx, JS::HandleObject obj);

extern JS_PUBLIC_API(void)
JS_DumpPCCounts(JSContext *cx, JS::HandleScript script);

extern JS_PUBLIC_API(void)
JS_DumpCompartmentPCCounts(JSContext *cx);

#endif /* js_OldDebugAPI_h */
