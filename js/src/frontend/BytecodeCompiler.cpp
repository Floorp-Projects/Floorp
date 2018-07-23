/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeCompiler.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Maybe.h"

#include "builtin/ModuleObject.h"
#if defined(JS_BUILD_BINAST)
# include "frontend/BinSource.h"
#endif // JS_BUILD_BINAST
#include "frontend/BytecodeEmitter.h"
#include "frontend/ErrorReporter.h"
#include "frontend/FoldConstants.h"
#include "frontend/Parser.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/JSScript.h"
#include "vm/TraceLogging.h"
#include "wasm/AsmJS.h"

#include "vm/EnvironmentObject-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::frontend;
using mozilla::Maybe;
using mozilla::Nothing;

// The BytecodeCompiler class contains resources common to compiling scripts and
// function bodies.
class MOZ_STACK_CLASS BytecodeCompiler
{
  public:
    // Construct an object passing mandatory arguments.
    BytecodeCompiler(JSContext* cx,
                     LifoAlloc& alloc,
                     const ReadOnlyCompileOptions& options,
                     SourceBufferHolder& sourceBuffer,
                     HandleScope enclosingScope);

    JSScript* compileGlobalScript(ScopeKind scopeKind);
    JSScript* compileEvalScript(HandleObject environment, HandleScope enclosingScope);
    ModuleObject* compileModule();
    bool compileStandaloneFunction(MutableHandleFunction fun, GeneratorKind generatorKind,
                                   FunctionAsyncKind asyncKind,
                                   const Maybe<uint32_t>& parameterListEnd);

    ScriptSourceObject* sourceObjectPtr() const;

  private:
    JSScript* compileScript(HandleObject environment, SharedContext* sc);
    bool checkLength();
    bool createScriptSource(const Maybe<uint32_t>& parameterListEnd);
    bool canLazilyParse();
    bool createParser(ParseGoal goal);
    bool createSourceAndParser(ParseGoal goal,
                               const Maybe<uint32_t>& parameterListEnd = Nothing());

    // If toString{Start,End} are not explicitly passed, assume the script's
    // offsets in the source used to parse it are the same as what should be
    // used to compute its Function.prototype.toString() value.
    bool createScript();
    bool createScript(uint32_t toStringStart, uint32_t toStringEnd);

    using TokenStreamPosition = frontend::TokenStreamPosition<char16_t>;

    bool emplaceEmitter(Maybe<BytecodeEmitter>& emitter, SharedContext* sharedContext);
    bool handleParseFailure(const Directives& newDirectives, TokenStreamPosition& startPosition);
    bool deoptimizeArgumentsInEnclosingScripts(JSContext* cx, HandleObject environment);

    AutoKeepAtoms keepAtoms;

    JSContext* cx;
    LifoAlloc& alloc;
    const ReadOnlyCompileOptions& options;
    SourceBufferHolder& sourceBuffer;

    RootedScope enclosingScope;

    RootedScriptSourceObject sourceObject;
    ScriptSource* scriptSource;

    Maybe<UsedNameTracker> usedNames;
    Maybe<Parser<SyntaxParseHandler, char16_t>> syntaxParser;
    Maybe<Parser<FullParseHandler, char16_t>> parser;

    Directives directives;

    RootedScript script;
};

AutoFrontendTraceLog::AutoFrontendTraceLog(JSContext* cx, const TraceLoggerTextId id,
                                           const ErrorReporter& errorReporter)
#ifdef JS_TRACE_LOGGING
  : logger_(TraceLoggerForCurrentThread(cx))
{
    // If the tokenizer hasn't yet gotten any tokens, use the line and column
    // numbers from CompileOptions.
    uint32_t line, column;
    if (errorReporter.hasTokenizationStarted()) {
        line = errorReporter.options().lineno;
        column = errorReporter.options().column;
    } else {
        errorReporter.currentLineAndColumn(&line, &column);
    }
    frontendEvent_.emplace(TraceLogger_Frontend, errorReporter.getFilename(), line, column);
    frontendLog_.emplace(logger_, *frontendEvent_);
    typeLog_.emplace(logger_, id);
}
#else
{ }
#endif

AutoFrontendTraceLog::AutoFrontendTraceLog(JSContext* cx, const TraceLoggerTextId id,
                                           const ErrorReporter& errorReporter,
                                           FunctionBox* funbox)
#ifdef JS_TRACE_LOGGING
  : logger_(TraceLoggerForCurrentThread(cx))
{
    frontendEvent_.emplace(TraceLogger_Frontend, errorReporter.getFilename(),
                           funbox->startLine, funbox->startColumn);
    frontendLog_.emplace(logger_, *frontendEvent_);
    typeLog_.emplace(logger_, id);
}
#else
{ }
#endif

AutoFrontendTraceLog::AutoFrontendTraceLog(JSContext* cx, const TraceLoggerTextId id,
                                           const ErrorReporter& errorReporter, ParseNode* pn)
#ifdef JS_TRACE_LOGGING
  : logger_(TraceLoggerForCurrentThread(cx))
{
    uint32_t line, column;
    errorReporter.lineAndColumnAt(pn->pn_pos.begin, &line, &column);
    frontendEvent_.emplace(TraceLogger_Frontend, errorReporter.getFilename(), line, column);
    frontendLog_.emplace(logger_, *frontendEvent_);
    typeLog_.emplace(logger_, id);
}
#else
{ }
#endif

BytecodeCompiler::BytecodeCompiler(JSContext* cx,
                                   LifoAlloc& alloc,
                                   const ReadOnlyCompileOptions& options,
                                   SourceBufferHolder& sourceBuffer,
                                   HandleScope enclosingScope)
  : keepAtoms(cx),
    cx(cx),
    alloc(alloc),
    options(options),
    sourceBuffer(sourceBuffer),
    enclosingScope(cx, enclosingScope),
    sourceObject(cx),
    scriptSource(nullptr),
    directives(options.strictOption),
    script(cx)
{
    MOZ_ASSERT(sourceBuffer.get());
}

bool
BytecodeCompiler::checkLength()
{
    // Note this limit is simply so we can store sourceStart and sourceEnd in
    // JSScript as 32-bits. It could be lifted fairly easily, since the compiler
    // is using size_t internally already.
    if (sourceBuffer.length() > UINT32_MAX) {
        if (!cx->helperThread())
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_SOURCE_TOO_LONG);
        return false;
    }
    return true;
}

bool
BytecodeCompiler::createScriptSource(const Maybe<uint32_t>& parameterListEnd)
{
    if (!checkLength())
        return false;

    sourceObject = CreateScriptSourceObject(cx, options, parameterListEnd);
    if (!sourceObject)
        return false;

    scriptSource = sourceObject->source();

    if (!cx->realm()->behaviors().discardSource()) {
        if (options.sourceIsLazy) {
            scriptSource->setSourceRetrievable();
        } else if (!scriptSource->setSourceCopy(cx, sourceBuffer)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeCompiler::canLazilyParse()
{
    return options.canLazilyParse &&
           !cx->realm()->behaviors().disableLazyParsing() &&
           !cx->realm()->behaviors().discardSource() &&
           !options.sourceIsLazy &&
           !cx->lcovEnabled() &&
           // Disabled during record/replay. The replay debugger requires
           // scripts to be constructed in a consistent order, which might not
           // happen with lazy parsing.
           !mozilla::recordreplay::IsRecordingOrReplaying();
}

bool
BytecodeCompiler::createParser(ParseGoal goal)
{
    usedNames.emplace(cx);
    if (!usedNames->init())
        return false;

    if (canLazilyParse()) {
        syntaxParser.emplace(cx, alloc, options, sourceBuffer.get(), sourceBuffer.length(),
                             /* foldConstants = */ false, *usedNames, nullptr, nullptr,
                             sourceObject, goal);
        if (!syntaxParser->checkOptions())
            return false;
    }

    parser.emplace(cx, alloc, options, sourceBuffer.get(), sourceBuffer.length(),
                   /* foldConstants = */ true, *usedNames, syntaxParser.ptrOr(nullptr), nullptr,
                   sourceObject, goal);
    parser->ss = scriptSource;
    return parser->checkOptions();
}

bool
BytecodeCompiler::createSourceAndParser(ParseGoal goal,
                                        const Maybe<uint32_t>& parameterListEnd /* = Nothing() */)
{
    return createScriptSource(parameterListEnd) &&
           createParser(goal);
}

bool
BytecodeCompiler::createScript()
{
    return createScript(0, sourceBuffer.length());
}

bool
BytecodeCompiler::createScript(uint32_t toStringStart, uint32_t toStringEnd)
{
    script = JSScript::Create(cx, options,
                              sourceObject, /* sourceStart = */ 0, sourceBuffer.length(),
                              toStringStart, toStringEnd);
    return script != nullptr;
}

bool
BytecodeCompiler::emplaceEmitter(Maybe<BytecodeEmitter>& emitter, SharedContext* sharedContext)
{
    BytecodeEmitter::EmitterMode emitterMode =
        options.selfHostingMode ? BytecodeEmitter::SelfHosting : BytecodeEmitter::Normal;
    emitter.emplace(/* parent = */ nullptr, parser.ptr(), sharedContext, script,
                    /* lazyScript = */ nullptr, options.lineno, emitterMode);
    return emitter->init();
}

bool
BytecodeCompiler::handleParseFailure(const Directives& newDirectives,
                                     TokenStreamPosition& startPosition)
{
    if (parser->hadAbortedSyntaxParse()) {
        // Hit some unrecoverable ambiguity during an inner syntax parse.
        // Syntax parsing has now been disabled in the parser, so retry
        // the parse.
        parser->clearAbortedSyntaxParse();
    } else if (parser->anyChars.hadError() || directives == newDirectives) {
        return false;
    }

    parser->tokenStream.seek(startPosition);

    // Assignment must be monotonic to prevent reparsing iloops
    MOZ_ASSERT_IF(directives.strict(), newDirectives.strict());
    MOZ_ASSERT_IF(directives.asmJS(), newDirectives.asmJS());
    directives = newDirectives;
    return true;
}

bool
BytecodeCompiler::deoptimizeArgumentsInEnclosingScripts(JSContext* cx, HandleObject environment)
{
    RootedObject env(cx, environment);
    while (env->is<EnvironmentObject>() || env->is<DebugEnvironmentProxy>()) {
        if (env->is<CallObject>()) {
            RootedFunction fun(cx, &env->as<CallObject>().callee());
            RootedScript script(cx, JSFunction::getOrCreateScript(cx, fun));
            if (!script)
                return false;
            if (script->argumentsHasVarBinding()) {
                if (!JSScript::argumentsOptimizationFailed(cx, script))
                    return false;
            }
        }
        env = env->enclosingEnvironment();
    }

    return true;
}

JSScript*
BytecodeCompiler::compileScript(HandleObject environment, SharedContext* sc)
{
    if (!createSourceAndParser(ParseGoal::Script))
        return nullptr;

    TokenStreamPosition startPosition(keepAtoms, parser->tokenStream);

    if (!createScript())
        return nullptr;

    Maybe<BytecodeEmitter> emitter;
    if (!emplaceEmitter(emitter, sc))
        return nullptr;

    for (;;) {
        ParseNode* pn;
        if (sc->isEvalContext())
            pn = parser->evalBody(sc->asEvalContext());
        else
            pn = parser->globalBody(sc->asGlobalContext());

        // Successfully parsed. Emit the script.
        if (pn) {
            if (sc->isEvalContext() && sc->hasDebuggerStatement() && !cx->helperThread()) {
                // If the eval'ed script contains any debugger statement, force construction
                // of arguments objects for the caller script and any other scripts it is
                // transitively nested inside. The debugger can access any variable on the
                // scope chain.
                if (!deoptimizeArgumentsInEnclosingScripts(cx, environment))
                    return nullptr;
            }
            if (!emitter->emitScript(pn))
                return nullptr;
            break;
        }

        // Maybe we aborted a syntax parse. See if we can try again.
        if (!handleParseFailure(directives, startPosition))
            return nullptr;

        // Reset UsedNameTracker state before trying again.
        usedNames->reset();
    }

    // We have just finished parsing the source. Inform the source so that we
    // can compute statistics (e.g. how much time our functions remain lazy).
    script->scriptSource()->recordParseEnded();

    // Enqueue an off-thread source compression task after finishing parsing.
    if (!scriptSource->tryCompressOffThread(cx))
        return nullptr;

    MOZ_ASSERT_IF(!cx->helperThread(), !cx->isExceptionPending());

    return script;
}

JSScript*
BytecodeCompiler::compileGlobalScript(ScopeKind scopeKind)
{
    GlobalSharedContext globalsc(cx, scopeKind, directives, options.extraWarningsOption);
    return compileScript(nullptr, &globalsc);
}

JSScript*
BytecodeCompiler::compileEvalScript(HandleObject environment, HandleScope enclosingScope)
{
    EvalSharedContext evalsc(cx, environment, enclosingScope,
                             directives, options.extraWarningsOption);
    return compileScript(environment, &evalsc);
}

ModuleObject*
BytecodeCompiler::compileModule()
{
    if (!createSourceAndParser(ParseGoal::Module))
        return nullptr;

    Rooted<ModuleObject*> module(cx, ModuleObject::create(cx));
    if (!module)
        return nullptr;

    if (!createScript())
        return nullptr;

    module->init(script);

    ModuleBuilder builder(cx, module, parser->anyChars);
    if (!builder.init())
        return nullptr;

    ModuleSharedContext modulesc(cx, module, enclosingScope, builder);
    ParseNode* pn = parser->moduleBody(&modulesc);
    if (!pn)
        return nullptr;

    Maybe<BytecodeEmitter> emitter;
    if (!emplaceEmitter(emitter, &modulesc))
        return nullptr;
    if (!emitter->emitScript(pn->pn_body))
        return nullptr;

    if (!builder.initModule())
        return nullptr;

    RootedModuleEnvironmentObject env(cx, ModuleEnvironmentObject::create(cx, module));
    if (!env)
        return nullptr;

    module->setInitialEnvironment(env);

    // Enqueue an off-thread source compression task after finishing parsing.
    if (!scriptSource->tryCompressOffThread(cx))
        return nullptr;

    MOZ_ASSERT_IF(!cx->helperThread(), !cx->isExceptionPending());
    return module;
}

// Compile a standalone JS function, which might appear as the value of an
// event handler attribute in an HTML <INPUT> tag, or in a Function()
// constructor.
bool
BytecodeCompiler::compileStandaloneFunction(MutableHandleFunction fun,
                                            GeneratorKind generatorKind,
                                            FunctionAsyncKind asyncKind,
                                            const Maybe<uint32_t>& parameterListEnd)
{
    MOZ_ASSERT(fun);
    MOZ_ASSERT(fun->isTenured());

    if (!createSourceAndParser(ParseGoal::Script, parameterListEnd))
        return false;

    TokenStreamPosition startPosition(keepAtoms, parser->tokenStream);

    // Speculatively parse using the default directives implied by the context.
    // If a directive is encountered (e.g., "use strict") that changes how the
    // function should have been parsed, we backup and reparse with the new set
    // of directives.

    ParseNode* fn;
    do {
        Directives newDirectives = directives;
        fn = parser->standaloneFunction(fun, enclosingScope, parameterListEnd, generatorKind,
                                        asyncKind, directives, &newDirectives);
        if (!fn && !handleParseFailure(newDirectives, startPosition))
            return false;
    } while (!fn);

    if (fn->pn_funbox->function()->isInterpreted()) {
        MOZ_ASSERT(fun == fn->pn_funbox->function());

        if (!createScript(fn->pn_funbox->toStringStart, fn->pn_funbox->toStringEnd))
            return false;

        Maybe<BytecodeEmitter> emitter;
        if (!emplaceEmitter(emitter, fn->pn_funbox))
            return false;
        if (!emitter->emitFunctionScript(fn, BytecodeEmitter::TopLevelFunction::Yes))
            return false;
    } else {
        fun.set(fn->pn_funbox->function());
        MOZ_ASSERT(IsAsmJSModule(fun));
    }

    // Enqueue an off-thread source compression task after finishing parsing.
    if (!scriptSource->tryCompressOffThread(cx))
        return false;

    return true;
}

ScriptSourceObject*
BytecodeCompiler::sourceObjectPtr() const
{
    return sourceObject.get();
}

ScriptSourceObject*
frontend::CreateScriptSourceObject(JSContext* cx, const ReadOnlyCompileOptions& options,
                                   const Maybe<uint32_t>& parameterListEnd /* = Nothing() */)
{
    ScriptSource* ss = cx->new_<ScriptSource>();
    if (!ss)
        return nullptr;
    ScriptSourceHolder ssHolder(ss);

    if (!ss->initFromOptions(cx, options, parameterListEnd))
        return nullptr;

    RootedScriptSourceObject sso(cx, ScriptSourceObject::create(cx, ss));
    if (!sso)
        return nullptr;

    // Off-thread compilations do all their GC heap allocation, including the
    // SSO, in a temporary compartment. Hence, for the SSO to refer to the
    // gc-heap-allocated values in |options|, it would need cross-compartment
    // wrappers from the temporary compartment to the real compartment --- which
    // would then be inappropriate once we merged the temporary and real
    // compartments.
    //
    // Instead, we put off populating those SSO slots in off-thread compilations
    // until after we've merged compartments.
    if (!cx->helperThread()) {
        if (!ScriptSourceObject::initFromOptions(cx, sso, options))
            return nullptr;
    }

    return sso;
}

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
class MOZ_STACK_CLASS AutoInitializeSourceObject
{
    BytecodeCompiler& compiler_;
    ScriptSourceObject** sourceObjectOut_;

  public:
    AutoInitializeSourceObject(BytecodeCompiler& compiler,
                               ScriptSourceObject** sourceObjectOut)
      : compiler_(compiler),
        sourceObjectOut_(sourceObjectOut)
    { }

    ~AutoInitializeSourceObject() {
        if (sourceObjectOut_)
            *sourceObjectOut_ = compiler_.sourceObjectPtr();
    }
};

// RAII class to check the frontend reports an exception when it fails to
// compile a script.
class MOZ_RAII AutoAssertReportedException
{
#ifdef DEBUG
    JSContext* cx_;
    bool check_;

  public:
    explicit AutoAssertReportedException(JSContext* cx)
      : cx_(cx),
        check_(true)
    {}
    void reset() {
        check_ = false;
    }
    ~AutoAssertReportedException() {
        if (!check_)
            return;

        if (!cx_->helperThread()) {
            MOZ_ASSERT(cx_->isExceptionPending());
            return;
        }

        ParseTask* task = cx_->helperThread()->parseTask();
        MOZ_ASSERT(task->outOfMemory ||
                   task->overRecursed ||
                   !task->errors.empty());
    }
#else
  public:
    explicit AutoAssertReportedException(JSContext*) {}
    void reset() {}
#endif
};

JSScript*
frontend::CompileGlobalScript(JSContext* cx, LifoAlloc& alloc, ScopeKind scopeKind,
                              const ReadOnlyCompileOptions& options,
                              SourceBufferHolder& srcBuf,
                              ScriptSourceObject** sourceObjectOut)
{
    MOZ_ASSERT(scopeKind == ScopeKind::Global || scopeKind == ScopeKind::NonSyntactic);
    AutoAssertReportedException assertException(cx);
    BytecodeCompiler compiler(cx, alloc, options, srcBuf, /* enclosingScope = */ nullptr);
    AutoInitializeSourceObject autoSSO(compiler, sourceObjectOut);
    JSScript* script = compiler.compileGlobalScript(scopeKind);
    if (!script)
        return nullptr;
    assertException.reset();
    return script;
}

#if defined(JS_BUILD_BINAST)

JSScript*
frontend::CompileGlobalBinASTScript(JSContext* cx, LifoAlloc& alloc, const ReadOnlyCompileOptions& options,
                                    const uint8_t* src, size_t len, ScriptSourceObject** sourceObjectOut)
{
    AutoAssertReportedException assertException(cx);

    frontend::UsedNameTracker usedNames(cx);
    if (!usedNames.init())
        return nullptr;

    RootedScriptSourceObject sourceObj(cx, CreateScriptSourceObject(cx, options));

    if (!sourceObj)
        return nullptr;

    RootedScript script(cx, JSScript::Create(cx, options, sourceObj, 0, len, 0, len));

    if (!script)
        return nullptr;

    frontend::BinASTParser<BinTokenReaderMultipart> parser(cx, alloc, usedNames, options);

    auto parsed = parser.parse(src, len);

    if (parsed.isErr())
        return nullptr;

    Directives dir(false);
    GlobalSharedContext sc(cx, ScopeKind::Global, dir, false);
    BytecodeEmitter bce(nullptr, &parser, &sc, script, nullptr, 0);

    if (!bce.init())
        return nullptr;

    ParseNode *pn = parsed.unwrap();
    if (!bce.emitScript(pn))
        return nullptr;

    if (sourceObjectOut)
        *sourceObjectOut = sourceObj;

    assertException.reset();
    return script;
}

#endif // JS_BUILD_BINAST

JSScript*
frontend::CompileEvalScript(JSContext* cx, LifoAlloc& alloc,
                            HandleObject environment, HandleScope enclosingScope,
                            const ReadOnlyCompileOptions& options,
                            SourceBufferHolder& srcBuf,
                            ScriptSourceObject** sourceObjectOut)
{
    AutoAssertReportedException assertException(cx);
    BytecodeCompiler compiler(cx, alloc, options, srcBuf, enclosingScope);
    AutoInitializeSourceObject autoSSO(compiler, sourceObjectOut);
    JSScript* script = compiler.compileEvalScript(environment, enclosingScope);
    if (!script)
        return nullptr;
    assertException.reset();
    return script;

}

ModuleObject*
frontend::CompileModule(JSContext* cx, const ReadOnlyCompileOptions& optionsInput,
                        SourceBufferHolder& srcBuf, LifoAlloc& alloc,
                        ScriptSourceObject** sourceObjectOut)
{
    MOZ_ASSERT(srcBuf.get());
    MOZ_ASSERT_IF(sourceObjectOut, *sourceObjectOut == nullptr);

    AutoAssertReportedException assertException(cx);

    CompileOptions options(cx, optionsInput);
    options.maybeMakeStrictMode(true); // ES6 10.2.1 Module code is always strict mode code.
    options.setIsRunOnce(true);
    options.allowHTMLComments = false;

    RootedScope emptyGlobalScope(cx, &cx->global()->emptyGlobalScope());
    BytecodeCompiler compiler(cx, alloc, options, srcBuf, emptyGlobalScope);
    AutoInitializeSourceObject autoSSO(compiler, sourceObjectOut);
    ModuleObject* module = compiler.compileModule();
    if (!module)
        return nullptr;

    assertException.reset();
    return module;
}

ModuleObject*
frontend::CompileModule(JSContext* cx, const ReadOnlyCompileOptions& options,
                        SourceBufferHolder& srcBuf)
{
    AutoAssertReportedException assertException(cx);

    if (!GlobalObject::ensureModulePrototypesCreated(cx, cx->global()))
        return nullptr;

    LifoAlloc& alloc = cx->tempLifoAlloc();
    RootedModuleObject module(cx, CompileModule(cx, options, srcBuf, alloc));
    if (!module)
        return nullptr;

    // This happens in GlobalHelperThreadState::finishModuleParseTask() when a
    // module is compiled off thread.
    if (!ModuleObject::Freeze(cx, module))
        return nullptr;

    assertException.reset();
    return module;
}

// When leaving this scope, the given function should either:
//   * be linked to a fully compiled script
//   * remain linking to a lazy script
class MOZ_STACK_CLASS AutoAssertFunctionDelazificationCompletion
{
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
        if (!fun_)
            return;
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

bool
frontend::CompileLazyFunction(JSContext* cx, Handle<LazyScript*> lazy, const char16_t* chars, size_t length)
{
    MOZ_ASSERT(cx->compartment() == lazy->functionNonDelazifying()->compartment());
    // We can only compile functions whose parents have previously been
    // compiled, because compilation requires full information about the
    // function's immediately enclosing scope.
    MOZ_ASSERT(lazy->enclosingScriptHasEverBeenCompiled());

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

    // Update statistics to find out if we are delazifying just after having
    // lazified. Note that we are interested in the delta between end of
    // syntax parsing and start of full parsing, so we do this now rather than
    // after parsing below.
    if (!lazy->scriptSource()->parseEnded().IsNull()) {
        const mozilla::TimeDuration delta = mozilla::TimeStamp::Now() -
            lazy->scriptSource()->parseEnded();

        // Differentiate between web-facing and privileged code, to aid
        // with optimization. Due to the number of calls to this function,
        // we use `cx->runningWithTrustedPrincipals`, which is fast but
        // will classify addons alongside with web-facing code.
        const int HISTOGRAM = cx->runningWithTrustedPrincipals()
            ? JS_TELEMETRY_PRIVILEGED_PARSER_COMPILE_LAZY_AFTER_MS
            : JS_TELEMETRY_WEB_PARSER_COMPILE_LAZY_AFTER_MS;
        cx->runtime()->addTelemetry(HISTOGRAM, delta.ToMilliseconds());
    }

    UsedNameTracker usedNames(cx);
    if (!usedNames.init())
        return false;

    RootedScriptSourceObject sourceObject(cx, &lazy->sourceObject());
    Parser<FullParseHandler, char16_t> parser(cx, cx->tempLifoAlloc(), options, chars, length,
                                              /* foldConstants = */ true, usedNames, nullptr,
                                              lazy, sourceObject, lazy->parseGoal());
    if (!parser.checkOptions())
        return false;

    ParseNode* pn = parser.standaloneLazyFunction(fun, lazy->toStringStart(),
                                                  lazy->strict(), lazy->generatorKind(),
                                                  lazy->asyncKind());
    if (!pn)
        return false;

    Rooted<JSScript*> script(cx, JSScript::Create(cx, options, sourceObject,
                                                  lazy->sourceStart(), lazy->sourceEnd(),
                                                  lazy->toStringStart(), lazy->toStringEnd()));
    if (!script)
        return false;

    if (lazy->isLikelyConstructorWrapper())
        script->setLikelyConstructorWrapper();
    if (lazy->hasBeenCloned())
        script->setHasBeenCloned();

    BytecodeEmitter bce(/* parent = */ nullptr, &parser, pn->pn_funbox, script, lazy,
                        pn->pn_pos, BytecodeEmitter::LazyFunction);
    if (!bce.init())
        return false;

    if (!bce.emitFunctionScript(pn, BytecodeEmitter::TopLevelFunction::Yes))
        return false;

    delazificationCompletion.complete();
    assertException.reset();
    return true;
}

bool
frontend::CompileStandaloneFunction(JSContext* cx, MutableHandleFunction fun,
                                    const ReadOnlyCompileOptions& options,
                                    JS::SourceBufferHolder& srcBuf,
                                    const Maybe<uint32_t>& parameterListEnd,
                                    HandleScope enclosingScope /* = nullptr */)
{
    AutoAssertReportedException assertException(cx);

    RootedScope scope(cx, enclosingScope);
    if (!scope)
        scope = &cx->global()->emptyGlobalScope();

    BytecodeCompiler compiler(cx, cx->tempLifoAlloc(), options, srcBuf, scope);
    if (!compiler.compileStandaloneFunction(fun, GeneratorKind::NotGenerator,
                                            FunctionAsyncKind::SyncFunction,
                                            parameterListEnd))
    {
        return false;
    }

    assertException.reset();
    return true;
}

bool
frontend::CompileStandaloneGenerator(JSContext* cx, MutableHandleFunction fun,
                                     const ReadOnlyCompileOptions& options,
                                     JS::SourceBufferHolder& srcBuf,
                                     const Maybe<uint32_t>& parameterListEnd)
{
    AutoAssertReportedException assertException(cx);

    RootedScope emptyGlobalScope(cx, &cx->global()->emptyGlobalScope());

    BytecodeCompiler compiler(cx, cx->tempLifoAlloc(), options, srcBuf, emptyGlobalScope);
    if (!compiler.compileStandaloneFunction(fun, GeneratorKind::Generator,
                                            FunctionAsyncKind::SyncFunction,
                                            parameterListEnd))
    {
        return false;
    }

    assertException.reset();
    return true;
}

bool
frontend::CompileStandaloneAsyncFunction(JSContext* cx, MutableHandleFunction fun,
                                         const ReadOnlyCompileOptions& options,
                                         JS::SourceBufferHolder& srcBuf,
                                         const Maybe<uint32_t>& parameterListEnd)
{
    AutoAssertReportedException assertException(cx);

    RootedScope emptyGlobalScope(cx, &cx->global()->emptyGlobalScope());

    BytecodeCompiler compiler(cx, cx->tempLifoAlloc(), options, srcBuf, emptyGlobalScope);
    if (!compiler.compileStandaloneFunction(fun, GeneratorKind::NotGenerator,
                                            FunctionAsyncKind::AsyncFunction,
                                            parameterListEnd))
    {
        return false;
    }

    assertException.reset();
    return true;
}

bool
frontend::CompileStandaloneAsyncGenerator(JSContext* cx, MutableHandleFunction fun,
                                          const ReadOnlyCompileOptions& options,
                                          JS::SourceBufferHolder& srcBuf,
                                          const Maybe<uint32_t>& parameterListEnd)
{
    AutoAssertReportedException assertException(cx);

    RootedScope emptyGlobalScope(cx, &cx->global()->emptyGlobalScope());

    BytecodeCompiler compiler(cx, cx->tempLifoAlloc(), options, srcBuf, emptyGlobalScope);
    if (!compiler.compileStandaloneFunction(fun, GeneratorKind::Generator,
                                            FunctionAsyncKind::AsyncFunction,
                                            parameterListEnd))
    {
        return false;
    }

    assertException.reset();
    return true;
}
