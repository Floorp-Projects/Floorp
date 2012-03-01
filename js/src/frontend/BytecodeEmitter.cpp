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
 * JS bytecode generation.
 */
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <new>
#include <string.h>

#include "jstypes.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsautooplen.h"        // generated headers last

#include "ds/LifoAlloc.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/Parser.h"
#include "frontend/TokenStream.h"
#include "vm/RegExpObject.h"

#include "jsatominlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"

#include "frontend/BytecodeEmitter-inl.h"
#include "frontend/ParseMaps-inl.h"

/* Allocation chunk counts, must be powers of two in general. */
#define BYTECODE_CHUNK_LENGTH  1024    /* initial bytecode chunk length */
#define SRCNOTE_CHUNK_LENGTH   1024    /* initial srcnote chunk length */

/* Macros to compute byte sizes from typed element counts. */
#define BYTECODE_SIZE(n)        ((n) * sizeof(jsbytecode))
#define SRCNOTE_SIZE(n)         ((n) * sizeof(jssrcnote))

using namespace js;
using namespace js::gc;
using namespace js::frontend;

static JSBool
NewTryNote(JSContext *cx, BytecodeEmitter *bce, JSTryNoteKind kind, unsigned stackDepth,
           size_t start, size_t end);

static JSBool
SetSrcNoteOffset(JSContext *cx, BytecodeEmitter *bce, unsigned index, unsigned which, ptrdiff_t offset);

void
TreeContext::trace(JSTracer *trc)
{
    bindings.trace(trc);
}

BytecodeEmitter::BytecodeEmitter(Parser *parser, unsigned lineno)
  : TreeContext(parser),
    atomIndices(parser->context),
    stackDepth(0), maxStackDepth(0),
    ntrynotes(0), lastTryNode(NULL),
    arrayCompDepth(0),
    emitLevel(0),
    constMap(parser->context),
    constList(parser->context),
    upvarIndices(parser->context),
    upvarMap(parser->context),
    globalScope(NULL),
    globalUses(parser->context),
    globalMap(parser->context),
    closedArgs(parser->context),
    closedVars(parser->context),
    typesetCount(0)
{
    flags = TCF_COMPILING;
    memset(&prolog, 0, sizeof prolog);
    memset(&main, 0, sizeof main);
    current = &main;
    firstLine = prolog.currentLine = main.currentLine = lineno;
}

bool
BytecodeEmitter::init(JSContext *cx, TreeContext::InitBehavior ib)
{
    roLexdeps.init();
    return TreeContext::init(cx, ib) && constMap.init() && atomIndices.ensureMap(cx);
}

BytecodeEmitter::~BytecodeEmitter()
{
    JSContext *cx = parser->context;

    cx->free_(prolog.base);
    cx->free_(prolog.notes);
    cx->free_(main.base);
    cx->free_(main.notes);
}

static ptrdiff_t
EmitCheck(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t delta)
{
    jsbytecode *base = bce->base();
    jsbytecode *newbase;
    jsbytecode *next = bce->next();
    jsbytecode *limit = bce->limit();
    ptrdiff_t offset = next - base;
    size_t minlength = offset + delta;

    if (next + delta > limit) {
        size_t newlength;
        if (!base) {
            JS_ASSERT(!next && !limit);
            newlength = BYTECODE_CHUNK_LENGTH;
            if (newlength < minlength)     /* make it bigger if necessary */
                newlength = RoundUpPow2(minlength);
            newbase = (jsbytecode *) cx->malloc_(BYTECODE_SIZE(newlength));
        } else {
            JS_ASSERT(base <= next && next <= limit);
            newlength = (limit - base) * 2;
            if (newlength < minlength)     /* make it bigger if necessary */
                newlength = RoundUpPow2(minlength);
            newbase = (jsbytecode *) cx->realloc_(base, BYTECODE_SIZE(newlength));
        }
        if (!newbase) {
            js_ReportOutOfMemory(cx);
            return -1;
        }
        JS_ASSERT(newlength >= size_t(offset + delta));
        bce->current->base = newbase;
        bce->current->limit = newbase + newlength;
        bce->current->next = newbase + offset;
    }
    return offset;
}

static StaticBlockObject &
CurrentBlock(BytecodeEmitter *bce)
{
    JS_ASSERT(bce->topStmt->type == STMT_BLOCK || bce->topStmt->type == STMT_SWITCH);
    JS_ASSERT(bce->topStmt->blockObj->isStaticBlock());
    return *bce->topStmt->blockObj;
}

static void
UpdateDepth(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t target)
{
    jsbytecode *pc = bce->code(target);
    JSOp op = (JSOp) *pc;
    const JSCodeSpec *cs = &js_CodeSpec[op];


    if (cs->format & JOF_TMPSLOT_MASK) {
        /*
         * An opcode may temporarily consume stack space during execution.
         * Account for this in maxStackDepth separately from uses/defs here.
         */
        unsigned depth = (unsigned) bce->stackDepth +
                      ((cs->format & JOF_TMPSLOT_MASK) >> JOF_TMPSLOT_SHIFT);
        if (depth > bce->maxStackDepth)
            bce->maxStackDepth = depth;
    }

    /*
     * Specially handle any case that would call js_GetIndexFromBytecode since
     * it requires a well-formed script. This allows us to safely pass NULL as
     * the 'script' parameter.
     */
    int nuses, ndefs;
    if (op == JSOP_ENTERBLOCK) {
        nuses = 0;
        ndefs = CurrentBlock(bce).slotCount();
    } else if (op == JSOP_ENTERLET0) {
        nuses = ndefs = CurrentBlock(bce).slotCount();
    } else if (op == JSOP_ENTERLET1) {
        nuses = ndefs = CurrentBlock(bce).slotCount() + 1;
    } else {
        nuses = StackUses(NULL, pc);
        ndefs = StackDefs(NULL, pc);
    }

    bce->stackDepth -= nuses;
    JS_ASSERT(bce->stackDepth >= 0);
    bce->stackDepth += ndefs;
    if ((unsigned)bce->stackDepth > bce->maxStackDepth)
        bce->maxStackDepth = bce->stackDepth;
}

static inline void
UpdateDecomposeLength(BytecodeEmitter *bce, unsigned start)
{
    unsigned end = bce->offset();
    JS_ASSERT(unsigned(end - start) < 256);
    bce->code(start)[-1] = end - start;
}

ptrdiff_t
frontend::Emit1(JSContext *cx, BytecodeEmitter *bce, JSOp op)
{
    JS_ASSERT_IF(op == JSOP_ARGUMENTS, !bce->mayOverwriteArguments());

    ptrdiff_t offset = EmitCheck(cx, bce, 1);

    if (offset >= 0) {
        *bce->current->next++ = (jsbytecode)op;
        UpdateDepth(cx, bce, offset);
    }
    return offset;
}

ptrdiff_t
frontend::Emit2(JSContext *cx, BytecodeEmitter *bce, JSOp op, jsbytecode op1)
{
    ptrdiff_t offset = EmitCheck(cx, bce, 2);

    if (offset >= 0) {
        jsbytecode *next = bce->next();
        next[0] = (jsbytecode)op;
        next[1] = op1;
        bce->current->next = next + 2;
        UpdateDepth(cx, bce, offset);
    }
    return offset;
}

ptrdiff_t
frontend::Emit3(JSContext *cx, BytecodeEmitter *bce, JSOp op, jsbytecode op1,
                    jsbytecode op2)
{
    ptrdiff_t offset = EmitCheck(cx, bce, 3);

    if (offset >= 0) {
        jsbytecode *next = bce->next();
        next[0] = (jsbytecode)op;
        next[1] = op1;
        next[2] = op2;
        bce->current->next = next + 3;
        UpdateDepth(cx, bce, offset);
    }
    return offset;
}

ptrdiff_t
frontend::EmitN(JSContext *cx, BytecodeEmitter *bce, JSOp op, size_t extra)
{
    ptrdiff_t length = 1 + (ptrdiff_t)extra;
    ptrdiff_t offset = EmitCheck(cx, bce, length);

    if (offset >= 0) {
        jsbytecode *next = bce->next();
        *next = (jsbytecode)op;
        memset(next + 1, 0, BYTECODE_SIZE(extra));
        bce->current->next = next + length;

        /*
         * Don't UpdateDepth if op's use-count comes from the immediate
         * operand yet to be stored in the extra bytes after op.
         */
        if (js_CodeSpec[op].nuses >= 0)
            UpdateDepth(cx, bce, offset);
    }
    return offset;
}

static ptrdiff_t
EmitJump(JSContext *cx, BytecodeEmitter *bce, JSOp op, ptrdiff_t off)
{
    ptrdiff_t offset = EmitCheck(cx, bce, 5);

    if (offset >= 0) {
        jsbytecode *next = bce->next();
        next[0] = (jsbytecode)op;
        SET_JUMP_OFFSET(next, off);
        bce->current->next = next + 5;
        UpdateDepth(cx, bce, offset);
    }
    return offset;
}

/* XXX too many "... statement" L10N gaffes below -- fix via js.msg! */
const char js_with_statement_str[] = "with statement";
const char js_finally_block_str[]  = "finally block";
const char js_script_str[]         = "script";

static const char *statementName[] = {
    "label statement",       /* LABEL */
    "if statement",          /* IF */
    "else statement",        /* ELSE */
    "destructuring body",    /* BODY */
    "switch statement",      /* SWITCH */
    "block",                 /* BLOCK */
    js_with_statement_str,   /* WITH */
    "catch block",           /* CATCH */
    "try block",             /* TRY */
    js_finally_block_str,    /* FINALLY */
    js_finally_block_str,    /* SUBROUTINE */
    "do loop",               /* DO_LOOP */
    "for loop",              /* FOR_LOOP */
    "for/in loop",           /* FOR_IN_LOOP */
    "while loop",            /* WHILE_LOOP */
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(statementName) == STMT_LIMIT);

static const char *
StatementName(BytecodeEmitter *bce)
{
    if (!bce->topStmt)
        return js_script_str;
    return statementName[bce->topStmt->type];
}

static void
ReportStatementTooLarge(JSContext *cx, BytecodeEmitter *bce)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEED_DIET,
                         StatementName(bce));
}

bool
TreeContext::inStatement(StmtType type)
{
    for (StmtInfo *stmt = topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == type)
            return true;
    }
    return false;
}

bool
TreeContext::skipSpansGenerator(unsigned skip)
{
    TreeContext *tc = this;
    for (unsigned i = 0; i < skip; ++i, tc = tc->parent) {
        if (!tc)
            return false;
        if (tc->flags & TCF_FUN_IS_GENERATOR)
            return true;
    }
    return false;
}

bool
frontend::SetStaticLevel(TreeContext *tc, unsigned staticLevel)
{
    /*
     * This is a lot simpler than error-checking every UpvarCookie::set, and
     * practically speaking it leaves more than enough room for upvars.
     */
    if (UpvarCookie::isLevelReserved(staticLevel)) {
        JS_ReportErrorNumber(tc->parser->context, js_GetErrorMessage, NULL,
                             JSMSG_TOO_DEEP, js_function_str);
        return false;
    }
    tc->staticLevel = staticLevel;
    return true;
}

bool
frontend::GenerateBlockId(TreeContext *tc, uint32_t &blockid)
{
    if (tc->blockidGen == JS_BIT(20)) {
        JS_ReportErrorNumber(tc->parser->context, js_GetErrorMessage, NULL,
                             JSMSG_NEED_DIET, "program");
        return false;
    }
    blockid = tc->blockidGen++;
    return true;
}

void
frontend::PushStatement(TreeContext *tc, StmtInfo *stmt, StmtType type, ptrdiff_t top)
{
    stmt->type = type;
    stmt->flags = 0;
    stmt->blockid = tc->blockid();
    SET_STATEMENT_TOP(stmt, top);
    stmt->label = NULL;
    JS_ASSERT(!stmt->blockObj);
    stmt->down = tc->topStmt;
    tc->topStmt = stmt;
    if (STMT_LINKS_SCOPE(stmt)) {
        stmt->downScope = tc->topScopeStmt;
        tc->topScopeStmt = stmt;
    } else {
        stmt->downScope = NULL;
    }
}

void
frontend::PushBlockScope(TreeContext *tc, StmtInfo *stmt, StaticBlockObject &blockObj, ptrdiff_t top)
{
    PushStatement(tc, stmt, STMT_BLOCK, top);
    stmt->flags |= SIF_SCOPE;
    blockObj.setEnclosingBlock(tc->blockChain);
    stmt->downScope = tc->topScopeStmt;
    tc->topScopeStmt = stmt;
    tc->blockChain = &blockObj;
    stmt->blockObj = &blockObj;
}

/*
 * Emit a backpatch op with offset pointing to the previous jump of this type,
 * so that we can walk back up the chain fixing up the op and jump offset.
 */
static ptrdiff_t
EmitBackPatchOp(JSContext *cx, BytecodeEmitter *bce, JSOp op, ptrdiff_t *lastp)
{
    ptrdiff_t offset, delta;

    offset = bce->offset();
    delta = offset - *lastp;
    *lastp = offset;
    JS_ASSERT(delta > 0);
    return EmitJump(cx, bce, op, delta);
}

/* A macro for inlining at the top of EmitTree (whence it came). */
#define UPDATE_LINE_NUMBER_NOTES(cx, bce, line)                               \
    JS_BEGIN_MACRO                                                            \
        unsigned line_ = (line);                                                 \
        unsigned delta_ = line_ - bce->currentLine();                            \
        if (delta_ != 0) {                                                    \
            /*                                                                \
             * Encode any change in the current source line number by using   \
             * either several SRC_NEWLINE notes or just one SRC_SETLINE note, \
             * whichever consumes less space.                                 \
             *                                                                \
             * NB: We handle backward line number deltas (possible with for   \
             * loops where the update part is emitted after the body, but its \
             * line number is <= any line number in the body) here by letting \
             * unsigned delta_ wrap to a very large number, which triggers a  \
             * SRC_SETLINE.                                                   \
             */                                                               \
            bce->current->currentLine = line_;                                \
            if (delta_ >= (unsigned)(2 + ((line_ > SN_3BYTE_OFFSET_MASK)<<1))) { \
                if (NewSrcNote2(cx, bce, SRC_SETLINE, (ptrdiff_t)line_) < 0)  \
                    return JS_FALSE;                                          \
            } else {                                                          \
                do {                                                          \
                    if (NewSrcNote(cx, bce, SRC_NEWLINE) < 0)                 \
                        return JS_FALSE;                                      \
                } while (--delta_ != 0);                                      \
            }                                                                 \
        }                                                                     \
    JS_END_MACRO

/* A function, so that we avoid macro-bloating all the other callsites. */
static JSBool
UpdateLineNumberNotes(JSContext *cx, BytecodeEmitter *bce, unsigned line)
{
    UPDATE_LINE_NUMBER_NOTES(cx, bce, line);
    return JS_TRUE;
}

static ptrdiff_t
EmitLoopHead(JSContext *cx, BytecodeEmitter *bce, ParseNode *nextpn)
{
    if (nextpn) {
        /*
         * Try to give the JSOP_LOOPHEAD the same line number as the next
         * instruction. nextpn is often a block, in which case the next
         * instruction typically comes from the first statement inside.
         */
        JS_ASSERT_IF(nextpn->isKind(PNK_STATEMENTLIST), nextpn->isArity(PN_LIST));
        if (nextpn->isKind(PNK_STATEMENTLIST) && nextpn->pn_head)
            nextpn = nextpn->pn_head;
        if (!UpdateLineNumberNotes(cx, bce, nextpn->pn_pos.begin.lineno))
            return -1;
    }

    return Emit1(cx, bce, JSOP_LOOPHEAD);
}

static bool
EmitLoopEntry(JSContext *cx, BytecodeEmitter *bce, ParseNode *nextpn)
{
    if (nextpn) {
        /* Update the line number, as for LOOPHEAD. */
        JS_ASSERT_IF(nextpn->isKind(PNK_STATEMENTLIST), nextpn->isArity(PN_LIST));
        if (nextpn->isKind(PNK_STATEMENTLIST) && nextpn->pn_head)
            nextpn = nextpn->pn_head;
        if (!UpdateLineNumberNotes(cx, bce, nextpn->pn_pos.begin.lineno))
            return false;
    }

    return Emit1(cx, bce, JSOP_LOOPENTRY) >= 0;
}

/*
 * If op is JOF_TYPESET (see the type barriers comment in jsinfer.h), reserve
 * a type set to store its result.
 */
static inline void
CheckTypeSet(JSContext *cx, BytecodeEmitter *bce, JSOp op)
{
    if (js_CodeSpec[op].format & JOF_TYPESET) {
        if (bce->typesetCount < UINT16_MAX)
            bce->typesetCount++;
    }
}

/*
 * Macro to emit a bytecode followed by a uint16_t immediate operand stored in
 * big-endian order, used for arg and var numbers as well as for atomIndexes.
 * NB: We use cx and bce from our caller's lexical environment, and return
 * false on error.
 */
#define EMIT_UINT16_IMM_OP(op, i)                                             \
    JS_BEGIN_MACRO                                                            \
        if (Emit3(cx, bce, op, UINT16_HI(i), UINT16_LO(i)) < 0)               \
            return JS_FALSE;                                                  \
        CheckTypeSet(cx, bce, op);                                            \
    JS_END_MACRO

#define EMIT_UINT16PAIR_IMM_OP(op, i, j)                                      \
    JS_BEGIN_MACRO                                                            \
        ptrdiff_t off_ = EmitN(cx, bce, op, 2 * UINT16_LEN);                  \
        if (off_ < 0)                                                         \
            return JS_FALSE;                                                  \
        jsbytecode *pc_ = bce->code(off_);                                    \
        SET_UINT16(pc_, i);                                                   \
        pc_ += UINT16_LEN;                                                    \
        SET_UINT16(pc_, j);                                                   \
    JS_END_MACRO

#define EMIT_UINT16_IN_PLACE(offset, op, i)                                   \
    JS_BEGIN_MACRO                                                            \
        bce->code(offset)[0] = op;                                            \
        bce->code(offset)[1] = UINT16_HI(i);                                  \
        bce->code(offset)[2] = UINT16_LO(i);                                  \
    JS_END_MACRO

#define EMIT_UINT32_IN_PLACE(offset, op, i)                                   \
    JS_BEGIN_MACRO                                                            \
        bce->code(offset)[0] = op;                                            \
        bce->code(offset)[1] = jsbytecode(i >> 24);                           \
        bce->code(offset)[2] = jsbytecode(i >> 16);                           \
        bce->code(offset)[3] = jsbytecode(i >> 8);                            \
        bce->code(offset)[4] = jsbytecode(i);                                 \
    JS_END_MACRO

static JSBool
FlushPops(JSContext *cx, BytecodeEmitter *bce, int *npops)
{
    JS_ASSERT(*npops != 0);
    if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
        return JS_FALSE;
    EMIT_UINT16_IMM_OP(JSOP_POPN, *npops);
    *npops = 0;
    return JS_TRUE;
}

static bool
PopIterator(JSContext *cx, BytecodeEmitter *bce)
{
    if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
        return false;
    if (Emit1(cx, bce, JSOP_ENDITER) < 0)
        return false;
    return true;
}

/*
 * Emit additional bytecode(s) for non-local jumps.
 */
static JSBool
EmitNonLocalJumpFixup(JSContext *cx, BytecodeEmitter *bce, StmtInfo *toStmt)
{
    /*
     * The non-local jump fixup we emit will unbalance bce->stackDepth, because
     * the fixup replicates balanced code such as JSOP_LEAVEWITH emitted at the
     * end of a with statement, so we save bce->stackDepth here and restore it
     * just before a successful return.
     */
    int depth = bce->stackDepth;
    int npops = 0;

#define FLUSH_POPS() if (npops && !FlushPops(cx, bce, &npops)) return JS_FALSE

    for (StmtInfo *stmt = bce->topStmt; stmt != toStmt; stmt = stmt->down) {
        switch (stmt->type) {
          case STMT_FINALLY:
            FLUSH_POPS();
            if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                return JS_FALSE;
            if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &GOSUBS(*stmt)) < 0)
                return JS_FALSE;
            break;

          case STMT_WITH:
            /* There's a With object on the stack that we need to pop. */
            FLUSH_POPS();
            if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                return JS_FALSE;
            if (Emit1(cx, bce, JSOP_LEAVEWITH) < 0)
                return JS_FALSE;
            break;

          case STMT_FOR_IN_LOOP:
            FLUSH_POPS();
            if (!PopIterator(cx, bce))
                return JS_FALSE;
            break;

          case STMT_SUBROUTINE:
            /*
             * There's a [exception or hole, retsub pc-index] pair on the
             * stack that we need to pop.
             */
            npops += 2;
            break;

          default:;
        }

        if (stmt->flags & SIF_SCOPE) {
            FLUSH_POPS();
            unsigned blockObjCount = stmt->blockObj->slotCount();
            if (stmt->flags & SIF_FOR_BLOCK) {
                /*
                 * For a for-let-in statement, pushing/popping the block is
                 * interleaved with JSOP_(END)ITER. Just handle both together
                 * here and skip over the enclosing STMT_FOR_IN_LOOP.
                 */
                JS_ASSERT(stmt->down->type == STMT_FOR_IN_LOOP);
                stmt = stmt->down;
                if (stmt == toStmt)
                    break;
                if (Emit1(cx, bce, JSOP_LEAVEFORLETIN) < 0)
                    return JS_FALSE;
                if (!PopIterator(cx, bce))
                    return JS_FALSE;
                if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                    return JS_FALSE;
                EMIT_UINT16_IMM_OP(JSOP_POPN, blockObjCount);
            } else {
                /* There is a Block object with locals on the stack to pop. */
                if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                    return JS_FALSE;
                EMIT_UINT16_IMM_OP(JSOP_LEAVEBLOCK, blockObjCount);
            }
        }
    }

    FLUSH_POPS();
    bce->stackDepth = depth;
    return JS_TRUE;

#undef FLUSH_POPS
}

static const jsatomid INVALID_ATOMID = -1;

static ptrdiff_t
EmitGoto(JSContext *cx, BytecodeEmitter *bce, StmtInfo *toStmt, ptrdiff_t *lastp,
         jsatomid labelIndex = INVALID_ATOMID, SrcNoteType noteType = SRC_NULL)
{
    int index;

    if (!EmitNonLocalJumpFixup(cx, bce, toStmt))
        return -1;

    if (labelIndex != INVALID_ATOMID)
        index = NewSrcNote2(cx, bce, noteType, ptrdiff_t(labelIndex));
    else if (noteType != SRC_NULL)
        index = NewSrcNote(cx, bce, noteType);
    else
        index = 0;
    if (index < 0)
        return -1;

    return EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, lastp);
}

static JSBool
BackPatch(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t last, jsbytecode *target, jsbytecode op)
{
    jsbytecode *pc, *stop;
    ptrdiff_t delta, span;

    pc = bce->code(last);
    stop = bce->code(-1);
    while (pc != stop) {
        delta = GET_JUMP_OFFSET(pc);
        span = target - pc;
        SET_JUMP_OFFSET(pc, span);
        *pc = op;
        pc -= delta;
    }
    return JS_TRUE;
}

void
frontend::PopStatementTC(TreeContext *tc)
{
    StmtInfo *stmt = tc->topStmt;
    tc->topStmt = stmt->down;
    if (STMT_LINKS_SCOPE(stmt)) {
        tc->topScopeStmt = stmt->downScope;
        if (stmt->flags & SIF_SCOPE)
            tc->blockChain = stmt->blockObj->enclosingBlock();
    }
}

JSBool
frontend::PopStatementBCE(JSContext *cx, BytecodeEmitter *bce)
{
    StmtInfo *stmt = bce->topStmt;
    if (!STMT_IS_TRYING(stmt) &&
        (!BackPatch(cx, bce, stmt->breaks, bce->next(), JSOP_GOTO) ||
         !BackPatch(cx, bce, stmt->continues, bce->code(stmt->update), JSOP_GOTO)))
    {
        return JS_FALSE;
    }
    PopStatementTC(bce);
    return JS_TRUE;
}

JSBool
frontend::DefineCompileTimeConstant(JSContext *cx, BytecodeEmitter *bce, JSAtom *atom, ParseNode *pn)
{
    /* XXX just do numbers for now */
    if (pn->isKind(PNK_NUMBER)) {
        if (!bce->constMap.put(atom, NumberValue(pn->pn_dval)))
            return JS_FALSE;
    }
    return JS_TRUE;
}

StmtInfo *
frontend::LexicalLookup(TreeContext *tc, JSAtom *atom, jsint *slotp, StmtInfo *stmt)
{
    if (!stmt)
        stmt = tc->topScopeStmt;
    for (; stmt; stmt = stmt->downScope) {
        if (stmt->type == STMT_WITH)
            break;

        /* Skip "maybe scope" statements that don't contain let bindings. */
        if (!(stmt->flags & SIF_SCOPE))
            continue;

        StaticBlockObject &blockObj = *stmt->blockObj;
        const Shape *shape = blockObj.nativeLookup(tc->parser->context, ATOM_TO_JSID(atom));
        if (shape) {
            JS_ASSERT(shape->hasShortID());

            if (slotp)
                *slotp = blockObj.stackDepth() + shape->shortid();
            return stmt;
        }
    }

    if (slotp)
        *slotp = -1;
    return stmt;
}

/*
 * The function sets vp to NO_CONSTANT when the atom does not corresponds to a
 * name defining a constant.
 */
static JSBool
LookupCompileTimeConstant(JSContext *cx, BytecodeEmitter *bce, JSAtom *atom, Value *constp)
{
    /*
     * Chase down the bce stack, but only until we reach the outermost bce.
     * This enables propagating consts from top-level into switch cases in a
     * function compiled along with the top-level script.
     */
    constp->setMagic(JS_NO_CONSTANT);
    do {
        if (bce->inFunction() || bce->compileAndGo()) {
            /* XXX this will need revising if 'const' becomes block-scoped. */
            StmtInfo *stmt = LexicalLookup(bce, atom, NULL);
            if (stmt)
                return JS_TRUE;

            if (BytecodeEmitter::ConstMap::Ptr p = bce->constMap.lookup(atom)) {
                JS_ASSERT(!p->value.isMagic(JS_NO_CONSTANT));
                *constp = p->value;
                return JS_TRUE;
            }

            /*
             * Try looking in the variable object for a direct property that
             * is readonly and permanent.  We know such a property can't be
             * shadowed by another property on obj's prototype chain, or a
             * with object or catch variable; nor can prop's value be changed,
             * nor can prop be deleted.
             */
            if (bce->inFunction()) {
                if (bce->bindings.hasBinding(cx, atom))
                    break;
            } else {
                JS_ASSERT(bce->compileAndGo());
                JSObject *obj = bce->scopeChain();

                const Shape *shape = obj->nativeLookup(cx, ATOM_TO_JSID(atom));
                if (shape) {
                    /*
                     * We're compiling code that will be executed immediately,
                     * not re-executed against a different scope chain and/or
                     * variable object.  Therefore we can get constant values
                     * from our variable object here.
                     */
                    if (!shape->writable() && !shape->configurable() &&
                        shape->hasDefaultGetter() && shape->hasSlot()) {
                        *constp = obj->nativeGetSlot(shape->slot());
                    }
                }

                if (shape)
                    break;
            }
        }
    } while (bce->parent && (bce = bce->parent->asBytecodeEmitter()));
    return JS_TRUE;
}

static bool
EmitIndex32(JSContext *cx, JSOp op, uint32_t index, BytecodeEmitter *bce)
{
    const size_t len = 1 + UINT32_INDEX_LEN;
    JS_ASSERT(len == size_t(js_CodeSpec[op].length));
    ptrdiff_t offset = EmitCheck(cx, bce, len);
    if (offset < 0)
        return false;

    jsbytecode *next = bce->next();
    next[0] = jsbytecode(op);
    SET_UINT32_INDEX(next, index);
    bce->current->next = next + len;
    UpdateDepth(cx, bce, offset);
    CheckTypeSet(cx, bce, op);
    return true;
}

static bool
EmitIndexOp(JSContext *cx, JSOp op, uint32_t index, BytecodeEmitter *bce)
{
    const size_t len = js_CodeSpec[op].length;
    JS_ASSERT(len >= 1 + UINT32_INDEX_LEN);
    ptrdiff_t offset = EmitCheck(cx, bce, len);
    if (offset < 0)
        return false;

    jsbytecode *next = bce->next();
    next[0] = jsbytecode(op);
    SET_UINT32_INDEX(next, index);
    bce->current->next = next + len;
    UpdateDepth(cx, bce, offset);
    CheckTypeSet(cx, bce, op);
    return true;
}

static bool
EmitAtomOp(JSContext *cx, JSAtom *atom, JSOp op, BytecodeEmitter *bce)
{
    JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);

    if (op == JSOP_GETPROP && atom == cx->runtime->atomState.lengthAtom) {
        /* Specialize length accesses for the interpreter. */
        op = JSOP_LENGTH;
    }

    jsatomid index;
    if (!bce->makeAtomIndex(atom, &index))
        return false;

    return EmitIndexOp(cx, op, index, bce);
}

static bool
EmitAtomOp(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    JS_ASSERT(pn->pn_atom != NULL);
    return EmitAtomOp(cx, pn->pn_atom, op, bce);
}

static bool
EmitAtomIncDec(JSContext *cx, JSAtom *atom, JSOp op, BytecodeEmitter *bce)
{
    JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);
    JS_ASSERT(js_CodeSpec[op].format & (JOF_INC | JOF_DEC));

    jsatomid index;
    if (!bce->makeAtomIndex(atom, &index))
        return false;

    const size_t len = 1 + UINT32_INDEX_LEN + 1;
    JS_ASSERT(size_t(js_CodeSpec[op].length) == len);
    ptrdiff_t offset = EmitCheck(cx, bce, len);
    if (offset < 0)
        return false;

    jsbytecode *next = bce->next();
    next[0] = jsbytecode(op);
    SET_UINT32_INDEX(next, index);
    bce->current->next = next + len;
    UpdateDepth(cx, bce, offset);
    CheckTypeSet(cx, bce, op);
    return true;
}

static bool
EmitFunctionOp(JSContext *cx, JSOp op, uint32_t index, BytecodeEmitter *bce)
{
    return EmitIndex32(cx, op, index, bce);
}

static bool
EmitObjectOp(JSContext *cx, ObjectBox *objbox, JSOp op, BytecodeEmitter *bce)
{
    JS_ASSERT(JOF_OPTYPE(op) == JOF_OBJECT);
    return EmitIndex32(cx, op, bce->objectList.index(objbox), bce);
}

static bool
EmitRegExp(JSContext *cx, uint32_t index, BytecodeEmitter *bce)
{
    return EmitIndex32(cx, JSOP_REGEXP, index, bce);
}

static bool
EmitSlotObjectOp(JSContext *cx, JSOp op, unsigned slot, uint32_t index, BytecodeEmitter *bce)
{
    JS_ASSERT(JOF_OPTYPE(op) == JOF_SLOTOBJECT);

    ptrdiff_t off = EmitN(cx, bce, op, SLOTNO_LEN + UINT32_INDEX_LEN);
    if (off < 0)
        return false;

    jsbytecode *pc = bce->code(off);
    SET_SLOTNO(pc, slot);
    pc += SLOTNO_LEN;
    SET_UINT32_INDEX(pc, index);
    return true;
}

static bool
EmitArguments(JSContext *cx, BytecodeEmitter *bce)
{
    if (!bce->mayOverwriteArguments())
        return Emit1(cx, bce, JSOP_ARGUMENTS) >= 0;
    return EmitAtomOp(cx, cx->runtime->atomState.argumentsAtom, JSOP_NAME, bce);
}

bool
BytecodeEmitter::shouldNoteClosedName(ParseNode *pn)
{
    return !callsEval() && pn->isDefn() && pn->isClosed();
}

/*
 * Adjust the slot for a block local to account for the number of variables
 * that share the same index space with locals. Due to the incremental code
 * generation for top-level script, we do the adjustment via code patching in
 * js::frontend::CompileScript; see comments there.
 *
 * The function returns -1 on failures.
 */
static jsint
AdjustBlockSlot(JSContext *cx, BytecodeEmitter *bce, jsint slot)
{
    JS_ASSERT((jsuint) slot < bce->maxStackDepth);
    if (bce->inFunction()) {
        slot += bce->bindings.countVars();
        if ((unsigned) slot >= SLOTNO_LIMIT) {
            ReportCompileErrorNumber(cx, bce->tokenStream(), NULL, JSREPORT_ERROR,
                                     JSMSG_TOO_MANY_LOCALS);
            slot = -1;
        }
    }
    return slot;
}

static bool
EmitEnterBlock(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, JSOp op)
{
    JS_ASSERT(pn->isKind(PNK_LEXICALSCOPE));
    if (!EmitObjectOp(cx, pn->pn_objbox, op, bce))
        return false;

    StaticBlockObject &blockObj = pn->pn_objbox->object->asStaticBlock();

    int depth = bce->stackDepth -
                (blockObj.slotCount() + ((op == JSOP_ENTERLET1) ? 1 : 0));
    JS_ASSERT(depth >= 0);

    blockObj.setStackDepth(depth);

    int depthPlusFixed = AdjustBlockSlot(cx, bce, depth);
    if (depthPlusFixed < 0)
        return false;

    for (unsigned i = 0; i < blockObj.slotCount(); i++) {
        Definition *dn = blockObj.maybeDefinitionParseNode(i);
        blockObj.poisonDefinitionParseNode(i);

        /* Beware the empty destructuring dummy. */
        if (!dn) {
            JS_ASSERT(i + 1 <= blockObj.slotCount());
            continue;
        }

        JS_ASSERT(dn->isDefn());
        JS_ASSERT(unsigned(dn->frameSlot() + depthPlusFixed) < JS_BIT(16));
        dn->pn_cookie.set(dn->pn_cookie.level(), uint16_t(dn->frameSlot() + depthPlusFixed));
#ifdef DEBUG
        for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
            JS_ASSERT(pnu->pn_lexdef == dn);
            JS_ASSERT(!(pnu->pn_dflags & PND_BOUND));
            JS_ASSERT(pnu->pn_cookie.isFree());
        }
#endif
    }

    /*
     * If clones of this block will have any extensible parents, then the
     * clones must get unique shapes; see the comments for
     * js::Bindings::extensibleParents.
     */
    if ((bce->flags & TCF_FUN_EXTENSIBLE_SCOPE) ||
        bce->bindings.extensibleParents()) {
        Shape *newShape = Shape::setExtensibleParents(cx, blockObj.lastProperty());
        if (!newShape)
            return false;
        blockObj.setLastPropertyInfallible(newShape);
    }

    return true;
}

/*
 * Try to convert a *NAME op to a *GNAME op, which optimizes access to
 * undeclared globals. Return true if a conversion was made.
 *
 * This conversion is not made if we are in strict mode.  In eval code nested
 * within (strict mode) eval code, access to an undeclared "global" might
 * merely be to a binding local to that outer eval:
 *
 *   "use strict";
 *   var x = "global";
 *   eval('var x = "eval"; eval("x");'); // 'eval', not 'global'
 *
 * Outside eval code, access to an undeclared global is a strict mode error:
 *
 *   "use strict";
 *   function foo()
 *   {
 *     undeclared = 17; // throws ReferenceError
 *   }
 *   foo();
 */
static bool
TryConvertToGname(BytecodeEmitter *bce, ParseNode *pn, JSOp *op)
{
    if (bce->compileAndGo() && 
        bce->globalScope->globalObj &&
        !bce->mightAliasLocals() &&
        !pn->isDeoptimized() &&
        !(bce->flags & TCF_STRICT_MODE_CODE)) { 
        switch (*op) {
          case JSOP_NAME:     *op = JSOP_GETGNAME; break;
          case JSOP_SETNAME:  *op = JSOP_SETGNAME; break;
          case JSOP_INCNAME:  *op = JSOP_INCGNAME; break;
          case JSOP_NAMEINC:  *op = JSOP_GNAMEINC; break;
          case JSOP_DECNAME:  *op = JSOP_DECGNAME; break;
          case JSOP_NAMEDEC:  *op = JSOP_GNAMEDEC; break;
          case JSOP_SETCONST:
          case JSOP_DELNAME:
            /* Not supported. */
            return false;
          default: JS_NOT_REACHED("gname");
        }
        return true;
    }
    return false;
}

// Binds a global, given a |dn| that is known to have the PND_GVAR bit, and a pn
// that is |dn| or whose definition is |dn|. |pn->pn_cookie| is an outparam
// that will be free (meaning no binding), or a slot number.
static bool
BindKnownGlobal(JSContext *cx, BytecodeEmitter *bce, ParseNode *dn, ParseNode *pn, JSAtom *atom)
{
    // Cookie is an outparam; make sure caller knew to clear it.
    JS_ASSERT(pn->pn_cookie.isFree());

    if (bce->mightAliasLocals())
        return true;

    GlobalScope *globalScope = bce->globalScope;

    jsatomid index;
    if (dn->pn_cookie.isFree()) {
        // The definition wasn't bound, so find its atom's index in the
        // mapping of defined globals.
        AtomIndexPtr p = globalScope->names.lookup(atom);
        JS_ASSERT(!!p);
        index = p.value();
    } else {
        BytecodeEmitter *globalbce = globalScope->bce;

        // If the definition is bound, and we're in the same bce, we can re-use
        // its cookie.
        if (globalbce == bce) {
            pn->pn_cookie = dn->pn_cookie;
            pn->pn_dflags |= PND_BOUND;
            return true;
        }

        // Otherwise, find the atom's index by using the originating bce's
        // global use table.
        index = globalbce->globalUses[dn->pn_cookie.asInteger()].slot;
    }

    if (!bce->addGlobalUse(atom, index, &pn->pn_cookie))
        return false;

    if (!pn->pn_cookie.isFree())
        pn->pn_dflags |= PND_BOUND;

    return true;
}

// See BindKnownGlobal()'s comment.
static bool
BindGlobal(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, JSAtom *atom)
{
    pn->pn_cookie.makeFree();

    Definition *dn;
    if (pn->isUsed()) {
        dn = pn->pn_lexdef;
    } else {
        if (!pn->isDefn())
            return true;
        dn = (Definition *)pn;
    }

    // Only optimize for defined globals.
    if (!dn->isGlobal())
        return true;

    return BindKnownGlobal(cx, bce, dn, pn, atom);
}

/*
 * BindNameToSlot attempts to optimize name gets and sets to stack slot loads
 * and stores, given the compile-time information in bce and a PNK_NAME node pn.
 * It returns false on error, true on success.
 *
 * The caller can inspect pn->pn_cookie for FREE_UPVAR_COOKIE to tell whether
 * optimization occurred, in which case BindNameToSlot also updated pn->pn_op.
 * If pn->pn_cookie is still FREE_UPVAR_COOKIE on return, pn->pn_op still may
 * have been optimized, e.g., from JSOP_NAME to JSOP_CALLEE.  Whether or not
 * pn->pn_op was modified, if this function finds an argument or local variable
 * name, PND_CONST will be set in pn_dflags for read-only properties after a
 * successful return.
 *
 * NB: if you add more opcodes specialized from JSOP_NAME, etc., don't forget
 * to update the special cases in EmitFor (for-in) and EmitAssignment (= and
 * op=, e.g. +=).
 */
static JSBool
BindNameToSlot(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    Definition *dn;
    JSOp op;
    JSAtom *atom;
    Definition::Kind dn_kind;

    JS_ASSERT(pn->isKind(PNK_NAME));

    /* Idempotency tests come first, since we may be called more than once. */
    if (pn->pn_dflags & PND_BOUND)
        return JS_TRUE;

    /* No cookie initialized for these two, they're pre-bound by definition. */
    JS_ASSERT(!pn->isOp(JSOP_ARGUMENTS) && !pn->isOp(JSOP_CALLEE));

    /*
     * The parser linked all uses (including forward references) to their
     * definitions, unless a with statement or direct eval intervened.
     */
    if (pn->isUsed()) {
        JS_ASSERT(pn->pn_cookie.isFree());
        dn = pn->pn_lexdef;
        JS_ASSERT(dn->isDefn());
        if (pn->isDeoptimized())
            return JS_TRUE;
        pn->pn_dflags |= (dn->pn_dflags & PND_CONST);
    } else {
        if (!pn->isDefn())
            return JS_TRUE;
        dn = (Definition *) pn;
    }

    op = pn->getOp();
    if (op == JSOP_NOP)
        return JS_TRUE;

    JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);
    atom = pn->pn_atom;
    UpvarCookie cookie = dn->pn_cookie;
    dn_kind = dn->kind();

    /*
     * Turn attempts to mutate const-declared bindings into get ops (for
     * pre-increment and pre-decrement ops, our caller will have to emit
     * JSOP_POS, JSOP_ONE, and JSOP_ADD as well).
     *
     * Turn JSOP_DELNAME into JSOP_FALSE if dn is known, as all declared
     * bindings visible to the compiler are permanent in JS unless the
     * declaration originates at top level in eval code.
     */
    switch (op) {
      case JSOP_NAME:
      case JSOP_SETCONST:
        break;
      case JSOP_DELNAME:
        if (dn_kind != Definition::UNKNOWN) {
            if (bce->parser->callerFrame && dn->isTopLevel())
                JS_ASSERT(bce->compileAndGo());
            else
                pn->setOp(JSOP_FALSE);
            pn->pn_dflags |= PND_BOUND;
            return JS_TRUE;
        }
        break;
      default:
        if (pn->isConst()) {
            if (bce->needStrictChecks()) {
                JSAutoByteString name;
                if (!js_AtomToPrintableString(cx, atom, &name) ||
                    !ReportStrictModeError(cx, bce->tokenStream(), bce, pn, JSMSG_READ_ONLY,
                                           name.ptr())) {
                    return JS_FALSE;
                }
            }
            pn->setOp(op = JSOP_NAME);
        }
    }

    if (dn->isGlobal()) {
        if (op == JSOP_NAME) {
            /*
             * If the definition is a defined global, not potentially aliased
             * by a local variable, and not mutating the variable, try and
             * optimize to a fast, unguarded global access.
             */
            if (!pn->pn_cookie.isFree()) {
                pn->setOp(JSOP_GETGNAME);
                pn->pn_dflags |= PND_BOUND;
                return JS_TRUE;
            }
        }

        /*
         * The locally stored cookie here should really come from |pn|, not
         * |dn|. For example, we could have a SETGNAME op's lexdef be a
         * GETGNAME op, and their cookies have very different meanings. As
         * a workaround, just make the cookie free.
         */
        cookie.makeFree();
    }

    if (cookie.isFree()) {
        StackFrame *caller = bce->parser->callerFrame;
        if (caller) {
            JS_ASSERT(bce->compileAndGo());

            /*
             * Don't generate upvars on the left side of a for loop. See
             * bug 470758.
             */
            if (bce->flags & TCF_IN_FOR_INIT)
                return JS_TRUE;

            JS_ASSERT(caller->isScriptFrame());

            /*
             * If this is an eval in the global scope, then unbound variables
             * must be globals, so try to use GNAME ops.
             */
            if (caller->isGlobalFrame() && TryConvertToGname(bce, pn, &op)) {
                jsatomid _;
                if (!bce->makeAtomIndex(atom, &_))
                    return JS_FALSE;

                pn->setOp(op);
                pn->pn_dflags |= PND_BOUND;
                return JS_TRUE;
            }

            /*
             * Out of tricks, so we must rely on PICs to optimize named
             * accesses from direct eval called from function code.
             */
            return JS_TRUE;
        }

        /* Optimize accesses to undeclared globals. */
        if (!bce->mightAliasLocals() && !TryConvertToGname(bce, pn, &op))
            return JS_TRUE;

        jsatomid _;
        if (!bce->makeAtomIndex(atom, &_))
            return JS_FALSE;

        pn->setOp(op);
        pn->pn_dflags |= PND_BOUND;

        return JS_TRUE;
    }

    uint16_t level = cookie.level();
    JS_ASSERT(bce->staticLevel >= level);

    const unsigned skip = bce->staticLevel - level;
    if (skip != 0) {
        JS_ASSERT(bce->inFunction());
        JS_ASSERT_IF(cookie.slot() != UpvarCookie::CALLEE_SLOT, bce->roLexdeps->lookup(atom));
        JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);

        /*
         * If op is a mutating opcode, this upvar's lookup skips too many levels,
         * or the function is heavyweight, we fall back on JSOP_*NAME*.
         */
        if (op != JSOP_NAME)
            return JS_TRUE;
        if (skip >= UpvarCookie::UPVAR_LEVEL_LIMIT)
            return JS_TRUE;
        if (bce->flags & TCF_FUN_HEAVYWEIGHT)
            return JS_TRUE;

        if (!bce->fun()->isFlatClosure())
            return JS_TRUE;

        if (!bce->upvarIndices.ensureMap(cx))
            return JS_FALSE;

        AtomIndexAddPtr p = bce->upvarIndices->lookupForAdd(atom);
        jsatomid index;
        if (p) {
            index = p.value();
        } else {
            if (!bce->bindings.addUpvar(cx, atom))
                return JS_FALSE;

            index = bce->upvarIndices->count();
            if (!bce->upvarIndices->add(p, atom, index))
                return JS_FALSE;

            UpvarCookies &upvarMap = bce->upvarMap;
            /* upvarMap should have the same number of UpvarCookies as there are lexdeps. */
            size_t lexdepCount = bce->roLexdeps->count();

            JS_ASSERT_IF(!upvarMap.empty(), lexdepCount == upvarMap.length());
            if (upvarMap.empty()) {
                /* Lazily initialize the upvar map with exactly the necessary capacity. */
                if (lexdepCount <= upvarMap.sMaxInlineStorage) {
                    JS_ALWAYS_TRUE(upvarMap.growByUninitialized(lexdepCount));
                } else {
                    void *buf = upvarMap.allocPolicy().malloc_(lexdepCount * sizeof(UpvarCookie));
                    if (!buf)
                        return JS_FALSE;
                    upvarMap.replaceRawBuffer(static_cast<UpvarCookie *>(buf), lexdepCount);
                }
                for (size_t i = 0; i < lexdepCount; ++i)
                    upvarMap[i] = UpvarCookie();
            }

            unsigned slot = cookie.slot();
            if (slot != UpvarCookie::CALLEE_SLOT && dn_kind != Definition::ARG) {
                TreeContext *tc = bce;
                do {
                    tc = tc->parent;
                } while (tc->staticLevel != level);
                if (tc->inFunction())
                    slot += tc->fun()->nargs;
            }

            JS_ASSERT(index < upvarMap.length());
            upvarMap[index].set(skip, slot);
        }

        pn->setOp(JSOP_GETFCSLOT);
        JS_ASSERT((index & JS_BITMASK(16)) == index);
        pn->pn_cookie.set(0, index);
        pn->pn_dflags |= PND_BOUND;
        return JS_TRUE;
    }

    /*
     * We are compiling a function body and may be able to optimize name
     * to stack slot. Look for an argument or variable in the function and
     * rewrite pn_op and update pn accordingly.
     */
    switch (dn_kind) {
      case Definition::UNKNOWN:
        return JS_TRUE;

      case Definition::LET:
        switch (op) {
          case JSOP_NAME:     op = JSOP_GETLOCAL; break;
          case JSOP_SETNAME:  op = JSOP_SETLOCAL; break;
          case JSOP_INCNAME:  op = JSOP_INCLOCAL; break;
          case JSOP_NAMEINC:  op = JSOP_LOCALINC; break;
          case JSOP_DECNAME:  op = JSOP_DECLOCAL; break;
          case JSOP_NAMEDEC:  op = JSOP_LOCALDEC; break;
          default: JS_NOT_REACHED("let");
        }
        break;

      case Definition::ARG:
        switch (op) {
          case JSOP_NAME:     op = JSOP_GETARG; break;
          case JSOP_SETNAME:  op = JSOP_SETARG; break;
          case JSOP_INCNAME:  op = JSOP_INCARG; break;
          case JSOP_NAMEINC:  op = JSOP_ARGINC; break;
          case JSOP_DECNAME:  op = JSOP_DECARG; break;
          case JSOP_NAMEDEC:  op = JSOP_ARGDEC; break;
          default: JS_NOT_REACHED("arg");
        }
        JS_ASSERT(!pn->isConst());
        break;

      case Definition::VAR:
        if (dn->isOp(JSOP_CALLEE)) {
            JS_ASSERT(op != JSOP_CALLEE);
            JS_ASSERT((bce->fun()->flags & JSFUN_LAMBDA) && atom == bce->fun()->atom);

            /*
             * Leave pn->isOp(JSOP_NAME) if bce->fun is heavyweight to
             * address two cases: a new binding introduced by eval, and
             * assignment to the name in strict mode.
             *
             *   var fun = (function f(s) { eval(s); return f; });
             *   assertEq(fun("var f = 42"), 42);
             *
             * ECMAScript specifies that a function expression's name is bound
             * in a lexical environment distinct from that used to bind its
             * named parameters, the arguments object, and its variables.  The
             * new binding for "var f = 42" shadows the binding for the
             * function itself, so the name of the function will not refer to
             * the function.
             *
             *    (function f() { "use strict"; f = 12; })();
             *
             * Outside strict mode, assignment to a function expression's name
             * has no effect.  But in strict mode, this attempt to mutate an
             * immutable binding must throw a TypeError.  We implement this by
             * not optimizing such assignments and by marking such functions as
             * heavyweight, ensuring that the function name is represented in
             * the scope chain so that assignment will throw a TypeError.
             */
            JS_ASSERT(op != JSOP_DELNAME);
            if (!(bce->flags & TCF_FUN_HEAVYWEIGHT)) {
                op = JSOP_CALLEE;
                pn->pn_dflags |= PND_CONST;
            }

            pn->setOp(op);
            pn->pn_dflags |= PND_BOUND;
            return JS_TRUE;
        }
        /* FALL THROUGH */

      default:
        JS_ASSERT_IF(dn_kind != Definition::FUNCTION,
                     dn_kind == Definition::VAR ||
                     dn_kind == Definition::CONST);
        switch (op) {
          case JSOP_NAME:     op = JSOP_GETLOCAL; break;
          case JSOP_SETNAME:  op = JSOP_SETLOCAL; break;
          case JSOP_SETCONST: op = JSOP_SETLOCAL; break;
          case JSOP_INCNAME:  op = JSOP_INCLOCAL; break;
          case JSOP_NAMEINC:  op = JSOP_LOCALINC; break;
          case JSOP_DECNAME:  op = JSOP_DECLOCAL; break;
          case JSOP_NAMEDEC:  op = JSOP_LOCALDEC; break;
          default: JS_NOT_REACHED("local");
        }
        JS_ASSERT_IF(dn_kind == Definition::CONST, pn->pn_dflags & PND_CONST);
        break;
    }

    JS_ASSERT(!pn->isOp(op));
    pn->setOp(op);
    pn->pn_cookie.set(0, cookie.slot());
    pn->pn_dflags |= PND_BOUND;
    return JS_TRUE;
}

bool
BytecodeEmitter::addGlobalUse(JSAtom *atom, uint32_t slot, UpvarCookie *cookie)
{
    if (!globalMap.ensureMap(context()))
        return false;

    AtomIndexAddPtr p = globalMap->lookupForAdd(atom);
    if (p) {
        jsatomid index = p.value();
        cookie->set(0, index);
        return true;
    }

    /* Don't bother encoding indexes >= uint16_t */
    if (globalUses.length() >= UINT16_LIMIT) {
        cookie->makeFree();
        return true;
    }

    /* Find or add an existing atom table entry. */
    jsatomid allAtomIndex;
    if (!makeAtomIndex(atom, &allAtomIndex))
        return false;

    jsatomid globalUseIndex = globalUses.length();
    cookie->set(0, globalUseIndex);

    GlobalSlotArray::Entry entry = { allAtomIndex, slot };
    if (!globalUses.append(entry))
        return false;

    return globalMap->add(p, atom, globalUseIndex);
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
CheckSideEffects(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, JSBool *answer)
{
    JSBool ok;
    ParseNode *pn2;

    ok = JS_TRUE;
    if (!pn || *answer)
        return ok;

    switch (pn->getArity()) {
      case PN_FUNC:
        /*
         * A named function, contrary to ES3, is no longer useful, because we
         * bind its name lexically (using JSOP_CALLEE) instead of creating an
         * Object instance and binding a readonly, permanent property in it
         * (the object and binding can be detected and hijacked or captured).
         * This is a bug fix to ES3; it is fixed in ES3.1 drafts.
         */
        *answer = JS_FALSE;
        break;

      case PN_LIST:
        if (pn->isOp(JSOP_NOP) || pn->isOp(JSOP_OR) || pn->isOp(JSOP_AND) ||
            pn->isOp(JSOP_STRICTEQ) || pn->isOp(JSOP_STRICTNE)) {
            /*
             * Non-operators along with ||, &&, ===, and !== never invoke
             * toString or valueOf.
             */
            for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next)
                ok &= CheckSideEffects(cx, bce, pn2, answer);
        } else {
            /*
             * All invocation operations (construct: PNK_NEW, call: PNK_LP)
             * are presumed to be useful, because they may have side effects
             * even if their main effect (their return value) is discarded.
             *
             * PNK_LB binary trees of 3 or more nodes are flattened into lists
             * to avoid too much recursion.  All such lists must be presumed
             * to be useful because each index operation could invoke a getter
             * (the JSOP_ARGUMENTS special case below, in the PN_BINARY case,
             * does not apply here: arguments[i][j] might invoke a getter).
             *
             * Likewise, array and object initialisers may call prototype
             * setters (the __defineSetter__ built-in, and writable __proto__
             * on Array.prototype create this hazard). Initialiser list nodes
             * have JSOP_NEWINIT in their pn_op.
             */
            *answer = JS_TRUE;
        }
        break;

      case PN_TERNARY:
        ok = CheckSideEffects(cx, bce, pn->pn_kid1, answer) &&
             CheckSideEffects(cx, bce, pn->pn_kid2, answer) &&
             CheckSideEffects(cx, bce, pn->pn_kid3, answer);
        break;

      case PN_BINARY:
        if (pn->isAssignment()) {
            /*
             * Assignment is presumed to be useful, even if the next operation
             * is another assignment overwriting this one's ostensible effect,
             * because the left operand may be a property with a setter that
             * has side effects.
             *
             * The only exception is assignment of a useless value to a const
             * declared in the function currently being compiled.
             */
            pn2 = pn->pn_left;
            if (!pn2->isKind(PNK_NAME)) {
                *answer = JS_TRUE;
            } else {
                if (!BindNameToSlot(cx, bce, pn2))
                    return JS_FALSE;
                if (!CheckSideEffects(cx, bce, pn->pn_right, answer))
                    return JS_FALSE;
                if (!*answer && (!pn->isOp(JSOP_NOP) || !pn2->isConst()))
                    *answer = JS_TRUE;
            }
        } else {
            if (pn->isOp(JSOP_OR) || pn->isOp(JSOP_AND) || pn->isOp(JSOP_STRICTEQ) ||
                pn->isOp(JSOP_STRICTNE)) {
                /*
                 * ||, &&, ===, and !== do not convert their operands via
                 * toString or valueOf method calls.
                 */
                ok = CheckSideEffects(cx, bce, pn->pn_left, answer) &&
                     CheckSideEffects(cx, bce, pn->pn_right, answer);
            } else {
                /*
                 * We can't easily prove that neither operand ever denotes an
                 * object with a toString or valueOf method.
                 */
                *answer = JS_TRUE;
            }
        }
        break;

      case PN_UNARY:
        switch (pn->getKind()) {
          case PNK_DELETE:
            pn2 = pn->pn_kid;
            switch (pn2->getKind()) {
              case PNK_NAME:
                if (!BindNameToSlot(cx, bce, pn2))
                    return JS_FALSE;
                if (pn2->isConst()) {
                    *answer = JS_FALSE;
                    break;
                }
                /* FALL THROUGH */
              case PNK_DOT:
#if JS_HAS_XML_SUPPORT
              case PNK_DBLDOT:
                JS_ASSERT_IF(pn2->getKind() == PNK_DBLDOT, !bce->inStrictMode());
                /* FALL THROUGH */

#endif
              case PNK_LP:
              case PNK_LB:
                /* All these delete addressing modes have effects too. */
                *answer = JS_TRUE;
                break;
              default:
                ok = CheckSideEffects(cx, bce, pn2, answer);
                break;
            }
            break;

          case PNK_TYPEOF:
          case PNK_VOID:
          case PNK_NOT:
          case PNK_BITNOT:
            if (pn->isOp(JSOP_NOT)) {
                /* ! does not convert its operand via toString or valueOf. */
                ok = CheckSideEffects(cx, bce, pn->pn_kid, answer);
                break;
            }
            /* FALL THROUGH */

          default:
            /*
             * All of PNK_INC, PNK_DEC, PNK_THROW, and PNK_YIELD have direct
             * effects. Of the remaining unary-arity node types, we can't
             * easily prove that the operand never denotes an object with a
             * toString or valueOf method.
             */
            *answer = JS_TRUE;
            break;
        }
        break;

      case PN_NAME:
        /*
         * Take care to avoid trying to bind a label name (labels, both for
         * statements and property values in object initialisers, have pn_op
         * defaulted to JSOP_NOP).
         */
        if (pn->isKind(PNK_NAME) && !pn->isOp(JSOP_NOP)) {
            if (!BindNameToSlot(cx, bce, pn))
                return JS_FALSE;
            if (!pn->isOp(JSOP_ARGUMENTS) && !pn->isOp(JSOP_CALLEE) &&
                pn->pn_cookie.isFree()) {
                /*
                 * Not an argument or local variable use, and not a use of a
                 * unshadowed named function expression's given name, so this
                 * expression could invoke a getter that has side effects.
                 */
                *answer = JS_TRUE;
            }
        }
        pn2 = pn->maybeExpr();
        if (pn->isKind(PNK_DOT)) {
            if (pn2->isKind(PNK_NAME) && !BindNameToSlot(cx, bce, pn2))
                return JS_FALSE;
            if (!(pn2->isOp(JSOP_ARGUMENTS) &&
                  pn->pn_atom == cx->runtime->atomState.lengthAtom)) {
                /*
                 * Any dotted property reference could call a getter, except
                 * for arguments.length where arguments is unambiguous.
                 */
                *answer = JS_TRUE;
            }
        }
        ok = CheckSideEffects(cx, bce, pn2, answer);
        break;

      case PN_NAMESET:
        ok = CheckSideEffects(cx, bce, pn->pn_tree, answer);
        break;

      case PN_NULLARY:
        if (pn->isKind(PNK_DEBUGGER))
            *answer = JS_TRUE;
        break;
    }
    return ok;
}

bool
BytecodeEmitter::needsImplicitThis()
{
    if (!compileAndGo())
        return true;
    if (!inFunction()) {
        JSObject *scope = scopeChain();
        while (scope) {
            if (scope->isWith())
                return true;
            scope = scope->enclosingScope();
        }
    }
    for (const FunctionBox *funbox = this->funbox; funbox; funbox = funbox->parent) {
        if (funbox->tcflags & TCF_IN_WITH)
            return true;
    }
    for (StmtInfo *stmt = topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == STMT_WITH)
            return true;
    }
    return false;
}

static JSBool
EmitNameOp(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, JSBool callContext)
{
    JSOp op;

    if (!BindNameToSlot(cx, bce, pn))
        return JS_FALSE;
    op = pn->getOp();

    if (callContext) {
        switch (op) {
          case JSOP_NAME:
            op = JSOP_CALLNAME;
            break;
          case JSOP_GETGNAME:
            op = JSOP_CALLGNAME;
            break;
          case JSOP_GETARG:
            op = JSOP_CALLARG;
            break;
          case JSOP_GETLOCAL:
            op = JSOP_CALLLOCAL;
            break;
          case JSOP_GETFCSLOT:
            op = JSOP_CALLFCSLOT;
            break;
          default:
            JS_ASSERT(op == JSOP_ARGUMENTS || op == JSOP_CALLEE);
            break;
        }
    }

    if (op == JSOP_ARGUMENTS) {
        if (!EmitArguments(cx, bce))
            return JS_FALSE;
    } else if (op == JSOP_CALLEE) {
        if (Emit1(cx, bce, op) < 0)
            return JS_FALSE;
    } else {
        if (!pn->pn_cookie.isFree()) {
            JS_ASSERT(JOF_OPTYPE(op) != JOF_ATOM);
            EMIT_UINT16_IMM_OP(op, pn->pn_cookie.asInteger());
        } else {
            if (!EmitAtomOp(cx, pn, op, bce))
                return JS_FALSE;
        }
    }

    /* Need to provide |this| value for call */
    if (callContext) {
        if (op == JSOP_CALLNAME && bce->needsImplicitThis()) {
            if (!EmitAtomOp(cx, pn, JSOP_IMPLICITTHIS, bce))
                return false;
        } else {
            if (Emit1(cx, bce, JSOP_UNDEFINED) < 0)
                return false;
        }
    }

    return JS_TRUE;
}

#if JS_HAS_XML_SUPPORT
static bool
EmitXMLName(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    JS_ASSERT(!bce->inStrictMode());
    JS_ASSERT(pn->isKind(PNK_XMLUNARY));
    JS_ASSERT(pn->isOp(JSOP_XMLNAME));
    JS_ASSERT(op == JSOP_XMLNAME || op == JSOP_CALLXMLNAME);

    ParseNode *pn2 = pn->pn_kid;
    unsigned oldflags = bce->flags;
    bce->flags &= ~TCF_IN_FOR_INIT;
    if (!EmitTree(cx, bce, pn2))
        return false;
    bce->flags |= oldflags & TCF_IN_FOR_INIT;
    if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - pn2->pn_offset) < 0)
        return false;

    if (Emit1(cx, bce, op) < 0)
        return false;

    return true;
}
#endif

static inline bool
EmitElemOpBase(JSContext *cx, BytecodeEmitter *bce, JSOp op)
{
    if (Emit1(cx, bce, op) < 0)
        return false;
    CheckTypeSet(cx, bce, op);
    if (op == JSOP_CALLELEM)
        return Emit1(cx, bce, JSOP_SWAP) >= 0;
    return true;
}

static bool
EmitSpecialPropOp(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    /*
     * Special case for obj.__proto__ to deoptimize away from fast paths in the
     * interpreter and trace recorder, which skip dense array instances by
     * going up to Array.prototype before looking up the property name.
     */
    jsatomid index;
    if (!bce->makeAtomIndex(pn->pn_atom, &index))
        return false;
    if (!EmitIndex32(cx, JSOP_QNAMEPART, index, bce))
        return false;

    if (op == JSOP_CALLELEM && Emit1(cx, bce, JSOP_DUP) < 0)
        return false;

    if (!EmitElemOpBase(cx, bce, op))
        return false;

    if (op == JSOP_CALLELEM && Emit1(cx, bce, JSOP_SWAP) < 0)
        return false;

    return true;
}

static bool
EmitPropOp(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce,
           JSBool callContext)
{
    ParseNode *pn2, *pndot, *pnup, *pndown;
    ptrdiff_t top;

    JS_ASSERT(pn->isArity(PN_NAME));
    pn2 = pn->maybeExpr();

    /* Special case deoptimization for __proto__. */
    if ((op == JSOP_GETPROP || op == JSOP_CALLPROP) &&
        pn->pn_atom == cx->runtime->atomState.protoAtom) {
        if (pn2 && !EmitTree(cx, bce, pn2))
            return false;
        return EmitSpecialPropOp(cx, pn, callContext ? JSOP_CALLELEM : JSOP_GETELEM, bce);
    }

    if (callContext) {
        JS_ASSERT(pn->isKind(PNK_DOT));
        JS_ASSERT(op == JSOP_GETPROP);
        op = JSOP_CALLPROP;
    } else if (op == JSOP_GETPROP && pn->isKind(PNK_DOT)) {
        if (pn2->isKind(PNK_NAME)) {
            if (!BindNameToSlot(cx, bce, pn2))
                return false;
        }
    }

    /*
     * If the object operand is also a dotted property reference, reverse the
     * list linked via pn_expr temporarily so we can iterate over it from the
     * bottom up (reversing again as we go), to avoid excessive recursion.
     */
    if (pn2->isKind(PNK_DOT)) {
        pndot = pn2;
        pnup = NULL;
        top = bce->offset();
        for (;;) {
            /* Reverse pndot->pn_expr to point up, not down. */
            pndot->pn_offset = top;
            JS_ASSERT(!pndot->isUsed());
            pndown = pndot->pn_expr;
            pndot->pn_expr = pnup;
            if (!pndown->isKind(PNK_DOT))
                break;
            pnup = pndot;
            pndot = pndown;
        }

        /* pndown is a primary expression, not a dotted property reference. */
        if (!EmitTree(cx, bce, pndown))
            return false;

        do {
            /* Walk back up the list, emitting annotated name ops. */
            if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - pndown->pn_offset) < 0)
                return false;

            /* Special case deoptimization on __proto__, as above. */
            if (pndot->isArity(PN_NAME) && pndot->pn_atom == cx->runtime->atomState.protoAtom) {
                if (!EmitSpecialPropOp(cx, pndot, JSOP_GETELEM, bce))
                    return false;
            } else if (!EmitAtomOp(cx, pndot, pndot->getOp(), bce)) {
                return false;
            }

            /* Reverse the pn_expr link again. */
            pnup = pndot->pn_expr;
            pndot->pn_expr = pndown;
            pndown = pndot;
        } while ((pndot = pnup) != NULL);
    } else {
        if (!EmitTree(cx, bce, pn2))
            return false;
    }

    if (op == JSOP_CALLPROP && Emit1(cx, bce, JSOP_DUP) < 0)
        return false;

    if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - pn2->pn_offset) < 0)
        return false;

    if (!EmitAtomOp(cx, pn, op, bce))
        return false;

    if (op == JSOP_CALLPROP && Emit1(cx, bce, JSOP_SWAP) < 0)
        return false;

    return true;
}

static bool
EmitPropIncDec(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    if (!EmitPropOp(cx, pn, op, bce, false))
        return false;

    /*
     * The stack is the same depth before/after INCPROP, so no balancing to do
     * before the decomposed version.
     */
    int start = bce->offset();

    const JSCodeSpec *cs = &js_CodeSpec[op];
    JS_ASSERT(cs->format & JOF_PROP);
    JS_ASSERT(cs->format & (JOF_INC | JOF_DEC));

    bool post = (cs->format & JOF_POST);
    JSOp binop = (cs->format & JOF_INC) ? JSOP_ADD : JSOP_SUB;

                                                    // OBJ
    if (Emit1(cx, bce, JSOP_DUP) < 0)               // OBJ OBJ
        return false;
    if (!EmitAtomOp(cx, pn, JSOP_GETPROP, bce))     // OBJ V
        return false;
    if (Emit1(cx, bce, JSOP_POS) < 0)               // OBJ N
        return false;
    if (post && Emit1(cx, bce, JSOP_DUP) < 0)       // OBJ N? N
        return false;
    if (Emit1(cx, bce, JSOP_ONE) < 0)               // OBJ N? N 1
        return false;
    if (Emit1(cx, bce, binop) < 0)                  // OBJ N? N+1
        return false;

    if (post) {
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)2) < 0)   // N? N+1 OBJ
            return false;
        if (Emit1(cx, bce, JSOP_SWAP) < 0)                  // N? OBJ N+1
            return false;
    }

    if (!EmitAtomOp(cx, pn, JSOP_SETPROP, bce))     // N? N+1
        return false;
    if (post && Emit1(cx, bce, JSOP_POP) < 0)       // RESULT
        return false;

    UpdateDecomposeLength(bce, start);

    return true;
}

static bool
EmitNameIncDec(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    /* Emit the composite op, including the slack byte at the end. */
    if (!EmitAtomIncDec(cx, pn->pn_atom, op, bce))
        return false;

    /* Remove the result to restore the stack depth before the INCNAME. */
    bce->stackDepth--;

    int start = bce->offset();

    const JSCodeSpec *cs = &js_CodeSpec[op];
    JS_ASSERT((cs->format & JOF_NAME) || (cs->format & JOF_GNAME));
    JS_ASSERT(cs->format & (JOF_INC | JOF_DEC));

    bool global = (cs->format & JOF_GNAME);
    bool post = (cs->format & JOF_POST);
    JSOp binop = (cs->format & JOF_INC) ? JSOP_ADD : JSOP_SUB;

    if (!EmitAtomOp(cx, pn, global ? JSOP_BINDGNAME : JSOP_BINDNAME, bce))  // OBJ
        return false;
    if (!EmitAtomOp(cx, pn, global ? JSOP_GETGNAME : JSOP_NAME, bce))       // OBJ V
        return false;
    if (Emit1(cx, bce, JSOP_POS) < 0)               // OBJ N
        return false;
    if (post && Emit1(cx, bce, JSOP_DUP) < 0)       // OBJ N? N
        return false;
    if (Emit1(cx, bce, JSOP_ONE) < 0)               // OBJ N? N 1
        return false;
    if (Emit1(cx, bce, binop) < 0)                  // OBJ N? N+1
        return false;

    if (post) {
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)2) < 0)   // N? N+1 OBJ
            return false;
        if (Emit1(cx, bce, JSOP_SWAP) < 0)                  // N? OBJ N+1
            return false;
    }

    if (!EmitAtomOp(cx, pn, global ? JSOP_SETGNAME : JSOP_SETNAME, bce))    // N? N+1
        return false;
    if (post && Emit1(cx, bce, JSOP_POP) < 0)       // RESULT
        return false;

    UpdateDecomposeLength(bce, start);

    return true;
}

static JSBool
EmitElemOp(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    ParseNode *left, *right;

    ptrdiff_t top = bce->offset();

    if (pn->isArity(PN_NAME)) {
        /*
         * Set left and right so pn appears to be a PNK_LB node, instead
         * of a PNK_DOT node.  See the PNK_FOR/IN case in EmitTree, and
         * EmitDestructuringOps nearer below.  In the destructuring case,
         * the base expression (pn_expr) of the name may be null, which
         * means we have to emit a JSOP_BINDNAME.
         */
        left = pn->maybeExpr();
        if (!left) {
            left = NullaryNode::create(PNK_STRING, bce);
            if (!left)
                return false;
            left->setOp(JSOP_BINDNAME);
            left->pn_pos = pn->pn_pos;
            left->pn_atom = pn->pn_atom;
        }
        right = NullaryNode::create(PNK_STRING, bce);
        if (!right)
            return false;
        right->setOp(IsIdentifier(pn->pn_atom) ? JSOP_QNAMEPART : JSOP_STRING);
        right->pn_pos = pn->pn_pos;
        right->pn_atom = pn->pn_atom;
    } else {
        JS_ASSERT(pn->isArity(PN_BINARY));
        left = pn->pn_left;
        right = pn->pn_right;
    }

    if (op == JSOP_GETELEM && left->isKind(PNK_NAME) && right->isKind(PNK_NUMBER)) {
        if (!BindNameToSlot(cx, bce, left))
            return false;
    }

    if (!EmitTree(cx, bce, left))
        return false;

    if (op == JSOP_CALLELEM && Emit1(cx, bce, JSOP_DUP) < 0)
        return false;

    /* The right side of the descendant operator is implicitly quoted. */
    JS_ASSERT(op != JSOP_DESCENDANTS || !right->isKind(PNK_STRING) ||
              right->isOp(JSOP_QNAMEPART));
    if (!EmitTree(cx, bce, right))
        return false;
    if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - top) < 0)
        return false;
    return EmitElemOpBase(cx, bce, op);
}

static bool
EmitElemIncDec(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    if (pn) {
        if (!EmitElemOp(cx, pn, op, bce))
            return false;
    } else {
        if (!EmitElemOpBase(cx, bce, op))
            return false;
    }
    if (Emit1(cx, bce, JSOP_NOP) < 0)
        return false;

    /* INCELEM pops two values and pushes one, so restore the initial depth. */
    bce->stackDepth++;

    int start = bce->offset();

    const JSCodeSpec *cs = &js_CodeSpec[op];
    JS_ASSERT(cs->format & JOF_ELEM);
    JS_ASSERT(cs->format & (JOF_INC | JOF_DEC));

    bool post = (cs->format & JOF_POST);
    JSOp binop = (cs->format & JOF_INC) ? JSOP_ADD : JSOP_SUB;

    /*
     * We need to convert the key to an object id first, so that we do not do
     * it inside both the GETELEM and the SETELEM.
     */
                                                    // OBJ KEY*
    if (Emit1(cx, bce, JSOP_TOID) < 0)              // OBJ KEY
        return false;
    if (Emit1(cx, bce, JSOP_DUP2) < 0)              // OBJ KEY OBJ KEY
        return false;
    if (!EmitElemOpBase(cx, bce, JSOP_GETELEM))     // OBJ KEY V
        return false;
    if (Emit1(cx, bce, JSOP_POS) < 0)               // OBJ KEY N
        return false;
    if (post && Emit1(cx, bce, JSOP_DUP) < 0)       // OBJ KEY N? N
        return false;
    if (Emit1(cx, bce, JSOP_ONE) < 0)               // OBJ KEY N? N 1
        return false;
    if (Emit1(cx, bce, binop) < 0)                  // OBJ KEY N? N+1
        return false;

    if (post) {
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)3) < 0)   // KEY N N+1 OBJ
            return false;
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)3) < 0)   // N N+1 OBJ KEY
            return false;
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)2) < 0)   // N OBJ KEY N+1
            return false;
    }

    if (!EmitElemOpBase(cx, bce, JSOP_SETELEM))     // N? N+1
        return false;
    if (post && Emit1(cx, bce, JSOP_POP) < 0)       // RESULT
        return false;

    UpdateDecomposeLength(bce, start);

    return true;
}

static JSBool
EmitNumberOp(JSContext *cx, double dval, BytecodeEmitter *bce)
{
    int32_t ival;
    uint32_t u;
    ptrdiff_t off;
    jsbytecode *pc;

    if (JSDOUBLE_IS_INT32(dval, &ival)) {
        if (ival == 0)
            return Emit1(cx, bce, JSOP_ZERO) >= 0;
        if (ival == 1)
            return Emit1(cx, bce, JSOP_ONE) >= 0;
        if ((jsint)(int8_t)ival == ival)
            return Emit2(cx, bce, JSOP_INT8, (jsbytecode)(int8_t)ival) >= 0;

        u = (uint32_t)ival;
        if (u < JS_BIT(16)) {
            EMIT_UINT16_IMM_OP(JSOP_UINT16, u);
        } else if (u < JS_BIT(24)) {
            off = EmitN(cx, bce, JSOP_UINT24, 3);
            if (off < 0)
                return JS_FALSE;
            pc = bce->code(off);
            SET_UINT24(pc, u);
        } else {
            off = EmitN(cx, bce, JSOP_INT32, 4);
            if (off < 0)
                return JS_FALSE;
            pc = bce->code(off);
            SET_INT32(pc, ival);
        }
        return JS_TRUE;
    }

    if (!bce->constList.append(DoubleValue(dval)))
        return JS_FALSE;

    return EmitIndex32(cx, JSOP_DOUBLE, bce->constList.length() - 1, bce);
}

/*
 * To avoid bloating all parse nodes for the special case of switch, values are
 * allocated in the temp pool and pointed to by the parse node. These values
 * are not currently recycled (like parse nodes) and the temp pool is only
 * flushed at the end of compiling a script, so these values are technically
 * leaked. This would only be a problem for scripts containing a large number
 * of large switches, which seems unlikely.
 */
static Value *
AllocateSwitchConstant(JSContext *cx)
{
    return cx->tempLifoAlloc().new_<Value>();
}

static inline void
SetJumpOffsetAt(BytecodeEmitter *bce, ptrdiff_t off)
{
    SET_JUMP_OFFSET(bce->code(off), bce->offset() - off);
}

/*
 * Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr12127.
 * LLVM is deciding to inline this function which uses a lot of stack space
 * into EmitTree which is recursive and uses relatively little stack space.
 */
MOZ_NEVER_INLINE static JSBool
EmitSwitch(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JSOp switchOp;
    JSBool ok, hasDefault, constPropagated;
    ptrdiff_t top, off, defaultOffset;
    ParseNode *pn2, *pn3, *pn4;
    uint32_t caseCount, tableLength;
    ParseNode **table;
    int32_t i, low, high;
    int noteIndex;
    size_t switchSize, tableSize;
    jsbytecode *pc, *savepc;
    StmtInfo stmtInfo;

    /* Try for most optimal, fall back if not dense ints, and per ECMAv2. */
    switchOp = JSOP_TABLESWITCH;
    ok = JS_TRUE;
    hasDefault = constPropagated = JS_FALSE;
    defaultOffset = -1;

    pn2 = pn->pn_right;
#if JS_HAS_BLOCK_SCOPE
    /*
     * If there are hoisted let declarations, their stack slots go under the
     * discriminant's value so push their slots now and enter the block later.
     */
    uint32_t blockObjCount = 0;
    if (pn2->isKind(PNK_LEXICALSCOPE)) {
        blockObjCount = pn2->pn_objbox->object->asStaticBlock().slotCount();
        for (uint32_t i = 0; i < blockObjCount; ++i) {
            if (Emit1(cx, bce, JSOP_UNDEFINED) < 0)
                return JS_FALSE;
        }
    }
#endif

    /* Push the discriminant. */
    if (!EmitTree(cx, bce, pn->pn_left))
        return JS_FALSE;

#if JS_HAS_BLOCK_SCOPE
    if (pn2->isKind(PNK_LEXICALSCOPE)) {
        PushBlockScope(bce, &stmtInfo, pn2->pn_objbox->object->asStaticBlock(), -1);
        stmtInfo.type = STMT_SWITCH;
        if (!EmitEnterBlock(cx, bce, pn2, JSOP_ENTERLET1))
            return JS_FALSE;
    }
#endif

    /* Switch bytecodes run from here till end of final case. */
    top = bce->offset();
#if !JS_HAS_BLOCK_SCOPE
    PushStatement(bce, &stmtInfo, STMT_SWITCH, top);
#else
    if (pn2->isKind(PNK_STATEMENTLIST)) {
        PushStatement(bce, &stmtInfo, STMT_SWITCH, top);
    } else {
        /*
         * Set the statement info record's idea of top. Reset top too, since
         * repushBlock emits code.
         */
        stmtInfo.update = top = bce->offset();

        /* Advance pn2 to refer to the switch case list. */
        pn2 = pn2->expr();
    }
#endif

    caseCount = pn2->pn_count;
    tableLength = 0;
    table = NULL;

    if (caseCount == 0 ||
        (caseCount == 1 &&
         (hasDefault = (pn2->pn_head->isKind(PNK_DEFAULT))))) {
        caseCount = 0;
        low = 0;
        high = -1;
    } else {
#define INTMAP_LENGTH   256
        jsbitmap intmap_space[INTMAP_LENGTH];
        jsbitmap *intmap = NULL;
        int32_t intmap_bitlen = 0;

        low  = JSVAL_INT_MAX;
        high = JSVAL_INT_MIN;

        for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            if (pn3->isKind(PNK_DEFAULT)) {
                hasDefault = JS_TRUE;
                caseCount--;    /* one of the "cases" was the default */
                continue;
            }

            JS_ASSERT(pn3->isKind(PNK_CASE));
            if (switchOp == JSOP_CONDSWITCH)
                continue;

            pn4 = pn3->pn_left;
            while (pn4->isKind(PNK_RP))
                pn4 = pn4->pn_kid;

            Value constVal;
            switch (pn4->getKind()) {
              case PNK_NUMBER:
                constVal.setNumber(pn4->pn_dval);
                break;
              case PNK_STRING:
                constVal.setString(pn4->pn_atom);
                break;
              case PNK_TRUE:
                constVal.setBoolean(true);
                break;
              case PNK_FALSE:
                constVal.setBoolean(false);
                break;
              case PNK_NULL:
                constVal.setNull();
                break;
              case PNK_NAME:
                if (!pn4->maybeExpr()) {
                    ok = LookupCompileTimeConstant(cx, bce, pn4->pn_atom, &constVal);
                    if (!ok)
                        goto release;
                    if (!constVal.isMagic(JS_NO_CONSTANT)) {
                        if (constVal.isObject()) {
                            /*
                             * XXX JSOP_LOOKUPSWITCH does not support const-
                             * propagated object values, see bug 407186.
                             */
                            switchOp = JSOP_CONDSWITCH;
                            continue;
                        }
                        constPropagated = JS_TRUE;
                        break;
                    }
                }
                /* FALL THROUGH */
              default:
                switchOp = JSOP_CONDSWITCH;
                continue;
            }
            JS_ASSERT(constVal.isPrimitive());

            pn3->pn_pval = AllocateSwitchConstant(cx);
            if (!pn3->pn_pval) {
                ok = JS_FALSE;
                goto release;
            }

            *pn3->pn_pval = constVal;

            if (switchOp != JSOP_TABLESWITCH)
                continue;
            if (!pn3->pn_pval->isInt32()) {
                switchOp = JSOP_LOOKUPSWITCH;
                continue;
            }
            i = pn3->pn_pval->toInt32();
            if ((jsuint)(i + (jsint)JS_BIT(15)) >= (jsuint)JS_BIT(16)) {
                switchOp = JSOP_LOOKUPSWITCH;
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
                    intmap = (jsbitmap *)
                        cx->malloc_((JS_BIT(16) >> JS_BITS_PER_WORD_LOG2)
                                   * sizeof(jsbitmap));
                    if (!intmap) {
                        JS_ReportOutOfMemory(cx);
                        return JS_FALSE;
                    }
                }
                memset(intmap, 0, intmap_bitlen >> JS_BITS_PER_BYTE_LOG2);
            }
            if (JS_TEST_BIT(intmap, i)) {
                switchOp = JSOP_LOOKUPSWITCH;
                continue;
            }
            JS_SET_BIT(intmap, i);
        }

      release:
        if (intmap && intmap != intmap_space)
            cx->free_(intmap);
        if (!ok)
            return JS_FALSE;

        /*
         * Compute table length and select lookup instead if overlarge or
         * more than half-sparse.
         */
        if (switchOp == JSOP_TABLESWITCH) {
            tableLength = (uint32_t)(high - low + 1);
            if (tableLength >= JS_BIT(16) || tableLength > 2 * caseCount)
                switchOp = JSOP_LOOKUPSWITCH;
        } else if (switchOp == JSOP_LOOKUPSWITCH) {
            /*
             * Lookup switch supports only atom indexes below 64K limit.
             * Conservatively estimate the maximum possible index during
             * switch generation and use conditional switch if it exceeds
             * the limit.
             */
            if (caseCount + bce->constList.length() > JS_BIT(16))
                switchOp = JSOP_CONDSWITCH;
        }
    }

    /*
     * Emit a note with two offsets: first tells total switch code length,
     * second tells offset to first JSOP_CASE if condswitch.
     */
    noteIndex = NewSrcNote3(cx, bce, SRC_SWITCH, 0, 0);
    if (noteIndex < 0)
        return JS_FALSE;

    if (switchOp == JSOP_CONDSWITCH) {
        /*
         * 0 bytes of immediate for unoptimized ECMAv2 switch.
         */
        switchSize = 0;
    } else if (switchOp == JSOP_TABLESWITCH) {
        /*
         * 3 offsets (len, low, high) before the table, 1 per entry.
         */
        switchSize = (size_t)(JUMP_OFFSET_LEN * (3 + tableLength));
    } else {
        /*
         * JSOP_LOOKUPSWITCH:
         * 1 offset (len) and 1 atom index (npairs) before the table,
         * 1 atom index and 1 jump offset per entry.
         */
        switchSize = (size_t)(JUMP_OFFSET_LEN + UINT16_LEN +
                              (UINT32_INDEX_LEN + JUMP_OFFSET_LEN) * caseCount);
    }

    /* Emit switchOp followed by switchSize bytes of jump or lookup table. */
    if (EmitN(cx, bce, switchOp, switchSize) < 0)
        return JS_FALSE;

    off = -1;
    if (switchOp == JSOP_CONDSWITCH) {
        int caseNoteIndex = -1;
        JSBool beforeCases = JS_TRUE;

        /* Emit code for evaluating cases and jumping to case statements. */
        for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            pn4 = pn3->pn_left;
            if (pn4 && !EmitTree(cx, bce, pn4))
                return JS_FALSE;
            if (caseNoteIndex >= 0) {
                /* off is the previous JSOP_CASE's bytecode offset. */
                if (!SetSrcNoteOffset(cx, bce, (unsigned)caseNoteIndex, 0, bce->offset() - off))
                    return JS_FALSE;
            }
            if (!pn4) {
                JS_ASSERT(pn3->isKind(PNK_DEFAULT));
                continue;
            }
            caseNoteIndex = NewSrcNote2(cx, bce, SRC_PCDELTA, 0);
            if (caseNoteIndex < 0)
                return JS_FALSE;
            off = EmitJump(cx, bce, JSOP_CASE, 0);
            if (off < 0)
                return JS_FALSE;
            pn3->pn_offset = off;
            if (beforeCases) {
                unsigned noteCount, noteCountDelta;

                /* Switch note's second offset is to first JSOP_CASE. */
                noteCount = bce->noteCount();
                if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 1, off - top))
                    return JS_FALSE;
                noteCountDelta = bce->noteCount() - noteCount;
                if (noteCountDelta != 0)
                    caseNoteIndex += noteCountDelta;
                beforeCases = JS_FALSE;
            }
        }

        /*
         * If we didn't have an explicit default (which could fall in between
         * cases, preventing us from fusing this SetSrcNoteOffset with the call
         * in the loop above), link the last case to the implicit default for
         * the decompiler.
         */
        if (!hasDefault &&
            caseNoteIndex >= 0 &&
            !SetSrcNoteOffset(cx, bce, (unsigned)caseNoteIndex, 0, bce->offset() - off))
        {
            return JS_FALSE;
        }

        /* Emit default even if no explicit default statement. */
        defaultOffset = EmitJump(cx, bce, JSOP_DEFAULT, 0);
        if (defaultOffset < 0)
            return JS_FALSE;
    } else {
        pc = bce->code(top + JUMP_OFFSET_LEN);

        if (switchOp == JSOP_TABLESWITCH) {
            /* Fill in switch bounds, which we know fit in 16-bit offsets. */
            SET_JUMP_OFFSET(pc, low);
            pc += JUMP_OFFSET_LEN;
            SET_JUMP_OFFSET(pc, high);
            pc += JUMP_OFFSET_LEN;

            /*
             * Use malloc to avoid arena bloat for programs with many switches.
             * We free table if non-null at label out, so all control flow must
             * exit this function through goto out or goto bad.
             */
            if (tableLength != 0) {
                tableSize = (size_t)tableLength * sizeof *table;
                table = (ParseNode **) cx->malloc_(tableSize);
                if (!table)
                    return JS_FALSE;
                memset(table, 0, tableSize);
                for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                    if (pn3->isKind(PNK_DEFAULT))
                        continue;
                    i = pn3->pn_pval->toInt32();
                    i -= low;
                    JS_ASSERT((uint32_t)i < tableLength);
                    table[i] = pn3;
                }
            }
        } else {
            JS_ASSERT(switchOp == JSOP_LOOKUPSWITCH);

            /* Fill in the number of cases. */
            SET_UINT16(pc, caseCount);
            pc += UINT16_LEN;
        }

        /*
         * After this point, all control flow involving JSOP_TABLESWITCH
         * must set ok and goto out to exit this function.  To keep things
         * simple, all switchOp cases exit that way.
         */
        MUST_FLOW_THROUGH("out");

        if (constPropagated) {
            /*
             * Skip switchOp, as we are not setting jump offsets in the two
             * for loops below.  We'll restore bce->next() from savepc after,
             * unless there was an error.
             */
            savepc = bce->next();
            bce->current->next = pc + 1;
            if (switchOp == JSOP_TABLESWITCH) {
                for (i = 0; i < (jsint)tableLength; i++) {
                    pn3 = table[i];
                    if (pn3 &&
                        (pn4 = pn3->pn_left) != NULL &&
                        pn4->isKind(PNK_NAME))
                    {
                        /* Note a propagated constant with the const's name. */
                        JS_ASSERT(!pn4->maybeExpr());
                        jsatomid index;
                        if (!bce->makeAtomIndex(pn4->pn_atom, &index))
                            goto bad;
                        bce->current->next = pc;
                        if (NewSrcNote2(cx, bce, SRC_LABEL, ptrdiff_t(index)) < 0)
                            goto bad;
                    }
                    pc += JUMP_OFFSET_LEN;
                }
            } else {
                for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                    pn4 = pn3->pn_left;
                    if (pn4 && pn4->isKind(PNK_NAME)) {
                        /* Note a propagated constant with the const's name. */
                        JS_ASSERT(!pn4->maybeExpr());
                        jsatomid index;
                        if (!bce->makeAtomIndex(pn4->pn_atom, &index))
                            goto bad;
                        bce->current->next = pc;
                        if (NewSrcNote2(cx, bce, SRC_LABEL, ptrdiff_t(index)) < 0)
                            goto bad;
                    }
                    pc += UINT32_INDEX_LEN + JUMP_OFFSET_LEN;
                }
            }
            bce->current->next = savepc;
        }
    }

    /* Emit code for each case's statements, copying pn_offset up to pn3. */
    for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
        if (switchOp == JSOP_CONDSWITCH && !pn3->isKind(PNK_DEFAULT))
            SetJumpOffsetAt(bce, pn3->pn_offset);
        pn4 = pn3->pn_right;
        ok = EmitTree(cx, bce, pn4);
        if (!ok)
            goto out;
        pn3->pn_offset = pn4->pn_offset;
        if (pn3->isKind(PNK_DEFAULT))
            off = pn3->pn_offset - top;
    }

    if (!hasDefault) {
        /* If no default case, offset for default is to end of switch. */
        off = bce->offset() - top;
    }

    /* We better have set "off" by now. */
    JS_ASSERT(off != -1);

    /* Set the default offset (to end of switch if no default). */
    if (switchOp == JSOP_CONDSWITCH) {
        pc = NULL;
        JS_ASSERT(defaultOffset != -1);
        SET_JUMP_OFFSET(bce->code(defaultOffset), off - (defaultOffset - top));
    } else {
        pc = bce->code(top);
        SET_JUMP_OFFSET(pc, off);
        pc += JUMP_OFFSET_LEN;
    }

    /* Set the SRC_SWITCH note's offset operand to tell end of switch. */
    off = bce->offset() - top;
    ok = SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 0, off);
    if (!ok)
        goto out;

    if (switchOp == JSOP_TABLESWITCH) {
        /* Skip over the already-initialized switch bounds. */
        pc += 2 * JUMP_OFFSET_LEN;

        /* Fill in the jump table, if there is one. */
        for (i = 0; i < (jsint)tableLength; i++) {
            pn3 = table[i];
            off = pn3 ? pn3->pn_offset - top : 0;
            SET_JUMP_OFFSET(pc, off);
            pc += JUMP_OFFSET_LEN;
        }
    } else if (switchOp == JSOP_LOOKUPSWITCH) {
        /* Skip over the already-initialized number of cases. */
        pc += UINT16_LEN;

        for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            if (pn3->isKind(PNK_DEFAULT))
                continue;
            if (!bce->constList.append(*pn3->pn_pval))
                goto bad;
            SET_UINT32_INDEX(pc, bce->constList.length() - 1);
            pc += UINT32_INDEX_LEN;

            off = pn3->pn_offset - top;
            SET_JUMP_OFFSET(pc, off);
            pc += JUMP_OFFSET_LEN;
        }
    }

out:
    if (table)
        cx->free_(table);
    if (ok) {
        ok = PopStatementBCE(cx, bce);

#if JS_HAS_BLOCK_SCOPE
        if (ok && pn->pn_right->isKind(PNK_LEXICALSCOPE))
            EMIT_UINT16_IMM_OP(JSOP_LEAVEBLOCK, blockObjCount);
#endif
    }
    return ok;

bad:
    ok = JS_FALSE;
    goto out;
}

JSBool
frontend::EmitFunctionScript(JSContext *cx, BytecodeEmitter *bce, ParseNode *body)
{
    /*
     * The decompiler has assumptions about what may occur immediately after
     * script->main (e.g., in the case of destructuring params). Thus, put the
     * following ops into the range [script->code, script->main). Note:
     * execution starts from script->code, so this has no semantic effect.
     */

    if (bce->flags & TCF_FUN_IS_GENERATOR) {
        /* JSOP_GENERATOR must be the first instruction. */
        bce->switchToProlog();
        JS_ASSERT(bce->next() == bce->base());
        if (Emit1(cx, bce, JSOP_GENERATOR) < 0)
            return false;
        bce->switchToMain();
    }

    /*
     * Strict mode functions' arguments objects copy initial parameter values.
     * We create arguments objects lazily -- but that doesn't work for strict
     * mode functions where a parameter might be modified and arguments might
     * be accessed. For such functions we synthesize an access to arguments to
     * initialize it with the original parameter values.
     */
    if (bce->needsEagerArguments()) {
        bce->switchToProlog();
        if (Emit1(cx, bce, JSOP_ARGUMENTS) < 0 || Emit1(cx, bce, JSOP_POP) < 0)
            return false;
        bce->switchToMain();
    }

    return EmitTree(cx, bce, body) &&
           Emit1(cx, bce, JSOP_STOP) >= 0 &&
           JSScript::NewScriptFromEmitter(cx, bce);
}

static bool
MaybeEmitVarDecl(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn,
                 jsatomid *result)
{
    jsatomid atomIndex;

    if (!pn->pn_cookie.isFree()) {
        atomIndex = pn->pn_cookie.slot();
    } else {
        if (!bce->makeAtomIndex(pn->pn_atom, &atomIndex))
            return false;
    }

    if (JOF_OPTYPE(pn->getOp()) == JOF_ATOM &&
        (!bce->inFunction() || (bce->flags & TCF_FUN_HEAVYWEIGHT)) &&
        !(pn->pn_dflags & PND_GVAR))
    {
        bce->switchToProlog();
        if (!UpdateLineNumberNotes(cx, bce, pn->pn_pos.begin.lineno))
            return false;
        if (!EmitIndexOp(cx, prologOp, atomIndex, bce))
            return false;
        bce->switchToMain();
    }

    if (bce->inFunction() &&
        JOF_OPTYPE(pn->getOp()) == JOF_LOCAL &&
        pn->pn_cookie.slot() < bce->bindings.countVars() &&
        bce->shouldNoteClosedName(pn))
    {
        if (!bce->closedVars.append(pn->pn_cookie.slot()))
            return false;
    }

    if (result)
        *result = atomIndex;
    return true;
}

/*
 * This enum tells EmitVariables and the destructuring functions how emit the
 * given Parser::variables parse tree. In the base case, DefineVars, the caller
 * only wants variables to be defined in the prologue (if necessary). For
 * PushInitialValues, variable initializer expressions are evaluated and left
 * on the stack. For InitializeVars, the initializer expressions values are
 * assigned (to local variables) and popped.
 */
enum VarEmitOption
{
    DefineVars        = 0,
    PushInitialValues = 1,
    InitializeVars    = 2
};

#if JS_HAS_DESTRUCTURING

typedef JSBool
(*DestructuringDeclEmitter)(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn);

static JSBool
EmitDestructuringDecl(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_NAME));
    if (!BindNameToSlot(cx, bce, pn))
        return JS_FALSE;

    JS_ASSERT(!pn->isOp(JSOP_ARGUMENTS) && !pn->isOp(JSOP_CALLEE));
    return MaybeEmitVarDecl(cx, bce, prologOp, pn, NULL);
}

static JSBool
EmitDestructuringDecls(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn)
{
    ParseNode *pn2, *pn3;
    DestructuringDeclEmitter emitter;

    if (pn->isKind(PNK_RB)) {
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (pn2->isKind(PNK_COMMA))
                continue;
            emitter = (pn2->isKind(PNK_NAME))
                      ? EmitDestructuringDecl
                      : EmitDestructuringDecls;
            if (!emitter(cx, bce, prologOp, pn2))
                return JS_FALSE;
        }
    } else {
        JS_ASSERT(pn->isKind(PNK_RC));
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            pn3 = pn2->pn_right;
            emitter = pn3->isKind(PNK_NAME) ? EmitDestructuringDecl : EmitDestructuringDecls;
            if (!emitter(cx, bce, prologOp, pn3))
                return JS_FALSE;
        }
    }
    return JS_TRUE;
}

static JSBool
EmitDestructuringOpsHelper(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn,
                           VarEmitOption emitOption);

/*
 * EmitDestructuringLHS assumes the to-be-destructured value has been pushed on
 * the stack and emits code to destructure a single lhs expression (either a
 * name or a compound []/{} expression).
 *
 * If emitOption is InitializeVars, the to-be-destructured value is assigned to
 * locals and ultimately the initial slot is popped (-1 total depth change).
 *
 * If emitOption is PushInitialValues, the to-be-destructured value is replaced
 * with the initial values of the N (where 0 <= N) variables assigned in the
 * lhs expression. (Same post-condition as EmitDestructuringOpsHelper)
 */
static JSBool
EmitDestructuringLHS(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, VarEmitOption emitOption)
{
    JS_ASSERT(emitOption != DefineVars);

    /*
     * Now emit the lvalue opcode sequence.  If the lvalue is a nested
     * destructuring initialiser-form, call ourselves to handle it, then
     * pop the matched value.  Otherwise emit an lvalue bytecode sequence
     * ending with a JSOP_ENUMELEM or equivalent op.
     */
    if (pn->isKind(PNK_RB) || pn->isKind(PNK_RC)) {
        if (!EmitDestructuringOpsHelper(cx, bce, pn, emitOption))
            return JS_FALSE;
        if (emitOption == InitializeVars) {
            /*
             * Per its post-condition, EmitDestructuringOpsHelper has left the
             * to-be-destructured value on top of the stack.
             */
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
        }
    } else {
        if (emitOption == PushInitialValues) {
            /*
             * The lhs is a simple name so the to-be-destructured value is
             * its initial value and there is nothing to do.
             */
            JS_ASSERT(pn->getOp() == JSOP_SETLOCAL);
            JS_ASSERT(pn->pn_dflags & PND_BOUND);
            return JS_TRUE;
        }

        /* All paths below must pop after assigning to the lhs. */

        if (pn->isKind(PNK_NAME)) {
            if (!BindNameToSlot(cx, bce, pn))
                return JS_FALSE;
            if (pn->isConst() && !pn->isInitialized())
                return Emit1(cx, bce, JSOP_POP) >= 0;
        }

        switch (pn->getOp()) {
          case JSOP_SETNAME:
          case JSOP_SETGNAME:
            /*
             * NB: pn is a PN_NAME node, not a PN_BINARY.  Nevertheless,
             * we want to emit JSOP_ENUMELEM, which has format JOF_ELEM.
             * So here and for JSOP_ENUMCONSTELEM, we use EmitElemOp.
             */
            if (!EmitElemOp(cx, pn, JSOP_ENUMELEM, bce))
                return JS_FALSE;
            break;

          case JSOP_SETCONST:
            if (!EmitElemOp(cx, pn, JSOP_ENUMCONSTELEM, bce))
                return JS_FALSE;
            break;

          case JSOP_SETLOCAL:
          {
            jsuint slot = pn->pn_cookie.asInteger();
            EMIT_UINT16_IMM_OP(JSOP_SETLOCALPOP, slot);
            break;
          }

          case JSOP_SETARG:
          {
            jsuint slot = pn->pn_cookie.asInteger();
            EMIT_UINT16_IMM_OP(pn->getOp(), slot);
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
            break;
          }

          default:
          {
            ptrdiff_t top;

            top = bce->offset();
            if (!EmitTree(cx, bce, pn))
                return JS_FALSE;
            if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - top) < 0)
                return JS_FALSE;
            if (!EmitElemOpBase(cx, bce, JSOP_ENUMELEM))
                return JS_FALSE;
            break;
          }

          case JSOP_ENUMELEM:
            JS_ASSERT(0);
        }
    }

    return JS_TRUE;
}

/*
 * Recursive helper for EmitDestructuringOps.
 * EmitDestructuringOpsHelper assumes the to-be-destructured value has been
 * pushed on the stack and emits code to destructure each part of a [] or {}
 * lhs expression.
 *
 * If emitOption is InitializeVars, the initial to-be-destructured value is
 * left untouched on the stack and the overall depth is not changed.
 *
 * If emitOption is PushInitialValues, the to-be-destructured value is replaced
 * with the initial values of the N (where 0 <= N) variables assigned in the
 * lhs expression. (Same post-condition as EmitDestructuringLHS)
 */
static JSBool
EmitDestructuringOpsHelper(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn,
                           VarEmitOption emitOption)
{
    JS_ASSERT(emitOption != DefineVars);

    jsuint index;
    ParseNode *pn2, *pn3;
    JSBool doElemOp;

#ifdef DEBUG
    int stackDepth = bce->stackDepth;
    JS_ASSERT(stackDepth != 0);
    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(pn->isKind(PNK_RB) || pn->isKind(PNK_RC));
#endif

    if (pn->pn_count == 0) {
        /* Emit a DUP;POP sequence for the decompiler. */
        if (Emit1(cx, bce, JSOP_DUP) < 0 || Emit1(cx, bce, JSOP_POP) < 0)
            return JS_FALSE;
    }

    index = 0;
    for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
        /*
         * Duplicate the value being destructured to use as a reference base.
         * If dup is not the first one, annotate it for the decompiler.
         */
        if (pn2 != pn->pn_head && NewSrcNote(cx, bce, SRC_CONTINUE) < 0)
            return JS_FALSE;
        if (Emit1(cx, bce, JSOP_DUP) < 0)
            return JS_FALSE;

        /*
         * Now push the property name currently being matched, which is either
         * the array initialiser's current index, or the current property name
         * "label" on the left of a colon in the object initialiser.  Set pn3
         * to the lvalue node, which is in the value-initializing position.
         */
        doElemOp = JS_TRUE;
        if (pn->isKind(PNK_RB)) {
            if (!EmitNumberOp(cx, index, bce))
                return JS_FALSE;
            pn3 = pn2;
        } else {
            JS_ASSERT(pn->isKind(PNK_RC));
            JS_ASSERT(pn2->isKind(PNK_COLON));
            pn3 = pn2->pn_left;
            if (pn3->isKind(PNK_NUMBER)) {
                /*
                 * If we are emitting an object destructuring initialiser,
                 * annotate the index op with SRC_INITPROP so we know we are
                 * not decompiling an array initialiser.
                 */
                if (NewSrcNote(cx, bce, SRC_INITPROP) < 0)
                    return JS_FALSE;
                if (!EmitNumberOp(cx, pn3->pn_dval, bce))
                    return JS_FALSE;
            } else {
                JS_ASSERT(pn3->isKind(PNK_STRING) || pn3->isKind(PNK_NAME));
                if (!EmitAtomOp(cx, pn3, JSOP_GETPROP, bce))
                    return JS_FALSE;
                doElemOp = JS_FALSE;
            }
            pn3 = pn2->pn_right;
        }

        if (doElemOp) {
            /*
             * Ok, get the value of the matching property name.  This leaves
             * that value on top of the value being destructured, so the stack
             * is one deeper than when we started.
             */
            if (!EmitElemOpBase(cx, bce, JSOP_GETELEM))
                return JS_FALSE;
            JS_ASSERT(bce->stackDepth >= stackDepth + 1);
        }

        /* Nullary comma node makes a hole in the array destructurer. */
        if (pn3->isKind(PNK_COMMA) && pn3->isArity(PN_NULLARY)) {
            JS_ASSERT(pn->isKind(PNK_RB));
            JS_ASSERT(pn2 == pn3);
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
        } else {
            int depthBefore = bce->stackDepth;
            if (!EmitDestructuringLHS(cx, bce, pn3, emitOption))
                return JS_FALSE;

            if (emitOption == PushInitialValues) {
                /*
                 * After '[x,y]' in 'let ([[x,y], z] = o)', the stack is
                 *   | to-be-decompiled-value | x | y |
                 * The goal is:
                 *   | x | y | z |
                 * so emit a pick to produce the intermediate state
                 *   | x | y | to-be-decompiled-value |
                 * before destructuring z. This gives the loop invariant that
                 * the to-be-compiled-value is always on top of the stack.
                 */
                JS_ASSERT((bce->stackDepth - bce->stackDepth) >= -1);
                unsigned pickDistance = (unsigned)((bce->stackDepth + 1) - depthBefore);
                if (pickDistance > 0) {
                    if (pickDistance > UINT8_MAX) {
                        ReportCompileErrorNumber(cx, bce->tokenStream(), pn3, JSREPORT_ERROR,
                                                 JSMSG_TOO_MANY_LOCALS);
                        return JS_FALSE;
                    }
                    if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)pickDistance) < 0)
                        return false;
                }
            }
        }

        ++index;
    }

    if (emitOption == PushInitialValues) {
        /*
         * Per the above loop invariant, to-be-decompiled-value is at the top
         * of the stack. To achieve the post-condition, pop it.
         */
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return JS_FALSE;
    }

    return JS_TRUE;
}

static ptrdiff_t
OpToDeclType(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
        return SRC_DECL_LET;
      case JSOP_DEFCONST:
        return SRC_DECL_CONST;
      case JSOP_DEFVAR:
        return SRC_DECL_VAR;
      default:
        return SRC_DECL_NONE;
    }
}

/*
 * This utility accumulates a set of SRC_DESTRUCTLET notes which need to be
 * backpatched with the offset from JSOP_DUP to JSOP_LET0.
 *
 * Also record whether the let head was a group assignment ([x,y] = [a,b])
 * (which implies no SRC_DESTRUCTLET notes).
 */
class LetNotes
{
    struct Pair {
        ptrdiff_t dup;
        unsigned index;
        Pair(ptrdiff_t dup, unsigned index) : dup(dup), index(index) {}
    };
    Vector<Pair> notes;
    bool groupAssign;
    DebugOnly<bool> updateCalled;

  public:
    LetNotes(JSContext *cx) : notes(cx), groupAssign(false), updateCalled(false) {}

    ~LetNotes() {
        JS_ASSERT_IF(!notes.allocPolicy().context()->isExceptionPending(), updateCalled);
    }

    void setGroupAssign() {
        JS_ASSERT(notes.empty());
        groupAssign = true;
    }

    bool isGroupAssign() const {
        return groupAssign;
    }

    bool append(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t dup, unsigned index) {
        JS_ASSERT(!groupAssign);
        JS_ASSERT(SN_TYPE(bce->notes() + index) == SRC_DESTRUCTLET);
        if (!notes.append(Pair(dup, index)))
            return false;

        /*
         * Pessimistically inflate each srcnote. That way, there is no danger
         * of inflation during update() (which would invalidate all indices).
         */
        if (!SetSrcNoteOffset(cx, bce, index, 0, SN_MAX_OFFSET))
            return false;
        JS_ASSERT(bce->notes()[index + 1] & SN_3BYTE_OFFSET_FLAG);
        return true;
    }

    /* This should be called exactly once, right before JSOP_ENTERLET0. */
    bool update(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t offset) {
        JS_ASSERT(!updateCalled);
        for (size_t i = 0; i < notes.length(); ++i) {
            JS_ASSERT(offset > notes[i].dup);
            JS_ASSERT(*bce->code(notes[i].dup) == JSOP_DUP);
            JS_ASSERT(bce->notes()[notes[i].index + 1] & SN_3BYTE_OFFSET_FLAG);
            if (!SetSrcNoteOffset(cx, bce, notes[i].index, 0, offset - notes[i].dup))
                return false;
        }
        updateCalled = true;
        return true;
    }
};

static JSBool
EmitDestructuringOps(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t declType, ParseNode *pn,
                     LetNotes *letNotes = NULL)
{
    /*
     * If we're called from a variable declaration, help the decompiler by
     * annotating the first JSOP_DUP that EmitDestructuringOpsHelper emits.
     * If the destructuring initialiser is empty, our helper will emit a
     * JSOP_DUP followed by a JSOP_POP for the decompiler.
     */
    if (letNotes) {
        ptrdiff_t index = NewSrcNote2(cx, bce, SRC_DESTRUCTLET, 0);
        if (index < 0 || !letNotes->append(cx, bce, bce->offset(), (unsigned)index))
            return JS_FALSE;
    } else {
        if (NewSrcNote2(cx, bce, SRC_DESTRUCT, declType) < 0)
            return JS_FALSE;
    }

    /*
     * Call our recursive helper to emit the destructuring assignments and
     * related stack manipulations.
     */
    VarEmitOption emitOption = letNotes ? PushInitialValues : InitializeVars;
    return EmitDestructuringOpsHelper(cx, bce, pn, emitOption);
}

static JSBool
EmitGroupAssignment(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp,
                    ParseNode *lhs, ParseNode *rhs)
{
    jsuint depth, limit, i, nslots;
    ParseNode *pn;

    depth = limit = (unsigned) bce->stackDepth;
    for (pn = rhs->pn_head; pn; pn = pn->pn_next) {
        if (limit == JS_BIT(16)) {
            ReportCompileErrorNumber(cx, bce->tokenStream(), rhs, JSREPORT_ERROR,
                                     JSMSG_ARRAY_INIT_TOO_BIG);
            return JS_FALSE;
        }

        /* MaybeEmitGroupAssignment won't call us if rhs is holey. */
        JS_ASSERT(!(pn->isKind(PNK_COMMA) && pn->isArity(PN_NULLARY)));
        if (!EmitTree(cx, bce, pn))
            return JS_FALSE;
        ++limit;
    }

    if (NewSrcNote2(cx, bce, SRC_GROUPASSIGN, OpToDeclType(prologOp)) < 0)
        return JS_FALSE;

    i = depth;
    for (pn = lhs->pn_head; pn; pn = pn->pn_next, ++i) {
        /* MaybeEmitGroupAssignment requires lhs->pn_count <= rhs->pn_count. */
        JS_ASSERT(i < limit);
        jsint slot = AdjustBlockSlot(cx, bce, i);
        if (slot < 0)
            return JS_FALSE;
        EMIT_UINT16_IMM_OP(JSOP_GETLOCAL, slot);

        if (pn->isKind(PNK_COMMA) && pn->isArity(PN_NULLARY)) {
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
        } else {
            if (!EmitDestructuringLHS(cx, bce, pn, InitializeVars))
                return JS_FALSE;
        }
    }

    nslots = limit - depth;
    EMIT_UINT16_IMM_OP(JSOP_POPN, nslots);
    bce->stackDepth = (unsigned) depth;
    return JS_TRUE;
}

/*
 * Helper called with pop out param initialized to a JSOP_POP* opcode.  If we
 * can emit a group assignment sequence, which results in 0 stack depth delta,
 * we set *pop to JSOP_NOP so callers can veto emitting pn followed by a pop.
 */
static JSBool
MaybeEmitGroupAssignment(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn,
                         JSOp *pop)
{
    JS_ASSERT(pn->isKind(PNK_ASSIGN));
    JS_ASSERT(pn->isOp(JSOP_NOP));
    JS_ASSERT(*pop == JSOP_POP || *pop == JSOP_POPV);

    ParseNode *lhs = pn->pn_left;
    ParseNode *rhs = pn->pn_right;
    if (lhs->isKind(PNK_RB) && rhs->isKind(PNK_RB) &&
        !(rhs->pn_xflags & PNX_HOLEY) &&
        lhs->pn_count <= rhs->pn_count) {
        if (!EmitGroupAssignment(cx, bce, prologOp, lhs, rhs))
            return JS_FALSE;
        *pop = JSOP_NOP;
    }
    return JS_TRUE;
}

/*
 * Like MaybeEmitGroupAssignment, but for 'let ([x,y] = [a,b]) ...'.
 *
 * Instead of issuing a sequence |dup|eval-rhs|set-lhs|pop| (which doesn't work
 * since the bound vars don't yet have slots), just eval/push each rhs element
 * just like what EmitLet would do for 'let (x = a, y = b) ...'. While shorter,
 * simpler and more efficient than MaybeEmitGroupAssignment, it is harder to
 * decompile so we restrict the ourselves to cases where the lhs and rhs are in
 * 1:1 correspondence and lhs elements are simple names.
 */
static bool
MaybeEmitLetGroupDecl(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn,
                      LetNotes *letNotes, JSOp *pop)
{
    JS_ASSERT(pn->isKind(PNK_ASSIGN));
    JS_ASSERT(pn->isOp(JSOP_NOP));
    JS_ASSERT(*pop == JSOP_POP || *pop == JSOP_POPV);

    ParseNode *lhs = pn->pn_left;
    ParseNode *rhs = pn->pn_right;
    if (lhs->isKind(PNK_RB) && rhs->isKind(PNK_RB) &&
        !(rhs->pn_xflags & PNX_HOLEY) &&
        !(lhs->pn_xflags & PNX_HOLEY) &&
        lhs->pn_count == rhs->pn_count)
    {
        for (ParseNode *l = lhs->pn_head; l; l = l->pn_next) {
            if (l->getOp() != JSOP_SETLOCAL)
                return true;
        }

        for (ParseNode *r = rhs->pn_head; r; r = r->pn_next) {
            if (!EmitTree(cx, bce, r))
                return false;
        }

        letNotes->setGroupAssign();
        *pop = JSOP_NOP;
    }
    return true;
}

#endif /* JS_HAS_DESTRUCTURING */

static JSBool
EmitVariables(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, VarEmitOption emitOption,
              LetNotes *letNotes = NULL)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(!!letNotes == (emitOption == PushInitialValues));

    ptrdiff_t off = -1, noteIndex = -1;
    ParseNode *next;
    for (ParseNode *pn2 = pn->pn_head; ; pn2 = next) {
        bool first = pn2 == pn->pn_head;
        next = pn2->pn_next;

        ParseNode *pn3;
        if (!pn2->isKind(PNK_NAME)) {
#if JS_HAS_DESTRUCTURING
            if (pn2->isKind(PNK_RB) || pn2->isKind(PNK_RC)) {
                /*
                 * Emit variable binding ops, but not destructuring ops.  The
                 * parser (see Parser::variables) has ensured that our caller
                 * will be the PNK_FOR/PNK_FORIN case in EmitTree, and that
                 * case will emit the destructuring code only after emitting an
                 * enumerating opcode and a branch that tests whether the
                 * enumeration ended.
                 */
                JS_ASSERT(emitOption == DefineVars);
                JS_ASSERT(pn->pn_count == 1);
                if (!EmitDestructuringDecls(cx, bce, pn->getOp(), pn2))
                    return JS_FALSE;
                break;
            }
#endif

            /*
             * A destructuring initialiser assignment preceded by var will
             * never occur to the left of 'in' in a for-in loop.  As with 'for
             * (var x = i in o)...', this will cause the entire 'var [a, b] =
             * i' to be hoisted out of the loop.
             */
            JS_ASSERT(pn2->isKind(PNK_ASSIGN));
            JS_ASSERT(pn2->isOp(JSOP_NOP));
            JS_ASSERT(emitOption != DefineVars);

            /*
             * To allow the front end to rewrite var f = x; as f = x; when a
             * function f(){} precedes the var, detect simple name assignment
             * here and initialize the name.
             */
#if !JS_HAS_DESTRUCTURING
            JS_ASSERT(pn2->pn_left->isKind(PNK_NAME));
#else
            if (pn2->pn_left->isKind(PNK_NAME))
#endif
            {
                pn3 = pn2->pn_right;
                pn2 = pn2->pn_left;
                goto do_name;
            }

#if JS_HAS_DESTRUCTURING
            ptrdiff_t stackDepthBefore = bce->stackDepth;
            JSOp op = JSOP_POP;
            if (pn->pn_count == 1) {
                /*
                 * If this is the only destructuring assignment in the list,
                 * try to optimize to a group assignment.  If we're in a let
                 * head, pass JSOP_POP rather than the pseudo-prolog JSOP_NOP
                 * in pn->pn_op, to suppress a second (and misplaced) 'let'.
                 */
                JS_ASSERT(noteIndex < 0 && !pn2->pn_next);
                if (letNotes) {
                    if (!MaybeEmitLetGroupDecl(cx, bce, pn2, letNotes, &op))
                        return JS_FALSE;
                } else {
                    if (!MaybeEmitGroupAssignment(cx, bce, pn->getOp(), pn2, &op))
                        return JS_FALSE;
                }
            }
            if (op == JSOP_NOP) {
                pn->pn_xflags = (pn->pn_xflags & ~PNX_POPVAR) | PNX_GROUPINIT;
            } else {
                pn3 = pn2->pn_left;
                if (!EmitDestructuringDecls(cx, bce, pn->getOp(), pn3))
                    return JS_FALSE;

                if (!EmitTree(cx, bce, pn2->pn_right))
                    return JS_FALSE;

                /* Only the first list element should print 'let' or 'var'. */
                ptrdiff_t declType = pn2 == pn->pn_head
                                     ? OpToDeclType(pn->getOp())
                                     : SRC_DECL_NONE;

                if (!EmitDestructuringOps(cx, bce, declType, pn3, letNotes))
                    return JS_FALSE;
            }
            ptrdiff_t stackDepthAfter = bce->stackDepth;

            /* Give let ([] = x) a slot (see CheckDestructuring). */
            JS_ASSERT(stackDepthBefore <= stackDepthAfter);
            if (letNotes && stackDepthBefore == stackDepthAfter) {
                if (Emit1(cx, bce, JSOP_UNDEFINED) < 0)
                    return JS_FALSE;
            }

            /* If we are not initializing, nothing to pop. */
            if (emitOption != InitializeVars) {
                if (next)
                    continue;
                break;
            }
            goto emit_note_pop;
#endif
        }

        /*
         * Load initializer early to share code above that jumps to do_name.
         * NB: if this var redeclares an existing binding, then pn2 is linked
         * on its definition's use-chain and pn_expr has been overlayed with
         * pn_lexdef.
         */
        pn3 = pn2->maybeExpr();

     do_name:
        if (!BindNameToSlot(cx, bce, pn2))
            return JS_FALSE;

        JSOp op;
        jsatomid atomIndex;

        op = pn2->getOp();
        if (op == JSOP_ARGUMENTS) {
            /* JSOP_ARGUMENTS => no initializer */
            JS_ASSERT(!pn3 && !letNotes);
            pn3 = NULL;
            atomIndex = 0;
        } else {
            JS_ASSERT(op != JSOP_CALLEE);
            JS_ASSERT(!pn2->pn_cookie.isFree() || !pn->isOp(JSOP_NOP));
            if (!MaybeEmitVarDecl(cx, bce, pn->getOp(), pn2, &atomIndex))
                return JS_FALSE;

            if (pn3) {
                JS_ASSERT(emitOption != DefineVars);
                JS_ASSERT_IF(emitOption == PushInitialValues, op == JSOP_SETLOCAL);
                if (op == JSOP_SETNAME || op == JSOP_SETGNAME) {
                    JSOp bindOp = (op == JSOP_SETNAME) ? JSOP_BINDNAME : JSOP_BINDGNAME;
                    if (!EmitIndex32(cx, bindOp, atomIndex, bce))
                        return false;
                }
                if (pn->isOp(JSOP_DEFCONST) &&
                    !DefineCompileTimeConstant(cx, bce, pn2->pn_atom, pn3))
                {
                    return JS_FALSE;
                }

                unsigned oldflags = bce->flags;
                bce->flags &= ~TCF_IN_FOR_INIT;
                if (!EmitTree(cx, bce, pn3))
                    return JS_FALSE;
                bce->flags |= oldflags & TCF_IN_FOR_INIT;
            } else if (letNotes) {
                /* JSOP_ENTERLETx expects at least 1 slot to have been pushed. */
                if (Emit1(cx, bce, JSOP_UNDEFINED) < 0)
                    return JS_FALSE;
            }
        }

        /* If we are not initializing, nothing to pop. */
        if (emitOption != InitializeVars) {
            if (next)
                continue;
            break;
        }

        JS_ASSERT_IF(pn2->isDefn(), pn3 == pn2->pn_expr);
        if (first && NewSrcNote2(cx, bce, SRC_DECL,
                                 (pn->isOp(JSOP_DEFCONST))
                                 ? SRC_DECL_CONST
                                 : (pn->isOp(JSOP_DEFVAR))
                                 ? SRC_DECL_VAR
                                 : SRC_DECL_LET) < 0) {
            return JS_FALSE;
        }
        if (op == JSOP_ARGUMENTS) {
            if (!EmitArguments(cx, bce))
                return JS_FALSE;
        } else if (!pn2->pn_cookie.isFree()) {
            EMIT_UINT16_IMM_OP(op, atomIndex);
        } else {
            if (!EmitIndexOp(cx, op, atomIndex, bce))
                return false;
        }

#if JS_HAS_DESTRUCTURING
    emit_note_pop:
#endif
        ptrdiff_t tmp = bce->offset();
        if (noteIndex >= 0) {
            if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 0, tmp-off))
                return JS_FALSE;
        }
        if (!next)
            break;
        off = tmp;
        noteIndex = NewSrcNote2(cx, bce, SRC_PCDELTA, 0);
        if (noteIndex < 0 || Emit1(cx, bce, JSOP_POP) < 0)
            return JS_FALSE;
    }

    if (pn->pn_xflags & PNX_POPVAR) {
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return JS_FALSE;
    }

    return JS_TRUE;
}

static bool
EmitAssignment(JSContext *cx, BytecodeEmitter *bce, ParseNode *lhs, JSOp op, ParseNode *rhs)
{
    ptrdiff_t top = bce->offset();

    /*
     * Check left operand type and generate specialized code for it.
     * Specialize to avoid ECMA "reference type" values on the operand
     * stack, which impose pervasive runtime "GetValue" costs.
     */
    jsatomid atomIndex = (jsatomid) -1;              /* quell GCC overwarning */
    jsbytecode offset = 1;

    switch (lhs->getKind()) {
      case PNK_NAME:
        if (!BindNameToSlot(cx, bce, lhs))
            return false;
        if (!lhs->pn_cookie.isFree()) {
            atomIndex = lhs->pn_cookie.asInteger();
        } else {
            if (!bce->makeAtomIndex(lhs->pn_atom, &atomIndex))
                return false;
            if (!lhs->isConst()) {
                JSOp op = lhs->isOp(JSOP_SETGNAME) ? JSOP_BINDGNAME : JSOP_BINDNAME;
                if (!EmitIndex32(cx, op, atomIndex, bce))
                    return false;
                offset++;
            }
        }
        break;
      case PNK_DOT:
        if (!EmitTree(cx, bce, lhs->expr()))
            return false;
        offset++;
        if (!bce->makeAtomIndex(lhs->pn_atom, &atomIndex))
            return false;
        break;
      case PNK_LB:
        JS_ASSERT(lhs->isArity(PN_BINARY));
        if (!EmitTree(cx, bce, lhs->pn_left))
            return false;
        if (!EmitTree(cx, bce, lhs->pn_right))
            return false;
        offset += 2;
        break;
#if JS_HAS_DESTRUCTURING
      case PNK_RB:
      case PNK_RC:
        break;
#endif
      case PNK_LP:
        if (!EmitTree(cx, bce, lhs))
            return false;
        offset++;
        break;
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(!bce->inStrictMode());
        JS_ASSERT(lhs->isOp(JSOP_SETXMLNAME));
        if (!EmitTree(cx, bce, lhs->pn_kid))
            return false;
        if (Emit1(cx, bce, JSOP_BINDXMLNAME) < 0)
            return false;
        offset++;
        break;
#endif
      default:
        JS_ASSERT(0);
    }

    if (op != JSOP_NOP) {
        JS_ASSERT(rhs);
        switch (lhs->getKind()) {
          case PNK_NAME:
            if (lhs->isConst()) {
                if (lhs->isOp(JSOP_CALLEE)) {
                    if (Emit1(cx, bce, JSOP_CALLEE) < 0)
                        return false;
                } else if (lhs->isOp(JSOP_NAME) || lhs->isOp(JSOP_GETGNAME)) {
                    if (!EmitIndex32(cx, lhs->getOp(), atomIndex, bce))
                        return false;
                } else {
                    JS_ASSERT(JOF_OPTYPE(lhs->getOp()) != JOF_ATOM);
                    EMIT_UINT16_IMM_OP(lhs->getOp(), atomIndex);
                }
            } else if (lhs->isOp(JSOP_SETNAME)) {
                if (Emit1(cx, bce, JSOP_DUP) < 0)
                    return false;
                if (!EmitIndex32(cx, JSOP_GETXPROP, atomIndex, bce))
                    return false;
            } else if (lhs->isOp(JSOP_SETGNAME)) {
                if (!BindGlobal(cx, bce, lhs, lhs->pn_atom))
                    return false;
                if (!EmitAtomOp(cx, lhs, JSOP_GETGNAME, bce))
                    return false;
            } else {
                EMIT_UINT16_IMM_OP(lhs->isOp(JSOP_SETARG) ? JSOP_GETARG : JSOP_GETLOCAL, atomIndex);
            }
            break;
          case PNK_DOT:
            if (Emit1(cx, bce, JSOP_DUP) < 0)
                return false;
            if (lhs->pn_atom == cx->runtime->atomState.protoAtom) {
                if (!EmitIndex32(cx, JSOP_QNAMEPART, atomIndex, bce))
                    return false;
                if (!EmitElemOpBase(cx, bce, JSOP_GETELEM))
                    return false;
            } else {
                bool isLength = (lhs->pn_atom == cx->runtime->atomState.lengthAtom);
                if (!EmitIndex32(cx, isLength ? JSOP_LENGTH : JSOP_GETPROP, atomIndex, bce))
                    return false;
            }
            break;
          case PNK_LB:
          case PNK_LP:
#if JS_HAS_XML_SUPPORT
          case PNK_XMLUNARY:
#endif
            if (Emit1(cx, bce, JSOP_DUP2) < 0)
                return false;
            if (!EmitElemOpBase(cx, bce, JSOP_GETELEM))
                return false;
            break;
          default:;
        }
    }

    /* Now emit the right operand (it may affect the namespace). */
    if (rhs) {
        if (!EmitTree(cx, bce, rhs))
            return false;
    } else {
        /* The value to assign is the next enumeration value in a for-in loop. */
        if (Emit2(cx, bce, JSOP_ITERNEXT, offset) < 0)
            return false;
    }

    /* If += etc., emit the binary operator with a decompiler note. */
    if (op != JSOP_NOP) {
        /*
         * Take care to avoid SRC_ASSIGNOP if the left-hand side is a const
         * declared in the current compilation unit, as in this case (just
         * a bit further below) we will avoid emitting the assignment op.
         */
        if (!lhs->isKind(PNK_NAME) || !lhs->isConst()) {
            if (NewSrcNote(cx, bce, SRC_ASSIGNOP) < 0)
                return false;
        }
        if (Emit1(cx, bce, op) < 0)
            return false;
    }

    /* Left parts such as a.b.c and a[b].c need a decompiler note. */
    if (!lhs->isKind(PNK_NAME) &&
#if JS_HAS_DESTRUCTURING
        !lhs->isKind(PNK_RB) &&
        !lhs->isKind(PNK_RC) &&
#endif
        NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - top) < 0)
    {
        return false;
    }

    /* Finally, emit the specialized assignment bytecode. */
    switch (lhs->getKind()) {
      case PNK_NAME:
        if (lhs->isConst()) {
            if (!rhs) {
                ReportCompileErrorNumber(cx, bce->tokenStream(), lhs, JSREPORT_ERROR,
                                         JSMSG_BAD_FOR_LEFTSIDE);
                return false;
            }
            break;
        }
        if (lhs->isOp(JSOP_SETARG) || lhs->isOp(JSOP_SETLOCAL)) {
            JS_ASSERT(atomIndex < UINT16_MAX);
            EMIT_UINT16_IMM_OP(lhs->getOp(), atomIndex);
        } else {
            if (!EmitIndexOp(cx, lhs->getOp(), atomIndex, bce))
                return false;
        }
        break;
      case PNK_DOT:
        if (!EmitIndexOp(cx, lhs->getOp(), atomIndex, bce))
            return false;
        break;
      case PNK_LB:
      case PNK_LP:
        if (Emit1(cx, bce, JSOP_SETELEM) < 0)
            return false;
        break;
#if JS_HAS_DESTRUCTURING
      case PNK_RB:
      case PNK_RC:
        if (!EmitDestructuringOps(cx, bce, SRC_DECL_NONE, lhs))
            return false;
        break;
#endif
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(!bce->inStrictMode());
        if (Emit1(cx, bce, JSOP_SETXMLNAME) < 0)
            return false;
        break;
#endif
      default:
        JS_ASSERT(0);
    }
    return true;
}

#ifdef DEBUG
static JSBool
GettableNoteForNextOp(BytecodeEmitter *bce)
{
    ptrdiff_t offset, target;
    jssrcnote *sn, *end;

    offset = 0;
    target = bce->offset();
    for (sn = bce->notes(), end = sn + bce->noteCount(); sn < end;
         sn = SN_NEXT(sn)) {
        if (offset == target && SN_IS_GETTABLE(sn))
            return JS_TRUE;
        offset += SN_DELTA(sn);
    }
    return JS_FALSE;
}
#endif

/* Top-level named functions need a nop for decompilation. */
static JSBool
EmitFunctionDefNop(JSContext *cx, BytecodeEmitter *bce, unsigned index)
{
    return NewSrcNote2(cx, bce, SRC_FUNCDEF, (ptrdiff_t)index) >= 0 &&
           Emit1(cx, bce, JSOP_NOP) >= 0;
}

static bool
EmitNewInit(JSContext *cx, BytecodeEmitter *bce, JSProtoKey key, ParseNode *pn)
{
    const size_t len = 1 + UINT32_INDEX_LEN;
    ptrdiff_t offset = EmitCheck(cx, bce, len);
    if (offset < 0)
        return false;

    jsbytecode *next = bce->next();
    next[0] = JSOP_NEWINIT;
    next[1] = jsbytecode(key);
    next[2] = 0;
    next[3] = 0;
    next[4] = 0;
    bce->current->next = next + len;
    UpdateDepth(cx, bce, offset);
    CheckTypeSet(cx, bce, JSOP_NEWINIT);
    return true;
}

bool
ParseNode::getConstantValue(JSContext *cx, bool strictChecks, Value *vp)
{
    switch (getKind()) {
      case PNK_NUMBER:
        vp->setNumber(pn_dval);
        return true;
      case PNK_STRING:
        vp->setString(pn_atom);
        return true;
      case PNK_TRUE:
        vp->setBoolean(true);
        return true;
      case PNK_FALSE:
        vp->setBoolean(false);
        return true;
      case PNK_NULL:
        vp->setNull();
        return true;
      case PNK_RB: {
        JS_ASSERT(isOp(JSOP_NEWINIT) && !(pn_xflags & PNX_NONCONST));

        JSObject *obj = NewDenseAllocatedArray(cx, pn_count);
        if (!obj)
            return false;

        unsigned idx = 0;
        for (ParseNode *pn = pn_head; pn; idx++, pn = pn->pn_next) {
            Value value;
            if (!pn->getConstantValue(cx, strictChecks, &value))
                return false;
            if (!obj->defineGeneric(cx, INT_TO_JSID(idx), value, NULL, NULL, JSPROP_ENUMERATE))
                return false;
        }
        JS_ASSERT(idx == pn_count);

        types::FixArrayType(cx, obj);
        vp->setObject(*obj);
        return true;
      }
      case PNK_RC: {
        JS_ASSERT(isOp(JSOP_NEWINIT) && !(pn_xflags & PNX_NONCONST));

        gc::AllocKind kind = GuessObjectGCKind(pn_count);
        JSObject *obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);
        if (!obj)
            return false;

        for (ParseNode *pn = pn_head; pn; pn = pn->pn_next) {
            Value value;
            if (!pn->pn_right->getConstantValue(cx, strictChecks, &value))
                return false;

            ParseNode *pnid = pn->pn_left;
            if (pnid->isKind(PNK_NUMBER)) {
                Value idvalue = NumberValue(pnid->pn_dval);
                jsid id;
                if (idvalue.isInt32() && INT_FITS_IN_JSID(idvalue.toInt32()))
                    id = INT_TO_JSID(idvalue.toInt32());
                else if (!js_InternNonIntElementId(cx, obj, idvalue, &id))
                    return false;
                if (!obj->defineGeneric(cx, id, value, NULL, NULL, JSPROP_ENUMERATE))
                    return false;
            } else {
                JS_ASSERT(pnid->isKind(PNK_NAME) || pnid->isKind(PNK_STRING));
                JS_ASSERT(pnid->pn_atom != cx->runtime->atomState.protoAtom);
                jsid id = ATOM_TO_JSID(pnid->pn_atom);
                if (!DefineNativeProperty(cx, obj, id, value, NULL, NULL,
                                          JSPROP_ENUMERATE, 0, 0)) {
                    return false;
                }
            }
        }

        types::FixObjectType(cx, obj);
        vp->setObject(*obj);
        return true;
      }
      default:
        JS_NOT_REACHED("Unexpected node");
    }
    return false;
}

static bool
EmitSingletonInitialiser(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    Value value;
    if (!pn->getConstantValue(cx, bce->needStrictChecks(), &value))
        return false;

    JS_ASSERT(value.isObject());
    ObjectBox *objbox = bce->parser->newObjectBox(&value.toObject());
    if (!objbox)
        return false;

    return EmitObjectOp(cx, objbox, JSOP_OBJECT, bce);
}

/* See the SRC_FOR source note offsetBias comments later in this file. */
JS_STATIC_ASSERT(JSOP_NOP_LENGTH == 1);
JS_STATIC_ASSERT(JSOP_POP_LENGTH == 1);

class EmitLevelManager
{
    BytecodeEmitter *bce;
  public:
    EmitLevelManager(BytecodeEmitter *bce) : bce(bce) { bce->emitLevel++; }
    ~EmitLevelManager() { bce->emitLevel--; }
};

static bool
EmitCatch(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    ptrdiff_t catchStart, guardJump;

    /*
     * Morph STMT_BLOCK to STMT_CATCH, note the block entry code offset,
     * and save the block object atom.
     */
    StmtInfo *stmt = bce->topStmt;
    JS_ASSERT(stmt->type == STMT_BLOCK && (stmt->flags & SIF_SCOPE));
    stmt->type = STMT_CATCH;
    catchStart = stmt->update;

    /* Go up one statement info record to the TRY or FINALLY record. */
    stmt = stmt->down;
    JS_ASSERT(stmt->type == STMT_TRY || stmt->type == STMT_FINALLY);

    /* Pick up the pending exception and bind it to the catch variable. */
    if (Emit1(cx, bce, JSOP_EXCEPTION) < 0)
        return false;

    /*
     * Dup the exception object if there is a guard for rethrowing to use
     * it later when rethrowing or in other catches.
     */
    if (pn->pn_kid2 && Emit1(cx, bce, JSOP_DUP) < 0)
        return false;

    ParseNode *pn2 = pn->pn_kid1;
    switch (pn2->getKind()) {
#if JS_HAS_DESTRUCTURING
      case PNK_RB:
      case PNK_RC:
        if (!EmitDestructuringOps(cx, bce, SRC_DECL_NONE, pn2))
            return false;
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return false;
        break;
#endif

      case PNK_NAME:
        /* Inline and specialize BindNameToSlot for pn2. */
        JS_ASSERT(!pn2->pn_cookie.isFree());
        EMIT_UINT16_IMM_OP(JSOP_SETLOCALPOP, pn2->pn_cookie.asInteger());
        break;

      default:
        JS_ASSERT(0);
    }

    /* Emit the guard expression, if there is one. */
    if (pn->pn_kid2) {
        if (!EmitTree(cx, bce, pn->pn_kid2))
            return false;
        if (!SetSrcNoteOffset(cx, bce, CATCHNOTE(*stmt), 0, bce->offset() - catchStart))
            return false;
        /* ifeq <next block> */
        guardJump = EmitJump(cx, bce, JSOP_IFEQ, 0);
        if (guardJump < 0)
            return false;
        GUARDJUMP(*stmt) = guardJump;

        /* Pop duplicated exception object as we no longer need it. */
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return false;
    }

    /* Emit the catch body. */
    if (!EmitTree(cx, bce, pn->pn_kid3))
        return false;

    /*
     * Annotate the JSOP_LEAVEBLOCK that will be emitted as we unwind via
     * our PNK_LEXICALSCOPE parent, so the decompiler knows to pop.
     */
    ptrdiff_t off = bce->stackDepth;
    if (NewSrcNote2(cx, bce, SRC_CATCH, off) < 0)
        return false;
    return true;
}

static bool
EmitTry(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    StmtInfo stmtInfo;
    ptrdiff_t catchJump = -1;

    /*
     * Push stmtInfo to track jumps-over-catches and gosubs-to-finally
     * for later fixup.
     *
     * When a finally block is active (STMT_FINALLY in our tree context),
     * non-local jumps (including jumps-over-catches) result in a GOSUB
     * being written into the bytecode stream and fixed-up later (c.f.
     * EmitBackPatchOp and BackPatch).
     */
    PushStatement(bce, &stmtInfo, pn->pn_kid3 ? STMT_FINALLY : STMT_TRY, bce->offset());

    /*
     * Since an exception can be thrown at any place inside the try block,
     * we need to restore the stack and the scope chain before we transfer
     * the control to the exception handler.
     *
     * For that we store in a try note associated with the catch or
     * finally block the stack depth upon the try entry. The interpreter
     * uses this depth to properly unwind the stack and the scope chain.
     */
    int depth = bce->stackDepth;

    /* Mark try location for decompilation, then emit try block. */
    if (Emit1(cx, bce, JSOP_TRY) < 0)
        return false;
    ptrdiff_t tryStart = bce->offset();
    if (!EmitTree(cx, bce, pn->pn_kid1))
        return false;
    JS_ASSERT(depth == bce->stackDepth);

    /* GOSUB to finally, if present. */
    if (pn->pn_kid3) {
        if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
            return false;
        if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &GOSUBS(stmtInfo)) < 0)
            return false;
    }

    /* Emit (hidden) jump over catch and/or finally. */
    if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
        return false;
    if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &catchJump) < 0)
        return false;

    ptrdiff_t tryEnd = bce->offset();

    /* If this try has a catch block, emit it. */
    ParseNode *lastCatch = NULL;
    if (ParseNode *pn2 = pn->pn_kid2) {
        unsigned count = 0;    /* previous catch block's population */

        /*
         * The emitted code for a catch block looks like:
         *
         * [throwing]                          only if 2nd+ catch block
         * [leaveblock]                        only if 2nd+ catch block
         * enterblock                          with SRC_CATCH
         * exception
         * [dup]                               only if catchguard
         * setlocalpop <slot>                  or destructuring code
         * [< catchguard code >]               if there's a catchguard
         * [ifeq <offset to next catch block>]         " "
         * [pop]                               only if catchguard
         * < catch block contents >
         * leaveblock
         * goto <end of catch blocks>          non-local; finally applies
         *
         * If there's no catch block without a catchguard, the last
         * <offset to next catch block> points to rethrow code.  This
         * code will [gosub] to the finally code if appropriate, and is
         * also used for the catch-all trynote for capturing exceptions
         * thrown from catch{} blocks.
         */
        for (ParseNode *pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            ptrdiff_t guardJump, catchNote;

            JS_ASSERT(bce->stackDepth == depth);
            guardJump = GUARDJUMP(stmtInfo);
            if (guardJump != -1) {
                /* Fix up and clean up previous catch block. */
                SetJumpOffsetAt(bce, guardJump);

                /*
                 * Account for JSOP_ENTERBLOCK (whose block object count
                 * is saved below) and pushed exception object that we
                 * still have after the jumping from the previous guard.
                 */
                bce->stackDepth = depth + count + 1;

                /*
                 * Move exception back to cx->exception to prepare for
                 * the next catch. We hide [throwing] from the decompiler
                 * since it compensates for the hidden JSOP_DUP at the
                 * start of the previous guarded catch.
                 */
                if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0 ||
                    Emit1(cx, bce, JSOP_THROWING) < 0) {
                    return false;
                }
                if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                    return false;
                EMIT_UINT16_IMM_OP(JSOP_LEAVEBLOCK, count);
                JS_ASSERT(bce->stackDepth == depth);
            }

            /*
             * Annotate the JSOP_ENTERBLOCK that's about to be generated
             * by the call to EmitTree immediately below.  Save this
             * source note's index in stmtInfo for use by the PNK_CATCH:
             * case, where the length of the catch guard is set as the
             * note's offset.
             */
            catchNote = NewSrcNote2(cx, bce, SRC_CATCH, 0);
            if (catchNote < 0)
                return false;
            CATCHNOTE(stmtInfo) = catchNote;

            /*
             * Emit the lexical scope and catch body.  Save the catch's
             * block object population via count, for use when targeting
             * guardJump at the next catch (the guard mismatch case).
             */
            JS_ASSERT(pn3->isKind(PNK_LEXICALSCOPE));
            count = pn3->pn_objbox->object->asStaticBlock().slotCount();
            if (!EmitTree(cx, bce, pn3))
                return false;

            /* gosub <finally>, if required */
            if (pn->pn_kid3) {
                if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &GOSUBS(stmtInfo)) < 0)
                    return false;
                JS_ASSERT(bce->stackDepth == depth);
            }

            /*
             * Jump over the remaining catch blocks.  This will get fixed
             * up to jump to after catch/finally.
             */
            if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                return false;
            if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &catchJump) < 0)
                return false;

            /*
             * Save a pointer to the last catch node to handle try-finally
             * and try-catch(guard)-finally special cases.
             */
            lastCatch = pn3->expr();
        }
    }

    /*
     * Last catch guard jumps to the rethrow code sequence if none of the
     * guards match. Target guardJump at the beginning of the rethrow
     * sequence, just in case a guard expression throws and leaves the
     * stack unbalanced.
     */
    if (lastCatch && lastCatch->pn_kid2) {
        SetJumpOffsetAt(bce, GUARDJUMP(stmtInfo));

        /* Sync the stack to take into account pushed exception. */
        JS_ASSERT(bce->stackDepth == depth);
        bce->stackDepth = depth + 1;

        /*
         * Rethrow the exception, delegating executing of finally if any
         * to the exception handler.
         */
        if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0 || Emit1(cx, bce, JSOP_THROW) < 0)
            return false;
    }

    JS_ASSERT(bce->stackDepth == depth);

    /* Emit finally handler if any. */
    ptrdiff_t finallyStart = 0;   /* to quell GCC uninitialized warnings */
    if (pn->pn_kid3) {
        /*
         * Fix up the gosubs that might have been emitted before non-local
         * jumps to the finally code.
         */
        if (!BackPatch(cx, bce, GOSUBS(stmtInfo), bce->next(), JSOP_GOSUB))
            return false;

        finallyStart = bce->offset();

        /* Indicate that we're emitting a subroutine body. */
        stmtInfo.type = STMT_SUBROUTINE;
        if (!UpdateLineNumberNotes(cx, bce, pn->pn_kid3->pn_pos.begin.lineno))
            return false;
        if (Emit1(cx, bce, JSOP_FINALLY) < 0 ||
            !EmitTree(cx, bce, pn->pn_kid3) ||
            Emit1(cx, bce, JSOP_RETSUB) < 0)
        {
            return false;
        }
        JS_ASSERT(bce->stackDepth == depth);
    }
    if (!PopStatementBCE(cx, bce))
        return false;

    if (NewSrcNote(cx, bce, SRC_ENDBRACE) < 0 || Emit1(cx, bce, JSOP_NOP) < 0)
        return false;

    /* Fix up the end-of-try/catch jumps to come here. */
    if (!BackPatch(cx, bce, catchJump, bce->next(), JSOP_GOTO))
        return false;

    /*
     * Add the try note last, to let post-order give us the right ordering
     * (first to last for a given nesting level, inner to outer by level).
     */
    if (pn->pn_kid2 && !NewTryNote(cx, bce, JSTRY_CATCH, depth, tryStart, tryEnd))
        return false;

    /*
     * If we've got a finally, mark try+catch region with additional
     * trynote to catch exceptions (re)thrown from a catch block or
     * for the try{}finally{} case.
     */
    if (pn->pn_kid3 && !NewTryNote(cx, bce, JSTRY_FINALLY, depth, tryStart, finallyStart))
        return false;

    return true;
}

static bool
EmitIf(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    StmtInfo stmtInfo;

    /* Initialize so we can detect else-if chains and avoid recursion. */
    stmtInfo.type = STMT_IF;
    ptrdiff_t beq = -1;
    ptrdiff_t jmp = -1;
    ptrdiff_t noteIndex = -1;

  if_again:
    /* Emit code for the condition before pushing stmtInfo. */
    if (!EmitTree(cx, bce, pn->pn_kid1))
        return JS_FALSE;
    ptrdiff_t top = bce->offset();
    if (stmtInfo.type == STMT_IF) {
        PushStatement(bce, &stmtInfo, STMT_IF, top);
    } else {
        /*
         * We came here from the goto further below that detects else-if
         * chains, so we must mutate stmtInfo back into a STMT_IF record.
         * Also (see below for why) we need a note offset for SRC_IF_ELSE
         * to help the decompiler.  Actually, we need two offsets, one for
         * decompiling any else clause and the second for decompiling an
         * else-if chain without bracing, overindenting, or incorrectly
         * scoping let declarations.
         */
        JS_ASSERT(stmtInfo.type == STMT_ELSE);
        stmtInfo.type = STMT_IF;
        stmtInfo.update = top;
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, jmp - beq))
            return JS_FALSE;
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 1, top - beq))
            return JS_FALSE;
    }

    /* Emit an annotated branch-if-false around the then part. */
    ParseNode *pn3 = pn->pn_kid3;
    noteIndex = NewSrcNote(cx, bce, pn3 ? SRC_IF_ELSE : SRC_IF);
    if (noteIndex < 0)
        return JS_FALSE;
    beq = EmitJump(cx, bce, JSOP_IFEQ, 0);
    if (beq < 0)
        return JS_FALSE;

    /* Emit code for the then and optional else parts. */
    if (!EmitTree(cx, bce, pn->pn_kid2))
        return JS_FALSE;
    if (pn3) {
        /* Modify stmtInfo so we know we're in the else part. */
        stmtInfo.type = STMT_ELSE;

        /*
         * Emit a JSOP_BACKPATCH op to jump from the end of our then part
         * around the else part.  The PopStatementBCE call at the bottom of
         * this function will fix up the backpatch chain linked from
         * stmtInfo.breaks.
         */
        jmp = EmitGoto(cx, bce, &stmtInfo, &stmtInfo.breaks);
        if (jmp < 0)
            return JS_FALSE;

        /* Ensure the branch-if-false comes here, then emit the else. */
        SetJumpOffsetAt(bce, beq);
        if (pn3->isKind(PNK_IF)) {
            pn = pn3;
            goto if_again;
        }

        if (!EmitTree(cx, bce, pn3))
            return JS_FALSE;

        /*
         * Annotate SRC_IF_ELSE with the offset from branch to jump, for
         * the decompiler's benefit.  We can't just "back up" from the pc
         * of the else clause, because we don't know whether an extended
         * jump was required to leap from the end of the then clause over
         * the else clause.
         */
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, jmp - beq))
            return JS_FALSE;
    } else {
        /* No else part, fixup the branch-if-false to come here. */
        SetJumpOffsetAt(bce, beq);
    }
    return PopStatementBCE(cx, bce);
}

#if JS_HAS_BLOCK_SCOPE
/*
 * pnLet represents one of:
 *
 *   let-expression:   (let (x = y) EXPR)
 *   let-statement:    let (x = y) { ... }
 *
 * For a let-expression 'let (x = a, [y,z] = b) e', EmitLet produces:
 *
 *  bytecode          stackDepth  srcnotes
 *  evaluate a        +1
 *  evaluate b        +1
 *  dup               +1          SRC_DESTRUCTLET + offset to enterlet0
 *  destructure y
 *  pick 1
 *  dup               +1          SRC_DESTRUCTLET + offset to enterlet0
 *  pick
 *  destructure z
 *  pick 1
 *  pop               -1
 *  enterlet0                     SRC_DECL + offset to leaveblockexpr
 *  evaluate e        +1
 *  leaveblockexpr    -3          SRC_PCBASE + offset to evaluate a
 *
 * Note that, since enterlet0 simply changes fp->blockChain and does not
 * otherwise touch the stack, evaluation of the let-var initializers must leave
 * the initial value in the let-var's future slot.
 *
 * The SRC_DESTRUCTLET distinguish JSOP_DUP as the beginning of a destructuring
 * let initialization and the offset allows the decompiler to find the block
 * object from which to find let var names. These forward offsets require
 * backpatching, which is handled by LetNotes.
 *
 * The SRC_DECL offset allows recursive decompilation of 'e'.
 *
 * The SRC_PCBASE allows js_DecompileValueGenerator to walk backwards from
 * JSOP_LEAVEBLOCKEXPR to the beginning of the let and is only needed for
 * let-expressions.
 */
static bool
EmitLet(JSContext *cx, BytecodeEmitter *bce, ParseNode *pnLet)
{
    JS_ASSERT(pnLet->isArity(PN_BINARY));
    ParseNode *varList = pnLet->pn_left;
    JS_ASSERT(varList->isArity(PN_LIST));
    ParseNode *letBody = pnLet->pn_right;
    JS_ASSERT(letBody->isLet() && letBody->isKind(PNK_LEXICALSCOPE));
    StaticBlockObject &blockObj = letBody->pn_objbox->object->asStaticBlock();

    ptrdiff_t letHeadOffset = bce->offset();
    int letHeadDepth = bce->stackDepth;

    LetNotes letNotes(cx);
    if (!EmitVariables(cx, bce, varList, PushInitialValues, &letNotes))
        return false;

    /* Push storage for hoisted let decls (e.g. 'let (x) { let y }'). */
    uint32_t alreadyPushed = unsigned(bce->stackDepth - letHeadDepth);
    uint32_t blockObjCount = blockObj.slotCount();
    for (uint32_t i = alreadyPushed; i < blockObjCount; ++i) {
        /* Tell the decompiler not to print the decl in the let head. */
        if (NewSrcNote(cx, bce, SRC_CONTINUE) < 0)
            return false;
        if (Emit1(cx, bce, JSOP_UNDEFINED) < 0)
            return false;
    }

    StmtInfo stmtInfo;
    PushBlockScope(bce, &stmtInfo, blockObj, bce->offset());

    if (!letNotes.update(cx, bce, bce->offset()))
        return false;

    ptrdiff_t declNote = NewSrcNote(cx, bce, SRC_DECL);
    if (declNote < 0)
        return false;

    ptrdiff_t bodyBegin = bce->offset();
    if (!EmitEnterBlock(cx, bce, letBody, JSOP_ENTERLET0))
        return false;

    if (!EmitTree(cx, bce, letBody->pn_expr))
        return false;

    JSOp leaveOp = letBody->getOp();
    if (leaveOp == JSOP_LEAVEBLOCKEXPR) {
        if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - letHeadOffset) < 0)
            return false;
    }

    JS_ASSERT(leaveOp == JSOP_LEAVEBLOCK || leaveOp == JSOP_LEAVEBLOCKEXPR);
    EMIT_UINT16_IMM_OP(leaveOp, blockObj.slotCount());

    ptrdiff_t bodyEnd = bce->offset();
    JS_ASSERT(bodyEnd > bodyBegin);

    if (!PopStatementBCE(cx, bce))
        return false;

    ptrdiff_t o = PackLetData((bodyEnd - bodyBegin) -
                              (JSOP_ENTERLET0_LENGTH + JSOP_LEAVEBLOCK_LENGTH),
                              letNotes.isGroupAssign());
    return SetSrcNoteOffset(cx, bce, declNote, 0, o);
}
#endif

#if JS_HAS_XML_SUPPORT
static bool
EmitXMLTag(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JS_ASSERT(!bce->inStrictMode());

    if (Emit1(cx, bce, JSOP_STARTXML) < 0)
        return false;

    {
        jsatomid index;
        JSAtom *tagAtom = (pn->isKind(PNK_XMLETAGO))
                          ? cx->runtime->atomState.etagoAtom
                          : cx->runtime->atomState.stagoAtom;
        if (!bce->makeAtomIndex(tagAtom, &index))
            return false;
        if (!EmitIndex32(cx, JSOP_STRING, index, bce))
            return false;
    }

    JS_ASSERT(pn->pn_count != 0);
    ParseNode *pn2 = pn->pn_head;
    if (pn2->isKind(PNK_XMLCURLYEXPR) && Emit1(cx, bce, JSOP_STARTXMLEXPR) < 0)
        return false;
    if (!EmitTree(cx, bce, pn2))
        return false;
    if (Emit1(cx, bce, JSOP_ADD) < 0)
        return false;

    uint32_t i;
    for (pn2 = pn2->pn_next, i = 0; pn2; pn2 = pn2->pn_next, i++) {
        if (pn2->isKind(PNK_XMLCURLYEXPR) && Emit1(cx, bce, JSOP_STARTXMLEXPR) < 0)
            return false;
        if (!EmitTree(cx, bce, pn2))
            return false;
        if ((i & 1) && pn2->isKind(PNK_XMLCURLYEXPR)) {
            if (Emit1(cx, bce, JSOP_TOATTRVAL) < 0)
                return false;
        }
        if (Emit1(cx, bce, (i & 1) ? JSOP_ADDATTRVAL : JSOP_ADDATTRNAME) < 0)
            return false;
    }

    {
        jsatomid index;
        JSAtom *tmp = (pn->isKind(PNK_XMLPTAGC)) ? cx->runtime->atomState.ptagcAtom
                                                 : cx->runtime->atomState.tagcAtom;
        if (!bce->makeAtomIndex(tmp, &index))
            return false;
        if (!EmitIndex32(cx, JSOP_STRING, index, bce))
            return false;
    }
    if (Emit1(cx, bce, JSOP_ADD) < 0)
        return false;

    if ((pn->pn_xflags & PNX_XMLROOT) && Emit1(cx, bce, pn->getOp()) < 0)
        return false;

    return true;
}

static bool
EmitXMLProcessingInstruction(JSContext *cx, BytecodeEmitter *bce, XMLProcessingInstruction &pi)
{
    JS_ASSERT(!bce->inStrictMode());

    jsatomid index;
    if (!bce->makeAtomIndex(pi.data(), &index))
        return false;
    if (!EmitIndex32(cx, JSOP_QNAMEPART, index, bce))
        return false;
    if (!EmitAtomOp(cx, pi.target(), JSOP_XMLPI, bce))
        return false;
    return true;
}
#endif

static bool
EmitLexicalScope(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_LEXICALSCOPE));
    JS_ASSERT(pn->getOp() == JSOP_LEAVEBLOCK);

    StmtInfo stmtInfo;
    ObjectBox *objbox = pn->pn_objbox;
    StaticBlockObject &blockObj = objbox->object->asStaticBlock();
    PushBlockScope(bce, &stmtInfo, blockObj, bce->offset());

    /*
     * For compound statements (i.e. { stmt-list }), the decompiler does not
     * emit curlies by default. However, if this stmt-list contains a let
     * declaration, this is semantically invalid so we need to add a srcnote to
     * enterblock to tell the decompiler to add curlies. This condition
     * shouldn't be so complicated; try to find a simpler condition.
     */
    ptrdiff_t noteIndex = -1;
    if (pn->expr()->getKind() != PNK_FOR &&
        pn->expr()->getKind() != PNK_CATCH &&
        (stmtInfo.down
         ? stmtInfo.down->type == STMT_BLOCK &&
           (!stmtInfo.down->down || stmtInfo.down->down->type != STMT_FOR_IN_LOOP)
         : !bce->inFunction()))
    {
        /* There must be no source note already output for the next op. */
        JS_ASSERT(bce->noteCount() == 0 ||
                  bce->lastNoteOffset() != bce->offset() ||
                  !GettableNoteForNextOp(bce));
        noteIndex = NewSrcNote2(cx, bce, SRC_BRACE, 0);
        if (noteIndex < 0)
            return false;
    }

    ptrdiff_t bodyBegin = bce->offset();
    if (!EmitEnterBlock(cx, bce, pn, JSOP_ENTERBLOCK))
        return false;

    if (!EmitTree(cx, bce, pn->pn_expr))
        return false;

    if (noteIndex >= 0) {
        if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 0, bce->offset() - bodyBegin))
            return false;
    }

    EMIT_UINT16_IMM_OP(JSOP_LEAVEBLOCK, blockObj.slotCount());

    return PopStatementBCE(cx, bce);
}

static bool
EmitWith(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    StmtInfo stmtInfo;
    if (!EmitTree(cx, bce, pn->pn_left))
        return false;
    PushStatement(bce, &stmtInfo, STMT_WITH, bce->offset());
    if (Emit1(cx, bce, JSOP_ENTERWITH) < 0)
        return false;

    if (!EmitTree(cx, bce, pn->pn_right))
        return false;
    if (Emit1(cx, bce, JSOP_LEAVEWITH) < 0)
        return false;
    return PopStatementBCE(cx, bce);
}

static bool
SetMethodFunction(JSContext *cx, FunctionBox *funbox, JSAtom *atom)
{
    RootedVarObject parent(cx);
    parent = funbox->function()->getParent();

    /*
     * Replace a boxed function with a new one with a method atom. Methods
     * require a function with the extended size finalize kind, which normal
     * functions don't have. We don't eagerly allocate functions with the
     * expanded size for boxed functions, as most functions are not methods.
     */
    JSFunction *fun = js_NewFunction(cx, NULL, NULL,
                                     funbox->function()->nargs,
                                     funbox->function()->flags,
                                     parent,
                                     funbox->function()->atom,
                                     JSFunction::ExtendedFinalizeKind);
    if (!fun)
        return false;

    JSScript *script = funbox->function()->script();
    if (script) {
        fun->setScript(script);
        if (!script->typeSetFunction(cx, fun))
            return false;
    }

    JS_ASSERT(funbox->function()->joinable());
    fun->setJoinable();

    fun->setMethodAtom(atom);

    funbox->object = fun;
    return true;
}

static bool
EmitForIn(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_FOR_IN_LOOP, top);

    ParseNode *forHead = pn->pn_left;
    ParseNode *forBody = pn->pn_right;

    ParseNode *pn1 = forHead->pn_kid1;
    bool letDecl = pn1 && pn1->isKind(PNK_LEXICALSCOPE);
    JS_ASSERT_IF(letDecl, pn1->isLet());

    StaticBlockObject *blockObj = letDecl ? &pn1->pn_objbox->object->asStaticBlock() : NULL;
    uint32_t blockObjCount = blockObj ? blockObj->slotCount() : 0;

    if (letDecl) {
        /*
         * The let's slot(s) will be under the iterator, but the block must not
         * be entered (i.e. fp->blockChain set) until after evaluating the rhs.
         * Thus, push to reserve space and enterblock after. The same argument
         * applies when leaving the loop. Thus, a for-let-in loop looks like:
         *
         *   push x N
         *   eval rhs
         *   iter
         *   enterlet1
         *   goto
         *     ... loop body
         *   ifne
         *   leaveforinlet
         *   enditer
         *   popn(N)
         */
        for (uint32_t i = 0; i < blockObjCount; ++i) {
            if (Emit1(cx, bce, JSOP_UNDEFINED) < 0)
                return false;
        }
    }

    /*
     * If the left part is 'var x', emit code to define x if necessary
     * using a prolog opcode, but do not emit a pop. If the left part was
     * originally 'var x = i', the parser will have rewritten it; see
     * Parser::forStatement. 'for (let x = i in o)' is mercifully banned.
     */
    if (pn1) {
        ParseNode *decl = letDecl ? pn1->pn_expr : pn1;
        JS_ASSERT(decl->isKind(PNK_VAR) || decl->isKind(PNK_LET));
        bce->flags |= TCF_IN_FOR_INIT;
        if (!EmitVariables(cx, bce, decl, DefineVars))
            return false;
        bce->flags &= ~TCF_IN_FOR_INIT;
    }

    /* Compile the object expression to the right of 'in'. */
    if (!EmitTree(cx, bce, forHead->pn_kid3))
        return JS_FALSE;

    /*
     * Emit a bytecode to convert top of stack value to the iterator
     * object depending on the loop variant (for-in, for-each-in, or
     * destructuring for-in).
     */
    JS_ASSERT(pn->isOp(JSOP_ITER));
    if (Emit2(cx, bce, JSOP_ITER, (uint8_t) pn->pn_iflags) < 0)
        return false;

    /* Enter the block before the loop body, after evaluating the obj. */
    StmtInfo letStmt;
    if (letDecl) {
        PushBlockScope(bce, &letStmt, *blockObj, bce->offset());
        letStmt.flags |= SIF_FOR_BLOCK;
        if (!EmitEnterBlock(cx, bce, pn1, JSOP_ENTERLET1))
            return false;
    }

    /* Annotate so the decompiler can find the loop-closing jump. */
    int noteIndex = NewSrcNote(cx, bce, SRC_FOR_IN);
    if (noteIndex < 0)
        return false;

    /*
     * Jump down to the loop condition to minimize overhead assuming at
     * least one iteration, as the other loop forms do.
     */
    ptrdiff_t jmp = EmitJump(cx, bce, JSOP_GOTO, 0);
    if (jmp < 0)
        return false;

    top = bce->offset();
    SET_STATEMENT_TOP(&stmtInfo, top);
    if (EmitLoopHead(cx, bce, NULL) < 0)
        return false;

#ifdef DEBUG
    int loopDepth = bce->stackDepth;
#endif

    /*
     * Emit code to get the next enumeration value and assign it to the
     * left hand side. The JSOP_POP after this assignment is annotated
     * so that the decompiler can distinguish 'for (x in y)' from
     * 'for (var x in y)'.
     */
    if (!EmitAssignment(cx, bce, forHead->pn_kid2, JSOP_NOP, NULL))
        return false;

    ptrdiff_t tmp2 = bce->offset();
    if (forHead->pn_kid1 && NewSrcNote2(cx, bce, SRC_DECL,
                                        (forHead->pn_kid1->isOp(JSOP_DEFVAR))
                                        ? SRC_DECL_VAR
                                        : SRC_DECL_LET) < 0) {
        return false;
    }
    if (Emit1(cx, bce, JSOP_POP) < 0)
        return false;

    /* The stack should be balanced around the assignment opcode sequence. */
    JS_ASSERT(bce->stackDepth == loopDepth);

    /* Emit code for the loop body. */
    if (!EmitTree(cx, bce, forBody))
        return false;

    /* Set loop and enclosing "update" offsets, for continue. */
    StmtInfo *stmt = &stmtInfo;
    do {
        stmt->update = bce->offset();
    } while ((stmt = stmt->down) != NULL && stmt->type == STMT_LABEL);

    /*
     * Fixup the goto that starts the loop to jump down to JSOP_MOREITER.
     */
    SetJumpOffsetAt(bce, jmp);
    if (!EmitLoopEntry(cx, bce, NULL))
        return false;
    if (Emit1(cx, bce, JSOP_MOREITER) < 0)
        return false;
    ptrdiff_t beq = EmitJump(cx, bce, JSOP_IFNE, top - bce->offset());
    if (beq < 0)
        return false;

    /* Set the first srcnote offset so we can find the start of the loop body. */
    if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 0, tmp2 - jmp))
        return false;
    /* Set the second srcnote offset so we can find the closing jump. */
    if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 1, beq - jmp))
        return false;

    /* Fixup breaks and continues before JSOP_ITER (and JSOP_LEAVEFORINLET). */
    if (!PopStatementBCE(cx, bce))
        return false;

    if (letDecl) {
        if (!PopStatementBCE(cx, bce))
            return false;
        if (Emit1(cx, bce, JSOP_LEAVEFORLETIN) < 0)
            return false;
    }

    if (!NewTryNote(cx, bce, JSTRY_ITER, bce->stackDepth, top, bce->offset()))
        return false;
    if (Emit1(cx, bce, JSOP_ENDITER) < 0)
        return false;

    if (letDecl) {
        /* Tell the decompiler to pop but not to print. */
        if (NewSrcNote(cx, bce, SRC_CONTINUE) < 0)
            return false;
        EMIT_UINT16_IMM_OP(JSOP_POPN, blockObjCount);
    }

    return true;
}

static bool
EmitNormalFor(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_FOR_LOOP, top);

    ParseNode *forHead = pn->pn_left;
    ParseNode *forBody = pn->pn_right;

    /* C-style for (init; cond; update) ... loop. */
    JSOp op = JSOP_POP;
    ParseNode *pn3 = forHead->pn_kid1;
    if (!pn3) {
        /* No initializer: emit an annotated nop for the decompiler. */
        op = JSOP_NOP;
    } else {
        bce->flags |= TCF_IN_FOR_INIT;
#if JS_HAS_DESTRUCTURING
        if (pn3->isKind(PNK_ASSIGN)) {
            JS_ASSERT(pn3->isOp(JSOP_NOP));
            if (!MaybeEmitGroupAssignment(cx, bce, op, pn3, &op))
                return false;
        }
#endif
        if (op == JSOP_POP) {
            if (!EmitTree(cx, bce, pn3))
                return false;
            if (pn3->isKind(PNK_VAR) || pn3->isKind(PNK_CONST) || pn3->isKind(PNK_LET)) {
                /*
                 * Check whether a destructuring-initialized var decl
                 * was optimized to a group assignment.  If so, we do
                 * not need to emit a pop below, so switch to a nop,
                 * just for the decompiler.
                 */
                JS_ASSERT(pn3->isArity(PN_LIST) || pn3->isArity(PN_BINARY));
                if (pn3->pn_xflags & PNX_GROUPINIT)
                    op = JSOP_NOP;
            }
        }
        bce->flags &= ~TCF_IN_FOR_INIT;
    }

    /*
     * NB: the SRC_FOR note has offsetBias 1 (JSOP_{NOP,POP}_LENGTH).
     * Use tmp to hold the biased srcnote "top" offset, which differs
     * from the top local variable by the length of the JSOP_GOTO
     * emitted in between tmp and top if this loop has a condition.
     */
    int noteIndex = NewSrcNote(cx, bce, SRC_FOR);
    if (noteIndex < 0 || Emit1(cx, bce, op) < 0)
        return false;
    ptrdiff_t tmp = bce->offset();

    ptrdiff_t jmp = -1;
    if (forHead->pn_kid2) {
        /* Goto the loop condition, which branches back to iterate. */
        jmp = EmitJump(cx, bce, JSOP_GOTO, 0);
        if (jmp < 0)
            return false;
    }

    top = bce->offset();
    SET_STATEMENT_TOP(&stmtInfo, top);

    /* Emit code for the loop body. */
    if (EmitLoopHead(cx, bce, forBody) < 0)
        return false;
    if (jmp == -1 && !EmitLoopEntry(cx, bce, forBody))
        return false;
    if (!EmitTree(cx, bce, forBody))
        return false;

    /* Set the second note offset so we can find the update part. */
    JS_ASSERT(noteIndex != -1);
    ptrdiff_t tmp2 = bce->offset();

    /* Set loop and enclosing "update" offsets, for continue. */
    StmtInfo *stmt = &stmtInfo;
    do {
        stmt->update = bce->offset();
    } while ((stmt = stmt->down) != NULL && stmt->type == STMT_LABEL);

    /* Check for update code to do before the condition (if any). */
    pn3 = forHead->pn_kid3;
    if (pn3) {
        op = JSOP_POP;
#if JS_HAS_DESTRUCTURING
        if (pn3->isKind(PNK_ASSIGN)) {
            JS_ASSERT(pn3->isOp(JSOP_NOP));
            if (!MaybeEmitGroupAssignment(cx, bce, op, pn3, &op))
                return false;
        }
#endif
        if (op == JSOP_POP && !EmitTree(cx, bce, pn3))
            return false;

        /* Always emit the POP or NOP, to help the decompiler. */
        if (Emit1(cx, bce, op) < 0)
            return false;

        /* Restore the absolute line number for source note readers. */
        ptrdiff_t lineno = pn->pn_pos.end.lineno;
        if (bce->currentLine() != (unsigned) lineno) {
            if (NewSrcNote2(cx, bce, SRC_SETLINE, lineno) < 0)
                return false;
            bce->current->currentLine = (unsigned) lineno;
        }
    }

    ptrdiff_t tmp3 = bce->offset();

    if (forHead->pn_kid2) {
        /* Fix up the goto from top to target the loop condition. */
        JS_ASSERT(jmp >= 0);
        SetJumpOffsetAt(bce, jmp);
        if (!EmitLoopEntry(cx, bce, forHead->pn_kid2))
            return false;

        if (!EmitTree(cx, bce, forHead->pn_kid2))
            return false;
    }

    /* Set the first note offset so we can find the loop condition. */
    if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 0, tmp3 - tmp))
        return false;
    if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 1, tmp2 - tmp))
        return false;
    /* The third note offset helps us find the loop-closing jump. */
    if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 2, bce->offset() - tmp))
        return false;

    /* If no loop condition, just emit a loop-closing jump. */
    op = forHead->pn_kid2 ? JSOP_IFNE : JSOP_GOTO;
    if (EmitJump(cx, bce, op, top - bce->offset()) < 0)
        return false;

    /* Now fixup all breaks and continues. */
    return PopStatementBCE(cx, bce);
}

static inline bool
EmitFor(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    JS_ASSERT(pn->pn_left->isKind(PNK_FORIN) || pn->pn_left->isKind(PNK_FORHEAD));
    return pn->pn_left->isKind(PNK_FORIN)
           ? EmitForIn(cx, bce, pn, top)
           : EmitNormalFor(cx, bce, pn, top);
}

static bool
EmitFunc(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
#if JS_HAS_XML_SUPPORT
    if (pn->isArity(PN_NULLARY))
        return Emit1(cx, bce, JSOP_GETFUNNS) >= 0;
#endif

    JSFunction *fun = pn->pn_funbox->function();
    JS_ASSERT(fun->isInterpreted());
    if (fun->script()) {
        /*
         * This second pass is needed to emit JSOP_NOP with a source note
         * for the already-emitted function definition prolog opcode. See
         * comments in the PNK_STATEMENTLIST case.
         */
        JS_ASSERT(pn->isOp(JSOP_NOP));
        JS_ASSERT(bce->inFunction());
        return EmitFunctionDefNop(cx, bce, pn->pn_index);
    }

    JS_ASSERT_IF(pn->pn_funbox->tcflags & TCF_FUN_HEAVYWEIGHT,
                 fun->kind() == JSFUN_INTERPRETED);

    {
        /*
         * Generate code for the function's body.  bce2 is not allocated on the
         * stack because doing so significantly reduces the maximum depth of
         * nested functions we can handle.  See bug 696284.
         */
        AutoPtr<BytecodeEmitter> bce2(cx);
        bce2 = cx->new_<BytecodeEmitter>(bce->parser, pn->pn_pos.begin.lineno);
        if (!bce2 || !bce2->init(cx))
            return false;

        bce2->flags = pn->pn_funbox->tcflags | TCF_COMPILING | TCF_IN_FUNCTION |
                     (bce->flags & TCF_FUN_MIGHT_ALIAS_LOCALS);
        bce2->bindings.transfer(cx, &pn->pn_funbox->bindings);
        bce2->setFunction(fun);
        bce2->funbox = pn->pn_funbox;
        bce2->parent = bce;
        bce2->globalScope = bce->globalScope;

        /*
         * js::frontend::SetStaticLevel limited static nesting depth to fit in
         * 16 bits and to reserve the all-ones value, thereby reserving the
         * magic FREE_UPVAR_COOKIE value. Note the bce2->staticLevel assignment
         * below.
         */
        JS_ASSERT(bce->staticLevel < JS_BITMASK(16) - 1);
        bce2->staticLevel = bce->staticLevel + 1;

        /* We measured the max scope depth when we parsed the function. */
        if (!EmitFunctionScript(cx, bce2.get(), pn->pn_body))
            return false;
    }

    /* Make the function object a literal in the outer script's pool. */
    unsigned index = bce->objectList.index(pn->pn_funbox);

    /* Emit a bytecode pointing to the closure object in its immediate. */
    if (pn->getOp() != JSOP_NOP) {
        if ((pn->pn_funbox->tcflags & TCF_GENEXP_LAMBDA) &&
            NewSrcNote(cx, bce, SRC_GENEXP) < 0)
        {
            return false;
        }

        return EmitFunctionOp(cx, pn->getOp(), index, bce);
    }

    /*
     * For a script we emit the code as we parse. Thus the bytecode for
     * top-level functions should go in the prolog to predefine their
     * names in the variable object before the already-generated main code
     * is executed. This extra work for top-level scripts is not necessary
     * when we emit the code for a function. It is fully parsed prior to
     * invocation of the emitter and calls to EmitTree for function
     * definitions can be scheduled before generating the rest of code.
     */
    if (!bce->inFunction()) {
        JS_ASSERT(!bce->topStmt);
        if (!BindGlobal(cx, bce, pn, fun->atom))
            return false;
        if (pn->pn_cookie.isFree()) {
            bce->switchToProlog();
            MOZ_ASSERT(!fun->isFlatClosure(),
                       "global functions can't have upvars, so they are never flat");
            if (!EmitFunctionOp(cx, JSOP_DEFFUN, index, bce))
                return false;
            if (!UpdateLineNumberNotes(cx, bce, pn->pn_pos.begin.lineno))
                return false;
            bce->switchToMain();
        }

        /* Emit NOP for the decompiler. */
        if (!EmitFunctionDefNop(cx, bce, index))
            return false;
    } else {
        unsigned slot;
        DebugOnly<BindingKind> kind = bce->bindings.lookup(cx, fun->atom, &slot);
        JS_ASSERT(kind == VARIABLE || kind == CONSTANT);
        JS_ASSERT(index < JS_BIT(20));
        pn->pn_index = index;
        JSOp op = fun->isFlatClosure() ? JSOP_DEFLOCALFUN_FC : JSOP_DEFLOCALFUN;
        if (pn->isClosed() &&
            !bce->callsEval() &&
            !bce->closedVars.append(pn->pn_cookie.slot()))
        {
            return false;
        }
        return EmitSlotObjectOp(cx, op, slot, index, bce);
    }

    return true;
}

static bool
EmitDo(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /* Emit an annotated nop so we know to decompile a 'do' keyword. */
    ptrdiff_t noteIndex = NewSrcNote(cx, bce, SRC_WHILE);
    if (noteIndex < 0 || Emit1(cx, bce, JSOP_NOP) < 0)
        return false;

    ptrdiff_t noteIndex2 = NewSrcNote(cx, bce, SRC_WHILE);
    if (noteIndex2 < 0)
        return false;

    /* Compile the loop body. */
    ptrdiff_t top = EmitLoopHead(cx, bce, pn->pn_left);
    if (top < 0)
        return false;
    if (!EmitLoopEntry(cx, bce, NULL))
        return false;

    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_DO_LOOP, top);
    if (!EmitTree(cx, bce, pn->pn_left))
        return false;

    /* Set loop and enclosing label update offsets, for continue. */
    ptrdiff_t off = bce->offset();
    StmtInfo *stmt = &stmtInfo;
    do {
        stmt->update = off;
    } while ((stmt = stmt->down) != NULL && stmt->type == STMT_LABEL);

    /* Compile the loop condition, now that continues know where to go. */
    if (!EmitTree(cx, bce, pn->pn_right))
        return false;

    /*
     * Since we use JSOP_IFNE for other purposes as well as for do-while
     * loops, we must store 1 + (beq - top) in the SRC_WHILE note offset,
     * and the decompiler must get that delta and decompile recursively.
     */
    ptrdiff_t beq = EmitJump(cx, bce, JSOP_IFNE, top - bce->offset());
    if (beq < 0)
        return false;

    /*
     * Be careful: We must set noteIndex2 before noteIndex in case the noteIndex
     * note gets bigger.
     */
    if (!SetSrcNoteOffset(cx, bce, noteIndex2, 0, beq - top))
        return false;
    if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, 1 + (off - top)))
        return false;

    return PopStatementBCE(cx, bce);
}

static bool
EmitWhile(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    /*
     * Minimize bytecodes issued for one or more iterations by jumping to
     * the condition below the body and closing the loop if the condition
     * is true with a backward branch. For iteration count i:
     *
     *  i    test at the top                 test at the bottom
     *  =    ===============                 ==================
     *  0    ifeq-pass                       goto; ifne-fail
     *  1    ifeq-fail; goto; ifne-pass      goto; ifne-pass; ifne-fail
     *  2    2*(ifeq-fail; goto); ifeq-pass  goto; 2*ifne-pass; ifne-fail
     *  . . .
     *  N    N*(ifeq-fail; goto); ifeq-pass  goto; N*ifne-pass; ifne-fail
     */
    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_WHILE_LOOP, top);

    ptrdiff_t noteIndex = NewSrcNote(cx, bce, SRC_WHILE);
    if (noteIndex < 0)
        return false;

    ptrdiff_t jmp = EmitJump(cx, bce, JSOP_GOTO, 0);
    if (jmp < 0)
        return false;

    top = EmitLoopHead(cx, bce, pn->pn_right);
    if (top < 0)
        return false;

    if (!EmitTree(cx, bce, pn->pn_right))
        return false;

    SetJumpOffsetAt(bce, jmp);
    if (!EmitLoopEntry(cx, bce, pn->pn_left))
        return false;
    if (!EmitTree(cx, bce, pn->pn_left))
        return false;

    ptrdiff_t beq = EmitJump(cx, bce, JSOP_IFNE, top - bce->offset());
    if (beq < 0)
        return false;

    if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, beq - jmp))
        return false;

    return PopStatementBCE(cx, bce);
}

static bool
EmitBreak(JSContext *cx, BytecodeEmitter *bce, PropertyName *label)
{
    StmtInfo *stmt = bce->topStmt;
    SrcNoteType noteType;
    jsatomid labelIndex;
    if (label) {
        if (!bce->makeAtomIndex(label, &labelIndex))
            return false;

        while (stmt->type != STMT_LABEL || stmt->label != label)
            stmt = stmt->down;
        noteType = SRC_BREAK2LABEL;
    } else {
        labelIndex = INVALID_ATOMID;
        while (!STMT_IS_LOOP(stmt) && stmt->type != STMT_SWITCH)
            stmt = stmt->down;
        noteType = (stmt->type == STMT_SWITCH) ? SRC_SWITCHBREAK : SRC_BREAK;
    }

    return EmitGoto(cx, bce, stmt, &stmt->breaks, labelIndex, noteType) >= 0;
}

static bool
EmitContinue(JSContext *cx, BytecodeEmitter *bce, PropertyName *label)
{
    StmtInfo *stmt = bce->topStmt;
    SrcNoteType noteType;
    jsatomid labelIndex;
    if (label) {
        if (!bce->makeAtomIndex(label, &labelIndex))
            return false;

        /* Find the loop statement enclosed by the matching label. */
        StmtInfo *loop = NULL;
        while (stmt->type != STMT_LABEL || stmt->label != label) {
            if (STMT_IS_LOOP(stmt))
                loop = stmt;
            stmt = stmt->down;
        }
        stmt = loop;
        noteType = SRC_CONT2LABEL;
    } else {
        labelIndex = INVALID_ATOMID;
        while (!STMT_IS_LOOP(stmt))
            stmt = stmt->down;
        noteType = SRC_CONTINUE;
    }

    return EmitGoto(cx, bce, stmt, &stmt->continues, labelIndex, noteType) >= 0;
}

static bool
EmitReturn(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /* Push a return value */
    if (ParseNode *pn2 = pn->pn_kid) {
        if (!EmitTree(cx, bce, pn2))
            return false;
    } else {
        /* No explicit return value provided */
        if (Emit1(cx, bce, JSOP_UNDEFINED) < 0)
            return false;
    }

    /*
     * EmitNonLocalJumpFixup may add fixup bytecode to close open try
     * blocks having finally clauses and to exit intermingled let blocks.
     * We can't simply transfer control flow to our caller in that case,
     * because we must gosub to those finally clauses from inner to outer,
     * with the correct stack pointer (i.e., after popping any with,
     * for/in, etc., slots nested inside the finally's try).
     *
     * In this case we mutate JSOP_RETURN into JSOP_SETRVAL and add an
     * extra JSOP_RETRVAL after the fixups.
     */
    ptrdiff_t top = bce->offset();

    if (Emit1(cx, bce, JSOP_RETURN) < 0)
        return false;
    if (!EmitNonLocalJumpFixup(cx, bce, NULL))
        return false;
    if (top + JSOP_RETURN_LENGTH != bce->offset()) {
        bce->base()[top] = JSOP_SETRVAL;
        if (Emit1(cx, bce, JSOP_RETRVAL) < 0)
            return false;
    }

    return true;
}

static bool
EmitStatementList(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    JS_ASSERT(pn->isArity(PN_LIST));

    ptrdiff_t noteIndex = -1;
    ptrdiff_t tmp = bce->offset();
    if (pn->pn_xflags & PNX_NEEDBRACES) {
        noteIndex = NewSrcNote2(cx, bce, SRC_BRACE, 0);
        if (noteIndex < 0 || Emit1(cx, bce, JSOP_NOP) < 0)
            return false;
    }

    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_BLOCK, top);

    ParseNode *pnchild = pn->pn_head;
    if (pn->pn_xflags & PNX_FUNCDEFS) {
        /*
         * This block contains top-level function definitions. To ensure
         * that we emit the bytecode defining them before the rest of code
         * in the block we use a separate pass over functions. During the
         * main pass later the emitter will add JSOP_NOP with source notes
         * for the function to preserve the original functions position
         * when decompiling.
         *
         * Currently this is used only for functions, as compile-as-we go
         * mode for scripts does not allow separate emitter passes.
         */
        JS_ASSERT(bce->inFunction());
        if (pn->pn_xflags & PNX_DESTRUCT) {
            /*
             * Assign the destructuring arguments before defining any
             * functions, see bug 419662.
             */
            JS_ASSERT(pnchild->isKind(PNK_SEMI));
            JS_ASSERT(pnchild->pn_kid->isKind(PNK_VAR) || pnchild->pn_kid->isKind(PNK_CONST));
            if (!EmitTree(cx, bce, pnchild))
                return false;
            pnchild = pnchild->pn_next;
        }

        for (ParseNode *pn2 = pnchild; pn2; pn2 = pn2->pn_next) {
            if (pn2->isKind(PNK_FUNCTION)) {
                if (pn2->isOp(JSOP_NOP)) {
                    if (!EmitTree(cx, bce, pn2))
                        return false;
                } else {
                    /*
                     * JSOP_DEFFUN in a top-level block with function
                     * definitions appears, for example, when "if (true)"
                     * is optimized away from "if (true) function x() {}".
                     * See bug 428424.
                     */
                    JS_ASSERT(pn2->isOp(JSOP_DEFFUN));
                }
            }
        }
    }

    for (ParseNode *pn2 = pnchild; pn2; pn2 = pn2->pn_next) {
        if (!EmitTree(cx, bce, pn2))
            return false;
    }

    if (noteIndex >= 0 && !SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 0, bce->offset() - tmp))
        return false;

    return PopStatementBCE(cx, bce);
}

static bool
EmitStatement(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_SEMI));

    ParseNode *pn2 = pn->pn_kid;
    if (!pn2)
        return true;

    /*
     * Top-level or called-from-a-native JS_Execute/EvaluateScript,
     * debugger, and eval frames may need the value of the ultimate
     * expression statement as the script's result, despite the fact
     * that it appears useless to the compiler.
     *
     * API users may also set the JSOPTION_NO_SCRIPT_RVAL option when
     * calling JS_Compile* to suppress JSOP_POPV.
     */
    bool wantval;
    JSBool useful = wantval = !(bce->flags & (TCF_IN_FUNCTION | TCF_NO_SCRIPT_RVAL));

    /* Don't eliminate expressions with side effects. */
    if (!useful) {
        if (!CheckSideEffects(cx, bce, pn2, &useful))
            return false;

        /*
         * Don't eliminate apparently useless expressions if they are
         * labeled expression statements.  The tc->topStmt->update test
         * catches the case where we are nesting in EmitTree for a labeled
         * compound statement.
         */
        if (bce->topStmt &&
            bce->topStmt->type == STMT_LABEL &&
            bce->topStmt->update >= bce->offset())
        {
            useful = true;
        }
    }

    if (useful) {
        JSOp op = wantval ? JSOP_POPV : JSOP_POP;
        JS_ASSERT_IF(pn2->isKind(PNK_ASSIGN), pn2->isOp(JSOP_NOP));
#if JS_HAS_DESTRUCTURING
        if (!wantval &&
            pn2->isKind(PNK_ASSIGN) &&
            !MaybeEmitGroupAssignment(cx, bce, op, pn2, &op))
        {
            return false;
        }
#endif
        if (op != JSOP_NOP) {
            /*
             * Specialize JSOP_SETPROP to JSOP_SETMETHOD to defer or
             * avoid null closure cloning. Do this only for assignment
             * statements that are not completion values wanted by a
             * script evaluator, to ensure that the joined function
             * can't escape directly.
             */
            if (!wantval &&
                pn2->isKind(PNK_ASSIGN) &&
                pn2->pn_left->isOp(JSOP_SETPROP) &&
                pn2->pn_right->isOp(JSOP_LAMBDA) &&
                pn2->pn_right->pn_funbox->joinable())
            {
                if (!SetMethodFunction(cx, pn2->pn_right->pn_funbox, pn2->pn_left->pn_atom))
                    return false;
                pn2->pn_left->setOp(JSOP_SETMETHOD);
            }
            if (!EmitTree(cx, bce, pn2))
                return false;
            if (Emit1(cx, bce, op) < 0)
                return false;
        }
    } else if (!pn->isDirectivePrologueMember()) {
        /* Don't complain about directive prologue members; just don't emit their code. */
        bce->current->currentLine = pn2->pn_pos.begin.lineno;
        if (!ReportCompileErrorNumber(cx, bce->tokenStream(), pn2,
                                      JSREPORT_WARNING | JSREPORT_STRICT, JSMSG_USELESS_EXPR))
        {
            return false;
        }
    }

    return true;
}

static bool
EmitDelete(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /*
     * Under ECMA 3, deleting a non-reference returns true -- but alas we
     * must evaluate the operand if it appears it might have side effects.
     */
    ParseNode *pn2 = pn->pn_kid;
    switch (pn2->getKind()) {
      case PNK_NAME:
      {
        if (!BindNameToSlot(cx, bce, pn2))
            return false;
        JSOp op = pn2->getOp();
        if (op == JSOP_FALSE) {
            if (Emit1(cx, bce, op) < 0)
                return false;
        } else {
            if (!EmitAtomOp(cx, pn2, op, bce))
                return false;
        }
        break;
      }
      case PNK_DOT:
        if (!EmitPropOp(cx, pn2, JSOP_DELPROP, bce, false))
            return false;
        break;
#if JS_HAS_XML_SUPPORT
      case PNK_DBLDOT:
        JS_ASSERT(!bce->inStrictMode());
        if (!EmitElemOp(cx, pn2, JSOP_DELDESC, bce))
            return false;
        break;
#endif
      case PNK_LB:
        if (!EmitElemOp(cx, pn2, JSOP_DELELEM, bce))
            return false;
        break;
      default:
      {
        /*
         * If useless, just emit JSOP_TRUE; otherwise convert delete foo()
         * to foo(), true (a comma expression, requiring SRC_PCDELTA).
         */
        JSBool useful = false;
        if (!CheckSideEffects(cx, bce, pn2, &useful))
            return false;

        ptrdiff_t off, noteIndex;
        if (useful) {
            JS_ASSERT_IF(pn2->isKind(PNK_LP), !(pn2->pn_xflags & PNX_SETCALL));
            if (!EmitTree(cx, bce, pn2))
                return false;
            off = bce->offset();
            noteIndex = NewSrcNote2(cx, bce, SRC_PCDELTA, 0);
            if (noteIndex < 0 || Emit1(cx, bce, JSOP_POP) < 0)
                return false;
        } else {
            off = noteIndex = -1;
        }

        if (Emit1(cx, bce, JSOP_TRUE) < 0)
            return false;
        if (noteIndex >= 0) {
            ptrdiff_t tmp = bce->offset();
            if (!SetSrcNoteOffset(cx, bce, unsigned(noteIndex), 0, tmp - off))
                return false;
        }
      }
    }

    return true;
}

static bool
EmitCallOrNew(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    bool callop = pn->isKind(PNK_LP);

    /*
     * Emit callable invocation or operator new (constructor call) code.
     * First, emit code for the left operand to evaluate the callable or
     * constructable object expression.
     *
     * For operator new applied to other expressions than E4X ones, we emit
     * JSOP_GETPROP instead of JSOP_CALLPROP, etc. This is necessary to
     * interpose the lambda-initialized method read barrier -- see the code
     * in jsinterp.cpp for JSOP_LAMBDA followed by JSOP_{SET,INIT}PROP.
     *
     * Then (or in a call case that has no explicit reference-base
     * object) we emit JSOP_UNDEFINED to produce the undefined |this| 
     * value required for calls (which non-strict mode functions 
     * will box into the global object).
     */
    ParseNode *pn2 = pn->pn_head;
    switch (pn2->getKind()) {
      case PNK_NAME:
        if (!EmitNameOp(cx, bce, pn2, callop))
            return false;
        break;
      case PNK_DOT:
        if (!EmitPropOp(cx, pn2, pn2->getOp(), bce, callop))
            return false;
        break;
      case PNK_LB:
        JS_ASSERT(pn2->isOp(JSOP_GETELEM));
        if (!EmitElemOp(cx, pn2, callop ? JSOP_CALLELEM : JSOP_GETELEM, bce))
            return false;
        break;
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(pn2->isOp(JSOP_XMLNAME));
        if (!EmitXMLName(cx, pn2, JSOP_CALLXMLNAME, bce))
            return false;
        callop = true;          /* suppress JSOP_UNDEFINED after */
        break;
#endif
      default:
        if (!EmitTree(cx, bce, pn2))
            return false;
        callop = false;             /* trigger JSOP_UNDEFINED after */
        break;
    }
    if (!callop && Emit1(cx, bce, JSOP_UNDEFINED) < 0)
        return false;

    /* Remember start of callable-object bytecode for decompilation hint. */
    ptrdiff_t off = top;

    /*
     * Emit code for each argument in order, then emit the JSOP_*CALL or
     * JSOP_NEW bytecode with a two-byte immediate telling how many args
     * were pushed on the operand stack.
     */
    unsigned oldflags = bce->flags;
    bce->flags &= ~TCF_IN_FOR_INIT;
    for (ParseNode *pn3 = pn2->pn_next; pn3; pn3 = pn3->pn_next) {
        if (!EmitTree(cx, bce, pn3))
            return false;
    }
    bce->flags |= oldflags & TCF_IN_FOR_INIT;
    if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - off) < 0)
        return false;

    uint32_t argc = pn->pn_count - 1;
    if (Emit3(cx, bce, pn->getOp(), ARGC_HI(argc), ARGC_LO(argc)) < 0)
        return false;
    CheckTypeSet(cx, bce, pn->getOp());
    if (pn->isOp(JSOP_EVAL))
        EMIT_UINT16_IMM_OP(JSOP_LINENO, pn->pn_pos.begin.lineno);
    if (pn->pn_xflags & PNX_SETCALL) {
        if (Emit1(cx, bce, JSOP_SETCALL) < 0)
            return false;
    }
    return true;
}

static bool
EmitLogical(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /*
     * JSOP_OR converts the operand on the stack to boolean, leaves the original
     * value on the stack and jumps if true; otherwise it falls into the next
     * bytecode, which pops the left operand and then evaluates the right operand.
     * The jump goes around the right operand evaluation.
     *
     * JSOP_AND converts the operand on the stack to boolean and jumps if false;
     * otherwise it falls into the right operand's bytecode.
     */

    if (pn->isArity(PN_BINARY)) {
        if (!EmitTree(cx, bce, pn->pn_left))
            return false;
        ptrdiff_t top = EmitJump(cx, bce, JSOP_BACKPATCH, 0);
        if (top < 0)
            return false;
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return false;
        if (!EmitTree(cx, bce, pn->pn_right))
            return false;
        ptrdiff_t off = bce->offset();
        jsbytecode *pc = bce->code(top);
        SET_JUMP_OFFSET(pc, off - top);
        *pc = pn->getOp();
        return true;
    }

    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(pn->pn_head->pn_next->pn_next);

    /* Left-associative operator chain: avoid too much recursion. */
    ParseNode *pn2 = pn->pn_head;
    if (!EmitTree(cx, bce, pn2))
        return false;
    ptrdiff_t top = EmitJump(cx, bce, JSOP_BACKPATCH, 0);
    if (top < 0)
        return false;
    if (Emit1(cx, bce, JSOP_POP) < 0)
        return false;

    /* Emit nodes between the head and the tail. */
    ptrdiff_t jmp = top;
    while ((pn2 = pn2->pn_next)->pn_next) {
        if (!EmitTree(cx, bce, pn2))
            return false;
        ptrdiff_t off = EmitJump(cx, bce, JSOP_BACKPATCH, 0);
        if (off < 0)
            return false;
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return false;
        SET_JUMP_OFFSET(bce->code(jmp), off - jmp);
        jmp = off;
    }
    if (!EmitTree(cx, bce, pn2))
        return false;

    pn2 = pn->pn_head;
    ptrdiff_t off = bce->offset();
    do {
        jsbytecode *pc = bce->code(top);
        ptrdiff_t tmp = GET_JUMP_OFFSET(pc);
        SET_JUMP_OFFSET(pc, off - top);
        *pc = pn->getOp();
        top += tmp;
    } while ((pn2 = pn2->pn_next)->pn_next);

    return true;
}

static bool
EmitIncOrDec(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /* Emit lvalue-specialized code for ++/-- operators. */
    ParseNode *pn2 = pn->pn_kid;
    JS_ASSERT(!pn2->isKind(PNK_RP));
    JSOp op = pn->getOp();
    switch (pn2->getKind()) {
      case PNK_DOT:
        if (!EmitPropIncDec(cx, pn2, op, bce))
            return false;
        break;
      case PNK_LB:
        if (!EmitElemIncDec(cx, pn2, op, bce))
            return false;
        break;
      case PNK_LP:
        if (!EmitTree(cx, bce, pn2))
            return false;
        if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - pn2->pn_offset) < 0)
            return false;
        if (Emit1(cx, bce, op) < 0)
            return false;
        /*
         * This is dead code for the decompiler, don't generate
         * a decomposed version of the opcode. We do need to balance
         * the stacks in the decomposed version.
         */
        JS_ASSERT(js_CodeSpec[op].format & JOF_DECOMPOSE);
        JS_ASSERT(js_CodeSpec[op].format & JOF_ELEM);
        if (Emit1(cx, bce, (JSOp)1) < 0)
            return false;
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return false;
        break;
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(!bce->inStrictMode());
        JS_ASSERT(pn2->isOp(JSOP_SETXMLNAME));
        if (!EmitTree(cx, bce, pn2->pn_kid))
            return false;
        if (Emit1(cx, bce, JSOP_BINDXMLNAME) < 0)
            return false;
        if (!EmitElemIncDec(cx, NULL, op, bce))
            return false;
        break;
#endif
      default:
        JS_ASSERT(pn2->isKind(PNK_NAME));
        pn2->setOp(op);
        if (!BindNameToSlot(cx, bce, pn2))
            return false;
        op = pn2->getOp();
        if (op == JSOP_CALLEE) {
            if (Emit1(cx, bce, op) < 0)
                return false;
        } else if (!pn2->pn_cookie.isFree()) {
            jsatomid atomIndex = pn2->pn_cookie.asInteger();
            EMIT_UINT16_IMM_OP(op, atomIndex);
        } else {
            JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);
            if (js_CodeSpec[op].format & (JOF_INC | JOF_DEC)) {
                if (!EmitNameIncDec(cx, pn2, op, bce))
                    return false;
            } else {
                if (!EmitAtomOp(cx, pn2, op, bce))
                    return false;
            }
            break;
        }
        if (pn2->isConst()) {
            if (Emit1(cx, bce, JSOP_POS) < 0)
                return false;
            op = pn->getOp();
            if (!(js_CodeSpec[op].format & JOF_POST)) {
                if (Emit1(cx, bce, JSOP_ONE) < 0)
                    return false;
                op = (js_CodeSpec[op].format & JOF_INC) ? JSOP_ADD : JSOP_SUB;
                if (Emit1(cx, bce, op) < 0)
                    return false;
            }
        }
    }
    return true;
}

static bool
EmitLabel(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /*
     * Emit a JSOP_LABEL instruction. The argument is the offset to the statement
     * following the labeled statement. This op has either a SRC_LABEL or
     * SRC_LABELBRACE source note for the decompiler.
     */
    JSAtom *atom = pn->pn_atom;

    jsatomid index;
    if (!bce->makeAtomIndex(atom, &index))
        return false;

    ParseNode *pn2 = pn->expr();
    SrcNoteType noteType = (pn2->isKind(PNK_STATEMENTLIST) ||
                            (pn2->isKind(PNK_LEXICALSCOPE) &&
                             pn2->expr()->isKind(PNK_STATEMENTLIST)))
                           ? SRC_LABELBRACE
                           : SRC_LABEL;
    ptrdiff_t noteIndex = NewSrcNote2(cx, bce, noteType, ptrdiff_t(index));
    if (noteIndex < 0)
        return false;

    ptrdiff_t top = EmitJump(cx, bce, JSOP_LABEL, 0);
    if (top < 0)
        return false;

    /* Emit code for the labeled statement. */
    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_LABEL, bce->offset());
    stmtInfo.label = atom;
    if (!EmitTree(cx, bce, pn2))
        return false;
    if (!PopStatementBCE(cx, bce))
        return false;

    /* Patch the JSOP_LABEL offset. */
    SetJumpOffsetAt(bce, top);

    /* If the statement was compound, emit a note for the end brace. */
    if (noteType == SRC_LABELBRACE) {
        if (NewSrcNote(cx, bce, SRC_ENDBRACE) < 0 || Emit1(cx, bce, JSOP_NOP) < 0)
            return false;
    }

    return true;
}

static bool
EmitSyntheticStatements(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_SEQ, top);
    for (ParseNode *pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
        if (!EmitTree(cx, bce, pn2))
            return false;
    }
    return PopStatementBCE(cx, bce);
}

static bool
EmitConditionalExpression(JSContext *cx, BytecodeEmitter *bce, ConditionalExpression &conditional)
{
    /* Emit the condition, then branch if false to the else part. */
    if (!EmitTree(cx, bce, &conditional.condition()))
        return false;
    ptrdiff_t noteIndex = NewSrcNote(cx, bce, SRC_COND);
    if (noteIndex < 0)
        return false;
    ptrdiff_t beq = EmitJump(cx, bce, JSOP_IFEQ, 0);
    if (beq < 0 || !EmitTree(cx, bce, &conditional.thenExpression()))
        return false;

    /* Jump around else, fixup the branch, emit else, fixup jump. */
    ptrdiff_t jmp = EmitJump(cx, bce, JSOP_GOTO, 0);
    if (jmp < 0)
        return false;
    SetJumpOffsetAt(bce, beq);

    /*
     * Because each branch pushes a single value, but our stack budgeting
     * analysis ignores branches, we now have to adjust bce->stackDepth to
     * ignore the value pushed by the first branch.  Execution will follow
     * only one path, so we must decrement bce->stackDepth.
     *
     * Failing to do this will foil code, such as let expression and block
     * code generation, which must use the stack depth to compute local
     * stack indexes correctly.
     */
    JS_ASSERT(bce->stackDepth > 0);
    bce->stackDepth--;
    if (!EmitTree(cx, bce, &conditional.elseExpression()))
        return false;
    SetJumpOffsetAt(bce, jmp);
    return SetSrcNoteOffset(cx, bce, noteIndex, 0, jmp - beq);
}

static bool
EmitObject(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
#if JS_HAS_DESTRUCTURING_SHORTHAND
    if (pn->pn_xflags & PNX_DESTRUCT) {
        ReportCompileErrorNumber(cx, bce->tokenStream(), pn, JSREPORT_ERROR,
                                 JSMSG_BAD_OBJECT_INIT);
        return false;
    }
#endif

    if (!(pn->pn_xflags & PNX_NONCONST) && pn->pn_head && bce->checkSingletonContext())
        return EmitSingletonInitialiser(cx, bce, pn);

    /*
     * Emit code for {p:a, '%q':b, 2:c} that is equivalent to constructing
     * a new object and in source order evaluating each property value and
     * adding the property to the object, without invoking latent setters.
     * We use the JSOP_NEWINIT and JSOP_INITELEM/JSOP_INITPROP bytecodes to
     * ignore setters and to avoid dup'ing and popping the object as each
     * property is added, as JSOP_SETELEM/JSOP_SETPROP would do.
     */
    ptrdiff_t offset = bce->next() - bce->base();
    if (!EmitNewInit(cx, bce, JSProto_Object, pn))
        return false;

    /*
     * Try to construct the shape of the object as we go, so we can emit a
     * JSOP_NEWOBJECT with the final shape instead.
     */
    JSObject *obj = NULL;
    if (bce->compileAndGo()) {
        gc::AllocKind kind = GuessObjectGCKind(pn->pn_count);
        obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);
        if (!obj)
            return false;
    }

    unsigned methodInits = 0, slowMethodInits = 0;
    for (ParseNode *pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
        /* Emit an index for t[2] for later consumption by JSOP_INITELEM. */
        ParseNode *pn3 = pn2->pn_left;
        if (pn3->isKind(PNK_NUMBER)) {
            if (!EmitNumberOp(cx, pn3->pn_dval, bce))
                return false;
        }

        /* Emit code for the property initializer. */
        if (!EmitTree(cx, bce, pn2->pn_right))
            return false;

        JSOp op = pn2->getOp();
        if (op == JSOP_GETTER || op == JSOP_SETTER) {
            obj = NULL;
            if (Emit1(cx, bce, op) < 0)
                return false;
        }

        /* Annotate JSOP_INITELEM so we decompile 2:c and not just c. */
        if (pn3->isKind(PNK_NUMBER)) {
            obj = NULL;
            if (NewSrcNote(cx, bce, SRC_INITPROP) < 0)
                return false;
            if (Emit1(cx, bce, JSOP_INITELEM) < 0)
                return false;
        } else {
            JS_ASSERT(pn3->isKind(PNK_NAME) || pn3->isKind(PNK_STRING));
            jsatomid index;
            if (!bce->makeAtomIndex(pn3->pn_atom, &index))
                return false;

            /* Check whether we can optimize to JSOP_INITMETHOD. */
            ParseNode *init = pn2->pn_right;
            bool lambda = init->isOp(JSOP_LAMBDA);
            if (lambda)
                ++methodInits;
            if (op == JSOP_INITPROP && lambda && init->pn_funbox->joinable()) {
                obj = NULL;
                op = JSOP_INITMETHOD;
                if (!SetMethodFunction(cx, init->pn_funbox, pn3->pn_atom))
                    return JS_FALSE;
                pn2->setOp(op);
            } else {
                /*
                 * Disable NEWOBJECT on initializers that set __proto__, which has
                 * a non-standard setter on objects.
                 */
                if (pn3->pn_atom == cx->runtime->atomState.protoAtom)
                    obj = NULL;
                op = JSOP_INITPROP;
                if (lambda)
                    ++slowMethodInits;
            }

            if (obj) {
                JS_ASSERT(!obj->inDictionaryMode());
                if (!DefineNativeProperty(cx, obj, ATOM_TO_JSID(pn3->pn_atom),
                                          UndefinedValue(), NULL, NULL,
                                          JSPROP_ENUMERATE, 0, 0))
                {
                    return false;
                }
                if (obj->inDictionaryMode())
                    obj = NULL;
            }

            if (!EmitIndex32(cx, op, index, bce))
                return false;
        }
    }

    if (Emit1(cx, bce, JSOP_ENDINIT) < 0)
        return false;

    if (obj) {
        /*
         * The object survived and has a predictable shape: update the original
         * bytecode.
         */
        ObjectBox *objbox = bce->parser->newObjectBox(obj);
        if (!objbox)
            return false;
        unsigned index = bce->objectList.index(objbox);
        MOZ_STATIC_ASSERT(JSOP_NEWINIT_LENGTH == JSOP_NEWOBJECT_LENGTH,
                          "newinit and newobject must have equal length to edit in-place");
        EMIT_UINT32_IN_PLACE(offset, JSOP_NEWOBJECT, uint32_t(index));
    }

    return true;
}

static bool
EmitArray(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /*
     * Emit code for [a, b, c] that is equivalent to constructing a new
     * array and in source order evaluating each element value and adding
     * it to the array, without invoking latent setters.  We use the
     * JSOP_NEWINIT and JSOP_INITELEM bytecodes to ignore setters and to
     * avoid dup'ing and popping the array as each element is added, as
     * JSOP_SETELEM/JSOP_SETPROP would do.
     */

#if JS_HAS_GENERATORS
    if (pn->isKind(PNK_ARRAYCOMP)) {
        if (!EmitNewInit(cx, bce, JSProto_Array, pn))
            return false;

        /*
         * Pass the new array's stack index to the PNK_ARRAYPUSH case via
         * bce->arrayCompDepth, then simply traverse the PNK_FOR node and
         * its kids under pn2 to generate this comprehension.
         */
        JS_ASSERT(bce->stackDepth > 0);
        unsigned saveDepth = bce->arrayCompDepth;
        bce->arrayCompDepth = (uint32_t) (bce->stackDepth - 1);
        if (!EmitTree(cx, bce, pn->pn_head))
            return false;
        bce->arrayCompDepth = saveDepth;

        /* Emit the usual op needed for decompilation. */
        return Emit1(cx, bce, JSOP_ENDINIT) >= 0;
    }
#endif /* JS_HAS_GENERATORS */

    if (!(pn->pn_xflags & PNX_NONCONST) && pn->pn_head && bce->checkSingletonContext())
        return EmitSingletonInitialiser(cx, bce, pn);

    ptrdiff_t off = EmitN(cx, bce, JSOP_NEWARRAY, 3);
    if (off < 0)
        return false;
    CheckTypeSet(cx, bce, JSOP_NEWARRAY);
    jsbytecode *pc = bce->code(off);
    SET_UINT24(pc, pn->pn_count);

    ParseNode *pn2 = pn->pn_head;
    jsatomid atomIndex;
    for (atomIndex = 0; pn2; atomIndex++, pn2 = pn2->pn_next) {
        if (!EmitNumberOp(cx, atomIndex, bce))
            return false;
        if (pn2->isKind(PNK_COMMA) && pn2->isArity(PN_NULLARY)) {
            if (Emit1(cx, bce, JSOP_HOLE) < 0)
                return false;
        } else {
            if (!EmitTree(cx, bce, pn2))
                return false;
        }
        if (Emit1(cx, bce, JSOP_INITELEM) < 0)
            return false;
    }
    JS_ASSERT(atomIndex == pn->pn_count);

    if (pn->pn_xflags & PNX_ENDCOMMA) {
        /* Emit a source note so we know to decompile an extra comma. */
        if (NewSrcNote(cx, bce, SRC_CONTINUE) < 0)
            return false;
    }

    /* Emit an op to finish the array and aid in decompilation. */
    return Emit1(cx, bce, JSOP_ENDINIT) >= 0;
}

static bool
EmitUnary(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /* Unary op, including unary +/-. */
    JSOp op = pn->getOp();
    ParseNode *pn2 = pn->pn_kid;

    JS_ASSERT(op != JSOP_XMLNAME);
    if (op == JSOP_TYPEOF && !pn2->isKind(PNK_NAME))
        op = JSOP_TYPEOFEXPR;

    unsigned oldflags = bce->flags;
    bce->flags &= ~TCF_IN_FOR_INIT;
    if (!EmitTree(cx, bce, pn2))
        return false;

    bce->flags |= oldflags & TCF_IN_FOR_INIT;
    return Emit1(cx, bce, op) >= 0;
}

JSBool
frontend::EmitTree(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    EmitLevelManager elm(bce);

    JSBool ok = true;
    ptrdiff_t top = bce->offset();
    pn->pn_offset = top;

    /* Emit notes to tell the current bytecode's source line number. */
    UPDATE_LINE_NUMBER_NOTES(cx, bce, pn->pn_pos.begin.lineno);

    switch (pn->getKind()) {
      case PNK_FUNCTION:
        ok = EmitFunc(cx, bce, pn);
        break;

      case PNK_ARGSBODY:
      {
        ParseNode *pnlast = pn->last();
        for (ParseNode *pn2 = pn->pn_head; pn2 != pnlast; pn2 = pn2->pn_next) {
            if (!pn2->isDefn())
                continue;
            if (!BindNameToSlot(cx, bce, pn2))
                return JS_FALSE;
            if (JOF_OPTYPE(pn2->getOp()) == JOF_QARG && bce->shouldNoteClosedName(pn2)) {
                if (!bce->closedArgs.append(pn2->pn_cookie.slot()))
                    return JS_FALSE;
            }
        }
        ok = EmitTree(cx, bce, pnlast);
        break;
      }

      case PNK_UPVARS:
        JS_ASSERT(pn->pn_names->count() != 0);
        bce->roLexdeps = pn->pn_names;
        ok = EmitTree(cx, bce, pn->pn_tree);
        bce->roLexdeps.clearMap();
        pn->pn_names.releaseMap(cx);
        break;

      case PNK_IF:
        ok = EmitIf(cx, bce, pn);
        break;

      case PNK_SWITCH:
        ok = EmitSwitch(cx, bce, pn);
        break;

      case PNK_WHILE:
        ok = EmitWhile(cx, bce, pn, top);
        break;

      case PNK_DOWHILE:
        ok = EmitDo(cx, bce, pn);
        break;

      case PNK_FOR:
        ok = EmitFor(cx, bce, pn, top);
        break;

      case PNK_BREAK:
        ok = EmitBreak(cx, bce, pn->asBreakStatement().label());
        break;

      case PNK_CONTINUE:
        ok = EmitContinue(cx, bce, pn->asContinueStatement().label());
        break;

      case PNK_WITH:
        ok = EmitWith(cx, bce, pn);
        break;

      case PNK_TRY:
        if (!EmitTry(cx, bce, pn))
            return false;
        break;

      case PNK_CATCH:
        if (!EmitCatch(cx, bce, pn))
            return false;
        break;

      case PNK_VAR:
      case PNK_CONST:
        if (!EmitVariables(cx, bce, pn, InitializeVars))
            return JS_FALSE;
        break;

      case PNK_RETURN:
        ok = EmitReturn(cx, bce, pn);
        break;

#if JS_HAS_GENERATORS
      case PNK_YIELD:
        JS_ASSERT(bce->inFunction());
        if (pn->pn_kid) {
            if (!EmitTree(cx, bce, pn->pn_kid))
                return JS_FALSE;
        } else {
            if (Emit1(cx, bce, JSOP_UNDEFINED) < 0)
                return JS_FALSE;
        }
        if (pn->pn_hidden && NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
            return JS_FALSE;
        if (Emit1(cx, bce, JSOP_YIELD) < 0)
            return JS_FALSE;
        break;
#endif

#if JS_HAS_XML_SUPPORT
      case PNK_XMLCURLYEXPR:
        JS_ASSERT(pn->isArity(PN_UNARY));
        if (!EmitTree(cx, bce, pn->pn_kid))
            return JS_FALSE;
        if (Emit1(cx, bce, pn->getOp()) < 0)
            return JS_FALSE;
        break;
#endif

      case PNK_STATEMENTLIST:
        ok = EmitStatementList(cx, bce, pn, top);
        break;

      case PNK_SEQ:
        ok = EmitSyntheticStatements(cx, bce, pn, top);
        break;

      case PNK_SEMI:
        ok = EmitStatement(cx, bce, pn);
        break;

      case PNK_COLON:
        ok = EmitLabel(cx, bce, pn);
        break;

      case PNK_COMMA:
      {
        /*
         * Emit SRC_PCDELTA notes on each JSOP_POP between comma operands.
         * These notes help the decompiler bracket the bytecodes generated
         * from each sub-expression that follows a comma.
         */
        ptrdiff_t off = -1, noteIndex = -1;
        for (ParseNode *pn2 = pn->pn_head; ; pn2 = pn2->pn_next) {
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            ptrdiff_t tmp = bce->offset();
            if (noteIndex >= 0) {
                if (!SetSrcNoteOffset(cx, bce, (unsigned)noteIndex, 0, tmp-off))
                    return JS_FALSE;
            }
            if (!pn2->pn_next)
                break;
            off = tmp;
            noteIndex = NewSrcNote2(cx, bce, SRC_PCDELTA, 0);
            if (noteIndex < 0 ||
                Emit1(cx, bce, JSOP_POP) < 0) {
                return JS_FALSE;
            }
        }
        break;
      }

      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
      case PNK_SUBASSIGN:
      case PNK_BITORASSIGN:
      case PNK_BITXORASSIGN:
      case PNK_BITANDASSIGN:
      case PNK_LSHASSIGN:
      case PNK_RSHASSIGN:
      case PNK_URSHASSIGN:
      case PNK_MULASSIGN:
      case PNK_DIVASSIGN:
      case PNK_MODASSIGN:
        if (!EmitAssignment(cx, bce, pn->pn_left, pn->getOp(), pn->pn_right))
            return false;
        break;

      case PNK_CONDITIONAL:
        ok = EmitConditionalExpression(cx, bce, pn->asConditionalExpression());
        break;

      case PNK_OR:
      case PNK_AND:
        ok = EmitLogical(cx, bce, pn);
        break;

      case PNK_ADD:
      case PNK_SUB:
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_STRICTEQ:
      case PNK_EQ:
      case PNK_STRICTNE:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_IN:
      case PNK_INSTANCEOF:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_STAR:
      case PNK_DIV:
      case PNK_MOD:
        if (pn->isArity(PN_LIST)) {
            /* Left-associative operator chain: avoid too much recursion. */
            ParseNode *pn2 = pn->pn_head;
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            JSOp op = pn->getOp();
            while ((pn2 = pn2->pn_next) != NULL) {
                if (!EmitTree(cx, bce, pn2))
                    return JS_FALSE;
                if (Emit1(cx, bce, op) < 0)
                    return JS_FALSE;
            }
        } else {
#if JS_HAS_XML_SUPPORT
            unsigned oldflags;

      case PNK_DBLCOLON:
            JS_ASSERT(pn->getOp() != JSOP_XMLNAME);
            if (pn->isArity(PN_NAME)) {
                if (!EmitTree(cx, bce, pn->expr()))
                    return JS_FALSE;
                if (!EmitAtomOp(cx, pn, pn->getOp(), bce))
                    return JS_FALSE;
                break;
            }

            /*
             * Binary :: has a right operand that brackets arbitrary code,
             * possibly including a let (a = b) ... expression.  We must clear
             * TCF_IN_FOR_INIT to avoid mis-compiling such beasts.
             */
            oldflags = bce->flags;
            bce->flags &= ~TCF_IN_FOR_INIT;
#endif

            /* Binary operators that evaluate both operands unconditionally. */
            if (!EmitTree(cx, bce, pn->pn_left))
                return JS_FALSE;
            if (!EmitTree(cx, bce, pn->pn_right))
                return JS_FALSE;
#if JS_HAS_XML_SUPPORT
            bce->flags |= oldflags & TCF_IN_FOR_INIT;
#endif
            if (Emit1(cx, bce, pn->getOp()) < 0)
                return JS_FALSE;
        }
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        if (pn->getOp() == JSOP_XMLNAME) {
            if (!EmitXMLName(cx, pn, JSOP_XMLNAME, bce))
                return false;
        } else {
            JSOp op = pn->getOp();
            JS_ASSERT(op == JSOP_BINDXMLNAME || op == JSOP_SETXMLNAME);
            unsigned oldflags = bce->flags;
            bce->flags &= ~TCF_IN_FOR_INIT;
            if (!EmitTree(cx, bce, pn->pn_kid))
                return false;
            bce->flags |= oldflags & TCF_IN_FOR_INIT;
            if (Emit1(cx, bce, op) < 0)
                return false;
        }
        break;
#endif

      case PNK_THROW:
#if JS_HAS_XML_SUPPORT
      case PNK_AT:
      case PNK_DEFXMLNS:
        JS_ASSERT(pn->isArity(PN_UNARY));
        /* FALL THROUGH */
#endif
      case PNK_TYPEOF:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG:
        ok = EmitUnary(cx, bce, pn);
        break;

      case PNK_PREINCREMENT:
      case PNK_PREDECREMENT:
      case PNK_POSTINCREMENT:
      case PNK_POSTDECREMENT:
        ok = EmitIncOrDec(cx, bce, pn);
        break;

      case PNK_DELETE:
        ok = EmitDelete(cx, bce, pn);
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_FILTER:
      {
        JS_ASSERT(!bce->inStrictMode());

        if (!EmitTree(cx, bce, pn->pn_left))
            return JS_FALSE;
        ptrdiff_t jmp = EmitJump(cx, bce, JSOP_FILTER, 0);
        if (jmp < 0)
            return JS_FALSE;
        top = EmitLoopHead(cx, bce, pn->pn_right);
        if (top < 0)
            return JS_FALSE;
        if (!EmitTree(cx, bce, pn->pn_right))
            return JS_FALSE;
        SetJumpOffsetAt(bce, jmp);
        if (!EmitLoopEntry(cx, bce, NULL))
            return false;
        if (EmitJump(cx, bce, JSOP_ENDFILTER, top - bce->offset()) < 0)
            return JS_FALSE;
        break;
      }
#endif

      case PNK_DOT:
        /*
         * Pop a stack operand, convert it to object, get a property named by
         * this bytecode's immediate-indexed atom operand, and push its value
         * (not a reference to it).
         */
        ok = EmitPropOp(cx, pn, pn->getOp(), bce, JS_FALSE);
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_DBLDOT:
        JS_ASSERT(!bce->inStrictMode());
        /* FALL THROUGH */
#endif
      case PNK_LB:
        /*
         * Pop two operands, convert the left one to object and the right one
         * to property name (atom or tagged int), get the named property, and
         * push its value.  Set the "obj" register to the result of ToObject
         * on the left operand.
         */
        ok = EmitElemOp(cx, pn, pn->getOp(), bce);
        break;

      case PNK_NEW:
      case PNK_LP:
        ok = EmitCallOrNew(cx, bce, pn, top);
        break;

      case PNK_LEXICALSCOPE:
        ok = EmitLexicalScope(cx, bce, pn);
        break;

#if JS_HAS_BLOCK_SCOPE
      case PNK_LET:
        ok = pn->isArity(PN_BINARY)
             ? EmitLet(cx, bce, pn)
             : EmitVariables(cx, bce, pn, InitializeVars);
        break;
#endif /* JS_HAS_BLOCK_SCOPE */
#if JS_HAS_GENERATORS
      case PNK_ARRAYPUSH: {
        jsint slot;

        /*
         * The array object's stack index is in bce->arrayCompDepth. See below
         * under the array initialiser code generator for array comprehension
         * special casing.
         */
        if (!EmitTree(cx, bce, pn->pn_kid))
            return JS_FALSE;
        slot = AdjustBlockSlot(cx, bce, bce->arrayCompDepth);
        if (slot < 0)
            return JS_FALSE;
        EMIT_UINT16_IMM_OP(pn->getOp(), slot);
        break;
      }
#endif

      case PNK_RB:
#if JS_HAS_GENERATORS
      case PNK_ARRAYCOMP:
#endif
        ok = EmitArray(cx, bce, pn);
        break;

      case PNK_RC:
        ok = EmitObject(cx, bce, pn);
        break;

      case PNK_NAME:
        /*
         * Cope with a left-over function definition that was replaced by a use
         * of a later function definition of the same name. See FunctionDef and
         * MakeDefIntoUse in Parser.cpp.
         */
        if (pn->isOp(JSOP_NOP))
            break;
        if (!EmitNameOp(cx, bce, pn, JS_FALSE))
            return JS_FALSE;
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_XMLATTR:
      case PNK_XMLSPACE:
      case PNK_XMLTEXT:
      case PNK_XMLCDATA:
      case PNK_XMLCOMMENT:
        JS_ASSERT(!bce->inStrictMode());
        /* FALL THROUGH */
#endif
      case PNK_STRING:
        ok = EmitAtomOp(cx, pn, pn->getOp(), bce);
        break;

      case PNK_NUMBER:
        ok = EmitNumberOp(cx, pn->pn_dval, bce);
        break;

      case PNK_REGEXP:
        JS_ASSERT(pn->isOp(JSOP_REGEXP));
        ok = EmitRegExp(cx, bce->regexpList.index(pn->pn_objbox), bce);
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_ANYNAME:
#endif
      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_THIS:
      case PNK_NULL:
        if (Emit1(cx, bce, pn->getOp()) < 0)
            return JS_FALSE;
        break;

      case PNK_DEBUGGER:
        if (Emit1(cx, bce, JSOP_DEBUGGER) < 0)
            return JS_FALSE;
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_XMLELEM:
      case PNK_XMLLIST:
        JS_ASSERT(!bce->inStrictMode());
        JS_ASSERT(pn->isKind(PNK_XMLLIST) || pn->pn_count != 0);

        switch (pn->pn_head ? pn->pn_head->getKind() : PNK_XMLLIST) {
          case PNK_XMLETAGO:
            JS_ASSERT(0);
            /* FALL THROUGH */
          case PNK_XMLPTAGC:
          case PNK_XMLSTAGO:
            break;
          default:
            if (Emit1(cx, bce, JSOP_STARTXML) < 0)
                return JS_FALSE;
        }

        for (ParseNode *pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (pn2->isKind(PNK_XMLCURLYEXPR) && Emit1(cx, bce, JSOP_STARTXMLEXPR) < 0)
                return JS_FALSE;
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            if (pn2 != pn->pn_head && Emit1(cx, bce, JSOP_ADD) < 0)
                return JS_FALSE;
        }

        if (pn->pn_xflags & PNX_XMLROOT) {
            if (pn->pn_count == 0) {
                JS_ASSERT(pn->isKind(PNK_XMLLIST));
                JSAtom *atom = cx->runtime->atomState.emptyAtom;
                jsatomid index;
                if (!bce->makeAtomIndex(atom, &index))
                    return JS_FALSE;
                if (!EmitIndex32(cx, JSOP_STRING, index, bce))
                    return false;
            }
            if (Emit1(cx, bce, pn->getOp()) < 0)
                return JS_FALSE;
        }
#ifdef DEBUG
        else
            JS_ASSERT(pn->pn_count != 0);
#endif
        break;

      case PNK_XMLPTAGC:
      case PNK_XMLSTAGO:
      case PNK_XMLETAGO:
        if (!EmitXMLTag(cx, bce, pn))
            return false;
        break;

      case PNK_XMLNAME:
        JS_ASSERT(!bce->inStrictMode());

        if (pn->isArity(PN_LIST)) {
            JS_ASSERT(pn->pn_count != 0);
            for (ParseNode *pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
                if (pn2->isKind(PNK_XMLCURLYEXPR) && Emit1(cx, bce, JSOP_STARTXMLEXPR) < 0)
                    return JS_FALSE;
                if (!EmitTree(cx, bce, pn2))
                    return JS_FALSE;
                if (pn2 != pn->pn_head && Emit1(cx, bce, JSOP_ADD) < 0)
                    return JS_FALSE;
            }
        } else {
            JS_ASSERT(pn->isArity(PN_NULLARY));
            ok = pn->isOp(JSOP_OBJECT)
                 ? EmitObjectOp(cx, pn->pn_objbox, pn->getOp(), bce)
                 : EmitAtomOp(cx, pn, pn->getOp(), bce);
        }
        break;

      case PNK_XMLPI:
        if (!EmitXMLProcessingInstruction(cx, bce, pn->asXMLProcessingInstruction()))
            return false;
        break;
#endif /* JS_HAS_XML_SUPPORT */

      default:
        JS_ASSERT(0);
    }

    /* bce->emitLevel == 1 means we're last on the stack, so finish up. */
    if (ok && bce->emitLevel == 1) {
        if (!UpdateLineNumberNotes(cx, bce, pn->pn_pos.end.lineno))
            return JS_FALSE;
    }

    return ok;
}

static int
AllocSrcNote(JSContext *cx, BytecodeEmitter *bce)
{
    jssrcnote *notes = bce->notes();
    jssrcnote *newnotes;
    unsigned index = bce->noteCount();
    unsigned max = bce->noteLimit();

    if (index == max) {
        size_t newlength;
        if (!notes) {
            JS_ASSERT(!index && !max);
            newlength = SRCNOTE_CHUNK_LENGTH;
            newnotes = (jssrcnote *) cx->malloc_(SRCNOTE_SIZE(newlength));
        } else {
            JS_ASSERT(index <= max);
            newlength = max * 2;
            newnotes = (jssrcnote *) cx->realloc_(notes, SRCNOTE_SIZE(newlength));
        }
        if (!newnotes) {
            js_ReportOutOfMemory(cx);
            return -1;
        }
        bce->current->notes = newnotes;
        bce->current->noteLimit = newlength;
    }

    bce->current->noteCount = index + 1;
    return (int)index;
}

int
frontend::NewSrcNote(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type)
{
    int index, n;
    jssrcnote *sn;
    ptrdiff_t offset, delta, xdelta;

    /*
     * Claim a note slot in bce->notes() by growing it if necessary and then
     * incrementing bce->noteCount().
     */
    index = AllocSrcNote(cx, bce);
    if (index < 0)
        return -1;
    sn = &bce->notes()[index];

    /*
     * Compute delta from the last annotated bytecode's offset.  If it's too
     * big to fit in sn, allocate one or more xdelta notes and reset sn.
     */
    offset = bce->offset();
    delta = offset - bce->lastNoteOffset();
    bce->current->lastNoteOffset = offset;
    if (delta >= SN_DELTA_LIMIT) {
        do {
            xdelta = JS_MIN(delta, SN_XDELTA_MASK);
            SN_MAKE_XDELTA(sn, xdelta);
            delta -= xdelta;
            index = AllocSrcNote(cx, bce);
            if (index < 0)
                return -1;
            sn = &bce->notes()[index];
        } while (delta >= SN_DELTA_LIMIT);
    }

    /*
     * Initialize type and delta, then allocate the minimum number of notes
     * needed for type's arity.  Usually, we won't need more, but if an offset
     * does take two bytes, SetSrcNoteOffset will grow bce->notes().
     */
    SN_MAKE_NOTE(sn, type, delta);
    for (n = (int)js_SrcNoteSpec[type].arity; n > 0; n--) {
        if (NewSrcNote(cx, bce, SRC_NULL) < 0)
            return -1;
    }
    return index;
}

int
frontend::NewSrcNote2(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type, ptrdiff_t offset)
{
    int index;

    index = NewSrcNote(cx, bce, type);
    if (index >= 0) {
        if (!SetSrcNoteOffset(cx, bce, index, 0, offset))
            return -1;
    }
    return index;
}

int
frontend::NewSrcNote3(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type, ptrdiff_t offset1,
            ptrdiff_t offset2)
{
    int index;

    index = NewSrcNote(cx, bce, type);
    if (index >= 0) {
        if (!SetSrcNoteOffset(cx, bce, index, 0, offset1))
            return -1;
        if (!SetSrcNoteOffset(cx, bce, index, 1, offset2))
            return -1;
    }
    return index;
}

static JSBool
GrowSrcNotes(JSContext *cx, BytecodeEmitter *bce)
{
    size_t newlength = bce->noteLimit() * 2;
    jssrcnote *newnotes = (jssrcnote *) cx->realloc_(bce->notes(), newlength);
    if (!newnotes) {
        js_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    bce->current->notes = newnotes;
    bce->current->noteLimit = newlength;
    return JS_TRUE;
}

jssrcnote *
frontend::AddToSrcNoteDelta(JSContext *cx, BytecodeEmitter *bce, jssrcnote *sn, ptrdiff_t delta)
{
    ptrdiff_t base, limit, newdelta, diff;
    int index;

    /*
     * Called only from OptimizeSpanDeps and FinishTakingSrcNotes to add to
     * main script note deltas, and only by a small positive amount.
     */
    JS_ASSERT(bce->current == &bce->main);
    JS_ASSERT((unsigned) delta < (unsigned) SN_XDELTA_LIMIT);

    base = SN_DELTA(sn);
    limit = SN_IS_XDELTA(sn) ? SN_XDELTA_LIMIT : SN_DELTA_LIMIT;
    newdelta = base + delta;
    if (newdelta < limit) {
        SN_SET_DELTA(sn, newdelta);
    } else {
        index = sn - bce->main.notes;
        if (bce->main.noteCount == bce->main.noteLimit) {
            if (!GrowSrcNotes(cx, bce))
                return NULL;
            sn = bce->main.notes + index;
        }
        diff = bce->main.noteCount - index;
        bce->main.noteCount++;
        memmove(sn + 1, sn, SRCNOTE_SIZE(diff));
        SN_MAKE_XDELTA(sn, delta);
        sn++;
    }
    return sn;
}

static JSBool
SetSrcNoteOffset(JSContext *cx, BytecodeEmitter *bce, unsigned index, unsigned which, ptrdiff_t offset)
{
    jssrcnote *sn;
    ptrdiff_t diff;

    if (size_t(offset) > SN_MAX_OFFSET) {
        ReportStatementTooLarge(cx, bce);
        return JS_FALSE;
    }

    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    sn = &bce->notes()[index];
    JS_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    JS_ASSERT((int) which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }

    /*
     * See if the new offset requires three bytes either by being too big or if
     * the offset has already been inflated (in which case, we need to stay big
     * to not break the srcnote encoding if this isn't the last srcnote).
     */
    if (offset > (ptrdiff_t)SN_3BYTE_OFFSET_MASK || (*sn & SN_3BYTE_OFFSET_FLAG)) {
        /* Maybe this offset was already set to a three-byte value. */
        if (!(*sn & SN_3BYTE_OFFSET_FLAG)) {
            /* Losing, need to insert another two bytes for this offset. */
            index = sn - bce->notes();

            /*
             * Test to see if the source note array must grow to accommodate
             * either the first or second byte of additional storage required
             * by this 3-byte offset.
             */
            if (bce->noteCount() + 1 >= bce->noteLimit()) {
                if (!GrowSrcNotes(cx, bce))
                    return JS_FALSE;
                sn = bce->notes() + index;
            }
            bce->current->noteCount += 2;

            diff = bce->noteCount() - (index + 3);
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

#ifdef DEBUG_notme
#define DEBUG_srcnotesize
#endif

#ifdef DEBUG_srcnotesize
#define NBINS 10
static uint32_t hist[NBINS];

static void
DumpSrcNoteSizeHist()
{
    static FILE *fp;
    int i, n;

    if (!fp) {
        fp = fopen("/tmp/srcnotes.hist", "w");
        if (!fp)
            return;
        setvbuf(fp, NULL, _IONBF, 0);
    }
    fprintf(fp, "SrcNote size histogram:\n");
    for (i = 0; i < NBINS; i++) {
        fprintf(fp, "%4u %4u ", JS_BIT(i), hist[i]);
        for (n = (int) JS_HOWMANY(hist[i], 10); n > 0; --n)
            fputc('*', fp);
        fputc('\n', fp);
    }
    fputc('\n', fp);
}
#endif

/*
 * Fill in the storage at notes with prolog and main srcnotes; the space at
 * notes was allocated using the BytecodeEmitter::countFinalSourceNotes()
 * method from BytecodeEmitter.h. SO DON'T CHANGE THIS FUNCTION WITHOUT AT
 * LEAST CHECKING WHETHER BytecodeEmitter::countFinalSourceNotes() NEEDS
 * CORRESPONDING CHANGES!
 */
JSBool
frontend::FinishTakingSrcNotes(JSContext *cx, BytecodeEmitter *bce, jssrcnote *notes)
{
    unsigned prologCount, mainCount, totalCount;
    ptrdiff_t offset, delta;
    jssrcnote *sn;

    JS_ASSERT(bce->current == &bce->main);

    prologCount = bce->prolog.noteCount;
    if (prologCount && bce->prolog.currentLine != bce->firstLine) {
        bce->switchToProlog();
        if (NewSrcNote2(cx, bce, SRC_SETLINE, (ptrdiff_t)bce->firstLine) < 0)
            return false;
        prologCount = bce->prolog.noteCount;
        bce->switchToMain();
    } else {
        /*
         * Either no prolog srcnotes, or no line number change over prolog.
         * We don't need a SRC_SETLINE, but we may need to adjust the offset
         * of the first main note, by adding to its delta and possibly even
         * prepending SRC_XDELTA notes to it to account for prolog bytecodes
         * that came at and after the last annotated bytecode.
         */
        offset = bce->prologOffset() - bce->prolog.lastNoteOffset;
        JS_ASSERT(offset >= 0);
        if (offset > 0 && bce->main.noteCount != 0) {
            /* NB: Use as much of the first main note's delta as we can. */
            sn = bce->main.notes;
            delta = SN_IS_XDELTA(sn)
                    ? SN_XDELTA_MASK - (*sn & SN_XDELTA_MASK)
                    : SN_DELTA_MASK - (*sn & SN_DELTA_MASK);
            if (offset < delta)
                delta = offset;
            for (;;) {
                if (!AddToSrcNoteDelta(cx, bce, sn, delta))
                    return false;
                offset -= delta;
                if (offset == 0)
                    break;
                delta = JS_MIN(offset, SN_XDELTA_MASK);
                sn = bce->main.notes;
            }
        }
    }

    mainCount = bce->main.noteCount;
    totalCount = prologCount + mainCount;
    if (prologCount)
        PodCopy(notes, bce->prolog.notes, prologCount);
    PodCopy(notes + prologCount, bce->main.notes, mainCount);
    SN_MAKE_TERMINATOR(&notes[totalCount]);

    return true;
}

static JSBool
NewTryNote(JSContext *cx, BytecodeEmitter *bce, JSTryNoteKind kind, unsigned stackDepth, size_t start,
           size_t end)
{
    JS_ASSERT((unsigned)(uint16_t)stackDepth == stackDepth);
    JS_ASSERT(start <= end);
    JS_ASSERT((size_t)(uint32_t)start == start);
    JS_ASSERT((size_t)(uint32_t)end == end);

    TryNode *tryNode = cx->tempLifoAlloc().new_<TryNode>();
    if (!tryNode) {
        js_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    tryNode->note.kind = kind;
    tryNode->note.stackDepth = (uint16_t)stackDepth;
    tryNode->note.start = (uint32_t)start;
    tryNode->note.length = (uint32_t)(end - start);
    tryNode->prev = bce->lastTryNode;
    bce->lastTryNode = tryNode;
    bce->ntrynotes++;
    return JS_TRUE;
}

void
frontend::FinishTakingTryNotes(BytecodeEmitter *bce, JSTryNoteArray *array)
{
    TryNode *tryNode;
    JSTryNote *tn;

    JS_ASSERT(array->length > 0 && array->length == bce->ntrynotes);
    tn = array->vector + array->length;
    tryNode = bce->lastTryNode;
    do {
        *--tn = tryNode->note;
    } while ((tryNode = tryNode->prev) != NULL);
    JS_ASSERT(tn == array->vector);
}

/*
 * Find the index of the given object for code generator.
 *
 * Since the emitter refers to each parsed object only once, for the index we
 * use the number of already indexes objects. We also add the object to a list
 * to convert the list to a fixed-size array when we complete code generation,
 * see js::CGObjectList::finish below.
 *
 * Most of the objects go to BytecodeEmitter::objectList but for regexp we use
 * a separated BytecodeEmitter::regexpList. In this way the emitted index can
 * be directly used to store and fetch a reference to a cloned RegExp object
 * that shares the same JSRegExp private data created for the object literal in
 * objbox. We need a cloned object to hold lastIndex and other direct
 * properties that should not be shared among threads sharing a precompiled
 * function or script.
 *
 * If the code being compiled is function code, allocate a reserved slot in
 * the cloned function object that shares its precompiled script with other
 * cloned function objects and with the compiler-created clone-parent. There
 * are nregexps = script->regexps()->length such reserved slots in each
 * function object cloned from fun->object. NB: during compilation, a funobj
 * slots element must never be allocated, because JSObject::allocSlot could
 * hand out one of the slots that should be given to a regexp clone.
 *
 * If the code being compiled is global code, the cloned regexp are stored in
 * fp->vars slot and to protect regexp slots from GC we set fp->nvars to
 * nregexps.
 *
 * The slots initially contain undefined or null. We populate them lazily when
 * JSOP_REGEXP is executed for the first time.
 *
 * Why clone regexp objects?  ECMA specifies that when a regular expression
 * literal is scanned, a RegExp object is created.  In the spec, compilation
 * and execution happen indivisibly, but in this implementation and many of
 * its embeddings, code is precompiled early and re-executed in multiple
 * threads, or using multiple global objects, or both, for efficiency.
 *
 * In such cases, naively following ECMA leads to wrongful sharing of RegExp
 * objects, which makes for collisions on the lastIndex property (especially
 * for global regexps) and on any ad-hoc properties.  Also, __proto__ refers to
 * the pre-compilation prototype, a pigeon-hole problem for instanceof tests.
 */
unsigned
CGObjectList::index(ObjectBox *objbox)
{
    JS_ASSERT(!objbox->emitLink);
    objbox->emitLink = lastbox;
    lastbox = objbox;
    return length++;
}

void
CGObjectList::finish(JSObjectArray *array)
{
    JS_ASSERT(length <= INDEX_LIMIT);
    JS_ASSERT(length == array->length);

    js::HeapPtrObject *cursor = array->vector + array->length;
    ObjectBox *objbox = lastbox;
    do {
        --cursor;
        JS_ASSERT(!*cursor);
        *cursor = objbox->object;
    } while ((objbox = objbox->emitLink) != NULL);
    JS_ASSERT(cursor == array->vector);
}

void
GCConstList::finish(JSConstArray *array)
{
    JS_ASSERT(array->length == list.length());
    Value *src = list.begin(), *srcend = list.end();
    HeapValue *dst = array->vector;
    for (; src != srcend; ++src, ++dst)
        *dst = *src;
}

/*
 * We should try to get rid of offsetBias (always 0 or 1, where 1 is
 * JSOP_{NOP,POP}_LENGTH), which is used only by SRC_FOR and SRC_DECL.
 */
JS_FRIEND_DATA(JSSrcNoteSpec) js_SrcNoteSpec[] = {
    {"null",            0},
    {"if",              0},
    {"if-else",         2},
    {"for",             3},
    {"while",           1},
    {"continue",        0},
    {"decl",            1},
    {"pcdelta",         1},
    {"assignop",        0},
    {"cond",            1},
    {"brace",           1},
    {"hidden",          0},
    {"pcbase",          1},
    {"label",           1},
    {"labelbrace",      1},
    {"endbrace",        0},
    {"break2label",     1},
    {"cont2label",      1},
    {"switch",          2},
    {"funcdef",         1},
    {"catch",           1},
    {"unused",         -1},
    {"newline",         0},
    {"setline",         1},
    {"xdelta",          0},
};

JS_FRIEND_API(unsigned)
js_SrcNoteLength(jssrcnote *sn)
{
    unsigned arity;
    jssrcnote *base;

    arity = (int)js_SrcNoteSpec[SN_TYPE(sn)].arity;
    for (base = sn++; arity; sn++, arity--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }
    return sn - base;
}

JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, unsigned which)
{
    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    JS_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    JS_ASSERT((int) which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }
    if (*sn & SN_3BYTE_OFFSET_FLAG) {
        return (ptrdiff_t)(((uint32_t)(sn[0] & SN_3BYTE_OFFSET_MASK) << 16)
                           | (sn[1] << 8)
                           | sn[2]);
    }
    return (ptrdiff_t)*sn;
}
