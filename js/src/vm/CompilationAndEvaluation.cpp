/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Same-thread compilation and evaluation APIs. */

#include "js/CompilationAndEvaluation.h"

#include "mozilla/Maybe.h"      // mozilla::None, mozilla::Some
#include "mozilla/TextUtils.h"  // mozilla::IsAscii
#include "mozilla/Utf8.h"       // mozilla::Utf8Unit

#include <utility>  // std::move

#include "jsfriendapi.h"  // js::GetErrorMessage
#include "jstypes.h"      // JS_PUBLIC_API

#include "frontend/BytecodeCompilation.h"  // frontend::CompileGlobalScript
#include "frontend/FullParseHandler.h"     // frontend::FullParseHandler
#include "frontend/ParseContext.h"         // frontend::UsedNameTracker
#include "frontend/Parser.h"       // frontend::Parser, frontend::ParseGoal
#include "js/CharacterEncoding.h"  // JS::UTF8Chars, JS::UTF8CharsToNewTwoByteCharsZ
#include "js/RootingAPI.h"         // JS::Rooted
#include "js/SourceText.h"         // JS::SourceText
#include "js/TypeDecls.h"          // JS::HandleObject, JS::MutableHandleScript
#include "js/Utility.h"            // JS::UniqueTwoByteChars
#include "js/Value.h"              // JS::Value
#include "util/CompleteFile.h"     // js::FileContents, js::ReadCompleteFile
#include "util/StringBuffer.h"     // js::StringBuffer
#include "vm/Debugger.h"           // js::Debugger
#include "vm/EnvironmentObject.h"  // js::CreateNonSyntacticEnvironmentChain
#include "vm/Interpreter.h"        // js::Execute
#include "vm/JSContext.h"          // JSContext

#include "vm/JSContext-inl.h"  // JSContext::check

using mozilla::Utf8Unit;

using JS::CompileOptions;
using JS::HandleObject;
using JS::ReadOnlyCompileOptions;
using JS::SourceOwnership;
using JS::SourceText;
using JS::UniqueTwoByteChars;
using JS::UTF8Chars;
using JS::UTF8CharsToNewTwoByteCharsZ;

using namespace js;

JS_PUBLIC_API void JS::detail::ReportSourceTooLong(JSContext* cx) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_SOURCE_TOO_LONG);
}

template <typename Unit>
static bool CompileSourceBuffer(JSContext* cx,
                                const ReadOnlyCompileOptions& options,
                                SourceText<Unit>& srcBuf,
                                JS::MutableHandleScript script) {
  ScopeKind scopeKind =
      options.nonSyntacticScope ? ScopeKind::NonSyntactic : ScopeKind::Global;

  MOZ_ASSERT(!cx->zone()->isAtomsZone());
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  frontend::GlobalScriptInfo info(cx, options, scopeKind);
  script.set(frontend::CompileGlobalScript(info, srcBuf));
  return !!script;
}

static bool CompileUtf8(JSContext* cx, const ReadOnlyCompileOptions& options,
                        const char* bytes, size_t length,
                        JS::MutableHandleScript script) {
  auto chars = UniqueTwoByteChars(
      UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(bytes, length), &length).get());
  if (!chars) {
    return false;
  }

  SourceText<char16_t> source;
  if (!source.init(cx, std::move(chars), length)) {
    return false;
  }

  return CompileSourceBuffer(cx, options, source, script);
}

static bool CompileUtf8DontInflate(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   const char* bytes, size_t length,
                                   JS::MutableHandleScript script) {
  SourceText<Utf8Unit> source;
  if (!source.init(cx, bytes, length, SourceOwnership::Borrowed)) {
    return false;
  }

  return CompileSourceBuffer(cx, options, source, script);
}

bool JS::Compile(JSContext* cx, const ReadOnlyCompileOptions& options,
                 SourceText<char16_t>& srcBuf, JS::MutableHandleScript script) {
  return CompileSourceBuffer(cx, options, srcBuf, script);
}

bool JS::CompileDontInflate(JSContext* cx,
                            const ReadOnlyCompileOptions& options,
                            SourceText<Utf8Unit>& srcBuf,
                            JS::MutableHandleScript script) {
  return CompileSourceBuffer(cx, options, srcBuf, script);
}

bool JS::CompileUtf8(JSContext* cx, const ReadOnlyCompileOptions& options,
                     const char* bytes, size_t length,
                     JS::MutableHandleScript script) {
  return ::CompileUtf8(cx, options, bytes, length, script);
}

bool JS::CompileUtf8DontInflate(JSContext* cx,
                                const ReadOnlyCompileOptions& options,
                                const char* bytes, size_t length,
                                JS::MutableHandleScript script) {
  return ::CompileUtf8DontInflate(cx, options, bytes, length, script);
}

bool JS::CompileUtf8File(JSContext* cx, const ReadOnlyCompileOptions& options,
                         FILE* file, JS::MutableHandleScript script) {
  FileContents buffer(cx);
  if (!ReadCompleteFile(cx, file, buffer)) {
    return false;
  }

  return ::CompileUtf8(cx, options,
                       reinterpret_cast<const char*>(buffer.begin()),
                       buffer.length(), script);
}

bool JS::CompileUtf8FileDontInflate(JSContext* cx,
                                    const ReadOnlyCompileOptions& options,
                                    FILE* file,
                                    JS::MutableHandleScript script) {
  FileContents buffer(cx);
  if (!ReadCompleteFile(cx, file, buffer)) {
    return false;
  }

  return ::CompileUtf8DontInflate(cx, options,
                                  reinterpret_cast<const char*>(buffer.begin()),
                                  buffer.length(), script);
}

bool JS::CompileUtf8Path(JSContext* cx,
                         const ReadOnlyCompileOptions& optionsArg,
                         const char* filename, JS::MutableHandleScript script) {
  AutoFile file;
  if (!file.open(cx, filename)) {
    return false;
  }

  CompileOptions options(cx, optionsArg);
  options.setFileAndLine(filename, 1);
  return CompileUtf8File(cx, options, file.fp(), script);
}

bool JS::CompileForNonSyntacticScope(JSContext* cx,
                                     const ReadOnlyCompileOptions& optionsArg,
                                     SourceText<char16_t>& srcBuf,
                                     JS::MutableHandleScript script) {
  CompileOptions options(cx, optionsArg);
  options.setNonSyntacticScope(true);
  return CompileSourceBuffer(cx, options, srcBuf, script);
}

bool JS::CompileUtf8ForNonSyntacticScope(
    JSContext* cx, const ReadOnlyCompileOptions& optionsArg, const char* bytes,
    size_t length, JS::MutableHandleScript script) {
  CompileOptions options(cx, optionsArg);
  options.setNonSyntacticScope(true);

  return ::CompileUtf8(cx, options, bytes, length, script);
}

JS_PUBLIC_API bool JS_Utf8BufferIsCompilableUnit(JSContext* cx,
                                                 HandleObject obj,
                                                 const char* utf8,
                                                 size_t length) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(obj);

  cx->clearPendingException();

  JS::UniqueTwoByteChars chars{
      UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(utf8, length), &length).get()};
  if (!chars) {
    return true;
  }

  // Return true on any out-of-memory error or non-EOF-related syntax error, so
  // our caller doesn't try to collect more buffered source.
  bool result = true;

  using frontend::CreateScriptSourceObject;
  using frontend::FullParseHandler;
  using frontend::ParseGoal;
  using frontend::Parser;
  using frontend::UsedNameTracker;

  CompileOptions options(cx);
  UsedNameTracker usedNames(cx);

  Rooted<ScriptSourceObject*> sourceObject(cx);
  sourceObject = CreateScriptSourceObject(cx, options, mozilla::Nothing());
  if (!sourceObject) {
    return false;
  }

  JS::AutoSuppressWarningReporter suppressWarnings(cx);
  Parser<FullParseHandler, char16_t> parser(
      cx, cx->tempLifoAlloc(), options, chars.get(), length,
      /* foldConstants = */ true, usedNames, nullptr, nullptr, sourceObject,
      ParseGoal::Script);
  if (!parser.checkOptions() || !parser.parse()) {
    // We ran into an error. If it was because we ran out of source, we
    // return false so our caller knows to try to collect more buffered
    // source.
    if (parser.isUnexpectedEOF()) {
      result = false;
    }

    cx->clearPendingException();
  }

  return result;
}

/*
 * enclosingScope is a scope, if any (e.g. a WithScope).  If the scope is the
 * global scope, this must be null.
 *
 * enclosingEnv is an environment to use, if it's not the global.
 */
static bool CompileFunction(
    JSContext* cx, const ReadOnlyCompileOptions& optionsArg, HandleAtom name,
    bool isInvalidName, SourceText<char16_t>& srcBuf, uint32_t parameterListEnd,
    HandleObject enclosingEnv, HandleScope enclosingScope,
    MutableHandleFunction fun) {
  MOZ_ASSERT(!cx->zone()->isAtomsZone());
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(enclosingEnv);
  RootedAtom funAtom(cx);

  fun.set(NewScriptedFunction(cx, 0, JSFunction::INTERPRETED_NORMAL,
                              isInvalidName ? nullptr : name,
                              /* proto = */ nullptr, gc::AllocKind::FUNCTION,
                              TenuredObject, enclosingEnv));
  if (!fun) {
    return false;
  }

  // Make sure the static scope chain matches up when we have a
  // non-syntactic scope.
  MOZ_ASSERT_IF(!IsGlobalLexicalEnvironment(enclosingEnv),
                enclosingScope->hasOnChain(ScopeKind::NonSyntactic));

  if (!js::frontend::CompileStandaloneFunction(cx, fun, optionsArg, srcBuf,
                                               mozilla::Some(parameterListEnd),
                                               enclosingScope)) {
    return false;
  }

  // When function name is not valid identifier, generated function source
  // in srcBuf doesn't have function name.  Set it here.
  if (isInvalidName) {
    fun->setAtom(name);
  }

  return true;
}

static MOZ_MUST_USE bool BuildFunctionString(const char* name, size_t nameLen,
                                             unsigned nargs,
                                             const char* const* argnames,
                                             const SourceText<char16_t>& srcBuf,
                                             StringBuffer* out,
                                             uint32_t* parameterListEnd) {
  MOZ_ASSERT(out);
  MOZ_ASSERT(parameterListEnd);

  if (!out->ensureTwoByteChars()) {
    return false;
  }
  if (!out->append("function ")) {
    return false;
  }
  if (name) {
    if (!out->append(name, nameLen)) {
      return false;
    }
  }
  if (!out->append("(")) {
    return false;
  }
  for (unsigned i = 0; i < nargs; i++) {
    if (i != 0) {
      if (!out->append(", ")) {
        return false;
      }
    }
    if (!out->append(argnames[i], strlen(argnames[i]))) {
      return false;
    }
  }

  // Remember the position of ")".
  *parameterListEnd = out->length();
  MOZ_ASSERT(FunctionConstructorMedialSigils[0] == ')');

  if (!out->append(FunctionConstructorMedialSigils)) {
    return false;
  }
  if (!out->append(srcBuf.get(), srcBuf.length())) {
    return false;
  }
  if (!out->append(FunctionConstructorFinalBrace)) {
    return false;
  }

  return true;
}

JS_PUBLIC_API bool JS::CompileFunction(JSContext* cx,
                                       AutoObjectVector& envChain,
                                       const ReadOnlyCompileOptions& options,
                                       const char* name, unsigned nargs,
                                       const char* const* argnames,
                                       SourceText<char16_t>& srcBuf,
                                       MutableHandleFunction fun) {
  RootedObject env(cx);
  RootedScope scope(cx);
  if (!CreateNonSyntacticEnvironmentChain(cx, envChain, &env, &scope)) {
    return false;
  }

  size_t nameLen = 0;
  bool isInvalidName = false;
  RootedAtom nameAtom(cx);
  if (name) {
    nameLen = strlen(name);
    nameAtom = Atomize(cx, name, nameLen);
    if (!nameAtom) {
      return false;
    }

    // If name is not valid identifier
    if (!js::frontend::IsIdentifier(reinterpret_cast<const Latin1Char*>(name),
                                    nameLen)) {
      isInvalidName = true;
    }
  }

  uint32_t parameterListEnd;
  StringBuffer funStr(cx);
  if (!BuildFunctionString(isInvalidName ? nullptr : name, nameLen, nargs,
                           argnames, srcBuf, &funStr, &parameterListEnd)) {
    return false;
  }

  size_t newLen = funStr.length();
  UniqueTwoByteChars stolen(funStr.stealChars());
  if (!stolen) {
    return false;
  }

  SourceText<char16_t> newSrcBuf;
  if (!newSrcBuf.init(cx, std::move(stolen), newLen)) {
    return false;
  }

  return CompileFunction(cx, options, nameAtom, isInvalidName, newSrcBuf,
                         parameterListEnd, env, scope, fun);
}

JS_PUBLIC_API bool JS::CompileFunctionUtf8(
    JSContext* cx, AutoObjectVector& envChain,
    const ReadOnlyCompileOptions& options, const char* name, unsigned nargs,
    const char* const* argnames, const char* bytes, size_t length,
    MutableHandleFunction fun) {
  auto chars = UniqueTwoByteChars(
      UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(bytes, length), &length).get());
  if (!chars) {
    return false;
  }

  SourceText<char16_t> source;
  if (!source.init(cx, std::move(chars), length)) {
    return false;
  }

  return CompileFunction(cx, envChain, options, name, nargs, argnames, source,
                         fun);
}

JS_PUBLIC_API bool JS::InitScriptSourceElement(JSContext* cx,
                                               HandleScript script,
                                               HandleObject element,
                                               HandleString elementAttrName) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));

  RootedScriptSourceObject sso(
      cx, &script->sourceObject()->as<ScriptSourceObject>());
  return ScriptSourceObject::initElementProperties(cx, sso, element,
                                                   elementAttrName);
}

JS_PUBLIC_API void JS::ExposeScriptToDebugger(JSContext* cx,
                                              HandleScript script) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));

  MOZ_ASSERT(script->hideScriptFromDebugger());
  script->clearHideScriptFromDebugger();
  Debugger::onNewScript(cx, script);
}

MOZ_NEVER_INLINE static bool ExecuteScript(JSContext* cx, HandleObject scope,
                                           HandleScript script, Value* rval) {
  MOZ_ASSERT(!cx->zone()->isAtomsZone());
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(scope, script);
  MOZ_ASSERT_IF(!IsGlobalLexicalEnvironment(scope),
                script->hasNonSyntacticScope());
  return Execute(cx, script, *scope, rval);
}

static bool ExecuteScript(JSContext* cx, AutoObjectVector& envChain,
                          HandleScript scriptArg, Value* rval) {
  RootedObject env(cx);
  RootedScope dummy(cx);
  if (!CreateNonSyntacticEnvironmentChain(cx, envChain, &env, &dummy)) {
    return false;
  }

  RootedScript script(cx, scriptArg);
  if (!script->hasNonSyntacticScope() && !IsGlobalLexicalEnvironment(env)) {
    script = CloneGlobalScript(cx, ScopeKind::NonSyntactic, script);
    if (!script) {
      return false;
    }
    js::Debugger::onNewScript(cx, script);
  }

  return ExecuteScript(cx, env, script, rval);
}

MOZ_NEVER_INLINE JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                                     HandleScript scriptArg,
                                                     MutableHandleValue rval) {
  RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());
  return ExecuteScript(cx, globalLexical, scriptArg, rval.address());
}

MOZ_NEVER_INLINE JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                                     HandleScript scriptArg) {
  RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());
  return ExecuteScript(cx, globalLexical, scriptArg, nullptr);
}

MOZ_NEVER_INLINE JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                                     AutoObjectVector& envChain,
                                                     HandleScript scriptArg,
                                                     MutableHandleValue rval) {
  return ExecuteScript(cx, envChain, scriptArg, rval.address());
}

MOZ_NEVER_INLINE JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                                     AutoObjectVector& envChain,
                                                     HandleScript scriptArg) {
  return ExecuteScript(cx, envChain, scriptArg, nullptr);
}

JS_PUBLIC_API bool JS::CloneAndExecuteScript(JSContext* cx,
                                             HandleScript scriptArg,
                                             JS::MutableHandleValue rval) {
  CHECK_THREAD(cx);
  RootedScript script(cx, scriptArg);
  RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());
  if (script->realm() != cx->realm()) {
    script = CloneGlobalScript(cx, ScopeKind::Global, script);
    if (!script) {
      return false;
    }

    js::Debugger::onNewScript(cx, script);
  }
  return ExecuteScript(cx, globalLexical, script, rval.address());
}

JS_PUBLIC_API bool JS::CloneAndExecuteScript(JSContext* cx,
                                             JS::AutoObjectVector& envChain,
                                             HandleScript scriptArg,
                                             JS::MutableHandleValue rval) {
  CHECK_THREAD(cx);
  RootedScript script(cx, scriptArg);
  if (script->realm() != cx->realm()) {
    script = CloneGlobalScript(cx, ScopeKind::NonSyntactic, script);
    if (!script) {
      return false;
    }

    js::Debugger::onNewScript(cx, script);
  }
  return ExecuteScript(cx, envChain, script, rval.address());
}

static bool Evaluate(JSContext* cx, ScopeKind scopeKind, HandleObject env,
                     const ReadOnlyCompileOptions& optionsArg,
                     SourceText<char16_t>& srcBuf, MutableHandleValue rval) {
  CompileOptions options(cx, optionsArg);
  MOZ_ASSERT(!cx->zone()->isAtomsZone());
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(env);
  MOZ_ASSERT_IF(!IsGlobalLexicalEnvironment(env),
                scopeKind == ScopeKind::NonSyntactic);

  options.setIsRunOnce(true);

  RootedScript script(cx);
  {
    frontend::GlobalScriptInfo info(cx, options, scopeKind);
    script = frontend::CompileGlobalScript(info, srcBuf);
    if (!script) {
      return false;
    }
  }

  return Execute(cx, script, *env,
                 options.noScriptRval ? nullptr : rval.address());
}

static bool Evaluate(JSContext* cx, AutoObjectVector& envChain,
                     const ReadOnlyCompileOptions& optionsArg,
                     SourceText<char16_t>& srcBuf, MutableHandleValue rval) {
  RootedObject env(cx);
  RootedScope scope(cx);
  if (!CreateNonSyntacticEnvironmentChain(cx, envChain, &env, &scope)) {
    return false;
  }
  return ::Evaluate(cx, scope->kind(), env, optionsArg, srcBuf, rval);
}

extern JS_PUBLIC_API bool JS::EvaluateUtf8(
    JSContext* cx, const ReadOnlyCompileOptions& options, const char* bytes,
    size_t length, MutableHandle<Value> rval) {
  auto chars = UniqueTwoByteChars(
      UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(bytes, length), &length).get());
  if (!chars) {
    return false;
  }

  SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, std::move(chars), length)) {
    return false;
  }

  RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());
  return ::Evaluate(cx, ScopeKind::Global, globalLexical, options, srcBuf,
                    rval);
}

JS_PUBLIC_API bool JS::Evaluate(JSContext* cx,
                                const ReadOnlyCompileOptions& optionsArg,
                                SourceText<char16_t>& srcBuf,
                                MutableHandleValue rval) {
  RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());
  return ::Evaluate(cx, ScopeKind::Global, globalLexical, optionsArg, srcBuf,
                    rval);
}

JS_PUBLIC_API bool JS::Evaluate(JSContext* cx, AutoObjectVector& envChain,
                                const ReadOnlyCompileOptions& optionsArg,
                                SourceText<char16_t>& srcBuf,
                                MutableHandleValue rval) {
  return ::Evaluate(cx, envChain, optionsArg, srcBuf, rval);
}

JS_PUBLIC_API bool JS::EvaluateUtf8Path(
    JSContext* cx, const ReadOnlyCompileOptions& optionsArg,
    const char* filename, MutableHandleValue rval) {
  FileContents buffer(cx);
  {
    AutoFile file;
    if (!file.open(cx, filename) || !file.readAll(cx, buffer)) {
      return false;
    }
  }

  CompileOptions options(cx, optionsArg);
  options.setFileAndLine(filename, 1);

  return EvaluateUtf8(cx, options,
                      reinterpret_cast<const char*>(buffer.begin()),
                      buffer.length(), rval);
}
