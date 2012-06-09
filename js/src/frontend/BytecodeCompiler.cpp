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

bool
MarkInnerAndOuterFunctions(JSContext *cx, JSScript* script_)
{
    Rooted<JSScript*> script(cx, script_);

    Vector<JSScript *, 16> worklist(cx);
    if (!worklist.append(script.reference()))
        return false;

    while (worklist.length()) {
        JSScript *outer = worklist.back();
        worklist.popBack();

        /*
         * If outer has an extensible scope, its slots may be resized which
         * will invalidate nesting->varArray/argArray.
         */
        if (outer->funHasExtensibleScope)
            continue;

        if (outer->hasObjects()) {
            ObjectArray *arr = outer->objects();

            /*
             * If this is an eval script, don't treat the saved caller function
             * stored in the first object slot as an inner function.
             */
            size_t start = outer->savedCallerFun ? 1 : 0;

            for (size_t i = start; i < arr->length; i++) {
                JSObject *obj = arr->vector[i];
                if (!obj->isFunction())
                    continue;
                JSFunction *fun = obj->toFunction();
                JS_ASSERT(fun->isInterpreted());
                JSScript *inner = fun->script();
                if (outer->function() && outer->function()->isHeavyweight()) {
                    outer->isOuterFunction = true;
                    inner->isInnerFunction = true;
                }
                if (!inner->hasObjects())
                    continue;
                if (!worklist.append(inner))
                    return false;
            }
        }
    }

    return true;
}

JSScript *
frontend::CompileScript(JSContext *cx, JSObject *scopeChain, StackFrame *callerFrame,
                        JSPrincipals *principals, JSPrincipals *originPrincipals,
                        bool compileAndGo, bool noScriptRval, bool needScriptGlobal,
                        const jschar *chars, size_t length,
                        const char *filename, unsigned lineno, JSVersion version,
                        JSString *source /* = NULL */,
                        unsigned staticLevel /* = 0 */)
{
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
    ProbesManager probesManager(filename, lineno);

    /*
     * The scripted callerFrame can only be given for compile-and-go scripts
     * and non-zero static level requires callerFrame.
     */
    JS_ASSERT_IF(callerFrame, compileAndGo);
    JS_ASSERT_IF(staticLevel != 0, callerFrame);

    Parser parser(cx, principals, originPrincipals, chars, length, filename, lineno, version,
                  callerFrame, /* foldConstants = */ true, compileAndGo);
    if (!parser.init())
        return NULL;

    SharedContext sc(cx, scopeChain, /* fun = */ NULL, /* funbox = */ NULL, staticLevel);

    TreeContext tc(&parser, &sc);
    if (!tc.init())
        return NULL;

    BytecodeEmitter bce(&parser, &sc, lineno, noScriptRval, needScriptGlobal);
    if (!bce.init())
        return NULL;

    // We can specialize a bit for the given scope chain if that scope chain is the global object.
    JSObject *globalObj = scopeChain && scopeChain == &scopeChain->global()
                          ? &scopeChain->global()
                          : NULL;

    JS_ASSERT_IF(globalObj, globalObj->isNative());
    JS_ASSERT_IF(globalObj, JSCLASS_HAS_GLOBAL_FLAG_AND_SLOTS(globalObj->getClass()));

    GlobalScope globalScope(cx, globalObj);
    bce.globalScope = &globalScope;

    /* If this is a direct call to eval, inherit the caller's strictness.  */
    if (callerFrame && callerFrame->isScriptFrame() && callerFrame->script()->strictModeCode)
        sc.setInStrictMode();

#ifdef DEBUG
    bool savedCallerFun;
    savedCallerFun = false;
#endif
    if (compileAndGo) {
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
#ifdef DEBUG
            savedCallerFun = true;
#endif
        }
    }

    /*
     * Inline this->statements to emit as we go to save AST space. We must
     * generate our script-body blockid since we aren't calling Statements.
     */
    if (!GenerateBlockId(&sc, sc.bodyid))
        return NULL;

    ParseNode *pn;
#if JS_HAS_XML_SUPPORT
    pn = NULL;
    bool onlyXML;
    onlyXML = true;
#endif

    bool inDirectivePrologue = true;
    TokenStream &tokenStream = parser.tokenStream;
    tokenStream.setOctalCharacterEscape(false);
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

        if (inDirectivePrologue && !parser.recognizeDirectivePrologue(pn, &inDirectivePrologue))
            return NULL;

        if (!FoldConstants(cx, pn, bce.parser))
            return NULL;

        if (!AnalyzeFunctions(bce.parser))
            return NULL;
        tc.functionList = NULL;

        if (!EmitTree(cx, &bce, pn))
            return NULL;

#if JS_HAS_XML_SUPPORT
        if (!pn->isKind(PNK_SEMI) || !pn->pn_kid || !pn->pn_kid->isXMLItem())
            onlyXML = false;
#endif
        bce.parser->freeTree(pn);
    }

#if JS_HAS_XML_SUPPORT
    /*
     * Prevent XML data theft via <script src="http://victim.com/foo.xml">.
     * For background, see:
     *
     * https://bugzilla.mozilla.org/show_bug.cgi?id=336551
     */
    if (pn && onlyXML && !callerFrame) {
        parser.reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_XML_WHOLE_PROGRAM);
        return NULL;
    }
#endif

    if (!parser.checkForArgumentsAndRest())
        return NULL;

    /*
     * Nowadays the threaded interpreter needs a stop instruction, so we
     * do have to emit that here.
     */
    if (Emit1(cx, &bce, JSOP_STOP) < 0)
        return NULL;

    JS_ASSERT(bce.version() == version);

    Rooted<JSScript*> script(cx);
    script = JSScript::NewScriptFromEmitter(cx, &bce);
    if (!script)
        return NULL;

    JS_ASSERT(script->savedCallerFun == savedCallerFun);

    if (!MarkInnerAndOuterFunctions(cx, script))
        return NULL;

    return script;
}

/*
 * Compile a JS function body, which might appear as the value of an event
 * handler attribute in an HTML <INPUT> tag.
 */
bool
frontend::CompileFunctionBody(JSContext *cx, JSFunction *fun,
                              JSPrincipals *principals, JSPrincipals *originPrincipals,
                              Bindings *bindings, const jschar *chars, size_t length,
                              const char *filename, unsigned lineno, JSVersion version)
{
    Parser parser(cx, principals, originPrincipals, chars, length, filename, lineno, version,
                  /* cfp = */ NULL, /* foldConstants = */ true, /* compileAndGo = */ false);
    if (!parser.init())
        return false;

    JS_ASSERT(fun);
    SharedContext funsc(cx, /* scopeChain = */ NULL, fun, /* funbox = */ NULL,
                        /* staticLevel = */ 0);

    TreeContext funtc(&parser, &funsc);
    if (!funtc.init())
        return NULL;

    BytecodeEmitter funbce(&parser, &funsc, lineno,
                           /* noScriptRval = */ false, /* needsScriptGlobal = */ false);
    if (!funbce.init())
        return false;

    funsc.bindings.transfer(cx, bindings);
    fun->setArgCount(funsc.bindings.numArgs());
    if (!GenerateBlockId(&funsc, funsc.bodyid))
        return false;

    /* FIXME: make Function format the source for a function definition. */
    ParseNode *fn = FunctionNode::create(PNK_NAME, &parser);
    if (fn) {
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
            BindingNames names(cx);
            if (!funsc.bindings.getLocalNameArray(cx, &names)) {
                fn = NULL;
            } else {
                for (unsigned i = 0; i < nargs; i++) {
                    if (!DefineArg(fn, names[i].maybeAtom, i, &parser)) {
                        fn = NULL;
                        break;
                    }
                }
            }
        }
    }

    /*
     * After we're done parsing, we must fold constants, analyze any nested
     * functions, and generate code for this function, including a stop opcode
     * at the end.
     */
    ParseNode *pn = fn ? parser.functionBody(Parser::StatementListBody) : NULL;
    if (pn) {
        if (!parser.tokenStream.matchToken(TOK_EOF)) {
            parser.reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
            pn = NULL;
        } else if (!FoldConstants(cx, pn, &parser)) {
            /* FoldConstants reported the error already. */
            pn = NULL;
        } else if (!AnalyzeFunctions(&parser)) {
            pn = NULL;
        } else {
            if (fn->pn_body) {
                JS_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
                fn->pn_body->append(pn);
                fn->pn_body->pn_pos = pn->pn_pos;
                pn = fn->pn_body;
            }

            if (!EmitFunctionScript(cx, &funbce, pn))
                pn = NULL;
        }
    }

    return pn != NULL;
}
