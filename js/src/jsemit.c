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
 * JS bytecode generation.
 */
#include "jsstddef.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsbit.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"

/* Allocation grain counts, must be powers of two in general. */
#define BYTECODE_GRAIN  256     /* code allocation increment */
#define SRCNOTE_GRAIN   64      /* initial srcnote allocation increment */
#define TRYNOTE_GRAIN   64      /* trynote allocation increment */

/* Macros to compute byte sizes from typed element counts. */
#define BYTECODE_SIZE(n)        ((n) * sizeof(jsbytecode))
#define SRCNOTE_SIZE(n)         ((n) * sizeof(jssrcnote))
#define TRYNOTE_SIZE(n)         ((n) * sizeof(JSTryNote))

JS_FRIEND_API(JSBool)
js_InitCodeGenerator(JSContext *cx, JSCodeGenerator *cg,
                     const char *filename, uintN lineno,
                     JSPrincipals *principals)
{
    memset(cg, 0, sizeof *cg);
    TREE_CONTEXT_INIT(&cg->treeContext);
    cg->treeContext.flags |= TCF_COMPILING;
    cg->codeMark = JS_ARENA_MARK(&cx->codePool);
    cg->noteMark = JS_ARENA_MARK(&cx->notePool);
    cg->tempMark = JS_ARENA_MARK(&cx->tempPool);
    cg->current = &cg->main;
    cg->filename = filename;
    cg->firstLine = cg->currentLine = lineno;
    cg->principals = principals;
    ATOM_LIST_INIT(&cg->atomList);
    cg->noteMask = SRCNOTE_GRAIN - 1;
    return JS_TRUE;
}

JS_FRIEND_API(void)
js_FinishCodeGenerator(JSContext *cx, JSCodeGenerator *cg)
{
    TREE_CONTEXT_FINISH(&cg->treeContext);
    JS_ARENA_RELEASE(&cx->codePool, cg->codeMark);
    JS_ARENA_RELEASE(&cx->notePool, cg->noteMark);
    JS_ARENA_RELEASE(&cx->tempPool, cg->tempMark);
}

static ptrdiff_t
EmitCheck(JSContext *cx, JSCodeGenerator *cg, JSOp op, ptrdiff_t delta)
{
    jsbytecode *base, *limit, *next;
    ptrdiff_t offset, length;
    size_t incr, size;

    base = CG_BASE(cg);
    next = CG_NEXT(cg);
    limit = CG_LIMIT(cg);
    offset = PTRDIFF(next, base, jsbytecode);
    if ((jsuword)(next + delta) > (jsuword)limit) {
        length = offset + delta;
        length = (length <= BYTECODE_GRAIN)
                 ? BYTECODE_GRAIN
                 : JS_BIT(JS_CeilingLog2(length));
        incr = BYTECODE_SIZE(length);
        if (!base) {
            JS_ARENA_ALLOCATE_CAST(base, jsbytecode *, &cx->codePool, incr);
        } else {
            size = BYTECODE_SIZE(PTRDIFF(limit, base, jsbytecode));
            incr -= size;
            JS_ARENA_GROW_CAST(base, jsbytecode *, &cx->codePool, size, incr);
        }
        if (!base) {
            JS_ReportOutOfMemory(cx);
            return -1;
        }
        CG_BASE(cg) = base;
        CG_LIMIT(cg) = base + length;
        CG_NEXT(cg) = base + offset;
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
        nuses = 2 + GET_ARGC(pc);       /* stack: fun, this, [argc arguments] */
    cg->stackDepth -= nuses;
    if (cg->stackDepth < 0) {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%d", target);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_STACK_UNDERFLOW,
                             cg->filename ? cg->filename : "stdin", numBuf);
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
        *CG_NEXT(cg)++ = (jsbytecode)op;
        UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_Emit2(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 2);

    if (offset >= 0) {
        jsbytecode *next = CG_NEXT(cg);
        next[0] = (jsbytecode)op;
        next[1] = op1;
        CG_NEXT(cg) = next + 2;
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
        jsbytecode *next = CG_NEXT(cg);
        next[0] = (jsbytecode)op;
        next[1] = op1;
        next[2] = op2;
        CG_NEXT(cg) = next + 3;
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
        jsbytecode *next = CG_NEXT(cg);
        *next = (jsbytecode)op;
        CG_NEXT(cg) = next + length;
        UpdateDepth(cx, cg, offset);
    }
    return offset;
}

/* XXX too many "... statement" L10N gaffes below -- fix via js.msg! */
const char js_with_statement_str[] = "with statement";

static const char *statementName[] = {
    "block",                 /* BLOCK */
    "label statement",       /* LABEL */
    "if statement",          /* IF */
    "else statement",        /* ELSE */
    "switch statement",      /* SWITCH */
    js_with_statement_str,   /* WITH */
    "try statement",         /* TRY */
    "catch block",           /* CATCH */
    "finally statement",     /* FINALLY */
    "do loop",               /* DO_LOOP */
    "for loop",              /* FOR_LOOP */
    "for/in loop",           /* FOR_IN_LOOP */
    "while loop",            /* WHILE_LOOP */
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
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEED_DIET,
                         StatementName(cg));
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

JSBool
js_InWithStatement(JSTreeContext *tc)
{
    JSStmtInfo *stmt;

    for (stmt = tc->topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == STMT_WITH)
            return JS_TRUE;
    }
    return JS_FALSE;
}

JSBool
js_InCatchBlock(JSTreeContext *tc, JSAtom *atom)
{
    JSStmtInfo *stmt;

    for (stmt = tc->topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == STMT_CATCH && stmt->label == atom)
            return JS_TRUE;
    }
    return JS_FALSE;
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

/*
 * Emit a jump op with offset pointing to the previous jump of this type, so
 * that we can walk back up the chain fixing up the final destination.  Note
 * that we could encode delta as an unsigned offset, and gain a bit of delta
 * domain -- but when we backpatch, we would ReportStatementTooLarge anyway.
 * We may as well report that error now, and we must avoid storing unchecked
 * deltas here.
 */
#define EMIT_CHAINED_JUMP(cx, cg, last, op, jmp)                              \
    JS_BEGIN_MACRO                                                            \
        ptrdiff_t offset, delta;                                              \
        offset = CG_OFFSET(cg);                                               \
        delta = offset - (last);                                              \
        last = offset;                                                        \
        JS_ASSERT(delta > 0);                                                 \
        if (delta > JUMP_OFFSET_MAX) {                                        \
            ReportStatementTooLarge(cx, cg);                                  \
            jmp = -1;                                                         \
        } else {                                                              \
            jmp = js_Emit3((cx), (cg), (op), JUMP_OFFSET_HI(delta),           \
                           JUMP_OFFSET_LO(delta));                            \
        }                                                                     \
    JS_END_MACRO

/* Emit additional bytecode(s) for non-local jumps. */
static JSBool
EmitNonLocalJumpFixup(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *toStmt,
                      JSBool preserveTop)
{
    JSStmtInfo *stmt;
    ptrdiff_t jmp;

    /*
     * If we're here as part of processing a return, emit JSOP_SWAP to preserve
     * the top element.
     */
    for (stmt = cg->treeContext.topStmt; stmt != toStmt; stmt = stmt->down) {
        switch (stmt->type) {
          case STMT_FINALLY:
            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                return JS_FALSE;
            EMIT_CHAINED_JUMP(cx, cg, stmt->gosub, JSOP_GOSUB, jmp);
            if (jmp < 0)
                return JS_FALSE;
            break;
          case STMT_WITH:
          case STMT_CATCH:
            if (preserveTop) {
                if (js_Emit1(cx, cg, JSOP_SWAP) < 0)
                    return JS_FALSE;
            }
            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                return JS_FALSE;
            cg->stackDepth++;
            if (js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0)
                return JS_FALSE;
            break;
          case STMT_FOR_IN_LOOP:
            cg->stackDepth += 2;
            if (preserveTop) {
                if (js_Emit1(cx, cg, JSOP_SWAP) < 0 ||
                    js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                    js_Emit1(cx, cg, JSOP_POP) < 0 ||
                    js_Emit1(cx, cg, JSOP_SWAP) < 0 ||
                    js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                    js_Emit1(cx, cg, JSOP_POP) < 0) {
                    return JS_FALSE;
                }
            } else {
                /* JSOP_POP2 isn't decompiled, and doesn't need a src note. */
                if (js_Emit1(cx, cg, JSOP_POP2) < 0)
                    return JS_FALSE;
            }
            break;
          default:;
        }
    }

    return JS_TRUE;
}

static ptrdiff_t
EmitGoto(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *toStmt,
         ptrdiff_t *last, JSAtomListElement *label, JSSrcNoteType noteType)
{
    intN index;
    ptrdiff_t jmp;

    if (!EmitNonLocalJumpFixup(cx, cg, toStmt, JS_FALSE))
        return -1;

    if (label) {
        index = js_NewSrcNote(cx, cg, noteType);
        if (index < 0)
            return -1;
        if (!js_SetSrcNoteOffset(cx, cg, (uintN)index, 0,
                                 (ptrdiff_t) ALE_INDEX(label))) {
            return -1;
        }
    }

    EMIT_CHAINED_JUMP(cx, cg, *last, JSOP_GOTO, jmp);
    return jmp;
}

static JSBool
PatchGotos(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *stmt,
           ptrdiff_t last, jsbytecode *target, jsbytecode op)
{
    jsbytecode *pc;
    ptrdiff_t delta, jumpOffset;

    pc = CG_CODE(cg, last);
    while (pc != CG_CODE(cg, -1)) {
        JS_ASSERT(*pc == op);
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
    if (!PatchGotos(cx, cg, stmt, stmt->breaks, CG_NEXT(cg), JSOP_GOTO) ||
        !PatchGotos(cx, cg, stmt, stmt->continues, CG_CODE(cg, stmt->update),
                    JSOP_GOTO)) {
        return JS_FALSE;
    }
    js_PopStatement(&cg->treeContext);
    return JS_TRUE;
}

/*
 * Emit a bytecode and its 2-byte constant (atom) index immediate operand.
 * NB: We use cx and cg from our caller's lexical environment, and return
 * false on error.
 */
#define EMIT_ATOM_INDEX_OP(op, atomIndex)                                     \
    JS_BEGIN_MACRO                                                            \
        if (js_Emit3(cx, cg, op, ATOM_INDEX_HI(atomIndex),                    \
                                 ATOM_INDEX_LO(atomIndex)) < 0) {             \
            return JS_FALSE;                                                  \
        }                                                                     \
    JS_END_MACRO

static JSBool
EmitAtomOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSAtomListElement *ale;

    ale = js_IndexAtom(cx, pn->pn_atom, &cg->atomList);
    if (!ale)
        return JS_FALSE;
    EMIT_ATOM_INDEX_OP(op, ALE_INDEX(ale));
    return JS_TRUE;
}

/*
 * This routine tries to optimize name gets and sets to stack slot loads and
 * stores, given the variables object and scope chain in cx's top frame, the
 * compile-time context in tc, and a TOK_NAME node pn.  It returns false on
 * error, true on success.
 *
 * The caller can inspect pn->pn_slot for a non-negative slot number to tell
 * whether optimization occurred, in which case LookupArgOrVar also updated
 * pn->pn_op.  If pn->pn_slot is still -1 on return, pn->pn_op nevertheless
 * may have been optimized, e.g., from JSOP_NAME to JSOP_ARGUMENTS.  Whether
 * or not pn->pn_op was modified, if this function finds an argument or local
 * variable name, pn->pn_attrs will contain the property's attributes after a
 * successful return.
 */
static JSBool
LookupArgOrVar(JSContext *cx, JSTreeContext *tc, JSParseNode *pn)
{
    JSObject *obj, *pobj;
    JSClass *clasp;
    JSAtom *atom;
    JSScopeProperty *sprop;
    JSOp op;

    JS_ASSERT(pn->pn_type == TOK_NAME);
    if (pn->pn_slot >= 0 || pn->pn_op == JSOP_ARGUMENTS)
        return JS_TRUE;

    /*
     * We can't optimize if var and closure (a local function not in a larger
     * expression and not at top-level within another's body) collide.
     * XXX suboptimal: keep track of colliding names and deoptimize only those
     */
    if (tc->flags & TCF_FUN_CLOSURE_VS_VAR)
        return JS_TRUE;

    /*
     * We can't optimize if we're not compiling a function body, whether via
     * eval, or directly when compiling a function statement or expression.
     */
    obj = cx->fp->varobj;
    clasp = OBJ_GET_CLASS(cx, obj);
    if (clasp != &js_FunctionClass && clasp != &js_CallClass)
        return JS_TRUE;

    /*
     * We can't optimize if we're in an eval called inside a with statement,
     * or we're compiling a with statement and its body, or we're in a catch
     * block whose exception variable has the same name as pn.
     */
    atom = pn->pn_atom;
    if (cx->fp->scopeChain != obj ||
        js_InWithStatement(tc) ||
        js_InCatchBlock(tc, atom)) {
        return JS_TRUE;
    }

    /*
     * Ok, we may be able to optimize name to stack slot. Look for an argument
     * or variable property in the function, or its call object, not found in
     * any prototype object.  Rewrite pn_op and update pn accordingly.  NB: We
     * know that JSOP_DELNAME on an argument or variable must evaluate to
     * false, due to JSPROP_PERMANENT.
     */
    if (!js_LookupProperty(cx, obj, (jsid)atom, &pobj, (JSProperty **)&sprop))
        return JS_FALSE;
    op = pn->pn_op;
    if (sprop) {
        if (pobj == obj) {
            JSPropertyOp getter = SPROP_GETTER(sprop, pobj);

            if (getter == js_GetArgument) {
                switch (op) {
                  case JSOP_NAME:     op = JSOP_GETARG; break;
                  case JSOP_SETNAME:  op = JSOP_SETARG; break;
                  case JSOP_INCNAME:  op = JSOP_INCARG; break;
                  case JSOP_NAMEINC:  op = JSOP_ARGINC; break;
                  case JSOP_DECNAME:  op = JSOP_DECARG; break;
                  case JSOP_NAMEDEC:  op = JSOP_ARGDEC; break;
                  case JSOP_FORNAME:  op = JSOP_FORARG; break;
                  case JSOP_DELNAME:  op = JSOP_FALSE; break;
                  default: JS_ASSERT(0);
                }
            } else if (getter == js_GetLocalVariable ||
                       getter == js_GetCallVariable)
            {
                switch (op) {
                  case JSOP_NAME:     op = JSOP_GETVAR; break;
                  case JSOP_SETNAME:  op = JSOP_SETVAR; break;
                  case JSOP_SETCONST: op = JSOP_SETVAR; break;
                  case JSOP_INCNAME:  op = JSOP_INCVAR; break;
                  case JSOP_NAMEINC:  op = JSOP_VARINC; break;
                  case JSOP_DECNAME:  op = JSOP_DECVAR; break;
                  case JSOP_NAMEDEC:  op = JSOP_VARDEC; break;
                  case JSOP_FORNAME:  op = JSOP_FORVAR; break;
                  case JSOP_DELNAME:  op = JSOP_FALSE; break;
                  default: JS_ASSERT(0);
                }
            }
            if (op != pn->pn_op) {
                pn->pn_op = op;
                pn->pn_slot = JSVAL_TO_INT(sprop->id);
            }
            pn->pn_attrs = sprop->attrs;
        }
        OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
    }

    if (pn->pn_slot < 0) {
        /*
         * We couldn't optimize it, so it's not an arg or local var name.  Now
         * we must check for the predefined arguments variable.  It may be
         * overridden by assignment, in which case the function is heavyweight
         * and the interpreter will look up 'arguments' in the function's call
         * object.
         */
        if (pn->pn_op == JSOP_NAME &&
            atom == cx->runtime->atomState.argumentsAtom) {
            pn->pn_op = JSOP_ARGUMENTS;
            return JS_TRUE;
        }

        tc->flags |= TCF_FUN_USES_NONLOCALS;
    }
    return JS_TRUE;
}

/*
 * If pn contains a useful expression, return true with *answer set to true.
 * If pn contains a useless expression, return true with *answer set to false.
 * Return false on error.
 *
 * The caller should initialize *answer to false and invoke this function on
 * an expression statement or similar subtree to decide whether the tree could
 * produce code that has any side effects.  For an expression statement, we
 * define useless code as code with no side effects, because the main effect,
 * the value left on the stack after the code executes, will be discarded by a
 * pop bytecode.
 */
static JSBool
CheckSideEffects(JSContext *cx, JSTreeContext *tc, JSParseNode *pn,
                 JSBool *answer)
{
    JSBool ok;
    JSParseNode *pn2;

    ok = JS_TRUE;
    if (!pn || *answer)
        return ok;

    switch (pn->pn_arity) {
      case PN_FUNC:
        /*
         * A named function is presumed useful: we can't yet know that it is
         * not called.  The side effects are the creation of a scope object
         * to parent this function object, and the binding of the function's
         * name in that scope object.  See comments at case JSOP_NAMEDFUNOBJ:
         * in jsinterp.c.
         */
        if (pn->pn_fun->atom)
            *answer = JS_TRUE;
        break;

      case PN_LIST:
        if (pn->pn_type == TOK_NEW || pn->pn_type == TOK_LP) {
            /*
             * All invocation operations (construct, call) are presumed to be
             * useful, because they may have side effects even if their main
             * effect (their return value) is discarded.
             */
            *answer = JS_TRUE;
        } else {
            for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next)
                ok &= CheckSideEffects(cx, tc, pn2, answer);
        }
        break;

      case PN_TERNARY:
        ok = CheckSideEffects(cx, tc, pn->pn_kid1, answer) &&
             CheckSideEffects(cx, tc, pn->pn_kid2, answer) &&
             CheckSideEffects(cx, tc, pn->pn_kid3, answer);
        break;

      case PN_BINARY:
        if (pn->pn_type == TOK_ASSIGN) {
            /*
             * Assignment is presumed to be useful, even if the next operation
             * is another assignment overwriting this one's ostensible effect,
             * because the left operand may be a property with a setter that
             * has side effects.
             */
            *answer = JS_TRUE;
        } else {
            if (pn->pn_type == TOK_LB) {
                pn2 = pn->pn_left;
                if (pn2->pn_type == TOK_NAME && !LookupArgOrVar(cx, tc, pn2))
                    return JS_FALSE;
                if (pn2->pn_op != JSOP_ARGUMENTS) {
                    /*
                     * Any indexed property reference could call a getter with
                     * side effects, except for arguments[i] where arguments is
                     * unambiguous.
                     */
                    *answer = JS_TRUE;
                }
            }
            ok = CheckSideEffects(cx, tc, pn->pn_left, answer) &&
                 CheckSideEffects(cx, tc, pn->pn_right, answer);
        }
        break;

      case PN_UNARY:
        if (pn->pn_type == TOK_INC || pn->pn_type == TOK_DEC ||
            pn->pn_type == TOK_DELETE ||
            pn->pn_type == TOK_THROW ||
            pn->pn_type == TOK_DEFSHARP) {
            /* All these operations have effects that we must commit. */
            *answer = JS_TRUE;
        } else {
            ok = CheckSideEffects(cx, tc, pn->pn_kid, answer);
        }
        break;

      case PN_NAME:
        if (pn->pn_type == TOK_NAME) {
            if (!LookupArgOrVar(cx, tc, pn))
                return JS_FALSE;
            if (pn->pn_slot < 0 && pn->pn_op != JSOP_ARGUMENTS) {
                /*
                 * Not an argument or local variable use, so this expression
                 * could invoke a getter that has side effects.
                 */
                *answer = JS_TRUE;
            }
        }
        pn2 = pn->pn_expr;
        if (pn->pn_type == TOK_DOT && pn2->pn_type == TOK_NAME) {
            if (!LookupArgOrVar(cx, tc, pn2))
                return JS_FALSE;
            if (!(pn2->pn_op == JSOP_ARGUMENTS &&
                  pn->pn_atom == cx->runtime->atomState.lengthAtom)) {
                /*
                 * Any dotted property reference could call a getter, except
                 * for arguments.length where arguments is unambiguous.
                 */
                *answer = JS_TRUE;
            }
        }
        ok = CheckSideEffects(cx, tc, pn2, answer);
        break;

      case PN_NULLARY:
        if (pn->pn_type == TOK_DEBUGGER)
            *answer = JS_TRUE;
        break;
    }
    return ok;
}

static JSBool
EmitPropOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSParseNode *pn2;
    JSAtomListElement *ale;

    pn2 = pn->pn_expr;
    if (op == JSOP_GETPROP &&
        pn->pn_type == TOK_DOT &&
        pn2->pn_type == TOK_NAME) {
        /* Try to optimize arguments.length into JSOP_ARGCNT. */
        if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
            return JS_FALSE;
        if (pn2->pn_op == JSOP_ARGUMENTS &&
            pn->pn_atom == cx->runtime->atomState.lengthAtom) {
            return js_Emit1(cx, cg, JSOP_ARGCNT) >= 0;
        }
    }
    if (!js_EmitTree(cx, cg, pn2))
        return JS_FALSE;
    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
                       (ptrdiff_t)(CG_OFFSET(cg) - pn2->pn_offset)) < 0) {
        return JS_FALSE;
    }
    if (!pn->pn_atom) {
        JS_ASSERT(op == JSOP_IMPORTALL);
        if (js_Emit1(cx, cg, op) < 0)
            return JS_FALSE;
    } else {
        ale = js_IndexAtom(cx, pn->pn_atom, &cg->atomList);
        if (!ale)
            return JS_FALSE;
        EMIT_ATOM_INDEX_OP(op, ALE_INDEX(ale));
    }
    return JS_TRUE;
}

static JSBool
EmitElemOp(JSContext *cx, JSParseNode *pn, JSOp op, JSCodeGenerator *cg)
{
    JSParseNode *left, *right;
    jsint slot;

    left = pn->pn_left;
    right = pn->pn_right;
    if (op == JSOP_GETELEM &&
        left->pn_type == TOK_NAME &&
        right->pn_type == TOK_NUMBER) {
        /* Try to optimize arguments[0] into JSOP_ARGSUB. */
        if (!LookupArgOrVar(cx, &cg->treeContext, left))
            return JS_FALSE;
        if (left->pn_op == JSOP_ARGUMENTS &&
            JSDOUBLE_IS_INT(right->pn_dval, slot) &&
            slot >= 0) {
            EMIT_ATOM_INDEX_OP(JSOP_ARGSUB, (jsatomid)slot);
            return JS_TRUE;
        }
    }
    if (!js_EmitTree(cx, cg, left) || !js_EmitTree(cx, cg, right))
        return JS_FALSE;
    if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
                       (ptrdiff_t)(CG_OFFSET(cg) - left->pn_offset)) < 0) {
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
    EMIT_ATOM_INDEX_OP(JSOP_NUMBER, ALE_INDEX(ale));
    return JS_TRUE;
}

JSBool
js_EmitFunctionBody(JSContext *cx, JSCodeGenerator *cg, JSParseNode *body,
                    JSFunction *fun)
{
    JSStackFrame *fp, frame;
    JSObject *funobj;
    JSBool ok;

    if (!js_AllocTryNotes(cx, cg))
        return JS_FALSE;

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
    ok = js_EmitTree(cx, cg, body);
    cx->fp = fp;
    if (!ok)
        return JS_FALSE;

    fun->script = js_NewScriptFromCG(cx, cg, fun);
    if (!fun->script)
        return JS_FALSE;
    if (cg->treeContext.flags & TCF_FUN_HEAVYWEIGHT)
        fun->flags |= JSFUN_HEAVYWEIGHT;
    return JS_TRUE;
}

/* A macro for inlining at the top of js_EmitTree (whence it came). */
#define UPDATE_LINENO_NOTES(cx, cg, pn)                                       \
    JS_BEGIN_MACRO                                                            \
        uintN _line = (pn)->pn_pos.begin.lineno;                              \
        uintN _delta = _line - (cg)->currentLine;                             \
        (cg)->currentLine = _line;                                            \
        if (_delta) {                                                         \
            /*                                                                \
             * Encode any change in the current source line number by using   \
             * either several SRC_NEWLINE notes or one SRC_SETLINE note,      \
             * whichever consumes less space.                                 \
             */                                                               \
            if (_delta >= (uintN)(2 + ((_line > SN_3BYTE_OFFSET_MASK)<<1))) { \
                if (js_NewSrcNote2(cx, cg, SRC_SETLINE, (ptrdiff_t)_line) < 0)\
                    return JS_FALSE;                                          \
            } else {                                                          \
                do {                                                          \
                    if (js_NewSrcNote(cx, cg, SRC_NEWLINE) < 0)               \
                        return JS_FALSE;                                      \
                } while (--_delta != 0);                                      \
            }                                                                 \
        }                                                                     \
    JS_END_MACRO

/* A function, so that we avoid macro-bloating all the other callsites. */
static JSBool
UpdateLinenoNotes(JSContext *cx, JSCodeGenerator *cg, JSParseNode *pn)
{
    UPDATE_LINENO_NOTES(cx, cg, pn);
    return JS_TRUE;
}

JSBool
js_EmitTree(JSContext *cx, JSCodeGenerator *cg, JSParseNode *pn)
{
    JSBool ok, useful;
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
    UPDATE_LINENO_NOTES(cx, cg, pn);

    switch (pn->pn_type) {
      case TOK_FUNCTION:
      {
        JSFunction *fun;

        /* Generate code for the function's body. */
        pn2 = pn->pn_body;
        if (!js_InitCodeGenerator(cx, &cg2, cg->filename,
                                  pn->pn_pos.begin.lineno,
                                  cg->principals)) {
            return JS_FALSE;
        }
        cg2.treeContext.flags = pn->pn_flags | TCF_IN_FUNCTION;
        cg2.treeContext.tryCount = pn->pn_tryCount;
        fun = pn->pn_fun;
        if (!js_EmitFunctionBody(cx, &cg2, pn2, fun))
            return JS_FALSE;

        /*
         * We need an activation object if an inner peeks out, or if such
         * inner-peeking caused one of our inners to become heavyweight.
         */
        if (cg2.treeContext.flags &
            (TCF_FUN_USES_NONLOCALS | TCF_FUN_HEAVYWEIGHT)) {
            cg->treeContext.flags |= TCF_FUN_HEAVYWEIGHT;
        }
        js_FinishCodeGenerator(cx, &cg2);

        /* Make the function object a literal in the outer script's pool. */
        atom = js_AtomizeObject(cx, fun->object, 0);
        if (!atom)
            return JS_FALSE;
        ale = js_IndexAtom(cx, atom, &cg->atomList);
        if (!ale)
            return JS_FALSE;
        atomIndex = ALE_INDEX(ale);

#if JS_HAS_LEXICAL_CLOSURE
        /* Emit a bytecode pointing to the closure object in its immediate. */
        if (pn->pn_op != JSOP_NOP) {
            EMIT_ATOM_INDEX_OP(pn->pn_op, atomIndex);
            break;
        }
#endif

        /* Top-level named functions need a nop for decompilation. */
        noteIndex = js_NewSrcNote2(cx, cg, SRC_FUNCDEF, (ptrdiff_t)atomIndex);
        if (noteIndex < 0 ||
            js_Emit1(cx, cg, JSOP_NOP) < 0) {
            return JS_FALSE;
        }

        /*
         * Top-levels also need a prolog op to predefine their names in the
         * variable object, or if local, to fill their stack slots.
         */
        CG_SWITCH_TO_PROLOG(cg);
#if JS_HAS_LEXICAL_CLOSURE
        if (cg->treeContext.flags & TCF_IN_FUNCTION) {
            JSObject *obj, *pobj;
            JSScopeProperty *sprop;
            uintN slot;
            jsbytecode *pc;

            obj = OBJ_GET_PARENT(cx, pn->pn_fun->object);
            if (!js_LookupProperty(cx, obj, (jsid)fun->atom, &pobj,
                                   (JSProperty **)&sprop)) {
                return JS_FALSE;
            }
            JS_ASSERT(sprop && pobj == obj);
            slot = (uintN) JSVAL_TO_INT(sprop->id);
            OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);

            /* Emit [JSOP_DEFLOCALFUN, local variable slot, atomIndex]. */
            off = js_EmitN(cx, cg, JSOP_DEFLOCALFUN, VARNO_LEN+ATOM_INDEX_LEN);
            if (off < 0)
                return JS_FALSE;
            pc = CG_CODE(cg, off);
            SET_VARNO(pc, slot);
            pc += VARNO_LEN;
            SET_ATOM_INDEX(pc, atomIndex);
        } else
#endif
            EMIT_ATOM_INDEX_OP(JSOP_DEFFUN, atomIndex);
        CG_SWITCH_TO_MAIN(cg);

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
             * properties of the variables object to flag as exported.
             */
            do {
                ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                EMIT_ATOM_INDEX_OP(JSOP_EXPORTNAME, ALE_INDEX(ale));
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
        uint32 ncases, tablen = 0;
        JSScript *script;
        jsint i, low, high;
        jsdouble d;
        size_t switchsize, tablesize;
        void *mark;
        JSParseNode **table;
        jsbytecode *pc;
        JSBool hasDefault = JS_FALSE;
        JSBool isEcmaSwitch = cx->version == JSVERSION_DEFAULT ||
                              cx->version >= JSVERSION_1_4;
        ptrdiff_t defaultOffset = -1;

        /* Try for most optimal, fall back if not dense ints, and per ECMAv2. */
        switchop = JSOP_TABLESWITCH;

        /* Emit code for the discriminant first. */
        if (!js_EmitTree(cx, cg, pn->pn_kid1))
            return JS_FALSE;

        /* Switch bytecodes run from here till end of final case. */
        top = CG_OFFSET(cg);
        js_PushStatement(&cg->treeContext, &stmtInfo, STMT_SWITCH, top);

        pn2 = pn->pn_kid2;
        ncases = pn2->pn_count;

        ok = JS_TRUE;
        if (ncases == 0 ||
            (ncases == 1 &&
             (hasDefault = (pn2->pn_head->pn_type == TOK_DEFAULT)))) {
            ncases = 0;
            low = 0;
            high = -1;
        } else {
#define INTMAP_LENGTH   256
            jsbitmap intmap_space[INTMAP_LENGTH];
            jsbitmap *intmap = NULL;
            int32 intmap_bitlen = 0;

            low  = JSVAL_INT_MAX;
            high = JSVAL_INT_MIN;
            cg2.current = NULL;
            mark = JS_ARENA_MARK(&cx->tempPool);

            for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                if (pn3->pn_type == TOK_DEFAULT) {
                    hasDefault = JS_TRUE;
                    ncases--;   /* one of the "cases" was the default */
                    continue;
                }
                JS_ASSERT(pn3->pn_type == TOK_CASE);
                pn4 = pn3->pn_left;
                if (isEcmaSwitch) {
                    if (switchop == JSOP_CONDSWITCH)
                        continue;
                    switch (pn4->pn_type) {
                      case TOK_NUMBER:
                        d = pn4->pn_dval;
                        if (JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i)) {
                            pn3->pn_val = INT_TO_JSVAL(i);
                        } else {
                            atom = js_AtomizeDouble(cx, d, 0);
                            if (!atom) {
                                ok = JS_FALSE;
                                goto release;
                            }
                            pn3->pn_val = ATOM_KEY(atom);
                        }
                        break;
                      case TOK_STRING:
                        pn3->pn_val = ATOM_KEY(pn4->pn_atom);
                        break;
                      case TOK_PRIMARY:
                        if (pn4->pn_op == JSOP_TRUE) {
                            pn3->pn_val = JSVAL_TRUE;
                            break;
                        }
                        if (pn4->pn_op == JSOP_FALSE) {
                            pn3->pn_val = JSVAL_FALSE;
                            break;
                        }
                        /* FALL THROUGH */
                      default:
                        switchop = JSOP_CONDSWITCH;
                        continue;
                    }
                } else {
                    /* Pre-ECMAv2 switch evals case exprs at compile time. */
                    ok = js_InitCodeGenerator(cx, &cg2, cg->filename,
                                              pn3->pn_pos.begin.lineno,
                                              cg->principals);
                    if (!ok)
                        goto release;
                    cg2.currentLine = pn4->pn_pos.begin.lineno;
                    ok = js_EmitTree(cx, &cg2, pn4);
                    if (!ok)
                        goto release;
                    if (js_Emit1(cx, &cg2, JSOP_POPV) < 0) {
                        ok = JS_FALSE;
                        goto release;
                    }
                    script = js_NewScriptFromCG(cx, &cg2, NULL);
                    if (!script) {
                        ok = JS_FALSE;
                        goto release;
                    }
                    ok = js_Execute(cx, cx->fp->scopeChain, script, cx->fp, 0,
                                    &pn3->pn_val);
                    js_DestroyScript(cx, script);
                    if (!ok)
                        goto release;
                }

                if (!JSVAL_IS_NUMBER(pn3->pn_val) &&
                    !JSVAL_IS_STRING(pn3->pn_val) &&
                    !JSVAL_IS_BOOLEAN(pn3->pn_val)) {
                    cg->currentLine = pn3->pn_pos.begin.lineno;
                    js_ReportCompileErrorNumber(cx, NULL, cg, JSREPORT_ERROR,
                                                JSMSG_BAD_CASE);
                    ok = JS_FALSE;
                    goto release;
                }

                if (switchop != JSOP_TABLESWITCH)
                    continue;
                if (!JSVAL_IS_INT(pn3->pn_val)) {
                    switchop = JSOP_LOOKUPSWITCH;
                    continue;
                }
                i = JSVAL_TO_INT(pn3->pn_val);
                if ((jsuint)(i + (jsint)JS_BIT(15)) >= (jsuint)JS_BIT(16)) {
                    switchop = JSOP_LOOKUPSWITCH;
                    continue;
                }
                if (i < low)
                    low = i;
                if (high < i)
                    high = i;

                /*
                 * Check for duplicates, which require a JSOP_LOOKUPSWITCH.
                 * We bias i by 65536 if it's negative, and hope that's a rare
                 * case (because it requires a malloc'd bitmap).
                 */
                if (i < 0)
                    i += JS_BIT(16);
                if (i >= intmap_bitlen) {
                    if (!intmap &&
                        i < (INTMAP_LENGTH << JS_BITS_PER_WORD_LOG2)) {
                        intmap = intmap_space;
                        intmap_bitlen = INTMAP_LENGTH << JS_BITS_PER_WORD_LOG2;
                    } else {
                        /* Just grab 8K for the worst-case bitmap. */
                        intmap_bitlen = JS_BIT(16);
                        JS_ARENA_ALLOCATE_CAST(intmap, jsbitmap *,
                                               &cx->tempPool,
                                               (JS_BIT(16) >>
                                                JS_BITS_PER_WORD_LOG2)
                                               * sizeof(jsbitmap));
                        if (!intmap) {
                            JS_ReportOutOfMemory(cx);
                            ok = JS_FALSE;
                            goto release;
                        }
                    }
                    memset(intmap, 0, intmap_bitlen >> JS_BITS_PER_BYTE_LOG2);
                }
                if (JS_TEST_BIT(intmap, i)) {
                    switchop = JSOP_LOOKUPSWITCH;
                    continue;
                }
                JS_SET_BIT(intmap, i);
            }

          release:
            JS_ARENA_RELEASE(&cx->tempPool, mark);
            if (!ok)
                return JS_FALSE;

            if (switchop == JSOP_CONDSWITCH) {
                JS_ASSERT(!cg2.current);
            } else {
                if (cg2.current)
                    js_FinishCodeGenerator(cx, &cg2);
                if (switchop == JSOP_TABLESWITCH) {
                    tablen = (uint32)(high - low + 1);
                    if (tablen >= JS_BIT(16) || tablen > 2 * ncases)
                        switchop = JSOP_LOOKUPSWITCH;
                }
            }
        }

        /*
         * Emit a note with two offsets: first tells total switch code length,
         * second tells offset to first JSOP_CASE if condswitch.
         */
        noteIndex = js_NewSrcNote3(cx, cg, SRC_SWITCH, 0, 0);
        if (noteIndex < 0)
            return JS_FALSE;

        if (switchop == JSOP_CONDSWITCH) {
            /*
             * 0 bytes of immediate for unoptimized ECMAv2 switch.
             */
            switchsize = 0;
        } else if (switchop == JSOP_TABLESWITCH) {
            /*
             * 3 offsets (len, low, high) before the table, 1 per entry.
             */
            switchsize = (size_t)(JUMP_OFFSET_LEN * (3 + tablen));
        } else {
            /*
             * JSOP_LOOKUPSWITCH:
             * 1 offset (len) and 1 atom index (npairs) before the table,
             * 1 atom index and 1 jump offset per entry.
             */
            switchsize = (size_t)(JUMP_OFFSET_LEN + ATOM_INDEX_LEN +
                                  (ATOM_INDEX_LEN + JUMP_OFFSET_LEN) * ncases);
        }

        /* Emit switchop and switchsize bytes of jump or lookup table. */
        if (js_EmitN(cx, cg, switchop, switchsize) < 0)
            return JS_FALSE;

        off = -1;
        if (switchop == JSOP_CONDSWITCH) {
            intN caseNoteIndex = -1;

            /* Emit code for evaluating cases and jumping to case statements. */
            for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                pn4 = pn3->pn_left;
                if (pn4 && !js_EmitTree(cx, cg, pn4))
                    return JS_FALSE;
                if (caseNoteIndex >= 0) {
                    /* off is the previous JSOP_CASE's bytecode offset. */
                    if (!js_SetSrcNoteOffset(cx, cg, (uintN)caseNoteIndex, 0,
                                             CG_OFFSET(cg) - off)) {
                        return JS_FALSE;
                    }
                }
                if (pn3->pn_type == TOK_DEFAULT)
                    continue;
                caseNoteIndex = js_NewSrcNote2(cx, cg, SRC_PCDELTA, 0);
                if (caseNoteIndex < 0)
                    return JS_FALSE;
                off = js_Emit3(cx, cg, JSOP_CASE, 0, 0);
                if (off < 0)
                    return JS_FALSE;
                pn3->pn_offset = off;
                if (pn3 == pn2->pn_head) {
                    /* Switch note's second offset is to first JSOP_CASE. */
                    if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 1,
                                             off - top)) {
                        return JS_FALSE;
                    }
                }
            }

            /* Emit default even if no explicit default statement. */
            defaultOffset = js_Emit3(cx, cg, JSOP_DEFAULT, 0, 0);
            if (defaultOffset < 0)
                return JS_FALSE;
        }

        /* Emit code for each case's statements, copying pn_offset up to pn3. */
        for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            if (switchop == JSOP_CONDSWITCH && pn3->pn_type != TOK_DEFAULT) {
                pn3->pn_val = INT_TO_JSVAL(pn3->pn_offset - top);
                CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, pn3->pn_offset);
            }
            pn4 = pn3->pn_right;
            if (!js_EmitTree(cx, cg, pn4))
                return JS_FALSE;
            pn3->pn_offset = pn4->pn_offset;
            if (pn3->pn_type == TOK_DEFAULT)
                off = pn3->pn_offset - top;
        }

        if (!hasDefault) {
            /* If no default case, offset for default is to end of switch. */
            off = CG_OFFSET(cg) - top;
        }

        /* We better have set "off" by now. */
        JS_ASSERT(off != -1);

        /* Set the default offset (to end of switch if no default). */
        pc = NULL;
        if (switchop == JSOP_CONDSWITCH) {
            JS_ASSERT(defaultOffset != -1);
            if (!js_SetJumpOffset(cx, cg, CG_CODE(cg, defaultOffset),
                                  off - (defaultOffset - top))) {
                return JS_FALSE;
            }
        } else {
            pc = CG_CODE(cg, top);
            if (!js_SetJumpOffset(cx, cg, pc, off))
                return JS_FALSE;
            pc += JUMP_OFFSET_LEN;
        }

        /* Set the SRC_SWITCH note's offset operand to tell end of switch. */
        off = CG_OFFSET(cg) - top;
        if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, off))
            return JS_FALSE;

        if (switchop == JSOP_TABLESWITCH) {
            /* Fill in jump table. */
            if (!js_SetJumpOffset(cx, cg, pc, low))
                return JS_FALSE;
            pc += JUMP_OFFSET_LEN;
            if (!js_SetJumpOffset(cx, cg, pc, high))
                return JS_FALSE;
            pc += JUMP_OFFSET_LEN;
            if (tablen) {
                /* Avoid bloat for a compilation unit with many switches. */
                mark = JS_ARENA_MARK(&cx->tempPool);
                tablesize = (size_t)tablen * sizeof *table;
                JS_ARENA_ALLOCATE_CAST(table, JSParseNode **, &cx->tempPool,
                                       tablesize);
                if (!table) {
                    JS_ReportOutOfMemory(cx);
                    return JS_FALSE;
                }
                memset(table, 0, tablesize);
                for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                    if (pn3->pn_type == TOK_DEFAULT)
                        continue;
                    i = JSVAL_TO_INT(pn3->pn_val);
                    i -= low;
                    JS_ASSERT((uint32)i < tablen);
                    table[i] = pn3;
                }
                for (i = 0; i < (jsint)tablen; i++) {
                    pn3 = table[i];
                    off = pn3 ? pn3->pn_offset - top : 0;
                    if (!js_SetJumpOffset(cx, cg, pc, off))
                        return JS_FALSE;
                    pc += JUMP_OFFSET_LEN;
                }
                JS_ARENA_RELEASE(&cx->tempPool, mark);
            }
        } else if (switchop == JSOP_LOOKUPSWITCH) {
            /* Fill in lookup table. */
            SET_ATOM_INDEX(pc, ncases);
            pc += ATOM_INDEX_LEN;

            for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                if (pn3->pn_type == TOK_DEFAULT)
                    continue;
                atom = js_AtomizeValue(cx, pn3->pn_val, 0);
                if (!atom)
                    return JS_FALSE;
                ale = js_IndexAtom(cx, atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                SET_ATOM_INDEX(pc, ALE_INDEX(ale));
                pc += ATOM_INDEX_LEN;

                off = pn3->pn_offset - top;
                if (!js_SetJumpOffset(cx, cg, pc, off))
                    return JS_FALSE;
                pc += JUMP_OFFSET_LEN;
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
                JS_ASSERT(pn3->pn_type == TOK_NAME);
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

            /* Compile a JSOP_FOR* bytecode based on the left hand side. */
            switch (pn3->pn_type) {
              case TOK_VAR:
                pn3 = pn3->pn_head;
                if (js_NewSrcNote(cx, cg, SRC_VAR) < 0)
                    return JS_FALSE;
                /* FALL THROUGH */
              case TOK_NAME:
                pn3->pn_op = JSOP_FORNAME;
                if (!LookupArgOrVar(cx, &cg->treeContext, pn3))
                    return JS_FALSE;
                op = pn3->pn_op;
                if (pn3->pn_slot >= 0) {
                    if (pn3->pn_attrs & JSPROP_READONLY)
                        op = JSOP_GETVAR;
                    atomIndex = (jsatomid) pn3->pn_slot;
                    EMIT_ATOM_INDEX_OP(op, atomIndex);
                } else {
                    if (!EmitAtomOp(cx, pn3, op, cg))
                        return JS_FALSE;
                }
                break;
              case TOK_DOT:
                if (!EmitPropOp(cx, pn3, JSOP_FORPROP, cg))
                    return JS_FALSE;
                break;
              case TOK_LB:
                /*
                 * We separate the first/next bytecode from the enumerator
                 * variable binding to avoid any side-effects in the index
                 * expression (e.g., for (x[i++] in {}) should not bind x[i]
                 * or increment i at all).
                 */
                if (!js_Emit1(cx, cg, JSOP_FORELEM))
                    return JS_FALSE;
                beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
                if (beq < 0)
                    return JS_FALSE;
                if (!EmitElemOp(cx, pn3, JSOP_ENUMELEM, cg))
                    return JS_FALSE;
                break;
              default:
                JS_ASSERT(0);
            }
            if (pn3->pn_type != TOK_LB) {
                /* Pop and test the loop condition generated by JSOP_FOR*. */
                beq = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
                if (beq < 0)
                    return JS_FALSE;
            }
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
                if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, 0))
                    return JS_FALSE;
                beq = 0;
            } else {
                if (!js_EmitTree(cx, cg, pn2->pn_kid2))
                    return JS_FALSE;
                if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0,
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
            JS_ASSERT(noteIndex != -1);
            if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 1,
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
            if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 2,
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
            if (pn3->pn_type != TOK_LB) {
                if (js_Emit1(cx, cg, JSOP_POP2) < 0)
                    return JS_FALSE;
            } else {
                /*
                 * With 'for(x[i]...)', there's only the object on the stack,
                 * so we need to hide the pop.
                 */
                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                    return JS_FALSE;
                if (js_Emit1(cx, cg, JSOP_POP) < 0)
                    return JS_FALSE;
            }
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
            /* Find the loop statement enclosed by the matching label. */
            JSStmtInfo *loop = NULL;
            ale = js_IndexAtom(cx, atom, &cg->atomList);
            if (!ale)
                return JS_FALSE;
            while (stmt->type != STMT_LABEL || stmt->label != atom) {
                if (STMT_IS_LOOP(stmt))
                    loop = stmt;
                stmt = stmt->down;
            }
            stmt = loop;
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
        ptrdiff_t start, end;
        ptrdiff_t catchStart = -1, finallyCatch = -1, catchjmp = -1;
        JSParseNode *iter;
        uint16 depth;

/* Emit JSOP_GOTO that points to the first op after the catch/finally blocks */
#define EMIT_CATCH_GOTO(cx, cg, jmp)                                          \
    EMIT_CHAINED_JUMP(cx, cg, stmtInfo.catchJump, JSOP_GOTO, jmp);

/* Emit JSOP_GOSUB that points to the finally block. */
#define EMIT_FINALLY_GOSUB(cx, cg, jmp)                                       \
    EMIT_CHAINED_JUMP(cx, cg, stmtInfo.gosub, JSOP_GOSUB, jmp);

        /*
         * Push stmtInfo to track jumps-over-catches and gosubs-to-finally
         * for later fixup.
         *
         * When a finally block is `active' (STMT_FINALLY on the treeContext),
         * non-local jumps (including jumps-over-catches) result in a GOSUB
         * being written into the bytecode stream and fixed-up later (c.f.
         * EMIT_CHAINED_JUMP and PatchGotos).
         */
        js_PushStatement(&cg->treeContext, &stmtInfo,
                         pn->pn_kid3 ? STMT_FINALLY : STMT_BLOCK,
                         CG_OFFSET(cg));

        /*
         * About JSOP_SETSP:
         * An exception can be thrown while the stack is in an unbalanced
         * state, and this causes problems with things like function invocation
         * later on.
         *
         * To fix this, we compute the `balanced' stack depth upon try entry,
         * and then restore the stack to this depth when we hit the first catch
         * or finally block.  We can't just zero the stack, because things like
         * for/in and with that are active upon entry to the block keep state
         * variables on the stack.
         */
        depth = cg->stackDepth;

        /* Mark try location for decompilation, then emit try block. */
        if (js_Emit1(cx, cg, JSOP_TRY) < 0)
            return JS_FALSE;
        start = CG_OFFSET(cg);
        if (!js_EmitTree(cx, cg, pn->pn_kid1))
            return JS_FALSE;

        /* Emit (hidden) jump over catch and/or finally. */
        if (pn->pn_kid3) {
            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                return JS_FALSE;
            EMIT_FINALLY_GOSUB(cx, cg, jmp);
            if (jmp < 0)
                return JS_FALSE;
        }

        if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
            return JS_FALSE;
        EMIT_CATCH_GOTO(cx, cg, jmp);
        if (jmp < 0)
            return JS_FALSE;

        end = CG_OFFSET(cg);

        /* If this try has a catch block, emit it. */
        iter = pn->pn_kid2;
        if (iter) {
            catchStart = end;

            /*
             * The emitted code for a catch block looks like:
             *
             * [ popscope ]                        only if 2nd+ catch block
             * name Object
             * pushobj
             * newinit
             * exception
             * initcatchvar <atom>
             * enterwith
             * [< catchguard code >]               if there's a catchguard
             * [ifeq <offset to next catch block>]         " "
             * < catch block contents >
             * leavewith
             * goto <end of catch blocks>          non-local; finally applies
             *
             * If there's no catch block without a catchguard, the last
             * <offset to next catch block> points to rethrow code.  This
             * code will GOSUB to the finally code if appropriate, and is
             * also used for the catch-all trynote for capturing exceptions
             * thrown from catch{} blocks.
             */
            for (;;) {
                JSStmtInfo stmtInfo2;
                JSParseNode *disc;
                ptrdiff_t guardnote;

                if (!UpdateLinenoNotes(cx, cg, iter))
                    return JS_FALSE;

                if (catchjmp != -1) {
                    /* Fix up and clean up previous catch block. */
                    CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, catchjmp);
                    if ((uintN)++cg->stackDepth > cg->maxStackDepth)
                        cg->maxStackDepth = cg->stackDepth;
                    if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                        js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0) {
                        return JS_FALSE;
                    }
                } else {
                    /* Set stack to original depth (see SETSP comment above). */
                    EMIT_ATOM_INDEX_OP(JSOP_SETSP, (jsatomid)depth);
                }

                /* Non-zero guardnote is length of catchguard. */
                guardnote = js_NewSrcNote2(cx, cg, SRC_CATCH, 0);
                if (guardnote < 0 || js_Emit1(cx, cg, JSOP_NOP) < 0)
                    return JS_FALSE;

                /* Construct the scope holder and push it on. */
                ale = js_IndexAtom(cx, cx->runtime->atomState.ObjectAtom,
                                   &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                EMIT_ATOM_INDEX_OP(JSOP_NAME, ALE_INDEX(ale));

                if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0 ||
                    js_Emit1(cx, cg, JSOP_NEWINIT) < 0 ||
                    js_Emit1(cx, cg, JSOP_EXCEPTION) < 0) {
                    return JS_FALSE;
                }

                /* initcatchvar <atomIndex> */
                disc = iter->pn_kid1;
                ale = js_IndexAtom(cx, disc->pn_atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;

                EMIT_ATOM_INDEX_OP(JSOP_INITCATCHVAR, ALE_INDEX(ale));
                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                    js_Emit1(cx, cg, JSOP_ENTERWITH) < 0) {
                    return JS_FALSE;
                }

                /* boolean_expr */
                if (disc->pn_expr) {
                    ptrdiff_t guardstart = CG_OFFSET(cg);
                    if (!js_EmitTree(cx, cg, disc->pn_expr))
                        return JS_FALSE;
                    /* ifeq <next block> */
                    catchjmp = js_Emit3(cx, cg, JSOP_IFEQ, 0, 0);
                    if (catchjmp < 0)
                        return JS_FALSE;
                    if (!js_SetSrcNoteOffset(cx, cg, guardnote, 0,
                                             (ptrdiff_t)CG_OFFSET(cg) -
                                             guardstart)) {
                        return JS_FALSE;
                    }
                }

                /* Emit catch block. */
                js_PushStatement(&cg->treeContext, &stmtInfo2, STMT_CATCH,
                                 CG_OFFSET(cg));
                stmtInfo2.label = disc->pn_atom;
                if (!js_EmitTree(cx, cg, iter->pn_kid3))
                    return JS_FALSE;
                js_PopStatementCG(cx, cg);

                /*
                 * Jump over the remaining catch blocks.
                 * This counts as a non-local jump, so do the finally thing.
                 */

                /* popscope */
                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                    js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0) {
                    return JS_FALSE;
                }

                /* gosub <finally>, if required */
                if (pn->pn_kid3) {
                    EMIT_FINALLY_GOSUB(cx, cg, jmp);
                    if (jmp < 0)
                        return JS_FALSE;
                }

                /* This will get fixed up to jump to after catch/finally. */
                if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0)
                    return JS_FALSE;
                EMIT_CATCH_GOTO(cx, cg, jmp);
                if (jmp < 0)
                    return JS_FALSE;
                if (!iter->pn_kid2)     /* leave iter at last catch */
                    break;
                iter = iter->pn_kid2;
            }
        }

        /*
         * We use a [leavewith],[gosub],rethrow block for rethrowing
         * when there's no unguarded catch, and also for running finally
         * code while letting an uncaught exception pass through.
         */
        if (pn->pn_kid3 ||
            (catchjmp != -1 && iter->pn_kid1->pn_expr)) {
            /*
             * Emit another stack fix, because the catch could itself
             * throw an exception in an unbalanced state, and the finally
             * may need to call functions etc.
             */
            EMIT_ATOM_INDEX_OP(JSOP_SETSP, (jsatomid)depth);

            if (catchjmp != -1 && iter->pn_kid1->pn_expr)
                CHECK_AND_SET_JUMP_OFFSET_AT(cx, cg, catchjmp);

            /* Last discriminant jumps to rethrow if none match. */
            if ((uintN)++cg->stackDepth > cg->maxStackDepth)
                cg->maxStackDepth = cg->stackDepth;
            if (pn->pn_kid2 &&
                (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                 js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0)) {
                return JS_FALSE;
            }

            if (pn->pn_kid3) {
                finallyCatch = CG_OFFSET(cg);
                EMIT_FINALLY_GOSUB(cx, cg, jmp);
                if (jmp < 0)
                    return JS_FALSE;
            }
            if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                js_Emit1(cx, cg, JSOP_EXCEPTION) < 0 ||
                js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
                js_Emit1(cx, cg, JSOP_THROW) < 0) {
                return JS_FALSE;
            }
        }

        /*
         * If we have a finally, it belongs here, and we have to fix up the
         * gosubs that might have been emitted before non-local jumps.
         */
        if (pn->pn_kid3) {
            if (!PatchGotos(cx, cg, &stmtInfo, stmtInfo.gosub, CG_NEXT(cg),
                            JSOP_GOSUB)) {
                return JS_FALSE;
            }
            js_PopStatementCG(cx, cg);
            if (!UpdateLinenoNotes(cx, cg, pn->pn_kid3))
                return JS_FALSE;
            if (js_Emit1(cx, cg, JSOP_FINALLY) < 0 ||
                !js_EmitTree(cx, cg, pn->pn_kid3) ||
                js_Emit1(cx, cg, JSOP_RETSUB) < 0) {
                return JS_FALSE;
            }
        } else {
            js_PopStatementCG(cx, cg);
        }

        if (js_NewSrcNote(cx, cg, SRC_ENDBRACE) < 0 ||
            js_Emit1(cx, cg, JSOP_NOP) < 0) {
            return JS_FALSE;
        }

        /* Fix up the end-of-try/catch jumps to come here. */
        if (!PatchGotos(cx, cg, &stmtInfo, stmtInfo.catchJump, CG_NEXT(cg),
                        JSOP_GOTO)) {
            return JS_FALSE;
        }

        /*
         * Add the try note last, to let post-order give us the right ordering
         * (first to last, inner to outer).
         */
        if (pn->pn_kid2) {
            JS_ASSERT(catchStart != -1);
            if (!js_NewTryNote(cx, cg, start, end, catchStart))
                return JS_FALSE;
        }

        /*
         * If we've got a finally, mark try+catch region with additional
         * trynote to catch exceptions (re)thrown from a catch block or
         * for the try{}finally{} case.
         */
        if (pn->pn_kid3) {
            JS_ASSERT(finallyCatch != -1);
            if (!js_NewTryNote(cx, cg, start, finallyCatch, finallyCatch))
                return JS_FALSE;
        }
        break;
      }

#endif /* JS_HAS_EXCEPTIONS */

      case TOK_VAR:
        off = noteIndex = -1;
        for (pn2 = pn->pn_head; ; pn2 = pn2->pn_next) {
            JS_ASSERT(pn2->pn_type == TOK_NAME);
            if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
                return JS_FALSE;
            op = pn2->pn_op;
            if (pn2->pn_slot >= 0) {
                atomIndex = (jsatomid) pn2->pn_slot;
            } else {
                ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                atomIndex = ALE_INDEX(ale);

                /* Emit a prolog bytecode to predefine the var w/ void value. */
                CG_SWITCH_TO_PROLOG(cg);
                EMIT_ATOM_INDEX_OP(pn->pn_op, atomIndex);
                CG_SWITCH_TO_MAIN(cg);
            }
            if (pn2->pn_expr) {
                if (op == JSOP_SETNAME)
                    EMIT_ATOM_INDEX_OP(JSOP_BINDNAME, atomIndex);
                if (!js_EmitTree(cx, cg, pn2->pn_expr))
                    return JS_FALSE;
            }
            if (pn2 == pn->pn_head &&
                js_NewSrcNote(cx, cg,
                              (pn->pn_op == JSOP_DEFCONST)
                              ? SRC_CONST
                              : SRC_VAR) < 0) {
                return JS_FALSE;
            }
            EMIT_ATOM_INDEX_OP(op, atomIndex);
            tmp = CG_OFFSET(cg);
            if (noteIndex >= 0) {
                if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, tmp-off))
                    return JS_FALSE;
            }
            if (!pn2->pn_next)
                break;
            off = tmp;
            noteIndex = js_NewSrcNote2(cx, cg, SRC_PCDELTA, 0);
            if (noteIndex < 0 ||
                js_Emit1(cx, cg, JSOP_POP) < 0) {
                return JS_FALSE;
            }
        }
        if (pn->pn_extra) {
            if (js_Emit1(cx, cg, JSOP_POP) < 0)
                return JS_FALSE;
        }
        break;

      case TOK_RETURN:
        /* Push a return value */
        pn2 = pn->pn_kid;
        if (pn2) {
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
        } else {
            if (js_Emit1(cx, cg, JSOP_PUSH) < 0)
                return JS_FALSE;
        }

        /*
         * EmitNonLocalJumpFixup emits JSOP_SWAPs to maintain the return value
         * at the top of the stack, so the return still executes OK.
         */
        if (!EmitNonLocalJumpFixup(cx, cg, NULL, JS_TRUE))
            return JS_FALSE;
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
        pn2 = pn->pn_kid;
        if (pn2) {
            /*
             * Top-level or called-from-a-native JS_Execute/EvaluateScript,
             * debugger, and eval frames may need the value of the ultimate
             * expression statement as the script's result, despite the fact
             * that it appears useless to the compiler.
             */
            useful = !cx->fp->fun || cx->fp->fun->native || cx->fp->special;
            if (!useful) {
                if (!CheckSideEffects(cx, &cg->treeContext, pn2, &useful))
                    return JS_FALSE;
            }
            if (!useful) {
                cg->currentLine = pn2->pn_pos.begin.lineno;
                if (!js_ReportCompileErrorNumber(cx, NULL, cg,
                                                 JSREPORT_WARNING |
                                                 JSREPORT_STRICT,
                                                 JSMSG_USELESS_EXPR)) {
                    return JS_FALSE;
                }
            } else {
                if (!js_EmitTree(cx, cg, pn2))
                    return JS_FALSE;
                if (js_Emit1(cx, cg, JSOP_POPV) < 0)
                    return JS_FALSE;
            }
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
                                   (ptrdiff_t) ALE_INDEX(ale));
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
         * Emit SRC_PCDELTA notes on each JSOP_POP between comma operands.
         * These notes help the decompiler bracket the bytecodes generated
         * from each sub-expression that follows a comma.
         */
        off = noteIndex = -1;
        for (pn2 = pn->pn_head; ; pn2 = pn2->pn_next) {
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
            tmp = CG_OFFSET(cg);
            if (noteIndex >= 0) {
                if (!js_SetSrcNoteOffset(cx, cg, (uintN)noteIndex, 0, tmp-off))
                    return JS_FALSE;
            }
            if (!pn2->pn_next)
                break;
            off = tmp;
            noteIndex = js_NewSrcNote2(cx, cg, SRC_PCDELTA, 0);
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
        JS_ASSERT(pn2->pn_type != TOK_RP);
        atomIndex = (jsatomid) -1; /* Suppress warning. */
        switch (pn2->pn_type) {
          case TOK_NAME:
            if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
                return JS_FALSE;
            if (pn2->pn_slot >= 0) {
                atomIndex = (jsatomid) pn2->pn_slot;
            } else {
                ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
                if (!ale)
                    return JS_FALSE;
                atomIndex = ALE_INDEX(ale);
                EMIT_ATOM_INDEX_OP(JSOP_BINDNAME, atomIndex);
            }
            break;
          case TOK_DOT:
            if (!js_EmitTree(cx, cg, pn2->pn_expr))
                return JS_FALSE;
            ale = js_IndexAtom(cx, pn2->pn_atom, &cg->atomList);
            if (!ale)
                return JS_FALSE;
            atomIndex = ALE_INDEX(ale);
            break;
          case TOK_LB:
            if (!js_EmitTree(cx, cg, pn2->pn_left))
                return JS_FALSE;
            if (!js_EmitTree(cx, cg, pn2->pn_right))
                return JS_FALSE;
            break;
#if JS_HAS_LVALUE_RETURN
          case TOK_LP:
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
            break;
#endif
          default:
            JS_ASSERT(0);
        }

        op = pn->pn_op;
#if JS_HAS_GETTER_SETTER
        if (op == JSOP_GETTER || op == JSOP_SETTER) {
            /* We'll emit these prefix bytecodes after emitting the r.h.s. */
        } else
#endif
        /* If += or similar, dup the left operand and get its value. */
        if (op != JSOP_NOP) {
            switch (pn2->pn_type) {
              case TOK_NAME:
                if (pn2->pn_op != JSOP_SETNAME) {
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
#if JS_HAS_LVALUE_RETURN
              case TOK_LP:
#endif
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
            if (pn2->pn_slot < 0 || !(pn2->pn_attrs & JSPROP_READONLY)) {
          case TOK_DOT:
                EMIT_ATOM_INDEX_OP(pn2->pn_op, atomIndex);
            }
            break;
          case TOK_LB:
#if JS_HAS_LVALUE_RETURN
          case TOK_LP:
#endif
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
      case TOK_AND:
        /*
         * JSOP_OR converts the operand on the stack to boolean, and if true,
         * leaves the original operand value on the stack and jumps; otherwise
         * it pops and falls into the next bytecode, which evaluates the right
         * operand.  The jump goes around the right operand evaluation.
         *
         * JSOP_AND converts the operand on the stack to boolean, and if false,
         * leaves the original operand value on the stack and jumps; otherwise
         * it pops and falls into the right operand's bytecode.
         *
         * Avoid tail recursion for long ||...|| expressions and long &&...&&
         * expressions or long mixtures of ||'s and &&'s that can easily blow
         * the stack, by forward-linking and then backpatching all the JSOP_OR
         * and JSOP_AND bytecodes' immediate jump-offset operands.
         */
        if (!js_EmitTree(cx, cg, pn->pn_left))
            return JS_FALSE;
        top = js_Emit3(cx, cg, pn->pn_op, 0, 0);
        if (top < 0)
            return JS_FALSE;
        jmp = top;
        pn2 = pn->pn_right;
        while (pn2->pn_type == TOK_OR || pn2->pn_type == TOK_AND) {
            pn = pn2;
            if (!js_EmitTree(cx, cg, pn->pn_left))
                return JS_FALSE;
            off = js_Emit3(cx, cg, pn->pn_op, 0, 0);
            if (off < 0)
                return JS_FALSE;
            SET_JUMP_OFFSET(CG_CODE(cg, jmp), off - jmp);
            jmp = off;
            pn2 = pn->pn_right;
        }
        if (!js_EmitTree(cx, cg, pn2))
            return JS_FALSE;
        for (off = CG_OFFSET(cg); top < jmp; top += tmp) {
            jsbytecode *pc = CG_CODE(cg, top);
            tmp = GET_JUMP_OFFSET(pc);
            CHECK_AND_SET_JUMP_OFFSET(cx, cg, pc, off - top);
        }
        CHECK_AND_SET_JUMP_OFFSET(cx, cg, CG_CODE(cg, jmp), off - jmp);
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
        JS_ASSERT(pn2->pn_type != TOK_RP);
        op = pn->pn_op;
        switch (pn2->pn_type) {
          case TOK_NAME:
            pn2->pn_op = op;
            if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
                return JS_FALSE;
            op = pn2->pn_op;
            if (pn2->pn_slot >= 0) {
                if (pn2->pn_attrs & JSPROP_READONLY)
                    op = JSOP_GETVAR;
                atomIndex = (jsatomid) pn2->pn_slot;
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
#if JS_HAS_LVALUE_RETURN
          case TOK_LP:
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
            if (js_NewSrcNote2(cx, cg, SRC_PCBASE,
                               (ptrdiff_t)(CG_OFFSET(cg) - pn2->pn_offset))
                               < 0) {
                return JS_FALSE;
            }
            if (js_Emit1(cx, cg, op) < 0)
                return JS_FALSE;
            break;
#endif
          default:
            JS_ASSERT(0);
        }
        break;

      case TOK_DELETE:
        /* Under ECMA 3, deleting a non-reference returns true. */
        pn2 = pn->pn_kid;
        switch (pn2->pn_type) {
          case TOK_NAME:
            pn2->pn_op = JSOP_DELNAME;
            if (!LookupArgOrVar(cx, &cg->treeContext, pn2))
                return JS_FALSE;
            op = pn2->pn_op;
            if (op == JSOP_FALSE) {
                if (js_Emit1(cx, cg, op) < 0)
                    return JS_FALSE;
            } else {
                if (!EmitAtomOp(cx, pn2, op, cg))
                    return JS_FALSE;
            }
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
            if (js_Emit1(cx, cg, JSOP_TRUE) < 0)
                return JS_FALSE;
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

      case TOK_NEW:
      case TOK_LP:
        /*
         * Emit function call or operator new (constructor call) code.
         * First, emit code for the left operand to evaluate the callable or
         * constructable object expression.
         */
        pn2 = pn->pn_head;
        if (!js_EmitTree(cx, cg, pn2))
            return JS_FALSE;

        /* Remember start of callable-object bytecode for decompilation hint. */
        off = pn2->pn_offset;

        /*
         * Push the virtual machine's "obj" register, which was set by a name,
         * property, or element get (or set) bytecode.
         */
        if (js_Emit1(cx, cg, JSOP_PUSHOBJ) < 0)
            return JS_FALSE;

        /*
         * Emit code for each argument in order, then emit the JSOP_*CALL or
         * JSOP_NEW bytecode with a two-byte immediate telling how many args
         * were pushed on the operand stack.
         */
        for (pn2 = pn2->pn_next; pn2; pn2 = pn2->pn_next) {
            if (!js_EmitTree(cx, cg, pn2))
                return JS_FALSE;
        }
        if (js_NewSrcNote2(cx, cg, SRC_PCBASE, CG_OFFSET(cg) - off) < 0)
            return JS_FALSE;
        argc = pn->pn_count - 1;
        if (js_Emit3(cx, cg, pn->pn_op, ARGC_HI(argc), ARGC_LO(argc)) < 0)
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
        EMIT_ATOM_INDEX_OP(JSOP_NAME, ALE_INDEX(ale));
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
            JS_ASSERT(atomIndex < ATOM_INDEX_LIMIT);
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
            if (js_NewSrcNote(cx, cg, SRC_CONTINUE) < 0)
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
        EMIT_ATOM_INDEX_OP(JSOP_NAME, ALE_INDEX(ale));

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
                JS_ASSERT(0);
            }

            /* Emit code for the property initializer. */
            if (!js_EmitTree(cx, cg, pn2->pn_right))
                return JS_FALSE;

#if JS_HAS_GETTER_SETTER
            op = pn2->pn_op;
            if (op == JSOP_GETTER || op == JSOP_SETTER) {
                if (js_Emit1(cx, cg, op) < 0)
                    return JS_FALSE;
            }
#endif
            /* Annotate JSOP_INITELEM so we decompile 2:c and not just c. */
            if (pn3->pn_type == TOK_NUMBER) {
                if (js_NewSrcNote(cx, cg, SRC_LABEL) < 0)
                    return JS_FALSE;
                if (js_Emit1(cx, cg, JSOP_INITELEM) < 0)
                    return JS_FALSE;
            } else {
                EMIT_ATOM_INDEX_OP(JSOP_INITPROP, ALE_INDEX(ale));
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
         */
        if (!js_EmitTree(cx, cg, pn->pn_kid))
            return JS_FALSE;
        if (js_Emit1(cx, cg, JSOP_GROUP) < 0)
            return JS_FALSE;
        break;

      case TOK_NAME:
        if (!LookupArgOrVar(cx, &cg->treeContext, pn))
            return JS_FALSE;
        op = pn->pn_op;
        if (op == JSOP_ARGUMENTS) {
            if (js_Emit1(cx, cg, op) < 0)
                return JS_FALSE;
            break;
        }
        if (pn->pn_slot >= 0) {
            atomIndex = (jsatomid) pn->pn_slot;
            EMIT_ATOM_INDEX_OP(op, atomIndex);
            break;
        }
        /* FALL THROUGH */
      case TOK_STRING:
      case TOK_OBJECT:
        /*
         * The scanner and parser associate JSOP_NAME with TOK_NAME, although
         * other bytecodes may result instead (JSOP_BINDNAME/JSOP_SETNAME,
         * JSOP_FORNAME, etc.).  Among JSOP_*NAME* variants, only JSOP_NAME
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

#if JS_HAS_DEBUGGER_KEYWORD
      case TOK_DEBUGGER:
        return js_Emit1(cx, cg, JSOP_DEBUGGER) >= 0;
#endif /* JS_HAS_DEBUGGER_KEYWORD */

      default:
        JS_ASSERT(0);
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
    "pcdelta",
    "assignop",
    "cond",
    "reserved0",
    "hidden",
    "pcbase",
    "label",
    "labelbrace",
    "endbrace",
    "break2label",
    "cont2label",
    "switch",
    "funcdef",
    "catch",
    "const",
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
    1,  /* SRC_PCDELTA */
    0,  /* SRC_ASSIGNOP */
    0,  /* SRC_COND */
    0,  /* SRC_RESERVED0 */
    0,  /* SRC_HIDDEN */
    1,  /* SRC_PCBASE */
    1,  /* SRC_LABEL */
    1,  /* SRC_LABELBRACE */
    0,  /* SRC_ENDBRACE */
    1,  /* SRC_BREAK2LABEL */
    1,  /* SRC_CONT2LABEL */
    2,  /* SRC_SWITCH */
    1,  /* SRC_FUNCDEF */
    1,  /* SRC_CATCH */
    0,  /* SRC_CONST */
    0,  /* SRC_NEWLINE */
    1,  /* SRC_SETLINE */
    0   /* SRC_XDELTA */
};

static intN
AllocSrcNote(JSContext *cx, JSCodeGenerator *cg)
{
    intN index;
    JSArenaPool *pool;
    size_t size;

    index = cg->noteCount;
    if (((uintN)index & cg->noteMask) == 0) {
        pool = &cx->notePool;
        size = SRCNOTE_SIZE(cg->noteMask + 1);
        if (!cg->notes) {
            /* Allocate the first note array lazily; leave noteMask alone. */
            JS_ARENA_ALLOCATE_CAST(cg->notes, jssrcnote *, pool, size);
        } else {
            /* Grow by doubling note array size; update noteMask on success. */
            JS_ARENA_GROW_CAST(cg->notes, jssrcnote *, pool, size, size);
            if (cg->notes)
                cg->noteMask = (cg->noteMask << 1) | 1;
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
    ptrdiff_t offset, delta, xdelta;

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
    delta = offset - cg->lastNoteOffset;
    cg->lastNoteOffset = offset;
    if (delta >= SN_DELTA_LIMIT) {
        do {
            xdelta = JS_MIN(delta, SN_XDELTA_MASK);
            SN_MAKE_XDELTA(sn, xdelta);
            delta -= xdelta;
            index = AllocSrcNote(cx, cg);
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
    JSArenaPool *pool;
    size_t size;

    /* Grow by doubling note array size; update noteMask on success. */
    pool = &cx->notePool;
    size = SRCNOTE_SIZE(cg->noteMask + 1);
    JS_ARENA_GROW_CAST(cg->notes, jssrcnote *, pool, size, size);
    if (!cg->notes) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    cg->noteMask = (cg->noteMask << 1) | 1;
    return JS_TRUE;
}

uintN
js_SrcNoteLength(jssrcnote *sn)
{
    uintN arity;
    jssrcnote *base;

    arity = (intN)js_SrcNoteArity[SN_TYPE(sn)];
    for (base = sn++; arity--; sn++) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }
    return sn - base;
}

JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, uintN which)
{
    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    JS_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    JS_ASSERT(which < js_SrcNoteArity[SN_TYPE(sn)]);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }
    if (*sn & SN_3BYTE_OFFSET_FLAG) {
        return (ptrdiff_t)((((uint32)(sn[0] & SN_3BYTE_OFFSET_MASK)) << 16)
                           | (sn[1] << 8) | sn[2]);
    }
    return (ptrdiff_t)*sn;
}

JSBool
js_SetSrcNoteOffset(JSContext *cx, JSCodeGenerator *cg, uintN index,
                    uintN which, ptrdiff_t offset)
{
    jssrcnote *sn;
    ptrdiff_t diff;

    if (offset >= (((ptrdiff_t)SN_3BYTE_OFFSET_FLAG) << 16)) {
        ReportStatementTooLarge(cx, cg);
        return JS_FALSE;
    }

    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    sn = &cg->notes[index];
    JS_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    JS_ASSERT(which < js_SrcNoteArity[SN_TYPE(sn)]);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }

    /* See if the new offset requires three bytes. */
    if (offset > (ptrdiff_t)SN_3BYTE_OFFSET_MASK) {
        /* Maybe this offset was already set to a three-byte value. */
        if (!(*sn & SN_3BYTE_OFFSET_FLAG)) {
            /* Losing, need to insert another two bytes for this offset. */
            index = PTRDIFF(sn, cg->notes, jssrcnote);

            /*
             * Simultaneously test to see if the source note array must grow to
             * accomodate either the first or second byte of additional storage
             * required by this 3-byte offset.
             */
            if (((cg->noteCount + 1) & cg->noteMask) <= 1) {
                if (!GrowSrcNotes(cx, cg))
                    return JS_FALSE;
                sn = cg->notes + index;
            }
            cg->noteCount += 2;

            diff = cg->noteCount - (index + 3);
            JS_ASSERT(diff >= 0);
            if (diff > 0)
                memmove(sn + 3, sn + 1, SRCNOTE_SIZE(diff));
        }
        *sn++ = (jssrcnote)(SN_3BYTE_OFFSET_FLAG | (offset >> 16));
        *sn++ = (jssrcnote)(offset >> 8);
    }
    *sn = (jssrcnote)offset;
    return JS_TRUE;
}

jssrcnote *
js_FinishTakingSrcNotes(JSContext *cx, JSCodeGenerator *cg)
{
    uintN count;
    jssrcnote *tmp, *final;

    count = cg->noteCount;
    tmp   = cg->notes;
    final = (jssrcnote *) JS_malloc(cx, SRCNOTE_SIZE(count + 1));
    if (!final)
        return NULL;
    memcpy(final, tmp, SRCNOTE_SIZE(count));
    SN_MAKE_TERMINATOR(&final[count]);
    return final;
}

JSBool
js_AllocTryNotes(JSContext *cx, JSCodeGenerator *cg)
{
    size_t size, incr;
    ptrdiff_t delta;

    size = TRYNOTE_SIZE(cg->treeContext.tryCount);
    if (size <= cg->tryNoteSpace)
        return JS_TRUE;

    /*
     * Allocate trynotes from cx->tempPool.
     * XXX too much growing and we bloat, as other tempPool allocators block
     * in-place growth, and we never recycle old free space in an arena.
     */
    if (!cg->tryBase) {
        size = JS_ROUNDUP(size, TRYNOTE_SIZE(TRYNOTE_GRAIN));
        JS_ARENA_ALLOCATE_CAST(cg->tryBase, JSTryNote *, &cx->tempPool, size);
        if (!cg->tryBase)
            return JS_FALSE;
        cg->tryNoteSpace = size;
        cg->tryNext = cg->tryBase;
    } else {
        delta = PTRDIFF((char *)cg->tryNext, (char *)cg->tryBase, char);
        incr = size - cg->tryNoteSpace;
        incr = JS_ROUNDUP(incr, TRYNOTE_SIZE(TRYNOTE_GRAIN));
        size = cg->tryNoteSpace;
        JS_ARENA_GROW_CAST(cg->tryBase, JSTryNote *, &cx->tempPool, size, incr);
        if (!cg->tryBase)
            return JS_FALSE;
        cg->tryNoteSpace = size + incr;
        cg->tryNext = (JSTryNote *)((char *)cg->tryBase + delta);
    }
    return JS_TRUE;
}

JSTryNote *
js_NewTryNote(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t start,
              ptrdiff_t end, ptrdiff_t catchStart)
{
    JSTryNote *tn;

    JS_ASSERT(cg->tryBase <= cg->tryNext);
    JS_ASSERT(catchStart >= 0);
    tn = cg->tryNext++;
    tn->start = start;
    tn->length = end - start;
    tn->catchStart = catchStart;
    return tn;
}

JSBool
js_FinishTakingTryNotes(JSContext *cx, JSCodeGenerator *cg, JSTryNote **tryp)
{
    uintN count;
    JSTryNote *tmp, *final;

    count = PTRDIFF(cg->tryNext, cg->tryBase, JSTryNote);
    if (!count) {
        *tryp = NULL;
        return JS_TRUE;
    }

    tmp = cg->tryBase;
    final = (JSTryNote *) JS_malloc(cx, TRYNOTE_SIZE(count + 1));
    if (!final) {
        *tryp = NULL;
        return JS_FALSE;
    }
    memcpy(final, tmp, TRYNOTE_SIZE(count));
    final[count].start = 0;
    final[count].length = CG_OFFSET(cg);
    final[count].catchStart = 0;
    *tryp = final;
    return JS_TRUE;
}
