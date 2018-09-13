/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Functions for compiling and evaluating scripts. */

#ifndef js_CompilationAndEvaluation_h
#define js_CompilationAndEvaluation_h

#include <stddef.h> // size_t
#include <stdio.h> // FILE

#include "jstypes.h" // JS_PUBLIC_API

#include "js/CompileOptions.h" // JS::CompileOptions, JS::ReadOnlyCompileOptions
#include "js/RootingAPI.h" // JS::Handle, JS::MutableHandle

struct JSContext;
class JSFunction;
class JSObject;
class JSScript;

namespace JS {

template<typename T> class AutoVector;

class SourceBufferHolder;
union Value;

} // namespace JS

/**
 * Given a buffer, return false if the buffer might become a valid JavaScript
 * script with the addition of more lines, or true if the validity of such a
 * script is conclusively known (because it's the prefix of a valid script --
 * and possibly the entirety of such a script).
 *
 * The intent of this function is to enable interactive compilation: accumulate
 * lines in a buffer until JS_Utf8BufferIsCompilableUnit is true, then pass it
 * to the compiler.
 *
 * The provided buffer is interpreted as UTF-8 data.  An error is reported if
 * a UTF-8 encoding error is encountered.
 */
extern JS_PUBLIC_API(bool)
JS_Utf8BufferIsCompilableUnit(JSContext* cx, JS::Handle<JSObject*> obj,
                              const char* utf8, size_t length);

/*
 * NB: JS_ExecuteScript and the JS::Evaluate APIs come in two flavors: either
 * they use the global as the scope, or they take an AutoObjectVector of objects
 * to use as the scope chain.  In the former case, the global is also used as
 * the "this" keyword value and the variables object (ECMA parlance for where
 * 'var' and 'function' bind names) of the execution context for script.  In the
 * latter case, the first object in the provided list is used, unless the list
 * is empty, in which case the global is used.
 *
 * Why a runtime option?  The alternative is to add APIs duplicating those
 * for the other value of flags, and that doesn't seem worth the code bloat
 * cost.  Such new entry points would probably have less obvious names, too, so
 * would not tend to be used.  The ContextOptionsRef adjustment, OTOH, can be
 * more easily hacked into existing code that does not depend on the bug; such
 * code can continue to use the familiar JS::Evaluate, etc., entry points.
 */

/**
 * Evaluate a script in the scope of the current global of cx.
 */
extern JS_PUBLIC_API(bool)
JS_ExecuteScript(JSContext* cx, JS::Handle<JSScript*> script, JS::MutableHandle<JS::Value> rval);

extern JS_PUBLIC_API(bool)
JS_ExecuteScript(JSContext* cx, JS::Handle<JSScript*> script);

/**
 * As above, but providing an explicit scope chain.  envChain must not include
 * the global object on it; that's implicit.  It needs to contain the other
 * objects that should end up on the script's scope chain.
 */
extern JS_PUBLIC_API(bool)
JS_ExecuteScript(JSContext* cx, JS::AutoVector<JSObject*>& envChain,
                 JS::Handle<JSScript*> script, JS::MutableHandle<JS::Value> rval);

extern JS_PUBLIC_API(bool)
JS_ExecuteScript(JSContext* cx, JS::AutoVector<JSObject*>& envChain, JS::Handle<JSScript*> script);

/**
 * |script| will always be set. On failure, it will be set to nullptr.
 */
extern JS_PUBLIC_API(bool)
JS_CompileScript(JSContext* cx, const char* bytes, size_t length,
                 const JS::CompileOptions& options,
                 JS::MutableHandle<JSScript*> script);

/**
 * |script| will always be set. On failure, it will be set to nullptr.
 */
extern JS_PUBLIC_API(bool)
JS_CompileUCScript(JSContext* cx, JS::SourceBufferHolder& srcBuf,
                   const JS::CompileOptions& options,
                   JS::MutableHandle<JSScript*> script);

namespace JS {

/**
 * Like the above, but handles a cross-compartment script. If the script is
 * cross-compartment, it is cloned into the current compartment before executing.
 */
extern JS_PUBLIC_API(bool)
CloneAndExecuteScript(JSContext* cx, Handle<JSScript*> script, MutableHandle<Value> rval);

/**
 * Like CloneAndExecuteScript above, but allows executing under a non-syntactic
 * environment chain.
 */
extern JS_PUBLIC_API(bool)
CloneAndExecuteScript(JSContext* cx, AutoVector<JSObject*>& envChain, Handle<JSScript*> script,
                      MutableHandle<Value> rval);

/**
 * Evaluate the given source buffer in the scope of the current global of cx.
 */
extern JS_PUBLIC_API(bool)
Evaluate(JSContext* cx, const ReadOnlyCompileOptions& options,
         SourceBufferHolder& srcBuf, MutableHandle<Value> rval);

/**
 * As above, but providing an explicit scope chain.  envChain must not include
 * the global object on it; that's implicit.  It needs to contain the other
 * objects that should end up on the script's scope chain.
 */
extern JS_PUBLIC_API(bool)
Evaluate(JSContext* cx, AutoVector<JSObject*>& envChain, const ReadOnlyCompileOptions& options,
         SourceBufferHolder& srcBuf, MutableHandle<Value> rval);

/**
 * Evaluate the given byte buffer in the scope of the current global of cx.
 */
extern JS_PUBLIC_API(bool)
Evaluate(JSContext* cx, const ReadOnlyCompileOptions& options,
         const char* bytes, size_t length, MutableHandle<Value> rval);

/**
 * Evaluate the given file in the scope of the current global of cx.
 */
extern JS_PUBLIC_API(bool)
Evaluate(JSContext* cx, const ReadOnlyCompileOptions& options,
         const char* filename, MutableHandle<Value> rval);

/**
 * |script| will always be set. On failure, it will be set to nullptr.
 */
extern JS_PUBLIC_API(bool)
Compile(JSContext* cx, const ReadOnlyCompileOptions& options,
        SourceBufferHolder& srcBuf, MutableHandle<JSScript*> script);

/**
 * Compile the provided UTF-8 data into a script.  If the data contains invalid
 * UTF-8, an error is reported.
 *
 * The |options.utf8| flag is asserted to be true.  At a future time this flag
 * will be removed and UTF-8-ness of source bytes will be entirely determined
 * by which compilation function is called.
 *
 * |script| is always set to the compiled script or to null in case of error.
 */
extern JS_PUBLIC_API(bool)
CompileUtf8(JSContext* cx, const ReadOnlyCompileOptions& options,
            const char* bytes, size_t length, MutableHandle<JSScript*> script);

/**
 * Compile the provided Latin-1 data (i.e. each byte directly corresponds to
 * the same Unicode code point) into a script.
 *
 * The |options.utf8| flag is asserted to be false.  At a future time this flag
 * will be removed and UTF-8-ness of source bytes will be entirely determined
 * by which compilation function is called.
 *
 * This function may eventually be removed, such that *only* bytes containing
 * UTF-8 source text may be directly compiled.  Avoid using it if you can.
 *
 * |script| is always set to the compiled script or to null in case of error.
 */
extern JS_PUBLIC_API(bool)
CompileLatin1(JSContext* cx, const ReadOnlyCompileOptions& options,
              const char* bytes, size_t length, MutableHandle<JSScript*> script);

/**
 * DEPRECATED
 *
 * Compile the provided bytes into a script.
 *
 * If |options.utf8|, the bytes are interpreted as UTF-8 data.  If the data
 * contains any malformed UTF-8, an error is reported.
 *
 * Otherwise they are interpreted as Latin-1, i.e. each byte directly
 * corresponds to the same Unicode code point.
 *
 * |script| is always set to the compiled script or to null in case of error.
 *
 * Do not use this API.  The JS::CompileOptions::utf8 flag that indicates how
 * to interpret |bytes| is currently being replaced by functions indicating an
 * exact expected encoding.  If you have byte data to compile, you should use
 * either JS::CompileUtf8 or JS::CompileLatin1, as appropriate.
 */
inline bool
Compile(JSContext* cx, const ReadOnlyCompileOptions& options,
        const char* bytes, size_t length, MutableHandle<JSScript*> script)
{
    return options.utf8
           ? CompileUtf8(cx, options, bytes, length, script)
           : CompileLatin1(cx, options, bytes, length, script);
}

/**
 * Compile the UTF-8 contents of the given file into a script.  If the contents
 * contain any malformed UTF-8, an error is reported.
 *
 * |script| is always set to the compiled script or to null in case of error.
 *
 * The |options.utf8| flag is asserted to be true.  At a future time this flag
 * will be removed.
 */
extern JS_PUBLIC_API(bool)
CompileUtf8File(JSContext* cx, const ReadOnlyCompileOptions& options,
                FILE* file, MutableHandle<JSScript*> script);

/**
 * Compile the UTF-8 contents of the file at the given path into a script.
 * (The path itself is in the system encoding, not [necessarily] UTF-8.)  If
 * the contents contain any malformed UTF-8, an error is reported.
 *
 * |script| is always set to the compiled script or to null in case of error.
 *
 * The |options.utf8| flag is asserted to be true.  At a future time this flag
 * will be removed.
 */
extern JS_PUBLIC_API(bool)
CompileUtf8Path(JSContext* cx, const ReadOnlyCompileOptions& options,
                const char* filename, MutableHandle<JSScript*> script);

extern JS_PUBLIC_API(bool)
CompileForNonSyntacticScope(JSContext* cx, const ReadOnlyCompileOptions& options,
                            SourceBufferHolder& srcBuf, MutableHandle<JSScript*> script);

/**
 * Compile the given Latin-1 data for non-syntactic scope.
 *
 * There is no way to compile UTF-8 data for non-syntactic scope because no
 * user currently needs it.  Such way could be added in the future if it's ever
 * needed.
 */
extern JS_PUBLIC_API(bool)
CompileLatin1ForNonSyntacticScope(JSContext* cx, const ReadOnlyCompileOptions& options,
                                  const char* bytes, size_t length,
                                  MutableHandle<JSScript*> script);

/**
 * Compile a function with envChain plus the global as its scope chain.
 * envChain must contain objects in the current compartment of cx.  The actual
 * scope chain used for the function will consist of With wrappers for those
 * objects, followed by the current global of the compartment cx is in.  This
 * global must not be explicitly included in the scope chain.
 */
extern JS_PUBLIC_API(bool)
CompileFunction(JSContext* cx, AutoVector<JSObject*>& envChain,
                const ReadOnlyCompileOptions& options,
                const char* name, unsigned nargs, const char* const* argnames,
                SourceBufferHolder& srcBuf, MutableHandle<JSFunction*> fun);

/**
 * Same as above, but taking a const char * for the function body.
 */
extern JS_PUBLIC_API(bool)
CompileFunction(JSContext* cx, AutoVector<JSObject*>& envChain,
                const ReadOnlyCompileOptions& options,
                const char* name, unsigned nargs, const char* const* argnames,
                const char* bytes, size_t length, MutableHandle<JSFunction*> fun);

/*
 * Associate an element wrapper and attribute name with a previously compiled
 * script, for debugging purposes. Calling this function is optional, but should
 * be done before script execution if it is required.
 */
extern JS_PUBLIC_API(bool)
InitScriptSourceElement(JSContext* cx, Handle<JSScript*> script,
                        Handle<JSObject*> element, Handle<JSString*> elementAttrName = nullptr);

/*
 * For a script compiled with the hideScriptFromDebugger option, expose the
 * script to the debugger by calling the debugger's onNewScript hook.
 */
extern JS_PUBLIC_API(void)
ExposeScriptToDebugger(JSContext* cx, Handle<JSScript*> script);

} /* namespace JS */

#endif /* js_CompilationAndEvaluation_h */
