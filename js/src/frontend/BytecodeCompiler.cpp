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

static bool EmplaceEmitter(CompilationStencil& stencil,
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
                               CompilationInput& input,
                               CompilationStencil& stencil,
                               SourceText<Unit>& sourceBuffer)
      : sourceBuffer_(sourceBuffer),
        compilationState_(cx, allocScope, input, stencil) {
    MOZ_ASSERT(sourceBuffer_.get() != nullptr);
  }

  bool init(JSContext* cx, InheritThis inheritThis = InheritThis::No,
            JSObject* enclosingEnv = nullptr) {
    return compilationState_.init(cx, inheritThis, enclosingEnv);
  }

  // Call this before calling compile{Global,Eval}Script.
  [[nodiscard]] bool createSourceAndParser(JSContext* cx,
                                           CompilationStencil& stencil);

  void assertSourceAndParserCreated(CompilationStencil& stencil) const {
    MOZ_ASSERT(stencil.source != nullptr);
    MOZ_ASSERT(parser.isSome());
  }

  void assertSourceParserAndScriptCreated(CompilationStencil& stencil) {
    assertSourceAndParserCreated(stencil);
  }

  [[nodiscard]] bool emplaceEmitter(CompilationStencil& stencil,
                                    Maybe<BytecodeEmitter>& emitter,
                                    SharedContext* sharedContext) {
    return EmplaceEmitter(stencil, compilationState_, emitter,
                          EitherParser(parser.ptr()), sharedContext);
  }

  bool canHandleParseFailure(const Directives& newDirectives);

  void handleParseFailure(const Directives& newDirectives,
                          TokenStreamPosition& startPosition,
                          CompilationState::RewindToken& startObj);

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
                          CompilationInput& input, CompilationStencil& stencil,
                          SourceText<Unit>& sourceBuffer)
      : Base(cx, allocScope, input, stencil, sourceBuffer) {}

  using Base::createSourceAndParser;
  using Base::init;

  bool compileScriptToStencil(JSContext* cx, CompilationStencil& stencil,
                              SharedContext* sc);
};

#ifdef JS_ENABLE_SMOOSH
static bool TrySmoosh(JSContext* cx, CompilationInput& input,
                      JS::SourceText<mozilla::Utf8Unit>& srcBuf,
                      UniquePtr<CompilationStencil>& stencilOut) {
  MOZ_ASSERT(!stencilOut);

  if (!cx->options().trySmoosh()) {
    return true;
  }

  JSRuntime* rt = cx->runtime();
  if (!Smoosh::tryCompileGlobalScriptToStencil(cx, input, srcBuf, stencilOut)) {
    return false;
  }

  if (cx->options().trackNotImplemented()) {
    if (stencilOut) {
      rt->parserWatcherFile.put("1");
    } else {
      rt->parserWatcherFile.put("0");
    }
  }

  if (!stencilOut) {
    fprintf(stderr, "Falling back!\n");
    return true;
  }

  return stencilOut->source->assignSource(cx, input.options, srcBuf);
}

static bool TrySmoosh(JSContext* cx, CompilationInput& input,
                      JS::SourceText<char16_t>& srcBuf,
                      UniquePtr<CompilationStencil>& stencilOut) {
  MOZ_ASSERT(!stencilOut);
  return true;
}
#endif  // JS_ENABLE_SMOOSH

template <typename Unit>
static UniquePtr<CompilationStencil> CompileGlobalScriptToStencilImpl(
    JSContext* cx, CompilationInput& input, JS::SourceText<Unit>& srcBuf,
    ScopeKind scopeKind) {
  UniquePtr<frontend::CompilationStencil> stencil;

#ifdef JS_ENABLE_SMOOSH
  if (!TrySmoosh(cx, input, srcBuf, stencil)) {
    return nullptr;
  }
  if (stencil) {
    return stencil;
  }
#endif  // JS_ENABLE_SMOOSH

  if (input.options.selfHostingMode) {
    if (!input.initForSelfHostingGlobal(cx)) {
      return nullptr;
    }
  } else {
    if (!input.initForGlobal(cx)) {
      return nullptr;
    }
  }

  stencil = cx->make_unique<frontend::CompilationStencil>(input);
  if (!stencil) {
    return nullptr;
  }

  AutoAssertReportedException assertException(cx);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  frontend::ScriptCompiler<Unit> compiler(cx, allocScope, input, *stencil,
                                          srcBuf);
  if (!compiler.init(cx)) {
    return nullptr;
  }

  if (!compiler.createSourceAndParser(cx, *stencil)) {
    return nullptr;
  }

  SourceExtent extent = SourceExtent::makeGlobalExtent(
      srcBuf.length(), input.options.lineno, input.options.column);

  frontend::GlobalSharedContext globalsc(cx, scopeKind, input.options,
                                         compiler.compilationState().directives,
                                         extent);

  if (!compiler.compileScriptToStencil(cx, *stencil, &globalsc)) {
    return nullptr;
  }

  assertException.reset();

  return stencil;
}

UniquePtr<CompilationStencil> frontend::CompileGlobalScriptToStencil(
    JSContext* cx, CompilationInput& input, JS::SourceText<char16_t>& srcBuf,
    ScopeKind scopeKind) {
  return CompileGlobalScriptToStencilImpl(cx, input, srcBuf, scopeKind);
}

UniquePtr<CompilationStencil> frontend::CompileGlobalScriptToStencil(
    JSContext* cx, CompilationInput& input, JS::SourceText<Utf8Unit>& srcBuf,
    ScopeKind scopeKind) {
  return CompileGlobalScriptToStencilImpl(cx, input, srcBuf, scopeKind);
}

bool frontend::InstantiateStencils(
    JSContext* cx, CompilationInput& input, CompilationStencil& stencil,
    CompilationGCOutput& gcOutput,
    CompilationGCOutput* gcOutputForDelazification) {
  {
    AutoGeckoProfilerEntry pseudoFrame(cx, "stencil instantiate",
                                       JS::ProfilingCategoryPair::JS_Parsing);

    if (!CompilationStencil::instantiateStencils(cx, input, stencil, gcOutput,
                                                 gcOutputForDelazification)) {
      return false;
    }
  }

  // Enqueue an off-thread source compression task after finishing parsing.
  if (!cx->isHelperThreadContext()) {
    if (!stencil.source->tryCompressOffThread(cx)) {
      return false;
    }

    Rooted<JSScript*> script(cx, gcOutput.script);
    if (!input.options.hideScriptFromDebugger) {
      DebugAPI::onNewScript(cx, script);
    }
  }

  return true;
}
bool frontend::PrepareForInstantiate(
    JSContext* cx, CompilationInput& input, CompilationStencil& stencil,
    CompilationGCOutput& gcOutput,
    CompilationGCOutput* gcOutputForDelazification) {
  AutoGeckoProfilerEntry pseudoFrame(cx, "stencil instantiate",
                                     JS::ProfilingCategoryPair::JS_Parsing);

  return CompilationStencil::prepareForInstantiate(cx, input, stencil, gcOutput,
                                                   gcOutputForDelazification);
}

template <typename Unit>
static JSScript* CompileGlobalScriptImpl(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<Unit>& srcBuf, ScopeKind scopeKind) {
  Rooted<CompilationInput> input(cx, CompilationInput(options));
  UniquePtr<CompilationStencil> stencil =
      CompileGlobalScriptToStencil(cx, input.get(), srcBuf, scopeKind);
  if (!stencil) {
    return nullptr;
  }

  Rooted<frontend::CompilationGCOutput> gcOutput(cx);
  if (!InstantiateStencils(cx, input.get(), *stencil, gcOutput.get())) {
    return nullptr;
  }

  return gcOutput.get().script;
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

  Rooted<CompilationInput> input(cx, CompilationInput(options));
  if (!input.get().initForEval(cx, enclosingScope)) {
    return nullptr;
  }
  CompilationStencil stencil(input.get());

  LifoAllocScope allocScope(&cx->tempLifoAlloc());

  frontend::ScriptCompiler<Unit> compiler(cx, allocScope, input.get(), stencil,
                                          srcBuf);
  if (!compiler.init(cx, InheritThis::Yes, enclosingEnv)) {
    return nullptr;
  }

  if (!compiler.createSourceAndParser(cx, stencil)) {
    return nullptr;
  }

  uint32_t len = srcBuf.length();
  SourceExtent extent =
      SourceExtent::makeGlobalExtent(len, options.lineno, options.column);
  frontend::EvalSharedContext evalsc(cx, compiler.compilationState(), extent);
  if (!compiler.compileScriptToStencil(cx, stencil, &evalsc)) {
    return nullptr;
  }

  Rooted<frontend::CompilationGCOutput> gcOutput(cx);
  if (!InstantiateStencils(cx, input.get(), stencil, gcOutput.get())) {
    return nullptr;
  }

  assertException.reset();
  return gcOutput.get().script;
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
  using Base::init;
  using Base::parser;

 public:
  explicit ModuleCompiler(JSContext* cx, LifoAllocScope& allocScope,
                          CompilationInput& input, CompilationStencil& stencil,
                          SourceText<Unit>& sourceBuffer)
      : Base(cx, allocScope, input, stencil, sourceBuffer) {}

  bool compile(JSContext* cx, CompilationStencil& stencil);
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
                                      CompilationInput& input,
                                      CompilationStencil& stencil,
                                      SourceText<Unit>& sourceBuffer)
      : Base(cx, allocScope, input, stencil, sourceBuffer) {}

  using Base::createSourceAndParser;
  using Base::init;

  FunctionNode* parse(JSContext* cx, CompilationStencil& stencil,
                      FunctionSyntaxKind syntaxKind,
                      GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                      const Maybe<uint32_t>& parameterListEnd);

  bool compile(JSContext* cx, CompilationStencil& stencil,
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

template <typename Unit>
bool frontend::SourceAwareCompiler<Unit>::createSourceAndParser(
    JSContext* cx, CompilationStencil& stencil) {
  const auto& options = compilationState_.input.options;

  if (!stencil.source->assignSource(cx, options, sourceBuffer_)) {
    return false;
  }

  if (CanLazilyParse(options)) {
    syntaxParser.emplace(
        cx, options, sourceBuffer_.units(), sourceBuffer_.length(),
        /* foldConstants = */ false, stencil, compilationState_,
        /* syntaxParser = */ nullptr);
    if (!syntaxParser->checkOptions()) {
      return false;
    }
  }

  parser.emplace(cx, options, sourceBuffer_.units(), sourceBuffer_.length(),
                 /* foldConstants = */ true, stencil, compilationState_,
                 syntaxParser.ptrOr(nullptr));
  parser->ss = stencil.source.get();
  return parser->checkOptions();
}

static bool EmplaceEmitter(CompilationStencil& stencil,
                           CompilationState& compilationState,
                           Maybe<BytecodeEmitter>& emitter,
                           const EitherParser& parser, SharedContext* sc) {
  BytecodeEmitter::EmitterMode emitterMode =
      sc->selfHosted() ? BytecodeEmitter::SelfHosting : BytecodeEmitter::Normal;
  emitter.emplace(/* parent = */ nullptr, parser, sc, stencil, compilationState,
                  emitterMode);
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
    const Directives& newDirectives, TokenStreamPosition& startPosition,
    CompilationState::RewindToken& startObj) {
  MOZ_ASSERT(canHandleParseFailure(newDirectives));

  // Rewind to starting position to retry.
  parser->tokenStream.rewind(startPosition);
  compilationState_.rewind(startObj);

  // Assignment must be monotonic to prevent reparsing iloops
  MOZ_ASSERT_IF(compilationState_.directives.strict(), newDirectives.strict());
  MOZ_ASSERT_IF(compilationState_.directives.asmJS(), newDirectives.asmJS());
  compilationState_.directives = newDirectives;
}

template <typename Unit>
bool frontend::ScriptCompiler<Unit>::compileScriptToStencil(
    JSContext* cx, CompilationStencil& stencil, SharedContext* sc) {
  assertSourceParserAndScriptCreated(stencil);

  TokenStreamPosition startPosition(parser->tokenStream);

  // Emplace the topLevel stencil
  MOZ_ASSERT(compilationState_.scriptData.length() ==
             CompilationStencil::TopLevelIndex);
  if (!compilationState_.scriptData.emplaceBack()) {
    ReportOutOfMemory(cx);
    return false;
  }
  if (!compilationState_.scriptExtra.emplaceBack()) {
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
    if (!emplaceEmitter(stencil, emitter, sc)) {
      return false;
    }

    if (!emitter->emitScript(pn)) {
      return false;
    }

    if (!compilationState_.finish(cx, stencil)) {
      return false;
    }
  }

  MOZ_ASSERT_IF(!cx->isHelperThreadContext(), !cx->isExceptionPending());

  return true;
}

template <typename Unit>
bool frontend::ModuleCompiler<Unit>::compile(JSContext* cx,
                                             CompilationStencil& stencil) {
  if (!init(cx)) {
    return false;
  }

  if (!createSourceAndParser(cx, stencil)) {
    return false;
  }

  // Emplace the topLevel stencil
  MOZ_ASSERT(compilationState_.scriptData.length() ==
             CompilationStencil::TopLevelIndex);
  if (!compilationState_.scriptData.emplaceBack()) {
    ReportOutOfMemory(cx);
    return false;
  }
  if (!compilationState_.scriptExtra.emplaceBack()) {
    ReportOutOfMemory(cx);
    return false;
  }

  ModuleBuilder builder(cx, parser.ptr());

  const auto& options = compilationState_.input.options;

  uint32_t len = this->sourceBuffer_.length();
  SourceExtent extent =
      SourceExtent::makeGlobalExtent(len, options.lineno, options.column);
  ModuleSharedContext modulesc(cx, options, builder, extent);

  ParseNode* pn = parser->moduleBody(&modulesc);
  if (!pn) {
    return false;
  }

  Maybe<BytecodeEmitter> emitter;
  if (!emplaceEmitter(stencil, emitter, &modulesc)) {
    return false;
  }

  if (!emitter->emitScript(pn->as<ModuleNode>().body())) {
    return false;
  }

  if (!compilationState_.finish(cx, stencil)) {
    return false;
  }

  StencilModuleMetadata& moduleMetadata = *stencil.moduleMetadata;

  builder.finishFunctionDecls(moduleMetadata);

  MOZ_ASSERT_IF(!cx->isHelperThreadContext(), !cx->isExceptionPending());
  return true;
}

// Parse a standalone JS function, which might appear as the value of an
// event handler attribute in an HTML <INPUT> tag, or in a Function()
// constructor.
template <typename Unit>
FunctionNode* frontend::StandaloneFunctionCompiler<Unit>::parse(
    JSContext* cx, CompilationStencil& stencil, FunctionSyntaxKind syntaxKind,
    GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
    const Maybe<uint32_t>& parameterListEnd) {
  assertSourceAndParserCreated(stencil);

  TokenStreamPosition startPosition(parser->tokenStream);
  CompilationState::RewindToken startObj = compilationState_.getRewindToken();

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

    handleParseFailure(newDirectives, startPosition, startObj);
  }

  return fn;
}

// Compile a standalone JS function.
template <typename Unit>
bool frontend::StandaloneFunctionCompiler<Unit>::compile(
    JSContext* cx, CompilationStencil& stencil, FunctionNode* parsedFunction,
    CompilationGCOutput& gcOutput) {
  FunctionBox* funbox = parsedFunction->funbox();

  if (funbox->isInterpreted()) {
    Maybe<BytecodeEmitter> emitter;
    if (!emplaceEmitter(stencil, emitter, funbox)) {
      return false;
    }

    if (!emitter->emitFunctionScript(parsedFunction)) {
      return false;
    }

    // The parser extent has stripped off the leading `function...` but
    // we want the SourceExtent used in the final standalone script to
    // start from the beginning of the buffer, and use the provided
    // line and column.
    const auto& options = compilationState_.input.options;
    compilationState_.scriptExtra[CompilationStencil::TopLevelIndex].extent =
        SourceExtent{/* sourceStart = */ 0,
                     sourceBuffer_.length(),
                     funbox->extent().toStringStart,
                     funbox->extent().toStringEnd,
                     options.lineno,
                     options.column};

    if (!compilationState_.finish(cx, stencil)) {
      return false;
    }
  } else {
    if (!compilationState_.finish(cx, stencil)) {
      return false;
    }

    // The asm.js module was created by parser. Instantiation below will
    // allocate the JSFunction that wraps it.
    MOZ_ASSERT(funbox->isAsmJSModule());
    MOZ_ASSERT(stencil.asmJS.has(funbox->index()));
    MOZ_ASSERT(compilationState_.scriptData[CompilationStencil::TopLevelIndex]
                   .functionFlags.isAsmJSNative());
  }

  if (!CompilationStencil::instantiateStencils(cx, compilationState_.input,
                                               stencil, gcOutput)) {
    return false;
  }

#ifdef DEBUG
  JSFunction* fun = gcOutput.functions[CompilationStencil::TopLevelIndex];
  MOZ_ASSERT(fun->hasBytecode() || IsAsmJSModule(fun));
#endif

  // Enqueue an off-thread source compression task after finishing parsing.
  if (!cx->isHelperThreadContext()) {
    if (!stencil.source->tryCompressOffThread(cx)) {
      return false;
    }
  }

  return true;
}

template <typename Unit>
static UniquePtr<CompilationStencil> ParseModuleToStencilImpl(
    JSContext* cx, CompilationInput& input, SourceText<Unit>& srcBuf) {
  MOZ_ASSERT(srcBuf.get());

  if (!input.initForModule(cx)) {
    return nullptr;
  }

  UniquePtr<frontend::CompilationStencil> stencil(
      cx->new_<frontend::CompilationStencil>(input));
  if (!stencil) {
    return nullptr;
  }

  AutoAssertReportedException assertException(cx);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  ModuleCompiler<Unit> compiler(cx, allocScope, input, *stencil, srcBuf);
  if (!compiler.compile(cx, *stencil)) {
    return nullptr;
  }

  assertException.reset();

  return stencil;
}

UniquePtr<CompilationStencil> frontend::ParseModuleToStencil(
    JSContext* cx, CompilationInput& input, SourceText<char16_t>& srcBuf) {
  return ParseModuleToStencilImpl(cx, input, srcBuf);
}

UniquePtr<CompilationStencil> frontend::ParseModuleToStencil(
    JSContext* cx, CompilationInput& input, SourceText<Utf8Unit>& srcBuf) {
  return ParseModuleToStencilImpl(cx, input, srcBuf);
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

  Rooted<CompilationInput> input(cx, CompilationInput(options));
  UniquePtr<CompilationStencil> stencil =
      ParseModuleToStencil(cx, input.get(), srcBuf);
  if (!stencil) {
    return nullptr;
  }

  Rooted<CompilationGCOutput> gcOutput(cx);
  if (!InstantiateStencils(cx, input.get(), *stencil, gcOutput.get())) {
    return nullptr;
  }

  assertException.reset();
  return gcOutput.get().module;
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
                                             CompilationInput& input,
                                             CompilationStencil& stencil,
                                             const Unit* units, size_t length) {
  MOZ_ASSERT(cx->compartment() == input.lazy->compartment());
  MOZ_ASSERT(!stencil.isInitialStencil());

  // We can only compile functions whose parents have previously been
  // compiled, because compilation requires full information about the
  // function's immediately enclosing scope.
  MOZ_ASSERT(input.lazy->isReadyForDelazification());

  AutoAssertReportedException assertException(cx);

  Rooted<JSFunction*> fun(cx, input.lazy->function());

  InheritThis inheritThis = fun->isArrow() ? InheritThis::Yes : InheritThis::No;

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  frontend::CompilationState compilationState(cx, allocScope, input, stencil);
  if (!compilationState.init(cx, inheritThis)) {
    return false;
  }

  Parser<FullParseHandler, Unit> parser(cx, input.options, units, length,
                                        /* foldConstants = */ true, stencil,
                                        compilationState,
                                        /* syntaxParser = */ nullptr);
  if (!parser.checkOptions()) {
    return false;
  }

  AutoGeckoProfilerEntry pseudoFrame(cx, "script delazify",
                                     JS::ProfilingCategoryPair::JS_Parsing);

  FunctionNode* pn = parser.standaloneLazyFunction(
      fun, input.lazy->toStringStart(), input.lazy->strict(),
      input.lazy->generatorKind(), input.lazy->asyncKind());
  if (!pn) {
    return false;
  }

  BytecodeEmitter bce(/* parent = */ nullptr, &parser, pn->funbox(), stencil,
                      compilationState, BytecodeEmitter::LazyFunction);
  if (!bce.init(pn->pn_pos)) {
    return false;
  }

  if (!bce.emitFunctionScript(pn)) {
    return false;
  }

  // NOTE: Only allow relazification if there was no lazy PrivateScriptData.
  // This excludes non-leaf functions and all script class constructors.
  bool hadLazyScriptData = input.lazy->hasPrivateScriptData();
  bool isRelazifiableAfterDelazify = input.lazy->isRelazifiableAfterDelazify();
  if (isRelazifiableAfterDelazify && !hadLazyScriptData) {
    compilationState.scriptData[CompilationStencil::TopLevelIndex]
        .setAllowRelazify();
  }

  if (!compilationState.finish(cx, stencil)) {
    return false;
  }

  assertException.reset();
  return true;
}

[[nodiscard]] bool frontend::CompileLazyFunctionToStencil(
    JSContext* cx, CompilationInput& input, CompilationStencil& stencil,
    const char16_t* units, size_t length) {
  return CompileLazyFunctionToStencilImpl(cx, input, stencil, units, length);
}

[[nodiscard]] bool frontend::CompileLazyFunctionToStencil(
    JSContext* cx, CompilationInput& input, CompilationStencil& stencil,
    const mozilla::Utf8Unit* units, size_t length) {
  return CompileLazyFunctionToStencilImpl(cx, input, stencil, units, length);
}

bool frontend::InstantiateStencilsForDelazify(JSContext* cx,
                                              CompilationInput& input,
                                              CompilationStencil& stencil) {
  AutoAssertReportedException assertException(cx);

  mozilla::DebugOnly<uint32_t> lazyFlags =
      static_cast<uint32_t>(input.lazy->immutableFlags());

  Rooted<CompilationGCOutput> gcOutput(cx);
  if (!CompilationStencil::instantiateStencils(cx, input, stencil,
                                               gcOutput.get())) {
    return false;
  }

  MOZ_ASSERT(lazyFlags == gcOutput.get().script->immutableFlags());
  MOZ_ASSERT(gcOutput.get().script->outermostScope()->hasOnChain(
                 ScopeKind::NonSyntactic) ==
             gcOutput.get().script->immutableFlags().hasFlag(
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

  Rooted<CompilationInput> input(cx, CompilationInput(options));
  if (enclosingScope) {
    if (!input.get().initForStandaloneFunctionInNonSyntacticScope(
            cx, enclosingScope)) {
      return nullptr;
    }
  } else {
    if (!input.get().initForStandaloneFunction(cx)) {
      return nullptr;
    }
  }
  CompilationStencil stencil(input.get());

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  InheritThis inheritThis = (syntaxKind == FunctionSyntaxKind::Arrow)
                                ? InheritThis::Yes
                                : InheritThis::No;
  StandaloneFunctionCompiler<char16_t> compiler(cx, allocScope, input.get(),
                                                stencil, srcBuf);
  if (!compiler.init(cx, inheritThis)) {
    return nullptr;
  }

  if (!compiler.createSourceAndParser(cx, stencil)) {
    return nullptr;
  }

  FunctionNode* parsedFunction = compiler.parse(
      cx, stencil, syntaxKind, generatorKind, asyncKind, parameterListEnd);
  if (!parsedFunction) {
    return nullptr;
  }

  Rooted<CompilationGCOutput> gcOutput(cx);
  if (!compiler.compile(cx, stencil, parsedFunction, gcOutput.get())) {
    return nullptr;
  }

  // Note: If AsmJS successfully compiles, the into.script will still be
  // nullptr. In this case we have compiled to a native function instead of an
  // interpreted script.
  if (gcOutput.get().script) {
    if (parameterListEnd) {
      stencil.source->setParameterListEnd(*parameterListEnd);
    }

    MOZ_ASSERT(!cx->isHelperThreadContext());

    Rooted<JSScript*> script(cx, gcOutput.get().script);
    if (!options.hideScriptFromDebugger) {
      DebugAPI::onNewScript(cx, script);
    }
  }

  assertException.reset();
  return gcOutput.get().functions[CompilationStencil::TopLevelIndex];
}

JSFunction* frontend::CompileStandaloneFunction(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, const Maybe<uint32_t>& parameterListEnd,
    FunctionSyntaxKind syntaxKind) {
  return CompileStandaloneFunction(cx, options, srcBuf, parameterListEnd,
                                   syntaxKind, GeneratorKind::NotGenerator,
                                   FunctionAsyncKind::SyncFunction);
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

JSFunction* frontend::CompileStandaloneFunctionInNonSyntacticScope(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, const Maybe<uint32_t>& parameterListEnd,
    FunctionSyntaxKind syntaxKind, HandleScope enclosingScope) {
  MOZ_ASSERT(enclosingScope);
  return CompileStandaloneFunction(cx, options, srcBuf, parameterListEnd,
                                   syntaxKind, GeneratorKind::NotGenerator,
                                   FunctionAsyncKind::SyncFunction,
                                   enclosingScope);
}
