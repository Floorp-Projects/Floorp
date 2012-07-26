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
#include "frontend/SemanticAnalysis.h"
#include "vm/GlobalObject.h"

#include "jsinferinlines.h"

#include "frontend/TreeContext-inl.h"

using namespace js;
using namespace js::frontend;

class AutoAttachToRuntime {
    JSRuntime *rt;
  public:
    ScriptSource *ss;
    AutoAttachToRuntime(JSRuntime *rt)
      : rt(rt), ss(NULL) {}
    ~AutoAttachToRuntime() {
        // This makes the source visible to the GC. If compilation fails, and no
        // script refers to it, it will be collected.
        if (ss)
            ss->attachToRuntime(rt);
    }
};

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

JSScript *
frontend::CompileScript(JSContext *cx, HandleObject scopeChain, StackFrame *callerFrame,
                        const CompileOptions &options,
                        const jschar *chars, size_t length,
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
    AutoAttachToRuntime attacher(cx->runtime);
    SourceCompressionToken sct(cx->runtime);
    ScriptSource *ss = NULL;
    if (!cx->hasRunOption(JSOPTION_ONLY_CNG_SOURCE) || options.compileAndGo) {
        ss = ScriptSource::createFromSource(cx, chars, length, false, &sct);
        if (!ss)
            return NULL;
        attacher.ss = ss;
    }

    Parser parser(cx, options, chars, length, /* foldConstants = */ true);
    if (!parser.init())
        return NULL;

    SharedContext sc(cx, scopeChain, /* fun = */ NULL, /* funbox = */ NULL, StrictModeFromContext(cx));

    TreeContext tc(&parser, &sc, staticLevel, /* bodyid = */ 0);
    if (!tc.init())
        return NULL;

    bool savedCallerFun = options.compileAndGo && callerFrame && callerFrame->isFunctionFrame();
    Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), savedCallerFun,
                                                  options, staticLevel, ss, 0, length));
    if (!script)
        return NULL;

    // We can specialize a bit for the given scope chain if that scope chain is the global object.
    JSObject *globalScope = scopeChain && scopeChain == &scopeChain->global() ? (JSObject*) scopeChain : NULL;
    JS_ASSERT_IF(globalScope, globalScope->isNative());
    JS_ASSERT_IF(globalScope, JSCLASS_HAS_GLOBAL_FLAG_AND_SLOTS(globalScope->getClass()));

    BytecodeEmitter bce(/* parent = */ NULL, &parser, &sc, script, callerFrame, !!globalScope,
                        options.lineno);
    if (!bce.init())
        return NULL;

    /* If this is a direct call to eval, inherit the caller's strictness.  */
    if (callerFrame && callerFrame->isScriptFrame() && callerFrame->script()->strictModeCode)
        sc.strictModeState = StrictMode::STRICT;

    if (options.compileAndGo) {
        if (source) {
            /*
             * Save eval program source in script->atoms[0] for the
             * eval cache (see EvalCacheLookup in jsobj.cpp).
             */
            JSAtom *atom = js_AtomizeString(cx, source);
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
            ObjectBox *funbox = parser.newObjectBox(callerFrame->fun());
            if (!funbox)
                return NULL;
            funbox->emitLink = bce.objectList.lastbox;
            bce.objectList.lastbox = funbox;
            bce.objectList.length++;
        }
    }

    ParseNode *pn;
#if JS_HAS_XML_SUPPORT
    pn = NULL;
    bool onlyXML;
    onlyXML = true;
#endif

    TokenStream &tokenStream = parser.tokenStream;
    {
        ParseNode *stringsAtStart = ListNode::create(PNK_STATEMENTLIST, &parser);
        if (!stringsAtStart)
            return NULL;
        stringsAtStart->makeEmpty();
        bool ok = parser.processDirectives(stringsAtStart) && EmitTree(cx, &bce, stringsAtStart);
        parser.freeTree(stringsAtStart);
        if (!ok)
            return NULL;
    }
    JS_ASSERT(sc.strictModeState != StrictMode::UNKNOWN);
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

        if (!FoldConstants(cx, pn, &parser))
            return NULL;

        if (!AnalyzeFunctions(&parser, callerFrame))
            return NULL;
        tc.functionList = NULL;

        if (!EmitTree(cx, &bce, pn))
            return NULL;

#if JS_HAS_XML_SUPPORT
        if (!pn->isKind(PNK_SEMI) || !pn->pn_kid || !pn->pn_kid->isXMLItem())
            onlyXML = false;
#endif
        parser.freeTree(pn);
    }

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
        PropertyName *arguments = cx->runtime->atomState.argumentsAtom;
        for (AtomDefnRange r = tc.lexdeps->all(); !r.empty(); r.popFront()) {
            if (r.front().key() == arguments) {
                parser.reportError(NULL, JSMSG_ARGUMENTS_AND_REST);
                return NULL;
            }
        }
        // We're not in a function context, so we don't expect any bindings.
        JS_ASSERT(!sc.bindings.hasBinding(cx, arguments));
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

    return script;
}

// Compile a JS function body, which might appear as the value of an event
// handler attribute in an HTML <INPUT> tag, or in a Function() constructor.
bool
frontend::CompileFunctionBody(JSContext *cx, HandleFunction fun, CompileOptions options,
                              Bindings *bindings, const jschar *chars, size_t length)
{
    if (!CheckLength(cx, length))
        return false;
    AutoAttachToRuntime attacher(cx->runtime);
    SourceCompressionToken sct(cx->runtime);
    ScriptSource *ss = ScriptSource::createFromSource(cx, chars, length, true, &sct);
    if (!ss)
        return NULL;
    attacher.ss = ss;

    options.setCompileAndGo(false);
    Parser parser(cx, options, chars, length, /* foldConstants = */ true);
    if (!parser.init())
        return false;

    JS_ASSERT(fun);
    SharedContext funsc(cx, /* scopeChain = */ NULL, fun, /* funbox = */ NULL,
                        StrictModeFromContext(cx));
    funsc.bindings.transfer(bindings);
    fun->setArgCount(funsc.bindings.numArgs());

    unsigned staticLevel = 0;
    TreeContext funtc(&parser, &funsc, staticLevel, /* bodyid = */ 0);
    if (!funtc.init())
        return false;

    Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), false, options,
                                                  staticLevel, ss, 0, length));
    if (!script)
        return false;

    StackFrame *nullCallerFrame = NULL;
    BytecodeEmitter funbce(/* parent = */ NULL, &parser, &funsc, script, nullCallerFrame,
                           /* hasGlobalScope = */ false, options.lineno);
    if (!funbce.init())
        return false;

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

    unsigned nargs = fun->nargs;
    if (nargs) {
        /*
         * NB: do not use AutoLocalNameArray because it will release space
         * allocated from cx->tempLifoAlloc by DefineArg.
         */
        BindingVector names(cx);
        if (!GetOrderedBindings(cx, funsc.bindings, &names))
            return false;

        for (unsigned i = 0; i < nargs; i++) {
            if (!DefineArg(fn, names[i].maybeName, i, &parser))
                return false;
        }
    }

    /*
     * After we're done parsing, we must fold constants, analyze any nested
     * functions, and generate code for this function, including a stop opcode
     * at the end.
     */
    ParseNode *pn = parser.functionBody(Parser::StatementListBody);
    if (!pn) 
        return false;

    if (!parser.tokenStream.matchToken(TOK_EOF)) {
        parser.reportError(NULL, JSMSG_SYNTAX_ERROR);
        return false;
    }

    if (!FoldConstants(cx, pn, &parser))
        return false;

    if (!AnalyzeFunctions(&parser, nullCallerFrame))
        return false;

    if (fn->pn_body) {
        JS_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
        fn->pn_body->append(pn);
        fn->pn_body->pn_pos = pn->pn_pos;
        pn = fn->pn_body;
    }

    if (!EmitFunctionScript(cx, &funbce, pn))
        return false;

    return true;
}
