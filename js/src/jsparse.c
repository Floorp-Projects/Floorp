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
 * feedback to disambiguate non-LL(1) structures.  It generates bytecode as it
 * parses, with backpatching and code rewriting limited to downward branches,
 * for loops, and assignable "lvalue" expressions.
 *
 * This parser attempts no error recovery.  The dense JSTokenType enumeration
 * was designed with error recovery built on 64-bit first and follow bitsets
 * in mind, however.
 */
#include <stdlib.h>
#include <string.h>
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
typedef JSBool
JSParser(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg);

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
 * Insist that the next token be of type tt, or report err and return false.
 * NB: this macro uses cx, ts, and cg from its lexical environment.
 */
#define MUST_MATCH_TOKEN_THROW(tt, err, throw)                                \
    PR_BEGIN_MACRO                                                            \
	if (js_GetToken(cx, ts, cg) != tt) {                                  \
	    js_ReportCompileError(cx, ts, err);                               \
	    throw;                                                            \
	}                                                                     \
    PR_END_MACRO

#define MUST_MATCH_TOKEN(tt, err)                                             \
    MUST_MATCH_TOKEN_THROW(tt, err, return JS_FALSE)

/*
 * Emit a bytecode and its 2-byte constant (atom) index immediate operand.
 * NB: We use cx and cg from our caller's lexical environment, and return
 * false on error.
 */
#define EMIT_CONST_ATOM_OP(op, atomIndex)                                     \
    PR_BEGIN_MACRO                                                            \
	if (js_Emit3(cx, cg, op, ATOM_INDEX_HI(atomIndex),                    \
				 ATOM_INDEX_LO(atomIndex)) < 0) {             \
	    return JS_FALSE;                                                  \
	}                                                                     \
    PR_END_MACRO

/*
 * Parse a top-level JS script.
 */
JS_FRIEND_API(JSBool)
js_Parse(JSContext *cx, JSObject *chain, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSStackFrame *fp, frame;
    JSTokenType stop, tt;
    JSBool ok;

    PR_ASSERT(JS_IS_LOCKED(cx));
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
	tt = js_GetToken(cx, ts, cg);
	ts->flags &= ~TSF_REGEXP;
	if (tt == stop || tt <= TOK_EOF) {
	    if (tt == TOK_ERROR)
		ok = JS_FALSE;
	    break;
	}

	switch (tt) {
	  case TOK_FUNCTION:
	    if (!FunctionStmt(cx, ts, cg))
		ok = JS_FALSE;
	    break;
	  default:
	    js_UngetToken(ts);
	    if (!Statement(cx, ts, cg))
		ok = JS_FALSE;
	}
    } while (ok);

    cx->fp = fp;
    if (ok)
	ok = js_FlushNewlines(cx, ts, cg);
    if (!ok) {
	CLEAR_PUSHBACK(ts);
	js_DropUnmappedAtoms(cx, &cg->atomList);
    }
    return ok;
}

/*
 * Parse a JS function body, which might appear as the value of an event
 * handler attribute in a HTML <INPUT> tag.
 */
JSBool
js_ParseFunctionBody(JSContext *cx, JSTokenStream *ts, JSFunction *fun,
		     JSSymbol *args)
{
    uintN lineno, oldflags;
    JSCodeGenerator funcg;
    JSStackFrame *fp, frame;
    JSBool ok;
    jsbytecode *pc;

    PR_ASSERT(JS_IS_LOCKED(cx));

    lineno = ts->lineno;
    if (!js_InitCodeGenerator(cx, &funcg, &cx->codePool))
	return JS_FALSE;

    fp = cx->fp;
    if (!fp || fp->scopeChain != fun->object) {
	memset(&frame, 0, sizeof frame);
	frame.scopeChain = fun->object;
	frame.down = fp;
	cx->fp = &frame;
    }

    oldflags = ts->flags;
    ts->flags &= ~(TSF_RETURN_EXPR | TSF_RETURN_VOID);
    ts->flags |= TSF_FUNCTION;
    ok = Statements(cx, ts, &funcg);

    /* Check for falling off the end of a function that returns a value. */
    if (ok && (ts->flags & TSF_RETURN_EXPR)) {
	for (pc = CG_CODE(&funcg,funcg.lastCodeOffset);
	     *pc == JSOP_NOP || *pc == JSOP_LEAVEWITH;
	     pc--) {
	    /* nothing */;
	}
	if (*pc != JSOP_RETURN) {
	    js_ReportCompileError(cx, ts,
				  "function does not always return a value");
	    ok = JS_FALSE;
	}
    }
    ts->flags = oldflags;
    cx->fp = fp;

    if (ok)
	ok = js_FlushNewlines(cx, ts, &funcg);
    if (!ok) {
	CLEAR_PUSHBACK(ts);
	js_DropUnmappedAtoms(cx, &funcg.atomList);
    } else {
	funcg.args = args;
	fun->script = js_NewScript(cx, &funcg, ts->filename, lineno,
				   ts->principals, fun);
	if (!fun->script)
	    ok = JS_FALSE;
    }
    return ok;
}

#if JS_HAS_LEXICAL_CLOSURE
static JSBool
InWithStatement(JSCodeGenerator *cg)
{
    StmtInfo *stmt;

    for (stmt = cg->stmtInfo; stmt; stmt = stmt->down) {
	if (stmt->type == STMT_WITH)
	    return JS_TRUE;
    }
    return JS_FALSE;
}
#endif

static JSBool
FunctionDef(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg, JSBool expr)
{
    JSAtomMap map;
    JSAtom *funAtom, *argAtom;
    JSObject *parent;
    JSFunction *fun, *outerFun;
    JSBool ok, named;
    JSSymbol *arg, *args, **argp;
    JSObject *pobj;
    JSProperty *prop;
    void *mark;
    jsval junk;
    uint32 i;

    /* Save atoms indexed but not mapped for the top-level script. */
    if (!js_InitAtomMap(cx, &map, &cg->atomList))
	return JS_FALSE;

    if (js_MatchToken(cx, ts, cg, TOK_NAME))
	funAtom = js_HoldAtom(cx, ts->token.u.atom);
    else
	funAtom = NULL;

    /* Find the nearest variable-declaring scope and use it as our parent. */
    parent = js_FindVariableScope(cx, &outerFun);
    if (!parent) {
	ok = JS_FALSE;
	goto out;
    }

#if JS_HAS_LEXICAL_CLOSURE
    if (!funAtom || InWithStatement(cg) || cx->fp->scopeChain != parent) {
	/* Don't name the function if enclosed by a with statement or equiv. */
	fun = js_NewFunction(cx, NULL, 0, 0, cx->fp->scopeChain, funAtom);
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
    args = NULL;
    MUST_MATCH_TOKEN_THROW(TOK_LP, "missing ( before formal parameters",
			   ok = JS_FALSE; goto out);
    /* balance) */
    if (!js_MatchToken(cx, ts, cg, TOK_RP)) {
	argp = &args;
	do {
	    MUST_MATCH_TOKEN_THROW(TOK_NAME, "missing formal parameter",
				   ok = JS_FALSE; goto out);
	    argAtom = js_HoldAtom(cx, ts->token.u.atom);
	    pobj = NULL;
	    ok = js_LookupProperty(cx, fun->object, (jsval)argAtom, &pobj,
				   &prop);
	    if (!ok)
		goto out;
	    if (prop && prop->object == fun->object) {
		if (prop->getter == js_GetArgument) {
		    js_ReportCompileError(cx, ts,
					  "duplicate formal argument %s",
					  ATOM_BYTES(argAtom));
		    ok = JS_FALSE;
		    goto out;
		}
		prop->getter = js_GetArgument;
		prop->setter = js_SetArgument;
		prop->flags |= JSPROP_ENUMERATE | JSPROP_PERMANENT;
	    } else {
		prop = js_DefineProperty(cx, fun->object,
					 (jsval)argAtom, JSVAL_VOID,
					 js_GetArgument, js_SetArgument,
					 JSPROP_ENUMERATE | JSPROP_PERMANENT);
	    }
	    js_DropAtom(cx, argAtom);
	    if (!prop) {
		ok = JS_FALSE;
		goto out;
	    }
	    prop->id = INT_TO_JSVAL(fun->nargs++);
	    arg = prop->symbols;
	    *argp = arg;
	    argp = &arg->next;
	} while (js_MatchToken(cx, ts, cg, TOK_COMMA));

	/* (balance: */
	MUST_MATCH_TOKEN_THROW(TOK_RP, "missing ) after formal parameters",
			       ok = JS_FALSE; goto out);
    }

    MUST_MATCH_TOKEN_THROW(TOK_LC, "missing { before function body",
			   ok = JS_FALSE; goto out);
    mark = PR_ARENA_MARK(&cx->codePool);
    ok = js_ParseFunctionBody(cx, ts, fun, args);
    if (ok) {
	MUST_MATCH_TOKEN_THROW(TOK_RC, "missing } after function body",
			       ok = JS_FALSE; goto out);
	fun->script->depth += fun->nvars;

	/* Generate a setline note for script that follows this function. */
	if (js_NewSrcNote2(cx, cg, SRC_SETLINE, (ptrdiff_t)TRUE_LINENO(ts)) < 0)
	    ok = JS_FALSE;
    }
    PR_ARENA_RELEASE(&cx->codePool, mark);

out:
    if (!ok && named)
	(void) js_DeleteProperty(cx, parent, (jsval)funAtom, &junk);
    for (i = 0; i < map.length; i++) {
	js_IndexAtom(cx, map.vector[i], &cg->atomList);
	PR_ASSERT(map.vector[i]->index == i);
    }
    js_FreeAtomMap(cx, &map);
    if (funAtom)
	js_DropAtom(cx, funAtom);

#if JS_HAS_LEXICAL_CLOSURE
    if (fun) {
	JSBool closure = (outerFun ||
			  InWithStatement(cg) ||
			  cx->fp->scopeChain != parent);

	if (closure || expr) {
	    jsatomid atomIndex;

	    /* Make the function object a literal in the outer script's pool. */
	    funAtom = js_AtomizeObject(cx, fun->object, ATOM_NOHOLD);
	    if (!funAtom)
		return JS_FALSE;

	    /* Emit a closure or push naming the literal in its immediate. */
	    atomIndex = js_IndexAtom(cx, funAtom, &cg->atomList);
	    EMIT_CONST_ATOM_OP(closure ? JSOP_CLOSURE : JSOP_OBJECT, atomIndex);
	}
    }
#endif
    return ok;
}

static JSBool
FunctionStmt(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    return FunctionDef(cx, ts, cg, JS_FALSE);
}

#if JS_HAS_LEXICAL_CLOSURE
static JSBool
FunctionExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    return FunctionDef(cx, ts, cg, JS_TRUE);
}
#endif

static JSBool
Statements(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    uintN newlines;
    JSBool ok;
    JSTokenType tt;

    newlines = ts->flags & TSF_NEWLINES;
    if (newlines)
	HIDE_NEWLINES(ts);

    ok = JS_TRUE;
    while ((tt = js_PeekToken(cx, ts, cg)) > TOK_EOF && tt != TOK_RC) {
	ok = Statement(cx, ts, cg);
	if (!ok)
	    break;
    }
    if (tt == TOK_ERROR)
	ok = JS_FALSE;
    if (newlines)
	SCAN_NEWLINES(ts);
    return ok;
}

static JSBool
Condition(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    jsbytecode *pc;
    JSOp lastop;
    jsatomid atomIndex;

    MUST_MATCH_TOKEN(TOK_LP, "missing ( before condition");
    if (!Expr(cx, ts, cg))
	return JS_FALSE;
    MUST_MATCH_TOKEN(TOK_RP, "missing ) after condition");

    /*
     * Check for an AssignExpr (see below) and "correct" it to an EqExpr.
     */
    pc = CG_CODE(cg, cg->lastCodeOffset);
    lastop = (JSOp) *pc;
    if (lastop == JSOP_SETNAME &&
	(cg->noteCount == 0 ||
	 cg->lastNoteOffset != cg->lastCodeOffset - 1 ||
	 SN_TYPE(&cg->notes[cg->saveNoteCount]) != SRC_ASSIGNOP)) {
	js_ReportCompileError(cx, ts,
	    "test for equality (==) mistyped as assignment (=)?\n"
	    "Assuming equality test");
	atomIndex = GET_ATOM_INDEX(pc);
	js_CancelLastOpcode(cx, cg, &ts->newlines);
	EMIT_CONST_ATOM_OP(JSOP_NAME, atomIndex);
	if (js_Emit1(cx, cg, cx->jsop_eq) < 0)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
WellTerminated(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg,
	       JSTokenType lastExprType)
{
    JSTokenType tt;

    tt = ts->pushback.type;
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

static JSBool
MatchLabel(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg,
	   JSAtom **labelp)
{
#if JS_HAS_LABEL_STATEMENT
    JSTokenType tt;

    tt = js_PeekTokenSameLine(cx, ts, cg);
    if (tt == TOK_ERROR)
	return JS_FALSE;
    if (tt == TOK_NAME) {
	if (js_GetToken(cx, ts, cg) == TOK_ERROR)
	    return JS_FALSE;
	*labelp = ts->token.u.atom;
	js_IndexAtom(cx, *labelp, &cg->atomList);
    } else {
	*labelp = NULL;
    }
#else
    *labelp = NULL;
#endif
    return WellTerminated(cx, ts, cg, TOK_ERROR);
}

#if JS_HAS_EXPORT_IMPORT
static JSBool
ImportExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    ptrdiff_t begin;
    jsatomid atomIndex;
    JSOp op;
    JSTokenType tt;

    begin = CG_OFFSET(cg);
    MUST_MATCH_TOKEN(TOK_NAME, "missing name in import statement");
    atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
    op = JSOP_NAME;

    ts->flags |= TSF_LOOKAHEAD;
    while ((tt = js_GetToken(cx, ts, cg)) == TOK_DOT || tt == TOK_LB) {
	ts->flags &= ~TSF_LOOKAHEAD;
	switch (op) {
	  case JSOP_NAME:
	  case JSOP_GETPROP:
	    EMIT_CONST_ATOM_OP(op, atomIndex);
	    break;
	  case JSOP_GETELEM:
	    if (js_Emit1(cx, cg, op) < 0)
		return JS_FALSE;
	    break;
	  case JSOP_IMPORTALL:
	    goto bad_import;
	  default:;
	}
	if (!js_FlushNewlines(cx, ts, cg))
	    return JS_FALSE;

	if (tt == TOK_DOT) {
	    /*
	     * Compile import-property opcode.
	     */
	    if (js_MatchToken(cx, ts, cg, TOK_STAR)) {
		op = JSOP_IMPORTALL;
	    } else {
		MUST_MATCH_TOKEN(TOK_NAME, "missing name after . operator");

		if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
				   (ptrdiff_t)(CG_OFFSET(cg) - begin)) < 0) {
		    return JS_FALSE;
		}
		atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
		op = JSOP_GETPROP;
	    }
	} else {
	    /*
	     * Compile index expression and get-slot opcode.
	     */
	    if (!Expr(cx, ts, cg))
		return JS_FALSE;
	    /* [balance: */
	    MUST_MATCH_TOKEN(TOK_RB, "missing ] in index expression");

	    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
			       (ptrdiff_t)(CG_OFFSET(cg) - begin)) < 0) {
		return JS_FALSE;
	    }
	    op = JSOP_GETELEM;
	}

	ts->flags |= TSF_LOOKAHEAD;
    }
    ts->flags &= ~TSF_LOOKAHEAD;
    if (tt == TOK_ERROR)
	return JS_FALSE;
    js_UngetToken(ts);

    switch (op) {
      case JSOP_GETPROP:
	EMIT_CONST_ATOM_OP(JSOP_IMPORTPROP, atomIndex);
	break;
      case JSOP_GETELEM:
	op = JSOP_IMPORTELEM;
	/* FALL THROUGH */
      case JSOP_IMPORTALL:
	if (js_Emit1(cx, cg, op) < 0)
	    return JS_FALSE;
	break;
      default:
	goto bad_import;
    }
    return JS_TRUE;

  bad_import:
    js_ReportCompileError(cx, ts, "invalid import expression");
    return JS_FALSE;
}
#endif /* JS_HAS_EXPORT_IMPORT */

static JSBool
Statement(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    StmtInfo stmtInfo, *stmt, *stmt2;
    intN snindex;
    uintN lineno, topNoteCount;
    JSBool ok, isForInLoop, hasVarNote;
    ptrdiff_t top, beq, jmp;
    JSCodeGenerator cg2;
    jsbytecode *pc;
    JSOp lastop;
    jsatomid atomIndex;
    JSTokenType tt, lastExprType;
    JSAtom *label;

    switch (js_GetToken(cx, ts, cg)) {
#if JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
	lineno = ts->lineno;
	if (js_MatchToken(cx, ts, cg, TOK_STAR)) {
	    if (js_Emit1(cx, cg, JSOP_EXPORTALL) < 0)
		return JS_FALSE;
	} else {
	    do {
		MUST_MATCH_TOKEN(TOK_NAME, "missing name in export statement");
		atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
		EMIT_CONST_ATOM_OP(JSOP_EXPORTNAME, atomIndex);
	    } while (js_MatchToken(cx, ts, cg, TOK_COMMA));
	}
	if (ts->lineno == lineno && !WellTerminated(cx, ts, cg, TOK_ERROR))
	    return JS_FALSE;
	break;

      case TOK_IMPORT:
	lineno = ts->lineno;
	do {
	    if (!ImportExpr(cx, ts, cg))
		return JS_FALSE;
	} while (js_MatchToken(cx, ts, cg, TOK_COMMA));
	if (ts->lineno == lineno && !WellTerminated(cx, ts, cg, TOK_ERROR))
	    return JS_FALSE;
	break;
#endif /* JS_HAS_EXPORT_IMPORT */

      case TOK_IF:
	if (!Condition(cx, ts, cg))
	    return JS_FALSE;
	js_PushStatement(cg, &stmtInfo, STMT_IF, CG_OFFSET(cg));
	snindex = js_NewSrcNote(cx, cg, SRC_IF);
	if (snindex < 0)
	    return JS_FALSE;
	beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	if (beq < 0)
	    return JS_FALSE;
	if (!Statement(cx, ts, cg))
	    return JS_FALSE;
	if (js_MatchToken(cx, ts, cg, TOK_ELSE)) {
	    stmtInfo.type = STMT_ELSE;
	    SN_SET_TYPE(&cg->notes[snindex], SRC_IF_ELSE);
	    jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	    if (jmp < 0)
		return JS_FALSE;
	    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	    if (!Statement(cx, ts, cg))
		return JS_FALSE;
	    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
	} else {
	    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	}
	return js_PopStatement(cx, cg);

#if JS_HAS_SWITCH_STATEMENT
      case TOK_SWITCH:
      {
	JSOp switchop;
	uintN newlines;
	SwitchCase *caselist;   /* singly-linked case list */
	SwitchCase **casetail;  /* pointer to pointer to last case */
	SwitchCase *defaultsc;  /* default switch case or null */
	uint32 ncases, tablen;
	JSAtomMap map;
	JSScript *script;
	jsval value;
	SwitchCase *sc;
	jsint i, low, high;
	size_t length;
	ptrdiff_t offset;

	MUST_MATCH_TOKEN(TOK_LP, "missing ( before switch expression");
	if (!Expr(cx, ts, cg))
	    return JS_FALSE;
	MUST_MATCH_TOKEN(TOK_RP, "missing ) after switch expression");
	MUST_MATCH_TOKEN(TOK_LC, "missing { before switch body");
	/* balance} */

	top = CG_OFFSET(cg);
	js_PushStatement(cg, &stmtInfo, STMT_SWITCH, top);

	snindex = js_NewSrcNote2(cx, cg, SRC_SWITCH, 0);
	if (snindex < 0)
	    return JS_FALSE;
	topNoteCount = cg->noteCount;

	switchop = JSOP_TABLESWITCH;
	if (js_Emit1(cx, cg, switchop) < 0)
	    return JS_FALSE;

	ok = JS_TRUE;
	cg2.base = NULL;
	caselist = NULL;
	casetail = &caselist;
	defaultsc = NULL;
	ncases = 0;

	newlines = ts->flags & TSF_NEWLINES;
	if (newlines)
	    HIDE_NEWLINES(ts);

	while ((tt = js_GetToken(cx, ts, cg)) != TOK_RC) {
	    switch (tt) {
	      case TOK_CASE:
		if (!cg2.base) {
		    ok = js_InitCodeGenerator(cx, &cg2, &cx->tempPool);
		    if (!ok)
			goto end_switch;
		} else {
		    CG_RESET(&cg2);
		}
		lineno = ts->lineno;

		/* Save atoms indexed but not mapped before compiling. */
		ok = js_InitAtomMap(cx, &map, &cg->atomList);
		if (!ok)
		    goto end_switch;
		ok = Expr(cx, ts, &cg2) && js_Emit1(cx, &cg2, JSOP_POPV) >= 0;
		if (ok) {
		    script = js_NewScript(cx, &cg2, ts->filename, lineno,
					  ts->principals, NULL);
		    if (!script)
			ok = JS_FALSE;
		}
		for (i = 0; i < (jsint)map.length; i++) {
		    js_IndexAtom(cx, map.vector[i], &cg->atomList);
		    PR_ASSERT((jsint)map.vector[i]->index == i);
		}
		js_FreeAtomMap(cx, &map);

		if (!ok)
		    goto end_switch;
		ok = js_Execute(cx, cx->fp->scopeChain, script, cx->fp, &value);
		js_DestroyScript(cx, script);
		if (!ok)
		    goto end_switch;

		if (!JSVAL_IS_NUMBER(value) &&
		    !JSVAL_IS_STRING(value) &&
		    !JSVAL_IS_BOOLEAN(value)) {
		    js_ReportCompileError(cx, ts, "invalid case expression");
		    ok = JS_FALSE;
		    goto end_switch;
		}
		if (++ncases == PR_BIT(16)) {
		    js_ReportCompileError(cx, ts, "too many switch cases");
		    ok = JS_FALSE;
		    goto end_switch;
		}
		JSVAL_LOCK(cx, value);
		break;

	      case TOK_DEFAULT:
		if (defaultsc) {
		    js_ReportCompileError(cx, ts,
					  "more than one switch default");
		    ok = JS_FALSE;
		    goto end_switch;
		}
		value = JSVAL_VOID;
		break;

	      case TOK_ERROR:
		ok = JS_FALSE;
		goto end_switch;

	      default:
		js_ReportCompileError(cx, ts, "invalid switch statement");
		ok = JS_FALSE;
		goto end_switch;
	    }
	    MUST_MATCH_TOKEN_THROW(TOK_COLON, "missing : after case expression",
				   ok = JS_FALSE; goto end_switch);

	    PR_ARENA_ALLOCATE(sc, &cx->tempPool, sizeof(SwitchCase));
	    if (!sc) {
		JSVAL_UNLOCK(cx, value);
		JS_ReportOutOfMemory(cx);
		ok = JS_FALSE;
		goto end_switch;
	    }
	    sc->value = value;
	    sc->offset = CG_OFFSET(cg);
	    sc->next = NULL;
	    *casetail = sc;
	    casetail = &sc->next;

	    if (tt == TOK_DEFAULT)
		defaultsc = sc;

	    while ((tt = js_PeekToken(cx, ts, cg)) != TOK_RC &&
		    tt != TOK_CASE && tt != TOK_DEFAULT) {
		if (tt == TOK_ERROR) {
		    ok = JS_FALSE;
		    goto end_switch;
		}
		ok = Statement(cx, ts, cg);
		if (!ok)
		    goto end_switch;
	    }
	}

	if (!caselist) {
	    low = high = 0;
	    tablen = 0;
	} else {
	    low  = JSVAL_INT_MAX;
	    high = JSVAL_INT_MIN;
	    for (sc = caselist; sc; sc = sc->next) {
		if (sc == defaultsc)
		    continue;
		if (!JSVAL_IS_INT(sc->value)) {
		    switchop = JSOP_LOOKUPSWITCH;
		    break;
		}
		i = JSVAL_TO_INT(sc->value);
		if ((jsuint)(i + (jsint)PR_BIT(15)) >= (jsuint)PR_BIT(16)) {
		    switchop = JSOP_LOOKUPSWITCH;
		    break;
		}
		if (i < low)
		    low = i;
		if (high < i)
		    high = i;
	    }
	    tablen = (uint32)(high - low + 1);
	    if (tablen >= PR_BIT(16) || tablen > 2 * ncases)
		switchop = JSOP_LOOKUPSWITCH;
	}

	if (switchop == JSOP_TABLESWITCH) {
	    length = (size_t)(6 + 2 * tablen);
	} else {
	    length = (size_t)(4 + 4 * ncases);
	    *CG_CODE(cg, top) = switchop;
	}

	/* Slide body code down to make room for the jump or lookup table. */
	if (js_MoveCode(cx, cg, top + 1, cg, top + 1 + length) < 0) {
	    ok = JS_FALSE;
	    goto end_switch;
	}
	pc = CG_CODE(cg, top);

	/* Don't forget to adjust the case offsets. */
	for (sc = caselist; sc; sc = sc->next)
	    sc->offset += length;

	/* If the body has source notes, bump the first one's delta. */
	if (cg->noteCount > topNoteCount) {
	    ok = js_BumpSrcNoteDelta(cx, cg, topNoteCount, (ptrdiff_t)length);
	    if (!ok)
		goto end_switch;
	}

	/* Set the SRC_SWITCH note's offset operand to tell end of switch. */
	offset = CG_OFFSET(cg) - top;
	ok = js_SetSrcNoteOffset(cx, cg, snindex, 0, (ptrdiff_t)offset);
	if (!ok)
	    goto end_switch;

	/* Fill in the default jump offset. */
	if (defaultsc)
	    offset = defaultsc->offset - top;
	else
	    offset = CG_OFFSET(cg) - top;
	ok = js_SetJumpOffset(cx, cg, pc, offset);
	if (!ok)
	    goto end_switch;
	pc += 2;

	/* Fill in jump or lookup table. */
	if (switchop == JSOP_TABLESWITCH) {
	    SET_JUMP_OFFSET(pc, low);
	    pc += 2;
	    SET_JUMP_OFFSET(pc, high);
	    pc += 2;
	    if (tablen) {
		size_t tablesize;
		SwitchCase **table;

		tablesize = (size_t)tablen * sizeof *table;
		PR_ARENA_ALLOCATE(table, &cx->tempPool, tablesize);
		if (!table) {
		    JS_ReportOutOfMemory(cx);
		    ok = JS_FALSE;
		    goto end_switch;
		}
		memset(table, 0, tablesize);
		for (sc = caselist; sc; sc = sc->next) {
		    if (sc == defaultsc)
			continue;
		    i = JSVAL_TO_INT(sc->value);
		    i -= low;
		    PR_ASSERT((uint32)i < tablen);
		    table[i] = sc;
		}
		for (i = 0; i < (jsint)tablen; i++) {
		    sc = table[i];
		    offset = sc ? sc->offset - top : 0;
		    ok = js_SetJumpOffset(cx, cg, pc, offset);
		    if (!ok)
			goto end_switch;
		    pc += 2;
		}
	    }
	} else {
	    SET_ATOM_INDEX(pc, ncases);
	    pc += 2;

	    for (sc = caselist; sc; sc = sc->next) {
		JSAtom *atom;

		if (sc == defaultsc)
		    continue;
		value = sc->value;
		atom = js_AtomizeValue(cx, value, 0);
		if (!atom) {
		    ok = JS_FALSE;
		    goto end_switch;
		}
		atomIndex = js_IndexAtom(cx, atom, &cg->atomList);
		js_DropAtom(cx, atom);

		SET_ATOM_INDEX(pc, atomIndex);
		pc += 2;

		offset = sc->offset - top;
		ok = js_SetJumpOffset(cx, cg, pc, offset);
		if (!ok)
		    goto end_switch;
		pc += 2;
	    }
	}

      end_switch:
	for (sc = caselist; sc; sc = sc->next)
	    JSVAL_UNLOCK(cx, sc->value);
	if (newlines)
	    SCAN_NEWLINES(ts);
	return js_PopStatement(cx, cg);
      }
#endif /* JS_HAS_SWITCH_STATEMENT */

      case TOK_WHILE:
	top = CG_OFFSET(cg);
	js_PushStatement(cg, &stmtInfo, STMT_WHILE_LOOP, top);
	if (!Condition(cx, ts, cg))
	    return JS_FALSE;
	snindex = js_NewSrcNote(cx, cg, SRC_WHILE);
	if (snindex < 0)
	    return JS_FALSE;
	beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	if (beq < 0)
	    return JS_FALSE;
	if (!Statement(cx, ts, cg))
	    return JS_FALSE;
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET(cx, cg, CG_CODE(cg,jmp), top - jmp);
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	return js_PopStatement(cx, cg);

#if JS_HAS_DO_WHILE_LOOP
      case TOK_DO:
	/* Emit an annotated nop so we know to decompile a 'do' keyword. */
	if (js_NewSrcNote(cx, cg, SRC_WHILE) < 0 ||
	    js_Emit1(cx, cg, JSOP_NOP) < 0) {
	    return JS_FALSE;
	}

	/* Compile the loop body and condition. */
	top = CG_OFFSET(cg);
	js_PushStatement(cg, &stmtInfo, STMT_DO_LOOP, top);
	if (!Statement(cx, ts, cg))
	    return JS_FALSE;
	MUST_MATCH_TOKEN(TOK_WHILE, "missing while after do-loop body");
	if (!Condition(cx, ts, cg))
	    return JS_FALSE;

	/* Re-use the SRC_WHILE note, this time for the JSOP_IFNE opcode. */
	if (js_NewSrcNote(cx, cg, SRC_WHILE) < 0)
	    return JS_FALSE;
	jmp = top - CG_OFFSET(cg);
	if (js_Emit3(cx, cg, JSOP_IFNE,
		     JUMP_OFFSET_HI(jmp), JUMP_OFFSET_LO(jmp)) < 0) {
	    return JS_FALSE;
	}
	return js_PopStatement(cx, cg);
#endif /* JS_HAS_DO_WHILE_LOOP */

      case TOK_FOR:
	top = CG_OFFSET(cg);
	js_PushStatement(cg, &stmtInfo, STMT_FOR_LOOP, top);
	snindex = -1;
	topNoteCount = cg->noteCount;
	cg2.base = NULL;

	MUST_MATCH_TOKEN(TOK_LP, "missing ( after for");	/* balance) */
	tt = js_PeekToken(cx, ts, cg);
	if (tt == TOK_SEMI) {
	    /* No initializer -- emit an annotated nop for the decompiler. */
	    snindex = js_NewSrcNote(cx, cg, SRC_FOR);
	    if (snindex < 0 || js_Emit1(cx, cg, JSOP_NOP) < 0)
		return JS_FALSE;
	} else {
	    if (tt == TOK_VAR) {
		(void) js_GetToken(cx, ts, cg);
		if (!Variables(cx, ts, cg))
		    return JS_FALSE;
	    } else {
		if (!Expr(cx, ts, cg))
		    return JS_FALSE;
	    }
	    if (js_PeekToken(cx, ts, cg) != TOK_IN) {
		snindex = js_NewSrcNote(cx, cg, SRC_FOR);
		if (snindex < 0 || js_Emit1(cx, cg, JSOP_POP) < 0)
		    return JS_FALSE;
	    }
	}

	isForInLoop = js_MatchToken(cx, ts, cg, TOK_IN);
	if (isForInLoop) {
	    /* Insert a JSOP_PUSH to allocate an iterator before this loop. */
	    stmtInfo.type = STMT_FOR_IN_LOOP;
	    if (js_MoveCode(cx, cg, top, cg, top + 1) < 0)
		return JS_FALSE;
	    *CG_CODE(cg, top) = JSOP_PUSH;
	    js_UpdateDepth(cx, cg, top);
	    top++;
	    SET_STATEMENT_TOP(&stmtInfo, top);

	    /* If the iterator has a SRC_VAR note, remember to emit it later. */
	    hasVarNote = (cg->noteCount > topNoteCount);
	    if (hasVarNote) {
		hasVarNote = (SN_TYPE(&cg->notes[topNoteCount]) == SRC_VAR);
		if (hasVarNote) {
		    /* XXX blow away any ASSIGNOP notes in initializers */
		    cg->noteCount = topNoteCount;
		} else {
		    /* Otherwise bump the first notes's delta. */
		    if (!js_BumpSrcNoteDelta(cx, cg, topNoteCount, 1))
			return JS_FALSE;
		}
	    }

	    /* Cancel the last bytecode, saving it for re-emission later. */
	    pc = CG_CODE(cg, cg->lastCodeOffset);
	    lastop = (JSOp) *pc;
	    switch (lastop) {
	      case JSOP_NAME:
	      case JSOP_GETPROP:
		atomIndex = GET_ATOM_INDEX(pc);
		break;
	      case JSOP_GETELEM:
		/* GETELEM and FORELEM have no immediate operand. */
		break;
	      default:
		js_ReportCompileError(cx, ts, "invalid for/in left-hand side");
		return JS_FALSE;
	    }
	    js_CancelLastOpcode(cx, cg, &ts->newlines);

	    /* Now compile the object expression over which we're iterating. */
	    if (!Expr(cx, ts, cg))
		return JS_FALSE;

	    if (lastop == JSOP_NAME) {
		if (hasVarNote && js_NewSrcNote(cx, cg, SRC_VAR) < 0)
		    return JS_FALSE;
	    } else {
		if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
				   (int16)(CG_OFFSET(cg) - top)) < 0) {
		    return JS_FALSE;
		}
	    }

	    /* Re-emit the left-hand side's bytecode as a JSOP_FOR* variant. */
	    switch (lastop) {
	      case JSOP_NAME:
		EMIT_CONST_ATOM_OP(JSOP_FORNAME, atomIndex);
		break;
	      case JSOP_GETPROP:
		EMIT_CONST_ATOM_OP(JSOP_FORPROP, atomIndex);
		break;
	      case JSOP_GETELEM:
		if (js_Emit1(cx, cg, JSOP_FORELEM) < 0)
		    return JS_FALSE;
		break;
	      default:;
	    }

	    /* Pop and test the loop condition generated by JSOP_FOR*. */
	    beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	    if (beq < 0)
		return JS_FALSE;
	} else {
	    MUST_MATCH_TOKEN(TOK_SEMI, "missing ; after for-loop initializer");
	    top = CG_OFFSET(cg);
	    SET_STATEMENT_TOP(&stmtInfo, top);
	    if (js_PeekToken(cx, ts, cg) == TOK_SEMI) {
		/* No loop condition -- flag this fact in the source note. */
		if (!js_SetSrcNoteOffset(cx, cg, snindex, 0, 0))
		    return JS_FALSE;
		beq = 0;
	    } else {
		if (!Expr(cx, ts, cg))
		    return JS_FALSE;
		if (!js_SetSrcNoteOffset(cx, cg, snindex, 0,
					 (ptrdiff_t)(CG_OFFSET(cg) - top))) {
		    return JS_FALSE;
		}
		beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
		if (beq < 0)
		    return JS_FALSE;
	    }

	    MUST_MATCH_TOKEN(TOK_SEMI, "missing ; after for-loop condition");
	    if (js_PeekToken(cx, ts, cg) != TOK_RP) {
		if (!js_InitCodeGenerator(cx, &cg2, &cx->tempPool))
		    return JS_FALSE;
		CG_PUSH(cg, &cg2);
		if (js_NewSrcNote2(cx, &cg2, SRC_SETLINE,
				   (ptrdiff_t)TRUE_LINENO(ts)) < 0) {
		    return JS_FALSE;
		}
		if (!Expr(cx, ts, &cg2))
		    return JS_FALSE;
		if (js_Emit1(cx, &cg2, JSOP_POP) < 0)
		    return JS_FALSE;
		CG_POP(cg, &cg2);
	    }
	}

	/* (balance: */
	MUST_MATCH_TOKEN(TOK_RP, "missing ) after for-loop control");
	if (!Statement(cx, ts, cg))
	    return JS_FALSE;

	if (snindex != -1 &&
	    !js_SetSrcNoteOffset(cx, cg, snindex, 1,
				 (ptrdiff_t)(CG_OFFSET(cg) - top))) {
	    return JS_FALSE;
	}

	if (cg2.base) {
	    /* Set loop and enclosing "update" offsets, for continue. */
	    stmt = &stmtInfo;
	    stmt->update = CG_OFFSET(cg);
	    while ((stmt = stmt->down) != 0 && stmt->type == STMT_LABEL)
		stmt->update = CG_OFFSET(cg);

	    /* Copy the update code from its temporary pool into cg's pool. */
	    if (!js_MoveSrcNotes(cx, &cg2, cg))
		return JS_FALSE;
	    if (js_MoveCode(cx, &cg2, 0, cg, stmtInfo.update) < 0)
		return JS_FALSE;

	    /* Restore the absolute line number for source note readers. */
	    if (js_NewSrcNote2(cx, cg, SRC_SETLINE, (ptrdiff_t)TRUE_LINENO(ts))
		< 0) {
		return JS_FALSE;
	    }
	}

	if (snindex != -1 &&
	    !js_SetSrcNoteOffset(cx, cg, snindex, 2,
				 (ptrdiff_t)(CG_OFFSET(cg) - top))) {
	    return JS_FALSE;
	}

	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET(cx, cg, CG_CODE(cg,jmp), top - jmp);
	if (beq > 0)
	    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	if (!js_PopStatement(cx, cg))
	    return JS_FALSE;
	if (isForInLoop) {
	    /*
	     * Generate the iterator pop after popping the stmtInfo stack, so
	     * breaks will go to the pop bytecode.
	     */
	    if (js_Emit1(cx, cg, JSOP_POP) < 0)
		return JS_FALSE;
	}
	return JS_TRUE;

      case TOK_BREAK:
	if (!MatchLabel(cx, ts, cg, &label))
	    return JS_FALSE;
	stmt = cg->stmtInfo;
	if (label) {
	    for (; ; stmt = stmt->down) {
		if (!stmt) {
		    js_ReportCompileError(cx, ts, "label not found");
		    return JS_FALSE;
		}
		if (stmt->type == STMT_LABEL && stmt->label == label)
		    break;
	    }
	} else {
	    for (; ; stmt = stmt->down) {
		if (!stmt) {
		    js_ReportCompileError(cx, ts, "invalid break");
		    return JS_FALSE;
		}
		if (STMT_IS_LOOP(stmt) || stmt->type == STMT_SWITCH)
		    break;
	    }
	}
	if (js_EmitBreak(cx, cg, stmt, label) < 0)
	    return JS_FALSE;
	break;

      case TOK_CONTINUE:
	if (!MatchLabel(cx, ts, cg, &label))
	    return JS_FALSE;
	stmt = cg->stmtInfo;
	if (label) {
	    for (stmt2 = NULL; ; stmt = stmt->down) {
		if (!stmt) {
		    js_ReportCompileError(cx, ts, "label not found");
		    return JS_FALSE;
		}
		if (stmt->type == STMT_LABEL) {
		    if (stmt->label == label) {
			if (!stmt2 || !STMT_IS_LOOP(stmt2)) {
			    js_ReportCompileError(cx, ts, "invalid continue");
			    return JS_FALSE;
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
		    return JS_FALSE;
		}
		if (STMT_IS_LOOP(stmt))
		    break;
	    }
	    if (js_NewSrcNote(cx, cg, SRC_CONTINUE) < 0)
		return JS_FALSE;
	}
	if (js_EmitContinue(cx, cg, stmt, label) < 0)
	    return JS_FALSE;
	break;

      case TOK_WITH:
	MUST_MATCH_TOKEN(TOK_LP, "missing ( before with-statement object");
	/* balance) */
	if (!Expr(cx, ts, cg))
	    return JS_FALSE;
	/* (balance: */
	MUST_MATCH_TOKEN(TOK_RP, "missing ) after with-statement object");

	js_PushStatement(cg, &stmtInfo, STMT_WITH, CG_OFFSET(cg));
	if (js_Emit1(cx, cg, JSOP_ENTERWITH) < 0)
	    return JS_FALSE;
	if (!Statement(cx, ts, cg))
	    return JS_FALSE;
	if (js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0)
	    return JS_FALSE;
	return js_PopStatement(cx, cg);

      case TOK_VAR:
	lineno = ts->lineno;
	if (!Variables(cx, ts, cg))
	    return JS_FALSE;
	if (ts->lineno == lineno && !WellTerminated(cx, ts, cg, TOK_ERROR))
	    return JS_FALSE;
	if (js_Emit1(cx, cg, JSOP_POP) < 0)
	    return JS_FALSE;
	break;

      case TOK_RETURN:
	if (!(ts->flags & TSF_FUNCTION)) {
	    js_ReportCompileError(cx, ts, "invalid return");
	    return JS_FALSE;
	}

	/* This is ugly, but we don't want to require a semicolon. */
	ts->flags |= TSF_REGEXP;
	tt = js_PeekTokenSameLine(cx, ts, cg);
	ts->flags &= ~TSF_REGEXP;
	if (tt == TOK_ERROR)
	    return JS_FALSE;

	if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
	    lineno = ts->lineno;
	    if (!Expr(cx, ts, cg))
		return JS_FALSE;
	    if (ts->lineno == lineno && !WellTerminated(cx, ts, cg, TOK_ERROR))
		return JS_FALSE;
	    ts->flags |= TSF_RETURN_EXPR;
	} else {
	    if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
		return JS_FALSE;
	    ts->flags |= TSF_RETURN_VOID;
	}

	if ((ts->flags & (TSF_RETURN_EXPR | TSF_RETURN_VOID)) ==
	    (TSF_RETURN_EXPR | TSF_RETURN_VOID)) {
	    js_ReportCompileError(cx, ts,
				  "function does not always return a value");
	    return JS_FALSE;
	}

	if (js_Emit1(cx, cg, JSOP_RETURN) < 0)
	    return JS_FALSE;
	break;

      case TOK_LC:
	js_PushStatement(cg, &stmtInfo, STMT_BLOCK, CG_OFFSET(cg));
	if (!Statements(cx, ts, cg))
	    return JS_FALSE;

	/* {balance: */
	MUST_MATCH_TOKEN(TOK_RC, "missing } in compound statement");
	return js_PopStatement(cx, cg);

      case TOK_EOL:
      case TOK_SEMI:
	return JS_TRUE;

      case TOK_ERROR:
	return JS_FALSE;

      default:
	lastExprType = ts->token.type;
	js_UngetToken(ts);
	lineno = ts->lineno;
	top = CG_OFFSET(cg);
	if (!Expr(cx, ts, cg))
	    return JS_FALSE;

	tt = ts->pushback.type;
	if (tt == TOK_COLON) {
	    if (cg->lastCodeOffset != top || *CG_CODE(cg, top) != JSOP_NAME) {
		js_ReportCompileError(cx, ts, "invalid label");
		return JS_FALSE;
	    }
	    label = cg->atomList.list;
	    for (stmt = cg->stmtInfo; stmt; stmt = stmt->down) {
		if (stmt->type == STMT_LABEL && stmt->label == label) {
		    js_ReportCompileError(cx, ts, "duplicate label");
		    return JS_FALSE;
		}
	    }
	    js_CancelLastOpcode(cx, cg, &ts->newlines);
	    js_GetToken(cx, ts, cg);

	    /* Emit an annotated nop so we know to decompile a label. */
	    snindex = js_NewSrcNote(cx, cg,
				    (js_PeekToken(cx, ts, cg) == TOK_LC)
				    ? SRC_LABELBRACE
				    : SRC_LABEL);
	    if (snindex < 0 ||
		!js_SetSrcNoteOffset(cx, cg, snindex, 0,
				     (ptrdiff_t)label->index) ||
		js_Emit1(cx, cg, JSOP_NOP) < 0) {
		return JS_FALSE;
	    }

	    /* Push a label struct and parse the statement. */
	    js_PushStatement(cg, &stmtInfo, STMT_LABEL, CG_OFFSET(cg));
	    stmtInfo.label = label;
	    if (!Statement(cx, ts, cg))
		return JS_FALSE;

	    /* If the statement was compound, emit a note for the end brace */
	    if (SN_TYPE(&cg->notes[snindex]) == SRC_LABELBRACE) {
		if (js_NewSrcNote(cx, cg, SRC_ENDBRACE) < 0 ||
		    js_Emit1(cx, cg, JSOP_NOP) < 0) {
		    return JS_FALSE;
		}
	    }

	    /* Finally, pop the label and return early. */
	    return js_PopStatement(cx, cg);
	}

	if (ts->lineno == lineno && !WellTerminated(cx, ts, cg, lastExprType))
	    return JS_FALSE;
	if (js_Emit1(cx, cg, JSOP_POPV) < 0)
	    return JS_FALSE;
	break;
    }

    (void) js_MatchToken(cx, ts, cg, TOK_SEMI);
    return JS_TRUE;
}

static JSBool
Variables(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSObject *obj, *pobj;
    JSFunction *fun;
    JSClass *clasp;
    JSBool ok, first;
    JSPropertyOp getter, setter;
    intN snindex;
    ptrdiff_t offset, newoffset;
    JSAtom *atom;
    jsatomid atomIndex;
    JSProperty *prop;
    JSSymbol *var;
    JSOp op;

    obj = js_FindVariableScope(cx, &fun);
    if (!obj)
	return JS_FALSE;

    clasp = obj->map->clasp;
    if (fun && clasp != &js_CallClass) {
	getter = js_GetLocalVariable;
	setter = js_SetLocalVariable;
    } else {
	getter = clasp->getProperty;
	setter = clasp->setProperty;
    }

    ok = first = JS_TRUE;
    snindex = -1;
    for (;;) {
	MUST_MATCH_TOKEN(TOK_NAME, "missing variable name");
	atom = ts->token.u.atom;
	atomIndex = js_IndexAtom(cx, atom, &cg->atomList);

	pobj = NULL;
	if (!js_LookupProperty(cx, obj, (jsval)atom, &pobj, &prop))
	    return JS_FALSE;
	if (prop && prop->object == obj) {
	    if (prop->getter == js_GetArgument) {
#ifdef CHECK_ARGUMENT_HIDING
		js_ReportCompileError(cx, ts, "variable %s hides argument",
				      ATOM_BYTES(atom));
		ok = JS_FALSE;
#endif
	    } else {
		if (fun) {
		    /* Not an argument, must be a redeclared local var. */
		    if (clasp != &js_CallClass) {
			PR_ASSERT(prop->getter == js_GetLocalVariable);
			PR_ASSERT(JSVAL_IS_INT(prop->id) &&
				  JSVAL_TO_INT(prop->id) < fun->nvars);
		    }
		} else {
		    /* Global var: (re-)set id a la js_DefineProperty. */
		    prop->id = ATOM_KEY(atom);
		}
		prop->getter = getter;
		prop->setter = setter;
		prop->flags |= JSPROP_ENUMERATE | JSPROP_PERMANENT;
		prop->flags &= ~JSPROP_READONLY;
	    }
	} else {
	    prop = js_DefineProperty(cx, obj, (jsval)atom, JSVAL_VOID,
				     getter, setter,
				     JSPROP_ENUMERATE | JSPROP_PERMANENT);
	    if (!prop)
		return JS_FALSE;
	    if (getter == js_GetLocalVariable)
		prop->id = INT_TO_JSVAL(fun->nvars++);
	    var = prop->symbols;
	    var->next = cg->vars;
	    cg->vars = var;
	}

	if (js_MatchToken(cx, ts, cg, TOK_ASSIGN)) {
	    if (ts->token.u.op != JSOP_NOP) {
		js_ReportCompileError(cx, ts,
				      "invalid variable initialization");
		ok = JS_FALSE;
	    }
	    if (!AssignExpr(cx, ts, cg))
		return JS_FALSE;
	    op = JSOP_SETNAME;
	} else {
	    op = JSOP_NAME;
	}
	if (first && js_NewSrcNote(cx, cg, SRC_VAR) < 0)
	    return JS_FALSE;
	EMIT_CONST_ATOM_OP(op, atomIndex);
	if (snindex >= 0) {
	    newoffset = CG_OFFSET(cg);
	    if (!js_SetSrcNoteOffset(cx, cg, snindex, 0, newoffset - offset))
		return JS_FALSE;
	    offset = newoffset;
	}
	if (!js_MatchToken(cx, ts, cg, TOK_COMMA))
	    break;
	offset = CG_OFFSET(cg);
	snindex = js_NewSrcNote2(cx, cg, SRC_COMMA, 0);
	if (snindex < 0 || js_Emit1(cx, cg, JSOP_POP) < 0)
	    return JS_FALSE;
	first = JS_FALSE;
    }
    return ok;
}

static JSBool
Expr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    intN snindex;
    ptrdiff_t offset, newoffset;

    snindex = -1;
    for (;;) {
	if (!AssignExpr(cx, ts, cg))
	    return JS_FALSE;
	if (snindex >= 0) {
	    newoffset = CG_OFFSET(cg);
	    if (!js_SetSrcNoteOffset(cx, cg, snindex, 0, newoffset - offset))
		return JS_FALSE;
	    offset = newoffset;
	}
	if (!js_MatchToken(cx, ts, cg, TOK_COMMA))
	    break;
	offset = CG_OFFSET(cg);
	snindex = js_NewSrcNote2(cx, cg, SRC_COMMA, 0);
	if (snindex < 0 || js_Emit1(cx, cg, JSOP_POP) < 0)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

/*
 * XXX Need to build abstract syntax tree for complete error checking:
 * XXX (true ? 3 : x) = 4 does not result in an error, it evaluates to 3.
 * XXX similarly for (true && x) = 5, etc.
 */
static JSBool
AssignExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    ptrdiff_t begin;
    jsbytecode *pc;
    JSOp lastop, op;
    jsatomid atomIndex;

    begin = CG_OFFSET(cg);
    if (!CondExpr(cx, ts, cg))
	return JS_FALSE;
    if (js_MatchToken(cx, ts, cg, TOK_ASSIGN)) {
	pc = CG_CODE(cg, cg->lastCodeOffset);
	lastop = (JSOp) *pc;
	switch (lastop) {
	  case JSOP_NAME:
	  case JSOP_GETPROP:
	    atomIndex = GET_ATOM_INDEX(pc);
	    break;
	  case JSOP_GETELEM:
	    /* GETELEM and SETELEM have no immediate operand. */
	    break;
	  default:
	    js_ReportCompileError(cx, ts, "invalid assignment left-hand side");
	    return JS_FALSE;
	}
	js_CancelLastOpcode(cx, cg, &ts->newlines);

	op = ts->token.u.op;
	if (op != JSOP_NOP) {
	    switch (lastop) {
	      case JSOP_NAME:
		EMIT_CONST_ATOM_OP(lastop, atomIndex);
		break;
	      case JSOP_GETPROP:
		if (js_Emit1(cx, cg, JSOP_DUP) < 0)
		    return JS_FALSE;
		EMIT_CONST_ATOM_OP(lastop, atomIndex);
		break;
	      case JSOP_GETELEM:
		if (js_Emit1(cx, cg, JSOP_DUP2) < 0)
		    return JS_FALSE;
		if (js_Emit1(cx, cg, JSOP_GETELEM) < 0)
		    return JS_FALSE;
		break;
	      default:;
	    }
	}
	if (!AssignExpr(cx, ts, cg))
	    return JS_FALSE;
	if (op != JSOP_NOP) {
	    if (js_NewSrcNote(cx, cg, SRC_ASSIGNOP) < 0 ||
		js_Emit1(cx, cg, op) < 0) {
		return JS_FALSE;
	    }
	}

	if (lastop != JSOP_NAME) {
	    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
			       (ptrdiff_t)(CG_OFFSET(cg) - begin)) < 0) {
		return JS_FALSE;
	    }
	}

	switch (lastop) {
	  case JSOP_NAME:
	    EMIT_CONST_ATOM_OP(JSOP_SETNAME, atomIndex);
	    break;
	  case JSOP_GETPROP:
	    EMIT_CONST_ATOM_OP(JSOP_SETPROP, atomIndex);
	    break;
	  case JSOP_GETELEM:
	    if (js_Emit1(cx, cg, JSOP_SETELEM) < 0)
		return JS_FALSE;
	    break;
	  default:;
	}
    }
    return JS_TRUE;
}

static JSBool
CondExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    ptrdiff_t beq, jmp;

    if (!OrExpr(cx, ts, cg))
	return JS_FALSE;
    if (js_MatchToken(cx, ts, cg, TOK_HOOK)) {
	if (js_NewSrcNote(cx, cg, SRC_COND) < 0)
	    return JS_FALSE;
	beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	if (beq < 0 || !AssignExpr(cx, ts, cg))
	    return JS_FALSE;
	MUST_MATCH_TOKEN(TOK_COLON, "missing : in conditional expression");
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	if (!AssignExpr(cx, ts, cg))
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
    }
    return JS_TRUE;
}

static JSBool
OrExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
#if JS_BUG_SHORT_CIRCUIT
    ptrdiff_t beq, tmp, jmp;
#else
    ptrdiff_t jmp;
#endif

    if (!AndExpr(cx, ts, cg))
	return JS_FALSE;
    if (js_MatchToken(cx, ts, cg, TOK_OR)) {
#if JS_BUG_SHORT_CIRCUIT
	beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	tmp = js_Emit1(cx, cg, JSOP_TRUE);
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (beq < 0 || tmp < 0 || jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
#else
	jmp = js_Emit3(cx, cg, JSOP_OR, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
#endif
	if (!OrExpr(cx, ts, cg))
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
    }
    return JS_TRUE;
}

static JSBool
AndExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
#if JS_BUG_SHORT_CIRCUIT
    ptrdiff_t bne, tmp, jmp;
#else
    ptrdiff_t jmp;
#endif

    if (!BitOrExpr(cx, ts, cg))
	return JS_FALSE;
    if (js_MatchToken(cx, ts, cg, TOK_AND)) {
#if JS_BUG_SHORT_CIRCUIT
	bne = js_Emit3(cx, cg, JSOP_IFNE, 0, 0);
	tmp = js_Emit1(cx, cg, JSOP_FALSE);
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (bne < 0 || tmp < 0 || jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, bne);
#else
	jmp = js_Emit3(cx, cg, JSOP_AND, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
#endif
	if (!AndExpr(cx, ts, cg))
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
    }
    return JS_TRUE;
}

static JSBool
BitOrExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    if (!BitXorExpr(cx, ts, cg))
	return JS_FALSE;
    while (js_MatchToken(cx, ts, cg, TOK_BITOR)) {
	if (!BitXorExpr(cx, ts, cg) ||
	    js_Emit1(cx, cg, JSOP_BITOR) < 0) {
	    return JS_FALSE;
	}
    }
    return JS_TRUE;
}

static JSBool
BitXorExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    if (!BitAndExpr(cx, ts, cg))
	return JS_FALSE;
    while (js_MatchToken(cx, ts, cg, TOK_BITXOR)) {
	if (!BitAndExpr(cx, ts, cg) ||
	    js_Emit1(cx, cg, JSOP_BITXOR) < 0) {
	    return JS_FALSE;
	}
    }
    return JS_TRUE;
}

static JSBool
BitAndExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    if (!EqExpr(cx, ts, cg))
	return JS_FALSE;
    while (js_MatchToken(cx, ts, cg, TOK_BITAND)) {
	if (!EqExpr(cx, ts, cg) || js_Emit1(cx, cg, JSOP_BITAND) < 0)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
EqExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSOp op;

    if (!RelExpr(cx, ts, cg))
	return JS_FALSE;
    while (js_MatchToken(cx, ts, cg, TOK_EQOP)) {
	op = ts->token.u.op;
	if (!RelExpr(cx, ts, cg) || js_Emit1(cx, cg, op) < 0)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
RelExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSOp op;

    if (!ShiftExpr(cx, ts, cg))
	return JS_FALSE;
    while (js_MatchToken(cx, ts, cg, TOK_RELOP)) {
	op = ts->token.u.op;
	if (!ShiftExpr(cx, ts, cg) || js_Emit1(cx, cg, op) < 0)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
ShiftExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSOp op;

    if (!AddExpr(cx, ts, cg))
	return JS_FALSE;
    while (js_MatchToken(cx, ts, cg, TOK_SHOP)) {
	op = ts->token.u.op;
	if (!AddExpr(cx, ts, cg) || js_Emit1(cx, cg, op) < 0)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
AddExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSTokenType tt;

    if (!MulExpr(cx, ts, cg))
	return JS_FALSE;
    ts->flags |= TSF_LOOKAHEAD;
    while ((tt = js_GetToken(cx, ts, cg)) == TOK_PLUS || tt == TOK_MINUS) {
	ts->flags &= ~TSF_LOOKAHEAD;
	if (!js_FlushNewlines(cx, ts, cg) ||
	    !MulExpr(cx, ts, cg) ||
	    js_Emit1(cx, cg, (tt == TOK_PLUS) ? JSOP_ADD : JSOP_SUB) < 0) {
	    return JS_FALSE;
	}
	ts->flags |= TSF_LOOKAHEAD;
    }
    ts->flags &= ~TSF_LOOKAHEAD;
    js_UngetToken(ts);
    return JS_TRUE;
}

static JSBool
MulExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSOp op;

    if (!UnaryExpr(cx, ts, cg))
	return JS_FALSE;
    while (js_MatchToken(cx, ts, cg, TOK_STAR) ||
	   js_MatchToken(cx, ts, cg, TOK_DIVOP)) {
	op = ts->token.u.op;
	if (!UnaryExpr(cx, ts, cg) || js_Emit1(cx, cg, op) < 0)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
EmitIncOp(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg,
	  JSTokenType tt, JSBool preorder)
{
    jsbytecode *pc;
    JSOp op;

    pc = CG_CODE(cg, cg->lastCodeOffset);
    op = (JSOp) *pc;
    switch (op) {
      case JSOP_NAME:
	if (tt == TOK_INC)
	    op = preorder ? JSOP_INCNAME : JSOP_NAMEINC;
	else
	    op = preorder ? JSOP_DECNAME : JSOP_NAMEDEC;
	break;
      case JSOP_GETPROP:
	if (tt == TOK_INC)
	    op = preorder ? JSOP_INCPROP : JSOP_PROPINC;
	else
	    op = preorder ? JSOP_DECPROP : JSOP_PROPDEC;
	break;
      case JSOP_GETELEM:
	if (tt == TOK_INC)
	    op = preorder ? JSOP_INCELEM : JSOP_ELEMINC;
	else
	    op = preorder ? JSOP_DECELEM : JSOP_ELEMDEC;
	break;
      default:
	js_ReportCompileError(cx, ts, "invalid %screment expression",
			      (tt == TOK_INC) ? "in" : "de");
	return JS_FALSE;
    }
    *pc = op;
    return JS_TRUE;
}

static JSBool
UnaryExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSTokenType tt;
    JSOp op, lastop;
    ptrdiff_t begin;
    jsatomid atomIndex;
    uintN argc, lineno;
    jsbytecode *pc;

    ts->flags |= TSF_REGEXP;
    tt = js_GetToken(cx, ts, cg);
    ts->flags &= ~TSF_REGEXP;

    switch (tt) {
      case TOK_UNARYOP:
      case TOK_PLUS:
      case TOK_MINUS:
	op = ts->token.u.op;
	if (!UnaryExpr(cx, ts, cg) || js_Emit1(cx, cg, op) < 0)
	    return JS_FALSE;
	break;
      case TOK_INC:
      case TOK_DEC:
	if (!MemberExpr(cx, ts, cg))
	    return JS_FALSE;
	if (!EmitIncOp(cx, ts, cg, tt, JS_TRUE))
	    return JS_FALSE;
	break;
      case TOK_NEW:
	/* Allow 'new this.ctor(...)' constructor expressions. */
	begin = CG_OFFSET(cg);
	tt = js_GetToken(cx, ts, cg);
	switch (tt) {
	  case TOK_NAME:
	    atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
	    EMIT_CONST_ATOM_OP(JSOP_NAME, atomIndex);
	    break;
	  case TOK_PRIMARY:
	    if (ts->token.u.op == JSOP_THIS) {
		if (js_Emit1(cx, cg, JSOP_THIS) < 0)
		    return JS_FALSE;
		break;
	    }
	    /* FALL THROUGH */
	  default:
	    js_ReportCompileError(cx, ts, "missing name after new operator");
	    return JS_FALSE;
	}
	while (js_MatchToken(cx, ts, cg, TOK_DOT)) {
	    MUST_MATCH_TOKEN(TOK_NAME,
			     "missing name in constructor expression");
	    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
			       (ptrdiff_t)(CG_OFFSET(cg) - begin)) < 0) {
		return JS_FALSE;
	    }
	    atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
	    EMIT_CONST_ATOM_OP(JSOP_GETPROP, atomIndex);
	}
	if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
	    return JS_FALSE;
	argc = 0;
	if (js_MatchToken(cx, ts, cg, TOK_LP) &&
	    !js_MatchToken(cx, ts, cg, TOK_RP)) {
	    do {
		if (!AssignExpr(cx, ts, cg))
		    return JS_FALSE;
		argc++;
	    } while (js_MatchToken(cx, ts, cg, TOK_COMMA));

	    /* (balance: */
	    MUST_MATCH_TOKEN(TOK_RP,
			     "missing ) after constructor argument list");
	}
	if (argc >= ARGC_LIMIT) {
	    JS_ReportError(cx, "too many constructor arguments");
	    return JS_FALSE;
	}
	if (js_Emit3(cx, cg, JSOP_NEW, ARGC_HI(argc), ARGC_LO(argc)) < 0)
	    return JS_FALSE;
	break;

      case TOK_DELETE:
	/* Compile the operand expression. */
	begin = CG_OFFSET(cg);
	if (!MemberExpr(cx, ts, cg))
	    return JS_FALSE;

	/* Cancel the last bytecode, saving it for re-emission later. */
	pc = CG_CODE(cg, cg->lastCodeOffset);
	lastop = (JSOp) *pc;
	switch (lastop) {
	  case JSOP_NAME:
	  case JSOP_GETPROP:
	    atomIndex = GET_ATOM_INDEX(pc);
	    break;
	  case JSOP_GETELEM:
	    /* GETELEM and DELELEM have no immediate operand. */
	    break;
	  default:
	    js_ReportCompileError(cx, ts, "invalid %s operand", js_delete_str);
	    return JS_FALSE;
	}
	js_CancelLastOpcode(cx, cg, &ts->newlines);

	if (lastop != JSOP_NAME) {
	    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
			       (int16)(CG_OFFSET(cg) - begin)) < 0) {
		return JS_FALSE;
	    }
	}

	/* Re-emit the operand's last bytecode as a JSOP_DEL* variant. */
	switch (lastop) {
	  case JSOP_NAME:
	    EMIT_CONST_ATOM_OP(JSOP_DELNAME, atomIndex);
	    break;
	  case JSOP_GETPROP:
	    EMIT_CONST_ATOM_OP(JSOP_DELPROP, atomIndex);
	    break;
	  case JSOP_GETELEM:
	    if (js_Emit1(cx, cg, JSOP_DELELEM) < 0)
		return JS_FALSE;
	    break;
	  default:;
	}
	break;

      case TOK_ERROR:
	return JS_FALSE;

      default:
	js_UngetToken(ts);
	lineno = ts->lineno;
	if (!MemberExpr(cx, ts, cg))
	    return JS_FALSE;

	/* Don't look across a newline boundary for a postfix incop. */
	if (ts->lineno == lineno &&
	    (js_MatchToken(cx, ts, cg, TOK_INC) ||
	     js_MatchToken(cx, ts, cg, TOK_DEC))) {
	    if (!EmitIncOp(cx, ts, cg, ts->token.type, JS_FALSE))
		return JS_FALSE;
	}
    }
    return JS_TRUE;
}

static JSBool
MemberExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    ptrdiff_t begin;
    JSToken token;
    JSTokenType tt;
    jsatomid atomIndex;
    uintN argc;
    JSBool matched;
    JSObject *pobj;
    JSProperty *prop;
    jsval val;
    JSFunction *fun;
    char *option;
    JSAtom *atom;

    begin = CG_OFFSET(cg);
    if (!PrimaryExpr(cx, ts, cg))
	return JS_FALSE;

    token = ts->token;
    ts->flags |= TSF_LOOKAHEAD;
    while ((tt = js_GetToken(cx, ts, cg)) > TOK_EOF) {
	ts->flags &= ~TSF_LOOKAHEAD;

	if (tt == TOK_DOT) {
	    /*
	     * Compile get-property opcode.
	     */
	    if (!js_FlushNewlines(cx, ts, cg))
		return JS_FALSE;
	    MUST_MATCH_TOKEN(TOK_NAME, "missing name after . operator");

	    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
			       (ptrdiff_t)(CG_OFFSET(cg) - begin)) < 0) {
		return JS_FALSE;
	    }
	    atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
	    EMIT_CONST_ATOM_OP(JSOP_GETPROP, atomIndex);
	} else if (tt == TOK_LB) {
	    /*
	     * Compile index expression and get-slot opcode.
	     */
	    if (!js_FlushNewlines(cx, ts, cg))
		return JS_FALSE;
	    if (!Expr(cx, ts, cg))
		return JS_FALSE;
	    /* [balance: */
	    MUST_MATCH_TOKEN(TOK_RB, "missing ] in index expression");

	    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
			       (ptrdiff_t)(CG_OFFSET(cg) - begin)) < 0) {
		return JS_FALSE;
	    }
	    if (js_Emit1(cx, cg, JSOP_GETELEM) < 0)
		return JS_FALSE;
	} else if (tt == TOK_LP) {
	    /*
	     * Compile 'this' and the actual args, followed by a call opcode.
	     */
	    if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
		return JS_FALSE;
	    if (!js_FlushNewlines(cx, ts, cg))
		return JS_FALSE;
	    argc = 0;
	    ts->flags |= TSF_REGEXP;
	    matched = js_MatchToken(cx, ts, cg, TOK_RP);
	    ts->flags &= ~TSF_REGEXP;
	    if (!matched) {
		do {
		    if (!AssignExpr(cx, ts, cg))
			return JS_FALSE;
		    argc++;
		} while (js_MatchToken(cx, ts, cg, TOK_COMMA));

		/* (balance: */
		MUST_MATCH_TOKEN(TOK_RP, "missing ) after argument list");
	    }

	    if (argc >= ARGC_LIMIT) {
		JS_ReportError(cx, "too many function arguments");
		return JS_FALSE;
	    }
	    if (js_Emit3(cx, cg, JSOP_CALL, ARGC_HI(argc), ARGC_LO(argc)) < 0)
		return JS_FALSE;
	} else {
	    js_UngetToken(ts);

	    /*
	     * Look for a name, number, or string immediately after a name.
	     * Such a token juxtaposition is likely to be a "command-style"
	     * function call.
	     */
	    if ((ts->flags & TSF_COMMAND) &&
		token.type == TOK_NAME &&
		*CG_CODE(cg, cg->lastCodeOffset) == JSOP_NAME &&
		(tt == TOK_EOL || tt == TOK_MINUS || IS_PRIMARY_TOKEN(tt))) {
		pobj = NULL;
		if (!js_LookupProperty(cx, cx->fp->scopeChain,
				       (jsval)token.u.atom, &pobj, &prop)) {
		    return JS_FALSE;
		}
		if (prop &&
		    (val = prop->object->slots[prop->slot]) &&
		    JSVAL_IS_FUNCTION(val) &&
		    (fun = js_ValueToFunction(cx, val)) &&
		    (tt != TOK_EOL || fun->nargs == 0)) {
		    if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
			return JS_FALSE;
		    if (!js_FlushNewlines(cx, ts, cg))
			return JS_FALSE;
		    argc = 0;
		    while (tt != TOK_EOL && tt != TOK_SEMI &&
			   tt != TOK_RP && tt != TOK_RC) {
			if (tt == TOK_MINUS && JS7_ISLET(ts->token.ptr[1])) {
			    (void) js_GetToken(cx, ts, cg);
			    MUST_MATCH_TOKEN(TOK_NAME,
				"missing option after - in command");
			    option = PR_smprintf("-%s",
						 ATOM_BYTES(ts->token.u.atom));
			    if (!option) {
				JS_ReportOutOfMemory(cx);
				return JS_FALSE;
			    }
			    atom = js_Atomize(cx, option, strlen(option), 0);
			    free(option);
			    if (!atom)
				return JS_FALSE;
			    atomIndex = js_IndexAtom(cx, atom, &cg->atomList);
			    EMIT_CONST_ATOM_OP(JSOP_STRING, atomIndex);
			    js_DropAtom(cx, atom);
			} else {
			    if (!AssignExpr(cx, ts, cg))
				return JS_FALSE;
			}
			argc++;
			js_MatchToken(cx, ts, cg, TOK_COMMA);
			tt = js_PeekToken(cx, ts, cg);
		    }
		    if (argc >= ARGC_LIMIT) {
			JS_ReportError(cx, "too many command arguments");
			return JS_FALSE;
		    }
		    if (js_Emit3(cx, cg, JSOP_CALL,
				 ARGC_HI(argc), ARGC_LO(argc)) < 0) {
			return JS_FALSE;
		    }
		}
	    }
	    break;
	}

	token = ts->token;
	ts->flags |= TSF_LOOKAHEAD;
    }
    ts->flags &= ~TSF_LOOKAHEAD;
    return (tt != TOK_ERROR);
}

static JSBool
EmitNumberOp(JSContext *cx, jsdouble dval, JSCodeGenerator *cg)
{
    jsint ival;
    JSAtom *atom;
    jsatomid atomIndex;

    ival = (jsint)dval;
    if (JSDOUBLE_IS_INT(dval, ival) && INT_FITS_IN_JSVAL(ival)) {
	if (ival == 0)
	    return js_Emit1(cx, cg, JSOP_ZERO) >= 0;
	if (ival == 1)
	    return js_Emit1(cx, cg, JSOP_ONE) >= 0;
	if ((jsuint)ival < (jsuint)ATOM_INDEX_LIMIT) {
	    atomIndex = (jsatomid)ival;
	    EMIT_CONST_ATOM_OP(JSOP_UINT16, atomIndex);
	    return JS_TRUE;
	}
	atom = js_AtomizeInt(cx, ival, ATOM_NOHOLD);
    } else {
	atom = js_AtomizeDouble(cx, dval, ATOM_NOHOLD);
    }
    if (!atom)
	return JS_FALSE;
    atomIndex = js_IndexAtom(cx, atom, &cg->atomList);
    EMIT_CONST_ATOM_OP(JSOP_NUMBER, atomIndex);
    return JS_TRUE;
}

static JSBool
PrimaryExpr(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg)
{
    JSTokenType tt;
    jsatomid atomIndex;

#if JS_HAS_SHARP_VARS
    JSBool defsharp = JS_FALSE;
    jsatomid sharpnum = 0;
  again:
#endif

    ts->flags |= TSF_REGEXP;
    tt = js_GetToken(cx, ts, cg);
    ts->flags &= ~TSF_REGEXP;

    switch (tt) {
#if JS_HAS_LEXICAL_CLOSURE
      case TOK_FUNCTION:
	return FunctionExpr(cx, ts, cg);
#endif

#if JS_HAS_OBJECT_LITERAL
      case TOK_LB:
      {
	JSBool matched;
	jsint index;

	atomIndex = js_IndexAtom(cx, cx->runtime->atomState.ArrayAtom,
				 &cg->atomList);
	EMIT_CONST_ATOM_OP(JSOP_NAME, atomIndex);
	if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
	    return JS_FALSE;
	if (js_Emit1(cx, cg, JSOP_NEWINIT) < 0)
	    return JS_FALSE;

#if JS_HAS_SHARP_VARS
	if (defsharp)
	    EMIT_CONST_ATOM_OP(JSOP_DEFSHARP, sharpnum);
#endif

	ts->flags |= TSF_REGEXP;
	matched = js_MatchToken(cx, ts, cg, TOK_RB);
	ts->flags &= ~TSF_REGEXP;
	if (!matched) {
	    for (index = 0; index < ATOM_INDEX_LIMIT; index++) {
		ts->flags |= TSF_REGEXP;
		tt = js_PeekToken(cx, ts, cg);
		ts->flags &= ~TSF_REGEXP;
		if (tt == TOK_RB) {
		    if (js_NewSrcNote(cx, cg, SRC_COMMA) < 0)
			return JS_FALSE;
		    break;
		}

		if (index == 0) {
		    if (js_Emit1(cx, cg, JSOP_ZERO) < 0)
			return JS_FALSE;
		} else if (index == 1) {
		    if (js_Emit1(cx, cg, JSOP_ONE) < 0)
			return JS_FALSE;
		} else {
		    atomIndex = (jsatomid) index;
		    EMIT_CONST_ATOM_OP(JSOP_UINT16, atomIndex);
		}

		if (tt == TOK_COMMA) {
		    if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
			return JS_FALSE;
		} else {
		    if (!AssignExpr(cx, ts, cg))
			return JS_FALSE;
		}
		if (js_Emit1(cx, cg, JSOP_INITELEM) < 0)
		    return JS_FALSE;

		if (!js_MatchToken(cx, ts, cg, TOK_COMMA))
		    break;
	    }

	    /* [balance: */
	    MUST_MATCH_TOKEN(TOK_RB, "missing ] after element list");
	}

	/* Emit an annotated op for sharpArray cleanup and decompilation. */
	if (js_Emit1(cx, cg, JSOP_ENDINIT) < 0)
	    return JS_FALSE;
	break;
      }

      case TOK_LC:
      {
	atomIndex = js_IndexAtom(cx, cx->runtime->atomState.ObjectAtom,
				 &cg->atomList);
	EMIT_CONST_ATOM_OP(JSOP_NAME, atomIndex);
	if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
	    return JS_FALSE;
	if (js_Emit1(cx, cg, JSOP_NEWINIT) < 0)
	    return JS_FALSE;

#if JS_HAS_SHARP_VARS
	if (defsharp)
	    EMIT_CONST_ATOM_OP(JSOP_DEFSHARP, sharpnum);
#endif

	if (!js_MatchToken(cx, ts, cg, TOK_RC)) {
	    do {
		tt = js_GetToken(cx, ts, cg);
		switch (tt) {
		  case TOK_NUMBER:
		    if (!EmitNumberOp(cx, ts->token.u.dval, cg))
			return JS_FALSE;
		    break;
		  case TOK_NAME:
		  case TOK_STRING:
		    atomIndex = js_IndexAtom(cx, ts->token.u.atom,
					     &cg->atomList);
		    break;
		  default:
		    js_ReportCompileError(cx, ts, "invalid property id");
		    return JS_FALSE;
		}

		MUST_MATCH_TOKEN(TOK_COLON, "missing : after property id");
		if (!AssignExpr(cx, ts, cg))
		    return JS_FALSE;

		if (tt == TOK_NUMBER) {
		    if (js_NewSrcNote(cx, cg, SRC_LABEL) < 0)
			return JS_FALSE;
		    if (js_Emit1(cx, cg, JSOP_INITELEM) < 0)
			return JS_FALSE;
		} else {
		    EMIT_CONST_ATOM_OP(JSOP_INITPROP, atomIndex);
		}
	    } while (js_MatchToken(cx, ts, cg, TOK_COMMA));

	    /* {balance: */
	    MUST_MATCH_TOKEN(TOK_RC, "missing } after property list");
	}

	/* Emit an op for sharpArray cleanup and decompilation. */
	if (js_Emit1(cx, cg, JSOP_ENDINIT) < 0)
	    return JS_FALSE;
	break;
      }

#if JS_HAS_SHARP_VARS
      case TOK_DEFSHARP:
	defsharp = JS_TRUE;
	sharpnum = (jsatomid) ts->token.u.dval;
	ts->flags |= TSF_REGEXP;
	tt = js_PeekToken(cx, ts, cg);
	ts->flags &= ~TSF_REGEXP;
	if (tt != TOK_LB && tt != TOK_LC) {
	    js_ReportCompileError(cx, ts, "invalid sharp variable definition");
	    return JS_FALSE;
	}
	goto again;

      case TOK_USESHARP:
	/* Check for forward/dangling references at runtime, to allow eval. */
	sharpnum = (jsatomid) ts->token.u.dval;
	EMIT_CONST_ATOM_OP(JSOP_USESHARP, sharpnum);
	break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_OBJECT_LITERAL */

      case TOK_LP:
	if (!Expr(cx, ts, cg))
	    return JS_FALSE;
	/* (balance: */
	MUST_MATCH_TOKEN(TOK_RP, "missing ) in parenthetical");

	/* Emit an annotated NOP so we can decompile user parens. */
	if (js_NewSrcNote(cx, cg, SRC_PAREN) < 0 ||
	    js_Emit1(cx, cg, JSOP_NOP) < 0) {
	    return JS_FALSE;
	}
	break;

      case TOK_NAME:
	atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
	EMIT_CONST_ATOM_OP(JSOP_NAME, atomIndex);
	break;

      case TOK_NUMBER:
	return EmitNumberOp(cx, ts->token.u.dval, cg);

      case TOK_STRING:
	atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
	EMIT_CONST_ATOM_OP(JSOP_STRING, atomIndex);
	break;

      case TOK_OBJECT:
	atomIndex = js_IndexAtom(cx, ts->token.u.atom, &cg->atomList);
	EMIT_CONST_ATOM_OP(JSOP_OBJECT, atomIndex);
	break;

      case TOK_PRIMARY:
	if (js_Emit1(cx, cg, ts->token.u.op) < 0)
	    return JS_FALSE;
	break;

#if !JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
      case TOK_IMPORT:
#endif
      case TOK_RESERVED:
	js_ReportCompileError(cx, ts, "identifier is a reserved word");
	return JS_FALSE;

      case TOK_ERROR:
	/* The scanner or one of its subroutines reported the error. */
	return JS_FALSE;

      default:
	js_ReportCompileError(cx, ts, "syntax error");
	return JS_FALSE;
    }
    return JS_TRUE;
}
