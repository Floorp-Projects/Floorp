/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeCompiler.h"

#include "jsprobes.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/FoldConstants.h"
#include "frontend/NameFunctions.h"
#include "vm/GlobalObject.h"

#include "jsinferinlines.h"

#include "frontend/ParseMaps-inl.h"
#include "frontend/Parser-inl.h"
#include "frontend/SharedContext-inl.h"

using namespace js;
using namespace js::frontend;

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
        if (!ss->setSourceMap(cx, tokenStream.releaseSourceMap(), script->filename))
            return false;
    }
    return true;
}

JSScript *
frontend::CompileScript(JSContext *cx, HandleObject scopeChain, StackFrame *callerFrame,
                        const CompileOptions &options,
                        StableCharPtr chars, size_t length,
                        JSString *source_ /* = NULL */,
                        unsigned staticLevel /* = 0 */)
{
    RootedString source(cx, source_);

    class ProbesManager
    {
        const char* filename;
        unsigned lineno;

      public:
        ProbesManager(const char *f, unsigned l) : filename(f), lineno(l) {
            Probes::compileScriptBegin(filename, lineno);
        }
        ~ProbesManager() { Probes::compileScriptEnd(filename, lineno); }
    };
    ProbesManager probesManager(options.filename, options.lineno);

    /*
     * The scripted callerFrame can only be given for compile-and-go scripts
     * and non-zero static level requires callerFrame.
     */
    JS_ASSERT_IF(callerFrame, options.compileAndGo);
    JS_ASSERT_IF(staticLevel != 0, callerFrame);

    if (!CheckLength(cx, length))
        return NULL;
    JS_ASSERT_IF(staticLevel != 0, options.sourcePolicy != CompileOptions::LAZY_SOURCE);
    ScriptSource *ss = cx->new_<ScriptSource>();
    if (!ss)
        return NULL;
    ScriptSourceHolder ssh(cx->runtime, ss);
    SourceCompressionToken sct(cx);
    switch (options.sourcePolicy) {
      case CompileOptions::SAVE_SOURCE:
        if (!ss->setSourceCopy(cx, chars, length, false, &sct))
            return NULL;
        break;
      case CompileOptions::LAZY_SOURCE:
        ss->setSourceRetrievable();
        break;
      case CompileOptions::NO_SOURCE:
        break;
    }

    Parser parser(cx, options, chars, length, /* foldConstants = */ true);
    if (!parser.init())
        return NULL;
    parser.sct = &sct;

    GlobalSharedContext globalsc(cx, scopeChain, StrictModeFromContext(cx));

    ParseContext pc(&parser, &globalsc, staticLevel, /* bodyid = */ 0);
    if (!pc.init())
        return NULL;

    bool savedCallerFun = options.compileAndGo && callerFrame && callerFrame->isFunctionFrame();
    Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), savedCallerFun,
                                                  options, staticLevel, ss, 0, length));
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

    BytecodeEmitter bce(/* parent = */ NULL, &parser, &globalsc, script, callerFrame, !!globalScope,
                        options.lineno, options.selfHostingMode);
    if (!bce.init())
        return NULL;

    /* If this is a direct call to eval, inherit the caller's strictness.  */
    if (callerFrame && callerFrame->script()->strict)
        globalsc.strict = true;

    if (options.compileAndGo) {
        if (source) {
            /*
             * Save eval program source in script->atoms[0] for the
             * eval cache (see EvalCacheLookup in jsobj.cpp).
             */
            JSAtom *atom = AtomizeString(cx, source);
            jsatomid _;
            if (!atom || !bce.makeAtomIndex(atom, &_))
                return NULL;
        }

        if (callerFrame && callerFrame->isFunctionFrame()) {
            /*
             * An eval script in a caller frame needs to have its enclosing
             * function captured in case it refers to an upvar, and someone
             * wishes to decompile it while it's running.
             */
            JSFunction *fun = callerFrame->fun();
            ObjectBox *funbox = parser.newFunctionBox(fun, &pc, fun->strict());
            if (!funbox)
                return NULL;
            bce.objectList.add(funbox);
        }
    }

    ParseNode *pn;
#if JS_HAS_XML_SUPPORT
    pn = NULL;
    bool onlyXML;
    onlyXML = true;
#endif

    TokenStream &tokenStream = parser.tokenStream;
    bool canHaveDirectives = true;
    for (;;) {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt <= TOK_EOF) {
            if (tt == TOK_EOF)
                break;
            JS_ASSERT(tt == TOK_ERROR);
            return NULL;
        }

        pn = parser.statement();
        if (!pn)
            return NULL;

        if (canHaveDirectives) {
            if (!parser.maybeParseDirective(pn, &canHaveDirectives))
                return NULL;
        }

        if (!FoldConstants(cx, pn, &parser))
            return NULL;
        if (!NameFunctions(cx, pn))
            return NULL;

        if (!EmitTree(cx, &bce, pn))
            return NULL;

#if JS_HAS_XML_SUPPORT
        if (!pn->isKind(PNK_SEMI) || !pn->pn_kid || !pn->pn_kid->isXMLItem())
            onlyXML = false;
#endif
        parser.freeTree(pn);
    }

    if (!SetSourceMap(cx, tokenStream, ss, script))
        return NULL;

#if JS_HAS_XML_SUPPORT
    /*
     * Prevent XML data theft via <script src="http://victim.com/foo.xml">.
     * For background, see:
     *
     * https://bugzilla.mozilla.org/show_bug.cgi?id=336551
     */
    if (pn && onlyXML && !callerFrame) {
        parser.reportError(NULL, JSMSG_XML_WHOLE_PROGRAM);
        return NULL;
    }
#endif

    // It's an error to use |arguments| in a function that has a rest parameter.
    if (callerFrame && callerFrame->isFunctionFrame() && callerFrame->fun()->hasRest()) {
        HandlePropertyName arguments = cx->names().arguments;
        for (AtomDefnRange r = pc.lexdeps->all(); !r.empty(); r.popFront()) {
            if (r.front().key() == arguments) {
                parser.reportError(NULL, JSMSG_ARGUMENTS_AND_REST);
                return NULL;
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

    if (!sct.complete())
        return NULL;

    return script;
}

// Compile a JS function body, which might appear as the value of an event
// handler attribute in an HTML <INPUT> tag, or in a Function() constructor.
bool
frontend::CompileFunctionBody(JSContext *cx, HandleFunction fun, CompileOptions options,
                              const AutoNameVector &formals, StableCharPtr chars, size_t length)
{
    if (!CheckLength(cx, length))
        return false;
    ScriptSource *ss = cx->new_<ScriptSource>();
    if (!ss)
        return false;
    ScriptSourceHolder ssh(cx->runtime, ss);
    SourceCompressionToken sct(cx);
    JS_ASSERT(options.sourcePolicy != CompileOptions::LAZY_SOURCE);
    if (options.sourcePolicy == CompileOptions::SAVE_SOURCE) {
        if (!ss->setSourceCopy(cx, chars, length, true, &sct))
            return false;
    }

    options.setCompileAndGo(false);
    Parser parser(cx, options, chars, length, /* foldConstants = */ true);
    if (!parser.init())
        return false;
    parser.sct = &sct;

    JS_ASSERT(fun);

    fun->setArgCount(formals.length());

    /* FIXME: make Function format the source for a function definition. */
    ParseNode *fn = FunctionNode::create(PNK_NAME, &parser);
    if (!fn)
        return false;

    fn->pn_body = NULL;
    fn->pn_cookie.makeFree();

    ParseNode *argsbody = ListNode::create(PNK_ARGSBODY, &parser);
    if (!argsbody)
        return false;
    argsbody->setOp(JSOP_NOP);
    argsbody->makeEmpty();
    fn->pn_body = argsbody;

    Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), false, options,
                                                  /* staticLevel = */ 0, ss,
                                                  /* sourceStart = */ 0, length));
    if (!script)
        return false;

    // If the context is strict, immediately parse the body in strict
    // mode. Otherwise, we parse it normally. If we see a "use strict"
    // directive, we backup and reparse it as strict.
    TokenStream::Position start;
    parser.tokenStream.tell(&start);
    bool initiallyStrict = StrictModeFromContext(cx);
    bool becameStrict;
    FunctionBox *funbox;
    ParseNode *pn = parser.standaloneFunctionBody(fun, formals, script, fn, &funbox,
                                                  initiallyStrict, &becameStrict);
    if (!pn) {
        if (initiallyStrict || !becameStrict || parser.tokenStream.hadError())
            return false;

        // Reparse in strict mode.
        parser.tokenStream.seek(start);
        pn = parser.standaloneFunctionBody(fun, formals, script, fn, &funbox,
                                           /* strict = */ true);
        if (!pn)
            return false;
    }

    BytecodeEmitter funbce(/* parent = */ NULL, &parser, funbox, script, /* callerFrame = */ NULL,
                           /* hasGlobalScope = */ false, options.lineno);
    if (!funbce.init())
        return false;

    if (!NameFunctions(cx, pn))
        return false;

    if (fn->pn_body) {
        JS_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
        fn->pn_body->append(pn);
        fn->pn_body->pn_pos = pn->pn_pos;
        pn = fn->pn_body;
    }

    if (!SetSourceMap(cx, parser.tokenStream, ss, script))
        return false;

    if (!EmitFunctionScript(cx, &funbce, pn))
        return false;

    if (!sct.complete())
        return false;

    return true;
}
