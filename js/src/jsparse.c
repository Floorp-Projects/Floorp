/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * JS parser.
 *
 * This is a recursive-descent parser for the JavaScript language specified by
 * "The JavaScript 1.4 Language Specification".  It uses lexical and semantic
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
 * Insist that the next token be of type tt, or report err and throw or fail.
 * NB: this macro uses cx and ts from its lexical environment.
 */

#define MUST_MATCH_TOKEN_THROW(tt, errno, throw)                              \
    JS_BEGIN_MACRO                                                            \
	if (js_GetToken(cx, ts) != tt) {                                      \
	    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR, errno);       \
	    throw;                                                            \
	}                                                                     \
    JS_END_MACRO

#define MUST_MATCH_TOKEN(tt, errno)                                           \
    MUST_MATCH_TOKEN_THROW(tt, errno, return NULL)


/*
 * Allocate a JSParseNode from cx's temporary arena.
 */
static JSParseNode *
NewParseNode(JSContext *cx, JSToken *tok, JSParseNodeArity arity)
{
    JSParseNode *pn;

    JS_ARENA_ALLOCATE(pn, &cx->tempPool, sizeof(JSParseNode));
    if (!pn)
	return NULL;
    pn->pn_type = tok->type;
    pn->pn_pos = tok->pos;
    pn->pn_arity = arity;
    pn->pn_next = NULL;
    return pn;
}

static JSParseNode *
NewBinary(JSContext *cx, JSTokenType tt,
	  JSOp op, JSParseNode *left, JSParseNode *right)
{
    JSParseNode *pn;

    if (!left || !right)
	return NULL;
    JS_ARENA_ALLOCATE(pn, &cx->tempPool, sizeof(JSParseNode));
    if (!pn)
	return NULL;
    pn->pn_type = tt;
    pn->pn_pos.begin = left->pn_pos.begin;
    pn->pn_pos.end = right->pn_pos.end;
    pn->pn_op = op;
    pn->pn_arity = PN_BINARY;
    pn->pn_left = left;
    pn->pn_right = right;
    pn->pn_next = NULL;
    return pn;
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
	js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
				    JSMSG_SEMI_BEFORE_STMNT);
	return JS_FALSE;
    }
    return JS_TRUE;
}

/*
 * Parse a top-level JS script.
 */
JS_FRIEND_API(JSBool)
js_CompileTokenStream(JSContext *cx, JSObject *chain, JSTokenStream *ts,
		      JSCodeGenerator *cg)
{
    JSStackFrame *fp, frame;
    JSTokenType tt;
    JSBool ok;
    JSParseNode *pn;

    fp = cx->fp;
    if (!fp || fp->scopeChain != chain) {
	memset(&frame, 0, sizeof frame);
	frame.scopeChain = chain;
	frame.down = fp;
	cx->fp = &frame;
    }

    /*
     * Prevent GC activation on this context (possible if out of memory when
     * atomizing, or from pre-ECMAv2 switch case expr eval in the unlikely
     * case of a branch-callback -- unlikely because it means the switch case
     * must have called a function).
     */
    cx->gcDisabled++;

    ok = JS_TRUE;
    do {
	ts->flags |= TSF_REGEXP;
	tt = js_GetToken(cx, ts);
	ts->flags &= ~TSF_REGEXP;
	if (tt <= TOK_EOF) {
	    if (tt == TOK_ERROR)
		ok = JS_FALSE;
	    break;
	}

	switch (tt) {
	  case TOK_FUNCTION:
	    pn = FunctionStmt(cx, ts, &cg->treeContext);
	    if (pn && pn->pn_pos.end.lineno == ts->lineno) {
		ok = WellTerminated(cx, ts, TOK_FUNCTION);
		if (!ok)
		    goto out;
	    }
	    break;

	  default:
	    js_UngetToken(ts);
	    pn = Statement(cx, ts, &cg->treeContext);
	    if (pn) {
		ok = js_FoldConstants(cx, pn);
		if (!ok)
		    goto out;
	    }
	    break;
	}
	if (pn) {
	    ok = js_AllocTryNotes(cx, cg);
	    if (ok)
		ok = js_EmitTree(cx, cg, pn);
	} else {
	    ok = JS_FALSE;
	}
    } while (ok);

out:
    ts->flags &= ~TSF_BADCOMPILE;
    cx->gcDisabled--;
    cx->fp = fp;
    if (!ok)
	CLEAR_PUSHBACK(ts);
    return ok;
}

#ifdef CHECK_RETURN_EXPR

/*
 * Insist on a final return before control flows out of pn, but don't be too
 * smart about loops (do {...; return e2;} while(0) at the end of a function
 * that contains an early return e1 will get an error XXX should be warning
 * option).
 */
static JSBool
CheckFinalReturn(JSParseNode *pn)
{
    JSBool ok, hasDefault;
    JSParseNode *pn2, *pn3;

    switch (pn->pn_type) {
      case TOK_LC:
	if (!pn->pn_head)
	    return JS_FALSE;
	return CheckFinalReturn(PN_LAST(pn));
      case TOK_IF:
	ok = CheckFinalReturn(pn->pn_kid2);
	ok &= pn->pn_kid3 && CheckFinalReturn(pn->pn_kid3);
	return ok;
#if JS_HAS_SWITCH_STATEMENT
      case TOK_SWITCH:
	ok = JS_TRUE;
	for (pn2 = pn->pn_kid2->pn_head; ok && pn2; pn2 = pn2->pn_next) {
	    if (pn2->pn_type == TOK_DEFAULT)
		hasDefault = JS_TRUE;
	    pn3 = pn2->pn_right;
	    JS_ASSERT(pn3->pn_type == TOK_LC);
	    if (pn3->pn_head)
		ok &= CheckFinalReturn(PN_LAST(pn3));
	}
	/* If a final switch has no default case, we judge it harshly. */
	ok &= hasDefault;
	return ok;
#endif /* JS_HAS_SWITCH_STATEMENT */
      case TOK_WITH:
	return CheckFinalReturn(pn->pn_right);
      case TOK_RETURN:
	return JS_TRUE;
      default:
	return JS_FALSE;
    }
}

#endif /* CHECK_RETURN_EXPR */

static JSParseNode *
FunctionBody(JSContext *cx, JSTokenStream *ts, JSFunction *fun,
	     JSTreeContext *tc)
{
    JSStackFrame *fp, frame;
    uintN oldflags;
    JSParseNode *pn;

    fp = cx->fp;
    if (!fp || fp->scopeChain != fun->object) {
	memset(&frame, 0, sizeof frame);
	frame.scopeChain = fun->object;
	frame.down = fp;
	cx->fp = &frame;
    }

    oldflags = tc->flags;
    tc->flags &= ~(TCF_RETURN_EXPR | TCF_RETURN_VOID);
    tc->flags |= TCF_IN_FUNCTION;
    pn = Statements(cx, ts, tc);

#ifdef CHECK_RETURN_EXPR
    /* Check for falling off the end of a function that returns a value. */
    if (pn && (tc->flags & TCF_RETURN_EXPR)) {
	if (!CheckFinalReturn(pn)) {
	    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
					JSMSG_NO_RETURN_VALUE);
	    pn = NULL;
	}
    }
#endif

    cx->fp = fp;
    tc->flags = oldflags;
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
    JSParseNode *pn;
    JSBool ok;

    if (!js_InitCodeGenerator(cx, &funcg, ts->filename, ts->lineno,
			      ts->principals)) {
	return JS_FALSE;
    }

    /* Prevent GC activation on this context during compilation. */
    cx->gcDisabled++;

    /* Satisfy the assertion at the top of Statements. */
    ts->token.type = TOK_LC;
    pn = FunctionBody(cx, ts, fun, &funcg.treeContext);
    if (!pn) {
	CLEAR_PUSHBACK(ts);
	ok = JS_FALSE;
    } else {
	ok = js_FoldConstants(cx, pn);
	if (ok)
	    ok = js_EmitFunctionBody(cx, &funcg, pn, fun);
    }

    cx->gcDisabled--;
    js_FinishCodeGenerator(cx, &funcg);
    return ok;
}

static JSBool
InWithStatement(JSTreeContext *tc)
{
    JSStmtInfo *stmt;

    for (stmt = tc->topStmt; stmt; stmt = stmt->down) {
	if (stmt->type == STMT_WITH)
	    return JS_TRUE;
    }
    return JS_FALSE;
}

static JSParseNode *
FunctionDef(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc,
	    JSBool lambda)
{
    JSParseNode *pn, *pn2;
    JSAtom *funAtom, *argAtom;
    JSObject *parent;
    JSFunction *fun, *outerFun;
    JSBool ok, named;
    JSObject *pobj;
    JSScopeProperty *sprop;
    JSTreeContext funtc;
    jsval junk;

    /* Make a TOK_FUNCTION node. */
    pn = NewParseNode(cx, &ts->token, PN_FUNC);
    if (!pn)
	return NULL;

    /* Scan the optional function name into funAtom. */
    if (js_MatchToken(cx, ts, TOK_NAME))
	funAtom = ts->token.t_atom;
    else
	funAtom = NULL;

    /* Find the nearest variable-declaring scope and use it as our parent. */
    parent = js_FindVariableScope(cx, &outerFun);
    if (!parent)
	return NULL;

#if JS_HAS_LEXICAL_CLOSURE
    if (!funAtom || InWithStatement(tc)) {
	/* Don't name the function if enclosed by a with statement or equiv. */
	fun = js_NewFunction(cx, NULL, NULL, 0, 0, cx->fp->scopeChain,
			     funAtom);
	named = JS_FALSE;
    } else
#endif
    {
	/* Override any previously defined property using js_DefineFunction. */
	fun = js_DefineFunction(cx, parent, funAtom, NULL, 0, JSPROP_ENUMERATE);
	named = (fun != NULL);
    }
    if (!fun) {
	ok = JS_FALSE;
	goto out;
    }

    /* Now parse formal argument list and compute fun->nargs. */
    MUST_MATCH_TOKEN_THROW(TOK_LP, JSMSG_PAREN_BEFORE_FORMAL,
			   ok = JS_FALSE; goto out);
    if (!js_MatchToken(cx, ts, TOK_RP)) {
	do {
	    MUST_MATCH_TOKEN_THROW(TOK_NAME, JSMSG_MISSING_FORMAL,
				   ok = JS_FALSE; goto out);
	    argAtom = ts->token.t_atom;
	    pobj = NULL;
	    ok = js_LookupProperty(cx, fun->object, (jsid)argAtom, &pobj,
				   (JSProperty **)&sprop);
	    if (!ok)
		goto out;
	    if (sprop && pobj == fun->object) {
		if (sprop->getter == js_GetArgument) {
#ifdef CHECK_ARGUMENT_HIDING
		    OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_WARNING,
						JSMSG_DUPLICATE_FORMAL,
						ATOM_BYTES(argAtom));
		    ok = JS_FALSE;
		    goto out;
#else
		    /*
		     * A duplicate parameter name. We create a dummy symbol
		     * entry with property id of the parameter number and set
		     * the id to the name of the parameter.
		     * The decompiler will know to treat this case specially.
		     */
		    jsid oldArgId = (jsid) sprop->id;
		    OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
		    sprop = NULL;
		    ok = js_DefineProperty(cx, fun->object,
					   oldArgId, JSVAL_VOID,
					   js_GetArgument, js_SetArgument,
					   JSPROP_ENUMERATE | JSPROP_PERMANENT,
					   (JSProperty **)&sprop);
		    if (!ok)
			goto out;
		    sprop->id = (jsid) argAtom;
#endif
		}
	    }
	    if (sprop) {
		OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
		sprop = NULL;
	    }
	    ok = js_DefineProperty(cx, fun->object,
				   (jsid)argAtom, JSVAL_VOID,
				   js_GetArgument, js_SetArgument,
				   JSPROP_ENUMERATE | JSPROP_PERMANENT,
				   (JSProperty **)&sprop);
	    if (!ok)
		goto out;
	    JS_ASSERT(sprop);
	    sprop->id = INT_TO_JSVAL(fun->nargs++);
	    OBJ_DROP_PROPERTY(cx, fun->object, (JSProperty *)sprop);
	} while (js_MatchToken(cx, ts, TOK_COMMA));

	MUST_MATCH_TOKEN_THROW(TOK_RP, JSMSG_PAREN_AFTER_FORMAL,
			       ok = JS_FALSE; goto out);
    }

    MUST_MATCH_TOKEN_THROW(TOK_LC, JSMSG_CURLY_BEFORE_BODY,
			   ok = JS_FALSE; goto out);
    pn->pn_pos.begin = ts->token.pos.begin;

    TREE_CONTEXT_INIT(&funtc);
    pn2 = FunctionBody(cx, ts, fun, &funtc);
    if (!pn2) {
	ok = JS_FALSE;
	goto out;
    }

    MUST_MATCH_TOKEN_THROW(TOK_RC, JSMSG_CURLY_AFTER_BODY,
			   ok = JS_FALSE; goto out);
    pn->pn_pos.end = ts->token.pos.end;

    pn->pn_fun = fun;
    pn->pn_body = pn2;
    pn->pn_tryCount = funtc.tryCount;

#if JS_HAS_LEXICAL_CLOSURE
    if (outerFun || cx->fp->scopeChain != parent || InWithStatement(tc))
	pn->pn_op = JSOP_CLOSURE;
    else if (lambda)
	pn->pn_op = JSOP_OBJECT;
    else
#endif
	pn->pn_op = JSOP_NOP;

    ok = JS_TRUE;
out:
    if (!ok) {
	if (named)
	    (void) OBJ_DELETE_PROPERTY(cx, parent, (jsid)funAtom, &junk);
	return NULL;
    }
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
 * statements' trees.  Our caller must match { before and } after.
 */
static JSParseNode *
Statements(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2;
    JSTokenType tt;

    JS_ASSERT(ts->token.type == TOK_LC);
    pn = NewParseNode(cx, &ts->token, PN_LIST);
    if (!pn)
	return NULL;
    PN_INIT_LIST(pn);

    while ((tt = js_PeekToken(cx, ts)) > TOK_EOF && tt != TOK_RC) {
	pn2 = Statement(cx, ts, tc);
	if (!pn2)
            return NULL;
	PN_APPEND(pn, pn2);
    }

    pn->pn_pos.end = ts->token.pos.end;
    if (tt == TOK_ERROR)
	pn = NULL;
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
	js_ReportCompileErrorNumber(cx, ts, JSREPORT_WARNING,
				    JSMSG_EQUAL_AS_ASSIGN,
				    JSVERSION_IS_ECMA(cx->version) ? ""
				    : "\nAssuming equality test");
	if (JSVERSION_IS_ECMA(cx->version))
	    goto no_rewrite;
	pn->pn_type = TOK_EQOP;
	pn->pn_op = (JSOp)cx->jsop_eq;
	pn2 = pn->pn_left;
	switch (pn2->pn_op) {
	  case JSOP_SETARG:
	    pn2->pn_op = JSOP_GETARG;
	    break;
	  case JSOP_SETVAR:
	    pn2->pn_op = JSOP_GETVAR;
	    break;
	  case JSOP_SETNAME2:
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
  no_rewrite:
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
	label = ts->token.t_atom;
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
    pn = NewParseNode(cx, &ts->token, PN_NULLARY);
    if (!pn)
	return NULL;
    pn->pn_op = JSOP_NAME;
    pn->pn_atom = ts->token.t_atom;
    pn->pn_slot = -1;

    ts->flags |= TSF_REGEXP;
    while ((tt = js_GetToken(cx, ts)) == TOK_DOT || tt == TOK_LB) {
	ts->flags &= ~TSF_REGEXP;
	if (pn->pn_op == JSOP_IMPORTALL)
	    goto bad_import;

	if (tt == TOK_DOT) {
	    pn2 = NewParseNode(cx, &ts->token, PN_NAME);
	    if (!pn2)
		return NULL;
	    pn2->pn_expr = pn;
	    if (js_MatchToken(cx, ts, TOK_STAR)) {
		pn2->pn_op = JSOP_IMPORTALL;
		pn2->pn_atom = NULL;
	    } else {
		MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NAME_AFTER_DOT);
		pn2->pn_op = JSOP_GETPROP;
		pn2->pn_atom = ts->token.t_atom;
		pn2->pn_slot = -1;
	    }
	    pn2->pn_pos.begin = pn->pn_pos.begin;
	    pn2->pn_pos.end = ts->token.pos.end;
	} else {
	    /* Make a TOK_LB node. */
	    pn2 = NewParseNode(cx, &ts->token, PN_BINARY);
	    if (!pn2)
		return NULL;
	    pn3 = Expr(cx, ts, tc);
	    if (!pn3)
		return NULL;

	    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);
	    pn2->pn_pos.begin = pn->pn_pos.begin;
	    pn2->pn_pos.end = ts->token.pos.end;

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
    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR, JSMSG_BAD_IMPORT);
    return NULL;
}
#endif /* JS_HAS_EXPORT_IMPORT */

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

    switch (tt) {
#if JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
	pn = NewParseNode(cx, &ts->token, PN_LIST);
	if (!pn)
	    return NULL;
	PN_INIT_LIST(pn);
	if (js_MatchToken(cx, ts, TOK_STAR)) {
	    pn2 = NewParseNode(cx, &ts->token, PN_NULLARY);
	    if (!pn2)
		return NULL;
	    PN_APPEND(pn, pn2);
	} else {
	    do {
		MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_EXPORT_NAME);
		pn2 = NewParseNode(cx, &ts->token, PN_NULLARY);
		if (!pn2)
		    return NULL;
		pn2->pn_op = JSOP_NAME;
		pn2->pn_atom = ts->token.t_atom;
		pn2->pn_slot = -1;
		PN_APPEND(pn, pn2);
	    } while (js_MatchToken(cx, ts, TOK_COMMA));
	}
	pn->pn_pos.end = PN_LAST(pn)->pn_pos.end;
	if (pn->pn_pos.end.lineno == ts->lineno &&
	    !WellTerminated(cx, ts, TOK_ERROR)) {
	    return NULL;
	}
	break;

      case TOK_IMPORT:
	pn = NewParseNode(cx, &ts->token, PN_LIST);
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
	break;
#endif /* JS_HAS_EXPORT_IMPORT */

      case TOK_IF:
	/* An IF node has three kids: condition, then, and optional else. */
	pn = NewParseNode(cx, &ts->token, PN_TERNARY);
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

	pn = NewParseNode(cx, &ts->token, PN_BINARY);
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
	pn2 = NewParseNode(cx, &ts->token, PN_LIST);
	if (!pn2)
	    return NULL;
	PN_INIT_LIST(pn2);

	js_PushStatement(tc, &stmtInfo, STMT_SWITCH, -1);

	while ((tt = js_GetToken(cx, ts)) != TOK_RC) {
	    switch (tt) {
	      case TOK_DEFAULT:
		if (seenDefault) {
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
						JSMSG_TOO_MANY_DEFAULTS);
		    return NULL;
		}
		seenDefault = JS_TRUE;
		/* fall through */

	      case TOK_CASE:
		pn3 = NewParseNode(cx, &ts->token, PN_BINARY);
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
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
						JSMSG_TOO_MANY_CASES);
		    return NULL;
		}
		break;

	      case TOK_ERROR:
		return NULL;

	      default:
		js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
					    JSMSG_BAD_SWITCH);
                return NULL;
	    }
	    MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_CASE);

	    pn4 = NewParseNode(cx, &ts->token, PN_LIST);
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

	pn->pn_pos.end = pn2->pn_pos.end = ts->token.pos.end;
	pn->pn_kid1 = pn1;
	pn->pn_kid2 = pn2;
	return pn;
      }
#endif /* JS_HAS_SWITCH_STATEMENT */

      case TOK_WHILE:
	pn = NewParseNode(cx, &ts->token, PN_BINARY);
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
	pn = NewParseNode(cx, &ts->token, PN_BINARY);
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
	pn = NewParseNode(cx, &ts->token, PN_BINARY);
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
	 * We can be sure that if it's a for/in loop, there's still an 'in'
	 * keyword here, even if Javascript recognizes it as an operator,
	 * because we've excluded it from parsing by setting the
	 * TCF_IN_FOR_INIT flag on the JSTreeContext argument.
	 */
	if (pn1 && js_MatchToken(cx, ts, TOK_IN)) {
	    stmtInfo.type = STMT_FOR_IN_LOOP;

	    /* Check that the left side of the 'in' is valid. */
	    if (pn1->pn_type != TOK_VAR &&
		pn1->pn_type != TOK_NAME &&
		pn1->pn_type != TOK_DOT &&
		pn1->pn_type != TOK_LB) {
		js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
					    JSMSG_BAD_FOR_LEFTSIDE);
		return NULL;
	    }

	    /* Parse the object expression as the right operand of 'in'. */
	    pn2 = NewBinary(cx, TOK_IN, JSOP_NOP, pn1, Expr(cx, ts, tc));
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
	    pn4 = NewParseNode(cx, &ts->token, PN_TERNARY);
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
	pn = NewParseNode(cx, &ts->token, PN_TERNARY);
	pn->pn_op = JSOP_NOP;

	MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_TRY);
	pn->pn_kid1 = Statements(cx, ts, tc);
	if (!pn->pn_kid1)
	    return NULL;
	MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_TRY);

	pn->pn_kid2 = NULL;
	catchtail = pn;
	while(js_PeekToken(cx, ts) == TOK_CATCH) {
	    /*
	     * legal catch form is:
	     * catch (v)
             * 
             * The form
	     * catch (v : <boolean_expression>)
             * has been pulled pending resolution in ECMA.
	     */

	    /* catch node */
	    pn2 = NewParseNode(cx, &ts->token, PN_TERNARY);
	    pn2->pn_op = JSOP_NOP;

	    /*
	     * We use a PN_NAME for the discriminant (catchguard) node
	     * with the actual discriminant code in the initializer spot
	     */
	    pn3 = NewParseNode(cx, &ts->token, PN_NAME);
	    if (!pn2 || !pn3)
		return NULL;

	    (void)js_GetToken(cx, ts); /* eat `catch' */

	    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_CATCH);
	    MUST_MATCH_TOKEN(TOK_NAME, JSMSG_CATCH_IDENTIFIER);
	    pn3->pn_atom = ts->token.t_atom;
            pn3->pn_expr = NULL;
#if JS_HAS_CATCH_GUARD
	    if (js_PeekToken(cx, ts) == TOK_COLON) {
		(void)js_GetToken(cx, ts); /* eat `:' */
		pn3->pn_expr = Expr(cx, ts, tc);
		if (!pn3->pn_expr)
		    return NULL;
	    } 
#endif
	    pn2->pn_kid1 = pn3;

	    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_CATCH);

	    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CATCH);
	    pn2->pn_kid3 = Statements(cx, ts, tc);
	    if (!pn2->pn_kid3)
		return NULL;
	    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_CATCH);

	    catchtail = catchtail->pn_kid2 = pn2;
	}
	catchtail->pn_kid2 = NULL;

	if (js_MatchToken(cx, ts, TOK_FINALLY)) {
	    tc->tryCount++;
	    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY);
	    pn->pn_kid3 = Statements(cx, ts, tc);
	    if (!pn->pn_kid3)
		return NULL;
	    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_FINALLY);
	} else {
	    pn->pn_kid3 = NULL;
	}
	if (!pn->pn_kid2 && !pn->pn_kid3) {
	    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
					JSMSG_CATCH_OR_FINALLY);
	    return NULL;
	}
	tc->tryCount++;
	return pn;
      }
      case TOK_THROW:
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
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
	js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
				    JSMSG_CATCH_WITHOUT_TRY);
	return NULL;

      case TOK_FINALLY:
	js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
				    JSMSG_FINALLY_WITHOUT_TRY);
	return NULL;

#endif /* JS_HAS_EXCEPTIONS */

      case TOK_BREAK:
	pn = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn)
	    return NULL;
	if (!MatchLabel(cx, ts, pn))
	    return NULL;
	stmt = tc->topStmt;
	label = pn->pn_atom;
	if (label) {
	    for (; ; stmt = stmt->down) {
		if (!stmt) {
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
						JSMSG_LABEL_NOT_FOUND);
		    return NULL;
		}
		if (stmt->type == STMT_LABEL && stmt->label == label)
		    break;
	    }
	} else {
	    for (; ; stmt = stmt->down) {
		if (!stmt) {
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
						JSMSG_TOUGH_BREAK);
		    return NULL;
		}
		if (STMT_IS_LOOP(stmt) || stmt->type == STMT_SWITCH)
		    break;
	    }
	}
	if (label)
	    pn->pn_pos.end = ts->token.pos.end;
	break;

      case TOK_CONTINUE:
	pn = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn)
	    return NULL;
	if (!MatchLabel(cx, ts, pn))
	    return NULL;
	stmt = tc->topStmt;
	label = pn->pn_atom;
	if (label) {
	    for (stmt2 = NULL; ; stmt = stmt->down) {
		if (!stmt) {
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
						JSMSG_LABEL_NOT_FOUND);
		    return NULL;
		}
		if (stmt->type == STMT_LABEL) {
		    if (stmt->label == label) {
			if (!stmt2 || !STMT_IS_LOOP(stmt2)) {
			    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
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
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
						JSMSG_BAD_CONTINUE);
		    return NULL;
		}
		if (STMT_IS_LOOP(stmt))
		    break;
	    }
	}
	if (label)
	    pn->pn_pos.end = ts->token.pos.end;
	break;

      case TOK_WITH:
	pn = NewParseNode(cx, &ts->token, PN_BINARY);
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

	pn->pn_pos.end = pn2->pn_pos.end;
	pn->pn_right = pn2;
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
	pn->pn_op = JSOP_POP;
	break;

      case TOK_RETURN:
	if (!(tc->flags & TCF_IN_FUNCTION)) {
	    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
					JSMSG_BAD_RETURN);
	    return NULL;
	}
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
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

#ifdef CHECK_RETURN_EXPR
	if ((tc->flags & (TCF_RETURN_EXPR | TCF_RETURN_VOID)) ==
	    (TCF_RETURN_EXPR | TCF_RETURN_VOID)) {
	    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
					JSMSG_NO_RETURN_VALUE);
	    return NULL;
	}
#endif
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
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
	if (!pn)
	    return NULL;
	pn->pn_type = TOK_SEMI;
	pn->pn_kid = NULL;
	return pn;

#if JS_HAS_DEBUGGER_KEYWORD
      case TOK_DEBUGGER:
	if(!WellTerminated(cx, ts, TOK_ERROR))
	    return NULL;
	pn = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn)
	    return NULL;
	pn->pn_type = TOK_DEBUGGER;
	return pn;
#endif /* JS_HAS_DEBUGGER_KEYWORD */

      case TOK_ERROR:
	return NULL;

      default:
	lastExprType = ts->token.type;
	js_UngetToken(ts);
	pn2 = Expr(cx, ts, tc);
	if (!pn2)
	    return NULL;

	tt = ts->pushback.type;
	if (tt == TOK_COLON) {
	    if (pn2->pn_type != TOK_NAME) {
		js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
					    JSMSG_BAD_LABEL);
		return NULL;
	    }
	    label = pn2->pn_atom;
	    for (stmt = tc->topStmt; stmt; stmt = stmt->down) {
		if (stmt->type == STMT_LABEL && stmt->label == label) {
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
						JSMSG_DUPLICATE_LABEL);
		    return NULL;
		}
	    }
	    js_GetToken(cx, ts);

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

	/* Check explicity against (multi-line) function statement */
	if (pn2->pn_pos.end.lineno == ts->lineno &&
	    !WellTerminated(cx, ts, lastExprType)) {
	    return NULL;
	}
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
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
    JSFunction *fun;
    JSClass *clasp;
    JSPropertyOp getter, setter, currentGetter, currentSetter;
    JSAtom *atom;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSBool ok;

    /*
     * The tricky part of this code is to create special
     * parsenode opcodes for getting and setting variables
     * (which will be stored as special slots in the frame).
     * The complex special case is an eval() inside a
     * function. If the evaluated string references variables in
     * the enclosing function, then we need to generate
     * the special variable opcodes.
     * We determine this by looking up the variable id in the
     * current variable scope.
     */
    JS_ASSERT(ts->token.type == TOK_VAR);
    pn = NewParseNode(cx, &ts->token, PN_LIST);
    if (!pn)
	return NULL;
    pn->pn_op = JSOP_NOP;
    PN_INIT_LIST(pn);

    obj = js_FindVariableScope(cx, &fun);
    if (!obj)
	return NULL;
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

    do {
        currentGetter = getter;
        currentSetter = setter;
	MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_VARIABLE_NAME);
	atom = ts->token.t_atom;

	pn2 = NewParseNode(cx, &ts->token, PN_NAME);
	if (!pn2)
	    return NULL;
	pn2->pn_op = JSOP_NAME;
	pn2->pn_atom = atom;
	pn2->pn_expr = NULL;
	pn2->pn_slot = -1;
	PN_APPEND(pn, pn2);

	if (!OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &pobj, &prop))
	    return NULL;
	if (pobj == obj &&
	    OBJ_IS_NATIVE(pobj) &&
	    (sprop = (JSScopeProperty *)prop) != NULL) {
	    if (sprop->getter == js_GetArgument) {
		currentGetter = sprop->getter;
#ifdef CHECK_ARGUMENT_HIDING
		js_ReportCompileErrorNumber(cx, ts, JSREPORT_WARNING,
					    JSMSG_VAR_HIDES_ARG,
					    ATOM_BYTES(atom));
		ok = JS_FALSE;
#else
		ok = JS_TRUE;
#endif
	    } else {
		ok = JS_TRUE;
		if (fun) {
		    /* Not an argument, must be a redeclared local var. */
		    if (clasp == &js_FunctionClass) {
			JS_ASSERT(sprop->getter == js_GetLocalVariable);
			JS_ASSERT(JSVAL_IS_INT(sprop->id) &&
				  JSVAL_TO_INT(sprop->id) < fun->nvars);
		    } else if (clasp == &js_CallClass) {
			if (sprop->getter == js_GetCallVariable) {
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
			    currentGetter = sprop->getter;
			    currentSetter = sprop->setter;
			}
		    }
		} else {
		    /* Global var: (re-)set id a la js_DefineProperty. */
		    sprop->id = ATOM_KEY(atom);
		}
		sprop->getter = currentGetter;
		sprop->setter = currentSetter;
		sprop->attrs |= JSPROP_ENUMERATE | JSPROP_PERMANENT;
		sprop->attrs &= ~JSPROP_READONLY;
	    }
	} else {
	    /*
	     * Property not found in current variable scope: we have not
	     * seen this variable before.
	     * Define a new variable by adding a property to the current
	     * scope, or by allocating more slots in the function's frame.
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
	    ok = OBJ_DEFINE_PROPERTY(cx, obj, (jsid)atom, JSVAL_VOID,
				     currentGetter, currentSetter,
				     JSPROP_ENUMERATE | JSPROP_PERMANENT,
				     &prop);
	    if (ok && prop) {
		pobj = obj;
		if (currentGetter == js_GetLocalVariable) {
		    /*
		     * Allocate more room for variables in the
		     * function's frame. We can do this only
		     * before the function is called.
		     */
		    sprop = (JSScopeProperty *)prop;
		    sprop->id = INT_TO_JSVAL(fun->nvars++);
		}
	    }
	}

	if (js_MatchToken(cx, ts, TOK_ASSIGN)) {
	    if (ts->token.t_op != JSOP_NOP) {
		js_ReportCompileErrorNumber(cx, ts,JSREPORT_ERROR,
					    JSMSG_BAD_VAR_INIT);
		ok = JS_FALSE;
	    } else {
		pn2->pn_expr = AssignExpr(cx, ts, tc);
		if (pn2->pn_expr)
		    pn2->pn_op = JSOP_SETNAME2;
		else
		    ok = JS_FALSE;
	    }
	}

	if (ok && fun && (clasp == &js_FunctionClass ||
			  clasp == &js_CallClass) &&
	    !InWithStatement(tc))
	{
	    /* Depending on the value of the getter, change the
	     * opcodes to the forms for arguments and variables.
	     */
	    if (currentGetter == js_GetArgument) {
		JS_ASSERT(sprop && JSVAL_IS_INT(sprop->id));
		pn2->pn_op = (pn2->pn_op == JSOP_NAME)
			     ? JSOP_GETARG
			     : JSOP_SETARG;
		pn2->pn_slot = JSVAL_TO_INT(sprop->id);
	    } else if (currentGetter == js_GetLocalVariable ||
		       currentGetter == js_GetCallVariable)
	    {
		JS_ASSERT(sprop && JSVAL_IS_INT(sprop->id));
		pn2->pn_op = (pn2->pn_op == JSOP_NAME)
			     ? JSOP_GETVAR
			     : JSOP_SETVAR;
		pn2->pn_slot = JSVAL_TO_INT(sprop->id);
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
	pn2 = NewParseNode(cx, &ts->token, PN_LIST);
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

/* ZZZbe don't create functions till codegen? or at least don't bind
 * fn name */
static JSBool
LookupArgOrVar(JSContext *cx, JSAtom *atom, JSTreeContext *tc,
	       JSOp *opp, jsint *slotp)
{
    JSObject *obj, *pobj;
    JSClass *clasp;
    JSFunction *fun;
    JSScopeProperty *sprop;

    obj = js_FindVariableScope(cx, &fun);
    clasp = OBJ_GET_CLASS(cx, obj);
    *opp = JSOP_NAME;
    if (clasp != &js_FunctionClass && clasp != &js_CallClass)
	return JS_TRUE;
    if (InWithStatement(tc))
	return JS_TRUE;
    if (!js_LookupProperty(cx, obj, (jsid)atom, &pobj, (JSProperty **)&sprop))
	return JS_FALSE;
    *slotp = -1;
    if (sprop) {
	if (sprop->getter == js_GetArgument) {
	    *opp = JSOP_GETARG;
	    *slotp = JSVAL_TO_INT(sprop->id);
	} else if (sprop->getter == js_GetLocalVariable ||
		   sprop->getter == js_GetCallVariable)
	{
	    *opp = JSOP_GETVAR;
	    *slotp = JSVAL_TO_INT(sprop->id);
	}
	OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
    }
    return JS_TRUE;
}

static JSParseNode *
AssignExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2;
    JSOp op;

    pn = CondExpr(cx, ts, tc);
    if (pn && js_MatchToken(cx, ts, TOK_ASSIGN)) {
	op = ts->token.t_op;
	for (pn2 = pn; pn2->pn_type == TOK_RP; pn2 = pn2->pn_kid)
	    ;
	switch (pn2->pn_type) {
	  case TOK_NAME:
	    if (pn2->pn_slot >= 0) {
		JS_ASSERT(pn2->pn_op == JSOP_GETARG || pn2->pn_op == JSOP_GETVAR);
		if (pn2->pn_op == JSOP_GETARG)
		    pn2->pn_op = JSOP_SETARG;
		else
		    pn2->pn_op = JSOP_SETVAR;
	    } else {
		pn2->pn_op = JSOP_SETNAME2;
	    }
	    break;
	  case TOK_DOT:
	    pn2->pn_op = JSOP_SETPROP;
	    break;
	  case TOK_LB:
	    pn2->pn_op = JSOP_SETELEM;
	    break;
	  default:
	    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
					JSMSG_BAD_LEFTSIDE_OF_ASS);
	    return NULL;
	}
	pn = NewBinary(cx, TOK_ASSIGN, op, pn2, AssignExpr(cx, ts, tc));
    }
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
	pn = NewParseNode(cx, &ts->token, PN_TERNARY);
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
	tc->flags = oldflags;
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
	pn = NewBinary(cx, TOK_OR, JSOP_OR, pn, OrExpr(cx, ts, tc));
    return pn;
}

static JSParseNode *
AndExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = BitOrExpr(cx, ts, tc);
    if (pn && js_MatchToken(cx, ts, TOK_AND))
	pn = NewBinary(cx, TOK_AND, JSOP_AND, pn, AndExpr(cx, ts, tc));
    return pn;
}

static JSParseNode *
BitOrExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = BitXorExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_BITOR))
	pn = NewBinary(cx, TOK_BITOR, JSOP_BITOR, pn, BitXorExpr(cx, ts, tc));
    return pn;
}

static JSParseNode *
BitXorExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = BitAndExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_BITXOR))
	pn = NewBinary(cx, TOK_BITXOR, JSOP_BITXOR, pn, BitAndExpr(cx, ts, tc));
    return pn;
}

static JSParseNode *
BitAndExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;

    pn = EqExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_BITAND))
	pn = NewBinary(cx, TOK_BITAND, JSOP_BITAND, pn, EqExpr(cx, ts, tc));
    return pn;
}

static JSParseNode *
EqExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn;
    JSOp op;

    pn = RelExpr(cx, ts, tc);
    while (pn && js_MatchToken(cx, ts, TOK_EQOP)) {
	op = ts->token.t_op;
	pn = NewBinary(cx, TOK_EQOP, op, pn, RelExpr(cx, ts, tc));
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
	     * Only recognize the 'in' token as an operator if we're not
	     * currently in the init expr of a for loop.
	     */
	    || (inForInitFlag == 0 && js_MatchToken(cx, ts, TOK_IN))
#endif /* JS_HAS_IN_OPERATOR */
#if JS_HAS_INSTANCEOF
	    || js_MatchToken(cx, ts, TOK_INSTANCEOF)
#endif /* JS_HAS_INSTANCEOF */
	    )) {
	tt = ts->token.type;
	op = ts->token.t_op;
	pn = NewBinary(cx, tt, op, pn, ShiftExpr(cx, ts, tc));
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
	op = ts->token.t_op;
	pn = NewBinary(cx, TOK_SHOP, op, pn, AddExpr(cx, ts, tc));
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
	tt = ts->token.type;
	op = (tt == TOK_PLUS) ? JSOP_ADD : JSOP_SUB;
	pn = NewBinary(cx, tt, op, pn, MulExpr(cx, ts, tc));
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
	tt = ts->token.type;
	op = ts->token.t_op;
	pn = NewBinary(cx, tt, op, pn, UnaryExpr(cx, ts, tc));
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
	kid->pn_type != TOK_LB) {
	js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
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
    jsint num;
    JSOp op;

    kid = SetLvalKid(cx, ts, pn, kid, incop_name_str[tt == TOK_DEC]);
    if (!kid)
	return JS_FALSE;
    num = -1;
    switch (kid->pn_type) {
      case TOK_NAME:
	if (!LookupArgOrVar(cx, kid->pn_atom, tc, &op, &num))
	    return JS_FALSE;
	if (op == JSOP_GETARG) {
	    op = (tt == TOK_INC)
		 ? (preorder ? JSOP_INCARG : JSOP_ARGINC)
		 : (preorder ? JSOP_DECARG : JSOP_ARGDEC);
	} else if (op == JSOP_GETVAR) {
	    op = (tt == TOK_INC)
		 ? (preorder ? JSOP_INCVAR : JSOP_VARINC)
		 : (preorder ? JSOP_DECVAR : JSOP_VARDEC);
	} else {
	    op = (tt == TOK_INC)
		 ? (preorder ? JSOP_INCNAME : JSOP_NAMEINC)
		 : (preorder ? JSOP_DECNAME : JSOP_NAMEDEC);
	}
	break;

      case TOK_DOT:
	op = (tt == TOK_INC)
	     ? (preorder ? JSOP_INCPROP : JSOP_PROPINC)
	     : (preorder ? JSOP_DECPROP : JSOP_PROPDEC);
	break;

      case TOK_LB:
	op = (tt == TOK_INC)
	     ? (preorder ? JSOP_INCELEM : JSOP_ELEMINC)
	     : (preorder ? JSOP_DECELEM : JSOP_ELEMDEC);
	break;

      default:
	JS_ASSERT(0);
    }
    pn->pn_op = op;
    pn->pn_num = num;
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
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
	if (!pn)
	    return NULL;
	pn->pn_type = TOK_UNARYOP;	/* PLUS and MINUS are binary */
	pn->pn_op = ts->token.t_op;
	pn2 = UnaryExpr(cx, ts, tc);
	if (!pn2)
	    return NULL;
	pn->pn_pos.end = pn2->pn_pos.end;
	pn->pn_kid = pn2;
	break;

      case TOK_INC:
      case TOK_DEC:
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
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
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
	if (!pn)
	    return NULL;
	pn2 = UnaryExpr(cx, ts, tc);
	if (!pn2)
	    return NULL;
	if (!SetLvalKid(cx, ts, pn, pn2, js_delete_str))
	    return NULL;
	pn->pn_pos.end = pn2->pn_pos.end;
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
		pn2 = NewParseNode(cx, &ts->token, PN_UNARY);
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

	pn = NewParseNode(cx, &ts->token, PN_LIST);
	if (!pn)
	    return NULL;
	pn2 = MemberExpr(cx, ts, tc, JS_FALSE);
	if (!pn2)
	    return NULL;
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
	    pn2 = NewParseNode(cx, &ts->token, PN_NAME);
	    if (!pn2)
		return NULL;
	    MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NAME_AFTER_DOT);
	    pn2->pn_pos.begin = pn->pn_pos.begin;
	    pn2->pn_pos.end = ts->token.pos.end;
	    pn2->pn_op = JSOP_GETPROP;
	    pn2->pn_expr = pn;
	    pn2->pn_atom = ts->token.t_atom;
	} else if (tt == TOK_LB) {
	    pn2 = NewParseNode(cx, &ts->token, PN_BINARY);
	    if (!pn2)
		return NULL;
	    pn3 = Expr(cx, ts, tc);
	    if (!pn3)
		return NULL;

	    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);
	    pn2->pn_pos.begin = pn->pn_pos.begin;
	    pn2->pn_pos.end = ts->token.pos.end;

            pn2->pn_op = JSOP_GETELEM;
            pn2->pn_left = pn;
            pn2->pn_right = pn3;
	} else if (allowCallSyntax && tt == TOK_LP) {
	    pn2 = NewParseNode(cx, &ts->token, PN_LIST);
	    if (!pn2)
		return NULL;
	    pn2->pn_op = JSOP_CALL;
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
	jsint atomIndex;

	pn = NewParseNode(cx, &ts->token, PN_LIST);
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

		if (tt == TOK_COMMA)
		    pn2 = NewParseNode(cx, &ts->token, PN_NULLARY);
		else
		    pn2 = AssignExpr(cx, ts, tc);
		if (!pn2)
		    return NULL;
		PN_APPEND(pn, pn2);

		if (!js_MatchToken(cx, ts, TOK_COMMA))
		    break;
	    }

	    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_LIST);
	}
	pn->pn_pos.end = ts->token.pos.end;
	return pn;
      }

      case TOK_LC:
	pn = NewParseNode(cx, &ts->token, PN_LIST);
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
		tt = js_GetToken(cx, ts);
		switch (tt) {
		  case TOK_NUMBER:
		    pn3 = NewParseNode(cx, &ts->token, PN_NULLARY);
		    if (pn3)
			pn3->pn_dval = ts->token.t_dval;
		    break;
		  case TOK_NAME:
		  case TOK_STRING:
		    pn3 = NewParseNode(cx, &ts->token, PN_NULLARY);
		    if (pn3)
			pn3->pn_atom = ts->token.t_atom;
		    break;
		  case TOK_RC:
		    goto end_obj_init;
		  default:
		    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
						JSMSG_BAD_PROP_ID);
		    return NULL;
		}

		MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_ID);
		pn2 = NewBinary(cx, TOK_COLON, JSOP_INITPROP, pn3,
				AssignExpr(cx, ts, tc));
		if (!pn2)
		    return NULL;
		PN_APPEND(pn, pn2);
	    } while (js_MatchToken(cx, ts, TOK_COMMA));

	    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_LIST);
	}
      end_obj_init:
	pn->pn_pos.end = ts->token.pos.end;
	return pn;

#if JS_HAS_SHARP_VARS
      case TOK_DEFSHARP:
	if (defsharp)
	    goto badsharp;
	defsharp = NewParseNode(cx, &ts->token, PN_UNARY);
	if (!defsharp)
	    return NULL;
	defsharp->pn_num = (jsint) ts->token.t_dval;
	defsharp->pn_kid = NULL;
	goto again;

      case TOK_USESHARP:
	/* Check for forward/dangling references at runtime, to allow eval. */
	pn = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn)
	    return NULL;
	pn->pn_num = (jsint) ts->token.t_dval;
	notsharp = JS_TRUE;
	break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_INITIALIZERS */

      case TOK_LP:
      {
#if JS_HAS_IN_OPERATOR
	uintN oldflags;
#endif
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
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
	tc->flags = oldflags;
#endif
	if (!pn2)
	    return NULL;

	MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);
	pn->pn_type = TOK_RP;
	pn->pn_pos.end = ts->token.pos.end;
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
	pn = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn)
	    return NULL;
	pn->pn_op = ts->token.t_op;
	pn->pn_atom = ts->token.t_atom;
	pn->pn_slot = -1;
	if (tt == TOK_NAME) {
	    if (!LookupArgOrVar(cx, pn->pn_atom, tc, &pn->pn_op, &pn->pn_slot))
		return NULL;
	    pn->pn_arity = PN_NAME;
	    pn->pn_expr = NULL;
	}
	break;

      case TOK_NUMBER:
	pn = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn)
	    return NULL;
	pn->pn_dval = ts->token.t_dval;
#if JS_HAS_SHARP_VARS
	notsharp = JS_TRUE;
#endif
	break;

      case TOK_PRIMARY:
	pn = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn)
	    return NULL;
	pn->pn_op = ts->token.t_op;
#if JS_HAS_SHARP_VARS
	notsharp = JS_TRUE;
#endif
	break;

#if !JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
      case TOK_IMPORT:
#endif
      case TOK_RESERVED:
        badWord = js_DeflateString(cx, ts->token.ptr, 
              (size_t) ts->token.pos.end.index - ts->token.pos.begin.index);
	js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR, 
                                        JSMSG_RESERVED_ID, badWord);
        JS_free(cx, badWord);
	return NULL;

      case TOK_ERROR:
	/* The scanner or one of its subroutines reported the error. */
	return NULL;

      default:
	js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR, JSMSG_SYNTAX_ERROR);
	return NULL;
    }

#if JS_HAS_SHARP_VARS
    if (defsharp) {
	if (notsharp) {
  badsharp:
	    js_ReportCompileErrorNumber(cx, ts, JSREPORT_ERROR,
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
js_FoldConstants(JSContext *cx, JSParseNode *pn)
{
    JSParseNode *pn1=NULL, *pn2=NULL, *pn3=NULL;

    switch (pn->pn_arity) {
      case PN_FUNC:
	if (!js_FoldConstants(cx, pn->pn_body))
	    return JS_FALSE;
	break;

      case PN_LIST:
	for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
	    if (!js_FoldConstants(cx, pn2))
		return JS_FALSE;
	}
	break;

      case PN_TERNARY:
	/* Any kid may be null (e.g. for (;;)). */
	pn1 = pn->pn_kid1;
	pn2 = pn->pn_kid2;
	pn3 = pn->pn_kid3;
	if (pn1 && !js_FoldConstants(cx, pn1))
	    return JS_FALSE;
	if (pn2 && !js_FoldConstants(cx, pn2))
	    return JS_FALSE;
	if (pn3 && !js_FoldConstants(cx, pn3))
	    return JS_FALSE;
	break;

      case PN_BINARY:
	/* First kid may be null (for default case in switch). */
	pn1 = pn->pn_left;
	pn2 = pn->pn_right;
	if (pn1 && !js_FoldConstants(cx, pn1))
	    return JS_FALSE;
	if (!js_FoldConstants(cx, pn2))
	    return JS_FALSE;
	break;

      case PN_UNARY:
	/* Our kid may be null (e.g. return; vs. return e;). */
	pn1 = pn->pn_kid;
	if (pn1 && !js_FoldConstants(cx, pn1))
	    return JS_FALSE;
	break;

      case PN_NAME:
	pn1 = pn->pn_expr;
	if (pn1 && !js_FoldConstants(cx, pn1))
	    return JS_FALSE;
	break;

      case PN_NULLARY:
	break;
    }

    switch (pn->pn_type) {
      case TOK_PLUS:
	if (pn1->pn_type == TOK_STRING && pn2->pn_type == TOK_STRING) {
	    JSString *str1, *str2;
	    size_t length, length1, length2, nbytes;
	    void *mark;
	    jschar *chars;

	    /* Concatenate string constants. */
	    str1 = ATOM_TO_STRING(pn1->pn_atom);
	    str2 = ATOM_TO_STRING(pn2->pn_atom);
	    length1 = str1->length;
	    length2 = str2->length;
	    length = length1 + length2;
	    nbytes = (length + 1) * sizeof(jschar);
	    mark = JS_ARENA_MARK(&cx->tempPool);
	    JS_ARENA_ALLOCATE(chars, &cx->tempPool, nbytes);
	    if (!chars) {
		JS_ReportOutOfMemory(cx);
		return JS_FALSE;
	    }
	    js_strncpy(chars, str1->chars, length1);
	    js_strncpy(chars + length1, str2->chars, length2);
	    chars[length] = 0;
	    pn->pn_atom = js_AtomizeChars(cx, chars, length, 0);
	    if (!pn->pn_atom)
		return JS_FALSE;
	    JS_ARENA_RELEASE(&cx->tempPool, mark);
	    pn->pn_type = TOK_STRING;
	    pn->pn_op = JSOP_STRING;
	    pn->pn_arity = PN_NULLARY;
	    break;
	}
	/* FALL THROUGH */

      case TOK_SHOP:
      case TOK_MINUS:
      case TOK_STAR:
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
	}
	break;

      default:;
    }

    return JS_TRUE;
}
