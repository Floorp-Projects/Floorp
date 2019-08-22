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
#if defined(JS_BUILD_BINAST)
#  include "frontend/BinASTParser.h"
#endif  // JS_BUILD_BINAST
#include "frontend/BytecodeCompilation.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/EitherParser.h"
#include "frontend/ErrorReporter.h"
#include "frontend/FoldConstants.h"
#include "frontend/ModuleSharedContext.h"
#include "frontend/Parser.h"
#include "js/SourceText.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/JSScript.h"
#include "vm/ModuleBuilder.h"  // js::ModuleBuilder
#include "vm/TraceLogging.h"
#include "wasm/AsmJS.h"

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

// CompileScript independently returns the ScriptSourceObject (SSO) for the
// compile.  This is used by off-thread script compilation (OT-SC).
//
// OT-SC cannot initialize the SSO when it is first constructed because the
// SSO is allocated initially in a separate compartment.
//
// After OT-SC, the separate compartment is merged with the main compartment,
// at which point the JSScripts created become observable by the debugger via
// memory-space scanning.
//
// Whatever happens to the top-level script compilation (even if it fails and
// returns null), we must finish initializing the SSO.  This is because there
// may be valid inner scripts observable by the debugger which reference the
// partially-initialized SSO.
class MOZ_STACK_CLASS AutoInitializeSourceObject {
  BytecodeCompiler& compiler_;
  ScriptSourceObject** sourceObjectOut_;

 public:
  AutoInitializeSourceObject(BytecodeCompiler& compiler,
                             ScriptSourceObject** sourceObjectOut)
      : compiler_(compiler), sourceObjectOut_(sourceObjectOut) {}

  inline ~AutoInitializeSourceObject() {
    if (sourceObjectOut_) {
      *sourceObjectOut_ = compiler_.sourceObjectPtr();
    }
  }
};

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

template <typename Unit>
class MOZ_STACK_CLASS frontend::SourceAwareCompiler {
 protected:
  SourceText<Unit>& sourceBuffer_;

  Maybe<Parser<SyntaxParseHandler, Unit>> syntaxParser;
  Maybe<Parser<FullParseHandler, Unit>> parser;

  using TokenStreamPosition = frontend::TokenStreamPosition<Unit>;

 protected:
  explicit SourceAwareCompiler(SourceText<Unit>& sourceBuffer)
      : sourceBuffer_(sourceBuffer) {
    MOZ_ASSERT(sourceBuffer_.get() != nullptr);
  }

  // Call this before calling compile{Global,Eval}Script.
  MOZ_MUST_USE bool prepareScriptParse(BytecodeCompiler& info) {
    return createSourceAndParser(info, ParseGoal::Script) &&
           createCompleteScript(info);
  }

  void assertSourceAndParserCreated(BytecodeCompiler& info) const {
    info.assertSourceCreated();
    MOZ_ASSERT(info.usedNames.isSome());
    MOZ_ASSERT(parser.isSome());
  }

  void assertSourceParserAndScriptCreated(BytecodeCompiler& info) {
    assertSourceAndParserCreated(info);
    MOZ_ASSERT(info.script != nullptr);
  }

  MOZ_MUST_USE bool emplaceEmitter(BytecodeCompiler& info,
                                   Maybe<BytecodeEmitter>& emitter,
                                   SharedContext* sharedContext) {
    return info.emplaceEmitter(emitter, EitherParser(parser.ptr()),
                               sharedContext);
  }

  MOZ_MUST_USE bool createSourceAndParser(
      BytecodeCompiler& compiler, ParseGoal goal,
      const Maybe<uint32_t>& parameterListEnd = Nothing());

  // This assumes the created script's offsets in the source used to parse it
  // are the same as are used to compute its Function.prototype.toString()
  // value.
  MOZ_MUST_USE bool createCompleteScript(BytecodeCompiler& info) {
    uint32_t toStringStart = 0;
    uint32_t len = sourceBuffer_.length();
    uint32_t toStringEnd = len;
    return info.internalCreateScript(toStringStart, toStringEnd, len);
  }

  MOZ_MUST_USE bool handleParseFailure(BytecodeCompiler& compiler,
                                       const Directives& newDirectives,
                                       TokenStreamPosition& startPosition);
};

template <typename Unit>
class MOZ_STACK_CLASS frontend::ScriptCompiler
    : public SourceAwareCompiler<Unit> {
  using Base = SourceAwareCompiler<Unit>;

 protected:
  using Base::parser;
  using Base::sourceBuffer_;

  using Base::assertSourceParserAndScriptCreated;
  using Base::emplaceEmitter;
  using Base::handleParseFailure;

  using typename Base::TokenStreamPosition;

 public:
  explicit ScriptCompiler(SourceText<Unit>& srcBuf) : Base(srcBuf) {}

  MOZ_MUST_USE bool prepareScriptParse(BytecodeCompiler& compiler) {
    return Base::prepareScriptParse(compiler);
  }

  JSScript* compileScript(BytecodeCompiler& compiler, HandleObject environment,
                          SharedContext* sc);
};

template <typename Unit>
static JSScript* CreateGlobalScript(
    GlobalScriptInfo& info, JS::SourceText<Unit>& srcBuf,
    ScriptSourceObject** sourceObjectOut = nullptr) {
  AutoAssertReportedException assertException(info.context());

  frontend::ScriptCompiler<Unit> compiler(srcBuf);
  AutoInitializeSourceObject autoSSO(info, sourceObjectOut);

  if (!compiler.prepareScriptParse(info)) {
    return nullptr;
  }

  JSScript* script =
      compiler.compileScript(info, nullptr, info.sharedContext());
  if (!script) {
    return nullptr;
  }

  assertException.reset();
  return script;
}

JSScript* frontend::CompileGlobalScript(
    GlobalScriptInfo& info, JS::SourceText<char16_t>& srcBuf,
    ScriptSourceObject** sourceObjectOut /* = nullptr */) {
  return CreateGlobalScript(info, srcBuf, sourceObjectOut);
}

JSScript* frontend::CompileGlobalScript(
    GlobalScriptInfo& info, JS::SourceText<Utf8Unit>& srcBuf,
    ScriptSourceObject** sourceObjectOut /* = nullptr */) {
  return CreateGlobalScript(info, srcBuf, sourceObjectOut);
}

template <typename Unit>
static JSScript* CreateEvalScript(frontend::EvalScriptInfo& info,
                                  SourceText<Unit>& srcBuf) {
  AutoAssertReportedException assertException(info.context());

  frontend::ScriptCompiler<Unit> compiler(srcBuf);
  if (!compiler.prepareScriptParse(info)) {
    return nullptr;
  }

  JSScript* script =
      compiler.compileScript(info, info.environment(), info.sharedContext());
  if (!script) {
    return nullptr;
  }

  assertException.reset();
  return script;
}

JSScript* frontend::CompileEvalScript(EvalScriptInfo& info,
                                      JS::SourceText<char16_t>& srcBuf) {
  return CreateEvalScript(info, srcBuf);
}

template <typename Unit>
class MOZ_STACK_CLASS frontend::ModuleCompiler final
    : public SourceAwareCompiler<Unit> {
  using Base = SourceAwareCompiler<Unit>;

  using Base::assertSourceParserAndScriptCreated;
  using Base::createCompleteScript;
  using Base::createSourceAndParser;
  using Base::emplaceEmitter;
  using Base::parser;

 public:
  explicit ModuleCompiler(SourceText<Unit>& srcBuf) : Base(srcBuf) {}

  ModuleObject* compile(ModuleInfo& info);
};

template <typename Unit>
class MOZ_STACK_CLASS frontend::StandaloneFunctionCompiler final
    : public SourceAwareCompiler<Unit> {
  using Base = SourceAwareCompiler<Unit>;

  using Base::assertSourceAndParserCreated;
  using Base::createSourceAndParser;
  using Base::emplaceEmitter;
  using Base::handleParseFailure;
  using Base::parser;
  using Base::sourceBuffer_;

  using typename Base::TokenStreamPosition;

 public:
  explicit StandaloneFunctionCompiler(SourceText<Unit>& srcBuf)
      : Base(srcBuf) {}

  MOZ_MUST_USE bool prepare(StandaloneFunctionInfo& info,
                            const Maybe<uint32_t>& parameterListEnd) {
    return createSourceAndParser(info, ParseGoal::Script, parameterListEnd);
  }

  FunctionNode* parse(StandaloneFunctionInfo& info, HandleFunction fun,
                      HandleScope enclosingScope, GeneratorKind generatorKind,
                      FunctionAsyncKind asyncKind,
                      const Maybe<uint32_t>& parameterListEnd);

  MOZ_MUST_USE bool compile(MutableHandleFunction fun,
                            StandaloneFunctionInfo& info,
                            FunctionNode* parsedFunction);

 private:
  // Create a script for a function with the given toString offsets in source
  // text.
  MOZ_MUST_USE bool createFunctionScript(StandaloneFunctionInfo& info,
                                         uint32_t toStringStart,
                                         uint32_t toStringEnd) {
    return info.internalCreateScript(toStringStart, toStringEnd,
                                     sourceBuffer_.length());
  }
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
                         funbox->startLine, funbox->startColumn);
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

BytecodeCompiler::BytecodeCompiler(JSContext* cx,
                                   const ReadOnlyCompileOptions& options)
    : keepAtoms(cx),
      cx(cx),
      options(options),
      sourceObject(cx),
      directives(options.forceStrictMode()),
      script(cx) {}

bool BytecodeCompiler::createScriptSource(
    const Maybe<uint32_t>& parameterListEnd) {
  sourceObject = CreateScriptSourceObject(cx, options, parameterListEnd);
  if (!sourceObject) {
    return false;
  }

  scriptSource = sourceObject->source();
  return true;
}

bool BytecodeCompiler::canLazilyParse() const {
  return !options.discardSource && !options.sourceIsLazy &&
         !options.forceFullParse();
}

template <typename Unit>
bool frontend::SourceAwareCompiler<Unit>::createSourceAndParser(
    BytecodeCompiler& info, ParseGoal goal,
    const Maybe<uint32_t>& parameterListEnd /* = Nothing() */) {
  if (!info.createScriptSource(parameterListEnd)) {
    return false;
  }

  if (!info.assignSource(sourceBuffer_)) {
    return false;
  }

  // Note the contents of any compiled scripts when recording/replaying.
  if (mozilla::recordreplay::IsRecordingOrReplaying()) {
    mozilla::recordreplay::NoteContentParse(
        this, info.options.filename(), "application/javascript",
        sourceBuffer_.units(), sourceBuffer_.length());
  }

  info.createUsedNames();

  if (info.canLazilyParse()) {
    syntaxParser.emplace(info.cx, info.cx->tempLifoAlloc(), info.options,
                         sourceBuffer_.units(), sourceBuffer_.length(),
                         /* foldConstants = */ false, *info.usedNames, nullptr,
                         nullptr, info.sourceObject, goal);
    if (!syntaxParser->checkOptions()) {
      return false;
    }
  }

  parser.emplace(info.cx, info.cx->tempLifoAlloc(), info.options,
                 sourceBuffer_.units(), sourceBuffer_.length(),
                 /* foldConstants = */ true, *info.usedNames,
                 syntaxParser.ptrOr(nullptr), nullptr, info.sourceObject, goal);
  parser->ss = info.scriptSource;
  return parser->checkOptions();
}

bool BytecodeCompiler::internalCreateScript(uint32_t toStringStart,
                                            uint32_t toStringEnd,
                                            uint32_t sourceBufferLength) {
  script = JSScript::Create(cx, options, sourceObject, /* sourceStart = */ 0,
                            sourceBufferLength, toStringStart, toStringEnd);
  return script != nullptr;
}

bool BytecodeCompiler::emplaceEmitter(Maybe<BytecodeEmitter>& emitter,
                                      const EitherParser& parser,
                                      SharedContext* sharedContext) {
  BytecodeEmitter::EmitterMode emitterMode = options.selfHostingMode
                                                 ? BytecodeEmitter::SelfHosting
                                                 : BytecodeEmitter::Normal;
  emitter.emplace(/* parent = */ nullptr, parser, sharedContext, script,
                  /* lazyScript = */ nullptr, options.lineno, options.column,
                  emitterMode);
  return emitter->init();
}

template <typename Unit>
bool frontend::SourceAwareCompiler<Unit>::handleParseFailure(
    BytecodeCompiler& info, const Directives& newDirectives,
    TokenStreamPosition& startPosition) {
  if (parser->hadAbortedSyntaxParse()) {
    // Hit some unrecoverable ambiguity during an inner syntax parse.
    // Syntax parsing has now been disabled in the parser, so retry
    // the parse.
    parser->clearAbortedSyntaxParse();
  } else if (parser->anyChars.hadError() || info.directives == newDirectives) {
    return false;
  }

  parser->tokenStream.seek(startPosition);

  // Assignment must be monotonic to prevent reparsing iloops
  MOZ_ASSERT_IF(info.directives.strict(), newDirectives.strict());
  MOZ_ASSERT_IF(info.directives.asmJS(), newDirectives.asmJS());
  info.directives = newDirectives;
  return true;
}

template <typename Unit>
JSScript* frontend::ScriptCompiler<Unit>::compileScript(
    BytecodeCompiler& info, HandleObject environment, SharedContext* sc) {
  assertSourceParserAndScriptCreated(info);

  TokenStreamPosition startPosition(info.keepAtoms, parser->tokenStream);

  JSContext* cx = info.cx;

  for (;;) {
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

    // Successfully parsed. Emit the script.
    AutoGeckoProfilerEntry pseudoFrame(cx, "script emit",
                                       JS::ProfilingCategoryPair::JS_Parsing);
    if (pn) {
      // Publish deferred items
      if (!parser->publishDeferredItems()) {
        return nullptr;
      }

      Maybe<BytecodeEmitter> emitter;
      if (!emplaceEmitter(info, emitter, sc)) {
        return nullptr;
      }

      if (!emitter->emitScript(pn)) {
        return nullptr;
      }

      // Success!
      break;
    }

    // Maybe we aborted a syntax parse. See if we can try again.
    if (!handleParseFailure(info, info.directives, startPosition)) {
      return nullptr;
    }

    // Reset preserved state before trying again.
    info.usedNames->reset();
    parser->getTreeHolder().resetFunctionTree();
  }

  // We have just finished parsing the source. Inform the source so that we
  // can compute statistics (e.g. how much time our functions remain lazy).
  info.script->scriptSource()->recordParseEnded();

  // Enqueue an off-thread source compression task after finishing parsing.
  if (!info.scriptSource->tryCompressOffThread(cx)) {
    return nullptr;
  }

  MOZ_ASSERT_IF(!cx->isHelperThreadContext(), !cx->isExceptionPending());

  return info.script;
}

template <typename Unit>
ModuleObject* frontend::ModuleCompiler<Unit>::compile(ModuleInfo& info) {
  if (!createSourceAndParser(info, ParseGoal::Module) ||
      !createCompleteScript(info)) {
    return nullptr;
  }

  JSContext* cx = info.cx;

  Rooted<ModuleObject*> module(cx, ModuleObject::create(cx));
  if (!module) {
    return nullptr;
  }

  module->init(info.script);

  ModuleBuilder builder(cx, module, parser.ptr());

  RootedScope enclosingScope(cx, &cx->global()->emptyGlobalScope());
  ModuleSharedContext modulesc(cx, module, enclosingScope, builder);
  ParseNode* pn = parser->moduleBody(&modulesc);
  if (!pn) {
    return nullptr;
  }

  if (!parser->publishDeferredItems()) {
    return nullptr;
  }

  Maybe<BytecodeEmitter> emitter;
  if (!emplaceEmitter(info, emitter, &modulesc)) {
    return nullptr;
  }

  if (!emitter->emitScript(pn->as<ModuleNode>().body())) {
    return nullptr;
  }

  if (!builder.initModule()) {
    return nullptr;
  }

  RootedModuleEnvironmentObject env(
      cx, ModuleEnvironmentObject::create(cx, module));
  if (!env) {
    return nullptr;
  }

  module->setInitialEnvironment(env);

  // Enqueue an off-thread source compression task after finishing parsing.
  if (!info.scriptSource->tryCompressOffThread(cx)) {
    return nullptr;
  }

  MOZ_ASSERT_IF(!cx->isHelperThreadContext(), !cx->isExceptionPending());
  return module;
}

// Parse a standalone JS function, which might appear as the value of an
// event handler attribute in an HTML <INPUT> tag, or in a Function()
// constructor.
template <typename Unit>
FunctionNode* frontend::StandaloneFunctionCompiler<Unit>::parse(
    StandaloneFunctionInfo& info, HandleFunction fun,
    HandleScope enclosingScope, GeneratorKind generatorKind,
    FunctionAsyncKind asyncKind, const Maybe<uint32_t>& parameterListEnd) {
  MOZ_ASSERT(fun);
  MOZ_ASSERT(fun->isTenured());

  assertSourceAndParserCreated(info);

  TokenStreamPosition startPosition(info.keepAtoms, parser->tokenStream);

  // Speculatively parse using the default directives implied by the context.
  // If a directive is encountered (e.g., "use strict") that changes how the
  // function should have been parsed, we backup and reparse with the new set
  // of directives.

  FunctionNode* fn;
  do {
    Directives newDirectives = info.directives;
    fn = parser->standaloneFunction(fun, enclosingScope, parameterListEnd,
                                    generatorKind, asyncKind, info.directives,
                                    &newDirectives);
    if (!fn && !handleParseFailure(info, newDirectives, startPosition)) {
      return nullptr;
    }
  } while (!fn);

  return fn;
}

// Compile a standalone JS function.
template <typename Unit>
bool frontend::StandaloneFunctionCompiler<Unit>::compile(
    MutableHandleFunction fun, StandaloneFunctionInfo& info,
    FunctionNode* parsedFunction) {
  FunctionBox* funbox = parsedFunction->funbox();
  if (funbox->isInterpreted()) {
    MOZ_ASSERT(fun == funbox->function());

    if (!createFunctionScript(info, funbox->toStringStart,
                              funbox->toStringEnd)) {
      return false;
    }

    if (!parser->publishDeferredItems()) {
      return false;
    }

    Maybe<BytecodeEmitter> emitter;
    if (!emplaceEmitter(info, emitter, funbox)) {
      return false;
    }

    if (!emitter->emitFunctionScript(parsedFunction,
                                     BytecodeEmitter::TopLevelFunction::Yes)) {
      return false;
    }
  } else {
    fun.set(funbox->function());
    MOZ_ASSERT(IsAsmJSModule(fun));
  }

  // Enqueue an off-thread source compression task after finishing parsing.
  return info.scriptSource->tryCompressOffThread(info.cx);
}

ScriptSourceObject* frontend::CreateScriptSourceObject(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    const Maybe<uint32_t>& parameterListEnd /* = Nothing() */) {
  ScriptSource* ss = cx->new_<ScriptSource>();
  if (!ss) {
    return nullptr;
  }
  ScriptSourceHolder ssHolder(ss);

  if (!ss->initFromOptions(cx, options, parameterListEnd)) {
    return nullptr;
  }

  RootedScriptSourceObject sso(cx, ScriptSourceObject::create(cx, ss));
  if (!sso) {
    return nullptr;
  }

  // Off-thread compilations do all their GC heap allocation, including the
  // SSO, in a temporary compartment. Hence, for the SSO to refer to the
  // gc-heap-allocated values in |options|, it would need cross-compartment
  // wrappers from the temporary compartment to the real compartment --- which
  // would then be inappropriate once we merged the temporary and real
  // compartments.
  //
  // Instead, we put off populating those SSO slots in off-thread compilations
  // until after we've merged compartments.
  if (!cx->isHelperThreadContext()) {
    if (!ScriptSourceObject::initFromOptions(cx, sso, options)) {
      return nullptr;
    }
  }

  return sso;
}

#if defined(JS_BUILD_BINAST)

JSScript* frontend::CompileGlobalBinASTScript(
    JSContext* cx, LifoAlloc& alloc, const ReadOnlyCompileOptions& options,
    const uint8_t* src, size_t len, ScriptSourceObject** sourceObjectOut) {
  AutoAssertReportedException assertException(cx);

  frontend::UsedNameTracker usedNames(cx);

  RootedScriptSourceObject sourceObj(cx, CreateScriptSourceObject(cx, options));

  if (!sourceObj) {
    return nullptr;
  }

  if (!sourceObj->source()->setBinASTSourceCopy(cx, src, len)) {
    return nullptr;
  }

  RootedScript script(cx,
                      JSScript::Create(cx, options, sourceObj, 0, len, 0, len));

  if (!script) {
    return nullptr;
  }

  Directives directives(options.forceStrictMode());
  GlobalSharedContext globalsc(cx, ScopeKind::Global, directives,
                               options.extraWarningsOption);

  frontend::BinASTParser<BinASTTokenReaderMultipart> parser(
      cx, alloc, usedNames, options, sourceObj);

  // Metadata stores internal pointers, so we must use the same buffer every
  // time, including for lazy parses
  ScriptSource* ss = sourceObj->source();
  BinASTSourceMetadata* metadata = nullptr;
  auto parsed =
      parser.parse(&globalsc, ss->binASTSource(), ss->length(), &metadata);

  if (parsed.isErr()) {
    return nullptr;
  }

  sourceObj->source()->setBinASTSourceMetadata(metadata);

  BytecodeEmitter bce(nullptr, &parser, &globalsc, script, nullptr, 0, 0);

  if (!bce.init()) {
    return nullptr;
  }

  ParseNode* pn = parsed.unwrap();
  if (!bce.emitScript(pn)) {
    return nullptr;
  }

  if (sourceObjectOut) {
    *sourceObjectOut = sourceObj;
  }

  assertException.reset();
  return script;
}

#endif  // JS_BUILD_BINAST

template <typename Unit>
static ModuleObject* InternalParseModule(
    JSContext* cx, const ReadOnlyCompileOptions& optionsInput,
    SourceText<Unit>& srcBuf, ScriptSourceObject** sourceObjectOut) {
  MOZ_ASSERT(srcBuf.get());
  MOZ_ASSERT_IF(sourceObjectOut, *sourceObjectOut == nullptr);

  AutoAssertReportedException assertException(cx);

  CompileOptions options(cx, optionsInput);
  options.setForceStrictMode();  // ES6 10.2.1 Module code is always strict mode
                                 // code.
  options.setIsRunOnce(true);
  options.allowHTMLComments = false;

  ModuleInfo info(cx, options);
  AutoInitializeSourceObject autoSSO(info, sourceObjectOut);

  ModuleCompiler<Unit> compiler(srcBuf);
  ModuleObject* module = compiler.compile(info);
  if (!module) {
    return nullptr;
  }

  assertException.reset();
  return module;
}

ModuleObject* frontend::ParseModule(JSContext* cx,
                                    const ReadOnlyCompileOptions& optionsInput,
                                    SourceText<char16_t>& srcBuf,
                                    ScriptSourceObject** sourceObjectOut) {
  return InternalParseModule(cx, optionsInput, srcBuf, sourceObjectOut);
}

ModuleObject* frontend::ParseModule(JSContext* cx,
                                    const ReadOnlyCompileOptions& optionsInput,
                                    SourceText<Utf8Unit>& srcBuf,
                                    ScriptSourceObject** sourceObjectOut) {
  return InternalParseModule(cx, optionsInput, srcBuf, sourceObjectOut);
}

template <typename Unit>
static ModuleObject* CreateModule(JSContext* cx,
                                  const JS::ReadOnlyCompileOptions& options,
                                  SourceText<Unit>& srcBuf) {
  AutoAssertReportedException assertException(cx);

  if (!GlobalObject::ensureModulePrototypesCreated(cx, cx->global())) {
    return nullptr;
  }

  RootedModuleObject module(cx, ParseModule(cx, options, srcBuf, nullptr));
  if (!module) {
    return nullptr;
  }

  // This happens in GlobalHelperThreadState::finishModuleParseTask() when a
  // module is compiled off thread.
  if (!ModuleObject::Freeze(cx, module)) {
    return nullptr;
  }

  assertException.reset();
  return module;
}

ModuleObject* frontend::CompileModule(JSContext* cx,
                                      const JS::ReadOnlyCompileOptions& options,
                                      SourceText<char16_t>& srcBuf) {
  return CreateModule(cx, options, srcBuf);
}

ModuleObject* frontend::CompileModule(JSContext* cx,
                                      const JS::ReadOnlyCompileOptions& options,
                                      SourceText<Utf8Unit>& srcBuf) {
  return CreateModule(cx, options, srcBuf);
}

// When leaving this scope, the given function should either:
//   * be linked to a fully compiled script
//   * remain linking to a lazy script
class MOZ_STACK_CLASS AutoAssertFunctionDelazificationCompletion {
#ifdef DEBUG
  RootedFunction fun_;
#endif

 public:
  AutoAssertFunctionDelazificationCompletion(JSContext* cx, HandleFunction fun)
#ifdef DEBUG
      : fun_(cx, fun)
#endif
  {
    MOZ_ASSERT(fun_->isInterpretedLazy());
    MOZ_ASSERT(!fun_->lazyScript()->hasScript());
  }

  ~AutoAssertFunctionDelazificationCompletion() {
#ifdef DEBUG
    if (!fun_) {
      return;
    }
#endif

    // If fun_ is not nullptr, it means delazification doesn't complete.
    // Assert that the function keeps linking to lazy script
    MOZ_ASSERT(fun_->isInterpretedLazy());
    MOZ_ASSERT(!fun_->lazyScript()->hasScript());
  }

  void complete() {
    // Assert the completion of delazification and forget the function.
    MOZ_ASSERT(fun_->hasScript());
    MOZ_ASSERT(!fun_->hasUncompletedScript());

#ifdef DEBUG
    fun_ = nullptr;
#endif
  }
};

template <typename Unit>
static bool CompileLazyFunctionImpl(JSContext* cx, Handle<LazyScript*> lazy,
                                    const Unit* units, size_t length) {
  MOZ_ASSERT(cx->compartment() ==
             lazy->functionNonDelazifying()->compartment());

  // We can only compile functions whose parents have previously been
  // compiled, because compilation requires full information about the
  // function's immediately enclosing scope.
  MOZ_ASSERT(lazy->enclosingScriptHasEverBeenCompiled());

  MOZ_ASSERT(!lazy->isBinAST());

  AutoAssertReportedException assertException(cx);
  Rooted<JSFunction*> fun(cx, lazy->functionNonDelazifying());
  AutoAssertFunctionDelazificationCompletion delazificationCompletion(cx, fun);

  JS::CompileOptions options(cx);
  options.setMutedErrors(lazy->mutedErrors())
      .setFileAndLine(lazy->filename(), lazy->lineno())
      .setColumn(lazy->column())
      .setScriptSourceOffset(lazy->sourceStart())
      .setNoScriptRval(false)
      .setSelfHostingMode(false);

  // Update statistics to find out if we are delazifying just after having
  // lazified. Note that we are interested in the delta between end of
  // syntax parsing and start of full parsing, so we do this now rather than
  // after parsing below.
  if (!lazy->scriptSource()->parseEnded().IsNull()) {
    const mozilla::TimeDuration delta =
        ReallyNow() - lazy->scriptSource()->parseEnded();

    // Differentiate between web-facing and privileged code, to aid
    // with optimization. Due to the number of calls to this function,
    // we use `cx->runningWithTrustedPrincipals`, which is fast but
    // will classify addons alongside with web-facing code.
    const int HISTOGRAM =
        cx->runningWithTrustedPrincipals()
            ? JS_TELEMETRY_PRIVILEGED_PARSER_COMPILE_LAZY_AFTER_MS
            : JS_TELEMETRY_WEB_PARSER_COMPILE_LAZY_AFTER_MS;
    cx->runtime()->addTelemetry(HISTOGRAM, delta.ToMilliseconds());
  }

  UsedNameTracker usedNames(cx);

  RootedScriptSourceObject sourceObject(cx, lazy->sourceObject());
  Parser<FullParseHandler, Unit> parser(
      cx, cx->tempLifoAlloc(), options, units, length,
      /* foldConstants = */ true, usedNames, nullptr, lazy, sourceObject,
      lazy->parseGoal());
  if (!parser.checkOptions()) {
    return false;
  }

  FunctionNode* pn =
      parser.standaloneLazyFunction(fun, lazy->toStringStart(), lazy->strict(),
                                    lazy->generatorKind(), lazy->asyncKind());
  if (!pn) {
    return false;
  }

  Rooted<JSScript*> script(cx, JSScript::CreateFromLazy(cx, lazy));
  if (!script) {
    return false;
  }

  if (lazy->isLikelyConstructorWrapper()) {
    script->setLikelyConstructorWrapper();
  }
  if (lazy->hasBeenCloned()) {
    script->setHasBeenCloned();
  }

  FieldInitializers fieldInitializers = FieldInitializers::Invalid();
  if (fun->kind() == FunctionFlags::FunctionKind::ClassConstructor) {
    fieldInitializers = lazy->getFieldInitializers();
  }

  BytecodeEmitter bce(/* parent = */ nullptr, &parser, pn->funbox(), script,
                      lazy, lazy->lineno(), lazy->column(),
                      BytecodeEmitter::LazyFunction, fieldInitializers);
  if (!bce.init(pn->pn_pos)) {
    return false;
  }

  if (!bce.emitFunctionScript(pn, BytecodeEmitter::TopLevelFunction::Yes)) {
    return false;
  }

  MOZ_ASSERT(lazy->hasDirectEval() == script->hasDirectEval());

  delazificationCompletion.complete();
  assertException.reset();
  return true;
}

bool frontend::CompileLazyFunction(JSContext* cx, Handle<LazyScript*> lazy,
                                   const char16_t* units, size_t length) {
  return CompileLazyFunctionImpl(cx, lazy, units, length);
}

bool frontend::CompileLazyFunction(JSContext* cx, Handle<LazyScript*> lazy,
                                   const Utf8Unit* units, size_t length) {
  return CompileLazyFunctionImpl(cx, lazy, units, length);
}

#ifdef JS_BUILD_BINAST

bool frontend::CompileLazyBinASTFunction(JSContext* cx,
                                         Handle<LazyScript*> lazy,
                                         const uint8_t* buf, size_t length) {
  MOZ_ASSERT(cx->compartment() ==
             lazy->functionNonDelazifying()->compartment());

  // We can only compile functions whose parents have previously been
  // compiled, because compilation requires full information about the
  // function's immediately enclosing scope.
  MOZ_ASSERT(lazy->enclosingScriptHasEverBeenCompiled());
  MOZ_ASSERT(lazy->isBinAST());

  AutoAssertReportedException assertException(cx);
  Rooted<JSFunction*> fun(cx, lazy->functionNonDelazifying());
  AutoAssertFunctionDelazificationCompletion delazificationCompletion(cx, fun);

  CompileOptions options(cx);
  options.setMutedErrors(lazy->mutedErrors())
      .setFileAndLine(lazy->filename(), lazy->lineno())
      .setColumn(lazy->column())
      .setScriptSourceOffset(lazy->sourceStart())
      .setNoScriptRval(false)
      .setSelfHostingMode(false);

  UsedNameTracker usedNames(cx);

  RootedScriptSourceObject sourceObj(cx, lazy->sourceObject());
  MOZ_ASSERT(sourceObj);

  RootedScript script(
      cx, JSScript::Create(cx, options, sourceObj, lazy->sourceStart(),
                           lazy->sourceEnd(), lazy->sourceStart(),
                           lazy->sourceEnd()));

  if (!script) {
    return false;
  }

  if (lazy->hasBeenCloned()) {
    script->setHasBeenCloned();
  }

  frontend::BinASTParser<BinASTTokenReaderMultipart> parser(
      cx, cx->tempLifoAlloc(), usedNames, options, sourceObj, lazy);

  auto parsed =
      parser.parseLazyFunction(lazy->scriptSource(), lazy->sourceStart());

  if (parsed.isErr()) {
    return false;
  }

  FunctionNode* pn = parsed.unwrap();

  BytecodeEmitter bce(nullptr, &parser, pn->funbox(), script, lazy,
                      lazy->lineno(), lazy->column(),
                      BytecodeEmitter::LazyFunction);

  if (!bce.init(pn->pn_pos)) {
    return false;
  }

  if (!bce.emitFunctionScript(pn, BytecodeEmitter::TopLevelFunction::Yes)) {
    return false;
  }

  delazificationCompletion.complete();
  assertException.reset();
  return script;
}

#endif  // JS_BUILD_BINAST

static bool CompileStandaloneFunction(JSContext* cx, MutableHandleFunction fun,
                                      const JS::ReadOnlyCompileOptions& options,
                                      JS::SourceText<char16_t>& srcBuf,
                                      const Maybe<uint32_t>& parameterListEnd,
                                      GeneratorKind generatorKind,
                                      FunctionAsyncKind asyncKind,
                                      HandleScope enclosingScope = nullptr) {
  AutoAssertReportedException assertException(cx);

  StandaloneFunctionInfo info(cx, options);

  StandaloneFunctionCompiler<char16_t> compiler(srcBuf);
  if (!compiler.prepare(info, parameterListEnd)) {
    return false;
  }

  RootedScope scope(cx, enclosingScope);
  if (!scope) {
    scope = &cx->global()->emptyGlobalScope();
  }

  FunctionNode* parsedFunction = compiler.parse(info, fun, scope, generatorKind,
                                                asyncKind, parameterListEnd);
  if (!parsedFunction || !compiler.compile(fun, info, parsedFunction)) {
    return false;
  }

  assertException.reset();
  return true;
}

bool frontend::CompileStandaloneFunction(
    JSContext* cx, MutableHandleFunction fun,
    const JS::ReadOnlyCompileOptions& options, JS::SourceText<char16_t>& srcBuf,
    const Maybe<uint32_t>& parameterListEnd,
    HandleScope enclosingScope /* = nullptr */) {
  return CompileStandaloneFunction(
      cx, fun, options, srcBuf, parameterListEnd, GeneratorKind::NotGenerator,
      FunctionAsyncKind::SyncFunction, enclosingScope);
}

bool frontend::CompileStandaloneGenerator(
    JSContext* cx, MutableHandleFunction fun,
    const JS::ReadOnlyCompileOptions& options, JS::SourceText<char16_t>& srcBuf,
    const Maybe<uint32_t>& parameterListEnd) {
  return CompileStandaloneFunction(cx, fun, options, srcBuf, parameterListEnd,
                                   GeneratorKind::Generator,
                                   FunctionAsyncKind::SyncFunction);
}

bool frontend::CompileStandaloneAsyncFunction(
    JSContext* cx, MutableHandleFunction fun,
    const ReadOnlyCompileOptions& options, JS::SourceText<char16_t>& srcBuf,
    const Maybe<uint32_t>& parameterListEnd) {
  return CompileStandaloneFunction(cx, fun, options, srcBuf, parameterListEnd,
                                   GeneratorKind::NotGenerator,
                                   FunctionAsyncKind::AsyncFunction);
}

bool frontend::CompileStandaloneAsyncGenerator(
    JSContext* cx, MutableHandleFunction fun,
    const ReadOnlyCompileOptions& options, JS::SourceText<char16_t>& srcBuf,
    const Maybe<uint32_t>& parameterListEnd) {
  return CompileStandaloneFunction(cx, fun, options, srcBuf, parameterListEnd,
                                   GeneratorKind::Generator,
                                   FunctionAsyncKind::AsyncFunction);
}
