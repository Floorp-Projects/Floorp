/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Functions for compiling and evaluating scripts. */

#ifndef js_CompilationAndEvaluation_h
#define js_CompilationAndEvaluation_h

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include <stddef.h>  // size_t
#include <stdio.h>   // FILE

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/CompileOptions.h"  // JS::CompileOptions, JS::ReadOnlyCompileOptions
#include "js/RootingAPI.h"      // JS::Handle, JS::MutableHandle
#include "js/Value.h"  // JS::Value and specializations of JS::*Handle-related types

struct JSContext;
class JSFunction;
class JSObject;
class JSScript;

namespace JS {

template <typename UnitT>
class SourceText;

}  // namespace JS

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
extern JS_PUBLIC_API bool JS_Utf8BufferIsCompilableUnit(
    JSContext* cx, JS::Handle<JSObject*> obj, const char* utf8, size_t length);

/*
 * NB: JS_ExecuteScript and the JS::Evaluate APIs come in two flavors: either
 * they use the global as the scope, or they take a HandleValueVector of
 * objects to use as the scope chain.  In the former case, the global is also
 * used as the "this" keyword value and the variables object (ECMA parlance for
 * where 'var' and 'function' bind names) of the execution context for script.
 * In the latter case, the first object in the provided list is used, unless the
 * list is empty, in which case the global is used.
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
extern JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                           JS::Handle<JSScript*> script,
                                           JS::MutableHandle<JS::Value> rval);

extern JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                           JS::Handle<JSScript*> script);

/**
 * As above, but providing an explicit scope chain.  envChain must not include
 * the global object on it; that's implicit.  It needs to contain the other
 * objects that should end up on the script's scope chain.
 */
extern JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                           JS::HandleObjectVector envChain,
                                           JS::Handle<JSScript*> script,
                                           JS::MutableHandle<JS::Value> rval);

extern JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                           JS::HandleObjectVector envChain,
                                           JS::Handle<JSScript*> script);

namespace JS {

/**
 * Like the above, but handles a cross-compartment script. If the script is
 * cross-compartment, it is cloned into the current compartment before
 * executing.
 */
extern JS_PUBLIC_API bool CloneAndExecuteScript(JSContext* cx,
                                                Handle<JSScript*> script,
                                                MutableHandle<Value> rval);

/**
 * Like CloneAndExecuteScript above, but allows executing under a non-syntactic
 * environment chain.
 */
extern JS_PUBLIC_API bool CloneAndExecuteScript(JSContext* cx,
                                                HandleObjectVector envChain,
                                                Handle<JSScript*> script,
                                                MutableHandle<Value> rval);

/**
 * Evaluate the given source buffer in the scope of the current global of cx,
 * and return the completion value in |rval|.
 */
extern JS_PUBLIC_API bool Evaluate(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   SourceText<char16_t>& srcBuf,
                                   MutableHandle<Value> rval);

/**
 * As above, but providing an explicit scope chain.  envChain must not include
 * the global object on it; that's implicit.  It needs to contain the other
 * objects that should end up on the script's scope chain.
 */
extern JS_PUBLIC_API bool Evaluate(JSContext* cx, HandleObjectVector envChain,
                                   const ReadOnlyCompileOptions& options,
                                   SourceText<char16_t>& srcBuf,
                                   MutableHandle<Value> rval);

/**
 * Evaluate the provided UTF-8 data in the scope of the current global of |cx|,
 * and return the completion value in |rval|.  If the data contains invalid
 * UTF-8, an error is reported.
 */
extern JS_PUBLIC_API bool Evaluate(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   SourceText<mozilla::Utf8Unit>& srcBuf,
                                   MutableHandle<Value> rval);

/**
 * Evaluate the provided UTF-8 data in the scope of the current global of |cx|,
 * and return the completion value in |rval|.  If the data contains invalid
 * UTF-8, an error is reported.
 *
 * The "DontInflate" suffix and (semantically unobservable) don't-inflate
 * characteristic are temporary while bugs in UTF-8 compilation are ironed out.
 * In the long term |JS::Evaluate| for UTF-8 will just never inflate, and this
 * separate function will die.
 *
 * NOTE: UTF-8 compilation is currently experimental, and it's possible it has
 *       as-yet-undiscovered bugs that the UTF-16 compilation functions do not
 *       have.  Use only if you're willing to take a risk!
 */
extern JS_PUBLIC_API bool EvaluateDontInflate(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<mozilla::Utf8Unit>& srcBuf, MutableHandle<Value> rval);

/**
 * Evaluate the UTF-8 contents of the file at the given path, and return the
 * completion value in |rval|.  (The path itself is in the system encoding, not
 * [necessarily] UTF-8.)  If the contents contain any malformed UTF-8, an error
 * is reported.
 */
extern JS_PUBLIC_API bool EvaluateUtf8Path(
    JSContext* cx, const ReadOnlyCompileOptions& options, const char* filename,
    MutableHandle<Value> rval);

/**
 * Compile the provided script using the given options.  Return the script on
 * success, or return null on failure (usually with an error reported).
 */
extern JS_PUBLIC_API JSScript* Compile(JSContext* cx,
                                       const ReadOnlyCompileOptions& options,
                                       SourceText<char16_t>& srcBuf);

/**
 * Compile the provided script using the given options.  Return the script on
 * success, or return null on failure (usually with an error reported).
 */
extern JS_PUBLIC_API JSScript* Compile(JSContext* cx,
                                       const ReadOnlyCompileOptions& options,
                                       SourceText<mozilla::Utf8Unit>& srcBuf);

/**
 * Identical to |JS::Compile| for UTF-8, except this function directly parses
 * its UTF-8 input without inflating it to UTF-16 and parsing that.
 *
 * The "DontInflate" suffix and (semantically unobservable) don't-inflate
 * characteristic are temporary while bugs in UTF-8 compilation are ironed out.
 * In the long term |JS::Compile| for UTF-8 will just never inflate, and this
 * separate function will die.
 *
 * NOTE: UTF-8 compilation is currently experimental, and it's possible it has
 *       as-yet-undiscovered bugs that the UTF-16 compilation functions do not
 *       have.  Use only if you're willing to take a risk!
 */
extern JS_PUBLIC_API JSScript* CompileDontInflate(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<mozilla::Utf8Unit>& srcBuf);

/**
 * Compile the UTF-8 contents of the given file into a script.  It is an error
 * if the file contains invalid UTF-8.  Return the script on success, or return
 * null on failure (usually with an error reported).
 */
extern JS_PUBLIC_API JSScript* CompileUtf8File(
    JSContext* cx, const ReadOnlyCompileOptions& options, FILE* file);

/**
 * Compile the UTF-8 contents of the given file into a script.  It is an error
 * if the file contains invalid UTF-8.  Return the script on success, or return
 * null on failure (usually with an error reported).
 *
 * NOTE: UTF-8 compilation is currently experimental, and it's possible it has
 *       as-yet-undiscovered bugs not present in |JS::CompileUtf8File| that
 *       first inflates to UTF-16.  Use only if you're willing to take a risk!
 */
extern JS_PUBLIC_API JSScript* CompileUtf8FileDontInflate(
    JSContext* cx, const ReadOnlyCompileOptions& options, FILE* file);

/**
 * Compile the UTF-8 contents of the file at the given path into a script.
 * (The path itself is in the system encoding, not [necessarily] UTF-8.)  It
 * is an error if the file's contents are invalid UTF-8.  Return the script on
 * success, or return null on failure (usually with an error reported).
 */
extern JS_PUBLIC_API JSScript* CompileUtf8Path(
    JSContext* cx, const ReadOnlyCompileOptions& options, const char* filename);

extern JS_PUBLIC_API JSScript* CompileForNonSyntacticScope(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<char16_t>& srcBuf);

/**
 * Compile the provided UTF-8 data into a script in a non-syntactic scope.  It
 * is an error if the data contains invalid UTF-8.  Return the script on
 * success, or return null on failure (usually with an error reported).
 */
extern JS_PUBLIC_API JSScript* CompileForNonSyntacticScope(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<mozilla::Utf8Unit>& srcBuf);

/**
 * Compile the provided UTF-8 data into a script in a non-syntactic scope.  It
 * is an error if the data contains invalid UTF-8.  Return the script on
 * success, or return null on failure (usually with an error reported).
 *
 * The "DontInflate" suffix and (semantically unobservable) don't-inflate
 * characteristic are temporary while bugs in UTF-8 compilation are ironed out.
 * In the long term |JS::CompileForNonSyntacticScope| for UTF-8 will just never
 * inflate, and this separate function will die.
 *
 * NOTE: UTF-8 compilation is currently experimental, and it's possible it has
 *       as-yet-undiscovered bugs that the UTF-16 compilation functions do not
 *       have.  Use only if you're willing to take a risk!
 */
extern JS_PUBLIC_API JSScript* CompileForNonSyntacticScopeDontInflate(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<mozilla::Utf8Unit>& srcBuf);

/**
 * Compile a function with envChain plus the global as its scope chain.
 * envChain must contain objects in the current compartment of cx.  The actual
 * scope chain used for the function will consist of With wrappers for those
 * objects, followed by the current global of the compartment cx is in.  This
 * global must not be explicitly included in the scope chain.
 */
extern JS_PUBLIC_API JSFunction* CompileFunction(
    JSContext* cx, HandleObjectVector envChain,
    const ReadOnlyCompileOptions& options, const char* name, unsigned nargs,
    const char* const* argnames, SourceText<char16_t>& srcBuf);

/**
 * Compile a function with envChain plus the global as its scope chain.
 * envChain must contain objects in the current compartment of cx.  The actual
 * scope chain used for the function will consist of With wrappers for those
 * objects, followed by the current global of the compartment cx is in.  This
 * global must not be explicitly included in the scope chain.
 */
extern JS_PUBLIC_API JSFunction* CompileFunction(
    JSContext* cx, HandleObjectVector envChain,
    const ReadOnlyCompileOptions& options, const char* name, unsigned nargs,
    const char* const* argnames, SourceText<mozilla::Utf8Unit>& srcBuf);

/**
 * Identical to the CompileFunction overload above for UTF-8, but with
 * Rust-friendly ergonomics.
 */
extern JS_PUBLIC_API JSFunction* CompileFunctionUtf8(
    JSContext* cx, HandleObjectVector envChain,
    const ReadOnlyCompileOptions& options, const char* name, unsigned nargs,
    const char* const* argnames, const char* utf8, size_t length);

/*
 * Associate an element wrapper and attribute name with a previously compiled
 * script, for debugging purposes. Calling this function is optional, but should
 * be done before script execution if it is required.
 */
extern JS_PUBLIC_API bool InitScriptSourceElement(
    JSContext* cx, Handle<JSScript*> script, Handle<JSObject*> element,
    Handle<JSString*> elementAttrName = nullptr);

/*
 * For a script compiled with the hideScriptFromDebugger option, expose the
 * script to the debugger by calling the debugger's onNewScript hook.
 */
extern JS_PUBLIC_API void ExposeScriptToDebugger(JSContext* cx,
                                                 Handle<JSScript*> script);

} /* namespace JS */

#endif /* js_CompilationAndEvaluation_h */
