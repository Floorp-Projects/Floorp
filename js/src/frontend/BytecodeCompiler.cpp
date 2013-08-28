/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeCompiler.h"

#include "jscntxt.h"
#include "jsscript.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/FoldConstants.h"
#include "frontend/NameFunctions.h"
#include "frontend/Parser.h"
#include "jit/AsmJSLink.h"
#include "vm/GlobalObject.h"

#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseMaps-inl.h"
#include "frontend/Parser-inl.h"

using namespace js;
using namespace js::frontend;
using mozilla::Maybe;

static bool
CheckLength(ExclusiveContext *cx, size_t length)
{
    // Note this limit is simply so we can store sourceStart and sourceEnd in
    // JSScript as 32-bits. It could be lifted fairly easily, since the compiler
    // is using size_t internally already.
    if (length > UINT32_MAX) {
        if (cx->isJSContext())
            JS_ReportErrorNumber(cx->asJSContext(), js_GetErrorMessage, NULL, JSMSG_SOURCE_TOO_LONG);
        return false;
    }
    return true;
}

static bool
SetSourceMap(ExclusiveContext *cx, TokenStream &tokenStream, ScriptSource *ss)
{
    if (tokenStream.hasSourceMap()) {
        if (!ss->setSourceMap(cx, tokenStream.releaseSourceMap()))
            return false;
    }
    return true;
}

static bool
CheckArgumentsWithinEval(JSContext *cx, Parser<FullParseHandler> &parser, HandleFunction fun)
{
    if (fun->hasRest()) {
        // It's an error to use |arguments| in a function that has a rest
        // parameter.
        parser.report(ParseError, false, NULL, JSMSG_ARGUMENTS_AND_REST);
        return false;
    }

    // Force construction of arguments objects for functions that use
    // |arguments| within an eval.
    RootedScript script(cx, fun->nonLazyScript());
    if (script->argumentsHasVarBinding()) {
        if (!JSScript::argumentsOptimizationFailed(cx, script))
            return false;
    }

    // It's an error to use |arguments| in a legacy generator expression.
    if (script->isGeneratorExp && script->isLegacyGenerator()) {
        parser.report(ParseError, false, NULL, JSMSG_BAD_GENEXP_BODY, js_arguments_str);
        return false;
    }

    return true;
}

static bool
MaybeCheckEvalFreeVariables(ExclusiveContext *cxArg, HandleScript evalCaller, HandleObject scopeChain,
                            Parser<FullParseHandler> &parser,
                            ParseContext<FullParseHandler> &pc)
{
    if (!evalCaller || !evalCaller->functionOrCallerFunction())
        return true;

    // Eval scripts are only compiled on the main thread.
    JSContext *cx = cxArg->asJSContext();

    // Watch for uses of 'arguments' within the evaluated script, both as
    // free variables and as variables redeclared with 'var'.
    RootedFunction fun(cx, evalCaller->functionOrCallerFunction());
    HandlePropertyName arguments = cx->names().arguments;
    for (AtomDefnRange r = pc.lexdeps->all(); !r.empty(); r.popFront()) {
        if (r.front().key() == arguments) {
            if (!CheckArgumentsWithinEval(cx, parser, fun))
                return false;
        }
    }
    for (AtomDefnListMap::Range r = pc.decls().all(); !r.empty(); r.popFront()) {
        if (r.front().key() == arguments) {
            if (!CheckArgumentsWithinEval(cx, parser, fun))
                return false;
        }
    }

    // If the eval'ed script contains any debugger statement, force construction
    // of arguments objects for the caller script and any other scripts it is
    // transitively nested inside. The debugger can access any variable on the
    // scope chain.
    if (pc.sc->hasDebuggerStatement()) {
        RootedObject scope(cx, scopeChain);
        while (scope->is<ScopeObject>() || scope->is<DebugScopeObject>()) {
            if (scope->is<CallObject>() && !scope->as<CallObject>().isForEval()) {
                RootedScript script(cx, scope->as<CallObject>().callee().nonLazyScript());
                if (script->argumentsHasVarBinding()) {
                    if (!JSScript::argumentsOptimizationFailed(cx, script))
                        return false;
                }
            }
            scope = scope->enclosingScope();
        }
    }

    return true;
}

inline bool
CanLazilyParse(ExclusiveContext *cx, const CompileOptions &options)
{
    return options.canLazilyParse &&
        options.compileAndGo &&
        options.sourcePolicy == CompileOptions::SAVE_SOURCE &&
        !cx->compartment()->debugMode();
}

void
frontend::MaybeCallSourceHandler(JSContext *cx, const CompileOptions &options,
                                 const jschar *chars, size_t length)
{
    JSSourceHandler listener = cx->runtime()->debugHooks.sourceHandler;
    void *listenerData = cx->runtime()->debugHooks.sourceHandlerData;

    if (listener) {
        void *listenerTSData;
        listener(options.filename, options.lineno, chars, length,
                 &listenerTSData, listenerData);
    }
}

JSScript *
frontend::CompileScript(ExclusiveContext *cx, LifoAlloc *alloc, HandleObject scopeChain,
                        HandleScript evalCaller,
                        const CompileOptions &options,
                        const jschar *chars, size_t length,
                        JSString *source_ /* = NULL */,
                        unsigned staticLevel /* = 0 */,
                        SourceCompressionTask *extraSct /* = NULL */)
{
    RootedString source(cx, source_);
    SkipRoot skip(cx, &chars);

#if JS_TRACE_LOGGING
        js::AutoTraceLog logger(js::TraceLogging::defaultLogger(),
                                js::TraceLogging::PARSER_COMPILE_SCRIPT_START,
                                js::TraceLogging::PARSER_COMPILE_SCRIPT_STOP,
                                options);
#endif

    if (cx->isJSContext())
        MaybeCallSourceHandler(cx->asJSContext(), options, chars, length);

    /*
     * The scripted callerFrame can only be given for compile-and-go scripts
     * and non-zero static level requires callerFrame.
     */
    JS_ASSERT_IF(evalCaller, options.compileAndGo);
    JS_ASSERT_IF(evalCaller, options.forEval);
    JS_ASSERT_IF(staticLevel != 0, evalCaller);

    if (!CheckLength(cx, length))
        return NULL;
    JS_ASSERT_IF(staticLevel != 0, options.sourcePolicy != CompileOptions::LAZY_SOURCE);
    ScriptSource *ss = cx->new_<ScriptSource>(options.originPrincipals());
    if (!ss)
        return NULL;
    if (options.filename && !ss->setFilename(cx, options.filename))
        return NULL;

    JS::RootedScriptSource sourceObject(cx, ScriptSourceObject::create(cx, ss));
    if (!sourceObject)
        return NULL;

    SourceCompressionTask mysct(cx);
    SourceCompressionTask *sct = extraSct ? extraSct : &mysct;

    switch (options.sourcePolicy) {
      case CompileOptions::SAVE_SOURCE:
        if (!ss->setSourceCopy(cx, chars, length, false, sct))
            return NULL;
        break;
      case CompileOptions::LAZY_SOURCE:
        ss->setSourceRetrievable();
        break;
      case CompileOptions::NO_SOURCE:
        break;
    }

    bool canLazilyParse = CanLazilyParse(cx, options);

    Maybe<Parser<SyntaxParseHandler> > syntaxParser;
    if (canLazilyParse) {
        syntaxParser.construct(cx, alloc, options, chars, length, /* foldConstants = */ false,
                               (Parser<SyntaxParseHandler> *) NULL,
                               (LazyScript *) NULL);
    }

    Parser<FullParseHandler> parser(cx, alloc, options, chars, length, /* foldConstants = */ true,
                                    canLazilyParse ? &syntaxParser.ref() : NULL, NULL);
    parser.sct = sct;
    parser.ss = ss;

    Directives directives(options.strictOption);
    GlobalSharedContext globalsc(cx, scopeChain, directives, options.extraWarningsOption);

    bool savedCallerFun =
        options.compileAndGo &&
        evalCaller &&
        (evalCaller->function() || evalCaller->savedCallerFun);
    Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), savedCallerFun,
                                                  options, staticLevel, sourceObject, 0, length));
    if (!script)
        return NULL;

    // Global/eval script bindings are always empty (all names are added to the
    // scope dynamically via JSOP_DEFFUN/VAR).
    InternalHandle<Bindings*> bindings(script, &script->bindings);
    if (!Bindings::initWithTemporaryStorage(cx, bindings, 0, 0, NULL))
        return NULL;

    // We can specialize a bit for the given scope chain if that scope chain is the global object.
    JSObject *globalScope = scopeChain && scopeChain == &scopeChain->global() ? (JSObject*) scopeChain : NULL;
    JS_ASSERT_IF(globalScope, globalScope->isNative());
    JS_ASSERT_IF(globalScope, JSCLASS_HAS_GLOBAL_FLAG_AND_SLOTS(globalScope->getClass()));

    BytecodeEmitter::EmitterMode emitterMode =
        options.selfHostingMode ? BytecodeEmitter::SelfHosting : BytecodeEmitter::Normal;
    BytecodeEmitter bce(/* parent = */ NULL, &parser, &globalsc, script, options.forEval, evalCaller,
                        !!globalScope, options.lineno, emitterMode);
    if (!bce.init())
        return NULL;

    // Syntax parsing may cause us to restart processing of top level
    // statements in the script. Use Maybe<> so that the parse context can be
    // reset when this occurs.
    Maybe<ParseContext<FullParseHandler> > pc;

    pc.construct(&parser, (GenericParseContext *) NULL, (ParseNode *) NULL, &globalsc,
                 (Directives *) NULL, staticLevel, /* bodyid = */ 0);
    if (!pc.ref().init())
        return NULL;

    /* If this is a direct call to eval, inherit the caller's strictness.  */
    if (evalCaller && evalCaller->strict)
        globalsc.strict = true;

    if (options.compileAndGo) {
        if (source) {
            /*
             * Save eval program source in script->atoms[0] for the
             * eval cache (see EvalCacheLookup in jsobj.cpp).
             */
            JSAtom *atom = AtomizeString<CanGC>(cx, source);
            jsatomid _;
            if (!atom || !bce.makeAtomIndex(atom, &_))
                return NULL;
        }

        if (evalCaller && evalCaller->functionOrCallerFunction()) {
            /*
             * An eval script in a caller frame needs to have its enclosing
             * function captured in case it refers to an upvar, and someone
             * wishes to decompile it while it's running.
             */
            JSFunction *fun = evalCaller->functionOrCallerFunction();
            Directives directives(/* strict = */ fun->strict());
            ObjectBox *funbox = parser.newFunctionBox(/* fn = */ NULL, fun, pc.addr(),
                                                      directives, fun->generatorKind());
            if (!funbox)
                return NULL;
            bce.objectList.add(funbox);
        }
    }

    bool canHaveDirectives = true;
    for (;;) {
        TokenKind tt = parser.tokenStream.peekToken(TokenStream::Operand);
        if (tt <= TOK_EOF) {
            if (tt == TOK_EOF)
                break;
            JS_ASSERT(tt == TOK_ERROR);
            return NULL;
        }

        TokenStream::Position pos(parser.keepAtoms);
        parser.tokenStream.tell(&pos);

        ParseNode *pn = parser.statement(canHaveDirectives);
        if (!pn) {
            if (parser.hadAbortedSyntaxParse()) {
                // Parsing inner functions lazily may lead the parser into an
                // unrecoverable state and may require starting over on the top
                // level statement. Restart the parse; syntax parsing has
                // already been disabled for the parser and the result will not
                // be ambiguous.
                parser.clearAbortedSyntaxParse();
                parser.tokenStream.seek(pos);

                // Destroying the parse context will destroy its free
                // variables, so check if any deoptimization is needed.
                if (!MaybeCheckEvalFreeVariables(cx, evalCaller, scopeChain, parser, pc.ref()))
                    return NULL;

                pc.destroy();
                pc.construct(&parser, (GenericParseContext *) NULL, (ParseNode *) NULL,
                             &globalsc, (Directives *) NULL, staticLevel, /* bodyid = */ 0);
                if (!pc.ref().init())
                    return NULL;
                JS_ASSERT(parser.pc == pc.addr());
                pn = parser.statement();
            }
            if (!pn) {
                JS_ASSERT(!parser.hadAbortedSyntaxParse());
                return NULL;
            }
        }

        if (canHaveDirectives) {
            if (!parser.maybeParseDirective(/* stmtList = */ NULL, pn, &canHaveDirectives))
                return NULL;
        }

        if (!FoldConstants(cx, &pn, &parser))
            return NULL;

        // Inferring names for functions in compiled scripts is currently only
        // supported while on the main thread. See bug 895395.
        if (cx->isJSContext() && !NameFunctions(cx->asJSContext(), pn))
            return NULL;

        if (!EmitTree(cx, &bce, pn))
            return NULL;

        parser.handler.freeTree(pn);
    }

    if (!MaybeCheckEvalFreeVariables(cx, evalCaller, scopeChain, parser, pc.ref()))
        return NULL;

    if (!SetSourceMap(cx, parser.tokenStream, ss))
        return NULL;

    /*
     * Nowadays the threaded interpreter needs a stop instruction, so we
     * do have to emit that here.
     */
    if (Emit1(cx, &bce, JSOP_STOP) < 0)
        return NULL;

    if (!JSScript::fullyInitFromEmitter(cx, script, &bce))
        return NULL;

    bce.tellDebuggerAboutCompiledScript(cx);

    if (sct && !extraSct && !sct->complete())
        return NULL;

    return script;
}

bool
frontend::CompileLazyFunction(JSContext *cx, LazyScript *lazy, const jschar *chars, size_t length)
{
    JS_ASSERT(cx->compartment() == lazy->function()->compartment());

    CompileOptions options(cx, lazy->version());
    options.setPrincipals(cx->compartment()->principals)
           .setOriginPrincipals(lazy->originPrincipals())
           .setFileAndLine(lazy->source()->filename(), lazy->lineno())
           .setColumn(lazy->column())
           .setCompileAndGo(true)
           .setNoScriptRval(false)
           .setSelfHostingMode(false);

#if JS_TRACE_LOGGING
        js::AutoTraceLog logger(js::TraceLogging::defaultLogger(),
                                js::TraceLogging::PARSER_COMPILE_LAZY_START,
                                js::TraceLogging::PARSER_COMPILE_LAZY_STOP,
                                options);
#endif

    Parser<FullParseHandler> parser(cx, &cx->tempLifoAlloc(), options, chars, length,
                                    /* foldConstants = */ true, NULL, lazy);

    uint32_t staticLevel = lazy->staticLevel(cx);

    Rooted<JSFunction*> fun(cx, lazy->function());
    JS_ASSERT(!lazy->isLegacyGenerator());
    ParseNode *pn = parser.standaloneLazyFunction(fun, staticLevel, lazy->strict(),
                                                  lazy->generatorKind());
    if (!pn)
        return false;

    if (!NameFunctions(cx, pn))
        return false;

    RootedObject enclosingScope(cx, lazy->enclosingScope());
    JS::RootedScriptSource sourceObject(cx, lazy->sourceObject());
    JS_ASSERT(sourceObject);

    Rooted<JSScript*> script(cx, JSScript::Create(cx, enclosingScope, false,
                                                  options, staticLevel,
                                                  sourceObject, lazy->begin(), lazy->end()));
    if (!script)
        return false;

    script->bindings = pn->pn_funbox->bindings;

    if (lazy->directlyInsideEval())
        script->directlyInsideEval = true;
    if (lazy->usesArgumentsAndApply())
        script->usesArgumentsAndApply = true;

    BytecodeEmitter bce(/* parent = */ NULL, &parser, pn->pn_funbox, script, options.forEval,
                        /* evalCaller = */ NullPtr(), /* hasGlobalScope = */ true,
                        options.lineno, BytecodeEmitter::LazyFunction);
    if (!bce.init())
        return false;

    return EmitFunctionScript(cx, &bce, pn->pn_body);
}

// Compile a JS function body, which might appear as the value of an event
// handler attribute in an HTML <INPUT> tag, or in a Function() constructor.
static bool
CompileFunctionBody(JSContext *cx, MutableHandleFunction fun, CompileOptions options,
                    const AutoNameVector &formals, const jschar *chars, size_t length,
                    GeneratorKind generatorKind)
{
#if JS_TRACE_LOGGING
        js::AutoTraceLog logger(js::TraceLogging::defaultLogger(),
                                js::TraceLogging::PARSER_COMPILE_FUNCTION_START,
                                js::TraceLogging::PARSER_COMPILE_FUNCTION_STOP,
                                options);
#endif

    // FIXME: make Function pass in two strings and parse them as arguments and
    // ProgramElements respectively.
    SkipRoot skip(cx, &chars);

    MaybeCallSourceHandler(cx, options, chars, length);

    if (!CheckLength(cx, length))
        return false;
    ScriptSource *ss = cx->new_<ScriptSource>(options.originPrincipals());
    if (!ss)
        return false;
    if (options.filename && !ss->setFilename(cx, options.filename))
        return false;
    JS::RootedScriptSource sourceObject(cx, ScriptSourceObject::create(cx, ss));
    if (!sourceObject)
        return false;
    SourceCompressionTask sct(cx);
    JS_ASSERT(options.sourcePolicy != CompileOptions::LAZY_SOURCE);
    if (options.sourcePolicy == CompileOptions::SAVE_SOURCE) {
        if (!ss->setSourceCopy(cx, chars, length, true, &sct))
            return false;
    }

    bool canLazilyParse = CanLazilyParse(cx, options);

    Maybe<Parser<SyntaxParseHandler> > syntaxParser;
    if (canLazilyParse) {
        syntaxParser.construct(cx, &cx->tempLifoAlloc(),
                               options, chars, length, /* foldConstants = */ false,
                               (Parser<SyntaxParseHandler> *) NULL,
                               (LazyScript *) NULL);
    }

    JS_ASSERT(!options.forEval);

    Parser<FullParseHandler> parser(cx, &cx->tempLifoAlloc(),
                                    options, chars, length, /* foldConstants = */ true,
                                    canLazilyParse ? &syntaxParser.ref() : NULL, NULL);
    parser.sct = &sct;
    parser.ss = ss;

    JS_ASSERT(fun);
    JS_ASSERT(fun->isTenured());

    fun->setArgCount(formals.length());

    // Speculatively parse using the default directives implied by the context.
    // If a directive is encountered (e.g., "use strict") that changes how the
    // function should have been parsed, we backup and reparse with the new set
    // of directives.
    Directives directives(options.strictOption);

    TokenStream::Position start(parser.keepAtoms);
    parser.tokenStream.tell(&start);

    ParseNode *fn;
    while (true) {
        Directives newDirectives = directives;
        fn = parser.standaloneFunctionBody(fun, formals, generatorKind, directives, &newDirectives);
        if (fn)
            break;

        if (parser.hadAbortedSyntaxParse()) {
            // Hit some unrecoverable ambiguity during an inner syntax parse.
            // Syntax parsing has now been disabled in the parser, so retry
            // the parse.
            parser.clearAbortedSyntaxParse();
        } else {
            if (parser.tokenStream.hadError() || directives == newDirectives)
                return false;

            // Assignment must be monotonic to prevent reparsing iloops
            JS_ASSERT_IF(directives.strict(), newDirectives.strict());
            JS_ASSERT_IF(directives.asmJS(), newDirectives.asmJS());
            directives = newDirectives;
        }

        parser.tokenStream.seek(start);
    }

    if (!NameFunctions(cx, fn))
        return false;

    if (fn->pn_funbox->function()->isInterpreted()) {
        JS_ASSERT(fun == fn->pn_funbox->function());

        Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), false, options,
                                                      /* staticLevel = */ 0, sourceObject,
                                                      /* sourceStart = */ 0, length));
        if (!script)
            return false;

        script->bindings = fn->pn_funbox->bindings;

        /*
         * The reason for checking fun->environment() below is that certain
         * consumers of JS::CompileFunction, namely
         * nsEventListenerManager::CompileEventHandlerInternal, passes in a
         * NULL environment. This compiled function is never used, but instead
         * is cloned immediately onto the right scope chain.
         */
        BytecodeEmitter funbce(/* parent = */ NULL, &parser, fn->pn_funbox, script,
                               /* insideEval = */ false, /* evalCaller = */ NullPtr(),
                               fun->environment() && fun->environment()->is<GlobalObject>(),
                               options.lineno);
        if (!funbce.init())
            return false;

        if (!EmitFunctionScript(cx, &funbce, fn->pn_body))
            return false;
    } else {
        fun.set(fn->pn_funbox->function());
        JS_ASSERT(IsAsmJSModuleNative(fun->native()));
    }

    if (!SetSourceMap(cx, parser.tokenStream, ss))
        return false;

    if (!sct.complete())
        return false;

    return true;
}

bool
frontend::CompileFunctionBody(JSContext *cx, MutableHandleFunction fun, CompileOptions options,
                              const AutoNameVector &formals, const jschar *chars, size_t length)
{
    return CompileFunctionBody(cx, fun, options, formals, chars, length, NotGenerator);
}

bool
frontend::CompileStarGeneratorBody(JSContext *cx, MutableHandleFunction fun,
                                   CompileOptions options, const AutoNameVector &formals,
                                   const jschar *chars, size_t length)
{
    return CompileFunctionBody(cx, fun, options, formals, chars, length, StarGenerator);
}
