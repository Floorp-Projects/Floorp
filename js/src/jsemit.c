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
 * JS bytecode generation.
 */
#include "jsstddef.h"
#include <memory.h>


#include <string.h>
#include "prtypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prlog.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsscript.h"

#define CGINCR  256     /* code generation bytecode allocation increment */
#define SNINCR  64      /* source note allocation increment */

JS_FRIEND_API(JSBool)
js_InitCodeGenerator(JSContext *cx, JSCodeGenerator *cg,
		     const char *filename, uintN lineno,
		     JSPrincipals *principals)
{
    cg->codeMark = PR_ARENA_MARK(&cx->codePool);
    cg->tempMark = PR_ARENA_MARK(&cx->tempPool);
    PR_ARENA_ALLOCATE(cg->base, &cx->codePool, CGINCR);
    if (!cg->base) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    cg->limit = CG_CODE(cg, CGINCR);
    cg->filename = filename;
    cg->firstLine = lineno;
    cg->principals = principals;
    cg->tryBase = cg->tryLimit = NULL;
    CG_RESET(cg);
    return JS_TRUE;
}

JS_FRIEND_API(void)
js_ResetCodeGenerator(JSContext *cx, JSCodeGenerator *cg)
{
    PR_ARENA_RELEASE(&cx->codePool, cg->codeMark);
    PR_ARENA_RELEASE(&cx->tempPool, cg->tempMark);
    CG_RESET(cg);
}

static ptrdiff_t
EmitCheck(JSContext *cx, JSCodeGenerator *cg, JSOp op, ptrdiff_t delta)
{
    ptrdiff_t offset, length;
    size_t cgsize;

    PR_ASSERT(delta < CGINCR);
    offset = CG_OFFSET(cg);
    if (op != JSOP_NOP)
	cg->lastCodeOffset = offset;
    if ((pruword)cg->next + delta >= (pruword)cg->limit) {
        length = PTRDIFF(cg->limit, cg->base, jsbytecode);
	cgsize = length * sizeof(jsbytecode);
        PR_ARENA_GROW(cg->base, &cx->codePool, cgsize, CGINCR);
	if (!cg->base) {
	    JS_ReportOutOfMemory(cx);
	    return -1;
	}
        cg->limit = CG_CODE(cg, length + CGINCR);
        cg->next = CG_CODE(cg, offset);
    }
    return offset;
}

static void
UpdateDepth(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t target)
{
    jsbytecode *pc;
    JSCodeSpec *cs;
    intN nuses;

    pc = CG_CODE(cg, target);
    cs = &js_CodeSpec[pc[0]];
    nuses = cs->nuses;
    if (nuses < 0)
        nuses = 2 + GET_ARGC(pc);	/* stack: fun, this, [argc arguments] */
    cg->stackDepth -= nuses;
    if (cg->stackDepth < 0) {
        JS_ReportError(cx,
		       "internal error compiling %s: stack underflow at pc %d",
		       cg->filename ? cg->filename : "stdin", target);
    }
    cg->stackDepth += cs->ndefs;
    if ((uintN)cg->stackDepth > cg->maxStackDepth)
        cg->maxStackDepth = cg->stackDepth;
}

ptrdiff_t
js_Emit1(JSContext *cx, JSCodeGenerator *cg, JSOp op)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 1);

    if (offset >= 0) {
	*cg->next++ = (jsbytecode)op;
	UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_Emit2(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 2);

    if (offset >= 0) {
	cg->next[0] = (jsbytecode)op;
	cg->next[1] = op1;
	cg->next += 2;
	UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_Emit3(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1,
	 jsbytecode op2)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 3);

    if (offset >= 0) {
	cg->next[0] = (jsbytecode)op;
	cg->next[1] = op1;
	cg->next[2] = op2;
	cg->next += 3;
	UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_EmitN(JSContext *cx, JSCodeGenerator *cg, JSOp op, size_t extra)
{
    ptrdiff_t length = 1 + (ptrdiff_t)extra;
    ptrdiff_t offset = EmitCheck(cx, cg, op, length);

    if (offset >= 0) {
	*cg->next = (jsbytecode)op;
	cg->next += length;
	UpdateDepth(cx, cg, offset);
    }
    return offset;
}

static const char *statementName[] = {
    "block",             /* BLOCK */
    "label statement",   /* LABEL */
    "if statement",      /* IF */
    "else statement",    /* ELSE */
    "switch statement",  /* SWITCH */
    "with statement",    /* WITH */
    "try statement",	 /* TRY */
    "catch block",	 /* CATCH */
    "finally statement", /* FINALLY */
    "do loop",           /* DO_LOOP */
    "for loop",          /* FOR_LOOP */
    "for/in loop",       /* FOR_IN_LOOP */
    "while loop",        /* WHILE_LOOP */
};

static const char *
StatementName(JSCodeGenerator *cg)
{
    if (!cg->treeContext.topStmt)
	return "script";
    return statementName[cg->treeContext.topStmt->type];
}

static void
ReportStatementTooLarge(JSContext *cx, JSCodeGenerator *cg)
{
    JS_ReportError(cx, "%s too large", StatementName(cg));
}

JSBool
js_SetJumpOffset(JSContext *cx, JSCodeGenerator *cg, jsbytecode *pc,
		 ptrdiff_t off)
{
    if (off < JUMP_OFFSET_MIN || JUMP_OFFSET_MAX < off) {
	ReportStatementTooLarge(cx, cg);
	return JS_FALSE;
    }
    SET_JUMP_OFFSET(pc, off);
    return JS_TRUE;
}

void
js_PushStatement(JSTreeContext *tc, JSStmtInfo *stmt, JSStmtType type,
		 ptrdiff_t top)
{
    stmt->type = type;
    SET_STATEMENT_TOP(stmt, top);
    stmt->label = NULL;
    stmt->down = tc->topStmt;
    tc->topStmt = stmt;
}

static ptrdiff_t
EmitGoto(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *toStmt,
	 ptrdiff_t *last, JSAtomListElement *label, JSSrcNoteType noteType)
{
    JSStmtInfo *stmt;
    intN index;
    ptrdiff_t offset, delta;

    for (stmt = cg->treeContext.topStmt; stmt != toStmt; stmt = stmt->down) {
	switch (stmt->type) {
	  case STMT_WITH:
	    if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
		return -1;
	    cg->stackDepth++;
	    if (js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0)
		return -1;
	    break;
	  case STMT_FOR_IN_LOOP:
	    if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
		return -1;
	    cg->stackDepth += 2;
	    if (js_Emit1(cx, cg, JSOP_POP2) < 0)
		return -1;
	    break;
	  default:;
	}
    }

    if (label) {
	index = js_NewSrcNote(cx, cg, noteType);
	if (index < 0)
	    return -1;
	if (!js_SetSrcNoteOffset(cx, cg, (uintN)index, 0,
				 (ptrdiff_t)label->index)) {
	    return -1;
	}
    }

    offset = CG_OFFSET(cg);
    delta = offset - *last;
    *last = offset;
    return js_Emit3(cx, cg, JSOP_GOTO,
		    JUMP_OFFSET_HI(delta), JUMP_OFFSET_LO(delta));
}

static JSBool
PatchGotos(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *stmt,
	   ptrdiff_t last, jsbytecode *target)
{
    jsbytecode *pc, *top;
    ptrdiff_t delta, jumpOffset;

    pc = CG_CODE(cg, last);
    top = CG_CODE(cg, stmt->top);
    while (pc > top) {
	PR_ASSERT(*pc == JSOP_GOTO);
        delta = GET_JUMP_OFFSET(pc);
        jumpOffset = PTRDIFF(target, pc, jsbytecode);
        CHECK_AND_SET_JUMP_OFFSET(cx, cg, pc, jumpOffset);
        pc -= delta;
    }
    return JS_TRUE;
}

ptrdiff_t
js_EmitBreak(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *stmt,
	     JSAtomListElement *label)
{
    return EmitGoto(cx, cg, stmt, &stmt->breaks, label, SRC_BREAK2LABEL);
}

ptrdiff_t
js_EmitContinue(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *stmt,
		JSAtomListElement *label)
{
    return EmitGoto(cx, cg, stmt, &stmt->continues, label, SRC_CONT2LABEL);
}

extern void
js_PopStatement(JSTreeContext *tc)
{
    tc->topStmt = tc->topStmt->down;
}

JSBool
js_PopStatementCG(JSContext *cx, JSCodeGenerator *cg)
{
    JSStmtInfo *stmt;

    stmt = cg->treeContext.topStmt;
    if (!PatchGotos(cx, cg, stmt, stmt->breaks, cg->next))
	return JS_FALSE;
    if (!PatchGotos(cx, cg, stmt, stmt->continues, CG_CODE(cg, stmt->update)))
	return JS_FALSE;
    cg->treeContext.topStmt = stmt->down;
    return JS_TRUE;
}

/*
 * Emit a bytecode and its 2-byte constant (atom) index immediate operand.
 * NB: We use cx and cg from our caller's lexical environment, and return
 * false on error.
 */
#define EMIT_ATOM_INDEX_OP(op, atomIndex)                                     \
    PR_BEGIN_MACRO                                                            \
	if (js_Emit3(cx, cg, op, ATOM_INDEX_HI(atomIndex),                    \
				 ATOM_INDEX_LO(atomIndex)) < 0) {             \
	    return JS_FALSE;                                                  \
	}                                                                     \
    PR_END_MACRO

static JSBool
EmitAtomOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSAtomListElement *ale;

    ale = js_IndexAtom(cx, pn->pn_atom, &cg->atomList);
    if (!ale)
    	return JS_FALSE;
    EMIT_ATOM_INDEX_OP(op, ale->index);
    return JS_TRUE;
}

static JSBool
EmitPropOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSParseNode *pn2;
    JSAtomListElement *ale;

    pn2 = pn->pn_expr;
    if (!js_EmitTree(cx, cg, pn2))
	return JS_FALSE;
    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
		       (ptrdiff_t)(CG_OFFSET(cg) - pn2->pn_offset)) < 0) {
	return JS_FALSE;
    }
    if (!pn->pn_atom) {
	PR_ASSERT(op == JSOP_IMPORTALL);
	if (js_Emit1(cx, cg, op) < 0)
	    return JS_FALSE;
    } else {
	ale = js_IndexAtom(cx, pn->pn_atom, &cg->atomList);
	if (!ale)
	    return JS_FALSE;
	EMIT_ATOM_INDEX_OP(op, ale->index);
    }
    return JS_TRUE;
}

static JSBool
EmitElemOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSParseNode *pn2;

    pn2 = pn->pn_left;
    if (!js_EmitTree(cx, cg, pn2) || !js_EmitTree(cx, cg, pn->pn_right))
	return JS_FALSE;
    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
		       (ptrdiff_t)(CG_OFFSET(cg) - pn2->pn_offset)) < 0) {
	return JS_FALSE;
    }
    return js_Emit1(cx, cg, op) >= 0;
}

static JSBool
EmitNumberOp(JSContext *cx, jsdouble dval, JSCodeGenerator *cg)
{
    jsint ival;
    jsatomid atomIndex;
    JSAtom *atom;
    JSAtomListElement *ale;

    ival = (jsint)dval;
    if (JSDOUBLE_IS_INT(dval, ival) && INT_FITS_IN_JSVAL(ival)) {
	if (ival == 0)
	    return js_Emit1(cx, cg, JSOP_ZERO) >= 0;
	if (ival == 1)
	    return js_Emit1(cx, cg, JSOP_ONE) >= 0;
	if ((jsuint)ival < (jsuint)ATOM_INDEX_LIMIT) {
	    atomIndex = (jsatomid)ival;
	    EMIT_ATOM_INDEX_OP(JSOP_UINT16, atomIndex);
	    return JS_TRUE;
	}
	atom = js_AtomizeInt(cx, ival, 0);
    } else {
	atom = js_AtomizeDouble(cx, dval, 0);
    }
    if (!atom)
	return JS_FALSE;
    ale = js_IndexAtom(cx, atom, &cg->atomList);
    if (!ale)
	return JS_FALSE;
    EMIT_ATOM_INDEX_OP(JSOP_NUMBER, ale->index);
    return JS_TRUE;
}

JSBool
js_EmitFunctionBody(JSContext *cx, JSCodeGenerator *cg, JSParseNode *body,
		    JSFunction *fun)
{
    if (!js_AllocTryNotes(cx, cg))
    	return JS_FALSE;
    if (!js_EmitTree(cx, cg, body))
    	return JS_FALSE;
    fun->script = js_NewScriptFromCG(cx, cg, fun);
    if (!fun->script)
	return JS_FALSE;
    return JS_TRUE;
}

JSBool
js_EmitTree(JSContext *cx, JSCodeGenerator *cg, JSParseNode *pn)
{
    JSBool ok;
    uintN lineno, delta;
    JSCodeGenerator cg2;
    JSStmtInfo *stmt, stmtInfo;
    ptrdiff_t top, off, tmp, beq, jmp;
    JSParseNode *pn2, *pn3, *pn4;
    JSAtom *atom;
    JSAtomListElement *ale;
    jsatomid atomIndex;
    intN noteIndex;
    JSOp op;
    uint32 argc;

    pn->pn_offset = top = CG_OFFSET(cg);

    /* Emit notes to tell the current bytecode's source line number. */
    lineno = pn->pn_pos.begin.lineno;
    delta = lineno - cg->currentLine;
    cg->currentLine = lineno;
    if (delta) {
    	/* If delta requires too many SRC_NEWLINE notes, use SRC_SETLINE. */
    	if (delta >= (uintN)(2 + (lineno > SN_2BYTE_OFFSET_MASK))) {
	    if (js_NewSrcNote2(cx, cg, SRC_SETLINE, (ptrdiff_t)lineno) < 0)
		return JS_FALSE;
    	} else {
	    do {
	    	if (js_NewSrcNote(cx, cg, SRC_NEWLINE) < 0)
		    return JS_FALSE;
	    } while (--delta != 0);
	}
    }

    switch (pn->pn_type) {
      case TOK_FUNCTION:
      {
	JSFunction *fun;

	if (!js_InitCodeGenerator(cx, &cg2, cg->filename,
				  pn->pn_pos.begin.lineno,
				  cg->principals)) {
	    return JS_FALSE;
	}
	ok = js_FoldConstants(cx, pn->pn_body);
	if (ok) {
	    cg2.treeContext.tryCount = pn->pn_tryCount;
	    fun = pn->pn_fun;
	    ok = js_EmitFunctionBody(cx, &cg2, pn->pn_body, fun);
	}
	js_ResetCodeGenerator(cx, &cg2);
	if (!ok)
	    return JS_FALSE;

	/* Make the function object a literal in the outer script's pool. */
	atom = js_AtomizeObject(cx, fun->object, 0);
	if (!atom)
	    return JS_FALSE;
	ale = js_IndexAtom(cx, atom, &cg->atomList);
	if (!ale)
	    return JS_FALSE;

	/* Emit a bytecode or srcnote naming the literal in its immediate. */
#if JS_HAS_LEXICAL_CLOSURE
	if (pn->pn_op != JSOP_NOP) {
	    EMIT_ATOM_INDEX_OP(pn->pn_op, ale->index);
	    break;
	}
#endif
	noteIndex = js_NewSrcNote2(cx, cg, SRC_FUNCDEF, (ptrdiff_t)ale->index);
	if (noteIndex < 0 ||
	    js_Emit1(cx, cg, JSOP_NOP) < 0) {
	    return JS_FALSE;
	}
	break;
      }

#if JS_HAS_EXPORT_IMPORT
      case TOK_EXPORT:
	pn2 = pn->pn_head;
	if (pn2->pn_type == TOK_STAR) {
	    /*
	     * 'export *' must have no other elements in the list (what would
	     * be the point?).
	     */
	    if (js_Emit1(cx, cg, JSOP_EXPORTALL) < 0)
		return JS_FALSE;
	} else {
	    /*
	     * If not 'export *', the list consists of NAME nodes identifying
	     * properties of the variable object to flag as exported.
	     */
	    do {
		ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
		if (!ale)
		    return JS_FALSE;
		EMIT_ATOM_INDEX_OP(JSOP_EXPORTNAME, ale->index);
	    } while ((pn2 = pn2->pn_next) != NULL);
	}
	break;

      case TOK_IMPORT:
	for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
	    /*
	     * Each subtree on an import list is rooted by a DOT or LB node.
	     * A DOT may have a null pn_atom member, in which case pn_op must
	     * be JSOP_IMPORTALL -- see EmitPropOp above.
	     */
	    if (!js_EmitTree(cx, cg, pn2))
	    	return JS_FALSE;
	}
	break;
#endif /* JS_HAS_EXPORT_IMPORT */

      case TOK_IF:
	/* Emit code for the condition before pushing stmtInfo. */
	if (!js_EmitTree(cx, cg, pn->pn_kid1))
	    return JS_FALSE;
	js_PushStatement(&cg->treeContext, &stmtInfo, STMT_IF, CG_OFFSET(cg));

	/* Emit an annotated branch-if-false around the then part. */
	noteIndex = js_NewSrcNote(cx, cg, SRC_IF);
	if (noteIndex < 0)
	    return JS_FALSE;
	beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	if (beq < 0)
	    return JS_FALSE;

	/* Emit code for the then and optional else parts. */
	if (!js_EmitTree(cx, cg, pn->pn_kid2))
	    return JS_FALSE;
	pn3 = pn->pn_kid3;
	if (pn3) {
	    /* Modify stmtInfo and the branch-if-false source note. */
	    stmtInfo.type = STMT_ELSE;
	    SN_SET_TYPE(&cg->notes[noteIndex], SRC_IF_ELSE);

	    /* Jump at end of then part around the else part. */
	    jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	    if (jmp < 0)
		return JS_FALSE;

	    /* Ensure the branch-if-false comes here, then emit the else. */
	    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	    if (!js_EmitTree(cx, cg, pn3))
		return JS_FALSE;

	    /* Fixup the jump around the else part. */
	    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
	} else {
	    /* No else part, fixup the branch-if-false to come here. */
	    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	}
	return js_PopStatementCG(cx, cg);

#if JS_HAS_SWITCH_STATEMENT
      case TOK_SWITCH:
      {
	JSOp switchop;
	uint32 ncases, tablen;
	JSScript *script;
	jsint i, low, high;
	size_t switchsize, tablesize;
	void *mark;
	JSParseNode **table;
	jsbytecode *pc;

	/* Emit code for the discriminant first. */
	if (!js_EmitTree(cx, cg, pn->pn_kid1))
	    return JS_FALSE;

	/* Switch bytecodes run from here till end of final case. */
	top = CG_OFFSET(cg);
	js_PushStatement(&cg->treeContext, &stmtInfo, STMT_SWITCH, top);

	switchop = JSOP_TABLESWITCH;
	pn2 = pn->pn_kid2;
	ncases = pn2->pn_count;

	if (pn2->pn_count == 0) {
	    low = high = 0;
	    tablen = 0;
	    ok = JS_TRUE;
	} else {
	    low  = JSVAL_INT_MAX;
	    high = JSVAL_INT_MIN;
	    cg2.base = NULL;
	    for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
		PR_ASSERT(pn3->pn_type == TOK_CASE);
		if (!cg2.base) {
		    if (!js_InitCodeGenerator(cx, &cg2, cg->filename,
					      pn3->pn_pos.begin.lineno,
					      cg->principals)) {
			return JS_FALSE;
		    }
		} else {
		    js_ResetCodeGenerator(cx, &cg2);
		}
		pn4 = pn3->pn_left;
		cg2.currentLine = pn4->pn_pos.begin.lineno;
		if (!js_EmitTree(cx, &cg2, pn4))
		    return JS_FALSE;
		if (js_Emit1(cx, &cg2, JSOP_POPV) < 0)
		    return JS_FALSE;
		script = js_NewScriptFromCG(cx, &cg2, NULL);
		if (!script)
		    return JS_FALSE;

		/*
		 * The GC must not run during this case expression evaluation.
		 * It won't if it runs only from the branch callback (because
		 * the only branches possible in a case expression come from ?:
		 * expressions, and those generate downward branches, while the
		 * branch callback runs only for upward branches).
		 */
		ok = js_Execute(cx, cx->fp->scopeChain, script, NULL, cx->fp,
				JS_FALSE, &pn3->pn_val);
		js_DestroyScript(cx, script);
		if (!ok)
		    return JS_FALSE;

		if (!JSVAL_IS_NUMBER(pn3->pn_val) &&
		    !JSVAL_IS_STRING(pn3->pn_val) &&
		    !JSVAL_IS_BOOLEAN(pn3->pn_val)) {
		    JS_ReportError(cx,
				   "%s, line %u: invalid case expression",
				   cg2.filename ? cg2.filename : "stdin",
				   pn4->pn_pos.begin.lineno);
		    return JS_FALSE;
		}

		if (switchop == JSOP_LOOKUPSWITCH)
		    continue;
		if (!JSVAL_IS_INT(pn3->pn_val)) {
		    switchop = JSOP_LOOKUPSWITCH;
		    continue;
		}
		i = JSVAL_TO_INT(pn3->pn_val);
		if ((jsuint)(i + (jsint)PR_BIT(15)) >= (jsuint)PR_BIT(16)) {
		    switchop = JSOP_LOOKUPSWITCH;
		    continue;
		}
		if (i < low)
		    low = i;
		if (high < i)
		    high = i;
	    }
	    if (cg2.base)
		js_ResetCodeGenerator(cx, &cg2);
	    if (switchop == JSOP_TABLESWITCH) {
		tablen = (uint32)(high - low + 1);
		if (tablen >= PR_BIT(16) || tablen > 2 * ncases)
		    switchop = JSOP_LOOKUPSWITCH;
	    }
	}

	/* Emit a note with an offset telling total switch code length. */
	noteIndex = js_NewSrcNote2(cx, cg, SRC_SWITCH, 0);
	if (noteIndex < 0)
	    return JS_FALSE;

	if (switchop == JSOP_TABLESWITCH) {
	    /*
	     * 3 offsets (len, low, high) before the table, 1 per entry.
	     */
	    switchsize = (size_t)(6 + 2 * tablen);
	} else {
	    /*
	     * 1 offset (len) and 1 atom index (npairs) before the table,
	     * 1 atom index and 1 jump offset per entry.
	     */
	    switchsize = (size_t)(4 + 4 * ncases);
	}

	/* Emit switchop and switchsize bytes of jump or lookup table. */
	if (js_EmitN(cx, cg, switchop, switchsize) < 0)
	    return JS_FALSE;

	/* Emit code for each case's statements, copying pn_offset up to pn3. */
	for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
	    pn4 = pn3->pn_right;
	    if (!js_EmitTree(cx, cg, pn4))
	    	return JS_FALSE;
	    pn3->pn_offset = pn4->pn_offset;
	}

	/* Emit the default case (if any) and compute default offset. */
	pn3 = pn->pn_kid3;
	if (pn3) {
	    if (!js_EmitTree(cx, cg, pn3))
	    	return JS_FALSE;
	    off = pn3->pn_offset - top;
	} else {
	    off = CG_OFFSET(cg) - top;
	}

	/* Set the default offset (to end of switch if no default). */
	pc = CG_CODE(cg, top);
	if (!js_SetJumpOffset(cx, cg, pc, off))
	    return JS_FALSE;
	pc += 2;

	/* Set the SRC_SWITCH note's offset operand to tell end of switch. */
	off = CG_OFFSET(cg) - top;
	if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 0, (ptrdiff_t)off))
	    return JS_FALSE;

	/* Fill in jump or lookup table. */
	if (switchop == JSOP_TABLESWITCH) {
	    if (!js_SetJumpOffset(cx, cg, pc, low))
		return JS_FALSE;
	    pc += 2;
	    if (!js_SetJumpOffset(cx, cg, pc, high))
		return JS_FALSE;
	    pc += 2;
	    if (tablen) {
		/* Avoid bloat for a compilation unit with many switches. */
		mark = PR_ARENA_MARK(&cx->tempPool);
		tablesize = (size_t)tablen * sizeof *table;
		PR_ARENA_ALLOCATE(table, &cx->tempPool, tablesize);
		if (!table) {
		    JS_ReportOutOfMemory(cx);
		    return JS_FALSE;
		}
		memset(table, 0, tablesize);
		for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
		    i = JSVAL_TO_INT(pn3->pn_val);
		    i -= low;
		    PR_ASSERT((uint32)i < tablen);
		    table[i] = pn3;
		}
		for (i = 0; i < (jsint)tablen; i++) {
		    pn3 = table[i];
		    off = pn3 ? pn3->pn_offset - top : 0;
		    if (!js_SetJumpOffset(cx, cg, pc, off))
			return JS_FALSE;
		    pc += 2;
		}
		PR_ARENA_RELEASE(&cx->tempPool, mark);
	    }
	} else {
	    SET_ATOM_INDEX(pc, ncases);
	    pc += 2;

	    for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
		atom = js_AtomizeValue(cx, pn3->pn_val, 0);
		if (!atom)
		    return JS_FALSE;
		ale = js_IndexAtom(cx, atom, &cg->atomList);
		if (!ale)
		    return JS_FALSE;
		SET_ATOM_INDEX(pc, ale->index);
		pc += 2;

		off = pn3->pn_offset - top;
		if (!js_SetJumpOffset(cx, cg, pc, off))
		    return JS_FALSE;
		pc += 2;
	    }
	}

	return js_PopStatementCG(cx, cg);
      }
#endif /* JS_HAS_SWITCH_STATEMENT */

      case TOK_WHILE:
	js_PushStatement(&cg->treeContext, &stmtInfo, STMT_WHILE_LOOP, top);
	if (!js_EmitTree(cx, cg, pn->pn_left))
	    return JS_FALSE;
	noteIndex = js_NewSrcNote(cx, cg, SRC_WHILE);
	if (noteIndex < 0)
	    return JS_FALSE;
	beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	if (beq < 0)
	    return JS_FALSE;
	if (!js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET(cx, cg, CG_CODE(cg,jmp), top - jmp);
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	return js_PopStatementCG(cx, cg);

#if JS_HAS_DO_WHILE_LOOP
      case TOK_DO:
	/* Emit an annotated nop so we know to decompile a 'do' keyword. */
	if (js_NewSrcNote(cx, cg, SRC_WHILE) < 0 ||
	    js_Emit1(cx, cg, JSOP_NOP) < 0) {
	    return JS_FALSE;
	}

	/* Compile the loop body. */
	top = CG_OFFSET(cg);
	js_PushStatement(&cg->treeContext, &stmtInfo, STMT_DO_LOOP, top);
	if (!js_EmitTree(cx, cg, pn->pn_left))
	    return JS_FALSE;

	/* Set loop and enclosing label update offsets, for continue. */
	stmt = &stmtInfo;
	do {
	    stmt->update = CG_OFFSET(cg);
	} while ((stmt = stmt->down) != NULL && stmt->type == STMT_LABEL);

	/* Compile the loop condition, now that continues know where to go. */
	if (!js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;

	/* Re-use the SRC_WHILE note, this time for the JSOP_IFNE opcode. */
	if (js_NewSrcNote(cx, cg, SRC_WHILE) < 0)
	    return JS_FALSE;
	jmp = top - CG_OFFSET(cg);
	if (js_Emit3(cx, cg, JSOP_IFNE,
		     JUMP_OFFSET_HI(jmp), JUMP_OFFSET_LO(jmp)) < 0) {
	    return JS_FALSE;
	}
	return js_PopStatementCG(cx, cg);
#endif /* JS_HAS_DO_WHILE_LOOP */

      case TOK_FOR:
	pn2 = pn->pn_left;
	js_PushStatement(&cg->treeContext, &stmtInfo, STMT_FOR_LOOP, top);

	if (pn2->pn_type == TOK_IN) {
	    /* If the left part is var x = i, bind x, evaluate i, and pop. */
	    pn3 = pn2->pn_left;
	    if (pn3->pn_type == TOK_VAR && pn3->pn_head->pn_expr) {
	      	if (!js_EmitTree(cx, cg, pn3))
		    return JS_FALSE;
	      	/* Set pn3 to the variable name, to avoid another var note. */
	      	pn3 = pn3->pn_head;
	      	PR_ASSERT(pn3->pn_type == TOK_NAME);
	    }

	    /* Fix stmtInfo and emit a push to allocate the iterator. */
	    stmtInfo.type = STMT_FOR_IN_LOOP;
	    noteIndex = -1;
	    if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
	    	return JS_FALSE;

	    /* Compile the object expression to the right of 'in'. */
	    if (!js_EmitTree(cx, cg, pn2->pn_right))
	    	return JS_FALSE;
	    if (js_Emit1(cx, cg, JSOP_TOOBJECT) < 0)
	    	return JS_FALSE;

	    top = CG_OFFSET(cg);
	    SET_STATEMENT_TOP(&stmtInfo, top);

	    /* Compile a JSOP_FOR*2 bytecode based on the left hand side. */
	    switch (pn3->pn_type) {
	      case TOK_VAR:
	      	pn3 = pn3->pn_head;
		if (js_NewSrcNote(cx, cg, SRC_VAR) < 0)
		    return JS_FALSE;
	      	/* FALL THROUGH */
	      case TOK_NAME:
		if (!EmitAtomOp(cx, pn3, JSOP_FORNAME2, cg))
		    return JS_FALSE;
	      	break;
	      case TOK_DOT:
	      	if (!EmitPropOp(cx, pn3, JSOP_FORPROP2, cg))
		    return JS_FALSE;
	      	break;
	      case TOK_LB:
	      	if (!EmitElemOp(cx, pn3, JSOP_FORELEM2, cg))
		    return JS_FALSE;
	      	break;
	      default:
	      	PR_ASSERT(0);
	    }

	    /* Pop and test the loop condition generated by JSOP_FOR*2. */
	    beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	    if (beq < 0)
		return JS_FALSE;
	} else {
	    if (!pn2->pn_kid1) {
		/* No initializer: emit an annotated nop for the decompiler. */
		noteIndex = js_NewSrcNote(cx, cg, SRC_FOR);
		if (noteIndex < 0 ||
		    js_Emit1(cx, cg, JSOP_NOP) < 0) {
		    return JS_FALSE;
		}
	    } else {
		if (!js_EmitTree(cx, cg, pn2->pn_kid1))
		    return JS_FALSE;
		noteIndex = js_NewSrcNote(cx, cg, SRC_FOR);
		if (noteIndex < 0 ||
		    js_Emit1(cx, cg, JSOP_POP) < 0) {
		    return JS_FALSE;
		}
	    }

	    top = CG_OFFSET(cg);
	    SET_STATEMENT_TOP(&stmtInfo, top);
	    if (!pn2->pn_kid2) {
		/* No loop condition: flag this fact in the source notes. */
		if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 0, 0))
		    return JS_FALSE;
		beq = 0;
	    } else {
		if (!js_EmitTree(cx, cg, pn2->pn_kid2))
		    return JS_FALSE;
		if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 0,
					 (ptrdiff_t)(CG_OFFSET(cg) - top))) {
		    return JS_FALSE;
		}
		beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
		if (beq < 0)
		    return JS_FALSE;
	    }
	}

	/* Emit code for the loop body. */
	if (!js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;

	if (pn2->pn_type != TOK_IN) {
	    /* Set the second note offset so we can find the update part. */
	    PR_ASSERT(noteIndex != -1);
	    if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 1,
				     (ptrdiff_t)(CG_OFFSET(cg) - top))) {
		return JS_FALSE;
	    }

	    pn3 = pn2->pn_kid3;
	    if (pn3) {
		/* Set loop and enclosing "update" offsets, for continue. */
		stmt = &stmtInfo;
		do {
		    stmt->update = CG_OFFSET(cg);
		} while ((stmt = stmt->down) != NULL &&
			 stmt->type == STMT_LABEL);

		if (!js_EmitTree(cx, cg, pn3))
		    return JS_FALSE;
		if (js_Emit1(cx, cg, JSOP_POP) < 0)
		    return JS_FALSE;

		/* Restore the absolute line number for source note readers. */
		off = (ptrdiff_t) pn->pn_pos.end.lineno;
		if (cg->currentLine != (uintN) off) {
		    if (js_NewSrcNote2(cx, cg, SRC_SETLINE, off) < 0)
			return JS_FALSE;
		    cg->currentLine = (uintN) off;
		}
	    }

	    /* The third note offset helps us find the loop-closing jump. */
	    if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 2,
				     (ptrdiff_t)(CG_OFFSET(cg) - top))) {
		return JS_FALSE;
	    }
	}

	/* Emit the loop-closing jump and fixup all jump offsets. */
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET(cx, cg, CG_CODE(cg,jmp), top - jmp);
	if (beq > 0)
	    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);

	/* Now fixup all breaks and continues (before for/in's final POP2). */
	if (!js_PopStatementCG(cx, cg))
	    return JS_FALSE;

	if (pn2->pn_type == TOK_IN) {
	    /*
	     * Generate the object and iterator pop opcodes after popping the
	     * stmtInfo stack, so breaks will go to this pop bytecode.
	     */
	    if (js_Emit1(cx, cg, JSOP_POP2) < 0)
		return JS_FALSE;
	}
	break;

      case TOK_BREAK:
	stmt = cg->treeContext.topStmt;
	atom = pn->pn_atom;
	if (atom) {
	    ale = js_IndexAtom(cx, atom, &cg->atomList);
	    if (!ale)
	    	return JS_FALSE;
	    while (stmt->type != STMT_LABEL || stmt->label != atom)
		stmt = stmt->down;
	} else {
	    ale = NULL;
	    while (!STMT_IS_LOOP(stmt) && stmt->type != STMT_SWITCH)
		stmt = stmt->down;
	}
	if (js_EmitBreak(cx, cg, stmt, ale) < 0)
	    return JS_FALSE;
	break;

      case TOK_CONTINUE:
	stmt = cg->treeContext.topStmt;
	atom = pn->pn_atom;
	if (atom) {
	    ale = js_IndexAtom(cx, atom, &cg->atomList);
	    if (!ale)
	    	return JS_FALSE;
	    while (stmt->type != STMT_LABEL || stmt->label != atom)
		stmt = stmt->down;
	} else {
	    ale = NULL;
	    while (!STMT_IS_LOOP(stmt))
		stmt = stmt->down;
	    if (js_NewSrcNote(cx, cg, SRC_CONTINUE) < 0)
		return JS_FALSE;
	}
	if (js_EmitContinue(cx, cg, stmt, ale) < 0)
	    return JS_FALSE;
	break;

      case TOK_WITH:
	if (!js_EmitTree(cx, cg, pn->pn_left))
	    return JS_FALSE;
	js_PushStatement(&cg->treeContext, &stmtInfo, STMT_WITH, CG_OFFSET(cg));
	if (js_Emit1(cx, cg, JSOP_ENTERWITH) < 0)
	    return JS_FALSE;
	if (!js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;
	if (js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0)
	    return JS_FALSE;
	return js_PopStatementCG(cx, cg);

#if JS_HAS_EXCEPTIONS
      case TOK_TRY: {
	ptrdiff_t start, end, catch;
#if shaver_finally
	/* if this try has a finally, push marker onto treeContext */
	if (pn->pn_kid3)
	    js_PushStatement(&cg->treeContext, &stmtInto, STMT_FINALLY,
			     CG_OFFSET(cg));
#endif

	/* mark try location for decompilation, then emit try block */
	if (js_NewSrcNote(cx, cg, SRC_TRY) < 0 ||
	    js_Emit1(cx, cg, JSOP_NOP) < 0)
	    return JS_FALSE;
	start = CG_OFFSET(cg);
	if(!js_EmitTree(cx, cg, pn->pn_kid1))
	    return JS_FALSE;
	
	/* emit (hidden) jump over try and/or finally */
	if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
	    return JS_FALSE;
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
	end = CG_OFFSET(cg);


	/* if this try has a catch block, emit it */
	if (pn->pn_kid2) {
	    catch = CG_OFFSET(cg);
	    if (!js_EmitTree(cx, cg, pn->pn_kid2))
		return JS_FALSE;
	}

#if shaver_finally
	/* if we've got a finally, it goes here, and we have to fix up
	   the jsrs that might have been emitted before non-local jumps */
	if (pn->pn_kid3) {
	    ptrdiff_t finallyIndex = CG_OFFSET(CG);
	    js_PopStatement(tc);
	    if (!FixupFinallyJumps(cx, cg, tryStart, finallyIndex))
		return JS_FALSE;
	    if (!js_EmitTree(cx, cg, pn->pn_kid3))
		return JS_FALSE;
	}
#endif
	/* come from post-try */
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);

	if (js_NewSrcNote(cx, cg, SRC_ENDBRACE) < 0 ||
	    js_Emit1(cx, cg, JSOP_NOP) < 0)
	    return JS_FALSE;

	/*
	 * Add the try note last, to let post-order give us the right ordering
	 * (first to last, inner to outer).
	 */
	if (!js_NewTryNote(cx, cg, start, end, catch, 0))
	    return JS_FALSE;
	break;
      }

      case TOK_CATCH:
	/* 
	 * Catch blocks are very magical.  Mainly, the magic involves
	 * special generation for the declaration/conditional, and
	 * mucking with the stack depth counter so that the implicit
	 * push of the exception that happens at runtime doesn't make
	 * the code generator very confused.
	 *
	 * If a catch block has a finally associated with it, the try
	 * block will already have pushed the appropriate statement info
	 * onto the tree context, so we don't need to worry about it.
	 */
	 noteIndex = js_NewSrcNote(cx, cg, SRC_CATCH);
	 if (noteIndex < 0 ||
	     !js_Emit1(cx, cg, JSOP_NOP))
	     return JS_FALSE;

	/* need to adjust stack depth */
	if ((uintN)++cg->stackDepth > cg->maxStackDepth)
	    cg->maxStackDepth = cg->stackDepth;
	if (!js_EmitTree(cx, cg, pn->pn_left) ||
	    !js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;
	break;
#endif /* JS_HAS_EXCEPTIONS */

      case TOK_VAR:
	noteIndex = -1;
	for (pn2 = pn->pn_head; ; pn2 = pn2->pn_next) {
	    PR_ASSERT(pn2->pn_type == TOK_NAME);
	    op = pn2->pn_op;
	    if (pn2->pn_slot >= 0) {
		atomIndex = (jsatomid) pn2->pn_slot;
	    } else {
		ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
		if (!ale)
		    return JS_FALSE;
		atomIndex = ale->index;
	    }
	    if (pn2->pn_expr) {
		if (op == JSOP_SETNAME2)
		    EMIT_ATOM_INDEX_OP(JSOP_BINDNAME, atomIndex);
		if (!js_EmitTree(cx, cg, pn2->pn_expr))
		    return JS_FALSE;
	    }
	    if (pn2 == pn->pn_head && js_NewSrcNote(cx, cg, SRC_VAR) < 0)
		return JS_FALSE;
	    EMIT_ATOM_INDEX_OP(op, atomIndex);
	    tmp = CG_OFFSET(cg);
	    if (noteIndex >= 0) {
		if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 0, tmp - off))
		    return JS_FALSE;
	    }
	    if (!pn2->pn_next)
	    	break;
	    off = tmp;
	    noteIndex = js_NewSrcNote2(cx, cg, SRC_COMMA, 0);
	    if (noteIndex < 0 ||
		js_Emit1(cx, cg, JSOP_POP) < 0) {
		return JS_FALSE;
	    }
	}
	if (pn->pn_op != JSOP_NOP) {
	    if (js_Emit1(cx, cg, pn->pn_op) < 0)
		return JS_FALSE;
      	}
      	break;

      case TOK_RETURN:
	pn2 = pn->pn_kid;
	if (pn2) {
	    if (!js_EmitTree(cx, cg, pn2))
		return JS_FALSE;
	} else {
	    if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
		return JS_FALSE;
      	}
	if (js_Emit1(cx, cg, JSOP_RETURN) < 0)
	    return JS_FALSE;
	break;

      case TOK_LC:
	js_PushStatement(&cg->treeContext, &stmtInfo, STMT_BLOCK, top);
	for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
	    if (!js_EmitTree(cx, cg, pn2))
		return JS_FALSE;
	}
	return js_PopStatementCG(cx, cg);

      case TOK_SEMI:
	if (pn->pn_kid) {
	    if (!js_EmitTree(cx, cg, pn->pn_kid))
		return JS_FALSE;
	    if (js_Emit1(cx, cg, JSOP_POPV) < 0)
		return JS_FALSE;
      	}
      	break;

      case TOK_COLON:
	/* Emit an annotated nop so we know to decompile a label. */
	atom = pn->pn_atom;
	ale = js_IndexAtom(cx, atom, &cg->atomList);
	if (!ale)
	    return JS_FALSE;
	pn2 = pn->pn_expr;
	noteIndex = js_NewSrcNote2(cx, cg,
				   (pn2->pn_type == TOK_LC)
				   ? SRC_LABELBRACE
				   : SRC_LABEL,
				   (ptrdiff_t)ale->index);
	if (noteIndex < 0 ||
	    js_Emit1(cx, cg, JSOP_NOP) < 0) {
	    return JS_FALSE;
	}

	/* Emit code for the labeled statement. */
	js_PushStatement(&cg->treeContext, &stmtInfo, STMT_LABEL, CG_OFFSET(cg));
	stmtInfo.label = atom;
	if (!js_EmitTree(cx, cg, pn2))
	    return JS_FALSE;
	if (!js_PopStatementCG(cx, cg))
	    return JS_FALSE;

	/* If the statement was compound, emit a note for the end brace. */
	if (pn2->pn_type == TOK_LC) {
	    if (js_NewSrcNote(cx, cg, SRC_ENDBRACE) < 0 ||
		js_Emit1(cx, cg, JSOP_NOP) < 0) {
		return JS_FALSE;
	    }
	}
      	break;

      case TOK_COMMA:
	/*
	 * Emit SRC_COMMA notes on each JSOP_POP between comma operands.
	 * These notes help the decompiler bracket the bytecodes generated
	 * from each sub-expression that follows a comma.
	 */
	noteIndex = -1;
	for (pn2 = pn->pn_head; ; pn2 = pn2->pn_next) {
	    if (!js_EmitTree(cx, cg, pn2))
		return JS_FALSE;
	    tmp = CG_OFFSET(cg);
	    if (noteIndex >= 0) {
		if (!js_SetSrcNoteOffset(cx, cg, noteIndex, 0, tmp - off))
		    return JS_FALSE;
	    }
	    if (!pn2->pn_next)
		break;
	    off = tmp;
	    noteIndex = js_NewSrcNote2(cx, cg, SRC_COMMA, 0);
	    if (noteIndex < 0 ||
		js_Emit1(cx, cg, JSOP_POP) < 0) {
		return JS_FALSE;
	    }
	}
      	break;

      case TOK_ASSIGN:
      	/*
      	 * Check left operand type and generate specialized code for it.
      	 * Specialize to avoid ECMA "reference type" values on the operand
      	 * stack, which impose pervasive runtime "GetValue" costs.
      	 */
	pn2 = pn->pn_left;
	PR_ASSERT(pn2->pn_type != TOK_RP);
      	switch (pn2->pn_type) {
	  case TOK_NAME:
	    if (pn2->pn_slot >= 0) {
	    	atomIndex = (jsatomid) pn2->pn_slot;
	    } else {
		ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
		if (!ale)
		    return JS_FALSE;
		atomIndex = ale->index;
		EMIT_ATOM_INDEX_OP(JSOP_BINDNAME, atomIndex);
	    }
	    break;
	  case TOK_DOT:
	    if (!js_EmitTree(cx, cg, pn2->pn_expr))
	    	return JS_FALSE;
	    ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
	    if (!ale)
	    	return JS_FALSE;
	    atomIndex = ale->index;
	    break;
	  case TOK_LB:
	    if (!js_EmitTree(cx, cg, pn2->pn_left))
		return JS_FALSE;
	    if (!js_EmitTree(cx, cg, pn2->pn_right))
		return JS_FALSE;
	    break;
	  default:
	    PR_ASSERT(0);
	}

	/* If += or similar, dup the left operand and get its value. */
	op = pn->pn_op;
	if (op != JSOP_NOP) {
	    switch (pn2->pn_type) {
	      case TOK_NAME:
		if (pn2->pn_op != JSOP_SETNAME2) {
		    EMIT_ATOM_INDEX_OP((pn2->pn_op == JSOP_SETARG)
				       ? JSOP_GETARG
				       : JSOP_GETVAR,
				       atomIndex);
		    break;
		}
		/* FALL THROUGH */
	      case TOK_DOT:
		if (js_Emit1(cx, cg, JSOP_DUP) < 0)
		    return JS_FALSE;
		EMIT_ATOM_INDEX_OP(JSOP_GETPROP, atomIndex);
		break;
	      case TOK_LB:
		if (js_Emit1(cx, cg, JSOP_DUP2) < 0)
		    return JS_FALSE;
		if (js_Emit1(cx, cg, JSOP_GETELEM) < 0)
		    return JS_FALSE;
		break;
	      default:;
	    }
	}

	/* Now emit the right operand (it may affect the namespace). */
	if (!js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;

	/* If += etc., emit the binary operator with a decompiler note. */
	if (op != JSOP_NOP) {
	    if (js_NewSrcNote(cx, cg, SRC_ASSIGNOP) < 0 ||
		js_Emit1(cx, cg, op) < 0) {
		return JS_FALSE;
	    }
	}

	/* Left parts such as a.b.c and a[b].c need a decompiler note. */
	if (pn2->pn_type != TOK_NAME) {
	    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
			       (ptrdiff_t)(CG_OFFSET(cg) - top)) < 0) {
		return JS_FALSE;
	    }
	}

	/* Finally, emit the specialized assignment bytecode. */
	switch (pn2->pn_type) {
	  case TOK_NAME:
	  case TOK_DOT:
	    EMIT_ATOM_INDEX_OP(pn2->pn_op, atomIndex);
	    break;
	  case TOK_LB:
	    if (js_Emit1(cx, cg, JSOP_SETELEM) < 0)
		return JS_FALSE;
	    break;
	  default:;
	}
      	break;

      case TOK_HOOK:
	/* Emit the condition, then branch if false to the else part. */
	if (!js_EmitTree(cx, cg, pn->pn_kid1))
	    return JS_FALSE;
	if (js_NewSrcNote(cx, cg, SRC_COND) < 0)
	    return JS_FALSE;
	beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	if (beq < 0 || !js_EmitTree(cx, cg, pn->pn_kid2))
	    return JS_FALSE;

	/* Jump around else, fixup the branch, emit else, fixup jump. */
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
	if (!js_EmitTree(cx, cg, pn->pn_kid3))
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
	break;

      case TOK_OR:
	/* Emit left operand, emit pop-if-converts-to-false-else-jump. */
	if (!js_EmitTree(cx, cg, pn->pn_left))
	    return JS_FALSE;
#if JS_BUG_SHORT_CIRCUIT
	beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
	tmp = js_Emit1(cx, cg, JSOP_TRUE);
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (beq < 0 || tmp < 0 || jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
#else
	/*
	 * JSOP_OR converts the operand on the stack to boolean, and if true,
	 * leaves the original operand value on the stack and jumps; otherwise
	 * it pops and falls into the next bytecode.
	 */
	jmp = js_Emit3(cx, cg, JSOP_OR, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
#endif
	if (!js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
	break;

      case TOK_AND:
	/* && is like || except it uses a pop-if-converts-to-true-else-jump. */
	if (!js_EmitTree(cx, cg, pn->pn_left))
	    return JS_FALSE;
#if JS_BUG_SHORT_CIRCUIT
	beq = js_Emit3(cx, cg, JSOP_IFNE, 0, 0);
	tmp = js_Emit1(cx, cg, JSOP_FALSE);
	jmp = js_Emit3(cx, cg, JSOP_GOTO, 0, 0);
	if (beq < 0 || tmp < 0 || jmp < 0)
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, beq);
#else
	jmp = js_Emit3(cx, cg, JSOP_AND, 0, 0);
	if (jmp < 0)
	    return JS_FALSE;
#endif
	if (!js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;
	CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, jmp);
	break;

      case TOK_BITOR:
      case TOK_BITXOR:
      case TOK_BITAND:
      case TOK_EQOP:
      case TOK_RELOP:
#if JS_HAS_IN_OPERATOR
      case TOK_IN:
#endif
#if JS_HAS_INSTANCEOF
      case TOK_INSTANCEOF:
#endif
      case TOK_SHOP:
      case TOK_PLUS:
      case TOK_MINUS:
      case TOK_STAR:
      case TOK_DIVOP:
      	/* Binary operators that evaluate both operands unconditionally. */
      	if (!js_EmitTree(cx, cg, pn->pn_left))
	    return JS_FALSE;
      	if (!js_EmitTree(cx, cg, pn->pn_right))
	    return JS_FALSE;
	if (js_Emit1(cx, cg, pn->pn_op) < 0)
	    return JS_FALSE;
	break;

#if JS_HAS_EXCEPTIONS
      case TOK_THROW:
#endif
      case TOK_UNARYOP:
	/* Unary op, including unary +/-. */
	if (!js_EmitTree(cx, cg, pn->pn_kid))
	    return JS_FALSE;
	if (js_Emit1(cx, cg, pn->pn_op) < 0)
	    return JS_FALSE;
	break;

      case TOK_INC:
      case TOK_DEC:
	/* Emit lvalue-specialized code for ++/-- operators. */
	pn2 = pn->pn_kid;
	PR_ASSERT(pn2->pn_type != TOK_RP);
	op = pn->pn_op;
	switch (pn2->pn_type) {
	  case TOK_NAME:
	    if (pn->pn_num >= 0) {
		atomIndex = (jsatomid) pn->pn_num;
		EMIT_ATOM_INDEX_OP(op, atomIndex);
	    } else {
		if (!EmitAtomOp(cx, pn2, op, cg))
		    return JS_FALSE;
	    }
	    break;
	  case TOK_DOT:
	    if (!EmitPropOp(cx, pn2, op, cg))
	    	return JS_FALSE;
	    break;
	  case TOK_LB:
	    if (!EmitElemOp(cx, pn2, op, cg))
	    	return JS_FALSE;
	    break;
	  default:
	    PR_ASSERT(0);
	}
      	break;

      case TOK_NEW:
      	/* Code for (new f()) and f() are the same, except for the opcode. */
      	op = JSOP_NEW;
      	goto emit_call;

      case TOK_DELETE:
	/* Delete is also lvalue-specialized to avoid reference types. */
	pn2 = pn->pn_kid;
	PR_ASSERT(pn2->pn_type != TOK_RP);
      	switch (pn2->pn_type) {
	  case TOK_NAME:
	    if (!EmitAtomOp(cx, pn2, JSOP_DELNAME, cg))
	    	return JS_FALSE;
	    break;
	  case TOK_DOT:
	    if (!EmitPropOp(cx, pn2, JSOP_DELPROP, cg))
	    	return JS_FALSE;
	    break;
	  case TOK_LB:
	    if (!EmitElemOp(cx, pn2, JSOP_DELELEM, cg))
	    	return JS_FALSE;
	    break;
	  default:
	    PR_ASSERT(0);
	}
      	break;

      case TOK_DOT:
	/*
	 * Pop a stack operand, convert it to object, get a property named by
	 * this bytecode's immediate-indexed atom operand, and push its value
	 * (not a reference to it).  This bytecode sets the virtual machine's
	 * "obj" register to the left operand's ToObject conversion result,
	 * for use by JSOP_PUSHOBJ.
	 */
	return EmitPropOp(cx, pn, pn->pn_op, cg);

      case TOK_LB:
	/*
	 * Pop two operands, convert the left one to object and the right one
	 * to property name (atom or tagged int), get the named property, and
	 * push its value.  Set the "obj" register to the result of ToObject
	 * on the left operand.
	 */
	return EmitElemOp(cx, pn, pn->pn_op, cg);

      case TOK_LP:
	/*
	 * Emit function call or operator new (constructor call) code.  First
	 * emit code for the left operand to evaluate the call- or construct-
	 * able object expression.
	 */
	op = JSOP_CALL;
      emit_call:
	pn2 = pn->pn_head;
	if (!js_EmitTree(cx, cg, pn2))
	    return JS_FALSE;

	/*
	 * Push the virtual machine's "obj" register, which was set by a name,
	 * property, or element get (or set) bytecode.
	 */
	if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
	    return JS_FALSE;

      	/*
      	 * Emit code for each argument in order, then emit the JSOP_CALL or
      	 * JSOP_NEW bytecode with a two-byte immediate telling how many args
      	 * were pushed on the operand stack.
      	 */
      	for (pn2 = pn2->pn_next; pn2; pn2 = pn2->pn_next) {
	    if (!js_EmitTree(cx, cg, pn2))
		return JS_FALSE;
      	}
	argc = pn->pn_count - 1;
	if (js_Emit3(cx, cg, op, ARGC_HI(argc), ARGC_LO(argc)) < 0)
	    return JS_FALSE;
	break;

#if JS_HAS_INITIALIZERS
      case TOK_RB:
	/*
	 * Emit code for [a, b, c] of the form:
	 *   t = new Array; t[0] = a; t[1] = b; t[2] = c; t;
	 * but use a stack slot for t and avoid dup'ing and popping it via
	 * the JSOP_NEWINIT and JSOP_INITELEM bytecodes.
	 */
	ale = js_IndexAtom(cx, cx->runtime->atomState.ArrayAtom,
			   &cg->atomList);
	if (!ale)
	    return JS_FALSE;
	EMIT_ATOM_INDEX_OP(JSOP_NAME, ale->index);
	if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
	    return JS_FALSE;
	if (js_Emit1(cx, cg, JSOP_NEWINIT) < 0)
	    return JS_FALSE;

	pn2 = pn->pn_head;
#if JS_HAS_SHARP_VARS
	if (pn2 && pn2->pn_type == TOK_DEFSHARP) {
	    EMIT_ATOM_INDEX_OP(JSOP_DEFSHARP, (jsatomid)pn2->pn_num);
	    pn2 = pn2->pn_next;
	}
#endif

	for (atomIndex = 0; pn2; pn2 = pn2->pn_next) {
	    /* PrimaryExpr enforced ATOM_INDEX_LIMIT, so in-line optimize. */
	    PR_ASSERT(atomIndex < ATOM_INDEX_LIMIT);
	    if (atomIndex == 0) {
		if (js_Emit1(cx, cg, JSOP_ZERO) < 0)
		    return JS_FALSE;
	    } else if (atomIndex == 1) {
		if (js_Emit1(cx, cg, JSOP_ONE) < 0)
		    return JS_FALSE;
	    } else {
		EMIT_ATOM_INDEX_OP(JSOP_UINT16, (jsatomid)atomIndex);
	    }

	    /* Sub-optimal: holes in a sparse initializer are void-filled. */
	    if (pn2->pn_type == TOK_COMMA) {
		if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
		    return JS_FALSE;
	    } else {
		if (!js_EmitTree(cx, cg, pn2))
		    return JS_FALSE;
	    }
	    if (js_Emit1(cx, cg, JSOP_INITELEM) < 0)
		return JS_FALSE;

	    atomIndex++;
	}

	if (pn->pn_extra) {
	    /* Emit a source note so we know to decompile an extra comma. */
	    if (js_NewSrcNote(cx, cg, SRC_COMMA) < 0)
		return JS_FALSE;
	}

	/* Emit an op for sharp array cleanup and decompilation. */
	if (js_Emit1(cx, cg, JSOP_ENDINIT) < 0)
	    return JS_FALSE;
	break;

      case TOK_RC:
	/*
	 * Emit code for {p:a, '%q':b, 2:c} of the form:
	 *   t = new Object; t.p = a; t['%q'] = b; t[2] = c; t;
	 * but use a stack slot for t and avoid dup'ing and popping it via
	 * the JSOP_NEWINIT and JSOP_INITELEM bytecodes.
	 */
	ale = js_IndexAtom(cx, cx->runtime->atomState.ObjectAtom,
			   &cg->atomList);
	if (!ale)
	    return JS_FALSE;
	EMIT_ATOM_INDEX_OP(JSOP_NAME, ale->index);

	if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
	    return JS_FALSE;
	if (js_Emit1(cx, cg, JSOP_NEWINIT) < 0)
	    return JS_FALSE;

	pn2 = pn->pn_head;
#if JS_HAS_SHARP_VARS
	if (pn2 && pn2->pn_type == TOK_DEFSHARP) {
	    EMIT_ATOM_INDEX_OP(JSOP_DEFSHARP, (jsatomid)pn2->pn_num);
	    pn2 = pn2->pn_next;
	}
#endif

	for (; pn2; pn2 = pn2->pn_next) {
	    /* Emit an index for t[2], else map an atom for t.p or t['%q']. */
	    pn3 = pn2->pn_left;
	    switch (pn3->pn_type) {
	      case TOK_NUMBER:
		if (!EmitNumberOp(cx, pn3->pn_dval, cg))
		    return JS_FALSE;
		break;
	      case TOK_NAME:
	      case TOK_STRING:
		ale = js_IndexAtom(cx, pn3->pn_atom, &cg->atomList);
		if (!ale)
		    return JS_FALSE;
		break;
	      default:
		PR_ASSERT(0);
	    }

	    /* Emit code for the property initializer. */
	    if (!js_EmitTree(cx, cg, pn2->pn_right))
		return JS_FALSE;

	    /* Annotate JSOP_INITELEM so we decompile 2:c and not just c. */
	    if (pn3->pn_type == TOK_NUMBER) {
		if (js_NewSrcNote(cx, cg, SRC_LABEL) < 0)
		    return JS_FALSE;
		if (js_Emit1(cx, cg, JSOP_INITELEM) < 0)
		    return JS_FALSE;
	    } else {
		EMIT_ATOM_INDEX_OP(JSOP_INITPROP, ale->index);
	    }
	}

	/* Emit an op for sharpArray cleanup and decompilation. */
	if (js_Emit1(cx, cg, JSOP_ENDINIT) < 0)
	    return JS_FALSE;
	break;

#if JS_HAS_SHARP_VARS
      case TOK_DEFSHARP:
	if (!js_EmitTree(cx, cg, pn->pn_kid))
	    return JS_FALSE;
	EMIT_ATOM_INDEX_OP(JSOP_DEFSHARP, (jsatomid) pn->pn_num);
      	break;

      case TOK_USESHARP:
	EMIT_ATOM_INDEX_OP(JSOP_USESHARP, (jsatomid) pn->pn_num);
      	break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_INITIALIZERS */

      case TOK_RP:
      	/*
      	 * The node for (e) has e as its kid, enabling users who want to nest
	 * assignment expressions in conditions to avoid the error correction
	 * done by Condition (from x = y to x == y) by double-parenthesizing.
	 *
	 * We also emit an annotated NOP so we can decompile user parentheses,
	 * but that's just a nicety (the decompiler does not preserve comments
	 * or white space, and it parenthesizes for correct precedence anyway,
	 * so this nop nicety should be considered with a cold eye, especially
	 * if another srcnote type is needed).
      	 */
      	if (!js_EmitTree(cx, cg, pn->pn_kid))
	    return JS_FALSE;

	if (js_NewSrcNote(cx, cg, SRC_PAREN) < 0 ||
	    js_Emit1(cx, cg, JSOP_NOP) < 0) {
	    return JS_FALSE;
	}
      	break;

      case TOK_NAME:
      	if (pn->pn_slot >= 0) {
	    EMIT_ATOM_INDEX_OP(pn->pn_op, (jsatomid) pn->pn_slot);
	    break;
	}
	/* FALL THROUGH */
      case TOK_STRING:
      case TOK_OBJECT:
	/*
	 * The scanner and parser associate JSOP_NAME with TOK_NAME, although
	 * other bytecodes may result instead (JSOP_BINDNAME/JSOP_SETNAME2,
	 * JSOP_FORNAME2, etc.).  Among JSOP_*NAME* variants, only JSOP_NAME
	 * may generate the first operand of a call or new expression, so only
	 * it sets the "obj" virtual machine register to the object along the
	 * scope chain in which the name was found.
	 *
	 * Token types for STRING and OBJECT have corresponding bytecode ops
	 * in pn_op and emit the same format as NAME, so they share this code.
	 */
	return EmitAtomOp(cx, pn, pn->pn_op, cg);

      case TOK_NUMBER:
	return EmitNumberOp(cx, pn->pn_dval, cg);

      case TOK_PRIMARY:
	return js_Emit1(cx, cg, pn->pn_op) >= 0;

      default:
      	PR_ASSERT(0);
    }

    return JS_TRUE;
}

JS_FRIEND_DATA(const char *) js_SrcNoteName[] = {
    "null",
    "if",
    "if-else",
    "while",
    "for",
    "continue",
    "var",
    "comma",
    "assignop",
    "cond",
    "paren",
    "hidden",
    "pcbase",
    "label",
    "labelbrace",
    "endbrace",
    "break2label",
    "cont2label",
    "switch",
    "funcdef",
    "try",
    "catch",
    "newline",
    "setline",
    "xdelta"
};

uint8 js_SrcNoteArity[] = {
    0,  /* SRC_NULL */
    0,  /* SRC_IF */
    0,  /* SRC_IF_ELSE */
    0,  /* SRC_WHILE */
    3,  /* SRC_FOR */
    0,  /* SRC_CONTINUE */
    0,  /* SRC_VAR */
    1,  /* SRC_COMMA */
    0,  /* SRC_ASSIGNOP */
    0,  /* SRC_COND */
    0,  /* SRC_PAREN */
    0,  /* SRC_HIDDEN */
    1,  /* SRC_PCBASE */
    1,  /* SRC_LABEL */
    1,  /* SRC_LABELBRACE */
    0,  /* SRC_ENDBRACE */
    1,  /* SRC_BREAK2LABEL */
    1,  /* SRC_CONT2LABEL */
    1,  /* SRC_SWITCH */
    1,  /* SRC_FUNCDEF */
    0,  /* SRC_TRY */
    0,  /* SRC_CATCH */
    0,  /* SRC_NEWLINE */
    1,  /* SRC_SETLINE */
    0   /* SRC_XDELTA */
};

static intN
AllocSrcNote(JSContext *cx, JSCodeGenerator *cg)
{
    intN index;
    PRArenaPool *pool;
    size_t incr, size;

    index = cg->noteCount;
    if (index % SNINCR == 0) {
        pool = &cx->tempPool;
	incr = SNINCR * sizeof(jssrcnote);
        if (!cg->notes) {
            PR_ARENA_ALLOCATE(cg->notes, pool, incr);
        } else {
	    size = cg->noteCount * sizeof(jssrcnote);
            PR_ARENA_GROW(cg->notes, pool, size, incr);
        }
	if (!cg->notes) {
	    JS_ReportOutOfMemory(cx);
	    return -1;
	}
    }

    cg->noteCount = index + 1;
    return index;
}

intN
js_NewSrcNote(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type)
{
    intN index, n;
    jssrcnote *sn;
    ptrdiff_t offset, saveOffset, delta, xdelta;

    /*
     * Claim a note slot in cg->notes by growing it if necessary and then
     * incrementing cg->noteCount.
     */
    index = AllocSrcNote(cx, cg);
    sn = &cg->notes[index];

    /*
     * Compute delta from the last annotated bytecode's offset.  If it's too
     * big to fit in sn, allocate one or more xdelta notes and reset sn.
     */
    offset = CG_OFFSET(cg);
    saveOffset = cg->lastNoteOffset;
    delta = offset - saveOffset;
    cg->lastNoteOffset = offset;
    if (delta >= SN_DELTA_LIMIT) {
	do {
	    xdelta = PR_MIN(delta, SN_XDELTA_MASK);
	    SN_MAKE_XDELTA(sn, xdelta);
	    delta -= xdelta;
	    index = js_NewSrcNote(cx, cg, SRC_NULL);
	    if (index < 0)
		return -1;
	    sn = &cg->notes[index];
	} while (delta >= SN_DELTA_LIMIT);
    }

    /*
     * Initialize type and delta, then allocate the minimum number of notes
     * needed for type's arity.  Usually, we won't need more, but if an offset
     * does take two bytes, js_SetSrcNoteOffset will grow cg->notes.
     */
    SN_MAKE_NOTE(sn, type, delta);
    for (n = (intN)js_SrcNoteArity[type]; n > 0; n--) {
        if (js_NewSrcNote(cx, cg, SRC_NULL) < 0)
	    return -1;
    }
    return index;
}

intN
js_NewSrcNote2(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type,
	       ptrdiff_t offset)
{
    intN index;

    index = js_NewSrcNote(cx, cg, type);
    if (index >= 0) {
	if (!js_SetSrcNoteOffset(cx, cg, index, 0, offset))
	    return -1;
    }
    return index;
}

intN
js_NewSrcNote3(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type,
	       ptrdiff_t offset1, ptrdiff_t offset2)
{
    intN index;

    index = js_NewSrcNote(cx, cg, type);
    if (index >= 0) {
	if (!js_SetSrcNoteOffset(cx, cg, index, 0, offset1))
	    return -1;
	if (!js_SetSrcNoteOffset(cx, cg, index, 1, offset2))
	    return -1;
    }
    return index;
}

static JSBool
GrowSrcNotes(JSContext *cx, JSCodeGenerator *cg)
{
    PRArenaPool *pool;
    size_t incr, size;

    pool = &cx->tempPool;
    incr = SNINCR * sizeof(jssrcnote);
    size = cg->noteCount * sizeof(jssrcnote);
    PR_ARENA_GROW(cg->notes, pool, size, incr);
    if (!cg->notes) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    return JS_TRUE;
}

uintN
js_SrcNoteLength(jssrcnote *sn)
{
    intN arity;
    jssrcnote *base;

    arity = (intN)js_SrcNoteArity[SN_TYPE(sn)];
    if (!arity)
	return 1;
    for (base = sn++; --arity >= 0; sn++) {
	if (*sn & SN_2BYTE_OFFSET_FLAG)
	    sn++;
    }
    return sn - base;
}

JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, uintN which)
{
    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    PR_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    PR_ASSERT(which < js_SrcNoteArity[SN_TYPE(sn)]);
    for (sn++; which; sn++, which--) {
	if (*sn & SN_2BYTE_OFFSET_FLAG)
	    sn++;
    }
    if (*sn & SN_2BYTE_OFFSET_FLAG)
	return (ptrdiff_t)((*sn & SN_2BYTE_OFFSET_MASK) << 8) | sn[1];
    return (ptrdiff_t)*sn;
}

JSBool
js_SetSrcNoteOffset(JSContext *cx, JSCodeGenerator *cg, uintN index,
		    uintN which, ptrdiff_t offset)
{
    jssrcnote *sn;
    ptrdiff_t diff;

    if ((size_t)offset >= (size_t)(SN_2BYTE_OFFSET_FLAG << 8)) {
	ReportStatementTooLarge(cx, cg);
	return JS_FALSE;
    }

    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    sn = &cg->notes[index];
    PR_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    PR_ASSERT(which < js_SrcNoteArity[SN_TYPE(sn)]);
    for (sn++; which; sn++, which--) {
	if (*sn & SN_2BYTE_OFFSET_FLAG)
	    sn++;
    }

    /* See if the new offset requires two bytes. */
    if ((uintN)offset > (uintN)SN_2BYTE_OFFSET_MASK) {
	/* Maybe this offset was already set to a two-byte value. */
	if (!(*sn & SN_2BYTE_OFFSET_FLAG)) {
	    /* Losing, need to insert another byte for this offset. */
	    index = sn - cg->notes;
	    if (cg->noteCount++ % SNINCR == 0) {
		if (!GrowSrcNotes(cx, cg))
		    return JS_FALSE;
		sn = cg->notes + index;
	    }
	    diff = cg->noteCount - (index + 2);
	    if (diff > 0)
		memmove(sn + 2, sn + 1, diff * sizeof(jssrcnote));
	}
	*sn++ = (jssrcnote)(SN_2BYTE_OFFSET_FLAG | (offset >> 8));
    }
    *sn = (jssrcnote)offset;
    return JS_TRUE;
}

JS_FRIEND_API(jssrcnote *)
js_FinishTakingSrcNotes(JSContext *cx, JSCodeGenerator *cg)
{
    uintN count;
    jssrcnote *tmp, *final;

    count = cg->noteCount;
    tmp   = cg->notes;
    final = JS_malloc(cx, (count + 1) * sizeof(jssrcnote));
    if (!final)
	return NULL;
    memcpy(final, tmp, count * sizeof(jssrcnote));
    SN_MAKE_TERMINATOR(&final[count]);
    CG_RESET_NOTES(cg);
    return final;
}

JSBool
js_AllocTryNotes(JSContext *cx, JSCodeGenerator *cg)
{
    size_t size;

    size = (cg->treeContext.tryCount + 1) * sizeof(JSTryNote);
    PR_ARENA_ALLOCATE(cg->tryBase, &cx->tempPool, size);
    if (!cg->tryBase)
	return JS_FALSE;
    cg->tryNext = cg->tryBase;
    cg->tryLimit = (JSTryNote *)((char *)cg->tryBase + size);
    return JS_TRUE;
}

JS_FRIEND_API(JSTryNote *)
js_NewTryNote(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t start,
	      ptrdiff_t end, ptrdiff_t catch, ptrdiff_t finally)
{
    JSTryNote *cur;

    PR_ASSERT(cg->tryNext < cg->tryLimit);
    cur = cg->tryNext;
    cg->tryNext++;
    cur->start = start;
    cur->end = end;
    cur->catch = catch;
    cur->finally = finally;
    return cur;
}

JS_FRIEND_API(JSBool)
js_FinishTakingTryNotes(JSContext *cx, JSCodeGenerator *cg, JSTryNote **tryp)
{
    uintN count;
    JSTryNote *tmp, *final;

    count = cg->tryNext - cg->tryBase;
    if (!count) {
	*tryp = NULL;
	return JS_TRUE;
    }

    tmp = cg->tryBase;
    final = JS_malloc(cx, (count + 1) * sizeof(JSTryNote));
    if (!final) {
	*tryp = NULL;
	return JS_FALSE;
    }
    memcpy(final, tmp, count * sizeof(JSTryNote));
    final[count].start = 0;
    final[count].end = 0;
    final[count].catch = 0;
    final[count].finally = 0;
    *tryp = final;
    return JS_TRUE;
}
