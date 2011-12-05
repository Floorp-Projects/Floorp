/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "frontend/BytecodeCompiler.h"

#include "jsprobes.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/FoldConstants.h"
#include "frontend/SemanticAnalysis.h"
#include "vm/GlobalObject.h"

#include "jsinferinlines.h"

using namespace js;
using namespace js::frontend;

bool
DefineGlobals(JSContext *cx, GlobalScope &globalScope, JSScript *script)
{
    JSObject *globalObj = globalScope.globalObj;

    /* Define and update global properties. */
    for (size_t i = 0; i < globalScope.defs.length(); i++) {
        GlobalScope::GlobalDef &def = globalScope.defs[i];

        /* Names that could be resolved ahead of time can be skipped. */
        if (!def.atom)
            continue;

        jsid id = ATOM_TO_JSID(def.atom);
        Value rval;

        if (def.funbox) {
            JSFunction *fun = def.funbox->function();

            /*
             * No need to check for redeclarations or anything, global
             * optimizations only take place if the property is not defined.
             */
            rval.setObject(*fun);
            types::AddTypePropertyId(cx, globalObj, id, rval);
        } else {
            rval.setUndefined();
        }

        /*
         * Don't update the type information when defining the property for the
         * global object, per the consistency rules for type properties. If the
         * property is only undefined before it is ever written, we can check
         * the global directly during compilation and avoid having to emit type
         * checks every time it is accessed in the script.
         */
        const Shape *shape =
            DefineNativeProperty(cx, globalObj, id, rval, JS_PropertyStub, JS_StrictPropertyStub,
                                 JSPROP_ENUMERATE | JSPROP_PERMANENT, 0, 0, DNP_SKIP_TYPE);
        if (!shape)
            return false;
        def.knownSlot = shape->slot();
    }

    Vector<JSScript *, 16> worklist(cx);
    if (!worklist.append(script))
        return false;

    /*
     * Recursively walk through all scripts we just compiled. For each script,
     * go through all global uses. Each global use indexes into globalScope->defs.
     * Use this information to repoint each use to the correct slot in the global
     * object.
     */
    while (worklist.length()) {
        JSScript *outer = worklist.back();
        worklist.popBack();

        if (JSScript::isValidOffset(outer->objectsOffset)) {
            JSObjectArray *arr = outer->objects();

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
                if (!JSScript::isValidOffset(inner->globalsOffset) &&
                    !JSScript::isValidOffset(inner->objectsOffset)) {
                    continue;
                }
                if (!worklist.append(inner))
                    return false;
            }
        }

        if (!JSScript::isValidOffset(outer->globalsOffset))
            continue;

        GlobalSlotArray *globalUses = outer->globals();
        uint32 nGlobalUses = globalUses->length;
        for (uint32 i = 0; i < nGlobalUses; i++) {
            uint32 index = globalUses->vector[i].slot;
            JS_ASSERT(index < globalScope.defs.length());
            globalUses->vector[i].slot = globalScope.defs[index].knownSlot;
        }
    }

    return true;
}

JSScript *
frontend::CompileScript(JSContext *cx, JSObject *scopeChain, StackFrame *callerFrame,
                        JSPrincipals *principals, uint32 tcflags,
                        const jschar *chars, size_t length,
                        const char *filename, uintN lineno, JSVersion version,
                        JSString *source /* = NULL */,
                        uintN staticLevel /* = 0 */)
{
    TokenKind tt;
    ParseNode *pn;
    JSScript *script;
    bool inDirectivePrologue;

    JS_ASSERT(!(tcflags & ~(TCF_COMPILE_N_GO | TCF_NO_SCRIPT_RVAL | TCF_COMPILE_FOR_EVAL
                            | TCF_NEED_SCRIPT_GLOBAL)));

    /*
     * The scripted callerFrame can only be given for compile-and-go scripts
     * and non-zero static level requires callerFrame.
     */
    JS_ASSERT_IF(callerFrame, tcflags & TCF_COMPILE_N_GO);
    JS_ASSERT_IF(staticLevel != 0, callerFrame);

    Parser parser(cx, principals, callerFrame);
    if (!parser.init(chars, length, filename, lineno, version))
        return NULL;

    TokenStream &tokenStream = parser.tokenStream;

    BytecodeEmitter bce(&parser, tokenStream.getLineno());
    if (!bce.init(cx, TreeContext::USED_AS_TREE_CONTEXT))
        return NULL;

    Probes::compileScriptBegin(cx, filename, lineno);
    MUST_FLOW_THROUGH("out");

    // We can specialize a bit for the given scope chain if that scope chain is the global object.
    JSObject *globalObj = scopeChain && scopeChain == scopeChain->getGlobal()
                        ? scopeChain->getGlobal()
                        : NULL;

    JS_ASSERT_IF(globalObj, globalObj->isNative());
    JS_ASSERT_IF(globalObj, JSCLASS_HAS_GLOBAL_FLAG_AND_SLOTS(globalObj->getClass()));

    /* Null script early in case of error, to reduce our code footprint. */
    script = NULL;

    GlobalScope globalScope(cx, globalObj, &bce);
    bce.flags |= tcflags;
    bce.setScopeChain(scopeChain);
    bce.globalScope = &globalScope;
    if (!SetStaticLevel(&bce, staticLevel))
        goto out;

    /* If this is a direct call to eval, inherit the caller's strictness.  */
    if (callerFrame &&
        callerFrame->isScriptFrame() &&
        callerFrame->script()->strictModeCode) {
        bce.flags |= TCF_STRICT_MODE_CODE;
        tokenStream.setStrictMode();
    }

#ifdef DEBUG
    bool savedCallerFun;
    savedCallerFun = false;
#endif
    if (tcflags & TCF_COMPILE_N_GO) {
        if (source) {
            /*
             * Save eval program source in script->atoms[0] for the
             * eval cache (see EvalCacheLookup in jsobj.cpp).
             */
            JSAtom *atom = js_AtomizeString(cx, source);
            jsatomid _;
            if (!atom || !bce.makeAtomIndex(atom, &_))
                goto out;
        }

        if (callerFrame && callerFrame->isFunctionFrame()) {
            /*
             * An eval script in a caller frame needs to have its enclosing
             * function captured in case it refers to an upvar, and someone
             * wishes to decompile it while it's running.
             */
            ObjectBox *funbox = parser.newObjectBox(callerFrame->fun());
            if (!funbox)
                goto out;
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
    uint32 bodyid;
    if (!GenerateBlockId(&bce, bodyid))
        goto out;
    bce.bodyid = bodyid;

#if JS_HAS_XML_SUPPORT
    pn = NULL;
    bool onlyXML;
    onlyXML = true;
#endif

    inDirectivePrologue = true;
    tokenStream.setOctalCharacterEscape(false);
    for (;;) {
        tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt <= TOK_EOF) {
            if (tt == TOK_EOF)
                break;
            JS_ASSERT(tt == TOK_ERROR);
            goto out;
        }

        pn = parser.statement();
        if (!pn)
            goto out;
        JS_ASSERT(!bce.blockNode);

        if (inDirectivePrologue && !parser.recognizeDirectivePrologue(pn, &inDirectivePrologue))
            goto out;

        if (!FoldConstants(cx, pn, &bce))
            goto out;

        if (!AnalyzeFunctions(&bce))
            goto out;
        bce.functionList = NULL;

        if (!EmitTree(cx, &bce, pn))
            goto out;

#if JS_HAS_XML_SUPPORT
        if (!pn->isKind(PNK_SEMI) || !pn->pn_kid || !pn->pn_kid->isXMLItem())
            onlyXML = false;
#endif
        bce.freeTree(pn);
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
        goto out;
    }
#endif

    /*
     * Global variables (gvars) share the atom index space with locals. Due to
     * incremental code generation we need to patch the bytecode to adjust the
     * local references to skip the globals.
     */
    if (bce.hasSharps()) {
        jsbytecode *code, *end;
        JSOp op;
        const JSCodeSpec *cs;
        uintN len, slot;

        code = bce.base();
        for (end = code + bce.offset(); code != end; code += len) {
            JS_ASSERT(code < end);
            op = (JSOp) *code;
            cs = &js_CodeSpec[op];
            len = (cs->length > 0)
                  ? (uintN) cs->length
                  : js_GetVariableBytecodeLength(code);
            if ((cs->format & JOF_SHARPSLOT) ||
                JOF_TYPE(cs->format) == JOF_LOCAL ||
                (JOF_TYPE(cs->format) == JOF_SLOTATOM)) {
                JS_ASSERT_IF(!(cs->format & JOF_SHARPSLOT),
                             JOF_TYPE(cs->format) != JOF_SLOTATOM);
                slot = GET_SLOTNO(code);
                if (!(cs->format & JOF_SHARPSLOT))
                    slot += bce.sharpSlots();
                if (slot >= SLOTNO_LIMIT)
                    goto too_many_slots;
                SET_SLOTNO(code, slot);
            }
        }
    }

    /*
     * Nowadays the threaded interpreter needs a stop instruction, so we
     * do have to emit that here.
     */
    if (Emit1(cx, &bce, JSOP_STOP) < 0)
        goto out;

    JS_ASSERT(bce.version() == version);

    script = JSScript::NewScriptFromEmitter(cx, &bce);
    if (!script)
        goto out;

    JS_ASSERT(script->savedCallerFun == savedCallerFun);

    if (!DefineGlobals(cx, globalScope, script))
        script = NULL;

  out:
    Probes::compileScriptEnd(cx, script, filename, lineno);
    return script;

  too_many_slots:
    parser.reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_TOO_MANY_LOCALS);
    script = NULL;
    goto out;
}

/*
 * Compile a JS function body, which might appear as the value of an event
 * handler attribute in an HTML <INPUT> tag.
 */
bool
frontend::CompileFunctionBody(JSContext *cx, JSFunction *fun, JSPrincipals *principals,
                              Bindings *bindings, const jschar *chars, size_t length,
                              const char *filename, uintN lineno, JSVersion version)
{
    Parser parser(cx, principals);
    if (!parser.init(chars, length, filename, lineno, version))
        return false;

    TokenStream &tokenStream = parser.tokenStream;

    BytecodeEmitter funbce(&parser, tokenStream.getLineno());
    if (!funbce.init(cx, TreeContext::USED_AS_TREE_CONTEXT))
        return false;

    funbce.flags |= TCF_IN_FUNCTION;
    funbce.setFunction(fun);
    funbce.bindings.transfer(cx, bindings);
    fun->setArgCount(funbce.bindings.countArgs());
    if (!GenerateBlockId(&funbce, funbce.bodyid))
        return false;

    /* FIXME: make Function format the source for a function definition. */
    ParseNode *fn = FunctionNode::create(PNK_NAME, &funbce);
    if (fn) {
        fn->pn_body = NULL;
        fn->pn_cookie.makeFree();

        uintN nargs = fun->nargs;
        if (nargs) {
            /*
             * NB: do not use AutoLocalNameArray because it will release space
             * allocated from cx->tempLifoAlloc by DefineArg.
             */
            Vector<JSAtom *> names(cx);
            if (!funbce.bindings.getLocalNameArray(cx, &names)) {
                fn = NULL;
            } else {
                for (uintN i = 0; i < nargs; i++) {
                    if (!DefineArg(fn, names[i], i, &funbce)) {
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
        if (!CheckStrictParameters(cx, &funbce)) {
            pn = NULL;
        } else if (!tokenStream.matchToken(TOK_EOF)) {
            parser.reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
            pn = NULL;
        } else if (!FoldConstants(cx, pn, &funbce)) {
            /* FoldConstants reported the error already. */
            pn = NULL;
        } else if (!AnalyzeFunctions(&funbce)) {
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
