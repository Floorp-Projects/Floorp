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
 * "The JavaScript 1.2 Language Specification".  It uses lexical and semantic
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "prtypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prlog.h"
#include "prprf.h"
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
static JSParser MemberExpr;
static JSParser PrimaryExpr;

/*
 * Insist that the next token be of type tt, or report err and throw or fail.
 * NB: this macro uses cx and ts from its lexical environment.
 */
#define MUST_MATCH_TOKEN_THROW(tt, err, throw)                                \
    PR_BEGIN_MACRO                                                            \
	if (js_GetToken(cx, ts) != tt) {                                      \
	    js_ReportCompileError(cx, ts, err);                               \
	    throw;                                                            \
	}                                                                     \
    PR_END_MACRO

#define MUST_MATCH_TOKEN(tt, err)                                             \
    MUST_MATCH_TOKEN_THROW(tt, err, return NULL)

/*
 * Allocate a JSParseNode from cx's temporary arena.
 */
static JSParseNode *
NewParseNode(JSContext *cx, JSToken *tok, JSParseNodeArity arity)
{
    JSParseNode *pn;

    PR_ARENA_ALLOCATE(pn, &cx->tempPool, sizeof(JSParseNode));
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
    PR_ARENA_ALLOCATE(pn, &cx->tempPool, sizeof(JSParseNode));
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
	    return JS_TRUE;
	}
#endif
	js_ReportCompileError(cx, ts, "missing ; before statement");
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
    JSTokenType stop, tt;
    JSBool ok;
    JSParseNode *pn;

    fp = cx->fp;
    if (!fp || fp->scopeChain != chain) {
	memset(&frame, 0, sizeof frame);
	frame.scopeChain = chain;
	frame.down = fp;
	cx->fp = &frame;
    }

    if (ts->flags & TSF_INTERACTIVE) {
	SCAN_NEWLINES(ts);
	stop = TOK_EOL;
    } else {
	stop = TOK_EOF;
    }

    ok = JS_TRUE;
    do {
	ts->flags |= TSF_REGEXP;
	tt = js_GetToken(cx, ts);
	ts->flags &= ~TSF_REGEXP;
	if (tt == stop || tt <= TOK_EOF) {
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
    cx->fp = fp;
    if (!ok)
	CLEAR_PUSHBACK(ts);
    return ok;
}

/*
 * Insist on a final return before control flows out of pn, but don't be too
 * smart about loops (do {...; return e2;} while(0) at the end of a function
 * that contains an early return e1 will get an error XXX should be warning
 * option).
 */
static JSBool
CheckFinalReturn(JSParseNode *pn)
{
    JSBool ok;
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
      case TOK_SWITCH:
	/* If a final switch has no default case, we judge it harshly. */
      	ok = pn->pn_kid3 && CheckFinalReturn(pn->pn_kid3);
      	for (pn2 = pn->pn_kid2->pn_head; ok && pn2; pn2 = pn2->pn_next) {
	    pn3 = pn2->pn_right;
	    PR_ASSERT(pn3->pn_type == TOK_LC);
	    if (pn3->pn_head)
		ok &= CheckFinalReturn(PN_LAST(pn3));
	}
	return ok;
      case TOK_WITH:
	return CheckFinalReturn(pn->pn_right);
      case TOK_RETURN:
      	return JS_TRUE;
      default:
      	return JS_FALSE;
    }
}

static char badreturn_str[] = "function does not always return a value";

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
	    js_ReportCompileError(cx, ts, badreturn_str);
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
    js_ResetCodeGenerator(cx, &funcg);
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
    JSStmtInfo *topStmt;
    JSBool ok, named;
    JSObject *pobj;
    JSScopeProperty *sprop;
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

    /* Clear tc->topStmt for semantic checking (restore at label out:). */
    topStmt = tc->topStmt;
    tc->topStmt = NULL;

#if JS_HAS_LEXICAL_CLOSURE
    if (!funAtom || cx->fp->scopeChain != parent || InWithStatement(tc)) {
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
    MUST_MATCH_TOKEN_THROW(TOK_LP, "missing ( before formal parameters",
			   ok = JS_FALSE; goto out);
    /* balance) */
    if (!js_MatchToken(cx, ts, TOK_RP)) {
	do {
	    MUST_MATCH_TOKEN_THROW(TOK_NAME, "missing formal parameter",
				   ok = JS_FALSE; goto out);
	    argAtom = ts->token.t_atom;
	    pobj = NULL;
	    ok = js_LookupProperty(cx, fun->object, (jsid)argAtom, &pobj,
				   (JSProperty **)&sprop);
	    if (!ok)
		goto out;
	    if (sprop && pobj == fun->object) {
#ifdef CHECK_ARGUMENT_HIDING
		if (sprop->getter == js_GetArgument) {
		    OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
		    js_ReportCompileError(cx, ts,
					  "duplicate formal argument %s",
					  ATOM_BYTES(argAtom));
		    ok = JS_FALSE;
		    goto out;
		}
#endif
		sprop->getter = js_GetArgument;
		sprop->setter = js_SetArgument;
		sprop->attrs |= JSPROP_ENUMERATE | JSPROP_PERMANENT;
	    } else {
		if (sprop) {
		    OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
		    sprop = NULL;
		}
		ok = js_DefineProperty(cx, fun->object,
				       (jsid)argAtom, JSVAL_VOID,
				       js_GetArgument, js_SetArgument,
				       JSPROP_ENUMERATE | JSPROP_PERMANENT,
				       (JSProperty **)&sprop);
	    }
	    if (!ok)
		goto out;
	    PR_ASSERT(sprop);
	    sprop->id = INT_TO_JSVAL(fun->nargs++);
	    OBJ_DROP_PROPERTY(cx, fun->object, (JSProperty *)sprop);
	} while (js_MatchToken(cx, ts, TOK_COMMA));

	/* (balance: */
	MUST_MATCH_TOKEN_THROW(TOK_RP, "missing ) after formal parameters",
			       ok = JS_FALSE; goto out);
    }

    MUST_MATCH_TOKEN_THROW(TOK_LC, "missing { before function body",
			   ok = JS_FALSE; goto out);
    pn->pn_pos.begin = ts->token.pos.begin;

    pn2 = FunctionBody(cx, ts, fun, tc);
    if (!pn2) {
    	ok = JS_FALSE;
    	goto out;
    }

    MUST_MATCH_TOKEN_THROW(TOK_RC, "missing } after function body",
			   ok = JS_FALSE; goto out);
    pn->pn_pos.end = ts->token.pos.end;

    pn->pn_fun = fun;
    pn->pn_body = pn2;
    pn->pn_tryCount = tc->tryCount;

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
    tc->topStmt = topStmt;
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
    uintN newlines;
    JSTokenType tt;

    PR_ASSERT(ts->token.type == TOK_LC);
    pn = NewParseNode(cx, &ts->token, PN_LIST);
    if (!pn)
    	return NULL;
    PN_INIT_LIST(pn);

    newlines = ts->flags & TSF_NEWLINES;
    if (newlines)
	HIDE_NEWLINES(ts);

    while ((tt = js_PeekToken(cx, ts)) > TOK_EOF && tt != TOK_RC) {
	pn2 = Statement(cx, ts, tc);
	if (!pn2) {
	    pn = NULL;
	    goto out;
	}
	PN_APPEND(pn, pn2);
    }

    pn->pn_pos.end = ts->token.pos.end;
    if (tt == TOK_ERROR)
	pn = NULL;
out:
    if (newlines)
	SCAN_NEWLINES(ts);
    return pn;
}

static JSParseNode *
Condition(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2;

    MUST_MATCH_TOKEN(TOK_LP, "missing ( before condition");
    pn = Expr(cx, ts, tc);
    if (!pn)
	return NULL;
    MUST_MATCH_TOKEN(TOK_RP, "missing ) after condition");

    /*
     * Check for (a = b) and "correct" it to (a == b).
     * XXX not ECMA, but documented in several books -- need a compile option
     */
    if (pn->pn_type == TOK_ASSIGN && pn->pn_op == JSOP_NOP) {
	js_ReportCompileError(cx, ts,
	    "test for equality (==) mistyped as assignment (=)?\n"
	    "Assuming equality test");
    	pn->pn_type = TOK_EQOP;
    	pn->pn_op = cx->jsop_eq;
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
	    PR_ASSERT(0);
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

    MUST_MATCH_TOKEN(TOK_NAME, "missing name in import statement");
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
		MUST_MATCH_TOKEN(TOK_NAME, "missing name after . operator");
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

	    /* [balance: */
	    MUST_MATCH_TOKEN(TOK_RB, "missing ] in index expression");
	    pn2->pn_pos.begin = pn->pn_pos.begin;
	    pn2->pn_pos.end = ts->token.pos.end;

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
    js_ReportCompileError(cx, ts, "invalid import expression");
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
		MUST_MATCH_TOKEN(TOK_NAME, "missing name in export statement");
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
	uintN newlines;
	JSParseNode *pn5, *pn6;

	pn = NewParseNode(cx, &ts->token, PN_TERNARY);
	if (!pn)
	    return NULL;
	MUST_MATCH_TOKEN(TOK_LP, "missing ( before switch expression");

	/* pn1 points to the switch's discriminant. */
	pn1 = Expr(cx, ts, tc);
	if (!pn1)
	    return NULL;

	MUST_MATCH_TOKEN(TOK_RP, "missing ) after switch expression");
	MUST_MATCH_TOKEN(TOK_LC, "missing { before switch body");
	/* balance} */

	/* pn2 is a list of case nodes, not including the default case. */
	pn2 = NewParseNode(cx, &ts->token, PN_LIST);
	if (!pn2)
	    return NULL;
	PN_INIT_LIST(pn2);

	/* If not null, pn3 points to the default case's node. */
	pn3 = NULL;

	js_PushStatement(tc, &stmtInfo, STMT_SWITCH, -1);
	newlines = ts->flags & TSF_NEWLINES;
	if (newlines)
	    HIDE_NEWLINES(ts);

	while ((tt = js_GetToken(cx, ts)) != TOK_RC) {
	    switch (tt) {
	      case TOK_CASE:
		pn4 = NewParseNode(cx, &ts->token, PN_BINARY);
		if (!pn4)
		    goto bad_switch;
		pn4->pn_left = Expr(cx, ts, tc);
		if (!pn4->pn_left)
		    goto bad_switch;
		PN_APPEND(pn2, pn4);
		if (pn2->pn_count == PR_BIT(16)) {
		    js_ReportCompileError(cx, ts, "too many switch cases");
		    goto bad_switch;
		}
		break;

	      case TOK_DEFAULT:
		if (pn3) {
		    js_ReportCompileError(cx, ts,
					  "more than one switch default");
		    goto bad_switch;
		}
		pn4 = NULL;
		break;

	      case TOK_ERROR:
		goto bad_switch;

	      default:
		js_ReportCompileError(cx, ts, "invalid switch statement");
		goto bad_switch;
	    }
	    MUST_MATCH_TOKEN(TOK_COLON, "missing : after case label");

	    pn5 = NewParseNode(cx, &ts->token, PN_LIST);
	    if (!pn5)
	    	goto bad_switch;
	    pn5->pn_type = TOK_LC;
	    PN_INIT_LIST(pn5);
	    while ((tt = js_PeekToken(cx, ts)) != TOK_RC &&
		    tt != TOK_CASE && tt != TOK_DEFAULT) {
		if (tt == TOK_ERROR)
		    goto bad_switch;
		pn6 = Statement(cx, ts, tc);
		if (!pn6)
		    goto bad_switch;
	    	pn5->pn_pos.end = pn6->pn_pos.end;
	    	PN_APPEND(pn5, pn6);
	    }
	    if (pn4) {
		pn4->pn_pos.end = pn5->pn_pos.end;
		pn4->pn_right = pn5;
	    } else {
	    	pn3 = pn5;
	    }
	}

	if (newlines)
	    SCAN_NEWLINES(ts);
	js_PopStatement(tc);

	pn->pn_pos.end = pn2->pn_pos.end = ts->token.pos.end;
      	pn->pn_kid1 = pn1;
      	pn->pn_kid2 = pn2;
      	pn->pn_kid3 = pn3;
      	return pn;

      bad_switch:
	if (newlines)
	    SCAN_NEWLINES(ts);
      	return NULL;
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
	MUST_MATCH_TOKEN(TOK_WHILE, "missing while after do-loop body");
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

	MUST_MATCH_TOKEN(TOK_LP, "missing ( after for");	/* balance) */
	tt = js_PeekToken(cx, ts);
	if (tt == TOK_SEMI) {
	    /* No initializer -- set first kid of left sub-node to null. */
	    pn1 = NULL;
	} else {
	    /* Set pn1 to a var list or an initializing expression. */
	    if (tt == TOK_VAR) {
		(void) js_GetToken(cx, ts);
		pn1 = Variables(cx, ts, tc);
	    } else {
		pn1 = Expr(cx, ts, tc);
	    }
	    if (!pn1)
		return NULL;
	}

	/*
	 * There are three kinds of for/in syntax (see ECMA 12.6.3):
	 *  1. for (i in o) ...
	 *  2. for (var i in o) ...
	 *  3. for (var i = e in o) ...
	 * The 'var i = e in o' in 3 will be parsed as a variable declaration
	 * with an 'in' expression as its initializer, leaving ts->pushback at
	 * the right parenthesis.  This condition tests 1, then 3, then 2:
	 */
	if (pn1 && 
	    (pn1->pn_type == TOK_IN ||
	     (pn1->pn_type == TOK_VAR && ts->pushback.type == TOK_RP) ||
	     js_MatchToken(cx, ts, TOK_IN))) {
	    stmtInfo.type = STMT_FOR_IN_LOOP;

	    switch (pn1->pn_type) {
	      case TOK_IN:
		pn2 = pn1;
		pn1 = pn1->pn_left;
		break;
	      case TOK_VAR:
		if (pn1->pn_count != 1) {
		    js_ReportCompileError(cx, ts, "invalid for/in variables");
		    return NULL;
		}
		pn2 = pn1->pn_head->pn_expr;
		if (pn2) {
		    /* pn1 is 'var i = e' -- must pop e before loop. */
		    pn1->pn_op = JSOP_POP;
		    if (pn2->pn_type == TOK_IN) {
			/* Found 'var i = e in o' -- make 'in' be the root. */
			PR_ASSERT(ts->pushback.type == TOK_RP);
			pn1->pn_head->pn_expr = pn2->pn_left;
			pn2->pn_left = pn1;
			break;
		    }
		}
		/* FALL THROUGH */
	      default:
		pn2 = NULL;
		break;
	    }

	    /* Check that the left side of the 'in' is valid. */
	    if (pn1->pn_type != TOK_VAR &&
		pn1->pn_type != TOK_NAME &&
		pn1->pn_type != TOK_DOT &&
		pn1->pn_type != TOK_LB) {
		js_ReportCompileError(cx, ts, "invalid for/in left-hand side");
		return NULL;
	    }

	    /* Parse the object expression as the right operand of 'in'. */
	    if (!pn2) {
		pn2 = NewBinary(cx, TOK_IN, JSOP_NOP, pn1, Expr(cx, ts, tc));
		if (!pn2)
	    	    return NULL;
	    }
	    pn->pn_left = pn2;
	} else {
	    /* Parse the loop condition or null into pn2. */
	    MUST_MATCH_TOKEN(TOK_SEMI, "missing ; after for-loop initializer");
	    if (js_PeekToken(cx, ts) == TOK_SEMI) {
		pn2 = NULL;
	    } else {
		pn2 = Expr(cx, ts, tc);
		if (!pn2)
		    return NULL;
	    }

	    /* Parse the update expression or null into pn3. */
	    MUST_MATCH_TOKEN(TOK_SEMI, "missing ; after for-loop condition");
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

	/* (balance: */
	MUST_MATCH_TOKEN(TOK_RP, "missing ) after for-loop control");

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
      case TOK_TRY:
	/*
	 * try nodes are ternary.
	 * kid1 is the try Statement
	 * kid2 is the catch node
	 * kid3 is the finally Statement
	 */
	pn = NewParseNode(cx, &ts->token, PN_TERNARY);
	pn1 = Statement(cx, ts, tc);
	if (!pn1)
	    return NULL;
	if (js_PeekToken(cx, ts) == TOK_CATCH) {
	    pn2 = NewParseNode(cx, &ts->token, PN_BINARY);
	    if (!pn2)
		return NULL;
	    (void)js_GetToken(cx, ts);
	    MUST_MATCH_TOKEN(TOK_LP, "missing ( after catch"); /* balance) */
	    tt = js_PeekToken(cx, ts);
	    if (tt == TOK_VAR) {
		(void)js_GetToken(cx, ts);
		pn4 = Variables(cx, ts, tc);
		if (!pn4)
		    return NULL;
		if (pn4->pn_count != 1) {
		    js_ReportCompileError(cx, ts,
	       "only one variable declaration permitted in catch declaration");
		    return NULL;
		}
	    } else {
		pn4 = Expr(cx, ts, tc);
		if (!pn4)
		    return NULL;
		/* restrict the expression to instanceof */
		if (pn4->pn_op != JSOP_INSTANCEOF &&
		    pn4->pn_op != JSOP_NAME &&
		    pn4->pn_op != JSOP_GETARG &&
		    pn4->pn_op != JSOP_GETVAR) {
		    js_ReportCompileError(cx, ts,
	       "catch conditional must be instanceof or variable declaration");
		    PR_ASSERT(0);
		    return NULL;
		}
	    }

	    /* rewrite the declaration/conditional expr as appropriate */
	    switch(pn4->pn_type) {
	      case TOK_NAME:
		switch(pn4->pn_op) {
		  case JSOP_NAME:
		    pn4->pn_op = JSOP_SETNAME;
		    break;
		  case JSOP_GETARG:
		    pn4->pn_op = JSOP_SETARG;
		    break;
		default:
		    PR_ASSERT(0);
		}
		break;
	      case TOK_VAR: 
		PR_ASSERT(pn4->pn_head->pn_type == TOK_NAME);
		switch(pn4->pn_head->pn_op) {
		  case JSOP_GETVAR:
		    pn4->pn_head->pn_op = JSOP_SETVAR;
		    break;
		  case JSOP_GETARG:
		    pn4->pn_head->pn_op = JSOP_SETARG;
		    break;
		  case JSOP_NAME:
		    pn4->pn_head->pn_op = JSOP_SETNAME;
		  case JSOP_NOP:
		    break;
		  default:
		    PR_ASSERT(0);
		}
		break;
	    case TOK_INSTANCEOF:
		PR_ASSERT(0);
	    default:
		PR_ASSERT(0);
	    }
	    pn2->pn_left = pn4;
	    
	    /* (balance: */
	    MUST_MATCH_TOKEN(TOK_RP, "missing ) after catch declaration");
	    pn2->pn_right = Statement(cx, ts, tc);
	    if (!pn2->pn_right)
		return NULL;
	} else {
	    pn2 = NULL;
	}
	if (js_MatchToken(cx, ts, TOK_FINALLY)) {
	    pn3 = Statement(cx, ts, tc);
	    if (!pn3)
		return NULL;
	} else {
	    pn3 = NULL;
	}
	if (!pn2 && !pn3) {
	    js_ReportCompileError(cx, ts,
				  "missing catch or finally after try");
	    return NULL;
	}
	tc->tryCount++;
	pn->pn_kid1 = pn1;
	pn->pn_kid2 = pn2;
	pn->pn_kid3 = pn3;
	return pn;

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
		    js_ReportCompileError(cx, ts, "label not found");
		    return NULL;
		}
		if (stmt->type == STMT_LABEL && stmt->label == label)
		    break;
	    }
	} else {
	    for (; ; stmt = stmt->down) {
		if (!stmt) {
		    js_ReportCompileError(cx, ts, "invalid break");
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
		    js_ReportCompileError(cx, ts, "label not found");
		    return NULL;
		}
		if (stmt->type == STMT_LABEL) {
		    if (stmt->label == label) {
			if (!stmt2 || !STMT_IS_LOOP(stmt2)) {
			    js_ReportCompileError(cx, ts, "invalid continue");
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
		    js_ReportCompileError(cx, ts, "invalid continue");
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
	MUST_MATCH_TOKEN(TOK_LP, "missing ( before with-statement object");
	/* balance) */
	pn2 = Expr(cx, ts, tc);
	if (!pn2)
	    return NULL;
	/* (balance: */
	MUST_MATCH_TOKEN(TOK_RP, "missing ) after with-statement object");
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
	    js_ReportCompileError(cx, ts, "invalid return");
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

	if ((tc->flags & (TCF_RETURN_EXPR | TCF_RETURN_VOID)) ==
	    (TCF_RETURN_EXPR | TCF_RETURN_VOID)) {
	    js_ReportCompileError(cx, ts, badreturn_str);
	    return NULL;
	}
	break;

      case TOK_LC:
	js_PushStatement(tc, &stmtInfo, STMT_BLOCK, -1);
	pn = Statements(cx, ts, tc);
	if (!pn)
	    return NULL;

	/* {balance: */
	MUST_MATCH_TOKEN(TOK_RC, "missing } in compound statement");
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
		js_ReportCompileError(cx, ts, "invalid label");
		return NULL;
	    }
	    label = pn2->pn_atom;
	    for (stmt = tc->topStmt; stmt; stmt = stmt->down) {
		if (stmt->type == STMT_LABEL && stmt->label == label) {
		    js_ReportCompileError(cx, ts, "duplicate label");
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
    JSPropertyOp getter, setter;
    JSAtom *atom;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSBool ok;

    PR_ASSERT(ts->token.type == TOK_VAR);
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
	getter = js_GetLocalVariable;
	setter = js_SetLocalVariable;
    } else {
	getter = clasp->getProperty;
	setter = clasp->setProperty;
    }

    do {
	MUST_MATCH_TOKEN(TOK_NAME, "missing variable name");
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
		getter = sprop->getter;
#ifdef CHECK_ARGUMENT_HIDING
		js_ReportCompileError(cx, ts, "variable %s hides argument",
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
			PR_ASSERT(sprop->getter == js_GetLocalVariable);
			PR_ASSERT(JSVAL_IS_INT(sprop->id) &&
				  JSVAL_TO_INT(sprop->id) < fun->nvars);
		    }
		} else {
		    /* Global var: (re-)set id a la js_DefineProperty. */
		    sprop->id = ATOM_KEY(atom);
		}
		sprop->getter = getter;
		sprop->setter = setter;
		sprop->attrs |= JSPROP_ENUMERATE | JSPROP_PERMANENT;
		sprop->attrs &= ~JSPROP_READONLY;
	    }
	} else {
	    sprop = NULL;
	    if (prop) {
		OBJ_DROP_PROPERTY(cx, pobj, prop);
		prop = NULL;
	    }
	    ok = OBJ_DEFINE_PROPERTY(cx, obj, (jsid)atom, JSVAL_VOID,
				     getter, setter,
				     JSPROP_ENUMERATE | JSPROP_PERMANENT,
				     &prop);
	    if (ok && prop) {
		pobj = obj;
		if (getter == js_GetLocalVariable) {
		    sprop = (JSScopeProperty *)prop;
		    sprop->id = INT_TO_JSVAL(fun->nvars++);
		}
	    }
	}

	if (js_MatchToken(cx, ts, TOK_ASSIGN)) {
	    if (ts->token.t_op != JSOP_NOP) {
		js_ReportCompileError(cx, ts,
				      "invalid variable initialization");
		ok = JS_FALSE;
	    } else {
		pn2->pn_expr = AssignExpr(cx, ts, tc);
		if (pn2->pn_expr)
		    pn2->pn_op = JSOP_SETNAME2;
		else
		    ok = JS_FALSE;
	    }
	}

	if (ok && fun && clasp == &js_FunctionClass && !InWithStatement(tc)) {
	    PR_ASSERT(sprop);
	    if (getter == js_GetArgument) {
		PR_ASSERT(sprop && JSVAL_IS_INT(sprop->id));
		pn2->pn_op = (pn2->pn_op == JSOP_NAME)
			     ? JSOP_GETARG
			     : JSOP_SETARG;
		pn2->pn_slot = JSVAL_TO_INT(sprop->id);
	    } else if (getter == js_GetLocalVariable) {
		PR_ASSERT(sprop && JSVAL_IS_INT(sprop->id));
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

/* ZZZbe don't create functions till codegen? or at least don't bind fn name */
static JSBool
LookupArgOrVar(JSContext *cx, JSAtom *atom, JSTreeContext *tc,
	       JSOp *opp, jsint *slotp)
{
    JSObject *obj, *pobj;
    JSScopeProperty *sprop;

    obj = cx->fp->scopeChain;
    if (OBJ_GET_CLASS(cx, obj) != &js_FunctionClass)
    	return JS_TRUE;
    if (InWithStatement(tc))
    	return JS_TRUE;
    if (!js_LookupProperty(cx, obj, (jsid)atom, &pobj, (JSProperty **)&sprop))
	return JS_FALSE;
    *opp = JSOP_NAME;
    *slotp = -1;
    if (sprop) {
	if (sprop->getter == js_GetArgument) {
	    *opp = JSOP_GETARG;
	    *slotp = JSVAL_TO_INT(sprop->id);
	} else if (sprop->getter == js_GetLocalVariable) {
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
		PR_ASSERT(pn2->pn_op == JSOP_GETARG || pn2->pn_op == JSOP_GETVAR);
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
	    js_ReportCompileError(cx, ts, "invalid assignment left-hand side");
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

    pn = OrExpr(cx, ts, tc);
    if (pn && js_MatchToken(cx, ts, TOK_HOOK)) {
	pn1 = pn;
	pn = NewParseNode(cx, &ts->token, PN_TERNARY);
	if (!pn)
	    return NULL;
	pn2 = AssignExpr(cx, ts, tc);
	if (!pn2)
	    return NULL;
	MUST_MATCH_TOKEN(TOK_COLON, "missing : in conditional expression");
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

    pn = ShiftExpr(cx, ts, tc);
    while (pn &&
	   (js_MatchToken(cx, ts, TOK_RELOP)
#if JS_HAS_IN_OPERATOR
	    || js_MatchToken(cx, ts, TOK_IN)
#endif
#if JS_HAS_INSTANCEOF
	    || js_MatchToken(cx, ts, TOK_INSTANCEOF)
#endif
	    )) {
	tt = ts->token.type;
	op = ts->token.t_op;
	pn = NewBinary(cx, tt, op, pn, ShiftExpr(cx, ts, tc));
    }
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
	js_ReportCompileError(cx, ts, "invalid %s operand", name);
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
	PR_ASSERT(0);
    }
    pn->pn_op = op;
    pn->pn_num = num;
    return JS_TRUE;
}

static JSParseNode *
UnaryExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSTokenType tt;
    JSParseNode *pn, *pn2, *pn3;

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
	pn2 = MemberExpr(cx, ts, tc);
	if (!pn2)
	    return NULL;
	if (!SetIncOpKid(cx, ts, tc, pn, pn2, tt, JS_TRUE))
	    return NULL;
	pn->pn_pos.end = pn2->pn_pos.end;
	break;

      case TOK_NEW:
	/* Allow 'new this.ctor(...)' constructor expressions. */
	pn = NewParseNode(cx, &ts->token, PN_LIST);
	if (!pn)
	    return NULL;
	tt = js_GetToken(cx, ts);
	pn2 = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn2)
	    return NULL;
	switch (tt) {
	  case TOK_NAME:
	    pn2->pn_op = JSOP_NAME;
	    pn2->pn_atom = ts->token.t_atom;
	    pn2->pn_slot = -1;
	    break;
	  case TOK_PRIMARY:
	    if (ts->token.t_op == JSOP_THIS) {
		pn2->pn_op = JSOP_THIS;
		break;
	    }
	    /* FALL THROUGH */
	  default:
	    js_ReportCompileError(cx, ts, "missing name after new operator");
	    return NULL;
	}
	while (js_MatchToken(cx, ts, TOK_DOT)) {
	    pn3 = NewParseNode(cx, &ts->token, PN_NAME);
	    if (!pn3)
	    	return NULL;
	    MUST_MATCH_TOKEN(TOK_NAME,
			     "missing name in constructor expression");
	    pn3->pn_pos.begin = pn2->pn_pos.begin;
	    pn3->pn_pos.end = ts->token.pos.end;
	    pn3->pn_op = JSOP_GETPROP;
	    pn3->pn_expr = pn2;
	    pn3->pn_atom = ts->token.t_atom;
	    pn2 = pn3;
	}
	PN_INIT_LIST_1(pn, pn2);
	if (js_MatchToken(cx, ts, TOK_LP) && !js_MatchToken(cx, ts, TOK_RP)) {
	    do {
		pn2 = AssignExpr(cx, ts, tc);
		if (!pn2)
		    return NULL;
		PN_APPEND(pn, pn2);
	    } while (js_MatchToken(cx, ts, TOK_COMMA));

	    /* (balance: */
	    MUST_MATCH_TOKEN(TOK_RP,
			     "missing ) after constructor argument list");
	}
	if (pn->pn_count - 1 >= ARGC_LIMIT) {
	    JS_ReportError(cx, "too many constructor arguments");
	    return NULL;
	}
	pn->pn_pos.end = PN_LAST(pn)->pn_pos.end;
	break;

      case TOK_DELETE:
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
	if (!pn)
	    return NULL;
	pn2 = MemberExpr(cx, ts, tc);
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
	pn = MemberExpr(cx, ts, tc);
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
MemberExpr(JSContext *cx, JSTokenStream *ts, JSTreeContext *tc)
{
    JSParseNode *pn, *pn2, *pn3;
    JSTokenType tt;
    JSBool matched;

    pn = PrimaryExpr(cx, ts, tc);
    if (!pn)
	return NULL;

    while ((tt = js_GetToken(cx, ts)) > TOK_EOF) {
	if (tt == TOK_DOT) {
	    pn2 = NewParseNode(cx, &ts->token, PN_NAME);
	    if (!pn2)
	    	return NULL;
	    MUST_MATCH_TOKEN(TOK_NAME, "missing name after . operator");
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

	    /* [balance: */
	    MUST_MATCH_TOKEN(TOK_RB, "missing ] in index expression");
	    pn2->pn_pos.begin = pn->pn_pos.begin;
	    pn2->pn_pos.end = ts->token.pos.end;

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
	} else if (tt == TOK_LP) {
	    pn2 = NewParseNode(cx, &ts->token, PN_LIST);
	    if (!pn2)
	    	return NULL;
	    PN_INIT_LIST_1(pn2, pn);
	    pn2->pn_op = JSOP_CALL;

	    ts->flags |= TSF_REGEXP;
	    matched = js_MatchToken(cx, ts, TOK_RP);
	    ts->flags &= ~TSF_REGEXP;
	    if (!matched) {
		do {
		    pn3 = AssignExpr(cx, ts, tc);
		    if (!pn3)
			return NULL;
		    PN_APPEND(pn2, pn3);
		} while (js_MatchToken(cx, ts, TOK_COMMA));

		/* (balance: */
		MUST_MATCH_TOKEN(TOK_RP, "missing ) after argument list");
	    }

	    if (pn2->pn_count - 1 >= ARGC_LIMIT) {
		JS_ReportError(cx, "too many function arguments");
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

	    /* [balance: */
	    MUST_MATCH_TOKEN(TOK_RB, "missing ] after element list");
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
		    js_ReportCompileError(cx, ts, "invalid property id");
		    return NULL;
		}

		MUST_MATCH_TOKEN(TOK_COLON, "missing : after property id");
		pn2 = NewBinary(cx, TOK_COLON, JSOP_INITPROP, pn3,
				AssignExpr(cx, ts, tc));
		if (!pn2)
		    return NULL;
	    	PN_APPEND(pn, pn2);
	    } while (js_MatchToken(cx, ts, TOK_COMMA));

	    /* {balance: */
	    MUST_MATCH_TOKEN(TOK_RC, "missing } after property list");
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
	pn = NewParseNode(cx, &ts->token, PN_UNARY);
	if (!pn)
	    return NULL;
	pn2 = Expr(cx, ts, tc);
	if (!pn2)
	    return NULL;

	/* (balance: */
	MUST_MATCH_TOKEN(TOK_RP, "missing ) in parenthetical");
	pn->pn_type = TOK_RP;
	pn->pn_pos.end = ts->token.pos.end;
	pn->pn_kid = pn2;
	break;

      case TOK_STRING:
	notsharp = JS_TRUE;
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
	notsharp = JS_TRUE;
	break;

      case TOK_PRIMARY:
	pn = NewParseNode(cx, &ts->token, PN_NULLARY);
	if (!pn)
	    return NULL;
	pn->pn_op = ts->token.t_op;
	notsharp = JS_TRUE;
	break;

#if !JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
      case TOK_IMPORT:
#endif
      case TOK_RESERVED:
	js_ReportCompileError(cx, ts, "%.*s is a reserved identifier",
	    ts->token.pos.end.index - ts->token.pos.begin.index,
	    ts->token.ptr);
	return NULL;

      case TOK_ERROR:
	/* The scanner or one of its subroutines reported the error. */
	return NULL;

      default:
	js_ReportCompileError(cx, ts, "syntax error");
	return NULL;
    }

#if JS_HAS_SHARP_VARS
    if (defsharp) {
	if (notsharp) {
  badsharp:
	    js_ReportCompileError(cx, ts, "invalid sharp variable definition");
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
    JSParseNode *pn1, *pn2, *pn3;

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
	/* Neither kid may be null. */
	pn1 = pn->pn_left;
	pn2 = pn->pn_right;
	if (!js_FoldConstants(cx, pn1))
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
	    mark = PR_ARENA_MARK(&cx->tempPool);
	    PR_ARENA_ALLOCATE(chars, &cx->tempPool, nbytes);
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
	    PR_ARENA_RELEASE(&cx->tempPool, mark);
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
		d = -d;
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
