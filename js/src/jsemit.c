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
#include <memory.h>
#include <stddef.h>
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
#include "jsemit.h"
#include "jsopcode.h"

#define CGINCR  256     /* code generation bytecode allocation increment */
#define SNINCR  64      /* source note allocation increment */

JS_FRIEND_API(JSBool)
js_InitCodeGenerator(JSContext *cx, JSCodeGenerator *cg, PRArenaPool *pool)
{
    cg->pool = pool;
    PR_ARENA_ALLOCATE(cg->base, pool, CGINCR);
    if (!cg->base) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    cg->limit = CG_CODE(cg, CGINCR);
    CG_RESET(cg);
    return JS_TRUE;
}

static ptrdiff_t
EmitCheck(JSContext *cx, JSCodeGenerator *cg, JSOp op, intN delta)
{
    ptrdiff_t offset, length;
    size_t cgsize;

    PR_ASSERT(delta < CGINCR);
    offset = CG_OFFSET(cg);
    if (op != JSOP_NOP)
	cg->lastCodeOffset = offset;
    if ((pruword)cg->ptr + delta >= (pruword)cg->limit) {
        length = cg->limit - cg->base;
	cgsize = length * sizeof(jsbytecode);
        PR_ARENA_GROW(cg->base, cg->pool, cgsize, CGINCR);
	if (!cg->base) {
	    JS_ReportOutOfMemory(cx);
	    return -1;
	}
        cg->limit = CG_CODE(cg, length + CGINCR);
        cg->ptr = CG_CODE(cg, offset);
    }
    return offset;
}

/*
 * Cancel the last instruction.  This can't be called more than once between
 * bytecode generations using cg.  Most of the work involves finding and then
 * cancelling any related source notes.
 */
void
js_CancelLastOpcode(JSContext *cx, JSCodeGenerator *cg, uint16 *newlinesp)
{
    ptrdiff_t offset, lastCodeOffset;
    ptrdiff_t shrink, delta;
    JSCodeSpec *cs;
    jssrcnote *sn, *last, *next;

    lastCodeOffset = cg->lastCodeOffset;
    shrink = CG_OFFSET(cg) - lastCodeOffset;
    if (shrink <= 0)
	return;

    cg->ptr = CG_CODE(cg, lastCodeOffset);
    cs = &js_CodeSpec[*cg->ptr];
    PR_ASSERT(cs->nuses >= 0);
    cg->stackDepth += cs->nuses;
    cg->stackDepth -= cs->ndefs;

    if (cg->noteCount) {
	offset = cg->saveNoteOffset;
	last = &cg->notes[cg->lastNoteCount];
	for (sn = &cg->notes[cg->saveNoteCount]; sn <= last; sn = next) {
	    next = SN_NEXT(sn);
	    delta = SN_DELTA(sn);
	    offset += delta;
	    if (offset == lastCodeOffset) {
		/* The canceled op may not have any non-newline note. */
		if (SN_IS_GETTABLE(sn)) {
		    SN_MAKE_TERMINATOR(sn);
		    cg->noteCount = cg->saveNoteCount;
		    cg->lastNoteOffset = cg->saveNoteOffset;

		    /* Clear so we'll know to increment *newlinesp below. */
		    shrink = 0;
		}
	    } else if (offset > lastCodeOffset) {
		/* Notes after the canceled op are newlines or xdeltas. */
		if (shrink == 0) {
		    if (SN_TYPE(sn) == SRC_NEWLINE) {
			/* Newlines after the canceled op are pushed back. */
			(*newlinesp)++;
		    }
		} else if (delta >= shrink) {
		    /* If there's enough delta to take away shrink, do it. */
		    delta -= shrink;
		    SN_SET_DELTA(sn, delta);
		}
	    }
	}
    }
}

void
js_UpdateDepth(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t target)
{
    jsbytecode *pc;
    jssrcnote *sn, *last;
    ptrdiff_t offset;
    JSCodeSpec *cs;
    intN nuses;

    pc = CG_CODE(cg, target);

    /*
     * If there are source notes, check for hidden JSOP_LEAVEWITH created when
     * breaking or continuing out of a with statement, or a JSOP_POP generated
     * when breaking or continuing out of a for/in loop.  The compiler ensures
     * stack balance for such bytecodes, but if we account for their popping
     * here, we'll misreport an underflow.
     */
    if (cg->noteCount) {
	sn = cg->notes;
	last = &sn[cg->saveNoteCount];
	if (SN_TYPE(last) == SRC_HIDDEN) {
	    /* We must ensure that this source note applies to target. */
	    for (offset = 0; sn <= last; sn = SN_NEXT(sn)) {
		offset += SN_DELTA(sn);
		if (offset == target && sn == last)
		    return;
	    }
	}
    }

    cs = &js_CodeSpec[pc[0]];
    nuses = cs->nuses;
    if (nuses < 0)
        nuses = 2 + GET_ARGC(pc);	/* stack: fun, this, [argc arguments] */
    cg->stackDepth -= nuses;
    if (cg->stackDepth < 0)
        JS_ReportError(cx, "internal error: stack underflow at pc %d", target);
    cg->stackDepth += cs->ndefs;
    if ((uintN)cg->stackDepth > cg->maxStackDepth)
        cg->maxStackDepth = cg->stackDepth;
}

ptrdiff_t
js_Emit1(JSContext *cx, JSCodeGenerator *cg, JSOp op)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 1);

    if (offset >= 0) {
	*cg->ptr++ = (jsbytecode)op;
	js_UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_Emit2(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 2);

    if (offset >= 0) {
	cg->ptr[0] = (jsbytecode)op;
	cg->ptr[1] = op1;
	cg->ptr += 2;
	js_UpdateDepth(cx, cg, offset);
    }
    return offset;
}

ptrdiff_t
js_Emit3(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1,
	 jsbytecode op2)
{
    ptrdiff_t offset = EmitCheck(cx, cg, op, 3);

    if (offset >= 0) {
	cg->ptr[0] = (jsbytecode)op;
	cg->ptr[1] = op1;
	cg->ptr[2] = op2;
	cg->ptr += 3;
	js_UpdateDepth(cx, cg, offset);
    }
    return offset;
}

static const char *statementName[] = {
    "block",            /* BLOCK */
    "label statement",  /* LABEL */
    "if statement",     /* IF */
    "else statement",   /* ELSE */
    "switch statement", /* SWITCH */
    "with statement",   /* WITH */
    "do loop",          /* DO_LOOP */
    "for loop",         /* FOR_LOOP */
    "for/in loop",      /* FOR_IN_LOOP */
    "while loop",       /* WHILE_LOOP */
};

static const char *
StatementName(JSCodeGenerator *cg)
{
    if (!cg || !cg->stmtInfo)
	return "script";
    return statementName[cg->stmtInfo->type];
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

/* Forward declaration to keep MoveGotos after PatchGotos. */
static void
MoveGotos(JSCodeGenerator *cg, StmtInfo *stmt, ptrdiff_t last, ptrdiff_t first,
	  ptrdiff_t growth);

ptrdiff_t
js_MoveCode(JSContext *cx,
	    JSCodeGenerator *from, ptrdiff_t fromOffset,
	    JSCodeGenerator *to, ptrdiff_t toOffset)
{
    ptrdiff_t length, growth, offset, saveOffset;
    StmtInfo *stmt;

    length = CG_OFFSET(from) - fromOffset;
    growth = (toOffset + length) - CG_OFFSET(to);
    saveOffset = to->lastCodeOffset;
    offset = EmitCheck(cx, to, *CG_CODE(from, from->lastCodeOffset), growth);
    if (offset >= 0) {
	memmove(CG_CODE(to, toOffset), CG_CODE(from, fromOffset), length);
	to->ptr += growth;
	if (to == from) {
	    growth = toOffset - fromOffset;
	    for (stmt = to->stmtInfo; stmt; stmt = stmt->down) {
		if (stmt->top >= fromOffset)
		    stmt->top += growth;
		if (stmt->update >= fromOffset)
		    stmt->update += growth;
		if (stmt->breaks >= fromOffset) {
		    stmt->breaks += growth;
		    MoveGotos(to, stmt, stmt->breaks, toOffset, growth);
		}
		if (stmt->continues >= fromOffset) {
		    stmt->continues += growth;
		    MoveGotos(to, stmt, stmt->continues, toOffset, growth);
		}
	    }
	    to->lastCodeOffset = saveOffset + growth;
	} else {
	    /* For now, we handle expressions only, not statements. */
	    PR_ASSERT(!from->stmtInfo);
	    to->lastCodeOffset += from->lastCodeOffset - fromOffset;
	    if (from->maxStackDepth > to->maxStackDepth)
		to->maxStackDepth = from->maxStackDepth;
	}
    }
    return offset;
}

void
js_PushStatement(JSCodeGenerator *cg, StmtInfo *stmt, StmtType type,
		 ptrdiff_t top)
{
    stmt->type = type;
    SET_STATEMENT_TOP(stmt, top);
    stmt->label = NULL;
    stmt->down = cg->stmtInfo;
    cg->stmtInfo = stmt;
}

static ptrdiff_t
EmitGoto(JSContext *cx, JSCodeGenerator *cg, StmtInfo *toStmt,
	 ptrdiff_t *last, JSAtom *label, JSSrcNoteType noteType)
{
    StmtInfo *stmt;
    intN index;
    ptrdiff_t offset, delta;

    for (stmt = cg->stmtInfo; stmt != toStmt; stmt = stmt->down) {
	switch (stmt->type) {
	  case STMT_WITH:
	    if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
		js_Emit1(cx, cg, JSOP_LEAVEWITH) < 0) {
		return -1;
	    }
	    break;
	  case STMT_FOR_IN_LOOP:
	    if (js_NewSrcNote(cx, cg, SRC_HIDDEN) < 0 ||
		js_Emit1(cx, cg, JSOP_POP) < 0) {
		return -1;
	    }
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
PatchGotos(JSContext *cx, JSCodeGenerator *cg, StmtInfo *stmt, ptrdiff_t last,
	   jsbytecode *target)
{
    jsbytecode *pc, *top;
    ptrdiff_t delta, jumpOffset;

    pc = CG_CODE(cg, last);
    top = CG_CODE(cg, stmt->top);
    while (pc > top) {
	PR_ASSERT(*pc == JSOP_GOTO);
        delta = GET_JUMP_OFFSET(pc);
        jumpOffset = target - pc;
        CHECK_AND_SET_JUMP_OFFSET(cx, cg, pc, jumpOffset);
        pc -= delta;
    }
    return JS_TRUE;
}

static void
MoveGotos(JSCodeGenerator *cg, StmtInfo *stmt, ptrdiff_t last, ptrdiff_t first,
	  ptrdiff_t growth)
{
    jsbytecode *pc, *top, *stop, *prev;
    ptrdiff_t delta;

    pc = CG_CODE(cg, last);
    top = CG_CODE(cg, stmt->top);
    stop = CG_CODE(cg, first);
    while (pc > top) {
	PR_ASSERT(*pc == JSOP_GOTO);
        delta = GET_JUMP_OFFSET(pc);
	prev = pc - delta;
	if (prev < stop) {
	    delta += growth;
	    SET_JUMP_OFFSET(pc, delta);
	    return;
	}
        pc = prev;
    }
}

ptrdiff_t
js_EmitBreak(JSContext *cx, JSCodeGenerator *cg, StmtInfo *stmt,
	     JSAtom *label)
{
    return EmitGoto(cx, cg, stmt, &stmt->breaks, label, SRC_BREAK2LABEL);
}

ptrdiff_t
js_EmitContinue(JSContext *cx, JSCodeGenerator *cg, StmtInfo *stmt,
		JSAtom *label)
{
    return EmitGoto(cx, cg, stmt, &stmt->continues, label, SRC_CONT2LABEL);
}

JSBool
js_PopStatement(JSContext *cx, JSCodeGenerator *cg)
{
    StmtInfo *stmt;

    stmt = cg->stmtInfo;
    if (!PatchGotos(cx, cg, stmt, stmt->breaks, cg->ptr))
	return JS_FALSE;
    if (!PatchGotos(cx, cg, stmt, stmt->continues, CG_CODE(cg, stmt->update)))
	return JS_FALSE;
    cg->stmtInfo = stmt->down;
    return JS_TRUE;
}

const char *js_SrcNoteName[] = {
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
    "unused19",
    "unused20",
    "unused21",
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
    0,  /* SRC_UNUSED19 */
    0,  /* SRC_UNUSED20 */
    0,  /* SRC_UNUSED21 */
    0,  /* SRC_NEWLINE */
    1,  /* SRC_SETLINE */
    0   /* SRC_XDELTA */
};

intN
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

    /*
     * Set lastNoteCount here, after any recursion, so it has the lowest
     * index value among all note bytes allocated by the non-recursive call
     * that claimed the first byte.
     */
    cg->lastNoteCount = index;
    if (type != SRC_NULL && type < SRC_NEWLINE) {
	cg->saveNoteCount = index;
	cg->saveNoteOffset = saveOffset;
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

JSBool
js_BumpSrcNoteDelta(JSContext *cx, JSCodeGenerator *cg, uintN index,
		    ptrdiff_t incr)
{
    jssrcnote *sn;
    ptrdiff_t delta, limit, xdelta;
    uintN length;

    sn = &cg->notes[index];
    delta = SN_DELTA(sn) + incr;
    limit = SN_IS_XDELTA(sn) ? SN_XDELTA_LIMIT : SN_DELTA_LIMIT;
    while (delta >= limit) {
	length = cg->noteCount - index;
	if (AllocSrcNote(cx, cg) < 0)
	    return JS_FALSE;
	sn = &cg->notes[index];
	memmove(sn + 1, sn, length * sizeof(jssrcnote));
	xdelta = PR_MIN(delta, SN_XDELTA_LIMIT - 1);
	SN_MAKE_XDELTA(sn, xdelta);
	sn++;
	delta -= xdelta;
    }
    SN_SET_DELTA(sn, delta);
    cg->lastNoteOffset += incr;
    if (index < cg->saveNoteCount)
	cg->saveNoteOffset += incr;
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

ptrdiff_t
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

JSBool
js_MoveSrcNotes(JSContext *cx, JSCodeGenerator *from, JSCodeGenerator *to)
{
    intN i, j, length, index;
    jssrcnote *sn;
    JSSrcNoteType type;
    ptrdiff_t delta, limit, xdelta;

    PR_ASSERT(from != to);
    for (i = 0; (uintN)i < from->noteCount; i += length) {
	length = SN_LENGTH(&from->notes[i]);
	type = SN_TYPE(&from->notes[i]);
	index = js_NewSrcNote(cx, to, type);
	if (index < 0)
	    return JS_FALSE;
	for (j = (intN)js_SrcNoteArity[type]; --j >= 0; ) {
	    if (!js_SetSrcNoteOffset(cx, to, index, j,
				     js_GetSrcNoteOffset(&from->notes[i], j))) {
		return JS_FALSE;
	    }
	}
	delta = SN_DELTA(&from->notes[i]);
	to->lastNoteOffset += delta;
	sn = &to->notes[index];
	if (i == 0) {
	    /* First note's delta must count bytecodes from last note in to. */
	    delta += SN_DELTA(sn);
	    limit = SN_IS_XDELTA(sn) ? SN_XDELTA_LIMIT : SN_DELTA_LIMIT;
	    if (delta >= limit) {
		do {
		    xdelta = PR_MIN(delta, SN_XDELTA_MASK);
		    SN_MAKE_XDELTA(sn, xdelta);
		    delta -= xdelta;
		    index = js_NewSrcNote(cx, to, SRC_NULL);
		    if (index < 0)
			return JS_FALSE;
		    sn = &to->notes[index];
		} while (delta >= limit);
	    }
	}
	SN_SET_DELTA(sn, delta);
    }
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
