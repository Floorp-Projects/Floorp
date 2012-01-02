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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * JS parser.
 *
 * This is a recursive-descent parser for the JavaScript language specified by
 * "The JavaScript 1.5 Language Specification".  It uses lexical and semantic
 * feedback to disambiguate non-LL(1) structures.  It generates trees of nodes
 * induced by the recursive parsing (not precise syntax trees, see Parser.h).
 * After tree construction, it rewrites trees to fold constants and evaluate
 * compile-time expressions.  Finally, it calls js::frontend::EmitTree (see
 * BytecodeEmitter.h) to generate bytecode.
 *
 * This parser attempts no error recovery.
 */

#include "frontend/Parser.h"

#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/FoldConstants.h"
#include "frontend/ParseMaps.h"
#include "frontend/TokenStream.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#if JS_HAS_DESTRUCTURING
#include "jsdhash.h"
#endif

#include "jsatominlines.h"
#include "jsscriptinlines.h"

#include "frontend/BytecodeEmitter-inl.h"
#include "frontend/ParseMaps-inl.h"
#include "frontend/ParseNode-inl.h"
#include "vm/RegExpObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::frontend;

/*
 * Insist that the next token be of type tt, or report errno and return null.
 * NB: this macro uses cx and ts from its lexical environment.
 */
#define MUST_MATCH_TOKEN_WITH_FLAGS(tt, errno, __flags)                                     \
    JS_BEGIN_MACRO                                                                          \
        if (tokenStream.getToken((__flags)) != tt) {                                        \
            reportErrorNumber(NULL, JSREPORT_ERROR, errno);                                 \
            return NULL;                                                                    \
        }                                                                                   \
    JS_END_MACRO
#define MUST_MATCH_TOKEN(tt, errno) MUST_MATCH_TOKEN_WITH_FLAGS(tt, errno, 0)

Parser::Parser(JSContext *cx, JSPrincipals *prin, JSPrincipals *originPrin,
               StackFrame *cfp, bool foldConstants)
  : AutoGCRooter(cx, PARSER),
    context(cx),
    tokenStream(cx, prin, originPrin),
    principals(NULL),
    originPrincipals(NULL),
    callerFrame(cfp),
    callerVarObj(cfp ? &cfp->varObj() : NULL),
    allocator(cx),
    functionCount(0),
    traceListHead(NULL),
    tc(NULL),
    keepAtoms(cx->runtime),
    foldConstants(foldConstants)
{
    cx->activeCompilations++;
    PodArrayZero(tempFreeList);
    setPrincipals(prin, originPrin);
    JS_ASSERT_IF(cfp, cfp->isScriptFrame());
}

bool
Parser::init(const jschar *base, size_t length, const char *filename, uintN lineno,
             JSVersion version)
{
    JSContext *cx = context;
    if (!cx->ensureParseMapPool())
        return false;
    tempPoolMark = cx->tempLifoAlloc().mark();
    if (!tokenStream.init(base, length, filename, lineno, version)) {
        cx->tempLifoAlloc().release(tempPoolMark);
        return false;
    }
    return true;
}

Parser::~Parser()
{
    JSContext *cx = context;
    if (principals)
        JSPRINCIPALS_DROP(cx, principals);
    if (originPrincipals)
        JSPRINCIPALS_DROP(cx, originPrincipals);
    cx->tempLifoAlloc().release(tempPoolMark);
    cx->activeCompilations--;
}

void
Parser::setPrincipals(JSPrincipals *prin, JSPrincipals *originPrin)
{
    JS_ASSERT(!principals && !originPrincipals);
    principals = prin;
    if (principals)
        JSPRINCIPALS_HOLD(context, principals);
    originPrincipals = originPrin;
    if (originPrincipals)
        JSPRINCIPALS_HOLD(context, originPrincipals);
}

ObjectBox *
Parser::newObjectBox(JSObject *obj)
{
    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    ObjectBox *objbox = context->tempLifoAlloc().new_<ObjectBox>();
    if (!objbox) {
        js_ReportOutOfMemory(context);
        return NULL;
    }
    objbox->traceLink = traceListHead;
    traceListHead = objbox;
    objbox->emitLink = NULL;
    objbox->object = obj;
    objbox->isFunctionBox = false;
    return objbox;
}

FunctionBox *
Parser::newFunctionBox(JSObject *obj, ParseNode *fn, TreeContext *tc)
{
    JS_ASSERT(obj);
    JS_ASSERT(obj->isFunction());

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    FunctionBox *funbox = context->tempLifoAlloc().newPod<FunctionBox>();
    if (!funbox) {
        js_ReportOutOfMemory(context);
        return NULL;
    }
    funbox->traceLink = traceListHead;
    traceListHead = funbox;
    funbox->emitLink = NULL;
    funbox->object = obj;
    funbox->isFunctionBox = true;
    funbox->node = fn;
    funbox->siblings = tc->functionList;
    tc->functionList = funbox;
    ++tc->parser->functionCount;
    funbox->kids = NULL;
    funbox->parent = tc->funbox;
    funbox->methods = NULL;
    new (&funbox->bindings) Bindings(context);
    funbox->queued = false;
    funbox->inLoop = false;
    for (StmtInfo *stmt = tc->topStmt; stmt; stmt = stmt->down) {
        if (STMT_IS_LOOP(stmt)) {
            funbox->inLoop = true;
            break;
        }
    }
    funbox->level = tc->staticLevel;
    funbox->tcflags = (TCF_IN_FUNCTION | (tc->flags & (TCF_COMPILE_N_GO | TCF_STRICT_MODE_CODE)));
    if (tc->innermostWith)
        funbox->tcflags |= TCF_IN_WITH;
    return funbox;
}

void
Parser::trace(JSTracer *trc)
{
    ObjectBox *objbox = traceListHead;
    while (objbox) {
        MarkRoot(trc, objbox->object, "parser.object");
        if (objbox->isFunctionBox)
            static_cast<FunctionBox *>(objbox)->bindings.trace(trc);
        objbox = objbox->traceLink;
    }

    for (TreeContext *tc = this->tc; tc; tc = tc->parent)
        tc->trace(trc);
}

static bool
GenerateBlockIdForStmtNode(ParseNode *pn, TreeContext *tc)
{
    JS_ASSERT(tc->topStmt);
    JS_ASSERT(STMT_MAYBE_SCOPE(tc->topStmt));
    JS_ASSERT(pn->isKind(PNK_STATEMENTLIST) || pn->isKind(PNK_LEXICALSCOPE));
    if (!GenerateBlockId(tc, tc->topStmt->blockid))
        return false;
    pn->pn_blockid = tc->topStmt->blockid;
    return true;
}

/*
 * Parse a top-level JS script.
 */
ParseNode *
Parser::parse(JSObject *chain)
{
    /*
     * Protect atoms from being collected by a GC activation, which might
     * - nest on this thread due to out of memory (the so-called "last ditch"
     *   GC attempted within js_NewGCThing), or
     * - run for any reason on another thread if this thread is suspended on
     *   an object lock before it finishes generating bytecode into a script
     *   protected from the GC by a root or a stack frame reference.
     */
    TreeContext globaltc(this);
    if (!globaltc.init(context))
        return NULL;
    globaltc.setScopeChain(chain);
    if (!GenerateBlockId(&globaltc, globaltc.bodyid))
        return NULL;

    ParseNode *pn = statements();
    if (pn) {
        if (!tokenStream.matchToken(TOK_EOF)) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
            pn = NULL;
        } else if (foldConstants) {
            if (!FoldConstants(context, pn, &globaltc))
                pn = NULL;
        }
    }
    return pn;
}

JS_STATIC_ASSERT(UpvarCookie::FREE_LEVEL == JS_BITMASK(JSFB_LEVEL_BITS));

/*
 * Insist on a final return before control flows out of pn.  Try to be a bit
 * smart about loops: do {...; return e2;} while(0) at the end of a function
 * that contains an early return e1 will get a strict warning.  Similarly for
 * iloops: while (true){...} is treated as though ... returns.
 */
#define ENDS_IN_OTHER   0
#define ENDS_IN_RETURN  1
#define ENDS_IN_BREAK   2

static int
HasFinalReturn(ParseNode *pn)
{
    ParseNode *pn2, *pn3;
    uintN rv, rv2, hasDefault;

    switch (pn->getKind()) {
      case PNK_STATEMENTLIST:
        if (!pn->pn_head)
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->last());

      case PNK_IF:
        if (!pn->pn_kid3)
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->pn_kid2) & HasFinalReturn(pn->pn_kid3);

      case PNK_WHILE:
        pn2 = pn->pn_left;
        if (pn2->isKind(PNK_TRUE))
            return ENDS_IN_RETURN;
        if (pn2->isKind(PNK_NUMBER) && pn2->pn_dval)
            return ENDS_IN_RETURN;
        return ENDS_IN_OTHER;

      case PNK_DOWHILE:
        pn2 = pn->pn_right;
        if (pn2->isKind(PNK_FALSE))
            return HasFinalReturn(pn->pn_left);
        if (pn2->isKind(PNK_TRUE))
            return ENDS_IN_RETURN;
        if (pn2->isKind(PNK_NUMBER)) {
            if (pn2->pn_dval == 0)
                return HasFinalReturn(pn->pn_left);
            return ENDS_IN_RETURN;
        }
        return ENDS_IN_OTHER;

      case PNK_FOR:
        pn2 = pn->pn_left;
        if (pn2->isArity(PN_TERNARY) && !pn2->pn_kid2)
            return ENDS_IN_RETURN;
        return ENDS_IN_OTHER;

      case PNK_SWITCH:
        rv = ENDS_IN_RETURN;
        hasDefault = ENDS_IN_OTHER;
        pn2 = pn->pn_right;
        if (pn2->isKind(PNK_LEXICALSCOPE))
            pn2 = pn2->expr();
        for (pn2 = pn2->pn_head; rv && pn2; pn2 = pn2->pn_next) {
            if (pn2->isKind(PNK_DEFAULT))
                hasDefault = ENDS_IN_RETURN;
            pn3 = pn2->pn_right;
            JS_ASSERT(pn3->isKind(PNK_STATEMENTLIST));
            if (pn3->pn_head) {
                rv2 = HasFinalReturn(pn3->last());
                if (rv2 == ENDS_IN_OTHER && pn2->pn_next)
                    /* Falling through to next case or default. */;
                else
                    rv &= rv2;
            }
        }
        /* If a final switch has no default case, we judge it harshly. */
        rv &= hasDefault;
        return rv;

      case PNK_BREAK:
        return ENDS_IN_BREAK;

      case PNK_WITH:
        return HasFinalReturn(pn->pn_right);

      case PNK_RETURN:
        return ENDS_IN_RETURN;

      case PNK_COLON:
      case PNK_LEXICALSCOPE:
        return HasFinalReturn(pn->expr());

      case PNK_THROW:
        return ENDS_IN_RETURN;

      case PNK_TRY:
        /* If we have a finally block that returns, we are done. */
        if (pn->pn_kid3) {
            rv = HasFinalReturn(pn->pn_kid3);
            if (rv == ENDS_IN_RETURN)
                return rv;
        }

        /* Else check the try block and any and all catch statements. */
        rv = HasFinalReturn(pn->pn_kid1);
        if (pn->pn_kid2) {
            JS_ASSERT(pn->pn_kid2->isArity(PN_LIST));
            for (pn2 = pn->pn_kid2->pn_head; pn2; pn2 = pn2->pn_next)
                rv &= HasFinalReturn(pn2);
        }
        return rv;

      case PNK_CATCH:
        /* Check this catch block's body. */
        return HasFinalReturn(pn->pn_kid3);

      case PNK_LET:
        /* Non-binary let statements are let declarations. */
        if (!pn->isArity(PN_BINARY))
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->pn_right);

      default:
        return ENDS_IN_OTHER;
    }
}

static JSBool
ReportBadReturn(JSContext *cx, TreeContext *tc, ParseNode *pn, uintN flags, uintN errnum,
                uintN anonerrnum)
{
    JSAutoByteString name;
    if (tc->fun()->atom) {
        if (!js_AtomToPrintableString(cx, tc->fun()->atom, &name))
            return false;
    } else {
        errnum = anonerrnum;
    }
    return ReportCompileErrorNumber(cx, TS(tc->parser), pn, flags, errnum, name.ptr());
}

static JSBool
CheckFinalReturn(JSContext *cx, TreeContext *tc, ParseNode *pn)
{
    JS_ASSERT(tc->inFunction());
    return HasFinalReturn(pn) == ENDS_IN_RETURN ||
           ReportBadReturn(cx, tc, pn, JSREPORT_WARNING | JSREPORT_STRICT,
                           JSMSG_NO_RETURN_VALUE, JSMSG_ANON_NO_RETURN_VALUE);
}

/*
 * Check that it is permitted to assign to lhs.  Strict mode code may not
 * assign to 'eval' or 'arguments'.
 */
static bool
CheckStrictAssignment(JSContext *cx, TreeContext *tc, ParseNode *lhs)
{
    if (tc->needStrictChecks() && lhs->isKind(PNK_NAME)) {
        JSAtom *atom = lhs->pn_atom;
        JSAtomState *atomState = &cx->runtime->atomState;
        if (atom == atomState->evalAtom || atom == atomState->argumentsAtom) {
            JSAutoByteString name;
            if (!js_AtomToPrintableString(cx, atom, &name) ||
                !ReportStrictModeError(cx, TS(tc->parser), tc, lhs, JSMSG_DEPRECATED_ASSIGN,
                                       name.ptr())) {
                return false;
            }
        }
    }
    return true;
}

/*
 * Check that it is permitted to introduce a binding for atom.  Strict mode
 * forbids introducing new definitions for 'eval', 'arguments', or for any
 * strict mode reserved keyword.  Use pn for reporting error locations, or use
 * tc's token stream if pn is NULL.
 */
bool
CheckStrictBinding(JSContext *cx, TreeContext *tc, PropertyName *name, ParseNode *pn)
{
    if (!tc->needStrictChecks())
        return true;

    JSAtomState *atomState = &cx->runtime->atomState;
    if (name == atomState->evalAtom ||
        name == atomState->argumentsAtom ||
        FindKeyword(name->charsZ(), name->length()))
    {
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(cx, name, &bytes))
            return false;
        return ReportStrictModeError(cx, TS(tc->parser), tc, pn, JSMSG_BAD_BINDING, bytes.ptr());
    }

    return true;
}

static bool
ReportBadParameter(JSContext *cx, TreeContext *tc, JSAtom *name, uintN errorNumber)
{
    Definition *dn = tc->decls.lookupFirst(name);
    JSAutoByteString bytes;
    return js_AtomToPrintableString(cx, name, &bytes) &&
           ReportStrictModeError(cx, TS(tc->parser), tc, dn, errorNumber, bytes.ptr());
}

/*
 * In strict mode code, all parameter names must be distinct, must not be
 * strict mode reserved keywords, and must not be 'eval' or 'arguments'.  We
 * must perform these checks here, and not eagerly during parsing, because a
 * function's body may turn on strict mode for the function head.
 */
bool
js::CheckStrictParameters(JSContext *cx, TreeContext *tc)
{
    JS_ASSERT(tc->inFunction());

    if (!tc->needStrictChecks() || tc->bindings.countArgs() == 0)
        return true;

    JSAtom *argumentsAtom = cx->runtime->atomState.argumentsAtom;
    JSAtom *evalAtom = cx->runtime->atomState.evalAtom;

    /* name => whether we've warned about the name already */
    HashMap<JSAtom *, bool> parameters(cx);
    if (!parameters.init(tc->bindings.countArgs()))
        return false;

    /* Start with lastVariable(), not lastArgument(), for destructuring. */
    for (Shape::Range r = tc->bindings.lastVariable(); !r.empty(); r.popFront()) {
        jsid id = r.front().propid();
        if (!JSID_IS_ATOM(id))
            continue;

        JSAtom *name = JSID_TO_ATOM(id);

        if (name == argumentsAtom || name == evalAtom) {
            if (!ReportBadParameter(cx, tc, name, JSMSG_BAD_BINDING))
                return false;
        }

        if (tc->inStrictMode() && FindKeyword(name->charsZ(), name->length())) {
            /*
             * JSOPTION_STRICT is supposed to warn about future keywords, too,
             * but we took care of that in the scanner.
             */
            JS_ALWAYS_TRUE(!ReportBadParameter(cx, tc, name, JSMSG_RESERVED_ID));
            return false;
        }

        /*
         * Check for a duplicate parameter: warn or report an error exactly
         * once for each duplicated parameter.
         */
        if (HashMap<JSAtom *, bool>::AddPtr p = parameters.lookupForAdd(name)) {
            if (!p->value && !ReportBadParameter(cx, tc, name, JSMSG_DUPLICATE_FORMAL))
                return false;
            p->value = true;
        } else {
            if (!parameters.add(p, name, false))
                return false;
        }
    }

    return true;
}

ParseNode *
Parser::functionBody(FunctionBodyType type)
{
    JS_ASSERT(tc->inFunction());

    StmtInfo stmtInfo;
    PushStatement(tc, &stmtInfo, STMT_BLOCK, -1);
    stmtInfo.flags = SIF_BODY_BLOCK;

    uintN oldflags = tc->flags;
    tc->flags &= ~(TCF_RETURN_EXPR | TCF_RETURN_VOID);

    ParseNode *pn;
    if (type == StatementListBody) {
        pn = statements();
    } else {
        JS_ASSERT(type == ExpressionBody);
        JS_ASSERT(JS_HAS_EXPR_CLOSURES);
        pn = UnaryNode::create(PNK_RETURN, tc);
        if (pn) {
            pn->pn_kid = assignExpr();
            if (!pn->pn_kid) {
                pn = NULL;
            } else {
                if (tc->flags & TCF_FUN_IS_GENERATOR) {
                    ReportBadReturn(context, tc, pn, JSREPORT_ERROR,
                                    JSMSG_BAD_GENERATOR_RETURN,
                                    JSMSG_BAD_ANON_GENERATOR_RETURN);
                    pn = NULL;
                } else {
                    pn->setOp(JSOP_RETURN);
                    pn->pn_pos.end = pn->pn_kid->pn_pos.end;
                }
            }
        }
    }

    if (pn) {
        JS_ASSERT(!(tc->topStmt->flags & SIF_SCOPE));
        PopStatementTC(tc);

        /* Check for falling off the end of a function that returns a value. */
        if (context->hasStrictOption() && (tc->flags & TCF_RETURN_EXPR) &&
            !CheckFinalReturn(context, tc, pn)) {
            pn = NULL;
        }
    }

    tc->flags = oldflags | (tc->flags & TCF_FUN_FLAGS);
    return pn;
}

/* Create a placeholder Definition node for |atom|. */
static Definition *
MakePlaceholder(ParseNode *pn, TreeContext *tc)
{
    Definition *dn = (Definition *) NameNode::create(PNK_NAME, pn->pn_atom, tc);
    if (!dn)
        return NULL;

    dn->setOp(JSOP_NOP);
    dn->setDefn(true);
    dn->pn_dflags |= PND_PLACEHOLDER;
    return dn;
}

static bool
Define(ParseNode *pn, JSAtom *atom, TreeContext *tc, bool let = false)
{
    JS_ASSERT(!pn->isUsed());
    JS_ASSERT_IF(pn->isDefn(), pn->isPlaceholder());

    bool foundLexdep = false;
    Definition *dn = NULL;

    if (let)
        dn = tc->decls.lookupFirst(atom);

    if (!dn) {
        dn = tc->lexdeps.lookupDefn(atom);
        foundLexdep = !!dn;
    }

    if (dn && dn != pn) {
        ParseNode **pnup = &dn->dn_uses;
        ParseNode *pnu;
        uintN start = let ? pn->pn_blockid : tc->bodyid;

        while ((pnu = *pnup) != NULL && pnu->pn_blockid >= start) {
            JS_ASSERT(pnu->isUsed());
            pnu->pn_lexdef = (Definition *) pn;
            pn->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
            pnup = &pnu->pn_link;
        }

        if (pnu != dn->dn_uses) {
            *pnup = pn->dn_uses;
            pn->dn_uses = dn->dn_uses;
            dn->dn_uses = pnu;

            if ((!pnu || pnu->pn_blockid < tc->bodyid) && foundLexdep)
                tc->lexdeps->remove(atom);
        }

        pn->pn_dflags |= dn->pn_dflags & PND_CLOSED;
    }

    Definition *toAdd = (Definition *) pn;
    bool ok = let ? tc->decls.addShadow(atom, toAdd) : tc->decls.addUnique(atom, toAdd);
    if (!ok)
        return false;
    pn->setDefn(true);
    pn->pn_dflags &= ~PND_PLACEHOLDER;
    if (!tc->parent)
        pn->pn_dflags |= PND_TOPLEVEL;
    return true;
}

static void
ForgetUse(ParseNode *pn)
{
    if (!pn->isUsed()) {
        JS_ASSERT(!pn->isDefn());
        return;
    }

    ParseNode **pnup = &pn->lexdef()->dn_uses;
    ParseNode *pnu;
    while ((pnu = *pnup) != pn)
        pnup = &pnu->pn_link;
    *pnup = pn->pn_link;
    pn->setUsed(false);
}

static ParseNode *
MakeAssignment(ParseNode *pn, ParseNode *rhs, TreeContext *tc)
{
    ParseNode *lhs = tc->parser->new_<ParseNode>(*pn);
    if (!lhs)
        return NULL;

    if (pn->isUsed()) {
        Definition *dn = pn->pn_lexdef;
        ParseNode **pnup = &dn->dn_uses;

        while (*pnup != pn)
            pnup = &(*pnup)->pn_link;
        *pnup = lhs;
        lhs->pn_link = pn->pn_link;
        pn->pn_link = NULL;
    }

    pn->setKind(PNK_ASSIGN);
    pn->setOp(JSOP_NOP);
    pn->setArity(PN_BINARY);
    pn->setInParens(false);
    pn->setUsed(false);
    pn->setDefn(false);
    pn->pn_left = lhs;
    pn->pn_right = rhs;
    return lhs;
}

static ParseNode *
MakeDefIntoUse(Definition *dn, ParseNode *pn, JSAtom *atom, TreeContext *tc)
{
    /*
     * If dn is arg, or in [var, const, let] and has an initializer, then we
     * must rewrite it to be an assignment node, whose freshly allocated
     * left-hand side becomes a use of pn.
     */
    if (dn->isBindingForm()) {
        ParseNode *rhs = dn->expr();
        if (rhs) {
            ParseNode *lhs = MakeAssignment(dn, rhs, tc);
            if (!lhs)
                return NULL;
            //pn->dn_uses = lhs;
            dn = (Definition *) lhs;
        }

        dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETNAME : JSOP_NAME);
    } else if (dn->kind() == Definition::FUNCTION) {
        JS_ASSERT(dn->isOp(JSOP_NOP));
        tc->parser->prepareNodeForMutation(dn);
        dn->setKind(PNK_NAME);
        dn->setArity(PN_NAME);
        dn->pn_atom = atom;
    }

    /* Now make dn no longer a definition, rather a use of pn. */
    JS_ASSERT(dn->isKind(PNK_NAME));
    JS_ASSERT(dn->isArity(PN_NAME));
    JS_ASSERT(dn->pn_atom == atom);

    for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
        JS_ASSERT(pnu->isUsed());
        JS_ASSERT(!pnu->isDefn());
        pnu->pn_lexdef = (Definition *) pn;
        pn->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
    }
    pn->pn_dflags |= dn->pn_dflags & PND_USE2DEF_FLAGS;
    pn->dn_uses = dn;

    dn->setDefn(false);
    dn->setUsed(true);
    dn->pn_lexdef = (Definition *) pn;
    dn->pn_cookie.makeFree();
    dn->pn_dflags &= ~PND_BOUND;
    return dn;
}

bool
js::DefineArg(ParseNode *pn, JSAtom *atom, uintN i, TreeContext *tc)
{
    /* Flag tc so we don't have to lookup arguments on every use. */
    if (atom == tc->parser->context->runtime->atomState.argumentsAtom)
        tc->flags |= TCF_FUN_PARAM_ARGUMENTS;

    /*
     * Make an argument definition node, distinguished by being in tc->decls
     * but having PNK_NAME kind and JSOP_NOP op. Insert it in a PNK_ARGSBODY
     * list node returned via pn->pn_body.
     */
    ParseNode *argpn = NameNode::create(PNK_NAME, atom, tc);
    if (!argpn)
        return false;
    JS_ASSERT(argpn->isKind(PNK_NAME) && argpn->isOp(JSOP_NOP));

    /* Arguments are initialized by definition. */
    argpn->pn_dflags |= PND_INITIALIZED;
    if (!Define(argpn, atom, tc))
        return false;

    ParseNode *argsbody = pn->pn_body;
    if (!argsbody) {
        argsbody = ListNode::create(PNK_ARGSBODY, tc);
        if (!argsbody)
            return false;
        argsbody->setOp(JSOP_NOP);
        argsbody->makeEmpty();
        pn->pn_body = argsbody;
    }
    argsbody->append(argpn);

    argpn->setOp(JSOP_GETARG);
    argpn->pn_cookie.set(tc->staticLevel, i);
    argpn->pn_dflags |= PND_BOUND;
    return true;
}

/*
 * Parameter block types for the several Binder functions.  We use a common
 * helper function signature in order to share code among destructuring and
 * simple variable declaration parsers.  In the destructuring case, the binder
 * function is called indirectly from the variable declaration parser by way
 * of CheckDestructuring and its friends.
 */
typedef JSBool
(*Binder)(JSContext *cx, BindData *data, JSAtom *atom, TreeContext *tc);

static JSBool
BindLet(JSContext *cx, BindData *data, JSAtom *atom, TreeContext *tc);

static JSBool
BindVarOrConst(JSContext *cx, BindData *data, JSAtom *atom, TreeContext *tc);

struct BindData {
    BindData() : fresh(true) {}

    ParseNode       *pn;        /* name node for definition processing and
                                   error source coordinates */
    JSOp            op;         /* prolog bytecode or nop */
    Binder          binder;     /* binder, discriminates u */
    union {
        struct {
            VarContext varContext;
            StaticBlockObject *blockObj;
            uintN   overflow;
        } let;
    };
    bool fresh;

    void initLet(VarContext varContext, StaticBlockObject &blockObj, uintN overflow) {
        this->pn = NULL;
        this->op = JSOP_NOP;
        this->binder = BindLet;
        this->let.varContext = varContext;
        this->let.blockObj = &blockObj;
        this->let.overflow = overflow;
    }

    void initVarOrConst(JSOp op) {
        this->op = op;
        this->binder = BindVarOrConst;
    }
};

static bool
BindLocalVariable(JSContext *cx, TreeContext *tc, ParseNode *pn, BindingKind kind)
{
    JS_ASSERT(kind == VARIABLE || kind == CONSTANT);

    /* 'arguments' can be bound as a local only via a destructuring formal parameter. */
    JS_ASSERT_IF(pn->pn_atom == cx->runtime->atomState.argumentsAtom, kind == VARIABLE);

    uintN index = tc->bindings.countVars();
    if (!tc->bindings.add(cx, pn->pn_atom, kind))
        return false;

    pn->pn_cookie.set(tc->staticLevel, index);
    pn->pn_dflags |= PND_BOUND;
    return true;
}

#if JS_HAS_DESTRUCTURING
static JSBool
BindDestructuringArg(JSContext *cx, BindData *data, JSAtom *atom, TreeContext *tc)
{
    /* Flag tc so we don't have to lookup arguments on every use. */
    if (atom == tc->parser->context->runtime->atomState.argumentsAtom)
        tc->flags |= TCF_FUN_PARAM_ARGUMENTS;

    JS_ASSERT(tc->inFunction());

    /*
     * NB: Check tc->decls rather than tc->bindings, because destructuring
     *     bindings aren't added to tc->bindings until after all arguments have
     *     been parsed.
     */
    if (tc->decls.lookupFirst(atom)) {
        ReportCompileErrorNumber(cx, TS(tc->parser), NULL, JSREPORT_ERROR,
                                 JSMSG_DESTRUCT_DUP_ARG);
        return JS_FALSE;
    }

    ParseNode *pn = data->pn;

    /*
     * Distinguish destructured-to binding nodes as vars, not args, by setting
     * pn_op to JSOP_SETLOCAL. Parser::functionDef checks for this pn_op value
     * when processing the destructuring-assignment AST prelude induced by such
     * destructuring args in Parser::functionArguments.
     *
     * We must set the PND_BOUND flag too to prevent pn_op from being reset to
     * JSOP_SETNAME by BindDestructuringVar. The only field not initialized is
     * pn_cookie; it gets set in functionDef in the first "if (prelude)" block.
     * We have to wait to set the cookie until we can call JSFunction::addLocal
     * with kind = JSLOCAL_VAR, after all JSLOCAL_ARG locals have been added.
     *
     * Thus a destructuring formal parameter binds an ARG (as in arguments[i]
     * element) with a null atom name for the object or array passed in to be
     * destructured, and zero or more VARs (as in named local variables) for
     * the destructured-to identifiers in the property value positions within
     * the object or array destructuring pattern, and all ARGs for the formal
     * parameter list bound as locals before any VAR for a destructured name.
     */
    pn->setOp(JSOP_SETLOCAL);
    pn->pn_dflags |= PND_BOUND;

    return Define(pn, atom, tc);
}
#endif /* JS_HAS_DESTRUCTURING */

JSFunction *
Parser::newFunction(TreeContext *tc, JSAtom *atom, FunctionSyntaxKind kind)
{
    JS_ASSERT_IF(kind == Statement, atom != NULL);

    /*
     * Find the global compilation context in order to pre-set the newborn
     * function's parent slot to tc->scopeChain. If the global context is a
     * compile-and-go one, we leave the pre-set parent intact; otherwise we
     * clear parent and proto.
     */
    while (tc->parent)
        tc = tc->parent;

    RootedVarObject parent(context);
    parent = tc->inFunction() ? NULL : tc->scopeChain();

    JSFunction *fun =
        js_NewFunction(context, NULL, NULL, 0,
                       JSFUN_INTERPRETED | (kind == Expression ? JSFUN_LAMBDA : 0),
                       parent, atom);
    if (fun && !tc->compileAndGo()) {
        if (!fun->clearParent(context))
            return NULL;
        if (!fun->clearType(context))
            return NULL;
        fun->setEnvironment(NULL);
    }
    return fun;
}

static JSBool
MatchOrInsertSemicolon(JSContext *cx, TokenStream *ts)
{
    TokenKind tt = ts->peekTokenSameLine(TSF_OPERAND);
    if (tt == TOK_ERROR)
        return JS_FALSE;
    if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
        /* Advance the scanner for proper error location reporting. */
        ts->getToken(TSF_OPERAND);
        ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR, JSMSG_SEMI_BEFORE_STMNT);
        return JS_FALSE;
    }
    (void) ts->matchToken(TOK_SEMI);
    return JS_TRUE;
}

static FunctionBox *
EnterFunction(ParseNode *fn, TreeContext *funtc, JSAtom *funAtom = NULL,
              FunctionSyntaxKind kind = Expression)
{
    TreeContext *tc = funtc->parent;
    JSFunction *fun = tc->parser->newFunction(tc, funAtom, kind);
    if (!fun)
        return NULL;

    /* Create box for fun->object early to protect against last-ditch GC. */
    FunctionBox *funbox = tc->parser->newFunctionBox(fun, fn, tc);
    if (!funbox)
        return NULL;

    /* Initialize non-default members of funtc. */
    funtc->flags |= funbox->tcflags;
    funtc->blockidGen = tc->blockidGen;
    if (!GenerateBlockId(funtc, funtc->bodyid))
        return NULL;
    funtc->setFunction(fun);
    funtc->funbox = funbox;
    if (!SetStaticLevel(funtc, tc->staticLevel + 1))
        return NULL;

    return funbox;
}

static bool
DeoptimizeUsesWithin(Definition *dn, const TokenPos &pos)
{
    uintN ndeoptimized = 0;

    for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
        JS_ASSERT(pnu->isUsed());
        JS_ASSERT(!pnu->isDefn());
        if (pnu->pn_pos.begin >= pos.begin && pnu->pn_pos.end <= pos.end) {
            pnu->pn_dflags |= PND_DEOPTIMIZED;
            ++ndeoptimized;
        }
    }

    return ndeoptimized != 0;
}

static bool
LeaveFunction(ParseNode *fn, TreeContext *funtc, PropertyName *funName = NULL,
              FunctionSyntaxKind kind = Expression)
{
    TreeContext *tc = funtc->parent;
    tc->blockidGen = funtc->blockidGen;

    FunctionBox *funbox = fn->pn_funbox;
    funbox->tcflags |= funtc->flags & (TCF_FUN_FLAGS | TCF_COMPILE_N_GO | TCF_RETURN_EXPR);

    fn->pn_dflags |= PND_INITIALIZED;
    if (!tc->topStmt || tc->topStmt->type == STMT_BLOCK)
        fn->pn_dflags |= PND_BLOCKCHILD;

    /*
     * Propagate unresolved lexical names up to tc->lexdeps, and save a copy
     * of funtc->lexdeps in a TOK_UPVARS node wrapping the function's formal
     * params and body. We do this only if there are lexical dependencies not
     * satisfied by the function's declarations, to avoid penalizing functions
     * that use only their arguments and other local bindings.
     */
    if (funtc->lexdeps->count()) {
        int foundCallee = 0;

        for (AtomDefnRange r = funtc->lexdeps->all(); !r.empty(); r.popFront()) {
            JSAtom *atom = r.front().key();
            Definition *dn = r.front().value();
            JS_ASSERT(dn->isPlaceholder());

            if (atom == funName && kind == Expression) {
                dn->setOp(JSOP_CALLEE);
                dn->pn_cookie.set(funtc->staticLevel, UpvarCookie::CALLEE_SLOT);
                dn->pn_dflags |= PND_BOUND;

                /*
                 * If this named function expression uses its own name other
                 * than to call itself, flag this function specially.
                 */
                if (dn->isFunArg())
                    funbox->tcflags |= TCF_FUN_USES_OWN_NAME;
                foundCallee = 1;
                continue;
            }

            if (!(funbox->tcflags & TCF_FUN_SETS_OUTER_NAME) &&
                dn->isAssigned()) {
                /*
                 * Make sure we do not fail to set TCF_FUN_SETS_OUTER_NAME if
                 * any use of dn in funtc assigns. See NoteLValue for the easy
                 * backward-reference case; this is the hard forward-reference
                 * case where we pay a higher price.
                 */
                for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
                    if (pnu->isAssigned() && pnu->pn_blockid >= funtc->bodyid) {
                        funbox->tcflags |= TCF_FUN_SETS_OUTER_NAME;
                        break;
                    }
                }
            }

            Definition *outer_dn = tc->decls.lookupFirst(atom);

            /*
             * Make sure to deoptimize lexical dependencies that are polluted
             * by eval or with, to safely bind globals (see bug 561923).
             */
            if (funtc->callsEval() ||
                (outer_dn && tc->innermostWith &&
                 outer_dn->pn_pos < tc->innermostWith->pn_pos)) {
                DeoptimizeUsesWithin(dn, fn->pn_pos);
            }

            if (!outer_dn) {
                AtomDefnAddPtr p = tc->lexdeps->lookupForAdd(atom);
                if (p) {
                    outer_dn = p.value();
                } else {
                    /*
                     * Create a new placeholder for our outer lexdep. We could
                     * simply re-use the inner placeholder, but that introduces
                     * subtleties in the case where we find a later definition
                     * that captures an existing lexdep. For example:
                     *
                     *   function f() { function g() { x; } let x; }
                     *
                     * Here, g's TOK_UPVARS node lists the placeholder for x,
                     * which must be captured by the 'let' declaration later,
                     * since 'let's are hoisted.  Taking g's placeholder as our
                     * own would work fine. But consider:
                     *
                     *   function f() { x; { function g() { x; } let x; } }
                     *
                     * Here, the 'let' must not capture all the uses of f's
                     * lexdep entry for x, but it must capture the x node
                     * referred to from g's TOK_UPVARS node.  Always turning
                     * inherited lexdeps into uses of a new outer definition
                     * allows us to handle both these cases in a natural way.
                     */
                    outer_dn = MakePlaceholder(dn, tc);
                    if (!outer_dn || !tc->lexdeps->add(p, atom, outer_dn))
                        return false;
                }
            }

            /*
             * Insert dn's uses list at the front of outer_dn's list.
             *
             * Without loss of generality or correctness, we allow a dn to
             * be in inner and outer lexdeps, since the purpose of lexdeps
             * is one-pass coordination of name use and definition across
             * functions, and if different dn's are used we'll merge lists
             * when leaving the inner function.
             *
             * The dn == outer_dn case arises with generator expressions
             * (see CompExprTransplanter::transplant, the PN_FUNC/PN_NAME
             * case), and nowhere else, currently.
             */
            if (dn != outer_dn) {
                ParseNode **pnup = &dn->dn_uses;
                ParseNode *pnu;

                while ((pnu = *pnup) != NULL) {
                    pnu->pn_lexdef = outer_dn;
                    pnup = &pnu->pn_link;
                }

                /*
                 * Make dn be a use that redirects to outer_dn, because we
                 * can't replace dn with outer_dn in all the pn_namesets in
                 * the AST where it may be. Instead we make it forward to
                 * outer_dn. See Definition::resolve.
                 */
                *pnup = outer_dn->dn_uses;
                outer_dn->dn_uses = dn;
                outer_dn->pn_dflags |= dn->pn_dflags & ~PND_PLACEHOLDER;
                dn->setDefn(false);
                dn->setUsed(true);
                dn->pn_lexdef = outer_dn;
            }

            /* Mark the outer dn as escaping. */
            outer_dn->pn_dflags |= PND_CLOSED;
        }

        if (funtc->lexdeps->count() - foundCallee != 0) {
            ParseNode *body = fn->pn_body;

            fn->pn_body = NameSetNode::create(PNK_UPVARS, tc);
            if (!fn->pn_body)
                return false;

            fn->pn_body->pn_pos = body->pn_pos;
            if (foundCallee)
                funtc->lexdeps->remove(funName);
            /* Transfer ownership of the lexdep map to the parse node. */
            fn->pn_body->pn_names = funtc->lexdeps;
            funtc->lexdeps.clearMap();
            fn->pn_body->pn_tree = body;
        } else {
            funtc->lexdeps.releaseMap(funtc->parser->context);
        }

    }

    /*
     * Check whether any parameters have been assigned within this function.
     * In strict mode parameters do not alias arguments[i], and to make the
     * arguments object reflect initial parameter values prior to any mutation
     * we create it eagerly whenever parameters are (or might, in the case of
     * calls to eval) be assigned.
     */
    if (funtc->inStrictMode() && funbox->object->toFunction()->nargs > 0) {
        AtomDeclsIter iter(&funtc->decls);
        Definition *dn;

        while ((dn = iter()) != NULL) {
            if (dn->kind() == Definition::ARG && dn->isAssigned()) {
                funbox->tcflags |= TCF_FUN_MUTATES_PARAMETER;
                break;
            }
        }
    }

    funbox->bindings.transfer(funtc->parser->context, &funtc->bindings);

    return true;
}

static bool
DefineGlobal(ParseNode *pn, BytecodeEmitter *bce, PropertyName *name);

/*
 * FIXME? this Parser method was factored from Parser::functionDef with minimal
 * change, hence the funtc ref param and funbox. It probably should match
 * functionBody, etc., and use tc and tc->funbox instead of taking explicit
 * parameters.
 */
bool
Parser::functionArguments(TreeContext &funtc, FunctionBox *funbox, ParseNode **listp)
{
    if (tokenStream.getToken() != TOK_LP) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_PAREN_BEFORE_FORMAL);
        return false;
    }

    if (!tokenStream.matchToken(TOK_RP)) {
#if JS_HAS_DESTRUCTURING
        JSAtom *duplicatedArg = NULL;
        bool destructuringArg = false;
        ParseNode *list = NULL;
#endif

        do {
            switch (TokenKind tt = tokenStream.getToken()) {
#if JS_HAS_DESTRUCTURING
              case TOK_LB:
              case TOK_LC:
              {
                /* See comment below in the TOK_NAME case. */
                if (duplicatedArg)
                    goto report_dup_and_destructuring;
                destructuringArg = true;

                /*
                 * A destructuring formal parameter turns into one or more
                 * local variables initialized from properties of a single
                 * anonymous positional parameter, so here we must tweak our
                 * binder and its data.
                 */
                BindData data;
                data.pn = NULL;
                data.op = JSOP_DEFVAR;
                data.binder = BindDestructuringArg;
                ParseNode *lhs = destructuringExpr(&data, tt);
                if (!lhs)
                    return false;

                /*
                 * Adjust fun->nargs to count the single anonymous positional
                 * parameter that is to be destructured.
                 */
                uint16_t slot;
                if (!funtc.bindings.addDestructuring(context, &slot))
                    return false;

                /*
                 * Synthesize a destructuring assignment from the single
                 * anonymous positional parameter into the destructuring
                 * left-hand-side expression and accumulate it in list.
                 */
                ParseNode *rhs =
                    NameNode::create(PNK_NAME, context->runtime->atomState.emptyAtom, &funtc);
                if (!rhs)
                    return false;
                rhs->setOp(JSOP_GETARG);
                rhs->pn_cookie.set(funtc.staticLevel, slot);
                rhs->pn_dflags |= PND_BOUND;

                ParseNode *item =
                    ParseNode::newBinaryOrAppend(PNK_ASSIGN, JSOP_NOP, lhs, rhs, &funtc);
                if (!item)
                    return false;
                if (!list) {
                    list = ListNode::create(PNK_VAR, &funtc);
                    if (!list)
                        return false;
                    list->makeEmpty();
                    *listp = list;
                }
                list->append(item);
                break;
              }
#endif /* JS_HAS_DESTRUCTURING */

              case TOK_NAME:
              {
                PropertyName *name = tokenStream.currentToken().name();

#ifdef JS_HAS_DESTRUCTURING
                /*
                 * ECMA-262 requires us to support duplicate parameter names,
                 * but if the parameter list includes destructuring, we
                 * consider the code to have "opted in" to higher standards and
                 * forbid duplicates. We may see a destructuring parameter
                 * later, so always note duplicates now.
                 *
                 * Duplicates are warned about (strict option) or cause errors
                 * (strict mode code), but we do those tests in one place
                 * below, after having parsed the body in case it begins with a
                 * "use strict"; directive.
                 *
                 * NB: Check funtc.decls rather than funtc.bindings, because
                 *     destructuring bindings aren't added to funtc.bindings
                 *     until after all arguments have been parsed.
                 */
                if (funtc.decls.lookupFirst(name)) {
                    duplicatedArg = name;
                    if (destructuringArg)
                        goto report_dup_and_destructuring;
                }
#endif

                uint16_t slot;
                if (!funtc.bindings.addArgument(context, name, &slot))
                    return false;
                if (!DefineArg(funbox->node, name, slot, &funtc))
                    return false;
                break;
              }

              default:
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_MISSING_FORMAL);
                /* FALL THROUGH */
              case TOK_ERROR:
                return false;

#if JS_HAS_DESTRUCTURING
              report_dup_and_destructuring:
                Definition *dn = funtc.decls.lookupFirst(duplicatedArg);
                reportErrorNumber(dn, JSREPORT_ERROR, JSMSG_DESTRUCT_DUP_ARG);
                return false;
#endif
            }
        } while (tokenStream.matchToken(TOK_COMMA));

        if (tokenStream.getToken() != TOK_RP) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_PAREN_AFTER_FORMAL);
            return false;
        }
    }

    return true;
}

ParseNode *
Parser::functionDef(PropertyName *funName, FunctionType type, FunctionSyntaxKind kind)
{
    JS_ASSERT_IF(kind == Statement, funName);

    /* Make a TOK_FUNCTION node. */
    ParseNode *pn = FunctionNode::create(PNK_FUNCTION, tc);
    if (!pn)
        return NULL;
    pn->pn_body = NULL;
    pn->pn_cookie.makeFree();

    /*
     * If this is a function expression, mark this function as escaping (as a
     * "funarg") unless it is immediately applied (we clear PND_FUNARG if so --
     * see memberExpr).
     *
     * Treat function sub-statements (those not at body level of a function or
     * program) as escaping funargs, since we can't statically analyze their
     * definitions and uses.
     */
    bool bodyLevel = tc->atBodyLevel();
    pn->pn_dflags = (kind == Expression || !bodyLevel) ? PND_FUNARG : 0;

    /*
     * Record names for function statements in tc->decls so we know when to
     * avoid optimizing variable references that might name a function.
     */
    if (kind == Statement) {
        if (Definition *dn = tc->decls.lookupFirst(funName)) {
            Definition::Kind dn_kind = dn->kind();

            JS_ASSERT(!dn->isUsed());
            JS_ASSERT(dn->isDefn());

            if (context->hasStrictOption() || dn_kind == Definition::CONST) {
                JSAutoByteString name;
                if (!js_AtomToPrintableString(context, funName, &name) ||
                    !reportErrorNumber(NULL,
                                       (dn_kind != Definition::CONST)
                                       ? JSREPORT_WARNING | JSREPORT_STRICT
                                       : JSREPORT_ERROR,
                                       JSMSG_REDECLARED_VAR,
                                       Definition::kindString(dn_kind),
                                       name.ptr())) {
                    return NULL;
                }
            }

            if (bodyLevel) {
                tc->decls.updateFirst(funName, (Definition *) pn);
                pn->setDefn(true);
                pn->dn_uses = dn; /* dn->dn_uses is now pn_link */

                if (!MakeDefIntoUse(dn, pn, funName, tc))
                    return NULL;
            }
        } else if (bodyLevel) {
            /*
             * If this function was used before it was defined, claim the
             * pre-created definition node for this function that primaryExpr
             * put in tc->lexdeps on first forward reference, and recycle pn.
             */

            if (Definition *fn = tc->lexdeps.lookupDefn(funName)) {
                JS_ASSERT(fn->isDefn());
                fn->setKind(PNK_FUNCTION);
                fn->setArity(PN_FUNC);
                fn->pn_pos.begin = pn->pn_pos.begin;

                /*
                 * Set fn->pn_pos.end too, in case of error before we parse the
                 * closing brace.  See bug 640075.
                 */
                fn->pn_pos.end = pn->pn_pos.end;

                fn->pn_body = NULL;
                fn->pn_cookie.makeFree();

                tc->lexdeps->remove(funName);
                freeTree(pn);
                pn = fn;
            }

            if (!Define(pn, funName, tc))
                return NULL;
        }

        /*
         * A function directly inside another's body needs only a local
         * variable to bind its name to its value, and not an activation object
         * property (it might also need the activation property, if the outer
         * function contains with statements, e.g., but the stack slot wins
         * when BytecodeEmitter.cpp's BindNameToSlot can optimize a JSOP_NAME
         * into a JSOP_GETLOCAL bytecode).
         */
        if (bodyLevel && tc->inFunction()) {
            /*
             * Define a local in the outer function so that BindNameToSlot
             * can properly optimize accesses. Note that we need a local
             * variable, not an argument, for the function statement. Thus
             * we add a variable even if a parameter with the given name
             * already exists.
             */
            uintN index;
            switch (tc->bindings.lookup(context, funName, &index)) {
              case NONE:
              case ARGUMENT:
                index = tc->bindings.countVars();
                if (!tc->bindings.addVariable(context, funName))
                    return NULL;
                /* FALL THROUGH */

              case VARIABLE:
                pn->pn_cookie.set(tc->staticLevel, index);
                pn->pn_dflags |= PND_BOUND;
                break;

              default:;
            }
        }
    }

    TreeContext *outertc = tc;

    /* Initialize early for possible flags mutation via destructuringExpr. */
    TreeContext funtc(tc->parser);
    if (!funtc.init(context))
        return NULL;

    FunctionBox *funbox = EnterFunction(pn, &funtc, funName, kind);
    if (!funbox)
        return NULL;

    JSFunction *fun = funbox->function();

    /* Now parse formal argument list and compute fun->nargs. */
    ParseNode *prelude = NULL;
    if (!functionArguments(funtc, funbox, &prelude))
        return NULL;

    fun->setArgCount(funtc.bindings.countArgs());

#if JS_HAS_DESTRUCTURING
    /*
     * If there were destructuring formal parameters, bind the destructured-to
     * local variables now that we've parsed all the regular and destructuring
     * formal parameters. Because js::Bindings::add must be called first for
     * all ARGUMENTs, then all VARIABLEs and CONSTANTs, and finally all UPVARs,
     * we can't bind vars induced by formal parameter destructuring until after
     * Parser::functionArguments has returned.
     */
    if (prelude) {
        AtomDeclsIter iter(&funtc.decls);

        while (Definition *apn = iter()) {
            /* Filter based on pn_op -- see BindDestructuringArg, above. */
            if (!apn->isOp(JSOP_SETLOCAL))
                continue;

            if (!BindLocalVariable(context, &funtc, apn, VARIABLE))
                return NULL;
        }
    }
#endif

    if (type == Getter && fun->nargs > 0) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_ACCESSOR_WRONG_ARGS,
                          "getter", "no", "s");
        return NULL;
    }
    if (type == Setter && fun->nargs != 1) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_ACCESSOR_WRONG_ARGS,
                          "setter", "one", "");
        return NULL;
    }

    FunctionBodyType bodyType = StatementListBody;
#if JS_HAS_EXPR_CLOSURES
    if (tokenStream.getToken(TSF_OPERAND) != TOK_LC) {
        tokenStream.ungetToken();
        fun->flags |= JSFUN_EXPR_CLOSURE;
        bodyType = ExpressionBody;
    }
#else
    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_BODY);
#endif

    ParseNode *body = functionBody(bodyType);
    if (!body)
        return NULL;

    if (funName && !CheckStrictBinding(context, &funtc, funName, pn))
        return NULL;

    if (!CheckStrictParameters(context, &funtc))
        return NULL;

#if JS_HAS_EXPR_CLOSURES
    if (bodyType == StatementListBody)
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_BODY);
    else if (kind == Statement && !MatchOrInsertSemicolon(context, &tokenStream))
        return NULL;
#else
    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_BODY);
#endif
    pn->pn_pos.end = tokenStream.currentToken().pos.end;

    /*
     * Fruit of the poisonous tree: if a closure calls eval, we consider the
     * parent to call eval. We need this for two reasons: (1) the Jaegermonkey
     * optimizations really need to know if eval is called transitively, and
     * (2) in strict mode, eval called transitively requires eager argument
     * creation in strict mode parent functions.
     *
     * For the latter, we really only need to propagate callsEval if both
     * functions are strict mode, but we don't lose much by always propagating.
     * The only optimization we lose this way is in the case where a function
     * is strict, does not mutate arguments, does not call eval directly, but
     * calls eval transitively.
     */
    if (funtc.callsEval())
        outertc->noteCallsEval();

#if JS_HAS_DESTRUCTURING
    /*
     * If there were destructuring formal parameters, prepend the initializing
     * comma expression that we synthesized to body. If the body is a return
     * node, we must make a special PNK_SEQ node, to prepend the destructuring
     * code without bracing the decompilation of the function body.
     */
    if (prelude) {
        if (!body->isArity(PN_LIST)) {
            ParseNode *block;

            block = ListNode::create(PNK_SEQ, outertc);
            if (!block)
                return NULL;
            block->pn_pos = body->pn_pos;
            block->initList(body);

            body = block;
        }

        ParseNode *item = UnaryNode::create(PNK_SEMI, outertc);
        if (!item)
            return NULL;

        item->pn_pos.begin = item->pn_pos.end = body->pn_pos.begin;
        item->pn_kid = prelude;
        item->pn_next = body->pn_head;
        body->pn_head = item;
        if (body->pn_tail == &body->pn_head)
            body->pn_tail = &item->pn_next;
        ++body->pn_count;
        body->pn_xflags |= PNX_DESTRUCT;
    }
#endif

    /*
     * If we collected flags that indicate nested heavyweight functions, or
     * this function contains heavyweight-making statements (with statement,
     * visible eval call, or assignment to 'arguments'), flag the function as
     * heavyweight (requiring a call object per invocation).
     */
    if (funtc.flags & TCF_FUN_HEAVYWEIGHT) {
        fun->flags |= JSFUN_HEAVYWEIGHT;
        outertc->flags |= TCF_FUN_HEAVYWEIGHT;
    } else {
        /*
         * If this function is not at body level of a program or function (i.e.
         * it is a function statement that is not a direct child of a program
         * or function), then our enclosing function, if any, must be
         * heavyweight.
         */
        if (!bodyLevel && kind == Statement)
            outertc->flags |= TCF_FUN_HEAVYWEIGHT;
    }

    JSOp op = JSOP_NOP;
    if (kind == Expression) {
        op = JSOP_LAMBDA;
    } else {
        if (!bodyLevel) {
            /*
             * Extension: in non-strict mode code, a function statement not at
             * the top level of a function body or whole program, e.g., in a
             * compound statement such as the "then" part of an "if" statement,
             * binds a closure only if control reaches that sub-statement.
             */
            JS_ASSERT(!outertc->inStrictMode());
            op = JSOP_DEFFUN;
            outertc->noteMightAliasLocals();
        }
    }

    funbox->kids = funtc.functionList;

    pn->pn_funbox = funbox;
    pn->setOp(op);
    if (pn->pn_body) {
        pn->pn_body->append(body);
        pn->pn_body->pn_pos = body->pn_pos;
    } else {
        pn->pn_body = body;
    }

    if (!outertc->inFunction() && bodyLevel && kind == Statement && outertc->compiling()) {
        JS_ASSERT(pn->pn_cookie.isFree());
        if (!DefineGlobal(pn, outertc->asBytecodeEmitter(), funName))
            return NULL;
    }

    pn->pn_blockid = outertc->blockid();

    if (!LeaveFunction(pn, &funtc, funName, kind))
        return NULL;

    /* If the surrounding function is not strict code, reset the lexer. */
    if (!outertc->inStrictMode())
        tokenStream.setStrictMode(false);

    return pn;
}

ParseNode *
Parser::functionStmt()
{
    PropertyName *name = NULL;
    if (tokenStream.getToken(TSF_KEYWORD_IS_NAME) == TOK_NAME) {
        name = tokenStream.currentToken().name();
    } else {
        /* Unnamed function expressions are forbidden in statement context. */
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_UNNAMED_FUNCTION_STMT);
        return NULL;
    }

    /* We forbid function statements in strict mode code. */
    if (!tc->atBodyLevel() && tc->inStrictMode()) {
        reportErrorNumber(NULL, JSREPORT_STRICT_MODE_ERROR, JSMSG_STRICT_FUNCTION_STATEMENT);
        return NULL;
    }

    return functionDef(name, Normal, Statement);
}

ParseNode *
Parser::functionExpr()
{
    PropertyName *name = NULL;
    if (tokenStream.getToken(TSF_KEYWORD_IS_NAME) == TOK_NAME)
        name = tokenStream.currentToken().name();
    else
        tokenStream.ungetToken();
    return functionDef(name, Normal, Expression);
}

/*
 * Recognize Directive Prologue members and directives. Assuming |pn| is a
 * candidate for membership in a directive prologue, recognize directives and
 * set |tc|'s flags accordingly. If |pn| is indeed part of a prologue, set its
 * |pn_prologue| flag.
 *
 * Note that the following is a strict mode function:
 *
 * function foo() {
 *   "blah" // inserted semi colon
 *        "blurgh"
 *   "use\x20loose"
 *   "use strict"
 * }
 *
 * That is, even though "use\x20loose" can never be a directive, now or in the
 * future (because of the hex escape), the Directive Prologue extends through it
 * to the "use strict" statement, which is indeed a directive.
 */
bool
Parser::recognizeDirectivePrologue(ParseNode *pn, bool *isDirectivePrologueMember)
{
    *isDirectivePrologueMember = pn->isStringExprStatement();
    if (!*isDirectivePrologueMember)
        return true;

    ParseNode *kid = pn->pn_kid;
    if (kid->isEscapeFreeStringLiteral()) {
        /*
         * Mark this statement as being a possibly legitimate part of a
         * directive prologue, so the byte code emitter won't warn about it
         * being useless code. (We mustn't just omit the statement entirely yet,
         * as it could be producing the value of an eval or JSScript execution.)
         *
         * Note that even if the string isn't one we recognize as a directive,
         * the emitter still shouldn't flag it as useless, as it could become a
         * directive in the future. We don't want to interfere with people
         * taking advantage of directive-prologue-enabled features that appear
         * in other browsers first.
         */
        pn->pn_prologue = true;

        JSAtom *directive = kid->pn_atom;
        if (directive == context->runtime->atomState.useStrictAtom) {
            /*
             * Unfortunately, Directive Prologue members in general may contain
             * escapes, even while "use strict" directives may not.  Therefore
             * we must check whether an octal character escape has been seen in
             * any previous directives whenever we encounter a "use strict"
             * directive, so that the octal escape is properly treated as a
             * syntax error.  An example of this case:
             *
             *   function error()
             *   {
             *     "\145"; // octal escape
             *     "use strict"; // retroactively makes "\145" a syntax error
             *   }
             */
            if (tokenStream.hasOctalCharacterEscape()) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_DEPRECATED_OCTAL);
                return false;
            }

            tc->flags |= TCF_STRICT_MODE_CODE;
            tokenStream.setStrictMode();
        }
    }
    return true;
}

/*
 * Parse the statements in a block, creating a TOK_LC node that lists the
 * statements' trees.  If called from block-parsing code, the caller must
 * match { before and } after.
 */
ParseNode *
Parser::statements()
{
    JS_CHECK_RECURSION(context, return NULL);

    ParseNode *pn = ListNode::create(PNK_STATEMENTLIST, tc);
    if (!pn)
        return NULL;
    pn->makeEmpty();
    pn->pn_blockid = tc->blockid();
    ParseNode *saveBlock = tc->blockNode;
    tc->blockNode = pn;

    bool inDirectivePrologue = tc->atBodyLevel();
    tokenStream.setOctalCharacterEscape(false);
    for (;;) {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt <= TOK_EOF || tt == TOK_RC) {
            if (tt == TOK_ERROR) {
                if (tokenStream.isEOF())
                    tokenStream.setUnexpectedEOF();
                return NULL;
            }
            break;
        }
        ParseNode *next = statement();
        if (!next) {
            if (tokenStream.isEOF())
                tokenStream.setUnexpectedEOF();
            return NULL;
        }

        if (inDirectivePrologue && !recognizeDirectivePrologue(next, &inDirectivePrologue))
            return NULL;

        if (next->isKind(PNK_FUNCTION)) {
            /*
             * PNX_FUNCDEFS notifies the emitter that the block contains body-
             * level function definitions that should be processed before the
             * rest of nodes.
             *
             * TCF_HAS_FUNCTION_STMT is for the TOK_LC case in Statement. It
             * is relevant only for function definitions not at body-level,
             * which we call function statements.
             */
            if (tc->atBodyLevel()) {
                pn->pn_xflags |= PNX_FUNCDEFS;
            } else {
                tc->flags |= TCF_HAS_FUNCTION_STMT;
                /* Function statements extend the Call object at runtime. */
                tc->noteHasExtensibleScope();
            }
        }
        pn->append(next);
    }

    /*
     * Handle the case where there was a let declaration under this block.  If
     * it replaced tc->blockNode with a new block node then we must refresh pn
     * and then restore tc->blockNode.
     */
    if (tc->blockNode != pn)
        pn = tc->blockNode;
    tc->blockNode = saveBlock;

    pn->pn_pos.end = tokenStream.currentToken().pos.end;
    return pn;
}

ParseNode *
Parser::condition()
{
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    ParseNode *pn = parenExpr();
    if (!pn)
        return NULL;
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    JS_ASSERT_IF(pn->isKind(PNK_ASSIGN), pn->isOp(JSOP_NOP));
    if (pn->isKind(PNK_ASSIGN) &&
        !pn->isInParens() &&
        !reportErrorNumber(NULL, JSREPORT_WARNING | JSREPORT_STRICT, JSMSG_EQUAL_AS_ASSIGN))
    {
        return NULL;
    }
    return pn;
}

static bool
MatchLabel(JSContext *cx, TokenStream *ts, PropertyName **label)
{
    TokenKind tt = ts->peekTokenSameLine(TSF_OPERAND);
    if (tt == TOK_ERROR)
        return false;
    if (tt == TOK_NAME) {
        (void) ts->getToken();
        *label = ts->currentToken().name();
    } else {
        *label = NULL;
    }
    return true;
}

static bool
ReportRedeclaration(JSContext *cx, TreeContext *tc, ParseNode *pn, bool isConst, JSAtom *atom)
{
    JSAutoByteString name;
    if (js_AtomToPrintableString(cx, atom, &name)) {
        ReportCompileErrorNumber(cx, TS(tc->parser), pn,
                                 JSREPORT_ERROR, JSMSG_REDECLARED_VAR,
                                 isConst ? "const" : "variable",
                                 name.ptr());
    }
    return false;
}

/*
 * Define a let-variable in a block, let-expression, or comprehension scope. tc
 * must already be in such a scope.
 *
 * Throw a SyntaxError if 'atom' is an invalid name. Otherwise create a
 * property for the new variable on the block object, tc->blockChain;
 * populate data->pn->pn_{op,cookie,defn,dflags}; and stash a pointer to
 * data->pn in a slot of the block object.
 */
static JSBool
BindLet(JSContext *cx, BindData *data, JSAtom *atom, TreeContext *tc)
{
    ParseNode *pn = data->pn;
    if (!CheckStrictBinding(cx, tc, atom->asPropertyName(), pn))
        return false;

    StaticBlockObject &blockObj = *data->let.blockObj;
    uintN blockCount = blockObj.slotCount();
    if (blockCount == JS_BIT(16)) {
        ReportCompileErrorNumber(cx, TS(tc->parser), pn,
                                 JSREPORT_ERROR, data->let.overflow);
        return false;
    }

    /*
     * For bindings that are hoisted to the beginning of the block/function,
     * Define() right now. For the rest, delay Define() until PushLetScope.
     */
    if (data->let.varContext == HoistVars) {
        JS_ASSERT(!tc->atBodyLevel());
        Definition *dn = tc->decls.lookupFirst(atom);
        if (dn && dn->pn_blockid == tc->blockid())
            return ReportRedeclaration(cx, tc, pn, dn->isConst(), atom);
        if (!Define(pn, atom, tc, true))
            return false;
    }

    /*
     * Assign block-local index to pn->pn_cookie right away, encoding it as an
     * upvar cookie whose skip tells the current static level. The emitter will
     * adjust the node's slot based on its stack depth model -- and, for global
     * and eval code, js::frontend::CompileScript will adjust the slot
     * again to include script->nfixed.
     */
    pn->setOp(JSOP_GETLOCAL);
    pn->pn_cookie.set(tc->staticLevel, uint16_t(blockCount));
    pn->pn_dflags |= PND_LET | PND_BOUND;

    /*
     * Define the let binding's property before storing pn in the the binding's
     * slot indexed by blockCount off the class-reserved slot base.
     */
    bool redeclared;
    jsid id = ATOM_TO_JSID(atom);
    const Shape *shape = blockObj.addVar(cx, id, blockCount, &redeclared);
    if (!shape) {
        if (redeclared)
            ReportRedeclaration(cx, tc, pn, false, atom);
        return false;
    }

    /* Store pn in the static block object. */
    blockObj.setDefinitionParseNode(blockCount, reinterpret_cast<Definition *>(pn));
    return true;
}

template <class Op>
static inline bool
ForEachLetDef(TreeContext *tc, StaticBlockObject &blockObj, Op op)
{
    for (Shape::Range r = blockObj.lastProperty()->all(); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();

        /* Beware the destructuring dummy slots. */
        if (JSID_IS_INT(shape.propid()))
            continue;

        if (!op(tc, blockObj, shape, JSID_TO_ATOM(shape.propid())))
            return false;
    }
    return true;
}

struct RemoveDecl {
    bool operator()(TreeContext *tc, StaticBlockObject &, const Shape &, JSAtom *atom) {
        tc->decls.remove(atom);
        return true;
    }
};

static void
PopStatement(TreeContext *tc)
{
    if (tc->topStmt->flags & SIF_SCOPE) {
        StaticBlockObject &blockObj = *tc->topStmt->blockObj;
        JS_ASSERT(!blockObj.inDictionaryMode());
        ForEachLetDef(tc, blockObj, RemoveDecl());
    }
    PopStatementTC(tc);
}

static inline bool
OuterLet(TreeContext *tc, StmtInfo *stmt, JSAtom *atom)
{
    while (stmt->downScope) {
        stmt = LexicalLookup(tc, atom, NULL, stmt->downScope);
        if (!stmt)
            return false;
        if (stmt->type == STMT_BLOCK)
            return true;
    }
    return false;
}

/*
 * If we are generating global or eval-called-from-global code, bind a "gvar"
 * here, as soon as possible. The JSOP_GETGVAR, etc., ops speed up interpreted
 * global variable access by memoizing name-to-slot mappings during execution
 * of the script prolog (via JSOP_DEFVAR/JSOP_DEFCONST). If the memoization
 * can't be done due to a pre-existing property of the same name as the var or
 * const but incompatible attributes/getter/setter/etc, these ops devolve to
 * JSOP_NAME, etc.
 *
 * For now, don't try to lookup eval frame variables at compile time. This is
 * sub-optimal: we could handle eval-called-from-global-code gvars since eval
 * gets its own script and frame. The eval-from-function-code case is harder,
 * since functions do not atomize gvars and then reserve their atom indexes as
 * stack frame slots.
 */
static bool
DefineGlobal(ParseNode *pn, BytecodeEmitter *bce, PropertyName *name)
{
    GlobalScope *globalScope = bce->globalScope;
    JSObject *globalObj = globalScope->globalObj;

    if (!bce->compileAndGo() || !globalObj || bce->compilingForEval())
        return true;

    AtomIndexAddPtr p = globalScope->names.lookupForAdd(name);
    if (!p) {
        JSContext *cx = bce->parser->context;

        JSObject *holder;
        JSProperty *prop;
        if (!globalObj->lookupProperty(cx, name, &holder, &prop))
            return false;

        FunctionBox *funbox = pn->isKind(PNK_FUNCTION) ? pn->pn_funbox : NULL;

        GlobalScope::GlobalDef def;
        if (prop) {
            /*
             * A few cases where we don't bother aggressively caching:
             *   1) Function value changes.
             *   2) Configurable properties.
             *   3) Properties without slots, or with getters/setters.
             */
            const Shape *shape = (const Shape *)prop;
            if (funbox ||
                globalObj != holder ||
                shape->configurable() ||
                !shape->hasSlot() ||
                !shape->hasDefaultGetterOrIsMethod() ||
                !shape->hasDefaultSetter()) {
                return true;
            }

            def = GlobalScope::GlobalDef(shape->slot());
        } else {
            def = GlobalScope::GlobalDef(name, funbox);
        }

        if (!globalScope->defs.append(def))
            return false;

        jsatomid index = globalScope->names.count();
        if (!globalScope->names.add(p, name, index))
            return false;

        JS_ASSERT(index == globalScope->defs.length() - 1);
    } else {
        /*
         * Functions can be redeclared, and the last one takes effect. Check
         * for this and make sure to rewrite the definition.
         *
         * Note: This could overwrite an existing variable declaration, for
         * example:
         *   var c = []
         *   function c() { }
         *
         * This rewrite is allowed because the function will be statically
         * hoisted to the top of the script, and the |c = []| will just
         * overwrite it at runtime.
         */
        if (pn->isKind(PNK_FUNCTION)) {
            JS_ASSERT(pn->isArity(PN_FUNC));
            jsatomid index = p.value();
            globalScope->defs[index].funbox = pn->pn_funbox;
        }
    }

    pn->pn_dflags |= PND_GVAR;

    return true;
}

static bool
BindTopLevelVar(JSContext *cx, BindData *data, ParseNode *pn, TreeContext *tc)
{
    JS_ASSERT(pn->isOp(JSOP_NAME));
    JS_ASSERT(!tc->inFunction());

    /* There's no need to optimize bindings if we're not compiling code. */
    if (!tc->compiling())
        return true;

    /*
     * Bindings at top level in eval code aren't like bindings at top level in
     * regular code, and we must handle them specially.
     */
    if (tc->parser->callerFrame) {
        /*
         * If the eval code is not strict mode code, such bindings are created
         * from scratch in the the caller's environment (if the eval is direct)
         * or in the global environment (if the eval is indirect) -- and they
         * can be deleted.  Therefore we can't bind early.
         */
        if (!tc->inStrictMode())
            return true;

        /*
         * But if the eval code is strict mode code, bindings are added to a
         * new environment specifically for that eval code's compilation, and
         * they can't be deleted.  Thus strict mode eval code does not affect
         * the caller's environment, and we can bind such names early.  (But
         * note: strict mode eval code can still affect the global environment
         * by performing an indirect eval of non-strict mode code.)
         *
         * However, optimizing such bindings requires either precarious
         * type-punning or, ideally, a new kind of Call object specifically for
         * strict mode eval frames.  Further, strict mode eval is not (yet)
         * common.  So for now (until we rewrite the scope chain to not use
         * objects?) just disable optimizations for top-level names in eval
         * code.
         */
        return true;
    }

    if (pn->pn_dflags & PND_CONST)
        return true;

    /*
     * If this is a global variable, we're compile-and-go, and a global object
     * is present, try to bake in either an already available slot or a
     * predicted slot that will be defined after compiling is completed.
     */
    return DefineGlobal(pn, tc->asBytecodeEmitter(), pn->pn_atom->asPropertyName());
}

static bool
BindFunctionLocal(JSContext *cx, BindData *data, MultiDeclRange &mdl, TreeContext *tc)
{
    JS_ASSERT(tc->inFunction());

    ParseNode *pn = data->pn;
    JSAtom *name = pn->pn_atom;

    /*
     * Don't create a local variable with the name 'arguments', per ECMA-262.
     * Instead, 'var arguments' always restates that predefined binding of the
     * lexical environment for function activations. Assignments to arguments
     * must be handled specially -- see NoteLValue.
     */
    if (name == cx->runtime->atomState.argumentsAtom) {
        pn->setOp(JSOP_ARGUMENTS);
        pn->pn_dflags |= PND_BOUND;
        return true;
    }

    BindingKind kind = tc->bindings.lookup(cx, name, NULL);
    if (kind == NONE) {
        /*
         * Property not found in current variable scope: we have not seen this
         * variable before, so bind a new local variable for it. Any locals
         * declared in a with statement body are handled at runtime, by script
         * prolog JSOP_DEFVAR opcodes generated for global and heavyweight-
         * function-local vars.
         */
        kind = (data->op == JSOP_DEFCONST) ? CONSTANT : VARIABLE;

        if (!BindLocalVariable(cx, tc, pn, kind))
            return false;
        pn->setOp(JSOP_GETLOCAL);
        return true;
    }

    if (kind == ARGUMENT) {
        JS_ASSERT(tc->inFunction());
        JS_ASSERT(!mdl.empty() && mdl.front()->kind() == Definition::ARG);
    } else {
        JS_ASSERT(kind == VARIABLE || kind == CONSTANT);
    }

    return true;
}

static JSBool
BindVarOrConst(JSContext *cx, BindData *data, JSAtom *atom, TreeContext *tc)
{
    ParseNode *pn = data->pn;

    /* Default best op for pn is JSOP_NAME; we'll try to improve below. */
    pn->setOp(JSOP_NAME);

    if (!CheckStrictBinding(cx, tc, atom->asPropertyName(), pn))
        return false;

    StmtInfo *stmt = LexicalLookup(tc, atom, NULL);

    if (stmt && stmt->type == STMT_WITH) {
        data->fresh = false;
        pn->pn_dflags |= PND_DEOPTIMIZED;
        tc->noteMightAliasLocals();
        return true;
    }

    MultiDeclRange mdl = tc->decls.lookupMulti(atom);
    JSOp op = data->op;

    if (stmt || !mdl.empty()) {
        Definition *dn = mdl.empty() ? NULL : mdl.front();
        Definition::Kind dn_kind = dn ? dn->kind() : Definition::VAR;

        if (dn_kind == Definition::ARG) {
            JSAutoByteString name;
            if (!js_AtomToPrintableString(cx, atom, &name))
                return JS_FALSE;

            if (op == JSOP_DEFCONST) {
                ReportCompileErrorNumber(cx, TS(tc->parser), pn,
                                         JSREPORT_ERROR, JSMSG_REDECLARED_PARAM,
                                         name.ptr());
                return JS_FALSE;
            }
            if (!ReportCompileErrorNumber(cx, TS(tc->parser), pn,
                                          JSREPORT_WARNING | JSREPORT_STRICT,
                                          JSMSG_VAR_HIDES_ARG, name.ptr())) {
                return JS_FALSE;
            }
        } else {
            bool error = (op == JSOP_DEFCONST ||
                          dn_kind == Definition::CONST ||
                          (dn_kind == Definition::LET &&
                           (stmt->type != STMT_CATCH || OuterLet(tc, stmt, atom))));

            if (cx->hasStrictOption()
                ? op != JSOP_DEFVAR || dn_kind != Definition::VAR
                : error) {
                JSAutoByteString name;
                if (!js_AtomToPrintableString(cx, atom, &name) ||
                    !ReportCompileErrorNumber(cx, TS(tc->parser), pn,
                                              !error
                                              ? JSREPORT_WARNING | JSREPORT_STRICT
                                              : JSREPORT_ERROR,
                                              JSMSG_REDECLARED_VAR,
                                              Definition::kindString(dn_kind),
                                              name.ptr())) {
                    return JS_FALSE;
                }
            }
        }
    }

    if (mdl.empty()) {
        if (!Define(pn, atom, tc))
            return JS_FALSE;
    } else {
        /*
         * A var declaration never recreates an existing binding, it restates
         * it and possibly reinitializes its value. Beware that if pn becomes a
         * use of |mdl.defn()|, and if we have an initializer for this var or
         * const (typically a const would ;-), then pn must be rewritten into a
         * PNK_ASSIGN node. See js::Parser::variables, further below.
         *
         * A case such as let (x = 1) { var x = 2; print(x); } is even harder.
         * There the x definition is hoisted but the x = 2 assignment mutates
         * the block-local binding of x.
         */
        Definition *dn = mdl.front();

        data->fresh = false;

        if (!pn->isUsed()) {
            /* Make pnu be a fresh name node that uses dn. */
            ParseNode *pnu = pn;

            if (pn->isDefn()) {
                pnu = NameNode::create(PNK_NAME, atom, tc);
                if (!pnu)
                    return JS_FALSE;
            }

            LinkUseToDef(pnu, dn, tc);
            pnu->setOp(JSOP_NAME);
        }

        /* Find the first non-let binding of this atom. */
        while (dn->kind() == Definition::LET) {
            mdl.popFront();
            if (mdl.empty())
                break;
            dn = mdl.front();
        }

        if (dn) {
            JS_ASSERT_IF(data->op == JSOP_DEFCONST,
                         dn->kind() == Definition::CONST);
            return JS_TRUE;
        }

        /*
         * A var or const that is shadowed by one or more let bindings of the
         * same name, but that has not been declared until this point, must be
         * hoisted above the let bindings.
         */
        if (!pn->isDefn()) {
            if (tc->lexdeps->lookup(atom)) {
                tc->lexdeps->remove(atom);
            } else {
                ParseNode *pn2 = NameNode::create(PNK_NAME, atom, tc);
                if (!pn2)
                    return JS_FALSE;

                /* The token stream may be past the location for pn. */
                pn2->pn_pos = pn->pn_pos;
                pn = pn2;
            }
            pn->setOp(JSOP_NAME);
        }

        if (!tc->decls.addHoist(atom, (Definition *) pn))
            return JS_FALSE;
        pn->setDefn(true);
        pn->pn_dflags &= ~PND_PLACEHOLDER;
    }

    if (data->op == JSOP_DEFCONST)
        pn->pn_dflags |= PND_CONST;

    if (tc->inFunction())
        return BindFunctionLocal(cx, data, mdl, tc);

    return BindTopLevelVar(cx, data, pn, tc);
}

static bool
MakeSetCall(JSContext *cx, ParseNode *pn, TreeContext *tc, uintN msg)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(pn->isOp(JSOP_CALL) || pn->isOp(JSOP_EVAL) ||
              pn->isOp(JSOP_FUNCALL) || pn->isOp(JSOP_FUNAPPLY));
    if (!ReportStrictModeError(cx, TS(tc->parser), tc, pn, msg))
        return false;

    ParseNode *pn2 = pn->pn_head;
    if (pn2->isKind(PNK_FUNCTION) && (pn2->pn_funbox->tcflags & TCF_GENEXP_LAMBDA)) {
        ReportCompileErrorNumber(cx, TS(tc->parser), pn, JSREPORT_ERROR, msg);
        return false;
    }
    pn->pn_xflags |= PNX_SETCALL;
    return true;
}

static void
NoteLValue(JSContext *cx, ParseNode *pn, TreeContext *tc, uintN dflag = PND_ASSIGNED)
{
    if (pn->isUsed()) {
        Definition *dn = pn->pn_lexdef;

        /*
         * Save the win of PND_INITIALIZED if we can prove 'var x;' and 'x = y'
         * occur as direct kids of the same block with no forward refs to x.
         */
        if (!(dn->pn_dflags & (PND_INITIALIZED | PND_CONST | PND_PLACEHOLDER)) &&
            dn->isBlockChild() &&
            pn->isBlockChild() &&
            dn->pn_blockid == pn->pn_blockid &&
            dn->pn_pos.end <= pn->pn_pos.begin &&
            dn->dn_uses == pn) {
            dflag = PND_INITIALIZED;
        }

        dn->pn_dflags |= dflag;

        if (dn->pn_cookie.isFree() || dn->frameLevel() < tc->staticLevel)
            tc->flags |= TCF_FUN_SETS_OUTER_NAME;
    }

    pn->pn_dflags |= dflag;

    /*
     * Both arguments and the enclosing function's name are immutable bindings
     * in ES5, so assignments to them must do nothing or throw a TypeError
     * depending on code strictness.  Assignment to arguments is a syntax error
     * in strict mode and thus cannot happen.  Outside strict mode, we optimize
     * away assignment to the function name.  For assignment to function name
     * to fail in strict mode, we must have a binding for it in the scope
     * chain; we ensure this happens by making such functions heavyweight.
     */
    JSAtom *lname = pn->pn_atom;
    if (lname == cx->runtime->atomState.argumentsAtom) {
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
        tc->countArgumentsUse(pn);
    } else if (tc->inFunction() && lname == tc->fun()->atom) {
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
    }
}

#if JS_HAS_DESTRUCTURING

static JSBool
BindDestructuringVar(JSContext *cx, BindData *data, ParseNode *pn, TreeContext *tc)
{
    JSAtom *atom;

    /*
     * Destructuring is a form of assignment, so just as for an initialized
     * simple variable, we must check for assignment to 'arguments' and flag
     * the enclosing function (if any) as heavyweight.
     */
    JS_ASSERT(pn->isKind(PNK_NAME));
    atom = pn->pn_atom;
    if (atom == cx->runtime->atomState.argumentsAtom)
        tc->flags |= TCF_FUN_HEAVYWEIGHT;

    data->pn = pn;
    if (!data->binder(cx, data, atom, tc))
        return JS_FALSE;

    /*
     * Select the appropriate name-setting opcode, respecting eager selection
     * done by the data->binder function.
     */
    if (pn->pn_dflags & PND_BOUND) {
        JS_ASSERT(!(pn->pn_dflags & PND_GVAR));
        pn->setOp(pn->isOp(JSOP_ARGUMENTS) ? JSOP_SETNAME : JSOP_SETLOCAL);
    } else {
        pn->setOp((data->op == JSOP_DEFCONST) ? JSOP_SETCONST : JSOP_SETNAME);
    }

    if (data->op == JSOP_DEFCONST)
        pn->pn_dflags |= PND_CONST;

    NoteLValue(cx, pn, tc, PND_INITIALIZED);
    return JS_TRUE;
}

/*
 * Here, we are destructuring {... P: Q, ...} = R, where P is any id, Q is any
 * LHS expression except a destructuring initialiser, and R is on the stack.
 * Because R is already evaluated, the usual LHS-specialized bytecodes won't
 * work.  After pushing R[P] we need to evaluate Q's "reference base" QB and
 * then push its property name QN.  At this point the stack looks like
 *
 *   [... R, R[P], QB, QN]
 *
 * We need to set QB[QN] = R[P].  This is a job for JSOP_ENUMELEM, which takes
 * its operands with left-hand side above right-hand side:
 *
 *   [rval, lval, xval]
 *
 * and pops all three values, setting lval[xval] = rval.  But we cannot select
 * JSOP_ENUMELEM yet, because the LHS may turn out to be an arg or local var,
 * which can be optimized further.  So we select JSOP_SETNAME.
 */
static JSBool
BindDestructuringLHS(JSContext *cx, ParseNode *pn, TreeContext *tc)
{
    switch (pn->getKind()) {
      case PNK_NAME:
        NoteLValue(cx, pn, tc);
        /* FALL THROUGH */

      case PNK_DOT:
      case PNK_LB:
        /*
         * We may be called on a name node that has already been specialized,
         * in the very weird and ECMA-262-required "for (var [x] = i in o) ..."
         * case. See bug 558633.
         */
        if (!(js_CodeSpec[pn->getOp()].format & JOF_SET))
            pn->setOp(JSOP_SETNAME);
        break;

      case PNK_LP:
        if (!MakeSetCall(cx, pn, tc, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return JS_FALSE;
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(pn->isOp(JSOP_XMLNAME));
        pn->setOp(JSOP_BINDXMLNAME);
        break;
#endif

      default:
        ReportCompileErrorNumber(cx, TS(tc->parser), pn,
                                 JSREPORT_ERROR, JSMSG_BAD_LEFTSIDE_OF_ASS);
        return JS_FALSE;
    }

    return JS_TRUE;
}

/*
 * Destructuring patterns can appear in two kinds of contexts:
 *
 * - assignment-like: assignment expressions and |for| loop heads.  In
 *   these cases, the patterns' property value positions can be
 *   arbitrary lvalue expressions; the destructuring is just a fancy
 *   assignment.
 *
 * - declaration-like: |var| and |let| declarations, functions' formal
 *   parameter lists, |catch| clauses, and comprehension tails.  In
 *   these cases, the patterns' property value positions must be
 *   simple names; the destructuring defines them as new variables.
 *
 * In both cases, other code parses the pattern as an arbitrary
 * primaryExpr, and then, here in CheckDestructuring, verify that the
 * tree is a valid destructuring expression.
 *
 * In assignment-like contexts, we parse the pattern with the
 * TCF_DECL_DESTRUCTURING flag clear, so the lvalue expressions in the
 * pattern are parsed normally.  primaryExpr links variable references
 * into the appropriate use chains; creates placeholder definitions;
 * and so on.  CheckDestructuring is called with |data| NULL (since we
 * won't be binding any new names), and we specialize lvalues as
 * appropriate.
 *
 * In declaration-like contexts, the normal variable reference
 * processing would just be an obstruction, because we're going to
 * define the names that appear in the property value positions as new
 * variables anyway.  In this case, we parse the pattern with
 * TCF_DECL_DESTRUCTURING set, which directs primaryExpr to leave
 * whatever name nodes it creates unconnected.  Then, here in
 * CheckDestructuring, we require the pattern's property value
 * positions to be simple names, and define them as appropriate to the
 * context.  For these calls, |data| points to the right sort of
 * BindData.
 *
 * See also UndominateInitializers, immediately below. If you change
 * either of these functions, you might have to change the other to
 * match.
 *
 * The 'toplevel' is a private detail of the recursive strategy used by
 * CheckDestructuring and callers should use the default value.
 */
static bool
CheckDestructuring(JSContext *cx, BindData *data, ParseNode *left, TreeContext *tc,
                   bool toplevel = true)
{
    bool ok;

    if (left->isKind(PNK_ARRAYCOMP)) {
        ReportCompileErrorNumber(cx, TS(tc->parser), left, JSREPORT_ERROR,
                                 JSMSG_ARRAY_COMP_LEFTSIDE);
        return false;
    }

    StaticBlockObject *blockObj = data && data->binder == BindLet ? data->let.blockObj : NULL;
    uint32_t blockCountBefore = blockObj ? blockObj->slotCount() : 0;

    if (left->isKind(PNK_RB)) {
        for (ParseNode *pn = left->pn_head; pn; pn = pn->pn_next) {
            /* Nullary comma is an elision; binary comma is an expression.*/
            if (!pn->isArrayHole()) {
                if (pn->isKind(PNK_RB) || pn->isKind(PNK_RC)) {
                    ok = CheckDestructuring(cx, data, pn, tc, false);
                } else {
                    if (data) {
                        if (!pn->isKind(PNK_NAME)) {
                            ReportCompileErrorNumber(cx, TS(tc->parser), pn, JSREPORT_ERROR,
                                                     JSMSG_NO_VARIABLE_NAME);
                            return false;
                        }
                        ok = BindDestructuringVar(cx, data, pn, tc);
                    } else {
                        ok = BindDestructuringLHS(cx, pn, tc);
                    }
                }
                if (!ok)
                    return false;
            }
        }
    } else {
        JS_ASSERT(left->isKind(PNK_RC));
        for (ParseNode *pair = left->pn_head; pair; pair = pair->pn_next) {
            JS_ASSERT(pair->isKind(PNK_COLON));
            ParseNode *pn = pair->pn_right;

            if (pn->isKind(PNK_RB) || pn->isKind(PNK_RC)) {
                ok = CheckDestructuring(cx, data, pn, tc, false);
            } else if (data) {
                if (!pn->isKind(PNK_NAME)) {
                    ReportCompileErrorNumber(cx, TS(tc->parser), pn, JSREPORT_ERROR,
                                             JSMSG_NO_VARIABLE_NAME);
                    return false;
                }
                ok = BindDestructuringVar(cx, data, pn, tc);
            } else {
                ok = BindDestructuringLHS(cx, pn, tc);
            }
            if (!ok)
                return false;
        }
    }

    /*
     * The catch/finally handler implementation in the interpreter assumes
     * that any operation that introduces a new scope (like a "let" or "with"
     * block) increases the stack depth. This way, it is possible to restore
     * the scope chain based on stack depth of the handler alone. "let" with
     * an empty destructuring pattern like in
     *
     *   let [] = 1;
     *
     * would violate this assumption as the there would be no let locals to
     * store on the stack.
     *
     * Furthermore, the decompiler needs an abstract stack location to store
     * the decompilation of each let block/expr initializer. E.g., given:
     *
     *   let (x = 1, [[]] = b, y = 3, {a:[]} = c) { ... }
     *
     * four slots are needed.
     *
     * To satisfy both constraints, we push a dummy slot (and add a
     * corresponding dummy property to the block object) for each initializer
     * that doesn't introduce at least one binding.
     */
    if (toplevel && blockObj && blockCountBefore == blockObj->slotCount()) {
        bool redeclared;
        if (!blockObj->addVar(cx, INT_TO_JSID(blockCountBefore), blockCountBefore, &redeclared))
            return false;
        JS_ASSERT(!redeclared);
        JS_ASSERT(blockObj->slotCount() == blockCountBefore + 1);
    }

    return true;
}

/*
 * Extend the pn_pos.end source coordinate of each name in a destructuring
 * binding such as
 *
 *   var [x, y] = [function () y, 42];
 *
 * to cover the entire initializer, so that the initialized bindings do not
 * appear to dominate any closures in the initializer. See bug 496134.
 *
 * The quick-and-dirty dominance computation in Parser::setFunctionKinds is not
 * very precise. With one-pass SSA construction from structured source code
 * (see "Single-Pass Generation of Static Single Assignment Form for Structured
 * Languages", Brandis and Mössenböck), we could do much better.
 *
 * See CheckDestructuring, immediately above. If you change either of these
 * functions, you might have to change the other to match.
 */
static void
UndominateInitializers(ParseNode *left, const TokenPtr &end, TreeContext *tc)
{
    if (left->isKind(PNK_RB)) {
        for (ParseNode *pn = left->pn_head; pn; pn = pn->pn_next) {
            /* Nullary comma is an elision; binary comma is an expression.*/
            if (!pn->isKind(PNK_COMMA) || !pn->isArity(PN_NULLARY)) {
                if (pn->isKind(PNK_RB) || pn->isKind(PNK_RC))
                    UndominateInitializers(pn, end, tc);
                else
                    pn->pn_pos.end = end;
            }
        }
    } else {
        JS_ASSERT(left->isKind(PNK_RC));

        for (ParseNode *pair = left->pn_head; pair; pair = pair->pn_next) {
            JS_ASSERT(pair->isKind(PNK_COLON));
            ParseNode *pn = pair->pn_right;
            if (pn->isKind(PNK_RB) || pn->isKind(PNK_RC))
                UndominateInitializers(pn, end, tc);
            else
                pn->pn_pos.end = end;
        }
    }
}

ParseNode *
Parser::destructuringExpr(BindData *data, TokenKind tt)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    tc->flags |= TCF_DECL_DESTRUCTURING;
    ParseNode *pn = primaryExpr(tt, JS_FALSE);
    tc->flags &= ~TCF_DECL_DESTRUCTURING;
    if (!pn)
        return NULL;
    if (!CheckDestructuring(context, data, pn, tc))
        return NULL;
    return pn;
}

#endif /* JS_HAS_DESTRUCTURING */

ParseNode *
Parser::returnOrYield(bool useAssignExpr)
{
    TokenKind tt = tokenStream.currentToken().type;
    if (!tc->inFunction()) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_RETURN_OR_YIELD,
                          (tt == TOK_RETURN) ? js_return_str : js_yield_str);
        return NULL;
    }

    ParseNode *pn = UnaryNode::create((tt == TOK_RETURN) ? PNK_RETURN : PNK_YIELD, tc);
    if (!pn)
        return NULL;

#if JS_HAS_GENERATORS
    if (tt == TOK_YIELD) {
        /*
         * If we're within parens, we won't know if this is a generator expression until we see
         * a |for| token, so we have to delay flagging the current function.
         */
        if (tc->parenDepth == 0) {
            tc->flags |= TCF_FUN_IS_GENERATOR;
        } else {
            tc->yieldCount++;
            tc->yieldNode = pn;
        }
    }
#endif

    /* This is ugly, but we don't want to require a semicolon. */
    TokenKind tt2 = tokenStream.peekTokenSameLine(TSF_OPERAND);
    if (tt2 == TOK_ERROR)
        return NULL;

    if (tt2 != TOK_EOF && tt2 != TOK_EOL && tt2 != TOK_SEMI && tt2 != TOK_RC
#if JS_HAS_GENERATORS
        && (tt != TOK_YIELD ||
            (tt2 != tt && tt2 != TOK_RB && tt2 != TOK_RP &&
             tt2 != TOK_COLON && tt2 != TOK_COMMA))
#endif
        )
    {
        ParseNode *pn2 = useAssignExpr ? assignExpr() : expr();
        if (!pn2)
            return NULL;
#if JS_HAS_GENERATORS
        if (tt == TOK_RETURN)
#endif
            tc->flags |= TCF_RETURN_EXPR;
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->pn_kid = pn2;
    } else {
#if JS_HAS_GENERATORS
        if (tt == TOK_RETURN)
#endif
            tc->flags |= TCF_RETURN_VOID;
    }

    if ((~tc->flags & (TCF_RETURN_EXPR | TCF_FUN_IS_GENERATOR)) == 0) {
        /* As in Python (see PEP-255), disallow return v; in generators. */
        ReportBadReturn(context, tc, pn, JSREPORT_ERROR,
                        JSMSG_BAD_GENERATOR_RETURN,
                        JSMSG_BAD_ANON_GENERATOR_RETURN);
        return NULL;
    }

    if (context->hasStrictOption() &&
        (~tc->flags & (TCF_RETURN_EXPR | TCF_RETURN_VOID)) == 0 &&
        !ReportBadReturn(context, tc, pn, JSREPORT_WARNING | JSREPORT_STRICT,
                         JSMSG_NO_RETURN_VALUE,
                         JSMSG_ANON_NO_RETURN_VALUE)) {
        return NULL;
    }

    return pn;
}

static ParseNode *
PushLexicalScope(JSContext *cx, TreeContext *tc, StaticBlockObject &obj, StmtInfo *stmt)
{
    ParseNode *pn = LexicalScopeNode::create(PNK_LEXICALSCOPE, tc);
    if (!pn)
        return NULL;

    ObjectBox *blockbox = tc->parser->newObjectBox(&obj);
    if (!blockbox)
        return NULL;

    PushBlockScope(tc, stmt, obj, -1);
    pn->setOp(JSOP_LEAVEBLOCK);
    pn->pn_objbox = blockbox;
    pn->pn_cookie.makeFree();
    pn->pn_dflags = 0;
    if (!GenerateBlockId(tc, stmt->blockid))
        return NULL;
    pn->pn_blockid = stmt->blockid;
    return pn;
}

static ParseNode *
PushLexicalScope(JSContext *cx, TreeContext *tc, StmtInfo *stmt)
{
    StaticBlockObject *blockObj = StaticBlockObject::create(cx);
    if (!blockObj)
        return NULL;

    return PushLexicalScope(cx, tc, *blockObj, stmt);
}

#if JS_HAS_BLOCK_SCOPE

struct AddDecl
{
    uint32_t blockid;

    AddDecl(uint32_t blockid) : blockid(blockid) {}

    bool operator()(TreeContext *tc, StaticBlockObject &blockObj, const Shape &shape, JSAtom *atom)
    {
        ParseNode *def = (ParseNode *) blockObj.getSlot(shape.slot()).toPrivate();
        def->pn_blockid = blockid;
        return Define(def, atom, tc, true);
    }
};

static ParseNode *
PushLetScope(JSContext *cx, TreeContext *tc, StaticBlockObject &blockObj, StmtInfo *stmt)
{
    ParseNode *pn = PushLexicalScope(cx, tc, blockObj, stmt);
    if (!pn)
        return NULL;

    /* Tell codegen to emit JSOP_ENTERLETx (not JSOP_ENTERBLOCK). */
    pn->pn_dflags |= PND_LET;

    /* Populate the new scope with decls found in the head with updated blockid. */
    if (!ForEachLetDef(tc, blockObj, AddDecl(stmt->blockid)))
        return NULL;

    return pn;
}

/*
 * Parse a let block statement or let expression (determined by 'letContext').
 * In both cases, bindings are not hoisted to the top of the enclosing block
 * and thus must be carefully injected between variables() and the let body.
 */
ParseNode *
Parser::letBlock(LetContext letContext)
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_LET);

    ParseNode *pnlet = BinaryNode::create(PNK_LET, tc);
    if (!pnlet)
        return NULL;

    StaticBlockObject *blockObj = StaticBlockObject::create(context);
    if (!blockObj)
        return NULL;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_LET);

    ParseNode *vars = variables(PNK_LET, blockObj, DontHoistVars);
    if (!vars)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_LET);

    StmtInfo stmtInfo;
    ParseNode *block = PushLetScope(context, tc, *blockObj, &stmtInfo);
    if (!block)
        return NULL;

    pnlet->pn_left = vars;
    pnlet->pn_right = block;

    ParseNode *ret;
    if (letContext == LetStatement && !tokenStream.matchToken(TOK_LC, TSF_OPERAND)) {
        /*
         * Strict mode eliminates a grammar ambiguity with unparenthesized
         * LetExpressions in an ExpressionStatement. If followed immediately
         * by an arguments list, it's ambiguous whether the let expression
         * is the callee or the call is inside the let expression body.
         *
         * See bug 569464.
         */
        if (!ReportStrictModeError(context, &tokenStream, tc, pnlet,
                                   JSMSG_STRICT_CODE_LET_EXPR_STMT)) {
            return NULL;
        }

        /*
         * If this is really an expression in let statement guise, then we
         * need to wrap the TOK_LET node in a TOK_SEMI node so that we pop
         * the return value of the expression.
         */
        ParseNode *semi = UnaryNode::create(PNK_SEMI, tc);
        if (!semi)
            return NULL;

        semi->pn_kid = pnlet;

        letContext = LetExpresion;
        ret = semi;
    } else {
        ret = pnlet;
    }

    if (letContext == LetStatement) {
        JS_ASSERT(block->getOp() == JSOP_LEAVEBLOCK);
        block->pn_expr = statements();
        if (!block->pn_expr)
            return NULL;
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_LET);
    } else {
        JS_ASSERT(letContext == LetExpresion);
        block->setOp(JSOP_LEAVEBLOCKEXPR);
        block->pn_expr = assignExpr();
        if (!block->pn_expr)
            return NULL;
    }

    PopStatement(tc);
    return ret;
}

#endif /* JS_HAS_BLOCK_SCOPE */

static bool
PushBlocklikeStatement(StmtInfo *stmt, StmtType type, TreeContext *tc)
{
    PushStatement(tc, stmt, type, -1);
    return GenerateBlockId(tc, stmt->blockid);
}

static ParseNode *
NewBindingNode(JSAtom *atom, TreeContext *tc, StaticBlockObject *blockObj = NULL,
               VarContext varContext = HoistVars)
{
    /*
     * If this name is being injected into an existing block/function, see if
     * it has already been declared or if it resolves an outstanding lexdep.
     * Otherwise, this is a let block/expr that introduces a new scope and thus
     * shadows existing decls and doesn't resolve existing lexdeps. Duplicate
     * names are caught by BindLet.
     */
    if (!blockObj || varContext == HoistVars) {
        ParseNode *pn = tc->decls.lookupFirst(atom);
        AtomDefnPtr removal;
        if (pn) {
            JS_ASSERT(!pn->isPlaceholder());
        } else {
            removal = tc->lexdeps->lookup(atom);
            pn = removal ? removal.value() : NULL;
            JS_ASSERT_IF(pn, pn->isPlaceholder());
        }

        if (pn) {
            JS_ASSERT(pn->isDefn());

            /*
             * A let binding at top level becomes a var before we get here, so if
             * pn and tc have the same blockid then that id must not be the bodyid.
             * If pn is a forward placeholder definition from the same or a higher
             * block then we claim it.
             */
            JS_ASSERT_IF(blockObj && pn->pn_blockid == tc->blockid(),
                         pn->pn_blockid != tc->bodyid);

            if (pn->isPlaceholder() && pn->pn_blockid >= tc->blockid()) {
                pn->pn_blockid = tc->blockid();
                tc->lexdeps->remove(removal);
                return pn;
            }
        }
    }

    /* Make a new node for this declarator name (or destructuring pattern). */
    JS_ASSERT(tc->parser->tokenStream.currentToken().type == TOK_NAME);
    ParseNode *pn = NameNode::create(PNK_NAME, atom, tc);
    if (!pn)
        return NULL;

    if (atom == tc->parser->context->runtime->atomState.argumentsAtom)
        tc->countArgumentsUse(pn);

    return pn;
}

ParseNode *
Parser::switchStatement()
{
    JS_ASSERT(tc->parser->tokenStream.currentToken().type == TOK_SWITCH);
    ParseNode *pn = BinaryNode::create(PNK_SWITCH, tc);
    if (!pn)
        return NULL;
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_SWITCH);

    /* pn1 points to the switch's discriminant. */
    ParseNode *pn1 = parenExpr();
    if (!pn1)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_SWITCH);
    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_SWITCH);

    /*
     * NB: we must push stmtInfo before calling GenerateBlockIdForStmtNode
     * because that function states tc->topStmt->blockid.
     */
    StmtInfo stmtInfo;
    PushStatement(tc, &stmtInfo, STMT_SWITCH, -1);

    /* pn2 is a list of case nodes. The default case has pn_left == NULL */
    ParseNode *pn2 = ListNode::create(PNK_STATEMENTLIST, tc);
    if (!pn2)
        return NULL;
    pn2->makeEmpty();
    if (!GenerateBlockIdForStmtNode(pn2, tc))
        return NULL;
    ParseNode *saveBlock = tc->blockNode;
    tc->blockNode = pn2;

    bool seenDefault = false;
    TokenKind tt;
    while ((tt = tokenStream.getToken()) != TOK_RC) {
        ParseNode *pn3;
        switch (tt) {
          case TOK_DEFAULT:
            if (seenDefault) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_TOO_MANY_DEFAULTS);
                return NULL;
            }
            seenDefault = true;
            pn3 = BinaryNode::create(PNK_DEFAULT, tc);
            if (!pn3)
                return NULL;
            break;

          case TOK_CASE:
          {
            pn3 = BinaryNode::create(PNK_CASE, tc);
            if (!pn3)
                return NULL;
            pn3->pn_left = expr();
            if (!pn3->pn_left)
                return NULL;
            break;
          }

          case TOK_ERROR:
            return NULL;

          default:
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_SWITCH);
            return NULL;
        }

        pn2->append(pn3);
        if (pn2->pn_count == JS_BIT(16)) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_TOO_MANY_CASES);
            return NULL;
        }

        MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_CASE);

        ParseNode *pn4 = ListNode::create(PNK_STATEMENTLIST, tc);
        if (!pn4)
            return NULL;
        pn4->makeEmpty();
        while ((tt = tokenStream.peekToken(TSF_OPERAND)) != TOK_RC &&
               tt != TOK_CASE && tt != TOK_DEFAULT) {
            if (tt == TOK_ERROR)
                return NULL;
            ParseNode *pn5 = statement();
            if (!pn5)
                return NULL;
            pn4->pn_pos.end = pn5->pn_pos.end;
            pn4->append(pn5);
        }

        /* Fix the PN_LIST so it doesn't begin at the TOK_COLON. */
        if (pn4->pn_head)
            pn4->pn_pos.begin = pn4->pn_head->pn_pos.begin;
        pn3->pn_pos.end = pn4->pn_pos.end;
        pn3->pn_right = pn4;
    }

    /*
     * Handle the case where there was a let declaration in any case in
     * the switch body, but not within an inner block.  If it replaced
     * tc->blockNode with a new block node then we must refresh pn2 and
     * then restore tc->blockNode.
     */
    if (tc->blockNode != pn2)
        pn2 = tc->blockNode;
    tc->blockNode = saveBlock;
    PopStatement(tc);

    pn->pn_pos.end = pn2->pn_pos.end = tokenStream.currentToken().pos.end;
    pn->pn_left = pn1;
    pn->pn_right = pn2;
    return pn;
}

ParseNode *
Parser::forStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    /* A FOR node is binary, left is loop control and right is the body. */
    ParseNode *pn = BinaryNode::create(PNK_FOR, tc);
    if (!pn)
        return NULL;

    StmtInfo forStmt;
    PushStatement(tc, &forStmt, STMT_FOR_LOOP, -1);

    pn->setOp(JSOP_ITER);
    pn->pn_iflags = 0;
    if (tokenStream.matchToken(TOK_NAME)) {
        if (tokenStream.currentToken().name() == context->runtime->atomState.eachAtom)
            pn->pn_iflags = JSITER_FOREACH;
        else
            tokenStream.ungetToken();
    }

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    /*
     * True if we have 'for (var/let/const ...)', except in the oddball case
     * where 'let' begins a let-expression in 'for (let (...) ...)'.
     */
    bool forDecl = false;

    /* Non-null when forDecl is true for a 'for (let ...)' statement. */
    StaticBlockObject *blockObj = NULL;

    /* Set to 'x' in 'for (x ;... ;...)' or 'for (x in ...)'. */
    ParseNode *pn1;

    {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt == TOK_SEMI) {
            if (pn->pn_iflags & JSITER_FOREACH) {
                reportErrorNumber(pn, JSREPORT_ERROR, JSMSG_BAD_FOR_EACH_LOOP);
                return NULL;
            }

            pn1 = NULL;
        } else {
            /*
             * Set pn1 to a var list or an initializing expression.
             *
             * Set the TCF_IN_FOR_INIT flag during parsing of the first clause
             * of the for statement.  This flag will be used by the RelExpr
             * production; if it is set, then the 'in' keyword will not be
             * recognized as an operator, leaving it available to be parsed as
             * part of a for/in loop.
             *
             * A side effect of this restriction is that (unparenthesized)
             * expressions involving an 'in' operator are illegal in the init
             * clause of an ordinary for loop.
             */
            tc->flags |= TCF_IN_FOR_INIT;
            if (tt == TOK_VAR || tt == TOK_CONST) {
                forDecl = true;
                tokenStream.consumeKnownToken(tt);
                pn1 = variables(tt == TOK_VAR ? PNK_VAR : PNK_CONST);
            }
#if JS_HAS_BLOCK_SCOPE
            else if (tt == TOK_LET) {
                (void) tokenStream.getToken();
                if (tokenStream.peekToken() == TOK_LP) {
                    pn1 = letBlock(LetExpresion);
                } else {
                    forDecl = true;
                    blockObj = StaticBlockObject::create(context);
                    if (!blockObj)
                        return NULL;
                    pn1 = variables(PNK_LET, blockObj, DontHoistVars);
                }
            }
#endif
            else {
                pn1 = expr();
            }
            tc->flags &= ~TCF_IN_FOR_INIT;
            if (!pn1)
                return NULL;
        }
    }

    JS_ASSERT_IF(forDecl, pn1->isArity(PN_LIST));
    JS_ASSERT(!!blockObj == (forDecl && pn1->isOp(JSOP_NOP)));

    const TokenPos pos = tokenStream.currentToken().pos;

    /* If non-null, the parent that should be returned instead of forHead. */
    ParseNode *forParent = NULL;

    /*
     * We can be sure that it's a for/in loop if there's still an 'in'
     * keyword here, even if JavaScript recognizes 'in' as an operator,
     * as we've excluded 'in' from being parsed in RelExpr by setting
     * the TCF_IN_FOR_INIT flag in our TreeContext.
     */
    ParseNode *forHead;     /* initialized by both branches. */
    StmtInfo letStmt;       /* used if blockObj != NULL. */
    ParseNode *pn2, *pn3;   /* forHead->pn_kid1 and pn_kid2. */
    if (pn1 && tokenStream.matchToken(TOK_IN)) {
        /*
         * Parse the rest of the for/in head.
         *
         * Here pn1 is everything to the left of 'in'. At the end of this block,
         * pn1 is a decl or NULL, pn2 is the assignment target that receives the
         * enumeration value each iteration, and pn3 is the rhs of 'in'.
         */
        pn->pn_iflags |= JSITER_ENUMERATE;
        forStmt.type = STMT_FOR_IN_LOOP;

        /* Check that the left side of the 'in' is valid. */
        if (forDecl
            ? (pn1->pn_count > 1 || pn1->isOp(JSOP_DEFCONST)
#if JS_HAS_DESTRUCTURING
               || (versionNumber() == JSVERSION_1_7 &&
                   pn->isOp(JSOP_ITER) &&
                   !(pn->pn_iflags & JSITER_FOREACH) &&
                   (pn1->pn_head->isKind(PNK_RC) ||
                    (pn1->pn_head->isKind(PNK_RB) &&
                     pn1->pn_head->pn_count != 2) ||
                    (pn1->pn_head->isKind(PNK_ASSIGN) &&
                     (!pn1->pn_head->pn_left->isKind(PNK_RB) ||
                      pn1->pn_head->pn_left->pn_count != 2))))
#endif
              )
            : (!pn1->isKind(PNK_NAME) &&
               !pn1->isKind(PNK_DOT) &&
#if JS_HAS_DESTRUCTURING
               ((versionNumber() == JSVERSION_1_7 &&
                 pn->isOp(JSOP_ITER) &&
                 !(pn->pn_iflags & JSITER_FOREACH))
                ? (!pn1->isKind(PNK_RB) || pn1->pn_count != 2)
                : (!pn1->isKind(PNK_RB) && !pn1->isKind(PNK_RC))) &&
#endif
               !pn1->isKind(PNK_LP) &&
#if JS_HAS_XML_SUPPORT
               !pn1->isKind(PNK_XMLUNARY) &&
#endif
               !pn1->isKind(PNK_LB)))
        {
            reportErrorNumber(pn1, JSREPORT_ERROR, JSMSG_BAD_FOR_LEFTSIDE);
            return NULL;
        }

        /*
         * After the following if-else, pn2 will point to the name or
         * destructuring pattern on in's left. pn1 will point to the decl, if
         * any, else NULL. Note that the "declaration with initializer" case
         * rewrites the loop-head, moving the decl and setting pn1 to NULL.
         */
        pn2 = NULL;
        uintN dflag = PND_ASSIGNED;
        if (forDecl) {
            /* Tell EmitVariables that pn1 is part of a for/in. */
            pn1->pn_xflags |= PNX_FORINVAR;

            pn2 = pn1->pn_head;
            if ((pn2->isKind(PNK_NAME) && pn2->maybeExpr())
#if JS_HAS_DESTRUCTURING
                || pn2->isKind(PNK_ASSIGN)
#endif
                )
            {
                /*
                 * Declaration with initializer.
                 *
                 * Rewrite 'for (<decl> x = i in o)' where <decl> is 'var' or
                 * 'const' to hoist the initializer or the entire decl out of
                 * the loop head.
                 */
#if JS_HAS_BLOCK_SCOPE
                if (blockObj) {
                    reportErrorNumber(pn2, JSREPORT_ERROR, JSMSG_INVALID_FOR_IN_INIT);
                    return NULL;
                }
#endif /* JS_HAS_BLOCK_SCOPE */

                ParseNode *pnseq = ListNode::create(PNK_SEQ, tc);
                if (!pnseq)
                    return NULL;

                dflag = PND_INITIALIZED;

                /*
                 * All of 'var x = i' is hoisted above 'for (x in o)',
                 * so clear PNX_FORINVAR.
                 *
                 * Request JSOP_POP here since the var is for a simple
                 * name (it is not a destructuring binding's left-hand
                 * side) and it has an initializer.
                 */
                pn1->pn_xflags &= ~PNX_FORINVAR;
                pn1->pn_xflags |= PNX_POPVAR;
                pnseq->initList(pn1);
                pn1 = NULL;

#if JS_HAS_DESTRUCTURING
                if (pn2->isKind(PNK_ASSIGN)) {
                    pn2 = pn2->pn_left;
                    JS_ASSERT(pn2->isKind(PNK_RB) || pn2->isKind(PNK_RC) ||
                              pn2->isKind(PNK_NAME));
                }
#endif
                pnseq->append(pn);
                forParent = pnseq;
            }
        } else {
            /* Not a declaration. */
            JS_ASSERT(!blockObj);
            pn2 = pn1;
            pn1 = NULL;

            if (!setAssignmentLhsOps(pn2, JSOP_NOP))
                return NULL;
        }

        pn3 = expr();
        if (!pn3)
            return NULL;

        if (blockObj) {
            /*
             * Now that the pn3 has been parsed, push the let scope. To hold
             * the blockObj for the emitter, wrap the TOK_LEXICALSCOPE node
             * created by PushLetScope around the for's initializer. This also
             * serves to indicate the let-decl to the emitter.
             */
            ParseNode *block = PushLetScope(context, tc, *blockObj, &letStmt);
            if (!block)
                return NULL;
            letStmt.flags |= SIF_FOR_BLOCK;
            block->pn_expr = pn1;
            pn1 = block;
        }

        if (forDecl) {
            /*
             * pn2 is part of a declaration. Make a copy that can be passed to
             * EmitAssignment. Take care to do this after PushLetScope has
             * Define's the new binding since this pn2->isDefn() which tells
             * CloneLeftHandSide to make the new pn2 a use.
             */
            pn2 = CloneLeftHandSide(pn2, tc);
            if (!pn2)
                return NULL;
        }

        switch (pn2->getKind()) {
          case PNK_NAME:
            /* Beware 'for (arguments in ...)' with or without a 'var'. */
            NoteLValue(context, pn2, tc, dflag);
            break;

#if JS_HAS_DESTRUCTURING
          case PNK_ASSIGN:
            JS_NOT_REACHED("forStatement TOK_ASSIGN");
            break;

          case PNK_RB:
          case PNK_RC:
            if (versionNumber() == JSVERSION_1_7) {
                /*
                 * Destructuring for-in requires [key, value] enumeration
                 * in JS1.7.
                 */
                JS_ASSERT(pn->isOp(JSOP_ITER));
                if (!(pn->pn_iflags & JSITER_FOREACH))
                    pn->pn_iflags |= JSITER_FOREACH | JSITER_KEYVALUE;
            }
            break;
#endif

          default:;
        }

        forHead = TernaryNode::create(PNK_FORIN, tc);
        if (!forHead)
            return NULL;
    } else {
        if (blockObj) {
            /*
             * Desugar 'for (let A; B; C) D' into 'let (A) { for (; B; C) D }'
             * to induce the correct scoping for A.
             */
            ParseNode *block = PushLetScope(context, tc, *blockObj, &letStmt);
            if (!block)
                return NULL;
            letStmt.flags |= SIF_FOR_BLOCK;

            ParseNode *let = new_<BinaryNode>(PNK_LET, JSOP_NOP, pos, pn1, block);
            if (!let)
                return NULL;

            pn1 = NULL;
            block->pn_expr = pn;
            forParent = let;
        }

        if (pn->pn_iflags & JSITER_FOREACH) {
            reportErrorNumber(pn, JSREPORT_ERROR, JSMSG_BAD_FOR_EACH_LOOP);
            return NULL;
        }
        pn->setOp(JSOP_NOP);

        /* Parse the loop condition or null into pn2. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
        if (tokenStream.peekToken(TSF_OPERAND) == TOK_SEMI) {
            pn2 = NULL;
        } else {
            pn2 = expr();
            if (!pn2)
                return NULL;
        }

        /* Parse the update expression or null into pn3. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
        if (tokenStream.peekToken(TSF_OPERAND) == TOK_RP) {
            pn3 = NULL;
        } else {
            pn3 = expr();
            if (!pn3)
                return NULL;
        }

        forHead = TernaryNode::create(PNK_FORHEAD, tc);
        if (!forHead)
            return NULL;
    }

    forHead->pn_pos = pos;
    forHead->setOp(JSOP_NOP);
    forHead->pn_kid1 = pn1;
    forHead->pn_kid2 = pn2;
    forHead->pn_kid3 = pn3;
    pn->pn_left = forHead;

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

    /* Parse the loop body. */
    ParseNode *body = statement();
    if (!body)
        return NULL;

    /* Record the absolute line number for source note emission. */
    pn->pn_pos.end = body->pn_pos.end;
    pn->pn_right = body;

    if (forParent) {
        forParent->pn_pos.begin = pn->pn_pos.begin;
        forParent->pn_pos.end = pn->pn_pos.end;
    }

#if JS_HAS_BLOCK_SCOPE
    if (blockObj)
        PopStatement(tc);
#endif
    PopStatement(tc);
    return forParent ? forParent : pn;
}

ParseNode *
Parser::tryStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_TRY));

    /*
     * try nodes are ternary.
     * kid1 is the try statement
     * kid2 is the catch node list or null
     * kid3 is the finally statement
     *
     * catch nodes are ternary.
     * kid1 is the lvalue (TOK_NAME, TOK_LB, or TOK_LC)
     * kid2 is the catch guard or null if no guard
     * kid3 is the catch block
     *
     * catch lvalue nodes are either:
     *   TOK_NAME for a single identifier
     *   TOK_RB or TOK_RC for a destructuring left-hand side
     *
     * finally nodes are TOK_LC statement lists.
     */
    ParseNode *pn = TernaryNode::create(PNK_TRY, tc);
    if (!pn)
        return NULL;
    pn->setOp(JSOP_NOP);

    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_TRY);
    StmtInfo stmtInfo;
    if (!PushBlocklikeStatement(&stmtInfo, STMT_TRY, tc))
        return NULL;
    pn->pn_kid1 = statements();
    if (!pn->pn_kid1)
        return NULL;
    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_TRY);
    PopStatement(tc);

    ParseNode *lastCatch;
    ParseNode *catchList = NULL;
    TokenKind tt = tokenStream.getToken();
    if (tt == TOK_CATCH) {
        catchList = ListNode::create(PNK_CATCHLIST, tc);
        if (!catchList)
            return NULL;
        catchList->makeEmpty();
        lastCatch = NULL;

        do {
            ParseNode *pnblock;
            BindData data;

            /* Check for another catch after unconditional catch. */
            if (lastCatch && !lastCatch->pn_kid2) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_CATCH_AFTER_GENERAL);
                return NULL;
            }

            /*
             * Create a lexical scope node around the whole catch clause,
             * including the head.
             */
            pnblock = PushLexicalScope(context, tc, &stmtInfo);
            if (!pnblock)
                return NULL;
            stmtInfo.type = STMT_CATCH;

            /*
             * Legal catch forms are:
             *   catch (lhs)
             *   catch (lhs if <boolean_expression>)
             * where lhs is a name or a destructuring left-hand side.
             * (the latter is legal only #ifdef JS_HAS_CATCH_GUARD)
             */
            ParseNode *pn2 = TernaryNode::create(PNK_CATCH, tc);
            if (!pn2)
                return NULL;
            pnblock->pn_expr = pn2;
            MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_CATCH);

            /*
             * Contrary to ECMA Ed. 3, the catch variable is lexically
             * scoped, not a property of a new Object instance.  This is
             * an intentional change that anticipates ECMA Ed. 4.
             */
            data.initLet(HoistVars, *tc->blockChain, JSMSG_TOO_MANY_CATCH_VARS);
            JS_ASSERT(data.let.blockObj && data.let.blockObj == pnblock->pn_objbox->object);

            tt = tokenStream.getToken();
            ParseNode *pn3;
            switch (tt) {
#if JS_HAS_DESTRUCTURING
              case TOK_LB:
              case TOK_LC:
                pn3 = destructuringExpr(&data, tt);
                if (!pn3)
                    return NULL;
                break;
#endif

              case TOK_NAME:
              {
                JSAtom *label = tokenStream.currentToken().name();
                pn3 = NewBindingNode(label, tc);
                if (!pn3)
                    return NULL;
                data.pn = pn3;
                if (!data.binder(context, &data, label, tc))
                    return NULL;
                break;
              }

              default:
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_CATCH_IDENTIFIER);
                return NULL;
            }

            pn2->pn_kid1 = pn3;
#if JS_HAS_CATCH_GUARD
            /*
             * We use 'catch (x if x === 5)' (not 'catch (x : x === 5)')
             * to avoid conflicting with the JS2/ECMAv4 type annotation
             * catchguard syntax.
             */
            if (tokenStream.matchToken(TOK_IF)) {
                pn2->pn_kid2 = expr();
                if (!pn2->pn_kid2)
                    return NULL;
            }
#endif
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_CATCH);

            MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CATCH);
            pn2->pn_kid3 = statements();
            if (!pn2->pn_kid3)
                return NULL;
            MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_CATCH);
            PopStatement(tc);

            catchList->append(pnblock);
            lastCatch = pn2;
            tt = tokenStream.getToken(TSF_OPERAND);
        } while (tt == TOK_CATCH);
    }
    pn->pn_kid2 = catchList;

    if (tt == TOK_FINALLY) {
        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY);
        if (!PushBlocklikeStatement(&stmtInfo, STMT_FINALLY, tc))
            return NULL;
        pn->pn_kid3 = statements();
        if (!pn->pn_kid3)
            return NULL;
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_FINALLY);
        PopStatement(tc);
    } else {
        tokenStream.ungetToken();
    }
    if (!catchList && !pn->pn_kid3) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_CATCH_OR_FINALLY);
        return NULL;
    }
    return pn;
}

ParseNode *
Parser::withStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_WITH));

    /*
     * In most cases, we want the constructs forbidden in strict mode
     * code to be a subset of those that JSOPTION_STRICT warns about, and
     * we should use ReportStrictModeError.  However, 'with' is the sole
     * instance of a construct that is forbidden in strict mode code, but
     * doesn't even merit a warning under JSOPTION_STRICT.  See
     * https://bugzilla.mozilla.org/show_bug.cgi?id=514576#c1.
     */
    if (tc->flags & TCF_STRICT_MODE_CODE) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_STRICT_CODE_WITH);
        return NULL;
    }

    ParseNode *pn = BinaryNode::create(PNK_WITH, tc);
    if (!pn)
        return NULL;
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_WITH);
    ParseNode *pn2 = parenExpr();
    if (!pn2)
        return NULL;
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_WITH);
    pn->pn_left = pn2;

    ParseNode *oldWith = tc->innermostWith;
    tc->innermostWith = pn;

    StmtInfo stmtInfo;
    PushStatement(tc, &stmtInfo, STMT_WITH, -1);
    pn2 = statement();
    if (!pn2)
        return NULL;
    PopStatement(tc);

    pn->pn_pos.end = pn2->pn_pos.end;
    pn->pn_right = pn2;
    tc->flags |= TCF_FUN_HEAVYWEIGHT;
    tc->innermostWith = oldWith;

    /*
     * Make sure to deoptimize lexical dependencies inside the |with|
     * to safely optimize binding globals (see bug 561923).
     */
    for (AtomDefnRange r = tc->lexdeps->all(); !r.empty(); r.popFront()) {
        Definition *defn = r.front().value();
        Definition *lexdep = defn->resolve();
        DeoptimizeUsesWithin(lexdep, pn->pn_pos);
    }

    return pn;
}

#if JS_HAS_BLOCK_SCOPE
ParseNode *
Parser::letStatement()
{
    ParseNode *pn;
    do {
        /* Check for a let statement or let expression. */
        if (tokenStream.peekToken() == TOK_LP) {
            pn = letBlock(LetStatement);
            if (!pn)
                return NULL;

            JS_ASSERT(pn->isKind(PNK_LET) || pn->isKind(PNK_SEMI));
            if (pn->isKind(PNK_LET) && pn->pn_expr->getOp() == JSOP_LEAVEBLOCK)
                return pn;

            /* Let expressions require automatic semicolon insertion. */
            JS_ASSERT(pn->isKind(PNK_SEMI) || pn->isOp(JSOP_NOP));
            break;
        }

        /*
         * This is a let declaration. We must be directly under a block per the
         * proposed ES4 specs, but not an implicit block created due to
         * 'for (let ...)'. If we pass this error test, make the enclosing
         * StmtInfo be our scope. Further let declarations in this block will
         * find this scope statement and use the same block object.
         *
         * If we are the first let declaration in this block (i.e., when the
         * enclosing maybe-scope StmtInfo isn't yet a scope statement) then
         * we also need to set tc->blockNode to be our TOK_LEXICALSCOPE.
         */
        StmtInfo *stmt = tc->topStmt;
        if (stmt &&
            (!STMT_MAYBE_SCOPE(stmt) || (stmt->flags & SIF_FOR_BLOCK))) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_LET_DECL_NOT_IN_BLOCK);
            return NULL;
        }

        if (stmt && (stmt->flags & SIF_SCOPE)) {
            JS_ASSERT(tc->blockChain == stmt->blockObj);
        } else {
            if (!stmt || (stmt->flags & SIF_BODY_BLOCK)) {
                /*
                 * ES4 specifies that let at top level and at body-block scope
                 * does not shadow var, so convert back to var.
                 */
                pn = variables(PNK_VAR);
                if (!pn)
                    return NULL;
                pn->pn_xflags |= PNX_POPVAR;
                break;
            }

            /*
             * Some obvious assertions here, but they may help clarify the
             * situation. This stmt is not yet a scope, so it must not be a
             * catch block (catch is a lexical scope by definition).
             */
            JS_ASSERT(!(stmt->flags & SIF_SCOPE));
            JS_ASSERT(stmt != tc->topScopeStmt);
            JS_ASSERT(stmt->type == STMT_BLOCK ||
                      stmt->type == STMT_SWITCH ||
                      stmt->type == STMT_TRY ||
                      stmt->type == STMT_FINALLY);
            JS_ASSERT(!stmt->downScope);

            /* Convert the block statement into a scope statement. */
            StaticBlockObject *blockObj = StaticBlockObject::create(tc->parser->context);
            if (!blockObj)
                return NULL;

            ObjectBox *blockbox = tc->parser->newObjectBox(blockObj);
            if (!blockbox)
                return NULL;

            /*
             * Insert stmt on the tc->topScopeStmt/stmtInfo.downScope linked
             * list stack, if it isn't already there.  If it is there, but it
             * lacks the SIF_SCOPE flag, it must be a try, catch, or finally
             * block.
             */
            stmt->flags |= SIF_SCOPE;
            stmt->downScope = tc->topScopeStmt;
            tc->topScopeStmt = stmt;

            blockObj->setEnclosingBlock(tc->blockChain);
            tc->blockChain = blockObj;
            stmt->blockObj = blockObj;

#ifdef DEBUG
            ParseNode *tmp = tc->blockNode;
            JS_ASSERT(!tmp || !tmp->isKind(PNK_LEXICALSCOPE));
#endif

            /* Create a new lexical scope node for these statements. */
            ParseNode *pn1 = LexicalScopeNode::create(PNK_LEXICALSCOPE, tc);
            if (!pn1)
                return NULL;

            pn1->setOp(JSOP_LEAVEBLOCK);
            pn1->pn_pos = tc->blockNode->pn_pos;
            pn1->pn_objbox = blockbox;
            pn1->pn_expr = tc->blockNode;
            pn1->pn_blockid = tc->blockNode->pn_blockid;
            tc->blockNode = pn1;
        }

        pn = variables(PNK_LET, tc->blockChain, HoistVars);
        if (!pn)
            return NULL;
        pn->pn_xflags = PNX_POPVAR;
    } while (0);

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : NULL;
}
#endif

ParseNode *
Parser::expressionStatement()
{
    tokenStream.ungetToken();
    ParseNode *pn2 = expr();
    if (!pn2)
        return NULL;

    if (tokenStream.peekToken() == TOK_COLON) {
        if (!pn2->isKind(PNK_NAME)) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_LABEL);
            return NULL;
        }
        JSAtom *label = pn2->pn_atom;
        for (StmtInfo *stmt = tc->topStmt; stmt; stmt = stmt->down) {
            if (stmt->type == STMT_LABEL && stmt->label == label) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_DUPLICATE_LABEL);
                return NULL;
            }
        }
        ForgetUse(pn2);

        (void) tokenStream.getToken();

        /* Push a label struct and parse the statement. */
        StmtInfo stmtInfo;
        PushStatement(tc, &stmtInfo, STMT_LABEL, -1);
        stmtInfo.label = label;
        ParseNode *pn = statement();
        if (!pn)
            return NULL;

        /* Normalize empty statement to empty block for the decompiler. */
        if (pn->isKind(PNK_SEMI) && !pn->pn_kid) {
            pn->setKind(PNK_STATEMENTLIST);
            pn->setArity(PN_LIST);
            pn->makeEmpty();
        }

        /* Pop the label, set pn_expr, and return early. */
        PopStatement(tc);
        pn2->setKind(PNK_COLON);
        pn2->pn_pos.end = pn->pn_pos.end;
        pn2->pn_expr = pn;
        return pn2;
    }

    ParseNode *pn = UnaryNode::create(PNK_SEMI, tc);
    if (!pn)
        return NULL;
    pn->pn_pos = pn2->pn_pos;
    pn->pn_kid = pn2;

    switch (pn2->getKind()) {
      case PNK_LP:
        /*
         * Flag lambdas immediately applied as statements as instances of
         * the JS "module pattern". See CheckForImmediatelyAppliedLambda.
         */
        if (pn2->pn_head->isKind(PNK_FUNCTION) &&
            !pn2->pn_head->pn_funbox->node->isFunArg()) {
            pn2->pn_head->pn_funbox->tcflags |= TCF_FUN_MODULE_PATTERN;
        }
        break;
      case PNK_ASSIGN:
        /*
         * Keep track of all apparent methods created by assignments such
         * as this.foo = function (...) {...} in a function that could end
         * up a constructor function. See Parser::setFunctionKinds.
         */
        JS_ASSERT(pn2->isOp(JSOP_NOP));
        if (tc->funbox &&
            pn2->pn_left->isOp(JSOP_SETPROP) &&
            pn2->pn_left->pn_expr->isKind(PNK_THIS) &&
            pn2->pn_right->isOp(JSOP_LAMBDA))
        {
            JS_ASSERT(!pn2->isDefn());
            JS_ASSERT(!pn2->isUsed());
            pn2->pn_right->pn_link = tc->funbox->methods;
            tc->funbox->methods = pn2->pn_right;
        }
        break;
      default:;
    }

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : NULL;
}

ParseNode *
Parser::statement()
{
    ParseNode *pn;

    JS_CHECK_RECURSION(context, return NULL);

    switch (tokenStream.getToken(TSF_OPERAND)) {
      case TOK_FUNCTION:
      {
#if JS_HAS_XML_SUPPORT
        if (!tc->inStrictMode()) {
            TokenKind tt = tokenStream.peekToken(TSF_KEYWORD_IS_NAME);
            if (tt == TOK_DBLCOLON)
                return expressionStatement();
        }
#endif
        return functionStmt();
      }

      case TOK_IF:
      {
        /* An IF node has three kids: condition, then, and optional else. */
        pn = TernaryNode::create(PNK_IF, tc);
        if (!pn)
            return NULL;
        ParseNode *pn1 = condition();
        if (!pn1)
            return NULL;

        StmtInfo stmtInfo;
        PushStatement(tc, &stmtInfo, STMT_IF, -1);
        ParseNode *pn2 = statement();
        if (!pn2)
            return NULL;

        if (pn2->isKind(PNK_SEMI) &&
            !pn2->pn_kid &&
            !reportErrorNumber(NULL, JSREPORT_WARNING | JSREPORT_STRICT, JSMSG_EMPTY_CONSEQUENT))
        {
            return NULL;
        }

        ParseNode *pn3;
        if (tokenStream.matchToken(TOK_ELSE, TSF_OPERAND)) {
            stmtInfo.type = STMT_ELSE;
            pn3 = statement();
            if (!pn3)
                return NULL;
            pn->pn_pos.end = pn3->pn_pos.end;
        } else {
            pn3 = NULL;
            pn->pn_pos.end = pn2->pn_pos.end;
        }
        PopStatement(tc);
        pn->pn_kid1 = pn1;
        pn->pn_kid2 = pn2;
        pn->pn_kid3 = pn3;
        return pn;
      }

      case TOK_SWITCH:
        return switchStatement();

      case TOK_WHILE:
      {
        pn = BinaryNode::create(PNK_WHILE, tc);
        if (!pn)
            return NULL;
        StmtInfo stmtInfo;
        PushStatement(tc, &stmtInfo, STMT_WHILE_LOOP, -1);
        ParseNode *pn2 = condition();
        if (!pn2)
            return NULL;
        pn->pn_left = pn2;
        ParseNode *pn3 = statement();
        if (!pn3)
            return NULL;
        PopStatement(tc);
        pn->pn_pos.end = pn3->pn_pos.end;
        pn->pn_right = pn3;
        return pn;
      }

      case TOK_DO:
      {
        pn = BinaryNode::create(PNK_DOWHILE, tc);
        if (!pn)
            return NULL;
        StmtInfo stmtInfo;
        PushStatement(tc, &stmtInfo, STMT_DO_LOOP, -1);
        ParseNode *pn2 = statement();
        if (!pn2)
            return NULL;
        pn->pn_left = pn2;
        MUST_MATCH_TOKEN(TOK_WHILE, JSMSG_WHILE_AFTER_DO);
        ParseNode *pn3 = condition();
        if (!pn3)
            return NULL;
        PopStatement(tc);
        pn->pn_pos.end = pn3->pn_pos.end;
        pn->pn_right = pn3;
        if (versionNumber() != JSVERSION_ECMA_3) {
            /*
             * All legacy and extended versions must do automatic semicolon
             * insertion after do-while.  See the testcase and discussion in
             * http://bugzilla.mozilla.org/show_bug.cgi?id=238945.
             */
            (void) tokenStream.matchToken(TOK_SEMI);
            return pn;
        }
        break;
      }

      case TOK_FOR:
        return forStatement();

      case TOK_TRY:
        return tryStatement();

      case TOK_THROW:
      {
        pn = UnaryNode::create(PNK_THROW, tc);
        if (!pn)
            return NULL;

        /* ECMA-262 Edition 3 says 'throw [no LineTerminator here] Expr'. */
        TokenKind tt = tokenStream.peekTokenSameLine(TSF_OPERAND);
        if (tt == TOK_ERROR)
            return NULL;
        if (tt == TOK_EOF || tt == TOK_EOL || tt == TOK_SEMI || tt == TOK_RC) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
            return NULL;
        }

        ParseNode *pn2 = expr();
        if (!pn2)
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->setOp(JSOP_THROW);
        pn->pn_kid = pn2;
        break;
      }

      /* TOK_CATCH and TOK_FINALLY are both handled in the TOK_TRY case */
      case TOK_CATCH:
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_CATCH_WITHOUT_TRY);
        return NULL;

      case TOK_FINALLY:
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_FINALLY_WITHOUT_TRY);
        return NULL;

      case TOK_BREAK:
      {
        TokenPtr begin = tokenStream.currentToken().pos.begin;
        PropertyName *label;
        if (!MatchLabel(context, &tokenStream, &label))
            return NULL;
        TokenPtr end = tokenStream.currentToken().pos.end;
        pn = new_<BreakStatement>(label, begin, end);
        if (!pn)
            return NULL;
        StmtInfo *stmt = tc->topStmt;
        if (label) {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_LABEL_NOT_FOUND);
                    return NULL;
                }
                if (stmt->type == STMT_LABEL && stmt->label == label)
                    break;
            }
        } else {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_TOUGH_BREAK);
                    return NULL;
                }
                if (STMT_IS_LOOP(stmt) || stmt->type == STMT_SWITCH)
                    break;
            }
        }
        break;
      }

      case TOK_CONTINUE:
      {
        TokenPtr begin = tokenStream.currentToken().pos.begin;
        PropertyName *label;
        if (!MatchLabel(context, &tokenStream, &label))
            return NULL;
        TokenPtr end = tokenStream.currentToken().pos.begin;
        pn = new_<ContinueStatement>(label, begin, end);
        if (!pn)
            return NULL;
        StmtInfo *stmt = tc->topStmt;
        if (label) {
            for (StmtInfo *stmt2 = NULL; ; stmt = stmt->down) {
                if (!stmt) {
                    reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_LABEL_NOT_FOUND);
                    return NULL;
                }
                if (stmt->type == STMT_LABEL) {
                    if (stmt->label == label) {
                        if (!stmt2 || !STMT_IS_LOOP(stmt2)) {
                            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_CONTINUE);
                            return NULL;
                        }
                        break;
                    }
                } else {
                    stmt2 = stmt;
                }
            }
        } else {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_CONTINUE);
                    return NULL;
                }
                if (STMT_IS_LOOP(stmt))
                    break;
            }
        }
        break;
      }

      case TOK_WITH:
        return withStatement();

      case TOK_VAR:
        pn = variables(PNK_VAR);
        if (!pn)
            return NULL;

        /* Tell js_EmitTree to generate a final POP. */
        pn->pn_xflags |= PNX_POPVAR;
        break;

      case TOK_CONST:
        pn = variables(PNK_CONST);
        if (!pn)
            return NULL;

        /* Tell js_EmitTree to generate a final POP. */
        pn->pn_xflags |= PNX_POPVAR;
        break;

#if JS_HAS_BLOCK_SCOPE
      case TOK_LET:
        return letStatement();
#endif /* JS_HAS_BLOCK_SCOPE */

      case TOK_RETURN:
        pn = returnOrYield(false);
        if (!pn)
            return NULL;
        break;

      case TOK_LC:
      {
        uintN oldflags;

        oldflags = tc->flags;
        tc->flags = oldflags & ~TCF_HAS_FUNCTION_STMT;
        StmtInfo stmtInfo;
        if (!PushBlocklikeStatement(&stmtInfo, STMT_BLOCK, tc))
            return NULL;
        pn = statements();
        if (!pn)
            return NULL;

        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_IN_COMPOUND);
        PopStatement(tc);

        /*
         * If we contain a function statement and our container is top-level
         * or another block, flag pn to preserve braces when decompiling.
         */
        if ((tc->flags & TCF_HAS_FUNCTION_STMT) &&
            (!tc->topStmt || tc->topStmt->type == STMT_BLOCK)) {
            pn->pn_xflags |= PNX_NEEDBRACES;
        }
        tc->flags = oldflags | (tc->flags & (TCF_FUN_FLAGS | TCF_RETURN_FLAGS));
        return pn;
      }

      case TOK_SEMI:
        pn = UnaryNode::create(PNK_SEMI, tc);
        if (!pn)
            return NULL;
        return pn;

      case TOK_DEBUGGER:
        pn = tc->parser->new_<DebuggerStatement>(tokenStream.currentToken().pos);
        if (!pn)
            return NULL;
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
        break;

#if JS_HAS_XML_SUPPORT
      case TOK_DEFAULT:
      {
        if (tc->inStrictMode())
            return expressionStatement();

        pn = UnaryNode::create(PNK_DEFXMLNS, tc);
        if (!pn)
            return NULL;
        if (!tokenStream.matchToken(TOK_NAME) ||
            tokenStream.currentToken().name() != context->runtime->atomState.xmlAtom ||
            !tokenStream.matchToken(TOK_NAME) ||
            tokenStream.currentToken().name() != context->runtime->atomState.namespaceAtom ||
            !tokenStream.matchToken(TOK_ASSIGN))
        {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_DEFAULT_XML_NAMESPACE);
            return NULL;
        }

        JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NOP);

        /* Is this an E4X dagger I see before me? */
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
        ParseNode *pn2 = expr();
        if (!pn2)
            return NULL;
        pn->setOp(JSOP_DEFXMLNS);
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->pn_kid = pn2;
        break;
      }
#endif

      case TOK_ERROR:
        return NULL;

      default:
        return expressionStatement();
    }

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : NULL;
}

/*
 * The 'blockObj' parameter is non-null when parsing the 'vars' in a let
 * expression, block statement, non-top-level let declaration in statement
 * context, and the let-initializer of a for-statement.
 */
ParseNode *
Parser::variables(ParseNodeKind kind, StaticBlockObject *blockObj, VarContext varContext)
{
    /*
     * The four options here are:
     * - PNK_VAR:   We're parsing var declarations.
     * - PNK_CONST: We're parsing const declarations.
     * - PNK_LET:   We are parsing a let declaration.
     * - PNK_LP:    We are parsing the head of a let block.
     */
    JS_ASSERT(kind == PNK_VAR || kind == PNK_CONST || kind == PNK_LET || kind == PNK_LP);

    ParseNode *pn = ListNode::create(kind, tc);
    if (!pn)
        return NULL;

    pn->setOp(blockObj ? JSOP_NOP : kind == PNK_VAR ? JSOP_DEFVAR : JSOP_DEFCONST);
    pn->makeEmpty();

    /*
     * SpiderMonkey const is really "write once per initialization evaluation"
     * var, whereas let is block scoped. ES-Harmony wants block-scoped const so
     * this code will change soon.
     */
    BindData data;
    if (blockObj)
        data.initLet(varContext, *blockObj, JSMSG_TOO_MANY_LOCALS);
    else
        data.initVarOrConst(pn->getOp());

    ParseNode *pn2;
    do {
        TokenKind tt = tokenStream.getToken();
#if JS_HAS_DESTRUCTURING
        if (tt == TOK_LB || tt == TOK_LC) {
            tc->flags |= TCF_DECL_DESTRUCTURING;
            pn2 = primaryExpr(tt, JS_FALSE);
            tc->flags &= ~TCF_DECL_DESTRUCTURING;
            if (!pn2)
                return NULL;

            if (!CheckDestructuring(context, &data, pn2, tc))
                return NULL;
            if ((tc->flags & TCF_IN_FOR_INIT) && tokenStream.peekToken() == TOK_IN) {
                pn->append(pn2);
                continue;
            }

            MUST_MATCH_TOKEN(TOK_ASSIGN, JSMSG_BAD_DESTRUCT_DECL);
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NOP);

            ParseNode *init = assignExpr();
            if (!init)
                return NULL;
            UndominateInitializers(pn2, init->pn_pos.end, tc);

            pn2 = ParseNode::newBinaryOrAppend(PNK_ASSIGN, JSOP_NOP, pn2, init, tc);
            if (!pn2)
                return NULL;
            pn->append(pn2);
            continue;
        }
#endif /* JS_HAS_DESTRUCTURING */

        if (tt != TOK_NAME) {
            if (tt != TOK_ERROR)
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_NO_VARIABLE_NAME);
            return NULL;
        }

        PropertyName *name = tokenStream.currentToken().name();
        pn2 = NewBindingNode(name, tc, blockObj, varContext);
        if (!pn2)
            return NULL;
        if (data.op == JSOP_DEFCONST)
            pn2->pn_dflags |= PND_CONST;
        data.pn = pn2;
        if (!data.binder(context, &data, name, tc))
            return NULL;
        pn->append(pn2);

        if (tokenStream.matchToken(TOK_ASSIGN)) {
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NOP);

            ParseNode *init = assignExpr();
            if (!init)
                return NULL;

            if (pn2->isUsed()) {
                pn2 = MakeAssignment(pn2, init, tc);
                if (!pn2)
                    return NULL;
            } else {
                pn2->pn_expr = init;
            }

            JS_ASSERT_IF(pn2->pn_dflags & PND_GVAR, !(pn2->pn_dflags & PND_BOUND));

            pn2->setOp(pn2->isOp(JSOP_ARGUMENTS)
                       ? JSOP_SETNAME
                       : (pn2->pn_dflags & PND_BOUND)
                       ? JSOP_SETLOCAL
                       : (data.op == JSOP_DEFCONST)
                       ? JSOP_SETCONST
                       : JSOP_SETNAME);

            NoteLValue(context, pn2, tc, data.fresh ? PND_INITIALIZED : PND_ASSIGNED);

            /* The declarator's position must include the initializer. */
            pn2->pn_pos.end = init->pn_pos.end;

            if (tc->inFunction() && name == context->runtime->atomState.argumentsAtom) {
                tc->noteArgumentsUse(pn2);
                if (!blockObj)
                    tc->flags |= TCF_FUN_HEAVYWEIGHT;
            }
        }
    } while (tokenStream.matchToken(TOK_COMMA));

    pn->pn_pos.end = pn->last()->pn_pos.end;
    return pn;
}

ParseNode *
Parser::expr()
{
    ParseNode *pn = assignExpr();
    if (pn && tokenStream.matchToken(TOK_COMMA)) {
        ParseNode *pn2 = ListNode::create(PNK_COMMA, tc);
        if (!pn2)
            return NULL;
        pn2->pn_pos.begin = pn->pn_pos.begin;
        pn2->initList(pn);
        pn = pn2;
        do {
#if JS_HAS_GENERATORS
            pn2 = pn->last();
            if (pn2->isKind(PNK_YIELD) && !pn2->isInParens()) {
                reportErrorNumber(pn2, JSREPORT_ERROR, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
                return NULL;
            }
#endif
            pn2 = assignExpr();
            if (!pn2)
                return NULL;
            pn->append(pn2);
        } while (tokenStream.matchToken(TOK_COMMA));
        pn->pn_pos.end = pn->last()->pn_pos.end;
    }
    return pn;
}

/*
 * For a number of the expression parsers we define an always-inlined version
 * and a never-inlined version (which just calls the always-inlined version).
 * Using the always-inlined version in the hot call-sites givs a ~5% parsing
 * speedup.  These macros help avoid some boilerplate code.
 */
#define BEGIN_EXPR_PARSER(name)                                               \
    JS_ALWAYS_INLINE ParseNode *                                              \
    Parser::name##i()

#define END_EXPR_PARSER(name)                                                 \
    JS_NEVER_INLINE ParseNode *                                               \
    Parser::name##n() {                                                       \
        return name##i();                                                     \
    }

BEGIN_EXPR_PARSER(mulExpr1)
{
    ParseNode *pn = unaryExpr();

    /*
     * Note: unlike addExpr1() et al, we use getToken() here instead of
     * isCurrentTokenType() because unaryExpr() doesn't leave the TokenStream
     * state one past the end of the unary expression.
     */
    TokenKind tt;
    while (pn && ((tt = tokenStream.getToken()) == TOK_STAR || tt == TOK_DIV || tt == TOK_MOD)) {
        ParseNodeKind kind = (tt == TOK_STAR)
                             ? PNK_STAR
                             : (tt == TOK_DIV)
                             ? PNK_DIV
                             : PNK_MOD;
        JSOp op = tokenStream.currentToken().t_op;
        pn = ParseNode::newBinaryOrAppend(kind, op, pn, unaryExpr(), tc);
    }
    return pn;
}
END_EXPR_PARSER(mulExpr1)

BEGIN_EXPR_PARSER(addExpr1)
{
    ParseNode *pn = mulExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_PLUS, TOK_MINUS)) {
        TokenKind tt = tokenStream.currentToken().type;
        JSOp op = (tt == TOK_PLUS) ? JSOP_ADD : JSOP_SUB;
        ParseNodeKind kind = (tt == TOK_PLUS) ? PNK_ADD : PNK_SUB;
        pn = ParseNode::newBinaryOrAppend(kind, op, pn, mulExpr1n(), tc);
    }
    return pn;
}
END_EXPR_PARSER(addExpr1)

inline ParseNodeKind
ShiftTokenToParseNodeKind(const Token &token)
{
    switch (token.type) {
      case TOK_LSH:
        return PNK_LSH;
      case TOK_RSH:
        return PNK_RSH;
      default:
        JS_ASSERT(token.type == TOK_URSH);
        return PNK_URSH;
    }
}

BEGIN_EXPR_PARSER(shiftExpr1)
{
    ParseNode *left = addExpr1i();
    while (left && tokenStream.isCurrentTokenShift()) {
        ParseNodeKind kind = ShiftTokenToParseNodeKind(tokenStream.currentToken());
        JSOp op = tokenStream.currentToken().t_op;
        ParseNode *right = addExpr1n();
        if (!right)
            return NULL;
        left = tc->parser->new_<BinaryNode>(kind, op, left, right);
    }
    return left;
}
END_EXPR_PARSER(shiftExpr1)

inline ParseNodeKind
RelationalTokenToParseNodeKind(const Token &token)
{
    switch (token.type) {
      case TOK_IN:
        return PNK_IN;
      case TOK_INSTANCEOF:
        return PNK_INSTANCEOF;
      case TOK_LT:
        return PNK_LT;
      case TOK_LE:
        return PNK_LE;
      case TOK_GT:
        return PNK_GT;
      default:
        JS_ASSERT(token.type == TOK_GE);
        return PNK_GE;
    }
}

BEGIN_EXPR_PARSER(relExpr1)
{
    uintN inForInitFlag = tc->flags & TCF_IN_FOR_INIT;

    /*
     * Uses of the in operator in shiftExprs are always unambiguous,
     * so unset the flag that prohibits recognizing it.
     */
    tc->flags &= ~TCF_IN_FOR_INIT;

    ParseNode *pn = shiftExpr1i();
    while (pn &&
           (tokenStream.isCurrentTokenRelational() ||
            /*
             * Recognize the 'in' token as an operator only if we're not
             * currently in the init expr of a for loop.
             */
            (inForInitFlag == 0 && tokenStream.isCurrentTokenType(TOK_IN)) ||
            tokenStream.isCurrentTokenType(TOK_INSTANCEOF))) {
        ParseNodeKind kind = RelationalTokenToParseNodeKind(tokenStream.currentToken());
        JSOp op = tokenStream.currentToken().t_op;
        pn = ParseNode::newBinaryOrAppend(kind, op, pn, shiftExpr1n(), tc);
    }
    /* Restore previous state of inForInit flag. */
    tc->flags |= inForInitFlag;

    return pn;
}
END_EXPR_PARSER(relExpr1)

inline ParseNodeKind
EqualityTokenToParseNodeKind(const Token &token)
{
    switch (token.type) {
      case TOK_STRICTEQ:
        return PNK_STRICTEQ;
      case TOK_EQ:
        return PNK_EQ;
      case TOK_STRICTNE:
        return PNK_STRICTNE;
      default:
        JS_ASSERT(token.type == TOK_NE);
        return PNK_NE;
    }
}

BEGIN_EXPR_PARSER(eqExpr1)
{
    ParseNode *left = relExpr1i();
    while (left && tokenStream.isCurrentTokenEquality()) {
        ParseNodeKind kind = EqualityTokenToParseNodeKind(tokenStream.currentToken());
        JSOp op = tokenStream.currentToken().t_op;
        ParseNode *right = relExpr1n();
        if (!right)
            return NULL;
        left = tc->parser->new_<BinaryNode>(kind, op, left, right);
    }
    return left;
}
END_EXPR_PARSER(eqExpr1)

BEGIN_EXPR_PARSER(bitAndExpr1)
{
    ParseNode *pn = eqExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_BITAND))
        pn = ParseNode::newBinaryOrAppend(PNK_BITAND, JSOP_BITAND, pn, eqExpr1n(), tc);
    return pn;
}
END_EXPR_PARSER(bitAndExpr1)

BEGIN_EXPR_PARSER(bitXorExpr1)
{
    ParseNode *pn = bitAndExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_BITXOR))
        pn = ParseNode::newBinaryOrAppend(PNK_BITXOR, JSOP_BITXOR, pn, bitAndExpr1n(), tc);
    return pn;
}
END_EXPR_PARSER(bitXorExpr1)

BEGIN_EXPR_PARSER(bitOrExpr1)
{
    ParseNode *pn = bitXorExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_BITOR))
        pn = ParseNode::newBinaryOrAppend(PNK_BITOR, JSOP_BITOR, pn, bitXorExpr1n(), tc);
    return pn;
}
END_EXPR_PARSER(bitOrExpr1)

BEGIN_EXPR_PARSER(andExpr1)
{
    ParseNode *pn = bitOrExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_AND))
        pn = ParseNode::newBinaryOrAppend(PNK_AND, JSOP_AND, pn, bitOrExpr1n(), tc);
    return pn;
}
END_EXPR_PARSER(andExpr1)

JS_ALWAYS_INLINE ParseNode *
Parser::orExpr1()
{
    ParseNode *pn = andExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_OR))
        pn = ParseNode::newBinaryOrAppend(PNK_OR, JSOP_OR, pn, andExpr1n(), tc);
    return pn;
}

JS_ALWAYS_INLINE ParseNode *
Parser::condExpr1()
{
    ParseNode *condition = orExpr1();
    if (!condition || !tokenStream.isCurrentTokenType(TOK_HOOK))
        return condition;

    /*
     * Always accept the 'in' operator in the middle clause of a ternary,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    uintN oldflags = tc->flags;
    tc->flags &= ~TCF_IN_FOR_INIT;
    ParseNode *thenExpr = assignExpr();
    tc->flags = oldflags | (tc->flags & TCF_FUN_FLAGS);
    if (!thenExpr)
        return NULL;

    MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_IN_COND);

    ParseNode *elseExpr = assignExpr();
    if (!elseExpr)
        return NULL;

    tokenStream.getToken(); /* read one token past the end */
    return new_<ConditionalExpression>(condition, thenExpr, elseExpr);
}

bool
Parser::setAssignmentLhsOps(ParseNode *pn, JSOp op)
{
    switch (pn->getKind()) {
      case PNK_NAME:
        if (!CheckStrictAssignment(context, tc, pn))
            return false;
        pn->setOp(pn->isOp(JSOP_GETLOCAL) ? JSOP_SETLOCAL : JSOP_SETNAME);
        NoteLValue(context, pn, tc);
        break;
      case PNK_DOT:
        pn->setOp(JSOP_SETPROP);
        break;
      case PNK_LB:
        pn->setOp(JSOP_SETELEM);
        break;
#if JS_HAS_DESTRUCTURING
      case PNK_RB:
      case PNK_RC:
        if (op != JSOP_NOP) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_DESTRUCT_ASS);
            return false;
        }
        if (!CheckDestructuring(context, NULL, pn, tc))
            return false;
        break;
#endif
      case PNK_LP:
        if (!MakeSetCall(context, pn, tc, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return false;
        break;
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(pn->isOp(JSOP_XMLNAME));
        pn->setOp(JSOP_SETXMLNAME);
        break;
#endif
      default:
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_LEFTSIDE_OF_ASS);
        return false;
    }
    return true;
}

ParseNode *
Parser::assignExpr()
{
    JS_CHECK_RECURSION(context, return NULL);

#if JS_HAS_GENERATORS
    if (tokenStream.matchToken(TOK_YIELD, TSF_OPERAND))
        return returnOrYield(true);
#endif

    ParseNode *lhs = condExpr1();
    if (!lhs)
        return NULL;

    ParseNodeKind kind;
    switch (tokenStream.currentToken().type) {
      case TOK_ASSIGN:       kind = PNK_ASSIGN;       break;
      case TOK_ADDASSIGN:    kind = PNK_ADDASSIGN;    break;
      case TOK_SUBASSIGN:    kind = PNK_SUBASSIGN;    break;
      case TOK_BITORASSIGN:  kind = PNK_BITORASSIGN;  break;
      case TOK_BITXORASSIGN: kind = PNK_BITXORASSIGN; break;
      case TOK_BITANDASSIGN: kind = PNK_BITANDASSIGN; break;
      case TOK_LSHASSIGN:    kind = PNK_LSHASSIGN;    break;
      case TOK_RSHASSIGN:    kind = PNK_RSHASSIGN;    break;
      case TOK_URSHASSIGN:   kind = PNK_URSHASSIGN;   break;
      case TOK_MULASSIGN:    kind = PNK_MULASSIGN;    break;
      case TOK_DIVASSIGN:    kind = PNK_DIVASSIGN;    break;
      case TOK_MODASSIGN:    kind = PNK_MODASSIGN;    break;
      default:
        JS_ASSERT(!tokenStream.isCurrentTokenAssignment());
        tokenStream.ungetToken();
        return lhs;
    }

    JSOp op = tokenStream.currentToken().t_op;
    if (!setAssignmentLhsOps(lhs, op))
        return NULL;

    ParseNode *rhs = assignExpr();
    if (!rhs)
        return NULL;
    if (lhs->isKind(PNK_NAME) && lhs->isUsed()) {
        Definition *dn = lhs->pn_lexdef;

        /*
         * If the definition is not flagged as assigned, we must have imputed
         * the initialized flag to it, to optimize for flat closures. But that
         * optimization uses source coordinates to check dominance relations,
         * so we must extend the end of the definition to cover the right-hand
         * side of this assignment, i.e., the initializer.
         */
        if (!dn->isAssigned()) {
            JS_ASSERT(dn->isInitialized());
            dn->pn_pos.end = rhs->pn_pos.end;
        }
    }

    return ParseNode::newBinaryOrAppend(kind, op, lhs, rhs, tc);
}

static bool
SetLvalKid(JSContext *cx, TokenStream *ts, TreeContext *tc, ParseNode *pn, ParseNode *kid,
           const char *name)
{
    if (!kid->isKind(PNK_NAME) &&
        !kid->isKind(PNK_DOT) &&
        (!kid->isKind(PNK_LP) ||
         (!kid->isOp(JSOP_CALL) && !kid->isOp(JSOP_EVAL) &&
          !kid->isOp(JSOP_FUNCALL) && !kid->isOp(JSOP_FUNAPPLY))) &&
#if JS_HAS_XML_SUPPORT
        !kid->isKind(PNK_XMLUNARY) &&
#endif
        !kid->isKind(PNK_LB))
    {
        ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR, JSMSG_BAD_OPERAND, name);
        return false;
    }
    if (!CheckStrictAssignment(cx, tc, kid))
        return false;
    pn->pn_kid = kid;
    return true;
}

static const char incop_name_str[][10] = {"increment", "decrement"};

static JSBool
SetIncOpKid(JSContext *cx, TokenStream *ts, TreeContext *tc, ParseNode *pn, ParseNode *kid,
            TokenKind tt, bool preorder)
{
    JSOp op;

    if (!SetLvalKid(cx, ts, tc, pn, kid, incop_name_str[tt == TOK_DEC]))
        return false;
    switch (kid->getKind()) {
      case PNK_NAME:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCNAME : JSOP_NAMEINC)
             : (preorder ? JSOP_DECNAME : JSOP_NAMEDEC);
        NoteLValue(cx, kid, tc);
        break;

      case PNK_DOT:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCPROP : JSOP_PROPINC)
             : (preorder ? JSOP_DECPROP : JSOP_PROPDEC);
        break;

      case PNK_LP:
        if (!MakeSetCall(cx, kid, tc, JSMSG_BAD_INCOP_OPERAND))
            return JS_FALSE;
        /* FALL THROUGH */
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        if (kid->isOp(JSOP_XMLNAME))
            kid->setOp(JSOP_SETXMLNAME);
        /* FALL THROUGH */
#endif
      case PNK_LB:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCELEM : JSOP_ELEMINC)
             : (preorder ? JSOP_DECELEM : JSOP_ELEMDEC);
        break;

      default:
        JS_ASSERT(0);
        op = JSOP_NOP;
    }
    pn->setOp(op);
    return JS_TRUE;
}

ParseNode *
Parser::unaryOpExpr(ParseNodeKind kind, JSOp op)
{
    TokenPtr begin = tokenStream.currentToken().pos.begin;
    ParseNode *kid = unaryExpr();
    if (!kid)
        return NULL;
    return new_<UnaryNode>(kind, op, TokenPos::make(begin, kid->pn_pos.end), kid);
}

ParseNode *
Parser::unaryExpr()
{
    ParseNode *pn, *pn2;

    JS_CHECK_RECURSION(context, return NULL);

    switch (TokenKind tt = tokenStream.getToken(TSF_OPERAND)) {
      case TOK_TYPEOF:
        return unaryOpExpr(PNK_TYPEOF, JSOP_TYPEOF);
      case TOK_VOID:
        return unaryOpExpr(PNK_VOID, JSOP_VOID);
      case TOK_NOT:
        return unaryOpExpr(PNK_NOT, JSOP_NOT);
      case TOK_BITNOT:
        return unaryOpExpr(PNK_BITNOT, JSOP_BITNOT);
      case TOK_PLUS:
        return unaryOpExpr(PNK_POS, JSOP_POS);
      case TOK_MINUS:
        return unaryOpExpr(PNK_NEG, JSOP_NEG);

      case TOK_INC:
      case TOK_DEC:
        pn = UnaryNode::create((tt == TOK_INC) ? PNK_PREINCREMENT : PNK_PREDECREMENT, tc);
        if (!pn)
            return NULL;
        pn2 = memberExpr(JS_TRUE);
        if (!pn2)
            return NULL;
        if (!SetIncOpKid(context, &tokenStream, tc, pn, pn2, tt, true))
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;
        break;

      case TOK_DELETE:
      {
        pn = UnaryNode::create(PNK_DELETE, tc);
        if (!pn)
            return NULL;
        pn2 = unaryExpr();
        if (!pn2)
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;

        /*
         * Under ECMA3, deleting any unary expression is valid -- it simply
         * returns true. Here we fold constants before checking for a call
         * expression, in order to rule out delete of a generator expression.
         */
        if (foldConstants && !FoldConstants(context, pn2, tc))
            return NULL;
        switch (pn2->getKind()) {
          case PNK_LP:
            if (!(pn2->pn_xflags & PNX_SETCALL)) {
                /*
                 * Call MakeSetCall to check for errors, but clear PNX_SETCALL
                 * because the optimizer will eliminate the useless delete.
                 */
                if (!MakeSetCall(context, pn2, tc, JSMSG_BAD_DELETE_OPERAND))
                    return NULL;
                pn2->pn_xflags &= ~PNX_SETCALL;
            }
            break;
          case PNK_NAME:
            if (!ReportStrictModeError(context, &tokenStream, tc, pn,
                                       JSMSG_DEPRECATED_DELETE_OPERAND)) {
                return NULL;
            }
            pn2->setOp(JSOP_DELNAME);
            if (pn2->pn_atom == context->runtime->atomState.argumentsAtom) {
                tc->flags |= TCF_FUN_HEAVYWEIGHT;
                tc->countArgumentsUse(pn2);
            }
            break;
          default:;
        }
        pn->pn_kid = pn2;
        break;
      }
      case TOK_ERROR:
        return NULL;

      default:
        tokenStream.ungetToken();
        pn = memberExpr(JS_TRUE);
        if (!pn)
            return NULL;

        /* Don't look across a newline boundary for a postfix incop. */
        if (tokenStream.onCurrentLine(pn->pn_pos)) {
            tt = tokenStream.peekTokenSameLine(TSF_OPERAND);
            if (tt == TOK_INC || tt == TOK_DEC) {
                tokenStream.consumeKnownToken(tt);
                pn2 = UnaryNode::create((tt == TOK_INC) ? PNK_POSTINCREMENT : PNK_POSTDECREMENT, tc);
                if (!pn2)
                    return NULL;
                if (!SetIncOpKid(context, &tokenStream, tc, pn2, pn, tt, false))
                    return NULL;
                pn2->pn_pos.begin = pn->pn_pos.begin;
                pn = pn2;
            }
        }
        break;
    }
    return pn;
}

#if JS_HAS_GENERATORS

/*
 * A dedicated helper for transplanting the comprehension expression E in
 *
 *   [E for (V in I)]   // array comprehension
 *   (E for (V in I))   // generator expression
 *
 * from its initial location in the AST, on the left of the 'for', to its final
 * position on the right. To avoid a separate pass we do this by adjusting the
 * blockids and name binding links that were established when E was parsed.
 *
 * A generator expression desugars like so:
 *
 *   (E for (V in I)) => (function () { for (var V in I) yield E; })()
 *
 * so the transplanter must adjust static level as well as blockid. E's source
 * coordinates in root->pn_pos are critical to deciding which binding links to
 * preserve and which to cut.
 *
 * NB: This is not a general tree transplanter -- it knows in particular that
 * the one or more bindings induced by V have not yet been created.
 */
class CompExprTransplanter {
    ParseNode       *root;
    TreeContext     *tc;
    bool            genexp;
    uintN           adjust;
    uintN           funcLevel;

  public:
    CompExprTransplanter(ParseNode *pn, TreeContext *tc, bool ge, uintN adj)
      : root(pn), tc(tc), genexp(ge), adjust(adj), funcLevel(0)
    {
    }

    bool transplant(ParseNode *pn);
};

/*
 * A helper for lazily checking for the presence of illegal |yield| or |arguments|
 * tokens inside of generator expressions. This must be done lazily since we don't
 * know whether we're in a generator expression until we see the "for" token after
 * we've already parsed the body expression.
 *
 * Use in any context which may turn out to be inside a generator expression. This
 * includes parenthesized expressions and argument lists, and it includes the tail
 * of generator expressions.
 *
 * The guard will keep track of any |yield| or |arguments| tokens that occur while
 * parsing the body. As soon as the parser reaches the end of the body expression,
 * call endBody() to reset the context's state, and then immediately call:
 *
 * - checkValidBody() if this *did* turn out to be a generator expression
 * - maybeNoteGenerator() if this *did not* turn out to be a generator expression
 */
class GenexpGuard {
    TreeContext     *tc;
    uint32_t        startYieldCount;
    uint32_t        startArgumentsCount;

  public:
    explicit GenexpGuard(TreeContext *tc)
      : tc(tc)
    {
        if (tc->parenDepth == 0) {
            tc->yieldCount = tc->argumentsCount = 0;
            tc->yieldNode = tc->argumentsNode = NULL;
        }
        startYieldCount = tc->yieldCount;
        startArgumentsCount = tc->argumentsCount;
        tc->parenDepth++;
    }

    void endBody();
    bool checkValidBody(ParseNode *pn);
    bool maybeNoteGenerator(ParseNode *pn);
};

void
GenexpGuard::endBody()
{
    tc->parenDepth--;
}

/*
 * Check whether a |yield| or |arguments| token has been encountered in the
 * body expression, and if so, report an error.
 *
 * Call this after endBody() when determining that the body *was* in a
 * generator expression.
 */
bool
GenexpGuard::checkValidBody(ParseNode *pn)
{
    if (tc->yieldCount > startYieldCount) {
        ParseNode *errorNode = tc->yieldNode;
        if (!errorNode)
            errorNode = pn;
        tc->parser->reportErrorNumber(errorNode, JSREPORT_ERROR, JSMSG_BAD_GENEXP_BODY, js_yield_str);
        return false;
    }

    if (tc->argumentsCount > startArgumentsCount) {
        ParseNode *errorNode = tc->argumentsNode;
        if (!errorNode)
            errorNode = pn;
        tc->parser->reportErrorNumber(errorNode, JSREPORT_ERROR, JSMSG_BAD_GENEXP_BODY, js_arguments_str);
        return false;
    }

    return true;
}

/*
 * Check whether a |yield| token has been encountered in the body expression,
 * and if so, note that the current function is a generator function.
 *
 * Call this after endBody() when determining that the body *was not* in a
 * generator expression.
 */
bool
GenexpGuard::maybeNoteGenerator(ParseNode *pn)
{
    if (tc->yieldCount > 0) {
        tc->flags |= TCF_FUN_IS_GENERATOR;
        if (!tc->inFunction()) {
            tc->parser->reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_RETURN_OR_YIELD,
                                          js_yield_str);
            return false;
        }
        if (tc->flags & TCF_RETURN_EXPR) {
            /* At the time we saw the yield, we might not have set TCF_FUN_IS_GENERATOR yet. */
            ReportBadReturn(tc->parser->context, tc, pn, JSREPORT_ERROR,
                            JSMSG_BAD_GENERATOR_RETURN,
                            JSMSG_BAD_ANON_GENERATOR_RETURN);
            return false;
        }
    }
    return true;
}

/*
 * Any definitions nested within the comprehension expression of a generator
 * expression must move "down" one static level, which of course increases the
 * upvar-frame-skip count.
 */
static bool
BumpStaticLevel(ParseNode *pn, TreeContext *tc)
{
    if (!pn->pn_cookie.isFree()) {
        uintN level = pn->pn_cookie.level() + 1;

        JS_ASSERT(level >= tc->staticLevel);
        if (level >= UpvarCookie::FREE_LEVEL) {
            JS_ReportErrorNumber(tc->parser->context, js_GetErrorMessage, NULL,
                                 JSMSG_TOO_DEEP, js_function_str);
            return false;
        }

        pn->pn_cookie.set(level, pn->pn_cookie.slot());
    }
    return true;
}

static void
AdjustBlockId(ParseNode *pn, uintN adjust, TreeContext *tc)
{
    JS_ASSERT(pn->isArity(PN_LIST) || pn->isArity(PN_FUNC) || pn->isArity(PN_NAME));
    pn->pn_blockid += adjust;
    if (pn->pn_blockid >= tc->blockidGen)
        tc->blockidGen = pn->pn_blockid + 1;
}

bool
CompExprTransplanter::transplant(ParseNode *pn)
{
    if (!pn)
        return true;

    switch (pn->getArity()) {
      case PN_LIST:
        for (ParseNode *pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (!transplant(pn2))
                return false;
        }
        if (pn->pn_pos >= root->pn_pos)
            AdjustBlockId(pn, adjust, tc);
        break;

      case PN_TERNARY:
        if (!transplant(pn->pn_kid1) ||
            !transplant(pn->pn_kid2) ||
            !transplant(pn->pn_kid3))
            return false;
        break;

      case PN_BINARY:
        if (!transplant(pn->pn_left))
            return false;

        /* Binary TOK_COLON nodes can have left == right. See bug 492714. */
        if (pn->pn_right != pn->pn_left) {
            if (!transplant(pn->pn_right))
                return false;
        }
        break;

      case PN_UNARY:
        if (!transplant(pn->pn_kid))
            return false;
        break;

      case PN_FUNC:
      {
        /*
         * Only the first level of transplant recursion through functions needs
         * to reparent the funbox, since all descendant functions are correctly
         * linked under the top-most funbox. But every visit to this case needs
         * to update funbox->level.
         *
         * Recall that funbox->level is the static level of the code containing
         * the definition or expression of the function and not the static level
         * of the function's body.
         */
        FunctionBox *funbox = pn->pn_funbox;

        funbox->level = tc->staticLevel + funcLevel;
        if (++funcLevel == 1 && genexp) {
            FunctionBox *parent = tc->funbox;

            FunctionBox **funboxp = &tc->parent->functionList;
            while (*funboxp != funbox)
                funboxp = &(*funboxp)->siblings;
            *funboxp = funbox->siblings;

            funbox->parent = parent;
            funbox->siblings = parent->kids;
            parent->kids = funbox;
            funbox->level = tc->staticLevel;
        }
        /* FALL THROUGH */
      }

      case PN_NAME:
        if (!transplant(pn->maybeExpr()))
            return false;
        if (pn->isArity(PN_FUNC))
            --funcLevel;

        if (pn->isDefn()) {
            if (genexp && !BumpStaticLevel(pn, tc))
                return false;
        } else if (pn->isUsed()) {
            JS_ASSERT(!pn->isOp(JSOP_NOP));
            JS_ASSERT(pn->pn_cookie.isFree());

            Definition *dn = pn->pn_lexdef;
            JS_ASSERT(dn->isDefn());

            /*
             * Adjust the definition's block id only if it is a placeholder not
             * to the left of the root node, and if pn is the last use visited
             * in the comprehension expression (to avoid adjusting the blockid
             * multiple times).
             *
             * Non-placeholder definitions within the comprehension expression
             * will be visited further below.
             */
            if (dn->isPlaceholder() && dn->pn_pos >= root->pn_pos && dn->dn_uses == pn) {
                if (genexp && !BumpStaticLevel(dn, tc))
                    return false;
                AdjustBlockId(dn, adjust, tc);
            }

            JSAtom *atom = pn->pn_atom;
#ifdef DEBUG
            StmtInfo *stmt = LexicalLookup(tc, atom, NULL);
            JS_ASSERT(!stmt || stmt != tc->topStmt);
#endif
            if (genexp && !dn->isOp(JSOP_CALLEE)) {
                JS_ASSERT(!tc->decls.lookupFirst(atom));

                if (dn->pn_pos < root->pn_pos) {
                    /*
                     * The variable originally appeared to be a use of a
                     * definition or placeholder outside the generator, but now
                     * we know it is scoped within the comprehension tail's
                     * clauses. Make it (along with any other uses within the
                     * generator) a use of a new placeholder in the generator's
                     * lexdeps.
                     */
                    Definition *dn2 = MakePlaceholder(pn, tc);
                    if (!dn2)
                        return false;
                    dn2->pn_pos = root->pn_pos;

                    /*
                     * Change all uses of |dn| that lie within the generator's
                     * |yield| expression into uses of dn2.
                     */
                    ParseNode **pnup = &dn->dn_uses;
                    ParseNode *pnu;
                    while ((pnu = *pnup) != NULL && pnu->pn_pos >= root->pn_pos) {
                        pnu->pn_lexdef = dn2;
                        dn2->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
                        pnup = &pnu->pn_link;
                    }
                    dn2->dn_uses = dn->dn_uses;
                    dn->dn_uses = *pnup;
                    *pnup = NULL;
                    if (!tc->lexdeps->put(atom, dn2))
                        return false;
                } else if (dn->isPlaceholder()) {
                    /*
                     * The variable first occurs free in the 'yield' expression;
                     * move the existing placeholder node (and all its uses)
                     * from the parent's lexdeps into the generator's lexdeps.
                     */
                    tc->parent->lexdeps->remove(atom);
                    if (!tc->lexdeps->put(atom, dn))
                        return false;
                }
            }
        }

        if (pn->pn_pos >= root->pn_pos)
            AdjustBlockId(pn, adjust, tc);
        break;

      case PN_NAMESET:
        if (!transplant(pn->pn_tree))
            return false;
        break;

      case PN_NULLARY:
        /* Nothing. */
        break;
    }
    return true;
}

/*
 * Starting from a |for| keyword after the first array initialiser element or
 * an expression in an open parenthesis, parse the tail of the comprehension
 * or generator expression signified by this |for| keyword in context.
 *
 * Return null on failure, else return the top-most parse node for the array
 * comprehension or generator expression, with a unary node as the body of the
 * (possibly nested) for-loop, initialized by |kind, op, kid|.
 */
ParseNode *
Parser::comprehensionTail(ParseNode *kid, uintN blockid, bool isGenexp,
                          ParseNodeKind kind, JSOp op)
{
    uintN adjust;
    ParseNode *pn, *pn2, *pn3, **pnp;
    StmtInfo stmtInfo;
    BindData data;
    TokenKind tt;

    JS_ASSERT(tokenStream.currentToken().type == TOK_FOR);

    if (kind == PNK_SEMI) {
        /*
         * Generator expression desugars to an immediately applied lambda that
         * yields the next value from a for-in loop (possibly nested, and with
         * optional if guard). Make pn be the TOK_LC body node.
         */
        pn = PushLexicalScope(context, tc, &stmtInfo);
        if (!pn)
            return NULL;
        adjust = pn->pn_blockid - blockid;
    } else {
        JS_ASSERT(kind == PNK_ARRAYPUSH);

        /*
         * Make a parse-node and literal object representing the block scope of
         * this array comprehension. Our caller in primaryExpr, the TOK_LB case
         * aka the array initialiser case, has passed the blockid to claim for
         * the comprehension's block scope. We allocate that id or one above it
         * here, by calling PushLexicalScope.
         *
         * In the case of a comprehension expression that has nested blocks
         * (e.g., let expressions), we will allocate a higher blockid but then
         * slide all blocks "to the right" to make room for the comprehension's
         * block scope.
         */
        adjust = tc->blockid();
        pn = PushLexicalScope(context, tc, &stmtInfo);
        if (!pn)
            return NULL;

        JS_ASSERT(blockid <= pn->pn_blockid);
        JS_ASSERT(blockid < tc->blockidGen);
        JS_ASSERT(tc->bodyid < blockid);
        pn->pn_blockid = stmtInfo.blockid = blockid;
        JS_ASSERT(adjust < blockid);
        adjust = blockid - adjust;
    }

    pnp = &pn->pn_expr;

    CompExprTransplanter transplanter(kid, tc, kind == PNK_SEMI, adjust);
    transplanter.transplant(kid);

    JS_ASSERT(tc->blockChain && tc->blockChain == pn->pn_objbox->object);
    data.initLet(HoistVars, *tc->blockChain, JSMSG_ARRAY_INIT_TOO_BIG);

    do {
        /*
         * FOR node is binary, left is loop control and right is body.  Use
         * index to count each block-local let-variable on the left-hand side
         * of the IN.
         */
        pn2 = BinaryNode::create(PNK_FOR, tc);
        if (!pn2)
            return NULL;

        pn2->setOp(JSOP_ITER);
        pn2->pn_iflags = JSITER_ENUMERATE;
        if (tokenStream.matchToken(TOK_NAME)) {
            if (tokenStream.currentToken().name() == context->runtime->atomState.eachAtom)
                pn2->pn_iflags |= JSITER_FOREACH;
            else
                tokenStream.ungetToken();
        }
        MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

        GenexpGuard guard(tc);

        PropertyName *name = NULL;
        tt = tokenStream.getToken();
        switch (tt) {
#if JS_HAS_DESTRUCTURING
          case TOK_LB:
          case TOK_LC:
            tc->flags |= TCF_DECL_DESTRUCTURING;
            pn3 = primaryExpr(tt, JS_FALSE);
            tc->flags &= ~TCF_DECL_DESTRUCTURING;
            if (!pn3)
                return NULL;
            break;
#endif

          case TOK_NAME:
            name = tokenStream.currentToken().name();

            /*
             * Create a name node with pn_op JSOP_NAME.  We can't set pn_op to
             * JSOP_GETLOCAL here, because we don't yet know the block's depth
             * in the operand stack frame.  The code generator computes that,
             * and it tries to bind all names to slots, so we must let it do
             * the deed.
             */
            pn3 = NewBindingNode(name, tc);
            if (!pn3)
                return NULL;
            break;

          default:
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_NO_VARIABLE_NAME);

          case TOK_ERROR:
            return NULL;
        }

        MUST_MATCH_TOKEN(TOK_IN, JSMSG_IN_AFTER_FOR_NAME);
        ParseNode *pn4 = expr();
        if (!pn4)
            return NULL;
        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

        guard.endBody();

        if (isGenexp) {
            if (!guard.checkValidBody(pn2))
                return NULL;
        } else {
            if (!guard.maybeNoteGenerator(pn2))
                return NULL;
        }

        switch (tt) {
#if JS_HAS_DESTRUCTURING
          case TOK_LB:
          case TOK_LC:
            if (!CheckDestructuring(context, &data, pn3, tc))
                return NULL;

            if (versionNumber() == JSVERSION_1_7) {
                /* Destructuring requires [key, value] enumeration in JS1.7. */
                if (!pn3->isKind(PNK_RB) || pn3->pn_count != 2) {
                    reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_FOR_LEFTSIDE);
                    return NULL;
                }

                JS_ASSERT(pn2->isOp(JSOP_ITER));
                JS_ASSERT(pn2->pn_iflags & JSITER_ENUMERATE);
                if (!(pn2->pn_iflags & JSITER_FOREACH))
                    pn2->pn_iflags |= JSITER_FOREACH | JSITER_KEYVALUE;
            }
            break;
#endif

          case TOK_NAME:
            data.pn = pn3;
            if (!data.binder(context, &data, name, tc))
                return NULL;
            break;

          default:;
        }

        /*
         * Synthesize a declaration. Every definition must appear in the parse
         * tree in order for ComprehensionTranslator to work.
         */
        ParseNode *vars = ListNode::create(PNK_VAR, tc);
        if (!vars)
            return NULL;
        vars->setOp(JSOP_NOP);
        vars->pn_pos = pn3->pn_pos;
        vars->makeEmpty();
        vars->append(pn3);
        vars->pn_xflags |= PNX_FORINVAR;

        /* Definitions can't be passed directly to EmitAssignment as lhs. */
        pn3 = CloneLeftHandSide(pn3, tc);
        if (!pn3)
            return NULL;

        pn2->pn_left = new_<TernaryNode>(PNK_FORIN, JSOP_NOP, vars, pn3, pn4);
        if (!pn2->pn_left)
            return NULL;
        *pnp = pn2;
        pnp = &pn2->pn_right;
    } while (tokenStream.matchToken(TOK_FOR));

    if (tokenStream.matchToken(TOK_IF)) {
        pn2 = TernaryNode::create(PNK_IF, tc);
        if (!pn2)
            return NULL;
        pn2->pn_kid1 = condition();
        if (!pn2->pn_kid1)
            return NULL;
        *pnp = pn2;
        pnp = &pn2->pn_kid2;
    }

    pn2 = UnaryNode::create(kind, tc);
    if (!pn2)
        return NULL;
    pn2->setOp(op);
    pn2->pn_kid = kid;
    *pnp = pn2;

    PopStatement(tc);
    return pn;
}

#if JS_HAS_GENERATOR_EXPRS

/*
 * Starting from a |for| keyword after an expression, parse the comprehension
 * tail completing this generator expression. Wrap the expression at kid in a
 * generator function that is immediately called to evaluate to the generator
 * iterator that is the value of this generator expression.
 *
 * |kid| must be the expression before the |for| keyword; we return an
 * application of a generator function that includes the |for| loops and
 * |if| guards, with |kid| as the operand of a |yield| expression as the
 * innermost loop body.
 *
 * Note how unlike Python, we do not evaluate the expression to the right of
 * the first |in| in the chain of |for| heads. Instead, a generator expression
 * is merely sugar for a generator function expression and its application.
 */
ParseNode *
Parser::generatorExpr(ParseNode *kid)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    /* Create a |yield| node for |kid|. */
    ParseNode *pn = UnaryNode::create(PNK_YIELD, tc);
    if (!pn)
        return NULL;
    pn->setOp(JSOP_YIELD);
    pn->setInParens(true);
    pn->pn_pos = kid->pn_pos;
    pn->pn_kid = kid;
    pn->pn_hidden = true;

    /* Make a new node for the desugared generator function. */
    ParseNode *genfn = FunctionNode::create(PNK_FUNCTION, tc);
    if (!genfn)
        return NULL;
    genfn->setOp(JSOP_LAMBDA);
    JS_ASSERT(!genfn->pn_body);
    genfn->pn_dflags = PND_FUNARG;

    {
        TreeContext *outertc = tc;
        TreeContext gentc(tc->parser);
        if (!gentc.init(context))
            return NULL;

        FunctionBox *funbox = EnterFunction(genfn, &gentc);
        if (!funbox)
            return NULL;

        /*
         * We have to dance around a bit to propagate sharp variables from
         * outertc to gentc before setting TCF_HAS_SHARPS implicitly by
         * propagating all of outertc's TCF_FUN_FLAGS flags. As below, we have
         * to be conservative by leaving TCF_HAS_SHARPS set in outertc if we
         * do propagate to gentc.
         */
        if (outertc->flags & TCF_HAS_SHARPS) {
            gentc.flags |= TCF_IN_FUNCTION;
            if (!gentc.ensureSharpSlots())
                return NULL;
        }

        /*
         * We assume conservatively that any deoptimization flag in tc->flags
         * besides TCF_FUN_PARAM_ARGUMENTS can come from the kid. So we
         * propagate these flags into genfn. For code simplicity we also do
         * not detect if the flags were only set in the kid and could be
         * removed from tc->flags.
         */
        gentc.flags |= TCF_FUN_IS_GENERATOR | TCF_GENEXP_LAMBDA |
                       (outertc->flags & (TCF_FUN_FLAGS & ~TCF_FUN_PARAM_ARGUMENTS));
        funbox->tcflags |= gentc.flags;
        genfn->pn_funbox = funbox;
        genfn->pn_blockid = gentc.bodyid;

        ParseNode *body = comprehensionTail(pn, outertc->blockid(), true);
        if (!body)
            return NULL;
        JS_ASSERT(!genfn->pn_body);
        genfn->pn_body = body;
        genfn->pn_pos.begin = body->pn_pos.begin = kid->pn_pos.begin;
        genfn->pn_pos.end = body->pn_pos.end = tokenStream.currentToken().pos.end;

        if (!LeaveFunction(genfn, &gentc))
            return NULL;
    }

    /*
     * Our result is a call expression that invokes the anonymous generator
     * function object.
     */
    ParseNode *result = ListNode::create(PNK_LP, tc);
    if (!result)
        return NULL;
    result->setOp(JSOP_CALL);
    result->pn_pos.begin = genfn->pn_pos.begin;
    result->initList(genfn);
    return result;
}

static const char js_generator_str[] = "generator";

#endif /* JS_HAS_GENERATOR_EXPRS */
#endif /* JS_HAS_GENERATORS */

JSBool
Parser::argumentList(ParseNode *listNode)
{
    if (tokenStream.matchToken(TOK_RP, TSF_OPERAND))
        return JS_TRUE;

    GenexpGuard guard(tc);
    bool arg0 = true;

    do {
        ParseNode *argNode = assignExpr();
        if (!argNode)
            return JS_FALSE;
        if (arg0)
            guard.endBody();

#if JS_HAS_GENERATORS
        if (argNode->isKind(PNK_YIELD) &&
            !argNode->isInParens() &&
            tokenStream.peekToken() == TOK_COMMA) {
            reportErrorNumber(argNode, JSREPORT_ERROR, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
            return JS_FALSE;
        }
#endif
#if JS_HAS_GENERATOR_EXPRS
        if (tokenStream.matchToken(TOK_FOR)) {
            if (!guard.checkValidBody(argNode))
                return JS_FALSE;
            argNode = generatorExpr(argNode);
            if (!argNode)
                return JS_FALSE;
            if (listNode->pn_count > 1 ||
                tokenStream.peekToken() == TOK_COMMA) {
                reportErrorNumber(argNode, JSREPORT_ERROR, JSMSG_BAD_GENERATOR_SYNTAX,
                                  js_generator_str);
                return JS_FALSE;
            }
        } else
#endif
        if (arg0 && !guard.maybeNoteGenerator(argNode))
            return JS_FALSE;

        arg0 = false;

        listNode->append(argNode);
    } while (tokenStream.matchToken(TOK_COMMA));

    if (tokenStream.getToken() != TOK_RP) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_PAREN_AFTER_ARGS);
        return JS_FALSE;
    }
    return JS_TRUE;
}

/* Check for an immediately-applied (new'ed) lambda and clear PND_FUNARG. */
static ParseNode *
CheckForImmediatelyAppliedLambda(ParseNode *pn)
{
    if (pn->isKind(PNK_FUNCTION)) {
        JS_ASSERT(pn->isArity(PN_FUNC));

        FunctionBox *funbox = pn->pn_funbox;
        JS_ASSERT((funbox->function())->flags & JSFUN_LAMBDA);
        if (!(funbox->tcflags & (TCF_FUN_USES_ARGUMENTS | TCF_FUN_USES_OWN_NAME)))
            pn->pn_dflags &= ~PND_FUNARG;
    }
    return pn;
}

ParseNode *
Parser::memberExpr(JSBool allowCallSyntax)
{
    ParseNode *pn, *pn2, *pn3;

    JS_CHECK_RECURSION(context, return NULL);

    /* Check for new expression first. */
    TokenKind tt = tokenStream.getToken(TSF_OPERAND);
    if (tt == TOK_NEW) {
        pn = ListNode::create(PNK_NEW, tc);
        if (!pn)
            return NULL;
        pn2 = memberExpr(JS_FALSE);
        if (!pn2)
            return NULL;
        pn2 = CheckForImmediatelyAppliedLambda(pn2);
        pn->setOp(JSOP_NEW);
        pn->initList(pn2);
        pn->pn_pos.begin = pn2->pn_pos.begin;

        if (tokenStream.matchToken(TOK_LP) && !argumentList(pn))
            return NULL;
        if (pn->pn_count > ARGC_LIMIT) {
            JS_ReportErrorNumber(context, js_GetErrorMessage, NULL,
                                 JSMSG_TOO_MANY_CON_ARGS);
            return NULL;
        }
        pn->pn_pos.end = pn->last()->pn_pos.end;
    } else {
        pn = primaryExpr(tt, JS_FALSE);
        if (!pn)
            return NULL;

        if (pn->isXMLNameOp()) {
            pn = new_<UnaryNode>(PNK_XMLUNARY, JSOP_XMLNAME, pn->pn_pos, pn);
            if (!pn)
                return NULL;
        }
    }

    while ((tt = tokenStream.getToken()) > TOK_EOF) {
        if (tt == TOK_DOT) {
            pn2 = NameNode::create(PNK_DOT, NULL, tc);
            if (!pn2)
                return NULL;

#if JS_HAS_XML_SUPPORT
            tt = tokenStream.getToken(TSF_OPERAND | TSF_KEYWORD_IS_NAME);

            /* Treat filters as 'with' statements for name deoptimization. */
            ParseNode *oldWith = tc->innermostWith;
            StmtInfo stmtInfo;
            if (tt == TOK_LP) {
                if (tc->inStrictMode()) {
                    reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_NAME_AFTER_DOT);
                    return NULL;
                }

                tc->innermostWith = pn;
                PushStatement(tc, &stmtInfo, STMT_WITH, -1);
            }

            pn3 = primaryExpr(tt, JS_TRUE);
            if (!pn3)
                return NULL;

            if (tt == TOK_LP) {
                tc->innermostWith = oldWith;
                PopStatement(tc);
            }

            /* Check both tt and pn_type, to distinguish |x.(y)| and |x.y::z| from |x.y|. */
            if (tt == TOK_NAME && pn3->isKind(PNK_NAME)) {
                pn2->setOp(JSOP_GETPROP);
                pn2->pn_expr = pn;
                pn2->pn_atom = pn3->pn_atom;
                freeTree(pn3);
            } else {
                if (tt == TOK_LP) {
                    pn2->setKind(PNK_FILTER);
                    pn2->setOp(JSOP_FILTER);

                    /* A filtering predicate is like a with statement. */
                    tc->flags |= TCF_FUN_HEAVYWEIGHT;
                } else if (pn3->isXMLPropertyIdentifier()) {
                    JS_ASSERT(!tc->inStrictMode());
                    pn2->setKind(PNK_LB);
                    pn2->setOp(JSOP_GETELEM);
                } else {
                    reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_NAME_AFTER_DOT);
                    return NULL;
                }
                pn2->setArity(PN_BINARY);
                pn2->pn_left = pn;
                pn2->pn_right = pn3;
            }
#else
            MUST_MATCH_TOKEN_WITH_FLAGS(TOK_NAME, JSMSG_NAME_AFTER_DOT, TSF_KEYWORD_IS_NAME);
            pn2->setOp(JSOP_GETPROP);
            pn2->pn_expr = pn;
            pn2->pn_atom = tokenStream.currentToken().name();
#endif
            pn2->pn_pos.begin = pn->pn_pos.begin;
            pn2->pn_pos.end = tokenStream.currentToken().pos.end;
#if JS_HAS_XML_SUPPORT
        } else if (tt == TOK_DBLDOT) {
            if (tc->inStrictMode()) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_NAME_AFTER_DOT);
                return NULL;
            }

            pn2 = BinaryNode::create(PNK_DBLDOT, tc);
            if (!pn2)
                return NULL;
            tt = tokenStream.getToken(TSF_OPERAND | TSF_KEYWORD_IS_NAME);
            pn3 = primaryExpr(tt, JS_TRUE);
            if (!pn3)
                return NULL;
            if (pn3->isKind(PNK_NAME) && !pn3->isInParens()) {
                pn3->setKind(PNK_STRING);
                pn3->setArity(PN_NULLARY);
                pn3->setOp(JSOP_QNAMEPART);
            } else if (!pn3->isXMLPropertyIdentifier()) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_NAME_AFTER_DOT);
                return NULL;
            }
            pn2->setOp(JSOP_DESCENDANTS);
            pn2->pn_left = pn;
            pn2->pn_right = pn3;
            pn2->pn_pos.begin = pn->pn_pos.begin;
            pn2->pn_pos.end = tokenStream.currentToken().pos.end;
#endif
        } else if (tt == TOK_LB) {
            pn2 = BinaryNode::create(PNK_LB, tc);
            if (!pn2)
                return NULL;
            pn3 = expr();
            if (!pn3)
                return NULL;

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);
            pn2->pn_pos.begin = pn->pn_pos.begin;
            pn2->pn_pos.end = tokenStream.currentToken().pos.end;

            /*
             * Optimize o['p'] to o.p by rewriting pn2, but avoid rewriting
             * o['0'] to use JSOP_GETPROP, to keep fast indexing disjoint in
             * the interpreter from fast property access. However, if the
             * bracketed string is a uint32, we rewrite pn3 to be a number
             * instead of a string.
             */
            do {
                if (pn3->isKind(PNK_STRING)) {
                    jsuint index;

                    if (!js_IdIsIndex(ATOM_TO_JSID(pn3->pn_atom), &index)) {
                        pn2->setKind(PNK_DOT);
                        pn2->setOp(JSOP_GETPROP);
                        pn2->setArity(PN_NAME);
                        pn2->pn_expr = pn;
                        pn2->pn_atom = pn3->pn_atom;
                        break;
                    }
                    pn3->setKind(PNK_NUMBER);
                    pn3->setOp(JSOP_DOUBLE);
                    pn3->pn_dval = index;
                }
                pn2->setOp(JSOP_GETELEM);
                pn2->pn_left = pn;
                pn2->pn_right = pn3;
            } while (0);
        } else if (allowCallSyntax && tt == TOK_LP) {
            pn2 = ListNode::create(PNK_LP, tc);
            if (!pn2)
                return NULL;
            pn2->setOp(JSOP_CALL);

            pn = CheckForImmediatelyAppliedLambda(pn);
            if (pn->isOp(JSOP_NAME)) {
                if (pn->pn_atom == context->runtime->atomState.evalAtom) {
                    /* Select JSOP_EVAL and flag tc as heavyweight. */
                    pn2->setOp(JSOP_EVAL);
                    tc->noteCallsEval();
                    tc->flags |= TCF_FUN_HEAVYWEIGHT;
                    /*
                     * In non-strict mode code, direct calls to eval can add
                     * variables to the call object.
                     */
                    if (!tc->inStrictMode())
                        tc->noteHasExtensibleScope();
                }
            } else if (pn->isOp(JSOP_GETPROP)) {
                /* Select JSOP_FUNAPPLY given foo.apply(...). */
                if (pn->pn_atom == context->runtime->atomState.applyAtom)
                    pn2->setOp(JSOP_FUNAPPLY);
                else if (pn->pn_atom == context->runtime->atomState.callAtom)
                    pn2->setOp(JSOP_FUNCALL);
            }

            pn2->initList(pn);
            pn2->pn_pos.begin = pn->pn_pos.begin;

            if (!argumentList(pn2))
                return NULL;
            if (pn2->pn_count > ARGC_LIMIT) {
                JS_ReportErrorNumber(context, js_GetErrorMessage, NULL,
                                     JSMSG_TOO_MANY_FUN_ARGS);
                return NULL;
            }
            pn2->pn_pos.end = tokenStream.currentToken().pos.end;
        } else {
            tokenStream.ungetToken();
            return pn;
        }

        pn = pn2;
    }
    if (tt == TOK_ERROR)
        return NULL;
    return pn;
}

ParseNode *
Parser::bracketedExpr()
{
    uintN oldflags;
    ParseNode *pn;

    /*
     * Always accept the 'in' operator in a parenthesized expression,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    oldflags = tc->flags;
    tc->flags &= ~TCF_IN_FOR_INIT;
    pn = expr();
    tc->flags = oldflags | (tc->flags & TCF_FUN_FLAGS);
    return pn;
}

#if JS_HAS_XML_SUPPORT

ParseNode *
Parser::endBracketedExpr()
{
    JS_ASSERT(!tc->inStrictMode());

    ParseNode *pn = bracketedExpr();
    if (!pn)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_ATTR_EXPR);
    return pn;
}

/*
 * From the ECMA-357 grammar in 11.1.1 and 11.1.2:
 *
 *      AttributeIdentifier:
 *              @ PropertySelector
 *              @ QualifiedIdentifier
 *              @ [ Expression ]
 *
 *      PropertySelector:
 *              Identifier
 *              *
 *
 *      QualifiedIdentifier:
 *              PropertySelector :: PropertySelector
 *              PropertySelector :: [ Expression ]
 *
 * We adapt AttributeIdentifier and QualifiedIdentier to be LL(1), like so:
 *
 *      AttributeIdentifier:
 *              @ QualifiedIdentifier
 *              @ [ Expression ]
 *
 *      PropertySelector:
 *              Identifier
 *              *
 *
 *      QualifiedIdentifier:
 *              PropertySelector :: PropertySelector
 *              PropertySelector :: [ Expression ]
 *              PropertySelector
 *
 * As PrimaryExpression: Identifier is in ECMA-262 and we want the semantics
 * for that rule to result in a name node, but ECMA-357 extends the grammar
 * to include PrimaryExpression: QualifiedIdentifier, we must factor further:
 *
 *      QualifiedIdentifier:
 *              PropertySelector QualifiedSuffix
 *
 *      QualifiedSuffix:
 *              :: PropertySelector
 *              :: [ Expression ]
 *              /nothing/
 *
 * And use this production instead of PrimaryExpression: QualifiedIdentifier:
 *
 *      PrimaryExpression:
 *              Identifier QualifiedSuffix
 *
 * We hoist the :: match into callers of QualifiedSuffix, in order to tweak
 * PropertySelector vs. Identifier pn_arity, pn_op, and other members.
 */
ParseNode *
Parser::propertySelector()
{
    JS_ASSERT(!tc->inStrictMode());

    ParseNode *selector;
    if (tokenStream.isCurrentTokenType(TOK_STAR)) {
        selector = NullaryNode::create(PNK_ANYNAME, tc);
        if (!selector)
            return NULL;
        selector->setOp(JSOP_ANYNAME);
        selector->pn_atom = context->runtime->atomState.starAtom;
    } else {
        JS_ASSERT(tokenStream.isCurrentTokenType(TOK_NAME));
        selector = NullaryNode::create(PNK_NAME, tc);
        if (!selector)
            return NULL;
        selector->setOp(JSOP_QNAMEPART);
        selector->setArity(PN_NAME);
        selector->pn_atom = tokenStream.currentToken().name();
        selector->pn_cookie.makeFree();
    }
    return selector;
}

ParseNode *
Parser::qualifiedSuffix(ParseNode *pn)
{
    JS_ASSERT(!tc->inStrictMode());

    JS_ASSERT(tokenStream.currentToken().type == TOK_DBLCOLON);
    ParseNode *pn2 = NameNode::create(PNK_DBLCOLON, NULL, tc);
    if (!pn2)
        return NULL;

    /* Left operand of :: must be evaluated if it is an identifier. */
    if (pn->isOp(JSOP_QNAMEPART))
        pn->setOp(JSOP_NAME);

    TokenKind tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
    if (tt == TOK_STAR || tt == TOK_NAME) {
        /* Inline and specialize propertySelector for JSOP_QNAMECONST. */
        pn2->setOp(JSOP_QNAMECONST);
        pn2->pn_pos.begin = pn->pn_pos.begin;
        pn2->pn_atom = (tt == TOK_STAR)
                       ? context->runtime->atomState.starAtom
                       : tokenStream.currentToken().name();
        pn2->pn_expr = pn;
        pn2->pn_cookie.makeFree();
        return pn2;
    }

    if (tt != TOK_LB) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
        return NULL;
    }
    ParseNode *pn3 = endBracketedExpr();
    if (!pn3)
        return NULL;

    pn2->setOp(JSOP_QNAME);
    pn2->setArity(PN_BINARY);
    pn2->pn_pos.begin = pn->pn_pos.begin;
    pn2->pn_pos.end = pn3->pn_pos.end;
    pn2->pn_left = pn;
    pn2->pn_right = pn3;
    return pn2;
}

ParseNode *
Parser::qualifiedIdentifier()
{
    JS_ASSERT(!tc->inStrictMode());

    ParseNode *pn = propertySelector();
    if (!pn)
        return NULL;
    if (tokenStream.matchToken(TOK_DBLCOLON)) {
        /* Hack for bug 496316. Slowing down E4X won't make it go away, alas. */
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
        pn = qualifiedSuffix(pn);
    }
    return pn;
}

ParseNode *
Parser::attributeIdentifier()
{
    JS_ASSERT(!tc->inStrictMode());

    JS_ASSERT(tokenStream.currentToken().type == TOK_AT);
    ParseNode *pn = UnaryNode::create(PNK_AT, tc);
    if (!pn)
        return NULL;
    pn->setOp(JSOP_TOATTRNAME);

    ParseNode *pn2;
    TokenKind tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
    if (tt == TOK_STAR || tt == TOK_NAME) {
        pn2 = qualifiedIdentifier();
    } else if (tt == TOK_LB) {
        pn2 = endBracketedExpr();
    } else {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
        return NULL;
    }
    if (!pn2)
        return NULL;
    pn->pn_kid = pn2;
    return pn;
}

/*
 * Make a TOK_LC unary node whose pn_kid is an expression.
 */
ParseNode *
Parser::xmlExpr(JSBool inTag)
{
    JS_ASSERT(!tc->inStrictMode());

    JS_ASSERT(tokenStream.currentToken().type == TOK_LC);
    ParseNode *pn = UnaryNode::create(PNK_XMLCURLYEXPR, tc);
    if (!pn)
        return NULL;

    /*
     * Turn off XML tag mode. We save the old value of the flag because it may
     * already be off: XMLExpr is called both from within a tag, and from
     * within text contained in an element, but outside of any start, end, or
     * point tag.
     */
    bool oldflag = tokenStream.isXMLTagMode();
    tokenStream.setXMLTagMode(false);
    ParseNode *pn2 = expr();
    if (!pn2)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_IN_XML_EXPR);
    tokenStream.setXMLTagMode(oldflag);
    pn->pn_kid = pn2;
    pn->setOp(inTag ? JSOP_XMLTAGEXPR : JSOP_XMLELTEXPR);
    return pn;
}

ParseNode *
Parser::atomNode(ParseNodeKind kind, JSOp op)
{
    ParseNode *node = NullaryNode::create(kind, tc);
    if (!node)
        return NULL;
    node->setOp(op);
    const Token &tok = tokenStream.currentToken();
    node->pn_atom = tok.atom();
    return node;
}

/*
 * Parse the productions:
 *
 *      XMLNameExpr:
 *              XMLName XMLNameExpr?
 *              { Expr } XMLNameExpr?
 *
 * Return a PN_LIST, PN_UNARY, or PN_NULLARY according as XMLNameExpr produces
 * a list of names and/or expressions, a single expression, or a single name.
 * If PN_LIST or PN_NULLARY, getKind() will be PNK_XMLNAME.  Otherwise if
 * PN_UNARY, getKind() will be PNK_XMLCURLYEXPR.
 */
ParseNode *
Parser::xmlNameExpr()
{
    JS_ASSERT(!tc->inStrictMode());

    ParseNode *pn, *pn2, *list;
    TokenKind tt;

    pn = list = NULL;
    do {
        tt = tokenStream.currentToken().type;
        if (tt == TOK_LC) {
            pn2 = xmlExpr(JS_TRUE);
            if (!pn2)
                return NULL;
        } else {
            JS_ASSERT(tt == TOK_XMLNAME);
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_STRING);
            pn2 = atomNode(PNK_XMLNAME, JSOP_STRING);
            if (!pn2)
                return NULL;
        }

        if (!pn) {
            pn = pn2;
        } else {
            if (!list) {
                list = ListNode::create(PNK_XMLNAME, tc);
                if (!list)
                    return NULL;
                list->pn_pos.begin = pn->pn_pos.begin;
                list->initList(pn);
                list->pn_xflags = PNX_CANTFOLD;
                pn = list;
            }
            pn->pn_pos.end = pn2->pn_pos.end;
            pn->append(pn2);
        }
    } while ((tt = tokenStream.getToken()) == TOK_XMLNAME || tt == TOK_LC);

    tokenStream.ungetToken();
    return pn;
}

/*
 * Macro to test whether an XMLNameExpr or XMLTagContent node can be folded
 * at compile time into a JSXML tree.
 */
#define XML_FOLDABLE(pn)        ((pn)->isArity(PN_LIST)                     \
                                 ? ((pn)->pn_xflags & PNX_CANTFOLD) == 0    \
                                 : !(pn)->isKind(PNK_XMLCURLYEXPR))

/*
 * Parse the productions:
 *
 *      XMLTagContent:
 *              XMLNameExpr
 *              XMLTagContent S XMLNameExpr S? = S? XMLAttr
 *              XMLTagContent S XMLNameExpr S? = S? { Expr }
 *
 * Return a PN_LIST, PN_UNARY, or PN_NULLARY according to how XMLTagContent
 * produces a list of name and attribute values and/or braced expressions, a
 * single expression, or a single name.
 *
 * If PN_LIST or PN_NULLARY, getKind() will be PNK_XMLNAME for the case where
 * XMLTagContent: XMLNameExpr.  If getKind() is not PNK_XMLNAME but getArity()
 * is PN_LIST, getKind() will be tagkind.  If PN_UNARY, getKind() will be
 * PNK_XMLCURLYEXPR and we parsed exactly one expression.
 */
ParseNode *
Parser::xmlTagContent(ParseNodeKind tagkind, JSAtom **namep)
{
    JS_ASSERT(!tc->inStrictMode());

    ParseNode *pn, *pn2, *list;
    TokenKind tt;

    pn = xmlNameExpr();
    if (!pn)
        return NULL;
    *namep = (pn->isArity(PN_NULLARY)) ? pn->pn_atom : NULL;
    list = NULL;

    while (tokenStream.matchToken(TOK_XMLSPACE)) {
        tt = tokenStream.getToken();
        if (tt != TOK_XMLNAME && tt != TOK_LC) {
            tokenStream.ungetToken();
            break;
        }

        pn2 = xmlNameExpr();
        if (!pn2)
            return NULL;
        if (!list) {
            list = ListNode::create(tagkind, tc);
            if (!list)
                return NULL;
            list->pn_pos.begin = pn->pn_pos.begin;
            list->initList(pn);
            pn = list;
        }
        pn->append(pn2);
        if (!XML_FOLDABLE(pn2))
            pn->pn_xflags |= PNX_CANTFOLD;

        tokenStream.matchToken(TOK_XMLSPACE);
        MUST_MATCH_TOKEN(TOK_ASSIGN, JSMSG_NO_ASSIGN_IN_XML_ATTR);
        tokenStream.matchToken(TOK_XMLSPACE);

        tt = tokenStream.getToken();
        if (tt == TOK_XMLATTR) {
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_STRING);
            pn2 = atomNode(PNK_XMLATTR, JSOP_STRING);
        } else if (tt == TOK_LC) {
            pn2 = xmlExpr(JS_TRUE);
            pn->pn_xflags |= PNX_CANTFOLD;
        } else {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_XML_ATTR_VALUE);
            return NULL;
        }
        if (!pn2)
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->append(pn2);
    }

    return pn;
}

#define XML_CHECK_FOR_ERROR_AND_EOF(tt,result)                                              \
    JS_BEGIN_MACRO                                                                          \
        if ((tt) <= TOK_EOF) {                                                              \
            if ((tt) == TOK_EOF) {                                                          \
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_END_OF_XML_SOURCE);           \
            }                                                                               \
            return result;                                                                  \
        }                                                                                   \
    JS_END_MACRO

/*
 * Consume XML element tag content, including the TOK_XMLETAGO (</) sequence
 * that opens the end tag for the container.
 */
JSBool
Parser::xmlElementContent(ParseNode *pn)
{
    JS_ASSERT(!tc->inStrictMode());

    tokenStream.setXMLTagMode(false);
    for (;;) {
        TokenKind tt = tokenStream.getToken(TSF_XMLTEXTMODE);
        XML_CHECK_FOR_ERROR_AND_EOF(tt, JS_FALSE);

        JS_ASSERT(tt == TOK_XMLSPACE || tt == TOK_XMLTEXT);
        JSAtom *textAtom = tokenStream.currentToken().atom();
        if (textAtom) {
            /* Non-zero-length XML text scanned. */
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_STRING);
            ParseNode *pn2 = atomNode(tt == TOK_XMLSPACE ? PNK_XMLSPACE : PNK_XMLTEXT,
                                      JSOP_STRING);
            if (!pn2)
                return false;
            pn->pn_pos.end = pn2->pn_pos.end;
            pn->append(pn2);
        }

        tt = tokenStream.getToken(TSF_OPERAND);
        XML_CHECK_FOR_ERROR_AND_EOF(tt, JS_FALSE);
        if (tt == TOK_XMLETAGO)
            break;

        ParseNode *pn2;
        if (tt == TOK_LC) {
            pn2 = xmlExpr(JS_FALSE);
            if (!pn2)
                return false;
            pn->pn_xflags |= PNX_CANTFOLD;
        } else if (tt == TOK_XMLSTAGO) {
            pn2 = xmlElementOrList(JS_FALSE);
            if (!pn2)
                return false;
            pn2->pn_xflags &= ~PNX_XMLROOT;
            pn->pn_xflags |= pn2->pn_xflags;
        } else if (tt == TOK_XMLPI) {
            const Token &tok = tokenStream.currentToken();
            pn2 = new_<XMLProcessingInstruction>(tok.xmlPITarget(), tok.xmlPIData(), tok.pos);
            if (!pn2)
                return false;
        } else {
            JS_ASSERT(tt == TOK_XMLCDATA || tt == TOK_XMLCOMMENT);
            pn2 = atomNode(tt == TOK_XMLCDATA ? PNK_XMLCDATA : PNK_XMLCOMMENT,
                           tokenStream.currentToken().t_op);
            if (!pn2)
                return false;
        }
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->append(pn2);
    }
    tokenStream.setXMLTagMode(true);

    JS_ASSERT(tokenStream.currentToken().type == TOK_XMLETAGO);
    return JS_TRUE;
}

/*
 * Return a PN_LIST node containing an XML or XMLList Initialiser.
 */
ParseNode *
Parser::xmlElementOrList(JSBool allowList)
{
    JS_ASSERT(!tc->inStrictMode());

    ParseNode *pn, *pn2, *list;
    TokenKind tt;
    JSAtom *startAtom, *endAtom;

    JS_CHECK_RECURSION(context, return NULL);

    JS_ASSERT(tokenStream.currentToken().type == TOK_XMLSTAGO);
    pn = ListNode::create(PNK_XMLSTAGO, tc);
    if (!pn)
        return NULL;

    tokenStream.setXMLTagMode(true);
    tt = tokenStream.getToken();
    if (tt == TOK_ERROR)
        return NULL;

    if (tt == TOK_XMLNAME || tt == TOK_LC) {
        /*
         * XMLElement.  Append the tag and its contents, if any, to pn.
         */
        pn2 = xmlTagContent(PNK_XMLSTAGO, &startAtom);
        if (!pn2)
            return NULL;
        tokenStream.matchToken(TOK_XMLSPACE);

        tt = tokenStream.getToken();
        if (tt == TOK_XMLPTAGC) {
            /* Point tag (/>): recycle pn if pn2 is a list of tag contents. */
            if (pn2->isKind(PNK_XMLSTAGO)) {
                pn->makeEmpty();
                freeTree(pn);
                pn = pn2;
            } else {
                JS_ASSERT(pn2->isKind(PNK_XMLNAME) || pn2->isKind(PNK_XMLCURLYEXPR));
                pn->initList(pn2);
                if (!XML_FOLDABLE(pn2))
                    pn->pn_xflags |= PNX_CANTFOLD;
            }
            pn->setKind(PNK_XMLPTAGC);
            pn->pn_xflags |= PNX_XMLROOT;
        } else {
            /* We had better have a tag-close (>) at this point. */
            if (tt != TOK_XMLTAGC) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_XML_TAG_SYNTAX);
                return NULL;
            }
            pn2->pn_pos.end = tokenStream.currentToken().pos.end;

            /* Make sure pn2 is a TOK_XMLSTAGO list containing tag contents. */
            if (!pn2->isKind(PNK_XMLSTAGO)) {
                pn->initList(pn2);
                if (!XML_FOLDABLE(pn2))
                    pn->pn_xflags |= PNX_CANTFOLD;
                pn2 = pn;
                pn = ListNode::create(PNK_XMLTAGC, tc);
                if (!pn)
                    return NULL;
            }

            /* Now make pn a nominal-root TOK_XMLELEM list containing pn2. */
            pn->setKind(PNK_XMLELEM);
            pn->pn_pos.begin = pn2->pn_pos.begin;
            pn->initList(pn2);
            if (!XML_FOLDABLE(pn2))
                pn->pn_xflags |= PNX_CANTFOLD;
            pn->pn_xflags |= PNX_XMLROOT;

            /* Get element contents and delimiting end-tag-open sequence. */
            if (!xmlElementContent(pn))
                return NULL;

            tt = tokenStream.getToken();
            XML_CHECK_FOR_ERROR_AND_EOF(tt, NULL);
            if (tt != TOK_XMLNAME && tt != TOK_LC) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_XML_TAG_SYNTAX);
                return NULL;
            }

            /* Parse end tag; check mismatch at compile-time if we can. */
            pn2 = xmlTagContent(PNK_XMLETAGO, &endAtom);
            if (!pn2)
                return NULL;
            if (pn2->isKind(PNK_XMLETAGO)) {
                /* Oops, end tag has attributes! */
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_XML_TAG_SYNTAX);
                return NULL;
            }
            if (endAtom && startAtom && endAtom != startAtom) {
                /* End vs. start tag name mismatch: point to the tag name. */
                reportErrorNumber(pn2, JSREPORT_UC | JSREPORT_ERROR, JSMSG_XML_TAG_NAME_MISMATCH,
                                  startAtom->chars());
                return NULL;
            }

            /* Make a TOK_XMLETAGO list with pn2 as its single child. */
            JS_ASSERT(pn2->isKind(PNK_XMLNAME) || pn2->isKind(PNK_XMLCURLYEXPR));
            list = ListNode::create(PNK_XMLETAGO, tc);
            if (!list)
                return NULL;
            list->initList(pn2);
            pn->append(list);
            if (!XML_FOLDABLE(pn2)) {
                list->pn_xflags |= PNX_CANTFOLD;
                pn->pn_xflags |= PNX_CANTFOLD;
            }

            tokenStream.matchToken(TOK_XMLSPACE);
            MUST_MATCH_TOKEN(TOK_XMLTAGC, JSMSG_BAD_XML_TAG_SYNTAX);
        }

        /* Set pn_op now that pn has been updated to its final value. */
        pn->setOp(JSOP_TOXML);
    } else if (allowList && tt == TOK_XMLTAGC) {
        /* XMLList Initialiser. */
        pn->setKind(PNK_XMLLIST);
        pn->setOp(JSOP_TOXMLLIST);
        pn->makeEmpty();
        pn->pn_xflags |= PNX_XMLROOT;
        if (!xmlElementContent(pn))
            return NULL;

        MUST_MATCH_TOKEN(TOK_XMLTAGC, JSMSG_BAD_XML_LIST_SYNTAX);
    } else {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_XML_NAME_SYNTAX);
        return NULL;
    }
    tokenStream.setXMLTagMode(false);

    pn->pn_pos.end = tokenStream.currentToken().pos.end;
    return pn;
}

ParseNode *
Parser::xmlElementOrListRoot(JSBool allowList)
{
    JS_ASSERT(!tc->inStrictMode());

    /*
     * Force XML support to be enabled so that comments and CDATA literals
     * are recognized, instead of <! followed by -- starting an HTML comment
     * to end of line (used in script tags to hide content from old browsers
     * that don't recognize <script>).
     */
    bool hadXML = tokenStream.hasXML();
    tokenStream.setXML(true);
    ParseNode *pn = xmlElementOrList(allowList);
    tokenStream.setXML(hadXML);
    return pn;
}

ParseNode *
Parser::parseXMLText(JSObject *chain, bool allowList)
{
    /*
     * Push a compiler frame if we have no frames, or if the top frame is a
     * lightweight function activation, or if its scope chain doesn't match
     * the one passed to us.
     */
    TreeContext xmltc(this);
    if (!xmltc.init(context))
        return NULL;
    JS_ASSERT(!xmltc.inStrictMode());
    xmltc.setScopeChain(chain);

    /* Set XML-only mode to turn off special treatment of {expr} in XML. */
    tokenStream.setXMLOnlyMode();
    TokenKind tt = tokenStream.getToken(TSF_OPERAND);

    ParseNode *pn;
    if (tt != TOK_XMLSTAGO) {
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_XML_MARKUP);
        pn = NULL;
    } else {
        pn = xmlElementOrListRoot(allowList);
    }
    tokenStream.setXMLOnlyMode(false);

    return pn;
}

#endif /* JS_HAS_XMLSUPPORT */

ParseNode *
Parser::primaryExpr(TokenKind tt, JSBool afterDot)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    ParseNode *pn, *pn2, *pn3;
    JSOp op;

    JS_CHECK_RECURSION(context, return NULL);

    switch (tt) {
      case TOK_FUNCTION:
#if JS_HAS_XML_SUPPORT
        if (!tc->inStrictMode() && tokenStream.matchToken(TOK_DBLCOLON, TSF_KEYWORD_IS_NAME)) {
            pn2 = NullaryNode::create(PNK_FUNCTION, tc);
            if (!pn2)
                return NULL;
            pn = qualifiedSuffix(pn2);
            if (!pn)
                return NULL;
            break;
        }
#endif
        pn = functionExpr();
        if (!pn)
            return NULL;
        break;

      case TOK_LB:
      {
        JSBool matched;
        jsuint index;

        pn = ListNode::create(PNK_RB, tc);
        if (!pn)
            return NULL;
        pn->setOp(JSOP_NEWINIT);
        pn->makeEmpty();

#if JS_HAS_GENERATORS
        pn->pn_blockid = tc->blockidGen;
#endif

        matched = tokenStream.matchToken(TOK_RB, TSF_OPERAND);
        if (!matched) {
            for (index = 0; ; index++) {
                if (index == StackSpace::ARGS_LENGTH_MAX) {
                    reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_ARRAY_INIT_TOO_BIG);
                    return NULL;
                }

                tt = tokenStream.peekToken(TSF_OPERAND);
                if (tt == TOK_RB) {
                    pn->pn_xflags |= PNX_ENDCOMMA;
                    break;
                }

                if (tt == TOK_COMMA) {
                    /* So CURRENT_TOKEN gets TOK_COMMA and not TOK_LB. */
                    tokenStream.matchToken(TOK_COMMA);
                    pn2 = NullaryNode::create(PNK_COMMA, tc);
                    pn->pn_xflags |= PNX_HOLEY | PNX_NONCONST;
                } else {
                    pn2 = assignExpr();
                    if (pn2 && !pn2->isConstant())
                        pn->pn_xflags |= PNX_NONCONST;
                }
                if (!pn2)
                    return NULL;
                pn->append(pn2);

                if (tt != TOK_COMMA) {
                    /* If we didn't already match TOK_COMMA in above case. */
                    if (!tokenStream.matchToken(TOK_COMMA))
                        break;
                }
            }

#if JS_HAS_GENERATORS
            /*
             * At this point, (index == 0 && pn->pn_count != 0) implies one
             * element initialiser was parsed.
             *
             * An array comprehension of the form:
             *
             *   [i * j for (i in o) for (j in p) if (i != j)]
             *
             * translates to roughly the following let expression:
             *
             *   let (array = new Array, i, j) {
             *     for (i in o) let {
             *       for (j in p)
             *         if (i != j)
             *           array.push(i * j)
             *     }
             *     array
             *   }
             *
             * where array is a nameless block-local variable. The "roughly"
             * means that an implementation may optimize away the array.push.
             * An array comprehension opens exactly one block scope, no matter
             * how many for heads it contains.
             *
             * Each let () {...} or for (let ...) ... compiles to:
             *
             *   JSOP_ENTERBLOCK <o> ... JSOP_LEAVEBLOCK <n>
             *
             * where <o> is a literal object representing the block scope,
             * with <n> properties, naming each var declared in the block.
             *
             * Each var declaration in a let-block binds a name in <o> at
             * compile time, and allocates a slot on the operand stack at
             * runtime via JSOP_ENTERBLOCK. A block-local var is accessed by
             * the JSOP_GETLOCAL and JSOP_SETLOCAL ops. These ops have an
             * immediate operand, the local slot's stack index from fp->spbase.
             *
             * The array comprehension iteration step, array.push(i * j) in
             * the example above, is done by <i * j>; JSOP_ARRAYCOMP <array>,
             * where <array> is the index of array's stack slot.
             */
            if (index == 0 && pn->pn_count != 0 && tokenStream.matchToken(TOK_FOR)) {
                ParseNode *pnexp, *pntop;

                /* Relabel pn as an array comprehension node. */
                pn->setKind(PNK_ARRAYCOMP);

                /*
                 * Remove the comprehension expression from pn's linked list
                 * and save it via pnexp.  We'll re-install it underneath the
                 * ARRAYPUSH node after we parse the rest of the comprehension.
                 */
                pnexp = pn->last();
                JS_ASSERT(pn->pn_count == 1);
                pn->pn_count = 0;
                pn->pn_tail = &pn->pn_head;
                *pn->pn_tail = NULL;

                pntop = comprehensionTail(pnexp, pn->pn_blockid, false,
                                          PNK_ARRAYPUSH, JSOP_ARRAYPUSH);
                if (!pntop)
                    return NULL;
                pn->append(pntop);
            }
#endif /* JS_HAS_GENERATORS */

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_LIST);
        }
        pn->pn_pos.end = tokenStream.currentToken().pos.end;
        return pn;
      }

      case TOK_LC:
      {
        ParseNode *pnval;

        /*
         * A map from property names we've seen thus far to a mask of property
         * assignment types, stored and retrieved with ALE_SET_INDEX/ALE_INDEX.
         */
        AtomIndexMap seen(context);

        enum AssignmentType {
            GET     = 0x1,
            SET     = 0x2,
            VALUE   = 0x4 | GET | SET
        };

        pn = ListNode::create(PNK_RC, tc);
        if (!pn)
            return NULL;
        pn->setOp(JSOP_NEWINIT);
        pn->makeEmpty();

        for (;;) {
            JSAtom *atom;
            TokenKind ltok = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
            switch (ltok) {
              case TOK_NUMBER:
                pn3 = NullaryNode::create(PNK_NUMBER, tc);
                if (!pn3)
                    return NULL;
                pn3->pn_dval = tokenStream.currentToken().number();
                if (!js_ValueToAtom(context, DoubleValue(pn3->pn_dval), &atom))
                    return NULL;
                break;
              case TOK_NAME:
                {
                    atom = tokenStream.currentToken().name();
                    if (atom == context->runtime->atomState.getAtom) {
                        op = JSOP_GETTER;
                    } else if (atom == context->runtime->atomState.setAtom) {
                        op = JSOP_SETTER;
                    } else {
                        pn3 = NullaryNode::create(PNK_NAME, tc);
                        if (!pn3)
                            return NULL;
                        pn3->pn_atom = atom;
                        break;
                    }

                    tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
                    if (tt == TOK_NAME) {
                        atom = tokenStream.currentToken().name();
                        pn3 = NameNode::create(PNK_NAME, atom, tc);
                        if (!pn3)
                            return NULL;
                    } else if (tt == TOK_STRING) {
                        atom = tokenStream.currentToken().atom();
                        pn3 = NameNode::create(PNK_STRING, atom, tc);
                        if (!pn3)
                            return NULL;
                    } else if (tt == TOK_NUMBER) {
                        pn3 = NullaryNode::create(PNK_NUMBER, tc);
                        if (!pn3)
                            return NULL;
                        pn3->pn_dval = tokenStream.currentToken().number();
                        if (!js_ValueToAtom(context, DoubleValue(pn3->pn_dval), &atom))
                            return NULL;
                    } else {
                        tokenStream.ungetToken();
                        pn3 = NullaryNode::create(PNK_NAME, tc);
                        if (!pn3)
                            return NULL;
                        pn3->pn_atom = atom;
                        break;
                    }

                    pn->pn_xflags |= PNX_NONCONST;

                    /* NB: Getter function in { get x(){} } is unnamed. */
                    pn2 = functionDef(NULL, op == JSOP_GETTER ? Getter : Setter, Expression);
                    pn2 = ParseNode::newBinaryOrAppend(PNK_COLON, op, pn3, pn2, tc);
                    goto skip;
                }
              case TOK_STRING: {
                atom = tokenStream.currentToken().atom();
                uint32_t index;
                if (atom->isIndex(&index)) {
                    pn3 = NullaryNode::create(PNK_NUMBER, tc);
                    if (!pn3)
                        return NULL;
                    pn3->pn_dval = index;
                } else {
                    pn3 = NullaryNode::create(PNK_STRING, tc);
                    if (!pn3)
                        return NULL;
                    pn3->pn_atom = atom;
                }
                break;
              }
              case TOK_RC:
                goto end_obj_init;
              default:
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_PROP_ID);
                return NULL;
            }

            op = JSOP_INITPROP;
            tt = tokenStream.getToken();
            if (tt == TOK_COLON) {
                pnval = assignExpr();

                /*
                 * Treat initializers which mutate __proto__ as non-constant,
                 * so that we can later assume singleton objects delegate to
                 * the default Object.prototype.
                 */
                if ((pnval && !pnval->isConstant()) ||
                    atom == context->runtime->atomState.protoAtom) {
                    pn->pn_xflags |= PNX_NONCONST;
                }
            }
#if JS_HAS_DESTRUCTURING_SHORTHAND
            else if (ltok == TOK_NAME && (tt == TOK_COMMA || tt == TOK_RC)) {
                /*
                 * Support, e.g., |var {x, y} = o| as destructuring shorthand
                 * for |var {x: x, y: y} = o|, per proposed JS2/ES4 for JS1.8.
                 */
                tokenStream.ungetToken();
                if (!tokenStream.checkForKeyword(atom->charsZ(), atom->length(), NULL, NULL))
                    return NULL;
                pn->pn_xflags |= PNX_DESTRUCT | PNX_NONCONST;
                pnval = pn3;
                JS_ASSERT(pnval->isKind(PNK_NAME));
                pnval->setArity(PN_NAME);
                ((NameNode *)pnval)->initCommon(tc);
            }
#endif
            else {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_COLON_AFTER_ID);
                return NULL;
            }

            pn2 = ParseNode::newBinaryOrAppend(PNK_COLON, op, pn3, pnval, tc);
          skip:
            if (!pn2)
                return NULL;
            pn->append(pn2);

            /*
             * Check for duplicate property names.  Duplicate data properties
             * only conflict in strict mode.  Duplicate getter or duplicate
             * setter halves always conflict.  A data property conflicts with
             * any part of an accessor property.
             */
            AssignmentType assignType;
            if (op == JSOP_INITPROP) {
                assignType = VALUE;
            } else if (op == JSOP_GETTER) {
                assignType = GET;
            } else if (op == JSOP_SETTER) {
                assignType = SET;
            } else {
                JS_NOT_REACHED("bad opcode in object initializer");
                assignType = VALUE; /* try to error early */
            }

            AtomIndexAddPtr p = seen.lookupForAdd(atom);
            if (p) {
                jsatomid index = p.value();
                AssignmentType oldAssignType = AssignmentType(index);
                if ((oldAssignType & assignType) &&
                    (oldAssignType != VALUE || assignType != VALUE || tc->needStrictChecks()))
                {
                    JSAutoByteString name;
                    if (!js_AtomToPrintableString(context, atom, &name))
                        return NULL;

                    uintN flags = (oldAssignType == VALUE &&
                                   assignType == VALUE &&
                                   !tc->inStrictMode())
                                  ? JSREPORT_WARNING
                                  : JSREPORT_ERROR;
                    if (!ReportCompileErrorNumber(context, &tokenStream, NULL, flags,
                                                  JSMSG_DUPLICATE_PROPERTY, name.ptr()))
                    {
                        return NULL;
                    }
                }
                p.value() = assignType | oldAssignType;
            } else {
                if (!seen.add(p, atom, assignType))
                    return NULL;
            }

            tt = tokenStream.getToken();
            if (tt == TOK_RC)
                goto end_obj_init;
            if (tt != TOK_COMMA) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_CURLY_AFTER_LIST);
                return NULL;
            }
        }

      end_obj_init:
        pn->pn_pos.end = tokenStream.currentToken().pos.end;
        return pn;
      }

#if JS_HAS_BLOCK_SCOPE
      case TOK_LET:
        pn = letBlock(LetExpresion);
        if (!pn)
            return NULL;
        break;
#endif

#if JS_HAS_SHARP_VARS
      case TOK_DEFSHARP: {
        if (!tc->ensureSharpSlots())
            return NULL;
        const Token &tok = tokenStream.currentToken();
        TokenPtr begin = tok.pos.begin;
        uint16_t number = tok.sharpNumber();

        tt = tokenStream.getToken(TSF_OPERAND);
        ParseNode *expr = primaryExpr(tt, false);
        if (!expr)
            return NULL;
        if (expr->isKind(PNK_USESHARP) ||
            expr->isKind(PNK_DEFSHARP) ||
            expr->isKind(PNK_STRING) ||
            expr->isKind(PNK_NUMBER) ||
            expr->isKind(PNK_TRUE) ||
            expr->isKind(PNK_FALSE) ||
            expr->isKind(PNK_NULL) ||
            expr->isKind(PNK_THIS))
        {
            reportErrorNumber(expr, JSREPORT_ERROR, JSMSG_BAD_SHARP_VAR_DEF);
            return NULL;
        }
        pn = new_<DefSharpExpression>(number, expr, begin, tokenStream.currentToken().pos.end);
        if (!pn)
            return NULL;
        break;
      }

      case TOK_USESHARP: {
        if (!tc->ensureSharpSlots())
            return NULL;
        /* Check for forward/dangling references at runtime, to allow eval. */
        const Token &tok = tokenStream.currentToken();
        pn = new_<UseSharpExpression>(tok.sharpNumber(), tok.pos);
        if (!pn)
            return NULL;
        break;
      }
#endif /* JS_HAS_SHARP_VARS */

      case TOK_LP:
      {
        JSBool genexp;

        pn = parenExpr(&genexp);
        if (!pn)
            return NULL;
        pn->setInParens(true);
        if (!genexp)
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);
        break;
      }

#if JS_HAS_XML_SUPPORT
      case TOK_STAR:
        if (tc->inStrictMode()) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
            return NULL;
        }
        pn = qualifiedIdentifier();
        if (!pn)
            return NULL;
        break;

      case TOK_AT:
        if (tc->inStrictMode()) {
            reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
            return NULL;
        }      
        pn = attributeIdentifier();
        if (!pn)
            return NULL;
        break;

      case TOK_XMLSTAGO:
        pn = xmlElementOrListRoot(JS_TRUE);
        if (!pn)
            return NULL;
        break;
#endif /* JS_HAS_XML_SUPPORT */

#if JS_HAS_XML_SUPPORT
      case TOK_XMLCDATA:
        JS_ASSERT(!tc->inStrictMode());
        pn = atomNode(PNK_XMLCDATA, JSOP_XMLCDATA);
        if (!pn)
            return NULL;
        break;

      case TOK_XMLCOMMENT:
        JS_ASSERT(!tc->inStrictMode());
        pn = atomNode(PNK_XMLCOMMENT, JSOP_XMLCOMMENT);
        if (!pn)
            return NULL;
        break;
#endif
      case TOK_STRING:
        pn = atomNode(PNK_STRING, JSOP_STRING);
        if (!pn)
            return NULL;
        break;

#if JS_HAS_XML_SUPPORT
      case TOK_XMLPI: {
        JS_ASSERT(!tc->inStrictMode());
        const Token &tok = tokenStream.currentToken();
        pn = new_<XMLProcessingInstruction>(tok.xmlPITarget(), tok.xmlPIData(), tok.pos);
        if (!pn)
            return NULL;
        break;
      }
#endif

      case TOK_NAME:
        pn = NameNode::create(PNK_NAME, tokenStream.currentToken().name(), tc);
        if (!pn)
            return NULL;
        JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NAME);
        pn->setOp(JSOP_NAME);

        if ((tc->flags & (TCF_IN_FUNCTION | TCF_FUN_PARAM_ARGUMENTS)) == TCF_IN_FUNCTION &&
            pn->pn_atom == context->runtime->atomState.argumentsAtom) {
            /*
             * Flag arguments usage so we can avoid unsafe optimizations such
             * as formal parameter assignment analysis (because of the hated
             * feature whereby arguments alias formals). We do this even for
             * a reference of the form foo.arguments, which ancient code may
             * still use instead of arguments (more hate).
             */
            tc->noteArgumentsUse(pn);

            /*
             * Bind early to JSOP_ARGUMENTS to relieve later code from having
             * to do this work (new rule for the emitter to count on).
             */
            if (!afterDot && !(tc->flags & TCF_DECL_DESTRUCTURING)
                && !tc->inStatement(STMT_WITH)) {
                pn->setOp(JSOP_ARGUMENTS);
                pn->pn_dflags |= PND_BOUND;
            }
        } else if ((!afterDot
#if JS_HAS_XML_SUPPORT
                    || (!tc->inStrictMode() && tokenStream.peekToken() == TOK_DBLCOLON)
#endif
                   ) && !(tc->flags & TCF_DECL_DESTRUCTURING)) {
            /* In case this is a generator expression outside of any function. */
            if (!tc->inFunction() &&
                pn->pn_atom == context->runtime->atomState.argumentsAtom) {
                tc->countArgumentsUse(pn);
            }

            StmtInfo *stmt = LexicalLookup(tc, pn->pn_atom, NULL);

            MultiDeclRange mdl = tc->decls.lookupMulti(pn->pn_atom);

            Definition *dn;
            if (!mdl.empty()) {
                dn = mdl.front();
            } else {
                AtomDefnAddPtr p = tc->lexdeps->lookupForAdd(pn->pn_atom);
                if (p) {
                    dn = p.value();
                } else {
                    /*
                     * No definition before this use in any lexical scope.
                     * Create a placeholder definition node to either:
                     * - Be adopted when we parse the real defining
                     *   declaration, or
                     * - Be left as a free variable definition if we never
                     *   see the real definition.
                     */
                    dn = MakePlaceholder(pn, tc);
                    if (!dn || !tc->lexdeps->add(p, dn->pn_atom, dn))
                        return NULL;

                    /*
                     * In case this is a forward reference to a function,
                     * we pessimistically set PND_FUNARG if the next token
                     * is not a left parenthesis.
                     *
                     * If the definition eventually parsed into dn is not a
                     * function, this flag won't hurt, and if we do parse a
                     * function with pn's name, then the PND_FUNARG flag is
                     * necessary for safe context->display-based optimiza-
                     * tion of the closure's static link.
                     */
                    if (tokenStream.peekToken() != TOK_LP)
                        dn->pn_dflags |= PND_FUNARG;
                }
            }

            JS_ASSERT(dn->isDefn());
            LinkUseToDef(pn, dn, tc);

            /* Here we handle the backward function reference case. */
            if (tokenStream.peekToken() != TOK_LP)
                dn->pn_dflags |= PND_FUNARG;

            pn->pn_dflags |= (dn->pn_dflags & PND_FUNARG);
            if (stmt && stmt->type == STMT_WITH)
                pn->pn_dflags |= PND_DEOPTIMIZED;
        }

#if JS_HAS_XML_SUPPORT
        if (!tc->inStrictMode() && tokenStream.matchToken(TOK_DBLCOLON)) {
            if (afterDot) {
                /*
                 * Here primaryExpr is called after . or .. followed by a name
                 * followed by ::. This is the only case where a keyword after
                 * . or .. is not treated as a property name.
                 */
                const KeywordInfo *ki = FindKeyword(pn->pn_atom->charsZ(), pn->pn_atom->length());
                if (ki) {
                    if (ki->tokentype != TOK_FUNCTION) {
                        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_KEYWORD_NOT_NS);
                        return NULL;
                    }

                    pn->setArity(PN_NULLARY);
                    pn->setKind(PNK_FUNCTION);
                }
            }
            pn = qualifiedSuffix(pn);
            if (!pn)
                return NULL;
        }
#endif
        break;

      case TOK_REGEXP:
      {
        pn = NullaryNode::create(PNK_REGEXP, tc);
        if (!pn)
            return NULL;

        const jschar *chars = tokenStream.getTokenbuf().begin();
        size_t length = tokenStream.getTokenbuf().length();
        RegExpFlag flags = tokenStream.currentToken().regExpFlags();
        RegExpStatics *res = context->regExpStatics();

        RegExpObject *reobj;
        if (context->hasfp())
            reobj = RegExpObject::create(context, res, chars, length, flags, &tokenStream);
        else
            reobj = RegExpObject::createNoStatics(context, chars, length, flags, &tokenStream);

        if (!reobj)
            return NULL;

        if (!tc->compileAndGo()) {
            if (!reobj->clearParent(context))
                return NULL;
            if (!reobj->clearType(context))
                return NULL;
        }

        pn->pn_objbox = tc->parser->newObjectBox(reobj);
        if (!pn->pn_objbox)
            return NULL;

        pn->setOp(JSOP_REGEXP);
        break;
      }

      case TOK_NUMBER:
        pn = NullaryNode::create(PNK_NUMBER, tc);
        if (!pn)
            return NULL;
        pn->setOp(JSOP_DOUBLE);
        pn->pn_dval = tokenStream.currentToken().number();
        break;

      case TOK_TRUE:
        return new_<BooleanLiteral>(true, tokenStream.currentToken().pos);
      case TOK_FALSE:
        return new_<BooleanLiteral>(false, tokenStream.currentToken().pos);
      case TOK_THIS:
        return new_<ThisLiteral>(tokenStream.currentToken().pos);
      case TOK_NULL:
        return new_<NullLiteral>(tokenStream.currentToken().pos);

      case TOK_ERROR:
        /* The scanner or one of its subroutines reported the error. */
        return NULL;

      default:
        reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
        return NULL;
    }
    return pn;
}

ParseNode *
Parser::parenExpr(JSBool *genexp)
{
    TokenPtr begin;
    ParseNode *pn;

    JS_ASSERT(tokenStream.currentToken().type == TOK_LP);
    begin = tokenStream.currentToken().pos.begin;

    if (genexp)
        *genexp = JS_FALSE;

    GenexpGuard guard(tc);

    pn = bracketedExpr();
    if (!pn)
        return NULL;
    guard.endBody();

#if JS_HAS_GENERATOR_EXPRS
    if (tokenStream.matchToken(TOK_FOR)) {
        if (!guard.checkValidBody(pn))
            return NULL;
        JS_ASSERT(!pn->isKind(PNK_YIELD));
        if (pn->isKind(PNK_COMMA) && !pn->isInParens()) {
            reportErrorNumber(pn->last(), JSREPORT_ERROR, JSMSG_BAD_GENERATOR_SYNTAX,
                              js_generator_str);
            return NULL;
        }
        pn = generatorExpr(pn);
        if (!pn)
            return NULL;
        pn->pn_pos.begin = begin;
        if (genexp) {
            if (tokenStream.getToken() != TOK_RP) {
                reportErrorNumber(NULL, JSREPORT_ERROR, JSMSG_BAD_GENERATOR_SYNTAX,
                                  js_generator_str);
                return NULL;
            }
            pn->pn_pos.end = tokenStream.currentToken().pos.end;
            *genexp = JS_TRUE;
        }
    } else
#endif /* JS_HAS_GENERATOR_EXPRS */

    if (!guard.maybeNoteGenerator(pn))
        return NULL;

    return pn;
}
