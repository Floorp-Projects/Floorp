/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * JS parser.
 *
 * This is a recursive-descent parser for the JavaScript language specified by
 * "The JavaScript 1.5 Language Specification".  It uses lexical and semantic
 * feedback to disambiguate non-LL(1) structures.  It generates trees of nodes
 * induced by the recursive parsing (not precise syntax trees, see jsparse.h).
 * After tree construction, it rewrites trees to fold constants and evaluate
 * compile-time expressions.  Finally, it calls js_EmitTree (see jsemit.h) to
 * generate bytecode.
 *
 * This parser attempts no error recovery.  The dense JSTokenType enumeration
 * was designed with error recovery built on 64-bit first and follow bitsets
 * in mind, however.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

/*
 * JS parsers, from lowest to highest precedence.
 *
 * Each parser takes a context and a token stream, and emits bytecode using
 * a code generator.
 */

typedef JSParseNode *
JSParser(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc);

typedef JSParseNode *
JSMemberParser(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc,
               JSBool allowCallSyntax);

static JSParser FunctionStmt;
#if JS_HAS_LEXICAL_CLOSURE
static JSParser FunctionExpr;
#endif
static JSParser Statements;
static JSParser Statement;
static JSParser Variables;
static JSParser Expr;
static JSParser AssignExpr;
static JSParser CondExpr;
static JSParser OrExpr;
static JSParser AndExpr;
static JSParser BitOrExpr;
static JSParser BitXorExpr;
static JSParser BitAndExpr;
static JSParser EqExpr;
static JSParser RelExpr;
static JSParser ShiftExpr;
static JSParser AddExpr;
static JSParser MulExpr;
static JSParser UnaryExpr;
static JSMemberParser MemberExpr;
static JSParser PrimaryExpr;

/*
 * Insist that the next token be of type tt, or report errno and return null.
 * NB: this macro uses cx and ts from its lexical environment.
 */

#define MUST_MATCH_TOKEN(tt, errno)                                           \
    JS_BEGIN_MACRO                                                            \
        if (js_GetToken(cx, ts) != tt) {                                      \
            js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR, errno); \
            return NULL;                                                      \
        }                                                                     \
    JS_END_MACRO

#ifdef METER_PARSENODES
static uint32 parsenodes = 0;
static uint32 maxparsenodes = 0;
static uint32 recyclednodes = 0;
#endif

/*
 * Allocate a JSParseNode from cx's temporary arena.
 */
static JSParseNode *
NewParseNode(JSContext *cx, JSToken *tok, JSParseNodeArity arity,
             JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = tc->nodeList;
    if (pn) {
        tc->nodeList = pn->pn_next;
    } else {
        JS_ARENA_ALLOCATE_TYPE(pn, JSParseNode, &cx->tempPool);
        if (!pn)
            return NULL;
    }
    pn->pn_type = tok->type;
    pn->pn_pos = tok->pos;
    pn->pn_op = JSOP_NOP;
    pn->pn_arity = arity;
    pn->pn_next = NULL;
#ifdef METER_PARSENODES
    parsenodes++;
    if (parsenodes - recyclednodes > maxparsenodes)
        maxparsenodes = parsenodes - recyclednodes;
#endif
    return pn;
}

static JSParseNode *
NewBinary(JSContext *cx, JSTokenType tt,
          JSOp op, JSParseNode *left, JSParseNode *right,
          JSTreeContext *tc)
{
    JSParseNode *pn;

    if (!left || !right)
        return NULL;
    pn = tc->nodeList;
    if (pn) {
        tc->nodeList = pn->pn_next;
    } else {
        JS_ARENA_ALLOCATE_TYPE(pn, JSParseNode, &cx->tempPool);
        if (!pn)
            return NULL;
    }
    pn->pn_type = tt;
    pn->pn_pos.begin = left->pn_pos.begin;
    pn->pn_pos.end = right->pn_pos.end;
    pn->pn_op = op;
    pn->pn_arity = PN_BINARY;
    pn->pn_left = left;
    pn->pn_right = right;
    pn->pn_next = NULL;
#ifdef METER_PARSENODES
    parsenodes++;
    if (parsenodes - recyclednodes > maxparsenodes)
        maxparsenodes = parsenodes - recyclednodes;
#endif
    return pn;
}

static void
RecycleTree(JSParseNode *pn, JSTreeContext *tc)
{
    JSParseNode *pn2;

    if (!pn)
        return;
    JS_ASSERT(pn != tc->nodeList);      /* catch back-to-back dup recycles */
    switch (pn->pn_arity) {
      case PN_FUNC:
        RecycleTree(pn->pn_body, tc);
        break;
      case PN_LIST:
        while ((pn2 = pn->pn_head) != NULL) {
            pn->pn_head = pn2->pn_next;
            RecycleTree(pn2, tc);
        }
        break;
      case PN_TERNARY:
        RecycleTree(pn->pn_kid1, tc);
        RecycleTree(pn->pn_kid2, tc);
        RecycleTree(pn->pn_kid3, tc);
        break;
      case PN_BINARY:
        RecycleTree(pn->pn_left, tc);
        RecycleTree(pn->pn_right, tc);
        break;
      case PN_UNARY:
        RecycleTree(pn->pn_kid, tc);
        break;
      case PN_NAME:
        RecycleTree(pn->pn_expr, tc);
        break;
      case PN_NULLARY:
        break;
    }
    pn->pn_next = tc->nodeList;
    tc->nodeList = pn;
#ifdef METER_PARSENODES
    recyclednodes++;
#endif
}

static JSBool
WellTerminated(JSContext *cx, JSTokenStream *ts, JSTokenType lastExprType)
{
    JSTokenType tt;

    tt = js_PeekTokenSameLine(cx, ts);
    if (tt == TOK_ERROR)
        return JS_FALSE;
    if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
#if JS_HAS_LEXICAL_CLOSURE
        if ((tt == TOK_FUNCTION || lastExprType == TOK_FUNCTION) &&
            cx->version < JSVERSION_1_2) {
            /*
             * Checking against version < 1.2 and version >= 1.0
             * in the above line breaks old javascript, so we keep it
             * this way for now... XXX warning needed?
             */
            return JS_TRUE;
        }
#endif
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_SEMI_BEFORE_STMNT);
        return JS_FALSE;
    }
    return JS_TRUE;
}

#if JS_HAS_GETTER_SETTER
static JSTokenType
CheckGetterOrSetter(JSContext *cx, JSTokenStream *ts, JSTokenType tt)
{
    JSAtom *atom;
    JSRuntime *rt;
    JSOp op;

    JS_ASSERT(CURRENT_TOKEN(ts).type == TOK_NAME);
    atom = CURRENT_TOKEN(ts).t_atom;
    rt = cx->runtime;
    if (atom == rt->atomState.getterAtom)
        op = JSOP_GETTER;
    else if (atom == rt->atomState.setterAtom)
        op = JSOP_SETTER;
    else
        return TOK_NAME;
    if (js_PeekTokenSameLine(cx, ts) != tt)
        return TOK_NAME;
    (void) js_GetToken(cx, ts);
    if (CURRENT_TOKEN(ts).t_op != JSOP_NOP) {
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_BAD_GETTER_OR_SETTER,
                                    (op == JSOP_GETTER)
                                    ? js_getter_str
                                    : js_setter_str);
        return TOK_ERROR;
    }
    CURRENT_TOKEN(ts).t_op = op;
    if (!js_ReportCompileErrorNumber(cx, ts, NULL,
                                     JSREPORT_WARNING |
                                     JSREPORT_STRICT,
                                     JSMSG_DEPRECATED_USAGE,
                                     ATOM_BYTES(atom))) {
        return TOK_ERROR;
    }
    return tt;
}
#endif

/*
 * Parse a top-level JS script.
 */
JS_FRIEND_API(JSParseNode *)
js_ParseTokenStream(JSContext *cx, JSObject *chain, JSTokenStream *ts)
{
    JSStackFrame *fp, frame;
    JSTreeContext tc;
    JSParseNode *pn;

    /*
     * Push a compiler frame if we have no frames, or if the top frame is a
     * lightweight function activation, or if its scope chain doesn't match
     * the one passed to us.
     */
    fp = cx->fp;
    if (!fp || !fp->varobj || fp->scopeChain != chain) {
        memset(&frame, 0, sizeof frame);
        frame.varobj = frame.scopeChain = chain;
        if (cx->options & JSOPTION_VAROBJFIX) {
            while ((chain = JS_GetParent(cx, chain)) != NULL)
                frame.varobj = chain;
        }
        frame.down = fp;
        cx->fp = &frame;
    }

    /*
     * Prevent GC activation (possible if out of memory when atomizing,
     * or from pre-ECMAv2 switch case expr eval in the unlikely case of a
     * branch-callback -- unlikely because it means the switch case must
     * have called a function).
     */
    JS_DISABLE_GC(cx->runtime);
    TREE_CONTEXT_INIT(&tc);
    pn = Statements(cx, ts, &tc);
    if (pn) {
        if (!js_MatchToken(cx, ts, TOK_EOF)) {
            js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                        JSMSG_SYNTAX_ERROR);
            pn = NULL;
        } else {
            pn->pn_type = TOK_LC;
            if (!js_FoldConstants(cx, pn, &tc))
                pn = NULL;
        }
    }

    TREE_CONTEXT_FINISH(&tc);
    JS_ENABLE_GC(cx->runtime);
    cx->fp = fp;
    return pn;
}

/*
 * Compile a top-level script.
 */
JS_FRIEND_API(JSBool)
js_CompileTokenStream(JSContext *cx, JSObject *chain, JSTokenStream *ts,
                      JSCodeGenerator *cg)
{
    JSStackFrame *fp, frame;
    JSParseNode *pn;
    JSBool ok;
#ifdef METER_PARSENODES
    void *sbrk(ptrdiff_t), *before = sbrk(0);
#endif

    /*
     * Push a compiler frame if we have no frames, or if the top frame is a
     * lightweight function activation, or if its scope chain doesn't match
     * the one passed to us.
     */
    fp = cx->fp;
    if (!fp || !fp->varobj || fp->scopeChain != chain) {
        memset(&frame, 0, sizeof frame);
        frame.varobj = frame.scopeChain = chain;
        if (cx->options & JSOPTION_VAROBJFIX) {
            while ((chain = JS_GetParent(cx, chain)) != NULL)
                frame.varobj = chain;
        }
        frame.down = fp;
        cx->fp = &frame;
    }

    /* Prevent GC activation while compiling. */
    JS_DISABLE_GC(cx->runtime);

    pn = Statements(cx, ts, &cg->treeContext);
    if (!pn) {
        ok = JS_FALSE;
    } else if (!js_MatchToken(cx, ts, TOK_EOF)) {
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_SYNTAX_ERROR);
        ok = JS_FALSE;
    } else {
#ifdef METER_PARSENODES
        printf("Parser growth: %d (%u nodes, %u max, %u unrecycled)\n",
               (char *)sbrk(0) - (char *)before,
               parsenodes,
               maxparsenodes,
               parsenodes - recyclednodes);
        before = sbrk(0);
#endif

        /*
         * No need to emit code here -- Statements already has, for each
         * statement in turn.  Search for TCF_COMPILING in Statements, below.
         * That flag is set for every tc == &cg->treeContext, and it implies
         * that the tc can be downcast to a cg and used to emit code during
         * parsing, rather than at the end of the parse phase.
         */
        ok = JS_TRUE;
    }

#ifdef METER_PARSENODES
    printf("Code-gen growth: %d (%u bytecodes, %u srcnotes)\n",
           (char *)sbrk(0) - (char *)before, CG_OFFSET(cg), cg->noteCount);
#endif
#ifdef JS_ARENAMETER
    JS_DumpArenaStats(stdout);
#endif
    JS_ENABLE_GC(cx->runtime);
    cx->fp = fp;
    return ok;
}

/*
 * Insist on a final return before control flows out of pn, but don't be too
 * smart about loops (do {...; return e2;} while(0) at the end of a function
 * that contains an early return e1 will get a strict-option-only warning).
 */
static JSBool
HasFinalReturn(JSParseNode *pn)
{
    JSBool ok, hasDefault;
    JSParseNode *pn2, *pn3;

    switch (pn->pn_type) {
      case TOK_LC:
        if (!pn->pn_head)
            return JS_FALSE;
        return HasFinalReturn(PN_LAST(pn));

      case TOK_IF:
        ok = HasFinalReturn(pn->pn_kid2);
        ok &= pn->pn_kid3 && HasFinalReturn(pn->pn_kid3);
        return ok;

#if JS_HAS_SWITCH_STATEMENT
      case TOK_SWITCH:
        ok = JS_TRUE;
        hasDefault = JS_FALSE;
        for (pn2 = pn->pn_kid2->pn_head; ok && pn2; pn2 = pn2->pn_next) {
            if (pn2->pn_type == TOK_DEFAULT)
                hasDefault = JS_TRUE;
            pn3 = pn2->pn_right;
            JS_ASSERT(pn3->pn_type == TOK_LC);
            if (pn3->pn_head)
                ok &= HasFinalReturn(PN_LAST(pn3));
        }
        /* If a final switch has no default case, we judge it harshly. */
        ok &= hasDefault;
        return ok;
#endif /* JS_HAS_SWITCH_STATEMENT */

      case TOK_WITH:
        return HasFinalReturn(pn->pn_right);

      case TOK_RETURN:
        return JS_TRUE;

#if JS_HAS_EXCEPTIONS
      case TOK_THROW:
        return JS_TRUE;

      case TOK_TRY:
        /* If we have a finally block that returns, we are done. */
        if (pn->pn_kid3 && HasFinalReturn(pn->pn_kid3))
            return JS_TRUE;

        /* Else check the try block and any and all catch statements. */
        ok = HasFinalReturn(pn->pn_kid1);
        if (pn->pn_kid2)
            ok &= HasFinalReturn(pn->pn_kid2);
        return ok;

      case TOK_CATCH:
        /* Check this block's code and iterate over further catch blocks. */
        ok = HasFinalReturn(pn->pn_kid3);
        for (pn2 = pn->pn_kid2; pn2; pn2 = pn2->pn_kid2)
            ok &= HasFinalReturn(pn2->pn_kid3);
        return ok;
#endif

      default:
        return JS_FALSE;
    }
}

static JSBool
ReportNoReturnValue(JSContext *cx, JSTokenStream *ts)
{
    JSFunction *fun;
    JSBool ok;

    fun = cx->fp->fun;
    if (fun->atom) {
        char *name = js_GetStringBytes(ATOM_TO_STRING(fun->atom));
        ok = js_ReportCompileErrorNumber(cx, ts, NULL,
                                         JSREPORT_WARNING |
                                         JSREPORT_STRICT,
                                         JSMSG_NO_RETURN_VALUE, name);
    } else {
        ok = js_ReportCompileErrorNumber(cx, ts, NULL,
                                         JSREPORT_WARNING |
                                         JSREPORT_STRICT,
                                         JSMSG_ANON_NO_RETURN_VALUE);
    }
    return ok;
}

static JSBool
CheckFinalReturn(JSContext *cx, JSTokenStream *ts, JSParseNode *pn)
{
    return HasFinalReturn(pn) || ReportNoReturnValue(cx, ts);
}

static JSParseNode *
FunctionBody(JSContext *cx, JSTokenStream *ts, JSFunction *fun,
             JSTreeContext *tc)
{
    JSStackFrame *fp, frame;
    JSObject *funobj;
    uintN oldflags;
    JSParseNode *pn;

    fp = cx->fp;
    funobj = fun->object;
    if (!fp || fp->fun != fun || fp->varobj != funobj ||
        fp->scopeChain != funobj) {
        memset(&frame, 0, sizeof frame);
        frame.fun = fun;
        frame.varobj = frame.scopeChain = funobj;
        frame.down = fp;
        cx->fp = &frame;
    }

    oldflags = tc->flags;
    tc->flags &= ~(TCF_RETURN_EXPR | TCF_RETURN_VOID);
    tc->flags |= TCF_IN_FUNCTION;
    pn = Statements(cx, ts, tc);

    /* Check for falling off the end of a function that returns a value. */
    if (pn && JS_HAS_STRICT_OPTION(cx) && (tc->flags & TCF_RETURN_EXPR)) {
        if (!CheckFinalReturn(cx, ts, pn))
            pn = NULL;
    }

    cx->fp = fp;
    tc->flags = oldflags | (tc->flags & TCF_FUN_FLAGS);
    return pn;
}

/*
 * Compile a JS function body, which might appear as the value of an event
 * handler attribute in an HTML <INPUT> tag.
 */
JSBool
js_CompileFunctionBody(JSContext *cx, JSTokenStream *ts, JSFunction *fun)
{
    JSCodeGenerator funcg;
    JSStackFrame *fp, frame;
    JSObject *funobj;
    JSParseNode *pn;
    JSBool ok;

    if (!js_InitCodeGenerator(cx, &funcg, ts->filename, ts->lineno,
                              ts->principals)) {
        return JS_FALSE;
    }

    /* Prevent GC activation while compiling. */
    JS_DISABLE_GC(cx->runtime);

    /* Push a JSStackFrame for use by FunctionBody and js_EmitFunctionBody. */
    fp = cx->fp;
    funobj = fun->object;
    JS_ASSERT(!fp || fp->fun != fun || fp->varobj != funobj ||
              fp->scopeChain != funobj);
    memset(&frame, 0, sizeof frame);
    frame.fun = fun;
    frame.varobj = frame.scopeChain = funobj;
    frame.down = fp;
    cx->fp = &frame;

    /* Ensure that the body looks like a block statement to js_EmitTree. */
    CURRENT_TOKEN(ts).type = TOK_LC;
    pn = FunctionBody(cx, ts, fun, &funcg.treeContext);
    if (!pn) {
        ok = JS_FALSE;
    } else {
        ok = js_FoldConstants(cx, pn, &funcg.treeContext) &&
             js_EmitFunctionBody(cx, &funcg, pn, fun);
    }

    /* Restore saved state and release code generation arenas. */
    cx->fp = fp;
    JS_ENABLE_GC(cx->runtime);
    js_FinishCodeGenerator(cx, &funcg);
    return ok;
}

static JSParseNode *
FunctionDef(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc,
            JSBool lambda)
{
    JSParseNode *pn, *body;
    JSOp op, prevop;
    JSAtom *funAtom, *argAtom;
    JSFunction *fun;
    JSObject *parent;
    JSObject *pobj;
    JSScopeProperty *sprop;
    JSTreeContext funtc;
    jsid oldArgId;
    JSAtomListElement *ale;

    /* Make a TOK_FUNCTION node. */
    pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_FUNC, tc);
    if (!pn)
        return NULL;
    op = CURRENT_TOKEN(ts).t_op;

    /* Scan the optional function name into funAtom. */
    if (js_MatchToken(cx, ts, TOK_NAME))
        funAtom = CURRENT_TOKEN(ts).t_atom;
    else
        funAtom = NULL;

    /* Find the nearest variable-declaring scope and use it as our parent. */
    parent = cx->fp->varobj;
    fun = js_NewFunction(cx, NULL, NULL, 0, lambda ? JSFUN_LAMBDA : 0, parent,
                         funAtom);
    if (!fun)
        return NULL;

#if JS_HAS_GETTER_SETTER
    if (op != JSOP_NOP)
        fun->flags |= (op == JSOP_GETTER) ? JSPROP_GETTER : JSPROP_SETTER;
#endif

    /* Now parse formal argument list and compute fun->nargs. */
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_FORMAL);
    if (!js_MatchToken(cx, ts, TOK_RP)) {
        do {
            MUST_MATCH_TOKEN(TOK_NAME, JSMSG_MISSING_FORMAL);
            argAtom = CURRENT_TOKEN(ts).t_atom;
            pobj = NULL;
            if (!js_LookupProperty(cx, fun->object, (jsid)argAtom, &pobj,
                                   (JSProperty **)&sprop)) {
                return NULL;
            }
            if (sprop && pobj == fun->object) {
                if (SPROP_GETTER(sprop, pobj) == js_GetArgument) {
                    if (JS_HAS_STRICT_OPTION(cx)) {
                        OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
                        if (!js_ReportCompileErrorNumber(cx, ts, NULL,
                                                         JSREPORT_WARNING |
                                                         JSREPORT_STRICT,
                                                         JSMSG_DUPLICATE_FORMAL,
                                                         ATOM_BYTES(argAtom))) {
                            return NULL;
                        }
                    }

                    /*
                     * A duplicate parameter name. We create a dummy symbol
                     * entry with property id of the parameter number and set
                     * the id to the name of the parameter.
                     * The decompiler will know to treat this case specially.
                     */
                    oldArgId = (jsid) sprop->id;
                    OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
                    sprop = NULL;
                    if (!js_DefineProperty(cx, fun->object,
                                           oldArgId, JSVAL_VOID,
                                           js_GetArgument, js_SetArgument,
                                           JSPROP_ENUMERATE | JSPROP_PERMANENT,
                                           (JSProperty **)&sprop)) {
                        return NULL;
                    }
                    sprop->id = (jsid) argAtom;
                }
            }
            if (sprop) {
                OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
                sprop = NULL;
            }
            if (!js_DefineProperty(cx, fun->object,
                                   (jsid)argAtom, JSVAL_VOID,
                                   js_GetArgument, js_SetArgument,
                                   JSPROP_ENUMERATE | JSPROP_PERMANENT,
                                   (JSProperty **)&sprop)) {
                return NULL;
            }
            JS_ASSERT(sprop);
            sprop->id = INT_TO_JSVAL(fun->nargs++);
            OBJ_DROP_PROPERTY(cx, fun->object, (JSProperty *)sprop);
        } while (js_MatchToken(cx, ts, TOK_COMMA));

        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FORMAL);
    }

    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_BODY);
    pn->pn_pos.begin = CURRENT_TOKEN(ts).pos.begin;

    TREE_CONTEXT_INIT(&funtc);
    body = FunctionBody(cx, ts, fun, &funtc);
    if (!body)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_BODY);
    pn->pn_pos.end = CURRENT_TOKEN(ts).pos.end;

#if JS_HAS_LEXICAL_CLOSURE
    /*
     * If we collected flags that indicate nested heavyweight functions, or
     * this function contains heavyweight-making statements (references to
     * __parent__ or __proto__; use of with, eval, import, or export; and
     * assignment to arguments), flag the function as heavyweight (requiring
     * a call object per invocation).
     */
    if (funtc.flags & TCF_FUN_HEAVYWEIGHT) {
        fun->flags |= JSFUN_HEAVYWEIGHT;
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
    } else {
        /*
         * If this function is a named statement function not at top-level
         * (i.e. a JSOP_CLOSURE), or if it refers to unqualified names that
         * are not local args or vars (TCF_FUN_USES_NONLOCALS), then our
         * enclosing function, if any, must be heavyweight.
         */
        if ((!lambda && funAtom && tc->topStmt) ||
            (funtc.flags & TCF_FUN_USES_NONLOCALS)) {
            tc->flags |= TCF_FUN_HEAVYWEIGHT;
        }
    }
#endif

    /*
     * Record names for function statements in tc->decls so we know when to
     * avoid optimizing variable references that might name a function.
     */
    if (!lambda && funAtom) {
        ATOM_LIST_SEARCH(ale, &tc->decls, funAtom);
        if (ale) {
            prevop = ALE_JSOP(ale);
            if ((JS_HAS_STRICT_OPTION(cx) || prevop == JSOP_DEFCONST) &&
                !js_ReportCompileErrorNumber(cx, ts, NULL,
                                             (prevop != JSOP_DEFCONST)
                                             ? JSREPORT_WARNING|JSREPORT_STRICT
                                             : JSREPORT_ERROR,
                                             JSMSG_REDECLARED_VAR,
                                             (prevop == JSOP_DEFFUN ||
                                              prevop == JSOP_CLOSURE)
                                             ? js_function_str
                                             : (prevop == JSOP_DEFCONST)
                                             ? js_const_str
                                             : js_var_str,
                                             ATOM_BYTES(funAtom))) {
                return NULL;
            }
            if (tc->topStmt && prevop == JSOP_DEFVAR)
                tc->flags |= TCF_FUN_CLOSURE_VS_VAR;
        } else {
            ale = js_IndexAtom(cx, funAtom, &tc->decls);
            if (!ale)
                return NULL;
        }
        ALE_SET_JSOP(ale, tc->topStmt ? JSOP_CLOSURE : JSOP_DEFFUN);

#if JS_HAS_LEXICAL_CLOSURE
        /*
         * A function nested at top level inside another's body needs only a
         * local variable to bind its name to its value, and not an activation
         * object property (it might also need the activation property, if the
         * outer function contains with statements, e.g., but the stack slot
         * wins when jsemit.c's LookupArgOrVar can optimize a JSOP_NAME into a
         * JSOP_GETVAR bytecode).
         */
        if (!tc->topStmt && (tc->flags & TCF_IN_FUNCTION)) {
            JSStackFrame *fp;
            JSObject *varobj;

            /*
             * Define a property on the outer function so that LookupArgOrVar
             * can properly optimize accesses.
             *
             * XXX Here and in Variables, we use the function object's scope,
             * XXX arguably polluting it, when we could use a compiler-private
             * XXX scope structure.  Tradition!
             */
            fp = cx->fp;
            varobj = fp->varobj;
            JS_ASSERT(OBJ_GET_CLASS(cx, varobj) == &js_FunctionClass);
            JS_ASSERT(fp->fun == (JSFunction *) JS_GetPrivate(cx, varobj));
            if (!js_DefineProperty(cx, varobj, (jsid)funAtom,
                                   OBJECT_TO_JSVAL(fun->object),
                                   js_GetLocalVariable, js_SetLocalVariable,
                                   JSPROP_ENUMERATE,
                                   (JSProperty **)&sprop)) {
                return NULL;
            }

            /* Allocate a slot for this property. */
            sprop->id = INT_TO_JSVAL(fp->fun->nvars++);
            OBJ_DROP_PROPERTY(cx, varobj, (JSProperty *)sprop);
        }
#endif
    }

#if JS_HAS_LEXICAL_CLOSURE
    if (lambda || !funAtom) {
        /*
         * ECMA ed. 3 standard: function expression, possibly anonymous (even
         * if at top-level, an unnamed function is an expression statement, not
         * a function declaration).
         */
        op = fun->atom ? JSOP_NAMEDFUNOBJ : JSOP_ANONFUNOBJ;
    } else if (tc->topStmt) {
        /*
         * ECMA ed. 3 extension: a function expression statement not at the
         * top level, e.g., in a compound statement such as the "then" part
         * of an "if" statement, binds a closure only if control reaches that
         * sub-statement.
         */
        op = JSOP_CLOSURE;
    } else
#endif
        op = JSOP_NOP;

    pn->pn_op = op;
    pn->pn_fun = fun;
    pn->pn_body = body;
    pn->pn_flags = funtc.flags & TCF_FUN_FLAGS;
    pn->pn_tryCount = funtc.tryCount;
    TREE_CONTEXT_FINISH(&funtc);
    return pn;
}

static JSParseNode *
FunctionStmt(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    return FunctionDef(cx, ts, tc, JS_FALSE);
}

#if JS_HAS_LEXICAL_CLOSURE
static JSParseNode *
FunctionExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    return FunctionDef(cx, ts, tc, JS_TRUE);
}
#endif

/*
 * Parse the statements in a block, creating a TOK_LC node that lists the
 * statements' trees.  If called from block-parsing code, the caller must
 * match { before and } after.
 */
static JSParseNode *
Statements(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2;
    JSTokenType tt;

    pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
    if (!pn)
        return NULL;
    PN_INIT_LIST(pn);

    ts->flags |= TSF_REGEXP;
    while ((tt = js_PeekToken(cx, ts)) > TOK_EOF && tt != TOK_RC) {
        ts->flags &= ~TSF_REGEXP;
        pn2 = Statement(cx, ts, tc);
        if (!pn2)
            return NULL;
        ts->flags |= TSF_REGEXP;

        /* If compiling top-level statements, emit as we go to save space. */
        if (!tc->topStmt && (tc->flags & TCF_COMPILING)) {
            if (cx->fp->fun &&
                JS_HAS_STRICT_OPTION(cx) &&
                (tc->flags & TCF_RETURN_EXPR)) {
                /*
                 * Check pn2 for lack of a final return statement if it is the
                 * last statement in the block.
                 */
                tt = js_PeekToken(cx, ts);
                if ((tt == TOK_EOF || tt == TOK_RC) &&
                    !CheckFinalReturn(cx, ts, pn2)) {
                    tt = TOK_ERROR;
                    break;
                }

                /*
                 * Clear TCF_RETURN_EXPR so FunctionBody doesn't try to
                 * CheckFinalReturn again.
                 */
                tc->flags &= ~TCF_RETURN_EXPR;
            }
            if (!js_FoldConstants(cx, pn2, tc) ||
                !js_AllocTryNotes(cx, (JSCodeGenerator *)tc) ||
                !js_EmitTree(cx, (JSCodeGenerator *)tc, pn2)) {
                tt = TOK_ERROR;
                break;
            }
            RecycleTree(pn2, tc);
        } else {
            PN_APPEND(pn, pn2);
        }
    }
    ts->flags &= ~TSF_REGEXP;
    if (tt == TOK_ERROR)
        return NULL;

    pn->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
    return pn;
}

static JSParseNode *
Condition(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    pn = Expr(cx, ts, tc);
    if (!pn)
        return NULL;
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /*
     * Check for (a = b) and "correct" it to (a == b).
     * XXX not ECMA, but documented in several books -- need a compile option
     */
    if (pn->pn_type == TOK_ASSIGN && pn->pn_op == JSOP_NOP) {
        JSBool rewrite = JS_HAS_STRICT_OPTION(cx) ||
                         !JSVERSION_IS_ECMA(cx->version);
        if (!js_ReportCompileErrorNumber(cx, ts, NULL,
                                         JSREPORT_WARNING | JSREPORT_STRICT,
                                         JSMSG_EQUAL_AS_ASSIGN,
                                         rewrite
                                         ? "\nAssuming equality test"
                                         : "")) {
            return NULL;
        }
        if (rewrite) {
            pn->pn_type = TOK_EQOP;
            pn->pn_op = (JSOp)cx->jsop_eq;
            pn2 = pn->pn_left;
            switch (pn2->pn_op) {
              case JSOP_SETNAME:
                pn2->pn_op = JSOP_NAME;
                break;
              case JSOP_SETPROP:
                pn2->pn_op = JSOP_GETPROP;
                break;
              case JSOP_SETELEM:
                pn2->pn_op = JSOP_GETELEM;
                break;
              default:
                JS_ASSERT(0);
            }
        }
    }
    return pn;
}

static JSBool
MatchLabel(JSContext *cx, JSTokenStream *ts, JSParseNode *pn)
{
    JSAtom *label;
#if JS_HAS_LABEL_STATEMENT
    JSTokenType tt;

    tt = js_PeekTokenSameLine(cx, ts);
    if (tt == TOK_ERROR)
        return JS_FALSE;
    if (tt == TOK_NAME) {
        (void) js_GetToken(cx, ts);
        label = CURRENT_TOKEN(ts).t_atom;
    } else {
        label = NULL;
    }
#else
    label = NULL;
#endif
    pn->pn_atom = label;
    if (pn->pn_pos.end.lineno == ts->lineno)
        return WellTerminated(cx, ts, TOK_ERROR);
    return JS_TRUE;
}

#if JS_HAS_EXPORT_IMPORT
static JSParseNode *
ImportExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2, *pn3;
    JSTokenType tt;

    MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_IMPORT_NAME);
    pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NAME, tc);
    if (!pn)
        return NULL;
    pn->pn_op = JSOP_NAME;
    pn->pn_atom = CURRENT_TOKEN(ts).t_atom;
    pn->pn_expr = NULL;
    pn->pn_slot = -1;
    pn->pn_attrs = 0;

    ts->flags |= TSF_REGEXP;
    while ((tt = js_GetToken(cx, ts)) == TOK_DOT || tt == TOK_LB) {
        ts->flags &= ~TSF_REGEXP;
        if (pn->pn_op == JSOP_IMPORTALL)
            goto bad_import;

        if (tt == TOK_DOT) {
            pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NAME, tc);
            if (!pn2)
                return NULL;
            if (js_MatchToken(cx, ts, TOK_STAR)) {
                pn2->pn_op = JSOP_IMPORTALL;
                pn2->pn_atom = NULL;
            } else {
                MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NAME_AFTER_DOT);
                pn2->pn_op = JSOP_GETPROP;
                pn2->pn_atom = CURRENT_TOKEN(ts).t_atom;
                pn2->pn_slot = -1;
                pn2->pn_attrs = 0;
            }
            pn2->pn_expr = pn;
            pn2->pn_pos.begin = pn->pn_pos.begin;
            pn2->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
        } else {
            /* Make a TOK_LB node. */
            pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_BINARY, tc);
            if (!pn2)
                return NULL;
            pn3 = Expr(cx, ts, tc);
            if (!pn3)
                return NULL;

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);
            pn2->pn_pos.begin = pn->pn_pos.begin;
            pn2->pn_pos.end = CURRENT_TOKEN(ts).pos.end;

            pn2->pn_op = JSOP_GETELEM;
            pn2->pn_left = pn;
            pn2->pn_right = pn3;
        }

        pn = pn2;
        ts->flags |= TSF_REGEXP;
    }
    ts->flags &= ~TSF_REGEXP;
    if (tt == TOK_ERROR)
        return NULL;
    js_UngetToken(ts);

    switch (pn->pn_op) {
      case JSOP_GETPROP:
        pn->pn_op = JSOP_IMPORTPROP;
        break;
      case JSOP_GETELEM:
        pn->pn_op = JSOP_IMPORTELEM;
        break;
      case JSOP_IMPORTALL:
        break;
      default:
        goto bad_import;
    }
    return pn;

  bad_import:
    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR, JSMSG_BAD_IMPORT);
    return NULL;
}
#endif /* JS_HAS_EXPORT_IMPORT */

extern const char js_with_statement_str[];

static JSParseNode *
Statement(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSTokenType tt, lastExprType;
    JSParseNode *pn, *pn1, *pn2, *pn3, *pn4;
    JSStmtInfo stmtInfo, *stmt, *stmt2;
    JSAtom *label;

    ts->flags |= TSF_REGEXP;
    tt = js_GetToken(cx, ts);
    ts->flags &= ~TSF_REGEXP;

#if JS_HAS_GETTER_SETTER
    if (tt == TOK_NAME) {
        tt = CheckGetterOrSetter(cx, ts, TOK_FUNCTION);
        if (tt == TOK_ERROR)
            return NULL;
    }
#endif

    switch (tt) {
#if JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
        if (!pn)
            return NULL;
        PN_INIT_LIST(pn);
        if (js_MatchToken(cx, ts, TOK_STAR)) {
            pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
            if (!pn2)
                return NULL;
            PN_APPEND(pn, pn2);
        } else {
            do {
                MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_EXPORT_NAME);
                pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NAME, tc);
                if (!pn2)
                    return NULL;
                pn2->pn_op = JSOP_NAME;
                pn2->pn_atom = CURRENT_TOKEN(ts).t_atom;
                pn2->pn_expr = NULL;
                pn2->pn_slot = -1;
                pn2->pn_attrs = 0;
                PN_APPEND(pn, pn2);
            } while (js_MatchToken(cx, ts, TOK_COMMA));
        }
        pn->pn_pos.end = PN_LAST(pn)->pn_pos.end;
        if (pn->pn_pos.end.lineno == ts->lineno &&
            !WellTerminated(cx, ts, TOK_ERROR)) {
            return NULL;
        }
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
        break;

      case TOK_IMPORT:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
        if (!pn)
            return NULL;
        PN_INIT_LIST(pn);
        do {
            pn2 = ImportExpr(cx, ts, tc);
            if (!pn2)
                return NULL;
            PN_APPEND(pn, pn2);
        } while (js_MatchToken(cx, ts, TOK_COMMA));
        pn->pn_pos.end = PN_LAST(pn)->pn_pos.end;
        if (pn->pn_pos.end.lineno == ts->lineno &&
            !WellTerminated(cx, ts, TOK_ERROR)) {
            return NULL;
        }
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
        break;
#endif /* JS_HAS_EXPORT_IMPORT */

      case TOK_FUNCTION:
        pn = FunctionStmt(cx, ts, tc);
        if (!pn)
            return NULL;
        if (pn->pn_pos.end.lineno == ts->lineno &&
            !WellTerminated(cx, ts, TOK_FUNCTION)) {
            return NULL;
        }
        break;

      case TOK_IF:
        /* An IF node has three kids: condition, then, and optional else. */
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_TERNARY, tc);
        if (!pn)
            return NULL;
        pn1 = Condition(cx, ts, tc);
        if (!pn1)
            return NULL;
        js_PushStatement(tc, &stmtInfo, STMT_IF, -1);
        pn2 = Statement(cx, ts, tc);
        if (!pn2)
            return NULL;
        if (js_MatchToken(cx, ts, TOK_ELSE)) {
            stmtInfo.type = STMT_ELSE;
            pn3 = Statement(cx, ts, tc);
            if (!pn3)
                return NULL;
            pn->pn_pos.end = pn3->pn_pos.end;
        } else {
            pn3 = NULL;
            pn->pn_pos.end = pn2->pn_pos.end;
        }
        js_PopStatement(tc);
        pn->pn_kid1 = pn1;
        pn->pn_kid2 = pn2;
        pn->pn_kid3 = pn3;
        return pn;

#if JS_HAS_SWITCH_STATEMENT
      case TOK_SWITCH:
      {
        JSParseNode *pn5;
        JSBool seenDefault = JS_FALSE;

        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_BINARY, tc);
        if (!pn)
            return NULL;
        MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_SWITCH);

        /* pn1 points to the switch's discriminant. */
        pn1 = Expr(cx, ts, tc);
        if (!pn1)
            return NULL;

        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_SWITCH);
        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_SWITCH);

        /* pn2 is a list of case nodes. The default case has pn_left == NULL */
        pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
        if (!pn2)
            return NULL;
        PN_INIT_LIST(pn2);

        js_PushStatement(tc, &stmtInfo, STMT_SWITCH, -1);

        while ((tt = js_GetToken(cx, ts)) != TOK_RC) {
            switch (tt) {
              case TOK_DEFAULT:
                if (seenDefault) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_TOO_MANY_DEFAULTS);
                    return NULL;
                }
                seenDefault = JS_TRUE;
                /* fall through */

              case TOK_CASE:
                pn3 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_BINARY, tc);
                if (!pn3)
                    return NULL;
                if (tt == TOK_DEFAULT) {
                    pn3->pn_left = NULL;
                } else {
                    pn3->pn_left = Expr(cx, ts, tc);
                    if (!pn3->pn_left)
                        return NULL;
                }
                PN_APPEND(pn2, pn3);
                if (pn2->pn_count == JS_BIT(16)) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_TOO_MANY_CASES);
                    return NULL;
                }
                break;

              case TOK_ERROR:
                return NULL;

              default:
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_BAD_SWITCH);
                return NULL;
            }
            MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_CASE);

            pn4 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
            if (!pn4)
                return NULL;
            pn4->pn_type = TOK_LC;
            PN_INIT_LIST(pn4);
            while ((tt = js_PeekToken(cx, ts)) != TOK_RC &&
                    tt != TOK_CASE && tt != TOK_DEFAULT) {
                if (tt == TOK_ERROR)
                    return NULL;
                pn5 = Statement(cx, ts, tc);
                if (!pn5)
                    return NULL;
                pn4->pn_pos.end = pn5->pn_pos.end;
                PN_APPEND(pn4, pn5);
            }
            pn3->pn_pos.end = pn4->pn_pos.end;
            pn3->pn_right = pn4;
        }

        js_PopStatement(tc);

        pn->pn_pos.end = pn2->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
        pn->pn_kid1 = pn1;
        pn->pn_kid2 = pn2;
        return pn;
      }
#endif /* JS_HAS_SWITCH_STATEMENT */

      case TOK_WHILE:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_BINARY, tc);
        if (!pn)
            return NULL;
        js_PushStatement(tc, &stmtInfo, STMT_WHILE_LOOP, -1);
        pn2 = Condition(cx, ts, tc);
        if (!pn2)
            return NULL;
        pn->pn_left = pn2;
        pn2 = Statement(cx, ts, tc);
        if (!pn2)
            return NULL;
        js_PopStatement(tc);
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->pn_right = pn2;
        return pn;

#if JS_HAS_DO_WHILE_LOOP
      case TOK_DO:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_BINARY, tc);
        if (!pn)
            return NULL;
        js_PushStatement(tc, &stmtInfo, STMT_DO_LOOP, -1);
        pn2 = Statement(cx, ts, tc);
        if (!pn2)
            return NULL;
        pn->pn_left = pn2;
        MUST_MATCH_TOKEN(TOK_WHILE, JSMSG_WHILE_AFTER_DO);
        pn2 = Condition(cx, ts, tc);
        if (!pn2)
            return NULL;
        js_PopStatement(tc);
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->pn_right = pn2;
        break;
#endif /* JS_HAS_DO_WHILE_LOOP */

      case TOK_FOR:
        /* A FOR node is binary, left is loop control and right is the body. */
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_BINARY, tc);
        if (!pn)
            return NULL;
        js_PushStatement(tc, &stmtInfo, STMT_FOR_LOOP, -1);

        MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);
        tt = js_PeekToken(cx, ts);
        if (tt == TOK_SEMI) {
            /* No initializer -- set first kid of left sub-node to null. */
            pn1 = NULL;
        } else {
            /* Set pn1 to a var list or an initializing expression. */
#if JS_HAS_IN_OPERATOR
            /*
             * Set the TCF_IN_FOR_INIT flag during parsing of the first clause
             * of the for statement.  This flag will be used by the RelExpr
             * production; if it is set, then the 'in' keyword will not be
             * recognized as an operator, leaving it available to be parsed as
             * part of a for/in loop.  A side effect of this restriction is
             * that (unparenthesized) expressions involving an 'in' operator
             * are illegal in the init clause of an ordinary for loop.
             */
            tc->flags |= TCF_IN_FOR_INIT;
#endif /* JS_HAS_IN_OPERATOR */
            if (tt == TOK_VAR) {
                (void) js_GetToken(cx, ts);
                pn1 = Variables(cx, ts, tc);
            } else {
                pn1 = Expr(cx, ts, tc);
            }
#if JS_HAS_IN_OPERATOR
            tc->flags &= ~TCF_IN_FOR_INIT;
#endif /* JS_HAS_IN_OPERATOR */
            if (!pn1)
                return NULL;
        }

        /*
         * We can be sure that it's a for/in loop if there's still an 'in'
         * keyword here, even if JavaScript recognizes 'in' as an operator,
         * as we've excluded 'in' from being parsed in RelExpr by setting
         * the TCF_IN_FOR_INIT flag in our JSTreeContext.
         */
        if (pn1 && js_MatchToken(cx, ts, TOK_IN)) {
            stmtInfo.type = STMT_FOR_IN_LOOP;

            /* Check that the left side of the 'in' is valid. */
            if ((pn1->pn_type == TOK_VAR)
                ? (pn1->pn_count > 1 || pn1->pn_op == JSOP_DEFCONST)
                : (pn1->pn_type != TOK_NAME &&
                   pn1->pn_type != TOK_DOT &&
                   pn1->pn_type != TOK_LB)) {
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_BAD_FOR_LEFTSIDE);
                return NULL;
            }

            /* Beware 'for (arguments in ...)' with or without a 'var'. */
            pn2 = (pn1->pn_type == TOK_VAR) ? pn1->pn_head : pn1;
            if (pn2->pn_type == TOK_NAME &&
                pn2->pn_atom == cx->runtime->atomState.argumentsAtom) {
                tc->flags |= TCF_FUN_HEAVYWEIGHT;
            }

            /* Parse the object expression as the right operand of 'in'. */
            pn2 = NewBinary(cx, TOK_IN, JSOP_NOP, pn1, Expr(cx, ts, tc), tc);
            if (!pn2)
                return NULL;
            pn->pn_left = pn2;
        } else {
            /* Parse the loop condition or null into pn2. */
            MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
            if (js_PeekToken(cx, ts) == TOK_SEMI) {
                pn2 = NULL;
            } else {
                pn2 = Expr(cx, ts, tc);
                if (!pn2)
                    return NULL;
            }

            /* Parse the update expression or null into pn3. */
            MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
            if (js_PeekToken(cx, ts) == TOK_RP) {
                pn3 = NULL;
            } else {
                pn3 = Expr(cx, ts, tc);
                if (!pn3)
                    return NULL;
            }

            /* Build the RESERVED node to use as the left kid of pn. */
            pn4 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_TERNARY, tc);
            if (!pn4)
                return NULL;
            pn4->pn_type = TOK_RESERVED;
            pn4->pn_kid1 = pn1;
            pn4->pn_kid2 = pn2;
            pn4->pn_kid3 = pn3;
            pn->pn_left = pn4;
        }

        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

        /* Parse the loop body into pn->pn_right. */
        pn2 = Statement(cx, ts, tc);
        if (!pn2)
            return NULL;
        pn->pn_right = pn2;
        js_PopStatement(tc);

        /* Record the absolute line number for source note emission. */
        pn->pn_pos.end = pn2->pn_pos.end;
        return pn;

#if JS_HAS_EXCEPTIONS
      case TOK_TRY: {
        JSParseNode *catchtail = NULL;
        /*
         * try nodes are ternary.
         * kid1 is the try Statement
         * kid2 is the catch node
         * kid3 is the finally Statement
         *
         * catch nodes are ternary.
         * kid1 is the discriminant
         * kid2 is the next catch node, or NULL
         * kid3 is the catch block (on kid3 so that we can always append a
         *                          new catch pn on catchtail->kid2)
         *
         * catch discriminant nodes are binary
         * atom is the receptacle
         * expr is the discriminant code
         *
         * finally nodes are unary (just the finally expression)
         */
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_TERNARY, tc);
        pn->pn_op = JSOP_NOP;

        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_TRY);
        js_PushStatement(tc, &stmtInfo, STMT_TRY, -1);
        pn->pn_kid1 = Statements(cx, ts, tc);
        if (!pn->pn_kid1)
            return NULL;
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_TRY);
        js_PopStatement(tc);

        catchtail = pn;
        while (js_PeekToken(cx, ts) == TOK_CATCH) {
            /* check for another catch after unconditional catch */
            if (catchtail != pn && !catchtail->pn_kid1->pn_expr) {
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_CATCH_AFTER_GENERAL);
                return NULL;
            }

            /*
             * legal catch forms are:
             * catch (v)
             * catch (v if <boolean_expression>)
             * (the latter is legal only #ifdef JS_HAS_CATCH_GUARD)
             */
            (void) js_GetToken(cx, ts); /* eat `catch' */
            pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_TERNARY, tc);
            if (!pn2)
                return NULL;

            /*
             * We use a PN_NAME for the discriminant (catchguard) node
             * with the actual discriminant code in the initializer spot
             */
            MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_CATCH);
            MUST_MATCH_TOKEN(TOK_NAME, JSMSG_CATCH_IDENTIFIER);
            pn3 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NAME, tc);
            if (!pn3)
                return NULL;

            pn3->pn_atom = CURRENT_TOKEN(ts).t_atom;
            pn3->pn_expr = NULL;
#if JS_HAS_CATCH_GUARD
            /*
             * We use `catch (x if x === 5)' (not `catch (x : x === 5)') to
             * avoid conflicting with the JS2/ECMA2 proposed catchguard syntax.
             */
            if (js_PeekToken(cx, ts) == TOK_IF) {
                (void)js_GetToken(cx, ts); /* eat `if' */
                pn3->pn_expr = Expr(cx, ts, tc);
                if (!pn3->pn_expr)
                    return NULL;
            }
#endif
            pn2->pn_kid1 = pn3;

            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_CATCH);

            MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CATCH);
            js_PushStatement(tc, &stmtInfo, STMT_CATCH, -1);
            stmtInfo.label = pn3->pn_atom;
            pn2->pn_kid3 = Statements(cx, ts, tc);
            if (!pn2->pn_kid3)
                return NULL;
            MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_CATCH);
            js_PopStatement(tc);

            catchtail = catchtail->pn_kid2 = pn2;
        }
        catchtail->pn_kid2 = NULL;

        if (js_MatchToken(cx, ts, TOK_FINALLY)) {
            tc->tryCount++;
            MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY);
            js_PushStatement(tc, &stmtInfo, STMT_FINALLY, -1);
            pn->pn_kid3 = Statements(cx, ts, tc);
            if (!pn->pn_kid3)
                return NULL;
            MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_FINALLY);
            js_PopStatement(tc);
        } else {
            pn->pn_kid3 = NULL;
        }
        if (!pn->pn_kid2 && !pn->pn_kid3) {
            js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                        JSMSG_CATCH_OR_FINALLY);
            return NULL;
        }
        tc->tryCount++;
        return pn;
      }

      case TOK_THROW:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!pn)
            return NULL;
        pn2 = Expr(cx, ts, tc);
        if (!pn2)
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;
        if (pn->pn_pos.end.lineno == ts->lineno &&
            !WellTerminated(cx, ts, TOK_ERROR)) {
            return NULL;
        }
        pn->pn_op = JSOP_THROW;
        pn->pn_kid = pn2;
        break;

      /* TOK_CATCH and TOK_FINALLY are both handled in the TOK_TRY case */
      case TOK_CATCH:
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_CATCH_WITHOUT_TRY);
        return NULL;

      case TOK_FINALLY:
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_FINALLY_WITHOUT_TRY);
        return NULL;

#endif /* JS_HAS_EXCEPTIONS */

      case TOK_BREAK:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
        if (!pn)
            return NULL;
        if (!MatchLabel(cx, ts, pn))
            return NULL;
        stmt = tc->topStmt;
        label = pn->pn_atom;
        if (label) {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_LABEL_NOT_FOUND);
                    return NULL;
                }
                if (stmt->type == STMT_LABEL && stmt->label == label)
                    break;
            }
        } else {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_TOUGH_BREAK);
                    return NULL;
                }
                if (STMT_IS_LOOP(stmt) || stmt->type == STMT_SWITCH)
                    break;
            }
        }
        if (label)
            pn->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
        break;

      case TOK_CONTINUE:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
        if (!pn)
            return NULL;
        if (!MatchLabel(cx, ts, pn))
            return NULL;
        stmt = tc->topStmt;
        label = pn->pn_atom;
        if (label) {
            for (stmt2 = NULL; ; stmt = stmt->down) {
                if (!stmt) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_LABEL_NOT_FOUND);
                    return NULL;
                }
                if (stmt->type == STMT_LABEL) {
                    if (stmt->label == label) {
                        if (!stmt2 || !STMT_IS_LOOP(stmt2)) {
                            js_ReportCompileErrorNumber(cx, ts, NULL,
                                                        JSREPORT_ERROR,
                                                        JSMSG_BAD_CONTINUE);
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
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_BAD_CONTINUE);
                    return NULL;
                }
                if (STMT_IS_LOOP(stmt))
                    break;
            }
        }
        if (label)
            pn->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
        break;

      case TOK_WITH:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_BINARY, tc);
        if (!pn)
            return NULL;
        MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_WITH);
        pn2 = Expr(cx, ts, tc);
        if (!pn2)
            return NULL;
        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_WITH);
        pn->pn_left = pn2;

        js_PushStatement(tc, &stmtInfo, STMT_WITH, -1);
        pn2 = Statement(cx, ts, tc);
        if (!pn2)
            return NULL;
        js_PopStatement(tc);

        /* Deprecate after parsing, in case of WERROR option. */
        if (JS_HAS_STRICT_OPTION(cx) &&
            !js_ReportCompileErrorNumber(cx, ts, NULL,
                                         JSREPORT_WARNING | JSREPORT_STRICT,
                                         JSMSG_DEPRECATED_USAGE,
                                         js_with_statement_str)) {
            return NULL;
        }

        pn->pn_pos.end = pn2->pn_pos.end;
        pn->pn_right = pn2;
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
        return pn;

      case TOK_VAR:
        pn = Variables(cx, ts, tc);
        if (!pn)
            return NULL;
        if (pn->pn_pos.end.lineno == ts->lineno &&
            !WellTerminated(cx, ts, TOK_ERROR)) {
            return NULL;
        }
        /* Tell js_EmitTree to generate a final POP. */
        pn->pn_extra = JS_TRUE;
        break;

      case TOK_RETURN:
        if (!(tc->flags & TCF_IN_FUNCTION)) {
            js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                        JSMSG_BAD_RETURN);
            return NULL;
        }
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!pn)
            return NULL;

        /* This is ugly, but we don't want to require a semicolon. */
        ts->flags |= TSF_REGEXP;
        tt = js_PeekTokenSameLine(cx, ts);
        ts->flags &= ~TSF_REGEXP;
        if (tt == TOK_ERROR)
            return NULL;

        if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
            pn2 = Expr(cx, ts, tc);
            if (!pn2)
                return NULL;
            if (pn2->pn_pos.end.lineno == ts->lineno &&
                !WellTerminated(cx, ts, TOK_ERROR)) {
                return NULL;
            }
            tc->flags |= TCF_RETURN_EXPR;
            pn->pn_pos.end = pn2->pn_pos.end;
            pn->pn_kid = pn2;
        } else {
            tc->flags |= TCF_RETURN_VOID;
            pn->pn_kid = NULL;
        }

        if (JS_HAS_STRICT_OPTION(cx) &&
            (~tc->flags & (TCF_RETURN_EXPR | TCF_RETURN_VOID)) == 0) {
            /*
             * We must be in a frame with a non-native function, because
             * we're compiling one.
             */
            if (!ReportNoReturnValue(cx, ts))
                return NULL;
        }
        break;

      case TOK_LC:
        js_PushStatement(tc, &stmtInfo, STMT_BLOCK, -1);
        pn = Statements(cx, ts, tc);
        if (!pn)
            return NULL;

        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_IN_COMPOUND);
        js_PopStatement(tc);
        return pn;

      case TOK_EOL:
      case TOK_SEMI:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!pn)
            return NULL;
        pn->pn_type = TOK_SEMI;
        pn->pn_kid = NULL;
        return pn;

#if JS_HAS_DEBUGGER_KEYWORD
      case TOK_DEBUGGER:
        if (!WellTerminated(cx, ts, TOK_ERROR))
            return NULL;
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
        if (!pn)
            return NULL;
        pn->pn_type = TOK_DEBUGGER;
        tc->flags |= TCF_FUN_HEAVYWEIGHT;
        return pn;
#endif /* JS_HAS_DEBUGGER_KEYWORD */

      case TOK_ERROR:
        return NULL;

      default:
        lastExprType = CURRENT_TOKEN(ts).type;
        js_UngetToken(ts);
        pn2 = Expr(cx, ts, tc);
        if (!pn2)
            return NULL;

        if (js_PeekToken(cx, ts) == TOK_COLON) {
            if (pn2->pn_type != TOK_NAME) {
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_BAD_LABEL);
                return NULL;
            }
            label = pn2->pn_atom;
            for (stmt = tc->topStmt; stmt; stmt = stmt->down) {
                if (stmt->type == STMT_LABEL && stmt->label == label) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_DUPLICATE_LABEL);
                    return NULL;
                }
            }
            (void) js_GetToken(cx, ts);

            /* Push a label struct and parse the statement. */
            js_PushStatement(tc, &stmtInfo, STMT_LABEL, -1);
            stmtInfo.label = label;
            pn = Statement(cx, ts, tc);
            if (!pn)
                return NULL;

            /* Pop the label, set pn_expr, and return early. */
            js_PopStatement(tc);
            pn2->pn_type = TOK_COLON;
            pn2->pn_pos.end = pn->pn_pos.end;
            pn2->pn_expr = pn;
            return pn2;
        }

        /* Check termination of (possibly multi-line) function expression. */
        if (pn2->pn_pos.end.lineno == ts->lineno &&
            !WellTerminated(cx, ts, lastExprType)) {
            return NULL;
        }

        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!pn)
            return NULL;
        pn->pn_type = TOK_SEMI;
        pn->pn_pos = pn2->pn_pos;
        pn->pn_kid = pn2;
        break;
    }

    (void) js_MatchToken(cx, ts, TOK_SEMI);
    return pn;
}

static JSParseNode *
Variables(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2;
    JSObject *obj, *pobj;
    JSStackFrame *fp;
    JSFunction *fun;
    JSClass *clasp;
    JSPropertyOp getter, setter, currentGetter, currentSetter;
    JSAtom *atom;
    JSAtomListElement *ale;
    JSOp prevop;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSBool ok;

    /*
     * The tricky part of this code is to create special parsenode opcodes for
     * getting and setting variables (which will be stored as special slots in
     * the frame).  The complex special case is an eval() inside a function.
     * If the evaluated string references variables in the enclosing function,
     * then we need to generate the special variable opcodes.  We determine
     * this by looking up the variable id in the current variable scope.
     */
    JS_ASSERT(CURRENT_TOKEN(ts).type == TOK_VAR);
    pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
    if (!pn)
        return NULL;
    pn->pn_op = CURRENT_TOKEN(ts).t_op;
    pn->pn_extra = JS_FALSE;            /* assume no JSOP_POP needed */
    PN_INIT_LIST(pn);

    /*
     * Skip eval and debugger frames when looking for the function whose code
     * is being compiled.  If we are called from FunctionBody, TCF_IN_FUNCTION
     * will be set in tc->flags, and we can be sure fp->fun is the function to
     * use.  But if a function calls eval, the string argument is treated as a
     * Program (per ECMA), so TCF_IN_FUNCTION won't be set.
     *
     * What's more, when the following code is reached from eval, cx->fp->fun
     * is eval's JSFunction (a native function), so we need to skip its frame.
     * We should find the scripted caller's function frame just below it, but
     * we code a loop out of paranoia.
     */
    for (fp = cx->fp; (fp->flags & JSFRAME_SPECIAL) && fp->down; fp = fp->down)
        continue;
    obj = fp->varobj;
    fun = fp->fun;
    clasp = OBJ_GET_CLASS(cx, obj);
    if (fun && clasp == &js_FunctionClass) {
        /* We are compiling code inside a function */
        getter = js_GetLocalVariable;
        setter = js_SetLocalVariable;
    } else if (fun && clasp == &js_CallClass) {
        /* We are compiling code from an eval inside a function */
        getter = js_GetCallVariable;
        setter = js_SetCallVariable;
    } else {
        getter = clasp->getProperty;
        setter = clasp->setProperty;
    }

    ok = JS_TRUE;
    do {
        currentGetter = getter;
        currentSetter = setter;
        MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_VARIABLE_NAME);
        atom = CURRENT_TOKEN(ts).t_atom;

        ATOM_LIST_SEARCH(ale, &tc->decls, atom);
        if (ale) {
            prevop = ALE_JSOP(ale);
            if ((JS_HAS_STRICT_OPTION(cx) ||
                 pn->pn_op == JSOP_DEFCONST ||
                 prevop == JSOP_DEFCONST) &&
                !js_ReportCompileErrorNumber(cx, ts, NULL,
                                             (pn->pn_op != JSOP_DEFCONST &&
                                              prevop != JSOP_DEFCONST)
                                             ? JSREPORT_WARNING|JSREPORT_STRICT
                                             : JSREPORT_ERROR,
                                             JSMSG_REDECLARED_VAR,
                                             (prevop == JSOP_DEFFUN ||
                                              prevop == JSOP_CLOSURE)
                                             ? js_function_str
                                             : (prevop == JSOP_DEFCONST)
                                             ? js_const_str
                                             : js_var_str,
                                             ATOM_BYTES(atom))) {
                return NULL;
            }
            if (pn->pn_op == JSOP_DEFVAR && prevop == JSOP_CLOSURE)
                tc->flags |= TCF_FUN_CLOSURE_VS_VAR;
        } else {
            ale = js_IndexAtom(cx, atom, &tc->decls);
            if (!ale)
                return NULL;
        }
        ALE_SET_JSOP(ale, pn->pn_op);

        pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NAME, tc);
        if (!pn2)
            return NULL;
        pn2->pn_op = JSOP_NAME;
        pn2->pn_atom = atom;
        pn2->pn_expr = NULL;
        pn2->pn_slot = -1;
        pn2->pn_attrs = (pn->pn_op == JSOP_DEFCONST)
                        ? JSPROP_ENUMERATE | JSPROP_PERMANENT |
                          JSPROP_READONLY
                        : JSPROP_ENUMERATE | JSPROP_PERMANENT;
        PN_APPEND(pn, pn2);

        if (!OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &pobj, &prop))
            return NULL;
        if (pobj == obj &&
            OBJ_IS_NATIVE(pobj) &&
            (sprop = (JSScopeProperty *)prop) != NULL) {
            if (SPROP_GETTER(sprop, pobj) == js_GetArgument) {
                if (pn->pn_op == JSOP_DEFCONST) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_REDECLARED_PARAM,
                                                ATOM_BYTES(atom));
                    return NULL;
                }
                currentGetter = js_GetArgument;
                currentSetter = js_SetArgument;
                if (JS_HAS_STRICT_OPTION(cx)) {
                    ok = js_ReportCompileErrorNumber(cx, ts, NULL,
                                                     JSREPORT_WARNING |
                                                     JSREPORT_STRICT,
                                                     JSMSG_VAR_HIDES_ARG,
                                                     ATOM_BYTES(atom));
                }
            } else {
                ok = JS_TRUE;
                if (fun) {
                    /* Not an argument, must be a redeclared local var. */
                    if (clasp == &js_FunctionClass) {
                        JS_ASSERT(SPROP_GETTER(sprop,pobj) == js_GetLocalVariable);
                        JS_ASSERT(JSVAL_IS_INT(sprop->id) &&
                                  JSVAL_TO_INT(sprop->id) < fun->nvars);
                    } else if (clasp == &js_CallClass) {
                        if (SPROP_GETTER(sprop, pobj) == js_GetCallVariable) {
                            /*
                             * Referencing a variable introduced by a var
                             * statement in the enclosing function. Check
                             * that the slot number we have is in range.
                             */
                            JS_ASSERT(JSVAL_IS_INT(sprop->id) &&
                                      JSVAL_TO_INT(sprop->id) < fun->nvars);
                        } else {
                            /*
                             * A variable introduced through another eval:
                             * don't use the special getters and setters
                             * since we can't allocate a slot in the frame.
                             */
                            currentGetter = SPROP_GETTER(sprop, pobj);
                            currentSetter = SPROP_SETTER(sprop, pobj);
                        }
                    }

                    /* Override the old getter and setter, to handle eval. */
                    SPROP_GETTER(sprop, pobj) = currentGetter;
                    SPROP_SETTER(sprop, pobj) = currentSetter;
                } else {
                    /* Global var: (re-)set id a la js_DefineProperty. */
                    sprop->id = ATOM_KEY(atom);
                }
            }
        } else {
            /*
             * Property not found in current variable scope: we have not seen
             * this variable before.  Define a new local variable by adding a
             * property to the function's scope, allocating one slot in the
             * function's frame.  Global variables and any locals declared in
             * with statement bodies are handled at runtime, by script prolog
             * JSOP_DEFVAR bytecodes generated for slot-less vars.
             */
            sprop = NULL;
            if (prop) {
                OBJ_DROP_PROPERTY(cx, pobj, prop);
                prop = NULL;
            }
            if (currentGetter == js_GetCallVariable) {
                /* Can't increase fun->nvars in an active frame! */
                currentGetter = clasp->getProperty;
                currentSetter = clasp->setProperty;
            }
            if (currentGetter == js_GetLocalVariable &&
                atom != cx->runtime->atomState.argumentsAtom &&
                fp->scopeChain == obj &&
                !js_InWithStatement(tc)) {
                ok = OBJ_DEFINE_PROPERTY(cx, obj, (jsid)atom, JSVAL_VOID,
                                         currentGetter, currentSetter,
                                         pn2->pn_attrs, &prop);
                if (ok) {
                    pobj = obj;

                    /*
                     * Allocate more room for variables in the function's
                     * frame.  We can do this only before the function is
                     * first called.
                     */
                    sprop = (JSScopeProperty *)prop;
                    sprop->id = INT_TO_JSVAL(fun->nvars++);
                }
            }
        }

        if (js_MatchToken(cx, ts, TOK_ASSIGN)) {
            if (CURRENT_TOKEN(ts).t_op != JSOP_NOP) {
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_BAD_VAR_INIT);
                ok = JS_FALSE;
            } else {
                pn2->pn_expr = AssignExpr(cx, ts, tc);
                if (!pn2->pn_expr) {
                    ok = JS_FALSE;
                } else {
                    pn2->pn_op = (pn->pn_op == JSOP_DEFCONST)
                                 ? JSOP_SETCONST
                                 : JSOP_SETNAME;
                    if (atom == cx->runtime->atomState.argumentsAtom)
                        tc->flags |= TCF_FUN_HEAVYWEIGHT;
                }
            }
        }

        if (prop)
            OBJ_DROP_PROPERTY(cx, pobj, prop);
        if (!ok)
            return NULL;
    } while (js_MatchToken(cx, ts, TOK_COMMA));

    pn->pn_pos.end = PN_LAST(pn)->pn_pos.end;
    return pn;
}

static JSParseNode *
Expr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2;

    pn = AssignExpr(cx, ts, tc);
    if (pn && js_MatchToken(cx, ts, TOK_COMMA)) {
        pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
        if (!pn2)
            return NULL;
        pn2->pn_pos.begin = pn->pn_pos.begin;
        PN_INIT_LIST_1(pn2, pn);
        pn = pn2;
        do {
            pn2 = AssignExpr(cx, ts, tc);
            if (!pn2)
                return NULL;
            PN_APPEND(pn, pn2);
        } while (js_MatchToken(cx, ts, TOK_COMMA));
        pn->pn_pos.end = PN_LAST(pn)->pn_pos.end;
    }
    return pn;
}

static JSParseNode *
AssignExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2;
    JSTokenType tt;
    JSOp op;

    pn = CondExpr(cx, ts, tc);
    if (!pn)
        return NULL;

    tt = js_GetToken(cx, ts);
#if JS_HAS_GETTER_SETTER
    if (tt == TOK_NAME) {
        tt = CheckGetterOrSetter(cx, ts, TOK_ASSIGN);
        if (tt == TOK_ERROR)
            return NULL;
    }
#endif
    if (tt != TOK_ASSIGN) {
        js_UngetToken(ts);
        return pn;
    }

    op = CURRENT_TOKEN(ts).t_op;
    for (pn2 = pn; pn2->pn_type == TOK_RP; pn2 = pn2->pn_kid)
        ;
    switch (pn2->pn_type) {
      case TOK_NAME:
        pn2->pn_op = JSOP_SETNAME;
        if (pn2->pn_atom == cx->runtime->atomState.argumentsAtom)
            tc->flags |= TCF_FUN_HEAVYWEIGHT;
        break;
      case TOK_DOT:
        pn2->pn_op = JSOP_SETPROP;
        break;
      case TOK_LB:
        pn2->pn_op = JSOP_SETELEM;
        break;
#if JS_HAS_LVALUE_RETURN
      case TOK_LP:
        pn2->pn_op = JSOP_SETCALL;
        break;
#endif
      default:
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_BAD_LEFTSIDE_OF_ASS);
        return NULL;
    }
    pn = NewBinary(cx, TOK_ASSIGN, op, pn2, AssignExpr(cx, ts, tc), tc);
    return pn;
}

static JSParseNode *
CondExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn1, *pn2, *pn3;
#if JS_HAS_IN_OPERATOR
    uintN oldflags;
#endif /* JS_HAS_IN_OPERATOR */

    pn = OrExpr(cx, ts, tc);
    if (pn && js_MatchToken(cx, ts, TOK_HOOK)) {
        pn1 = pn;
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_TERNARY, tc);
        if (!pn)
            return NULL;
#if JS_HAS_IN_OPERATOR
        /*
         * Always accept the 'in' operator in the middle clause of a ternary,
         * where it's unambiguous, even if we might be parsing the init of a
         * for statement.
         */
        oldflags = tc->flags;
        tc->flags &= ~TCF_IN_FOR_INIT;
#endif /* JS_HAS_IN_OPERATOR */
        pn2 = AssignExpr(cx, ts, tc);
#if JS_HAS_IN_OPERATOR
        tc->flags = oldflags | (tc->flags & TCF_FUN_FLAGS);
#endif /* JS_HAS_IN_OPERATOR */

        if (!pn2)
            return NULL;
        MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_IN_COND);
        pn3 = AssignExpr(cx, ts, tc);
        if (!pn3)
            return NULL;
        pn->pn_pos.begin = pn1->pn_pos.begin;
        pn->pn_pos.end = pn3->pn_pos.end;
        pn->pn_kid1 = pn1;
        pn->pn_kid2 = pn2;
        pn->pn_kid3 = pn3;
    }
    return pn;
}

static JSParseNode *
OrExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = AndExpr(cx, ts, tc);
    if (pn && js_MatchToken(cx, ts, TOK_OR))
        pn = NewBinary(cx, TOK_OR, JSOP_OR, pn, OrExpr(cx, ts, tc), tc);
    return pn;
}

static JSParseNode *
AndExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = BitOrExpr(cx, ts, tc);
    if (pn && js_MatchToken(cx, ts, TOK_AND))
        pn = NewBinary(cx, TOK_AND, JSOP_AND, pn, AndExpr(cx, ts, tc), tc);
    return pn;
}

static JSParseNode *
BitOrExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = BitXorExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_BITOR)) {
        pn = NewBinary(cx, TOK_BITOR, JSOP_BITOR, pn, BitXorExpr(cx, ts, tc),
                       tc);
    }
    return pn;
}

static JSParseNode *
BitXorExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = BitAndExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_BITXOR)) {
        pn = NewBinary(cx, TOK_BITXOR, JSOP_BITXOR, pn, BitAndExpr(cx, ts, tc),
                       tc);
    }
    return pn;
}

static JSParseNode *
BitAndExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = EqExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_BITAND))
        pn = NewBinary(cx, TOK_BITAND, JSOP_BITAND, pn, EqExpr(cx, ts, tc), tc);
    return pn;
}

static JSParseNode *
EqExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;
    JSOp op;

    pn = RelExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_EQOP)) {
        op = CURRENT_TOKEN(ts).t_op;
        pn = NewBinary(cx, TOK_EQOP, op, pn, RelExpr(cx, ts, tc), tc);
    }
    return pn;
}

static JSParseNode *
RelExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;
    JSTokenType tt;
    JSOp op;
#if JS_HAS_IN_OPERATOR
    uintN inForInitFlag;

    inForInitFlag = tc->flags & TCF_IN_FOR_INIT;
    /*
     * Uses of the in operator in ShiftExprs are always unambiguous,
     * so unset the flag that prohibits recognizing it.
     */
    tc->flags &= ~TCF_IN_FOR_INIT;
#endif /* JS_HAS_IN_OPERATOR */

    pn = ShiftExpr(cx, ts, tc);
    while (pn &&
           (js_MatchToken(cx, ts, TOK_RELOP)
#if JS_HAS_IN_OPERATOR
            /*
             * Recognize the 'in' token as an operator only if we're not
             * currently in the init expr of a for loop.
             */
            || (inForInitFlag == 0 && js_MatchToken(cx, ts, TOK_IN))
#endif /* JS_HAS_IN_OPERATOR */
#if JS_HAS_INSTANCEOF
            || js_MatchToken(cx, ts, TOK_INSTANCEOF)
#endif /* JS_HAS_INSTANCEOF */
            )) {
        tt = CURRENT_TOKEN(ts).type;
        op = CURRENT_TOKEN(ts).t_op;
        pn = NewBinary(cx, tt, op, pn, ShiftExpr(cx, ts, tc), tc);
    }
#if JS_HAS_IN_OPERATOR
    /* Restore previous state of inForInit flag. */
    tc->flags |= inForInitFlag;
#endif /* JS_HAS_IN_OPERATOR */

    return pn;
}

static JSParseNode *
ShiftExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;
    JSOp op;

    pn = AddExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_SHOP)) {
        op = CURRENT_TOKEN(ts).t_op;
        pn = NewBinary(cx, TOK_SHOP, op, pn, AddExpr(cx, ts, tc), tc);
    }
    return pn;
}

static JSParseNode *
AddExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;
    JSTokenType tt;
    JSOp op;

    pn = MulExpr(cx, ts, tc);
    while (pn &&
           (js_MatchToken(cx, ts, TOK_PLUS) ||
            js_MatchToken(cx, ts, TOK_MINUS))) {
        tt = CURRENT_TOKEN(ts).type;
        op = (tt == TOK_PLUS) ? JSOP_ADD : JSOP_SUB;
        pn = NewBinary(cx, tt, op, pn, MulExpr(cx, ts, tc), tc);
    }
    return pn;
}

static JSParseNode *
MulExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;
    JSTokenType tt;
    JSOp op;

    pn = UnaryExpr(cx, ts, tc);
    while (pn &&
           (js_MatchToken(cx, ts, TOK_STAR) ||
            js_MatchToken(cx, ts, TOK_DIVOP))) {
        tt = CURRENT_TOKEN(ts).type;
        op = CURRENT_TOKEN(ts).t_op;
        pn = NewBinary(cx, tt, op, pn, UnaryExpr(cx, ts, tc), tc);
    }
    return pn;
}

static JSParseNode *
SetLvalKid(JSContext *cx, JSTokenStream *ts, JSParseNode *pn, JSParseNode *kid,
           const char *name)
{
    while (kid->pn_type == TOK_RP)
        kid = kid->pn_kid;
    if (kid->pn_type != TOK_NAME &&
        kid->pn_type != TOK_DOT &&
#if JS_HAS_LVALUE_RETURN
        (kid->pn_type != TOK_LP || kid->pn_op != JSOP_CALL) &&
#endif
        kid->pn_type != TOK_LB) {
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_BAD_OPERAND, name);
        return NULL;
    }
    pn->pn_kid = kid;
    return kid;
}

static const char *incop_name_str[] = {"increment", "decrement"};

static JSBool
SetIncOpKid(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc,
            JSParseNode *pn, JSParseNode *kid,
            JSTokenType tt, JSBool preorder)
{
    JSOp op;

    kid = SetLvalKid(cx, ts, pn, kid, incop_name_str[tt == TOK_DEC]);
    if (!kid)
        return JS_FALSE;
    switch (kid->pn_type) {
      case TOK_NAME:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCNAME : JSOP_NAMEINC)
             : (preorder ? JSOP_DECNAME : JSOP_NAMEDEC);
        if (kid->pn_atom == cx->runtime->atomState.argumentsAtom)
            tc->flags |= TCF_FUN_HEAVYWEIGHT;
        break;

      case TOK_DOT:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCPROP : JSOP_PROPINC)
             : (preorder ? JSOP_DECPROP : JSOP_PROPDEC);
        break;

#if JS_HAS_LVALUE_RETURN
      case TOK_LP:
        kid->pn_op = JSOP_SETCALL;
#endif
      case TOK_LB:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCELEM : JSOP_ELEMINC)
             : (preorder ? JSOP_DECELEM : JSOP_ELEMDEC);
        break;

      default:
        JS_ASSERT(0);
        op = JSOP_NOP;
    }
    pn->pn_op = op;
    return JS_TRUE;
}

static JSParseNode *
UnaryExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSTokenType tt;
    JSParseNode *pn, *pn2;

    ts->flags |= TSF_REGEXP;
    tt = js_GetToken(cx, ts);
    ts->flags &= ~TSF_REGEXP;

    switch (tt) {
      case TOK_UNARYOP:
      case TOK_PLUS:
      case TOK_MINUS:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!pn)
            return NULL;
        pn->pn_type = TOK_UNARYOP;      /* PLUS and MINUS are binary */
        pn->pn_op = CURRENT_TOKEN(ts).t_op;
        pn2 = UnaryExpr(cx, ts, tc);
        if (!pn2)
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->pn_kid = pn2;
        break;

      case TOK_INC:
      case TOK_DEC:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!pn)
            return NULL;
        pn2 = MemberExpr(cx, ts, tc, JS_TRUE);
        if (!pn2)
            return NULL;
        if (!SetIncOpKid(cx, ts, tc, pn, pn2, tt, JS_TRUE))
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;
        break;

      case TOK_DELETE:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!pn)
            return NULL;
        pn2 = UnaryExpr(cx, ts, tc);
        if (!pn2)
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;

        /*
         * Under ECMA3, deleting any unary expression is valid -- it simply
         * returns true. Here we strip off any parentheses.
         */
        while (pn2->pn_type == TOK_RP)
            pn2 = pn2->pn_kid;
        pn->pn_kid = pn2;
        break;

      case TOK_ERROR:
        return NULL;

      default:
        js_UngetToken(ts);
        pn = MemberExpr(cx, ts, tc, JS_TRUE);
        if (!pn)
            return NULL;

        /* Don't look across a newline boundary for a postfix incop. */
        if (pn->pn_pos.end.lineno == ts->lineno) {
            tt = js_PeekTokenSameLine(cx, ts);
            if (tt == TOK_INC || tt == TOK_DEC) {
                (void) js_GetToken(cx, ts);
                pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
                if (!pn2)
                    return NULL;
                if (!SetIncOpKid(cx, ts, tc, pn2, pn, tt, JS_FALSE))
                    return NULL;
                pn2->pn_pos.begin = pn->pn_pos.begin;
                pn = pn2;
            }
        }
        break;
    }
    return pn;
}

static JSParseNode *
ArgumentList(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc,
             JSParseNode *listNode)
{
    JSBool matched;

    ts->flags |= TSF_REGEXP;
    matched = js_MatchToken(cx, ts, TOK_RP);
    ts->flags &= ~TSF_REGEXP;
    if (!matched) {
        do {
            JSParseNode *argNode = AssignExpr(cx, ts, tc);
            if (!argNode)
                return NULL;
            PN_APPEND(listNode, argNode);
        } while (js_MatchToken(cx, ts, TOK_COMMA));

        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_ARGS);
    }
    return listNode;
}

static JSParseNode *
MemberExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc,
           JSBool allowCallSyntax)
{
    JSParseNode *pn, *pn2, *pn3;
    JSTokenType tt;

    /* Check for new expression first. */
    ts->flags |= TSF_REGEXP;
    tt = js_PeekToken(cx, ts);
    ts->flags &= ~TSF_REGEXP;
    if (tt == TOK_NEW) {
        (void) js_GetToken(cx, ts);

        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
        if (!pn)
            return NULL;
        pn2 = MemberExpr(cx, ts, tc, JS_FALSE);
        if (!pn2)
            return NULL;
        pn->pn_op = JSOP_NEW;
        PN_INIT_LIST_1(pn, pn2);

        if (js_MatchToken(cx, ts, TOK_LP)) {
            pn = ArgumentList(cx, ts, tc, pn);
            if (!pn)
                return NULL;
        }
        if (pn->pn_count - 1 >= ARGC_LIMIT) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_TOO_MANY_CON_ARGS);
            return NULL;
        }
        pn->pn_pos.end = PN_LAST(pn)->pn_pos.end;
    } else {
        pn = PrimaryExpr(cx, ts, tc);
        if (!pn)
            return NULL;
    }

    while ((tt = js_GetToken(cx, ts)) > TOK_EOF) {
        if (tt == TOK_DOT) {
            pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NAME, tc);
            if (!pn2)
                return NULL;
            MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NAME_AFTER_DOT);
            pn2->pn_pos.begin = pn->pn_pos.begin;
            pn2->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
            pn2->pn_op = JSOP_GETPROP;
            pn2->pn_expr = pn;
            pn2->pn_atom = CURRENT_TOKEN(ts).t_atom;
        } else if (tt == TOK_LB) {
            pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_BINARY, tc);
            if (!pn2)
                return NULL;
            pn3 = Expr(cx, ts, tc);
            if (!pn3)
                return NULL;

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);
            pn2->pn_pos.begin = pn->pn_pos.begin;
            pn2->pn_pos.end = CURRENT_TOKEN(ts).pos.end;

            /* Optimize o['p'] to o.p by rewriting pn2. */
            if (pn3->pn_type == TOK_STRING) {
                pn2->pn_type = TOK_DOT;
                pn2->pn_op = JSOP_GETPROP;
                pn2->pn_arity = PN_NAME;
                pn2->pn_expr = pn;
                pn2->pn_atom = pn3->pn_atom;
            } else {
                pn2->pn_op = JSOP_GETELEM;
                pn2->pn_left = pn;
                pn2->pn_right = pn3;
            }
        } else if (allowCallSyntax && tt == TOK_LP) {
            pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
            if (!pn2)
                return NULL;

            /* Pick JSOP_EVAL and flag tc as heavyweight if eval(...). */
            pn2->pn_op = JSOP_CALL;
            if (pn->pn_op == JSOP_NAME &&
                pn->pn_atom == cx->runtime->atomState.evalAtom) {
                pn2->pn_op = JSOP_EVAL;
                tc->flags |= TCF_FUN_HEAVYWEIGHT;
            }

            PN_INIT_LIST_1(pn2, pn);

            pn2 = ArgumentList(cx, ts, tc, pn2);
            if (!pn2)
                return NULL;
            if (pn2->pn_count - 1 >= ARGC_LIMIT) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_TOO_MANY_FUN_ARGS);
                return NULL;
            }
            pn2->pn_pos.end = PN_LAST(pn2)->pn_pos.end;
        } else {
            js_UngetToken(ts);
            return pn;
        }

        pn = pn2;
    }
    if (tt == TOK_ERROR)
        return NULL;
    return pn;
}

static JSParseNode *
PrimaryExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSTokenType tt;
    JSParseNode *pn, *pn2, *pn3;
    char *badWord;
#if JS_HAS_GETTER_SETTER
    JSAtom *atom;
    JSRuntime *rt;
#endif

#if JS_HAS_SHARP_VARS
    JSParseNode *defsharp;
    JSBool notsharp;

    defsharp = NULL;
    notsharp = JS_FALSE;
  again:
    /*
     * Control flows here after #n= is scanned.  If the following primary is
     * not valid after such a "sharp variable" definition, the token type case
     * should set notsharp.
     */
#endif

    ts->flags |= TSF_REGEXP;
    tt = js_GetToken(cx, ts);
    ts->flags &= ~TSF_REGEXP;

#if JS_HAS_GETTER_SETTER
    if (tt == TOK_NAME) {
        tt = CheckGetterOrSetter(cx, ts, TOK_FUNCTION);
        if (tt == TOK_ERROR)
            return NULL;
    }
#endif

    switch (tt) {
#if JS_HAS_LEXICAL_CLOSURE
      case TOK_FUNCTION:
        pn = FunctionExpr(cx, ts, tc);
        if (!pn)
            return NULL;
        break;
#endif

#if JS_HAS_INITIALIZERS
      case TOK_LB:
      {
        JSBool matched;
        jsuint atomIndex;

        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
        if (!pn)
            return NULL;
        pn->pn_type = TOK_RB;
        pn->pn_extra = JS_FALSE;

#if JS_HAS_SHARP_VARS
        if (defsharp) {
            PN_INIT_LIST_1(pn, defsharp);
            defsharp = NULL;
        } else
#endif
            PN_INIT_LIST(pn);

        ts->flags |= TSF_REGEXP;
        matched = js_MatchToken(cx, ts, TOK_RB);
        ts->flags &= ~TSF_REGEXP;
        if (!matched) {
            for (atomIndex = 0; atomIndex < ATOM_INDEX_LIMIT; atomIndex++) {
                ts->flags |= TSF_REGEXP;
                tt = js_PeekToken(cx, ts);
                ts->flags &= ~TSF_REGEXP;
                if (tt == TOK_RB) {
                    pn->pn_extra = JS_TRUE;
                    break;
                }

                if (tt == TOK_COMMA) {
                    /* So CURRENT_TOKEN gets TOK_COMMA and not TOK_LB. */
                    js_MatchToken(cx, ts, TOK_COMMA);
                    pn2 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
                } else
                    pn2 = AssignExpr(cx, ts, tc);
                if (!pn2)
                    return NULL;
                PN_APPEND(pn, pn2);

                if (tt != TOK_COMMA) {
                    /* If we didn't already match TOK_COMMA in above case. */
                    if (!js_MatchToken(cx, ts, TOK_COMMA))
                        break;
                }
            }

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_LIST);
        }
        pn->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
        return pn;
      }

      case TOK_LC:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_LIST, tc);
        if (!pn)
            return NULL;
        pn->pn_type = TOK_RC;

#if JS_HAS_SHARP_VARS
        if (defsharp) {
            PN_INIT_LIST_1(pn, defsharp);
            defsharp = NULL;
        } else
#endif
            PN_INIT_LIST(pn);

        if (!js_MatchToken(cx, ts, TOK_RC)) {
            do {
                JSOp op;

                tt = js_GetToken(cx, ts);
                switch (tt) {
                  case TOK_NUMBER:
                    pn3 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
                    if (pn3)
                        pn3->pn_dval = CURRENT_TOKEN(ts).t_dval;
                    break;
                  case TOK_NAME:
#if JS_HAS_GETTER_SETTER
                    atom = CURRENT_TOKEN(ts).t_atom;
                    rt = cx->runtime;
                    if (atom == rt->atomState.getAtom ||
                        atom == rt->atomState.setAtom) {
                        op = (atom == rt->atomState.getAtom)
                             ? JSOP_GETTER
                             : JSOP_SETTER;
                        if (js_MatchToken(cx, ts, TOK_NAME)) {
                            pn3 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NAME,
                                               tc);
                            if (!pn3)
                                return NULL;
                            pn3->pn_atom = CURRENT_TOKEN(ts).t_atom;
                            pn3->pn_expr = NULL;

                            /* We have to fake a 'function' token here. */
                            CURRENT_TOKEN(ts).t_op = JSOP_NOP;
                            CURRENT_TOKEN(ts).type = TOK_FUNCTION;
                            pn2 = FunctionExpr(cx, ts, tc);
                            pn2 = NewBinary(cx, TOK_COLON, op, pn3, pn2, tc);
                            goto skip;
                        }
                    }
                    /* else fall thru ... */
#endif
                  case TOK_STRING:
                    pn3 = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
                    if (pn3)
                        pn3->pn_atom = CURRENT_TOKEN(ts).t_atom;
                    break;
                  case TOK_RC:
                    if (JS_HAS_STRICT_OPTION(cx) &&
                        !js_ReportCompileErrorNumber(cx, ts, NULL,
                                                     JSREPORT_WARNING |
                                                     JSREPORT_STRICT,
                                                     JSMSG_TRAILING_COMMA)) {
                        return NULL;
                    }
                    goto end_obj_init;
                  default:
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_BAD_PROP_ID);
                    return NULL;
                }

                tt = js_GetToken(cx, ts);
#if JS_HAS_GETTER_SETTER
                if (tt == TOK_NAME) {
                    tt = CheckGetterOrSetter(cx, ts, TOK_COLON);
                    if (tt == TOK_ERROR)
                        return NULL;
                }
#endif
                if (tt != TOK_COLON) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_COLON_AFTER_ID);
                    return NULL;
                }
                op = CURRENT_TOKEN(ts).t_op;
                pn2 = NewBinary(cx, TOK_COLON, op, pn3, AssignExpr(cx, ts, tc),
                                tc);
#if JS_HAS_GETTER_SETTER
              skip:
#endif
                if (!pn2)
                    return NULL;
                PN_APPEND(pn, pn2);
            } while (js_MatchToken(cx, ts, TOK_COMMA));

            MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_LIST);
        }
      end_obj_init:
        pn->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
        return pn;

#if JS_HAS_SHARP_VARS
      case TOK_DEFSHARP:
        if (defsharp)
            goto badsharp;
        defsharp = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!defsharp)
            return NULL;
        defsharp->pn_kid = NULL;
        defsharp->pn_num = (jsint) CURRENT_TOKEN(ts).t_dval;
        goto again;

      case TOK_USESHARP:
        /* Check for forward/dangling references at runtime, to allow eval. */
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
        if (!pn)
            return NULL;
        pn->pn_num = (jsint) CURRENT_TOKEN(ts).t_dval;
        notsharp = JS_TRUE;
        break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_INITIALIZERS */

      case TOK_LP:
      {
#if JS_HAS_IN_OPERATOR
        uintN oldflags;
#endif
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_UNARY, tc);
        if (!pn)
            return NULL;
#if JS_HAS_IN_OPERATOR
        /*
         * Always accept the 'in' operator in a parenthesized expression,
         * where it's unambiguous, even if we might be parsing the init of a
         * for statement.
         */
        oldflags = tc->flags;
        tc->flags &= ~TCF_IN_FOR_INIT;
#endif
        pn2 = Expr(cx, ts, tc);
#if JS_HAS_IN_OPERATOR
        tc->flags = oldflags | (tc->flags & TCF_FUN_FLAGS);
#endif
        if (!pn2)
            return NULL;

        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);
        pn->pn_type = TOK_RP;
        pn->pn_pos.end = CURRENT_TOKEN(ts).pos.end;
        pn->pn_kid = pn2;
        break;
      }

      case TOK_STRING:
#if JS_HAS_SHARP_VARS
        notsharp = JS_TRUE;
#endif
        /* FALL THROUGH */
      case TOK_NAME:
      case TOK_OBJECT:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
        if (!pn)
            return NULL;
        pn->pn_op = CURRENT_TOKEN(ts).t_op;
        pn->pn_atom = CURRENT_TOKEN(ts).t_atom;
        if (tt == TOK_NAME) {
            pn->pn_arity = PN_NAME;
            pn->pn_expr = NULL;
            pn->pn_slot = -1;
            pn->pn_attrs = 0;

            /* Unqualified __parent__ and __proto__ uses require activations. */
            if (pn->pn_atom == cx->runtime->atomState.parentAtom ||
                pn->pn_atom == cx->runtime->atomState.protoAtom) {
                tc->flags |= TCF_FUN_HEAVYWEIGHT;
            }
        }
        break;

      case TOK_NUMBER:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
        if (!pn)
            return NULL;
        pn->pn_dval = CURRENT_TOKEN(ts).t_dval;
#if JS_HAS_SHARP_VARS
        notsharp = JS_TRUE;
#endif
        break;

      case TOK_PRIMARY:
        pn = NewParseNode(cx, &CURRENT_TOKEN(ts), PN_NULLARY, tc);
        if (!pn)
            return NULL;
        pn->pn_op = CURRENT_TOKEN(ts).t_op;
#if JS_HAS_SHARP_VARS
        notsharp = JS_TRUE;
#endif
        break;

#if !JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
      case TOK_IMPORT:
#endif
      case TOK_RESERVED:
        badWord = js_DeflateString(cx, CURRENT_TOKEN(ts).ptr,
                                   (size_t) CURRENT_TOKEN(ts).pos.end.index
                                          - CURRENT_TOKEN(ts).pos.begin.index);
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_RESERVED_ID, badWord);
        JS_free(cx, badWord);
        return NULL;

      case TOK_ERROR:
        /* The scanner or one of its subroutines reported the error. */
        return NULL;

      default:
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_SYNTAX_ERROR);
        return NULL;
    }

#if JS_HAS_SHARP_VARS
    if (defsharp) {
        if (notsharp) {
  badsharp:
            js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                        JSMSG_BAD_SHARP_VAR_DEF);
            return NULL;
        }
        defsharp->pn_kid = pn;
        return defsharp;
    }
#endif
    return pn;
}

JSBool
js_FoldConstants(JSContext *cx, JSParseNode *pn, JSTreeContext *tc)
{
    JSParseNode *pn1 = NULL, *pn2 = NULL, *pn3 = NULL;

    switch (pn->pn_arity) {
      case PN_FUNC:
        if (!js_FoldConstants(cx, pn->pn_body, tc))
            return JS_FALSE;
        break;

      case PN_LIST:
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (!js_FoldConstants(cx, pn2, tc))
                return JS_FALSE;
        }
        break;

      case PN_TERNARY:
        /* Any kid may be null (e.g. for (;;)). */
        pn1 = pn->pn_kid1;
        pn2 = pn->pn_kid2;
        pn3 = pn->pn_kid3;
        if (pn1 && !js_FoldConstants(cx, pn1, tc))
            return JS_FALSE;
        if (pn2 && !js_FoldConstants(cx, pn2, tc))
            return JS_FALSE;
        if (pn3 && !js_FoldConstants(cx, pn3, tc))
            return JS_FALSE;
        break;

      case PN_BINARY:
        /* First kid may be null (for default case in switch). */
        pn1 = pn->pn_left;
        pn2 = pn->pn_right;
        if (pn1 && !js_FoldConstants(cx, pn1, tc))
            return JS_FALSE;
        if (!js_FoldConstants(cx, pn2, tc))
            return JS_FALSE;
        break;

      case PN_UNARY:
        /* Our kid may be null (e.g. return; vs. return e;). */
        pn1 = pn->pn_kid;
        if (pn1 && !js_FoldConstants(cx, pn1, tc))
            return JS_FALSE;
        break;

      case PN_NAME:
        pn1 = pn->pn_expr;
        if (pn1 && !js_FoldConstants(cx, pn1, tc))
            return JS_FALSE;
        break;

      case PN_NULLARY:
        break;
    }

    switch (pn->pn_type) {
      case TOK_IF:
      case TOK_HOOK:
        /* Reduce 'if (C) T; else E' into T for true C, E for false. */
        switch (pn1->pn_type) {
          case TOK_NUMBER:
            if (pn1->pn_dval == 0)
                pn2 = pn3;
            break;
          case TOK_STRING:
            if (JSSTRING_LENGTH(ATOM_TO_STRING(pn1->pn_atom)) == 0)
                pn2 = pn3;
            break;
          case TOK_PRIMARY:
            if (pn1->pn_op == JSOP_TRUE)
                break;
            if (pn1->pn_op == JSOP_FALSE || pn1->pn_op == JSOP_NULL) {
                pn2 = pn3;
                break;
            }
            /* FALL THROUGH */
          default:
            /* Early return to dodge common code that copies pn2 to pn. */
            return JS_TRUE;
        }

        if (pn2) {
            /* pn2 is the then- or else-statement subtree to compile. */
            PN_MOVE_NODE(pn, pn2);
        } else {
            /* False condition and no else: make pn an empty statement. */
            pn->pn_type = TOK_SEMI;
            pn->pn_arity = PN_UNARY;
            pn->pn_kid = NULL;
        }
        RecycleTree(pn2, tc);
        if (pn3 && pn3 != pn2)
            RecycleTree(pn3, tc);
        break;

      case TOK_PLUS:
        if (pn1->pn_type == TOK_STRING && pn2->pn_type == TOK_STRING) {
            JSString *left, *right, *str;

            left = ATOM_TO_STRING(pn1->pn_atom);
            right = ATOM_TO_STRING(pn2->pn_atom);
            str = js_ConcatStrings(cx, left, right);
            if (!str)
                return JS_FALSE;
            pn->pn_atom = js_AtomizeString(cx, str, 0);
            if (!pn->pn_atom)
                return JS_FALSE;
            pn->pn_type = TOK_STRING;
            pn->pn_op = JSOP_STRING;
            pn->pn_arity = PN_NULLARY;
            RecycleTree(pn1, tc);
            RecycleTree(pn2, tc);
            break;
        }
        /* FALL THROUGH */

      case TOK_STAR:
        /* The * in 'import *;' parses as a nullary star node. */
        if (pn->pn_arity == PN_NULLARY)
            break;
        /* FALL THROUGH */

      case TOK_SHOP:
      case TOK_MINUS:
      case TOK_DIVOP:
        if (pn1->pn_type == TOK_NUMBER && pn2->pn_type == TOK_NUMBER) {
            jsdouble d, d2;
            int32 i, j;
            uint32 u;

            /* Fold two numeric constants. */
            d = pn1->pn_dval;
            d2 = pn2->pn_dval;
            switch (pn->pn_op) {
              case JSOP_LSH:
              case JSOP_RSH:
                if (!js_DoubleToECMAInt32(cx, d, &i))
                    return JS_FALSE;
                if (!js_DoubleToECMAInt32(cx, d2, &j))
                    return JS_FALSE;
                j &= 31;
                d = (pn->pn_op == JSOP_LSH) ? i << j : i >> j;
                break;

              case JSOP_URSH:
                if (!js_DoubleToECMAUint32(cx, d, &u))
                    return JS_FALSE;
                if (!js_DoubleToECMAInt32(cx, d2, &j))
                    return JS_FALSE;
                j &= 31;
                d = u >> j;
                break;

              case JSOP_ADD:
                d += d2;
                break;

              case JSOP_SUB:
                d -= d2;
                break;

              case JSOP_MUL:
                d *= d2;
                break;

              case JSOP_DIV:
                if (d2 == 0) {
#ifdef XP_PC
                    /* XXX MSVC miscompiles such that (NaN == 0) */
                    if (JSDOUBLE_IS_NaN(d2))
                        d = *cx->runtime->jsNaN;
                    else
#endif
                    if (d == 0 || JSDOUBLE_IS_NaN(d))
                        d = *cx->runtime->jsNaN;
                    else if ((JSDOUBLE_HI32(d) ^ JSDOUBLE_HI32(d2)) >> 31)
                        d = *cx->runtime->jsNegativeInfinity;
                    else
                        d = *cx->runtime->jsPositiveInfinity;
                } else {
                    d /= d2;
                }
                break;

              case JSOP_MOD:
                if (d2 == 0) {
                    d = *cx->runtime->jsNaN;
                } else {
#ifdef XP_PC
                  /* Workaround MS fmod bug where 42 % (1/0) => NaN, not 42. */
                  if (!(JSDOUBLE_IS_FINITE(d) && JSDOUBLE_IS_INFINITE(d2)))
#endif
                    d = fmod(d, d2);
                }
                break;

              default:;
            }
            pn->pn_type = TOK_NUMBER;
            pn->pn_op = JSOP_NUMBER;
            pn->pn_arity = PN_NULLARY;
            pn->pn_dval = d;
            RecycleTree(pn1, tc);
            RecycleTree(pn2, tc);
        }
        break;

      case TOK_UNARYOP:
        if (pn1->pn_type == TOK_NUMBER) {
            jsdouble d;
            int32 i;

            /* Operate on one numeric constants. */
            d = pn1->pn_dval;
            switch (pn->pn_op) {
              case JSOP_BITNOT:
                if (!js_DoubleToECMAInt32(cx, d, &i))
                    return JS_FALSE;
                d = ~i;
                break;

              case JSOP_NEG:
#ifdef HPUX
                /*
                 * Negation of a zero doesn't produce a negative
                 * zero on HPUX. Perform the operation by bit
                 * twiddling.
                 */
                JSDOUBLE_HI32(d) ^= JSDOUBLE_HI32_SIGNBIT;
#else
                d = -d;
#endif
                break;

              case JSOP_POS:
                break;

              case JSOP_NOT:
                pn->pn_type = TOK_PRIMARY;
                pn->pn_op = (d == 0) ? JSOP_TRUE : JSOP_FALSE;
                pn->pn_arity = PN_NULLARY;
                /* FALL THROUGH */

              default:
                /* Return early to dodge the common TOK_NUMBER code. */
                return JS_TRUE;
            }
            pn->pn_type = TOK_NUMBER;
            pn->pn_op = JSOP_NUMBER;
            pn->pn_arity = PN_NULLARY;
            pn->pn_dval = d;
            RecycleTree(pn1, tc);
        }
        break;

      default:;
    }

    /*
     * Flatten a left-associative (left-heavy) tree of a given operator into
     * a list, to reduce js_EmitTree recursion.
     */
    if (pn->pn_arity == PN_BINARY &&
        pn1 &&
        pn1->pn_type == pn->pn_type &&
        pn1->pn_op == pn->pn_op &&
        (js_CodeSpec[pn->pn_op].format & JOF_LEFTASSOC)) {
        if (pn1->pn_arity == PN_LIST) {
            /* We already flattened pn1, so move it to pn and append pn2. */
            PN_MOVE_NODE(pn, pn1);
            PN_APPEND(pn, pn2);
        } else {
            /* Convert pn into a list containing pn1's kids followed by pn2. */
            pn->pn_arity = PN_LIST;
            PN_INIT_LIST_1(pn, pn1->pn_left);
            PN_APPEND(pn, pn1->pn_right);
            PN_APPEND(pn, pn2);

            /* Clear pn1 so it can be recycled by itself, without its kids. */
            PN_CLEAR_NODE(pn1);
        }
        RecycleTree(pn1, tc);
    }

    return JS_TRUE;
}
