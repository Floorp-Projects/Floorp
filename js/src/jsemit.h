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

#ifndef jsemit_h___
#define jsemit_h___
/*
 * JS bytecode generation.
 */
#include <stddef.h>
#include "prtypes.h"
#include "jsatom.h"
#include "jsopcode.h"
#include "jsprvtd.h"
#include "jspubtd.h"

PR_BEGIN_EXTERN_C

typedef enum StmtType {
    STMT_BLOCK        = 0,      /* compound statement: { s1[;... sN] } */
    STMT_LABEL        = 1,      /* labeled statement:  l: s */
    STMT_IF           = 2,      /* if (then) statement */
    STMT_ELSE         = 3,      /* else statement */
    STMT_SWITCH       = 4,      /* switch statement */
    STMT_WITH         = 5,      /* with statement */
    STMT_DO_LOOP      = 6,      /* do/while loop statement */
    STMT_FOR_LOOP     = 7,      /* for loop statement */
    STMT_FOR_IN_LOOP  = 8,      /* for/in loop statement */
    STMT_WHILE_LOOP   = 9       /* while loop statement */
} StmtType;

#define STMT_IS_LOOP(stmt)      ((stmt)->type >= STMT_DO_LOOP)

typedef struct SwitchCase SwitchCase;
typedef struct StmtInfo   StmtInfo;

struct StmtInfo {
    StmtType    type;           /* statement type */
    ptrdiff_t   top;            /* offset of loop top from cg base */
    ptrdiff_t   update;         /* loop update offset (top if none) */
    ptrdiff_t   breaks;         /* offset of last break in loop */
    ptrdiff_t   continues;      /* offset of last continue in loop */
    JSAtom      *label;         /* label name if type is STMT_LABEL */
    StmtInfo    *down;          /* info for enclosing statement */
};

struct SwitchCase {
    jsval       value;
    ptrdiff_t   offset;
    SwitchCase  *next;
};

#define SET_STATEMENT_TOP(stmt, top) \
    ((stmt)->top = (stmt)->update = (stmt)->breaks = (stmt)->continues = (top))

struct JSCodeGenerator {
    PRArenaPool *pool;          /* pool in which to allocate code */
    jsbytecode  *base;          /* base of JS bytecode vector */
    jsbytecode  *limit;         /* one byte beyond end of bytecode */
    jsbytecode  *ptr;           /* pointer to next free bytecode */
    JSAtomList  atomList;       /* literals indexed for mapping */
    JSSymbol    *args;          /* list of formal argument symbols */
    JSSymbol    *vars;          /* list of local variable symbols */
    ptrdiff_t   lastCodeOffset; /* offset of last non-nop opcode */
    StmtInfo    *stmtInfo;      /* statement stack for break/continue */
    intN        stackDepth;     /* current stack depth in basic block */
    uintN       maxStackDepth;  /* maximum stack depth so far */
    jssrcnote   *notes;         /* source notes, see below */
    uintN       noteCount;      /* number of source notes so far */
    uintN       lastNoteCount;  /* index of last emitted source note */
    uintN       saveNoteCount;  /* saved index for source note cancellation */
    ptrdiff_t   lastNoteOffset; /* code offset of last emitted source note */
    ptrdiff_t   saveNoteOffset; /* saved offset before last cancellable note */
};

#define CG_CODE(cg,offset)      ((cg)->base + (offset))
#define CG_OFFSET(cg)           ((cg)->ptr - (cg)->base)
#define CG_RESET(cg)            ((cg)->ptr = (cg)->base,                      \
				 INIT_ATOM_LIST(&(cg)->atomList),             \
				 (cg)->args = (cg)->vars = NULL,              \
                                 (cg)->lastCodeOffset = 0,                    \
				 (cg)->stmtInfo = NULL,                       \
                                 (cg)->stackDepth = (cg)->maxStackDepth = 0,  \
                                 CG_RESET_NOTES(cg))
#define CG_RESET_NOTES(cg)      ((cg)->notes = NULL, (cg)->noteCount = 0,     \
                                 (cg)->lastNoteCount = 0,                     \
				 (cg)->saveNoteCount = 0,                     \
                                 (cg)->lastNoteOffset = 0,                    \
                                 (cg)->saveNoteOffset = 0)
#define CG_PUSH(cg, newcg)      ((newcg)->atomList = (cg)->atomList)
#define CG_POP(cg, newcg)       ((cg)->atomList = (newcg)->atomList)

/*
 * Initialize cg to allocate from an arena pool.  Return true on success.
 * Report an exception and return false if the initial code segment can't
 * be allocated.
 */
extern JS_FRIEND_API(JSBool)
js_InitCodeGenerator(JSContext *cx, JSCodeGenerator *cg, PRArenaPool *pool);

/*
 * Emit one bytecode.
 */
extern ptrdiff_t
js_Emit1(JSContext *cx, JSCodeGenerator *cg, JSOp op);

/*
 * Emit two bytecodes, an opcode (op) with a byte of immediate operand (op1).
 */
extern ptrdiff_t
js_Emit2(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1);

/*
 * Emit three bytecodes, an opcode with two bytes of immediate operands.
 */
extern ptrdiff_t
js_Emit3(JSContext *cx, JSCodeGenerator *cg, JSOp op, jsbytecode op1,
	 jsbytecode op2);

/*
 * Unsafe macro to call js_SetJumpOffset and return false if it does.
 */
#define CHECK_AND_SET_JUMP_OFFSET(cx,cg,pc,off)                               \
    PR_BEGIN_MACRO                                                            \
	if (!js_SetJumpOffset(cx, cg, pc, off))                               \
	    return JS_FALSE;                                                  \
    PR_END_MACRO

#define CHECK_AND_SET_JUMP_OFFSET_AT(cx,cg,off)                               \
    CHECK_AND_SET_JUMP_OFFSET(cx, cg, CG_CODE(cg,off), CG_OFFSET(cg) - (off))

extern JSBool
js_SetJumpOffset(JSContext *cx, JSCodeGenerator *cg, jsbytecode *pc,
		 ptrdiff_t off);

/*
 * Cancel the last bytecode and any immediate operands emitted via cg.
 * You can't Cancel more than once between Emit calls.
 */
extern void
js_CancelLastOpcode(JSContext *cx, JSCodeGenerator *cg, uint16 *newlinesp);

/*
 * Update cg's stack depth budget for the opcode located at offset in cg.
 */
extern void
js_UpdateDepth(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t offset);

/*
 * Copy code from code generator from starting at fromOffset and running to
 * CG_OFFSET(from) into code generator to, starting at toOffset.
 *
 * NB: this function does not move source notes; the caller must do that.
 */
extern ptrdiff_t
js_MoveCode(JSContext *cx,
            JSCodeGenerator *from, ptrdiff_t fromOffset,
            JSCodeGenerator *to, ptrdiff_t toOffset);

/*
 * Push the stack-allocated struct at stmt onto the cg->stmtInfo stack.
 */
extern void
js_PushStatement(JSCodeGenerator *cg, StmtInfo *stmt, StmtType type,
		 ptrdiff_t top);

/*
 * Emit a break instruction, recording it for backpatching.
 */
extern ptrdiff_t
js_EmitBreak(JSContext *cx, JSCodeGenerator *cg, StmtInfo *stmt,
	     JSAtom *label);

/*
 * Emit a continue instruction, recording it for backpatching.
 */
extern ptrdiff_t
js_EmitContinue(JSContext *cx, JSCodeGenerator *cg, StmtInfo *stmt,
		JSAtom *label);

/*
 * Pop cg->stmtInfo.  If the top StmtInfo struct is not stack-allocated, it
 * is up to the caller to free it.
 */
extern JSBool
js_PopStatement(JSContext *cx, JSCodeGenerator *cg);

/*
 * Source notes generated along with bytecode for decompiling and debugging.
 * A source note is a uint8 with 5 bits of type and 3 of offset from the pc of
 * the previous note.  If 3 bits of offset aren't enough, extended delta notes
 * (SRC_XDELTA) consisting of 2 set high order bits followed by 6 offset bits
 * are emitted before the next note.  Some notes have operand offsets encoded
 * in note bytes or byte-pairs.
 *
 * At most one "gettable" note (i.e., a note of type other than SRC_NEWLINE,
 * SRC_SETLINE, and SRC_XDELTA) applies to a given bytecode.
 *
 * NB: the js_SrcNoteName and js_SrcNoteArity arrays in jsemit.c are indexed
 * by this enum, so their initializers need to match the order here.
 */
typedef enum JSSrcNoteType {
    SRC_NULL        = 0,        /* terminates a note vector */
    SRC_IF          = 1,        /* JSOP_IFEQ bytecode is from an if-then */
    SRC_IF_ELSE     = 2,        /* JSOP_IFEQ bytecode is from an if-then-else */
    SRC_WHILE       = 3,        /* JSOP_IFEQ is from a while loop */
    SRC_FOR         = 4,        /* JSOP_NOP or JSOP_POP in for loop head */
    SRC_CONTINUE    = 5,        /* JSOP_GOTO is a continue, not a break */
    SRC_VAR         = 6,        /* JSOP_NAME/FORNAME with a var declaration */
    SRC_COMMA       = 7,        /* JSOP_POP representing a comma operator */
    SRC_ASSIGNOP    = 8,        /* += or another assign-op follows */
    SRC_COND        = 9,        /* JSOP_IFEQ is from conditional ?: operator */
    SRC_PAREN       = 10,       /* JSOP_NOP generated to mark user parens */
    SRC_HIDDEN      = 11,       /* JSOP_LEAVEWITH for break/continue in with */
    SRC_PCBASE      = 12,       /* offset of first obj.prop.subprop bytecode */
    SRC_LABEL       = 13,       /* JSOP_NOP for label: with atomid immediate */
    SRC_LABELBRACE  = 14,       /* JSOP_NOP for label: {...} begin brace */
    SRC_ENDBRACE    = 15,       /* JSOP_NOP for label: {...} end brace */
    SRC_BREAK2LABEL = 16,       /* JSOP_GOTO for 'break label' with atomid */
    SRC_CONT2LABEL  = 17,       /* JSOP_GOTO for 'continue label' with atomid */
    SRC_SWITCH      = 18,       /* JSOP_*SWITCH with offset to end of switch */
    SRC_UNUSED19    = 19,       /* unused */
    SRC_UNUSED20    = 20,       /* unused */
    SRC_UNUSED21    = 21,       /* unused */
    SRC_NEWLINE     = 22,       /* bytecode follows a source newline */
    SRC_SETLINE     = 23,       /* a file-absolute source line number note */
    SRC_XDELTA      = 24        /* 24-31 are for extended delta notes */
} JSSrcNoteType;

#define SN_TYPE_BITS            5
#define SN_DELTA_BITS           3
#define SN_XDELTA_BITS          6
#define SN_TYPE_MASK            (PR_BITMASK(SN_TYPE_BITS) << SN_DELTA_BITS)
#define SN_DELTA_MASK           ((ptrdiff_t)PR_BITMASK(SN_DELTA_BITS))
#define SN_XDELTA_MASK          ((ptrdiff_t)PR_BITMASK(SN_XDELTA_BITS))

#define SN_MAKE_NOTE(sn,t,d)    (*(sn) = (jssrcnote)                          \
					  (((t) << SN_DELTA_BITS)             \
					   | ((d) & SN_DELTA_MASK)))
#define SN_MAKE_XDELTA(sn,d)    (*(sn) = (jssrcnote)                          \
					  ((SRC_XDELTA << SN_DELTA_BITS)      \
					   | ((d) & SN_XDELTA_MASK)))

#define SN_IS_XDELTA(sn)        ((*(sn) >> SN_DELTA_BITS) >= SRC_XDELTA)
#define SN_TYPE(sn)             (SN_IS_XDELTA(sn) ? SRC_XDELTA                \
						  : *(sn) >> SN_DELTA_BITS)
#define SN_SET_TYPE(sn,type)    SN_MAKE_NOTE(sn, type, SN_DELTA(sn))
#define SN_IS_GETTABLE(sn)      (SN_TYPE(sn) < SRC_NEWLINE)

#define SN_DELTA(sn)            ((ptrdiff_t)(SN_IS_XDELTA(sn)                 \
					     ? *(sn) & SN_XDELTA_MASK         \
					     : *(sn) & SN_DELTA_MASK))
#define SN_SET_DELTA(sn,delta)  (SN_IS_XDELTA(sn)                             \
				 ? SN_MAKE_XDELTA(sn, delta)                  \
				 : SN_MAKE_NOTE(sn, SN_TYPE(sn), delta))

#define SN_DELTA_LIMIT          ((ptrdiff_t)PR_BIT(SN_DELTA_BITS))
#define SN_XDELTA_LIMIT         ((ptrdiff_t)PR_BIT(SN_XDELTA_BITS))

/*
 * Offset fields follow certain notes and are frequency-encoded: an offset in
 * [0,127] consumes one byte, an offset in [128,32767] takes two, and the high
 * bit of the first byte is set.
 */
#define SN_2BYTE_OFFSET_FLAG    0x80
#define SN_2BYTE_OFFSET_MASK    0x7f

extern const char *js_SrcNoteName[];
extern uint8 js_SrcNoteArity[];
extern uintN js_SrcNoteLength(jssrcnote *sn);

#define SN_LENGTH(sn)           ((js_SrcNoteArity[SN_TYPE(sn)] == 0) ? 1      \
				 : js_SrcNoteLength(sn))
#define SN_NEXT(sn)             ((sn) + SN_LENGTH(sn))

/* A source note array is terminated by an all-zero element. */
#define SN_MAKE_TERMINATOR(sn)  (*(sn) = SRC_NULL)
#define SN_IS_TERMINATOR(sn)    (*(sn) == SRC_NULL)

/*
 * Append a new source note of the given type (and therefore size) to cg's
 * notes dynamic array, updating cg->noteCount.  Return the new note's index
 * within the array pointed at by cg->notes.  Return -1 if out of memory.
 */
extern intN
js_NewSrcNote(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type);

extern intN
js_NewSrcNote2(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type,
	       ptrdiff_t offset);

extern intN
js_NewSrcNote3(JSContext *cx, JSCodeGenerator *cg, JSSrcNoteType type,
	       ptrdiff_t offset1, ptrdiff_t offset2);

/*
 * Increment the delta of the note at cg->notes[index], overflowing into one
 * or more xdelta notes if necessary.
 */
extern JSBool
js_BumpSrcNoteDelta(JSContext *cx, JSCodeGenerator *cg, uintN index,
		    ptrdiff_t incr);

/*
 * Get and set the offset operand identified by which (0 for the first, etc.).
 */
extern ptrdiff_t
js_GetSrcNoteOffset(jssrcnote *sn, uintN which);

extern JSBool
js_SetSrcNoteOffset(JSContext *cx, JSCodeGenerator *cg, uintN index,
		    uintN which, ptrdiff_t offset);

/*
 * Copy source notes from one code generator to another.  The code annotated
 * by the copied notes must be moved *after* the notes are moved.
 */
extern JSBool
js_MoveSrcNotes(JSContext *cx, JSCodeGenerator *from, JSCodeGenerator *to);

/*
 * Finish taking source notes in cx's tempPool by copying them to new stable
 * store allocated via JS_malloc.  Return null on malloc failure, which means
 * this function reported an error.
 */
extern JS_FRIEND_API(jssrcnote *)
js_FinishTakingSrcNotes(JSContext *cx, JSCodeGenerator *cg);

PR_END_EXTERN_C

#endif /* jsemit_h___ */
