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
  MOZ_MUST_USE bool prepareScriptParse(LifoAllocScope& allocScope,
                                       BytecodeCompiler& info) {
    return createSourceAndParser(allocScope, info) &&
           createCompleteScript(info);
  }

  void assertSourceAndParserCreated(BytecodeCompiler& info) const {
    info.assertSourceCreated();
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

  MOZ_MUST_USE bool createSourceAndParser(LifoAllocScope& allocScope,
                                          BytecodeCompiler& compiler);

  // This assumes the created script's offsets in the source used to parse it
  // are the same as are used to compute its Function.prototype.toString()
  // value.
  MOZ_MUST_USE bool createCompleteScript(BytecodeCompiler& info) {
    JSContext* cx = info.cx;
    RootedObject global(cx, cx->global());
    uint32_t toStringStart = 0;
    uint32_t len = sourceBuffer_.length();
    uint32_t toStringEnd = len;
    return info.internalCreateScript(global, toStringStart, toStringEnd, len);
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

  MOZ_MUST_USE bool prepareScriptParse(LifoAllocScope& allocScope,
                                       BytecodeCompiler& compiler) {
    return Base::prepareScriptParse(allocScope, compiler);
  }

  JSScript* compileScript(BytecodeCompiler& compiler, HandleObject environment,
                          SharedContext* sc);
};

/* If we're on main thread, tell the Debugger about a newly compiled script.
 *
 * See: finishSingleParseTask/finishMultiParseTask for the off-thread case.
 */
static void tellDebuggerAboutCompiledScript(JSContext* cx,
                                            Handle<JSScript*> script) {
  if (cx->isHelperThreadContext()) {
    return;
  }
  DebugAPI::onNewScript(cx, script);
}

template <typename Unit>
static JSScript* CreateGlobalScript(GlobalScriptInfo& info,
                                    JS::SourceText<Unit>& srcBuf) {
  AutoAssertReportedException assertException(info.context());

  LifoAllocScope allocScope(&info.context()->tempLifoAlloc());
  frontend::ScriptCompiler<Unit> compiler(srcBuf);

  if (!compiler.prepareScriptParse(allocScope, info)) {
    return nullptr;
  }

  if (!compiler.compileScript(info, nullptr, info.sharedContext())) {
    return nullptr;
  }

  tellDebuggerAboutCompiledScript(info.context(), info.getScript());

  assertException.reset();
  return info.getScript();
}

JSScript* frontend::CompileGlobalScript(GlobalScriptInfo& info,
                                        JS::SourceText<char16_t>& srcBuf) {
  return CreateGlobalScript(info, srcBuf);
}

JSScript* frontend::CompileGlobalScript(GlobalScriptInfo& info,
                                        JS::SourceText<Utf8Unit>& srcBuf) {
  return CreateGlobalScript(info, srcBuf);
}

template <typename Unit>
static JSScript* CreateEvalScript(frontend::EvalScriptInfo& info,
                                  SourceText<Unit>& srcBuf) {
  AutoAssertReportedException assertException(info.context());
  LifoAllocScope allocScope(&info.context()->tempLifoAlloc());

  frontend::ScriptCompiler<Unit> compiler(srcBuf);
  if (!compiler.prepareScriptParse(allocScope, info)) {
    return nullptr;
  }

  if (!compiler.compileScript(info, info.environment(), info.sharedContext())) {
    return nullptr;
  }

  tellDebuggerAboutCompiledScript(info.context(), info.getScript());

  assertException.reset();
  return info.getScript();
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

  ModuleObject* compile(LifoAllocScope& allocScope, ModuleInfo& info);
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

  MOZ_MUST_USE bool prepare(LifoAllocScope& allocScope,
                            StandaloneFunctionInfo& info) {
    return createSourceAndParser(allocScope, info);
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
                                         HandleObject function,
                                         uint32_t toStringStart,
                                         uint32_t toStringEnd) {
    return info.internalCreateScript(function, toStringStart, toStringEnd,
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

BytecodeCompiler::BytecodeCompiler(JSContext* cx, ParseInfo& parseInfo,
                                   const ReadOnlyCompileOptions& options)
    : keepAtoms(cx),
      cx(cx),
      options(options),
      parseInfo(parseInfo),
      directives(options.forceStrictMode()),
      script(cx) {}

bool BytecodeCompiler::canLazilyParse() const {
  return !options.discardSource && !options.sourceIsLazy &&
         !options.forceFullParse();
}

template <typename Unit>
bool frontend::SourceAwareCompiler<Unit>::createSourceAndParser(
    LifoAllocScope& allocScope, BytecodeCompiler& info) {
  if (!info.assignSource(sourceBuffer_)) {
    return false;
  }

  // Note the contents of any compiled scripts when recording/replaying.
  if (mozilla::recordreplay::IsRecordingOrReplaying()) {
    mozilla::recordreplay::NoteContentParse(
        this, info.options.filename(), "application/javascript",
        sourceBuffer_.units(), sourceBuffer_.length());
  }

  if (info.canLazilyParse()) {
    syntaxParser.emplace(info.cx, info.options, sourceBuffer_.units(),
                         sourceBuffer_.length(),
                         /* foldConstants = */ false, info.parseInfo, nullptr,
                         nullptr, info.parseInfo.sourceObject);
    if (!syntaxParser->checkOptions()) {
      return false;
    }
  }

  parser.emplace(
      info.cx, info.options, sourceBuffer_.units(), sourceBuffer_.length(),
      /* foldConstants = */ true, info.parseInfo, syntaxParser.ptrOr(nullptr),
      nullptr, info.parseInfo.sourceObject);
  parser->ss = info.scriptSource();
  return parser->checkOptions();
}

bool BytecodeCompiler::internalCreateScript(HandleObject functionOrGlobal,
                                            uint32_t toStringStart,
                                            uint32_t toStringEnd,
                                            uint32_t sourceBufferLength) {
  script =
      JSScript::Create(cx, functionOrGlobal, options, parseInfo.sourceObject,
                       /* sourceStart = */ 0, sourceBufferLength, toStringStart,
                       toStringEnd, options.lineno, options.column);
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
                  parseInfo, emitterMode);
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

  // We are about to start parsing the source. Record this information for
  // telemetry purposes.
  info.script->scriptSource()->recordParseStarted();

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
      // We are about to start emitting bytecode. Record this information for
      // telemetry purposes.
      info.script->scriptSource()->recordEmitStarted();

      // Publish deferred items
      if (!parser->publishDeferredFunctions()) {
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
    info.parseInfo.usedNames.reset();
    parser->getTreeHolder().resetFunctionTree();
  }

  // We have just finished parsing the source. Inform the source so that we
  // can compute statistics (e.g. how much time our functions remain lazy).
  info.script->scriptSource()->recordParseEnded();

  // Enqueue an off-thread source compression task after finishing parsing.
  if (!info.scriptSource()->tryCompressOffThread(cx)) {
    return nullptr;
  }

  MOZ_ASSERT_IF(!cx->isHelperThreadContext(), !cx->isExceptionPending());

  return info.script;
}

template <typename Unit>
ModuleObject* frontend::ModuleCompiler<Unit>::compile(
    LifoAllocScope& allocScope, ModuleInfo& info) {
  if (!createSourceAndParser(allocScope, info) || !createCompleteScript(info)) {
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

  if (!parser->publishDeferredFunctions()) {
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

  if (!ModuleObject::createEnvironment(cx, module)) {
    return nullptr;
  }

  // Enqueue an off-thread source compression task after finishing parsing.
  if (!info.scriptSource()->tryCompressOffThread(cx)) {
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

    if (!createFunctionScript(info, fun, funbox->toStringStart,
                              funbox->toStringEnd)) {
      return false;
    }

    if (!parser->publishDeferredFunctions()) {
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
  return info.scriptSource()->tryCompressOffThread(info.cx);
}

ScriptSourceObject* frontend::CreateScriptSourceObject(
    JSContext* cx, const ReadOnlyCompileOptions& options) {
  ScriptSource* ss = cx->new_<ScriptSource>();
  if (!ss) {
    return nullptr;
  }
  ScriptSourceHolder ssHolder(ss);

  if (!ss->initFromOptions(cx, options)) {
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

template <class ParserT>
static JSScript* CompileGlobalBinASTScriptImpl(
    JSContext* cx, const ReadOnlyCompileOptions& options, const uint8_t* src,
    size_t len, JS::BinASTFormat format, ScriptSourceObject** sourceObjectOut) {
  AutoAssertReportedException assertException(cx);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  ParseInfo parseInfo(cx, allocScope);
  if (!parseInfo.initFromOptions(cx, options)) {
    return nullptr;
  }

  if (!parseInfo.sourceObject->source()->setBinASTSourceCopy(cx, src, len)) {
    return nullptr;
  }

  RootedScript script(
      cx, JSScript::Create(cx, cx->global(), options, parseInfo.sourceObject, 0,
                           len, 0, len, 0, 0));

  if (!script) {
    return nullptr;
  }

  Directives directives(options.forceStrictMode());
  GlobalSharedContext globalsc(cx, ScopeKind::Global, directives,
                               options.extraWarningsOption);

  frontend::BinASTParser<ParserT> parser(cx, parseInfo, options,
                                         parseInfo.sourceObject);

  // Metadata stores internal pointers, so we must use the same buffer every
  // time, including for lazy parses
  ScriptSource* ss = parseInfo.sourceObject->source();
  BinASTSourceMetadata* metadata = nullptr;
  auto parsed =
      parser.parse(&globalsc, ss->binASTSource(), ss->length(), &metadata);

  if (parsed.isErr()) {
    return nullptr;
  }

  parseInfo.sourceObject->source()->setBinASTSourceMetadata(metadata);

  BytecodeEmitter bce(nullptr, &parser, &globalsc, script, nullptr, 0, 0,
                      parseInfo);

  if (!bce.init()) {
    return nullptr;
  }

  ParseNode* pn = parsed.unwrap();
  if (!bce.emitScript(pn)) {
    return nullptr;
  }

  if (sourceObjectOut) {
    *sourceObjectOut = parseInfo.sourceObject;
  }

  tellDebuggerAboutCompiledScript(cx, script);

  assertException.reset();
  return script;
}

JSScript* frontend::CompileGlobalBinASTScript(
    JSContext* cx, const ReadOnlyCompileOptions& options, const uint8_t* src,
    size_t len, JS::BinASTFormat format, ScriptSourceObject** sourceObjectOut) {
  if (format == JS::BinASTFormat::Multipart) {
    return CompileGlobalBinASTScriptImpl<BinASTTokenReaderMultipart>(
        cx, options, src, len, format, sourceObjectOut);
  }

  MOZ_ASSERT(format == JS::BinASTFormat::Context);
  return CompileGlobalBinASTScriptImpl<BinASTTokenReaderContext>(
      cx, options, src, len, format, sourceObjectOut);
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

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  ParseInfo parseInfo(cx, allocScope);
  if (!parseInfo.initFromOptions(cx, options)) {
    return nullptr;
  }

  ModuleInfo info(cx, parseInfo, options);

  AutoInitializeSourceObject autoSSO(info, sourceObjectOut);

  ModuleCompiler<Unit> compiler(srcBuf);
  Rooted<ModuleObject*> module(cx, compiler.compile(allocScope, info));
  if (!module) {
    return nullptr;
  }

  tellDebuggerAboutCompiledScript(cx, info.getScript());

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

  void checkIsLazy() {
    MOZ_ASSERT(fun_->isInterpretedLazy(), "Function should remain lazy");
    MOZ_ASSERT(!fun_->lazyScript()->hasScript(),
               "LazyScript should not have a script");
  }

 public:
  AutoAssertFunctionDelazificationCompletion(JSContext* cx, HandleFunction fun)
#ifdef DEBUG
      : fun_(cx, fun)
#endif
  {
    checkIsLazy();
  }

  ~AutoAssertFunctionDelazificationCompletion() {
#ifdef DEBUG
    if (!fun_) {
      return;
    }
#endif

    // If fun_ is not nullptr, it means delazification doesn't complete.
    // Assert that the function keeps linking to lazy script
    checkIsLazy();
  }

  void complete() {
    // Assert the completion of delazification and forget the function.
    MOZ_ASSERT(fun_->nonLazyScript());

#ifdef DEBUG
    fun_ = nullptr;
#endif
  }
};

static void CheckFlagsOnDelazification(uint32_t lazy, uint32_t nonLazy) {
#ifdef DEBUG
  // These flags are expect to be unset for lazy scripts and are only valid
  // after a script has been compiled with the full parser.
  constexpr uint32_t NonLazyFlagsMask =
      uint32_t(BaseScript::ImmutableFlags::HasNonSyntacticScope) |
      uint32_t(BaseScript::ImmutableFlags::FunctionHasExtraBodyVarScope) |
      uint32_t(BaseScript::ImmutableFlags::NeedsFunctionEnvironmentObjects);

  // These flags are computed for lazy scripts and may have a different
  // definition for non-lazy scripts.
  //
  //  HasInnerFunctions:  The full parse performs basic analysis for dead code
  //                      and may remove inner functions that existed after lazy
  //                      parse.
  //  TreatAsRunOnce:     Some conditions depend on parent context and are
  //                      computed during lazy parsing, while other conditions
  //                      need to full parse.
  constexpr uint32_t CustomFlagsMask =
      uint32_t(BaseScript::ImmutableFlags::HasInnerFunctions) |
      uint32_t(BaseScript::ImmutableFlags::TreatAsRunOnce);

  // These flags are expected to match between lazy and full parsing.
  constexpr uint32_t MatchedFlagsMask = ~(NonLazyFlagsMask | CustomFlagsMask);

  MOZ_ASSERT((lazy & NonLazyFlagsMask) == 0);
  MOZ_ASSERT((lazy & MatchedFlagsMask) == (nonLazy & MatchedFlagsMask));
#endif  // DEBUG
}

template <typename Unit>
static bool CompileLazyFunctionImpl(JSContext* cx, Handle<LazyScript*> lazy,
                                    const Unit* units, size_t length) {
  MOZ_ASSERT(cx->compartment() == lazy->compartment());

  // We can only compile functions whose parents have previously been
  // compiled, because compilation requires full information about the
  // function's immediately enclosing scope.
  MOZ_ASSERT(lazy->enclosingScriptHasEverBeenCompiled());

  MOZ_ASSERT(!lazy->isBinAST());

  AutoAssertReportedException assertException(cx);
  Rooted<JSFunction*> fun(cx, lazy->function());
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

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  ParseInfo parseInfo(cx, allocScope);
  parseInfo.initFromSourceObject(lazy->sourceObject());

  Parser<FullParseHandler, Unit> parser(cx, options, units, length,
                                        /* foldConstants = */ true, parseInfo,
                                        nullptr, lazy, parseInfo.sourceObject);
  if (!parser.checkOptions()) {
    return false;
  }

  FunctionNode* pn =
      parser.standaloneLazyFunction(fun, lazy->toStringStart(), lazy->strict(),
                                    lazy->generatorKind(), lazy->asyncKind());
  if (!pn) {
    return false;
  }
  if (!parser.publishDeferredFunctions()) {
    return false;
  }

  Rooted<JSScript*> script(cx, JSScript::CreateFromLazy(cx, lazy));
  if (!script) {
    return false;
  }

  if (lazy->isLikelyConstructorWrapper()) {
    script->setIsLikelyConstructorWrapper();
  }
  if (lazy->hasBeenCloned()) {
    script->setHasBeenCloned();
  }

  FieldInitializers fieldInitializers = FieldInitializers::Invalid();
  if (fun->isClassConstructor()) {
    fieldInitializers = lazy->getFieldInitializers();
  }

  BytecodeEmitter bce(/* parent = */ nullptr, &parser, pn->funbox(), script,
                      lazy, lazy->lineno(), lazy->column(), parseInfo,
                      BytecodeEmitter::LazyFunction, fieldInitializers);
  if (!bce.init(pn->pn_pos)) {
    return false;
  }

  if (!bce.emitFunctionScript(pn, BytecodeEmitter::TopLevelFunction::Yes)) {
    return false;
  }

  CheckFlagsOnDelazification(lazy->immutableFlags(), script->immutableFlags());

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

template <class ParserT>
static bool CompileLazyBinASTFunctionImpl(JSContext* cx,
                                          Handle<LazyScript*> lazy,
                                          const uint8_t* buf, size_t length) {
  MOZ_ASSERT(cx->compartment() == lazy->compartment());

  // We can only compile functions whose parents have previously been
  // compiled, because compilation requires full information about the
  // function's immediately enclosing scope.
  MOZ_ASSERT(lazy->enclosingScriptHasEverBeenCompiled());
  MOZ_ASSERT(lazy->isBinAST());

  AutoAssertReportedException assertException(cx);
  Rooted<JSFunction*> fun(cx, lazy->function());
  AutoAssertFunctionDelazificationCompletion delazificationCompletion(cx, fun);

  CompileOptions options(cx);
  options.setMutedErrors(lazy->mutedErrors())
      .setFileAndLine(lazy->filename(), lazy->lineno())
      .setColumn(lazy->column())
      .setScriptSourceOffset(lazy->sourceStart())
      .setNoScriptRval(false)
      .setSelfHostingMode(false);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  ParseInfo parseInfo(cx, allocScope);
  parseInfo.initFromSourceObject(lazy->sourceObject());

  RootedScript script(cx, JSScript::CreateFromLazy(cx, lazy));

  if (!script) {
    return false;
  }

  if (lazy->hasBeenCloned()) {
    script->setHasBeenCloned();
  }

  frontend::BinASTParser<ParserT> parser(cx, parseInfo, options,
                                         parseInfo.sourceObject, lazy);

  auto parsed =
      parser.parseLazyFunction(lazy->scriptSource(), lazy->sourceStart());

  if (parsed.isErr()) {
    return false;
  }

  FunctionNode* pn = parsed.unwrap();

  BytecodeEmitter bce(nullptr, &parser, pn->funbox(), script, lazy,
                      lazy->lineno(), lazy->column(), parseInfo,
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

bool frontend::CompileLazyBinASTFunction(JSContext* cx,
                                         Handle<LazyScript*> lazy,
                                         const uint8_t* buf, size_t length) {
  if (lazy->scriptSource()->binASTSourceMetadata()->isMultipart()) {
    return CompileLazyBinASTFunctionImpl<BinASTTokenReaderMultipart>(
        cx, lazy, buf, length);
  }

  MOZ_ASSERT(lazy->scriptSource()->binASTSourceMetadata()->isContext());
  return CompileLazyBinASTFunctionImpl<BinASTTokenReaderContext>(cx, lazy, buf,
                                                                 length);
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

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  ParseInfo parseInfo(cx, allocScope);
  if (!parseInfo.initFromOptions(cx, options)) {
    return false;
  }

  StandaloneFunctionInfo info(cx, parseInfo, options);

  StandaloneFunctionCompiler<char16_t> compiler(srcBuf);
  if (!compiler.prepare(allocScope, info)) {
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

  // Note: If AsmJS successfully compiles, the into.script will still be
  // nullptr. In this case we have compiled to a native function instead of an
  // interpreted script.
  if (info.getScript()) {
    if (parameterListEnd) {
      info.getScript()->scriptSource()->setParameterListEnd(*parameterListEnd);
    }
    tellDebuggerAboutCompiledScript(cx, info.getScript());
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

bool frontend::ParseInfo::initFromOptions(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options) {
  sourceObject = CreateScriptSourceObject(cx, options);
  return !!sourceObject;
}