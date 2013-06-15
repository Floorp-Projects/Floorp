/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeCompiler.h"

#include "jsscript.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/FoldConstants.h"
#include "frontend/NameFunctions.h"
#include "ion/AsmJS.h"
#include "vm/GlobalObject.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "frontend/ParseMaps-inl.h"
#include "frontend/ParseNode-inl.h"
#include "frontend/Parser-inl.h"
#include "frontend/SharedContext-inl.h"
#include "vm/Probes-inl.h"

using namespace js;
using namespace js::frontend;
using mozilla::Maybe;

static bool
CheckLength(JSContext *cx, size_t length)
{
    // Note this limit is simply so we can store sourceStart and sourceEnd in
    // JSScript as 32-bits. It could be lifted fairly easily, since the compiler
    // is using size_t internally already.
    if (length > UINT32_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_SOURCE_TOO_LONG);
        return false;
    }
    return true;
}

static bool
SetSourceMap(JSContext *cx, TokenStream &tokenStream, ScriptSource *ss, JSScript *script)
{
    if (tokenStream.hasSourceMap()) {
        if (!ss->setSourceMap(cx, tokenStream.releaseSourceMap(), script->filename()))
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

    // It's an error to use |arguments| in a generator expression.
    if (script->isGeneratorExp) {
        parser.report(ParseError, false, NULL, JSMSG_BAD_GENEXP_BODY, js_arguments_str);
        return false;
    }

    return true;
}

inline bool
CanLazilyParse(JSContext *cx, const CompileOptions &options)
{
    return options.canLazilyParse &&
        options.compileAndGo &&
        options.sourcePolicy == CompileOptions::SAVE_SOURCE &&
        !cx->compartment()->debugMode();
}

JSScript *
frontend::CompileScript(JSContext *cx, HandleObject scopeChain,
                        HandleScript evalCaller,
                        const CompileOptions &options,
                        const jschar *chars, size_t length,
                        JSString *source_ /* = NULL */,
                        unsigned staticLevel /* = 0 */,
                        SourceCompressionToken *extraSct /* = NULL */)
{
    RootedString source(cx, source_);
    SkipRoot skip(cx, &chars);

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
    ScriptSource *ss = cx->new_<ScriptSource>();
    if (!ss)
        return NULL;
    if (options.filename && !ss->setFilename(cx, options.filename))
        return NULL;

    JS::RootedScriptSource sourceObject(cx, ScriptSourceObject::create(cx, ss));
    if (!sourceObject)
        return NULL;
    SourceCompressionToken mysct(cx);
    SourceCompressionToken *sct = (extraSct) ? extraSct : &mysct;
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
        syntaxParser.construct(cx, options, chars, length, /* foldConstants = */ false,
                               (Parser<SyntaxParseHandler> *) NULL,
                               (LazyScript *) NULL);
    }

    Parser<FullParseHandler> parser(cx, options, chars, length, /* foldConstants = */ true,
                                    canLazilyParse ? &syntaxParser.ref() : NULL, NULL);
    parser.sct = sct;

    GlobalSharedContext globalsc(cx, scopeChain, StrictModeFromContext(cx));

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

    pc.construct(&parser, (GenericParseContext *) NULL, &globalsc, staticLevel, /* bodyid = */ 0);
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
            ObjectBox *funbox = parser.newFunctionBox(fun, pc.addr(), fun->strict());
            if (!funbox)
                return NULL;
            bce.objectList.add(funbox);
        }
    }

    bool canHaveDirectives = true;
    for (;;) {
        TokenKind tt = parser.tokenStream.peekToken(TSF_OPERAND);
        if (tt <= TOK_EOF) {
            if (tt == TOK_EOF)
                break;
            JS_ASSERT(tt == TOK_ERROR);
            return NULL;
        }

        TokenStream::Position pos(parser.keepAtoms);
        parser.tokenStream.tell(&pos);

        ParseNode *pn = parser.statement();
        if (!pn) {
            if (parser.hadAbortedSyntaxParse()) {
                // Parsing inner functions lazily may lead the parser into an
                // unrecoverable state and may require starting over on the top
                // level statement. Restart the parse; syntax parsing has
                // already been disabled for the parser and the result will not
                // be ambiguous.
                parser.clearAbortedSyntaxParse();
                parser.tokenStream.seek(pos);
                pc.destroy();
                pc.construct(&parser, (GenericParseContext *) NULL, &globalsc,
                             staticLevel, /* bodyid = */ 0);
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
            if (!parser.maybeParseDirective(pn, &canHaveDirectives))
                return NULL;
        }

        if (!FoldConstants(cx, &pn, &parser))
            return NULL;
        if (!NameFunctions(cx, pn))
            return NULL;

        if (!EmitTree(cx, &bce, pn))
            return NULL;

        parser.handler.freeTree(pn);
    }

    if (!SetSourceMap(cx, parser.tokenStream, ss, script))
        return NULL;

    if (evalCaller && evalCaller->functionOrCallerFunction()) {
        // Watch for uses of 'arguments' within the evaluated script, both as
        // free variables and as variables redeclared with 'var'.
        RootedFunction fun(cx, evalCaller->functionOrCallerFunction());
        HandlePropertyName arguments = cx->names().arguments;
        for (AtomDefnRange r = pc.ref().lexdeps->all(); !r.empty(); r.popFront()) {
            if (r.front().key() == arguments) {
                if (!CheckArgumentsWithinEval(cx, parser, fun))
                    return NULL;
            }
        }
        for (AtomDefnListMap::Range r = pc.ref().decls().all(); !r.empty(); r.popFront()) {
            if (r.front().key() == arguments) {
                if (!CheckArgumentsWithinEval(cx, parser, fun))
                    return NULL;
            }
        }

        // If the eval'ed script contains any debugger statement, force construction
        // of arguments objects for the caller script and any other scripts it is
        // transitively nested inside.
        if (pc.ref().sc->hasDebuggerStatement()) {
            RootedObject scope(cx, scopeChain);
            while (scope->isScope() || scope->isDebugScope()) {
                if (scope->isCall() && !scope->asCall().isForEval()) {
                    RootedScript script(cx, scope->asCall().callee().nonLazyScript());
                    if (script->argumentsHasVarBinding()) {
                        if (!JSScript::argumentsOptimizationFailed(cx, script))
                            return NULL;
                    }
                }
                scope = scope->enclosingScope();
            }
        }
    }

    /*
     * Nowadays the threaded interpreter needs a stop instruction, so we
     * do have to emit that here.
     */
    if (Emit1(cx, &bce, JSOP_STOP) < 0)
        return NULL;

    if (!JSScript::fullyInitFromEmitter(cx, script, &bce))
        return NULL;

    bce.tellDebuggerAboutCompiledScript(cx);

    if (sct == &mysct && !sct->complete())
        return NULL;

    return script;
}

bool
frontend::CompileLazyFunction(JSContext *cx, HandleFunction fun, LazyScript *lazy,
                              const jschar *chars, size_t length)
{
    JS_ASSERT(cx->compartment() == fun->compartment());

    CompileOptions options(cx, lazy->version());
    options.setPrincipals(cx->compartment()->principals)
           .setOriginPrincipals(lazy->originPrincipals())
           .setFileAndLine(lazy->source()->filename(), lazy->lineno())
           .setColumn(lazy->column())
           .setCompileAndGo(true)
           .setNoScriptRval(false)
           .setSelfHostingMode(false);

    Parser<FullParseHandler> parser(cx, options, chars, length,
                                    /* foldConstants = */ true, NULL, lazy);

    uint32_t staticLevel = lazy->staticLevel(cx);

    ParseNode *pn = parser.standaloneLazyFunction(fun, staticLevel, lazy->strict());
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
bool
frontend::CompileFunctionBody(JSContext *cx, MutableHandleFunction fun, CompileOptions options,
                              const AutoNameVector &formals, const jschar *chars, size_t length,
                              bool isAsmJSRecompile)
{
    SkipRoot skip(cx, &chars);

    if (!CheckLength(cx, length))
        return false;
    ScriptSource *ss = cx->new_<ScriptSource>();
    if (!ss)
        return false;
    if (options.filename && !ss->setFilename(cx, options.filename))
        return false;
    JS::RootedScriptSource sourceObject(cx, ScriptSourceObject::create(cx, ss));
    if (!sourceObject)
        return false;
    SourceCompressionToken sct(cx);
    JS_ASSERT(options.sourcePolicy != CompileOptions::LAZY_SOURCE);
    if (options.sourcePolicy == CompileOptions::SAVE_SOURCE) {
        if (!ss->setSourceCopy(cx, chars, length, true, &sct))
            return false;
    }

    bool canLazilyParse = CanLazilyParse(cx, options);

    Maybe<Parser<SyntaxParseHandler> > syntaxParser;
    if (canLazilyParse) {
        syntaxParser.construct(cx, options, chars, length, /* foldConstants = */ false,
                               (Parser<SyntaxParseHandler> *) NULL,
                               (LazyScript *) NULL);
    }

    JS_ASSERT(!options.forEval);

    Parser<FullParseHandler> parser(cx, options, chars, length, /* foldConstants = */ true,
                                    canLazilyParse ? &syntaxParser.ref() : NULL, NULL);
    parser.sct = &sct;

    JS_ASSERT(fun);
    JS_ASSERT(fun->isTenured());

    fun->setArgCount(formals.length());

    /* FIXME: make Function format the source for a function definition. */
    ParseNode *fn = CodeNode::create(PNK_FUNCTION, &parser.handler);
    if (!fn)
        return false;

    fn->pn_body = NULL;
    fn->pn_funbox = NULL;
    fn->pn_cookie.makeFree();

    ParseNode *argsbody = ListNode::create(PNK_ARGSBODY, &parser.handler);
    if (!argsbody)
        return false;
    argsbody->setOp(JSOP_NOP);
    argsbody->makeEmpty();
    fn->pn_body = argsbody;

    Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), false, options,
                                                  /* staticLevel = */ 0, sourceObject,
                                                  /* sourceStart = */ 0, length));
    if (!script)
        return false;

    // If the context is strict, immediately parse the body in strict
    // mode. Otherwise, we parse it normally. If we see a "use strict"
    // directive, we backup and reparse it as strict.
    TokenStream::Position start(parser.keepAtoms);
    parser.tokenStream.tell(&start);
    bool strict = StrictModeFromContext(cx);
    bool becameStrict;
    FunctionBox *funbox;
    ParseNode *pn;
    while (true) {
        pn = parser.standaloneFunctionBody(fun, formals, script, fn, &funbox,
                                           strict, &becameStrict);
        if (pn)
            break;

        if (parser.hadAbortedSyntaxParse()) {
            // Hit some unrecoverable ambiguity during an inner syntax parse.
            // Syntax parsing has now been disabled in the parser, so retry
            // the parse.
            parser.clearAbortedSyntaxParse();
        } else {
            // If the function became strict, reparse in strict mode.
            if (strict || !becameStrict || parser.tokenStream.hadError())
                return false;
            strict = true;
        }

        parser.tokenStream.seek(start);
    }

    if (!NameFunctions(cx, pn))
        return false;

    if (fn->pn_body) {
        JS_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
        fn->pn_body->append(pn);
        fn->pn_body->pn_pos = pn->pn_pos;
        pn = fn->pn_body;
    }

    bool generateBytecode = true;
#ifdef JS_ION
    JS_ASSERT_IF(isAsmJSRecompile, fn->pn_funbox->useAsm);
    if (fn->pn_funbox->useAsm && !isAsmJSRecompile) {
        RootedFunction moduleFun(cx);
        if (!CompileAsmJS(cx, parser.tokenStream, fn, options,
                          ss, /* bufStart = */ 0, /* bufEnd = */ length,
                          &moduleFun))
            return false;

        if (moduleFun) {
            funbox->object = moduleFun;
            fun.set(moduleFun); // replace the existing function with the LinkAsmJS native
            generateBytecode = false;
        }
    }
#endif

    if (generateBytecode) {
        /*
         * The reason for checking fun->environment() below is that certain
         * consumers of JS::CompileFunction, namely
         * nsEventListenerManager::CompileEventHandlerInternal, passes in a
         * NULL environment. This compiled function is never used, but instead
         * is cloned immediately onto the right scope chain.
         */
        BytecodeEmitter funbce(/* parent = */ NULL, &parser, funbox, script,
                               /* insideEval = */ false, /* evalCaller = */ NullPtr(),
                               fun->environment() && fun->environment()->isGlobal(),
                               options.lineno);
        if (!funbce.init())
            return false;

        if (!EmitFunctionScript(cx, &funbce, pn))
            return false;
    }

    if (!SetSourceMap(cx, parser.tokenStream, ss, script))
        return false;

    if (!sct.complete())
        return false;

    return true;
}
