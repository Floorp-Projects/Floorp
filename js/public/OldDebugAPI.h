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
