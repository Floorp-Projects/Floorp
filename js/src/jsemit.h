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

#ifndef jsemit_h___
#define jsemit_h___
/*
 * JS bytecode generation.
 */

#include "jsstddef.h"
#include "jstypes.h"
#include "jsatom.h"
#include "jsopcode.h"
#include "jsprvtd.h"
#include "jspubtd.h"

JS_BEGIN_EXTERN_C

typedef enum JSStmtType {
    STMT_BLOCK        = 0,      /* compound statement: { s1[;... sN] } */
    STMT_LABEL        = 1,      /* labeled statement:  L: s */
    STMT_IF           = 2,      /* if (then) statement */
    STMT_ELSE         = 3,      /* else clause of if statement */
    STMT_SWITCH       = 4,      /* switch statement */
    STMT_WITH         = 5,      /* with statement */
    STMT_TRY          = 6,      /* try statement */
    STMT_CATCH        = 7,      /* catch block */
    STMT_FINALLY      = 8,      /* finally statement */
    STMT_DO_LOOP      = 9,      /* do/while loop statement */
    STMT_FOR_LOOP     = 10,     /* for loop statement */
    STMT_FOR_IN_LOOP  = 11,     /* for/in loop statement */
    STMT_WHILE_LOOP   = 12      /* while loop statement */
} JSStmtType;

#define STMT_IS_LOOP(stmt)      ((stmt)->type >= STMT_DO_LOOP)

typedef struct JSStmtInfo JSStmtInfo;

struct JSStmtInfo {
    JSStmtType      type;           /* statement type */
    ptrdiff_t       top;            /* offset of loop top from cg base */
    ptrdiff_t       update;         /* loop update offset (top if none) */
    ptrdiff_t       breaks;         /* offset of last break in loop */
    ptrdiff_t       continues;      /* offset of last continue in loop */
    ptrdiff_t       gosub;          /* offset of last GOSUB for this finally */
    ptrdiff_t       catchJump;      /* offset of last end-of-catch jump */
    JSAtom          *label;         /* name of LABEL or CATCH var */
    JSStmtInfo      *down;          /* info for enclosing statement */
};

#define SET_STATEMENT_TOP(stmt, top)                                          \
    ((stmt)->top = (stmt)->update = (top), (stmt)->breaks =                   \
     (stmt)->continues = (stmt)->catchJump = (stmt)->gosub = (-1))

struct JSTreeContext {              /* tree context for semantic checks */
    uint32          flags;          /* statement state flags, see below */
    uint32          tryCount;       /* total count of try statements parsed */
    JSStmtInfo      *topStmt;       /* top of statement info stack */
    JSAtomList      decls;          /* function, const, and var declarations */
    JSParseNode     *nodeList;      /* list of recyclable parse-node structs */
};

#define TCF_COMPILING          0x01 /* generating bytecode; this tc is a cg */
#define TCF_IN_FUNCTION        0x02 /* parsing inside function body */
#define TCF_RETURN_EXPR        0x04 /* function has 'return expr;' */
#define TCF_RETURN_VOID        0x08 /* function has 'return;' */
#define TCF_IN_FOR_INIT        0x10 /* parsing init expr of for; exclude 'in' */
#define TCF_FUN_CLOSURE_VS_VAR 0x20 /* function and var with same name */
#define TCF_FUN_USES_NONLOCALS 0x40 /* function refers to non-local names */
#define TCF_FUN_HEAVYWEIGHT    0x80 /* function needs Call object per call */
#define TCF_FUN_FLAGS          0xE0 /* flags to propagate from FunctionBody */

#define TREE_CONTEXT_INIT(tc)                                                 \
    ((tc)->flags = 0, (tc)->tryCount = 0, (tc)->topStmt = NULL,               \
     ATOM_LIST_INIT(&(tc)->decls), (tc)->nodeList = NULL)

#define TREE_CONTEXT_FINISH(tc)                                               \
    ((void)0)

/*
 * Span-dependent instructions are jumps whose span (from the jump bytecode to
 * the jump target) may require 2 or 4 bytes of immediate operand.
 */
typedef struct JSSpanDep    JSSpanDep;
typedef struct JSJumpTarget JSJumpTarget;

struct JSSpanDep {
    ptrdiff_t       top;        /* offset of first bytecode in an opcode */
    ptrdiff_t       offset;     /* offset - 1 within opcode of jump operand */
    ptrdiff_t       before;     /* original offset - 1 of jump operand */
    JSJumpTarget    *target;    /* tagged target pointer or backpatch delta */
};

/*
 * Jump targets are stored in an AVL tree, for O(log(n)) lookup with targets
 * sorted by offset from left to right, so that targets above a span-dependent
 * instruction whose jump offset operand must be extended can be found quickly
 * and adjusted upward (toward higher offsets).
 */
struct JSJumpTarget {
    ptrdiff_t       offset;     /* offset of span-dependent jump target */
    int             balance;    /* AVL tree balance number */
    JSJumpTarget    *kids[2];   /* left and right AVL tree child pointers */
};

#define JT_LEFT                 0
#define JT_RIGHT                1
#define JT_OTHER_DIR(dir)       (1 - (dir))
#define JT_IMBALANCE(dir)       (((dir) << 1) - 1)
#define JT_DIR(imbalance)       (((imbalance) + 1) >> 1)

/*
 * Backpatch deltas are encoded in JSSpanDep.target if JT_TAG_BIT is clear,
 * so we can maintain backpatch chains when using span dependency records to
 * hold jump offsets that overflow 16 bits.
 */
#define JT_TAG_BIT              ((jsword) 1)
#define JT_UNTAG_SHIFT          1
#define JT_SET_TAG(jt)          ((JSJumpTarget *)((jsword)(jt) | JT_TAG_BIT))
#define JT_CLR_TAG(jt)          ((JSJumpTarget *)((jsword)(jt) & ~JT_TAG_BIT))
#define JT_HAS_TAG(jt)          ((jsword)(jt) & JT_TAG_BIT)

#define BITS_PER_PTRDIFF        (sizeof(ptrdiff_t) * JS_BITS_PER_BYTE)
#define BITS_PER_BPDELTA        (BITS_PER_PTRDIFF - 1 - JT_UNTAG_SHIFT)
#define BPDELTA_MAX             ((ptrdiff_t)(JS_BIT(BITS_PER_BPDELTA) - 1))
#define BPDELTA_TO_TN(bp)       ((JSJumpTarget *)((bp) << JT_UNTAG_SHIFT))
#define JT_TO_BPDELTA(jt)       ((ptrdiff_t)((jsword)(jt) >> JT_UNTAG_SHIFT))

#define SD_SET_TARGET(sd,jt)    ((sd)->target = JT_SET_TAG(jt))
#define SD_SET_BPDELTA(sd,bp)   ((sd)->target = BPDELTA_TO_TN(bp))
#define SD_GET_BPDELTA(sd)      (JS_ASSERT(!JT_HAS_TAG((sd)->target)),        \
                                 JT_TO_BPDELTA((sd)->target))
#define SD_TARGET_OFFSET(sd)    (JS_ASSERT(JT_HAS_TAG((sd)->target)),         \
                                 JT_CLR_TAG((sd)->target)->offset)

struct JSCodeGenerator {
    JSTreeContext   treeContext;    /* base state: statement info stack, etc. */
    void            *codeMark;      /* low watermark in cx->codePool */
    void            *noteMark;      /* low watermark in cx->notePool */
    void            *tempMark;      /* low watermark in cx->tempPool */
    struct {
        jsbytecode  *base;          /* base of JS bytecode vector */
        jsbytecode  *limit;         /* one byte beyond end of bytecode */
        jsbytecode  *next;          /* pointer to next free bytecode */
    } prolog, main, *current;
    const char      *filename;      /* null or weak link to source filename */
    uintN           firstLine;      /* first line, for js_NewScriptFromCG */
    uintN           currentLine;    /* line number for tree-based srcnote gen */
    JSPrincipals    *principals;    /* principals for constant folding eval */
    JSAtomList      atomList;       /* literals indexed for mapping */
    intN            stackDepth;     /* current stack depth in script frame */
    uintN           maxStackDepth;  /* maximum stack depth so far */
    jssrcnote       *notes;         /* source notes, see below */
    uintN           noteCount;      /* number of source notes so far */
    uintN           noteMask;       /* growth increment for notes */
    ptrdiff_t       lastNoteOffset; /* code offset for last source note */
    JSTryNote       *tryBase;       /* first exception handling note */
    JSTryNote       *tryNext;       /* next available note */
    size_t          tryNoteSpace;   /* # of bytes allocated at tryBase */
    JSSpanDep       *spanDeps;      /* span dependent instruction records */
    JSJumpTarget    *jumpTargets;   /* AVL tree of jump target offsets */
    JSJumpTarget    *jtFreeList;    /* JT_LEFT-linked list of free structs */
    uintN           numSpanDeps;    /* number of span dependencies */
    uintN           numJumpTargets; /* number of jump targets */
    uintN           emitLevel;      /* js_EmitTree recursion level */
};

#define CG_BASE(cg)             ((cg)->current->base)
#define CG_LIMIT(cg)            ((cg)->current->limit)
#define CG_NEXT(cg)             ((cg)->current->next)
#define CG_CODE(cg,offset)      (CG_BASE(cg) + (offset))
#define CG_OFFSET(cg)           PTRDIFF(CG_NEXT(cg), CG_BASE(cg), jsbytecode)

#define CG_PROLOG_BASE(cg)      ((cg)->prolog.base)
#define CG_PROLOG_LIMIT(cg)     ((cg)->prolog.limit)
#define CG_PROLOG_NEXT(cg)      ((cg)->prolog.next)
#define CG_PROLOG_CODE(cg,poff) (CG_PROLOG_BASE(cg) + (poff))
#define CG_PROLOG_OFFSET(cg)    PTRDIFF(CG_PROLOG_NEXT(cg), CG_PROLOG_BASE(cg),\
                                        jsbytecode)

#define CG_SWITCH_TO_MAIN(cg)   ((cg)->current = &(cg)->main)
#define CG_SWITCH_TO_PROLOG(cg) ((cg)->current = &(cg)->prolog)

/*
 * Initialize cg to allocate bytecode space from cx->codePool, source note
 * space from cx->notePool, and all other arena-allocated temporaries from
 * cx->tempPool.  Return true on success.  Report an error and return false
 * if the initial code segment can't be allocated.
 */
extern JS_FRIEND_API(JSBool)
js_InitCodeGenerator(JSContext *cx, JSCodeGenerator *cg,
		     const char *filename, uintN lineno,
		     JSPrincipals *principals);

/*
 * Release cx->codePool, cx->notePool, and cx->tempPool to marks set by
 * js_InitCodeGenerator.  Note that cgs are magic: they own the arena pool
 * "tops-of-stack" space above their codeMark, noteMark, and tempMark points.
 * This means you cannot alloc from tempPool and save the pointer beyond the
 * next JS_FinishCodeGenerator.
 */
extern JS_FRIEND_API(void)
js_FinishCodeGenerator(JSContext *cx, JSCodeGenerator *cg);

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
 * Emit (1 + extra) bytecodes, for N bytes of op and its immediate operand.
 */
extern ptrdiff_t
js_EmitN(JSContext *cx, JSCodeGenerator *cg, JSOp op, size_t extra);

/*
 * Unsafe macro to call js_SetJumpOffset and return false if it does.
 */
#define CHECK_AND_SET_JUMP_OFFSET(cx,cg,pc,off)                               \
    JS_BEGIN_MACRO                                                            \
	if (!js_SetJumpOffset(cx, cg, pc, off))                               \
	    return JS_FALSE;                                                  \
    JS_END_MACRO

#define CHECK_AND_SET_JUMP_OFFSET_AT(cx,cg,off)                               \
    CHECK_AND_SET_JUMP_OFFSET(cx, cg, CG_CODE(cg,off), CG_OFFSET(cg) - (off))

extern JSBool
js_SetJumpOffset(JSContext *cx, JSCodeGenerator *cg, jsbytecode *pc,
		 ptrdiff_t off);

/* Test whether we're in a with statement. */
extern JSBool
js_InWithStatement(JSTreeContext *tc);

/* Test whether we're in a catch block with exception named by atom. */
extern JSBool
js_InCatchBlock(JSTreeContext *tc, JSAtom *atom);

/*
 * Push the C-stack-allocated struct at stmt onto the stmtInfo stack.
 */
extern void
js_PushStatement(JSTreeContext *tc, JSStmtInfo *stmt, JSStmtType type,
		 ptrdiff_t top);

/*
 * Emit a break instruction, recording it for backpatching.
 */
extern ptrdiff_t
js_EmitBreak(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *stmt,
	     JSAtomListElement *label);

/*
 * Emit a continue instruction, recording it for backpatching.
 */
extern ptrdiff_t
js_EmitContinue(JSContext *cx, JSCodeGenerator *cg, JSStmtInfo *stmt,
		JSAtomListElement *label);

/*
 * Pop tc->topStmt.  If the top JSStmtInfo struct is not stack-allocated, it
 * is up to the caller to free it.
 */
extern void
js_PopStatement(JSTreeContext *tc);

/*
 * Like js_PopStatement(&cg->treeContext), also patch breaks and continues.
 * May fail if a jump offset overflows.
 */
extern JSBool
js_PopStatementCG(JSContext *cx, JSCodeGenerator *cg);

/*
 * Emit code into cg for the tree rooted at pn.
 */
extern JSBool
js_EmitTree(JSContext *cx, JSCodeGenerator *cg, JSParseNode *pn);

/*
 * Emit code into cg for the tree rooted at body, then create a persistent
 * script for fun from cg.
 */
extern JSBool
js_EmitFunctionBody(JSContext *cx, JSCodeGenerator *cg, JSParseNode *body,
		    JSFunction *fun);

/*
 * Source notes generated along with bytecode for decompiling and debugging.
 * A source note is a uint8 with 5 bits of type and 3 of offset from the pc of
 * the previous note.  If 3 bits of offset aren't enough, extended delta notes
 * (SRC_XDELTA) consisting of 2 set high order bits followed by 6 offset bits
 * are emitted before the next note.  Some notes have operand offsets encoded
 * immediately after them, in note bytes or byte-triples.
 *
 *                 Source Note               Extended Delta
 *              +7-6-5-4-3+2-1-0+           +7-6-5+4-3-2-1-0+
 *              |note-type|delta|           |1 1| ext-delta |
 *              +---------+-----+           +---+-----------+
 *
 * At most one "gettable" note (i.e., a note of type other than SRC_NEWLINE,
 * SRC_SETLINE, and SRC_XDELTA) applies to a given bytecode.
 *
 * NB: the js_SrcNoteSpec array in jsemit.c is indexed by this enum, so its
 * initializers need to match the order here.
 */
typedef enum JSSrcNoteType {
    SRC_NULL        = 0,        /* terminates a note vector */
    SRC_IF          = 1,        /* JSOP_IFEQ bytecode is from an if-then */
    SRC_IF_ELSE     = 2,        /* JSOP_IFEQ bytecode is from an if-then-else */
    SRC_WHILE       = 3,        /* JSOP_IFEQ is from a while loop */
    SRC_FOR         = 4,        /* JSOP_NOP or JSOP_POP in for loop head */
    SRC_CONTINUE    = 5,        /* JSOP_GOTO is a continue, not a break;
                                   also used on JSOP_ENDINIT if extra comma
                                   at end of array literal: [1,2,,] */
    SRC_VAR         = 6,        /* JSOP_NAME/SETNAME/FORNAME in a var decl */
    SRC_PCDELTA     = 7,        /* offset from comma-operator to next POP,
				   or from CONDSWITCH to first CASE opcode */
    SRC_ASSIGNOP    = 8,        /* += or another assign-op follows */
    SRC_COND        = 9,        /* JSOP_IFEQ is from conditional ?: operator */
    SRC_RESERVED0   = 10,       /* reserved for future use */
    SRC_HIDDEN      = 11,       /* opcode shouldn't be decompiled */
    SRC_PCBASE      = 12,       /* offset of first obj.prop.subprop bytecode */
    SRC_LABEL       = 13,       /* JSOP_NOP for label: with atomid immediate */
    SRC_LABELBRACE  = 14,       /* JSOP_NOP for label: {...} begin brace */
    SRC_ENDBRACE    = 15,       /* JSOP_NOP for label: {...} end brace */
    SRC_BREAK2LABEL = 16,       /* JSOP_GOTO for 'break label' with atomid */
    SRC_CONT2LABEL  = 17,       /* JSOP_GOTO for 'continue label' with atomid */
    SRC_SWITCH      = 18,       /* JSOP_*SWITCH with offset to end of switch,
				   2nd off to first JSOP_CASE if condswitch */
    SRC_FUNCDEF     = 19,       /* JSOP_NOP for function f() with atomid */
    SRC_CATCH       = 20,       /* catch block has guard */
    SRC_CONST       = 21,       /* JSOP_SETCONST in a const decl */
    SRC_NEWLINE     = 22,       /* bytecode follows a source newline */
    SRC_SETLINE     = 23,       /* a file-absolute source line number note */
    SRC_XDELTA      = 24        /* 24-31 are for extended delta notes */
} JSSrcNoteType;

#define SN_TYPE_BITS            5
#define SN_DELTA_BITS           3
#define SN_XDELTA_BITS          6
#define SN_TYPE_MASK            (JS_BITMASK(SN_TYPE_BITS) << SN_DELTA_BITS)
#define SN_DELTA_MASK           ((ptrdiff_t)JS_BITMASK(SN_DELTA_BITS))
#define SN_XDELTA_MASK          ((ptrdiff_t)JS_BITMASK(SN_XDELTA_BITS))

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

#define SN_DELTA_LIMIT          ((ptrdiff_t)JS_BIT(SN_DELTA_BITS))
#define SN_XDELTA_LIMIT         ((ptrdiff_t)JS_BIT(SN_XDELTA_BITS))

/*
 * Offset fields follow certain notes and are frequency-encoded: an offset in
 * [0,0x7f] consumes one byte, an offset in [0x80,0x7fffff] takes three, and
 * the high bit of the first byte is set.
 */
#define SN_3BYTE_OFFSET_FLAG    0x80
#define SN_3BYTE_OFFSET_MASK    0x7f

typedef struct JSSrcNoteSpec {
    const char      *name;      /* name for disassembly/debugging output */
    uint8           arity;      /* number of offset operands */
    uint8           offsetBias; /* bias of offset(s) from annotated pc */
    int8            isSpanDep;  /* 1 or -1 if offsets could span extended ops,
                                   0 otherwise; sign tells span direction */
} JSSrcNoteSpec;

extern JS_FRIEND_DATA(JSSrcNoteSpec) js_SrcNoteSpec[];
extern JS_FRIEND_API(uintN)          js_SrcNoteLength(jssrcnote *sn);

#define SN_LENGTH(sn)           ((js_SrcNoteSpec[SN_TYPE(sn)].arity == 0) ? 1 \
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
 * NB: this function can add at most one extra extended delta note.
 */
extern jssrcnote *
js_AddToSrcNoteDelta(JSContext *cx, JSCodeGenerator *cg, jssrcnote *sn,
                     ptrdiff_t delta);

/*
 * Get and set the offset operand identified by which (0 for the first, etc.).
 */
extern JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, uintN which);

extern JSBool
js_SetSrcNoteOffset(JSContext *cx, JSCodeGenerator *cg, uintN index,
		    uintN which, ptrdiff_t offset);

/*
 * Finish taking source notes in cx's notePool by copying them to new
 * stable store allocated via JS_malloc.  Return null on malloc failure,
 * which means this function reported an error.
 */
extern jssrcnote *
js_FinishTakingSrcNotes(JSContext *cx, JSCodeGenerator *cg);

/*
 * Allocate cg->treeContext.tryCount notes (plus one for the end sentinel)
 * from cx->tempPool and set up cg->tryBase/tryNext for exactly tryCount
 * js_NewTryNote calls.  The storage is freed by js_FinishCodeGenerator.
 */
extern JSBool
js_AllocTryNotes(JSContext *cx, JSCodeGenerator *cg);

/*
 * Grab the next trynote slot in cg, filling it in appropriately.
 */
extern JSTryNote *
js_NewTryNote(JSContext *cx, JSCodeGenerator *cg, ptrdiff_t start,
	      ptrdiff_t end, ptrdiff_t catchStart);

/*
 * Finish generating exception information, and copy it to JS_malloc
 * storage.
 */
extern JSBool
js_FinishTakingTryNotes(JSContext *cx, JSCodeGenerator *cg, JSTryNote **tryp);

JS_END_EXTERN_C

#endif /* jsemit_h___ */
