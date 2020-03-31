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
#include "frontend/CompilationInfo.h"      // for frontened::CompilationInfo
#include "frontend/FullParseHandler.h"     // frontend::FullParseHandler
#include "frontend/ParseContext.h"         // frontend::UsedNameTracker
#include "frontend/Parser.h"       // frontend::Parser, frontend::ParseGoal
#include "js/CharacterEncoding.h"  // JS::UTF8Chars, JS::UTF8CharsToNewTwoByteCharsZ
#include "js/RootingAPI.h"         // JS::Rooted
#include "js/SourceText.h"         // JS::SourceText
#include "js/TypeDecls.h"          // JS::HandleObject, JS::MutableHandleScript
#include "js/Utility.h"            // js::MallocArena, JS::UniqueTwoByteChars
#include "js/Value.h"              // JS::Value
#include "util/CompleteFile.h"     // js::FileContents, js::ReadCompleteFile
#include "util/StringBuffer.h"     // js::StringBuffer
#include "vm/EnvironmentObject.h"  // js::CreateNonSyntacticEnvironmentChain
#include "vm/Interpreter.h"        // js::Execute
#include "vm/JSContext.h"          // JSContext

#include "debugger/DebugAPI-inl.h"  // js::DebugAPI
#include "vm/JSContext-inl.h"       // JSContext::check

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
static JSScript* CompileSourceBuffer(JSContext* cx,
                                     const ReadOnlyCompileOptions& options,
                                     SourceText<Unit>& srcBuf) {
  ScopeKind scopeKind =
      options.nonSyntacticScope ? ScopeKind::NonSyntactic : ScopeKind::Global;

  MOZ_ASSERT(!cx->zone()->isAtomsZone());
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  frontend::CompilationInfo compilationInfo(cx, allocScope, options);
  if (!compilationInfo.init(cx)) {
    return nullptr;
  }

  frontend::GlobalSharedContext globalsc(cx, scopeKind, compilationInfo,
                                         compilationInfo.directives);
  return frontend::CompileGlobalScript(compilationInfo, globalsc, srcBuf);
}

JSScript* JS::Compile(JSContext* cx, const ReadOnlyCompileOptions& options,
                      SourceText<char16_t>& srcBuf) {
  return CompileSourceBuffer(cx, options, srcBuf);
}

JSScript* JS::Compile(JSContext* cx, const ReadOnlyCompileOptions& options,
                      SourceText<Utf8Unit>& srcBuf) {
  return CompileSourceBuffer(cx, options, srcBuf);
}

JSScript* JS::CompileUtf8File(JSContext* cx,
                              const ReadOnlyCompileOptions& options,
                              FILE* file) {
  FileContents buffer(cx);
  if (!ReadCompleteFile(cx, file, buffer)) {
    return nullptr;
  }

  SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, reinterpret_cast<const char*>(buffer.begin()),
                   buffer.length(), SourceOwnership::Borrowed)) {
    return nullptr;
  }

  return CompileSourceBuffer(cx, options, srcBuf);
}

JSScript* JS::CompileUtf8Path(JSContext* cx,
                              const ReadOnlyCompileOptions& optionsArg,
                              const char* filename) {
  AutoFile file;
  if (!file.open(cx, filename)) {
    return nullptr;
  }

  CompileOptions options(cx, optionsArg);
  options.setFileAndLine(filename, 1);
  return CompileUtf8File(cx, options, file.fp());
}

JSScript* JS::CompileForNonSyntacticScope(
    JSContext* cx, const ReadOnlyCompileOptions& optionsArg,
    SourceText<char16_t>& srcBuf) {
  CompileOptions options(cx, optionsArg);
  options.setNonSyntacticScope(true);
  return CompileSourceBuffer(cx, options, srcBuf);
}

JSScript* JS::CompileForNonSyntacticScope(
    JSContext* cx, const ReadOnlyCompileOptions& optionsArg,
    SourceText<Utf8Unit>& srcBuf) {
  CompileOptions options(cx, optionsArg);
  options.setNonSyntacticScope(true);
  return CompileSourceBuffer(cx, options, srcBuf);
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
      UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(utf8, length), &length,
                                  js::MallocArena)
          .get()};
  if (!chars) {
    return true;
  }

  // Return true on any out-of-memory error or non-EOF-related syntax error, so
  // our caller doesn't try to collect more buffered source.
  bool result = true;

  using frontend::CompilationInfo;
  using frontend::FullParseHandler;
  using frontend::ParseGoal;
  using frontend::Parser;

  CompileOptions options(cx);
  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  CompilationInfo compilationInfo(cx, allocScope, options);
  if (!compilationInfo.init(cx)) {
    return false;
  }

  JS::AutoSuppressWarningReporter suppressWarnings(cx);
  Parser<FullParseHandler, char16_t> parser(cx, options, chars.get(), length,
                                            /* foldConstants = */ true,
                                            compilationInfo, nullptr, nullptr,
                                            compilationInfo.sourceObject);
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

class FunctionCompiler {
 private:
  JSContext* const cx_;
  RootedAtom nameAtom_;
  StringBuffer funStr_;

  uint32_t parameterListEnd_ = 0;
  bool nameIsIdentifier_ = true;

 public:
  explicit FunctionCompiler(JSContext* cx)
      : cx_(cx), nameAtom_(cx), funStr_(cx) {
    AssertHeapIsIdle();
    CHECK_THREAD(cx);
    MOZ_ASSERT(!cx->zone()->isAtomsZone());
  }

  MOZ_MUST_USE bool init(const char* name, unsigned nargs,
                         const char* const* argnames) {
    if (!funStr_.ensureTwoByteChars()) {
      return false;
    }
    if (!funStr_.append("function ")) {
      return false;
    }

    if (name) {
      size_t nameLen = strlen(name);

      nameAtom_ = Atomize(cx_, name, nameLen);
      if (!nameAtom_) {
        return false;
      }

      // If the name is an identifier, we can just add it to source text.
      // Otherwise we'll have to set it manually later.
      nameIsIdentifier_ = js::frontend::IsIdentifier(
          reinterpret_cast<const Latin1Char*>(name), nameLen);
      if (nameIsIdentifier_) {
        if (!funStr_.append(nameAtom_)) {
          return false;
        }
      }
    }

    if (!funStr_.append("(")) {
      return false;
    }

    for (unsigned i = 0; i < nargs; i++) {
      if (i != 0) {
        if (!funStr_.append(", ")) {
          return false;
        }
      }
      if (!funStr_.append(argnames[i], strlen(argnames[i]))) {
        return false;
      }
    }

    // Remember the position of ")".
    parameterListEnd_ = funStr_.length();
    MOZ_ASSERT(FunctionConstructorMedialSigils[0] == ')');

    return funStr_.append(FunctionConstructorMedialSigils);
  }

  template <typename Unit>
  inline MOZ_MUST_USE bool addFunctionBody(const SourceText<Unit>& srcBuf) {
    return funStr_.append(srcBuf.get(), srcBuf.length());
  }

  JSFunction* finish(HandleObjectVector envChain,
                     const ReadOnlyCompileOptions& options) {
    if (!funStr_.append(FunctionConstructorFinalBrace)) {
      return nullptr;
    }

    size_t newLen = funStr_.length();
    UniqueTwoByteChars stolen(funStr_.stealChars());
    if (!stolen) {
      return nullptr;
    }

    SourceText<char16_t> newSrcBuf;
    if (!newSrcBuf.init(cx_, std::move(stolen), newLen)) {
      return nullptr;
    }

    RootedObject enclosingEnv(cx_);
    RootedScope enclosingScope(cx_);
    if (!CreateNonSyntacticEnvironmentChain(cx_, envChain, &enclosingEnv,
                                            &enclosingScope)) {
      return nullptr;
    }

    cx_->check(enclosingEnv);

    RootedFunction fun(
        cx_,
        NewScriptedFunction(cx_, 0, FunctionFlags::INTERPRETED_NORMAL,
                            nameIsIdentifier_ ? HandleAtom(nameAtom_) : nullptr,
                            /* proto = */ nullptr, gc::AllocKind::FUNCTION,
                            TenuredObject, enclosingEnv));
    if (!fun) {
      return nullptr;
    }

    // Make sure the static scope chain matches up when we have a
    // non-syntactic scope.
    MOZ_ASSERT_IF(!IsGlobalLexicalEnvironment(enclosingEnv),
                  enclosingScope->hasOnChain(ScopeKind::NonSyntactic));

    if (!js::frontend::CompileStandaloneFunction(
            cx_, &fun, options, newSrcBuf, mozilla::Some(parameterListEnd_),
            enclosingScope)) {
      return nullptr;
    }

    // When the function name isn't a valid identifier, the generated function
    // source in srcBuf won't include the name, so name the function manually.
    if (!nameIsIdentifier_) {
      fun->setAtom(nameAtom_);
    }

    return fun;
  }
};

JS_PUBLIC_API JSFunction* JS::CompileFunction(
    JSContext* cx, HandleObjectVector envChain,
    const ReadOnlyCompileOptions& options, const char* name, unsigned nargs,
    const char* const* argnames, SourceText<char16_t>& srcBuf) {
  FunctionCompiler compiler(cx);
  if (!compiler.init(name, nargs, argnames) ||
      !compiler.addFunctionBody(srcBuf)) {
    return nullptr;
  }

  return compiler.finish(envChain, options);
}

JS_PUBLIC_API JSFunction* JS::CompileFunction(
    JSContext* cx, HandleObjectVector envChain,
    const ReadOnlyCompileOptions& options, const char* name, unsigned nargs,
    const char* const* argnames, SourceText<Utf8Unit>& srcBuf) {
  FunctionCompiler compiler(cx);
  if (!compiler.init(name, nargs, argnames) ||
      !compiler.addFunctionBody(srcBuf)) {
    return nullptr;
  }

  return compiler.finish(envChain, options);
}

JS_PUBLIC_API JSFunction* JS::CompileFunctionUtf8(
    JSContext* cx, HandleObjectVector envChain,
    const ReadOnlyCompileOptions& options, const char* name, unsigned nargs,
    const char* const* argnames, const char* bytes, size_t length) {
  SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, bytes, length, SourceOwnership::Borrowed)) {
    return nullptr;
  }

  return CompileFunction(cx, envChain, options, name, nargs, argnames, srcBuf);
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

  DebugAPI::onNewScript(cx, script);
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

static bool ExecuteScript(JSContext* cx, HandleObjectVector envChain,
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

MOZ_NEVER_INLINE JS_PUBLIC_API bool JS_ExecuteScript(
    JSContext* cx, HandleObjectVector envChain, HandleScript scriptArg,
    MutableHandleValue rval) {
  return ExecuteScript(cx, envChain, scriptArg, rval.address());
}

MOZ_NEVER_INLINE JS_PUBLIC_API bool JS_ExecuteScript(
    JSContext* cx, HandleObjectVector envChain, HandleScript scriptArg) {
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
  }
  return ExecuteScript(cx, globalLexical, script, rval.address());
}

JS_PUBLIC_API bool JS::CloneAndExecuteScript(JSContext* cx,
                                             JS::HandleObjectVector envChain,
                                             HandleScript scriptArg,
                                             JS::MutableHandleValue rval) {
  CHECK_THREAD(cx);
  RootedScript script(cx, scriptArg);
  if (script->realm() != cx->realm()) {
    script = CloneGlobalScript(cx, ScopeKind::NonSyntactic, script);
    if (!script) {
      return false;
    }
  }
  return ExecuteScript(cx, envChain, script, rval.address());
}

template <typename Unit>
static bool EvaluateSourceBuffer(JSContext* cx, ScopeKind scopeKind,
                                 Handle<JSObject*> env,
                                 const ReadOnlyCompileOptions& optionsArg,
                                 SourceText<Unit>& srcBuf,
                                 MutableHandle<Value> rval) {
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
    LifoAllocScope allocScope(&cx->tempLifoAlloc());
    frontend::CompilationInfo compilationInfo(cx, allocScope, options);
    if (!compilationInfo.init(cx)) {
      return false;
    }

    frontend::GlobalSharedContext globalsc(cx, scopeKind, compilationInfo,
                                           compilationInfo.directives);
    script = frontend::CompileGlobalScript(compilationInfo, globalsc, srcBuf);
    if (!script) {
      return false;
    }
  }

  return Execute(cx, script, *env,
                 options.noScriptRval ? nullptr : rval.address());
}

JS_PUBLIC_API bool JS::Evaluate(JSContext* cx,
                                const ReadOnlyCompileOptions& options,
                                SourceText<Utf8Unit>& srcBuf,
                                MutableHandle<Value> rval) {
  RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());

  size_t length = srcBuf.length();
  auto chars = UniqueTwoByteChars(
      UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(srcBuf.get(), length), &length,
                                  js::MallocArena)
          .get());
  if (!chars) {
    return false;
  }

  SourceText<char16_t> inflatedSrc;
  if (!inflatedSrc.init(cx, std::move(chars), length)) {
    return false;
  }

  return EvaluateSourceBuffer(cx, ScopeKind::Global, globalLexical, options,
                              inflatedSrc, rval);
}

JS_PUBLIC_API bool JS::EvaluateDontInflate(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<Utf8Unit>& srcBuf, MutableHandle<Value> rval) {
  RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());
  return EvaluateSourceBuffer(cx, ScopeKind::Global, globalLexical, options,
                              srcBuf, rval);
}

JS_PUBLIC_API bool JS::Evaluate(JSContext* cx,
                                const ReadOnlyCompileOptions& optionsArg,
                                SourceText<char16_t>& srcBuf,
                                MutableHandleValue rval) {
  RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());
  return EvaluateSourceBuffer(cx, ScopeKind::Global, globalLexical, optionsArg,
                              srcBuf, rval);
}

JS_PUBLIC_API bool JS::Evaluate(JSContext* cx, HandleObjectVector envChain,
                                const ReadOnlyCompileOptions& options,
                                SourceText<char16_t>& srcBuf,
                                MutableHandleValue rval) {
  RootedObject env(cx);
  RootedScope scope(cx);
  if (!CreateNonSyntacticEnvironmentChain(cx, envChain, &env, &scope)) {
    return false;
  }

  return EvaluateSourceBuffer(cx, scope->kind(), env, options, srcBuf, rval);
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

  auto contents = reinterpret_cast<const char*>(buffer.begin());
  size_t length = buffer.length();

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, contents, length, JS::SourceOwnership::Borrowed)) {
    return false;
  }

  return Evaluate(cx, options, srcBuf, rval);
}

JS_PUBLIC_API bool JS::EvaluateUtf8PathDontInflate(
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

  auto contents = reinterpret_cast<const char*>(buffer.begin());
  size_t length = buffer.length();

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, contents, length, JS::SourceOwnership::Borrowed)) {
    return false;
  }

  return EvaluateDontInflate(cx, options, srcBuf, rval);
}
