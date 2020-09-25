/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeCompiler.h"

#include "mozilla/Attributes.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Maybe.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "builtin/ModuleObject.h"
#include "frontend/BytecodeCompilation.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/EitherParser.h"
#include "frontend/ErrorReporter.h"
#include "frontend/FoldConstants.h"
#ifdef JS_ENABLE_SMOOSH
#  include "frontend/Frontend2.h"  // Smoosh
#endif
#include "frontend/ModuleSharedContext.h"
#include "frontend/Parser.h"
#include "js/SourceText.h"
#include "vm/FunctionFlags.h"          // FunctionFlags
#include "vm/GeneratorAndAsyncKind.h"  // js::GeneratorKind, js::FunctionAsyncKind
#include "vm/GlobalObject.h"
#include "vm/HelperThreadState.h"  // ParseTask
#include "vm/JSContext.h"
#include "vm/JSScript.h"
#include "vm/ModuleBuilder.h"  // js::ModuleBuilder
#include "vm/TraceLogging.h"
#include "wasm/AsmJS.h"

#include "debugger/DebugAPI-inl.h"  // DebugAPI
#include "vm/EnvironmentObject-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/JSContext-inl.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Utf8Unit;

using JS::CompileOptions;
using JS::ReadOnlyCompileOptions;
using JS::SourceText;

// RAII class to check the frontend reports an exception when it fails to
// compile a script.
class MOZ_RAII AutoAssertReportedException {
#ifdef DEBUG
  JSContext* cx_;
  bool check_;

 public:
  explicit AutoAssertReportedException(JSContext* cx) : cx_(cx), check_(true) {}
  void reset() { check_ = false; }
  ~AutoAssertReportedException() {
    if (!check_) {
      return;
    }

    if (!cx_->isHelperThreadContext()) {
      MOZ_ASSERT(cx_->isExceptionPending());
      return;
    }

    ParseTask* task = cx_->parseTask();
    MOZ_ASSERT(task->outOfMemory || task->overRecursed ||
               !task->errors.empty());
  }
#else
 public:
  explicit AutoAssertReportedException(JSContext*) {}
  void reset() {}
#endif
};

static bool EmplaceEmitter(CompilationInfo& compilationInfo,
                           CompilationState& compilationState,
                           Maybe<BytecodeEmitter>& emitter,
                           const EitherParser& parser, SharedContext* sc);

template <typename Unit>
class MOZ_STACK_CLASS frontend::SourceAwareCompiler {
 protected:
  SourceText<Unit>& sourceBuffer_;

  frontend::CompilationState compilationState_;

  Maybe<Parser<SyntaxParseHandler, Unit>> syntaxParser;
  Maybe<Parser<FullParseHandler, Unit>> parser;

  using TokenStreamPosition = frontend::TokenStreamPosition<Unit>;

 protected:
  explicit SourceAwareCompiler(JSContext* cx, LifoAllocScope& allocScope,
                               const JS::ReadOnlyCompileOptions& options,
                               SourceText<Unit>& sourceBuffer,
                               js::Scope* enclosingScope = nullptr,
                               JSObject* enclosingEnv = nullptr)
      : sourceBuffer_(sourceBuffer),
        compilationState_(cx, allocScope, options, enclosingScope,
                          enclosingEnv) {
    MOZ_ASSERT(sourceBuffer_.get() != nullptr);
  }

  // Call this before calling compile{Global,Eval}Script.
  MOZ_MUST_USE bool createSourceAndParser(JSContext* cx,
                                          CompilationInfo& compilationInfo);

  void assertSourceAndParserCreated(CompilationInput& compilationInput) const {
    MOZ_ASSERT(compilationInput.source() != nullptr);
    MOZ_ASSERT(parser.isSome());
  }

  void assertSourceParserAndScriptCreated(CompilationInput& compilationInput) {
    assertSourceAndParserCreated(compilationInput);
  }

  MOZ_MUST_USE bool emplaceEmitter(CompilationInfo& compilationInfo,
                                   Maybe<BytecodeEmitter>& emitter,
                                   SharedContext* sharedContext) {
    return EmplaceEmitter(compilationInfo, compilationState_, emitter,
                          EitherParser(parser.ptr()), sharedContext);
  }

  bool canHandleParseFailure(const Directives& newDirectives);

  void handleParseFailure(CompilationInfo& compilationInfo,
                          const Directives& newDirectives,
                          TokenStreamPosition& startPosition,
                          CompilationInfo::RewindToken& startObj);

 public:
  frontend::CompilationState& compilationState() { return compilationState_; };
};

template <typename Unit>
class MOZ_STACK_CLASS frontend::ScriptCompiler
    : public SourceAwareCompiler<Unit> {
  using Base = SourceAwareCompiler<Unit>;

 protected:
  using Base::compilationState_;
  using Base::parser;
  using Base::sourceBuffer_;

  using Base::assertSourceParserAndScriptCreated;
  using Base::canHandleParseFailure;
  using Base::emplaceEmitter;
  using Base::handleParseFailure;

  using typename Base::TokenStreamPosition;

 public:
  explicit ScriptCompiler(JSContext* cx, LifoAllocScope& allocScope,
                          const JS::ReadOnlyCompileOptions& options,
                          SourceText<Unit>& sourceBuffer,
                          js::Scope* enclosingScope = nullptr,
                          JSObject* enclosingEnv = nullptr)
      : Base(cx, allocScope, options, sourceBuffer, enclosingScope,
             enclosingEnv) {}

  using Base::createSourceAndParser;

  bool compileScriptToStencil(JSContext* cx, CompilationInfo& compilationInfo,
                              SharedContext* sc);
};

/* If we're on main thread, tell the Debugger about a newly compiled script.
 *
 * See: finishSingleParseTask/finishMultiParseTask for the off-thread case.
 */
static void tellDebuggerAboutCompiledScript(JSContext* cx, bool hideScript,
                                            Handle<JSScript*> script) {
  if (cx->isHelperThreadContext()) {
    return;
  }

  // If hideScript then script may not be ready to be interrogated by the
  // debugger.
  if (!hideScript) {
    DebugAPI::onNewScript(cx, script);
  }
}

#ifdef JS_ENABLE_SMOOSH
bool TrySmoosh(JSContext* cx, CompilationInfo& compilationInfo,
               JS::SourceText<Utf8Unit>& srcBuf, bool* fallback) {
  if (!cx->options().trySmoosh()) {
    *fallback = true;
    return true;
  }

  bool unimplemented = false;
  JSRuntime* rt = cx->runtime();
  bool result = Smoosh::compileGlobalScriptToStencil(cx, compilationInfo,
                                                     srcBuf, &unimplemented);
  if (!unimplemented) {
    *fallback = false;

    if (!compilationInfo.input.assignSource(cx, srcBuf)) {
      return false;
    }

    if (cx->options().trackNotImplemented()) {
      rt->parserWatcherFile.put("1");
    }
    return result;
  }
  *fallback = true;

  if (cx->options().trackNotImplemented()) {
    rt->parserWatcherFile.put("0");
  }
  fprintf(stderr, "Falling back!\n");

  return true;
}

bool TrySmoosh(JSContext* cx, CompilationInfo& compilationInfo,
               JS::SourceText<char16_t>& srcBuf, bool* fallback) {
  *fallback = true;
  return true;
}
#endif  // JS_ENABLE_SMOOSH

template <typename Unit>
static bool CompileGlobalScriptToStencilImpl(JSContext* cx,
                                             CompilationInfo& compilationInfo,
                                             JS::SourceText<Unit>& srcBuf,
                                             ScopeKind scopeKind) {
#ifdef JS_ENABLE_SMOOSH
  bool fallback = false;
  if (!TrySmoosh(cx, compilationInfo, srcBuf, &fallback)) {
    return false;
  }
  if (!fallback) {
    return true;
  }
#endif  // JS_ENABLE_SMOOSH

  AutoAssertReportedException assertException(cx);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  frontend::ScriptCompiler<Unit> compiler(
      cx, allocScope, compilationInfo.input.options, srcBuf);

  if (!compiler.createSourceAndParser(cx, compilationInfo)) {
    return false;
  }

  SourceExtent extent = SourceExtent::makeGlobalExtent(
      srcBuf.length(), compilationInfo.input.options.lineno,
      compilationInfo.input.options.column);
  frontend::GlobalSharedContext globalsc(cx, scopeKind, compilationInfo,
                                         compiler.compilationState().directives,
                                         extent);

  if (!compiler.compileScriptToStencil(cx, compilationInfo, &globalsc)) {
    return false;
  }

  assertException.reset();
  return true;
}

bool frontend::CompileGlobalScriptToStencil(JSContext* cx,
                                            CompilationInfo& compilationInfo,
                                            JS::SourceText<char16_t>& srcBuf,
                                            ScopeKind scopeKind) {
  return CompileGlobalScriptToStencilImpl(cx, compilationInfo, srcBuf,
                                          scopeKind);
}

bool frontend::CompileGlobalScriptToStencil(JSContext* cx,
                                            CompilationInfo& compilationInfo,
                                            JS::SourceText<Utf8Unit>& srcBuf,
                                            ScopeKind scopeKind) {
  return CompileGlobalScriptToStencilImpl(cx, compilationInfo, srcBuf,
                                          scopeKind);
}

template <typename Unit>
static UniquePtr<CompilationInfo> CompileGlobalScriptToStencilImpl(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Unit>& srcBuf, ScopeKind scopeKind) {
  Rooted<UniquePtr<frontend::CompilationInfo>> compilationInfo(
      cx, js_new<frontend::CompilationInfo>(cx, options));
  if (!compilationInfo) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  if (!compilationInfo.get()->input.initForGlobal(cx)) {
    return nullptr;
  }

  if (!CompileGlobalScriptToStencil(cx, *compilationInfo, srcBuf, scopeKind)) {
    return nullptr;
  }

  return std::move(compilationInfo.get());
}

UniquePtr<CompilationInfo> frontend::CompileGlobalScriptToStencil(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, ScopeKind scopeKind) {
  return CompileGlobalScriptToStencilImpl(cx, options, srcBuf, scopeKind);
}

UniquePtr<CompilationInfo> frontend::CompileGlobalScriptToStencil(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Utf8Unit>& srcBuf, ScopeKind scopeKind) {
  return CompileGlobalScriptToStencilImpl(cx, options, srcBuf, scopeKind);
}

bool frontend::InstantiateStencils(JSContext* cx,
                                   CompilationInfo& compilationInfo,
                                   CompilationGCOutput& gcOutput) {
  {
    AutoGeckoProfilerEntry pseudoFrame(cx, "stencil instantiate",
                                       JS::ProfilingCategoryPair::JS_Parsing);

    if (!compilationInfo.instantiateStencils(cx, gcOutput)) {
      return false;
    }
  }

  // Enqueue an off-thread source compression task after finishing parsing.
  if (!cx->isHelperThreadContext()) {
    if (!compilationInfo.input.source()->tryCompressOffThread(cx)) {
      return false;
    }
  }

  tellDebuggerAboutCompiledScript(
      cx, compilationInfo.input.options.hideScriptFromDebugger,
      gcOutput.script);

  return true;
}

template <typename Unit>
static JSScript* CompileGlobalScriptImpl(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<Unit>& srcBuf, ScopeKind scopeKind) {
  Rooted<CompilationInfo> compilationInfo(cx, CompilationInfo(cx, options));
  if (options.selfHostingMode) {
    if (!compilationInfo.get().input.initForSelfHostingGlobal(cx)) {
      return nullptr;
    }
  } else {
    if (!compilationInfo.get().input.initForGlobal(cx)) {
      return nullptr;
    }
  }

  if (!CompileGlobalScriptToStencil(cx, compilationInfo.get(), srcBuf,
                                    scopeKind)) {
    return nullptr;
  }

  frontend::CompilationGCOutput gcOutput(cx);
  if (!InstantiateStencils(cx, compilationInfo.get(), gcOutput)) {
    return nullptr;
  }

  return gcOutput.script;
}

JSScript* frontend::CompileGlobalScript(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, ScopeKind scopeKind) {
  return CompileGlobalScriptImpl(cx, options, srcBuf, scopeKind);
}

JSScript* frontend::CompileGlobalScript(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<Utf8Unit>& srcBuf, ScopeKind scopeKind) {
  return CompileGlobalScriptImpl(cx, options, srcBuf, scopeKind);
}

template <typename Unit>
static JSScript* CompileEvalScriptImpl(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    SourceText<Unit>& srcBuf, JS::Handle<js::Scope*> enclosingScope,
    JS::Handle<JSObject*> enclosingEnv) {
  AutoAssertReportedException assertException(cx);

  Rooted<CompilationInfo> compilationInfo(cx, CompilationInfo(cx, options));
  if (!compilationInfo.get().input.initForEval(cx, enclosingScope)) {
    return nullptr;
  }

  LifoAllocScope allocScope(&cx->tempLifoAlloc());

  frontend::ScriptCompiler<Unit> compiler(cx, allocScope,
                                          compilationInfo.get().input.options,
                                          srcBuf, enclosingScope, enclosingEnv);
  if (!compiler.createSourceAndParser(cx, compilationInfo.get())) {
    return nullptr;
  }

  uint32_t len = srcBuf.length();
  SourceExtent extent = SourceExtent::makeGlobalExtent(
      len, compilationInfo.get().input.options.lineno,
      compilationInfo.get().input.options.column);
  frontend::EvalSharedContext evalsc(cx, compilationInfo.get(),
                                     compiler.compilationState(), extent);
  if (!compiler.compileScriptToStencil(cx, compilationInfo.get(), &evalsc)) {
    return nullptr;
  }

  frontend::CompilationGCOutput gcOutput(cx);
  if (!InstantiateStencils(cx, compilationInfo.get(), gcOutput)) {
    return nullptr;
  }

  assertException.reset();
  return gcOutput.script;
}

JSScript* frontend::CompileEvalScript(JSContext* cx,
                                      const JS::ReadOnlyCompileOptions& options,
                                      JS::SourceText<char16_t>& srcBuf,
                                      JS::Handle<js::Scope*> enclosingScope,
                                      JS::Handle<JSObject*> enclosingEnv) {
  return CompileEvalScriptImpl(cx, options, srcBuf, enclosingScope,
                               enclosingEnv);
}

template <typename Unit>
class MOZ_STACK_CLASS frontend::ModuleCompiler final
    : public SourceAwareCompiler<Unit> {
  using Base = SourceAwareCompiler<Unit>;

  using Base::assertSourceParserAndScriptCreated;
  using Base::compilationState_;
  using Base::createSourceAndParser;
  using Base::emplaceEmitter;
  using Base::parser;

 public:
  explicit ModuleCompiler(JSContext* cx, LifoAllocScope& allocScope,
                          const JS::ReadOnlyCompileOptions& options,
                          SourceText<Unit>& sourceBuffer,
                          js::Scope* enclosingScope = nullptr,
                          JSObject* enclosingEnv = nullptr)
      : Base(cx, allocScope, options, sourceBuffer, enclosingScope,
             enclosingEnv) {}

  bool compile(JSContext* cx, CompilationInfo& compilationInfo);
};

template <typename Unit>
class MOZ_STACK_CLASS frontend::StandaloneFunctionCompiler final
    : public SourceAwareCompiler<Unit> {
  using Base = SourceAwareCompiler<Unit>;

  using Base::assertSourceAndParserCreated;
  using Base::canHandleParseFailure;
  using Base::compilationState_;
  using Base::emplaceEmitter;
  using Base::handleParseFailure;
  using Base::parser;
  using Base::sourceBuffer_;

  using typename Base::TokenStreamPosition;

 public:
  explicit StandaloneFunctionCompiler(JSContext* cx, LifoAllocScope& allocScope,
                                      const JS::ReadOnlyCompileOptions& options,
                                      SourceText<Unit>& sourceBuffer,
                                      js::Scope* enclosingScope = nullptr,
                                      JSObject* enclosingEnv = nullptr)
      : Base(cx, allocScope, options, sourceBuffer, enclosingScope,
             enclosingEnv) {}

  using Base::createSourceAndParser;

  FunctionNode* parse(JSContext* cx, CompilationInfo& compilationInfo,
                      FunctionSyntaxKind syntaxKind,
                      GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                      const Maybe<uint32_t>& parameterListEnd);

  bool compile(JSContext* cx, CompilationInfo& compilationInfo,
               FunctionNode* parsedFunction, CompilationGCOutput& gcOutput);
};

AutoFrontendTraceLog::AutoFrontendTraceLog(JSContext* cx,
                                           const TraceLoggerTextId id,
                                           const ErrorReporter& errorReporter)
#ifdef JS_TRACE_LOGGING
    : logger_(TraceLoggerForCurrentThread(cx)) {
  if (!logger_) {
    return;
  }

  // If the tokenizer hasn't yet gotten any tokens, use the line and column
  // numbers from CompileOptions.
  uint32_t line, column;
  if (errorReporter.hasTokenizationStarted()) {
    line = errorReporter.options().lineno;
    column = errorReporter.options().column;
  } else {
    errorReporter.currentLineAndColumn(&line, &column);
  }
  frontendEvent_.emplace(TraceLogger_Frontend, errorReporter.getFilename(),
                         line, column);
  frontendLog_.emplace(logger_, *frontendEvent_);
  typeLog_.emplace(logger_, id);
}
#else
{
}
#endif

AutoFrontendTraceLog::AutoFrontendTraceLog(JSContext* cx,
                                           const TraceLoggerTextId id,
                                           const ErrorReporter& errorReporter,
                                           FunctionBox* funbox)
#ifdef JS_TRACE_LOGGING
    : logger_(TraceLoggerForCurrentThread(cx)) {
  if (!logger_) {
    return;
  }

  frontendEvent_.emplace(TraceLogger_Frontend, errorReporter.getFilename(),
                         funbox->extent().lineno, funbox->extent().column);
  frontendLog_.emplace(logger_, *frontendEvent_);
  typeLog_.emplace(logger_, id);
}
#else
{
}
#endif

AutoFrontendTraceLog::AutoFrontendTraceLog(JSContext* cx,
                                           const TraceLoggerTextId id,
                                           const ErrorReporter& errorReporter,
                                           ParseNode* pn)
#ifdef JS_TRACE_LOGGING
    : logger_(TraceLoggerForCurrentThread(cx)) {
  if (!logger_) {
    return;
  }

  uint32_t line, column;
  errorReporter.lineAndColumnAt(pn->pn_pos.begin, &line, &column);
  frontendEvent_.emplace(TraceLogger_Frontend, errorReporter.getFilename(),
                         line, column);
  frontendLog_.emplace(logger_, *frontendEvent_);
  typeLog_.emplace(logger_, id);
}
#else
{
}
#endif

static bool CanLazilyParse(const CompilationInfo& compilationInfo) {
  return !compilationInfo.input.options.discardSource &&
         !compilationInfo.input.options.sourceIsLazy &&
         !compilationInfo.input.options.forceFullParse();
}

template <typename Unit>
bool frontend::SourceAwareCompiler<Unit>::createSourceAndParser(
    JSContext* cx, CompilationInfo& compilationInfo) {
  if (!compilationInfo.input.assignSource(cx, sourceBuffer_)) {
    return false;
  }

  if (CanLazilyParse(compilationInfo)) {
    syntaxParser.emplace(cx, compilationInfo.input.options,
                         sourceBuffer_.units(), sourceBuffer_.length(),
                         /* foldConstants = */ false, compilationInfo,
                         compilationState_, nullptr, nullptr);
    if (!syntaxParser->checkOptions()) {
      return false;
    }
  }

  parser.emplace(cx, compilationInfo.input.options, sourceBuffer_.units(),
                 sourceBuffer_.length(),
                 /* foldConstants = */ true, compilationInfo, compilationState_,
                 syntaxParser.ptrOr(nullptr), nullptr);
  parser->ss = compilationInfo.input.source();
  return parser->checkOptions();
}

static bool EmplaceEmitter(CompilationInfo& compilationInfo,
                           CompilationState& compilationState,
                           Maybe<BytecodeEmitter>& emitter,
                           const EitherParser& parser, SharedContext* sc) {
  BytecodeEmitter::EmitterMode emitterMode =
      sc->selfHosted() ? BytecodeEmitter::SelfHosting : BytecodeEmitter::Normal;
  emitter.emplace(/* parent = */ nullptr, parser, sc, compilationInfo,
                  compilationState, emitterMode);
  return emitter->init();
}

template <typename Unit>
bool frontend::SourceAwareCompiler<Unit>::canHandleParseFailure(
    const Directives& newDirectives) {
  // Try to reparse if no parse errors were thrown and the directives changed.
  //
  // NOTE:
  // Only the following two directive changes force us to reparse the script:
  // - The "use asm" directive was encountered.
  // - The "use strict" directive was encountered and duplicate parameter names
  //   are present. We reparse in this case to display the error at the correct
  //   source location. See |Parser::hasValidSimpleStrictParameterNames()|.
  return !parser->anyChars.hadError() &&
         compilationState_.directives != newDirectives;
}

template <typename Unit>
void frontend::SourceAwareCompiler<Unit>::handleParseFailure(
    CompilationInfo& compilationInfo, const Directives& newDirectives,
    TokenStreamPosition& startPosition,
    CompilationInfo::RewindToken& startObj) {
  MOZ_ASSERT(canHandleParseFailure(newDirectives));

  // Rewind to starting position to retry.
  parser->tokenStream.rewind(startPosition);
  compilationInfo.rewind(startObj);

  // Assignment must be monotonic to prevent reparsing iloops
  MOZ_ASSERT_IF(compilationState_.directives.strict(), newDirectives.strict());
  MOZ_ASSERT_IF(compilationState_.directives.asmJS(), newDirectives.asmJS());
  compilationState_.directives = newDirectives;
}

template <typename Unit>
bool frontend::ScriptCompiler<Unit>::compileScriptToStencil(
    JSContext* cx, CompilationInfo& compilationInfo, SharedContext* sc) {
  assertSourceParserAndScriptCreated(compilationInfo.input);

  TokenStreamPosition startPosition(parser->tokenStream);

  // Emplace the topLevel stencil
  MOZ_ASSERT(compilationInfo.stencil.scriptData.length() ==
             CompilationInfo::TopLevelIndex);
  if (!compilationInfo.stencil.scriptData.emplaceBack()) {
    ReportOutOfMemory(cx);
    return false;
  }

  ParseNode* pn;
  {
    AutoGeckoProfilerEntry pseudoFrame(cx, "script parsing",
                                       JS::ProfilingCategoryPair::JS_Parsing);
    if (sc->isEvalContext()) {
      pn = parser->evalBody(sc->asEvalContext());
    } else {
      pn = parser->globalBody(sc->asGlobalContext());
    }
  }

  if (!pn) {
    // Global and eval scripts don't get reparsed after a new directive was
    // encountered:
    // - "use strict" doesn't require any special error reporting for scripts.
    // - "use asm" directives don't have an effect in global/eval contexts.
    MOZ_ASSERT(!canHandleParseFailure(compilationState_.directives));
    return false;
  }

  {
    // Successfully parsed. Emit the script.
    AutoGeckoProfilerEntry pseudoFrame(cx, "script emit",
                                       JS::ProfilingCategoryPair::JS_Parsing);

    Maybe<BytecodeEmitter> emitter;
    if (!emplaceEmitter(compilationInfo, emitter, sc)) {
      return false;
    }

    if (!emitter->emitScript(pn)) {
      return false;
    }
  }

  MOZ_ASSERT_IF(!cx->isHelperThreadContext(), !cx->isExceptionPending());

  return true;
}

template <typename Unit>
bool frontend::ModuleCompiler<Unit>::compile(JSContext* cx,
                                             CompilationInfo& compilationInfo) {
  if (!createSourceAndParser(cx, compilationInfo)) {
    return false;
  }

  // Emplace the topLevel stencil
  MOZ_ASSERT(compilationInfo.stencil.scriptData.length() ==
             CompilationInfo::TopLevelIndex);
  if (!compilationInfo.stencil.scriptData.emplaceBack()) {
    ReportOutOfMemory(cx);
    return false;
  }

  ModuleBuilder builder(cx, parser.ptr());
  StencilModuleMetadata& moduleMetadata =
      compilationInfo.stencil.moduleMetadata;

  uint32_t len = this->sourceBuffer_.length();
  SourceExtent extent =
      SourceExtent::makeGlobalExtent(len, compilationInfo.input.options.lineno,
                                     compilationInfo.input.options.column);
  ModuleSharedContext modulesc(cx, compilationInfo, builder, extent);

  ParseNode* pn = parser->moduleBody(&modulesc);
  if (!pn) {
    return false;
  }

  Maybe<BytecodeEmitter> emitter;
  if (!emplaceEmitter(compilationInfo, emitter, &modulesc)) {
    return false;
  }

  if (!emitter->emitScript(pn->as<ModuleNode>().body())) {
    return false;
  }

  builder.finishFunctionDecls(moduleMetadata);

  MOZ_ASSERT_IF(!cx->isHelperThreadContext(), !cx->isExceptionPending());
  return true;
}

// Parse a standalone JS function, which might appear as the value of an
// event handler attribute in an HTML <INPUT> tag, or in a Function()
// constructor.
template <typename Unit>
FunctionNode* frontend::StandaloneFunctionCompiler<Unit>::parse(
    JSContext* cx, CompilationInfo& compilationInfo,
    FunctionSyntaxKind syntaxKind, GeneratorKind generatorKind,
    FunctionAsyncKind asyncKind, const Maybe<uint32_t>& parameterListEnd) {
  assertSourceAndParserCreated(compilationInfo.input);

  TokenStreamPosition startPosition(parser->tokenStream);
  CompilationInfo::RewindToken startObj = compilationInfo.getRewindToken();

  // Speculatively parse using the default directives implied by the context.
  // If a directive is encountered (e.g., "use strict") that changes how the
  // function should have been parsed, we backup and reparse with the new set
  // of directives.

  FunctionNode* fn;
  for (;;) {
    Directives newDirectives = compilationState_.directives;
    fn = parser->standaloneFunction(parameterListEnd, syntaxKind, generatorKind,
                                    asyncKind, compilationState_.directives,
                                    &newDirectives);
    if (fn) {
      break;
    }

    // Maybe we encountered a new directive. See if we can try again.
    if (!canHandleParseFailure(newDirectives)) {
      return nullptr;
    }

    handleParseFailure(compilationInfo, newDirectives, startPosition, startObj);
  }

  return fn;
}

// Compile a standalone JS function.
template <typename Unit>
bool frontend::StandaloneFunctionCompiler<Unit>::compile(
    JSContext* cx, CompilationInfo& compilationInfo,
    FunctionNode* parsedFunction, CompilationGCOutput& gcOutput) {
  FunctionBox* funbox = parsedFunction->funbox();

  if (funbox->isInterpreted()) {
    Maybe<BytecodeEmitter> emitter;
    if (!emplaceEmitter(compilationInfo, emitter, funbox)) {
      return false;
    }

    if (!emitter->emitFunctionScript(parsedFunction, TopLevelFunction::Yes)) {
      return false;
    }

    // The parser extent has stripped off the leading `function...` but
    // we want the SourceExtent used in the final standalone script to
    // start from the beginning of the buffer, and use the provided
    // line and column.
    compilationInfo.stencil.scriptData[CompilationInfo::TopLevelIndex].extent =
        SourceExtent{/* sourceStart = */ 0,
                     sourceBuffer_.length(),
                     funbox->extent().toStringStart,
                     funbox->extent().toStringEnd,
                     compilationInfo.input.options.lineno,
                     compilationInfo.input.options.column};
  } else {
    // The asm.js module was created by parser. Instantiation below will
    // allocate the JSFunction that wraps it.
    MOZ_ASSERT(funbox->isAsmJSModule());
    MOZ_ASSERT(compilationInfo.stencil.asmJS.has(funbox->index()));
    MOZ_ASSERT(
        compilationInfo.stencil.scriptData[CompilationInfo::TopLevelIndex]
            .functionFlags.isAsmJSNative());
  }

  if (!compilationInfo.instantiateStencils(cx, gcOutput)) {
    return false;
  }

#ifdef DEBUG
  JSFunction* fun = gcOutput.functions[CompilationInfo::TopLevelIndex];
  MOZ_ASSERT(fun->hasBytecode() || IsAsmJSModule(fun));
#endif

  // Enqueue an off-thread source compression task after finishing parsing.
  if (!cx->isHelperThreadContext()) {
    if (!compilationInfo.input.source()->tryCompressOffThread(cx)) {
      return false;
    }
  }

  return true;
}

template <typename Unit>
static bool ParseModuleToStencilImpl(JSContext* cx,
                                     CompilationInfo& compilationInfo,
                                     SourceText<Unit>& srcBuf) {
  MOZ_ASSERT(srcBuf.get());

  AutoAssertReportedException assertException(cx);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  ModuleCompiler<Unit> compiler(cx, allocScope, compilationInfo.input.options,
                                srcBuf);
  if (!compiler.compile(cx, compilationInfo)) {
    return false;
  }

  assertException.reset();
  return true;
}

bool frontend::ParseModuleToStencil(JSContext* cx,
                                    CompilationInfo& compilationInfo,
                                    SourceText<char16_t>& srcBuf) {
  return ParseModuleToStencilImpl(cx, compilationInfo, srcBuf);
}

bool frontend::ParseModuleToStencil(JSContext* cx,
                                    CompilationInfo& compilationInfo,
                                    SourceText<Utf8Unit>& srcBuf) {
  return ParseModuleToStencilImpl(cx, compilationInfo, srcBuf);
}

template <typename Unit>
static UniquePtr<CompilationInfo> ParseModuleToStencilImpl(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<Unit>& srcBuf) {
  Rooted<UniquePtr<frontend::CompilationInfo>> compilationInfo(
      cx, js_new<frontend::CompilationInfo>(cx, options));
  if (!compilationInfo) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  if (!compilationInfo.get()->input.initForModule(cx)) {
    return nullptr;
  }

  if (!ParseModuleToStencilImpl(cx, *compilationInfo, srcBuf)) {
    return nullptr;
  }

  return std::move(compilationInfo.get());
}

UniquePtr<CompilationInfo> frontend::ParseModuleToStencil(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<char16_t>& srcBuf) {
  return ParseModuleToStencilImpl(cx, options, srcBuf);
}

UniquePtr<CompilationInfo> frontend::ParseModuleToStencil(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<Utf8Unit>& srcBuf) {
  return ParseModuleToStencilImpl(cx, options, srcBuf);
}

template <typename Unit>
static ModuleObject* CompileModuleImpl(
    JSContext* cx, const JS::ReadOnlyCompileOptions& optionsInput,
    SourceText<Unit>& srcBuf) {
  AutoAssertReportedException assertException(cx);

  if (!GlobalObject::ensureModulePrototypesCreated(cx, cx->global())) {
    return nullptr;
  }

  CompileOptions options(cx, optionsInput);
  options.setModule();

  Rooted<CompilationInfo> compilationInfo(cx, CompilationInfo(cx, options));
  if (!compilationInfo.get().input.initForModule(cx)) {
    return nullptr;
  }

  if (!ParseModuleToStencil(cx, compilationInfo.get(), srcBuf)) {
    return nullptr;
  }

  CompilationGCOutput gcOutput(cx);
  if (!InstantiateStencils(cx, compilationInfo.get(), gcOutput)) {
    return nullptr;
  }

  assertException.reset();
  return gcOutput.module;
}

ModuleObject* frontend::CompileModule(JSContext* cx,
                                      const JS::ReadOnlyCompileOptions& options,
                                      SourceText<char16_t>& srcBuf) {
  return CompileModuleImpl(cx, options, srcBuf);
}

ModuleObject* frontend::CompileModule(JSContext* cx,
                                      const JS::ReadOnlyCompileOptions& options,
                                      SourceText<Utf8Unit>& srcBuf) {
  return CompileModuleImpl(cx, options, srcBuf);
}

void frontend::FillCompileOptionsForLazyFunction(JS::CompileOptions& options,
                                                 JS::Handle<BaseScript*> lazy) {
  options.setMutedErrors(lazy->mutedErrors())
      .setFileAndLine(lazy->filename(), lazy->lineno())
      .setColumn(lazy->column())
      .setScriptSourceOffset(lazy->sourceStart())
      .setNoScriptRval(false)
      .setSelfHostingMode(false);
}

template <typename Unit>
static bool CompileLazyFunctionToStencilImpl(JSContext* cx,
                                             CompilationInfo& compilationInfo,
                                             Handle<BaseScript*> lazy,
                                             const Unit* units, size_t length) {
  MOZ_ASSERT(cx->compartment() == lazy->compartment());

  // We can only compile functions whose parents have previously been
  // compiled, because compilation requires full information about the
  // function's immediately enclosing scope.
  MOZ_ASSERT(lazy->isReadyForDelazification());

  AutoAssertReportedException assertException(cx);

  Rooted<JSFunction*> fun(cx, lazy->function());

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  frontend::CompilationState compilationState(
      cx, allocScope, compilationInfo.input.options, fun->enclosingScope());

  Parser<FullParseHandler, Unit> parser(
      cx, compilationInfo.input.options, units, length,
      /* foldConstants = */ true, compilationInfo, compilationState, nullptr,
      lazy);
  if (!parser.checkOptions()) {
    return false;
  }

  AutoGeckoProfilerEntry pseudoFrame(cx, "script delazify",
                                     JS::ProfilingCategoryPair::JS_Parsing);

  FunctionNode* pn =
      parser.standaloneLazyFunction(fun, lazy->toStringStart(), lazy->strict(),
                                    lazy->generatorKind(), lazy->asyncKind());
  if (!pn) {
    return false;
  }

  BytecodeEmitter bce(/* parent = */ nullptr, &parser, pn->funbox(),
                      compilationInfo, compilationState,
                      BytecodeEmitter::LazyFunction);
  if (!bce.init(pn->pn_pos)) {
    return false;
  }

  if (!bce.emitFunctionScript(pn, TopLevelFunction::Yes)) {
    return false;
  }

  // NOTE: Only allow relazification if there was no lazy PrivateScriptData.
  // This excludes non-leaf functions and all script class constructors.
  bool hadLazyScriptData = lazy->hasPrivateScriptData();
  bool isRelazifiableAfterDelazify = lazy->isRelazifiableAfterDelazify();
  compilationInfo.stencil.scriptData[CompilationInfo::TopLevelIndex]
      .allowRelazify = isRelazifiableAfterDelazify && !hadLazyScriptData;

  assertException.reset();
  return true;
}

MOZ_MUST_USE bool frontend::CompileLazyFunctionToStencil(
    JSContext* cx, CompilationInfo& compilationInfo,
    JS::Handle<BaseScript*> lazy, const char16_t* units, size_t length) {
  return CompileLazyFunctionToStencilImpl(cx, compilationInfo, lazy, units,
                                          length);
}

MOZ_MUST_USE bool frontend::CompileLazyFunctionToStencil(
    JSContext* cx, CompilationInfo& compilationInfo,
    JS::Handle<BaseScript*> lazy, const mozilla::Utf8Unit* units,
    size_t length) {
  return CompileLazyFunctionToStencilImpl(cx, compilationInfo, lazy, units,
                                          length);
}

bool frontend::InstantiateStencilsForDelazify(
    JSContext* cx, CompilationInfo& compilationInfo) {
  AutoAssertReportedException assertException(cx);

  mozilla::DebugOnly<uint32_t> lazyFlags =
      static_cast<uint32_t>(compilationInfo.input.lazy->immutableFlags());

  CompilationGCOutput gcOutput(cx);
  if (!compilationInfo.instantiateStencils(cx, gcOutput)) {
    return false;
  }

  MOZ_ASSERT(lazyFlags == gcOutput.script->immutableFlags());
  MOZ_ASSERT(
      gcOutput.script->outermostScope()->hasOnChain(ScopeKind::NonSyntactic) ==
      gcOutput.script->immutableFlags().hasFlag(
          JSScript::ImmutableFlags::HasNonSyntacticScope));

  assertException.reset();
  return true;
}

static JSFunction* CompileStandaloneFunction(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, const Maybe<uint32_t>& parameterListEnd,
    FunctionSyntaxKind syntaxKind, GeneratorKind generatorKind,
    FunctionAsyncKind asyncKind, HandleScope enclosingScope = nullptr) {
  AutoAssertReportedException assertException(cx);

  RootedScope scope(cx, enclosingScope);
  if (!scope) {
    scope = &cx->global()->emptyGlobalScope();
  }

  Rooted<CompilationInfo> compilationInfo(cx, CompilationInfo(cx, options));
  if (!compilationInfo.get().input.initForStandaloneFunction(cx, scope)) {
    return nullptr;
  }

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  StandaloneFunctionCompiler<char16_t> compiler(
      cx, allocScope, compilationInfo.get().input.options, srcBuf,
      enclosingScope);
  if (!compiler.createSourceAndParser(cx, compilationInfo.get())) {
    return nullptr;
  }

  FunctionNode* parsedFunction =
      compiler.parse(cx, compilationInfo.get(), syntaxKind, generatorKind,
                     asyncKind, parameterListEnd);
  if (!parsedFunction) {
    return nullptr;
  }

  CompilationGCOutput gcOutput(cx);
  if (!compiler.compile(cx, compilationInfo.get(), parsedFunction, gcOutput)) {
    return nullptr;
  }

  // Note: If AsmJS successfully compiles, the into.script will still be
  // nullptr. In this case we have compiled to a native function instead of an
  // interpreted script.
  if (gcOutput.script) {
    if (parameterListEnd) {
      compilationInfo.get().input.source()->setParameterListEnd(
          *parameterListEnd);
    }
    tellDebuggerAboutCompiledScript(cx, options.hideScriptFromDebugger,
                                    gcOutput.script);
  }

  assertException.reset();
  return gcOutput.functions[CompilationInfo::TopLevelIndex];
}

JSFunction* frontend::CompileStandaloneFunction(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, const Maybe<uint32_t>& parameterListEnd,
    FunctionSyntaxKind syntaxKind, HandleScope enclosingScope /* = nullptr */) {
  return CompileStandaloneFunction(cx, options, srcBuf, parameterListEnd,
                                   syntaxKind, GeneratorKind::NotGenerator,
                                   FunctionAsyncKind::SyncFunction,
                                   enclosingScope);
}

JSFunction* frontend::CompileStandaloneGenerator(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, const Maybe<uint32_t>& parameterListEnd,
    FunctionSyntaxKind syntaxKind) {
  return CompileStandaloneFunction(cx, options, srcBuf, parameterListEnd,
                                   syntaxKind, GeneratorKind::Generator,
                                   FunctionAsyncKind::SyncFunction);
}

JSFunction* frontend::CompileStandaloneAsyncFunction(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, const Maybe<uint32_t>& parameterListEnd,
    FunctionSyntaxKind syntaxKind) {
  return CompileStandaloneFunction(cx, options, srcBuf, parameterListEnd,
                                   syntaxKind, GeneratorKind::NotGenerator,
                                   FunctionAsyncKind::AsyncFunction);
}

JSFunction* frontend::CompileStandaloneAsyncGenerator(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, const Maybe<uint32_t>& parameterListEnd,
    FunctionSyntaxKind syntaxKind) {
  return CompileStandaloneFunction(cx, options, srcBuf, parameterListEnd,
                                   syntaxKind, GeneratorKind::Generator,
                                   FunctionAsyncKind::AsyncFunction);
}

bool frontend::CompilationInput::initScriptSource(JSContext* cx) {
  ScriptSource* ss = cx->new_<ScriptSource>();
  if (!ss) {
    return false;
  }
  setSource(ss);

  return ss->initFromOptions(cx, options);
}

void CompilationInput::trace(JSTracer* trc) {
  atoms.trace(trc);
  TraceNullableRoot(trc, &lazy, "compilation-input-lazy");
  source_.trace(trc);
  TraceNullableRoot(trc, &enclosingScope, "compilation-input-enclosing-scope");
}

void CompilationInfo::trace(JSTracer* trc) { input.trace(trc); }
